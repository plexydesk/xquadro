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

#include "gcstruct.h"

#include "xquadro-pixmap.h"
#include "xquadro-screen.h"
#include "xquadro-window-buffers.h"
#include "xquadro-window.h"

#ifdef XQR_HAS_GLAMOR
#include "glamor.h"
#include "xquadro-glamor-gbm.h"
#include "xquadro-glamor.h"
#endif

#define BUFFER_TIMEOUT 1 * 1000

struct xqr_window_buffer {
  struct xqr_window *xqr_window;
  PixmapPtr pixmap;
  RegionPtr damage_region;
  int refcnt;
  uint32_t time;
  struct xorg_list link_buffer;
};

static void copy_pixmap_area(PixmapPtr src, PixmapPtr dst, int x, int y,
                             int width, int height) {
  GCPtr pGC = GetScratchGC(dst->drawable.depth, dst->drawable.pScreen);
  if (!pGC)
    FatalError("xquadro: GetScratchGC failed for depth %d",
               dst->drawable.depth);
  ValidateGC(&dst->drawable, pGC);
  (*pGC->ops->CopyArea)(&src->drawable, &dst->drawable, pGC, x, y, width,
                        height, x, y);
  FreeScratchGC(pGC);
}

static struct xqr_window_buffer *
xqr_window_buffer_new(struct xqr_window *xqr_window) {
  struct xqr_window_buffer *buf = calloc(1, sizeof(*buf));
  if (!buf)
    return NULL;

  buf->xqr_window = xqr_window;
  buf->damage_region = RegionCreate(NullBox, 1);
  buf->pixmap = NullPixmap;
  buf->refcnt = 1;
  xorg_list_init(&buf->link_buffer);
  return buf;
}

static void xqr_window_buffer_destroy_pixmap(struct xqr_window_buffer *buf) {
  ScreenPtr screen = buf->pixmap->drawable.pScreen;
  xqr_pixmap_del_buffer_release_cb(buf->pixmap);
  (*screen->DestroyPixmap)(buf->pixmap);
  buf->pixmap = NullPixmap;
}

static void xqr_window_buffer_dispose(struct xqr_window_buffer *buf) {
  RegionDestroy(buf->damage_region);
  if (buf->pixmap)
    xqr_window_buffer_destroy_pixmap(buf);
  xorg_list_del(&buf->link_buffer);
  free(buf);
}

static Bool xqr_window_buffer_maybe_dispose(struct xqr_window_buffer *buf) {
  assert(buf->refcnt > 0);
  if (--buf->refcnt)
    return FALSE;
  xqr_window_buffer_dispose(buf);
  return TRUE;
}

void xqr_window_buffer_add_damage_region(struct xqr_window *xqr_window) {
  RegionPtr region = xqr_window_get_damage_region(xqr_window);
  struct xqr_window_buffer *buf;

  xorg_list_for_each_entry(buf, &xqr_window->window_buffers_available,
                           link_buffer)
      RegionUnion(buf->damage_region, buf->damage_region, region);
  xorg_list_for_each_entry(buf, &xqr_window->window_buffers_unavailable,
                           link_buffer)
      RegionUnion(buf->damage_region, buf->damage_region, region);
}

static struct xqr_window_buffer *
xqr_window_buffer_get_available(struct xqr_window *xqr_window) {
  if (xorg_list_is_empty(&xqr_window->window_buffers_available))
    return NULL;
  return xorg_list_last_entry(&xqr_window->window_buffers_available,
                              struct xqr_window_buffer, link_buffer);
}

static CARD32 xqr_window_buffer_timer_callback(OsTimerPtr timer, CARD32 time,
                                               void *arg) {
  struct xqr_window *xqr_window = arg;
  struct xqr_window_buffer *buf, *tmp;

  xorg_list_for_each_entry_safe(buf, tmp, &xqr_window->window_buffers_available,
                                link_buffer) {
    if ((int64_t)(time - buf->time) >= BUFFER_TIMEOUT)
      xqr_window_buffer_maybe_dispose(buf);
  }

  if (!xorg_list_is_empty(&xqr_window->window_buffers_available)) {
    struct xqr_window_buffer *oldest =
        xorg_list_first_entry(&xqr_window->window_buffers_available,
                              struct xqr_window_buffer, link_buffer);
    return oldest->time + BUFFER_TIMEOUT - time;
  }
  return 0;
}

static void xqr_window_buffer_release_callback(void *data) {
  struct xqr_window_buffer *buf = data;
  struct xqr_window *xqr_win = buf->xqr_window;
  struct xqr_window_buffer *oldest;

  if (xqr_window_buffer_maybe_dispose(buf))
    return;

  xorg_list_del(&buf->link_buffer);
  xorg_list_append(&buf->link_buffer, &xqr_win->window_buffers_available);
  buf->time = (uint32_t)GetTimeInMillis();

  oldest = xorg_list_first_entry(&xqr_win->window_buffers_available,
                                 struct xqr_window_buffer, link_buffer);
  xqr_win->window_buffers_timer =
      TimerSet(xqr_win->window_buffers_timer, TimerAbsolute,
               oldest->time + BUFFER_TIMEOUT, &xqr_window_buffer_timer_callback,
               xqr_win);
}

void xqr_window_buffer_release(struct xqr_window_buffer *buf) {
  xqr_window_buffer_release_callback(buf);
}

void xqr_window_buffers_init(struct xqr_window *xqr_window) {
  xorg_list_init(&xqr_window->window_buffers_available);
  xorg_list_init(&xqr_window->window_buffers_unavailable);
}

static void xqr_window_buffer_disposal(struct xqr_window_buffer *buf,
                                       Bool force) {
  if (force)
    xqr_window_buffer_dispose(buf);
  else
    xqr_window_buffer_maybe_dispose(buf);
}

void xqr_window_buffers_dispose(struct xqr_window *xqr_window, Bool force) {
  struct xqr_window_buffer *buf, *tmp;

  xorg_list_for_each_entry_safe(buf, tmp, &xqr_window->window_buffers_available,
                                link_buffer) {
    xorg_list_del(&buf->link_buffer);
    xqr_window_buffer_disposal(buf, force);
  }
  xorg_list_for_each_entry_safe(
      buf, tmp, &xqr_window->window_buffers_unavailable, link_buffer) {
    xorg_list_del(&buf->link_buffer);
    xqr_window_buffer_disposal(buf, force);
  }
  if (xqr_window->window_buffers_timer) {
    TimerFree(xqr_window->window_buffers_timer);
    xqr_window->window_buffers_timer = NULL;
  }
}

void xqr_window_buffers_release_unavailable(struct xqr_window *xqr_window) {
  struct xqr_window_buffer *buf, *tmp;

  xorg_list_for_each_entry_safe(
      buf, tmp, &xqr_window->window_buffers_unavailable, link_buffer) {
    xqr_window_buffer_release(buf);
  }
}

struct xqr_pixmap_visit {
  PixmapPtr old;
  PixmapPtr new;
};

static int xqr_set_pixmap_visit_window(WindowPtr w, void *data) {
  ScreenPtr screen = w->drawable.pScreen;
  struct xqr_pixmap_visit *v = data;

  if (screen->GetWindowPixmap(w) == v->old) {
    screen->SetWindowPixmap(w, v->new);
    return WT_WALKCHILDREN;
  }
  return WT_DONTWALKCHILDREN;
}

static void xqr_window_set_pixmap(WindowPtr window, PixmapPtr pixmap) {
  ScreenPtr screen = window->drawable.pScreen;
  struct xqr_pixmap_visit visit;

  visit.old = screen->GetWindowPixmap(window);
  visit.new = pixmap;

#ifdef COMPOSITE
  pixmap->screen_x = visit.old->screen_x;
  pixmap->screen_y = visit.old->screen_y;
#endif

  TraverseTree(window, xqr_set_pixmap_visit_window, &visit);

  if (window == screen->root && screen->GetScreenPixmap(screen) == visit.old)
    screen->SetScreenPixmap(pixmap);
}

static PixmapPtr xqr_window_allocate_pixmap(struct xqr_window *xqr_window) {
  ScreenPtr screen = xqr_window->xqr_screen->screen;
  PixmapPtr win_px;

#ifdef XQR_HAS_GLAMOR
  if (xqr_window->xqr_screen->glamor) {
    PixmapPtr glamor_px = xqr_glamor_create_pixmap_for_window(xqr_window);
    if (glamor_px)
      return glamor_px;

    ErrorF("Xquadro: glamor: failed to allocate BO-backed window pixmap\n");
    return NullPixmap;
  }
#endif

  win_px = screen->GetWindowPixmap(xqr_window->surface_window);
  return screen->CreatePixmap(screen, win_px->drawable.width,
                              win_px->drawable.height, win_px->drawable.depth,
                              CREATE_PIXMAP_USAGE_BACKING_PIXMAP);
}

void xqr_window_realloc_pixmap(struct xqr_window *xqr_window) {
  PixmapPtr new_px = xqr_window_allocate_pixmap(xqr_window);
  WindowPtr window;
  ScreenPtr screen;
  PixmapPtr old_px;

  if (!new_px)
    return;

  window = xqr_window->surface_window;
  screen = window->drawable.pScreen;
  old_px = screen->GetWindowPixmap(window);

  copy_pixmap_area(old_px, new_px, 0, 0, old_px->drawable.width,
                   old_px->drawable.height);

  xqr_window_set_pixmap(xqr_window->surface_window, new_px);
  screen->DestroyPixmap(old_px);
}

PixmapPtr xqr_window_swap_pixmap(struct xqr_window *xqr_window,
                                 Bool handle_sync) {
  ScreenPtr screen = xqr_window->xqr_screen->screen;

#ifdef XQR_HAS_GLAMOR

  if (handle_sync)
    glamor_block_handler(screen);
#else
  (void)handle_sync;
#endif
  WindowPtr win = xqr_window->surface_window;
  struct xqr_window_buffer *buf;
  PixmapPtr win_px;

  win_px = (*screen->GetWindowPixmap)(win);

#ifdef XQR_HAS_GLAMOR
  if (xqr_window->xqr_screen->glamor && win_px &&
      !xqr_glamor_pixmap_has_bo(win_px)) {
    xqr_window_realloc_pixmap(xqr_window);
    win_px = (*screen->GetWindowPixmap)(win);
  }
#endif

  xqr_window_buffer_add_damage_region(xqr_window);

  buf = xqr_window_buffer_get_available(xqr_window);
  if (buf) {
#ifdef XQR_HAS_GLAMOR
    if (xqr_window->xqr_screen->glamor && buf->pixmap &&
        !xqr_glamor_pixmap_has_bo(buf->pixmap)) {
      PixmapPtr replacement = xqr_window_allocate_pixmap(xqr_window);
      if (replacement) {
        copy_pixmap_area(buf->pixmap, replacement, 0, 0,
                         buf->pixmap->drawable.width,
                         buf->pixmap->drawable.height);
        screen->DestroyPixmap(buf->pixmap);
        buf->pixmap = replacement;
      }
    }
#endif

    RegionPtr full_damage = buf->damage_region;
    int nBox = RegionNumRects(full_damage);

    if (nBox > 0) {
      BoxPtr pBox = RegionRects(full_damage);
      GCPtr pGC = GetScratchGC(buf->pixmap->drawable.depth,
                               buf->pixmap->drawable.pScreen);
      if (pGC) {
        ValidateGC(&buf->pixmap->drawable, pGC);
        while (nBox--) {
          int bx = pBox->x1 + win->borderWidth;
          int by = pBox->y1 + win->borderWidth;
          (*pGC->ops->CopyArea)(&win_px->drawable, &buf->pixmap->drawable, pGC,
                                bx, by, pBox->x2 - pBox->x1,
                                pBox->y2 - pBox->y1, bx, by);
          pBox++;
        }
        FreeScratchGC(pGC);
      }
    }

    RegionEmpty(buf->damage_region);
    xorg_list_del(&buf->link_buffer);
    xqr_window_set_pixmap(win, buf->pixmap);
  } else {
    buf = xqr_window_buffer_new(xqr_window);
    win_px->refcnt++;
    xqr_window_realloc_pixmap(xqr_window);

    if (!buf) {

      win_px->refcnt--;
      return win_px;
    }
  }

  buf->pixmap = win_px;
  buf->refcnt++;

  xqr_pixmap_set_buffer_release_cb(buf->pixmap,
                                   xqr_window_buffer_release_callback, buf);

  xorg_list_append(&buf->link_buffer, &xqr_window->window_buffers_unavailable);

  if (xorg_list_is_empty(&xqr_window->window_buffers_available))
    TimerCancel(xqr_window->window_buffers_timer);

  return buf->pixmap;
}
