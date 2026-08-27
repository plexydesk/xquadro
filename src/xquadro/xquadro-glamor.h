/*
 * Copyright (c) 2024-2026 Siraj Razick
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */



#ifndef XQUADRO_GLAMOR_H
#define XQUADRO_GLAMOR_H

#include <pixmapstr.h>
#include <plexy/plexy.h>
#include <xquadro-config.h>

struct xqr_screen;
struct xqr_window;

typedef enum {
  XQR_GLAMOR_NONE = 0,
  XQR_GLAMOR_GL = (1 << 0),
  XQR_GLAMOR_GLES = (1 << 1),
} xqr_glamor_mode_flags;

#ifdef XQR_HAS_GLAMOR

Bool xqr_glamor_init(struct xqr_screen *xqr_screen);
void xqr_glamor_fini(struct xqr_screen *xqr_screen);

Bool xqr_glamor_destroy_pixmap(PixmapPtr pixmap);
PixmapPtr xqr_glamor_create_pixmap_for_window(struct xqr_window *xqr_window);

PlexyBuffer *xqr_glamor_pixmap_get_plexy_buffer(PixmapPtr pixmap);

PlexyBuffer *
xqr_glamor_get_or_create_plexy_buffer(struct xqr_screen *xqr_screen,
                                      PixmapPtr pixmap);

#else

static inline Bool xqr_glamor_init(struct xqr_screen *xqr_screen) {
  return FALSE;
}

static inline void xqr_glamor_fini(struct xqr_screen *xqr_screen) {}

#endif

#endif
