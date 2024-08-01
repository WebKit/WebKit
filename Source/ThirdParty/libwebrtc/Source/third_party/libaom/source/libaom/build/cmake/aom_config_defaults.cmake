#
# Copyright (c) 2016, Alliance for Open Media. All rights reserved.
#
# This source code is subject to the terms of the BSD 2 Clause License and the
# Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License was
# not distributed with this source code in the LICENSE file, you can obtain it
# at www.aomedia.org/license/software. If the Alliance for Open Media Patent
# License 1.0 was not distributed with this source code in the PATENTS file, you
# can obtain it at www.aomedia.org/license/patent.

include("${AOM_ROOT}/build/cmake/util.cmake")

# This file sets default values for libaom configuration variables. All libaom
# config variables are added to the CMake variable cache via the macros provided
# in util.cmake.

#
# The variables in this section of the file are detected at configuration time,
# but can be overridden via the use of CONFIG_* and ENABLE_* values also defined
# in this file.
#

set_aom_detect_var(INLINE "" "Sets INLINE value for current target.")

# CPUs.
set_aom_detect_var(AOM_ARCH_AARCH64 0 "Enables AArch64 architecture.")
set_aom_detect_var(AOM_ARCH_ARM 0 "Enables ARM architecture.")
set_aom_detect_var(AOM_ARCH_PPC 0 "Enables PPC architecture.")
set_aom_detect_var(AOM_ARCH_X86 0 "Enables X86 architecture.")
set_aom_detect_var(AOM_ARCH_X86_64 0 "Enables X86_64 architecture.")

# Arm/AArch64 feature flags.
set_aom_detect_var(HAVE_NEON 0 "Enables Neon intrinsics optimizations.")
set_aom_detect_var(HAVE_ARM_CRC32 0 "Enables Arm CRC32 optimizations.")
set_aom_detect_var(HAVE_NEON_DOTPROD 0
                   "Enables Armv8.2-A Neon dotprod intrinsics optimizations.")
set_aom_detect_var(HAVE_NEON_I8MM 0
                   "Enables Armv8.2-A Neon i8mm intrinsics optimizations.")
set_aom_detect_var(HAVE_SVE 0 "Enables Armv8.2-A SVE intrinsics optimizations.")
set_aom_detect_var(HAVE_SVE2 0 "Enables Armv9-A SVE2 intrinsics optimizations.")

# PPC feature flags.
set_aom_detect_var(HAVE_VSX 0 "Enables VSX optimizations.")

# x86/x86_64 feature flags.
set_aom_detect_var(HAVE_MMX 0 "Enables MMX optimizations. ")
set_aom_detect_var(HAVE_SSE 0 "Enables SSE optimizations.")
set_aom_detect_var(HAVE_SSE2 0 "Enables SSE2 optimizations.")
set_aom_detect_var(HAVE_SSE3 0 "Enables SSE3 optimizations.")
set_aom_detect_var(HAVE_SSSE3 0 "Enables SSSE3 optimizations.")
set_aom_detect_var(HAVE_SSE4_1 0 "Enables SSE 4.1 optimizations.")
set_aom_detect_var(HAVE_SSE4_2 0 "Enables SSE 4.2 optimizations.")
set_aom_detect_var(HAVE_AVX 0 "Enables AVX optimizations.")
set_aom_detect_var(HAVE_AVX2 0 "Enables AVX2 optimizations.")

# Flags describing the build environment.
set_aom_detect_var(HAVE_FEXCEPT 0
                   "Internal flag, GNU fenv.h present for target.")
set_aom_detect_var(HAVE_PTHREAD_H 0 "Internal flag, target pthread support.")
set_aom_detect_var(HAVE_UNISTD_H 0
                   "Internal flag, unistd.h present for target.")
set_aom_detect_var(HAVE_WXWIDGETS 0 "WxWidgets present.")

#
# Variables in this section can be set from the CMake command line or from
# within the CMake GUI. The variables control libaom features.
#

# Build configuration flags.
set_aom_config_var(AOM_RTCD_FLAGS ""
                   "Arguments to pass to rtcd.pl. Separate with ';'")
set_aom_config_var(CONFIG_AV1_DECODER 1 "Enable AV1 decoder.")
set_aom_config_var(CONFIG_AV1_ENCODER 1 "Enable AV1 encoder.")
set_aom_config_var(CONFIG_BIG_ENDIAN 0 "Internal flag.")
set_aom_config_var(CONFIG_FPMT_TEST 0 "Enable FPMT testing.")
set_aom_config_var(CONFIG_GCC 0 "Building with GCC (detect).")
set_aom_config_var(CONFIG_GCOV 0 "Enable gcov support.")
set_aom_config_var(CONFIG_GPROF 0 "Enable gprof support.")
set_aom_config_var(CONFIG_LIBYUV 1 "Enables libyuv scaling/conversion support.")

set_aom_config_var(CONFIG_AV1_HIGHBITDEPTH 1
                   "Build with high bitdepth support.")
set_aom_config_var(CONFIG_AV1_TEMPORAL_DENOISING 0
                   "Build with temporal denoising support.")
set_aom_config_var(CONFIG_MULTITHREAD 1 "Multithread support.")
set_aom_config_var(CONFIG_OS_SUPPORT 0 "Internal flag.")
set_aom_config_var(CONFIG_PIC 0 "Build with PIC enabled.")
set_aom_config_var(CONFIG_QUANT_MATRIX 1
                   "Build with quantization matrices for AV1 encoder."
                   "AV1 decoder is always built with quantization matrices.")
set_aom_config_var(CONFIG_REALTIME_ONLY 0
                   "Build for RTC-only. See aomcx.h for all disabled features.")
set_aom_config_var(CONFIG_RUNTIME_CPU_DETECT 1 "Runtime CPU detection support.")
set_aom_config_var(CONFIG_SHARED 0 "Build shared libs.")
set_aom_config_var(CONFIG_WEBM_IO 1 "Enables WebM support.")

# Debugging flags.
set_aom_config_var(CONFIG_DEBUG 0 "Enable debug-only code.")
set_aom_config_var(CONFIG_EXCLUDE_SIMD_MISMATCH 0
                   "Exclude mismatch in SIMD functions for testing/debugging.")
set_aom_config_var(CONFIG_MISMATCH_DEBUG 0 "Mismatch debugging flag.")

# AV1 feature flags.
set_aom_config_var(CONFIG_ACCOUNTING 0 "Enables bit accounting.")
set_aom_config_var(CONFIG_ANALYZER 0 "Enables bit stream analyzer.")
set_aom_config_var(CONFIG_COEFFICIENT_RANGE_CHECKING 0
                   "Coefficient range check.")
set_aom_config_var(CONFIG_DENOISE 1
                   "Denoise/noise modeling support in encoder.")
set_aom_config_var(CONFIG_INSPECTION 0 "Enables bitstream inspection.")
set_aom_config_var(CONFIG_INTERNAL_STATS 0 "Enables internal encoder stats.")
set_aom_config_var(FORCE_HIGHBITDEPTH_DECODING 0
                   "Force high bitdepth decoding pipeline on 8-bit input.")
mark_as_advanced(FORCE_HIGHBITDEPTH_DECODING)
set_aom_config_var(CONFIG_MAX_DECODE_PROFILE 2
                   "Max profile to support decoding.")
set_aom_config_var(
  CONFIG_NORMAL_TILE_MODE 0
  "Only enables general decoding (disables large scale tile decoding).")
set_aom_config_var(CONFIG_SIZE_LIMIT 0 "Limit max decode width/height.")
set_aom_config_var(CONFIG_SPATIAL_RESAMPLING 1 "Spatial resampling.")
set_aom_config_var(CONFIG_TUNE_BUTTERAUGLI 0
                   "Enable encoding tuning for Butteraugli.")
set_aom_config_var(CONFIG_TUNE_VMAF 0 "Enable encoding tuning for VMAF.")
set_aom_config_var(DECODE_HEIGHT_LIMIT 0 "Set limit for decode height.")
set_aom_config_var(DECODE_WIDTH_LIMIT 0 "Set limit for decode width.")
set_aom_config_var(STATIC_LINK_JXL 0 "Statically link the JPEG-XL library.")

# AV1 experiment flags.
set_aom_config_var(CONFIG_BITRATE_ACCURACY 0
                   "AV1 experiment: Improve bitrate accuracy.")
set_aom_config_var(
  CONFIG_BITRATE_ACCURACY_BL 0
  "AV1 experiment: Baseline of improve bitrate accuracy experiment.")
set_aom_config_var(CONFIG_BITSTREAM_DEBUG 0
                   "AV1 experiment: Bitstream debugging.")
set_aom_config_var(
  CONFIG_COLLECT_COMPONENT_TIMING 0
  "AV1 experiment: Collect encoding component timing information.")
set_aom_config_var(
  CONFIG_COLLECT_PARTITION_STATS 0
  "AV1 experiment: Collect partition timing stats. Can be 1 or 2.")
set_aom_config_var(CONFIG_COLLECT_RD_STATS 0 "AV1 experiment.")
set_aom_config_var(
  CONFIG_DISABLE_FULL_PIXEL_SPLIT_8X8 1
  "AV1 experiment: Disable full_pixel_motion_search_based_split on BLOCK_8X8.")
set_aom_config_var(CONFIG_ENTROPY_STATS 0 "AV1 experiment.")
set_aom_config_var(CONFIG_INTER_STATS_ONLY 0 "AV1 experiment.")
set_aom_config_var(CONFIG_NN_V2 0
                   "AV1 experiment: Fully-connected neural nets ver.2.")
set_aom_config_var(CONFIG_OPTICAL_FLOW_API 0
                   "AV1 experiment: for optical flow API.")
set_aom_config_var(CONFIG_PARTITION_SEARCH_ORDER 0
                   "AV1 experiment: Use alternative partition search order.")
set_aom_config_var(CONFIG_RATECTRL_LOG 0
                   "AV1 experiment: Log rate control decision.")
set_aom_config_var(CONFIG_RD_COMMAND 0
                   "AV1 experiment: Use external rdmult and q_index.")
set_aom_config_var(CONFIG_RD_DEBUG 0 "AV1 experiment.")
set_aom_config_var(
  CONFIG_RT_ML_PARTITIONING 0
  "AV1 experiment: Build with ML-based partitioning for Real Time.")
set_aom_config_var(CONFIG_SPEED_STATS 0 "AV1 experiment.")
set_aom_config_var(CONFIG_TFLITE 0
                   "AV1 experiment: Enable tensorflow lite library.")
set_aom_config_var(CONFIG_THREE_PASS 0
                   "AV1 experiment: Enable three-pass encoding.")
set_aom_config_var(CONFIG_OUTPUT_FRAME_SIZE 0
                   "AV1 experiment: Output frame size information.")
set_aom_config_var(
  CONFIG_SALIENCY_MAP 0
  "AV1 experiment: Enable saliency map based encoding tuning for VMAF.")
set_aom_config_var(CONFIG_CWG_C013 0
                   "AV1 experiment: Support for 7.x and 8.x levels.")
# Add this change to make aomenc reported PSNR consistent with libvmaf result.
set_aom_config_var(CONFIG_LIBVMAF_PSNR_PEAK 1
                   "Use libvmaf PSNR peak for 10- and 12-bit")

#
# Variables in this section control optional features of the build system.
#
set_aom_option_var(ENABLE_CCACHE "Enable ccache support." OFF)
set_aom_option_var(ENABLE_DECODE_PERF_TESTS "Enables decoder performance tests"
                   OFF)
set_aom_option_var(ENABLE_DISTCC "Enable distcc support." OFF)
set_aom_option_var(ENABLE_DOCS
                   "Enable documentation generation (doxygen required)." ON)
set_aom_option_var(ENABLE_ENCODE_PERF_TESTS "Enables encoder performance tests"
                   OFF)
set_aom_option_var(ENABLE_EXAMPLES "Enables build of example code." ON)
set_aom_option_var(ENABLE_GOMA "Enable goma support." OFF)
set_aom_option_var(
  ENABLE_IDE_TEST_HOSTING
  "Enables running tests within IDEs like Visual Studio and Xcode." OFF)
set_aom_option_var(ENABLE_NASM "Use nasm instead of yasm for x86 assembly." OFF)
set_aom_option_var(ENABLE_TESTDATA "Enables unit test data download targets."
                   ON)
set_aom_option_var(ENABLE_TESTS "Enables unit tests." ON)
set_aom_option_var(ENABLE_TOOLS "Enable applications in tools sub directory."
                   ON)
set_aom_option_var(ENABLE_WERROR "Converts warnings to errors at compile time."
                   OFF)

# Arm/AArch64 assembly/intrinsics flags.
set_aom_option_var(ENABLE_NEON
                   "Enables Neon optimizations on Arm/AArch64 targets." ON)
set_aom_option_var(ENABLE_ARM_CRC32 "Enables Arm CRC32 optimizations." ON)
set_aom_option_var(
  ENABLE_NEON_DOTPROD
  "Enables Armv8.2-A Neon dotprod optimizations on AArch64 targets." ON)
set_aom_option_var(
  ENABLE_NEON_I8MM
  "Enables Armv8.2-A Neon i8mm optimizations on AArch64 targets." ON)
set_aom_option_var(ENABLE_SVE
                   "Enables Armv8.2-A SVE optimizations on AArch64 targets." ON)
set_aom_option_var(ENABLE_SVE2
                   "Enables Armv9-A SVE2 optimizations on AArch64 targets." ON)

# VSX intrinsics flags.
set_aom_option_var(ENABLE_VSX "Enables VSX optimizations on PowerPC targets."
                   ON)

# x86/x86_64 assembly/intrinsics flags.
set_aom_option_var(ENABLE_MMX "Enables MMX optimizations on x86/x86_64 targets."
                   ON)
set_aom_option_var(ENABLE_SSE "Enables SSE optimizations on x86/x86_64 targets."
                   ON)
set_aom_option_var(ENABLE_SSE2
                   "Enables SSE2 optimizations on x86/x86_64 targets." ON)
set_aom_option_var(ENABLE_SSE3
                   "Enables SSE3 optimizations on x86/x86_64 targets." ON)
set_aom_option_var(ENABLE_SSSE3
                   "Enables SSSE3 optimizations on x86/x86_64 targets." ON)
set_aom_option_var(ENABLE_SSE4_1
                   "Enables SSE4_1 optimizations on x86/x86_64 targets." ON)
set_aom_option_var(ENABLE_SSE4_2
                   "Enables SSE4_2 optimizations on x86/x86_64 targets." ON)
set_aom_option_var(ENABLE_AVX "Enables AVX optimizations on x86/x86_64 targets."
                   ON)
set_aom_option_var(ENABLE_AVX2
                   "Enables AVX2 optimizations on x86/x86_64 targets." ON)
