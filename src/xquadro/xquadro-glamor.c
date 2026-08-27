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

#ifdef XQR_HAS_GLAMOR

#define MESA_EGL_NO_X11_HEADERS
#define EGL_NO_X11

#include <glamor.h>
#include <glamor_context.h>
#include <glamor_egl.h>
#include <glamor_glx_provider.h>
#ifdef GLXEXT
#include "glx_extinit.h"
#endif
#include "fb.h"

#include "xquadro-dmabuf.h"
#include "xquadro-glamor-gbm.h"
#include "xquadro-glamor.h"
#include "xquadro-pixmap.h"
#include "xquadro-screen.h"
#include "xquadro-window-buffers.h"
#include "xquadro-window.h"

static Bool xqr_glamor_create_screen_resources(ScreenPtr screen) {
  struct xqr_screen *xqr_screen = xqr_screen_get(screen);
  int ret;

  screen->CreateScreenResources = xqr_screen->CreateScreenResources;
  ret = (*screen->CreateScreenResources)(screen);
  xqr_screen->CreateScreenResources = screen->CreateScreenResources;
  screen->CreateScreenResources = xqr_glamor_create_screen_resources;

  if (!ret)
    return ret;

  if (xqr_screen->rootless) {
    screen->devPrivate = fbCreatePixmap(screen, 0, 0, screen->rootDepth, 0);
  } else {
    screen->devPrivate = screen->CreatePixmap(
        screen, screen->width, screen->height, screen->rootDepth,
        CREATE_PIXMAP_USAGE_BACKING_PIXMAP);
  }

  SetRootClip(screen, xqr_screen->root_clip_mode);
  return screen->devPrivate != NULL;
}

static void glamor_egl_make_current(struct glamor_context *glamor_ctx) {
  eglMakeCurrent(glamor_ctx->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                 EGL_NO_CONTEXT);
  if (!eglMakeCurrent(glamor_ctx->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                      glamor_ctx->ctx))
    FatalError("Xquadro: failed to make EGL context current\n");
}

void glamor_egl_screen_init(ScreenPtr screen,
                            struct glamor_context *glamor_ctx) {
  struct xqr_screen *xqr_screen = xqr_screen_get(screen);

  glamor_ctx->ctx = (EGLContext)xqr_screen->egl_context;
  glamor_ctx->display = (EGLDisplay)xqr_screen->egl_display;
  xqr_screen->glamor_ctx = glamor_ctx;
  glamor_ctx->make_current = glamor_egl_make_current;
}

PlexyBuffer *
xqr_glamor_get_or_create_plexy_buffer(struct xqr_screen *xqr_screen,
                                      PixmapPtr pixmap) {
  PlexyBuffer *buf = xqr_glamor_pixmap_get_plexy_buffer(pixmap);

  if (!buf) {
    buf = xqr_glamor_export_pixmap_to_plexy(xqr_screen, pixmap);
    if (!buf) {
      ErrorF("Xquadro: glamor: failed to export pixmap to PlexyBuffer\n");
      return NULL;
    }
    xqr_glamor_set_pixmap_plexy_buffer(pixmap, buf);
  }

  return buf;
}

PixmapPtr xqr_glamor_create_pixmap_for_window(struct xqr_window *xqr_window) {
  return xqr_glamor_gbm_create_pixmap_for_window(xqr_window);
}

int glamor_egl_fd_name_from_pixmap(ScreenPtr screen, PixmapPtr pixmap,
                                   CARD16 *stride, CARD32 *size) {
  (void)screen;
  (void)pixmap;
  (void)stride;
  (void)size;
  return 0;
}

Bool xqr_glamor_destroy_pixmap(PixmapPtr pixmap) {
  if (pixmap->refcnt == 1) {
    xqr_glamor_cleanup_pixmap(pixmap);
  }

  return glamor_destroy_pixmap(pixmap);
}

Bool xqr_glamor_init(struct xqr_screen *xqr_screen) {
  ScreenPtr screen = xqr_screen->screen;

  if (!xqr_glamor_gbm_init(xqr_screen)) {
    ErrorF("Xquadro: glamor GBM init failed, falling back to SHM\n");
    return FALSE;
  }

  if (!glamor_init(screen, GLAMOR_USE_EGL_SCREEN)) {
    ErrorF("Xquadro: glamor_init failed\n");
    return FALSE;
  }

  if (!xqr_glamor_gbm_init_screen(xqr_screen)) {
    ErrorF("Xquadro: glamor GBM screen hook init failed\n");
    return FALSE;
  }

  xqr_screen->CreateScreenResources = screen->CreateScreenResources;
  screen->CreateScreenResources = xqr_glamor_create_screen_resources;

  screen->DestroyPixmap = xqr_glamor_destroy_pixmap;

  if (!xqr_dmabuf_init(xqr_screen)) {
    ErrorF("Xquadro: DRI3 init failed\n");
    return FALSE;
  }

  xqr_screen->glamor = TRUE;
  return TRUE;
}

void xqr_glamor_fini(struct xqr_screen *xqr_screen) {
  if (!xqr_screen->glamor)
    return;

  glamor_fini(xqr_screen->screen);
  xqr_glamor_gbm_fini(xqr_screen);
  xqr_screen->glamor = FALSE;
}

#endif
