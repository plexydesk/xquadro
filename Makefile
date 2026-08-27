/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */

XSERVER_DIR  ?= xserver
PLEXY_LIB    ?= /usr/local/lib
PLEXY_INCDIR ?= /usr/local/include/plexy
BUILD_DIR    ?= build/xquadro

XQUADRO_SRC = src/xquadro
XQUADRO_HW  = $(XSERVER_DIR)/hw/xquadro

.PHONY: all install clean check-deps

all: check-deps
	@echo "--> Injecting xquadro into xserver tree..."
	@rsync -a --delete "$(XQUADRO_SRC)/" "$(XQUADRO_HW)/"
	@if ! grep -q "subdir('xquadro')" "$(XSERVER_DIR)/hw/meson.build"; then \
	    echo "subdir('xquadro')" >> "$(XSERVER_DIR)/hw/meson.build"; \
	fi
	@if ! grep -q "plexy_lib" "$(XSERVER_DIR)/meson_options.txt" 2>/dev/null; then \
	    printf "option('plexy_lib',    type : 'string', value : '$(PLEXY_LIB)',    description : 'Path to libplexy.so')\n" \
	        >> "$(XSERVER_DIR)/meson_options.txt"; \
	    printf "option('plexy_include', type : 'string', value : '$(PLEXY_INCDIR)', description : 'Path to plexy headers')\n" \
	        >> "$(XSERVER_DIR)/meson_options.txt"; \
	fi
	@mkdir -p "$(BUILD_DIR)"
	meson setup "$(BUILD_DIR)" "$(XSERVER_DIR)" \
	    -Dxwayland=false -Dxorg=false -Dxvfb=false -Dxquadro=true \
	    -Dplexy_lib="$(PLEXY_LIB)" -Dplexy_include="$(PLEXY_INCDIR)" \
	    --reconfigure 2>/dev/null || \
	meson setup "$(BUILD_DIR)" "$(XSERVER_DIR)" \
	    -Dxwayland=false -Dxorg=false -Dxvfb=false -Dxquadro=true \
	    -Dplexy_lib="$(PLEXY_LIB)" -Dplexy_include="$(PLEXY_INCDIR)"
	ninja -C "$(BUILD_DIR)"

install: all
	ninja -C "$(BUILD_DIR)" install

clean:
	rm -rf "$(BUILD_DIR)"

check-deps:
	@test -d "$(XSERVER_DIR)" || \
	    (echo "ERROR: xserver/ directory not found."; \
	     echo "       Clone it first: git clone https://gitlab.freedesktop.org/xorg/xserver.git"; \
	     exit 1)
	@command -v meson  >/dev/null || (echo "ERROR: meson not found. Install: apt install meson"; exit 1)
	@command -v ninja  >/dev/null || (echo "ERROR: ninja not found. Install: apt install ninja-build"; exit 1)
