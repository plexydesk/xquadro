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

#include "fb.h"
#include "os.h"
#include "pixmapstr.h"

#include "xquadro-pixmap.h"
#include "xquadro-screen.h"
#include "xquadro-shm.h"

#include <plexy/plexy.h>

struct xqr_pixmap {
  PlexyBuffer *buffer;
  void *data;
  size_t size;
};

static uint32_t plexy_format_for_depth(int depth) {
  switch (depth) {
  case 32:
    return PLEXY_FORMAT_ARGB8888;
  case 24:
  default:
    return PLEXY_FORMAT_XRGB8888;
  }
}

static Bool dimensions_match_toplevel(ScreenPtr screen, int width, int height) {
  struct xqr_screen *xqr_screen = xqr_screen_get(screen);
  WindowPtr toplevel;

  if (xqr_screen->rootless)
    toplevel = screen->root->firstChild;
  else
    toplevel = screen->root;

  while (toplevel) {
    if (width == toplevel->drawable.width &&
        height == toplevel->drawable.height)
      return TRUE;
    toplevel = toplevel->nextSib;
  }
  return FALSE;
}

PixmapPtr xqr_shm_create_pixmap(ScreenPtr screen, int width, int height,
                                int depth, unsigned int hint) {
  struct xqr_screen *xqr_screen = xqr_screen_get(screen);
  struct xqr_pixmap *xqr_pixmap;
  PixmapPtr pixmap;
  size_t stride, size;
  uint32_t format;

  if (hint == CREATE_PIXMAP_USAGE_GLYPH_PICTURE ||
      (width == 0 && height == 0) || depth < 15 ||
      (hint != CREATE_PIXMAP_USAGE_BACKING_PIXMAP &&
       !dimensions_match_toplevel(screen, width, height)))
    return fbCreatePixmap(screen, width, height, depth, hint);

  stride = PixmapBytePad(width, depth);
  size = stride * height;
  if (size > (size_t)INT32_MAX)
    return NULL;

  format = plexy_format_for_depth(depth);

  pixmap = fbCreatePixmap(screen, 0, 0, depth, hint);
  if (!pixmap)
    return NULL;

  xqr_pixmap = calloc(1, sizeof(*xqr_pixmap));
  if (!xqr_pixmap)
    goto err_destroy_pixmap;

  xqr_pixmap->buffer = plexy_create_buffer(xqr_screen->plexy, (uint32_t)width,
                                           (uint32_t)height, format);
  if (!xqr_pixmap->buffer)
    goto err_free;

  xqr_pixmap->data = plexy_buffer_get_data(xqr_pixmap->buffer);
  if (!xqr_pixmap->data)
    goto err_destroy_buf;

  xqr_pixmap->size = size;

  if (!(*screen->ModifyPixmapHeader)(
          pixmap, width, height, depth, BitsPerPixel(depth),
          (int)plexy_buffer_get_stride(xqr_pixmap->buffer), xqr_pixmap->data))
    goto err_destroy_buf;

  xqr_pixmap_set_private(pixmap, xqr_pixmap);
  return pixmap;

err_destroy_buf:
  plexy_destroy_buffer(xqr_pixmap->buffer);
err_free:
  free(xqr_pixmap);
err_destroy_pixmap:
  fbDestroyPixmap(pixmap);
  return NULL;
}

Bool xqr_shm_destroy_pixmap(PixmapPtr pixmap) {
  struct xqr_pixmap *xqr_pixmap = xqr_pixmap_get(pixmap);

  if (xqr_pixmap && pixmap->refcnt == 1) {
    xqr_pixmap_del_buffer_release_cb(pixmap);
    if (xqr_pixmap->buffer)
      plexy_destroy_buffer(xqr_pixmap->buffer);
    free(xqr_pixmap);
  }

  return fbDestroyPixmap(pixmap);
}

PlexyBuffer *xqr_shm_pixmap_get_plexy_buffer(PixmapPtr pixmap) {
  struct xqr_pixmap *xqr_pixmap = xqr_pixmap_get(pixmap);

  if (!xqr_pixmap)
    return NULL;

  return xqr_pixmap->buffer;
}

Bool xqr_shm_create_screen_resources(ScreenPtr screen) {
  struct xqr_screen *xqr_screen = xqr_screen_get(screen);
  int ret;

  screen->CreateScreenResources = xqr_screen->CreateScreenResources;
  ret = (*screen->CreateScreenResources)(screen);
  xqr_screen->CreateScreenResources = screen->CreateScreenResources;
  screen->CreateScreenResources = xqr_shm_create_screen_resources;

  if (!ret)
    return FALSE;

  if (xqr_screen->rootless)
    screen->devPrivate = fbCreatePixmap(screen, 0, 0, screen->rootDepth, 0);
  else
    screen->devPrivate = xqr_shm_create_pixmap(
        screen, screen->width, screen->height, screen->rootDepth,
        CREATE_PIXMAP_USAGE_BACKING_PIXMAP);

  SetRootClip(screen, xqr_screen->root_clip_mode);

  return screen->devPrivate != NULL;
}
