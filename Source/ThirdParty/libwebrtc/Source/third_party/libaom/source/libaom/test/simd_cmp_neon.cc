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

#if defined(__OPTIMIZE__) && __OPTIMIZE__
#define ARCH NEON
#define ARCH_POSTFIX(name) name##_neon
#define SIMD_NAMESPACE simd_test_neon
#include "test/simd_cmp_impl.h"
#endif
