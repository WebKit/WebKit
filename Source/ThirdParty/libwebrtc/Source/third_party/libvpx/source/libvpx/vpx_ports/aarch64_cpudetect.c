/*
 *  Copyright (c) 2023 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "./vpx_config.h"
#include "arm_cpudetect.h"

#if defined(__APPLE__)
#include <sys/sysctl.h>
#endif

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

#elif defined(__APPLE__)  // end !CONFIG_RUNTIME_CPU_DETECT

// sysctlbyname() parameter documentation for instruction set characteristics:
// https://developer.apple.com/documentation/kernel/1387446-sysctlbyname/determining_instruction_set_characteristics
#if HAVE_NEON_DOTPROD || HAVE_NEON_I8MM
static INLINE int64_t have_feature(const char *feature) {
  int64_t feature_present = 0;
  size_t size = sizeof(feature_present);
  if (sysctlbyname(feature, &feature_present, &size, NULL, 0) != 0) {
    return 0;
  }
  return feature_present;
}
#endif

static int arm_get_cpu_caps(void) {
  int flags = 0;
#if HAVE_NEON
  flags |= HAS_NEON;
#endif  // HAVE_NEON
#if HAVE_NEON_DOTPROD
  if (have_feature("hw.optional.arm.FEAT_DotProd")) {
    flags |= HAS_NEON_DOTPROD;
  }
#endif  // HAVE_NEON_DOTPROD
#if HAVE_NEON_I8MM
  if (have_feature("hw.optional.arm.FEAT_I8MM")) {
    flags |= HAS_NEON_I8MM;
  }
#endif  // HAVE_NEON_I8MM
  return flags;
}

#elif defined(_WIN32)  // end __APPLE__

static int arm_get_cpu_caps(void) {
  int flags = 0;
// IsProcessorFeaturePresent() parameter documentation:
// https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-isprocessorfeaturepresent#parameters
#if HAVE_NEON
  flags |= HAS_NEON;  // Neon is mandatory in Armv8.0-A.
#endif  // HAVE_NEON
#if HAVE_NEON_DOTPROD
// Support for PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE was added in Windows SDK
// 20348, supported by Windows 11 and Windows Server 2022.
#if defined(PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE)
  if (IsProcessorFeaturePresent(PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE)) {
    flags |= HAS_NEON_DOTPROD;
  }
#endif  // defined(PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE)
#endif  // HAVE_NEON_DOTPROD
  // No I8MM feature detection available on Windows at time of writing.
  return flags;
}

#elif defined(ANDROID_USE_CPU_FEATURES_LIB)

static int arm_get_cpu_caps(void) {
  int flags = 0;
#if HAVE_NEON
  flags |= HAS_NEON;  // Neon is mandatory in Armv8.0-A.
#endif  // HAVE_NEON
  return flags;
}

#elif defined(__linux__)  // end defined(VPX_USE_ANDROID_CPU_FEATURES)

#include <sys/auxv.h>

// Define hwcap values ourselves: building with an old auxv header where these
// hwcap values are not defined should not prevent features from being enabled.
#define VPX_AARCH64_HWCAP_ASIMDDP (1 << 20)
#define VPX_AARCH64_HWCAP2_I8MM (1 << 13)

static int arm_get_cpu_caps(void) {
  int flags = 0;
  unsigned long hwcap = getauxval(AT_HWCAP);
  unsigned long hwcap2 = getauxval(AT_HWCAP2);
#if HAVE_NEON
  flags |= HAS_NEON;  // Neon is mandatory in Armv8.0-A.
#endif  // HAVE_NEON
#if HAVE_NEON_DOTPROD
  if (hwcap & VPX_AARCH64_HWCAP_ASIMDDP) {
    flags |= HAS_NEON_DOTPROD;
  }
#endif  // HAVE_NEON_DOTPROD
#if HAVE_NEON_I8MM
  if (hwcap2 & VPX_AARCH64_HWCAP2_I8MM) {
    flags |= HAS_NEON_I8MM;
  }
#endif  // HAVE_NEON_I8MM
  return flags;
}

#elif defined(__Fuchsia__)  // end __linux__

#include <zircon/features.h>
#include <zircon/syscalls.h>

// Added in https://fuchsia-review.googlesource.com/c/fuchsia/+/894282.
#ifndef ZX_ARM64_FEATURE_ISA_I8MM
#define ZX_ARM64_FEATURE_ISA_I8MM ((uint32_t)(1u << 19))
#endif

static int arm_get_cpu_caps(void) {
  int flags = 0;
#if HAVE_NEON
  flags |= HAS_NEON;  // Neon is mandatory in Armv8.0-A.
#endif  // HAVE_NEON
  uint32_t features;
  zx_status_t status = zx_system_get_features(ZX_FEATURE_KIND_CPU, &features);
  if (status != ZX_OK) {
    return flags;
  }
#if HAVE_NEON_DOTPROD
  if (features & ZX_ARM64_FEATURE_ISA_DP) {
    flags |= HAS_NEON_DOTPROD;
  }
#endif  // HAVE_NEON_DOTPROD
#if HAVE_NEON_I8MM
  if (features & ZX_ARM64_FEATURE_ISA_I8MM) {
    flags |= HAS_NEON_I8MM;
  }
#endif  // HAVE_NEON_I8MM
  return flags;
}

#else  // end __Fuchsia__
#error \
    "Runtime CPU detection selected, but no CPU detection method available" \
"for your platform. Rerun configure with --disable-runtime-cpu-detect."
#endif

int arm_cpu_caps(void) {
  int flags = 0;
  if (!arm_cpu_env_flags(&flags)) {
    flags = arm_get_cpu_caps() & arm_cpu_env_mask();
  }

  // Restrict flags: FEAT_I8MM assumes that FEAT_DotProd is available.
  if (!(flags & HAS_NEON_DOTPROD)) {
    flags &= ~HAS_NEON_I8MM;
  }

  return flags;
}
