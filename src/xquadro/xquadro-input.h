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



#ifndef XQUADRO_INPUT_H
#define XQUADRO_INPUT_H

#include <xquadro-config.h>

#include <dix.h>
#include <input.h>

#include "xquadro-types.h"

#define XQR_POINTER_NAME "Xquadro Pointer"
#define XQR_KEYBOARD_NAME "Xquadro Keyboard"

Bool xqr_input_init(struct xqr_screen *xqr_screen);
void xqr_input_fini(struct xqr_screen *xqr_screen);

void xqr_input_pointer_enter(struct xqr_window *xqr_window, int32_t x,
                             int32_t y);
void xqr_input_pointer_leave(struct xqr_window *xqr_window);
void xqr_input_pointer_motion(struct xqr_window *xqr_window, int32_t x,
                              int32_t y);
void xqr_input_pointer_button(struct xqr_window *xqr_window, uint32_t button,
                              Bool pressed, int32_t x, int32_t y);
void xqr_input_pointer_axis(struct xqr_window *xqr_window, int32_t axis,
                            int32_t value, int32_t discrete);

void xqr_input_key(struct xqr_window *xqr_window, uint32_t keycode,
                   Bool pressed, uint32_t modifiers);
void xqr_input_modifiers(struct xqr_window *xqr_window, uint32_t depressed,
                         uint32_t latched, uint32_t locked, uint32_t group);
void xqr_input_focus_in(struct xqr_window *xqr_window);
void xqr_input_focus_out(struct xqr_window *xqr_window);

void xqr_window_install_callbacks(struct xqr_window *xqr_window);

#endif
