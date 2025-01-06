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

#ifdef __linux__
#include <ctype.h>
#include <sys/utsname.h>
#endif

#include "libyuv/cpu_id.h"

#ifdef __cplusplus
using namespace libyuv;
#endif

#ifdef __linux__
static void KernelVersion(int *version) {
  struct utsname buffer;
  int i = 0;

  version[0] = version[1] = 0;
  if (uname(&buffer) == 0) {
    char *v = buffer.release;
    for (i = 0; *v && i < 2; ++v) {
      if (isdigit(*v)) {
        version[i++] = (int) strtol(v, &v, 10);
      }
    }
  }
}
#endif

int main(int argc, const char* argv[]) {
  (void)argc;
  (void)argv;

#if defined(__linux__)
  {
    int kernelversion[2];
    KernelVersion(kernelversion);
    printf("Kernel Version %d.%d\n", kernelversion[0], kernelversion[1]);
  }
#endif  // defined(__linux__)

#if defined(__arm__) || defined(__aarch64__)
  int has_arm = TestCpuFlag(kCpuHasARM);
  if (has_arm) {
    int has_neon = TestCpuFlag(kCpuHasNEON);
    int has_neon_dotprod = TestCpuFlag(kCpuHasNeonDotProd);
    int has_neon_i8mm = TestCpuFlag(kCpuHasNeonI8MM);
    int has_sve = TestCpuFlag(kCpuHasSVE);
    int has_sve2 = TestCpuFlag(kCpuHasSVE2);
    int has_sme = TestCpuFlag(kCpuHasSME);
    printf("Has Arm 0x%x\n", has_arm);
    printf("Has Neon 0x%x\n", has_neon);
    printf("Has Neon DotProd 0x%x\n", has_neon_dotprod);
    printf("Has Neon I8MM 0x%x\n", has_neon_i8mm);
    printf("Has SVE 0x%x\n", has_sve);
    printf("Has SVE2 0x%x\n", has_sve2);
    printf("Has SME 0x%x\n", has_sme);

#if __aarch64__
    // Read and print the SVE and SME vector lengths.
    if (has_sve) {
      int sve_vl;
      __asm__(".inst 0x04bf5020    \n"  // rdvl x0, #1
          "mov %w[sve_vl], w0  \n"
          : [sve_vl] "=r"(sve_vl)  // %[sve_vl]
          :
          : "x0");
      printf("SVE vector length: %d bytes\n", sve_vl);
    }
    if (has_sme) {
      int sme_vl;
      __asm__(".inst 0x04bf5820    \n"  // rdsvl x0, #1
          "mov %w[sme_vl], w0  \n"
          : [sme_vl] "=r"(sme_vl)  // %[sme_vl]
          :
          : "x0");
      printf("SME vector length: %d bytes\n", sme_vl);
    }
#endif  // defined(__aarch64__)
  }
#endif  // if defined(__arm__) || defined(__aarch64__)

#if defined(__riscv)
  int has_riscv = TestCpuFlag(kCpuHasRISCV);
  if (has_riscv) {
    int has_rvv = TestCpuFlag(kCpuHasRVV);
    printf("Has RISCV 0x%x\n", has_riscv);
    printf("Has RVV 0x%x\n", has_rvv);

    // Read and print the RVV vector length.
    if (has_rvv) {
      register uint32_t vlenb __asm__ ("t0");
      __asm__(".word 0xC22022F3"  /* CSRR t0, vlenb */ : "=r" (vlenb));
      printf("RVV vector length: %d bytes\n", vlenb);
    }
  }
#endif  // defined(__riscv)

#if defined(__mips__)
  int has_mips = TestCpuFlag(kCpuHasMIPS);
  if (has_mips) {
    int has_msa = TestCpuFlag(kCpuHasMSA);
    printf("Has MIPS 0x%x\n", has_mips);
    printf("Has MSA 0x%x\n", has_msa);
  }
#endif  // defined(__mips__)

#if defined(__loongarch__)
  int has_loongarch = TestCpuFlag(kCpuHasLOONGARCH);
  if (has_loongarch) {
    int has_lsx  = TestCpuFlag(kCpuHasLSX);
    int has_lasx = TestCpuFlag(kCpuHasLASX);
    printf("Has LOONGARCH 0x%x\n", has_loongarch);
    printf("Has LSX 0x%x\n", has_lsx);
    printf("Has LASX 0x%x\n", has_lasx);
  }
#endif  // defined(__loongarch__)

#if defined(__i386__) || defined(__x86_64__) || \
    defined(_M_IX86) || defined(_M_X64)
  int has_x86 = TestCpuFlag(kCpuHasX86);
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

    int has_sse2 = TestCpuFlag(kCpuHasSSE2);
    int has_ssse3 = TestCpuFlag(kCpuHasSSSE3);
    int has_sse41 = TestCpuFlag(kCpuHasSSE41);
    int has_sse42 = TestCpuFlag(kCpuHasSSE42);
    int has_avx = TestCpuFlag(kCpuHasAVX);
    int has_avx2 = TestCpuFlag(kCpuHasAVX2);
    int has_erms = TestCpuFlag(kCpuHasERMS);
    int has_fma3 = TestCpuFlag(kCpuHasFMA3);
    int has_f16c = TestCpuFlag(kCpuHasF16C);
    int has_avx512bw = TestCpuFlag(kCpuHasAVX512BW);
    int has_avx512vl = TestCpuFlag(kCpuHasAVX512VL);
    int has_avx512vnni = TestCpuFlag(kCpuHasAVX512VNNI);
    int has_avx512vbmi = TestCpuFlag(kCpuHasAVX512VBMI);
    int has_avx512vbmi2 = TestCpuFlag(kCpuHasAVX512VBMI2);
    int has_avx512vbitalg = TestCpuFlag(kCpuHasAVX512VBITALG);
    int has_avx10 = TestCpuFlag(kCpuHasAVX10);
    int has_avxvnni = TestCpuFlag(kCpuHasAVXVNNI);
    int has_avxvnniint8 = TestCpuFlag(kCpuHasAVXVNNIINT8);
    int has_amxint8 = TestCpuFlag(kCpuHasAMXINT8);
    printf("Has X86 0x%x\n", has_x86);
    printf("Has SSE2 0x%x\n", has_sse2);
    printf("Has SSSE3 0x%x\n", has_ssse3);
    printf("Has SSE4.1 0x%x\n", has_sse41);
    printf("Has SSE4.2 0x%x\n", has_sse42);
    printf("Has AVX 0x%x\n", has_avx);
    printf("Has AVX2 0x%x\n", has_avx2);
    printf("Has ERMS 0x%x\n", has_erms);
    printf("Has FMA3 0x%x\n", has_fma3);
    printf("Has F16C 0x%x\n", has_f16c);
    printf("Has AVX512BW 0x%x\n", has_avx512bw);
    printf("Has AVX512VL 0x%x\n", has_avx512vl);
    printf("Has AVX512VNNI 0x%x\n", has_avx512vnni);
    printf("Has AVX512VBMI 0x%x\n", has_avx512vbmi);
    printf("Has AVX512VBMI2 0x%x\n", has_avx512vbmi2);
    printf("Has AVX512VBITALG 0x%x\n", has_avx512vbitalg);
    printf("Has AVX10 0x%x\n", has_avx10);
    printf("HAS AVXVNNI 0x%x\n", has_avxvnni);
    printf("Has AVXVNNIINT8 0x%x\n", has_avxvnniint8);
    printf("Has AMXINT8 0x%x\n", has_amxint8);
  }
#endif  // defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
  return 0;
}

