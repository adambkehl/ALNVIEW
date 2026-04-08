# Top-level GNUmakefile for ALNview
# GNU make reads GNUmakefile before Makefile, so this won't conflict
# with the qmake-generated Makefile.
#
# Targets:
#   make linux-build    — native Linux release build (requires Qt 6, qmake6)
#   make macos-build    — macOS release build (requires Qt 6, qmake)
#   make asan-build     — ASan debug build
#   make test           — build and run unit tests under ASan
#   make clean          — clean all build artifacts

NPROC := $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

.PHONY: linux-build macos-build asan-build test clean clean-build clean-test help

help:
	@echo "ALNview build targets:"
	@echo "  make linux-build   — Linux release build (output: build/ALNview)"
	@echo "  make macos-build   — macOS release build (output: ALNview.app)"
	@echo "  make asan-build    — ASan debug build (output: build-asan/ALNview)"
	@echo "  make test          — build & run unit tests under ASan"
	@echo "  make clean         — clean all build artifacts"

# --- Linux native build ---
linux-build:
	@mkdir -p build
	cd build && qmake6 ../viewer.pro -spec linux-g++ && $(MAKE) -j$(NPROC)
	@echo "Built: build/ALNview"

# --- macOS build ---
macos-build:
	@mkdir -p build
	cd build && qmake ../viewer.pro && $(MAKE) -j$(NPROC)
	@echo "Built: build/ALNview.app"

# --- ASan debug build ---
asan-build:
	@mkdir -p build-asan
	cd build-asan && qmake6 "CONFIG+=asan" ../viewer.pro && $(MAKE) -j$(NPROC)
	@echo "Built: build-asan/ALNview"

# --- Tests ---
test:
	$(MAKE) -C tests test

# --- Clean ---
clean: clean-build clean-test

clean-build:
	rm -rf build/BUILD build/ALNview build/Makefile
	rm -rf build-asan/BUILD build-asan/ALNview build-asan/Makefile

clean-test:
	$(MAKE) -C tests clean
