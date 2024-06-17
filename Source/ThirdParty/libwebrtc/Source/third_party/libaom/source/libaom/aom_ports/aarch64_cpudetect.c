/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "config/aom_config.h"

#include "arm_cpudetect.h"

#include "aom_ports/arm.h"

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
static INLINE bool have_feature(const char *feature) {
  int64_t feature_present = 0;
  size_t size = sizeof(feature_present);
  if (sysctlbyname(feature, &feature_present, &size, NULL, 0) != 0) {
    return false;
  }
  return feature_present;
}

static int arm_get_cpu_caps(void) {
  int flags = 0;
#if HAVE_NEON
  flags |= HAS_NEON;
#endif  // HAVE_NEON
#if HAVE_ARM_CRC32
  if (have_feature("hw.optional.armv8_crc32")) flags |= HAS_ARM_CRC32;
#endif  // HAVE_ARM_CRC32
#if HAVE_NEON_DOTPROD
  if (have_feature("hw.optional.arm.FEAT_DotProd")) flags |= HAS_NEON_DOTPROD;
#endif  // HAVE_NEON_DOTPROD
#if HAVE_NEON_I8MM
  if (have_feature("hw.optional.arm.FEAT_I8MM")) flags |= HAS_NEON_I8MM;
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
#if HAVE_ARM_CRC32
  if (IsProcessorFeaturePresent(PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE)) {
    flags |= HAS_ARM_CRC32;
  }
#endif  // HAVE_ARM_CRC32
#if HAVE_NEON_DOTPROD
// Support for PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE was added in Windows SDK
// 20348, supported by Windows 11 and Windows Server 2022.
#if defined(PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE)
  if (IsProcessorFeaturePresent(PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE)) {
    flags |= HAS_NEON_DOTPROD;
  }
#endif  // defined(PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE)
#endif  // HAVE_NEON_DOTPROD
  // No I8MM or SVE feature detection available on Windows at time of writing.
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

#elif defined(__linux__)  // end defined(AOM_USE_ANDROID_CPU_FEATURES)

#include <sys/auxv.h>

// Define hwcap values ourselves: building with an old auxv header where these
// hwcap values are not defined should not prevent features from being enabled.
#define AOM_AARCH64_HWCAP_CRC32 (1 << 7)
#define AOM_AARCH64_HWCAP_ASIMDDP (1 << 20)
#define AOM_AARCH64_HWCAP_SVE (1 << 22)
#define AOM_AARCH64_HWCAP2_SVE2 (1 << 1)
#define AOM_AARCH64_HWCAP2_I8MM (1 << 13)

static int arm_get_cpu_caps(void) {
  int flags = 0;
#if HAVE_ARM_CRC32 || HAVE_NEON_DOTPROD || HAVE_SVE
  unsigned long hwcap = getauxval(AT_HWCAP);
#endif
#if HAVE_NEON_I8MM || HAVE_SVE2
  unsigned long hwcap2 = getauxval(AT_HWCAP2);
#endif

#if HAVE_NEON
  flags |= HAS_NEON;  // Neon is mandatory in Armv8.0-A.
#endif  // HAVE_NEON
#if HAVE_ARM_CRC32
  if (hwcap & AOM_AARCH64_HWCAP_CRC32) flags |= HAS_ARM_CRC32;
#endif  // HAVE_ARM_CRC32
#if HAVE_NEON_DOTPROD
  if (hwcap & AOM_AARCH64_HWCAP_ASIMDDP) flags |= HAS_NEON_DOTPROD;
#endif  // HAVE_NEON_DOTPROD
#if HAVE_NEON_I8MM
  if (hwcap2 & AOM_AARCH64_HWCAP2_I8MM) flags |= HAS_NEON_I8MM;
#endif  // HAVE_NEON_I8MM
#if HAVE_SVE
  if (hwcap & AOM_AARCH64_HWCAP_SVE) flags |= HAS_SVE;
#endif  // HAVE_SVE
#if HAVE_SVE2
  if (hwcap2 & AOM_AARCH64_HWCAP2_SVE2) flags |= HAS_SVE2;
#endif  // HAVE_SVE2
  return flags;
}

#elif defined(__Fuchsia__)  // end __linux__

#include <zircon/features.h>
#include <zircon/syscalls.h>

// Added in https://fuchsia-review.googlesource.com/c/fuchsia/+/894282.
#ifndef ZX_ARM64_FEATURE_ISA_I8MM
#define ZX_ARM64_FEATURE_ISA_I8MM ((uint32_t)(1u << 19))
#endif
// Added in https://fuchsia-review.googlesource.com/c/fuchsia/+/895083.
#ifndef ZX_ARM64_FEATURE_ISA_SVE
#define ZX_ARM64_FEATURE_ISA_SVE ((uint32_t)(1u << 20))
#endif

static int arm_get_cpu_caps(void) {
  int flags = 0;
#if HAVE_NEON
  flags |= HAS_NEON;  // Neon is mandatory in Armv8.0-A.
#endif  // HAVE_NEON
  uint32_t features;
  zx_status_t status = zx_system_get_features(ZX_FEATURE_KIND_CPU, &features);
  if (status != ZX_OK) return flags;
#if HAVE_ARM_CRC32
  if (features & ZX_ARM64_FEATURE_ISA_CRC32) flags |= HAS_ARM_CRC32;
#endif  // HAVE_ARM_CRC32
#if HAVE_NEON_DOTPROD
  if (features & ZX_ARM64_FEATURE_ISA_DP) flags |= HAS_NEON_DOTPROD;
#endif  // HAVE_NEON_DOTPROD
#if HAVE_NEON_I8MM
  if (features & ZX_ARM64_FEATURE_ISA_I8MM) flags |= HAS_NEON_I8MM;
#endif  // HAVE_NEON_I8MM
#if HAVE_SVE
  if (features & ZX_ARM64_FEATURE_ISA_SVE) flags |= HAS_SVE;
#endif  // HAVE_SVE
  return flags;
}

#else  // end __Fuchsia__
#error \
    "Runtime CPU detection selected, but no CPU detection method " \
"available for your platform. Rerun cmake with -DCONFIG_RUNTIME_CPU_DETECT=0."
#endif

int aom_arm_cpu_caps(void) {
  int flags = 0;
  if (!arm_cpu_env_flags(&flags)) {
    flags = arm_get_cpu_caps() & arm_cpu_env_mask();
  }

  // Restrict flags: FEAT_I8MM assumes that FEAT_DotProd is available.
  if (!(flags & HAS_NEON_DOTPROD)) flags &= ~HAS_NEON_I8MM;

  // Restrict flags: SVE assumes that FEAT_{DotProd,I8MM} are available.
  if (!(flags & HAS_NEON_DOTPROD)) flags &= ~HAS_SVE;
  if (!(flags & HAS_NEON_I8MM)) flags &= ~HAS_SVE;

  // Restrict flags: SVE2 assumes that FEAT_SVE is available.
  if (!(flags & HAS_SVE)) flags &= ~HAS_SVE2;

  return flags;
}
