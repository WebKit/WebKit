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

#include <stdlib.h>
#include <string.h>
#include "aom_ports/arm.h"
#include "config/aom_config.h"

#ifdef WINAPI_FAMILY
#include <winapifamily.h>
#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#define getenv(x) NULL
#endif
#endif

static int arm_cpu_env_flags(int *flags) {
  char *env;
  env = getenv("AOM_SIMD_CAPS");
  if (env && *env) {
    *flags = (int)strtol(env, NULL, 0);
    return 0;
  }
  *flags = 0;
  return -1;
}

static int arm_cpu_env_mask(void) {
  char *env;
  env = getenv("AOM_SIMD_CAPS_MASK");
  return env && *env ? (int)strtol(env, NULL, 0) : ~0;
}

#if !CONFIG_RUNTIME_CPU_DETECT || defined(__APPLE__)

int aom_arm_cpu_caps(void) {
  /* This function should actually be a no-op. There is no way to adjust any of
   * these because the RTCD tables do not exist: the functions are called
   * statically */
  int flags;
  int mask;
  if (!arm_cpu_env_flags(&flags)) {
    return flags;
  }
  mask = arm_cpu_env_mask();
#if HAVE_NEON
  flags |= HAS_NEON;
#endif /* HAVE_NEON */
  return flags & mask;
}

#elif defined(_MSC_VER) /* end !CONFIG_RUNTIME_CPU_DETECT || __APPLE__ */
/*For GetExceptionCode() and EXCEPTION_ILLEGAL_INSTRUCTION.*/
#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#include <windows.h>

int aom_arm_cpu_caps(void) {
  int flags;
  int mask;
  if (!arm_cpu_env_flags(&flags)) {
    return flags;
  }
  mask = arm_cpu_env_mask();
/* MSVC has no inline __asm support for ARM, but it does let you __emit
 *  instructions via their assembled hex code.
 * All of these instructions should be essentially nops.
 */
#if HAVE_NEON
  if (mask & HAS_NEON) {
    __try {
      /*VORR q0,q0,q0*/
      __emit(0xF2200150);
      flags |= HAS_NEON;
    } __except (GetExceptionCode() == EXCEPTION_ILLEGAL_INSTRUCTION) {
      /*Ignore exception.*/
    }
  }
#endif /* HAVE_NEON */
  return flags & mask;
}

#elif defined(__ANDROID__) /* end _MSC_VER */
#include <cpu-features.h>

int aom_arm_cpu_caps(void) {
  int flags;
  int mask;
  uint64_t features;
  if (!arm_cpu_env_flags(&flags)) {
    return flags;
  }
  mask = arm_cpu_env_mask();
  features = android_getCpuFeatures();

#if HAVE_NEON
  if (features & ANDROID_CPU_ARM_FEATURE_NEON) flags |= HAS_NEON;
#endif /* HAVE_NEON */
  return flags & mask;
}

#elif defined(__linux__) /* end __ANDROID__ */

#include <stdio.h>

int aom_arm_cpu_caps(void) {
  FILE *fin;
  int flags;
  int mask;
  if (!arm_cpu_env_flags(&flags)) {
    return flags;
  }
  mask = arm_cpu_env_mask();
  /* Reading /proc/self/auxv would be easier, but that doesn't work reliably
   *  on Android.
   * This also means that detection will fail in Scratchbox.
   */
  fin = fopen("/proc/cpuinfo", "r");
  if (fin != NULL) {
    /* 512 should be enough for anybody (it's even enough for all the flags
     * that x86 has accumulated... so far).
     */
    char buf[512];
    while (fgets(buf, 511, fin) != NULL) {
#if HAVE_NEON
      if (memcmp(buf, "Features", 8) == 0) {
        char *p;
        p = strstr(buf, " neon");
        if (p != NULL && (p[5] == ' ' || p[5] == '\n')) {
          flags |= HAS_NEON;
        }
      }
#endif /* HAVE_NEON */
    }
    fclose(fin);
  }
  return flags & mask;
}
#else  /* end __linux__ */
#error \
    "Runtime CPU detection selected, but no CPU detection method " \
"available for your platform. Rerun cmake with -DCONFIG_RUNTIME_CPU_DETECT=0."
#endif
