##
## Copyright (c) 2017, Alliance for Open Media. All rights reserved.
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##
sub av1_common_forward_decls() {
print <<EOF
/*
 * AV1
 */

#include "aom/aom_integer.h"
#include "aom_dsp/odintrin.h"
#include "aom_dsp/txfm_common.h"
#include "av1/common/av1_txfm.h"
#include "av1/common/common.h"
#include "av1/common/convolve.h"
#include "av1/common/enums.h"
#include "av1/common/filter.h"
#include "av1/common/quant_common.h"
#include "av1/common/restoration.h"

struct macroblockd;

/* Encoder forward decls */
struct macroblock;
struct txfm_param;
struct aom_variance_vtable;
struct search_site_config;
struct yv12_buffer_config;
struct NN_CONFIG;
typedef struct NN_CONFIG NN_CONFIG;

enum { NONE, RELU, SOFTSIGN, SIGMOID } UENUM1BYTE(ACTIVATION);
#if CONFIG_NN_V2
enum { SOFTMAX_CROSS_ENTROPY } UENUM1BYTE(LOSS);
struct NN_CONFIG_V2;
typedef struct NN_CONFIG_V2 NN_CONFIG_V2;
struct FC_LAYER;
typedef struct FC_LAYER FC_LAYER;
#endif  // CONFIG_NN_V2

struct CNN_CONFIG;
typedef struct CNN_CONFIG CNN_CONFIG;
struct CNN_LAYER_CONFIG;
typedef struct CNN_LAYER_CONFIG CNN_LAYER_CONFIG;
struct CNN_THREAD_DATA;
typedef struct CNN_THREAD_DATA CNN_THREAD_DATA;
struct CNN_BRANCH_CONFIG;
typedef struct CNN_BRANCH_CONFIG CNN_BRANCH_CONFIG;
struct CNN_MULTI_OUT;
typedef struct CNN_MULTI_OUT CNN_MULTI_OUT;

/* Function pointers return by CfL functions */
typedef void (*cfl_subsample_lbd_fn)(const uint8_t *input, int input_stride,
                                     uint16_t *output_q3);

#if CONFIG_AV1_HIGHBITDEPTH
typedef void (*cfl_subsample_hbd_fn)(const uint16_t *input, int input_stride,
                                     uint16_t *output_q3);

typedef void (*cfl_predict_hbd_fn)(const int16_t *src, uint16_t *dst,
                                   int dst_stride, int alpha_q3, int bd);
#endif

typedef void (*cfl_subtract_average_fn)(const uint16_t *src, int16_t *dst);

typedef void (*cfl_predict_lbd_fn)(const int16_t *src, uint8_t *dst,
                                   int dst_stride, int alpha_q3);

EOF
}
forward_decls qw/av1_common_forward_decls/;

# Fallbacks for Valgrind support
# For normal use, we require SSE4.1. However, 32-bit Valgrind does not support
# SSE4.1, so we include fallbacks for some critical functions to improve
# performance
$sse2_x86 = $ssse3_x86 = '';
if ($opts{arch} eq "x86") {
  $sse2_x86 = 'sse2';
  $ssse3_x86 = 'ssse3';
}

# functions that are 64 bit only.
$mmx_x86_64 = $sse2_x86_64 = $ssse3_x86_64 = $avx_x86_64 = $avx2_x86_64 = '';
if ($opts{arch} eq "x86_64") {
  $mmx_x86_64 = 'mmx';
  $sse2_x86_64 = 'sse2';
  $ssse3_x86_64 = 'ssse3';
  $avx_x86_64 = 'avx';
  $avx2_x86_64 = 'avx2';
}

add_proto qw/void av1_convolve_horiz_rs/, "const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, const int16_t *x_filters, int x0_qn, int x_step_qn";
specialize qw/av1_convolve_horiz_rs sse4_1 neon/;

if(aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
  add_proto qw/void av1_highbd_convolve_horiz_rs/, "const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w, int h, const int16_t *x_filters, int x0_qn, int x_step_qn, int bd";
  specialize qw/av1_highbd_convolve_horiz_rs sse4_1 neon/;

  if ((aom_config("CONFIG_REALTIME_ONLY") ne "yes") ||
      (aom_config("CONFIG_AV1_DECODER") eq "yes")) {
    add_proto qw/void av1_highbd_wiener_convolve_add_src/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, const WienerConvolveParams *conv_params, int bd";
    specialize qw/av1_highbd_wiener_convolve_add_src ssse3 avx2 neon/;
  }
}

if ((aom_config("CONFIG_REALTIME_ONLY") ne "yes") || (aom_config("CONFIG_AV1_DECODER") eq "yes")) {
  add_proto qw/void av1_wiener_convolve_add_src/,       "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, const WienerConvolveParams *conv_params";
  specialize qw/av1_wiener_convolve_add_src sse2 avx2 neon/;
}

# directional intra predictor functions
add_proto qw/void av1_dr_prediction_z1/, "uint8_t *dst, ptrdiff_t stride, int bw, int bh, const uint8_t *above, const uint8_t *left, int upsample_above, int dx, int dy";
specialize qw/av1_dr_prediction_z1 sse4_1 avx2 neon/;
add_proto qw/void av1_dr_prediction_z2/, "uint8_t *dst, ptrdiff_t stride, int bw, int bh, const uint8_t *above, const uint8_t *left, int upsample_above, int upsample_left, int dx, int dy";
specialize qw/av1_dr_prediction_z2 sse4_1 avx2 neon/;
add_proto qw/void av1_dr_prediction_z3/, "uint8_t *dst, ptrdiff_t stride, int bw, int bh, const uint8_t *above, const uint8_t *left, int upsample_left, int dx, int dy";
specialize qw/av1_dr_prediction_z3 sse4_1 avx2 neon/;

# FILTER_INTRA predictor functions
add_proto qw/void av1_filter_intra_predictor/, "uint8_t *dst, ptrdiff_t stride, TX_SIZE tx_size, const uint8_t *above, const uint8_t *left, int mode";
# TODO(aomedia:349436249): enable NEON for armv7 after SIGBUS is fixed.
if (aom_config("AOM_ARCH_ARM") eq "yes" && aom_config("AOM_ARCH_AARCH64") eq "") {
  specialize qw/av1_filter_intra_predictor sse4_1/;
} else {
  specialize qw/av1_filter_intra_predictor sse4_1 neon/;
}

# High bitdepth functions

#
# Sub Pixel Filters
#
if (aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
  add_proto qw/void av1_highbd_convolve_copy/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";

  add_proto qw/void av1_highbd_convolve_avg/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";

  add_proto qw/void av1_highbd_convolve8/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
  specialize qw/av1_highbd_convolve8/, "$sse2_x86_64";

  add_proto qw/void av1_highbd_convolve8_horiz/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
  specialize qw/av1_highbd_convolve8_horiz/, "$sse2_x86_64";

  add_proto qw/void av1_highbd_convolve8_vert/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
  specialize qw/av1_highbd_convolve8_vert/, "$sse2_x86_64";
}

#inv txfm
add_proto qw/void av1_inv_txfm_add/, "const tran_low_t *dqcoeff, uint8_t *dst, int stride, const TxfmParam *txfm_param";
specialize qw/av1_inv_txfm_add ssse3 avx2 neon/;

add_proto qw/void av1_highbd_inv_txfm_add/, "const tran_low_t *input, uint8_t *dest, int stride, const TxfmParam *txfm_param";
specialize qw/av1_highbd_inv_txfm_add sse4_1 avx2 neon/;

add_proto qw/void av1_inv_txfm2d_add_4x4/,  "const tran_low_t *input, uint8_t *dest, int stride, TX_TYPE tx_type, const int bd";
specialize qw/av1_inv_txfm2d_add_4x4 neon/;
add_proto qw/void av1_inv_txfm2d_add_8x8/,  "const tran_low_t *input, uint8_t *dest, int stride, TX_TYPE tx_type, const int bd";
specialize qw/av1_inv_txfm2d_add_8x8 neon/;
add_proto qw/void av1_inv_txfm2d_add_4x8/,  "const tran_low_t *input, uint8_t *dest, int stride, TX_TYPE tx_type, const int bd";
specialize qw/av1_inv_txfm2d_add_4x8 neon/;
add_proto qw/void av1_inv_txfm2d_add_8x4/,  "const tran_low_t *input, uint8_t *dest, int stride, TX_TYPE tx_type, const int bd";
specialize qw/av1_inv_txfm2d_add_8x4 neon/;
add_proto qw/void av1_inv_txfm2d_add_4x16/,  "const tran_low_t *input, uint8_t *dest, int stride, TX_TYPE tx_type, const int bd";
specialize qw/av1_inv_txfm2d_add_4x16 neon/;
add_proto qw/void av1_inv_txfm2d_add_16x4/,  "const tran_low_t *input, uint8_t *dest, int stride, TX_TYPE tx_type, const int bd";
specialize qw/av1_inv_txfm2d_add_16x4 neon/;
add_proto qw/void av1_inv_txfm2d_add_8x16/,  "const tran_low_t *input, uint8_t *dest, int stride, TX_TYPE tx_type, const int bd";
specialize qw/av1_inv_txfm2d_add_8x16  neon/;
add_proto qw/void av1_inv_txfm2d_add_16x8/,  "const tran_low_t *input, uint8_t *dest, int stride, TX_TYPE tx_type, const int bd";
specialize qw/av1_inv_txfm2d_add_16x8  neon/;
add_proto qw/void av1_inv_txfm2d_add_16x32/,  "const tran_low_t *input, uint8_t *dest, int stride, TX_TYPE tx_type, const int bd";
specialize qw/av1_inv_txfm2d_add_16x32  neon/;
add_proto qw/void av1_inv_txfm2d_add_32x16/,  "const tran_low_t *input, uint8_t *dest, int stride, TX_TYPE tx_type, const int bd";
specialize qw/av1_inv_txfm2d_add_32x16  neon/;
add_proto qw/void av1_inv_txfm2d_add_32x32/,  "const tran_low_t *input, uint8_t *dest, int stride, TX_TYPE tx_type, const int bd";
specialize qw/av1_inv_txfm2d_add_32x32  neon/;
add_proto qw/void av1_inv_txfm2d_add_32x64/,  "const tran_low_t *input, uint8_t *dest, int stride, TX_TYPE tx_type, const int bd";
specialize qw/av1_inv_txfm2d_add_32x64  neon/;
add_proto qw/void av1_inv_txfm2d_add_64x32/,  "const tran_low_t *input, uint8_t *dest, int stride, TX_TYPE tx_type, const int bd";
specialize qw/av1_inv_txfm2d_add_64x32  neon/;
add_proto qw/void av1_inv_txfm2d_add_64x64/,  "const tran_low_t *input, uint8_t *dest, int stride, TX_TYPE tx_type, const int bd";
specialize qw/av1_inv_txfm2d_add_64x64  neon/;
add_proto qw/void av1_inv_txfm2d_add_8x32/,  "const tran_low_t *input, uint8_t *dest, int stride, TX_TYPE tx_type, const int bd";
specialize qw/av1_inv_txfm2d_add_8x32  neon/;
add_proto qw/void av1_inv_txfm2d_add_32x8/,  "const tran_low_t *input, uint8_t *dest, int stride, TX_TYPE tx_type, const int bd";
specialize qw/av1_inv_txfm2d_add_32x8  neon/;
add_proto qw/void av1_inv_txfm2d_add_16x64/,  "const tran_low_t *input, uint8_t *dest, int stride, TX_TYPE tx_type, const int bd";
specialize qw/av1_inv_txfm2d_add_16x64  neon/;
add_proto qw/void av1_inv_txfm2d_add_64x16/,  "const tran_low_t *input, uint8_t *dest, int stride, TX_TYPE tx_type, const int bd";
specialize qw/av1_inv_txfm2d_add_64x16  neon/;

add_proto qw/void av1_highbd_iwht4x4_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
add_proto qw/void av1_highbd_iwht4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
specialize qw/av1_highbd_iwht4x4_16_add  sse4_1/;

add_proto qw/void av1_inv_txfm2d_add_4x8/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";
add_proto qw/void av1_inv_txfm2d_add_8x4/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";
add_proto qw/void av1_inv_txfm2d_add_8x16/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";
add_proto qw/void av1_inv_txfm2d_add_16x8/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";
add_proto qw/void av1_inv_txfm2d_add_16x32/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";
add_proto qw/void av1_inv_txfm2d_add_32x16/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";
add_proto qw/void av1_inv_txfm2d_add_4x4/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";
specialize qw/av1_inv_txfm2d_add_4x4 sse4_1/;
add_proto qw/void av1_inv_txfm2d_add_8x8/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";
specialize qw/av1_inv_txfm2d_add_8x8 sse4_1/;
add_proto qw/void av1_inv_txfm2d_add_16x16/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";
add_proto qw/void av1_inv_txfm2d_add_32x32/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";

add_proto qw/void av1_inv_txfm2d_add_64x64/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";
add_proto qw/void av1_inv_txfm2d_add_32x64/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";
add_proto qw/void av1_inv_txfm2d_add_64x32/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";
add_proto qw/void av1_inv_txfm2d_add_16x64/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";
add_proto qw/void av1_inv_txfm2d_add_64x16/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";

add_proto qw/void av1_inv_txfm2d_add_4x16/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";
add_proto qw/void av1_inv_txfm2d_add_16x4/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";
add_proto qw/void av1_inv_txfm2d_add_8x32/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";
add_proto qw/void av1_inv_txfm2d_add_32x8/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";

if (aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
  # directional intra predictor functions
  add_proto qw/void av1_highbd_dr_prediction_z1/, "uint16_t *dst, ptrdiff_t stride, int bw, int bh, const uint16_t *above, const uint16_t *left, int upsample_above, int dx, int dy, int bd";
  specialize qw/av1_highbd_dr_prediction_z1 avx2 neon/;
  add_proto qw/void av1_highbd_dr_prediction_z2/, "uint16_t *dst, ptrdiff_t stride, int bw, int bh, const uint16_t *above, const uint16_t *left, int upsample_above, int upsample_left, int dx, int dy, int bd";
  specialize qw/av1_highbd_dr_prediction_z2 avx2 neon/;
  add_proto qw/void av1_highbd_dr_prediction_z3/, "uint16_t *dst, ptrdiff_t stride, int bw, int bh, const uint16_t *above, const uint16_t *left, int upsample_left, int dx, int dy, int bd";
  specialize qw/av1_highbd_dr_prediction_z3 avx2 neon/;
}

# build compound seg mask functions
add_proto qw/void av1_build_compound_diffwtd_mask/, "uint8_t *mask, DIFFWTD_MASK_TYPE mask_type, const uint8_t *src0, int src0_stride, const uint8_t *src1, int src1_stride, int h, int w";
specialize qw/av1_build_compound_diffwtd_mask neon sse4_1 avx2/;

if (aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
  add_proto qw/void av1_build_compound_diffwtd_mask_highbd/, "uint8_t *mask, DIFFWTD_MASK_TYPE mask_type, const uint8_t *src0, int src0_stride, const uint8_t *src1, int src1_stride, int h, int w, int bd";
  specialize qw/av1_build_compound_diffwtd_mask_highbd ssse3 avx2 neon/;
}

add_proto qw/void av1_build_compound_diffwtd_mask_d16/, "uint8_t *mask, DIFFWTD_MASK_TYPE mask_type, const CONV_BUF_TYPE *src0, int src0_stride, const CONV_BUF_TYPE *src1, int src1_stride, int h, int w, ConvolveParams *conv_params, int bd";
specialize qw/av1_build_compound_diffwtd_mask_d16 sse4_1 avx2 neon/;

# Helper functions.
add_proto qw/void av1_round_shift_array/, "int32_t *arr, int size, int bit";
specialize "av1_round_shift_array", qw/sse4_1 neon/;

# Resize functions.
add_proto qw/void av1_resize_and_extend_frame/, "const YV12_BUFFER_CONFIG *src, YV12_BUFFER_CONFIG *dst, const InterpFilter filter, const int phase, const int num_planes";
specialize qw/av1_resize_and_extend_frame ssse3 neon neon_dotprod neon_i8mm/;

#
# Encoder functions below this point.
#
if (aom_config("CONFIG_AV1_ENCODER") eq "yes") {
  add_proto qw/void av1_fdwt8x8_uint8_input/, "const uint8_t *input, tran_low_t *output, int stride, int hbd";

  # ENCODEMB INVOKE
  add_proto qw/void aom_upsampled_pred/, "MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
                                          const MV *const mv, uint8_t *comp_pred, int width, int height, int subpel_x_q3,
                                          int subpel_y_q3, const uint8_t *ref, int ref_stride, int subpel_search";
  specialize qw/aom_upsampled_pred neon sse2/;
  #
  #
  #
  add_proto qw/void aom_comp_avg_upsampled_pred/, "MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
                                                   const MV *const mv, uint8_t *comp_pred, const uint8_t *pred, int width,
                                                   int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref,
                                                   int ref_stride, int subpel_search";
  specialize qw/aom_comp_avg_upsampled_pred sse2 neon/;

  if (aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
    add_proto qw/void aom_highbd_upsampled_pred/, "MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
                                                   const MV *const mv, uint8_t *comp_pred8, int width, int height, int subpel_x_q3,
                                                   int subpel_y_q3, const uint8_t *ref8, int ref_stride, int bd, int subpel_search";
    specialize qw/aom_highbd_upsampled_pred sse2 neon/;

    add_proto qw/void aom_highbd_comp_avg_upsampled_pred/, "MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
                                                            const MV *const mv, uint8_t *comp_pred8, const uint8_t *pred8, int width,
                                                            int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref8, int ref_stride, int bd, int subpel_search";
    specialize qw/aom_highbd_comp_avg_upsampled_pred sse2 neon/;
  }

  # the transform coefficients are held in 32-bit
  # values, so the assembler code for  av1_block_error can no longer be used.
  add_proto qw/int64_t av1_block_error/, "const tran_low_t *coeff, const tran_low_t *dqcoeff, intptr_t block_size, int64_t *ssz";
  specialize qw/av1_block_error sse2 avx2 neon sve/;

  add_proto qw/int64_t av1_block_error_lp/, "const int16_t *coeff, const int16_t *dqcoeff, intptr_t block_size";
  specialize qw/av1_block_error_lp sse2 avx2 neon sve/;

  add_proto qw/void av1_quantize_fp/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
  specialize qw/av1_quantize_fp sse2 avx2 neon/;

  add_proto qw/void av1_quantize_lp/, "const int16_t *coeff_ptr, intptr_t n_coeffs, const int16_t *round_ptr, const int16_t *quant_ptr, int16_t *qcoeff_ptr, int16_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
  specialize qw/av1_quantize_lp sse2 avx2 neon/;

  add_proto qw/void av1_quantize_fp_32x32/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
  specialize qw/av1_quantize_fp_32x32 neon avx2/;

  add_proto qw/void av1_quantize_fp_64x64/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
  specialize qw/av1_quantize_fp_64x64 neon avx2/;

  add_proto qw/void aom_quantize_b_helper/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan, const qm_val_t *qm_ptr, const qm_val_t *iqm_ptr, const int log_scale";
  specialize qw/aom_quantize_b_helper neon/;

  # fdct functions

  add_proto qw/void av1_fwht4x4/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/av1_fwht4x4 sse4_1 neon/;

  #fwd txfm
  add_proto qw/void av1_lowbd_fwd_txfm/, "const int16_t *src_diff, tran_low_t *coeff, int diff_stride, TxfmParam *txfm_param";
  specialize qw/av1_lowbd_fwd_txfm sse4_1 avx2 neon/, $sse2_x86;

  add_proto qw/void av1_fwd_txfm2d_4x8/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
  specialize qw/av1_fwd_txfm2d_4x8 sse4_1 neon/;
  add_proto qw/void av1_fwd_txfm2d_8x4/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
  specialize qw/av1_fwd_txfm2d_8x4 sse4_1 neon/;
  add_proto qw/void av1_fwd_txfm2d_8x16/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
  specialize qw/av1_fwd_txfm2d_8x16 sse4_1 avx2 neon/;
  add_proto qw/void av1_fwd_txfm2d_16x8/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
  specialize qw/av1_fwd_txfm2d_16x8 sse4_1 avx2 neon/;
  add_proto qw/void av1_fwd_txfm2d_16x32/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
  specialize qw/av1_fwd_txfm2d_16x32 sse4_1 neon/;
  add_proto qw/void av1_fwd_txfm2d_32x16/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
  specialize qw/av1_fwd_txfm2d_32x16 sse4_1 neon/;

  add_proto qw/void av1_fwd_txfm2d_4x4/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
  specialize qw/av1_fwd_txfm2d_4x4 sse4_1 neon/;
  add_proto qw/void av1_fwd_txfm2d_8x8/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
  specialize qw/av1_fwd_txfm2d_8x8 sse4_1 avx2 neon/;
  add_proto qw/void av1_fwd_txfm2d_16x16/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
  specialize qw/av1_fwd_txfm2d_16x16 sse4_1 avx2 neon/;
  add_proto qw/void av1_fwd_txfm2d_32x32/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
  specialize qw/av1_fwd_txfm2d_32x32 sse4_1 avx2 neon/;

  add_proto qw/void av1_fwd_txfm2d_64x64/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
  specialize qw/av1_fwd_txfm2d_64x64 sse4_1 avx2 neon/;
  add_proto qw/void av1_fwd_txfm2d_32x64/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
  specialize qw/av1_fwd_txfm2d_32x64 sse4_1 neon/;
  add_proto qw/void av1_fwd_txfm2d_64x32/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
  specialize qw/av1_fwd_txfm2d_64x32 sse4_1 neon/;
  add_proto qw/void av1_fwd_txfm2d_16x4/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
  specialize qw/av1_fwd_txfm2d_16x4 sse4_1 neon/;

  if (aom_config("CONFIG_REALTIME_ONLY") ne "yes") {
    add_proto qw/void av1_fwd_txfm2d_4x16/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
    specialize qw/av1_fwd_txfm2d_4x16 sse4_1 neon/;
    add_proto qw/void av1_fwd_txfm2d_8x32/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
    specialize qw/av1_fwd_txfm2d_8x32 sse4_1 neon/;
    add_proto qw/void av1_fwd_txfm2d_32x8/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
    specialize qw/av1_fwd_txfm2d_32x8 sse4_1 neon/;
    add_proto qw/void av1_fwd_txfm2d_16x64/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
    specialize qw/av1_fwd_txfm2d_16x64 sse4_1 neon/;
    add_proto qw/void av1_fwd_txfm2d_64x16/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
    specialize qw/av1_fwd_txfm2d_64x16 sse4_1 neon/;
  }
  #
  # Motion search
  #
  if (aom_config("CONFIG_REALTIME_ONLY") ne "yes") {
    add_proto qw/void av1_apply_temporal_filter/, "const struct yv12_buffer_config *frame_to_filter, const struct macroblockd *mbd, const BLOCK_SIZE block_size, const int mb_row, const int mb_col, const int num_planes, const double *noise_levels, const MV *subblock_mvs, const int *subblock_mses, const int q_factor, const int filter_strength, int tf_wgt_calc_lvl, const uint8_t *pred, uint32_t *accum, uint16_t *count";
    specialize qw/av1_apply_temporal_filter sse2 avx2 neon neon_dotprod/;

    add_proto qw/double av1_estimate_noise_from_single_plane/, "const uint8_t *src, int height, int width, int stride, int edge_thresh";
    specialize qw/av1_estimate_noise_from_single_plane avx2 neon/;
    if (aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
      add_proto qw/void av1_highbd_apply_temporal_filter/, "const struct yv12_buffer_config *frame_to_filter, const struct macroblockd *mbd, const BLOCK_SIZE block_size, const int mb_row, const int mb_col, const int num_planes, const double *noise_levels, const MV *subblock_mvs, const int *subblock_mses, const int q_factor, const int filter_strength, int tf_wgt_calc_lvl, const uint8_t *pred, uint32_t *accum, uint16_t *count";
      specialize qw/av1_highbd_apply_temporal_filter sse2 avx2 neon/;

      add_proto qw/double av1_highbd_estimate_noise_from_single_plane/, "const uint16_t *src, int height, int width, int stride, int bit_depth, int edge_thresh";
      specialize qw/av1_highbd_estimate_noise_from_single_plane neon/;
    }
  }

  add_proto qw/void av1_quantize_b/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan, const qm_val_t * qm_ptr, const qm_val_t * iqm_ptr, int log_scale";

  add_proto qw/void av1_calc_indices_dim1/, "const int16_t *data, const int16_t *centroids, uint8_t *indices, int64_t *total_dist, int n, int k";
  specialize qw/av1_calc_indices_dim1 sse2 avx2 neon/;

  add_proto qw/void av1_calc_indices_dim2/, "const int16_t *data, const int16_t *centroids, uint8_t *indices, int64_t *total_dist, int n, int k";
  specialize qw/av1_calc_indices_dim2 sse2 avx2 neon/;

  # ENCODEMB INVOKE
  if (aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
    add_proto qw/int64_t av1_highbd_block_error/, "const tran_low_t *coeff, const tran_low_t *dqcoeff, intptr_t block_size, int64_t *ssz, int bd";
    specialize qw/av1_highbd_block_error sse2 avx2 neon/;
  }

  if (aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
    add_proto qw/void av1_highbd_quantize_fp/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan, int log_scale";
    specialize qw/av1_highbd_quantize_fp sse4_1 avx2 neon/;
  }

  # End av1_high encoder functions

  # txb
  add_proto qw/void av1_get_nz_map_contexts/, "const uint8_t *const levels, const int16_t *const scan, const uint16_t eob, const TX_SIZE tx_size, const TX_CLASS tx_class, int8_t *const coeff_contexts";
  specialize qw/av1_get_nz_map_contexts sse2 neon/;
  add_proto qw/void av1_txb_init_levels/, "const tran_low_t *const coeff, const int width, const int height, uint8_t *const levels";
  specialize qw/av1_txb_init_levels sse4_1 avx2 neon/;

  add_proto qw/uint64_t av1_wedge_sse_from_residuals/, "const int16_t *r1, const int16_t *d, const uint8_t *m, int N";
  specialize qw/av1_wedge_sse_from_residuals sse2 avx2 neon sve/;
  add_proto qw/int8_t av1_wedge_sign_from_residuals/, "const int16_t *ds, const uint8_t *m, int N, int64_t limit";
  specialize qw/av1_wedge_sign_from_residuals sse2 avx2 neon sve/;
  add_proto qw/void av1_wedge_compute_delta_squares/, "int16_t *d, const int16_t *a, const int16_t *b, int N";
  specialize qw/av1_wedge_compute_delta_squares sse2 avx2 neon/;

  # hash
  add_proto qw/uint32_t av1_get_crc32c_value/, "void *crc_calculator, uint8_t *p, size_t length";
  specialize qw/av1_get_crc32c_value sse4_2 arm_crc32/;

  if (aom_config("CONFIG_REALTIME_ONLY") ne "yes") {
    add_proto qw/void av1_compute_stats/,  "int wiener_win, const uint8_t *dgd8, const uint8_t *src8, int16_t *dgd_avg, int16_t *src_avg, int h_start, int h_end, int v_start, int v_end, int dgd_stride, int src_stride, int64_t *M, int64_t *H, int use_downsampled_wiener_stats";
    specialize qw/av1_compute_stats sse4_1 avx2 neon sve/;
    add_proto qw/void av1_calc_proj_params/, "const uint8_t *src8, int width, int height, int src_stride, const uint8_t *dat8, int dat_stride, int32_t *flt0, int flt0_stride, int32_t *flt1, int flt1_stride, int64_t H[2][2], int64_t C[2], const sgr_params_type *params";
    specialize qw/av1_calc_proj_params sse4_1 avx2 neon/;
    add_proto qw/int64_t av1_lowbd_pixel_proj_error/, "const uint8_t *src8, int width, int height, int src_stride, const uint8_t *dat8, int dat_stride, int32_t *flt0, int flt0_stride, int32_t *flt1, int flt1_stride, int xq[2], const sgr_params_type *params";
    specialize qw/av1_lowbd_pixel_proj_error sse4_1 avx2 neon/;

    if (aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
      add_proto qw/void av1_calc_proj_params_high_bd/, "const uint8_t *src8, int width, int height, int src_stride, const uint8_t *dat8, int dat_stride, int32_t *flt0, int flt0_stride, int32_t *flt1, int flt1_stride, int64_t H[2][2], int64_t C[2], const sgr_params_type *params";
      specialize qw/av1_calc_proj_params_high_bd sse4_1 avx2 neon/;
      add_proto qw/int64_t av1_highbd_pixel_proj_error/, "const uint8_t *src8, int width, int height, int src_stride, const uint8_t *dat8, int dat_stride, int32_t *flt0, int flt0_stride, int32_t *flt1, int flt1_stride, int xq[2], const sgr_params_type *params";
      specialize qw/av1_highbd_pixel_proj_error sse4_1 avx2 neon/;
      add_proto qw/void av1_compute_stats_highbd/,  "int wiener_win, const uint8_t *dgd8, const uint8_t *src8, int16_t *dgd_avg, int16_t *src_avg, int h_start, int h_end, int v_start, int v_end, int dgd_stride, int src_stride, int64_t *M, int64_t *H, aom_bit_depth_t bit_depth";
      specialize qw/av1_compute_stats_highbd sse4_1 avx2 neon sve/;
    }
  }

  add_proto qw/void av1_get_horver_correlation_full/, "const int16_t *diff, int stride, int w, int h, float *hcorr, float *vcorr";
  specialize qw/av1_get_horver_correlation_full sse4_1 avx2 neon/;

  add_proto qw/void av1_nn_predict/, "const float *input_nodes, const NN_CONFIG *const nn_config, int reduce_prec, float *const output";

  add_proto qw/void av1_nn_fast_softmax_16/, "const float *input_nodes, float *output";
  if (aom_config("CONFIG_EXCLUDE_SIMD_MISMATCH") ne "yes") {
    specialize qw/av1_nn_predict sse3 avx2 neon/;
    specialize qw/av1_nn_fast_softmax_16 sse3/;
  }

  # CNN functions
  if (aom_config("CONFIG_REALTIME_ONLY") ne "yes") {
    add_proto qw/void av1_cnn_activate/, "float **input, int channels, int width, int height, int stride, ACTIVATION layer_activation";
    add_proto qw/void av1_cnn_add/, "float **input, int channels, int width, int height, int stride, const float **add";
    add_proto qw/bool av1_cnn_predict/, "const float **input, int in_width, int in_height, int in_stride, const CNN_CONFIG *cnn_config, const CNN_THREAD_DATA *thread_data, CNN_MULTI_OUT *output_struct";
    add_proto qw/void av1_cnn_convolve_no_maxpool_padding_valid/, "const float **input, int in_width, int in_height, int in_stride, const CNN_LAYER_CONFIG *layer_config, float **output, int out_stride, int start_idx, int cstep, int channel_step";
    if (aom_config("CONFIG_EXCLUDE_SIMD_MISMATCH") ne "yes") {
      specialize qw/av1_cnn_convolve_no_maxpool_padding_valid avx2/;
    }
    specialize qw/av1_cnn_convolve_no_maxpool_padding_valid neon/;
    add_proto qw/void av1_cnn_deconvolve/, "const float **input, int in_width, int in_height, int in_stride, const CNN_LAYER_CONFIG *layer_config, float **output, int out_stride";
    add_proto qw/void av1_cnn_batchnorm/, "float **image, int channels, int width, int height, int stride, const float *gamma, const float *beta, const float *mean, const float *std";
  }

  # Temporal Denoiser
  if (aom_config("CONFIG_AV1_TEMPORAL_DENOISING") eq "yes") {
    add_proto qw/int av1_denoiser_filter/, "const uint8_t *sig, int sig_stride, const uint8_t *mc_avg, int mc_avg_stride, uint8_t *avg, int avg_stride, int increase_denoising, BLOCK_SIZE bs, int motion_magnitude";
    specialize qw/av1_denoiser_filter neon sse2/;
  }
}
# end encoder functions


# Deringing Functions

add_proto qw/int cdef_find_dir/, "const uint16_t *img, int stride, int32_t *var, int coeff_shift";
add_proto qw/void cdef_find_dir_dual/, "const uint16_t *img1, const uint16_t *img2, int stride, int32_t *var1, int32_t *var2, int coeff_shift, int *out1, int *out2";

# 8 bit dst
add_proto qw/void cdef_filter_8_0/, "void *dst8, int dstride, const uint16_t *in, int pri_strength, int sec_strength, int dir, int pri_damping, int sec_damping, int coeff_shift, int block_width, int block_height";
add_proto qw/void cdef_filter_8_1/, "void *dst8, int dstride, const uint16_t *in, int pri_strength, int sec_strength, int dir, int pri_damping, int sec_damping, int coeff_shift, int block_width, int block_height";
add_proto qw/void cdef_filter_8_2/, "void *dst8, int dstride, const uint16_t *in, int pri_strength, int sec_strength, int dir, int pri_damping, int sec_damping, int coeff_shift, int block_width, int block_height";
add_proto qw/void cdef_filter_8_3/, "void *dst8, int dstride, const uint16_t *in, int pri_strength, int sec_strength, int dir, int pri_damping, int sec_damping, int coeff_shift, int block_width, int block_height";
# 16 bit dst
add_proto qw/void cdef_filter_16_0/, "void *dst16, int dstride, const uint16_t *in, int pri_strength, int sec_strength, int dir, int pri_damping, int sec_damping, int coeff_shift, int block_width, int block_height";
add_proto qw/void cdef_filter_16_1/, "void *dst16, int dstride, const uint16_t *in, int pri_strength, int sec_strength, int dir, int pri_damping, int sec_damping, int coeff_shift, int block_width, int block_height";
add_proto qw/void cdef_filter_16_2/, "void *dst16, int dstride, const uint16_t *in, int pri_strength, int sec_strength, int dir, int pri_damping, int sec_damping, int coeff_shift, int block_width, int block_height";
add_proto qw/void cdef_filter_16_3/, "void *dst16, int dstride, const uint16_t *in, int pri_strength, int sec_strength, int dir, int pri_damping, int sec_damping, int coeff_shift, int block_width, int block_height";

add_proto qw/void cdef_copy_rect8_8bit_to_16bit/, "uint16_t *dst, int dstride, const uint8_t *src, int sstride, int width, int height";
if (aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
  add_proto qw/void cdef_copy_rect8_16bit_to_16bit/, "uint16_t *dst, int dstride, const uint16_t *src, int sstride, int width, int height";
}

# VS compiling for 32 bit targets does not support vector types in
# structs as arguments, which makes the v256 type of the intrinsics
# hard to support, so optimizations for this target are disabled.
if ($opts{config} !~ /libs-x86-win32-vs.*/) {
  specialize qw/cdef_find_dir sse4_1 avx2 neon/, "$ssse3_x86";
  specialize qw/cdef_find_dir_dual sse4_1 avx2 neon/, "$ssse3_x86";

  specialize qw/cdef_filter_8_0 sse4_1 avx2 neon/, "$ssse3_x86";
  specialize qw/cdef_filter_8_1 sse4_1 avx2 neon/, "$ssse3_x86";
  specialize qw/cdef_filter_8_2 sse4_1 avx2 neon/, "$ssse3_x86";
  specialize qw/cdef_filter_8_3 sse4_1 avx2 neon/, "$ssse3_x86";

  specialize qw/cdef_filter_16_0 sse4_1 avx2 neon/, "$ssse3_x86";
  specialize qw/cdef_filter_16_1 sse4_1 avx2 neon/, "$ssse3_x86";
  specialize qw/cdef_filter_16_2 sse4_1 avx2 neon/, "$ssse3_x86";
  specialize qw/cdef_filter_16_3 sse4_1 avx2 neon/, "$ssse3_x86";

  specialize qw/cdef_copy_rect8_8bit_to_16bit sse4_1 avx2 neon/, "$ssse3_x86";
  if (aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
    specialize qw/cdef_copy_rect8_16bit_to_16bit sse4_1 avx2 neon/, "$ssse3_x86";
  }
}

# WARPED_MOTION / GLOBAL_MOTION functions
if (aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes" &&
    ((aom_config("CONFIG_REALTIME_ONLY") ne "yes") ||
      (aom_config("CONFIG_AV1_DECODER") eq "yes"))) {
  add_proto qw/void av1_highbd_warp_affine/, "const int32_t *mat, const uint16_t *ref, int width, int height, int stride, uint16_t *pred, int p_col, int p_row, int p_width, int p_height, int p_stride, int subsampling_x, int subsampling_y, int bd, ConvolveParams *conv_params, int16_t alpha, int16_t beta, int16_t gamma, int16_t delta";
  specialize qw/av1_highbd_warp_affine sse4_1 avx2 neon sve/;
}

add_proto qw/bool av1_resize_vert_dir/, "uint8_t *intbuf, uint8_t *output, int out_stride, int height, int height2, int width2, int start_col";
specialize qw/av1_resize_vert_dir sse2 avx2/;

add_proto qw/void av1_resize_horz_dir/, "const uint8_t *const input, int in_stride, uint8_t *intbuf, int height, int filtered_length, int width2";
specialize qw/av1_resize_horz_dir sse2 avx2/;

if ((aom_config("CONFIG_REALTIME_ONLY") ne "yes") || (aom_config("CONFIG_AV1_DECODER") eq "yes")) {
  add_proto qw/void av1_warp_affine/, "const int32_t *mat, const uint8_t *ref, int width, int height, int stride, uint8_t *pred, int p_col, int p_row, int p_width, int p_height, int p_stride, int subsampling_x, int subsampling_y, ConvolveParams *conv_params, int16_t alpha, int16_t beta, int16_t gamma, int16_t delta";
  specialize qw/av1_warp_affine sse4_1 avx2 neon neon_i8mm sve/;
}

# LOOP_RESTORATION functions
if ((aom_config("CONFIG_REALTIME_ONLY") ne "yes") || (aom_config("CONFIG_AV1_DECODER") eq "yes")) {
  add_proto qw/int av1_apply_selfguided_restoration/, "const uint8_t *dat, int width, int height, int stride, int eps, const int *xqd, uint8_t *dst, int dst_stride, int32_t *tmpbuf, int bit_depth, int highbd";
  specialize qw/av1_apply_selfguided_restoration sse4_1 avx2 neon/;

  add_proto qw/int av1_selfguided_restoration/, "const uint8_t *dgd8, int width, int height,
                                  int dgd_stride, int32_t *flt0, int32_t *flt1, int flt_stride,
                                  int sgr_params_idx, int bit_depth, int highbd";
  specialize qw/av1_selfguided_restoration sse4_1 avx2 neon/;
}

# CONVOLVE_ROUND/COMPOUND_ROUND functions

add_proto qw/void av1_convolve_2d_sr/, "const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, const InterpFilterParams *filter_params_x, const InterpFilterParams *filter_params_y, const int subpel_x_qn, const int subpel_y_qn, ConvolveParams *conv_params";
add_proto qw/void av1_convolve_2d_sr_intrabc/, "const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, const InterpFilterParams *filter_params_x, const InterpFilterParams *filter_params_y, const int subpel_x_qn, const int subpel_y_qn, ConvolveParams *conv_params";
add_proto qw/void av1_convolve_x_sr/, "const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, const InterpFilterParams *filter_params_x, const int subpel_x_qn, ConvolveParams *conv_params";
add_proto qw/void av1_convolve_x_sr_intrabc/, "const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, const InterpFilterParams *filter_params_x, const int subpel_x_qn, ConvolveParams *conv_params";
add_proto qw/void av1_convolve_y_sr/, "const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, const InterpFilterParams *filter_params_y, const int subpel_y_qn";
add_proto qw/void av1_convolve_y_sr_intrabc/, "const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, const InterpFilterParams *filter_params_y, const int subpel_y_qn";
add_proto qw/void av1_dist_wtd_convolve_2d/, "const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, const InterpFilterParams *filter_params_x, const InterpFilterParams *filter_params_y, const int subpel_x_qn, const int subpel_y_qn, ConvolveParams *conv_params";
add_proto qw/void av1_dist_wtd_convolve_2d_copy/, "const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, ConvolveParams *conv_params";
add_proto qw/void av1_dist_wtd_convolve_x/, "const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, const InterpFilterParams *filter_params_x, const int subpel_x_qn, ConvolveParams *conv_params";
add_proto qw/void av1_dist_wtd_convolve_y/, "const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, const InterpFilterParams *filter_params_y, const int subpel_y_qn, ConvolveParams *conv_params";
if(aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
  add_proto qw/void av1_highbd_convolve_2d_sr/, "const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w, int h, const InterpFilterParams *filter_params_x, const InterpFilterParams *filter_params_y, const int subpel_x_qn, const int subpel_y_qn, ConvolveParams *conv_params, int bd";
  add_proto qw/void av1_highbd_convolve_2d_sr_intrabc/, "const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w, int h, const InterpFilterParams *filter_params_x, const InterpFilterParams *filter_params_y, const int subpel_x_qn, const int subpel_y_qn, ConvolveParams *conv_params, int bd";
  add_proto qw/void av1_highbd_convolve_x_sr/, "const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w, int h, const InterpFilterParams *filter_params_x, const int subpel_x_qn, ConvolveParams *conv_params, int bd";
  add_proto qw/void av1_highbd_convolve_x_sr_intrabc/, "const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w, int h, const InterpFilterParams *filter_params_x, const int subpel_x_qn, ConvolveParams *conv_params, int bd";
  add_proto qw/void av1_highbd_convolve_y_sr/, "const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w, int h, const InterpFilterParams *filter_params_y, const int subpel_y_qn, int bd";
  add_proto qw/void av1_highbd_convolve_y_sr_intrabc/, "const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w, int h, const InterpFilterParams *filter_params_y, const int subpel_y_qn, int bd";
  add_proto qw/void av1_highbd_dist_wtd_convolve_2d/, "const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w, int h, const InterpFilterParams *filter_params_x, const InterpFilterParams *filter_params_y, const int subpel_x_qn, const int subpel_y_qn, ConvolveParams *conv_params, int bd";
  add_proto qw/void av1_highbd_dist_wtd_convolve_x/, "const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w, int h, const InterpFilterParams *filter_params_x, const int subpel_x_qn, ConvolveParams *conv_params, int bd";
  add_proto qw/void av1_highbd_dist_wtd_convolve_y/, "const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w, int h, const InterpFilterParams *filter_params_y, const int subpel_y_qn, ConvolveParams *conv_params, int bd";
  add_proto qw/void av1_highbd_dist_wtd_convolve_2d_copy/, "const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w, int h, ConvolveParams *conv_params, int bd";
  add_proto qw/void av1_highbd_convolve_2d_scale/, "const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w, int h, const InterpFilterParams *filter_params_x, const InterpFilterParams *filter_params_y, const int subpel_x_qn, const int x_step_qn, const int subpel_y_qn, const int y_step_qn, ConvolveParams *conv_params, int bd";
}

  add_proto qw/void av1_convolve_2d_scale/, "const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, const InterpFilterParams *filter_params_x, const InterpFilterParams *filter_params_y, const int subpel_x_qn, const int x_step_qn, const int subpel_y_qn, const int y_step_qn, ConvolveParams *conv_params";

  specialize qw/av1_convolve_2d_sr sse2 avx2 neon neon_dotprod neon_i8mm sve2/;
  specialize qw/av1_convolve_2d_sr_intrabc neon/;
  specialize qw/av1_convolve_x_sr sse2 avx2 neon neon_dotprod neon_i8mm/;
  specialize qw/av1_convolve_x_sr_intrabc neon/;
  specialize qw/av1_convolve_y_sr sse2 avx2 neon neon_dotprod neon_i8mm/;
  specialize qw/av1_convolve_y_sr_intrabc neon/;
  specialize qw/av1_convolve_2d_scale sse4_1 neon neon_dotprod neon_i8mm/;
  specialize qw/av1_dist_wtd_convolve_2d ssse3 avx2 neon neon_dotprod neon_i8mm/;
  specialize qw/av1_dist_wtd_convolve_2d_copy sse2 avx2 neon/;
  specialize qw/av1_dist_wtd_convolve_x sse2 avx2 neon neon_dotprod neon_i8mm/;
  specialize qw/av1_dist_wtd_convolve_y sse2 avx2 neon/;
  if(aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
    specialize qw/av1_highbd_dist_wtd_convolve_2d sse4_1 avx2 neon sve2/;
    specialize qw/av1_highbd_dist_wtd_convolve_x sse4_1 avx2 neon sve2/;
    specialize qw/av1_highbd_dist_wtd_convolve_y sse4_1 avx2 neon sve2/;
    specialize qw/av1_highbd_dist_wtd_convolve_2d_copy sse4_1 avx2 neon/;
    specialize qw/av1_highbd_convolve_2d_sr ssse3 avx2 neon sve2/;
    specialize qw/av1_highbd_convolve_2d_sr_intrabc neon/;
    specialize qw/av1_highbd_convolve_x_sr ssse3 avx2 neon sve2/;
    specialize qw/av1_highbd_convolve_x_sr_intrabc neon/;
    specialize qw/av1_highbd_convolve_y_sr ssse3 avx2 neon sve2/;
    specialize qw/av1_highbd_convolve_y_sr_intrabc neon/;
    specialize qw/av1_highbd_convolve_2d_scale sse4_1 neon/;
  }

# INTRA_EDGE functions
add_proto qw/void av1_filter_intra_edge/, "uint8_t *p, int sz, int strength";
specialize qw/av1_filter_intra_edge sse4_1 neon/;
add_proto qw/void av1_upsample_intra_edge/, "uint8_t *p, int sz";
specialize qw/av1_upsample_intra_edge sse4_1 neon/;

if (aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
  add_proto qw/void av1_highbd_filter_intra_edge/, "uint16_t *p, int sz, int strength";
  specialize qw/av1_highbd_filter_intra_edge sse4_1 neon/;
  add_proto qw/void av1_highbd_upsample_intra_edge/, "uint16_t *p, int sz, int bd";
  specialize qw/av1_highbd_upsample_intra_edge sse4_1 neon/;
}

# CFL
if ((aom_config("CONFIG_REALTIME_ONLY") ne "yes") ||
      (aom_config("CONFIG_AV1_DECODER") eq "yes")) {
  add_proto qw/cfl_subtract_average_fn cfl_get_subtract_average_fn/, "TX_SIZE tx_size";
  specialize qw/cfl_get_subtract_average_fn sse2 avx2 neon vsx/;

  add_proto qw/cfl_subsample_lbd_fn cfl_get_luma_subsampling_420_lbd/, "TX_SIZE tx_size";
  specialize qw/cfl_get_luma_subsampling_420_lbd ssse3 avx2 neon/;

  add_proto qw/cfl_subsample_lbd_fn cfl_get_luma_subsampling_422_lbd/, "TX_SIZE tx_size";
  specialize qw/cfl_get_luma_subsampling_422_lbd ssse3 avx2 neon/;

  add_proto qw/cfl_subsample_lbd_fn cfl_get_luma_subsampling_444_lbd/, "TX_SIZE tx_size";
  specialize qw/cfl_get_luma_subsampling_444_lbd ssse3 avx2 neon/;

  if (aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
    add_proto qw/cfl_subsample_hbd_fn cfl_get_luma_subsampling_420_hbd/, "TX_SIZE tx_size";
    specialize qw/cfl_get_luma_subsampling_420_hbd ssse3 avx2 neon/;

    add_proto qw/cfl_subsample_hbd_fn cfl_get_luma_subsampling_422_hbd/, "TX_SIZE tx_size";
    specialize qw/cfl_get_luma_subsampling_422_hbd ssse3 avx2 neon/;

    add_proto qw/cfl_subsample_hbd_fn cfl_get_luma_subsampling_444_hbd/, "TX_SIZE tx_size";
    specialize qw/cfl_get_luma_subsampling_444_hbd ssse3 avx2 neon/;

    add_proto qw/cfl_predict_hbd_fn cfl_get_predict_hbd_fn/, "TX_SIZE tx_size";
    specialize qw/cfl_get_predict_hbd_fn ssse3 avx2 neon/;
  }
  add_proto qw/cfl_predict_lbd_fn cfl_get_predict_lbd_fn/, "TX_SIZE tx_size";
  specialize qw/cfl_get_predict_lbd_fn ssse3 avx2 neon/;
}

1;
