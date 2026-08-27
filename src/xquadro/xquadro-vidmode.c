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



#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include "extinit.h"
#include "misc.h"
#include "os.h"
#include <X11/X.h>

#ifdef XF86VIDMODE

#include "randrstr.h"
#include "vidmodestr.h"

#include "xquadro-output.h"
#include "xquadro-screen.h"
#include "xquadro-vidmode.h"

static DevPrivateKeyRec xqrVidModePrivateKeyRec;
#define xqrVidModePrivateKey (&xqrVidModePrivateKeyRec)

static double mode_hsync(const xRRModeInfo *m) {
  return m->hTotal ? (double)m->dotClock / (double)m->hTotal / 1000.0 : 0.0;
}

static double mode_refresh(const xRRModeInfo *m) {
  double vTotal = m->vTotal;

  if (m->modeFlags & RR_DoubleScan)
    vTotal *= 2.0;
  if (m->modeFlags & RR_Interlace)
    vTotal /= 2.0;

  return (m->hTotal > 0 && vTotal > 0)
             ? (double)m->dotClock / ((double)m->hTotal * vTotal)
             : 0.0;
}

static void xqrRRModeToDisplayMode(RRModePtr rrmode, DisplayModePtr mode) {
  const xRRModeInfo *i = &rrmode->mode;

  mode->next = mode;
  mode->prev = mode;
  mode->name = "";
  mode->VScan = 1;
  mode->Private = NULL;
  mode->HDisplay = i->width;
  mode->HSyncStart = i->hSyncStart;
  mode->HSyncEnd = i->hSyncEnd;
  mode->HTotal = i->hTotal;
  mode->HSkew = i->hSkew;
  mode->VDisplay = i->height;
  mode->VSyncStart = i->vSyncStart;
  mode->VSyncEnd = i->vSyncEnd;
  mode->VTotal = i->vTotal;
  mode->Flags = i->modeFlags;
  mode->Clock = i->dotClock / 1000.0;
  mode->VRefresh = mode_refresh(i);
  mode->HSync = mode_hsync(i);
}

static RRModePtr xqrVidModeGetRRMode(ScreenPtr pScreen, int32_t width,
                                     int32_t height) {
  struct xqr_screen *xqr_screen = xqr_screen_get(pScreen);
  struct xqr_output *xqr_output =
      xqr_screen_get_fixed_or_first_output(xqr_screen);

  if (!xqr_output)
    return NULL;
  return xqr_output_find_mode(xqr_output, width, height);
}

static RRModePtr xqrVidModeGetCurrentRRMode(ScreenPtr pScreen) {
  struct xqr_screen *xqr_screen = xqr_screen_get(pScreen);
  struct xqr_output *xqr_output =
      xqr_screen_get_fixed_or_first_output(xqr_screen);
  struct xqr_emulated_mode *emulated_mode;

  if (!xqr_output)
    return NULL;

  emulated_mode =
      xqr_output_get_emulated_mode_for_client(xqr_output, GetCurrentClient());
  if (emulated_mode)
    return xqr_output_find_mode(xqr_output, emulated_mode->width,
                                emulated_mode->height);
  else
    return xqr_output_find_mode(xqr_output, -1, -1);
}

static Bool xqrVidModeGetCurrentModeline(ScreenPtr pScreen,
                                         DisplayModePtr *mode, int *dotClock) {
  DisplayModePtr pMod;
  RRModePtr rrmode;

  pMod = dixLookupPrivate(&pScreen->devPrivates, xqrVidModePrivateKey);
  if (!pMod)
    return FALSE;

  rrmode = xqrVidModeGetCurrentRRMode(pScreen);
  if (!rrmode)
    return FALSE;

  xqrRRModeToDisplayMode(rrmode, pMod);
  *mode = pMod;
  if (dotClock)
    *dotClock = pMod->Clock;
  return TRUE;
}

static vidMonitorValue xqrVidModeGetMonitorValue(ScreenPtr pScreen, int valtyp,
                                                 int indx) {
  vidMonitorValue ret = {
      NULL,
  };
  RRModePtr rrmode = xqrVidModeGetCurrentRRMode(pScreen);

  if (!rrmode)
    return ret;

  switch (valtyp) {
  case VIDMODE_MON_VENDOR:
    ret.ptr = XVENDORNAME;
    break;
  case VIDMODE_MON_MODEL:
    ret.ptr = "XQUADRO";
    break;
  case VIDMODE_MON_NHSYNC:
    ret.i = 1;
    break;
  case VIDMODE_MON_NVREFRESH:
    ret.i = 1;
    break;
  case VIDMODE_MON_HSYNC_LO:
  case VIDMODE_MON_HSYNC_HI:
    ret.f = mode_hsync(&rrmode->mode) * 100.0;
    break;
  case VIDMODE_MON_VREFRESH_LO:
  case VIDMODE_MON_VREFRESH_HI:
    ret.f = mode_refresh(&rrmode->mode) * 100.0;
    break;
  }
  return ret;
}

static int xqrVidModeGetDotClock(ScreenPtr pScreen, int Clock) { return Clock; }

static int xqrVidModeGetNumOfClocks(ScreenPtr pScreen, Bool *progClock) {
  *progClock = TRUE;
  return 0;
}

static Bool xqrVidModeGetClocks(ScreenPtr pScreen, int *Clocks) {
  return FALSE;
}

static Bool xqrVidModeGetNextModeline(ScreenPtr pScreen, DisplayModePtr *mode,
                                      int *dotClock) {
  struct xqr_screen *xqr_screen = xqr_screen_get(pScreen);
  struct xqr_output *xqr_output =
      xqr_screen_get_fixed_or_first_output(xqr_screen);
  VidModePtr pVidMode;
  DisplayModePtr pMod;
  intptr_t index;

  if (!xqr_output)
    return FALSE;
  pMod = dixLookupPrivate(&pScreen->devPrivates, xqrVidModePrivateKey);
  pVidMode = VidModeGetPtr(pScreen);
  if (!pMod || !pVidMode)
    return FALSE;

  index = (intptr_t)pVidMode->Next;
  if (index >= xqr_output->randr_output->numModes)
    return FALSE;

  xqrRRModeToDisplayMode(xqr_output->randr_output->modes[index], pMod);
  pVidMode->Next = (void *)(index + 1);
  *mode = pMod;
  if (dotClock)
    *dotClock = pMod->Clock;
  return TRUE;
}

static Bool xqrVidModeGetFirstModeline(ScreenPtr pScreen, DisplayModePtr *mode,
                                       int *dotClock) {
  VidModePtr pVidMode = VidModeGetPtr(pScreen);
  if (!pVidMode)
    return FALSE;
  pVidMode->Next = (void *)(intptr_t)0;
  return xqrVidModeGetNextModeline(pScreen, mode, dotClock);
}

static Bool xqrVidModeDeleteModeline(ScreenPtr pScreen, DisplayModePtr mode) {
  return FALSE;
}

static Bool xqrVidModeZoomViewport(ScreenPtr pScreen, int zoom) {
  return (zoom == 1);
}

static Bool xqrVidModeGetViewPort(ScreenPtr pScreen, int *x, int *y) {
  struct xqr_screen *xqr_screen = xqr_screen_get(pScreen);
  struct xqr_output *xqr_output =
      xqr_screen_get_fixed_or_first_output(xqr_screen);
  if (!xqr_output)
    return FALSE;
  *x = xqr_output->x;
  *y = xqr_output->y;
  return TRUE;
}

static Bool xqrVidModeSetViewPort(ScreenPtr pScreen, int x, int y) {
  struct xqr_screen *xqr_screen = xqr_screen_get(pScreen);
  struct xqr_output *xqr_output =
      xqr_screen_get_fixed_or_first_output(xqr_screen);
  if (!xqr_output)
    return FALSE;
  return (x == xqr_output->x && y == xqr_output->y);
}

static Bool xqrVidModeSwitchMode(ScreenPtr pScreen, DisplayModePtr mode) {
  struct xqr_screen *xqr_screen = xqr_screen_get(pScreen);
  struct xqr_output *xqr_output =
      xqr_screen_get_fixed_or_first_output(xqr_screen);
  RRModePtr rrmode;

  if (!xqr_output)
    return FALSE;
  rrmode = xqr_output_find_mode(xqr_output, mode->HDisplay, mode->VDisplay);
  if (!rrmode)
    return FALSE;

  if (xqr_screen->rootless)
    xqr_output_set_emulated_mode(xqr_output, GetCurrentClient(), rrmode, TRUE);
  else if (xqr_screen->fixed_output)
    xqr_output_set_mode_fixed(xqr_screen->fixed_output, rrmode);

  return TRUE;
}

static Bool xqrVidModeLockZoom(ScreenPtr pScreen, Bool lock) { return TRUE; }
static Bool xqrVidModeAddModeline(ScreenPtr pScreen, DisplayModePtr mode) {
  return FALSE;
}
static void xqrVidModeSetCrtcForMode(ScreenPtr pScreen, DisplayModePtr mode) {}

static int xqrVidModeGetNumOfModes(ScreenPtr pScreen) {
  struct xqr_screen *xqr_screen = xqr_screen_get(pScreen);
  struct xqr_output *xqr_output =
      xqr_screen_get_fixed_or_first_output(xqr_screen);
  return xqr_output ? xqr_output->randr_output->numModes : 0;
}

static ModeStatus xqrVidModeCheckModeForMonitor(ScreenPtr pScreen,
                                                DisplayModePtr mode) {
  RRModePtr rrmode =
      xqrVidModeGetRRMode(pScreen, mode->HDisplay, mode->VDisplay);
  if (!rrmode)
    return MODE_ERROR;
  if (mode->HSync == mode_hsync(&rrmode->mode) &&
      mode->VRefresh == mode_refresh(&rrmode->mode))
    return MODE_OK;
  return MODE_ONE_SIZE;
}

static ModeStatus xqrVidModeCheckModeForDriver(ScreenPtr pScreen,
                                               DisplayModePtr mode) {
  return xqrVidModeGetRRMode(pScreen, mode->HDisplay, mode->VDisplay)
             ? MODE_OK
             : MODE_ERROR;
}

static Bool xqrVidModeSetGamma(ScreenPtr pScreen, float r, float g, float b) {

  (void)pScreen;
  (void)r;
  (void)g;
  (void)b;
  return TRUE;
}

static Bool xqrVidModeGetGamma(ScreenPtr pScreen, float *r, float *g,
                               float *b) {

  *r = *g = *b = 1.0f;
  return TRUE;
}

static Bool xqrVidModeSetGammaRamp(ScreenPtr pScreen, int size, CARD16 *r,
                                   CARD16 *g, CARD16 *b) {

  struct xqr_screen *xqr_screen = xqr_screen_get(pScreen);
  struct xqr_output *xqr_output =
      xqr_screen_get_fixed_or_first_output(xqr_screen);

  if (!xqr_output || !xqr_output->randr_crtc)
    return FALSE;

  RRCrtcGammaSet(xqr_output->randr_crtc, r, g, b);
  return TRUE;
}

static Bool xqrVidModeGetGammaRamp(ScreenPtr pScreen, int size, CARD16 *r,
                                   CARD16 *g, CARD16 *b) {
  int i;

  if (size <= 0)
    return FALSE;

  for (i = 0; i < size; i++) {
    CARD16 val = (CARD16)((i * 0xFFFF + (size - 1) / 2) / (size - 1));
    r[i] = val;
    g[i] = val;
    b[i] = val;
  }
  return TRUE;
}

static int xqrVidModeGetGammaRampSize(ScreenPtr pScreen) { return 256; }

static Bool xqrVidModeInit(ScreenPtr pScreen) {
  VidModePtr pVidMode = VidModeInit(pScreen);
  if (!pVidMode)
    return FALSE;

  pVidMode->Flags = 0;
  pVidMode->Next = NULL;

  pVidMode->GetMonitorValue = xqrVidModeGetMonitorValue;
  pVidMode->GetCurrentModeline = xqrVidModeGetCurrentModeline;
  pVidMode->GetFirstModeline = xqrVidModeGetFirstModeline;
  pVidMode->GetNextModeline = xqrVidModeGetNextModeline;
  pVidMode->DeleteModeline = xqrVidModeDeleteModeline;
  pVidMode->ZoomViewport = xqrVidModeZoomViewport;
  pVidMode->GetViewPort = xqrVidModeGetViewPort;
  pVidMode->SetViewPort = xqrVidModeSetViewPort;
  pVidMode->SwitchMode = xqrVidModeSwitchMode;
  pVidMode->LockZoom = xqrVidModeLockZoom;
  pVidMode->GetNumOfClocks = xqrVidModeGetNumOfClocks;
  pVidMode->GetClocks = xqrVidModeGetClocks;
  pVidMode->CheckModeForMonitor = xqrVidModeCheckModeForMonitor;
  pVidMode->CheckModeForDriver = xqrVidModeCheckModeForDriver;
  pVidMode->SetCrtcForMode = xqrVidModeSetCrtcForMode;
  pVidMode->AddModeline = xqrVidModeAddModeline;
  pVidMode->GetDotClock = xqrVidModeGetDotClock;
  pVidMode->GetNumOfModes = xqrVidModeGetNumOfModes;
  pVidMode->SetGamma = xqrVidModeSetGamma;
  pVidMode->GetGamma = xqrVidModeGetGamma;
  pVidMode->SetGammaRamp = xqrVidModeSetGammaRamp;
  pVidMode->GetGammaRamp = xqrVidModeGetGammaRamp;
  pVidMode->GetGammaRampSize = xqrVidModeGetGammaRampSize;

  return TRUE;
}

void xqrVidModeExtensionInit(void) {
  int i;
  Bool enabled = FALSE;

  for (i = 0; i < screenInfo.numScreens; i++) {
    if (xqrVidModeInit(screenInfo.screens[i]))
      enabled = TRUE;
  }
  if (!enabled)
    return;

  if (!dixRegisterPrivateKey(xqrVidModePrivateKey, PRIVATE_SCREEN,
                             sizeof(DisplayModeRec)))
    return;

  VidModeAddExtension(FALSE);
}

#endif
