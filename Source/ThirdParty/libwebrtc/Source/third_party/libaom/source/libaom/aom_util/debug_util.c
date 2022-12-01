/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "aom_util/debug_util.h"

static int frame_idx_w = 0;

static int frame_idx_r = 0;

void aom_bitstream_queue_set_frame_write(int frame_idx) {
  frame_idx_w = frame_idx;
}

int aom_bitstream_queue_get_frame_write(void) { return frame_idx_w; }

void aom_bitstream_queue_set_frame_read(int frame_idx) {
  frame_idx_r = frame_idx;
}

int aom_bitstream_queue_get_frame_read(void) { return frame_idx_r; }

#if CONFIG_BITSTREAM_DEBUG
#define QUEUE_MAX_SIZE 2000000
static int result_queue[QUEUE_MAX_SIZE];
static int nsymbs_queue[QUEUE_MAX_SIZE];
static aom_cdf_prob cdf_queue[QUEUE_MAX_SIZE][16];

static int queue_r = 0;
static int queue_w = 0;
static int queue_prev_w = -1;
static int skip_r = 0;
static int skip_w = 0;

void bitstream_queue_set_skip_write(int skip) { skip_w = skip; }

void bitstream_queue_set_skip_read(int skip) { skip_r = skip; }

void bitstream_queue_record_write(void) { queue_prev_w = queue_w; }

void bitstream_queue_reset_write(void) { queue_w = queue_prev_w; }

int bitstream_queue_get_write(void) { return queue_w; }

int bitstream_queue_get_read(void) { return queue_r; }

void bitstream_queue_pop(int *result, aom_cdf_prob *cdf, int *nsymbs) {
  if (!skip_r) {
    if (queue_w == queue_r) {
      printf("buffer underflow queue_w %d queue_r %d\n", queue_w, queue_r);
      assert(0);
    }
    *result = result_queue[queue_r];
    *nsymbs = nsymbs_queue[queue_r];
    memcpy(cdf, cdf_queue[queue_r], *nsymbs * sizeof(*cdf));
    queue_r = (queue_r + 1) % QUEUE_MAX_SIZE;
  }
}

void bitstream_queue_push(int result, const aom_cdf_prob *cdf, int nsymbs) {
  // If you observe a CDF error:
  // - Set 'debug_cdf_mismatch' to true
  // - Set target_frame_idx_r and target_queue_r to where CDF error was reported
  // - Set a breakpoint in debugger at the 'fprintf' below.
  const bool debug_cdf_mismatch = false;
  if (debug_cdf_mismatch) {
    int target_frame_idx_r = 1;
    int target_queue_r = 18005;
    if (frame_idx_w == target_frame_idx_r && queue_w == target_queue_r) {
      fprintf(stderr, "\n *** bitstream queue at frame_idx_w %d queue_w %d\n",
              frame_idx_w, queue_w);
    }
  }
  if (!skip_w) {
    result_queue[queue_w] = result;
    nsymbs_queue[queue_w] = nsymbs;
    memcpy(cdf_queue[queue_w], cdf, nsymbs * sizeof(*cdf));
    queue_w = (queue_w + 1) % QUEUE_MAX_SIZE;
    if (queue_w == queue_r) {
      printf("buffer overflow queue_w %d queue_r %d\n", queue_w, queue_r);
      assert(0);
    }
  }
}
#endif  // CONFIG_BITSTREAM_DEBUG

#if CONFIG_MISMATCH_DEBUG
static int frame_buf_idx_r = 0;
static int frame_buf_idx_w = 0;
static int max_frame_buf_num = 5;
#define MAX_FRAME_STRIDE 1280
#define MAX_FRAME_HEIGHT 720
static uint16_t
    frame_pre[5][3][MAX_FRAME_STRIDE * MAX_FRAME_HEIGHT];  // prediction only
static uint16_t
    frame_tx[5][3][MAX_FRAME_STRIDE * MAX_FRAME_HEIGHT];  // prediction + txfm
static int frame_stride = MAX_FRAME_STRIDE;
static int frame_height = MAX_FRAME_HEIGHT;
static int frame_size = MAX_FRAME_STRIDE * MAX_FRAME_HEIGHT;
void mismatch_move_frame_idx_w() {
  frame_buf_idx_w = (frame_buf_idx_w + 1) % max_frame_buf_num;
  if (frame_buf_idx_w == frame_buf_idx_r) {
    printf("frame_buf overflow\n");
    assert(0);
  }
}

void mismatch_reset_frame(int num_planes) {
  for (int plane = 0; plane < num_planes; ++plane) {
    memset(frame_pre[frame_buf_idx_w][plane], 0,
           sizeof(frame_pre[frame_buf_idx_w][plane][0]) * frame_size);
    memset(frame_tx[frame_buf_idx_w][plane], 0,
           sizeof(frame_tx[frame_buf_idx_w][plane][0]) * frame_size);
  }
}

void mismatch_move_frame_idx_r() {
  if (frame_buf_idx_w == frame_buf_idx_r) {
    printf("frame_buf underflow\n");
    assert(0);
  }
  frame_buf_idx_r = (frame_buf_idx_r + 1) % max_frame_buf_num;
}

void mismatch_record_block_pre(const uint8_t *src, int src_stride,
                               int frame_offset, int plane, int pixel_c,
                               int pixel_r, int blk_w, int blk_h, int highbd) {
  if (pixel_c + blk_w >= frame_stride || pixel_r + blk_h >= frame_height) {
    printf("frame_buf undersized\n");
    assert(0);
  }

  const uint16_t *src16 = highbd ? CONVERT_TO_SHORTPTR(src) : NULL;
  for (int r = 0; r < blk_h; ++r) {
    for (int c = 0; c < blk_w; ++c) {
      frame_pre[frame_buf_idx_w][plane]
               [(r + pixel_r) * frame_stride + c + pixel_c] =
                   src16 ? src16[r * src_stride + c] : src[r * src_stride + c];
    }
  }
#if 0
  int ref_frame_idx = 3;
  int ref_frame_offset = 4;
  int ref_plane = 1;
  int ref_pixel_c = 162;
  int ref_pixel_r = 16;
  if (frame_idx_w == ref_frame_idx && plane == ref_plane &&
      frame_offset == ref_frame_offset && ref_pixel_c >= pixel_c &&
      ref_pixel_c < pixel_c + blk_w && ref_pixel_r >= pixel_r &&
      ref_pixel_r < pixel_r + blk_h) {
    printf(
        "\nrecord_block_pre frame_idx %d frame_offset %d plane %d pixel_c %d pixel_r %d blk_w "
        "%d blk_h %d\n",
        frame_idx_w, frame_offset, plane, pixel_c, pixel_r, blk_w, blk_h);
  }
#endif
}
void mismatch_record_block_tx(const uint8_t *src, int src_stride,
                              int frame_offset, int plane, int pixel_c,
                              int pixel_r, int blk_w, int blk_h, int highbd) {
  if (pixel_c + blk_w >= frame_stride || pixel_r + blk_h >= frame_height) {
    printf("frame_buf undersized\n");
    assert(0);
  }

  const uint16_t *src16 = highbd ? CONVERT_TO_SHORTPTR(src) : NULL;
  for (int r = 0; r < blk_h; ++r) {
    for (int c = 0; c < blk_w; ++c) {
      frame_tx[frame_buf_idx_w][plane]
              [(r + pixel_r) * frame_stride + c + pixel_c] =
                  src16 ? src16[r * src_stride + c] : src[r * src_stride + c];
    }
  }
#if 0
  int ref_frame_idx = 3;
  int ref_frame_offset = 4;
  int ref_plane = 1;
  int ref_pixel_c = 162;
  int ref_pixel_r = 16;
  if (frame_idx_w == ref_frame_idx && plane == ref_plane && frame_offset == ref_frame_offset &&
      ref_pixel_c >= pixel_c && ref_pixel_c < pixel_c + blk_w &&
      ref_pixel_r >= pixel_r && ref_pixel_r < pixel_r + blk_h) {
    printf(
        "\nrecord_block_tx frame_idx %d frame_offset %d plane %d pixel_c %d pixel_r %d blk_w "
        "%d blk_h %d\n",
        frame_idx_w, frame_offset, plane, pixel_c, pixel_r, blk_w, blk_h);
  }
#endif
}
void mismatch_check_block_pre(const uint8_t *src, int src_stride,
                              int frame_offset, int plane, int pixel_c,
                              int pixel_r, int blk_w, int blk_h, int highbd) {
  if (pixel_c + blk_w >= frame_stride || pixel_r + blk_h >= frame_height) {
    printf("frame_buf undersized\n");
    assert(0);
  }

  const uint16_t *src16 = highbd ? CONVERT_TO_SHORTPTR(src) : NULL;
  int mismatch = 0;
  for (int r = 0; r < blk_h; ++r) {
    for (int c = 0; c < blk_w; ++c) {
      if (frame_pre[frame_buf_idx_r][plane]
                   [(r + pixel_r) * frame_stride + c + pixel_c] !=
          (uint16_t)(src16 ? src16[r * src_stride + c]
                           : src[r * src_stride + c])) {
        mismatch = 1;
      }
    }
  }
  if (mismatch) {
    printf(
        "\ncheck_block_pre failed frame_idx %d frame_offset %d plane %d "
        "pixel_c %d pixel_r "
        "%d blk_w %d blk_h %d\n",
        frame_idx_r, frame_offset, plane, pixel_c, pixel_r, blk_w, blk_h);
    printf("enc\n");
    for (int rr = 0; rr < blk_h; ++rr) {
      for (int cc = 0; cc < blk_w; ++cc) {
        printf("%d ", frame_pre[frame_buf_idx_r][plane]
                               [(rr + pixel_r) * frame_stride + cc + pixel_c]);
      }
      printf("\n");
    }

    printf("dec\n");
    for (int rr = 0; rr < blk_h; ++rr) {
      for (int cc = 0; cc < blk_w; ++cc) {
        printf("%d ",
               src16 ? src16[rr * src_stride + cc] : src[rr * src_stride + cc]);
      }
      printf("\n");
    }
    assert(0);
  }
}
void mismatch_check_block_tx(const uint8_t *src, int src_stride,
                             int frame_offset, int plane, int pixel_c,
                             int pixel_r, int blk_w, int blk_h, int highbd) {
  if (pixel_c + blk_w >= frame_stride || pixel_r + blk_h >= frame_height) {
    printf("frame_buf undersized\n");
    assert(0);
  }

  const uint16_t *src16 = highbd ? CONVERT_TO_SHORTPTR(src) : NULL;
  int mismatch = 0;
  for (int r = 0; r < blk_h; ++r) {
    for (int c = 0; c < blk_w; ++c) {
      if (frame_tx[frame_buf_idx_r][plane]
                  [(r + pixel_r) * frame_stride + c + pixel_c] !=
          (uint16_t)(src16 ? src16[r * src_stride + c]
                           : src[r * src_stride + c])) {
        mismatch = 1;
      }
    }
  }
  if (mismatch) {
    printf(
        "\ncheck_block_tx failed frame_idx %d frame_offset %d plane %d pixel_c "
        "%d pixel_r "
        "%d blk_w %d blk_h %d\n",
        frame_idx_r, frame_offset, plane, pixel_c, pixel_r, blk_w, blk_h);
    printf("enc\n");
    for (int rr = 0; rr < blk_h; ++rr) {
      for (int cc = 0; cc < blk_w; ++cc) {
        printf("%d ", frame_tx[frame_buf_idx_r][plane]
                              [(rr + pixel_r) * frame_stride + cc + pixel_c]);
      }
      printf("\n");
    }

    printf("dec\n");
    for (int rr = 0; rr < blk_h; ++rr) {
      for (int cc = 0; cc < blk_w; ++cc) {
        printf("%d ",
               src16 ? src16[rr * src_stride + cc] : src[rr * src_stride + cc]);
      }
      printf("\n");
    }
    assert(0);
  }
}
#endif  // CONFIG_MISMATCH_DEBUG
