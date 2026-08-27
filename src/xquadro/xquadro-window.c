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

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <X11/X.h>
#include <X11/Xatom.h>

#include "compint.h"
#include "compositeext.h"
#include "damage.h"
#include "gcstruct.h"
#include "inputstr.h"
#include "propertyst.h"
#include "window.h"
#include "windowstr.h"

#include "xquadro-cursor.h"
#include "xquadro-input.h"
#include "xquadro-pixmap.h"
#include "xquadro-screen.h"
#include "xquadro-shm.h"
#include "xquadro-types.h"
#include "xquadro-window-buffers.h"
#include "xquadro-window.h"

#ifdef XQR_HAS_GLAMOR
#include "glamor.h"
#include "xquadro-glamor.h"
#endif

#include <plexy/plexy.h>

static DevPrivateKeyRec xqr_window_private_key;
static DevPrivateKeyRec xqr_wm_window_private_key;
static DevPrivateKeyRec xqr_damage_private_key;

static Atom xqr_window_id_atom = None;
static Atom xqr_net_wm_window_type_atom = None;
static Atom xqr_net_wm_type_desktop_atom = None;
static Atom xqr_net_wm_type_dock_atom = None;
static Atom xqr_net_wm_type_toolbar_atom = None;
static Atom xqr_net_wm_type_menu_atom = None;
static Atom xqr_net_wm_type_utility_atom = None;
static Atom xqr_net_wm_type_splash_atom = None;
static Atom xqr_net_wm_type_dialog_atom = None;
static Atom xqr_net_wm_type_dropdown_atom = None;
static Atom xqr_net_wm_type_popup_atom = None;
static Atom xqr_net_wm_type_tooltip_atom = None;
static Atom xqr_net_wm_type_notification_atom = None;
static Atom xqr_net_wm_type_combo_atom = None;
static Atom xqr_net_wm_type_dnd_atom = None;
static Atom xqr_net_wm_type_normal_atom = None;

static void ensure_window_id_atom(void) {
  if (xqr_window_id_atom == None)
    xqr_window_id_atom = MakeAtom("_XQUADRO_WINDOW_ID", 18, TRUE);
}

static void xqr_ensure_type_atoms(void) {
  if (xqr_net_wm_window_type_atom != None)
    return;
  xqr_net_wm_window_type_atom = MakeAtom("_NET_WM_WINDOW_TYPE", 19, TRUE);
  xqr_net_wm_type_desktop_atom =
      MakeAtom("_NET_WM_WINDOW_TYPE_DESKTOP", 27, TRUE);
  xqr_net_wm_type_dock_atom = MakeAtom("_NET_WM_WINDOW_TYPE_DOCK", 24, TRUE);
  xqr_net_wm_type_toolbar_atom =
      MakeAtom("_NET_WM_WINDOW_TYPE_TOOLBAR", 27, TRUE);
  xqr_net_wm_type_menu_atom = MakeAtom("_NET_WM_WINDOW_TYPE_MENU", 24, TRUE);
  xqr_net_wm_type_utility_atom =
      MakeAtom("_NET_WM_WINDOW_TYPE_UTILITY", 27, TRUE);
  xqr_net_wm_type_splash_atom =
      MakeAtom("_NET_WM_WINDOW_TYPE_SPLASH", 26, TRUE);
  xqr_net_wm_type_dialog_atom =
      MakeAtom("_NET_WM_WINDOW_TYPE_DIALOG", 26, TRUE);
  xqr_net_wm_type_dropdown_atom =
      MakeAtom("_NET_WM_WINDOW_TYPE_DROPDOWN_MENU", 33, TRUE);
  xqr_net_wm_type_popup_atom =
      MakeAtom("_NET_WM_WINDOW_TYPE_POPUP_MENU", 30, TRUE);
  xqr_net_wm_type_tooltip_atom =
      MakeAtom("_NET_WM_WINDOW_TYPE_TOOLTIP", 27, TRUE);
  xqr_net_wm_type_notification_atom =
      MakeAtom("_NET_WM_WINDOW_TYPE_NOTIFICATION", 32, TRUE);
  xqr_net_wm_type_combo_atom = MakeAtom("_NET_WM_WINDOW_TYPE_COMBO", 25, TRUE);
  xqr_net_wm_type_dnd_atom = MakeAtom("_NET_WM_WINDOW_TYPE_DND", 23, TRUE);
  xqr_net_wm_type_normal_atom =
      MakeAtom("_NET_WM_WINDOW_TYPE_NORMAL", 26, TRUE);
}

static uint32_t xqr_atom_to_plexy_type(Atom type_atom) {
  if (type_atom == xqr_net_wm_type_popup_atom)
    return PLEXY_WINDOW_TYPE_POPUP_MENU;
  if (type_atom == xqr_net_wm_type_dropdown_atom)
    return PLEXY_WINDOW_TYPE_DROPDOWN_MENU;
  if (type_atom == xqr_net_wm_type_menu_atom)
    return PLEXY_WINDOW_TYPE_MENU;
  if (type_atom == xqr_net_wm_type_tooltip_atom)
    return PLEXY_WINDOW_TYPE_TOOLTIP;
  if (type_atom == xqr_net_wm_type_combo_atom)
    return PLEXY_WINDOW_TYPE_COMBO;
  if (type_atom == xqr_net_wm_type_notification_atom)
    return PLEXY_WINDOW_TYPE_NOTIFICATION;
  if (type_atom == xqr_net_wm_type_splash_atom)
    return PLEXY_WINDOW_TYPE_SPLASH;
  if (type_atom == xqr_net_wm_type_dialog_atom)
    return PLEXY_WINDOW_TYPE_DIALOG;
  if (type_atom == xqr_net_wm_type_toolbar_atom)
    return PLEXY_WINDOW_TYPE_TOOLBAR;
  if (type_atom == xqr_net_wm_type_utility_atom)
    return PLEXY_WINDOW_TYPE_UTILITY;
  if (type_atom == xqr_net_wm_type_dock_atom)
    return PLEXY_WINDOW_TYPE_DOCK;
  if (type_atom == xqr_net_wm_type_desktop_atom)
    return PLEXY_WINDOW_TYPE_DESKTOP;
  if (type_atom == xqr_net_wm_type_dnd_atom)
    return PLEXY_WINDOW_TYPE_DND;
  return PLEXY_WINDOW_TYPE_NORMAL;
}

static uint32_t xqr_read_net_wm_type(WindowPtr window) {
  PropertyPtr prop;

  xqr_ensure_type_atoms();

  for (prop = wUserProps(window); prop; prop = prop->next) {
    if (prop->propertyName == xqr_net_wm_window_type_atom &&
        prop->type == XA_ATOM && prop->format == 32 && prop->size >= 1) {
      Atom type_atom = ((Atom *)prop->data)[0];
      return xqr_atom_to_plexy_type(type_atom);
    }
  }

  return PLEXY_WINDOW_TYPE_NORMAL;
}

static Bool xqr_type_is_popup(uint32_t plexy_type) {
  switch (plexy_type) {
  case PLEXY_WINDOW_TYPE_POPUP_MENU:
  case PLEXY_WINDOW_TYPE_DROPDOWN_MENU:
  case PLEXY_WINDOW_TYPE_MENU:
  case PLEXY_WINDOW_TYPE_TOOLTIP:
  case PLEXY_WINDOW_TYPE_COMBO:
  case PLEXY_WINDOW_TYPE_NOTIFICATION:
  case PLEXY_WINDOW_TYPE_DND:
    return TRUE;
  default:
    return FALSE;
  }
}

static Bool xqr_type_wants_no_decorations(uint32_t plexy_type) {
  switch (plexy_type) {
  case PLEXY_WINDOW_TYPE_POPUP_MENU:
  case PLEXY_WINDOW_TYPE_DROPDOWN_MENU:
  case PLEXY_WINDOW_TYPE_MENU:
  case PLEXY_WINDOW_TYPE_TOOLTIP:
  case PLEXY_WINDOW_TYPE_COMBO:
  case PLEXY_WINDOW_TYPE_NOTIFICATION:
  case PLEXY_WINDOW_TYPE_DND:
  case PLEXY_WINDOW_TYPE_SPLASH:
  case PLEXY_WINDOW_TYPE_DOCK:
  case PLEXY_WINDOW_TYPE_DESKTOP:
    return TRUE;
  default:
    return FALSE;
  }
}

static PlexyWindow *xqr_find_parent_plexy(WindowPtr window,
                                          struct xqr_screen *xqr_screen) {
  WindowPtr parent;
  struct xqr_window *parent_xqr;

  for (parent = window->parent; parent; parent = parent->parent) {
    parent_xqr = xqr_window_get(parent);
    if (parent_xqr && parent_xqr->plexy_window)
      return parent_xqr->plexy_window;
  }

  if (xqr_screen->keyboard_focus && xqr_screen->keyboard_focus->plexy_window)
    return xqr_screen->keyboard_focus->plexy_window;

  return NULL;
}

struct xqr_window *xqr_window_get(WindowPtr window) {
  return dixLookupPrivate(&window->devPrivates, &xqr_window_private_key);
}

struct xqr_window *xqr_window_from_window(WindowPtr window) {
  struct xqr_window *xqr_window;

  while (window) {
    xqr_window = xqr_window_get(window);
    if (xqr_window)
      return xqr_window;
    window = window->parent;
  }
  return NULL;
}

static void need_source_validate_dec(struct xqr_screen *xqr_screen) {
  xqr_screen->need_source_validate--;

  if (!xqr_screen->need_source_validate)
    xqr_screen->screen->SourceValidate = xqr_screen->SourceValidate;
}

static void xqr_source_validate(DrawablePtr drawable, int x, int y, int width,
                                int height, unsigned int sub_window_mode);

static void need_source_validate_inc(struct xqr_screen *xqr_screen) {
  if (!xqr_screen->need_source_validate) {
    ScreenPtr screen = xqr_screen->screen;

    xqr_screen->SourceValidate = screen->SourceValidate;
    screen->SourceValidate = xqr_source_validate;
  }

  xqr_screen->need_source_validate++;
}

static void xqr_source_validate(DrawablePtr drawable, int x, int y, int width,
                                int height, unsigned int sub_window_mode) {
  struct xqr_window *xqr_window;
  WindowPtr window, iterator;
  RegionRec region;
  BoxRec box;

  if (sub_window_mode != IncludeInferiors || drawable->type != DRAWABLE_WINDOW)
    return;

  window = (WindowPtr)drawable;
  xqr_window = xqr_window_from_window(window);
  if (!xqr_window || !xqr_window->surface_window_damage ||
      !RegionNotEmpty(xqr_window->surface_window_damage))
    return;

  for (iterator = xqr_window->toplevel;; iterator = iterator->firstChild) {
    if (iterator == xqr_window->surface_window)
      return;

    if (iterator == window)
      break;
  }

  box.x1 = x;
  box.y1 = y;
  box.x2 = x + width;
  box.y2 = y + height;
  RegionInit(&region, &box, 1);
  RegionIntersect(&region, &region, xqr_window->surface_window_damage);

  if (RegionNotEmpty(&region)) {
    ScreenPtr screen = drawable->pScreen;
    PixmapPtr dst_pix, src_pix;
    BoxPtr pbox;
    GCPtr pGC;
    int nbox;

    dst_pix = screen->GetWindowPixmap(window);
    pGC = GetScratchGC(dst_pix->drawable.depth, screen);
    if (!pGC)
      FatalError("GetScratchGC failed for depth %d", dst_pix->drawable.depth);
    ValidateGC(&dst_pix->drawable, pGC);

    src_pix = screen->GetWindowPixmap(xqr_window->surface_window);

    RegionSubtract(xqr_window->surface_window_damage,
                   xqr_window->surface_window_damage, &region);

    if (!RegionNotEmpty(xqr_window->surface_window_damage))
      need_source_validate_dec(xqr_window->xqr_screen);

#if defined(COMPOSITE)
    if (dst_pix->screen_x || dst_pix->screen_y)
      RegionTranslate(&region, -dst_pix->screen_x, -dst_pix->screen_y);
#endif

    pbox = RegionRects(&region);
    nbox = RegionNumRects(&region);
    while (nbox--) {
      (void)(*pGC->ops->CopyArea)(&src_pix->drawable, &dst_pix->drawable, pGC,
                                  pbox->x1, pbox->y1, pbox->x2 - pbox->x1,
                                  pbox->y2 - pbox->y1, pbox->x1, pbox->y1);
      pbox++;
    }
    FreeScratchGC(pGC);
  }

  RegionUninit(&region);
}

static DamagePtr window_get_damage(WindowPtr window) {
  return dixLookupPrivate(&window->devPrivates, &xqr_damage_private_key);
}

static void xqr_unregister_damage(struct xqr_window *xqr_window) {
  WindowPtr surface_window = xqr_window->surface_window;
  DamagePtr damage = window_get_damage(surface_window);

  if (!damage)
    return;

  DamageUnregister(damage);
  DamageDestroy(damage);
  dixSetPrivate(&surface_window->devPrivates, &xqr_damage_private_key, NULL);
}

static void xqr_window_schedule_redraw(struct xqr_window *xqr_window) {
  struct xqr_screen *xqr_screen = xqr_window->xqr_screen;
  DamagePtr damage = window_get_damage(xqr_window->surface_window);

  if (xqr_window->allow_commits && !xqr_window->frame_pending &&
      xorg_list_is_empty(&xqr_window->link_damage) && damage &&
      RegionNotEmpty(DamageRegion(damage))) {
    xorg_list_add(&xqr_window->link_damage, &xqr_screen->damage_window_list);
  }
}

static void xqr_damage_report(DamagePtr damage, RegionPtr region, void *data) {
  struct xqr_window *xqr_window = data;
  struct xqr_screen *xqr_screen = xqr_window->xqr_screen;

  if (xqr_window->surface_window_damage && RegionNotEmpty(region)) {
    if (!RegionNotEmpty(xqr_window->surface_window_damage))
      need_source_validate_inc(xqr_screen);

    RegionUnion(xqr_window->surface_window_damage,
                xqr_window->surface_window_damage, DamageRegion(damage));
  }

  xqr_window_schedule_redraw(xqr_window);
}

static void xqr_damage_destroy(DamagePtr damage, void *data) {
  struct xqr_window *xqr_window = data;

  if (xqr_window && xqr_window->surface_window)
    dixSetPrivate(&xqr_window->surface_window->devPrivates,
                  &xqr_damage_private_key, NULL);
}

Bool xqr_window_is_toplevel(WindowPtr window) {
  struct xqr_screen *xqr_screen;
  Bool *is_wm_window;

  if (!window->parent)
    return FALSE;

  xqr_screen = xqr_screen_get(window->drawable.pScreen);
  if (CLIENT_ID(window->drawable.id) == xqr_screen->wm_client_id)
    return FALSE;

  is_wm_window =
      dixLookupPrivate(&window->devPrivates, &xqr_wm_window_private_key);
  if (is_wm_window && *is_wm_window)
    return FALSE;

  if (!window->parent->parent)
    return TRUE;

  is_wm_window = dixLookupPrivate(&window->parent->devPrivates,
                                  &xqr_wm_window_private_key);
  return is_wm_window && *is_wm_window;
}

static void set_xquadro_window_id(struct xqr_window *xqr_window) {
  uint32_t window_id;
  CARD32 prop_value;

  ensure_window_id_atom();
  if (xqr_window_id_atom == None || !xqr_window->plexy_window)
    return;

  window_id = plexy_window_get_id(xqr_window->plexy_window);
  prop_value = (CARD32)window_id;

  dixChangeWindowProperty(serverClient, xqr_window->toplevel,
                          xqr_window_id_atom, XA_CARDINAL, 32, PropModeReplace,
                          1, &prop_value, FALSE);
}

void xqr_window_schedule_damage(struct xqr_window *xqr_window) {
  xqr_window_schedule_redraw(xqr_window);
}

void xqr_window_create_frame_callback(struct xqr_window *xqr_window) {

  xqr_window->frame_pending = TRUE;
}

static Bool xqr_window_attach_buffer(struct xqr_window *xqr_window) {
  PixmapPtr pixmap;
  PlexyBuffer *buf;
  ScreenPtr screen = xqr_window->xqr_screen->screen;
  Bool retried_export = FALSE;

  xqr_window_swap_pixmap(xqr_window, TRUE);

#ifdef XQR_HAS_GLAMOR

  if (xqr_window->xqr_screen->glamor)
    glamor_block_handler(screen);
#endif

  pixmap = (*screen->GetWindowPixmap)(xqr_window->surface_window);
  if (!pixmap)
    return FALSE;

retry_export:
  buf = xqr_pixmap_get_plexy_buffer(pixmap);
  if (!buf) {
#ifdef XQR_HAS_GLAMOR

    if (!retried_export && xqr_window->xqr_screen->glamor) {
      retried_export = TRUE;
      xqr_window_realloc_pixmap(xqr_window);
      pixmap = (*screen->GetWindowPixmap)(xqr_window->surface_window);
      if (pixmap)
        goto retry_export;
    }
#endif
    return FALSE;
  }

  if (plexy_window_attach(xqr_window->plexy_window, buf) < 0)
    return FALSE;

  return TRUE;
}

RegionPtr xqr_window_get_damage_region(struct xqr_window *xqr_window) {
  if (!xqr_window->surface_window_damage)
    xqr_window->surface_window_damage = RegionCreate(NullBox, 1);
  return xqr_window->surface_window_damage;
}

void xqr_window_post_damage(struct xqr_window *xqr_window) {
  ScreenPtr screen;
  DamagePtr window_damage;
  BoxRec damage_box = {0, 0, 0, 0};
  Bool have_damage = FALSE;

  if (!xqr_window->allow_commits || xqr_window->frame_pending)
    return;

  LogMessageVerb(X_INFO, 4, "Xquadro: post_damage: window %p %ux%u\n",
                 xqr_window->plexy_window,
                 (unsigned)xqr_window->toplevel->drawable.width,
                 (unsigned)xqr_window->toplevel->drawable.height);

  if (!xqr_window_attach_buffer(xqr_window)) {
    ErrorF("Xquadro: post_damage: attach_buffer FAILED\n");
    return;
  }

  screen = xqr_window->xqr_screen->screen;
  if (xqr_window->surface_window_damage &&
      RegionNotEmpty(xqr_window->surface_window_damage) &&
      screen->SourceValidate == xqr_source_validate) {
    WindowPtr toplevel = xqr_window->toplevel;

    xqr_source_validate(&toplevel->drawable, toplevel->drawable.x,
                        toplevel->drawable.y, toplevel->drawable.width,
                        toplevel->drawable.height, IncludeInferiors);
  }

  if (xqr_window->surface_window_damage) {
    if (RegionNotEmpty(xqr_window->surface_window_damage)) {
      damage_box = *RegionExtents(xqr_window->surface_window_damage);
      have_damage = TRUE;
      need_source_validate_dec(xqr_window->xqr_screen);
    }

    RegionDestroy(xqr_window->surface_window_damage);
    xqr_window->surface_window_damage = NULL;
  }

  window_damage = window_get_damage(xqr_window->surface_window);
  if (window_damage) {
    RegionPtr damage_region = DamageRegion(window_damage);
    if (RegionNotEmpty(damage_region)) {
      BoxPtr extents = RegionExtents(damage_region);

      if (have_damage) {

        if (extents->x1 < damage_box.x1)
          damage_box.x1 = extents->x1;
        if (extents->y1 < damage_box.y1)
          damage_box.y1 = extents->y1;
        if (extents->x2 > damage_box.x2)
          damage_box.x2 = extents->x2;
        if (extents->y2 > damage_box.y2)
          damage_box.y2 = extents->y2;
      } else {
        damage_box = *extents;
        have_damage = TRUE;
      }

      xqr_window->surface_window_damage = RegionCreate(NullBox, 1);
      RegionCopy(xqr_window->surface_window_damage, damage_region);
      DamageEmpty(window_damage);
    }
  }

  xqr_window_create_frame_callback(xqr_window);

  {
    int cx = 0, cy = 0;
    xqr_cursor_get_position(screen, &cx, &cy);

    if (have_damage) {
      int32_t dx = damage_box.x1;
      int32_t dy = damage_box.y1;
      int32_t dw = damage_box.x2 - damage_box.x1;
      int32_t dh = damage_box.y2 - damage_box.y1;
      LogMessageVerb(X_INFO, 4,
                     "Xquadro: commit_damage: dmg=[%d,%d %dx%d] win=%ux%u\n",
                     (int)dx, (int)dy, (int)dw, (int)dh,
                     (unsigned)xqr_window->toplevel->drawable.width,
                     (unsigned)xqr_window->toplevel->drawable.height);
      plexy_window_commit_damage(xqr_window->plexy_window, dx, dy, dw, dh, cx,
                                 cy, xqr_window->toplevel->drawable.width,
                                 xqr_window->toplevel->drawable.height);
    } else {

      LogMessageVerb(X_INFO, 4, "Xquadro: commit_full: win=%ux%u\n",
                     (unsigned)xqr_window->toplevel->drawable.width,
                     (unsigned)xqr_window->toplevel->drawable.height);
      plexy_window_commit_with_cursor(xqr_window->plexy_window, cx, cy,
                                      xqr_window->toplevel->drawable.width,
                                      xqr_window->toplevel->drawable.height);
    }
  }
}

void xqr_window_update_surface_window(struct xqr_window *xqr_window) {

  if (xqr_window->surface_window == xqr_window->toplevel)
    return;
  xqr_window->surface_window = xqr_window->toplevel;
}

static struct xqr_window *xqr_window_create(struct xqr_screen *xqr_screen,
                                            WindowPtr window) {
  struct xqr_window *xqr_window;
  DrawablePtr draw = &window->drawable;
  char title[64];
  DamagePtr damage;

  xqr_window = calloc(1, sizeof(*xqr_window));
  if (!xqr_window)
    return NULL;

  xqr_window->xqr_screen = xqr_screen;
  xqr_window->toplevel = window;
  xqr_window->surface_window = window;
  xqr_window->allow_commits = TRUE;

  xorg_list_init(&xqr_window->window_buffers_available);
  xorg_list_init(&xqr_window->window_buffers_unavailable);
  xorg_list_init(&xqr_window->xqr_output_list);
  xorg_list_init(&xqr_window->link_damage);
  xorg_list_init(&xqr_window->link_window);

  snprintf(title, sizeof(title), "X11:%08lx",
           (unsigned long)window->drawable.id);

  uint32_t plexy_type = xqr_read_net_wm_type(window);
  if (plexy_type == PLEXY_WINDOW_TYPE_NORMAL && window->overrideRedirect)
    plexy_type = PLEXY_WINDOW_TYPE_POPUP_MENU;

  Bool want_popup = xqr_type_is_popup(plexy_type) || window->overrideRedirect;

  if (want_popup) {
    PlexyWindow *parent_plexy = xqr_find_parent_plexy(window, xqr_screen);

    if (parent_plexy) {
      xqr_window->plexy_window = plexy_create_popup(
          xqr_screen->plexy, parent_plexy, draw->x, draw->y,
          (uint32_t)draw->width, (uint32_t)draw->height, PLEXY_POPUP_FLAG_NONE);
      xqr_window->is_popup = TRUE;
    }
  }

  if (!xqr_window->plexy_window) {
    int32_t win_x = draw->x;
    int32_t win_y = draw->y;

    if (!want_popup && win_x == 0 && win_y == 0 && draw->width > 0 &&
        draw->height > 0) {
      int scr_w = xqr_screen_get_width(xqr_screen);
      int scr_h = xqr_screen_get_height(xqr_screen);
      if (scr_w > 0 && scr_h > 0) {
        win_x = (scr_w - (int)draw->width) / 2;
        win_y = (scr_h - (int)draw->height) / 2;
        if (win_x < 0)
          win_x = 0;
        if (win_y < 0)
          win_y = 0;
      }
    }

    xqr_window->plexy_window = plexy_create_window(
        xqr_screen->plexy, win_x, win_y, (uint32_t)draw->width,
        (uint32_t)draw->height, title);
  }
  if (!xqr_window->plexy_window)
    goto err;

  plexy_window_set_type(xqr_window->plexy_window, plexy_type);

  if (!window->overrideRedirect && !xqr_window->is_popup &&
      !xqr_type_wants_no_decorations(plexy_type))
    plexy_window_set_decorations(xqr_window->plexy_window, TRUE);

  xqr_window_install_callbacks(xqr_window);

  set_xquadro_window_id(xqr_window);

  compRedirectWindow(serverClient, window, CompositeRedirectManual);

  dixSetPrivate(&window->devPrivates, &xqr_window_private_key, xqr_window);
  xorg_list_add(&xqr_window->link_window, &xqr_screen->window_list);

  damage =
      DamageCreate(xqr_damage_report, xqr_damage_destroy, DamageReportNonEmpty,
                   FALSE, xqr_screen->screen, xqr_window);
  if (!damage)
    goto err_destroy_win;

  DamageRegister(&window->drawable, damage);
  dixSetPrivate(&window->devPrivates, &xqr_damage_private_key, damage);

  return xqr_window;

err_destroy_win:
  if (dixLookupPrivate(&window->devPrivates, &xqr_window_private_key) ==
      xqr_window)
    dixSetPrivate(&window->devPrivates, &xqr_window_private_key, NULL);
  if (!xorg_list_is_empty(&xqr_window->link_window))
    xorg_list_del(&xqr_window->link_window);
  plexy_destroy_window(xqr_window->plexy_window);
err:
  free(xqr_window);
  return NULL;
}

static void xqr_window_destroy(struct xqr_window *xqr_window) {
  struct xqr_screen *xqr_screen = xqr_window->xqr_screen;

  if (xqr_window->destroying)
    return;
  xqr_window->destroying = TRUE;

  dixSetPrivate(&xqr_window->toplevel->devPrivates, &xqr_window_private_key,
                NULL);

  if (xqr_screen->pointer_focus == xqr_window)
    xqr_screen->pointer_focus = NULL;
  if (xqr_screen->keyboard_focus == xqr_window)
    xqr_screen->keyboard_focus = NULL;

  xorg_list_del(&xqr_window->link_damage);
  xorg_list_del(&xqr_window->link_window);

  xqr_window_buffers_dispose(xqr_window, TRUE);

  if (xqr_window->plexy_window) {

    plexy_window_set_callbacks(xqr_window->plexy_window, NULL, NULL);
    plexy_destroy_window(xqr_window->plexy_window);
    xqr_window->plexy_window = NULL;
  }

  if (xqr_window->surface_window_damage) {
    if (RegionNotEmpty(xqr_window->surface_window_damage))
      need_source_validate_dec(xqr_screen);
    RegionDestroy(xqr_window->surface_window_damage);
    xqr_window->surface_window_damage = NULL;
  }

  free(xqr_window);
}

Bool xqr_realize_window(WindowPtr window) {
  ScreenPtr screen = window->drawable.pScreen;
  struct xqr_screen *xqr_screen = xqr_screen_get(screen);
  Bool ret;

  screen->RealizeWindow = xqr_screen->RealizeWindow;
  ret = (*screen->RealizeWindow)(window);
  xqr_screen->RealizeWindow = screen->RealizeWindow;
  screen->RealizeWindow = xqr_realize_window;

  if (!ret)
    return FALSE;

  if (xqr_screen->rootless) {
    if (!window->parent) {

    } else if (xqr_window_is_toplevel(window)) {
      if (!xqr_window_get(window) && !xqr_window_create(xqr_screen, window)) {
        ErrorF("Xquadro: failed to create PlexyWindow for window %lu\n",
               (unsigned long)window->drawable.id);
      }
    }
  } else {

    if (!window->parent && !xqr_window_get(window))
      xqr_window_create(xqr_screen, window);
  }

  return TRUE;
}

Bool xqr_unrealize_window(WindowPtr window) {
  ScreenPtr screen = window->drawable.pScreen;
  struct xqr_screen *xqr_screen = xqr_screen_get(screen);
  struct xqr_window *xqr_window = xqr_window_get(window);
  Bool ret;

  if (xqr_window) {
    xqr_unregister_damage(xqr_window);
    xqr_window_destroy(xqr_window);
  }

  screen->UnrealizeWindow = xqr_screen->UnrealizeWindow;
  ret = (*screen->UnrealizeWindow)(window);
  xqr_screen->UnrealizeWindow = screen->UnrealizeWindow;
  screen->UnrealizeWindow = xqr_unrealize_window;

  return ret;
}

Bool xqr_destroy_window(WindowPtr window) {
  ScreenPtr screen = window->drawable.pScreen;
  struct xqr_screen *xqr_screen = xqr_screen_get(screen);
  struct xqr_window *xqr_window = xqr_window_get(window);
  Bool ret;

  if (xqr_window)
    xqr_unregister_damage(xqr_window);
  if (xqr_window)
    xqr_window_destroy(xqr_window);

  screen->DestroyWindow = xqr_screen->DestroyWindow;
  ret = (*screen->DestroyWindow)(window);
  xqr_screen->DestroyWindow = screen->DestroyWindow;
  screen->DestroyWindow = xqr_destroy_window;

  return ret;
}

Bool xqr_change_window_attributes(WindowPtr window, unsigned long mask) {
  ScreenPtr screen = window->drawable.pScreen;
  struct xqr_screen *xqr_screen = xqr_screen_get(screen);
  OtherClients *others;
  Bool ret;

  screen->ChangeWindowAttributes = xqr_screen->ChangeWindowAttributes;
  ret = (*screen->ChangeWindowAttributes)(window, mask);
  xqr_screen->ChangeWindowAttributes = screen->ChangeWindowAttributes;
  screen->ChangeWindowAttributes = xqr_change_window_attributes;

  if (window != screen->root || !(mask & CWEventMask))
    return ret;

  for (others = wOtherClients(window); others; others = others->next) {
    if (others->mask & (SubstructureRedirectMask | ResizeRedirectMask))
      xqr_screen->wm_client_id = CLIENT_ID(others->resource);
  }

  return ret;
}

void xqr_clip_notify(WindowPtr window, int dx, int dy) {
  ScreenPtr screen = window->drawable.pScreen;
  struct xqr_screen *xqr_screen = xqr_screen_get(screen);
  struct xqr_window *xqr_window;

  screen->ClipNotify = xqr_screen->ClipNotify;
  if (screen->ClipNotify)
    (*screen->ClipNotify)(window, dx, dy);
  xqr_screen->ClipNotify = screen->ClipNotify;
  screen->ClipNotify = xqr_clip_notify;

  xqr_window = xqr_window_from_window(window);
  if (xqr_window && xqr_window->plexy_window) {
    RegionPtr input_shape = wInputShape(window);

    if (input_shape && RegionNotEmpty(input_shape)) {
      int nbox = RegionNumRects(input_shape);
      BoxPtr boxes = RegionRects(input_shape);
      int i;

      plexy_window_set_input_region_mode(xqr_window->plexy_window, TRUE);
      for (i = 0; i < nbox; i++) {
        plexy_window_input_region_op(xqr_window->plexy_window, boxes[i].x1,
                                     boxes[i].y1, boxes[i].x2 - boxes[i].x1,
                                     boxes[i].y2 - boxes[i].y1, TRUE);
      }
    } else {

      plexy_window_set_input_region_mode(xqr_window->plexy_window, FALSE);
    }
  }
}

int xqr_config_notify(WindowPtr window, int x, int y, int width, int height,
                      int bw, WindowPtr sib) {
  ScreenPtr screen = window->drawable.pScreen;
  struct xqr_screen *xqr_screen = xqr_screen_get(screen);
  struct xqr_window *xqr_window = xqr_window_from_window(window);
  int ret = 0;

  if (xqr_window && xqr_window->surface_window_damage &&
      RegionNotEmpty(xqr_window->surface_window_damage) &&
      screen->SourceValidate == xqr_source_validate) {
    xqr_source_validate(&window->drawable, window->drawable.x,
                        window->drawable.y, window->drawable.width,
                        window->drawable.height, IncludeInferiors);
  }

  screen->ConfigNotify = xqr_screen->ConfigNotify;
  if (screen->ConfigNotify)
    ret = (*screen->ConfigNotify)(window, x, y, width, height, bw, sib);
  xqr_screen->ConfigNotify = screen->ConfigNotify;
  screen->ConfigNotify = xqr_config_notify;

  return ret;
}

void xqr_reparent_window(WindowPtr window, WindowPtr prior_parent) {
  ScreenPtr screen = window->drawable.pScreen;
  struct xqr_screen *xqr_screen = xqr_screen_get(screen);
  WindowPtr parent = window->parent;
  Bool *is_wm_window;

  screen->ReparentWindow = xqr_screen->ReparentWindow;
  if (screen->ReparentWindow)
    (*screen->ReparentWindow)(window, prior_parent);
  xqr_screen->ReparentWindow = screen->ReparentWindow;
  screen->ReparentWindow = xqr_reparent_window;

  if (!parent->parent)
    return;

  {
    ClientPtr current = GetCurrentClient();
    if (!current || current->index != xqr_screen->wm_client_id)
      return;
  }

  is_wm_window =
      dixLookupPrivate(&parent->devPrivates, &xqr_wm_window_private_key);
  if (is_wm_window)
    *is_wm_window = TRUE;
}

void xqr_resize_window(WindowPtr window, int x, int y, unsigned int width,
                       unsigned int height, WindowPtr sib) {
  ScreenPtr screen = window->drawable.pScreen;
  struct xqr_screen *xqr_screen = xqr_screen_get(screen);

  screen->ResizeWindow = xqr_screen->ResizeWindow;
  (*screen->ResizeWindow)(window, x, y, width, height, sib);
  xqr_screen->ResizeWindow = screen->ResizeWindow;
  screen->ResizeWindow = xqr_resize_window;
}

void xqr_move_window(WindowPtr window, int x, int y, WindowPtr next_sib,
                     VTKind kind) {
  ScreenPtr screen = window->drawable.pScreen;
  struct xqr_screen *xqr_screen = xqr_screen_get(screen);

  screen->MoveWindow = xqr_screen->MoveWindow;
  (*screen->MoveWindow)(window, x, y, next_sib, kind);
  xqr_screen->MoveWindow = screen->MoveWindow;
  screen->MoveWindow = xqr_move_window;
}

void xqr_window_set_window_pixmap(WindowPtr window, PixmapPtr pixmap) {
  ScreenPtr screen = window->drawable.pScreen;
  struct xqr_screen *xqr_screen = xqr_screen_get(screen);
  struct xqr_window *xqr_window;

  screen->SetWindowPixmap = xqr_screen->SetWindowPixmap;
  (*screen->SetWindowPixmap)(window, pixmap);
  xqr_screen->SetWindowPixmap = screen->SetWindowPixmap;
  screen->SetWindowPixmap = xqr_window_set_window_pixmap;

  xqr_window = xqr_window_from_window(window);
  if (xqr_window) {

    if (xqr_window->surface_window_damage &&
        RegionNotEmpty(xqr_window->surface_window_damage)) {
      need_source_validate_dec(xqr_screen);
      RegionEmpty(xqr_window->surface_window_damage);
    }
    xqr_window_update_surface_window(xqr_window);
  }
}

Bool xqr_window_init(void) {
  if (!dixRegisterPrivateKey(&xqr_window_private_key, PRIVATE_WINDOW, 0))
    return FALSE;
  if (!dixRegisterPrivateKey(&xqr_wm_window_private_key, PRIVATE_WINDOW,
                             sizeof(Bool)))
    return FALSE;
  if (!dixRegisterPrivateKey(&xqr_damage_private_key, PRIVATE_WINDOW, 0))
    return FALSE;
  return TRUE;
}
