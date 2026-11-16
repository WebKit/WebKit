/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

#include "config/aom_config.h"

#include "aom_ports/riscv.h"

#if CONFIG_RUNTIME_CPU_DETECT

#include <sys/auxv.h>

#define HWCAP_RVV (1 << ('v' - 'a'))

int riscv_simd_caps(void) {
  int flags = 0;
#if HAVE_RVV
  unsigned long hwcap = getauxval(AT_HWCAP);
  if (hwcap & HWCAP_RVV) flags |= HAS_RVV;
#endif
  return flags;
}
#else
// If there is no RTCD the function pointers are not used and can not be
// changed.
int riscv_simd_caps(void) { return 0; }
#endif  // CONFIG_RUNTIME_CPU_DETECT
