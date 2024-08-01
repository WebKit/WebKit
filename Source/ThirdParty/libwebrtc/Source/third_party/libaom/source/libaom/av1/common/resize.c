/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/flow_estimation/corner_detect.h"
#include "aom_ports/mem.h"
#include "aom_scale/aom_scale.h"
#include "av1/common/common.h"
#include "av1/common/resize.h"

#include "config/aom_dsp_rtcd.h"
#include "config/aom_scale_rtcd.h"

// Filters for interpolation (0.5-band) - note this also filters integer pels.
static const InterpKernel filteredinterp_filters500[(1 << RS_SUBPEL_BITS)] = {
  { -3, 0, 35, 64, 35, 0, -3, 0 },    { -3, 0, 34, 64, 36, 0, -3, 0 },
  { -3, -1, 34, 64, 36, 1, -3, 0 },   { -3, -1, 33, 64, 37, 1, -3, 0 },
  { -3, -1, 32, 64, 38, 1, -3, 0 },   { -3, -1, 31, 64, 39, 1, -3, 0 },
  { -3, -1, 31, 63, 39, 2, -3, 0 },   { -2, -2, 30, 63, 40, 2, -3, 0 },
  { -2, -2, 29, 63, 41, 2, -3, 0 },   { -2, -2, 29, 63, 41, 3, -4, 0 },
  { -2, -2, 28, 63, 42, 3, -4, 0 },   { -2, -2, 27, 63, 43, 3, -4, 0 },
  { -2, -3, 27, 63, 43, 4, -4, 0 },   { -2, -3, 26, 62, 44, 5, -4, 0 },
  { -2, -3, 25, 62, 45, 5, -4, 0 },   { -2, -3, 25, 62, 45, 5, -4, 0 },
  { -2, -3, 24, 62, 46, 5, -4, 0 },   { -2, -3, 23, 61, 47, 6, -4, 0 },
  { -2, -3, 23, 61, 47, 6, -4, 0 },   { -2, -3, 22, 61, 48, 7, -4, -1 },
  { -2, -3, 21, 60, 49, 7, -4, 0 },   { -1, -4, 20, 60, 49, 8, -4, 0 },
  { -1, -4, 20, 60, 50, 8, -4, -1 },  { -1, -4, 19, 59, 51, 9, -4, -1 },
  { -1, -4, 19, 59, 51, 9, -4, -1 },  { -1, -4, 18, 58, 52, 10, -4, -1 },
  { -1, -4, 17, 58, 52, 11, -4, -1 }, { -1, -4, 16, 58, 53, 11, -4, -1 },
  { -1, -4, 16, 57, 53, 12, -4, -1 }, { -1, -4, 15, 57, 54, 12, -4, -1 },
  { -1, -4, 15, 56, 54, 13, -4, -1 }, { -1, -4, 14, 56, 55, 13, -4, -1 },
  { -1, -4, 14, 55, 55, 14, -4, -1 }, { -1, -4, 13, 55, 56, 14, -4, -1 },
  { -1, -4, 13, 54, 56, 15, -4, -1 }, { -1, -4, 12, 54, 57, 15, -4, -1 },
  { -1, -4, 12, 53, 57, 16, -4, -1 }, { -1, -4, 11, 53, 58, 16, -4, -1 },
  { -1, -4, 11, 52, 58, 17, -4, -1 }, { -1, -4, 10, 52, 58, 18, -4, -1 },
  { -1, -4, 9, 51, 59, 19, -4, -1 },  { -1, -4, 9, 51, 59, 19, -4, -1 },
  { -1, -4, 8, 50, 60, 20, -4, -1 },  { 0, -4, 8, 49, 60, 20, -4, -1 },
  { 0, -4, 7, 49, 60, 21, -3, -2 },   { -1, -4, 7, 48, 61, 22, -3, -2 },
  { 0, -4, 6, 47, 61, 23, -3, -2 },   { 0, -4, 6, 47, 61, 23, -3, -2 },
  { 0, -4, 5, 46, 62, 24, -3, -2 },   { 0, -4, 5, 45, 62, 25, -3, -2 },
  { 0, -4, 5, 45, 62, 25, -3, -2 },   { 0, -4, 5, 44, 62, 26, -3, -2 },
  { 0, -4, 4, 43, 63, 27, -3, -2 },   { 0, -4, 3, 43, 63, 27, -2, -2 },
  { 0, -4, 3, 42, 63, 28, -2, -2 },   { 0, -4, 3, 41, 63, 29, -2, -2 },
  { 0, -3, 2, 41, 63, 29, -2, -2 },   { 0, -3, 2, 40, 63, 30, -2, -2 },
  { 0, -3, 2, 39, 63, 31, -1, -3 },   { 0, -3, 1, 39, 64, 31, -1, -3 },
  { 0, -3, 1, 38, 64, 32, -1, -3 },   { 0, -3, 1, 37, 64, 33, -1, -3 },
  { 0, -3, 1, 36, 64, 34, -1, -3 },   { 0, -3, 0, 36, 64, 34, 0, -3 },
};

// Filters for interpolation (0.625-band) - note this also filters integer pels.
static const InterpKernel filteredinterp_filters625[(1 << RS_SUBPEL_BITS)] = {
  { -1, -8, 33, 80, 33, -8, -1, 0 }, { -1, -8, 31, 80, 34, -8, -1, 1 },
  { -1, -8, 30, 80, 35, -8, -1, 1 }, { -1, -8, 29, 80, 36, -7, -2, 1 },
  { -1, -8, 28, 80, 37, -7, -2, 1 }, { -1, -8, 27, 80, 38, -7, -2, 1 },
  { 0, -8, 26, 79, 39, -7, -2, 1 },  { 0, -8, 25, 79, 40, -7, -2, 1 },
  { 0, -8, 24, 79, 41, -7, -2, 1 },  { 0, -8, 23, 78, 42, -6, -2, 1 },
  { 0, -8, 22, 78, 43, -6, -2, 1 },  { 0, -8, 21, 78, 44, -6, -2, 1 },
  { 0, -8, 20, 78, 45, -5, -3, 1 },  { 0, -8, 19, 77, 47, -5, -3, 1 },
  { 0, -8, 18, 77, 48, -5, -3, 1 },  { 0, -8, 17, 77, 49, -5, -3, 1 },
  { 0, -8, 16, 76, 50, -4, -3, 1 },  { 0, -8, 15, 76, 51, -4, -3, 1 },
  { 0, -8, 15, 75, 52, -3, -4, 1 },  { 0, -7, 14, 74, 53, -3, -4, 1 },
  { 0, -7, 13, 74, 54, -3, -4, 1 },  { 0, -7, 12, 73, 55, -2, -4, 1 },
  { 0, -7, 11, 73, 56, -2, -4, 1 },  { 0, -7, 10, 72, 57, -1, -4, 1 },
  { 1, -7, 10, 71, 58, -1, -5, 1 },  { 0, -7, 9, 71, 59, 0, -5, 1 },
  { 1, -7, 8, 70, 60, 0, -5, 1 },    { 1, -7, 7, 69, 61, 1, -5, 1 },
  { 1, -6, 6, 68, 62, 1, -5, 1 },    { 0, -6, 6, 68, 62, 2, -5, 1 },
  { 1, -6, 5, 67, 63, 2, -5, 1 },    { 1, -6, 5, 66, 64, 3, -6, 1 },
  { 1, -6, 4, 65, 65, 4, -6, 1 },    { 1, -6, 3, 64, 66, 5, -6, 1 },
  { 1, -5, 2, 63, 67, 5, -6, 1 },    { 1, -5, 2, 62, 68, 6, -6, 0 },
  { 1, -5, 1, 62, 68, 6, -6, 1 },    { 1, -5, 1, 61, 69, 7, -7, 1 },
  { 1, -5, 0, 60, 70, 8, -7, 1 },    { 1, -5, 0, 59, 71, 9, -7, 0 },
  { 1, -5, -1, 58, 71, 10, -7, 1 },  { 1, -4, -1, 57, 72, 10, -7, 0 },
  { 1, -4, -2, 56, 73, 11, -7, 0 },  { 1, -4, -2, 55, 73, 12, -7, 0 },
  { 1, -4, -3, 54, 74, 13, -7, 0 },  { 1, -4, -3, 53, 74, 14, -7, 0 },
  { 1, -4, -3, 52, 75, 15, -8, 0 },  { 1, -3, -4, 51, 76, 15, -8, 0 },
  { 1, -3, -4, 50, 76, 16, -8, 0 },  { 1, -3, -5, 49, 77, 17, -8, 0 },
  { 1, -3, -5, 48, 77, 18, -8, 0 },  { 1, -3, -5, 47, 77, 19, -8, 0 },
  { 1, -3, -5, 45, 78, 20, -8, 0 },  { 1, -2, -6, 44, 78, 21, -8, 0 },
  { 1, -2, -6, 43, 78, 22, -8, 0 },  { 1, -2, -6, 42, 78, 23, -8, 0 },
  { 1, -2, -7, 41, 79, 24, -8, 0 },  { 1, -2, -7, 40, 79, 25, -8, 0 },
  { 1, -2, -7, 39, 79, 26, -8, 0 },  { 1, -2, -7, 38, 80, 27, -8, -1 },
  { 1, -2, -7, 37, 80, 28, -8, -1 }, { 1, -2, -7, 36, 80, 29, -8, -1 },
  { 1, -1, -8, 35, 80, 30, -8, -1 }, { 1, -1, -8, 34, 80, 31, -8, -1 },
};

// Filters for interpolation (0.75-band) - note this also filters integer pels.
static const InterpKernel filteredinterp_filters750[(1 << RS_SUBPEL_BITS)] = {
  { 2, -11, 25, 96, 25, -11, 2, 0 }, { 2, -11, 24, 96, 26, -11, 2, 0 },
  { 2, -11, 22, 96, 28, -11, 2, 0 }, { 2, -10, 21, 96, 29, -12, 2, 0 },
  { 2, -10, 19, 96, 31, -12, 2, 0 }, { 2, -10, 18, 95, 32, -11, 2, 0 },
  { 2, -10, 17, 95, 34, -12, 2, 0 }, { 2, -9, 15, 95, 35, -12, 2, 0 },
  { 2, -9, 14, 94, 37, -12, 2, 0 },  { 2, -9, 13, 94, 38, -12, 2, 0 },
  { 2, -8, 12, 93, 40, -12, 1, 0 },  { 2, -8, 11, 93, 41, -12, 1, 0 },
  { 2, -8, 9, 92, 43, -12, 1, 1 },   { 2, -8, 8, 92, 44, -12, 1, 1 },
  { 2, -7, 7, 91, 46, -12, 1, 0 },   { 2, -7, 6, 90, 47, -12, 1, 1 },
  { 2, -7, 5, 90, 49, -12, 1, 0 },   { 2, -6, 4, 89, 50, -12, 1, 0 },
  { 2, -6, 3, 88, 52, -12, 0, 1 },   { 2, -6, 2, 87, 54, -12, 0, 1 },
  { 2, -5, 1, 86, 55, -12, 0, 1 },   { 2, -5, 0, 85, 57, -12, 0, 1 },
  { 2, -5, -1, 84, 58, -11, 0, 1 },  { 2, -5, -2, 83, 60, -11, 0, 1 },
  { 2, -4, -2, 82, 61, -11, -1, 1 }, { 1, -4, -3, 81, 63, -10, -1, 1 },
  { 2, -4, -4, 80, 64, -10, -1, 1 }, { 1, -4, -4, 79, 66, -10, -1, 1 },
  { 1, -3, -5, 77, 67, -9, -1, 1 },  { 1, -3, -6, 76, 69, -9, -1, 1 },
  { 1, -3, -6, 75, 70, -8, -2, 1 },  { 1, -2, -7, 74, 71, -8, -2, 1 },
  { 1, -2, -7, 72, 72, -7, -2, 1 },  { 1, -2, -8, 71, 74, -7, -2, 1 },
  { 1, -2, -8, 70, 75, -6, -3, 1 },  { 1, -1, -9, 69, 76, -6, -3, 1 },
  { 1, -1, -9, 67, 77, -5, -3, 1 },  { 1, -1, -10, 66, 79, -4, -4, 1 },
  { 1, -1, -10, 64, 80, -4, -4, 2 }, { 1, -1, -10, 63, 81, -3, -4, 1 },
  { 1, -1, -11, 61, 82, -2, -4, 2 }, { 1, 0, -11, 60, 83, -2, -5, 2 },
  { 1, 0, -11, 58, 84, -1, -5, 2 },  { 1, 0, -12, 57, 85, 0, -5, 2 },
  { 1, 0, -12, 55, 86, 1, -5, 2 },   { 1, 0, -12, 54, 87, 2, -6, 2 },
  { 1, 0, -12, 52, 88, 3, -6, 2 },   { 0, 1, -12, 50, 89, 4, -6, 2 },
  { 0, 1, -12, 49, 90, 5, -7, 2 },   { 1, 1, -12, 47, 90, 6, -7, 2 },
  { 0, 1, -12, 46, 91, 7, -7, 2 },   { 1, 1, -12, 44, 92, 8, -8, 2 },
  { 1, 1, -12, 43, 92, 9, -8, 2 },   { 0, 1, -12, 41, 93, 11, -8, 2 },
  { 0, 1, -12, 40, 93, 12, -8, 2 },  { 0, 2, -12, 38, 94, 13, -9, 2 },
  { 0, 2, -12, 37, 94, 14, -9, 2 },  { 0, 2, -12, 35, 95, 15, -9, 2 },
  { 0, 2, -12, 34, 95, 17, -10, 2 }, { 0, 2, -11, 32, 95, 18, -10, 2 },
  { 0, 2, -12, 31, 96, 19, -10, 2 }, { 0, 2, -12, 29, 96, 21, -10, 2 },
  { 0, 2, -11, 28, 96, 22, -11, 2 }, { 0, 2, -11, 26, 96, 24, -11, 2 },
};

// Filters for interpolation (0.875-band) - note this also filters integer pels.
static const InterpKernel filteredinterp_filters875[(1 << RS_SUBPEL_BITS)] = {
  { 3, -8, 13, 112, 13, -8, 3, 0 },   { 2, -7, 12, 112, 15, -8, 3, -1 },
  { 3, -7, 10, 112, 17, -9, 3, -1 },  { 2, -6, 8, 112, 19, -9, 3, -1 },
  { 2, -6, 7, 112, 21, -10, 3, -1 },  { 2, -5, 6, 111, 22, -10, 3, -1 },
  { 2, -5, 4, 111, 24, -10, 3, -1 },  { 2, -4, 3, 110, 26, -11, 3, -1 },
  { 2, -4, 1, 110, 28, -11, 3, -1 },  { 2, -4, 0, 109, 30, -12, 4, -1 },
  { 1, -3, -1, 108, 32, -12, 4, -1 }, { 1, -3, -2, 108, 34, -13, 4, -1 },
  { 1, -2, -4, 107, 36, -13, 4, -1 }, { 1, -2, -5, 106, 38, -13, 4, -1 },
  { 1, -1, -6, 105, 40, -14, 4, -1 }, { 1, -1, -7, 104, 42, -14, 4, -1 },
  { 1, -1, -7, 103, 44, -15, 4, -1 }, { 1, 0, -8, 101, 46, -15, 4, -1 },
  { 1, 0, -9, 100, 48, -15, 4, -1 },  { 1, 0, -10, 99, 50, -15, 4, -1 },
  { 1, 1, -11, 97, 53, -16, 4, -1 },  { 0, 1, -11, 96, 55, -16, 4, -1 },
  { 0, 1, -12, 95, 57, -16, 4, -1 },  { 0, 2, -13, 93, 59, -16, 4, -1 },
  { 0, 2, -13, 91, 61, -16, 4, -1 },  { 0, 2, -14, 90, 63, -16, 4, -1 },
  { 0, 2, -14, 88, 65, -16, 4, -1 },  { 0, 2, -15, 86, 67, -16, 4, 0 },
  { 0, 3, -15, 84, 69, -17, 4, 0 },   { 0, 3, -16, 83, 71, -17, 4, 0 },
  { 0, 3, -16, 81, 73, -16, 3, 0 },   { 0, 3, -16, 79, 75, -16, 3, 0 },
  { 0, 3, -16, 77, 77, -16, 3, 0 },   { 0, 3, -16, 75, 79, -16, 3, 0 },
  { 0, 3, -16, 73, 81, -16, 3, 0 },   { 0, 4, -17, 71, 83, -16, 3, 0 },
  { 0, 4, -17, 69, 84, -15, 3, 0 },   { 0, 4, -16, 67, 86, -15, 2, 0 },
  { -1, 4, -16, 65, 88, -14, 2, 0 },  { -1, 4, -16, 63, 90, -14, 2, 0 },
  { -1, 4, -16, 61, 91, -13, 2, 0 },  { -1, 4, -16, 59, 93, -13, 2, 0 },
  { -1, 4, -16, 57, 95, -12, 1, 0 },  { -1, 4, -16, 55, 96, -11, 1, 0 },
  { -1, 4, -16, 53, 97, -11, 1, 1 },  { -1, 4, -15, 50, 99, -10, 0, 1 },
  { -1, 4, -15, 48, 100, -9, 0, 1 },  { -1, 4, -15, 46, 101, -8, 0, 1 },
  { -1, 4, -15, 44, 103, -7, -1, 1 }, { -1, 4, -14, 42, 104, -7, -1, 1 },
  { -1, 4, -14, 40, 105, -6, -1, 1 }, { -1, 4, -13, 38, 106, -5, -2, 1 },
  { -1, 4, -13, 36, 107, -4, -2, 1 }, { -1, 4, -13, 34, 108, -2, -3, 1 },
  { -1, 4, -12, 32, 108, -1, -3, 1 }, { -1, 4, -12, 30, 109, 0, -4, 2 },
  { -1, 3, -11, 28, 110, 1, -4, 2 },  { -1, 3, -11, 26, 110, 3, -4, 2 },
  { -1, 3, -10, 24, 111, 4, -5, 2 },  { -1, 3, -10, 22, 111, 6, -5, 2 },
  { -1, 3, -10, 21, 112, 7, -6, 2 },  { -1, 3, -9, 19, 112, 8, -6, 2 },
  { -1, 3, -9, 17, 112, 10, -7, 3 },  { -1, 3, -8, 15, 112, 12, -7, 2 },
};

const int16_t av1_resize_filter_normative[(
    1 << RS_SUBPEL_BITS)][UPSCALE_NORMATIVE_TAPS] = {
#if UPSCALE_NORMATIVE_TAPS == 8
  { 0, 0, 0, 128, 0, 0, 0, 0 },        { 0, 0, -1, 128, 2, -1, 0, 0 },
  { 0, 1, -3, 127, 4, -2, 1, 0 },      { 0, 1, -4, 127, 6, -3, 1, 0 },
  { 0, 2, -6, 126, 8, -3, 1, 0 },      { 0, 2, -7, 125, 11, -4, 1, 0 },
  { -1, 2, -8, 125, 13, -5, 2, 0 },    { -1, 3, -9, 124, 15, -6, 2, 0 },
  { -1, 3, -10, 123, 18, -6, 2, -1 },  { -1, 3, -11, 122, 20, -7, 3, -1 },
  { -1, 4, -12, 121, 22, -8, 3, -1 },  { -1, 4, -13, 120, 25, -9, 3, -1 },
  { -1, 4, -14, 118, 28, -9, 3, -1 },  { -1, 4, -15, 117, 30, -10, 4, -1 },
  { -1, 5, -16, 116, 32, -11, 4, -1 }, { -1, 5, -16, 114, 35, -12, 4, -1 },
  { -1, 5, -17, 112, 38, -12, 4, -1 }, { -1, 5, -18, 111, 40, -13, 5, -1 },
  { -1, 5, -18, 109, 43, -14, 5, -1 }, { -1, 6, -19, 107, 45, -14, 5, -1 },
  { -1, 6, -19, 105, 48, -15, 5, -1 }, { -1, 6, -19, 103, 51, -16, 5, -1 },
  { -1, 6, -20, 101, 53, -16, 6, -1 }, { -1, 6, -20, 99, 56, -17, 6, -1 },
  { -1, 6, -20, 97, 58, -17, 6, -1 },  { -1, 6, -20, 95, 61, -18, 6, -1 },
  { -2, 7, -20, 93, 64, -18, 6, -2 },  { -2, 7, -20, 91, 66, -19, 6, -1 },
  { -2, 7, -20, 88, 69, -19, 6, -1 },  { -2, 7, -20, 86, 71, -19, 6, -1 },
  { -2, 7, -20, 84, 74, -20, 7, -2 },  { -2, 7, -20, 81, 76, -20, 7, -1 },
  { -2, 7, -20, 79, 79, -20, 7, -2 },  { -1, 7, -20, 76, 81, -20, 7, -2 },
  { -2, 7, -20, 74, 84, -20, 7, -2 },  { -1, 6, -19, 71, 86, -20, 7, -2 },
  { -1, 6, -19, 69, 88, -20, 7, -2 },  { -1, 6, -19, 66, 91, -20, 7, -2 },
  { -2, 6, -18, 64, 93, -20, 7, -2 },  { -1, 6, -18, 61, 95, -20, 6, -1 },
  { -1, 6, -17, 58, 97, -20, 6, -1 },  { -1, 6, -17, 56, 99, -20, 6, -1 },
  { -1, 6, -16, 53, 101, -20, 6, -1 }, { -1, 5, -16, 51, 103, -19, 6, -1 },
  { -1, 5, -15, 48, 105, -19, 6, -1 }, { -1, 5, -14, 45, 107, -19, 6, -1 },
  { -1, 5, -14, 43, 109, -18, 5, -1 }, { -1, 5, -13, 40, 111, -18, 5, -1 },
  { -1, 4, -12, 38, 112, -17, 5, -1 }, { -1, 4, -12, 35, 114, -16, 5, -1 },
  { -1, 4, -11, 32, 116, -16, 5, -1 }, { -1, 4, -10, 30, 117, -15, 4, -1 },
  { -1, 3, -9, 28, 118, -14, 4, -1 },  { -1, 3, -9, 25, 120, -13, 4, -1 },
  { -1, 3, -8, 22, 121, -12, 4, -1 },  { -1, 3, -7, 20, 122, -11, 3, -1 },
  { -1, 2, -6, 18, 123, -10, 3, -1 },  { 0, 2, -6, 15, 124, -9, 3, -1 },
  { 0, 2, -5, 13, 125, -8, 2, -1 },    { 0, 1, -4, 11, 125, -7, 2, 0 },
  { 0, 1, -3, 8, 126, -6, 2, 0 },      { 0, 1, -3, 6, 127, -4, 1, 0 },
  { 0, 1, -2, 4, 127, -3, 1, 0 },      { 0, 0, -1, 2, 128, -1, 0, 0 },
#else
#error "Invalid value of UPSCALE_NORMATIVE_TAPS"
#endif  // UPSCALE_NORMATIVE_TAPS == 8
};

// Filters for interpolation (full-band) - no filtering for integer pixels
#define filteredinterp_filters1000 av1_resize_filter_normative

static const InterpKernel *choose_interp_filter(int in_length, int out_length) {
  int out_length16 = out_length * 16;
  if (out_length16 >= in_length * 16)
    return filteredinterp_filters1000;
  else if (out_length16 >= in_length * 13)
    return filteredinterp_filters875;
  else if (out_length16 >= in_length * 11)
    return filteredinterp_filters750;
  else if (out_length16 >= in_length * 9)
    return filteredinterp_filters625;
  else
    return filteredinterp_filters500;
}

static void interpolate_core(const uint8_t *const input, int in_length,
                             uint8_t *output, int out_length,
                             const int16_t *interp_filters, int interp_taps) {
  const int32_t delta =
      (((uint32_t)in_length << RS_SCALE_SUBPEL_BITS) + out_length / 2) /
      out_length;
  const int32_t offset =
      in_length > out_length
          ? (((int32_t)(in_length - out_length) << (RS_SCALE_SUBPEL_BITS - 1)) +
             out_length / 2) /
                out_length
          : -(((int32_t)(out_length - in_length)
               << (RS_SCALE_SUBPEL_BITS - 1)) +
              out_length / 2) /
                out_length;
  uint8_t *optr = output;
  int x, x1, x2, sum, k, int_pel, sub_pel;
  int32_t y;

  x = 0;
  y = offset + RS_SCALE_EXTRA_OFF;
  while ((y >> RS_SCALE_SUBPEL_BITS) < (interp_taps / 2 - 1)) {
    x++;
    y += delta;
  }
  x1 = x;
  x = out_length - 1;
  y = delta * x + offset + RS_SCALE_EXTRA_OFF;
  while ((y >> RS_SCALE_SUBPEL_BITS) + (int32_t)(interp_taps / 2) >=
         in_length) {
    x--;
    y -= delta;
  }
  x2 = x;
  if (x1 > x2) {
    for (x = 0, y = offset + RS_SCALE_EXTRA_OFF; x < out_length;
         ++x, y += delta) {
      int_pel = y >> RS_SCALE_SUBPEL_BITS;
      sub_pel = (y >> RS_SCALE_EXTRA_BITS) & RS_SUBPEL_MASK;
      const int16_t *filter = &interp_filters[sub_pel * interp_taps];
      sum = 0;
      for (k = 0; k < interp_taps; ++k) {
        const int pk = int_pel - interp_taps / 2 + 1 + k;
        sum += filter[k] * input[AOMMAX(AOMMIN(pk, in_length - 1), 0)];
      }
      *optr++ = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
    }
  } else {
    // Initial part.
    for (x = 0, y = offset + RS_SCALE_EXTRA_OFF; x < x1; ++x, y += delta) {
      int_pel = y >> RS_SCALE_SUBPEL_BITS;
      sub_pel = (y >> RS_SCALE_EXTRA_BITS) & RS_SUBPEL_MASK;
      const int16_t *filter = &interp_filters[sub_pel * interp_taps];
      sum = 0;
      for (k = 0; k < interp_taps; ++k)
        sum += filter[k] * input[AOMMAX(int_pel - interp_taps / 2 + 1 + k, 0)];
      *optr++ = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
    }
    // Middle part.
    for (; x <= x2; ++x, y += delta) {
      int_pel = y >> RS_SCALE_SUBPEL_BITS;
      sub_pel = (y >> RS_SCALE_EXTRA_BITS) & RS_SUBPEL_MASK;
      const int16_t *filter = &interp_filters[sub_pel * interp_taps];
      sum = 0;
      for (k = 0; k < interp_taps; ++k)
        sum += filter[k] * input[int_pel - interp_taps / 2 + 1 + k];
      *optr++ = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
    }
    // End part.
    for (; x < out_length; ++x, y += delta) {
      int_pel = y >> RS_SCALE_SUBPEL_BITS;
      sub_pel = (y >> RS_SCALE_EXTRA_BITS) & RS_SUBPEL_MASK;
      const int16_t *filter = &interp_filters[sub_pel * interp_taps];
      sum = 0;
      for (k = 0; k < interp_taps; ++k)
        sum += filter[k] *
               input[AOMMIN(int_pel - interp_taps / 2 + 1 + k, in_length - 1)];
      *optr++ = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
    }
  }
}

static void interpolate(const uint8_t *const input, int in_length,
                        uint8_t *output, int out_length) {
  const InterpKernel *interp_filters =
      choose_interp_filter(in_length, out_length);

  interpolate_core(input, in_length, output, out_length, &interp_filters[0][0],
                   SUBPEL_TAPS);
}

int32_t av1_get_upscale_convolve_step(int in_length, int out_length) {
  return ((in_length << RS_SCALE_SUBPEL_BITS) + out_length / 2) / out_length;
}

static int32_t get_upscale_convolve_x0(int in_length, int out_length,
                                       int32_t x_step_qn) {
  const int err = out_length * x_step_qn - (in_length << RS_SCALE_SUBPEL_BITS);
  const int32_t x0 =
      (-((out_length - in_length) << (RS_SCALE_SUBPEL_BITS - 1)) +
       out_length / 2) /
          out_length +
      RS_SCALE_EXTRA_OFF - err / 2;
  return (int32_t)((uint32_t)x0 & RS_SCALE_SUBPEL_MASK);
}

void down2_symeven(const uint8_t *const input, int length, uint8_t *output,
                   int start_offset) {
  // Actual filter len = 2 * filter_len_half.
  const int16_t *filter = av1_down2_symeven_half_filter;
  const int filter_len_half = sizeof(av1_down2_symeven_half_filter) / 2;
  int i, j;
  uint8_t *optr = output;
  int l1 = filter_len_half;
  int l2 = (length - filter_len_half);
  l1 += (l1 & 1);
  l2 += (l2 & 1);
  if (l1 > l2) {
    // Short input length.
    for (i = start_offset; i < length; i += 2) {
      int sum = (1 << (FILTER_BITS - 1));
      for (j = 0; j < filter_len_half; ++j) {
        sum +=
            (input[AOMMAX(i - j, 0)] + input[AOMMIN(i + 1 + j, length - 1)]) *
            filter[j];
      }
      sum >>= FILTER_BITS;
      *optr++ = clip_pixel(sum);
    }
  } else {
    // Initial part.
    for (i = start_offset; i < l1; i += 2) {
      int sum = (1 << (FILTER_BITS - 1));
      for (j = 0; j < filter_len_half; ++j) {
        sum += (input[AOMMAX(i - j, 0)] + input[i + 1 + j]) * filter[j];
      }
      sum >>= FILTER_BITS;
      *optr++ = clip_pixel(sum);
    }
    // Middle part.
    for (; i < l2; i += 2) {
      int sum = (1 << (FILTER_BITS - 1));
      for (j = 0; j < filter_len_half; ++j) {
        sum += (input[i - j] + input[i + 1 + j]) * filter[j];
      }
      sum >>= FILTER_BITS;
      *optr++ = clip_pixel(sum);
    }
    // End part.
    for (; i < length; i += 2) {
      int sum = (1 << (FILTER_BITS - 1));
      for (j = 0; j < filter_len_half; ++j) {
        sum +=
            (input[i - j] + input[AOMMIN(i + 1 + j, length - 1)]) * filter[j];
      }
      sum >>= FILTER_BITS;
      *optr++ = clip_pixel(sum);
    }
  }
}

static void down2_symodd(const uint8_t *const input, int length,
                         uint8_t *output) {
  // Actual filter len = 2 * filter_len_half - 1.
  const int16_t *filter = av1_down2_symodd_half_filter;
  const int filter_len_half = sizeof(av1_down2_symodd_half_filter) / 2;
  int i, j;
  uint8_t *optr = output;
  int l1 = filter_len_half - 1;
  int l2 = (length - filter_len_half + 1);
  l1 += (l1 & 1);
  l2 += (l2 & 1);
  if (l1 > l2) {
    // Short input length.
    for (i = 0; i < length; i += 2) {
      int sum = (1 << (FILTER_BITS - 1)) + input[i] * filter[0];
      for (j = 1; j < filter_len_half; ++j) {
        sum += (input[(i - j < 0 ? 0 : i - j)] +
                input[(i + j >= length ? length - 1 : i + j)]) *
               filter[j];
      }
      sum >>= FILTER_BITS;
      *optr++ = clip_pixel(sum);
    }
  } else {
    // Initial part.
    for (i = 0; i < l1; i += 2) {
      int sum = (1 << (FILTER_BITS - 1)) + input[i] * filter[0];
      for (j = 1; j < filter_len_half; ++j) {
        sum += (input[(i - j < 0 ? 0 : i - j)] + input[i + j]) * filter[j];
      }
      sum >>= FILTER_BITS;
      *optr++ = clip_pixel(sum);
    }
    // Middle part.
    for (; i < l2; i += 2) {
      int sum = (1 << (FILTER_BITS - 1)) + input[i] * filter[0];
      for (j = 1; j < filter_len_half; ++j) {
        sum += (input[i - j] + input[i + j]) * filter[j];
      }
      sum >>= FILTER_BITS;
      *optr++ = clip_pixel(sum);
    }
    // End part.
    for (; i < length; i += 2) {
      int sum = (1 << (FILTER_BITS - 1)) + input[i] * filter[0];
      for (j = 1; j < filter_len_half; ++j) {
        sum += (input[i - j] + input[(i + j >= length ? length - 1 : i + j)]) *
               filter[j];
      }
      sum >>= FILTER_BITS;
      *optr++ = clip_pixel(sum);
    }
  }
}

static int get_down2_length(int length, int steps) {
  for (int s = 0; s < steps; ++s) length = (length + 1) >> 1;
  return length;
}

static int get_down2_steps(int in_length, int out_length) {
  int steps = 0;
  int proj_in_length;
  while ((proj_in_length = get_down2_length(in_length, 1)) >= out_length) {
    ++steps;
    in_length = proj_in_length;
    if (in_length == 1) {
      // Special case: we break because any further calls to get_down2_length()
      // with be with length == 1, which return 1, resulting in an infinite
      // loop.
      break;
    }
  }
  return steps;
}

static void resize_multistep(const uint8_t *const input, int length,
                             uint8_t *output, int olength, uint8_t *otmp) {
  if (length == olength) {
    memcpy(output, input, sizeof(output[0]) * length);
    return;
  }
  const int steps = get_down2_steps(length, olength);

  if (steps > 0) {
    uint8_t *out = NULL;
    int filteredlength = length;

    assert(otmp != NULL);
    uint8_t *otmp2 = otmp + get_down2_length(length, 1);
    for (int s = 0; s < steps; ++s) {
      const int proj_filteredlength = get_down2_length(filteredlength, 1);
      const uint8_t *const in = (s == 0 ? input : out);
      if (s == steps - 1 && proj_filteredlength == olength)
        out = output;
      else
        out = (s & 1 ? otmp2 : otmp);
      if (filteredlength & 1)
        down2_symodd(in, filteredlength, out);
      else
        down2_symeven(in, filteredlength, out, 0);
      filteredlength = proj_filteredlength;
    }
    if (filteredlength != olength) {
      interpolate(out, filteredlength, output, olength);
    }
  } else {
    interpolate(input, length, output, olength);
  }
}

static void fill_col_to_arr(uint8_t *img, int stride, int len, uint8_t *arr) {
  int i;
  uint8_t *iptr = img;
  uint8_t *aptr = arr;
  for (i = 0; i < len; ++i, iptr += stride) {
    *aptr++ = *iptr;
  }
}

static void fill_arr_to_col(uint8_t *img, int stride, int len, uint8_t *arr) {
  int i;
  uint8_t *iptr = img;
  uint8_t *aptr = arr;
  for (i = 0; i < len; ++i, iptr += stride) {
    *iptr = *aptr++;
  }
}

bool av1_resize_vert_dir_c(uint8_t *intbuf, uint8_t *output, int out_stride,
                           int height, int height2, int width2, int start_col) {
  bool mem_status = true;
  uint8_t *arrbuf = (uint8_t *)aom_malloc(sizeof(*arrbuf) * height);
  uint8_t *arrbuf2 = (uint8_t *)aom_malloc(sizeof(*arrbuf2) * height2);
  if (arrbuf == NULL || arrbuf2 == NULL) {
    mem_status = false;
    goto Error;
  }

  for (int i = start_col; i < width2; ++i) {
    fill_col_to_arr(intbuf + i, width2, height, arrbuf);
    down2_symeven(arrbuf, height, arrbuf2, 0);
    fill_arr_to_col(output + i, out_stride, height2, arrbuf2);
  }

Error:
  aom_free(arrbuf);
  aom_free(arrbuf2);
  return mem_status;
}

void av1_resize_horz_dir_c(const uint8_t *const input, int in_stride,
                           uint8_t *intbuf, int height, int filtered_length,
                           int width2) {
  for (int i = 0; i < height; ++i)
    down2_symeven(input + in_stride * i, filtered_length, intbuf + width2 * i,
                  0);
}

bool av1_resize_plane_to_half(const uint8_t *const input, int height, int width,
                              int in_stride, uint8_t *output, int height2,
                              int width2, int out_stride) {
  uint8_t *intbuf = (uint8_t *)aom_malloc(sizeof(*intbuf) * width2 * height);
  if (intbuf == NULL) {
    return false;
  }

  // Resize in the horizontal direction
  av1_resize_horz_dir(input, in_stride, intbuf, height, width, width2);
  // Resize in the vertical direction
  bool mem_status = av1_resize_vert_dir(intbuf, output, out_stride, height,
                                        height2, width2, 0 /*start_col*/);
  aom_free(intbuf);
  return mem_status;
}

// Check if both the output width and height are half of input width and
// height respectively.
bool should_resize_by_half(int height, int width, int height2, int width2) {
  const bool is_width_by_2 = get_down2_length(width, 1) == width2;
  const bool is_height_by_2 = get_down2_length(height, 1) == height2;
  return (is_width_by_2 && is_height_by_2);
}

bool av1_resize_plane(const uint8_t *input, int height, int width,
                      int in_stride, uint8_t *output, int height2, int width2,
                      int out_stride) {
  int i;
  bool mem_status = true;
  uint8_t *intbuf = (uint8_t *)aom_malloc(sizeof(uint8_t) * width2 * height);
  uint8_t *tmpbuf =
      (uint8_t *)aom_malloc(sizeof(uint8_t) * AOMMAX(width, height));
  uint8_t *arrbuf = (uint8_t *)aom_malloc(sizeof(uint8_t) * height);
  uint8_t *arrbuf2 = (uint8_t *)aom_malloc(sizeof(uint8_t) * height2);
  if (intbuf == NULL || tmpbuf == NULL || arrbuf == NULL || arrbuf2 == NULL) {
    mem_status = false;
    goto Error;
  }
  assert(width > 0);
  assert(height > 0);
  assert(width2 > 0);
  assert(height2 > 0);
  for (i = 0; i < height; ++i)
    resize_multistep(input + in_stride * i, width, intbuf + width2 * i, width2,
                     tmpbuf);
  for (i = 0; i < width2; ++i) {
    fill_col_to_arr(intbuf + i, width2, height, arrbuf);
    resize_multistep(arrbuf, height, arrbuf2, height2, tmpbuf);
    fill_arr_to_col(output + i, out_stride, height2, arrbuf2);
  }

Error:
  aom_free(intbuf);
  aom_free(tmpbuf);
  aom_free(arrbuf);
  aom_free(arrbuf2);
  return mem_status;
}

static bool upscale_normative_rect(const uint8_t *const input, int height,
                                   int width, int in_stride, uint8_t *output,
                                   int height2, int width2, int out_stride,
                                   int x_step_qn, int x0_qn, int pad_left,
                                   int pad_right) {
  assert(width > 0);
  assert(height > 0);
  assert(width2 > 0);
  assert(height2 > 0);
  assert(height2 == height);

  // Extend the left/right pixels of the tile column if needed
  // (either because we can't sample from other tiles, or because we're at
  // a frame edge).
  // Save the overwritten pixels into tmp_left and tmp_right.
  // Note: Because we pass input-1 to av1_convolve_horiz_rs, we need one extra
  // column of border pixels compared to what we'd naively think.
  const int border_cols = UPSCALE_NORMATIVE_TAPS / 2 + 1;
  uint8_t *tmp_left =
      NULL;  // Silence spurious "may be used uninitialized" warnings
  uint8_t *tmp_right = NULL;
  uint8_t *const in_tl = (uint8_t *)(input - border_cols);  // Cast off 'const'
  uint8_t *const in_tr = (uint8_t *)(input + width);
  if (pad_left) {
    tmp_left = (uint8_t *)aom_malloc(sizeof(*tmp_left) * border_cols * height);
    if (!tmp_left) return false;
    for (int i = 0; i < height; i++) {
      memcpy(tmp_left + i * border_cols, in_tl + i * in_stride, border_cols);
      memset(in_tl + i * in_stride, input[i * in_stride], border_cols);
    }
  }
  if (pad_right) {
    tmp_right =
        (uint8_t *)aom_malloc(sizeof(*tmp_right) * border_cols * height);
    if (!tmp_right) {
      aom_free(tmp_left);
      return false;
    }
    for (int i = 0; i < height; i++) {
      memcpy(tmp_right + i * border_cols, in_tr + i * in_stride, border_cols);
      memset(in_tr + i * in_stride, input[i * in_stride + width - 1],
             border_cols);
    }
  }

  av1_convolve_horiz_rs(input - 1, in_stride, output, out_stride, width2,
                        height2, &av1_resize_filter_normative[0][0], x0_qn,
                        x_step_qn);

  // Restore the left/right border pixels
  if (pad_left) {
    for (int i = 0; i < height; i++) {
      memcpy(in_tl + i * in_stride, tmp_left + i * border_cols, border_cols);
    }
    aom_free(tmp_left);
  }
  if (pad_right) {
    for (int i = 0; i < height; i++) {
      memcpy(in_tr + i * in_stride, tmp_right + i * border_cols, border_cols);
    }
    aom_free(tmp_right);
  }
  return true;
}

#if CONFIG_AV1_HIGHBITDEPTH
static void highbd_interpolate_core(const uint16_t *const input, int in_length,
                                    uint16_t *output, int out_length, int bd,
                                    const int16_t *interp_filters,
                                    int interp_taps) {
  const int32_t delta =
      (((uint32_t)in_length << RS_SCALE_SUBPEL_BITS) + out_length / 2) /
      out_length;
  const int32_t offset =
      in_length > out_length
          ? (((int32_t)(in_length - out_length) << (RS_SCALE_SUBPEL_BITS - 1)) +
             out_length / 2) /
                out_length
          : -(((int32_t)(out_length - in_length)
               << (RS_SCALE_SUBPEL_BITS - 1)) +
              out_length / 2) /
                out_length;
  uint16_t *optr = output;
  int x, x1, x2, sum, k, int_pel, sub_pel;
  int32_t y;

  x = 0;
  y = offset + RS_SCALE_EXTRA_OFF;
  while ((y >> RS_SCALE_SUBPEL_BITS) < (interp_taps / 2 - 1)) {
    x++;
    y += delta;
  }
  x1 = x;
  x = out_length - 1;
  y = delta * x + offset + RS_SCALE_EXTRA_OFF;
  while ((y >> RS_SCALE_SUBPEL_BITS) + (int32_t)(interp_taps / 2) >=
         in_length) {
    x--;
    y -= delta;
  }
  x2 = x;
  if (x1 > x2) {
    for (x = 0, y = offset + RS_SCALE_EXTRA_OFF; x < out_length;
         ++x, y += delta) {
      int_pel = y >> RS_SCALE_SUBPEL_BITS;
      sub_pel = (y >> RS_SCALE_EXTRA_BITS) & RS_SUBPEL_MASK;
      const int16_t *filter = &interp_filters[sub_pel * interp_taps];
      sum = 0;
      for (k = 0; k < interp_taps; ++k) {
        const int pk = int_pel - interp_taps / 2 + 1 + k;
        sum += filter[k] * input[AOMMAX(AOMMIN(pk, in_length - 1), 0)];
      }
      *optr++ = clip_pixel_highbd(ROUND_POWER_OF_TWO(sum, FILTER_BITS), bd);
    }
  } else {
    // Initial part.
    for (x = 0, y = offset + RS_SCALE_EXTRA_OFF; x < x1; ++x, y += delta) {
      int_pel = y >> RS_SCALE_SUBPEL_BITS;
      sub_pel = (y >> RS_SCALE_EXTRA_BITS) & RS_SUBPEL_MASK;
      const int16_t *filter = &interp_filters[sub_pel * interp_taps];
      sum = 0;
      for (k = 0; k < interp_taps; ++k)
        sum += filter[k] * input[AOMMAX(int_pel - interp_taps / 2 + 1 + k, 0)];
      *optr++ = clip_pixel_highbd(ROUND_POWER_OF_TWO(sum, FILTER_BITS), bd);
    }
    // Middle part.
    for (; x <= x2; ++x, y += delta) {
      int_pel = y >> RS_SCALE_SUBPEL_BITS;
      sub_pel = (y >> RS_SCALE_EXTRA_BITS) & RS_SUBPEL_MASK;
      const int16_t *filter = &interp_filters[sub_pel * interp_taps];
      sum = 0;
      for (k = 0; k < interp_taps; ++k)
        sum += filter[k] * input[int_pel - interp_taps / 2 + 1 + k];
      *optr++ = clip_pixel_highbd(ROUND_POWER_OF_TWO(sum, FILTER_BITS), bd);
    }
    // End part.
    for (; x < out_length; ++x, y += delta) {
      int_pel = y >> RS_SCALE_SUBPEL_BITS;
      sub_pel = (y >> RS_SCALE_EXTRA_BITS) & RS_SUBPEL_MASK;
      const int16_t *filter = &interp_filters[sub_pel * interp_taps];
      sum = 0;
      for (k = 0; k < interp_taps; ++k)
        sum += filter[k] *
               input[AOMMIN(int_pel - interp_taps / 2 + 1 + k, in_length - 1)];
      *optr++ = clip_pixel_highbd(ROUND_POWER_OF_TWO(sum, FILTER_BITS), bd);
    }
  }
}

static void highbd_interpolate(const uint16_t *const input, int in_length,
                               uint16_t *output, int out_length, int bd) {
  const InterpKernel *interp_filters =
      choose_interp_filter(in_length, out_length);

  highbd_interpolate_core(input, in_length, output, out_length, bd,
                          &interp_filters[0][0], SUBPEL_TAPS);
}

static void highbd_down2_symeven(const uint16_t *const input, int length,
                                 uint16_t *output, int bd) {
  // Actual filter len = 2 * filter_len_half.
  static const int16_t *filter = av1_down2_symeven_half_filter;
  const int filter_len_half = sizeof(av1_down2_symeven_half_filter) / 2;
  int i, j;
  uint16_t *optr = output;
  int l1 = filter_len_half;
  int l2 = (length - filter_len_half);
  l1 += (l1 & 1);
  l2 += (l2 & 1);
  if (l1 > l2) {
    // Short input length.
    for (i = 0; i < length; i += 2) {
      int sum = (1 << (FILTER_BITS - 1));
      for (j = 0; j < filter_len_half; ++j) {
        sum +=
            (input[AOMMAX(0, i - j)] + input[AOMMIN(i + 1 + j, length - 1)]) *
            filter[j];
      }
      sum >>= FILTER_BITS;
      *optr++ = clip_pixel_highbd(sum, bd);
    }
  } else {
    // Initial part.
    for (i = 0; i < l1; i += 2) {
      int sum = (1 << (FILTER_BITS - 1));
      for (j = 0; j < filter_len_half; ++j) {
        sum += (input[AOMMAX(0, i - j)] + input[i + 1 + j]) * filter[j];
      }
      sum >>= FILTER_BITS;
      *optr++ = clip_pixel_highbd(sum, bd);
    }
    // Middle part.
    for (; i < l2; i += 2) {
      int sum = (1 << (FILTER_BITS - 1));
      for (j = 0; j < filter_len_half; ++j) {
        sum += (input[i - j] + input[i + 1 + j]) * filter[j];
      }
      sum >>= FILTER_BITS;
      *optr++ = clip_pixel_highbd(sum, bd);
    }
    // End part.
    for (; i < length; i += 2) {
      int sum = (1 << (FILTER_BITS - 1));
      for (j = 0; j < filter_len_half; ++j) {
        sum +=
            (input[i - j] + input[AOMMIN(i + 1 + j, length - 1)]) * filter[j];
      }
      sum >>= FILTER_BITS;
      *optr++ = clip_pixel_highbd(sum, bd);
    }
  }
}

static void highbd_down2_symodd(const uint16_t *const input, int length,
                                uint16_t *output, int bd) {
  // Actual filter len = 2 * filter_len_half - 1.
  static const int16_t *filter = av1_down2_symodd_half_filter;
  const int filter_len_half = sizeof(av1_down2_symodd_half_filter) / 2;
  int i, j;
  uint16_t *optr = output;
  int l1 = filter_len_half - 1;
  int l2 = (length - filter_len_half + 1);
  l1 += (l1 & 1);
  l2 += (l2 & 1);
  if (l1 > l2) {
    // Short input length.
    for (i = 0; i < length; i += 2) {
      int sum = (1 << (FILTER_BITS - 1)) + input[i] * filter[0];
      for (j = 1; j < filter_len_half; ++j) {
        sum += (input[AOMMAX(i - j, 0)] + input[AOMMIN(i + j, length - 1)]) *
               filter[j];
      }
      sum >>= FILTER_BITS;
      *optr++ = clip_pixel_highbd(sum, bd);
    }
  } else {
    // Initial part.
    for (i = 0; i < l1; i += 2) {
      int sum = (1 << (FILTER_BITS - 1)) + input[i] * filter[0];
      for (j = 1; j < filter_len_half; ++j) {
        sum += (input[AOMMAX(i - j, 0)] + input[i + j]) * filter[j];
      }
      sum >>= FILTER_BITS;
      *optr++ = clip_pixel_highbd(sum, bd);
    }
    // Middle part.
    for (; i < l2; i += 2) {
      int sum = (1 << (FILTER_BITS - 1)) + input[i] * filter[0];
      for (j = 1; j < filter_len_half; ++j) {
        sum += (input[i - j] + input[i + j]) * filter[j];
      }
      sum >>= FILTER_BITS;
      *optr++ = clip_pixel_highbd(sum, bd);
    }
    // End part.
    for (; i < length; i += 2) {
      int sum = (1 << (FILTER_BITS - 1)) + input[i] * filter[0];
      for (j = 1; j < filter_len_half; ++j) {
        sum += (input[i - j] + input[AOMMIN(i + j, length - 1)]) * filter[j];
      }
      sum >>= FILTER_BITS;
      *optr++ = clip_pixel_highbd(sum, bd);
    }
  }
}

static void highbd_resize_multistep(const uint16_t *const input, int length,
                                    uint16_t *output, int olength,
                                    uint16_t *otmp, int bd) {
  if (length == olength) {
    memcpy(output, input, sizeof(output[0]) * length);
    return;
  }
  const int steps = get_down2_steps(length, olength);

  if (steps > 0) {
    uint16_t *out = NULL;
    int filteredlength = length;

    assert(otmp != NULL);
    uint16_t *otmp2 = otmp + get_down2_length(length, 1);
    for (int s = 0; s < steps; ++s) {
      const int proj_filteredlength = get_down2_length(filteredlength, 1);
      const uint16_t *const in = (s == 0 ? input : out);
      if (s == steps - 1 && proj_filteredlength == olength)
        out = output;
      else
        out = (s & 1 ? otmp2 : otmp);
      if (filteredlength & 1)
        highbd_down2_symodd(in, filteredlength, out, bd);
      else
        highbd_down2_symeven(in, filteredlength, out, bd);
      filteredlength = proj_filteredlength;
    }
    if (filteredlength != olength) {
      highbd_interpolate(out, filteredlength, output, olength, bd);
    }
  } else {
    highbd_interpolate(input, length, output, olength, bd);
  }
}

static void highbd_fill_col_to_arr(uint16_t *img, int stride, int len,
                                   uint16_t *arr) {
  int i;
  uint16_t *iptr = img;
  uint16_t *aptr = arr;
  for (i = 0; i < len; ++i, iptr += stride) {
    *aptr++ = *iptr;
  }
}

static void highbd_fill_arr_to_col(uint16_t *img, int stride, int len,
                                   uint16_t *arr) {
  int i;
  uint16_t *iptr = img;
  uint16_t *aptr = arr;
  for (i = 0; i < len; ++i, iptr += stride) {
    *iptr = *aptr++;
  }
}

void av1_highbd_resize_plane(const uint8_t *input, int height, int width,
                             int in_stride, uint8_t *output, int height2,
                             int width2, int out_stride, int bd) {
  int i;
  uint16_t *intbuf = (uint16_t *)aom_malloc(sizeof(uint16_t) * width2 * height);
  uint16_t *tmpbuf =
      (uint16_t *)aom_malloc(sizeof(uint16_t) * AOMMAX(width, height));
  uint16_t *arrbuf = (uint16_t *)aom_malloc(sizeof(uint16_t) * height);
  uint16_t *arrbuf2 = (uint16_t *)aom_malloc(sizeof(uint16_t) * height2);
  if (intbuf == NULL || tmpbuf == NULL || arrbuf == NULL || arrbuf2 == NULL)
    goto Error;
  for (i = 0; i < height; ++i) {
    highbd_resize_multistep(CONVERT_TO_SHORTPTR(input + in_stride * i), width,
                            intbuf + width2 * i, width2, tmpbuf, bd);
  }
  for (i = 0; i < width2; ++i) {
    highbd_fill_col_to_arr(intbuf + i, width2, height, arrbuf);
    highbd_resize_multistep(arrbuf, height, arrbuf2, height2, tmpbuf, bd);
    highbd_fill_arr_to_col(CONVERT_TO_SHORTPTR(output + i), out_stride, height2,
                           arrbuf2);
  }

Error:
  aom_free(intbuf);
  aom_free(tmpbuf);
  aom_free(arrbuf);
  aom_free(arrbuf2);
}

static bool highbd_upscale_normative_rect(const uint8_t *const input,
                                          int height, int width, int in_stride,
                                          uint8_t *output, int height2,
                                          int width2, int out_stride,
                                          int x_step_qn, int x0_qn,
                                          int pad_left, int pad_right, int bd) {
  assert(width > 0);
  assert(height > 0);
  assert(width2 > 0);
  assert(height2 > 0);
  assert(height2 == height);

  // Extend the left/right pixels of the tile column if needed
  // (either because we can't sample from other tiles, or because we're at
  // a frame edge).
  // Save the overwritten pixels into tmp_left and tmp_right.
  // Note: Because we pass input-1 to av1_convolve_horiz_rs, we need one extra
  // column of border pixels compared to what we'd naively think.
  const int border_cols = UPSCALE_NORMATIVE_TAPS / 2 + 1;
  const int border_size = border_cols * sizeof(uint16_t);
  uint16_t *tmp_left =
      NULL;  // Silence spurious "may be used uninitialized" warnings
  uint16_t *tmp_right = NULL;
  uint16_t *const input16 = CONVERT_TO_SHORTPTR(input);
  uint16_t *const in_tl = input16 - border_cols;
  uint16_t *const in_tr = input16 + width;
  if (pad_left) {
    tmp_left = (uint16_t *)aom_malloc(sizeof(*tmp_left) * border_cols * height);
    if (!tmp_left) return false;
    for (int i = 0; i < height; i++) {
      memcpy(tmp_left + i * border_cols, in_tl + i * in_stride, border_size);
      aom_memset16(in_tl + i * in_stride, input16[i * in_stride], border_cols);
    }
  }
  if (pad_right) {
    tmp_right =
        (uint16_t *)aom_malloc(sizeof(*tmp_right) * border_cols * height);
    if (!tmp_right) {
      aom_free(tmp_left);
      return false;
    }
    for (int i = 0; i < height; i++) {
      memcpy(tmp_right + i * border_cols, in_tr + i * in_stride, border_size);
      aom_memset16(in_tr + i * in_stride, input16[i * in_stride + width - 1],
                   border_cols);
    }
  }

  av1_highbd_convolve_horiz_rs(CONVERT_TO_SHORTPTR(input - 1), in_stride,
                               CONVERT_TO_SHORTPTR(output), out_stride, width2,
                               height2, &av1_resize_filter_normative[0][0],
                               x0_qn, x_step_qn, bd);

  // Restore the left/right border pixels
  if (pad_left) {
    for (int i = 0; i < height; i++) {
      memcpy(in_tl + i * in_stride, tmp_left + i * border_cols, border_size);
    }
    aom_free(tmp_left);
  }
  if (pad_right) {
    for (int i = 0; i < height; i++) {
      memcpy(in_tr + i * in_stride, tmp_right + i * border_cols, border_size);
    }
    aom_free(tmp_right);
  }
  return true;
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

void av1_resize_frame420(const uint8_t *y, int y_stride, const uint8_t *u,
                         const uint8_t *v, int uv_stride, int height, int width,
                         uint8_t *oy, int oy_stride, uint8_t *ou, uint8_t *ov,
                         int ouv_stride, int oheight, int owidth) {
  if (!av1_resize_plane(y, height, width, y_stride, oy, oheight, owidth,
                        oy_stride))
    abort();
  if (!av1_resize_plane(u, height / 2, width / 2, uv_stride, ou, oheight / 2,
                        owidth / 2, ouv_stride))
    abort();
  if (!av1_resize_plane(v, height / 2, width / 2, uv_stride, ov, oheight / 2,
                        owidth / 2, ouv_stride))
    abort();
}

bool av1_resize_frame422(const uint8_t *y, int y_stride, const uint8_t *u,
                         const uint8_t *v, int uv_stride, int height, int width,
                         uint8_t *oy, int oy_stride, uint8_t *ou, uint8_t *ov,
                         int ouv_stride, int oheight, int owidth) {
  if (!av1_resize_plane(y, height, width, y_stride, oy, oheight, owidth,
                        oy_stride))
    return false;
  if (!av1_resize_plane(u, height, width / 2, uv_stride, ou, oheight,
                        owidth / 2, ouv_stride))
    return false;
  if (!av1_resize_plane(v, height, width / 2, uv_stride, ov, oheight,
                        owidth / 2, ouv_stride))
    return false;
  return true;
}

bool av1_resize_frame444(const uint8_t *y, int y_stride, const uint8_t *u,
                         const uint8_t *v, int uv_stride, int height, int width,
                         uint8_t *oy, int oy_stride, uint8_t *ou, uint8_t *ov,
                         int ouv_stride, int oheight, int owidth) {
  if (!av1_resize_plane(y, height, width, y_stride, oy, oheight, owidth,
                        oy_stride))
    return false;
  if (!av1_resize_plane(u, height, width, uv_stride, ou, oheight, owidth,
                        ouv_stride))
    return false;
  if (!av1_resize_plane(v, height, width, uv_stride, ov, oheight, owidth,
                        ouv_stride))
    return false;
  return true;
}

#if CONFIG_AV1_HIGHBITDEPTH
void av1_highbd_resize_frame420(const uint8_t *y, int y_stride,
                                const uint8_t *u, const uint8_t *v,
                                int uv_stride, int height, int width,
                                uint8_t *oy, int oy_stride, uint8_t *ou,
                                uint8_t *ov, int ouv_stride, int oheight,
                                int owidth, int bd) {
  av1_highbd_resize_plane(y, height, width, y_stride, oy, oheight, owidth,
                          oy_stride, bd);
  av1_highbd_resize_plane(u, height / 2, width / 2, uv_stride, ou, oheight / 2,
                          owidth / 2, ouv_stride, bd);
  av1_highbd_resize_plane(v, height / 2, width / 2, uv_stride, ov, oheight / 2,
                          owidth / 2, ouv_stride, bd);
}

void av1_highbd_resize_frame422(const uint8_t *y, int y_stride,
                                const uint8_t *u, const uint8_t *v,
                                int uv_stride, int height, int width,
                                uint8_t *oy, int oy_stride, uint8_t *ou,
                                uint8_t *ov, int ouv_stride, int oheight,
                                int owidth, int bd) {
  av1_highbd_resize_plane(y, height, width, y_stride, oy, oheight, owidth,
                          oy_stride, bd);
  av1_highbd_resize_plane(u, height, width / 2, uv_stride, ou, oheight,
                          owidth / 2, ouv_stride, bd);
  av1_highbd_resize_plane(v, height, width / 2, uv_stride, ov, oheight,
                          owidth / 2, ouv_stride, bd);
}

void av1_highbd_resize_frame444(const uint8_t *y, int y_stride,
                                const uint8_t *u, const uint8_t *v,
                                int uv_stride, int height, int width,
                                uint8_t *oy, int oy_stride, uint8_t *ou,
                                uint8_t *ov, int ouv_stride, int oheight,
                                int owidth, int bd) {
  av1_highbd_resize_plane(y, height, width, y_stride, oy, oheight, owidth,
                          oy_stride, bd);
  av1_highbd_resize_plane(u, height, width, uv_stride, ou, oheight, owidth,
                          ouv_stride, bd);
  av1_highbd_resize_plane(v, height, width, uv_stride, ov, oheight, owidth,
                          ouv_stride, bd);
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

void av1_resize_and_extend_frame_c(const YV12_BUFFER_CONFIG *src,
                                   YV12_BUFFER_CONFIG *dst,
                                   const InterpFilter filter,
                                   const int phase_scaler,
                                   const int num_planes) {
  assert(filter == BILINEAR || filter == EIGHTTAP_SMOOTH ||
         filter == EIGHTTAP_REGULAR);
  const InterpKernel *const kernel =
      (const InterpKernel *)av1_interp_filter_params_list[filter].filter_ptr;

  for (int i = 0; i < AOMMIN(num_planes, MAX_MB_PLANE); ++i) {
    const int is_uv = i > 0;
    const int src_w = src->crop_widths[is_uv];
    const int src_h = src->crop_heights[is_uv];
    const uint8_t *src_buffer = src->buffers[i];
    const int src_stride = src->strides[is_uv];
    const int dst_w = dst->crop_widths[is_uv];
    const int dst_h = dst->crop_heights[is_uv];
    uint8_t *dst_buffer = dst->buffers[i];
    const int dst_stride = dst->strides[is_uv];
    for (int y = 0; y < dst_h; y += 16) {
      const int y_q4 =
          src_h == dst_h ? 0 : y * 16 * src_h / dst_h + phase_scaler;
      for (int x = 0; x < dst_w; x += 16) {
        const int x_q4 =
            src_w == dst_w ? 0 : x * 16 * src_w / dst_w + phase_scaler;
        const uint8_t *src_ptr =
            src_buffer + y * src_h / dst_h * src_stride + x * src_w / dst_w;
        uint8_t *dst_ptr = dst_buffer + y * dst_stride + x;

        // Width and height of the actual working area.
        const int work_w = AOMMIN(16, dst_w - x);
        const int work_h = AOMMIN(16, dst_h - y);
        // SIMD versions of aom_scaled_2d() have some trouble handling
        // nonstandard sizes, so fall back on the C version to handle borders.
        if (work_w != 16 || work_h != 16) {
          aom_scaled_2d_c(src_ptr, src_stride, dst_ptr, dst_stride, kernel,
                          x_q4 & 0xf, 16 * src_w / dst_w, y_q4 & 0xf,
                          16 * src_h / dst_h, work_w, work_h);
        } else {
          aom_scaled_2d(src_ptr, src_stride, dst_ptr, dst_stride, kernel,
                        x_q4 & 0xf, 16 * src_w / dst_w, y_q4 & 0xf,
                        16 * src_h / dst_h, 16, 16);
        }
      }
    }
  }
  aom_extend_frame_borders(dst, num_planes);
}

bool av1_resize_and_extend_frame_nonnormative(const YV12_BUFFER_CONFIG *src,
                                              YV12_BUFFER_CONFIG *dst, int bd,
                                              int num_planes) {
  // TODO(dkovalev): replace YV12_BUFFER_CONFIG with aom_image_t

  // We use AOMMIN(num_planes, MAX_MB_PLANE) instead of num_planes to quiet
  // the static analysis warnings.
  for (int i = 0; i < AOMMIN(num_planes, MAX_MB_PLANE); ++i) {
    const int is_uv = i > 0;
#if CONFIG_AV1_HIGHBITDEPTH
    if (src->flags & YV12_FLAG_HIGHBITDEPTH) {
      av1_highbd_resize_plane(src->buffers[i], src->crop_heights[is_uv],
                              src->crop_widths[is_uv], src->strides[is_uv],
                              dst->buffers[i], dst->crop_heights[is_uv],
                              dst->crop_widths[is_uv], dst->strides[is_uv], bd);
    } else if (!av1_resize_plane(src->buffers[i], src->crop_heights[is_uv],
                                 src->crop_widths[is_uv], src->strides[is_uv],
                                 dst->buffers[i], dst->crop_heights[is_uv],
                                 dst->crop_widths[is_uv],
                                 dst->strides[is_uv])) {
      return false;
    }
#else
    (void)bd;
    if (!av1_resize_plane(src->buffers[i], src->crop_heights[is_uv],
                          src->crop_widths[is_uv], src->strides[is_uv],
                          dst->buffers[i], dst->crop_heights[is_uv],
                          dst->crop_widths[is_uv], dst->strides[is_uv]))
      return false;
#endif
  }
  aom_extend_frame_borders(dst, num_planes);
  return true;
}

void av1_upscale_normative_rows(const AV1_COMMON *cm, const uint8_t *src,
                                int src_stride, uint8_t *dst, int dst_stride,
                                int plane, int rows) {
  const int is_uv = (plane > 0);
  const int ss_x = is_uv && cm->seq_params->subsampling_x;
  const int downscaled_plane_width = ROUND_POWER_OF_TWO(cm->width, ss_x);
  const int upscaled_plane_width =
      ROUND_POWER_OF_TWO(cm->superres_upscaled_width, ss_x);
  const int superres_denom = cm->superres_scale_denominator;

  TileInfo tile_col;
  const int32_t x_step_qn = av1_get_upscale_convolve_step(
      downscaled_plane_width, upscaled_plane_width);
  int32_t x0_qn = get_upscale_convolve_x0(downscaled_plane_width,
                                          upscaled_plane_width, x_step_qn);

  for (int j = 0; j < cm->tiles.cols; j++) {
    av1_tile_set_col(&tile_col, cm, j);
    // Determine the limits of this tile column in both the source
    // and destination images.
    // Note: The actual location which we start sampling from is
    // (downscaled_x0 - 1 + (x0_qn/2^14)), and this quantity increases
    // by exactly dst_width * (x_step_qn/2^14) pixels each iteration.
    const int downscaled_x0 = tile_col.mi_col_start << (MI_SIZE_LOG2 - ss_x);
    const int downscaled_x1 = tile_col.mi_col_end << (MI_SIZE_LOG2 - ss_x);
    const int src_width = downscaled_x1 - downscaled_x0;

    const int upscaled_x0 = (downscaled_x0 * superres_denom) / SCALE_NUMERATOR;
    int upscaled_x1;
    if (j == cm->tiles.cols - 1) {
      // Note that we can't just use AOMMIN here - due to rounding,
      // (downscaled_x1 * superres_denom) / SCALE_NUMERATOR may be less than
      // upscaled_plane_width.
      upscaled_x1 = upscaled_plane_width;
    } else {
      upscaled_x1 = (downscaled_x1 * superres_denom) / SCALE_NUMERATOR;
    }

    const uint8_t *const src_ptr = src + downscaled_x0;
    uint8_t *const dst_ptr = dst + upscaled_x0;
    const int dst_width = upscaled_x1 - upscaled_x0;

    const int pad_left = (j == 0);
    const int pad_right = (j == cm->tiles.cols - 1);

    bool success;
#if CONFIG_AV1_HIGHBITDEPTH
    if (cm->seq_params->use_highbitdepth)
      success = highbd_upscale_normative_rect(
          src_ptr, rows, src_width, src_stride, dst_ptr, rows, dst_width,
          dst_stride, x_step_qn, x0_qn, pad_left, pad_right,
          cm->seq_params->bit_depth);
    else
      success = upscale_normative_rect(src_ptr, rows, src_width, src_stride,
                                       dst_ptr, rows, dst_width, dst_stride,
                                       x_step_qn, x0_qn, pad_left, pad_right);
#else
    success = upscale_normative_rect(src_ptr, rows, src_width, src_stride,
                                     dst_ptr, rows, dst_width, dst_stride,
                                     x_step_qn, x0_qn, pad_left, pad_right);
#endif
    if (!success) {
      aom_internal_error(cm->error, AOM_CODEC_MEM_ERROR,
                         "Error upscaling frame");
    }
    // Update the fractional pixel offset to prepare for the next tile column.
    x0_qn += (dst_width * x_step_qn) - (src_width << RS_SCALE_SUBPEL_BITS);
  }
}

void av1_upscale_normative_and_extend_frame(const AV1_COMMON *cm,
                                            const YV12_BUFFER_CONFIG *src,
                                            YV12_BUFFER_CONFIG *dst) {
  const int num_planes = av1_num_planes(cm);
  for (int i = 0; i < num_planes; ++i) {
    const int is_uv = (i > 0);
    av1_upscale_normative_rows(cm, src->buffers[i], src->strides[is_uv],
                               dst->buffers[i], dst->strides[is_uv], i,
                               src->crop_heights[is_uv]);
  }

  aom_extend_frame_borders(dst, num_planes);
}

YV12_BUFFER_CONFIG *av1_realloc_and_scale_if_required(
    AV1_COMMON *cm, YV12_BUFFER_CONFIG *unscaled, YV12_BUFFER_CONFIG *scaled,
    const InterpFilter filter, const int phase, const bool use_optimized_scaler,
    const bool for_psnr, const int border_in_pixels, const bool alloc_pyramid) {
  // If scaling is performed for the sole purpose of calculating PSNR, then our
  // target dimensions are superres upscaled width/height. Otherwise our target
  // dimensions are coded width/height.
  const int scaled_width = for_psnr ? cm->superres_upscaled_width : cm->width;
  const int scaled_height =
      for_psnr ? cm->superres_upscaled_height : cm->height;
  const bool scaling_required = (scaled_width != unscaled->y_crop_width) ||
                                (scaled_height != unscaled->y_crop_height);

  if (scaling_required) {
    const int num_planes = av1_num_planes(cm);
    const SequenceHeader *seq_params = cm->seq_params;

    // Reallocate the frame buffer based on the target dimensions when scaling
    // is required.
    if (aom_realloc_frame_buffer(
            scaled, scaled_width, scaled_height, seq_params->subsampling_x,
            seq_params->subsampling_y, seq_params->use_highbitdepth,
            border_in_pixels, cm->features.byte_alignment, NULL, NULL, NULL,
            alloc_pyramid, 0))
      aom_internal_error(cm->error, AOM_CODEC_MEM_ERROR,
                         "Failed to allocate scaled buffer");

    bool has_optimized_scaler = av1_has_optimized_scaler(
        unscaled->y_crop_width, unscaled->y_crop_height, scaled_width,
        scaled_height);
    if (num_planes > 1) {
      has_optimized_scaler = has_optimized_scaler &&
                             av1_has_optimized_scaler(unscaled->uv_crop_width,
                                                      unscaled->uv_crop_height,
                                                      scaled->uv_crop_width,
                                                      scaled->uv_crop_height);
    }

#if CONFIG_AV1_HIGHBITDEPTH
    if (use_optimized_scaler && has_optimized_scaler &&
        cm->seq_params->bit_depth == AOM_BITS_8) {
      av1_resize_and_extend_frame(unscaled, scaled, filter, phase, num_planes);
    } else {
      if (!av1_resize_and_extend_frame_nonnormative(
              unscaled, scaled, (int)cm->seq_params->bit_depth, num_planes))
        aom_internal_error(cm->error, AOM_CODEC_MEM_ERROR,
                           "Failed to allocate buffers during resize");
    }
#else
    if (use_optimized_scaler && has_optimized_scaler) {
      av1_resize_and_extend_frame(unscaled, scaled, filter, phase, num_planes);
    } else {
      if (!av1_resize_and_extend_frame_nonnormative(
              unscaled, scaled, (int)cm->seq_params->bit_depth, num_planes))
        aom_internal_error(cm->error, AOM_CODEC_MEM_ERROR,
                           "Failed to allocate buffers during resize");
    }
#endif
    return scaled;
  }
  return unscaled;
}

// Calculates the scaled dimension given the original dimension and the scale
// denominator.
static void calculate_scaled_size_helper(int *dim, int denom) {
  if (denom != SCALE_NUMERATOR) {
    // We need to ensure the constraint in "Appendix A" of the spec:
    // * FrameWidth is greater than or equal to 16
    // * FrameHeight is greater than or equal to 16
    // For this, we clamp the downscaled dimension to at least 16. One
    // exception: if original dimension itself was < 16, then we keep the
    // downscaled dimension to be same as the original, to ensure that resizing
    // is valid.
    const int min_dim = AOMMIN(16, *dim);
    // Use this version if we need *dim to be even
    // *width = (*width * SCALE_NUMERATOR + denom) / (2 * denom);
    // *width <<= 1;
    *dim = (*dim * SCALE_NUMERATOR + denom / 2) / (denom);
    *dim = AOMMAX(*dim, min_dim);
  }
}

void av1_calculate_scaled_size(int *width, int *height, int resize_denom) {
  calculate_scaled_size_helper(width, resize_denom);
  calculate_scaled_size_helper(height, resize_denom);
}

void av1_calculate_scaled_superres_size(int *width, int *height,
                                        int superres_denom) {
  (void)height;
  calculate_scaled_size_helper(width, superres_denom);
}

void av1_calculate_unscaled_superres_size(int *width, int *height, int denom) {
  if (denom != SCALE_NUMERATOR) {
    // Note: av1_calculate_scaled_superres_size() rounds *up* after division
    // when the resulting dimensions are odd. So here, we round *down*.
    *width = *width * denom / SCALE_NUMERATOR;
    (void)height;
  }
}

// Copy only the config data from 'src' to 'dst'.
static void copy_buffer_config(const YV12_BUFFER_CONFIG *const src,
                               YV12_BUFFER_CONFIG *const dst) {
  dst->bit_depth = src->bit_depth;
  dst->color_primaries = src->color_primaries;
  dst->transfer_characteristics = src->transfer_characteristics;
  dst->matrix_coefficients = src->matrix_coefficients;
  dst->monochrome = src->monochrome;
  dst->chroma_sample_position = src->chroma_sample_position;
  dst->color_range = src->color_range;
}

// TODO(afergs): Look for in-place upscaling
// TODO(afergs): aom_ vs av1_ functions? Which can I use?
// Upscale decoded image.
void av1_superres_upscale(AV1_COMMON *cm, BufferPool *const pool,
                          bool alloc_pyramid) {
  const int num_planes = av1_num_planes(cm);
  if (!av1_superres_scaled(cm)) return;
  const SequenceHeader *const seq_params = cm->seq_params;
  const int byte_alignment = cm->features.byte_alignment;

  YV12_BUFFER_CONFIG copy_buffer;
  memset(&copy_buffer, 0, sizeof(copy_buffer));

  YV12_BUFFER_CONFIG *const frame_to_show = &cm->cur_frame->buf;

  const int aligned_width = ALIGN_POWER_OF_TWO(cm->width, 3);
  if (aom_alloc_frame_buffer(
          &copy_buffer, aligned_width, cm->height, seq_params->subsampling_x,
          seq_params->subsampling_y, seq_params->use_highbitdepth,
          AOM_BORDER_IN_PIXELS, byte_alignment, false, 0))
    aom_internal_error(cm->error, AOM_CODEC_MEM_ERROR,
                       "Failed to allocate copy buffer for superres upscaling");

  // Copy function assumes the frames are the same size.
  // Note that it does not copy YV12_BUFFER_CONFIG config data.
  aom_yv12_copy_frame(frame_to_show, &copy_buffer, num_planes);

  assert(copy_buffer.y_crop_width == aligned_width);
  assert(copy_buffer.y_crop_height == cm->height);

  // Realloc the current frame buffer at a higher resolution in place.
  if (pool != NULL) {
    // Use callbacks if on the decoder.
    aom_codec_frame_buffer_t *fb = &cm->cur_frame->raw_frame_buffer;
    aom_release_frame_buffer_cb_fn_t release_fb_cb = pool->release_fb_cb;
    aom_get_frame_buffer_cb_fn_t cb = pool->get_fb_cb;
    void *cb_priv = pool->cb_priv;

    lock_buffer_pool(pool);
    // Realloc with callback does not release the frame buffer - release first.
    if (release_fb_cb(cb_priv, fb)) {
      unlock_buffer_pool(pool);
      aom_internal_error(
          cm->error, AOM_CODEC_MEM_ERROR,
          "Failed to free current frame buffer before superres upscaling");
    }
    // aom_realloc_frame_buffer() leaves config data for frame_to_show intact
    if (aom_realloc_frame_buffer(
            frame_to_show, cm->superres_upscaled_width,
            cm->superres_upscaled_height, seq_params->subsampling_x,
            seq_params->subsampling_y, seq_params->use_highbitdepth,
            AOM_BORDER_IN_PIXELS, byte_alignment, fb, cb, cb_priv,
            alloc_pyramid, 0)) {
      unlock_buffer_pool(pool);
      aom_internal_error(
          cm->error, AOM_CODEC_MEM_ERROR,
          "Failed to allocate current frame buffer for superres upscaling");
    }
    unlock_buffer_pool(pool);
  } else {
    // Make a copy of the config data for frame_to_show in copy_buffer
    copy_buffer_config(frame_to_show, &copy_buffer);

    // Don't use callbacks on the encoder.
    // aom_alloc_frame_buffer() clears the config data for frame_to_show
    if (aom_alloc_frame_buffer(
            frame_to_show, cm->superres_upscaled_width,
            cm->superres_upscaled_height, seq_params->subsampling_x,
            seq_params->subsampling_y, seq_params->use_highbitdepth,
            AOM_BORDER_IN_PIXELS, byte_alignment, alloc_pyramid, 0))
      aom_internal_error(
          cm->error, AOM_CODEC_MEM_ERROR,
          "Failed to reallocate current frame buffer for superres upscaling");

    // Restore config data back to frame_to_show
    copy_buffer_config(&copy_buffer, frame_to_show);
  }
  // TODO(afergs): verify frame_to_show is correct after realloc
  //               encoder:
  //               decoder:

  assert(frame_to_show->y_crop_width == cm->superres_upscaled_width);
  assert(frame_to_show->y_crop_height == cm->superres_upscaled_height);

  // Scale up and back into frame_to_show.
  assert(frame_to_show->y_crop_width != cm->width);
  av1_upscale_normative_and_extend_frame(cm, &copy_buffer, frame_to_show);

  // Free the copy buffer
  aom_free_frame_buffer(&copy_buffer);
}
