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



#ifndef XQUADRO_SCREEN_H
#define XQUADRO_SCREEN_H

#include <xquadro-config.h>

#include <X11/X.h>
#include <dix.h>
#include <stdio.h>
#include <unistd.h>

#include "xquadro-glamor.h"
#include "xquadro-output.h"
#include "xquadro-types.h"
#include "xquadro-window.h"

#include <plexy/plexy.h>

#define XQR_CLIENT_MAX_EMULATED_MODES 16

struct xqr_emulated_mode {
  uint32_t server_output_id;
  int32_t width;
  int32_t height;
  RRMode id;
  Bool from_vidmode;
};

struct xqr_client {
  struct xqr_emulated_mode emulated_modes[XQR_CLIENT_MAX_EMULATED_MODES];
};

struct xqr_screen {

  double width;
  double height;
  int depth;
  int global_surface_scale;
  int output_name_serial;

  ScreenPtr screen;
  int wm_client_id;
  int expecting_event;
  enum RootClipMode root_clip_mode;

  Bool active;
  int rootless;
  xqr_glamor_mode_flags glamor;
  int force_xrandr_emulation;

  PlexyConnection *plexy;
  int plexy_fd;

  struct xqr_output *fixed_output;
  struct xorg_list output_list;
  struct xorg_list window_list;
  struct xorg_list damage_window_list;
  int need_source_validate;

  DeviceIntPtr pointer;
  DeviceIntPtr keyboard;
  struct xqr_window *pointer_focus;
  struct xqr_window *keyboard_focus;

  Atom clipboard_atom;
  Atom targets_atom;
  Atom utf8_string_atom;
  Atom xqr_clipboard_atom;
  char *clipboard_text;
  WindowPtr clipboard_window;

  void *egl_display;
  void *egl_context;
  struct glamor_context *glamor_ctx;
  uint32_t num_formats;
  struct xqr_format *formats;

  ClipNotifyProcPtr ClipNotify;
  CreateScreenResourcesProcPtr CreateScreenResources;
  CloseScreenProcPtr CloseScreen;
  ConfigNotifyProcPtr ConfigNotify;
  RealizeWindowProcPtr RealizeWindow;
  UnrealizeWindowProcPtr UnrealizeWindow;
  DestroyWindowProcPtr DestroyWindow;
  XYToWindowProcPtr XYToWindow;
  SetWindowPixmapProcPtr SetWindowPixmap;
  ChangeWindowAttributesProcPtr ChangeWindowAttributes;
  ReparentWindowProcPtr ReparentWindow;
  ResizeWindowProcPtr ResizeWindow;
  MoveWindowProcPtr MoveWindow;
  SourceValidateProcPtr SourceValidate;
  SetShapeProcPtr SetShape;
};

struct xqr_client *xqr_client_get(ClientPtr client);
struct xqr_screen *xqr_screen_get(ScreenPtr screen);

struct xqr_output *xqr_screen_get_first_output(struct xqr_screen *xqr_screen);
struct xqr_output *
xqr_screen_get_fixed_or_first_output(struct xqr_screen *xqr_screen);
int xqr_screen_get_width(struct xqr_screen *xqr_screen);
int xqr_screen_get_height(struct xqr_screen *xqr_screen);

Bool xqr_close_screen(ScreenPtr screen);
Bool xqr_screen_init(ScreenPtr pScreen, int argc, char **argv);
void xqr_sync_events(struct xqr_screen *xqr_screen);

int xqr_screen_get_next_output_serial(struct xqr_screen *xqr_screen);
Bool xqr_screen_update_global_surface_scale(struct xqr_screen *xqr_screen);

#endif
