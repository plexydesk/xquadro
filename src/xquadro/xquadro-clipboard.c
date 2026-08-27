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

#include <X11/Xatom.h>

#include <dix.h>
#include <propertyst.h>
#include <selection.h>
#include <windowstr.h>

#include "xquadro-clipboard.h"
#include "xquadro-screen.h"

#include <plexy/plexy.h>

static void plexy_clipboard_text_received(PlexyConnection *conn,
                                          const char *text, void *user_data) {
  struct xqr_screen *xqr_screen = user_data;
  (void)conn;

  if (!xqr_screen || !text)
    return;

  free(xqr_screen->clipboard_text);
  xqr_screen->clipboard_text = strdup(text);
}

static void clipboard_selection_callback(CallbackListPtr *pcbl, void *closure,
                                         void *calldata) {
  SelectionInfoRec *info = calldata;
  struct xqr_screen *xqr_screen = closure;
  Selection *sel;

  if (!info || !xqr_screen)
    return;

  sel = info->selection;

  if (info->kind != SelectionSetOwner)
    return;

  if (sel->selection != xqr_screen->clipboard_atom &&
      sel->selection != XA_PRIMARY)
    return;
}

Bool xqr_clipboard_init(struct xqr_screen *xqr_screen) {
  if (!xqr_screen->plexy)
    return FALSE;

  xqr_screen->clipboard_atom = MakeAtom("CLIPBOARD", 9, TRUE);
  xqr_screen->targets_atom = MakeAtom("TARGETS", 7, TRUE);
  xqr_screen->utf8_string_atom = MakeAtom("UTF8_STRING", 11, TRUE);
  xqr_screen->xqr_clipboard_atom = MakeAtom("_XQUADRO_CLIPBOARD", 18, TRUE);
  xqr_screen->clipboard_text = NULL;

  plexy_set_clipboard_text_callback(xqr_screen->plexy,
                                    plexy_clipboard_text_received, xqr_screen);

  AddCallback(&SelectionCallback, clipboard_selection_callback, xqr_screen);

  return TRUE;
}

void xqr_clipboard_fini(struct xqr_screen *xqr_screen) {
  DeleteCallback(&SelectionCallback, clipboard_selection_callback, xqr_screen);

  if (xqr_screen->plexy)
    plexy_set_clipboard_text_callback(xqr_screen->plexy, NULL, NULL);

  free(xqr_screen->clipboard_text);
  xqr_screen->clipboard_text = NULL;
}
