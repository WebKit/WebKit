/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
// Feature detection code for Armv7-A / AArch32.

#include "arm_cpudetect.h"

#if !CONFIG_RUNTIME_CPU_DETECT

static int arm_get_cpu_caps(void) {
  // This function should actually be a no-op. There is no way to adjust any of
  // these because the RTCD tables do not exist: the functions are called
  // statically.
  int flags = 0;
#if HAVE_NEON
  flags |= HAS_NEON;
#endif  // HAVE_NEON
  return flags;
}

#elif defined(_MSC_VER)  // end !CONFIG_RUNTIME_CPU_DETECT

static int arm_get_cpu_caps(void) {
  int flags = 0;
#if HAVE_NEON
  // MSVC has no inline __asm support for Arm, but it does let you __emit
  // instructions via their assembled hex code.
  // All of these instructions should be essentially nops.
  __try {
    // VORR q0,q0,q0
    __emit(0xF2200150);
    flags |= HAS_NEON;
  } __except (GetExceptionCode() == EXCEPTION_ILLEGAL_INSTRUCTION) {
    // Ignore exception.
  }
#endif  // HAVE_NEON
  return flags;
}

#elif defined(AOM_USE_ANDROID_CPU_FEATURES)

static int arm_get_cpu_caps(void) {
  int flags = 0;
#if HAVE_NEON
  uint64_t features = android_getCpuFeatures();
  if (features & ANDROID_CPU_ARM_FEATURE_NEON) flags |= HAS_NEON;
#endif  // HAVE_NEON
  return flags;
}

#elif defined(__linux__)  // end defined(AOM_USE_ANDROID_CPU_FEATURES)

#include <sys/auxv.h>

// Define hwcap values ourselves: building with an old auxv header where these
// hwcap values are not defined should not prevent features from being enabled.
#define AOM_AARCH32_HWCAP_NEON (1 << 12)

static int arm_get_cpu_caps(void) {
  int flags = 0;
  unsigned long hwcap = getauxval(AT_HWCAP);
#if HAVE_NEON
  if (hwcap & AOM_AARCH32_HWCAP_NEON) flags |= HAS_NEON;
#endif  // HAVE_NEON
  return flags;
}
#else   // end __linux__
#error \
    "Runtime CPU detection selected, but no CPU detection method " \
"available for your platform. Rerun cmake with -DCONFIG_RUNTIME_CPU_DETECT=0."
#endif

int aom_arm_cpu_caps(void) {
  int flags = 0;
  if (arm_cpu_env_flags(&flags)) {
    return flags;
  }
  return arm_get_cpu_caps() & arm_cpu_env_mask();
}
