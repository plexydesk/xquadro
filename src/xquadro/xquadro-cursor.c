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



#include <xquadro-config.h>

#include "cursorstr.h"
#include "inputstr.h"
#include "mipointer.h"
#include "scrnintstr.h"

#include "xquadro-cursor.h"
#include "xquadro-screen.h"
#include "xquadro-types.h"

struct xqr_cursor {
  CursorPtr current;
  int x, y;
  Bool visible;
};

static DevPrivateKeyRec xqr_cursor_private_key;

static struct xqr_cursor *xqr_cursor_get(ScreenPtr screen) {
  return dixLookupPrivate(&screen->devPrivates, &xqr_cursor_private_key);
}

static Bool xqr_realize_cursor(DeviceIntPtr device, ScreenPtr screen,
                               CursorPtr cursor) {
  (void)device;
  (void)screen;

  return TRUE;
}

static Bool xqr_unrealize_cursor(DeviceIntPtr device, ScreenPtr screen,
                                 CursorPtr cursor) {
  struct xqr_cursor *xqr_cur = xqr_cursor_get(screen);
  (void)device;

  if (xqr_cur && xqr_cur->current == cursor)
    xqr_cur->current = NULL;

  return TRUE;
}

static void xqr_set_cursor(DeviceIntPtr device, ScreenPtr screen,
                           CursorPtr cursor, int x, int y) {
  struct xqr_cursor *xqr_cur = xqr_cursor_get(screen);
  (void)device;

  if (!xqr_cur)
    return;

  xqr_cur->current = cursor;
  xqr_cur->x = x;
  xqr_cur->y = y;
  xqr_cur->visible = (cursor != NULL);
}

static void xqr_move_cursor(DeviceIntPtr device, ScreenPtr screen, int x,
                            int y) {
  struct xqr_cursor *xqr_cur = xqr_cursor_get(screen);
  (void)device;

  if (!xqr_cur)
    return;

  xqr_cur->x = x;
  xqr_cur->y = y;
}

static Bool xqr_device_cursor_initialize(DeviceIntPtr device,
                                         ScreenPtr screen) {
  (void)device;
  (void)screen;
  return TRUE;
}

static void xqr_device_cursor_cleanup(DeviceIntPtr device, ScreenPtr screen) {
  (void)device;
  (void)screen;
}

static miPointerSpriteFuncRec xqr_pointer_sprite_funcs = {
    xqr_realize_cursor, xqr_unrealize_cursor,         xqr_set_cursor,
    xqr_move_cursor,    xqr_device_cursor_initialize, xqr_device_cursor_cleanup,
};

static Bool xqr_cursor_off_screen(ScreenPtr *ppScreen, int *x, int *y) {

  (void)ppScreen;
  (void)x;
  (void)y;
  return FALSE;
}

static void xqr_cross_screen(ScreenPtr screen, Bool entering) {
  (void)screen;
  (void)entering;
}

static void xqr_pointer_warp_cursor(DeviceIntPtr dev, ScreenPtr screen, int x,
                                    int y) {
  struct xqr_cursor *xqr_cur = xqr_cursor_get(screen);

  if (xqr_cur) {
    xqr_cur->x = x;
    xqr_cur->y = y;
  }

  miPointerWarpCursor(dev, screen, x, y);
}

static miPointerScreenFuncRec xqr_pointer_screen_funcs = {
    xqr_cursor_off_screen,
    xqr_cross_screen,
    xqr_pointer_warp_cursor,
};

Bool xqr_cursor_init(ScreenPtr screen) {
  struct xqr_cursor *xqr_cur;

  if (!dixRegisterPrivateKey(&xqr_cursor_private_key, PRIVATE_SCREEN, 0))
    return FALSE;

  xqr_cur = calloc(1, sizeof(*xqr_cur));
  if (!xqr_cur)
    return FALSE;

  dixSetPrivate(&screen->devPrivates, &xqr_cursor_private_key, xqr_cur);

  return miPointerInitialize(screen, &xqr_pointer_sprite_funcs,
                             &xqr_pointer_screen_funcs, TRUE);
}

void xqr_cursor_fini(ScreenPtr screen) {
  struct xqr_cursor *xqr_cur = xqr_cursor_get(screen);

  if (xqr_cur) {
    dixSetPrivate(&screen->devPrivates, &xqr_cursor_private_key, NULL);
    free(xqr_cur);
  }
}

void xqr_screen_set_cursor(DeviceIntPtr device, ScreenPtr screen,
                           CursorPtr cursor, int x, int y) {
  xqr_set_cursor(device, screen, cursor, x, y);
}

void xqr_cursor_get_position(ScreenPtr screen, int *x, int *y) {
  struct xqr_cursor *xqr_cur = xqr_cursor_get(screen);

  if (xqr_cur) {
    if (x)
      *x = xqr_cur->x;
    if (y)
      *y = xqr_cur->y;
  } else {
    if (x)
      *x = 0;
    if (y)
      *y = 0;
  }
}
