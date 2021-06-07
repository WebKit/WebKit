/*
 *  Copyright 2012 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libyuv/cpu_id.h"

#ifdef __cplusplus
using namespace libyuv;
#endif

int main(int argc, const char* argv[]) {
  int cpu_flags = TestCpuFlag(-1);
  int has_arm = TestCpuFlag(kCpuHasARM);
  int has_mips = TestCpuFlag(kCpuHasMIPS);
  int has_x86 = TestCpuFlag(kCpuHasX86);
  (void)argc;
  (void)argv;

#if defined(__i386__) || defined(__x86_64__) || \
    defined(_M_IX86) || defined(_M_X64)
  if (has_x86) {
    int family, model, cpu_info[4];
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
    CpuId(0, 0, &cpu_info[0]);
    cpu_info[0] = cpu_info[1];  // Reorder output
    cpu_info[1] = cpu_info[3];
    cpu_info[3] = 0;
    printf("Cpu Vendor: %s\n", (char*)(&cpu_info[0]));

    // CPU Family and Model
    // 3:0 - Stepping
    // 7:4 - Model
    // 11:8 - Family
    // 13:12 - Processor Type
    // 19:16 - Extended Model
    // 27:20 - Extended Family
    CpuId(1, 0, &cpu_info[0]);
    family = ((cpu_info[0] >> 8) & 0x0f) | ((cpu_info[0] >> 16) & 0xff0);
    model = ((cpu_info[0] >> 4) & 0x0f) | ((cpu_info[0] >> 12) & 0xf0);
    printf("Cpu Family %d (0x%x), Model %d (0x%x)\n", family, family,
           model, model);
  }
#endif
  printf("Cpu Flags %x\n", cpu_flags);
  printf("Has ARM %x\n", has_arm);
  printf("Has MIPS %x\n", has_mips);
  printf("Has X86 %x\n", has_x86);
  if (has_arm) {
    int has_neon = TestCpuFlag(kCpuHasNEON);
    printf("Has NEON %x\n", has_neon);
  }
  if (has_mips) {
    int has_msa = TestCpuFlag(kCpuHasMSA);
    printf("Has MSA %x\n", has_msa);
    int has_mmi = TestCpuFlag(kCpuHasMMI);
    printf("Has MMI %x\n", has_mmi);
  }
  if (has_x86) {
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
  }
  return 0;
}

