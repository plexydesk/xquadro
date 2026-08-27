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



#ifndef XQUADRO_OUTPUT_H
#define XQUADRO_OUTPUT_H

#include <xquadro-config.h>

#include <dix.h>
#include <randrstr.h>

#include "xquadro-types.h"

#define ALL_ROTATIONS                                                          \
  (RR_Rotate_0 | RR_Rotate_90 | RR_Rotate_180 | RR_Rotate_270 | RR_Reflect_X | \
   RR_Reflect_Y)

#define MAX_OUTPUT_NAME 256

struct xqr_output {
  struct xorg_list link;
  struct xqr_screen *xqr_screen;
  RROutputPtr randr_output;
  RRCrtcPtr randr_crtc;

  uint32_t plexy_output_id;
  uint32_t server_output_id;

  int32_t x, y;
  int32_t width, height;
  int32_t refresh;
  double xscale;
  int32_t scale;
  Rotation rotation;

  char name[MAX_OUTPUT_NAME];
};

struct xqr_emulated_mode;

Bool xqr_screen_init_output(struct xqr_screen *xqr_screen);
Bool xqr_screen_init_randr_fixed(struct xqr_screen *xqr_screen);

struct xqr_output *xqr_output_create(struct xqr_screen *xqr_screen,
                                     uint32_t plexy_output_id, int32_t x,
                                     int32_t y, int32_t width, int32_t height,
                                     double scale, int32_t refresh);

void xqr_output_destroy(struct xqr_output *xqr_output);
void xqr_output_remove(struct xqr_output *xqr_output);
void xqr_output_set_name(struct xqr_output *xqr_output, const char *name);
void xqr_output_set_xscale(struct xqr_output *xqr_output, double xscale);

RRModePtr xqr_output_find_mode(struct xqr_output *xqr_output, int32_t width,
                               int32_t height);
Bool xqr_randr_add_modes_fixed(struct xqr_output *xqr_output, int current_width,
                               int current_height);
void xqr_output_set_mode_fixed(struct xqr_output *xqr_output, RRModePtr mode);

struct xqr_emulated_mode *
xqr_output_get_emulated_mode_for_client(struct xqr_output *xqr_output,
                                        ClientPtr client);

void xqr_output_set_emulated_mode(struct xqr_output *xqr_output,
                                  ClientPtr client, RRModePtr mode,
                                  Bool from_vidmode);

#endif
