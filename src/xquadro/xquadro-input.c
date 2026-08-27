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

#include <stdio.h>
#include <string.h>

#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/extensions/XKB.h>
#include <X11/keysym.h>

#include "window.h"
#include <dix.h>
#include <eventstr.h>
#include <exevents.h>
#include <input.h>
#include <inputstr.h>
#include <mi.h>
#include <randrstr.h>
#include <scrnintstr.h>
#include <xkbsrv.h>
#include <xserver-properties.h>

#include "xquadro-input.h"
#include "xquadro-output.h"
#include "xquadro-screen.h"
#include "xquadro-window-buffers.h"
#include "xquadro-window.h"

#include <plexy/plexy.h>

static ValuatorMask *s_motion_mask = NULL;
static ValuatorMask *s_button_mask = NULL;

static int linux_btn_to_x(uint32_t linux_btn) {
  switch (linux_btn) {
  case PLEXY_BTN_LEFT:
    return 1;
  case PLEXY_BTN_MIDDLE:
    return 2;
  case PLEXY_BTN_RIGHT:
    return 3;
  default:
    if (linux_btn >= PLEXY_BTN_LEFT && linux_btn <= PLEXY_BTN_LEFT + 8)
      return (int)(linux_btn - PLEXY_BTN_LEFT + 1);
    return 0;
  }
}

static int xqr_pointer_proc(DeviceIntPtr device, int what) {
  unsigned char map[33];
  Atom btn_labels[32] = {0};
  Atom axes_labels[2] = {0};
  int i;

  switch (what) {
  case DEVICE_INIT:
    for (i = 0; i < 33; i++)
      map[i] = i;
    btn_labels[0] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_LEFT);
    btn_labels[1] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_MIDDLE);
    btn_labels[2] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_RIGHT);
    axes_labels[0] = XIGetKnownProperty(AXIS_LABEL_PROP_ABS_X);
    axes_labels[1] = XIGetKnownProperty(AXIS_LABEL_PROP_ABS_Y);

    InitPointerDeviceStruct((DevicePtr)device, map, 32, btn_labels,
                            (PtrCtrlProcPtr)NoopDDA, GetMotionHistorySize(), 2,
                            axes_labels);
    return Success;

  case DEVICE_ON:
    device->enabled = TRUE;
    return Success;

  case DEVICE_OFF:
  case DEVICE_CLOSE:
    device->enabled = FALSE;
    return Success;
  }
  return BadMatch;
}

static int xqr_keyboard_proc(DeviceIntPtr device, int what) {
  XkbRMLVOSet rmlvo;
  rmlvo.rules = (char *)"evdev";
  rmlvo.model = (char *)"pc105";
  rmlvo.layout = (char *)"us";
  rmlvo.variant = (char *)"";
  rmlvo.options = (char *)"";

  switch (what) {
  case DEVICE_INIT:
    InitKeyboardDeviceStruct(device, &rmlvo, NULL, NULL);
    return Success;

  case DEVICE_ON:
    device->enabled = TRUE;
    return Success;

  case DEVICE_OFF:
  case DEVICE_CLOSE:
    device->enabled = FALSE;
    return Success;
  }
  return BadMatch;
}

static void inject_pointer_motion(struct xqr_screen *xqr_screen, int x, int y) {
  DeviceIntPtr dev = xqr_screen->pointer;

  if (!dev || !dev->enabled || !s_motion_mask)
    return;

  valuator_mask_set(s_motion_mask, 0, x);
  valuator_mask_set(s_motion_mask, 1, y);
  QueuePointerEvents(dev, MotionNotify, 0, POINTER_ABSOLUTE | POINTER_SCREEN,
                     s_motion_mask);
}

static void inject_pointer_button(struct xqr_screen *xqr_screen, int x_button,
                                  int state) {
  DeviceIntPtr dev = xqr_screen->pointer;

  if (!dev || !dev->enabled || x_button == 0 || !s_button_mask)
    return;

  QueuePointerEvents(dev, state ? ButtonPress : ButtonRelease, x_button,
                     POINTER_ABSOLUTE, s_button_mask);
}

void xqr_input_pointer_enter(struct xqr_window *xqr_window, int32_t x,
                             int32_t y) {
  struct xqr_screen *xqr_screen;
  int sx, sy;

  if (!xqr_window || !xqr_window->xqr_screen || !xqr_window->toplevel)
    return;

  xqr_screen = xqr_window->xqr_screen;
  xqr_screen->pointer_focus = xqr_window;

  sx = xqr_window->toplevel->drawable.x + x * xqr_screen->global_surface_scale;
  sy = xqr_window->toplevel->drawable.y + y * xqr_screen->global_surface_scale;
  inject_pointer_motion(xqr_screen, sx, sy);
}

void xqr_input_pointer_leave(struct xqr_window *xqr_window) {
  struct xqr_screen *xqr_screen;

  if (!xqr_window || !xqr_window->xqr_screen)
    return;

  xqr_screen = xqr_window->xqr_screen;
  if (xqr_screen->pointer_focus == xqr_window)
    xqr_screen->pointer_focus = NULL;
}

void xqr_input_pointer_motion(struct xqr_window *xqr_window, int32_t x,
                              int32_t y) {
  int sx, sy;

  if (!xqr_window || !xqr_window->xqr_screen || !xqr_window->toplevel)
    return;

  sx = xqr_window->toplevel->drawable.x +
       x * xqr_window->xqr_screen->global_surface_scale;
  sy = xqr_window->toplevel->drawable.y +
       y * xqr_window->xqr_screen->global_surface_scale;
  inject_pointer_motion(xqr_window->xqr_screen, sx, sy);
}

void xqr_input_pointer_button(struct xqr_window *xqr_window, uint32_t button,
                              Bool pressed, int32_t x, int32_t y) {
  int xbtn;

  if (!xqr_window || !xqr_window->xqr_screen)
    return;

  xbtn = linux_btn_to_x(button);
  inject_pointer_button(xqr_window->xqr_screen, xbtn, pressed ? 1 : 0);
}

void xqr_input_pointer_axis(struct xqr_window *xqr_window, int32_t axis,
                            int32_t value, int32_t discrete) {
  DeviceIntPtr dev;
  int button;

  if (!xqr_window || !xqr_window->xqr_screen || !s_button_mask)
    return;

  dev = xqr_window->xqr_screen->pointer;
  if (!dev || !dev->enabled)
    return;

  if (axis == 0) {
    button = (value > 0) ? 5 : 4;
  } else {
    button = (value > 0) ? 7 : 6;
  }

  QueuePointerEvents(dev, ButtonPress, button, POINTER_ABSOLUTE, s_button_mask);
  QueuePointerEvents(dev, ButtonRelease, button, POINTER_ABSOLUTE,
                     s_button_mask);
}

void xqr_input_key(struct xqr_window *xqr_window, uint32_t keycode,
                   Bool pressed, uint32_t modifiers) {
  DeviceIntPtr dev;

  if (!xqr_window || !xqr_window->xqr_screen)
    return;

  dev = xqr_window->xqr_screen->keyboard;
  if (!dev || !dev->enabled)
    return;

  QueueKeyboardEvents(dev, pressed ? KeyPress : KeyRelease, (int)(keycode + 8));
}

void xqr_input_modifiers(struct xqr_window *xqr_window, uint32_t depressed,
                         uint32_t latched, uint32_t locked, uint32_t group) {
  DeviceIntPtr dev;
  DeviceIntPtr master;
  DeviceIntPtr it;
  XkbStateRec old_state, *new_state;
  xkbStateNotify sn;
  CARD16 changed;

  if (!xqr_window || !xqr_window->xqr_screen)
    return;

  dev = xqr_window->xqr_screen->keyboard;

  if (!dev || !dev->enabled || !dev->key || !dev->key->xkbInfo)
    return;

  mieqProcessInputEvents();

  master = GetMaster(dev, MASTER_KEYBOARD);
  for (it = inputInfo.devices; it; it = it->next) {
    if (it != dev && it != master)
      continue;
    if (!it->enabled || !it->key || !it->key->xkbInfo)
      continue;

    old_state = it->key->xkbInfo->state;
    new_state = &it->key->xkbInfo->state;

    new_state->base_group = 0;
    new_state->latched_group = 0;
    new_state->locked_group = group & XkbAllGroupsMask;
    new_state->base_mods = depressed & XkbAllModifiersMask;
    new_state->latched_mods = latched & XkbAllModifiersMask;
    new_state->locked_mods = locked & XkbAllModifiersMask;

    XkbComputeDerivedState(it->key->xkbInfo);

    changed = XkbStateChangedFlags(&old_state, new_state);
    if (!changed)
      continue;

    sn.keycode = 0;
    sn.eventType = 0;
    sn.requestMajor = XkbReqCode;
    sn.requestMinor = X_kbLatchLockState;
    sn.changed = changed;
    XkbSendStateNotify(it, &sn);
  }
}

void xqr_input_focus_in(struct xqr_window *xqr_window) {
  struct xqr_screen *xqr_screen;

  if (!xqr_window || !xqr_window->xqr_screen)
    return;

  xqr_screen = xqr_window->xqr_screen;
  xqr_screen->keyboard_focus = xqr_window;

  if (!xqr_screen->keyboard || !xqr_screen->keyboard->enabled)
    return;
  if (!xqr_window->toplevel)
    return;

  SetInputFocus(serverClient, xqr_screen->keyboard,
                xqr_window->toplevel->drawable.id, RevertToPointerRoot,
                CurrentTime, FALSE);
}

void xqr_input_focus_out(struct xqr_window *xqr_window) {
  struct xqr_screen *xqr_screen;

  if (!xqr_window || !xqr_window->xqr_screen)
    return;

  xqr_screen = xqr_window->xqr_screen;
  if (xqr_screen->keyboard_focus == xqr_window)
    xqr_screen->keyboard_focus = NULL;
}

static void cb_configure(PlexyWindow *win, uint32_t width, uint32_t height,
                         uint32_t state_flags, void *user_data) {
  struct xqr_window *xqr_window = user_data;
  (void)win;
  (void)state_flags;

  xqr_window->allow_commits = TRUE;

  if (xqr_window->toplevel &&
      ((uint32_t)xqr_window->toplevel->drawable.width != width ||
       (uint32_t)xqr_window->toplevel->drawable.height != height) &&
      width > 0 && height > 0) {
    XID values[2] = {(XID)width, (XID)height};
    ConfigureWindow(xqr_window->toplevel, CWWidth | CWHeight, values,
                    serverClient);
  }
}

static void cb_pointer_enter(PlexyWindow *win, int32_t x, int32_t y,
                             void *user_data) {
  xqr_input_pointer_enter((struct xqr_window *)user_data, x, y);
}

static void cb_pointer_leave(PlexyWindow *win, void *user_data) {
  xqr_input_pointer_leave((struct xqr_window *)user_data);
}

static void cb_pointer_motion(PlexyWindow *win, int32_t x, int32_t y,
                              void *user_data) {
  xqr_input_pointer_motion((struct xqr_window *)user_data, x, y);
}

static void cb_pointer_button(PlexyWindow *win, uint32_t button, bool pressed,
                              int32_t x, int32_t y, void *user_data) {
  xqr_input_pointer_button((struct xqr_window *)user_data, button,
                           (Bool)pressed, x, y);
}

static void cb_pointer_axis(PlexyWindow *win, int32_t axis, int32_t value,
                            int32_t discrete, void *user_data) {
  xqr_input_pointer_axis((struct xqr_window *)user_data, axis, value, discrete);
}

static void cb_key(PlexyWindow *win, uint32_t keycode, bool pressed,
                   uint32_t modifiers, void *user_data) {
  xqr_input_key((struct xqr_window *)user_data, keycode, (Bool)pressed,
                modifiers);
}

static void cb_modifiers(PlexyWindow *win, uint32_t depressed, uint32_t latched,
                         uint32_t locked, uint32_t group, void *user_data) {
  xqr_input_modifiers((struct xqr_window *)user_data, depressed, latched,
                      locked, group);
}

static void cb_focus_in(PlexyWindow *win, void *user_data) {
  xqr_input_focus_in((struct xqr_window *)user_data);
}

static void cb_focus_out(PlexyWindow *win, void *user_data) {
  xqr_input_focus_out((struct xqr_window *)user_data);
}

static void cb_frame_done(PlexyWindow *win, void *user_data) {
  struct xqr_window *xqr_window = user_data;
  xqr_window->frame_pending = FALSE;

  xqr_window_buffers_release_unavailable(xqr_window);

  xqr_window_schedule_damage(xqr_window);
}

static void cb_close(PlexyWindow *win, void *user_data) {
  struct xqr_window *xqr_window = user_data;
  (void)win;

  if (!xqr_window || !xqr_window->toplevel)
    return;

  UnmapWindow(xqr_window->toplevel, FALSE);
}

static void cb_scale_changed(PlexyWindow *win, float scale_factor,
                             uint32_t buffer_scale, void *user_data) {
  struct xqr_window *xqr_window = user_data;
  struct xqr_screen *xqr_screen;
  (void)win;

  if (!xqr_window || !xqr_window->xqr_screen)
    return;

  xqr_screen = xqr_window->xqr_screen;

  if (buffer_scale > 0 &&
      (int)buffer_scale != xqr_screen->global_surface_scale) {
    xqr_screen->global_surface_scale = (int)buffer_scale;

    if (xqr_window->plexy_window)
      plexy_window_set_buffer_scale(xqr_window->plexy_window, buffer_scale);
    xqr_window_realloc_pixmap(xqr_window);
  }
}

static void cb_enter_output(PlexyWindow *win, uint32_t output_id,
                            void *user_data) {
  struct xqr_window *xqr_window = user_data;
  struct xqr_screen *xqr_screen;
  struct xqr_output *xqr_output;
  struct xqr_window_output *entry;
  (void)win;

  if (!xqr_window || !xqr_window->xqr_screen)
    return;

  xqr_screen = xqr_window->xqr_screen;

  xorg_list_for_each_entry(xqr_output, &xqr_screen->output_list, link) {
    if (xqr_output->plexy_output_id == output_id)
      goto found;
  }
  return;

found:

  xorg_list_for_each_entry(entry, &xqr_window->xqr_output_list, link) {
    if (entry->xqr_output == xqr_output)
      return;
  }

  entry = calloc(1, sizeof(*entry));
  if (!entry)
    return;

  entry->xqr_output = xqr_output;
  xorg_list_append(&entry->link, &xqr_window->xqr_output_list);
}

static void cb_leave_output(PlexyWindow *win, uint32_t output_id,
                            void *user_data) {
  struct xqr_window *xqr_window = user_data;
  struct xqr_window_output *entry, *tmp;
  (void)win;

  if (!xqr_window)
    return;

  xorg_list_for_each_entry_safe(entry, tmp, &xqr_window->xqr_output_list,
                                link) {
    if (entry->xqr_output->plexy_output_id == output_id) {
      xorg_list_del(&entry->link);
      free(entry);
      return;
    }
  }
}

static void cb_menu_action(PlexyWindow *win, uint32_t item_id,
                           void *user_data) {
  struct xqr_window *xqr_window = user_data;
  (void)win;

  if (!xqr_window || !xqr_window->toplevel)
    return;

  {
    static Atom xqr_menu_atom = None;
    xEvent ev;

    if (xqr_menu_atom == None)
      xqr_menu_atom = MakeAtom("_XQUADRO_MENU_ACTION", 20, TRUE);

    memset(&ev, 0, sizeof(ev));
    ev.u.u.type = ClientMessage;
    ev.u.clientMessage.u.l.type = xqr_menu_atom;
    ev.u.clientMessage.u.l.longs0 = (int32_t)item_id;
    ev.u.clientMessage.window = xqr_window->toplevel->drawable.id;

    DeliverEvents(xqr_window->toplevel, &ev, 1, NullWindow);
  }
}

static const PlexyWindowCallbacks xqr_input_callbacks = {
    .configure = cb_configure,
    .close = cb_close,
    .pointer_enter = cb_pointer_enter,
    .pointer_leave = cb_pointer_leave,
    .pointer_motion = cb_pointer_motion,
    .pointer_button = cb_pointer_button,
    .pointer_axis = cb_pointer_axis,
    .key = cb_key,
    .modifiers = cb_modifiers,
    .focus_in = cb_focus_in,
    .focus_out = cb_focus_out,
    .frame_done = cb_frame_done,
    .menu_action = cb_menu_action,
    .scale_changed = cb_scale_changed,
    .enter_output = cb_enter_output,
    .leave_output = cb_leave_output,
};

void xqr_window_install_callbacks(struct xqr_window *xqr_window) {
  plexy_window_set_callbacks(xqr_window->plexy_window, &xqr_input_callbacks,
                             xqr_window);
}

Bool xqr_input_init(struct xqr_screen *xqr_screen) {
  static Atom pointer_atom, keyboard_atom;

  if (!s_motion_mask)
    s_motion_mask = valuator_mask_new(2);
  if (!s_button_mask)
    s_button_mask = valuator_mask_new(0);
  if (!s_motion_mask || !s_button_mask) {
    ErrorF("Xquadro: failed to allocate ValuatorMask\n");
    return FALSE;
  }

  xqr_screen->pointer = AddInputDevice(serverClient, xqr_pointer_proc, TRUE);
  if (!xqr_screen->pointer) {
    ErrorF("Xquadro: failed to create pointer device\n");
    return FALSE;
  }
  if (!pointer_atom)
    pointer_atom = MakeAtom("xquadro-pointer", 15, TRUE);
  AssignTypeAndName(xqr_screen->pointer, pointer_atom, "xquadro pointer");

  xqr_screen->keyboard = AddInputDevice(serverClient, xqr_keyboard_proc, TRUE);
  if (!xqr_screen->keyboard) {
    ErrorF("Xquadro: failed to create keyboard device\n");
    return FALSE;
  }
  if (!keyboard_atom)
    keyboard_atom = MakeAtom("xquadro-keyboard", 16, TRUE);
  AssignTypeAndName(xqr_screen->keyboard, keyboard_atom, "xquadro keyboard");

  ActivateDevice(xqr_screen->pointer, TRUE);
  ActivateDevice(xqr_screen->keyboard, TRUE);
  EnableDevice(xqr_screen->pointer, TRUE);
  EnableDevice(xqr_screen->keyboard, TRUE);

  return TRUE;
}

void xqr_input_fini(struct xqr_screen *xqr_screen) {

  xqr_screen->pointer = NULL;
  xqr_screen->keyboard = NULL;

  if (s_motion_mask) {
    valuator_mask_free(&s_motion_mask);
    s_motion_mask = NULL;
  }
  if (s_button_mask) {
    valuator_mask_free(&s_button_mask);
    s_button_mask = NULL;
  }
}

void ProcessInputEvents(void) { mieqProcessInputEvents(); }

void DDXRingBell(int volume, int pitch, int duration) {
  (void)volume;
  (void)pitch;
  (void)duration;
}

void InitInput(int argc, char *argv[]) {
  ScreenPtr pScreen;
  struct xqr_screen *xqr_screen;

  (void)argc;
  (void)argv;

  mieqInit();

  if (screenInfo.numScreens < 1)
    return;

  pScreen = screenInfo.screens[0];
  xqr_screen = xqr_screen_get(pScreen);

  if (!xqr_input_init(xqr_screen))
    FatalError("Xquadro: InitInput: failed to create input devices\n");
}

void CloseInput(void) {
  if (screenInfo.numScreens > 0) {
    ScreenPtr pScreen = screenInfo.screens[0];
    xqr_input_fini(xqr_screen_get(pScreen));
  }
  mieqFini();
}
