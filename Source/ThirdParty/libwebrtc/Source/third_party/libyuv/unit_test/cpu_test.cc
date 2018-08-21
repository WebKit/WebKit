/*
 *  Copyright 2012 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>
#include <string.h>

#include "../unit_test/unit_test.h"
#include "libyuv/basic_types.h"
#include "libyuv/cpu_id.h"
#include "libyuv/version.h"

namespace libyuv {

TEST_F(LibYUVBaseTest, TestCpuHas) {
  int cpu_flags = TestCpuFlag(-1);
  printf("Cpu Flags %x\n", cpu_flags);
#if defined(__arm__) || defined(__aarch64__)
  int has_arm = TestCpuFlag(kCpuHasARM);
  printf("Has ARM %x\n", has_arm);
  int has_neon = TestCpuFlag(kCpuHasNEON);
  printf("Has NEON %x\n", has_neon);
#endif
  int has_x86 = TestCpuFlag(kCpuHasX86);
  int has_sse2 = TestCpuFlag(kCpuHasSSE2);
  int has_ssse3 = TestCpuFlag(kCpuHasSSSE3);
  int has_sse41 = TestCpuFlag(kCpuHasSSE41);
  int has_sse42 = TestCpuFlag(kCpuHasSSE42);
  int has_avx = TestCpuFlag(kCpuHasAVX);
  int has_avx2 = TestCpuFlag(kCpuHasAVX2);
  int has_erms = TestCpuFlag(kCpuHasERMS);
  int has_fma3 = TestCpuFlag(kCpuHasFMA3);
  int has_f16c = TestCpuFlag(kCpuHasF16C);
  int has_gfni = TestCpuFlag(kCpuHasGFNI);
  int has_avx512bw = TestCpuFlag(kCpuHasAVX512BW);
  int has_avx512vl = TestCpuFlag(kCpuHasAVX512VL);
  int has_avx512vbmi = TestCpuFlag(kCpuHasAVX512VBMI);
  int has_avx512vbmi2 = TestCpuFlag(kCpuHasAVX512VBMI2);
  int has_avx512vbitalg = TestCpuFlag(kCpuHasAVX512VBITALG);
  int has_avx512vpopcntdq = TestCpuFlag(kCpuHasAVX512VPOPCNTDQ);
  printf("Has X86 %x\n", has_x86);
  printf("Has SSE2 %x\n", has_sse2);
  printf("Has SSSE3 %x\n", has_ssse3);
  printf("Has SSE4.1 %x\n", has_sse41);
  printf("Has SSE4.2 %x\n", has_sse42);
  printf("Has AVX %x\n", has_avx);
  printf("Has AVX2 %x\n", has_avx2);
  printf("Has ERMS %x\n", has_erms);
  printf("Has FMA3 %x\n", has_fma3);
  printf("Has F16C %x\n", has_f16c);
  printf("Has GFNI %x\n", has_gfni);
  printf("Has AVX512BW %x\n", has_avx512bw);
  printf("Has AVX512VL %x\n", has_avx512vl);
  printf("Has AVX512VBMI %x\n", has_avx512vbmi);
  printf("Has AVX512VBMI2 %x\n", has_avx512vbmi2);
  printf("Has AVX512VBITALG %x\n", has_avx512vbitalg);
  printf("Has AVX512VPOPCNTDQ %x\n", has_avx512vpopcntdq);

#if defined(__mips__)
  int has_mips = TestCpuFlag(kCpuHasMIPS);
  printf("Has MIPS %x\n", has_mips);
  int has_msa = TestCpuFlag(kCpuHasMSA);
  printf("Has MSA %x\n", has_msa);
#endif
}

TEST_F(LibYUVBaseTest, TestCpuCompilerEnabled) {
#if defined(__aarch64__)
  printf("Arm64 build\n");
#endif
#if defined(__aarch64__) || defined(__ARM_NEON__) || defined(LIBYUV_NEON)
  printf("Neon build enabled\n");
#endif
#if defined(__x86_64__) || defined(_M_X64)
  printf("x64 build\n");
#endif
#ifdef _MSC_VER
  printf("_MSC_VER %d\n", _MSC_VER);
#endif
#if !defined(LIBYUV_DISABLE_X86) &&                      \
    (defined(GCC_HAS_AVX2) || defined(CLANG_HAS_AVX2) || \
     defined(VISUALC_HAS_AVX2))
  printf("Has AVX2 1\n");
#else
  printf("Has AVX2 0\n");
// If compiler does not support AVX2, the following function not expected:
#endif
}

#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || \
    defined(_M_X64)
TEST_F(LibYUVBaseTest, TestCpuId) {
  int has_x86 = TestCpuFlag(kCpuHasX86);
  if (has_x86) {
    int cpu_info[4];
    // Vendor ID:
    // AuthenticAMD AMD processor
    // CentaurHauls Centaur processor
    // CyrixInstead Cyrix processor
    // GenuineIntel Intel processor
    // GenuineTMx86 Transmeta processor
    // Geode by NSC National Semiconductor processor
    // NexGenDriven NexGen processor
    // RiseRiseRise Rise Technology processor
    // SiS SiS SiS  SiS processor
    // UMC UMC UMC  UMC processor
    CpuId(0, 0, cpu_info);
    cpu_info[0] = cpu_info[1];  // Reorder output
    cpu_info[1] = cpu_info[3];
    cpu_info[3] = 0;
    printf("Cpu Vendor: %s %x %x %x\n", reinterpret_cast<char*>(&cpu_info[0]),
           cpu_info[0], cpu_info[1], cpu_info[2]);
    EXPECT_EQ(12u, strlen(reinterpret_cast<char*>(&cpu_info[0])));

    // CPU Family and Model
    // 3:0 - Stepping
    // 7:4 - Model
    // 11:8 - Family
    // 13:12 - Processor Type
    // 19:16 - Extended Model
    // 27:20 - Extended Family
    CpuId(1, 0, cpu_info);
    int family = ((cpu_info[0] >> 8) & 0x0f) | ((cpu_info[0] >> 16) & 0xff0);
    int model = ((cpu_info[0] >> 4) & 0x0f) | ((cpu_info[0] >> 12) & 0xf0);
    printf("Cpu Family %d (0x%x), Model %d (0x%x)\n", family, family, model,
           model);
  }
}
#endif

static int FileExists(const char* file_name) {
  FILE* f = fopen(file_name, "r");
  if (!f) {
    return 0;
  }
  fclose(f);
  return 1;
}

TEST_F(LibYUVBaseTest, TestLinuxNeon) {
  if (FileExists("../../unit_test/testdata/arm_v7.txt")) {
    printf("Note: testing to load \"../../unit_test/testdata/arm_v7.txt\"\n");

    EXPECT_EQ(0, ArmCpuCaps("../../unit_test/testdata/arm_v7.txt"));
    EXPECT_EQ(kCpuHasNEON, ArmCpuCaps("../../unit_test/testdata/tegra3.txt"));
    EXPECT_EQ(kCpuHasNEON, ArmCpuCaps("../../unit_test/testdata/juno.txt"));
  } else {
    printf("WARNING: unable to load \"../../unit_test/testdata/arm_v7.txt\"\n");
  }
#if defined(__linux__) && defined(__ARM_NEON__)
  EXPECT_EQ(kCpuHasNEON, ArmCpuCaps("/proc/cpuinfo"));
#endif
}

}  // namespace libyuv
