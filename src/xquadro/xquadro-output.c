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

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <X11/Xatom.h>
#include <dix.h>
#include <randrstr.h>
#include <scrnintstr.h>

#include "xquadro-cvt.h"
#include "xquadro-output.h"
#include "xquadro-screen.h"

void xqr_output_set_name(struct xqr_output *xqr_output, const char *name) {
  snprintf(xqr_output->name, sizeof(xqr_output->name), "%s", name);
  if (xqr_output->randr_output)
    RROutputSetPhysicalSize(xqr_output->randr_output, 0, 0);
}

void xqr_output_set_xscale(struct xqr_output *xqr_output, double xscale) {
  xqr_output->xscale = xscale;
}

Bool xqr_randr_add_modes_fixed(struct xqr_output *xqr_output, int current_width,
                               int current_height) {
  RRModePtr mode;
  RRModePtr modes[1];

  mode = xquadro_cvt(current_width, current_height, 60.0f, FALSE, FALSE);
  if (!mode)
    return FALSE;

  modes[0] = mode;
  if (RROutputSetModes(xqr_output->randr_output, modes, 1, 1) != Success)
    return FALSE;

  RRCrtcNotify(xqr_output->randr_crtc, mode, xqr_output->x, xqr_output->y,
               xqr_output->rotation, NULL, 1, &xqr_output->randr_output);

  return TRUE;
}

void xqr_output_set_mode_fixed(struct xqr_output *xqr_output, RRModePtr mode) {
  RRCrtcNotify(xqr_output->randr_crtc, mode, xqr_output->x, xqr_output->y,
               xqr_output->rotation, NULL, 1, &xqr_output->randr_output);
}

RRModePtr xqr_output_find_mode(struct xqr_output *xqr_output, int32_t width,
                               int32_t height) {
  RROutputPtr randr_output = xqr_output->randr_output;
  int i;

  if (!randr_output)
    return NULL;

  for (i = 0; i < randr_output->numModes; i++) {
    RRModePtr mode = randr_output->modes[i];
    if ((int)mode->mode.width == width && (int)mode->mode.height == height)
      return mode;
  }
  return NULL;
}

static Bool xqr_randr_get_info(ScreenPtr pScreen, Rotation *rotations) {
  *rotations = RR_Rotate_0;
  return TRUE;
}

static Bool xqr_randr_set_config(ScreenPtr pScreen, Rotation rotation, int rate,
                                 RRScreenSizePtr pSize) {
  struct xqr_screen *xqr_screen = xqr_screen_get(pScreen);
  struct xqr_output *xqr_output;

  if (rotation != RR_Rotate_0)
    return FALSE;

  if (!pSize)
    return TRUE;

  xqr_output = xqr_screen_get_fixed_or_first_output(xqr_screen);
  if (!xqr_output)
    return FALSE;

  if ((int)pSize->width == xqr_output->width &&
      (int)pSize->height == xqr_output->height)
    return TRUE;

  {
    RRModePtr mode = xqr_output_find_mode(xqr_output, (int32_t)pSize->width,
                                          (int32_t)pSize->height);
    if (!mode)
      return FALSE;

    if (xqr_screen->rootless) {
      xqr_output_set_emulated_mode(xqr_output, GetCurrentClient(), mode, FALSE);
    } else {
      xqr_output_set_mode_fixed(xqr_output, mode);
    }
  }

  return TRUE;
}

static Bool xqr_rrscreen_set_size(ScreenPtr pScreen, CARD16 width,
                                  CARD16 height, CARD32 mmWidth,
                                  CARD32 mmHeight) {
  SetRootClip(pScreen, ROOT_CLIP_NONE);
  pScreen->width = width;
  pScreen->height = height;
  pScreen->mmWidth = mmWidth;
  pScreen->mmHeight = mmHeight;
  SetRootClip(pScreen, ROOT_CLIP_FULL);
  RRScreenSizeNotify(pScreen);
  return TRUE;
}

static Bool xqr_rroutput_validate_mode(ScreenPtr pScreen,
                                       RROutputPtr randr_output,
                                       RRModePtr mode) {
  return TRUE;
}

static void xqr_rrmode_destroy(ScreenPtr pScreen, RRModePtr mode) {}

static Bool xqr_rrcrtc_set(ScreenPtr pScreen, RRCrtcPtr crtc, RRModePtr mode,
                           int x, int y, Rotation rotation, int numOutputs,
                           RROutputPtr *outputs) {
  struct xqr_output *xqr_output = NULL;
  struct xqr_screen *xqr_screen = xqr_screen_get(pScreen);
  struct xqr_output *it;

  xorg_list_for_each_entry(it, &xqr_screen->output_list, link) {
    if (it->randr_crtc == crtc) {
      xqr_output = it;
      break;
    }
  }

  if (!xqr_output)
    return FALSE;

  if (!mode)
    return TRUE;

  RRCrtcNotify(crtc, mode, x, y, rotation, NULL, numOutputs, outputs);

  return TRUE;
}

static Bool xqr_rrcrtc_get_gamma(ScreenPtr pScreen, RRCrtcPtr crtc) {

  CARD16 *red, *green, *blue;
  int i, size;

  size = crtc->gammaSize;
  if (size <= 0)
    return TRUE;

  red = crtc->gammaRed;
  green = crtc->gammaGreen;
  blue = crtc->gammaBlue;

  for (i = 0; i < size; i++) {
    CARD16 val = (CARD16)((i * 0xFFFF + (size - 1) / 2) / (size - 1));
    red[i] = val;
    green[i] = val;
    blue[i] = val;
  }

  return TRUE;
}

static Bool xqr_rrcrtc_set_gamma(ScreenPtr pScreen, RRCrtcPtr crtc) {

  return TRUE;
}

struct xqr_output *xqr_output_create(struct xqr_screen *xqr_screen,
                                     uint32_t plexy_output_id, int32_t x,
                                     int32_t y, int32_t width, int32_t height,
                                     double scale, int32_t refresh_mhz) {
  struct xqr_output *xqr_output;
  char name[MAX_OUTPUT_NAME];
  int serial;

  xqr_output = calloc(1, sizeof(*xqr_output));
  if (!xqr_output)
    return NULL;

  xqr_output->xqr_screen = xqr_screen;
  xqr_output->plexy_output_id = plexy_output_id;
  xqr_output->x = x;
  xqr_output->y = y;
  xqr_output->width = width;
  xqr_output->height = height;
  xqr_output->xscale = scale;
  xqr_output->scale = (int32_t)lround(scale);
  xqr_output->refresh = refresh_mhz;
  xqr_output->rotation = RR_Rotate_0;

  serial = xqr_screen_get_next_output_serial(xqr_screen);
  snprintf(name, sizeof(name), "PLEXY-%d", serial);
  xqr_output->server_output_id = (uint32_t)serial;
  xqr_output_set_name(xqr_output, name);

  xqr_output->randr_output =
      RROutputCreate(xqr_screen->screen, xqr_output->name,
                     strlen(xqr_output->name), xqr_output);
  if (!xqr_output->randr_output)
    goto err;

  xqr_output->randr_crtc = RRCrtcCreate(xqr_screen->screen, xqr_output);
  if (!xqr_output->randr_crtc)
    goto err;

  RROutputSetCrtcs(xqr_output->randr_output, &xqr_output->randr_crtc, 1);
  RROutputSetConnection(xqr_output->randr_output, RR_Connected);

  if (!xqr_randr_add_modes_fixed(xqr_output, width, height))
    goto err;

  xorg_list_append(&xqr_output->link, &xqr_screen->output_list);
  return xqr_output;

err:
  free(xqr_output);
  return NULL;
}

void xqr_output_destroy(struct xqr_output *xqr_output) {
  xorg_list_del(&xqr_output->link);
  if (xqr_output->randr_crtc) {
    RRCrtcNotify(xqr_output->randr_crtc, NULL, 0, 0, RR_Rotate_0, NULL, 0,
                 NULL);
    RRCrtcDestroy(xqr_output->randr_crtc);
  }
  if (xqr_output->randr_output)
    RROutputDestroy(xqr_output->randr_output);
  free(xqr_output);
}

void xqr_output_remove(struct xqr_output *xqr_output) {
  RROutputSetConnection(xqr_output->randr_output, RR_Disconnected);
  xqr_output_destroy(xqr_output);
}

Bool xqr_screen_init_output(struct xqr_screen *xqr_screen) {
  ScreenPtr screen = xqr_screen->screen;

  if (!RRScreenInit(screen))
    return FALSE;

  {
    rrScrPriv(screen);
    pScrPriv->rrGetInfo = xqr_randr_get_info;
    pScrPriv->rrSetConfig = xqr_randr_set_config;
    pScrPriv->rrScreenSetSize = xqr_rrscreen_set_size;
    pScrPriv->rrCrtcSet = xqr_rrcrtc_set;
    pScrPriv->rrCrtcGetGamma = xqr_rrcrtc_get_gamma;
    pScrPriv->rrCrtcSetGamma = xqr_rrcrtc_set_gamma;
    pScrPriv->rrOutputValidateMode = xqr_rroutput_validate_mode;
    pScrPriv->rrModeDestroy = xqr_rrmode_destroy;
  }

  RRScreenSetSizeRange(screen, 1, 1, 32767, 32767);

  return TRUE;
}

Bool xqr_screen_init_randr_fixed(struct xqr_screen *xqr_screen) {
  if (!xqr_screen_init_output(xqr_screen))
    return FALSE;

  if (!xqr_output_create(xqr_screen, 0, 0, 0, (int32_t)xqr_screen->width,
                         (int32_t)xqr_screen->height, 1.0, 60000))
    return FALSE;

  return TRUE;
}

struct xqr_emulated_mode *
xqr_output_get_emulated_mode_for_client(struct xqr_output *xqr_output,
                                        ClientPtr client) {
  struct xqr_client *xqr_client = xqr_client_get(client);
  int i;

  if (!xqr_client)
    return NULL;

  for (i = 0; i < XQR_CLIENT_MAX_EMULATED_MODES; i++) {
    if (xqr_client->emulated_modes[i].server_output_id ==
        xqr_output->server_output_id)
      return &xqr_client->emulated_modes[i];
  }

  return NULL;
}

void xqr_output_set_emulated_mode(struct xqr_output *xqr_output,
                                  ClientPtr client, RRModePtr mode,
                                  Bool from_vidmode) {
  struct xqr_client *xqr_client = xqr_client_get(client);
  int i;

  if (!xqr_client)
    return;

  for (i = 0; i < XQR_CLIENT_MAX_EMULATED_MODES; i++) {
    if (xqr_client->emulated_modes[i].server_output_id == 0 ||
        xqr_client->emulated_modes[i].server_output_id ==
            xqr_output->server_output_id) {
      xqr_client->emulated_modes[i].server_output_id =
          xqr_output->server_output_id;
      xqr_client->emulated_modes[i].width = mode->mode.width;
      xqr_client->emulated_modes[i].height = mode->mode.height;
      xqr_client->emulated_modes[i].id = mode->mode.id;
      xqr_client->emulated_modes[i].from_vidmode = from_vidmode;
      return;
    }
  }
}
