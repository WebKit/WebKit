/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 *
 */

#include <math.h>
#include <stddef.h>

#include "config/aom_config.h"
#include "config/aom_scale_rtcd.h"

#include "aom/internal/aom_codec_internal.h"
#include "aom_mem/aom_mem.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"
#include "aom_util/aom_pthread.h"

#include "av1/common/av1_common_int.h"
#include "av1/common/convolve.h"
#include "av1/common/enums.h"
#include "av1/common/resize.h"
#include "av1/common/restoration.h"
#include "av1/common/thread_common.h"

// The 's' values are calculated based on original 'r' and 'e' values in the
// spec using GenSgrprojVtable().
// Note: Setting r = 0 skips the filter; with corresponding s = -1 (invalid).
const sgr_params_type av1_sgr_params[SGRPROJ_PARAMS] = {
  { { 2, 1 }, { 140, 3236 } }, { { 2, 1 }, { 112, 2158 } },
  { { 2, 1 }, { 93, 1618 } },  { { 2, 1 }, { 80, 1438 } },
  { { 2, 1 }, { 70, 1295 } },  { { 2, 1 }, { 58, 1177 } },
  { { 2, 1 }, { 47, 1079 } },  { { 2, 1 }, { 37, 996 } },
  { { 2, 1 }, { 30, 925 } },   { { 2, 1 }, { 25, 863 } },
  { { 0, 1 }, { -1, 2589 } },  { { 0, 1 }, { -1, 1618 } },
  { { 0, 1 }, { -1, 1177 } },  { { 0, 1 }, { -1, 925 } },
  { { 2, 0 }, { 56, -1 } },    { { 2, 0 }, { 22, -1 } },
};

void av1_get_upsampled_plane_size(const AV1_COMMON *cm, int is_uv, int *plane_w,
                                  int *plane_h) {
  int ss_x = is_uv && cm->seq_params->subsampling_x;
  int ss_y = is_uv && cm->seq_params->subsampling_y;
  *plane_w = ROUND_POWER_OF_TWO(cm->superres_upscaled_width, ss_x);
  *plane_h = ROUND_POWER_OF_TWO(cm->height, ss_y);
}

// Count horizontal or vertical units in a plane (use a width or height for
// plane_size, respectively). We basically want to divide the plane size by the
// size of a restoration unit. Rather than rounding up unconditionally as you
// might expect, we round to nearest, which models the way a right or bottom
// restoration unit can extend to up to 150% its normal width or height.
//
// The max with 1 is to deal with small frames, which may be smaller than
// half of an LR unit in size.
int av1_lr_count_units(int unit_size, int plane_size) {
  return AOMMAX((plane_size + (unit_size >> 1)) / unit_size, 1);
}

void av1_alloc_restoration_struct(AV1_COMMON *cm, RestorationInfo *rsi,
                                  int is_uv) {
  int plane_w, plane_h;
  av1_get_upsampled_plane_size(cm, is_uv, &plane_w, &plane_h);

  const int unit_size = rsi->restoration_unit_size;
  const int horz_units = av1_lr_count_units(unit_size, plane_w);
  const int vert_units = av1_lr_count_units(unit_size, plane_h);

  rsi->num_rest_units = horz_units * vert_units;
  rsi->horz_units = horz_units;
  rsi->vert_units = vert_units;

  aom_free(rsi->unit_info);
  CHECK_MEM_ERROR(cm, rsi->unit_info,
                  (RestorationUnitInfo *)aom_memalign(
                      16, sizeof(*rsi->unit_info) * rsi->num_rest_units));
}

void av1_free_restoration_struct(RestorationInfo *rst_info) {
  aom_free(rst_info->unit_info);
  rst_info->unit_info = NULL;
}

#if 0
// Pair of values for each sgrproj parameter:
// Index 0 corresponds to r[0], e[0]
// Index 1 corresponds to r[1], e[1]
int sgrproj_mtable[SGRPROJ_PARAMS][2];

static void GenSgrprojVtable(void) {
  for (int i = 0; i < SGRPROJ_PARAMS; ++i) {
    const sgr_params_type *const params = &av1_sgr_params[i];
    for (int j = 0; j < 2; ++j) {
      const int e = params->e[j];
      const int r = params->r[j];
      if (r == 0) {                 // filter is disabled
        sgrproj_mtable[i][j] = -1;  // mark invalid
      } else {                      // filter is enabled
        const int n = (2 * r + 1) * (2 * r + 1);
        const int n2e = n * n * e;
        assert(n2e != 0);
        sgrproj_mtable[i][j] = (((1 << SGRPROJ_MTABLE_BITS) + n2e / 2) / n2e);
      }
    }
  }
}
#endif

void av1_loop_restoration_precal(void) {
#if 0
  GenSgrprojVtable();
#endif
}

static void extend_frame_lowbd(uint8_t *data, int width, int height,
                               ptrdiff_t stride, int border_horz,
                               int border_vert) {
  uint8_t *data_p;
  int i;
  for (i = 0; i < height; ++i) {
    data_p = data + i * stride;
    memset(data_p - border_horz, data_p[0], border_horz);
    memset(data_p + width, data_p[width - 1], border_horz);
  }
  data_p = data - border_horz;
  for (i = -border_vert; i < 0; ++i) {
    memcpy(data_p + i * stride, data_p, width + 2 * border_horz);
  }
  for (i = height; i < height + border_vert; ++i) {
    memcpy(data_p + i * stride, data_p + (height - 1) * stride,
           width + 2 * border_horz);
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
static void extend_frame_highbd(uint16_t *data, int width, int height,
                                ptrdiff_t stride, int border_horz,
                                int border_vert) {
  uint16_t *data_p;
  int i, j;
  for (i = 0; i < height; ++i) {
    data_p = data + i * stride;
    for (j = -border_horz; j < 0; ++j) data_p[j] = data_p[0];
    for (j = width; j < width + border_horz; ++j) data_p[j] = data_p[width - 1];
  }
  data_p = data - border_horz;
  for (i = -border_vert; i < 0; ++i) {
    memcpy(data_p + i * stride, data_p,
           (width + 2 * border_horz) * sizeof(uint16_t));
  }
  for (i = height; i < height + border_vert; ++i) {
    memcpy(data_p + i * stride, data_p + (height - 1) * stride,
           (width + 2 * border_horz) * sizeof(uint16_t));
  }
}

static void copy_rest_unit_highbd(int width, int height, const uint16_t *src,
                                  int src_stride, uint16_t *dst,
                                  int dst_stride) {
  for (int i = 0; i < height; ++i)
    memcpy(dst + i * dst_stride, src + i * src_stride, width * sizeof(*dst));
}
#endif

void av1_extend_frame(uint8_t *data, int width, int height, int stride,
                      int border_horz, int border_vert, int highbd) {
#if CONFIG_AV1_HIGHBITDEPTH
  if (highbd) {
    extend_frame_highbd(CONVERT_TO_SHORTPTR(data), width, height, stride,
                        border_horz, border_vert);
    return;
  }
#endif
  (void)highbd;
  extend_frame_lowbd(data, width, height, stride, border_horz, border_vert);
}

static void copy_rest_unit_lowbd(int width, int height, const uint8_t *src,
                                 int src_stride, uint8_t *dst, int dst_stride) {
  for (int i = 0; i < height; ++i)
    memcpy(dst + i * dst_stride, src + i * src_stride, width);
}

static void copy_rest_unit(int width, int height, const uint8_t *src,
                           int src_stride, uint8_t *dst, int dst_stride,
                           int highbd) {
#if CONFIG_AV1_HIGHBITDEPTH
  if (highbd) {
    copy_rest_unit_highbd(width, height, CONVERT_TO_SHORTPTR(src), src_stride,
                          CONVERT_TO_SHORTPTR(dst), dst_stride);
    return;
  }
#endif
  (void)highbd;
  copy_rest_unit_lowbd(width, height, src, src_stride, dst, dst_stride);
}

#define REAL_PTR(hbd, d) ((hbd) ? (uint8_t *)CONVERT_TO_SHORTPTR(d) : (d))

// With striped loop restoration, the filtering for each 64-pixel stripe gets
// most of its input from the output of CDEF (stored in data8), but we need to
// fill out a border of 3 pixels above/below the stripe according to the
// following rules:
//
// * At the top and bottom of the frame, we copy the outermost row of CDEF
//   pixels three times. This extension is done by a call to av1_extend_frame()
//   at the start of the loop restoration process, so the value of
//   copy_above/copy_below doesn't strictly matter.
//
// * All other boundaries are stripe boundaries within the frame. In that case,
//   we take 2 rows of deblocked pixels and extend them to 3 rows of context.
static void get_stripe_boundary_info(const RestorationTileLimits *limits,
                                     int plane_w, int plane_h, int ss_y,
                                     int *copy_above, int *copy_below) {
  (void)plane_w;

  *copy_above = 1;
  *copy_below = 1;

  const int full_stripe_height = RESTORATION_PROC_UNIT_SIZE >> ss_y;
  const int runit_offset = RESTORATION_UNIT_OFFSET >> ss_y;

  const int first_stripe_in_plane = (limits->v_start == 0);
  const int this_stripe_height =
      full_stripe_height - (first_stripe_in_plane ? runit_offset : 0);
  const int last_stripe_in_plane =
      (limits->v_start + this_stripe_height >= plane_h);

  if (first_stripe_in_plane) *copy_above = 0;
  if (last_stripe_in_plane) *copy_below = 0;
}

// Overwrite the border pixels around a processing stripe so that the conditions
// listed above get_stripe_boundary_info() are preserved.
// We save the pixels which get overwritten into a temporary buffer, so that
// they can be restored by restore_processing_stripe_boundary() after we've
// processed the stripe.
//
// limits gives the rectangular limits of the remaining stripes for the current
// restoration unit. rsb is the stored stripe boundaries (taken from either
// deblock or CDEF output as necessary).
static void setup_processing_stripe_boundary(
    const RestorationTileLimits *limits, const RestorationStripeBoundaries *rsb,
    int rsb_row, int use_highbd, int h, uint8_t *data8, int data_stride,
    RestorationLineBuffers *rlbs, int copy_above, int copy_below, int opt) {
  // Offsets within the line buffers. The buffer logically starts at column
  // -RESTORATION_EXTRA_HORZ so the 1st column (at x0 - RESTORATION_EXTRA_HORZ)
  // has column x0 in the buffer.
  const int buf_stride = rsb->stripe_boundary_stride;
  const int buf_x0_off = limits->h_start;
  const int line_width =
      (limits->h_end - limits->h_start) + 2 * RESTORATION_EXTRA_HORZ;
  const int line_size = line_width << use_highbd;

  const int data_x0 = limits->h_start - RESTORATION_EXTRA_HORZ;

  // Replace RESTORATION_BORDER pixels above the top of the stripe
  // We expand RESTORATION_CTX_VERT=2 lines from rsb->stripe_boundary_above
  // to fill RESTORATION_BORDER=3 lines of above pixels. This is done by
  // duplicating the topmost of the 2 lines (see the AOMMAX call when
  // calculating src_row, which gets the values 0, 0, 1 for i = -3, -2, -1).
  if (!opt) {
    if (copy_above) {
      uint8_t *data8_tl = data8 + data_x0 + limits->v_start * data_stride;

      for (int i = -RESTORATION_BORDER; i < 0; ++i) {
        const int buf_row = rsb_row + AOMMAX(i + RESTORATION_CTX_VERT, 0);
        const int buf_off = buf_x0_off + buf_row * buf_stride;
        const uint8_t *buf =
            rsb->stripe_boundary_above + (buf_off << use_highbd);
        uint8_t *dst8 = data8_tl + i * data_stride;
        // Save old pixels, then replace with data from stripe_boundary_above
        memcpy(rlbs->tmp_save_above[i + RESTORATION_BORDER],
               REAL_PTR(use_highbd, dst8), line_size);
        memcpy(REAL_PTR(use_highbd, dst8), buf, line_size);
      }
    }

    // Replace RESTORATION_BORDER pixels below the bottom of the stripe.
    // The second buffer row is repeated, so src_row gets the values 0, 1, 1
    // for i = 0, 1, 2.
    if (copy_below) {
      const int stripe_end = limits->v_start + h;
      uint8_t *data8_bl = data8 + data_x0 + stripe_end * data_stride;

      for (int i = 0; i < RESTORATION_BORDER; ++i) {
        const int buf_row = rsb_row + AOMMIN(i, RESTORATION_CTX_VERT - 1);
        const int buf_off = buf_x0_off + buf_row * buf_stride;
        const uint8_t *src =
            rsb->stripe_boundary_below + (buf_off << use_highbd);

        uint8_t *dst8 = data8_bl + i * data_stride;
        // Save old pixels, then replace with data from stripe_boundary_below
        memcpy(rlbs->tmp_save_below[i], REAL_PTR(use_highbd, dst8), line_size);
        memcpy(REAL_PTR(use_highbd, dst8), src, line_size);
      }
    }
  } else {
    if (copy_above) {
      uint8_t *data8_tl = data8 + data_x0 + limits->v_start * data_stride;

      // Only save and overwrite i=-RESTORATION_BORDER line.
      uint8_t *dst8 = data8_tl + (-RESTORATION_BORDER) * data_stride;
      // Save old pixels, then replace with data from stripe_boundary_above
      memcpy(rlbs->tmp_save_above[0], REAL_PTR(use_highbd, dst8), line_size);
      memcpy(REAL_PTR(use_highbd, dst8),
             REAL_PTR(use_highbd,
                      data8_tl + (-RESTORATION_BORDER + 1) * data_stride),
             line_size);
    }

    if (copy_below) {
      const int stripe_end = limits->v_start + h;
      uint8_t *data8_bl = data8 + data_x0 + stripe_end * data_stride;

      // Only save and overwrite i=2 line.
      uint8_t *dst8 = data8_bl + 2 * data_stride;
      // Save old pixels, then replace with data from stripe_boundary_below
      memcpy(rlbs->tmp_save_below[2], REAL_PTR(use_highbd, dst8), line_size);
      memcpy(REAL_PTR(use_highbd, dst8),
             REAL_PTR(use_highbd, data8_bl + (2 - 1) * data_stride), line_size);
    }
  }
}

// Once a processing stripe is finished, this function sets the boundary
// pixels which were overwritten by setup_processing_stripe_boundary()
// back to their original values
static void restore_processing_stripe_boundary(
    const RestorationTileLimits *limits, const RestorationLineBuffers *rlbs,
    int use_highbd, int h, uint8_t *data8, int data_stride, int copy_above,
    int copy_below, int opt) {
  const int line_width =
      (limits->h_end - limits->h_start) + 2 * RESTORATION_EXTRA_HORZ;
  const int line_size = line_width << use_highbd;

  const int data_x0 = limits->h_start - RESTORATION_EXTRA_HORZ;

  if (!opt) {
    if (copy_above) {
      uint8_t *data8_tl = data8 + data_x0 + limits->v_start * data_stride;
      for (int i = -RESTORATION_BORDER; i < 0; ++i) {
        uint8_t *dst8 = data8_tl + i * data_stride;
        memcpy(REAL_PTR(use_highbd, dst8),
               rlbs->tmp_save_above[i + RESTORATION_BORDER], line_size);
      }
    }

    if (copy_below) {
      const int stripe_bottom = limits->v_start + h;
      uint8_t *data8_bl = data8 + data_x0 + stripe_bottom * data_stride;

      for (int i = 0; i < RESTORATION_BORDER; ++i) {
        if (stripe_bottom + i >= limits->v_end + RESTORATION_BORDER) break;

        uint8_t *dst8 = data8_bl + i * data_stride;
        memcpy(REAL_PTR(use_highbd, dst8), rlbs->tmp_save_below[i], line_size);
      }
    }
  } else {
    if (copy_above) {
      uint8_t *data8_tl = data8 + data_x0 + limits->v_start * data_stride;

      // Only restore i=-RESTORATION_BORDER line.
      uint8_t *dst8 = data8_tl + (-RESTORATION_BORDER) * data_stride;
      memcpy(REAL_PTR(use_highbd, dst8), rlbs->tmp_save_above[0], line_size);
    }

    if (copy_below) {
      const int stripe_bottom = limits->v_start + h;
      uint8_t *data8_bl = data8 + data_x0 + stripe_bottom * data_stride;

      // Only restore i=2 line.
      if (stripe_bottom + 2 < limits->v_end + RESTORATION_BORDER) {
        uint8_t *dst8 = data8_bl + 2 * data_stride;
        memcpy(REAL_PTR(use_highbd, dst8), rlbs->tmp_save_below[2], line_size);
      }
    }
  }
}

static void wiener_filter_stripe(const RestorationUnitInfo *rui,
                                 int stripe_width, int stripe_height,
                                 int procunit_width, const uint8_t *src,
                                 int src_stride, uint8_t *dst, int dst_stride,
                                 int32_t *tmpbuf, int bit_depth,
                                 struct aom_internal_error_info *error_info) {
  (void)tmpbuf;
  (void)bit_depth;
  (void)error_info;
  assert(bit_depth == 8);
  const WienerConvolveParams conv_params = get_conv_params_wiener(8);

  for (int j = 0; j < stripe_width; j += procunit_width) {
    int w = AOMMIN(procunit_width, (stripe_width - j + 15) & ~15);
    const uint8_t *src_p = src + j;
    uint8_t *dst_p = dst + j;
    av1_wiener_convolve_add_src(
        src_p, src_stride, dst_p, dst_stride, rui->wiener_info.hfilter, 16,
        rui->wiener_info.vfilter, 16, w, stripe_height, &conv_params);
  }
}

/* Calculate windowed sums (if sqr=0) or sums of squares (if sqr=1)
   over the input. The window is of size (2r + 1)x(2r + 1), and we
   specialize to r = 1, 2, 3. A default function is used for r > 3.

   Each loop follows the same format: We keep a window's worth of input
   in individual variables and select data out of that as appropriate.
*/
static void boxsum1(int32_t *src, int width, int height, int src_stride,
                    int sqr, int32_t *dst, int dst_stride) {
  int i, j, a, b, c;
  assert(width > 2 * SGRPROJ_BORDER_HORZ);
  assert(height > 2 * SGRPROJ_BORDER_VERT);

  // Vertical sum over 3-pixel regions, from src into dst.
  if (!sqr) {
    for (j = 0; j < width; ++j) {
      a = src[j];
      b = src[src_stride + j];
      c = src[2 * src_stride + j];

      dst[j] = a + b;
      for (i = 1; i < height - 2; ++i) {
        // Loop invariant: At the start of each iteration,
        // a = src[(i - 1) * src_stride + j]
        // b = src[(i    ) * src_stride + j]
        // c = src[(i + 1) * src_stride + j]
        dst[i * dst_stride + j] = a + b + c;
        a = b;
        b = c;
        c = src[(i + 2) * src_stride + j];
      }
      dst[i * dst_stride + j] = a + b + c;
      dst[(i + 1) * dst_stride + j] = b + c;
    }
  } else {
    for (j = 0; j < width; ++j) {
      a = src[j] * src[j];
      b = src[src_stride + j] * src[src_stride + j];
      c = src[2 * src_stride + j] * src[2 * src_stride + j];

      dst[j] = a + b;
      for (i = 1; i < height - 2; ++i) {
        dst[i * dst_stride + j] = a + b + c;
        a = b;
        b = c;
        c = src[(i + 2) * src_stride + j] * src[(i + 2) * src_stride + j];
      }
      dst[i * dst_stride + j] = a + b + c;
      dst[(i + 1) * dst_stride + j] = b + c;
    }
  }

  // Horizontal sum over 3-pixel regions of dst
  for (i = 0; i < height; ++i) {
    a = dst[i * dst_stride];
    b = dst[i * dst_stride + 1];
    c = dst[i * dst_stride + 2];

    dst[i * dst_stride] = a + b;
    for (j = 1; j < width - 2; ++j) {
      // Loop invariant: At the start of each iteration,
      // a = src[i * src_stride + (j - 1)]
      // b = src[i * src_stride + (j    )]
      // c = src[i * src_stride + (j + 1)]
      dst[i * dst_stride + j] = a + b + c;
      a = b;
      b = c;
      c = dst[i * dst_stride + (j + 2)];
    }
    dst[i * dst_stride + j] = a + b + c;
    dst[i * dst_stride + (j + 1)] = b + c;
  }
}

static void boxsum2(int32_t *src, int width, int height, int src_stride,
                    int sqr, int32_t *dst, int dst_stride) {
  int i, j, a, b, c, d, e;
  assert(width > 2 * SGRPROJ_BORDER_HORZ);
  assert(height > 2 * SGRPROJ_BORDER_VERT);

  // Vertical sum over 5-pixel regions, from src into dst.
  if (!sqr) {
    for (j = 0; j < width; ++j) {
      a = src[j];
      b = src[src_stride + j];
      c = src[2 * src_stride + j];
      d = src[3 * src_stride + j];
      e = src[4 * src_stride + j];

      dst[j] = a + b + c;
      dst[dst_stride + j] = a + b + c + d;
      for (i = 2; i < height - 3; ++i) {
        // Loop invariant: At the start of each iteration,
        // a = src[(i - 2) * src_stride + j]
        // b = src[(i - 1) * src_stride + j]
        // c = src[(i    ) * src_stride + j]
        // d = src[(i + 1) * src_stride + j]
        // e = src[(i + 2) * src_stride + j]
        dst[i * dst_stride + j] = a + b + c + d + e;
        a = b;
        b = c;
        c = d;
        d = e;
        e = src[(i + 3) * src_stride + j];
      }
      dst[i * dst_stride + j] = a + b + c + d + e;
      dst[(i + 1) * dst_stride + j] = b + c + d + e;
      dst[(i + 2) * dst_stride + j] = c + d + e;
    }
  } else {
    for (j = 0; j < width; ++j) {
      a = src[j] * src[j];
      b = src[src_stride + j] * src[src_stride + j];
      c = src[2 * src_stride + j] * src[2 * src_stride + j];
      d = src[3 * src_stride + j] * src[3 * src_stride + j];
      e = src[4 * src_stride + j] * src[4 * src_stride + j];

      dst[j] = a + b + c;
      dst[dst_stride + j] = a + b + c + d;
      for (i = 2; i < height - 3; ++i) {
        dst[i * dst_stride + j] = a + b + c + d + e;
        a = b;
        b = c;
        c = d;
        d = e;
        e = src[(i + 3) * src_stride + j] * src[(i + 3) * src_stride + j];
      }
      dst[i * dst_stride + j] = a + b + c + d + e;
      dst[(i + 1) * dst_stride + j] = b + c + d + e;
      dst[(i + 2) * dst_stride + j] = c + d + e;
    }
  }

  // Horizontal sum over 5-pixel regions of dst
  for (i = 0; i < height; ++i) {
    a = dst[i * dst_stride];
    b = dst[i * dst_stride + 1];
    c = dst[i * dst_stride + 2];
    d = dst[i * dst_stride + 3];
    e = dst[i * dst_stride + 4];

    dst[i * dst_stride] = a + b + c;
    dst[i * dst_stride + 1] = a + b + c + d;
    for (j = 2; j < width - 3; ++j) {
      // Loop invariant: At the start of each iteration,
      // a = src[i * src_stride + (j - 2)]
      // b = src[i * src_stride + (j - 1)]
      // c = src[i * src_stride + (j    )]
      // d = src[i * src_stride + (j + 1)]
      // e = src[i * src_stride + (j + 2)]
      dst[i * dst_stride + j] = a + b + c + d + e;
      a = b;
      b = c;
      c = d;
      d = e;
      e = dst[i * dst_stride + (j + 3)];
    }
    dst[i * dst_stride + j] = a + b + c + d + e;
    dst[i * dst_stride + (j + 1)] = b + c + d + e;
    dst[i * dst_stride + (j + 2)] = c + d + e;
  }
}

static void boxsum(int32_t *src, int width, int height, int src_stride, int r,
                   int sqr, int32_t *dst, int dst_stride) {
  if (r == 1)
    boxsum1(src, width, height, src_stride, sqr, dst, dst_stride);
  else if (r == 2)
    boxsum2(src, width, height, src_stride, sqr, dst, dst_stride);
  else
    assert(0 && "Invalid value of r in self-guided filter");
}

void av1_decode_xq(const int *xqd, int *xq, const sgr_params_type *params) {
  if (params->r[0] == 0) {
    xq[0] = 0;
    xq[1] = (1 << SGRPROJ_PRJ_BITS) - xqd[1];
  } else if (params->r[1] == 0) {
    xq[0] = xqd[0];
    xq[1] = 0;
  } else {
    xq[0] = xqd[0];
    xq[1] = (1 << SGRPROJ_PRJ_BITS) - xq[0] - xqd[1];
  }
}

const int32_t av1_x_by_xplus1[256] = {
  // Special case: Map 0 -> 1 (corresponding to a value of 1/256)
  // instead of 0. See comments in selfguided_restoration_internal() for why
  1,   128, 171, 192, 205, 213, 219, 224, 228, 230, 233, 235, 236, 238, 239,
  240, 241, 242, 243, 243, 244, 244, 245, 245, 246, 246, 247, 247, 247, 247,
  248, 248, 248, 248, 249, 249, 249, 249, 249, 250, 250, 250, 250, 250, 250,
  250, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 252, 252, 252, 252,
  252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 253, 253,
  253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253,
  253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 254, 254, 254,
  254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
  254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
  254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
  254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
  254, 254, 254, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  256,
};

const int32_t av1_one_by_x[MAX_NELEM] = {
  4096, 2048, 1365, 1024, 819, 683, 585, 512, 455, 410, 372, 341, 315,
  293,  273,  256,  241,  228, 216, 205, 195, 186, 178, 171, 164,
};

static void calculate_intermediate_result(int32_t *dgd, int width, int height,
                                          int dgd_stride, int bit_depth,
                                          int sgr_params_idx, int radius_idx,
                                          int pass, int32_t *A, int32_t *B) {
  const sgr_params_type *const params = &av1_sgr_params[sgr_params_idx];
  const int r = params->r[radius_idx];
  const int width_ext = width + 2 * SGRPROJ_BORDER_HORZ;
  const int height_ext = height + 2 * SGRPROJ_BORDER_VERT;
  // Adjusting the stride of A and B here appears to avoid bad cache effects,
  // leading to a significant speed improvement.
  // We also align the stride to a multiple of 16 bytes, for consistency
  // with the SIMD version of this function.
  int buf_stride = ((width_ext + 3) & ~3) + 16;
  const int step = pass == 0 ? 1 : 2;
  int i, j;

  assert(r <= MAX_RADIUS && "Need MAX_RADIUS >= r");
  assert(r <= SGRPROJ_BORDER_VERT - 1 && r <= SGRPROJ_BORDER_HORZ - 1 &&
         "Need SGRPROJ_BORDER_* >= r+1");

  boxsum(dgd - dgd_stride * SGRPROJ_BORDER_VERT - SGRPROJ_BORDER_HORZ,
         width_ext, height_ext, dgd_stride, r, 0, B, buf_stride);
  boxsum(dgd - dgd_stride * SGRPROJ_BORDER_VERT - SGRPROJ_BORDER_HORZ,
         width_ext, height_ext, dgd_stride, r, 1, A, buf_stride);
  A += SGRPROJ_BORDER_VERT * buf_stride + SGRPROJ_BORDER_HORZ;
  B += SGRPROJ_BORDER_VERT * buf_stride + SGRPROJ_BORDER_HORZ;
  // Calculate the eventual A[] and B[] arrays. Include a 1-pixel border - ie,
  // for a 64x64 processing unit, we calculate 66x66 pixels of A[] and B[].
  for (i = -1; i < height + 1; i += step) {
    for (j = -1; j < width + 1; ++j) {
      const int k = i * buf_stride + j;
      const int n = (2 * r + 1) * (2 * r + 1);

      // a < 2^16 * n < 2^22 regardless of bit depth
      uint32_t a = ROUND_POWER_OF_TWO(A[k], 2 * (bit_depth - 8));
      // b < 2^8 * n < 2^14 regardless of bit depth
      uint32_t b = ROUND_POWER_OF_TWO(B[k], bit_depth - 8);

      // Each term in calculating p = a * n - b * b is < 2^16 * n^2 < 2^28,
      // and p itself satisfies p < 2^14 * n^2 < 2^26.
      // This bound on p is due to:
      // https://en.wikipedia.org/wiki/Popoviciu's_inequality_on_variances
      //
      // Note: Sometimes, in high bit depth, we can end up with a*n < b*b.
      // This is an artefact of rounding, and can only happen if all pixels
      // are (almost) identical, so in this case we saturate to p=0.
      uint32_t p = (a * n < b * b) ? 0 : a * n - b * b;

      const uint32_t s = params->s[radius_idx];

      // p * s < (2^14 * n^2) * round(2^20 / n^2 eps) < 2^34 / eps < 2^32
      // as long as eps >= 4. So p * s fits into a uint32_t, and z < 2^12
      // (this holds even after accounting for the rounding in s)
      const uint32_t z = ROUND_POWER_OF_TWO(p * s, SGRPROJ_MTABLE_BITS);

      // Note: We have to be quite careful about the value of A[k].
      // This is used as a blend factor between individual pixel values and the
      // local mean. So it logically has a range of [0, 256], including both
      // endpoints.
      //
      // This is a pain for hardware, as we'd like something which can be stored
      // in exactly 8 bits.
      // Further, in the calculation of B[k] below, if z == 0 and r == 2,
      // then A[k] "should be" 0. But then we can end up setting B[k] to a value
      // slightly above 2^(8 + bit depth), due to rounding in the value of
      // av1_one_by_x[25-1].
      //
      // Thus we saturate so that, when z == 0, A[k] is set to 1 instead of 0.
      // This fixes the above issues (256 - A[k] fits in a uint8, and we can't
      // overflow), without significantly affecting the final result: z == 0
      // implies that the image is essentially "flat", so the local mean and
      // individual pixel values are very similar.
      //
      // Note that saturating on the other side, ie. requring A[k] <= 255,
      // would be a bad idea, as that corresponds to the case where the image
      // is very variable, when we want to preserve the local pixel value as
      // much as possible.
      A[k] = av1_x_by_xplus1[AOMMIN(z, 255)];  // in range [1, 256]

      // SGRPROJ_SGR - A[k] < 2^8 (from above), B[k] < 2^(bit_depth) * n,
      // av1_one_by_x[n - 1] = round(2^12 / n)
      // => the product here is < 2^(20 + bit_depth) <= 2^32,
      // and B[k] is set to a value < 2^(8 + bit depth)
      // This holds even with the rounding in av1_one_by_x and in the overall
      // result, as long as SGRPROJ_SGR - A[k] is strictly less than 2^8.
      B[k] = (int32_t)ROUND_POWER_OF_TWO((uint32_t)(SGRPROJ_SGR - A[k]) *
                                             (uint32_t)B[k] *
                                             (uint32_t)av1_one_by_x[n - 1],
                                         SGRPROJ_RECIP_BITS);
    }
  }
}

static void selfguided_restoration_fast_internal(
    int32_t *dgd, int width, int height, int dgd_stride, int32_t *dst,
    int dst_stride, int bit_depth, int sgr_params_idx, int radius_idx) {
  const sgr_params_type *const params = &av1_sgr_params[sgr_params_idx];
  const int r = params->r[radius_idx];
  const int width_ext = width + 2 * SGRPROJ_BORDER_HORZ;
  // Adjusting the stride of A and B here appears to avoid bad cache effects,
  // leading to a significant speed improvement.
  // We also align the stride to a multiple of 16 bytes, for consistency
  // with the SIMD version of this function.
  int buf_stride = ((width_ext + 3) & ~3) + 16;
  int32_t A_[RESTORATION_PROC_UNIT_PELS];
  int32_t B_[RESTORATION_PROC_UNIT_PELS];
  int32_t *A = A_;
  int32_t *B = B_;
  int i, j;
  calculate_intermediate_result(dgd, width, height, dgd_stride, bit_depth,
                                sgr_params_idx, radius_idx, 1, A, B);
  A += SGRPROJ_BORDER_VERT * buf_stride + SGRPROJ_BORDER_HORZ;
  B += SGRPROJ_BORDER_VERT * buf_stride + SGRPROJ_BORDER_HORZ;

  // Use the A[] and B[] arrays to calculate the filtered image
  (void)r;
  assert(r == 2);
  for (i = 0; i < height; ++i) {
    if (!(i & 1)) {  // even row
      for (j = 0; j < width; ++j) {
        const int k = i * buf_stride + j;
        const int l = i * dgd_stride + j;
        const int m = i * dst_stride + j;
        const int nb = 5;
        const int32_t a = (A[k - buf_stride] + A[k + buf_stride]) * 6 +
                          (A[k - 1 - buf_stride] + A[k - 1 + buf_stride] +
                           A[k + 1 - buf_stride] + A[k + 1 + buf_stride]) *
                              5;
        const int32_t b = (B[k - buf_stride] + B[k + buf_stride]) * 6 +
                          (B[k - 1 - buf_stride] + B[k - 1 + buf_stride] +
                           B[k + 1 - buf_stride] + B[k + 1 + buf_stride]) *
                              5;
        const int32_t v = a * dgd[l] + b;
        dst[m] =
            ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
      }
    } else {  // odd row
      for (j = 0; j < width; ++j) {
        const int k = i * buf_stride + j;
        const int l = i * dgd_stride + j;
        const int m = i * dst_stride + j;
        const int nb = 4;
        const int32_t a = A[k] * 6 + (A[k - 1] + A[k + 1]) * 5;
        const int32_t b = B[k] * 6 + (B[k - 1] + B[k + 1]) * 5;
        const int32_t v = a * dgd[l] + b;
        dst[m] =
            ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
      }
    }
  }
}

static void selfguided_restoration_internal(int32_t *dgd, int width, int height,
                                            int dgd_stride, int32_t *dst,
                                            int dst_stride, int bit_depth,
                                            int sgr_params_idx,
                                            int radius_idx) {
  const int width_ext = width + 2 * SGRPROJ_BORDER_HORZ;
  // Adjusting the stride of A and B here appears to avoid bad cache effects,
  // leading to a significant speed improvement.
  // We also align the stride to a multiple of 16 bytes, for consistency
  // with the SIMD version of this function.
  int buf_stride = ((width_ext + 3) & ~3) + 16;
  int32_t A_[RESTORATION_PROC_UNIT_PELS];
  int32_t B_[RESTORATION_PROC_UNIT_PELS];
  int32_t *A = A_;
  int32_t *B = B_;
  int i, j;
  calculate_intermediate_result(dgd, width, height, dgd_stride, bit_depth,
                                sgr_params_idx, radius_idx, 0, A, B);
  A += SGRPROJ_BORDER_VERT * buf_stride + SGRPROJ_BORDER_HORZ;
  B += SGRPROJ_BORDER_VERT * buf_stride + SGRPROJ_BORDER_HORZ;

  // Use the A[] and B[] arrays to calculate the filtered image
  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      const int k = i * buf_stride + j;
      const int l = i * dgd_stride + j;
      const int m = i * dst_stride + j;
      const int nb = 5;
      const int32_t a =
          (A[k] + A[k - 1] + A[k + 1] + A[k - buf_stride] + A[k + buf_stride]) *
              4 +
          (A[k - 1 - buf_stride] + A[k - 1 + buf_stride] +
           A[k + 1 - buf_stride] + A[k + 1 + buf_stride]) *
              3;
      const int32_t b =
          (B[k] + B[k - 1] + B[k + 1] + B[k - buf_stride] + B[k + buf_stride]) *
              4 +
          (B[k - 1 - buf_stride] + B[k - 1 + buf_stride] +
           B[k + 1 - buf_stride] + B[k + 1 + buf_stride]) *
              3;
      const int32_t v = a * dgd[l] + b;
      dst[m] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
    }
  }
}

int av1_selfguided_restoration_c(const uint8_t *dgd8, int width, int height,
                                 int dgd_stride, int32_t *flt0, int32_t *flt1,
                                 int flt_stride, int sgr_params_idx,
                                 int bit_depth, int highbd) {
  int32_t dgd32_[RESTORATION_PROC_UNIT_PELS];
  const int dgd32_stride = width + 2 * SGRPROJ_BORDER_HORZ;
  int32_t *dgd32 =
      dgd32_ + dgd32_stride * SGRPROJ_BORDER_VERT + SGRPROJ_BORDER_HORZ;

  if (highbd) {
    const uint16_t *dgd16 = CONVERT_TO_SHORTPTR(dgd8);
    for (int i = -SGRPROJ_BORDER_VERT; i < height + SGRPROJ_BORDER_VERT; ++i) {
      for (int j = -SGRPROJ_BORDER_HORZ; j < width + SGRPROJ_BORDER_HORZ; ++j) {
        dgd32[i * dgd32_stride + j] = dgd16[i * dgd_stride + j];
      }
    }
  } else {
    for (int i = -SGRPROJ_BORDER_VERT; i < height + SGRPROJ_BORDER_VERT; ++i) {
      for (int j = -SGRPROJ_BORDER_HORZ; j < width + SGRPROJ_BORDER_HORZ; ++j) {
        dgd32[i * dgd32_stride + j] = dgd8[i * dgd_stride + j];
      }
    }
  }

  const sgr_params_type *const params = &av1_sgr_params[sgr_params_idx];
  // If params->r == 0 we skip the corresponding filter. We only allow one of
  // the radii to be 0, as having both equal to 0 would be equivalent to
  // skipping SGR entirely.
  assert(!(params->r[0] == 0 && params->r[1] == 0));

  if (params->r[0] > 0)
    selfguided_restoration_fast_internal(dgd32, width, height, dgd32_stride,
                                         flt0, flt_stride, bit_depth,
                                         sgr_params_idx, 0);
  if (params->r[1] > 0)
    selfguided_restoration_internal(dgd32, width, height, dgd32_stride, flt1,
                                    flt_stride, bit_depth, sgr_params_idx, 1);
  return 0;
}

int av1_apply_selfguided_restoration_c(const uint8_t *dat8, int width,
                                       int height, int stride, int eps,
                                       const int *xqd, uint8_t *dst8,
                                       int dst_stride, int32_t *tmpbuf,
                                       int bit_depth, int highbd) {
  int32_t *flt0 = tmpbuf;
  int32_t *flt1 = flt0 + RESTORATION_UNITPELS_MAX;
  assert(width * height <= RESTORATION_UNITPELS_MAX);

  const int ret = av1_selfguided_restoration_c(
      dat8, width, height, stride, flt0, flt1, width, eps, bit_depth, highbd);
  if (ret != 0) return ret;
  const sgr_params_type *const params = &av1_sgr_params[eps];
  int xq[2];
  av1_decode_xq(xqd, xq, params);
  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; ++j) {
      const int k = i * width + j;
      uint8_t *dst8ij = dst8 + i * dst_stride + j;
      const uint8_t *dat8ij = dat8 + i * stride + j;

      const uint16_t pre_u = highbd ? *CONVERT_TO_SHORTPTR(dat8ij) : *dat8ij;
      const int32_t u = (int32_t)pre_u << SGRPROJ_RST_BITS;
      int32_t v = u << SGRPROJ_PRJ_BITS;
      // If params->r == 0 then we skipped the filtering in
      // av1_selfguided_restoration_c, i.e. flt[k] == u
      if (params->r[0] > 0) v += xq[0] * (flt0[k] - u);
      if (params->r[1] > 0) v += xq[1] * (flt1[k] - u);
      const int16_t w =
          (int16_t)ROUND_POWER_OF_TWO(v, SGRPROJ_PRJ_BITS + SGRPROJ_RST_BITS);

      const uint16_t out = clip_pixel_highbd(w, bit_depth);
      if (highbd)
        *CONVERT_TO_SHORTPTR(dst8ij) = out;
      else
        *dst8ij = (uint8_t)out;
    }
  }
  return 0;
}

static void sgrproj_filter_stripe(const RestorationUnitInfo *rui,
                                  int stripe_width, int stripe_height,
                                  int procunit_width, const uint8_t *src,
                                  int src_stride, uint8_t *dst, int dst_stride,
                                  int32_t *tmpbuf, int bit_depth,
                                  struct aom_internal_error_info *error_info) {
  (void)bit_depth;
  assert(bit_depth == 8);

  for (int j = 0; j < stripe_width; j += procunit_width) {
    int w = AOMMIN(procunit_width, stripe_width - j);
    if (av1_apply_selfguided_restoration(
            src + j, w, stripe_height, src_stride, rui->sgrproj_info.ep,
            rui->sgrproj_info.xqd, dst + j, dst_stride, tmpbuf, bit_depth,
            0) != 0) {
      aom_internal_error(
          error_info, AOM_CODEC_MEM_ERROR,
          "Error allocating buffer in av1_apply_selfguided_restoration");
    }
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
static void wiener_filter_stripe_highbd(
    const RestorationUnitInfo *rui, int stripe_width, int stripe_height,
    int procunit_width, const uint8_t *src8, int src_stride, uint8_t *dst8,
    int dst_stride, int32_t *tmpbuf, int bit_depth,
    struct aom_internal_error_info *error_info) {
  (void)tmpbuf;
  (void)error_info;
  const WienerConvolveParams conv_params = get_conv_params_wiener(bit_depth);

  for (int j = 0; j < stripe_width; j += procunit_width) {
    int w = AOMMIN(procunit_width, (stripe_width - j + 15) & ~15);
    const uint8_t *src8_p = src8 + j;
    uint8_t *dst8_p = dst8 + j;
    av1_highbd_wiener_convolve_add_src(src8_p, src_stride, dst8_p, dst_stride,
                                       rui->wiener_info.hfilter, 16,
                                       rui->wiener_info.vfilter, 16, w,
                                       stripe_height, &conv_params, bit_depth);
  }
}

static void sgrproj_filter_stripe_highbd(
    const RestorationUnitInfo *rui, int stripe_width, int stripe_height,
    int procunit_width, const uint8_t *src8, int src_stride, uint8_t *dst8,
    int dst_stride, int32_t *tmpbuf, int bit_depth,
    struct aom_internal_error_info *error_info) {
  for (int j = 0; j < stripe_width; j += procunit_width) {
    int w = AOMMIN(procunit_width, stripe_width - j);
    if (av1_apply_selfguided_restoration(
            src8 + j, w, stripe_height, src_stride, rui->sgrproj_info.ep,
            rui->sgrproj_info.xqd, dst8 + j, dst_stride, tmpbuf, bit_depth,
            1) != 0) {
      aom_internal_error(
          error_info, AOM_CODEC_MEM_ERROR,
          "Error allocating buffer in av1_apply_selfguided_restoration");
    }
  }
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

typedef void (*stripe_filter_fun)(const RestorationUnitInfo *rui,
                                  int stripe_width, int stripe_height,
                                  int procunit_width, const uint8_t *src,
                                  int src_stride, uint8_t *dst, int dst_stride,
                                  int32_t *tmpbuf, int bit_depth,
                                  struct aom_internal_error_info *error_info);

#if CONFIG_AV1_HIGHBITDEPTH
#define NUM_STRIPE_FILTERS 4
static const stripe_filter_fun stripe_filters[NUM_STRIPE_FILTERS] = {
  wiener_filter_stripe, sgrproj_filter_stripe, wiener_filter_stripe_highbd,
  sgrproj_filter_stripe_highbd
};
#else
#define NUM_STRIPE_FILTERS 2
static const stripe_filter_fun stripe_filters[NUM_STRIPE_FILTERS] = {
  wiener_filter_stripe, sgrproj_filter_stripe
};
#endif  // CONFIG_AV1_HIGHBITDEPTH

// Filter one restoration unit
void av1_loop_restoration_filter_unit(
    const RestorationTileLimits *limits, const RestorationUnitInfo *rui,
    const RestorationStripeBoundaries *rsb, RestorationLineBuffers *rlbs,
    int plane_w, int plane_h, int ss_x, int ss_y, int highbd, int bit_depth,
    uint8_t *data8, int stride, uint8_t *dst8, int dst_stride, int32_t *tmpbuf,
    int optimized_lr, struct aom_internal_error_info *error_info) {
  RestorationType unit_rtype = rui->restoration_type;

  int unit_h = limits->v_end - limits->v_start;
  int unit_w = limits->h_end - limits->h_start;
  uint8_t *data8_tl =
      data8 + limits->v_start * (ptrdiff_t)stride + limits->h_start;
  uint8_t *dst8_tl =
      dst8 + limits->v_start * (ptrdiff_t)dst_stride + limits->h_start;

  if (unit_rtype == RESTORE_NONE) {
    copy_rest_unit(unit_w, unit_h, data8_tl, stride, dst8_tl, dst_stride,
                   highbd);
    return;
  }

  const int filter_idx = 2 * highbd + (unit_rtype == RESTORE_SGRPROJ);
  assert(filter_idx < NUM_STRIPE_FILTERS);
  const stripe_filter_fun stripe_filter = stripe_filters[filter_idx];

  const int procunit_width = RESTORATION_PROC_UNIT_SIZE >> ss_x;

  // Filter the whole image one stripe at a time
  RestorationTileLimits remaining_stripes = *limits;
  int i = 0;
  while (i < unit_h) {
    int copy_above, copy_below;
    remaining_stripes.v_start = limits->v_start + i;

    get_stripe_boundary_info(&remaining_stripes, plane_w, plane_h, ss_y,
                             &copy_above, &copy_below);

    const int full_stripe_height = RESTORATION_PROC_UNIT_SIZE >> ss_y;
    const int runit_offset = RESTORATION_UNIT_OFFSET >> ss_y;

    // Work out where this stripe's boundaries are within
    // rsb->stripe_boundary_{above,below}
    const int frame_stripe =
        (remaining_stripes.v_start + runit_offset) / full_stripe_height;
    const int rsb_row = RESTORATION_CTX_VERT * frame_stripe;

    // Calculate this stripe's height, based on two rules:
    // * The topmost stripe in the frame is 8 luma pixels shorter than usual.
    // * We can't extend past the end of the current restoration unit
    const int nominal_stripe_height =
        full_stripe_height - ((frame_stripe == 0) ? runit_offset : 0);
    const int h = AOMMIN(nominal_stripe_height,
                         remaining_stripes.v_end - remaining_stripes.v_start);

    setup_processing_stripe_boundary(&remaining_stripes, rsb, rsb_row, highbd,
                                     h, data8, stride, rlbs, copy_above,
                                     copy_below, optimized_lr);

    stripe_filter(rui, unit_w, h, procunit_width, data8_tl + i * stride, stride,
                  dst8_tl + i * dst_stride, dst_stride, tmpbuf, bit_depth,
                  error_info);

    restore_processing_stripe_boundary(&remaining_stripes, rlbs, highbd, h,
                                       data8, stride, copy_above, copy_below,
                                       optimized_lr);

    i += h;
  }
}

static void filter_frame_on_unit(const RestorationTileLimits *limits,
                                 int rest_unit_idx, void *priv, int32_t *tmpbuf,
                                 RestorationLineBuffers *rlbs,
                                 struct aom_internal_error_info *error_info) {
  FilterFrameCtxt *ctxt = (FilterFrameCtxt *)priv;
  const RestorationInfo *rsi = ctxt->rsi;

  av1_loop_restoration_filter_unit(
      limits, &rsi->unit_info[rest_unit_idx], &rsi->boundaries, rlbs,
      ctxt->plane_w, ctxt->plane_h, ctxt->ss_x, ctxt->ss_y, ctxt->highbd,
      ctxt->bit_depth, ctxt->data8, ctxt->data_stride, ctxt->dst8,
      ctxt->dst_stride, tmpbuf, rsi->optimized_lr, error_info);
}

void av1_loop_restoration_filter_frame_init(AV1LrStruct *lr_ctxt,
                                            YV12_BUFFER_CONFIG *frame,
                                            AV1_COMMON *cm, int optimized_lr,
                                            int num_planes) {
  const SequenceHeader *const seq_params = cm->seq_params;
  const int bit_depth = seq_params->bit_depth;
  const int highbd = seq_params->use_highbitdepth;
  lr_ctxt->dst = &cm->rst_frame;

  const int frame_width = frame->crop_widths[0];
  const int frame_height = frame->crop_heights[0];
  if (aom_realloc_frame_buffer(
          lr_ctxt->dst, frame_width, frame_height, seq_params->subsampling_x,
          seq_params->subsampling_y, highbd, AOM_RESTORATION_FRAME_BORDER,
          cm->features.byte_alignment, NULL, NULL, NULL, false,
          0) != AOM_CODEC_OK)
    aom_internal_error(cm->error, AOM_CODEC_MEM_ERROR,
                       "Failed to allocate restoration dst buffer");

  lr_ctxt->on_rest_unit = filter_frame_on_unit;
  lr_ctxt->frame = frame;
  for (int plane = 0; plane < num_planes; ++plane) {
    RestorationInfo *rsi = &cm->rst_info[plane];
    RestorationType rtype = rsi->frame_restoration_type;
    rsi->optimized_lr = optimized_lr;
    lr_ctxt->ctxt[plane].rsi = rsi;

    if (rtype == RESTORE_NONE) {
      continue;
    }

    const int is_uv = plane > 0;
    int plane_w, plane_h;
    av1_get_upsampled_plane_size(cm, is_uv, &plane_w, &plane_h);
    assert(plane_w == frame->crop_widths[is_uv]);
    assert(plane_h == frame->crop_heights[is_uv]);

    av1_extend_frame(frame->buffers[plane], plane_w, plane_h,
                     frame->strides[is_uv], RESTORATION_BORDER,
                     RESTORATION_BORDER, highbd);

    FilterFrameCtxt *lr_plane_ctxt = &lr_ctxt->ctxt[plane];
    lr_plane_ctxt->ss_x = is_uv && seq_params->subsampling_x;
    lr_plane_ctxt->ss_y = is_uv && seq_params->subsampling_y;
    lr_plane_ctxt->plane_w = plane_w;
    lr_plane_ctxt->plane_h = plane_h;
    lr_plane_ctxt->highbd = highbd;
    lr_plane_ctxt->bit_depth = bit_depth;
    lr_plane_ctxt->data8 = frame->buffers[plane];
    lr_plane_ctxt->dst8 = lr_ctxt->dst->buffers[plane];
    lr_plane_ctxt->data_stride = frame->strides[is_uv];
    lr_plane_ctxt->dst_stride = lr_ctxt->dst->strides[is_uv];
  }
}

void av1_loop_restoration_copy_planes(AV1LrStruct *loop_rest_ctxt,
                                      AV1_COMMON *cm, int num_planes) {
  typedef void (*copy_fun)(const YV12_BUFFER_CONFIG *src_ybc,
                           YV12_BUFFER_CONFIG *dst_ybc, int hstart, int hend,
                           int vstart, int vend);
  static const copy_fun copy_funs[3] = { aom_yv12_partial_coloc_copy_y,
                                         aom_yv12_partial_coloc_copy_u,
                                         aom_yv12_partial_coloc_copy_v };
  assert(num_planes <= 3);
  for (int plane = 0; plane < num_planes; ++plane) {
    if (cm->rst_info[plane].frame_restoration_type == RESTORE_NONE) continue;
    FilterFrameCtxt *lr_plane_ctxt = &loop_rest_ctxt->ctxt[plane];
    copy_funs[plane](loop_rest_ctxt->dst, loop_rest_ctxt->frame, 0,
                     lr_plane_ctxt->plane_w, 0, lr_plane_ctxt->plane_h);
  }
}

static void foreach_rest_unit_in_planes(AV1LrStruct *lr_ctxt, AV1_COMMON *cm,
                                        int num_planes) {
  FilterFrameCtxt *ctxt = lr_ctxt->ctxt;

  for (int plane = 0; plane < num_planes; ++plane) {
    if (cm->rst_info[plane].frame_restoration_type == RESTORE_NONE) {
      continue;
    }

    av1_foreach_rest_unit_in_plane(cm, plane, lr_ctxt->on_rest_unit,
                                   &ctxt[plane], cm->rst_tmpbuf, cm->rlbs);
  }
}

void av1_loop_restoration_filter_frame(YV12_BUFFER_CONFIG *frame,
                                       AV1_COMMON *cm, int optimized_lr,
                                       void *lr_ctxt) {
  assert(!cm->features.all_lossless);
  const int num_planes = av1_num_planes(cm);

  AV1LrStruct *loop_rest_ctxt = (AV1LrStruct *)lr_ctxt;

  av1_loop_restoration_filter_frame_init(loop_rest_ctxt, frame, cm,
                                         optimized_lr, num_planes);

  foreach_rest_unit_in_planes(loop_rest_ctxt, cm, num_planes);

  av1_loop_restoration_copy_planes(loop_rest_ctxt, cm, num_planes);
}

void av1_foreach_rest_unit_in_row(
    RestorationTileLimits *limits, int plane_w,
    rest_unit_visitor_t on_rest_unit, int row_number, int unit_size,
    int hnum_rest_units, int vnum_rest_units, int plane, void *priv,
    int32_t *tmpbuf, RestorationLineBuffers *rlbs, sync_read_fn_t on_sync_read,
    sync_write_fn_t on_sync_write, struct AV1LrSyncData *const lr_sync,
    struct aom_internal_error_info *error_info) {
  const int ext_size = unit_size * 3 / 2;
  int x0 = 0, j = 0;
  while (x0 < plane_w) {
    int remaining_w = plane_w - x0;
    int w = (remaining_w < ext_size) ? remaining_w : unit_size;

    limits->h_start = x0;
    limits->h_end = x0 + w;
    assert(limits->h_end <= plane_w);

    const int unit_idx = row_number * hnum_rest_units + j;

    // No sync for even numbered rows
    // For odd numbered rows, Loop Restoration of current block requires the LR
    // of top-right and bottom-right blocks to be completed

    // top-right sync
    on_sync_read(lr_sync, row_number, j, plane);
    if ((row_number + 1) < vnum_rest_units)
      // bottom-right sync
      on_sync_read(lr_sync, row_number + 2, j, plane);

#if CONFIG_MULTITHREAD
    if (lr_sync && lr_sync->num_workers > 1) {
      pthread_mutex_lock(lr_sync->job_mutex);
      const bool lr_mt_exit = lr_sync->lr_mt_exit;
      pthread_mutex_unlock(lr_sync->job_mutex);
      // Exit in case any worker has encountered an error.
      if (lr_mt_exit) return;
    }
#endif

    on_rest_unit(limits, unit_idx, priv, tmpbuf, rlbs, error_info);

    on_sync_write(lr_sync, row_number, j, hnum_rest_units, plane);

    x0 += w;
    ++j;
  }
}

void av1_lr_sync_read_dummy(void *const lr_sync, int r, int c, int plane) {
  (void)lr_sync;
  (void)r;
  (void)c;
  (void)plane;
}

void av1_lr_sync_write_dummy(void *const lr_sync, int r, int c,
                             const int sb_cols, int plane) {
  (void)lr_sync;
  (void)r;
  (void)c;
  (void)sb_cols;
  (void)plane;
}

void av1_foreach_rest_unit_in_plane(const struct AV1Common *cm, int plane,
                                    rest_unit_visitor_t on_rest_unit,
                                    void *priv, int32_t *tmpbuf,
                                    RestorationLineBuffers *rlbs) {
  const RestorationInfo *rsi = &cm->rst_info[plane];
  const int hnum_rest_units = rsi->horz_units;
  const int vnum_rest_units = rsi->vert_units;
  const int unit_size = rsi->restoration_unit_size;

  const int is_uv = plane > 0;
  const int ss_y = is_uv && cm->seq_params->subsampling_y;
  const int ext_size = unit_size * 3 / 2;
  int plane_w, plane_h;
  av1_get_upsampled_plane_size(cm, is_uv, &plane_w, &plane_h);

  int y0 = 0, i = 0;
  while (y0 < plane_h) {
    int remaining_h = plane_h - y0;
    int h = (remaining_h < ext_size) ? remaining_h : unit_size;

    RestorationTileLimits limits;
    limits.v_start = y0;
    limits.v_end = y0 + h;
    assert(limits.v_end <= plane_h);
    // Offset upwards to align with the restoration processing stripe
    const int voffset = RESTORATION_UNIT_OFFSET >> ss_y;
    limits.v_start = AOMMAX(0, limits.v_start - voffset);
    if (limits.v_end < plane_h) limits.v_end -= voffset;

    av1_foreach_rest_unit_in_row(&limits, plane_w, on_rest_unit, i, unit_size,
                                 hnum_rest_units, vnum_rest_units, plane, priv,
                                 tmpbuf, rlbs, av1_lr_sync_read_dummy,
                                 av1_lr_sync_write_dummy, NULL, cm->error);

    y0 += h;
    ++i;
  }
}

int av1_loop_restoration_corners_in_sb(const struct AV1Common *cm, int plane,
                                       int mi_row, int mi_col, BLOCK_SIZE bsize,
                                       int *rcol0, int *rcol1, int *rrow0,
                                       int *rrow1) {
  assert(rcol0 && rcol1 && rrow0 && rrow1);

  if (bsize != cm->seq_params->sb_size) return 0;

  assert(!cm->features.all_lossless);

  const int is_uv = plane > 0;

  // Compute the mi-unit corners of the superblock
  const int mi_row0 = mi_row;
  const int mi_col0 = mi_col;
  const int mi_row1 = mi_row0 + mi_size_high[bsize];
  const int mi_col1 = mi_col0 + mi_size_wide[bsize];

  const RestorationInfo *rsi = &cm->rst_info[plane];
  const int size = rsi->restoration_unit_size;
  const int horz_units = rsi->horz_units;
  const int vert_units = rsi->vert_units;

  // The size of an MI-unit on this plane of the image
  const int ss_x = is_uv && cm->seq_params->subsampling_x;
  const int ss_y = is_uv && cm->seq_params->subsampling_y;
  const int mi_size_x = MI_SIZE >> ss_x;
  const int mi_size_y = MI_SIZE >> ss_y;

  // Write m for the relative mi column or row, D for the superres denominator
  // and N for the superres numerator. If u is the upscaled pixel offset then
  // we can write the downscaled pixel offset in two ways as:
  //
  //   MI_SIZE * m = N / D u
  //
  // from which we get u = D * MI_SIZE * m / N
  const int mi_to_num_x = av1_superres_scaled(cm)
                              ? mi_size_x * cm->superres_scale_denominator
                              : mi_size_x;
  const int mi_to_num_y = mi_size_y;
  const int denom_x = av1_superres_scaled(cm) ? size * SCALE_NUMERATOR : size;
  const int denom_y = size;

  const int rnd_x = denom_x - 1;
  const int rnd_y = denom_y - 1;

  // rcol0/rrow0 should be the first column/row of restoration units that
  // doesn't start left/below of mi_col/mi_row. For this calculation, we need
  // to round up the division (if the sb starts at runit column 10.1, the first
  // matching runit has column index 11)
  *rcol0 = (mi_col0 * mi_to_num_x + rnd_x) / denom_x;
  *rrow0 = (mi_row0 * mi_to_num_y + rnd_y) / denom_y;

  // rel_col1/rel_row1 is the equivalent calculation, but for the superblock
  // below-right. If we're at the bottom or right of the frame, this restoration
  // unit might not exist, in which case we'll clamp accordingly.
  *rcol1 = AOMMIN((mi_col1 * mi_to_num_x + rnd_x) / denom_x, horz_units);
  *rrow1 = AOMMIN((mi_row1 * mi_to_num_y + rnd_y) / denom_y, vert_units);

  return *rcol0 < *rcol1 && *rrow0 < *rrow1;
}

// Extend to left and right
static void extend_lines(uint8_t *buf, int width, int height, int stride,
                         int extend, int use_highbitdepth) {
  for (int i = 0; i < height; ++i) {
    if (use_highbitdepth) {
      uint16_t *buf16 = (uint16_t *)buf;
      aom_memset16(buf16 - extend, buf16[0], extend);
      aom_memset16(buf16 + width, buf16[width - 1], extend);
    } else {
      memset(buf - extend, buf[0], extend);
      memset(buf + width, buf[width - 1], extend);
    }
    buf += stride;
  }
}

static void save_deblock_boundary_lines(
    const YV12_BUFFER_CONFIG *frame, const AV1_COMMON *cm, int plane, int row,
    int stripe, int use_highbd, int is_above,
    RestorationStripeBoundaries *boundaries) {
  const int is_uv = plane > 0;
  const uint8_t *src_buf = REAL_PTR(use_highbd, frame->buffers[plane]);
  const int src_stride = frame->strides[is_uv] << use_highbd;
  const uint8_t *src_rows = src_buf + row * (ptrdiff_t)src_stride;

  uint8_t *bdry_buf = is_above ? boundaries->stripe_boundary_above
                               : boundaries->stripe_boundary_below;
  uint8_t *bdry_start = bdry_buf + (RESTORATION_EXTRA_HORZ << use_highbd);
  const int bdry_stride = boundaries->stripe_boundary_stride << use_highbd;
  uint8_t *bdry_rows = bdry_start + RESTORATION_CTX_VERT * stripe * bdry_stride;

  // There is a rare case in which a processing stripe can end 1px above the
  // crop border. In this case, we do want to use deblocked pixels from below
  // the stripe (hence why we ended up in this function), but instead of
  // fetching 2 "below" rows we need to fetch one and duplicate it.
  // This is equivalent to clamping the sample locations against the crop border
  const int lines_to_save =
      AOMMIN(RESTORATION_CTX_VERT, frame->crop_heights[is_uv] - row);
  assert(lines_to_save == 1 || lines_to_save == 2);

  int upscaled_width;
  int line_bytes;
  if (av1_superres_scaled(cm)) {
    const int ss_x = is_uv && cm->seq_params->subsampling_x;
    upscaled_width = (cm->superres_upscaled_width + ss_x) >> ss_x;
    line_bytes = upscaled_width << use_highbd;
    if (use_highbd)
      av1_upscale_normative_rows(
          cm, CONVERT_TO_BYTEPTR(src_rows), frame->strides[is_uv],
          CONVERT_TO_BYTEPTR(bdry_rows), boundaries->stripe_boundary_stride,
          plane, lines_to_save);
    else
      av1_upscale_normative_rows(cm, src_rows, frame->strides[is_uv], bdry_rows,
                                 boundaries->stripe_boundary_stride, plane,
                                 lines_to_save);
  } else {
    upscaled_width = frame->crop_widths[is_uv];
    line_bytes = upscaled_width << use_highbd;
    for (int i = 0; i < lines_to_save; i++) {
      memcpy(bdry_rows + i * bdry_stride, src_rows + i * src_stride,
             line_bytes);
    }
  }
  // If we only saved one line, then copy it into the second line buffer
  if (lines_to_save == 1)
    memcpy(bdry_rows + bdry_stride, bdry_rows, line_bytes);

  extend_lines(bdry_rows, upscaled_width, RESTORATION_CTX_VERT, bdry_stride,
               RESTORATION_EXTRA_HORZ, use_highbd);
}

static void save_cdef_boundary_lines(const YV12_BUFFER_CONFIG *frame,
                                     const AV1_COMMON *cm, int plane, int row,
                                     int stripe, int use_highbd, int is_above,
                                     RestorationStripeBoundaries *boundaries) {
  const int is_uv = plane > 0;
  const uint8_t *src_buf = REAL_PTR(use_highbd, frame->buffers[plane]);
  const int src_stride = frame->strides[is_uv] << use_highbd;
  const uint8_t *src_rows = src_buf + row * (ptrdiff_t)src_stride;

  uint8_t *bdry_buf = is_above ? boundaries->stripe_boundary_above
                               : boundaries->stripe_boundary_below;
  uint8_t *bdry_start = bdry_buf + (RESTORATION_EXTRA_HORZ << use_highbd);
  const int bdry_stride = boundaries->stripe_boundary_stride << use_highbd;
  uint8_t *bdry_rows = bdry_start + RESTORATION_CTX_VERT * stripe * bdry_stride;
  const int src_width = frame->crop_widths[is_uv];

  // At the point where this function is called, we've already applied
  // superres. So we don't need to extend the lines here, we can just
  // pull directly from the topmost row of the upscaled frame.
  const int ss_x = is_uv && cm->seq_params->subsampling_x;
  const int upscaled_width = av1_superres_scaled(cm)
                                 ? (cm->superres_upscaled_width + ss_x) >> ss_x
                                 : src_width;
  const int line_bytes = upscaled_width << use_highbd;
  for (int i = 0; i < RESTORATION_CTX_VERT; i++) {
    // Copy the line at 'src_rows' into both context lines
    memcpy(bdry_rows + i * bdry_stride, src_rows, line_bytes);
  }
  extend_lines(bdry_rows, upscaled_width, RESTORATION_CTX_VERT, bdry_stride,
               RESTORATION_EXTRA_HORZ, use_highbd);
}

static void save_boundary_lines(const YV12_BUFFER_CONFIG *frame, int use_highbd,
                                int plane, AV1_COMMON *cm, int after_cdef) {
  const int is_uv = plane > 0;
  const int ss_y = is_uv && cm->seq_params->subsampling_y;
  const int stripe_height = RESTORATION_PROC_UNIT_SIZE >> ss_y;
  const int stripe_off = RESTORATION_UNIT_OFFSET >> ss_y;

  int plane_w, plane_h;
  av1_get_upsampled_plane_size(cm, is_uv, &plane_w, &plane_h);

  RestorationStripeBoundaries *boundaries = &cm->rst_info[plane].boundaries;

  const int plane_height = ROUND_POWER_OF_TWO(cm->height, ss_y);

  int stripe_idx;
  for (stripe_idx = 0;; ++stripe_idx) {
    const int rel_y0 = AOMMAX(0, stripe_idx * stripe_height - stripe_off);
    const int y0 = rel_y0;
    if (y0 >= plane_h) break;

    const int rel_y1 = (stripe_idx + 1) * stripe_height - stripe_off;
    const int y1 = AOMMIN(rel_y1, plane_h);

    // Extend using CDEF pixels at the top and bottom of the frame,
    // and deblocked pixels at internal stripe boundaries
    const int use_deblock_above = (stripe_idx > 0);
    const int use_deblock_below = (y1 < plane_height);

    if (!after_cdef) {
      // Save deblocked context at internal stripe boundaries
      if (use_deblock_above) {
        save_deblock_boundary_lines(frame, cm, plane, y0 - RESTORATION_CTX_VERT,
                                    stripe_idx, use_highbd, 1, boundaries);
      }
      if (use_deblock_below) {
        save_deblock_boundary_lines(frame, cm, plane, y1, stripe_idx,
                                    use_highbd, 0, boundaries);
      }
    } else {
      // Save CDEF context at frame boundaries
      if (!use_deblock_above) {
        save_cdef_boundary_lines(frame, cm, plane, y0, stripe_idx, use_highbd,
                                 1, boundaries);
      }
      if (!use_deblock_below) {
        save_cdef_boundary_lines(frame, cm, plane, y1 - 1, stripe_idx,
                                 use_highbd, 0, boundaries);
      }
    }
  }
}

// For each RESTORATION_PROC_UNIT_SIZE pixel high stripe, save 4 scan
// lines to be used as boundary in the loop restoration process. The
// lines are saved in rst_internal.stripe_boundary_lines
void av1_loop_restoration_save_boundary_lines(const YV12_BUFFER_CONFIG *frame,
                                              AV1_COMMON *cm, int after_cdef) {
  const int num_planes = av1_num_planes(cm);
  const int use_highbd = cm->seq_params->use_highbitdepth;
  for (int p = 0; p < num_planes; ++p) {
    save_boundary_lines(frame, use_highbd, p, cm, after_cdef);
  }
}
