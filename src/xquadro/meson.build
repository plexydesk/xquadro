# Copyright (c) 2024-2026 Siraj Razick
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# meson.build — Xquadro hw backend
#
# Included from xserver/hw/meson.build via the Makefile patch.
# Replaces xwayland's Wayland-client dependencies with libplexy.

# ── Configuration header ────────────────────────────────────────────────

xquadro_conf = configuration_data()
xquadro_conf.set('HAVE_DIX_CONFIG_H', true)

# ── Source list ──────────────────────────────────────────────────────────

xquadro_srcs = [
    'xquadro.c',
    'xquadro-clipboard.c',
    'xquadro-clipboard.h',
    'xquadro-cursor.c',
    'xquadro-cursor.h',
    'xquadro-cvt.c',
    'xquadro-cvt.h',
    'xquadro-dmabuf.c',
    'xquadro-dmabuf.h',
    'xquadro-input.c',
    'xquadro-input.h',
    'xquadro-output.c',
    'xquadro-output.h',
    'xquadro-present.c',
    'xquadro-present.h',
    'xquadro-pixmap.c',
    'xquadro-pixmap.h',
    'xquadro-screen.c',
    'xquadro-screen.h',
    'xquadro-shm.c',
    'xquadro-shm.h',
    'xquadro-types.h',
    'xquadro-vidmode.c',
    'xquadro-vidmode.h',
    'xquadro-window.c',
    'xquadro-window.h',
    'xquadro-window-buffers.c',
    'xquadro-window-buffers.h',
    '../../mi/miinitext.c',
    '../../mi/miinitext.h',
]

# ── libplexy dependency ──────────────────────────────────────────────────
#
# The Makefile passes:
#   -Dplexy_lib=<absolute-path-to-lib-dir>
#   -Dplexy_include=<absolute-path-to-include-dir>

plexy_lib_dir     = get_option('plexy_lib')
plexy_include_dir = get_option('plexy_include')

plexy_dep = declare_dependency(
    include_directories : include_directories(plexy_include_dir, is_system: true),
    link_args           : [
        '-L' + plexy_lib_dir,
        '-lplexy',
        '-Wl,-rpath,' + plexy_lib_dir,
    ],
)

# ── Dependencies ─────────────────────────────────────────────────────────

xquadro_dep = [
    plexy_dep,
    dependency('xcb'),
    dependency('xcb-composite'),
    dependency('xcb-render'),
    dependency('xcb-xfixes'),
    dependency('xkbcommon'),
    dependency('libxcvt'),
]

# ── Glamor / GPU path ────────────────────────────────────────────────────

xquadro_glamor_srcs = []
xquadro_glamor_libs = []
build_xquadro_glamor = false

if build_glamor  # defined by xserver top-level meson
    gbm_dep = dependency('gbm', required: false)
    if gbm_dep.found()
        xquadro_conf.set('XQR_HAS_GLAMOR', true)
        xquadro_glamor_srcs += [
            'xquadro-glamor.c',
            'xquadro-glamor.h',
            'xquadro-glamor-gbm.c',
            'xquadro-glamor-gbm.h',
        ]
        xquadro_dep += [gbm_dep, dependency('libdrm')]
        xquadro_glamor_libs += glamor
        build_xquadro_glamor = true
    endif
endif

# ── Generated config header ──────────────────────────────────────────────

xquadro_config_h = configure_file(
    input         : 'xquadro-config.h.in',
    output        : 'xquadro-config.h',
    configuration : xquadro_conf,
)
xquadro_srcs += xquadro_config_h
xquadro_srcs += xquadro_glamor_srcs

# ── Include directories ──────────────────────────────────────────────────

xquadro_inc = [ inc ]
if build_glx
    xquadro_inc += glx_inc
endif

# ── Executable ───────────────────────────────────────────────────────────

xquadro_server = executable(
    'Xquadro',
    xquadro_srcs,
    include_directories : xquadro_inc,
    dependencies        : [
        common_dep,
        epoll_dep,
        xquadro_dep,
    ],
    link_with           : [
        libxserver_main,
        xquadro_glamor_libs,
        libxserver_fb,
        libxserver,
        libxserver_present,
        libxserver_dri3,
        libxserver_xext_vidmode,
        libxserver_xkb_stubs,
        libxserver_xi_stubs,
        libxserver_glx,
        libglxvnd,
    ],
    install             : true,
    install_dir         : get_option('bindir'),
)
