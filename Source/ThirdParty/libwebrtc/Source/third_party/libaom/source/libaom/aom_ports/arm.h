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

#ifndef AOM_AOM_PORTS_ARM_H_
#define AOM_AOM_PORTS_ARM_H_
#include <stdlib.h>

#include "config/aom_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/*ARMv5TE "Enhanced DSP" instructions.*/
#define HAS_EDSP 0x01
/*ARMv6 "Parallel" or "Media" instructions.*/
#define HAS_MEDIA 0x02
/*ARMv7 optional NEON instructions.*/
#define HAS_NEON 0x04

int aom_arm_cpu_caps(void);

// Earlier gcc compilers have issues with some neon intrinsics
#if !defined(__clang__) && defined(__GNUC__) && __GNUC__ == 4 && \
    __GNUC_MINOR__ <= 6
#define AOM_INCOMPATIBLE_GCC
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AOM_PORTS_ARM_H_
