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

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <drm_fourcc.h>
#include <xf86drm.h>

#include <dix.h>
#include <dri3.h>
#include <glamor.h>
#include <glamor_egl.h>

#include "xquadro-dmabuf.h"
#include "xquadro-glamor-gbm.h"
#include "xquadro-screen.h"

static int xqr_dri3_open_client(ClientPtr client, ScreenPtr screen,
                                RRProviderPtr provider, int *pfd) {
  struct xqr_gbm_private *gbm = xqr_glamor_gbm_get(screen);
  int fd;

  (void)client;
  (void)provider;

  if (!gbm)
    return BadAlloc;

  fd = open(gbm->device_name, O_RDWR | O_CLOEXEC);
  if (fd < 0) {
    ErrorF("Xquadro: dri3 open_client: open(%s): %s\n", gbm->device_name,
           strerror(errno));
    return BadAlloc;
  }

  *pfd = fd;
  return Success;
}

static int xqr_dri3_get_formats(ScreenPtr screen, CARD32 *num_formats,
                                CARD32 **formats) {
  struct xqr_screen *xqr_screen = xqr_screen_get(screen);
  EGLDisplay dpy;
  EGLint n = 0;
  EGLint *egl_formats = NULL;
  int i;

  *num_formats = 0;
  *formats = NULL;

  if (!xqr_screen || !xqr_screen->egl_display)
    return FALSE;

  dpy = (EGLDisplay)xqr_screen->egl_display;
  if (!epoxy_has_egl_extension(dpy, "EGL_EXT_image_dma_buf_import"))
    return FALSE;

  if (!eglQueryDmaBufFormatsEXT(dpy, 0, NULL, &n) || n <= 0)
    return FALSE;

  egl_formats = calloc((size_t)n, sizeof(EGLint));
  if (!egl_formats)
    return FALSE;

  if (!eglQueryDmaBufFormatsEXT(dpy, n, egl_formats, &n)) {
    free(egl_formats);
    return FALSE;
  }

  *formats = calloc((size_t)n, sizeof(CARD32));
  if (!*formats) {
    free(egl_formats);
    return FALSE;
  }

  for (i = 0; i < n; i++)
    (*formats)[i] = (CARD32)egl_formats[i];

  *num_formats = (CARD32)n;
  free(egl_formats);
  return TRUE;
}

static int xqr_dri3_get_modifiers(ScreenPtr screen, uint32_t format,
                                  uint32_t *num_modifiers,
                                  uint64_t **modifiers) {
  struct xqr_screen *xqr_screen = xqr_screen_get(screen);
  EGLDisplay dpy;
  EGLint n = 0;

  *num_modifiers = 0;
  *modifiers = NULL;

  if (!xqr_screen || !xqr_screen->egl_display)
    return FALSE;

  dpy = (EGLDisplay)xqr_screen->egl_display;
  if (!epoxy_has_egl_extension(dpy, "EGL_EXT_image_dma_buf_import_modifiers"))
    return FALSE;

  if (!eglQueryDmaBufModifiersEXT(dpy, format, 0, NULL, NULL, &n) || n <= 0)
    return FALSE;

  *modifiers = calloc((size_t)n, sizeof(uint64_t));
  if (!*modifiers)
    return FALSE;

  if (!eglQueryDmaBufModifiersEXT(dpy, format, n, (EGLuint64KHR *)*modifiers,
                                  NULL, &n)) {
    free(*modifiers);
    *modifiers = NULL;
    return FALSE;
  }

  *num_modifiers = (uint32_t)n;
  return TRUE;
}

static int xqr_dri3_get_drawable_modifiers(DrawablePtr draw, uint32_t format,
                                           uint32_t *num_modifiers,
                                           uint64_t **modifiers) {
  return xqr_dri3_get_modifiers(draw->pScreen, format, num_modifiers,
                                modifiers);
}

static dri3_screen_info_rec xqr_dri3_info = {
    .version = 2,
    .open = NULL,
    .open_client = xqr_dri3_open_client,

    .pixmap_from_fds = glamor_pixmap_from_fds,
    .fds_from_pixmap = glamor_fds_from_pixmap,
    .get_formats = xqr_dri3_get_formats,
    .get_modifiers = xqr_dri3_get_modifiers,
    .get_drawable_modifiers = xqr_dri3_get_drawable_modifiers,
};

Bool xqr_dmabuf_init(struct xqr_screen *xqr_screen) {
  if (!dri3_screen_init(xqr_screen->screen, &xqr_dri3_info)) {
    ErrorF("Xquadro: dri3_screen_init failed\n");
    return FALSE;
  }
  return TRUE;
}

#endif
