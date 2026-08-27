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

#include "present_priv.h"

#include <randrstr.h>

#ifdef XQR_HAS_GLAMOR
#include <glamor.h>
#endif

#include "xquadro-present.h"
#include "xquadro-screen.h"

static uint32_t
xqr_present_query_capabilities(present_screen_priv_ptr screen_priv) {
  return PresentCapabilityAsync;
}

static RRCrtcPtr xqr_present_get_crtc(present_screen_priv_ptr screen_priv,
                                      WindowPtr window) {
  rrScrPrivPtr rr_private;

  if (!window)
    return NULL;

  rr_private = rrGetScrPriv(window->drawable.pScreen);
  if (!rr_private || rr_private->numCrtcs == 0)
    return NULL;

  return rr_private->crtcs[0];
}

static Bool xqr_present_check_flip(RRCrtcPtr crtc, WindowPtr window,
                                   PixmapPtr pixmap, Bool sync_flip,
                                   RegionPtr valid, int16_t x_off,
                                   int16_t y_off, PresentFlipReason *reason) {
  return FALSE;
}

static void xqr_present_check_flip_window(WindowPtr window) {}

static Bool xqr_present_can_window_flip(WindowPtr window) { return FALSE; }

static void xqr_present_clear_window_flip(WindowPtr window) {}

static void xqr_present_flush(WindowPtr window) {
#ifdef XQR_HAS_GLAMOR
  ScreenPtr screen = window->drawable.pScreen;
  struct xqr_screen *xqr_screen = xqr_screen_get(screen);

  if (xqr_screen->glamor)
    glamor_block_handler(screen);
#endif
}

Bool xqr_present_init(ScreenPtr screen) {
  present_screen_priv_ptr screen_priv;

  if (!present_screen_register_priv_keys())
    return FALSE;

  screen_priv = present_screen_priv(screen);
  if (!screen_priv) {
    screen_priv = present_screen_priv_init(screen);
    if (!screen_priv)
      return FALSE;
  }

  if (!screen_priv->present_pixmap || !screen_priv->queue_vblank ||
      !screen_priv->abort_vblank || !screen_priv->re_execute) {

    present_scmd_init_mode_hooks(screen_priv);
  }
  if (!screen_priv->fake_interval)
    present_fake_screen_init(screen);

  screen_priv->query_capabilities = xqr_present_query_capabilities;
  screen_priv->get_crtc = xqr_present_get_crtc;

  screen_priv->check_flip = xqr_present_check_flip;
  screen_priv->check_flip_window = xqr_present_check_flip_window;
  screen_priv->can_window_flip = xqr_present_can_window_flip;
  screen_priv->clear_window_flip = xqr_present_clear_window_flip;

  screen_priv->flush = xqr_present_flush;

  return TRUE;
}
