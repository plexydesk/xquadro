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

#include "dix.h"
#include "fb.h"
#include "os.h"
#include "pixmapstr.h"
#include "privates.h"
#include <X11/X.h>

#ifdef XQR_HAS_GLAMOR
#include "xquadro-glamor-gbm.h"
#include "xquadro-glamor.h"
#endif
#include "xquadro-pixmap.h"
#include "xquadro-screen.h"
#include "xquadro-shm.h"
#include "xquadro-types.h"
#include "xquadro-window-buffers.h"

static DevPrivateKeyRec xqr_pixmap_private_key;
static DevPrivateKeyRec xqr_pixmap_cb_private_key;

struct xqr_pixmap_buffer_release_callback {
  xqr_buffer_release_cb callback;
  void *data;
};

void xqr_pixmap_set_private(PixmapPtr pixmap, struct xqr_pixmap *xqr_pixmap) {
  dixSetPrivate(&pixmap->devPrivates, &xqr_pixmap_private_key, xqr_pixmap);
}

struct xqr_pixmap *xqr_pixmap_get(PixmapPtr pixmap) {
  return dixLookupPrivate(&pixmap->devPrivates, &xqr_pixmap_private_key);
}

PlexyBuffer *xqr_pixmap_get_plexy_buffer(PixmapPtr pixmap) {
#ifdef XQR_HAS_GLAMOR
  struct xqr_screen *xqr_screen = xqr_screen_get(pixmap->drawable.pScreen);

  if (xqr_screen->glamor) {

    return xqr_glamor_get_or_create_plexy_buffer(xqr_screen, pixmap);
  }
#endif
  return xqr_shm_pixmap_get_plexy_buffer(pixmap);
}

Bool xqr_pixmap_set_buffer_release_cb(PixmapPtr pixmap,
                                      xqr_buffer_release_cb func, void *data) {
  struct xqr_pixmap_buffer_release_callback *cb;

  cb = dixLookupPrivate(&pixmap->devPrivates, &xqr_pixmap_cb_private_key);

  if (!cb) {
    cb = calloc(1, sizeof(*cb));
    if (!cb) {
      ErrorF("Xquadro: failed to allocate pixmap callback\n");
      return FALSE;
    }
    dixSetPrivate(&pixmap->devPrivates, &xqr_pixmap_cb_private_key, cb);
  }

  cb->callback = func;
  cb->data = data;
  return TRUE;
}

void xqr_pixmap_del_buffer_release_cb(PixmapPtr pixmap) {
  struct xqr_pixmap_buffer_release_callback *cb;

  cb = dixLookupPrivate(&pixmap->devPrivates, &xqr_pixmap_cb_private_key);
  if (cb) {
    dixSetPrivate(&pixmap->devPrivates, &xqr_pixmap_cb_private_key, NULL);
    free(cb);
  }
}

void xqr_pixmap_buffer_release_cb(PixmapPtr pixmap) {
  struct xqr_pixmap_buffer_release_callback *cb;

  cb = dixLookupPrivate(&pixmap->devPrivates, &xqr_pixmap_cb_private_key);
  if (cb)
    (*cb->callback)(cb->data);
}

Bool xqr_pixmap_init(void) {
  if (!dixRegisterPrivateKey(&xqr_pixmap_private_key, PRIVATE_PIXMAP, 0))
    return FALSE;
  if (!dixRegisterPrivateKey(&xqr_pixmap_cb_private_key, PRIVATE_PIXMAP, 0))
    return FALSE;
  return TRUE;
}
