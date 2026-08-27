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

#if !defined(WIN32)
#include <sys/resource.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <X11/Xatom.h>
#include <X11/Xfuncproto.h>

#include "os/osdep.h"
#include <compint.h>
#include <compositeext.h>
#include <glx_extinit.h>
#include <micmap.h>
#include <misyncshm.h>
#include <opaque.h>
#include <os.h>
#include <propertyst.h>
#include <selection.h>

#include "xquadro-screen.h"
#include "xquadro-vidmode.h"

#ifdef XF86VIDMODE
#include <X11/extensions/xf86vmproto.h>
extern _X_EXPORT Bool noXFree86VidModeExtension;
#endif

void ddxGiveUp(enum ExitCode error) {
  ScreenPtr pScreen;
  struct xqr_screen *xqr_screen;

  if (screenInfo.numScreens < 1)
    return;

  pScreen = screenInfo.screens[0];
  xqr_screen = xqr_screen_get(pScreen);

  if (!xqr_screen)
    return;

  if (xqr_screen->plexy)
    plexy_flush(xqr_screen->plexy);
}

void OsVendorInit(void) {
  if (serverGeneration == 1)
    ForceClockId(CLOCK_MONOTONIC);
}

void OsVendorFatalError(const char *f, va_list args) {
  (void)f;
  (void)args;
}

#if defined(DDXBEFORERESET)
void ddxBeforeReset(void) {}
#endif

#if INPUTTHREAD
void ddxInputThreadInit(void) {}
#endif

void ddxUseMsg(void) {
  ErrorF("-rootless              run rootless (requires WM support)\n");
  ErrorF("-wm fd                 create X client for WM on given fd\n");
  ErrorF("-initfd fd             add given fd as a listen socket for init "
         "clients\n");
  ErrorF("-listenfd fd           add given fd as a listen socket\n");
  ErrorF("-listen fd             deprecated, use -listenfd instead\n");
  ErrorF("-shm                   force shared memory buffer path\n");
#ifdef XQR_HAS_GLAMOR
  ErrorF(
      "-glamor [gl|es|off]    select GL/ES backend for glamor acceleration\n");
#endif
  ErrorF("-verbose [n]           verbose startup messages\n");
  ErrorF("-version               show the server version and exit\n");
  ErrorF("-plexy-socket PATH     path to PlexyShell socket (overrides "
         "PLEXY_SOCKET)\n");
}

static int init_fd = -1;
static int wm_fd = -1;
static int listen_fds[5] = {-1, -1, -1, -1, -1};
static int listen_fd_count = 0;
static int verbosity = 0;

static void xqr_add_listen_fd(int argc, char *argv[], int i) {
  NoListenAll = TRUE;
  if (listen_fd_count == ARRAY_SIZE(listen_fds))
    FatalError("Too many -listen/-listenfd arguments (max %zu)\n",
               ARRAY_SIZE(listen_fds));
  if (!isdigit((unsigned char)*argv[i + 1]))
    FatalError("Xquadro: -listenfd requires a numeric file descriptor\n");
  listen_fds[listen_fd_count++] = atoi(argv[i + 1]);
}

int ddxProcessArgument(int argc, char *argv[], int i) {
  if (strcmp(argv[i], "-rootless") == 0) {
    return 1;
  } else if (strcmp(argv[i], "-listen") == 0) {
    CHECK_FOR_REQUIRED_ARGUMENTS(1);
    if (!isdigit(*argv[i + 1]))
      return 0;
    LogMessageVerb(X_WARNING, 0,
                   "Option -listen for FDs is deprecated; use -listenfd\n");
    xqr_add_listen_fd(argc, argv, i);
    return 2;
  } else if (strcmp(argv[i], "-listenfd") == 0) {
    CHECK_FOR_REQUIRED_ARGUMENTS(1);
    xqr_add_listen_fd(argc, argv, i);
    return 2;
  } else if (strcmp(argv[i], "-wm") == 0) {
    CHECK_FOR_REQUIRED_ARGUMENTS(1);
    wm_fd = atoi(argv[i + 1]);
    return 2;
  } else if (strcmp(argv[i], "-initfd") == 0) {
    CHECK_FOR_REQUIRED_ARGUMENTS(1);
    init_fd = atoi(argv[i + 1]);
    return 2;
  } else if (strcmp(argv[i], "-shm") == 0) {
    return 1;
  } else if (strcmp(argv[i], "-plexy-socket") == 0) {
    CHECK_FOR_REQUIRED_ARGUMENTS(1);

    return 2;
  }
#ifdef XQR_HAS_GLAMOR
  else if (strcmp(argv[i], "-glamor") == 0) {
    CHECK_FOR_REQUIRED_ARGUMENTS(1);
    return 2;
  }
#endif
  else if (strcmp(argv[i], "-verbose") == 0) {
    if (i + 1 < argc && argv[i + 1]) {
      char *end;
      long val = strtol(argv[i + 1], &end, 0);
      if (*end == '\0') {
        verbosity = (int)val;
        LogSetParameter(XLOG_VERBOSITY, verbosity);
        return 2;
      }
    }
    LogSetParameter(XLOG_VERBOSITY, ++verbosity);
    return 1;
  } else if (strcmp(argv[i], "-version") == 0) {
    ErrorF("Xquadro (PlexyShell X server) — built %s\n", __DATE__);
    exit(0);
  }

  return 0;
}

static CARD32 add_client_fd(OsTimerPtr timer, CARD32 time, void *arg) {
  (void)time;
  (void)arg;
  if (!AddClientOnOpenFD(wm_fd))
    FatalError("Xquadro: failed to add WM client on fd %d\n", wm_fd);
  TimerFree(timer);
  return 0;
}

static void listen_on_fds(void) {
  int i;
  for (i = 0; i < listen_fd_count; i++)
    ListenOnOpenFD(listen_fds[i], FALSE);
}

static void wm_selection_callback(CallbackListPtr *p, void *data, void *arg) {
  SelectionInfoRec *info;
  static const char atom_name[] = "WM_S0";
  static Atom atom_wm_s0;

  (void)data;

  if (!arg)
    return;

  info = arg;

  if (atom_wm_s0 == None)
    atom_wm_s0 = MakeAtom(atom_name, strlen(atom_name), TRUE);
  if (info->selection->selection != atom_wm_s0 ||
      info->kind != SelectionSetOwner)
    return;

  listen_on_fds();
  DeleteCallback(&SelectionCallback, wm_selection_callback, NULL);
}

static void try_raising_nofile_limit(void) {
#ifdef RLIMIT_NOFILE
  struct rlimit rlim;

  if (limitNoFile >= 0)
    return;
  if (getrlimit(RLIMIT_NOFILE, &rlim) < 0) {
    ErrorF("Xquadro: getrlimit(NOFILE): %s\n", strerror(errno));
    return;
  }
  rlim.rlim_cur = rlim.rlim_max;
  if (setrlimit(RLIMIT_NOFILE, &rlim) < 0)
    ErrorF("Xquadro: setrlimit(NOFILE): %s\n", strerror(errno));
#endif
}

static const ExtensionModule xquadro_extensions[] = {
#ifdef XF86VIDMODE
    {xqrVidModeExtensionInit, XF86VIDMODENAME, &noXFree86VidModeExtension},
#endif
};

void InitOutput(ScreenInfo *screen_info, int argc, char **argv) {
  int depths[] = {1, 4, 8, 15, 16, 24, 32};
  int bpp[] = {1, 8, 8, 16, 16, 32, 32};
  int i;

  for (i = 0; i < ARRAY_SIZE(depths); i++) {
    screen_info->formats[i].depth = depths[i];
    screen_info->formats[i].bitsPerPixel = bpp[i];
    screen_info->formats[i].scanlinePad = BITMAP_SCANLINE_PAD;
  }

  screen_info->imageByteOrder = IMAGE_BYTE_ORDER;
  screen_info->bitmapScanlineUnit = BITMAP_SCANLINE_UNIT;
  screen_info->bitmapScanlinePad = BITMAP_SCANLINE_PAD;
  screen_info->bitmapBitOrder = BITMAP_BIT_ORDER;
  screen_info->numPixmapFormats = ARRAY_SIZE(depths);

  if (serverGeneration == 1) {
    try_raising_nofile_limit();
    LoadExtensionList(xquadro_extensions, ARRAY_SIZE(xquadro_extensions),
                      FALSE);
  }

  if (AddScreen(xqr_screen_init, argc, argv) == -1)
    FatalError("Xquadro: AddScreen failed\n");

#ifdef XQR_HAS_GLAMOR
  xorgGlxCreateVendor();
#endif
  LocalAccessScopeUser();

  if (wm_fd >= 0 || init_fd >= 0) {
    if (wm_fd >= 0)
      TimerSet(NULL, 0, 1, add_client_fd, NULL);
    if (init_fd >= 0)
      ListenOnOpenFD(init_fd, FALSE);
    AddCallback(&SelectionCallback, wm_selection_callback, NULL);
  } else if (listen_fd_count > 0) {
    listen_on_fds();
  }
}
