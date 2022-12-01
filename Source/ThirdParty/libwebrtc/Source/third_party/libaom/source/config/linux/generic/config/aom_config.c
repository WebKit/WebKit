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
#include "aom/aom_codec.h"
static const char* const cfg = "cmake ../source/libaom -G \"Unix Makefiles\" -DAOM_TARGET_CPU=generic -DCONFIG_AV1_ENCODER=1 -DCONFIG_LIBYUV=0 -DCONFIG_AV1_HIGHBITDEPTH=0 -DCONFIG_AV1_TEMPORAL_DENOISING=1 -DCONFIG_REALTIME_ONLY=1 -DCONFIG_MAX_DECODE_PROFILE=0 -DCONFIG_NORMAL_TILE_MODE=1 -DCONFIG_SIZE_LIMIT=1 -DDECODE_HEIGHT_LIMIT=16384 -DDECODE_WIDTH_LIMIT=16384";
const char *aom_codec_build_config(void) {return cfg;}
