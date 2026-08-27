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

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef XQR_HAS_GLAMOR
#include <glamor.h>
#endif

#include <X11/Xatom.h>
#include <X11/Xfuncproto.h>

#include "os/xserver_poll.h"

#include "window.h"
#include <dixstruct.h>
#include <fb.h>
#include <inputstr.h>
#include <micmap.h>
#include <misyncshm.h>
#include <os.h>
#include <propertyst.h>

#include "xquadro-clipboard.h"
#include "xquadro-cursor.h"
#include "xquadro-input.h"
#include "xquadro-output.h"
#include "xquadro-pixmap.h"
#include "xquadro-present.h"
#include "xquadro-screen.h"
#include "xquadro-shm.h"
#include "xquadro-window.h"
#ifdef XQR_HAS_GLAMOR
#include "xquadro-glamor.h"
#endif

#ifdef MITSHM
#include "shmint.h"
#endif

#include <plexy/plexy.h>

static DevPrivateKeyRec xqr_screen_private_key;
static DevPrivateKeyRec xqr_client_private_key;

#define DEFAULT_DPI 96

struct xqr_client *xqr_client_get(ClientPtr client) {
  return dixLookupPrivate(&client->devPrivates, &xqr_client_private_key);
}

struct xqr_screen *xqr_screen_get(ScreenPtr screen) {
  return dixLookupPrivate(&screen->devPrivates, &xqr_screen_private_key);
}

int xqr_screen_get_width(struct xqr_screen *xqr_screen) {
  return (int)lround(xqr_screen->width);
}

int xqr_screen_get_height(struct xqr_screen *xqr_screen) {
  return (int)lround(xqr_screen->height);
}

struct xqr_output *xqr_screen_get_first_output(struct xqr_screen *xqr_screen) {
  struct xqr_output *xqr_output;

  xorg_list_for_each_entry(xqr_output, &xqr_screen->output_list, link) {
    if (xqr_output->x == 0 && xqr_output->y == 0)
      return xqr_output;
  }

  if (xorg_list_is_empty(&xqr_screen->output_list))
    return NULL;

  return xorg_list_first_entry(&xqr_screen->output_list, struct xqr_output,
                               link);
}

struct xqr_output *
xqr_screen_get_fixed_or_first_output(struct xqr_screen *xqr_screen) {
  if (xqr_screen->fixed_output)
    return xqr_screen->fixed_output;
  return xqr_screen_get_first_output(xqr_screen);
}

int xqr_screen_get_next_output_serial(struct xqr_screen *xqr_screen) {
  return xqr_screen->output_name_serial++;
}

Bool xqr_screen_update_global_surface_scale(struct xqr_screen *xqr_screen) {
  struct xqr_output *xqr_output;
  int max_scale = 1;

  xorg_list_for_each_entry(xqr_output, &xqr_screen->output_list, link) {
    if (xqr_output->scale > max_scale)
      max_scale = xqr_output->scale;
  }

  if (max_scale == xqr_screen->global_surface_scale)
    return FALSE;

  xqr_screen->global_surface_scale = max_scale;
  return TRUE;
}

static void plexy_socket_handler(int fd, int ready, void *data) {
  struct xqr_screen *xqr_screen = data;
  (void)fd;
  (void)ready;

  if (plexy_dispatch_pending(xqr_screen->plexy) < 0) {

    FatalError("Xquadro: PlexyShell connection lost\n");
  }
}

static void wakeup_handler(void *data, int err) {
  (void)data;
  (void)err;
}

static void block_handler(void *data, void *timeout) {
  struct xqr_screen *xqr_screen = data;
  struct xqr_window *xqr_window, *next;

  xorg_list_for_each_entry_safe(xqr_window, next,
                                &xqr_screen->damage_window_list, link_damage) {
    if (!xqr_window->frame_pending && xqr_window->allow_commits) {
      xqr_window_post_damage(xqr_window);
      xorg_list_del(&xqr_window->link_damage);
    }
  }

#ifdef XQR_HAS_GLAMOR
  if (xqr_screen->glamor)
    glamor_block_handler(xqr_screen->screen);
#endif

  plexy_flush(xqr_screen->plexy);
}

void xqr_sync_events(struct xqr_screen *xqr_screen) {
  plexy_dispatch(xqr_screen->plexy);
  plexy_flush(xqr_screen->plexy);
}

Bool xqr_close_screen(ScreenPtr screen) {
  struct xqr_screen *xqr_screen = xqr_screen_get(screen);
  struct xqr_output *xqr_output, *next_output;

  xqr_clipboard_fini(xqr_screen);

  xorg_list_for_each_entry_safe(xqr_output, next_output,
                                &xqr_screen->output_list, link)
      xqr_output_destroy(xqr_output);

  RemoveNotifyFd(xqr_screen->plexy_fd);
  RemoveBlockAndWakeupHandlers(block_handler, wakeup_handler, xqr_screen);

  plexy_disconnect(xqr_screen->plexy);

  screen->CloseScreen = xqr_screen->CloseScreen;
  free(xqr_screen);

  return screen->CloseScreen(screen);
}

static void xqr_root_window_finalized_callback(CallbackListPtr *pcbl,
                                               void *closure, void *calldata) {
  ScreenPtr screen = closure;
  struct xqr_screen *xqr_screen = xqr_screen_get(screen);

  SetRootClip(screen, xqr_screen->root_clip_mode);

  DeleteCallback(&RootWindowFinalizeCallback,
                 xqr_root_window_finalized_callback, screen);
}

Bool xqr_screen_init(ScreenPtr pScreen, int argc, char **argv) {
  struct xqr_screen *xqr_screen;
  Pixel red_mask, blue_mask, green_mask;
  int ret, bpc, green_bpc, i;
  unsigned int xqr_width = 1280;
  unsigned int xqr_height = 800;
  const char *plexy_socket = NULL;

  if (!dixRegisterPrivateKey(&xqr_screen_private_key, PRIVATE_SCREEN, 0))
    return FALSE;
  if (!xqr_pixmap_init())
    return FALSE;
  if (!xqr_window_init())
    return FALSE;
  if (!dixRegisterPrivateKey(&xqr_client_private_key, PRIVATE_CLIENT,
                             sizeof(struct xqr_client)))
    return FALSE;

  xqr_screen = calloc(1, sizeof(*xqr_screen));
  if (!xqr_screen)
    return FALSE;

  dixSetPrivate(&pScreen->devPrivates, &xqr_screen_private_key, xqr_screen);
  xqr_screen->screen = pScreen;
  xqr_screen->depth = 24;
  xqr_screen->global_surface_scale = 1;

#ifdef XQR_HAS_GLAMOR
  xqr_screen->glamor = XQR_GLAMOR_GL;
#endif

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-rootless") == 0) {
      xqr_screen->rootless = 1;
#ifdef SCREENSAVER
      noScreenSaverExtension = TRUE;
#endif
      ScreenSaverTime = 0;
      ScreenSaverInterval = 0;
      defaultScreenSaverTime = 0;
      defaultScreenSaverInterval = 0;
    } else if (strcmp(argv[i], "-shm") == 0) {
      xqr_screen->glamor = XQR_GLAMOR_NONE;
    } else if (strcmp(argv[i], "-plexy-socket") == 0 && i + 1 < argc) {
      plexy_socket = argv[++i];
    } else if (strcmp(argv[i], "-geometry") == 0 && i + 1 < argc) {
      if (sscanf(argv[i + 1], "%ux%u", &xqr_width, &xqr_height) != 2 ||
          xqr_width == 0 || xqr_height == 0) {
        ErrorF("Xquadro: invalid -geometry argument: %s\n", argv[i + 1]);
        return FALSE;
      }
      i++;
    } else if (strcmp(argv[i], "-force-xrandr-emulation") == 0) {
      xqr_screen->force_xrandr_emulation = 1;
    }
  }

  if (!xqr_screen->rootless) {
    xqr_screen->width = xqr_width;
    xqr_screen->height = xqr_height;
  }

  if (!plexy_socket)
    plexy_socket = getenv("PLEXY_SOCKET");

  xqr_screen->plexy = NULL;
  for (i = 0; i < 20; i++) {
    xqr_screen->plexy = plexy_connect(plexy_socket);
    if (xqr_screen->plexy)
      break;
    usleep(100 * 1000);
  }
  if (!xqr_screen->plexy) {
    ErrorF("Xquadro: could not connect to PlexyShell (PLEXY_SOCKET=%s)\n",
           plexy_socket ? plexy_socket : "(auto)");
    free(xqr_screen);
    return FALSE;
  }

  xqr_screen->plexy_fd = plexy_get_fd(xqr_screen->plexy);

  xqr_screen->root_clip_mode =
      xqr_screen->rootless ? ROOT_CLIP_INPUT_ONLY : ROOT_CLIP_FULL;

  xorg_list_init(&xqr_screen->output_list);
  xorg_list_init(&xqr_screen->damage_window_list);
  xorg_list_init(&xqr_screen->window_list);

  if (!monitorResolution)
    monitorResolution = DEFAULT_DPI;

  if (xqr_screen->rootless) {

    if (!xqr_screen_init_output(xqr_screen))
      return FALSE;

    uint32_t screen_w = 0, screen_h = 0;
    plexy_get_screen_size(xqr_screen->plexy, &screen_w, &screen_h);

    if (screen_w == 0)
      screen_w = 1280;
    if (screen_h == 0)
      screen_h = 800;

    uint32_t out_count = plexy_get_output_count(xqr_screen->plexy);
    if (out_count == 0) {

      xqr_output_create(xqr_screen, 0, 0, 0, (int32_t)screen_w,
                        (int32_t)screen_h, 1.0, 60000);
      xqr_screen->width = screen_w;
      xqr_screen->height = screen_h;
    } else {
      for (uint32_t idx = 0; idx < out_count; idx++) {
        const PlexyOutputInfo *info =
            plexy_get_output_info(xqr_screen->plexy, idx);
        if (!info)
          continue;
        xqr_output_create(xqr_screen, info->output_id, info->x, info->y,
                          (int32_t)info->pixel_width,
                          (int32_t)info->pixel_height, info->scale_factor,
                          60000);

        if (idx == 0) {
          xqr_screen->width = info->pixel_width;
          xqr_screen->height = info->pixel_height;
        }
      }
    }
  } else {

    if (!xqr_screen_init_randr_fixed(xqr_screen))
      return FALSE;
  }

  bpc = xqr_screen->depth / 3;
  green_bpc = xqr_screen->depth - 2 * bpc;
  blue_mask = (1 << bpc) - 1;
  green_mask = ((1 << green_bpc) - 1) << bpc;
  red_mask = blue_mask << (green_bpc + bpc);

  miSetVisualTypesAndMasks(xqr_screen->depth,
                           ((1 << TrueColor) | (1 << DirectColor)), green_bpc,
                           TrueColor, red_mask, green_mask, blue_mask);
  miSetPixmapDepths();

  ret = fbScreenInit(pScreen, NULL, xqr_screen_get_width(xqr_screen),
                     xqr_screen_get_height(xqr_screen), monitorResolution,
                     monitorResolution, 0, BitsPerPixel(xqr_screen->depth));
  if (!ret)
    return FALSE;

  fbPictureInit(pScreen, 0, 0);

#ifdef MITSHM
  ShmRegisterFbFuncs(pScreen);
#endif

#ifdef HAVE_XSHMFENCE
  if (!miSyncShmScreenInit(pScreen))
    return FALSE;
#endif

#ifdef XQR_HAS_GLAMOR
  if (xqr_screen->glamor && !xqr_glamor_init(xqr_screen)) {
    ErrorF("Xquadro: Glamor init failed, falling back to SHM\n");
    xqr_screen->glamor = XQR_GLAMOR_NONE;
  }
#endif

  SetNotifyFd(xqr_screen->plexy_fd, plexy_socket_handler, X_NOTIFY_READ,
              xqr_screen);
  RegisterBlockAndWakeupHandlers(block_handler, wakeup_handler, xqr_screen);

  pScreen->blackPixel = 0;
  pScreen->whitePixel = 1;
  fbCreateDefColormap(pScreen);

  if (!xqr_cursor_init(pScreen))
    return FALSE;

  if (!xqr_screen->glamor) {

    xqr_screen->CreateScreenResources = pScreen->CreateScreenResources;
    pScreen->CreateScreenResources = xqr_shm_create_screen_resources;
    pScreen->CreatePixmap = xqr_shm_create_pixmap;
    pScreen->DestroyPixmap = xqr_shm_destroy_pixmap;
  }

  xqr_screen->CloseScreen = pScreen->CloseScreen;
  pScreen->CloseScreen = xqr_close_screen;

  xqr_screen->RealizeWindow = pScreen->RealizeWindow;
  pScreen->RealizeWindow = xqr_realize_window;

  xqr_screen->UnrealizeWindow = pScreen->UnrealizeWindow;
  pScreen->UnrealizeWindow = xqr_unrealize_window;

  xqr_screen->DestroyWindow = pScreen->DestroyWindow;
  pScreen->DestroyWindow = xqr_destroy_window;

  xqr_screen->SetWindowPixmap = pScreen->SetWindowPixmap;
  pScreen->SetWindowPixmap = xqr_window_set_window_pixmap;

  xqr_screen->ChangeWindowAttributes = pScreen->ChangeWindowAttributes;
  pScreen->ChangeWindowAttributes = xqr_change_window_attributes;

  xqr_screen->ReparentWindow = pScreen->ReparentWindow;
  pScreen->ReparentWindow = xqr_reparent_window;

  xqr_screen->ResizeWindow = pScreen->ResizeWindow;
  pScreen->ResizeWindow = xqr_resize_window;

  xqr_screen->MoveWindow = pScreen->MoveWindow;
  pScreen->MoveWindow = xqr_move_window;

  xqr_screen->ClipNotify = pScreen->ClipNotify;
  pScreen->ClipNotify = xqr_clip_notify;

  xqr_screen->ConfigNotify = pScreen->ConfigNotify;
  pScreen->ConfigNotify = xqr_config_notify;

  plexy_flush(xqr_screen->plexy);

  if (!xqr_clipboard_init(xqr_screen))
    ErrorF("Xquadro: clipboard bridging init failed (non-fatal)\n");

  if (!xqr_present_init(pScreen))
    ErrorF("Xquadro: Present extension init failed (non-fatal)\n");

  AddCallback(&RootWindowFinalizeCallback, xqr_root_window_finalized_callback,
              pScreen);

  return TRUE;
}
