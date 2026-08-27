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

#include <drm_fourcc.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <xf86drm.h>

#define MESA_EGL_NO_X11_HEADERS
#define EGL_NO_X11
#include <gbm.h>
#include <glamor.h>
#include <glamor_context.h>
#include <glamor_egl.h>

#include "pixmapstr.h"
#include "privates.h"

#include "xquadro-glamor-gbm.h"
#include "xquadro-glamor.h"
#include "xquadro-pixmap.h"
#include "xquadro-screen.h"
#include "xquadro-window.h"

#include <plexy/plexy.h>

static DevPrivateKeyRec xqr_gbm_private_key;

struct xqr_gbm_private *xqr_glamor_gbm_get(ScreenPtr screen) {
  return dixLookupPrivate(&screen->devPrivates, &xqr_gbm_private_key);
}

static struct xqr_gbm_private *xqr_gbm_get(struct xqr_screen *xqr_screen) {
  return xqr_glamor_gbm_get(xqr_screen->screen);
}

struct xqr_gbm_pixmap {
  PlexyBuffer *buffer;
  EGLImage image;
  unsigned int texture;
  struct gbm_bo *bo;
};

static DevPrivateKeyRec xqr_gbm_pixmap_key;

static struct xqr_gbm_pixmap *xqr_gbm_pixmap_get(PixmapPtr pixmap) {
  return dixLookupPrivate(&pixmap->devPrivates, &xqr_gbm_pixmap_key);
}

static uint32_t gbm_format_for_depth(int depth, Bool gles) {
  switch (depth) {
  case 15:
    return GBM_FORMAT_ARGB1555;
  case 16:
    return GBM_FORMAT_RGB565;
  case 24:
    if (gles)
      return GBM_FORMAT_ARGB8888;
    return GBM_FORMAT_XRGB8888;
  case 30:
    return GBM_FORMAT_ARGB2101010;
  default:
  case 32:
    return GBM_FORMAT_ARGB8888;
  }
}

static Bool xqr_gbm_query_modifiers(EGLDisplay dpy, uint32_t format,
                                    uint32_t *num_modifiers,
                                    uint64_t **modifiers) {
  EGLint n = 0;

  *num_modifiers = 0;
  *modifiers = NULL;

  if (!dpy ||
      !epoxy_has_egl_extension(dpy, "EGL_EXT_image_dma_buf_import_modifiers"))
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

static struct gbm_bo *
xqr_gbm_bo_create_for_depth(struct xqr_screen *xqr_screen,
                            struct xqr_gbm_private *gbm_priv, int width,
                            int height, int depth, uint32_t base_usage,
                            Bool *implicit_modifier, int *last_errno_out) {
  struct gbm_bo *bo = NULL;
  uint32_t format =
      gbm_format_for_depth(depth, !!(xqr_screen->glamor & XQR_GLAMOR_GLES));
  uint32_t num_modifiers = 0;
  uint64_t *modifiers = NULL;
  uint32_t formats[3];
  uint32_t usages[5];
  int num_formats = 0;
  int num_usages = 0;
  Bool implicit = FALSE;
  Bool has_mod_invalid = FALSE;
  Bool has_mod_linear = FALSE;
  Bool add_linear = FALSE;
  uint32_t i;
  int last_errno = 0;

  xqr_gbm_query_modifiers((EGLDisplay)xqr_screen->egl_display, format,
                          &num_modifiers, &modifiers);

  formats[num_formats++] = format;
  if (depth == 24) {
    if (format != GBM_FORMAT_ARGB8888)
      formats[num_formats++] = GBM_FORMAT_ARGB8888;
#ifdef GBM_FORMAT_XBGR8888
    if (format != GBM_FORMAT_XBGR8888)
      formats[num_formats++] = GBM_FORMAT_XBGR8888;
#endif
  }

#ifdef GBM_BO_WITH_MODIFIERS
  if (num_modifiers > 0) {
    for (i = 0; i < (uint32_t)num_formats && !bo; i++) {
#ifdef GBM_BO_WITH_MODIFIERS2
      bo = gbm_bo_create_with_modifiers2(gbm_priv->gbm, width, height,
                                         formats[i], modifiers, num_modifiers,
                                         base_usage);
#else
      bo = gbm_bo_create_with_modifiers(gbm_priv->gbm, width, height,
                                        formats[i], modifiers, num_modifiers);
#endif
    }
  }
#endif

  for (i = 0; i < num_modifiers; i++) {
    if (modifiers[i] == DRM_FORMAT_MOD_INVALID)
      has_mod_invalid = TRUE;
    else if (modifiers[i] == DRM_FORMAT_MOD_LINEAR)
      has_mod_linear = TRUE;
  }
  if (num_modifiers > 0 && !has_mod_invalid && has_mod_linear)
    add_linear = TRUE;

  usages[num_usages++] = base_usage;
  if (add_linear)
    usages[num_usages++] = base_usage | GBM_BO_USE_LINEAR;
#ifdef GBM_BO_USE_TEXTURING
  usages[num_usages++] = base_usage | GBM_BO_USE_TEXTURING;
  if (add_linear)
    usages[num_usages++] =
        base_usage | GBM_BO_USE_TEXTURING | GBM_BO_USE_LINEAR;
#endif
  usages[num_usages++] = 0;

  if (!bo)
    implicit = TRUE;

  for (i = 0; i < (uint32_t)num_usages && !bo; i++) {
    uint32_t j;
    for (j = 0; j < (uint32_t)num_formats && !bo; j++) {
      errno = 0;
      bo = gbm_bo_create(gbm_priv->gbm, width, height, formats[j], usages[i]);
      if (!bo && errno != 0)
        last_errno = errno;
    }
  }

  free(modifiers);
  if (last_errno_out)
    *last_errno_out = last_errno;
  if (implicit_modifier)
    *implicit_modifier = implicit;
  return bo;
}

static PixmapPtr xqr_glamor_gbm_create_pixmap_for_bo(ScreenPtr screen,
                                                     struct gbm_bo *bo,
                                                     int depth) {
  struct xqr_screen *xqr_screen = xqr_screen_get(screen);
  struct xqr_gbm_private *gbm_priv = xqr_gbm_get(xqr_screen);
  struct xqr_gbm_pixmap *gp = NULL;
  PixmapPtr pixmap = NULL;
  EGLDisplay dpy = (EGLDisplay)xqr_screen->egl_display;

  gp = calloc(1, sizeof(*gp));
  if (!gp)
    return NULL;

  pixmap =
      glamor_create_pixmap(screen, gbm_bo_get_width(bo), gbm_bo_get_height(bo),
                           depth, GLAMOR_CREATE_PIXMAP_NO_TEXTURE);
  if (!pixmap) {
    free(gp);
    return NULL;
  }

  gp->image = EGL_NO_IMAGE_KHR;
  gp->texture = 0;
  gp->bo = bo;

  eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE,
                 (EGLContext)xqr_screen->egl_context);

#ifdef GBM_BO_FD_FOR_PLANE

  if (gbm_priv && gbm_priv->dmabuf_capable) {
    uint64_t modifier = gbm_bo_get_modifier(bo);
    int num_planes = gbm_bo_get_plane_count(bo);
    int fds[GBM_MAX_PLANES];
    int plane;
    int attr_num = 0;
    EGLint img_attrs[64];

    static const EGLint kPlaneAttrs[][5] = {
        {EGL_DMA_BUF_PLANE0_FD_EXT, EGL_DMA_BUF_PLANE0_OFFSET_EXT,
         EGL_DMA_BUF_PLANE0_PITCH_EXT, EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT,
         EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT},
        {EGL_DMA_BUF_PLANE1_FD_EXT, EGL_DMA_BUF_PLANE1_OFFSET_EXT,
         EGL_DMA_BUF_PLANE1_PITCH_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT,
         EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT},
        {EGL_DMA_BUF_PLANE2_FD_EXT, EGL_DMA_BUF_PLANE2_OFFSET_EXT,
         EGL_DMA_BUF_PLANE2_PITCH_EXT, EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT,
         EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT},
        {EGL_DMA_BUF_PLANE3_FD_EXT, EGL_DMA_BUF_PLANE3_OFFSET_EXT,
         EGL_DMA_BUF_PLANE3_PITCH_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT,
         EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT},
    };

    for (plane = 0; plane < num_planes && plane < GBM_MAX_PLANES; plane++)
      fds[plane] = -1;

    memset(img_attrs, 0, sizeof(img_attrs));
    img_attrs[attr_num++] = EGL_WIDTH;
    img_attrs[attr_num++] = (EGLint)gbm_bo_get_width(bo);
    img_attrs[attr_num++] = EGL_HEIGHT;
    img_attrs[attr_num++] = (EGLint)gbm_bo_get_height(bo);
    img_attrs[attr_num++] = EGL_LINUX_DRM_FOURCC_EXT;
    img_attrs[attr_num++] = (EGLint)gbm_bo_get_format(bo);

    for (plane = 0; plane < num_planes && plane < GBM_MAX_PLANES; plane++) {
      fds[plane] = gbm_bo_get_fd_for_plane(bo, plane);
      img_attrs[attr_num++] = kPlaneAttrs[plane][0];
      img_attrs[attr_num++] = fds[plane];
      img_attrs[attr_num++] = kPlaneAttrs[plane][1];
      img_attrs[attr_num++] = (EGLint)gbm_bo_get_offset(bo, plane);
      img_attrs[attr_num++] = kPlaneAttrs[plane][2];
      img_attrs[attr_num++] = (EGLint)gbm_bo_get_stride_for_plane(bo, plane);
      img_attrs[attr_num++] = kPlaneAttrs[plane][3];
      img_attrs[attr_num++] = (EGLint)(modifier & 0xFFFFFFFFULL);
      img_attrs[attr_num++] = kPlaneAttrs[plane][4];
      img_attrs[attr_num++] = (EGLint)(modifier >> 32ULL);
    }
    img_attrs[attr_num++] = EGL_NONE;

    gp->image = eglCreateImageKHR(dpy, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT,
                                  NULL, img_attrs);

    for (plane = 0; plane < num_planes && plane < GBM_MAX_PLANES; plane++) {
      if (fds[plane] >= 0) {
        close(fds[plane]);
        fds[plane] = -1;
      }
    }
  }
#endif

  if (gp->image == EGL_NO_IMAGE_KHR) {

    gp->image = eglCreateImageKHR(dpy, EGL_NO_CONTEXT, EGL_NATIVE_PIXMAP_KHR,
                                  gp->bo, NULL);
  }

  if (gp->image == EGL_NO_IMAGE_KHR) {
    ErrorF("Xquadro: glamor gbm: eglCreateImageKHR failed for BO "
           "(%ux%u fmt=0x%x EGL=0x%x)\n",
           (unsigned)gbm_bo_get_width(bo), (unsigned)gbm_bo_get_height(bo),
           (unsigned)gbm_bo_get_format(bo), (unsigned)eglGetError());
    screen->DestroyPixmap(pixmap);
    free(gp);
    return NULL;
  }

  glGenTextures(1, &gp->texture);
  glBindTexture(GL_TEXTURE_2D, gp->texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, gp->image);
  if (glGetError() != GL_NO_ERROR ||
      !glamor_set_pixmap_texture(pixmap, gp->texture)) {
    glBindTexture(GL_TEXTURE_2D, 0);
    if (gp->texture)
      glDeleteTextures(1, &gp->texture);
    eglDestroyImageKHR(dpy, gp->image);
    screen->DestroyPixmap(pixmap);
    free(gp);
    return NULL;
  }
  glBindTexture(GL_TEXTURE_2D, 0);
  glamor_set_pixmap_type(pixmap, GLAMOR_TEXTURE_DRM);

  dixSetPrivate(&pixmap->devPrivates, &xqr_gbm_pixmap_key, gp);
  return pixmap;
}

static PixmapPtr
xqr_glamor_gbm_create_pixmap_internal(struct xqr_screen *xqr_screen, int width,
                                      int height, int depth,
                                      unsigned int hint) {
  struct xqr_gbm_private *gbm_priv = xqr_gbm_get(xqr_screen);
  ScreenPtr screen = xqr_screen->screen;
  struct gbm_bo *bo = NULL;
  Bool require_bo = FALSE;
  PixmapPtr pixmap = NULL;
  int bo_errno = 0;

  if (hint == CREATE_PIXMAP_USAGE_BACKING_PIXMAP ||
      hint == CREATE_PIXMAP_USAGE_SHARED || (xqr_screen->rootless && hint == 0))
    require_bo = TRUE;

  if (gbm_priv && width > 0 && height > 0 && depth >= 15 && require_bo) {
    bo = xqr_gbm_bo_create_for_depth(xqr_screen, gbm_priv, width, height, depth,
                                     GBM_BO_USE_RENDERING, NULL, &bo_errno);
    if (bo)
      pixmap = xqr_glamor_gbm_create_pixmap_for_bo(screen, bo, depth);
    if (pixmap) {
      if (xqr_screen->rootless && hint == CREATE_PIXMAP_USAGE_BACKING_PIXMAP)
        glamor_clear_pixmap(pixmap);
      return pixmap;
    }
    if (bo)
      gbm_bo_destroy(bo);

    ErrorF("Xquadro: glamor gbm: failed to create BO-backed pixmap "
           "(%dx%d depth=%d hint=0x%x errno=%d: %s)\n",
           width, height, depth, hint, bo_errno,
           bo_errno ? strerror(bo_errno) : "n/a");
    ErrorF("Xquadro: glamor gbm: falling back to non-BO glamor pixmap to keep "
           "server alive\n");
  }

  return glamor_create_pixmap(screen, width, height, depth, hint);
}

static PixmapPtr xqr_glamor_gbm_create_pixmap(ScreenPtr screen, int width,
                                              int height, int depth,
                                              unsigned int hint) {
  return xqr_glamor_gbm_create_pixmap_internal(xqr_screen_get(screen), width,
                                               height, depth, hint);
}

static int open_render_node(char **name_out) {
  drmDevicePtr *devs;
  int count, fd = -1;
  int i;
  const char *preferred;

  preferred = getenv("PLEXY_RENDER_NODE");
  if (preferred) {
    fd = open(preferred, O_RDWR | O_CLOEXEC);
    if (fd >= 0) {
      ErrorF("Xquadro: using compositor render node %s\n", preferred);
      if (name_out)
        *name_out = strdup(preferred);
      return fd;
    }
    ErrorF("Xquadro: PLEXY_RENDER_NODE=%s but open failed, falling back\n",
           preferred);
  }

  count = drmGetDevices2(0, NULL, 0);
  if (count <= 0)
    return -1;

  devs = calloc(count, sizeof(*devs));
  if (!devs)
    return -1;

  drmGetDevices2(0, devs, count);

  for (i = 0; i < count && fd < 0; i++) {
    if (!(devs[i]->available_nodes & (1 << DRM_NODE_RENDER)))
      continue;
    fd = open(devs[i]->nodes[DRM_NODE_RENDER], O_RDWR | O_CLOEXEC);
    if (fd >= 0 && name_out)
      *name_out = strdup(devs[i]->nodes[DRM_NODE_RENDER]);
  }

  drmFreeDevices(devs, count);
  free(devs);
  return fd;
}

static Bool xqr_glamor_egl_init(struct xqr_screen *xqr_screen,
                                struct xqr_gbm_private *gbm_priv) {
  EGLDisplay dpy;
  EGLContext ctx;

  dpy = glamor_egl_get_display(EGL_PLATFORM_GBM_MESA, gbm_priv->gbm);
  if (dpy == EGL_NO_DISPLAY) {
    ErrorF("Xquadro: glamor_egl_get_display failed\n");
    return FALSE;
  }

  if (!eglInitialize(dpy, NULL, NULL)) {
    ErrorF("Xquadro: eglInitialize failed\n");
    return FALSE;
  }

  eglBindAPI(EGL_OPENGL_API);

  ctx = eglCreateContext(dpy, NULL, EGL_NO_CONTEXT, NULL);
  if (ctx == EGL_NO_CONTEXT) {
    ErrorF("Xquadro: eglCreateContext failed\n");
    return FALSE;
  }

  xqr_screen->egl_display = dpy;
  xqr_screen->egl_context = ctx;
  return TRUE;
}

Bool xqr_glamor_gbm_init(struct xqr_screen *xqr_screen) {
  struct xqr_gbm_private *gbm_priv;

  if (!dixRegisterPrivateKey(&xqr_gbm_private_key, PRIVATE_SCREEN, 0))
    return FALSE;
  if (!dixRegisterPrivateKey(&xqr_gbm_pixmap_key, PRIVATE_PIXMAP, 0))
    return FALSE;

  gbm_priv = calloc(1, sizeof(*gbm_priv));
  if (!gbm_priv)
    return FALSE;

  gbm_priv->drm_fd = open_render_node(&gbm_priv->device_name);
  if (gbm_priv->drm_fd < 0) {
    ErrorF("Xquadro: no DRM render node found\n");
    free(gbm_priv);
    return FALSE;
  }
  gbm_priv->fd_render_node = TRUE;

  gbm_priv->gbm = gbm_create_device(gbm_priv->drm_fd);
  if (!gbm_priv->gbm) {
    ErrorF("Xquadro: gbm_create_device failed\n");
    close(gbm_priv->drm_fd);
    free(gbm_priv->device_name);
    free(gbm_priv);
    return FALSE;
  }

  dixSetPrivate(&xqr_screen->screen->devPrivates, &xqr_gbm_private_key,
                gbm_priv);

  if (!xqr_glamor_egl_init(xqr_screen, gbm_priv)) {
    gbm_device_destroy(gbm_priv->gbm);
    close(gbm_priv->drm_fd);
    free(gbm_priv->device_name);
    free(gbm_priv);
    return FALSE;
  }

  gbm_priv->dmabuf_capable =
      epoxy_has_egl_extension((EGLDisplay)xqr_screen->egl_display,
                              "EGL_EXT_image_dma_buf_import") &&
      epoxy_has_egl_extension((EGLDisplay)xqr_screen->egl_display,
                              "EGL_EXT_image_dma_buf_import_modifiers");
  ErrorF("Xquadro: glamor gbm: device=%s dmabuf_capable=%d\n",
         gbm_priv->device_name, gbm_priv->dmabuf_capable);

  return TRUE;
}

Bool xqr_glamor_gbm_init_screen(struct xqr_screen *xqr_screen) {
  ScreenPtr screen = xqr_screen->screen;

  screen->CreatePixmap = xqr_glamor_gbm_create_pixmap;
  return TRUE;
}

PixmapPtr
xqr_glamor_gbm_create_pixmap_for_window(struct xqr_window *xqr_window) {
  struct xqr_screen *xqr_screen = xqr_window->xqr_screen;
  WindowPtr window = xqr_window->surface_window;
  unsigned border_width = 2 * window->borderWidth;

  if (!xqr_screen->glamor)
    return NullPixmap;

  return xqr_glamor_gbm_create_pixmap_internal(
      xqr_screen, window->drawable.width + border_width,
      window->drawable.height + border_width, window->drawable.depth,
      CREATE_PIXMAP_USAGE_BACKING_PIXMAP);
}

void xqr_glamor_gbm_fini(struct xqr_screen *xqr_screen) {
  struct xqr_gbm_private *gbm_priv = xqr_gbm_get(xqr_screen);

  if (!gbm_priv)
    return;

  if (gbm_priv->gbm)
    gbm_device_destroy(gbm_priv->gbm);
  if (gbm_priv->drm_fd >= 0)
    close(gbm_priv->drm_fd);
  free(gbm_priv->device_name);
  free(gbm_priv);
}

PlexyBuffer *xqr_glamor_export_pixmap_to_plexy(struct xqr_screen *xqr_screen,
                                               PixmapPtr pixmap) {
  struct xqr_gbm_private *gbm_priv = xqr_gbm_get(xqr_screen);
  struct xqr_gbm_pixmap *gp = xqr_gbm_pixmap_get(pixmap);
  int fds[4] = {-1, -1, -1, -1};
  uint32_t strides[4] = {0, 0, 0, 0};
  uint32_t offsets[4] = {0, 0, 0, 0};
  uint64_t modifier = DRM_FORMAT_MOD_INVALID;
  int num_planes = 0;
  uint32_t format = 0;
  int i;
  PlexyBuffer *buf;

  if (!gbm_priv) {
    ErrorF("Xquadro: glamor export: missing GBM private\n");
    return NULL;
  }

  if (gp && gp->bo) {
    fds[0] = gbm_bo_get_fd(gp->bo);
    if (fds[0] >= 0) {
      strides[0] = gbm_bo_get_stride(gp->bo);
      offsets[0] = 0;
      modifier = gbm_bo_get_modifier(gp->bo);
      format = gbm_bo_get_format(gp->bo);
      num_planes = 1;
    } else {
      ErrorF("Xquadro: glamor export: gbm_bo_get_fd failed (errno=%d: %s)\n",
             errno, strerror(errno));
    }
  }
  if (num_planes < 1) {
    num_planes = glamor_fds_from_pixmap(xqr_screen->screen, pixmap, fds,
                                        strides, offsets, &modifier);
  }
  if (num_planes < 1) {
    ErrorF("Xquadro: glamor export: no dma-buf fds (pixmap=%p %ux%u depth=%u "
           "gp=%p bo=%p)\n",
           pixmap, (unsigned)pixmap->drawable.width,
           (unsigned)pixmap->drawable.height, (unsigned)pixmap->drawable.depth,
           gp, gp ? gp->bo : NULL);
    return NULL;
  }

  if (num_planes != 1 || fds[0] < 0) {
    ErrorF("Xquadro: glamor export: unsupported plane layout (planes=%d fd0=%d "
           "mod=0x%llx)\n",
           num_planes, fds[0], (unsigned long long)modifier);
    for (i = 0; i < num_planes && i < 4; i++) {
      if (fds[i] >= 0)
        close(fds[i]);
    }
    return NULL;
  }

  if (format == 0) {
    switch (pixmap->drawable.depth) {
    case 15:
      format = GBM_FORMAT_ARGB1555;
      break;
    case 16:
      format = GBM_FORMAT_RGB565;
      break;
    case 24:
      format = GBM_FORMAT_XRGB8888;
      break;
    default:
    case 32:
      format = GBM_FORMAT_ARGB8888;
      break;
    }
  }

  ErrorF("Xquadro: glamor export: fd=%d %ux%u stride=%u fmt=0x%x mod=0x%llx\n",
         fds[0], (unsigned)pixmap->drawable.width,
         (unsigned)pixmap->drawable.height, (unsigned)strides[0], format,
         (unsigned long long)modifier);

  buf = plexy_create_buffer_from_dmabuf(
      xqr_screen->plexy, fds[0], (uint32_t)pixmap->drawable.width,
      (uint32_t)pixmap->drawable.height, strides[0], format, modifier);

  if (!buf) {
    ErrorF("Xquadro: glamor export: plexy_create_buffer_from_dmabuf failed "
           "(fd=%d %ux%u stride=%u fmt=0x%x mod=0x%llx)\n",
           fds[0], (unsigned)pixmap->drawable.width,
           (unsigned)pixmap->drawable.height, (unsigned)strides[0], format,
           (unsigned long long)modifier);
  }

  close(fds[0]);

  return buf;
}

PixmapPtr glamor_pixmap_from_fds(ScreenPtr screen, CARD8 num_fds,
                                 const int *fds, CARD16 width, CARD16 height,
                                 const CARD32 *strides, const CARD32 *offsets,
                                 CARD8 depth, CARD8 bpp, uint64_t modifier) {
  struct xqr_screen *xqr_screen = xqr_screen_get(screen);
  struct xqr_gbm_private *gbm_priv = xqr_gbm_get(xqr_screen);
  struct gbm_bo *bo = NULL;
  PixmapPtr pixmap = NULL;

  if (!gbm_priv || width == 0 || height == 0 || num_fds == 0 || depth < 15 ||
      bpp != BitsPerPixel(depth) || strides[0] < width * bpp / 8)
    goto error;

  if (gbm_priv->dmabuf_capable && modifier != DRM_FORMAT_MOD_INVALID) {
#ifdef GBM_BO_WITH_MODIFIERS
    struct gbm_import_fd_modifier_data data;
    int i;

    data.width = width;
    data.height = height;
    data.num_fds = num_fds;
    data.format =
        gbm_format_for_depth(depth, !!(xqr_screen->glamor & XQR_GLAMOR_GLES));
    data.modifier = modifier;

    for (i = 0; i < num_fds; i++) {
      data.fds[i] = fds[i];
      data.strides[i] = strides[i];
      data.offsets[i] = offsets[i];
    }

    bo = gbm_bo_import(gbm_priv->gbm, GBM_BO_IMPORT_FD_MODIFIER, &data,
                       GBM_BO_USE_RENDERING);
#endif
  } else if (num_fds == 1) {
    struct gbm_import_fd_data data;

    data.fd = fds[0];
    data.width = width;
    data.height = height;
    data.stride = strides[0];
    data.format =
        gbm_format_for_depth(depth, !!(xqr_screen->glamor & XQR_GLAMOR_GLES));
    bo = gbm_bo_import(gbm_priv->gbm, GBM_BO_IMPORT_FD, &data,
                       GBM_BO_USE_RENDERING);
  } else {
    goto error;
  }

  if (!bo)
    goto error;

  pixmap = xqr_glamor_gbm_create_pixmap_for_bo(screen, bo, depth);
  if (!pixmap) {
    gbm_bo_destroy(bo);
    goto error;
  }

  return pixmap;

error:
  return NULL;
}

int glamor_egl_fds_from_pixmap(ScreenPtr screen, PixmapPtr pixmap, int *fds,
                               uint32_t *strides, uint32_t *offsets,
                               uint64_t *modifier) {
  struct xqr_gbm_pixmap *gp = xqr_gbm_pixmap_get(pixmap);

  (void)screen;

  if (!gp || !gp->bo)
    return 0;

  *modifier = DRM_FORMAT_MOD_INVALID;
  fds[0] = gbm_bo_get_fd(gp->bo);
  if (fds[0] < 0)
    return 0;
  strides[0] = gbm_bo_get_stride(gp->bo);
  offsets[0] = 0;
  return 1;
}

int glamor_egl_fd_from_pixmap(ScreenPtr screen, PixmapPtr pixmap,
                              CARD16 *stride, CARD32 *size) {
  (void)screen;
  (void)pixmap;
  (void)stride;
  (void)size;
  return -1;
}

PlexyBuffer *xqr_glamor_pixmap_get_plexy_buffer(PixmapPtr pixmap) {
  struct xqr_gbm_pixmap *gp = xqr_gbm_pixmap_get(pixmap);
  return gp ? gp->buffer : NULL;
}

void xqr_glamor_set_pixmap_plexy_buffer(PixmapPtr pixmap, PlexyBuffer *buf) {
  struct xqr_gbm_pixmap *gp = xqr_gbm_pixmap_get(pixmap);
  if (!gp) {
    gp = calloc(1, sizeof(*gp));
    if (!gp)
      return;
    gp->image = EGL_NO_IMAGE_KHR;
    dixSetPrivate(&pixmap->devPrivates, &xqr_gbm_pixmap_key, gp);
  }
  gp->buffer = buf;
}

Bool xqr_glamor_pixmap_has_bo(PixmapPtr pixmap) {
  struct xqr_gbm_pixmap *gp = xqr_gbm_pixmap_get(pixmap);
  return gp && gp->bo;
}

void xqr_glamor_cleanup_pixmap(PixmapPtr pixmap) {
  struct xqr_gbm_pixmap *gp = xqr_gbm_pixmap_get(pixmap);
  struct xqr_screen *xqr_screen = xqr_screen_get(pixmap->drawable.pScreen);

  if (!gp)
    return;

  if (gp->buffer) {
    plexy_destroy_buffer(gp->buffer);
    gp->buffer = NULL;
  }

  if (gp->texture && xqr_screen && xqr_screen->egl_display &&
      xqr_screen->egl_context) {
    eglMakeCurrent((EGLDisplay)xqr_screen->egl_display, EGL_NO_SURFACE,
                   EGL_NO_SURFACE, (EGLContext)xqr_screen->egl_context);
    glDeleteTextures(1, &gp->texture);
    gp->texture = 0;
  }

  if (gp->image != EGL_NO_IMAGE_KHR && xqr_screen && xqr_screen->egl_display) {
    eglDestroyImageKHR((EGLDisplay)xqr_screen->egl_display, gp->image);
    gp->image = EGL_NO_IMAGE_KHR;
  }

  if (gp->bo) {
    gbm_bo_destroy(gp->bo);
    gp->bo = NULL;
  }

  dixSetPrivate(&pixmap->devPrivates, &xqr_gbm_pixmap_key, NULL);
  free(gp);
}

#endif
