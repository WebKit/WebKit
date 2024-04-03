/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef INCLUDE_LIBYUV_SCALE_H_
#define INCLUDE_LIBYUV_SCALE_H_

#include "libyuv/basic_types.h"

// Supported filtering.
typedef enum FilterMode {
  kFilterNone = 0,      // Point sample; Fastest.
  kFilterLinear = 1,    // Filter horizontally only.
  kFilterBilinear = 2,  // Faster than box, but lower quality scaling down.
  kFilterBox = 3        // Highest quality.
} FilterModeEnum;

int ScalePlane(const uint8_t* src,
               int src_stride,
               int src_width,
               int src_height,
               uint8_t* dst,
               int dst_stride,
               int dst_width,
               int dst_height,
               enum FilterMode filtering);

int ScalePlane_12(const uint16_t* src,
                  int src_stride,
                  int src_width,
                  int src_height,
                  uint16_t* dst,
                  int dst_stride,
                  int dst_width,
                  int dst_height,
                  enum FilterMode filtering);

int ScalePlane_16(const uint16_t* src,
                  int src_stride,
                  int src_width,
                  int src_height,
                  uint16_t* dst,
                  int dst_stride,
                  int dst_width,
                  int dst_height,
                  enum FilterMode filtering);

#endif  // INCLUDE_LIBYUV_SCALE_H_
