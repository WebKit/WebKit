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

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "config/aom_config.h"

#if !CONFIG_SHARED
#include <string.h>

#include <string>

#if AOM_ARCH_ARM
#include "aom_ports/arm.h"
#endif
#if AOM_ARCH_X86 || AOM_ARCH_X86_64
#include "aom_ports/x86.h"
#endif

extern "C" {
extern void av1_rtcd();
extern void aom_dsp_rtcd();
extern void aom_scale_rtcd();
}

#if AOM_ARCH_ARM || AOM_ARCH_X86 || AOM_ARCH_X86_64
static void append_negative_gtest_filter(const char *str) {
  std::string flag_value = GTEST_FLAG_GET(filter);
  // Negative patterns begin with one '-' followed by a ':' separated list.
  if (flag_value.find('-') == std::string::npos) flag_value += '-';
  // OPT.* matches TEST() functions
  // OPT/* matches TEST_P() functions
  // OPT_* matches tests which have been manually sharded.
  // We do not match OPT* because of SSE/SSE2 collisions.
  const char *search_terminators = "./_";
  for (size_t pos = 0; pos < strlen(search_terminators); ++pos) {
    flag_value += ":";
    flag_value += str;
    flag_value += search_terminators[pos];
    flag_value += "*";
  }
  GTEST_FLAG_SET(filter, flag_value);
}
#endif  // AOM_ARCH_ARM || AOM_ARCH_X86 || AOM_ARCH_X86_64
#endif  // !CONFIG_SHARED

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

#if !CONFIG_SHARED
#if AOM_ARCH_AARCH64
  const int caps = aom_arm_cpu_caps();
  if (!(caps & HAS_ARM_CRC32)) append_negative_gtest_filter("ARM_CRC32");
  if (!(caps & HAS_NEON_DOTPROD)) append_negative_gtest_filter("NEON_DOTPROD");
  if (!(caps & HAS_NEON_I8MM)) append_negative_gtest_filter("NEON_I8MM");
  if (!(caps & HAS_SVE)) append_negative_gtest_filter("SVE");
  if (!(caps & HAS_SVE2)) append_negative_gtest_filter("SVE2");
#elif AOM_ARCH_ARM
  const int caps = aom_arm_cpu_caps();
  if (!(caps & HAS_NEON)) append_negative_gtest_filter("NEON");
#endif  // AOM_ARCH_ARM

#if AOM_ARCH_X86 || AOM_ARCH_X86_64
  const int simd_caps = x86_simd_caps();
  if (!(simd_caps & HAS_MMX)) append_negative_gtest_filter("MMX");
  if (!(simd_caps & HAS_SSE)) append_negative_gtest_filter("SSE");
  if (!(simd_caps & HAS_SSE2)) append_negative_gtest_filter("SSE2");
  if (!(simd_caps & HAS_SSE3)) append_negative_gtest_filter("SSE3");
  if (!(simd_caps & HAS_SSSE3)) append_negative_gtest_filter("SSSE3");
  if (!(simd_caps & HAS_SSE4_1)) append_negative_gtest_filter("SSE4_1");
  if (!(simd_caps & HAS_SSE4_2)) append_negative_gtest_filter("SSE4_2");
  if (!(simd_caps & HAS_AVX)) append_negative_gtest_filter("AVX");
  if (!(simd_caps & HAS_AVX2)) append_negative_gtest_filter("AVX2");
#endif  // AOM_ARCH_X86 || AOM_ARCH_X86_64

  // Shared library builds don't support whitebox tests that exercise internal
  // symbols.
  av1_rtcd();
  aom_dsp_rtcd();
  aom_scale_rtcd();
#endif  // !CONFIG_SHARED

  return RUN_ALL_TESTS();
}
