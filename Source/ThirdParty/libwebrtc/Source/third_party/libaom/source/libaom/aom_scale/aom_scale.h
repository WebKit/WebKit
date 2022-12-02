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

#ifndef AOM_AOM_SCALE_AOM_SCALE_H_
#define AOM_AOM_SCALE_AOM_SCALE_H_

#include "aom_scale/yv12config.h"

extern void aom_scale_frame(YV12_BUFFER_CONFIG *src, YV12_BUFFER_CONFIG *dst,
                            unsigned char *temp_area, unsigned char temp_height,
                            unsigned int hscale, unsigned int hratio,
                            unsigned int vscale, unsigned int vratio,
                            unsigned int interlaced, const int num_planes);

#endif  // AOM_AOM_SCALE_AOM_SCALE_H_
