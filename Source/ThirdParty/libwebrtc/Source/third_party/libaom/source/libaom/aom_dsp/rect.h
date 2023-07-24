/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AOM_DSP_RECT_H_
#define AOM_AOM_DSP_RECT_H_

#include "config/aom_config.h"

#include <stdbool.h>

// Struct representing a rectangle of pixels.
// The axes are inclusive-exclusive, ie. the point (top, left) is included
// in the rectangle but (bottom, right) is not.
typedef struct {
  int left, right, top, bottom;
} PixelRect;

static INLINE int rect_width(const PixelRect *r) { return r->right - r->left; }

static INLINE int rect_height(const PixelRect *r) { return r->bottom - r->top; }

static INLINE bool is_inside_rect(const int x, const int y,
                                  const PixelRect *r) {
  return (r->left <= x && x < r->right) && (r->top <= y && y < r->bottom);
}

#endif  // AOM_AOM_DSP_RECT_H_
