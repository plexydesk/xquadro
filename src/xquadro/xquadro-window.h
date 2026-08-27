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



#ifndef XQUADRO_WINDOW_H
#define XQUADRO_WINDOW_H

#include <xquadro-config.h>

#include <X11/X.h>
#include <dix.h>
#include <validate.h>

#include "xquadro-types.h"
#include <plexy/plexy.h>

struct xqr_window_output {
  struct xorg_list link;
  struct xqr_output *xqr_output;
};

struct xqr_window {
  struct xqr_screen *xqr_screen;

  PlexyWindow *plexy_window;
  Bool is_popup;

  WindowPtr toplevel;
  WindowPtr surface_window;
  RegionPtr surface_window_damage;

  Bool allow_commits;
  Bool frame_pending;
  Bool destroying;

  struct xorg_list window_buffers_available;
  struct xorg_list window_buffers_unavailable;
  OsTimerPtr window_buffers_timer;

  struct xorg_list xqr_output_list;

  struct xorg_list link_damage;
  struct xorg_list link_window;
};

struct xqr_window *xqr_window_get(WindowPtr window);
struct xqr_window *xqr_window_from_window(WindowPtr window);

Bool xqr_window_is_toplevel(WindowPtr window);
void xqr_window_update_surface_window(struct xqr_window *xqr_window);
void xqr_window_set_window_pixmap(WindowPtr window, PixmapPtr pixmap);

void xqr_window_post_damage(struct xqr_window *xqr_window);
RegionPtr xqr_window_get_damage_region(struct xqr_window *xqr_window);
void xqr_window_create_frame_callback(struct xqr_window *xqr_window);

Bool xqr_realize_window(WindowPtr window);
Bool xqr_unrealize_window(WindowPtr window);
Bool xqr_change_window_attributes(WindowPtr window, unsigned long mask);
void xqr_clip_notify(WindowPtr window, int dx, int dy);
int xqr_config_notify(WindowPtr window, int x, int y, int width, int height,
                      int bw, WindowPtr sib);
void xqr_reparent_window(WindowPtr window, WindowPtr prior_parent);
void xqr_resize_window(WindowPtr window, int x, int y, unsigned int width,
                       unsigned int height, WindowPtr sib);
void xqr_move_window(WindowPtr window, int x, int y, WindowPtr next_sib,
                     VTKind kind);
Bool xqr_destroy_window(WindowPtr window);

Bool xqr_window_init(void);

void xqr_window_schedule_damage(struct xqr_window *xqr_window);

#endif
