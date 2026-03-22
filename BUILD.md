# Building ALNview from Source

ALNview is a Qt 6 application. This document covers building it on Linux,
both natively and via Docker for producing a portable static binary.

## Dependencies

| Dependency | Minimum Version | Purpose |
|------------|----------------|---------|
| Qt 6 (qtbase) | 6.4.2+ (6.9.0 recommended) | GUI framework (widgets) |
| zlib | 1.2+ | Compression |
| g++ / gcc | 13+ | C/C++ compiler |
| make | any | Build tool |

### Ubuntu / Debian

```bash
sudo apt-get install -y \
  build-essential \
  qt6-base-dev \
  qt6-base-dev-tools \
  qmake6 \
  zlib1g-dev
```

### Fedora / RHEL

```bash
sudo dnf install -y \
  gcc gcc-c++ make \
  qt6-qtbase-devel \
  zlib-devel
```

## Building Natively on Linux

```bash
mkdir build && cd build
qmake6 ../viewer.pro -spec linux-g++
make -j$(nproc)
```

The binary is produced at `build/ALNview`.

### Note on the .pro file

The `viewer.pro` file wraps the macOS-specific `QMAKE_SPEC = macx-clang`
in a `macx { }` guard, so it is only applied when building on macOS. On
Linux, pass `-spec linux-g++` to qmake, or let qmake auto-detect the
platform.

## Building on macOS

```bash
mkdir build && cd build
qmake ../viewer.pro
make -j$(sysctl -n hw.ncpu)
```

This uses the `macx-clang` spec set in the .pro file. Requires Qt 6
installed via Homebrew (`brew install qt@6`) or the Qt installer.

### Creating a macOS DMG

After building, create a distributable `.dmg`:

```bash
cd build
macdeployqt ALNview.app

# Ad-hoc code sign (inside-out to avoid "damaged" errors)
find ALNview.app/Contents/Frameworks -name "*.framework" -type d | while read fw; do
  codesign --force --sign - --timestamp=none "$fw"
done
find ALNview.app/Contents/Frameworks -name "*.dylib" -type f | while read lib; do
  codesign --force --sign - --timestamp=none "$lib"
done
find ALNview.app/Contents/PlugIns -name "*.dylib" -type f 2>/dev/null | while read plugin; do
  codesign --force --sign - --timestamp=none "$plugin"
done
codesign --force --sign - --timestamp=none ALNview.app/Contents/MacOS/ALNview
codesign --force --sign - --timestamp=none ALNview.app

# Create DMG
hdiutil create -volname "ALNview" -srcfolder ALNview.app -ov -format UDZO ALNview.dmg
```

### macOS "damaged" app workaround

Since ALNview is ad-hoc signed (not notarized by Apple), macOS
Gatekeeper may show "'ALNview' is damaged and can't be opened" when
opening a downloaded DMG. To fix this, remove the quarantine attribute:

```bash
xattr -cr /Applications/ALNview.app
```

Or right-click the app and choose "Open" (bypasses Gatekeeper once).

## Building a Portable Static Linux Binary via Docker

The included `Dockerfile` produces a **fully static, single-file binary**
using Alpine Linux (musl libc) with Qt 6.9.0 and all dependencies linked
statically. The result has zero runtime dependencies and runs on any
x86_64 Linux system regardless of installed libraries or glibc version.

```bash
# Build and extract the binary
docker build --target output --output type=local,dest=dist .

# Verify it's fully static
file dist/ALNview
# => ELF 64-bit LSB executable, x86_64, statically linked, stripped

# Copy to any Linux machine and run (no installation needed)
scp dist/ALNview user@remote:~/
ssh -X user@remote ./ALNview
```

The resulting binary is ~28 MB.

### What the Docker build does

1. **Stage 1 (qt-builder):** On Alpine, builds xcb-util from source as a
   static library, then downloads and builds Qt 6.9.0 qtbase as a static
   library with only the needed features (widgets, xcb, fontconfig — no
   OpenGL, dbus, glib, ICU, sql, network, or testlib).
2. **Stage 2 (app-builder):** Compiles ALNview with `-static` linking
   against the static Qt and all transitive static dependencies
   (xcb, X11, Xau, Xdmcp, fontconfig, freetype, harfbuzz, graphite2,
   libpng, libjpeg, zlib, brotli, bz2, expat, pcre2, xkbcommon, etc.),
   producing a single fully static musl binary.
3. **Stage 3 (output):** A `scratch` image containing only the binary.

### Build time

The Qt static build takes ~5-10 minutes depending on hardware. Docker
layer caching means subsequent ALNview-only changes rebuild in seconds.

### Running on a remote server (no sudo)

The static binary needs only an X11 display. Use SSH X forwarding or VNC:

```bash
ssh -X user@remote
./ALNview
```

## GitHub Actions CI

The `.github/workflows/build.yml` workflow:

- **On push to `main` or PR:** Builds the Linux static binary via Docker
  and the macOS DMG on a macOS runner, uploading both as artifacts.
- **On tag push (`v*`):** Creates a GitHub Release with both the Linux
  tarball and macOS DMG attached.

To create a release:

```bash
git tag v1.0.0
git push origin v1.0.0
```
