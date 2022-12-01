/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

// Utility functions used by encoder binaries.

#include "examples/encoder_util.h"

#include <assert.h>
#include <string.h>

#include "aom/aom_integer.h"

#define mmin(a, b) ((a) < (b) ? (a) : (b))

static void find_mismatch_plane(const aom_image_t *const img1,
                                const aom_image_t *const img2, int plane,
                                int use_highbitdepth, int loc[4]) {
  const unsigned char *const p1 = img1->planes[plane];
  const int p1_stride = img1->stride[plane] >> use_highbitdepth;
  const unsigned char *const p2 = img2->planes[plane];
  const int p2_stride = img2->stride[plane] >> use_highbitdepth;
  const uint32_t bsize = 64;
  const int is_y_plane = (plane == AOM_PLANE_Y);
  const uint32_t bsizex = is_y_plane ? bsize : bsize >> img1->x_chroma_shift;
  const uint32_t bsizey = is_y_plane ? bsize : bsize >> img1->y_chroma_shift;
  const uint32_t c_w =
      is_y_plane ? img1->d_w
                 : (img1->d_w + img1->x_chroma_shift) >> img1->x_chroma_shift;
  const uint32_t c_h =
      is_y_plane ? img1->d_h
                 : (img1->d_h + img1->y_chroma_shift) >> img1->y_chroma_shift;
  assert(img1->d_w == img2->d_w && img1->d_h == img2->d_h);
  assert(img1->x_chroma_shift == img2->x_chroma_shift &&
         img1->y_chroma_shift == img2->y_chroma_shift);
  loc[0] = loc[1] = loc[2] = loc[3] = -1;
  if (img1->monochrome && img2->monochrome && plane) return;
  int match = 1;
  uint32_t i, j;
  for (i = 0; match && i < c_h; i += bsizey) {
    for (j = 0; match && j < c_w; j += bsizex) {
      const int si =
          is_y_plane ? mmin(i + bsizey, c_h) - i : mmin(i + bsizey, c_h - i);
      const int sj =
          is_y_plane ? mmin(j + bsizex, c_w) - j : mmin(j + bsizex, c_w - j);
      int k, l;
      for (k = 0; match && k < si; ++k) {
        for (l = 0; match && l < sj; ++l) {
          const int row = i + k;
          const int col = j + l;
          const int offset1 = row * p1_stride + col;
          const int offset2 = row * p2_stride + col;
          const int val1 = use_highbitdepth
                               ? p1[2 * offset1] | (p1[2 * offset1 + 1] << 8)
                               : p1[offset1];
          const int val2 = use_highbitdepth
                               ? p2[2 * offset2] | (p2[2 * offset2 + 1] << 8)
                               : p2[offset2];
          if (val1 != val2) {
            loc[0] = row;
            loc[1] = col;
            loc[2] = val1;
            loc[3] = val2;
            match = 0;
            break;
          }
        }
      }
    }
  }
}

static void find_mismatch_helper(const aom_image_t *const img1,
                                 const aom_image_t *const img2,
                                 int use_highbitdepth, int yloc[4], int uloc[4],
                                 int vloc[4]) {
  find_mismatch_plane(img1, img2, AOM_PLANE_Y, use_highbitdepth, yloc);
  find_mismatch_plane(img1, img2, AOM_PLANE_U, use_highbitdepth, uloc);
  find_mismatch_plane(img1, img2, AOM_PLANE_V, use_highbitdepth, vloc);
}

void aom_find_mismatch_high(const aom_image_t *const img1,
                            const aom_image_t *const img2, int yloc[4],
                            int uloc[4], int vloc[4]) {
  find_mismatch_helper(img1, img2, 1, yloc, uloc, vloc);
}

void aom_find_mismatch(const aom_image_t *const img1,
                       const aom_image_t *const img2, int yloc[4], int uloc[4],
                       int vloc[4]) {
  find_mismatch_helper(img1, img2, 0, yloc, uloc, vloc);
}

int aom_compare_img(const aom_image_t *const img1,
                    const aom_image_t *const img2) {
  assert(img1->cp == img2->cp);
  assert(img1->tc == img2->tc);
  assert(img1->mc == img2->mc);
  assert(img1->monochrome == img2->monochrome);

  int num_planes = img1->monochrome ? 1 : 3;

  uint32_t l_w = img1->d_w;
  uint32_t c_w = (img1->d_w + img1->x_chroma_shift) >> img1->x_chroma_shift;
  const uint32_t c_h =
      (img1->d_h + img1->y_chroma_shift) >> img1->y_chroma_shift;
  int match = 1;

  match &= (img1->fmt == img2->fmt);
  match &= (img1->d_w == img2->d_w);
  match &= (img1->d_h == img2->d_h);
  if (img1->fmt & AOM_IMG_FMT_HIGHBITDEPTH) {
    l_w *= 2;
    c_w *= 2;
  }

  for (int plane = 0; plane < num_planes; ++plane) {
    uint32_t height = plane ? c_h : img1->d_h;
    uint32_t width = plane ? c_w : l_w;

    for (uint32_t i = 0; i < height; ++i) {
      match &=
          (memcmp(img1->planes[plane] + i * img1->stride[plane],
                  img2->planes[plane] + i * img2->stride[plane], width) == 0);
    }
  }

  return match;
}
