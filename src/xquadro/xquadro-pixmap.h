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



#ifndef XQUADRO_PIXMAP_H
#define XQUADRO_PIXMAP_H

#include <pixmapstr.h>
#include <plexy/plexy.h>
#include <xquadro-config.h>

struct xqr_pixmap;

typedef void (*xqr_buffer_release_cb)(void *data);

void xqr_pixmap_set_private(PixmapPtr pixmap, struct xqr_pixmap *xqr_pixmap);
struct xqr_pixmap *xqr_pixmap_get(PixmapPtr pixmap);

PlexyBuffer *xqr_pixmap_get_plexy_buffer(PixmapPtr pixmap);

Bool xqr_pixmap_set_buffer_release_cb(PixmapPtr pixmap,
                                      xqr_buffer_release_cb func, void *data);
void xqr_pixmap_del_buffer_release_cb(PixmapPtr pixmap);

void xqr_pixmap_buffer_release_cb(PixmapPtr pixmap);

Bool xqr_pixmap_init(void);

static inline Bool xqr_is_client_pixmap(PixmapPtr pixmap) {
  return clients[CLIENT_ID(pixmap->drawable.id)] != serverClient;
}

#endif
