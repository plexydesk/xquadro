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



#ifndef XQUADRO_WINDOW_BUFFERS_H
#define XQUADRO_WINDOW_BUFFERS_H

#include "xquadro-types.h"
#include <xquadro-config.h>

void xqr_window_buffer_add_damage_region(struct xqr_window *xqr_window);
void xqr_window_buffer_release(struct xqr_window_buffer *xqr_window_buffer);
void xqr_window_buffers_init(struct xqr_window *xqr_window);
void xqr_window_buffers_dispose(struct xqr_window *xqr_window, Bool force);
void xqr_window_buffers_release_unavailable(struct xqr_window *xqr_window);
void xqr_window_realloc_pixmap(struct xqr_window *xqr_window);
PixmapPtr xqr_window_swap_pixmap(struct xqr_window *xqr_window,
                                 Bool handle_sync);

#endif
