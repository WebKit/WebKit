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

#ifndef AOM_AOM_DSP_PROB_H_
#define AOM_AOM_DSP_PROB_H_

#include <assert.h>
#include <stdio.h>

#include "config/aom_config.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/entcode.h"
#include "aom_ports/bitops.h"
#include "aom_ports/mem.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t aom_cdf_prob;

#define CDF_SIZE(x) ((x) + 1)
#define CDF_PROB_BITS 15
#define CDF_PROB_TOP (1 << CDF_PROB_BITS)
#define CDF_INIT_TOP 32768
#define CDF_SHIFT (15 - CDF_PROB_BITS)
/*The value stored in an iCDF is CDF_PROB_TOP minus the actual cumulative
  probability (an "inverse" CDF).
  This function converts from one representation to the other (and is its own
  inverse).*/
#define AOM_ICDF(x) (CDF_PROB_TOP - (x))

#if CDF_SHIFT == 0

#define AOM_CDF2(a0) AOM_ICDF(a0), AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF3(a0, a1) AOM_ICDF(a0), AOM_ICDF(a1), AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF4(a0, a1, a2) \
  AOM_ICDF(a0), AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF5(a0, a1, a2, a3) \
  AOM_ICDF(a0)                   \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF6(a0, a1, a2, a3, a4)                        \
  AOM_ICDF(a0)                                              \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(a4), \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF7(a0, a1, a2, a3, a4, a5)                                  \
  AOM_ICDF(a0)                                                            \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(a4), AOM_ICDF(a5), \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF8(a0, a1, a2, a3, a4, a5, a6)                              \
  AOM_ICDF(a0)                                                            \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(a4), AOM_ICDF(a5), \
      AOM_ICDF(a6), AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF9(a0, a1, a2, a3, a4, a5, a6, a7)                          \
  AOM_ICDF(a0)                                                            \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(a4), AOM_ICDF(a5), \
      AOM_ICDF(a6), AOM_ICDF(a7), AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF10(a0, a1, a2, a3, a4, a5, a6, a7, a8)                     \
  AOM_ICDF(a0)                                                            \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(a4), AOM_ICDF(a5), \
      AOM_ICDF(a6), AOM_ICDF(a7), AOM_ICDF(a8), AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF11(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9)                 \
  AOM_ICDF(a0)                                                            \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(a4), AOM_ICDF(a5), \
      AOM_ICDF(a6), AOM_ICDF(a7), AOM_ICDF(a8), AOM_ICDF(a9),             \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF12(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)               \
  AOM_ICDF(a0)                                                               \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(a4), AOM_ICDF(a5),    \
      AOM_ICDF(a6), AOM_ICDF(a7), AOM_ICDF(a8), AOM_ICDF(a9), AOM_ICDF(a10), \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF13(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11)          \
  AOM_ICDF(a0)                                                               \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(a4), AOM_ICDF(a5),    \
      AOM_ICDF(a6), AOM_ICDF(a7), AOM_ICDF(a8), AOM_ICDF(a9), AOM_ICDF(a10), \
      AOM_ICDF(a11), AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF14(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12)     \
  AOM_ICDF(a0)                                                               \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(a4), AOM_ICDF(a5),    \
      AOM_ICDF(a6), AOM_ICDF(a7), AOM_ICDF(a8), AOM_ICDF(a9), AOM_ICDF(a10), \
      AOM_ICDF(a11), AOM_ICDF(a12), AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF15(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13) \
  AOM_ICDF(a0)                                                                \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(a4), AOM_ICDF(a5),     \
      AOM_ICDF(a6), AOM_ICDF(a7), AOM_ICDF(a8), AOM_ICDF(a9), AOM_ICDF(a10),  \
      AOM_ICDF(a11), AOM_ICDF(a12), AOM_ICDF(a13), AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF16(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, \
                  a14)                                                        \
  AOM_ICDF(a0)                                                                \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(a4), AOM_ICDF(a5),     \
      AOM_ICDF(a6), AOM_ICDF(a7), AOM_ICDF(a8), AOM_ICDF(a9), AOM_ICDF(a10),  \
      AOM_ICDF(a11), AOM_ICDF(a12), AOM_ICDF(a13), AOM_ICDF(a14),             \
      AOM_ICDF(CDF_PROB_TOP), 0

#else
#define AOM_CDF2(a0)                                       \
  AOM_ICDF((((a0)-1) * ((CDF_INIT_TOP >> CDF_SHIFT) - 2) + \
            ((CDF_INIT_TOP - 2) >> 1)) /                   \
               ((CDF_INIT_TOP - 2)) +                      \
           1)                                              \
  , AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF3(a0, a1)                                       \
  AOM_ICDF((((a0)-1) * ((CDF_INIT_TOP >> CDF_SHIFT) - 3) +     \
            ((CDF_INIT_TOP - 3) >> 1)) /                       \
               ((CDF_INIT_TOP - 3)) +                          \
           1)                                                  \
  ,                                                            \
      AOM_ICDF((((a1)-2) * ((CDF_INIT_TOP >> CDF_SHIFT) - 3) + \
                ((CDF_INIT_TOP - 3) >> 1)) /                   \
                   ((CDF_INIT_TOP - 3)) +                      \
               2),                                             \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF4(a0, a1, a2)                                   \
  AOM_ICDF((((a0)-1) * ((CDF_INIT_TOP >> CDF_SHIFT) - 4) +     \
            ((CDF_INIT_TOP - 4) >> 1)) /                       \
               ((CDF_INIT_TOP - 4)) +                          \
           1)                                                  \
  ,                                                            \
      AOM_ICDF((((a1)-2) * ((CDF_INIT_TOP >> CDF_SHIFT) - 4) + \
                ((CDF_INIT_TOP - 4) >> 1)) /                   \
                   ((CDF_INIT_TOP - 4)) +                      \
               2),                                             \
      AOM_ICDF((((a2)-3) * ((CDF_INIT_TOP >> CDF_SHIFT) - 4) + \
                ((CDF_INIT_TOP - 4) >> 1)) /                   \
                   ((CDF_INIT_TOP - 4)) +                      \
               3),                                             \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF5(a0, a1, a2, a3)                               \
  AOM_ICDF((((a0)-1) * ((CDF_INIT_TOP >> CDF_SHIFT) - 5) +     \
            ((CDF_INIT_TOP - 5) >> 1)) /                       \
               ((CDF_INIT_TOP - 5)) +                          \
           1)                                                  \
  ,                                                            \
      AOM_ICDF((((a1)-2) * ((CDF_INIT_TOP >> CDF_SHIFT) - 5) + \
                ((CDF_INIT_TOP - 5) >> 1)) /                   \
                   ((CDF_INIT_TOP - 5)) +                      \
               2),                                             \
      AOM_ICDF((((a2)-3) * ((CDF_INIT_TOP >> CDF_SHIFT) - 5) + \
                ((CDF_INIT_TOP - 5) >> 1)) /                   \
                   ((CDF_INIT_TOP - 5)) +                      \
               3),                                             \
      AOM_ICDF((((a3)-4) * ((CDF_INIT_TOP >> CDF_SHIFT) - 5) + \
                ((CDF_INIT_TOP - 5) >> 1)) /                   \
                   ((CDF_INIT_TOP - 5)) +                      \
               4),                                             \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF6(a0, a1, a2, a3, a4)                           \
  AOM_ICDF((((a0)-1) * ((CDF_INIT_TOP >> CDF_SHIFT) - 6) +     \
            ((CDF_INIT_TOP - 6) >> 1)) /                       \
               ((CDF_INIT_TOP - 6)) +                          \
           1)                                                  \
  ,                                                            \
      AOM_ICDF((((a1)-2) * ((CDF_INIT_TOP >> CDF_SHIFT) - 6) + \
                ((CDF_INIT_TOP - 6) >> 1)) /                   \
                   ((CDF_INIT_TOP - 6)) +                      \
               2),                                             \
      AOM_ICDF((((a2)-3) * ((CDF_INIT_TOP >> CDF_SHIFT) - 6) + \
                ((CDF_INIT_TOP - 6) >> 1)) /                   \
                   ((CDF_INIT_TOP - 6)) +                      \
               3),                                             \
      AOM_ICDF((((a3)-4) * ((CDF_INIT_TOP >> CDF_SHIFT) - 6) + \
                ((CDF_INIT_TOP - 6) >> 1)) /                   \
                   ((CDF_INIT_TOP - 6)) +                      \
               4),                                             \
      AOM_ICDF((((a4)-5) * ((CDF_INIT_TOP >> CDF_SHIFT) - 6) + \
                ((CDF_INIT_TOP - 6) >> 1)) /                   \
                   ((CDF_INIT_TOP - 6)) +                      \
               5),                                             \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF7(a0, a1, a2, a3, a4, a5)                       \
  AOM_ICDF((((a0)-1) * ((CDF_INIT_TOP >> CDF_SHIFT) - 7) +     \
            ((CDF_INIT_TOP - 7) >> 1)) /                       \
               ((CDF_INIT_TOP - 7)) +                          \
           1)                                                  \
  ,                                                            \
      AOM_ICDF((((a1)-2) * ((CDF_INIT_TOP >> CDF_SHIFT) - 7) + \
                ((CDF_INIT_TOP - 7) >> 1)) /                   \
                   ((CDF_INIT_TOP - 7)) +                      \
               2),                                             \
      AOM_ICDF((((a2)-3) * ((CDF_INIT_TOP >> CDF_SHIFT) - 7) + \
                ((CDF_INIT_TOP - 7) >> 1)) /                   \
                   ((CDF_INIT_TOP - 7)) +                      \
               3),                                             \
      AOM_ICDF((((a3)-4) * ((CDF_INIT_TOP >> CDF_SHIFT) - 7) + \
                ((CDF_INIT_TOP - 7) >> 1)) /                   \
                   ((CDF_INIT_TOP - 7)) +                      \
               4),                                             \
      AOM_ICDF((((a4)-5) * ((CDF_INIT_TOP >> CDF_SHIFT) - 7) + \
                ((CDF_INIT_TOP - 7) >> 1)) /                   \
                   ((CDF_INIT_TOP - 7)) +                      \
               5),                                             \
      AOM_ICDF((((a5)-6) * ((CDF_INIT_TOP >> CDF_SHIFT) - 7) + \
                ((CDF_INIT_TOP - 7) >> 1)) /                   \
                   ((CDF_INIT_TOP - 7)) +                      \
               6),                                             \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF8(a0, a1, a2, a3, a4, a5, a6)                   \
  AOM_ICDF((((a0)-1) * ((CDF_INIT_TOP >> CDF_SHIFT) - 8) +     \
            ((CDF_INIT_TOP - 8) >> 1)) /                       \
               ((CDF_INIT_TOP - 8)) +                          \
           1)                                                  \
  ,                                                            \
      AOM_ICDF((((a1)-2) * ((CDF_INIT_TOP >> CDF_SHIFT) - 8) + \
                ((CDF_INIT_TOP - 8) >> 1)) /                   \
                   ((CDF_INIT_TOP - 8)) +                      \
               2),                                             \
      AOM_ICDF((((a2)-3) * ((CDF_INIT_TOP >> CDF_SHIFT) - 8) + \
                ((CDF_INIT_TOP - 8) >> 1)) /                   \
                   ((CDF_INIT_TOP - 8)) +                      \
               3),                                             \
      AOM_ICDF((((a3)-4) * ((CDF_INIT_TOP >> CDF_SHIFT) - 8) + \
                ((CDF_INIT_TOP - 8) >> 1)) /                   \
                   ((CDF_INIT_TOP - 8)) +                      \
               4),                                             \
      AOM_ICDF((((a4)-5) * ((CDF_INIT_TOP >> CDF_SHIFT) - 8) + \
                ((CDF_INIT_TOP - 8) >> 1)) /                   \
                   ((CDF_INIT_TOP - 8)) +                      \
               5),                                             \
      AOM_ICDF((((a5)-6) * ((CDF_INIT_TOP >> CDF_SHIFT) - 8) + \
                ((CDF_INIT_TOP - 8) >> 1)) /                   \
                   ((CDF_INIT_TOP - 8)) +                      \
               6),                                             \
      AOM_ICDF((((a6)-7) * ((CDF_INIT_TOP >> CDF_SHIFT) - 8) + \
                ((CDF_INIT_TOP - 8) >> 1)) /                   \
                   ((CDF_INIT_TOP - 8)) +                      \
               7),                                             \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF9(a0, a1, a2, a3, a4, a5, a6, a7)               \
  AOM_ICDF((((a0)-1) * ((CDF_INIT_TOP >> CDF_SHIFT) - 9) +     \
            ((CDF_INIT_TOP - 9) >> 1)) /                       \
               ((CDF_INIT_TOP - 9)) +                          \
           1)                                                  \
  ,                                                            \
      AOM_ICDF((((a1)-2) * ((CDF_INIT_TOP >> CDF_SHIFT) - 9) + \
                ((CDF_INIT_TOP - 9) >> 1)) /                   \
                   ((CDF_INIT_TOP - 9)) +                      \
               2),                                             \
      AOM_ICDF((((a2)-3) * ((CDF_INIT_TOP >> CDF_SHIFT) - 9) + \
                ((CDF_INIT_TOP - 9) >> 1)) /                   \
                   ((CDF_INIT_TOP - 9)) +                      \
               3),                                             \
      AOM_ICDF((((a3)-4) * ((CDF_INIT_TOP >> CDF_SHIFT) - 9) + \
                ((CDF_INIT_TOP - 9) >> 1)) /                   \
                   ((CDF_INIT_TOP - 9)) +                      \
               4),                                             \
      AOM_ICDF((((a4)-5) * ((CDF_INIT_TOP >> CDF_SHIFT) - 9) + \
                ((CDF_INIT_TOP - 9) >> 1)) /                   \
                   ((CDF_INIT_TOP - 9)) +                      \
               5),                                             \
      AOM_ICDF((((a5)-6) * ((CDF_INIT_TOP >> CDF_SHIFT) - 9) + \
                ((CDF_INIT_TOP - 9) >> 1)) /                   \
                   ((CDF_INIT_TOP - 9)) +                      \
               6),                                             \
      AOM_ICDF((((a6)-7) * ((CDF_INIT_TOP >> CDF_SHIFT) - 9) + \
                ((CDF_INIT_TOP - 9) >> 1)) /                   \
                   ((CDF_INIT_TOP - 9)) +                      \
               7),                                             \
      AOM_ICDF((((a7)-8) * ((CDF_INIT_TOP >> CDF_SHIFT) - 9) + \
                ((CDF_INIT_TOP - 9) >> 1)) /                   \
                   ((CDF_INIT_TOP - 9)) +                      \
               8),                                             \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF10(a0, a1, a2, a3, a4, a5, a6, a7, a8)           \
  AOM_ICDF((((a0)-1) * ((CDF_INIT_TOP >> CDF_SHIFT) - 10) +     \
            ((CDF_INIT_TOP - 10) >> 1)) /                       \
               ((CDF_INIT_TOP - 10)) +                          \
           1)                                                   \
  ,                                                             \
      AOM_ICDF((((a1)-2) * ((CDF_INIT_TOP >> CDF_SHIFT) - 10) + \
                ((CDF_INIT_TOP - 10) >> 1)) /                   \
                   ((CDF_INIT_TOP - 10)) +                      \
               2),                                              \
      AOM_ICDF((((a2)-3) * ((CDF_INIT_TOP >> CDF_SHIFT) - 10) + \
                ((CDF_INIT_TOP - 10) >> 1)) /                   \
                   ((CDF_INIT_TOP - 10)) +                      \
               3),                                              \
      AOM_ICDF((((a3)-4) * ((CDF_INIT_TOP >> CDF_SHIFT) - 10) + \
                ((CDF_INIT_TOP - 10) >> 1)) /                   \
                   ((CDF_INIT_TOP - 10)) +                      \
               4),                                              \
      AOM_ICDF((((a4)-5) * ((CDF_INIT_TOP >> CDF_SHIFT) - 10) + \
                ((CDF_INIT_TOP - 10) >> 1)) /                   \
                   ((CDF_INIT_TOP - 10)) +                      \
               5),                                              \
      AOM_ICDF((((a5)-6) * ((CDF_INIT_TOP >> CDF_SHIFT) - 10) + \
                ((CDF_INIT_TOP - 10) >> 1)) /                   \
                   ((CDF_INIT_TOP - 10)) +                      \
               6),                                              \
      AOM_ICDF((((a6)-7) * ((CDF_INIT_TOP >> CDF_SHIFT) - 10) + \
                ((CDF_INIT_TOP - 10) >> 1)) /                   \
                   ((CDF_INIT_TOP - 10)) +                      \
               7),                                              \
      AOM_ICDF((((a7)-8) * ((CDF_INIT_TOP >> CDF_SHIFT) - 10) + \
                ((CDF_INIT_TOP - 10) >> 1)) /                   \
                   ((CDF_INIT_TOP - 10)) +                      \
               8),                                              \
      AOM_ICDF((((a8)-9) * ((CDF_INIT_TOP >> CDF_SHIFT) - 10) + \
                ((CDF_INIT_TOP - 10) >> 1)) /                   \
                   ((CDF_INIT_TOP - 10)) +                      \
               9),                                              \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF11(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9)        \
  AOM_ICDF((((a0)-1) * ((CDF_INIT_TOP >> CDF_SHIFT) - 11) +      \
            ((CDF_INIT_TOP - 11) >> 1)) /                        \
               ((CDF_INIT_TOP - 11)) +                           \
           1)                                                    \
  ,                                                              \
      AOM_ICDF((((a1)-2) * ((CDF_INIT_TOP >> CDF_SHIFT) - 11) +  \
                ((CDF_INIT_TOP - 11) >> 1)) /                    \
                   ((CDF_INIT_TOP - 11)) +                       \
               2),                                               \
      AOM_ICDF((((a2)-3) * ((CDF_INIT_TOP >> CDF_SHIFT) - 11) +  \
                ((CDF_INIT_TOP - 11) >> 1)) /                    \
                   ((CDF_INIT_TOP - 11)) +                       \
               3),                                               \
      AOM_ICDF((((a3)-4) * ((CDF_INIT_TOP >> CDF_SHIFT) - 11) +  \
                ((CDF_INIT_TOP - 11) >> 1)) /                    \
                   ((CDF_INIT_TOP - 11)) +                       \
               4),                                               \
      AOM_ICDF((((a4)-5) * ((CDF_INIT_TOP >> CDF_SHIFT) - 11) +  \
                ((CDF_INIT_TOP - 11) >> 1)) /                    \
                   ((CDF_INIT_TOP - 11)) +                       \
               5),                                               \
      AOM_ICDF((((a5)-6) * ((CDF_INIT_TOP >> CDF_SHIFT) - 11) +  \
                ((CDF_INIT_TOP - 11) >> 1)) /                    \
                   ((CDF_INIT_TOP - 11)) +                       \
               6),                                               \
      AOM_ICDF((((a6)-7) * ((CDF_INIT_TOP >> CDF_SHIFT) - 11) +  \
                ((CDF_INIT_TOP - 11) >> 1)) /                    \
                   ((CDF_INIT_TOP - 11)) +                       \
               7),                                               \
      AOM_ICDF((((a7)-8) * ((CDF_INIT_TOP >> CDF_SHIFT) - 11) +  \
                ((CDF_INIT_TOP - 11) >> 1)) /                    \
                   ((CDF_INIT_TOP - 11)) +                       \
               8),                                               \
      AOM_ICDF((((a8)-9) * ((CDF_INIT_TOP >> CDF_SHIFT) - 11) +  \
                ((CDF_INIT_TOP - 11) >> 1)) /                    \
                   ((CDF_INIT_TOP - 11)) +                       \
               9),                                               \
      AOM_ICDF((((a9)-10) * ((CDF_INIT_TOP >> CDF_SHIFT) - 11) + \
                ((CDF_INIT_TOP - 11) >> 1)) /                    \
                   ((CDF_INIT_TOP - 11)) +                       \
               10),                                              \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF12(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)    \
  AOM_ICDF((((a0)-1) * ((CDF_INIT_TOP >> CDF_SHIFT) - 12) +       \
            ((CDF_INIT_TOP - 12) >> 1)) /                         \
               ((CDF_INIT_TOP - 12)) +                            \
           1)                                                     \
  ,                                                               \
      AOM_ICDF((((a1)-2) * ((CDF_INIT_TOP >> CDF_SHIFT) - 12) +   \
                ((CDF_INIT_TOP - 12) >> 1)) /                     \
                   ((CDF_INIT_TOP - 12)) +                        \
               2),                                                \
      AOM_ICDF((((a2)-3) * ((CDF_INIT_TOP >> CDF_SHIFT) - 12) +   \
                ((CDF_INIT_TOP - 12) >> 1)) /                     \
                   ((CDF_INIT_TOP - 12)) +                        \
               3),                                                \
      AOM_ICDF((((a3)-4) * ((CDF_INIT_TOP >> CDF_SHIFT) - 12) +   \
                ((CDF_INIT_TOP - 12) >> 1)) /                     \
                   ((CDF_INIT_TOP - 12)) +                        \
               4),                                                \
      AOM_ICDF((((a4)-5) * ((CDF_INIT_TOP >> CDF_SHIFT) - 12) +   \
                ((CDF_INIT_TOP - 12) >> 1)) /                     \
                   ((CDF_INIT_TOP - 12)) +                        \
               5),                                                \
      AOM_ICDF((((a5)-6) * ((CDF_INIT_TOP >> CDF_SHIFT) - 12) +   \
                ((CDF_INIT_TOP - 12) >> 1)) /                     \
                   ((CDF_INIT_TOP - 12)) +                        \
               6),                                                \
      AOM_ICDF((((a6)-7) * ((CDF_INIT_TOP >> CDF_SHIFT) - 12) +   \
                ((CDF_INIT_TOP - 12) >> 1)) /                     \
                   ((CDF_INIT_TOP - 12)) +                        \
               7),                                                \
      AOM_ICDF((((a7)-8) * ((CDF_INIT_TOP >> CDF_SHIFT) - 12) +   \
                ((CDF_INIT_TOP - 12) >> 1)) /                     \
                   ((CDF_INIT_TOP - 12)) +                        \
               8),                                                \
      AOM_ICDF((((a8)-9) * ((CDF_INIT_TOP >> CDF_SHIFT) - 12) +   \
                ((CDF_INIT_TOP - 12) >> 1)) /                     \
                   ((CDF_INIT_TOP - 12)) +                        \
               9),                                                \
      AOM_ICDF((((a9)-10) * ((CDF_INIT_TOP >> CDF_SHIFT) - 12) +  \
                ((CDF_INIT_TOP - 12) >> 1)) /                     \
                   ((CDF_INIT_TOP - 12)) +                        \
               10),                                               \
      AOM_ICDF((((a10)-11) * ((CDF_INIT_TOP >> CDF_SHIFT) - 12) + \
                ((CDF_INIT_TOP - 12) >> 1)) /                     \
                   ((CDF_INIT_TOP - 12)) +                        \
               11),                                               \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF13(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11) \
  AOM_ICDF((((a0)-1) * ((CDF_INIT_TOP >> CDF_SHIFT) - 13) +         \
            ((CDF_INIT_TOP - 13) >> 1)) /                           \
               ((CDF_INIT_TOP - 13)) +                              \
           1)                                                       \
  ,                                                                 \
      AOM_ICDF((((a1)-2) * ((CDF_INIT_TOP >> CDF_SHIFT) - 13) +     \
                ((CDF_INIT_TOP - 13) >> 1)) /                       \
                   ((CDF_INIT_TOP - 13)) +                          \
               2),                                                  \
      AOM_ICDF((((a2)-3) * ((CDF_INIT_TOP >> CDF_SHIFT) - 13) +     \
                ((CDF_INIT_TOP - 13) >> 1)) /                       \
                   ((CDF_INIT_TOP - 13)) +                          \
               3),                                                  \
      AOM_ICDF((((a3)-4) * ((CDF_INIT_TOP >> CDF_SHIFT) - 13) +     \
                ((CDF_INIT_TOP - 13) >> 1)) /                       \
                   ((CDF_INIT_TOP - 13)) +                          \
               4),                                                  \
      AOM_ICDF((((a4)-5) * ((CDF_INIT_TOP >> CDF_SHIFT) - 13) +     \
                ((CDF_INIT_TOP - 13) >> 1)) /                       \
                   ((CDF_INIT_TOP - 13)) +                          \
               5),                                                  \
      AOM_ICDF((((a5)-6) * ((CDF_INIT_TOP >> CDF_SHIFT) - 13) +     \
                ((CDF_INIT_TOP - 13) >> 1)) /                       \
                   ((CDF_INIT_TOP - 13)) +                          \
               6),                                                  \
      AOM_ICDF((((a6)-7) * ((CDF_INIT_TOP >> CDF_SHIFT) - 13) +     \
                ((CDF_INIT_TOP - 13) >> 1)) /                       \
                   ((CDF_INIT_TOP - 13)) +                          \
               7),                                                  \
      AOM_ICDF((((a7)-8) * ((CDF_INIT_TOP >> CDF_SHIFT) - 13) +     \
                ((CDF_INIT_TOP - 13) >> 1)) /                       \
                   ((CDF_INIT_TOP - 13)) +                          \
               8),                                                  \
      AOM_ICDF((((a8)-9) * ((CDF_INIT_TOP >> CDF_SHIFT) - 13) +     \
                ((CDF_INIT_TOP - 13) >> 1)) /                       \
                   ((CDF_INIT_TOP - 13)) +                          \
               9),                                                  \
      AOM_ICDF((((a9)-10) * ((CDF_INIT_TOP >> CDF_SHIFT) - 13) +    \
                ((CDF_INIT_TOP - 13) >> 1)) /                       \
                   ((CDF_INIT_TOP - 13)) +                          \
               10),                                                 \
      AOM_ICDF((((a10)-11) * ((CDF_INIT_TOP >> CDF_SHIFT) - 13) +   \
                ((CDF_INIT_TOP - 13) >> 1)) /                       \
                   ((CDF_INIT_TOP - 13)) +                          \
               11),                                                 \
      AOM_ICDF((((a11)-12) * ((CDF_INIT_TOP >> CDF_SHIFT) - 13) +   \
                ((CDF_INIT_TOP - 13) >> 1)) /                       \
                   ((CDF_INIT_TOP - 13)) +                          \
               12),                                                 \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF14(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12) \
  AOM_ICDF((((a0)-1) * ((CDF_INIT_TOP >> CDF_SHIFT) - 14) +              \
            ((CDF_INIT_TOP - 14) >> 1)) /                                \
               ((CDF_INIT_TOP - 14)) +                                   \
           1)                                                            \
  ,                                                                      \
      AOM_ICDF((((a1)-2) * ((CDF_INIT_TOP >> CDF_SHIFT) - 14) +          \
                ((CDF_INIT_TOP - 14) >> 1)) /                            \
                   ((CDF_INIT_TOP - 14)) +                               \
               2),                                                       \
      AOM_ICDF((((a2)-3) * ((CDF_INIT_TOP >> CDF_SHIFT) - 14) +          \
                ((CDF_INIT_TOP - 14) >> 1)) /                            \
                   ((CDF_INIT_TOP - 14)) +                               \
               3),                                                       \
      AOM_ICDF((((a3)-4) * ((CDF_INIT_TOP >> CDF_SHIFT) - 14) +          \
                ((CDF_INIT_TOP - 14) >> 1)) /                            \
                   ((CDF_INIT_TOP - 14)) +                               \
               4),                                                       \
      AOM_ICDF((((a4)-5) * ((CDF_INIT_TOP >> CDF_SHIFT) - 14) +          \
                ((CDF_INIT_TOP - 14) >> 1)) /                            \
                   ((CDF_INIT_TOP - 14)) +                               \
               5),                                                       \
      AOM_ICDF((((a5)-6) * ((CDF_INIT_TOP >> CDF_SHIFT) - 14) +          \
                ((CDF_INIT_TOP - 14) >> 1)) /                            \
                   ((CDF_INIT_TOP - 14)) +                               \
               6),                                                       \
      AOM_ICDF((((a6)-7) * ((CDF_INIT_TOP >> CDF_SHIFT) - 14) +          \
                ((CDF_INIT_TOP - 14) >> 1)) /                            \
                   ((CDF_INIT_TOP - 14)) +                               \
               7),                                                       \
      AOM_ICDF((((a7)-8) * ((CDF_INIT_TOP >> CDF_SHIFT) - 14) +          \
                ((CDF_INIT_TOP - 14) >> 1)) /                            \
                   ((CDF_INIT_TOP - 14)) +                               \
               8),                                                       \
      AOM_ICDF((((a8)-9) * ((CDF_INIT_TOP >> CDF_SHIFT) - 14) +          \
                ((CDF_INIT_TOP - 14) >> 1)) /                            \
                   ((CDF_INIT_TOP - 14)) +                               \
               9),                                                       \
      AOM_ICDF((((a9)-10) * ((CDF_INIT_TOP >> CDF_SHIFT) - 14) +         \
                ((CDF_INIT_TOP - 14) >> 1)) /                            \
                   ((CDF_INIT_TOP - 14)) +                               \
               10),                                                      \
      AOM_ICDF((((a10)-11) * ((CDF_INIT_TOP >> CDF_SHIFT) - 14) +        \
                ((CDF_INIT_TOP - 14) >> 1)) /                            \
                   ((CDF_INIT_TOP - 14)) +                               \
               11),                                                      \
      AOM_ICDF((((a11)-12) * ((CDF_INIT_TOP >> CDF_SHIFT) - 14) +        \
                ((CDF_INIT_TOP - 14) >> 1)) /                            \
                   ((CDF_INIT_TOP - 14)) +                               \
               12),                                                      \
      AOM_ICDF((((a12)-13) * ((CDF_INIT_TOP >> CDF_SHIFT) - 14) +        \
                ((CDF_INIT_TOP - 14) >> 1)) /                            \
                   ((CDF_INIT_TOP - 14)) +                               \
               13),                                                      \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF15(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13) \
  AOM_ICDF((((a0)-1) * ((CDF_INIT_TOP >> CDF_SHIFT) - 15) +                   \
            ((CDF_INIT_TOP - 15) >> 1)) /                                     \
               ((CDF_INIT_TOP - 15)) +                                        \
           1)                                                                 \
  ,                                                                           \
      AOM_ICDF((((a1)-2) * ((CDF_INIT_TOP >> CDF_SHIFT) - 15) +               \
                ((CDF_INIT_TOP - 15) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 15)) +                                    \
               2),                                                            \
      AOM_ICDF((((a2)-3) * ((CDF_INIT_TOP >> CDF_SHIFT) - 15) +               \
                ((CDF_INIT_TOP - 15) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 15)) +                                    \
               3),                                                            \
      AOM_ICDF((((a3)-4) * ((CDF_INIT_TOP >> CDF_SHIFT) - 15) +               \
                ((CDF_INIT_TOP - 15) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 15)) +                                    \
               4),                                                            \
      AOM_ICDF((((a4)-5) * ((CDF_INIT_TOP >> CDF_SHIFT) - 15) +               \
                ((CDF_INIT_TOP - 15) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 15)) +                                    \
               5),                                                            \
      AOM_ICDF((((a5)-6) * ((CDF_INIT_TOP >> CDF_SHIFT) - 15) +               \
                ((CDF_INIT_TOP - 15) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 15)) +                                    \
               6),                                                            \
      AOM_ICDF((((a6)-7) * ((CDF_INIT_TOP >> CDF_SHIFT) - 15) +               \
                ((CDF_INIT_TOP - 15) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 15)) +                                    \
               7),                                                            \
      AOM_ICDF((((a7)-8) * ((CDF_INIT_TOP >> CDF_SHIFT) - 15) +               \
                ((CDF_INIT_TOP - 15) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 15)) +                                    \
               8),                                                            \
      AOM_ICDF((((a8)-9) * ((CDF_INIT_TOP >> CDF_SHIFT) - 15) +               \
                ((CDF_INIT_TOP - 15) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 15)) +                                    \
               9),                                                            \
      AOM_ICDF((((a9)-10) * ((CDF_INIT_TOP >> CDF_SHIFT) - 15) +              \
                ((CDF_INIT_TOP - 15) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 15)) +                                    \
               10),                                                           \
      AOM_ICDF((((a10)-11) * ((CDF_INIT_TOP >> CDF_SHIFT) - 15) +             \
                ((CDF_INIT_TOP - 15) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 15)) +                                    \
               11),                                                           \
      AOM_ICDF((((a11)-12) * ((CDF_INIT_TOP >> CDF_SHIFT) - 15) +             \
                ((CDF_INIT_TOP - 15) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 15)) +                                    \
               12),                                                           \
      AOM_ICDF((((a12)-13) * ((CDF_INIT_TOP >> CDF_SHIFT) - 15) +             \
                ((CDF_INIT_TOP - 15) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 15)) +                                    \
               13),                                                           \
      AOM_ICDF((((a13)-14) * ((CDF_INIT_TOP >> CDF_SHIFT) - 15) +             \
                ((CDF_INIT_TOP - 15) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 15)) +                                    \
               14),                                                           \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF16(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, \
                  a14)                                                        \
  AOM_ICDF((((a0)-1) * ((CDF_INIT_TOP >> CDF_SHIFT) - 16) +                   \
            ((CDF_INIT_TOP - 16) >> 1)) /                                     \
               ((CDF_INIT_TOP - 16)) +                                        \
           1)                                                                 \
  ,                                                                           \
      AOM_ICDF((((a1)-2) * ((CDF_INIT_TOP >> CDF_SHIFT) - 16) +               \
                ((CDF_INIT_TOP - 16) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 16)) +                                    \
               2),                                                            \
      AOM_ICDF((((a2)-3) * ((CDF_INIT_TOP >> CDF_SHIFT) - 16) +               \
                ((CDF_INIT_TOP - 16) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 16)) +                                    \
               3),                                                            \
      AOM_ICDF((((a3)-4) * ((CDF_INIT_TOP >> CDF_SHIFT) - 16) +               \
                ((CDF_INIT_TOP - 16) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 16)) +                                    \
               4),                                                            \
      AOM_ICDF((((a4)-5) * ((CDF_INIT_TOP >> CDF_SHIFT) - 16) +               \
                ((CDF_INIT_TOP - 16) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 16)) +                                    \
               5),                                                            \
      AOM_ICDF((((a5)-6) * ((CDF_INIT_TOP >> CDF_SHIFT) - 16) +               \
                ((CDF_INIT_TOP - 16) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 16)) +                                    \
               6),                                                            \
      AOM_ICDF((((a6)-7) * ((CDF_INIT_TOP >> CDF_SHIFT) - 16) +               \
                ((CDF_INIT_TOP - 16) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 16)) +                                    \
               7),                                                            \
      AOM_ICDF((((a7)-8) * ((CDF_INIT_TOP >> CDF_SHIFT) - 16) +               \
                ((CDF_INIT_TOP - 16) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 16)) +                                    \
               8),                                                            \
      AOM_ICDF((((a8)-9) * ((CDF_INIT_TOP >> CDF_SHIFT) - 16) +               \
                ((CDF_INIT_TOP - 16) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 16)) +                                    \
               9),                                                            \
      AOM_ICDF((((a9)-10) * ((CDF_INIT_TOP >> CDF_SHIFT) - 16) +              \
                ((CDF_INIT_TOP - 16) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 16)) +                                    \
               10),                                                           \
      AOM_ICDF((((a10)-11) * ((CDF_INIT_TOP >> CDF_SHIFT) - 16) +             \
                ((CDF_INIT_TOP - 16) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 16)) +                                    \
               11),                                                           \
      AOM_ICDF((((a11)-12) * ((CDF_INIT_TOP >> CDF_SHIFT) - 16) +             \
                ((CDF_INIT_TOP - 16) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 16)) +                                    \
               12),                                                           \
      AOM_ICDF((((a12)-13) * ((CDF_INIT_TOP >> CDF_SHIFT) - 16) +             \
                ((CDF_INIT_TOP - 16) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 16)) +                                    \
               13),                                                           \
      AOM_ICDF((((a13)-14) * ((CDF_INIT_TOP >> CDF_SHIFT) - 16) +             \
                ((CDF_INIT_TOP - 16) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 16)) +                                    \
               14),                                                           \
      AOM_ICDF((((a14)-15) * ((CDF_INIT_TOP >> CDF_SHIFT) - 16) +             \
                ((CDF_INIT_TOP - 16) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 16)) +                                    \
               15),                                                           \
      AOM_ICDF(CDF_PROB_TOP), 0

#endif

static INLINE uint8_t get_prob(unsigned int num, unsigned int den) {
  assert(den != 0);
  {
    const int p = (int)(((uint64_t)num * 256 + (den >> 1)) / den);
    // (p > 255) ? 255 : (p < 1) ? 1 : p;
    const int clipped_prob = p | ((255 - p) >> 23) | (p == 0);
    return (uint8_t)clipped_prob;
  }
}

static INLINE void update_cdf(aom_cdf_prob *cdf, int8_t val, int nsymbs) {
  assert(nsymbs < 17);
  const int count = cdf[nsymbs];

  // rate is computed in the spec as:
  //  3 + ( cdf[N] > 15 ) + ( cdf[N] > 31 ) + Min(FloorLog2(N), 2)
  // In this case cdf[N] is |count|.
  // Min(FloorLog2(N), 2) is 1 for nsymbs == {2, 3} and 2 for all
  // nsymbs > 3. So the equation becomes:
  //  4 + (count > 15) + (count > 31) + (nsymbs > 3).
  // Note that the largest value for count is 32 (it is not incremented beyond
  // 32). So using that information:
  //  count >> 4 is 0 for count from 0 to 15.
  //  count >> 4 is 1 for count from 16 to 31.
  //  count >> 4 is 2 for count == 31.
  // Now, the equation becomes:
  //  4 + (count >> 4) + (nsymbs > 3).
  const int rate = 4 + (count >> 4) + (nsymbs > 3);

  int i = 0;
  do {
    if (i < val) {
      cdf[i] += (CDF_PROB_TOP - cdf[i]) >> rate;
    } else {
      cdf[i] -= cdf[i] >> rate;
    }
  } while (++i < nsymbs - 1);
  cdf[nsymbs] += (count < 32);
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AOM_DSP_PROB_H_
