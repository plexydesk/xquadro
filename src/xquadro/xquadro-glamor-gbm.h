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



#ifndef XQUADRO_GLAMOR_GBM_H
#define XQUADRO_GLAMOR_GBM_H

#include <pixmapstr.h>
#include <plexy/plexy.h>
#include <xquadro-config.h>

struct xqr_screen;
struct xqr_window;

#ifdef XQR_HAS_GLAMOR

struct xqr_gbm_private {
  char *device_name;
  struct gbm_device *gbm;
  int drm_fd;
  Bool fd_render_node;
  Bool dmabuf_capable;
};

struct xqr_gbm_private *xqr_glamor_gbm_get(ScreenPtr screen);

Bool xqr_glamor_gbm_init(struct xqr_screen *xqr_screen);
Bool xqr_glamor_gbm_init_screen(struct xqr_screen *xqr_screen);
void xqr_glamor_gbm_fini(struct xqr_screen *xqr_screen);
PixmapPtr
xqr_glamor_gbm_create_pixmap_for_window(struct xqr_window *xqr_window);

PlexyBuffer *xqr_glamor_export_pixmap_to_plexy(struct xqr_screen *xqr_screen,
                                               PixmapPtr pixmap);
void xqr_glamor_set_pixmap_plexy_buffer(PixmapPtr pixmap, PlexyBuffer *buf);
Bool xqr_glamor_pixmap_has_bo(PixmapPtr pixmap);
void xqr_glamor_cleanup_pixmap(PixmapPtr pixmap);

#endif

#endif
