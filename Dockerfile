# Dockerfile for building ALNview as a fully static Linux binary.
#
# Uses Alpine Linux (musl libc) with all dependencies statically linked.
# The resulting single binary has ZERO runtime dependencies — no glibc,
# no shared libraries — and runs on any x86_64 Linux kernel.
#
# Usage:
#   docker build --target output --output type=local,dest=dist .
#   scp dist/ALNview user@remote:~/
#   ssh -X user@remote ./ALNview

# ---------------------------------------------------------------------------
# Stage 1 — Build static Qt 6 on Alpine (musl)
# ---------------------------------------------------------------------------
FROM alpine:3.21 AS qt-builder

ENV QT_VERSION=6.9.0

RUN apk add --no-cache \
    build-base cmake ninja perl python3 git curl pkgconf linux-headers \
    # X11 / xcb (dev packages include static .a on Alpine)
    libx11-dev libxcb-dev libx11-static libxcb-static \
    xcb-util-dev xcb-util-image-dev xcb-util-keysyms-dev \
    xcb-util-renderutil-dev xcb-util-wm-dev xcb-util-cursor-dev \
    libxkbcommon-dev libxkbcommon-static \
    libxshmfence-dev \
    # Font rendering
    fontconfig-dev fontconfig-static \
    freetype-dev freetype-static \
    harfbuzz-dev harfbuzz-static \
    graphite2-dev graphite2-static \
    # Image codecs
    libpng-dev libpng-static \
    libjpeg-turbo-dev libjpeg-turbo-static \
    # Compression
    zlib-dev zlib-static \
    brotli-dev brotli-static \
    bzip2-dev bzip2-static \
    zstd-dev zstd-static \
    # Misc transitive deps
    expat-dev expat-static \
    pcre2-dev \
    libxau-dev \
    libxdmcp-dev \
    libmd-dev \
    libbsd-dev libbsd-static \
    # For building xcb-util from source
    autoconf automake libtool m4

WORKDIR /build

# Build xcb-util as a static lib (Alpine only ships .so, not .a)
RUN curl -fsSL "https://xcb.freedesktop.org/dist/xcb-util-0.4.1.tar.xz" -o xcb-util.tar.xz \
    && tar xf xcb-util.tar.xz && rm xcb-util.tar.xz \
    && cd xcb-util-0.4.1 \
    && ./configure --prefix=/usr --enable-static --disable-shared \
    && make -j$(nproc) && make install \
    && cd .. && rm -rf xcb-util-0.4.1

# Download Qt source (qtbase only — we only need widgets)
RUN curl -fsSL "https://download.qt.io/official_releases/qt/6.9/6.9.0/submodules/qtbase-everywhere-src-6.9.0.tar.xz" \
    -o qtbase.tar.xz \
    && tar xf qtbase.tar.xz \
    && rm qtbase.tar.xz

# Build a minimal static Qt: widgets + xcb + fonts, no GL/dbus/glib/icu.
# Disabling opengl, glib, icu, dbus eliminates many deps and ~35 MB of
# ICU data. ALNview is a 2D widget app and doesn't need OpenGL.
RUN cd qtbase-everywhere-src-${QT_VERSION} && \
    cmake -B build -G Ninja \
      -DCMAKE_INSTALL_PREFIX=/opt/qt6-static \
      -DBUILD_SHARED_LIBS=OFF \
      -DCMAKE_BUILD_TYPE=Release \
      -DQT_BUILD_EXAMPLES=OFF \
      -DQT_BUILD_TESTS=OFF \
      -DFEATURE_dbus=OFF \
      -DFEATURE_sql=OFF \
      -DFEATURE_testlib=OFF \
      -DFEATURE_network=OFF \
      -DINPUT_opengl=no \
      -DFEATURE_egl=OFF \
      -DFEATURE_eglfs=OFF \
      -DFEATURE_glib=OFF \
      -DFEATURE_icu=OFF \
      -DFEATURE_widgets=ON \
      -DFEATURE_gui=ON \
      -DFEATURE_xcb=ON \
      -DFEATURE_xkbcommon=ON \
      -DFEATURE_fontconfig=ON \
      -DFEATURE_freetype=ON \
      -DFEATURE_harfbuzz=ON \
      -DFEATURE_png=ON \
      -DFEATURE_ico=ON \
      -DFEATURE_jpeg=ON \
    && cmake --build build --parallel $(nproc) \
    && cmake --install build

# ---------------------------------------------------------------------------
# Stage 2 — Build ALNview, fully statically linked
# ---------------------------------------------------------------------------
FROM alpine:3.21 AS app-builder

RUN apk add --no-cache \
    build-base cmake ninja perl pkgconf linux-headers \
    curl autoconf automake libtool m4 \
    libx11-dev libxcb-dev libx11-static libxcb-static \
    xcb-util-dev xcb-util-image-dev xcb-util-keysyms-dev \
    xcb-util-renderutil-dev xcb-util-wm-dev xcb-util-cursor-dev \
    libxkbcommon-dev libxkbcommon-static \
    libxshmfence-dev \
    fontconfig-dev fontconfig-static \
    freetype-dev freetype-static \
    harfbuzz-dev harfbuzz-static \
    graphite2-dev graphite2-static \
    libpng-dev libpng-static \
    libjpeg-turbo-dev libjpeg-turbo-static \
    zlib-dev zlib-static \
    brotli-dev brotli-static \
    bzip2-dev bzip2-static \
    zstd-dev zstd-static \
    expat-dev expat-static \
    pcre2-dev \
    libxau-dev \
    libxdmcp-dev \
    libmd-dev \
    libbsd-dev libbsd-static \
    file

# Build xcb-util static lib (same as stage 1)
RUN curl -fsSL "https://xcb.freedesktop.org/dist/xcb-util-0.4.1.tar.xz" -o /tmp/xcb-util.tar.xz \
    && cd /tmp && tar xf xcb-util.tar.xz && rm xcb-util.tar.xz \
    && cd xcb-util-0.4.1 \
    && ./configure --prefix=/usr --enable-static --disable-shared \
    && make -j$(nproc) && make install \
    && cd / && rm -rf /tmp/xcb-util-0.4.1

COPY --from=qt-builder /opt/qt6-static /opt/qt6-static

WORKDIR /src
COPY . /src/

# Build with -static and supply all transitive static deps.
# With static linking, the linker can't auto-discover transitive deps
# from .so metadata, so we must list every leaf library explicitly.
#
# Dependency chains resolved:
#   fontconfig → expat (XML parsing of fonts.conf)
#   freetype   → brotlidec → brotlicommon (WOFF2 fonts), bz2 (BDF fonts)
#   harfbuzz   → graphite2 (font shaping)
#   xcb-image  → xcb-util (xcb_aux_create_gc)
#   xcb        → Xau (auth), Xdmcp (display mgmt)
#   libbsd     → libmd (message digest)
RUN mkdir build && cd build \
    && /opt/qt6-static/bin/qmake6 ../viewer.pro -spec linux-g++ \
       "QMAKE_LFLAGS+=-static" \
       "QMAKE_LIBS+=-lxcb-util -lXau -lXdmcp -lbrotlidec -lbrotlicommon -lbz2 -lgraphite2 -lexpat -lmd -lbsd" \
    && make -j$(nproc)

RUN strip build/ALNview

# Verify: should say "statically linked"
RUN file build/ALNview && ldd build/ALNview 2>&1 || true

# ---------------------------------------------------------------------------
# Stage 3 — Output just the single binary
# ---------------------------------------------------------------------------
FROM scratch AS output
COPY --from=app-builder /src/build/ALNview /ALNview
