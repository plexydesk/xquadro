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



#ifndef XQUADRO_CURSOR_H
#define XQUADRO_CURSOR_H

#include <dix.h>
#include <xquadro-config.h>

#include "xquadro-types.h"

Bool xqr_cursor_init(ScreenPtr screen);
void xqr_cursor_fini(ScreenPtr screen);

void xqr_screen_set_cursor(DeviceIntPtr device, ScreenPtr screen,
                           CursorPtr cursor, int x, int y);

void xqr_cursor_get_position(ScreenPtr screen, int *x, int *y);

#endif
