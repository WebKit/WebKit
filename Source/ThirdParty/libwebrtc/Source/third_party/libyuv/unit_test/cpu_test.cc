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
  printf("Cpu Flags 0x%x\n", cpu_flags);
#if defined(__arm__) || defined(__aarch64__)
  int has_arm = TestCpuFlag(kCpuHasARM);
  printf("Has ARM 0x%x\n", has_arm);
  int has_neon = TestCpuFlag(kCpuHasNEON);
  printf("Has NEON 0x%x\n", has_neon);
#endif
#if defined(__riscv) && defined(__linux__)
  int has_riscv = TestCpuFlag(kCpuHasRISCV);
  printf("Has RISCV 0x%x\n", has_riscv);
  int has_rvv = TestCpuFlag(kCpuHasRVV);
  printf("Has RVV 0x%x\n", has_rvv);
  int has_rvvzvfh = TestCpuFlag(kCpuHasRVVZVFH);
  printf("Has RVVZVFH 0x%x\n", has_rvvzvfh);
#endif
#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || \
    defined(_M_X64)
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
  printf("Has SSE41 0x%x\n", has_sse41);
  printf("Has SSE42 0x%x\n", has_sse42);
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
#endif
#if defined(__mips__)
  int has_mips = TestCpuFlag(kCpuHasMIPS);
  printf("Has MIPS 0x%x\n", has_mips);
  int has_msa = TestCpuFlag(kCpuHasMSA);
  printf("Has MSA 0x%x\n", has_msa);
#endif
#if defined(__loongarch__)
  int has_loongarch = TestCpuFlag(kCpuHasLOONGARCH);
  printf("Has LOONGARCH 0x%x\n", has_loongarch);
  int has_lsx = TestCpuFlag(kCpuHasLSX);
  printf("Has LSX 0x%x\n", has_lsx);
  int has_lasx = TestCpuFlag(kCpuHasLASX);
  printf("Has LASX 0x%x\n", has_lasx);
#endif
}

TEST_F(LibYUVBaseTest, TestCompilerMacros) {
  // Tests all macros used in public headers.
#ifdef __ATOMIC_RELAXED
  printf("__ATOMIC_RELAXED %d\n", __ATOMIC_RELAXED);
#endif
#ifdef __cplusplus
  printf("__cplusplus %ld\n", __cplusplus);
#endif
#ifdef __clang_major__
  printf("__clang_major__ %d\n", __clang_major__);
#endif
#ifdef __clang_minor__
  printf("__clang_minor__ %d\n", __clang_minor__);
#endif
#ifdef __GNUC__
  printf("__GNUC__ %d\n", __GNUC__);
#endif
#ifdef __GNUC_MINOR__
  printf("__GNUC_MINOR__ %d\n", __GNUC_MINOR__);
#endif
#ifdef __i386__
  printf("__i386__ %d\n", __i386__);
#endif
#ifdef __x86_64__
  printf("__x86_64__ %d\n", __x86_64__);
#endif
#ifdef _M_IX86
  printf("_M_IX86 %d\n", _M_IX86);
#endif
#ifdef _M_X64
  printf("_M_X64 %d\n", _M_X64);
#endif
#ifdef _MSC_VER
  printf("_MSC_VER %d\n", _MSC_VER);
#endif
#ifdef __aarch64__
  printf("__aarch64__ %d\n", __aarch64__);
#endif
#ifdef __arm__
  printf("__arm__ %d\n", __arm__);
#endif
#ifdef __riscv
  printf("__riscv %d\n", __riscv);
#endif
#ifdef __riscv_vector
  printf("__riscv_vector %d\n", __riscv_vector);
#endif
#ifdef __riscv_v_intrinsic
  printf("__riscv_v_intrinsic %d\n", __riscv_v_intrinsic);
#endif
#ifdef __APPLE__
  printf("__APPLE__ %d\n", __APPLE__);
#endif
#ifdef __clang__
  printf("__clang__ %d\n", __clang__);
#endif
#ifdef __CLR_VER
  printf("__CLR_VER %d\n", __CLR_VER);
#endif
#ifdef __CYGWIN__
  printf("__CYGWIN__ %d\n", __CYGWIN__);
#endif
#ifdef __llvm__
  printf("__llvm__ %d\n", __llvm__);
#endif
#ifdef __mips_msa
  printf("__mips_msa %d\n", __mips_msa);
#endif
#ifdef __mips
  printf("__mips %d\n", __mips);
#endif
#ifdef __mips_isa_rev
  printf("__mips_isa_rev %d\n", __mips_isa_rev);
#endif
#ifdef _MIPS_ARCH_LOONGSON3A
  printf("_MIPS_ARCH_LOONGSON3A %d\n", _MIPS_ARCH_LOONGSON3A);
#endif
#ifdef __loongarch__
  printf("__loongarch__ %d\n", __loongarch__);
#endif
#ifdef _WIN32
  printf("_WIN32 %d\n", _WIN32);
#endif
#ifdef __native_client__
  printf("__native_client__ %d\n", __native_client__);
#endif
#ifdef __pic__
  printf("__pic__ %d\n", __pic__);
#endif
#ifdef __pnacl__
  printf("__pnacl__ %d\n", __pnacl__);
#endif
#ifdef GG_LONGLONG
  printf("GG_LONGLONG %lld\n", GG_LONGLONG(1));
#endif
#ifdef INT_TYPES_DEFINED
  printf("INT_TYPES_DEFINED\n");
#endif
#ifdef __has_feature
  printf("__has_feature\n");
#if __has_feature(memory_sanitizer)
  printf("__has_feature(memory_sanitizer) %d\n",
         __has_feature(memory_sanitizer));
#endif
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
    printf("Cpu Vendor: %s 0x%x 0x%x 0x%x\n",
           reinterpret_cast<char*>(&cpu_info[0]), cpu_info[0], cpu_info[1],
           cpu_info[2]);
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
  if (FileExists("/proc/cpuinfo")) {
    if (kCpuHasNEON != ArmCpuCaps("/proc/cpuinfo")) {
      // This can happen on ARM emulator but /proc/cpuinfo is from host.
      printf("WARNING: Neon build enabled but CPU does not have NEON\n");
    }
  } else {
    printf("WARNING: unable to load \"/proc/cpuinfo\"\n");
  }
#endif
}

TEST_F(LibYUVBaseTest, TestLinuxMipsMsa) {
  if (FileExists("../../unit_test/testdata/mips.txt")) {
    printf("Note: testing to load \"../../unit_test/testdata/mips.txt\"\n");

    EXPECT_EQ(0, MipsCpuCaps("../../unit_test/testdata/mips.txt"));
    EXPECT_EQ(kCpuHasMSA, MipsCpuCaps("../../unit_test/testdata/mips_msa.txt"));
    EXPECT_EQ(kCpuHasMSA,
              MipsCpuCaps("../../unit_test/testdata/mips_loongson2k.txt"));
  } else {
    printf("WARNING: unable to load \"../../unit_test/testdata/mips.txt\"\n");
  }
}

TEST_F(LibYUVBaseTest, TestLinuxRVV) {
  if (FileExists("../../unit_test/testdata/riscv64.txt")) {
    printf("Note: testing to load \"../../unit_test/testdata/riscv64.txt\"\n");

    EXPECT_EQ(0, RiscvCpuCaps("../../unit_test/testdata/riscv64.txt"));
    EXPECT_EQ(kCpuHasRVV,
              RiscvCpuCaps("../../unit_test/testdata/riscv64_rvv.txt"));
    EXPECT_EQ(kCpuHasRVV | kCpuHasRVVZVFH,
              RiscvCpuCaps("../../unit_test/testdata/riscv64_rvv_zvfh.txt"));
  } else {
    printf(
        "WARNING: unable to load "
        "\"../../unit_test/testdata/riscv64.txt\"\n");
  }
#if defined(__linux__) && defined(__riscv)
  if (FileExists("/proc/cpuinfo")) {
    if (!(kCpuHasRVV & RiscvCpuCaps("/proc/cpuinfo"))) {
      // This can happen on RVV emulator but /proc/cpuinfo is from host.
      printf("WARNING: RVV build enabled but CPU does not have RVV\n");
    }
  } else {
    printf("WARNING: unable to load \"/proc/cpuinfo\"\n");
  }
#endif
}

// TODO(fbarchard): Fix clangcl test of cpuflags.
#ifdef _MSC_VER
TEST_F(LibYUVBaseTest, DISABLED_TestSetCpuFlags) {
#else
TEST_F(LibYUVBaseTest, TestSetCpuFlags) {
#endif
  // Reset any masked flags that may have been set so auto init is enabled.
  MaskCpuFlags(0);

  int original_cpu_flags = TestCpuFlag(-1);

  // Test setting different CPU configurations.
  int cpu_flags = kCpuHasARM | kCpuHasNEON | kCpuInitialized;
  SetCpuFlags(cpu_flags);
  EXPECT_EQ(cpu_flags, TestCpuFlag(-1));

  cpu_flags = kCpuHasX86 | kCpuInitialized;
  SetCpuFlags(cpu_flags);
  EXPECT_EQ(cpu_flags, TestCpuFlag(-1));

  // Test that setting 0 turns auto-init back on.
  SetCpuFlags(0);
  EXPECT_EQ(original_cpu_flags, TestCpuFlag(-1));

  // Restore the CPU flag mask.
  MaskCpuFlags(benchmark_cpu_info_);
}

}  // namespace libyuv
