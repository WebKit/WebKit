#
# Copyright (c) 2017, Alliance for Open Media. All rights reserved
#
# This source code is subject to the terms of the BSD 2 Clause License and the
# Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License was
# not distributed with this source code in the LICENSE file, you can obtain it
# at www.aomedia.org/license/software. If the Alliance for Open Media Patent
# License 1.0 was not distributed with this source code in the PATENTS file, you
# can obtain it at www.aomedia.org/license/patent.
#
if(AOM_AOM_DSP_AOM_DSP_CMAKE_)
  return()
endif() # AOM_AOM_DSP_AOM_DSP_CMAKE_
set(AOM_AOM_DSP_AOM_DSP_CMAKE_ 1)

list(APPEND AOM_DSP_COMMON_SOURCES
            "${AOM_ROOT}/aom_dsp/aom_convolve.c"
            "${AOM_ROOT}/aom_dsp/aom_dsp_common.h"
            "${AOM_ROOT}/aom_dsp/aom_filter.h"
            "${AOM_ROOT}/aom_dsp/aom_simd.h"
            "${AOM_ROOT}/aom_dsp/aom_simd_inline.h"
            "${AOM_ROOT}/aom_dsp/bitreader_buffer.c"
            "${AOM_ROOT}/aom_dsp/bitreader_buffer.h"
            "${AOM_ROOT}/aom_dsp/bitwriter_buffer.c"
            "${AOM_ROOT}/aom_dsp/bitwriter_buffer.h"
            "${AOM_ROOT}/aom_dsp/blend.h"
            "${AOM_ROOT}/aom_dsp/blend_a64_hmask.c"
            "${AOM_ROOT}/aom_dsp/blend_a64_mask.c"
            "${AOM_ROOT}/aom_dsp/blend_a64_vmask.c"
            "${AOM_ROOT}/aom_dsp/entcode.c"
            "${AOM_ROOT}/aom_dsp/entcode.h"
            "${AOM_ROOT}/aom_dsp/fft.c"
            "${AOM_ROOT}/aom_dsp/fft_common.h"
            "${AOM_ROOT}/aom_dsp/grain_params.h"
            "${AOM_ROOT}/aom_dsp/intrapred.c"
            "${AOM_ROOT}/aom_dsp/intrapred_common.h"
            "${AOM_ROOT}/aom_dsp/loopfilter.c"
            "${AOM_ROOT}/aom_dsp/odintrin.c"
            "${AOM_ROOT}/aom_dsp/odintrin.h"
            "${AOM_ROOT}/aom_dsp/prob.h"
            "${AOM_ROOT}/aom_dsp/recenter.h"
            "${AOM_ROOT}/aom_dsp/simd/v128_intrinsics.h"
            "${AOM_ROOT}/aom_dsp/simd/v128_intrinsics_c.h"
            "${AOM_ROOT}/aom_dsp/simd/v256_intrinsics.h"
            "${AOM_ROOT}/aom_dsp/simd/v256_intrinsics_c.h"
            "${AOM_ROOT}/aom_dsp/simd/v64_intrinsics.h"
            "${AOM_ROOT}/aom_dsp/simd/v64_intrinsics_c.h"
            "${AOM_ROOT}/aom_dsp/subtract.c"
            "${AOM_ROOT}/aom_dsp/txfm_common.h"
            "${AOM_ROOT}/aom_dsp/x86/convolve_common_intrin.h")

list(APPEND AOM_DSP_COMMON_ASM_SSE2
            "${AOM_ROOT}/aom_dsp/x86/aom_high_subpixel_8t_sse2.asm"
            "${AOM_ROOT}/aom_dsp/x86/aom_high_subpixel_bilinear_sse2.asm"
            "${AOM_ROOT}/aom_dsp/x86/aom_subpixel_8t_sse2.asm"
            "${AOM_ROOT}/aom_dsp/x86/aom_subpixel_bilinear_sse2.asm"
            "${AOM_ROOT}/aom_dsp/x86/highbd_intrapred_asm_sse2.asm"
            "${AOM_ROOT}/aom_dsp/x86/intrapred_asm_sse2.asm"
            "${AOM_ROOT}/aom_dsp/x86/inv_wht_sse2.asm")

list(APPEND AOM_DSP_COMMON_INTRIN_SSE2
            "${AOM_ROOT}/aom_dsp/x86/aom_convolve_copy_sse2.c"
            "${AOM_ROOT}/aom_dsp/x86/aom_subpixel_8t_intrin_sse2.c"
            "${AOM_ROOT}/aom_dsp/x86/aom_asm_stubs.c"
            "${AOM_ROOT}/aom_dsp/x86/convolve.h"
            "${AOM_ROOT}/aom_dsp/x86/convolve_sse2.h"
            "${AOM_ROOT}/aom_dsp/x86/fft_sse2.c"
            "${AOM_ROOT}/aom_dsp/x86/highbd_intrapred_sse2.c"
            "${AOM_ROOT}/aom_dsp/x86/intrapred_sse2.c"
            "${AOM_ROOT}/aom_dsp/x86/intrapred_x86.h"
            "${AOM_ROOT}/aom_dsp/x86/loopfilter_sse2.c"
            "${AOM_ROOT}/aom_dsp/x86/lpf_common_sse2.h"
            "${AOM_ROOT}/aom_dsp/x86/mem_sse2.h"
            "${AOM_ROOT}/aom_dsp/x86/transpose_sse2.h"
            "${AOM_ROOT}/aom_dsp/x86/txfm_common_sse2.h"
            "${AOM_ROOT}/aom_dsp/x86/sum_squares_sse2.h"
            "${AOM_ROOT}/aom_dsp/x86/bitdepth_conversion_sse2.h")

list(APPEND AOM_DSP_COMMON_ASM_SSSE3
            "${AOM_ROOT}/aom_dsp/x86/aom_subpixel_8t_ssse3.asm"
            "${AOM_ROOT}/aom_dsp/x86/aom_subpixel_bilinear_ssse3.asm")

list(APPEND AOM_DSP_COMMON_INTRIN_SSSE3
            "${AOM_ROOT}/aom_dsp/x86/aom_subpixel_8t_intrin_ssse3.c"
            "${AOM_ROOT}/aom_dsp/x86/convolve_ssse3.h"
            "${AOM_ROOT}/aom_dsp/x86/intrapred_ssse3.c")

list(APPEND AOM_DSP_COMMON_INTRIN_SSE4_1
            "${AOM_ROOT}/aom_dsp/x86/blend_mask_sse4.h"
            "${AOM_ROOT}/aom_dsp/x86/blend_a64_hmask_sse4.c"
            "${AOM_ROOT}/aom_dsp/x86/blend_a64_mask_sse4.c"
            "${AOM_ROOT}/aom_dsp/x86/blend_a64_vmask_sse4.c"
            "${AOM_ROOT}/aom_dsp/x86/intrapred_sse4.c"
            "${AOM_ROOT}/aom_dsp/x86/intrapred_utils.h")

list(APPEND AOM_DSP_COMMON_INTRIN_AVX2
            "${AOM_ROOT}/aom_dsp/x86/aom_convolve_copy_avx2.c"
            "${AOM_ROOT}/aom_dsp/x86/aom_subpixel_8t_intrin_avx2.c"
            "${AOM_ROOT}/aom_dsp/x86/common_avx2.h"
            "${AOM_ROOT}/aom_dsp/x86/txfm_common_avx2.h"
            "${AOM_ROOT}/aom_dsp/x86/convolve_avx2.h"
            "${AOM_ROOT}/aom_dsp/x86/fft_avx2.c"
            "${AOM_ROOT}/aom_dsp/x86/intrapred_avx2.c"
            "${AOM_ROOT}/aom_dsp/x86/loopfilter_avx2.c"
            "${AOM_ROOT}/aom_dsp/x86/blend_a64_mask_avx2.c"
            "${AOM_ROOT}/aom_dsp/x86/bitdepth_conversion_avx2.h"
            "${AOM_ROOT}/third_party/SVT-AV1/convolve_2d_avx2.h"
            "${AOM_ROOT}/third_party/SVT-AV1/convolve_avx2.h"
            "${AOM_ROOT}/third_party/SVT-AV1/EbMemory_AVX2.h"
            "${AOM_ROOT}/third_party/SVT-AV1/EbMemory_SSE4_1.h"
            "${AOM_ROOT}/third_party/SVT-AV1/synonyms.h")

list(APPEND AOM_DSP_COMMON_INTRIN_NEON
            "${AOM_ROOT}/aom_dsp/arm/aom_convolve_copy_neon.c"
            "${AOM_ROOT}/aom_dsp/arm/aom_convolve8_neon.c"
            "${AOM_ROOT}/aom_dsp/arm/fwd_txfm_neon.c"
            "${AOM_ROOT}/aom_dsp/arm/loopfilter_neon.c"
            "${AOM_ROOT}/aom_dsp/arm/intrapred_neon.c"
            "${AOM_ROOT}/aom_dsp/arm/subtract_neon.c"
            "${AOM_ROOT}/aom_dsp/arm/blend_a64_mask_neon.c"
            "${AOM_ROOT}/aom_dsp/arm/avg_pred_neon.c")

list(APPEND AOM_DSP_COMMON_INTRIN_NEON_DOTPROD
            "${AOM_ROOT}/aom_dsp/arm/aom_convolve8_neon_dotprod.c")

list(APPEND AOM_DSP_COMMON_INTRIN_NEON_I8MM
            "${AOM_ROOT}/aom_dsp/arm/aom_convolve8_neon_i8mm.c")

if(CONFIG_AV1_HIGHBITDEPTH)
  list(APPEND AOM_DSP_COMMON_INTRIN_SSE2
              "${AOM_ROOT}/aom_dsp/x86/highbd_convolve_sse2.c"
              "${AOM_ROOT}/aom_dsp/x86/highbd_loopfilter_sse2.c")

  list(APPEND AOM_DSP_COMMON_INTRIN_SSSE3
              "${AOM_ROOT}/aom_dsp/x86/highbd_convolve_ssse3.c")

  list(APPEND AOM_DSP_COMMON_INTRIN_AVX2
              "${AOM_ROOT}/aom_dsp/x86/highbd_convolve_avx2.c"
              "${AOM_ROOT}/aom_dsp/x86/highbd_loopfilter_avx2.c")

  list(APPEND AOM_DSP_COMMON_INTRIN_NEON
              "${AOM_ROOT}/aom_dsp/arm/highbd_blend_a64_hmask_neon.c"
              "${AOM_ROOT}/aom_dsp/arm/highbd_blend_a64_mask_neon.c"
              "${AOM_ROOT}/aom_dsp/arm/highbd_blend_a64_vmask_neon.c"
              "${AOM_ROOT}/aom_dsp/arm/highbd_convolve8_neon.c"
              "${AOM_ROOT}/aom_dsp/arm/highbd_intrapred_neon.c"
              "${AOM_ROOT}/aom_dsp/arm/highbd_loopfilter_neon.c")
endif()

if(CONFIG_AV1_DECODER)
  list(APPEND AOM_DSP_DECODER_SOURCES
              "${AOM_ROOT}/aom_dsp/binary_codes_reader.c"
              "${AOM_ROOT}/aom_dsp/binary_codes_reader.h"
              "${AOM_ROOT}/aom_dsp/bitreader.c"
              "${AOM_ROOT}/aom_dsp/bitreader.h" "${AOM_ROOT}/aom_dsp/entdec.c"
              "${AOM_ROOT}/aom_dsp/entdec.h")
endif()

if(CONFIG_AV1_ENCODER)
  list(APPEND AOM_DSP_ENCODER_SOURCES
              "${AOM_ROOT}/aom_dsp/avg.c"
              "${AOM_ROOT}/aom_dsp/binary_codes_writer.c"
              "${AOM_ROOT}/aom_dsp/binary_codes_writer.h"
              "${AOM_ROOT}/aom_dsp/bitwriter.c"
              "${AOM_ROOT}/aom_dsp/bitwriter.h"
              "${AOM_ROOT}/aom_dsp/blk_sse_sum.c"
              "${AOM_ROOT}/aom_dsp/entenc.c"
              "${AOM_ROOT}/aom_dsp/entenc.h"
              "${AOM_ROOT}/aom_dsp/fwd_txfm.c"
              "${AOM_ROOT}/aom_dsp/grain_table.c"
              "${AOM_ROOT}/aom_dsp/grain_table.h"
              "${AOM_ROOT}/aom_dsp/noise_model.c"
              "${AOM_ROOT}/aom_dsp/noise_model.h"
              "${AOM_ROOT}/aom_dsp/noise_util.c"
              "${AOM_ROOT}/aom_dsp/noise_util.h"
              "${AOM_ROOT}/aom_dsp/psnr.c"
              "${AOM_ROOT}/aom_dsp/psnr.h"
              "${AOM_ROOT}/aom_dsp/quantize.c"
              "${AOM_ROOT}/aom_dsp/quantize.h"
              "${AOM_ROOT}/aom_dsp/sad.c"
              "${AOM_ROOT}/aom_dsp/sad_av1.c"
              "${AOM_ROOT}/aom_dsp/sse.c"
              "${AOM_ROOT}/aom_dsp/ssim.c"
              "${AOM_ROOT}/aom_dsp/ssim.h"
              "${AOM_ROOT}/aom_dsp/sum_squares.c"
              "${AOM_ROOT}/aom_dsp/variance.c"
              "${AOM_ROOT}/aom_dsp/variance.h")

  # Flow estimation library
  if(NOT CONFIG_REALTIME_ONLY)
    list(APPEND AOM_DSP_ENCODER_SOURCES "${AOM_ROOT}/aom_dsp/pyramid.c"
                "${AOM_ROOT}/aom_dsp/flow_estimation/corner_detect.c"
                "${AOM_ROOT}/aom_dsp/flow_estimation/corner_match.c"
                "${AOM_ROOT}/aom_dsp/flow_estimation/disflow.c"
                "${AOM_ROOT}/aom_dsp/flow_estimation/flow_estimation.c"
                "${AOM_ROOT}/aom_dsp/flow_estimation/ransac.c")

    list(APPEND AOM_DSP_ENCODER_INTRIN_SSE4_1
                "${AOM_ROOT}/aom_dsp/flow_estimation/x86/corner_match_sse4.c"
                "${AOM_ROOT}/aom_dsp/flow_estimation/x86/disflow_sse4.c")

    list(APPEND AOM_DSP_ENCODER_INTRIN_AVX2
                "${AOM_ROOT}/aom_dsp/flow_estimation/x86/corner_match_avx2.c")

    list(APPEND AOM_DSP_ENCODER_INTRIN_NEON
                "${AOM_ROOT}/aom_dsp/flow_estimation/arm/disflow_neon.c")
  endif()

  list(APPEND AOM_DSP_ENCODER_ASM_SSE2 "${AOM_ROOT}/aom_dsp/x86/sad4d_sse2.asm"
              "${AOM_ROOT}/aom_dsp/x86/sad_sse2.asm"
              "${AOM_ROOT}/aom_dsp/x86/subpel_variance_sse2.asm"
              "${AOM_ROOT}/aom_dsp/x86/subtract_sse2.asm")

  list(APPEND AOM_DSP_ENCODER_ASM_SSE2_X86_64
              "${AOM_ROOT}/aom_dsp/x86/ssim_sse2_x86_64.asm")

  list(APPEND AOM_DSP_ENCODER_INTRIN_SSE2
              "${AOM_ROOT}/aom_dsp/x86/avg_intrin_sse2.c"
              "${AOM_ROOT}/aom_dsp/x86/fwd_txfm_impl_sse2.h"
              "${AOM_ROOT}/aom_dsp/x86/fwd_txfm_sse2.c"
              "${AOM_ROOT}/aom_dsp/x86/fwd_txfm_sse2.h"
              "${AOM_ROOT}/aom_dsp/x86/quantize_sse2.c"
              "${AOM_ROOT}/aom_dsp/x86/adaptive_quantize_sse2.c"
              "${AOM_ROOT}/aom_dsp/x86/quantize_x86.h"
              "${AOM_ROOT}/aom_dsp/x86/blk_sse_sum_sse2.c"
              "${AOM_ROOT}/aom_dsp/x86/sum_squares_sse2.c"
              "${AOM_ROOT}/aom_dsp/x86/variance_sse2.c"
              "${AOM_ROOT}/aom_dsp/x86/jnt_sad_sse2.c")

  list(APPEND AOM_DSP_ENCODER_ASM_SSSE3_X86_64
              "${AOM_ROOT}/aom_dsp/x86/fwd_txfm_ssse3_x86_64.asm"
              "${AOM_ROOT}/aom_dsp/x86/quantize_ssse3_x86_64.asm")

  list(APPEND AOM_DSP_ENCODER_INTRIN_AVX2
              "${AOM_ROOT}/aom_dsp/x86/avg_intrin_avx2.c"
              "${AOM_ROOT}/aom_dsp/x86/masked_sad_intrin_avx2.c"
              "${AOM_ROOT}/aom_dsp/x86/subtract_avx2.c"
              "${AOM_ROOT}/aom_dsp/x86/highbd_quantize_intrin_avx2.c"
              "${AOM_ROOT}/aom_dsp/x86/adaptive_quantize_avx2.c"
              "${AOM_ROOT}/aom_dsp/x86/highbd_adaptive_quantize_avx2.c"
              "${AOM_ROOT}/aom_dsp/x86/quantize_avx2.c"
              "${AOM_ROOT}/aom_dsp/x86/sad4d_avx2.c"
              "${AOM_ROOT}/aom_dsp/x86/sad_avx2.c"
              "${AOM_ROOT}/aom_dsp/x86/highbd_sad_avx2.c"
              "${AOM_ROOT}/aom_dsp/x86/sad_impl_avx2.c"
              "${AOM_ROOT}/aom_dsp/x86/variance_avx2.c"
              "${AOM_ROOT}/aom_dsp/x86/sse_avx2.c"
              "${AOM_ROOT}/aom_dsp/x86/variance_impl_avx2.c"
              "${AOM_ROOT}/aom_dsp/x86/obmc_sad_avx2.c"
              "${AOM_ROOT}/aom_dsp/x86/obmc_variance_avx2.c"
              "${AOM_ROOT}/aom_dsp/x86/blk_sse_sum_avx2.c"
              "${AOM_ROOT}/aom_dsp/x86/sum_squares_avx2.c")

  list(APPEND AOM_DSP_ENCODER_INTRIN_AVX
              "${AOM_ROOT}/aom_dsp/x86/aom_quantize_avx.c")

  list(APPEND AOM_DSP_ENCODER_INTRIN_SSSE3
              "${AOM_ROOT}/aom_dsp/x86/masked_sad_intrin_ssse3.h"
              "${AOM_ROOT}/aom_dsp/x86/masked_sad_intrin_ssse3.c"
              "${AOM_ROOT}/aom_dsp/x86/masked_sad4d_ssse3.c"
              "${AOM_ROOT}/aom_dsp/x86/masked_variance_intrin_ssse3.h"
              "${AOM_ROOT}/aom_dsp/x86/masked_variance_intrin_ssse3.c"
              "${AOM_ROOT}/aom_dsp/x86/quantize_ssse3.c"
              "${AOM_ROOT}/aom_dsp/x86/variance_impl_ssse3.c"
              "${AOM_ROOT}/aom_dsp/x86/jnt_variance_ssse3.c")

  list(APPEND AOM_DSP_ENCODER_INTRIN_SSE4_1
              "${AOM_ROOT}/aom_dsp/x86/avg_intrin_sse4.c"
              "${AOM_ROOT}/aom_dsp/x86/sse_sse4.c"
              "${AOM_ROOT}/aom_dsp/x86/obmc_sad_sse4.c"
              "${AOM_ROOT}/aom_dsp/x86/obmc_variance_sse4.c")

  list(APPEND AOM_DSP_ENCODER_INTRIN_NEON
              "${AOM_ROOT}/aom_dsp/arm/sadxd_neon.c"
              "${AOM_ROOT}/aom_dsp/arm/sad_neon.c"
              "${AOM_ROOT}/aom_dsp/arm/masked_sad_neon.c"
              "${AOM_ROOT}/aom_dsp/arm/masked_sad4d_neon.c"
              "${AOM_ROOT}/aom_dsp/arm/subpel_variance_neon.c"
              "${AOM_ROOT}/aom_dsp/arm/variance_neon.c"
              "${AOM_ROOT}/aom_dsp/arm/hadamard_neon.c"
              "${AOM_ROOT}/aom_dsp/arm/avg_neon.c"
              "${AOM_ROOT}/aom_dsp/arm/obmc_variance_neon.c"
              "${AOM_ROOT}/aom_dsp/arm/obmc_sad_neon.c"
              "${AOM_ROOT}/aom_dsp/arm/sse_neon.c"
              "${AOM_ROOT}/aom_dsp/arm/sum_squares_neon.c"
              "${AOM_ROOT}/aom_dsp/arm/blk_sse_sum_neon.c")

  list(APPEND AOM_DSP_ENCODER_INTRIN_NEON_DOTPROD
              "${AOM_ROOT}/aom_dsp/arm/sad_neon_dotprod.c"
              "${AOM_ROOT}/aom_dsp/arm/sadxd_neon_dotprod.c"
              "${AOM_ROOT}/aom_dsp/arm/sse_neon_dotprod.c"
              "${AOM_ROOT}/aom_dsp/arm/sum_squares_neon_dotprod.c"
              "${AOM_ROOT}/aom_dsp/arm/variance_neon_dotprod.c")

  if(CONFIG_AV1_HIGHBITDEPTH)
    list(APPEND AOM_DSP_ENCODER_ASM_SSE2
                "${AOM_ROOT}/aom_dsp/x86/highbd_sad4d_sse2.asm"
                "${AOM_ROOT}/aom_dsp/x86/highbd_sad_sse2.asm"
                "${AOM_ROOT}/aom_dsp/x86/highbd_subpel_variance_impl_sse2.asm"
                "${AOM_ROOT}/aom_dsp/x86/highbd_variance_impl_sse2.asm")

    list(APPEND AOM_DSP_ENCODER_INTRIN_SSE2
                "${AOM_ROOT}/aom_dsp/x86/highbd_adaptive_quantize_sse2.c"
                "${AOM_ROOT}/aom_dsp/x86/highbd_quantize_intrin_sse2.c"
                "${AOM_ROOT}/aom_dsp/x86/highbd_subtract_sse2.c"
                "${AOM_ROOT}/aom_dsp/x86/highbd_variance_sse2.c")

    list(APPEND AOM_DSP_ENCODER_INTRIN_AVX2
                "${AOM_ROOT}/aom_dsp/x86/highbd_variance_avx2.c")

    list(APPEND AOM_DSP_ENCODER_INTRIN_SSE4_1
                "${AOM_ROOT}/aom_dsp/x86/highbd_variance_sse4.c")

    list(APPEND AOM_DSP_ENCODER_INTRIN_NEON
                "${AOM_ROOT}/aom_dsp/arm/highbd_avg_neon.c"
                "${AOM_ROOT}/aom_dsp/arm/highbd_avg_pred_neon.c"
                "${AOM_ROOT}/aom_dsp/arm/highbd_hadamard_neon.c"
                "${AOM_ROOT}/aom_dsp/arm/highbd_masked_sad_neon.c"
                "${AOM_ROOT}/aom_dsp/arm/highbd_obmc_sad_neon.c"
                "${AOM_ROOT}/aom_dsp/arm/highbd_obmc_variance_neon.c"
                "${AOM_ROOT}/aom_dsp/arm/highbd_quantize_neon.c"
                "${AOM_ROOT}/aom_dsp/arm/highbd_sad_neon.c"
                "${AOM_ROOT}/aom_dsp/arm/highbd_sadxd_neon.c"
                "${AOM_ROOT}/aom_dsp/arm/highbd_sse_neon.c"
                "${AOM_ROOT}/aom_dsp/arm/highbd_subpel_variance_neon.c"
                "${AOM_ROOT}/aom_dsp/arm/highbd_variance_neon.c")

    list(APPEND AOM_DSP_ENCODER_INTRIN_NEON_DOTPROD
                "${AOM_ROOT}/aom_dsp/arm/highbd_variance_neon_dotprod.c")
  endif()

  if(CONFIG_INTERNAL_STATS)
    list(APPEND AOM_DSP_ENCODER_SOURCES "${AOM_ROOT}/aom_dsp/fastssim.c"
                "${AOM_ROOT}/aom_dsp/psnrhvs.c")
  endif()

  if(CONFIG_TUNE_VMAF)
    list(APPEND AOM_DSP_ENCODER_SOURCES "${AOM_ROOT}/aom_dsp/vmaf.c"
                "${AOM_ROOT}/aom_dsp/vmaf.h")
  endif()

  if(CONFIG_TUNE_BUTTERAUGLI)
    list(APPEND AOM_DSP_ENCODER_SOURCES "${AOM_ROOT}/aom_dsp/butteraugli.c"
                "${AOM_ROOT}/aom_dsp/butteraugli.h")
  endif()

  if(CONFIG_REALTIME_ONLY)
    list(REMOVE_ITEM AOM_DSP_ENCODER_INTRIN_AVX2
                     "${AOM_ROOT}/aom_dsp/x86/adaptive_quantize_avx2.c"
                     "${AOM_ROOT}/aom_dsp/x86/obmc_sad_avx2.c"
                     "${AOM_ROOT}/aom_dsp/x86/obmc_variance_avx2.c")

    list(REMOVE_ITEM AOM_DSP_ENCODER_INTRIN_SSE4_1
                     "${AOM_ROOT}/aom_dsp/x86/obmc_sad_sse4.c"
                     "${AOM_ROOT}/aom_dsp/x86/obmc_variance_sse4.c")

    list(REMOVE_ITEM AOM_DSP_ENCODER_INTRIN_SSE2
                     "${AOM_ROOT}/aom_dsp/x86/adaptive_quantize_sse2.c")

    list(REMOVE_ITEM AOM_DSP_ENCODER_INTRIN_NEON
                     "${AOM_ROOT}/aom_dsp/arm/highbd_obmc_variance_neon.c"
                     "${AOM_ROOT}/aom_dsp/arm/obmc_variance_neon.c")
  endif()
endif()

# Creates aom_dsp build targets. Must not be called until after libaom target
# has been created.
function(setup_aom_dsp_targets)
  add_library(aom_dsp_common OBJECT ${AOM_DSP_COMMON_SOURCES})
  list(APPEND AOM_LIB_TARGETS aom_dsp_common)
  create_no_op_source_file("aom_av1" "c" "no_op_source_file")
  add_library(aom_dsp OBJECT "${no_op_source_file}")
  target_sources(aom PRIVATE $<TARGET_OBJECTS:aom_dsp_common>)
  if(BUILD_SHARED_LIBS)
    target_sources(aom_static PRIVATE $<TARGET_OBJECTS:aom_dsp_common>)
  endif()
  list(APPEND AOM_LIB_TARGETS aom_dsp)

  # Not all generators support libraries consisting only of object files. Add a
  # source file to the aom_dsp target.
  add_no_op_source_file_to_target("aom_dsp" "c")

  if(CONFIG_AV1_DECODER)
    add_library(aom_dsp_decoder OBJECT ${AOM_DSP_DECODER_SOURCES})
    list(APPEND AOM_LIB_TARGETS aom_dsp_decoder)
    target_sources(aom PRIVATE $<TARGET_OBJECTS:aom_dsp_decoder>)
    if(BUILD_SHARED_LIBS)
      target_sources(aom_static PRIVATE $<TARGET_OBJECTS:aom_dsp_decoder>)
    endif()
  endif()

  if(CONFIG_AV1_ENCODER)
    add_library(aom_dsp_encoder OBJECT ${AOM_DSP_ENCODER_SOURCES})
    list(APPEND AOM_LIB_TARGETS aom_dsp_encoder)
    target_sources(aom PRIVATE $<TARGET_OBJECTS:aom_dsp_encoder>)
    if(BUILD_SHARED_LIBS)
      target_sources(aom_static PRIVATE $<TARGET_OBJECTS:aom_dsp_encoder>)
    endif()
    if(CONFIG_TUNE_VMAF)
      target_include_directories(aom_dsp_encoder PRIVATE ${VMAF_INCLUDE_DIRS})
    endif()
  endif()

  if(HAVE_SSE2)
    add_asm_library("aom_dsp_common_sse2" "AOM_DSP_COMMON_ASM_SSE2")
    add_intrinsics_object_library("-msse2" "sse2" "aom_dsp_common"
                                  "AOM_DSP_COMMON_INTRIN_SSE2")

    if(CONFIG_AV1_ENCODER)
      if("${AOM_TARGET_CPU}" STREQUAL "x86_64")
        list(APPEND AOM_DSP_ENCODER_ASM_SSE2 ${AOM_DSP_ENCODER_ASM_SSE2_X86_64})
      endif()
      add_asm_library("aom_dsp_encoder_sse2" "AOM_DSP_ENCODER_ASM_SSE2")
      add_intrinsics_object_library("-msse2" "sse2" "aom_dsp_encoder"
                                    "AOM_DSP_ENCODER_INTRIN_SSE2")
    endif()
  endif()

  if(HAVE_SSSE3)
    add_asm_library("aom_dsp_common_ssse3" "AOM_DSP_COMMON_ASM_SSSE3")
    add_intrinsics_object_library("-mssse3" "ssse3" "aom_dsp_common"
                                  "AOM_DSP_COMMON_INTRIN_SSSE3")

    if(CONFIG_AV1_ENCODER)
      if("${AOM_TARGET_CPU}" STREQUAL "x86_64")
        list(APPEND AOM_DSP_ENCODER_ASM_SSSE3
                    ${AOM_DSP_ENCODER_ASM_SSSE3_X86_64})
      endif()
      add_asm_library("aom_dsp_encoder_ssse3" "AOM_DSP_ENCODER_ASM_SSSE3")
      add_intrinsics_object_library("-mssse3" "ssse3" "aom_dsp_encoder"
                                    "AOM_DSP_ENCODER_INTRIN_SSSE3")
    endif()
  endif()

  if(HAVE_SSE4_1)
    add_intrinsics_object_library("-msse4.1" "sse4_1" "aom_dsp_common"
                                  "AOM_DSP_COMMON_INTRIN_SSE4_1")
    if(CONFIG_AV1_ENCODER)
      add_intrinsics_object_library("-msse4.1" "sse4_1" "aom_dsp_encoder"
                                    "AOM_DSP_ENCODER_INTRIN_SSE4_1")
    endif()
  endif()

  if(HAVE_AVX)
    if(CONFIG_AV1_ENCODER)
      add_intrinsics_object_library("-mavx" "avx" "aom_dsp_encoder"
                                    "AOM_DSP_ENCODER_INTRIN_AVX")
    endif()
  endif()

  if(HAVE_AVX2)
    add_intrinsics_object_library("-mavx2" "avx2" "aom_dsp_common"
                                  "AOM_DSP_COMMON_INTRIN_AVX2")
    if(CONFIG_AV1_ENCODER)
      add_intrinsics_object_library("-mavx2" "avx2" "aom_dsp_encoder"
                                    "AOM_DSP_ENCODER_INTRIN_AVX2")
    endif()
  endif()

  if(HAVE_NEON)
    add_intrinsics_object_library("${AOM_NEON_INTRIN_FLAG}" "neon"
                                  "aom_dsp_common" "AOM_DSP_COMMON_INTRIN_NEON")
    if(CONFIG_AV1_ENCODER)
      add_intrinsics_object_library("${AOM_NEON_INTRIN_FLAG}" "neon"
                                    "aom_dsp_encoder"
                                    "AOM_DSP_ENCODER_INTRIN_NEON")
    endif()
  endif()

  if(HAVE_NEON_DOTPROD)
    add_intrinsics_object_library("${AOM_NEON_DOTPROD_FLAG}" "neon_dotprod"
                                  "aom_dsp_common"
                                  "AOM_DSP_COMMON_INTRIN_NEON_DOTPROD")
    if(CONFIG_AV1_ENCODER)
      add_intrinsics_object_library("${AOM_NEON_DOTPROD_FLAG}" "neon_dotprod"
                                    "aom_dsp_encoder"
                                    "AOM_DSP_ENCODER_INTRIN_NEON_DOTPROD")
    endif()
  endif()

  if(HAVE_NEON_I8MM)
    add_intrinsics_object_library("${AOM_NEON_I8MM_FLAG}" "neon_i8mm"
                                  "aom_dsp_common"
                                  "AOM_DSP_COMMON_INTRIN_NEON_I8MM")
  endif()

  target_sources(aom PRIVATE $<TARGET_OBJECTS:aom_dsp>)
  if(BUILD_SHARED_LIBS)
    target_sources(aom_static PRIVATE $<TARGET_OBJECTS:aom_dsp>)
  endif()

  # Pass the new lib targets up to the parent scope instance of
  # $AOM_LIB_TARGETS.
  set(AOM_LIB_TARGETS ${AOM_LIB_TARGETS} PARENT_SCOPE)
endfunction()
