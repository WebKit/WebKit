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
sub aom_dsp_forward_decls() {
print <<EOF
/*
 * DSP
 */

#include "aom/aom_integer.h"
#include "aom_dsp/aom_dsp_common.h"
#include "av1/common/blockd.h"
#include "av1/common/enums.h"

EOF
}
forward_decls qw/aom_dsp_forward_decls/;

# optimizations which depend on multiple features
$avx2_ssse3 = '';
if ((aom_config("HAVE_AVX2") eq "yes") && (aom_config("HAVE_SSSE3") eq "yes")) {
  $avx2_ssse3 = 'avx2';
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

@block_widths = (4, 8, 16, 32, 64, 128);

@encoder_block_sizes = ();
foreach $w (@block_widths) {
  foreach $h (@block_widths) {
    push @encoder_block_sizes, [$w, $h] if ($w <= 2*$h && $h <= 2*$w);
  }
}

if (aom_config("CONFIG_REALTIME_ONLY") ne "yes") {
  push @encoder_block_sizes, [4, 16];
  push @encoder_block_sizes, [16, 4];
  push @encoder_block_sizes, [8, 32];
  push @encoder_block_sizes, [32, 8];
  push @encoder_block_sizes, [16, 64];
  push @encoder_block_sizes, [64, 16];
}

@tx_dims = (4, 8, 16, 32, 64);
@tx_sizes = ();
foreach $w (@tx_dims) {
  push @tx_sizes, [$w, $w];
  foreach $h (@tx_dims) {
    push @tx_sizes, [$w, $h] if ($w >=4 && $h >=4 && ($w == 2*$h || $h == 2*$w));
    if ((aom_config("CONFIG_REALTIME_ONLY") ne "yes") ||
        (aom_config("CONFIG_AV1_DECODER") eq "yes")) {
      push @tx_sizes, [$w, $h] if ($w >=4 && $h >=4 && ($w == 4*$h || $h == 4*$w));
    }  # !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
  }
}

@pred_names = qw/dc dc_top dc_left dc_128 v h paeth smooth smooth_v smooth_h/;

#
# Intra prediction
#

foreach (@tx_sizes) {
  ($w, $h) = @$_;
  foreach $pred_name (@pred_names) {
    add_proto "void", "aom_${pred_name}_predictor_${w}x${h}",
              "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
    if (aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
        add_proto "void", "aom_highbd_${pred_name}_predictor_${w}x${h}",
                  "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
    }
  }
}

specialize qw/aom_dc_top_predictor_4x4 neon sse2/;
specialize qw/aom_dc_top_predictor_4x8 neon sse2/;
specialize qw/aom_dc_top_predictor_8x4 neon sse2/;
specialize qw/aom_dc_top_predictor_8x8 neon sse2/;
specialize qw/aom_dc_top_predictor_8x16 neon sse2/;
specialize qw/aom_dc_top_predictor_16x8 neon sse2/;
specialize qw/aom_dc_top_predictor_16x16 neon sse2/;
specialize qw/aom_dc_top_predictor_16x32 neon sse2/;
specialize qw/aom_dc_top_predictor_32x16 neon sse2 avx2/;
specialize qw/aom_dc_top_predictor_32x32 neon sse2 avx2/;
specialize qw/aom_dc_top_predictor_32x64 neon sse2 avx2/;
specialize qw/aom_dc_top_predictor_64x32 neon sse2 avx2/;
specialize qw/aom_dc_top_predictor_64x64 neon sse2 avx2/;

specialize qw/aom_dc_left_predictor_4x4 neon sse2/;
specialize qw/aom_dc_left_predictor_4x8 neon sse2/;
specialize qw/aom_dc_left_predictor_8x4 neon sse2/;
specialize qw/aom_dc_left_predictor_8x8 neon sse2/;
specialize qw/aom_dc_left_predictor_8x16 neon sse2/;
specialize qw/aom_dc_left_predictor_16x8 neon sse2/;
specialize qw/aom_dc_left_predictor_16x16 neon sse2/;
specialize qw/aom_dc_left_predictor_16x32 neon sse2/;
specialize qw/aom_dc_left_predictor_32x16 neon sse2 avx2/;
specialize qw/aom_dc_left_predictor_32x32 neon sse2 avx2/;
specialize qw/aom_dc_left_predictor_32x64 neon sse2 avx2/;
specialize qw/aom_dc_left_predictor_64x32 neon sse2 avx2/;
specialize qw/aom_dc_left_predictor_64x64 neon sse2 avx2/;

specialize qw/aom_dc_128_predictor_4x4 neon sse2/;
specialize qw/aom_dc_128_predictor_4x8 neon sse2/;
specialize qw/aom_dc_128_predictor_8x4 neon sse2/;
specialize qw/aom_dc_128_predictor_8x8 neon sse2/;
specialize qw/aom_dc_128_predictor_8x16 neon sse2/;
specialize qw/aom_dc_128_predictor_16x8 neon sse2/;
specialize qw/aom_dc_128_predictor_16x16 neon sse2/;
specialize qw/aom_dc_128_predictor_16x32 neon sse2/;
specialize qw/aom_dc_128_predictor_32x16 neon sse2 avx2/;
specialize qw/aom_dc_128_predictor_32x32 neon sse2 avx2/;
specialize qw/aom_dc_128_predictor_32x64 neon sse2 avx2/;
specialize qw/aom_dc_128_predictor_64x32 neon sse2 avx2/;
specialize qw/aom_dc_128_predictor_64x64 neon sse2 avx2/;

specialize qw/aom_v_predictor_4x4 neon sse2/;
specialize qw/aom_v_predictor_4x8 neon sse2/;
specialize qw/aom_v_predictor_8x4 neon sse2/;
specialize qw/aom_v_predictor_8x8 neon sse2/;
specialize qw/aom_v_predictor_8x16 neon sse2/;
specialize qw/aom_v_predictor_16x8 neon sse2/;
specialize qw/aom_v_predictor_16x16 neon sse2/;
specialize qw/aom_v_predictor_16x32 neon sse2/;
specialize qw/aom_v_predictor_32x16 neon sse2 avx2/;
specialize qw/aom_v_predictor_32x32 neon sse2 avx2/;
specialize qw/aom_v_predictor_32x64 neon sse2 avx2/;
specialize qw/aom_v_predictor_64x32 neon sse2 avx2/;
specialize qw/aom_v_predictor_64x64 neon sse2 avx2/;

specialize qw/aom_h_predictor_4x4 neon sse2/;
specialize qw/aom_h_predictor_4x8 neon sse2/;
specialize qw/aom_h_predictor_8x4 neon sse2/;
specialize qw/aom_h_predictor_8x8 neon sse2/;
specialize qw/aom_h_predictor_8x16 neon sse2/;
specialize qw/aom_h_predictor_16x8 neon sse2/;
specialize qw/aom_h_predictor_16x16 neon sse2/;
specialize qw/aom_h_predictor_16x32 neon sse2/;
specialize qw/aom_h_predictor_32x16 neon sse2/;
specialize qw/aom_h_predictor_32x32 neon sse2 avx2/;
specialize qw/aom_h_predictor_32x64 neon sse2/;
specialize qw/aom_h_predictor_64x32 neon sse2/;
specialize qw/aom_h_predictor_64x64 neon sse2/;

specialize qw/aom_paeth_predictor_4x4 ssse3 neon/;
specialize qw/aom_paeth_predictor_4x8 ssse3 neon/;
specialize qw/aom_paeth_predictor_8x4 ssse3 neon/;
specialize qw/aom_paeth_predictor_8x8 ssse3 neon/;
specialize qw/aom_paeth_predictor_8x16 ssse3 neon/;
specialize qw/aom_paeth_predictor_16x8 ssse3 avx2 neon/;
specialize qw/aom_paeth_predictor_16x16 ssse3 avx2 neon/;
specialize qw/aom_paeth_predictor_16x32 ssse3 avx2 neon/;
specialize qw/aom_paeth_predictor_32x16 ssse3 avx2 neon/;
specialize qw/aom_paeth_predictor_32x32 ssse3 avx2 neon/;
specialize qw/aom_paeth_predictor_32x64 ssse3 avx2 neon/;
specialize qw/aom_paeth_predictor_64x32 ssse3 avx2 neon/;
specialize qw/aom_paeth_predictor_64x64 ssse3 avx2 neon/;

specialize qw/aom_smooth_predictor_4x4 neon ssse3/;
specialize qw/aom_smooth_predictor_4x8 neon ssse3/;
specialize qw/aom_smooth_predictor_8x4 neon ssse3/;
specialize qw/aom_smooth_predictor_8x8 neon ssse3/;
specialize qw/aom_smooth_predictor_8x16 neon ssse3/;
specialize qw/aom_smooth_predictor_16x8 neon ssse3/;
specialize qw/aom_smooth_predictor_16x16 neon ssse3/;
specialize qw/aom_smooth_predictor_16x32 neon ssse3/;
specialize qw/aom_smooth_predictor_32x16 neon ssse3/;
specialize qw/aom_smooth_predictor_32x32 neon ssse3/;
specialize qw/aom_smooth_predictor_32x64 neon ssse3/;
specialize qw/aom_smooth_predictor_64x32 neon ssse3/;
specialize qw/aom_smooth_predictor_64x64 neon ssse3/;

specialize qw/aom_smooth_v_predictor_4x4 neon ssse3/;
specialize qw/aom_smooth_v_predictor_4x8 neon ssse3/;
specialize qw/aom_smooth_v_predictor_8x4 neon ssse3/;
specialize qw/aom_smooth_v_predictor_8x8 neon ssse3/;
specialize qw/aom_smooth_v_predictor_8x16 neon ssse3/;
specialize qw/aom_smooth_v_predictor_16x8 neon ssse3/;
specialize qw/aom_smooth_v_predictor_16x16 neon ssse3/;
specialize qw/aom_smooth_v_predictor_16x32 neon ssse3/;
specialize qw/aom_smooth_v_predictor_32x16 neon ssse3/;
specialize qw/aom_smooth_v_predictor_32x32 neon ssse3/;
specialize qw/aom_smooth_v_predictor_32x64 neon ssse3/;
specialize qw/aom_smooth_v_predictor_64x32 neon ssse3/;
specialize qw/aom_smooth_v_predictor_64x64 neon ssse3/;

specialize qw/aom_smooth_h_predictor_4x4 neon ssse3/;
specialize qw/aom_smooth_h_predictor_4x8 neon ssse3/;
specialize qw/aom_smooth_h_predictor_8x4 neon ssse3/;
specialize qw/aom_smooth_h_predictor_8x8 neon ssse3/;
specialize qw/aom_smooth_h_predictor_8x16 neon ssse3/;
specialize qw/aom_smooth_h_predictor_16x8 neon ssse3/;
specialize qw/aom_smooth_h_predictor_16x16 neon ssse3/;
specialize qw/aom_smooth_h_predictor_16x32 neon ssse3/;
specialize qw/aom_smooth_h_predictor_32x16 neon ssse3/;
specialize qw/aom_smooth_h_predictor_32x32 neon ssse3/;
specialize qw/aom_smooth_h_predictor_32x64 neon ssse3/;
specialize qw/aom_smooth_h_predictor_64x32 neon ssse3/;
specialize qw/aom_smooth_h_predictor_64x64 neon ssse3/;

# TODO(yunqingwang): optimize rectangular DC_PRED to replace division
# by multiply and shift.
specialize qw/aom_dc_predictor_4x4 neon sse2/;
specialize qw/aom_dc_predictor_4x8 neon sse2/;
specialize qw/aom_dc_predictor_8x4 neon sse2/;
specialize qw/aom_dc_predictor_8x8 neon sse2/;
specialize qw/aom_dc_predictor_8x16 neon sse2/;
specialize qw/aom_dc_predictor_16x8 neon sse2/;
specialize qw/aom_dc_predictor_16x16 neon sse2/;
specialize qw/aom_dc_predictor_16x32 neon sse2/;
specialize qw/aom_dc_predictor_32x16 neon sse2 avx2/;
specialize qw/aom_dc_predictor_32x32 neon sse2 avx2/;
specialize qw/aom_dc_predictor_32x64 neon sse2 avx2/;
specialize qw/aom_dc_predictor_64x64 neon sse2 avx2/;
specialize qw/aom_dc_predictor_64x32 neon sse2 avx2/;


if ((aom_config("CONFIG_REALTIME_ONLY") ne "yes") || (aom_config("CONFIG_AV1_DECODER") eq "yes")) {
  specialize qw/aom_dc_top_predictor_4x16 neon sse2/;
  specialize qw/aom_dc_top_predictor_8x32 neon sse2/;
  specialize qw/aom_dc_top_predictor_16x4 neon sse2/;
  specialize qw/aom_dc_top_predictor_16x64 neon sse2/;
  specialize qw/aom_dc_top_predictor_32x8 neon sse2/;
  specialize qw/aom_dc_top_predictor_64x16 neon sse2 avx2/;

  specialize qw/aom_dc_left_predictor_4x16 neon sse2/;
  specialize qw/aom_dc_left_predictor_8x32 neon sse2/;
  specialize qw/aom_dc_left_predictor_16x4 neon sse2/;
  specialize qw/aom_dc_left_predictor_16x64 neon sse2/;
  specialize qw/aom_dc_left_predictor_32x8 neon sse2/;
  specialize qw/aom_dc_left_predictor_64x16 neon sse2 avx2/;

  specialize qw/aom_dc_128_predictor_4x16 neon sse2/;
  specialize qw/aom_dc_128_predictor_8x32 neon sse2/;
  specialize qw/aom_dc_128_predictor_16x4 neon sse2/;
  specialize qw/aom_dc_128_predictor_16x64 neon sse2/;
  specialize qw/aom_dc_128_predictor_32x8 neon sse2/;
  specialize qw/aom_dc_128_predictor_64x16 neon sse2 avx2/;

  specialize qw/aom_v_predictor_4x16 neon sse2/;
  specialize qw/aom_v_predictor_8x32 neon sse2/;
  specialize qw/aom_v_predictor_16x4 neon sse2/;
  specialize qw/aom_v_predictor_16x64 neon sse2/;
  specialize qw/aom_v_predictor_32x8 neon sse2/;
  specialize qw/aom_v_predictor_64x16 neon sse2 avx2/;

  specialize qw/aom_h_predictor_4x16 neon sse2/;
  specialize qw/aom_h_predictor_8x32 neon sse2/;
  specialize qw/aom_h_predictor_16x4 neon sse2/;
  specialize qw/aom_h_predictor_16x64 neon sse2/;
  specialize qw/aom_h_predictor_32x8 neon sse2/;
  specialize qw/aom_h_predictor_64x16 neon sse2/;

  specialize qw/aom_paeth_predictor_4x16 ssse3 neon/;
  specialize qw/aom_paeth_predictor_8x32 ssse3 neon/;
  specialize qw/aom_paeth_predictor_16x4 ssse3 neon/;
  specialize qw/aom_paeth_predictor_16x64 ssse3 avx2 neon/;
  specialize qw/aom_paeth_predictor_32x8 ssse3 neon/;
  specialize qw/aom_paeth_predictor_64x16 ssse3 avx2 neon/;

  specialize qw/aom_smooth_predictor_4x16 neon ssse3/;
  specialize qw/aom_smooth_predictor_8x32 neon ssse3/;
  specialize qw/aom_smooth_predictor_16x4 neon ssse3/;
  specialize qw/aom_smooth_predictor_16x64 neon ssse3/;
  specialize qw/aom_smooth_predictor_32x8 neon ssse3/;
  specialize qw/aom_smooth_predictor_64x16 neon ssse3/;

  specialize qw/aom_smooth_v_predictor_4x16 neon ssse3/;
  specialize qw/aom_smooth_v_predictor_8x32 neon ssse3/;
  specialize qw/aom_smooth_v_predictor_16x4 neon ssse3/;
  specialize qw/aom_smooth_v_predictor_16x64 neon ssse3/;
  specialize qw/aom_smooth_v_predictor_32x8 neon ssse3/;
  specialize qw/aom_smooth_v_predictor_64x16 neon ssse3/;

  specialize qw/aom_smooth_h_predictor_4x16 neon ssse3/;
  specialize qw/aom_smooth_h_predictor_8x32 neon ssse3/;
  specialize qw/aom_smooth_h_predictor_16x4 neon ssse3/;
  specialize qw/aom_smooth_h_predictor_16x64 neon ssse3/;
  specialize qw/aom_smooth_h_predictor_32x8 neon ssse3/;
  specialize qw/aom_smooth_h_predictor_64x16 neon ssse3/;

  specialize qw/aom_dc_predictor_4x16 neon sse2/;
  specialize qw/aom_dc_predictor_8x32 neon sse2/;
  specialize qw/aom_dc_predictor_16x4 neon sse2/;
  specialize qw/aom_dc_predictor_16x64 neon sse2/;
  specialize qw/aom_dc_predictor_32x8 neon sse2/;
  specialize qw/aom_dc_predictor_64x16 neon sse2 avx2/;
}  # !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

if (aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
  specialize qw/aom_highbd_v_predictor_4x4 sse2 neon/;
  specialize qw/aom_highbd_v_predictor_4x8 sse2 neon/;
  specialize qw/aom_highbd_v_predictor_8x4 sse2 neon/;
  specialize qw/aom_highbd_v_predictor_8x8 sse2 neon/;
  specialize qw/aom_highbd_v_predictor_8x16 sse2 neon/;
  specialize qw/aom_highbd_v_predictor_16x8 sse2 neon/;
  specialize qw/aom_highbd_v_predictor_16x16 sse2 neon/;
  specialize qw/aom_highbd_v_predictor_16x32 sse2 neon/;
  specialize qw/aom_highbd_v_predictor_32x16 sse2 neon/;
  specialize qw/aom_highbd_v_predictor_32x32 sse2 neon/;
  specialize qw/aom_highbd_v_predictor_32x64 neon/;
  specialize qw/aom_highbd_v_predictor_64x32 neon/;
  specialize qw/aom_highbd_v_predictor_64x64 neon/;

  # TODO(yunqingwang): optimize rectangular DC_PRED to replace division
  # by multiply and shift.
  specialize qw/aom_highbd_dc_predictor_4x4 sse2 neon/;
  specialize qw/aom_highbd_dc_predictor_4x8 sse2 neon/;
  specialize qw/aom_highbd_dc_predictor_8x4 sse2 neon/;
  specialize qw/aom_highbd_dc_predictor_8x8 sse2 neon/;
  specialize qw/aom_highbd_dc_predictor_8x16 sse2 neon/;
  specialize qw/aom_highbd_dc_predictor_16x8 sse2 neon/;
  specialize qw/aom_highbd_dc_predictor_16x16 sse2 neon/;
  specialize qw/aom_highbd_dc_predictor_16x32 sse2 neon/;
  specialize qw/aom_highbd_dc_predictor_32x16 sse2 neon/;
  specialize qw/aom_highbd_dc_predictor_32x32 sse2 neon/;
  specialize qw/aom_highbd_dc_predictor_32x64 neon/;
  specialize qw/aom_highbd_dc_predictor_64x32 neon/;
  specialize qw/aom_highbd_dc_predictor_64x64 neon/;

  specialize qw/aom_highbd_h_predictor_4x4 sse2 neon/;
  specialize qw/aom_highbd_h_predictor_4x8 sse2 neon/;
  specialize qw/aom_highbd_h_predictor_8x4 sse2 neon/;
  specialize qw/aom_highbd_h_predictor_8x8 sse2 neon/;
  specialize qw/aom_highbd_h_predictor_8x16 sse2 neon/;
  specialize qw/aom_highbd_h_predictor_16x8 sse2 neon/;
  specialize qw/aom_highbd_h_predictor_16x16 sse2 neon/;
  specialize qw/aom_highbd_h_predictor_16x32 sse2 neon/;
  specialize qw/aom_highbd_h_predictor_32x16 sse2 neon/;
  specialize qw/aom_highbd_h_predictor_32x32 sse2 neon/;
  specialize qw/aom_highbd_h_predictor_32x64 neon/;
  specialize qw/aom_highbd_h_predictor_64x32 neon/;
  specialize qw/aom_highbd_h_predictor_64x64 neon/;

  specialize qw/aom_highbd_dc_128_predictor_4x4 sse2 neon/;
  specialize qw/aom_highbd_dc_128_predictor_4x8 sse2 neon/;
  specialize qw/aom_highbd_dc_128_predictor_8x4 sse2 neon/;
  specialize qw/aom_highbd_dc_128_predictor_8x8 sse2 neon/;
  specialize qw/aom_highbd_dc_128_predictor_8x16 sse2 neon/;
  specialize qw/aom_highbd_dc_128_predictor_16x8 sse2 neon/;
  specialize qw/aom_highbd_dc_128_predictor_16x16 sse2 neon/;
  specialize qw/aom_highbd_dc_128_predictor_16x32 sse2 neon/;
  specialize qw/aom_highbd_dc_128_predictor_32x16 sse2 neon/;
  specialize qw/aom_highbd_dc_128_predictor_32x32 sse2 neon/;
  specialize qw/aom_highbd_dc_128_predictor_32x64 neon/;
  specialize qw/aom_highbd_dc_128_predictor_64x32 neon/;
  specialize qw/aom_highbd_dc_128_predictor_64x64 neon/;

  specialize qw/aom_highbd_dc_left_predictor_4x4 sse2 neon/;
  specialize qw/aom_highbd_dc_left_predictor_4x8 sse2 neon/;
  specialize qw/aom_highbd_dc_left_predictor_8x4 sse2 neon/;
  specialize qw/aom_highbd_dc_left_predictor_8x8 sse2 neon/;
  specialize qw/aom_highbd_dc_left_predictor_8x16 sse2 neon/;
  specialize qw/aom_highbd_dc_left_predictor_16x8 sse2 neon/;
  specialize qw/aom_highbd_dc_left_predictor_16x16 sse2 neon/;
  specialize qw/aom_highbd_dc_left_predictor_16x32 sse2 neon/;
  specialize qw/aom_highbd_dc_left_predictor_32x16 sse2 neon/;
  specialize qw/aom_highbd_dc_left_predictor_32x32 sse2 neon/;
  specialize qw/aom_highbd_dc_left_predictor_32x64 neon/;
  specialize qw/aom_highbd_dc_left_predictor_64x32 neon/;
  specialize qw/aom_highbd_dc_left_predictor_64x64 neon/;

  specialize qw/aom_highbd_dc_top_predictor_4x4 sse2 neon/;
  specialize qw/aom_highbd_dc_top_predictor_4x8 sse2 neon/;
  specialize qw/aom_highbd_dc_top_predictor_8x4 sse2 neon/;
  specialize qw/aom_highbd_dc_top_predictor_8x8 sse2 neon/;
  specialize qw/aom_highbd_dc_top_predictor_8x16 sse2 neon/;
  specialize qw/aom_highbd_dc_top_predictor_16x8 sse2 neon/;
  specialize qw/aom_highbd_dc_top_predictor_16x16 sse2 neon/;
  specialize qw/aom_highbd_dc_top_predictor_16x32 sse2 neon/;
  specialize qw/aom_highbd_dc_top_predictor_32x16 sse2 neon/;
  specialize qw/aom_highbd_dc_top_predictor_32x32 sse2 neon/;
  specialize qw/aom_highbd_dc_top_predictor_32x64 neon/;
  specialize qw/aom_highbd_dc_top_predictor_64x32 neon/;
  specialize qw/aom_highbd_dc_top_predictor_64x64 neon/;

  specialize qw/aom_highbd_paeth_predictor_4x4 neon/;
  specialize qw/aom_highbd_paeth_predictor_4x8 neon/;
  specialize qw/aom_highbd_paeth_predictor_8x4 neon/;
  specialize qw/aom_highbd_paeth_predictor_8x8 neon/;
  specialize qw/aom_highbd_paeth_predictor_8x16 neon/;
  specialize qw/aom_highbd_paeth_predictor_16x8 neon/;
  specialize qw/aom_highbd_paeth_predictor_16x16 neon/;
  specialize qw/aom_highbd_paeth_predictor_16x32 neon/;
  specialize qw/aom_highbd_paeth_predictor_32x16 neon/;
  specialize qw/aom_highbd_paeth_predictor_32x32 neon/;
  specialize qw/aom_highbd_paeth_predictor_32x64 neon/;
  specialize qw/aom_highbd_paeth_predictor_64x32 neon/;
  specialize qw/aom_highbd_paeth_predictor_64x64 neon/;

  specialize qw/aom_highbd_smooth_predictor_4x4 neon/;
  specialize qw/aom_highbd_smooth_predictor_4x8 neon/;
  specialize qw/aom_highbd_smooth_predictor_8x4 neon/;
  specialize qw/aom_highbd_smooth_predictor_8x8 neon/;
  specialize qw/aom_highbd_smooth_predictor_8x16 neon/;
  specialize qw/aom_highbd_smooth_predictor_16x8 neon/;
  specialize qw/aom_highbd_smooth_predictor_16x16 neon/;
  specialize qw/aom_highbd_smooth_predictor_16x32 neon/;
  specialize qw/aom_highbd_smooth_predictor_32x16 neon/;
  specialize qw/aom_highbd_smooth_predictor_32x32 neon/;
  specialize qw/aom_highbd_smooth_predictor_32x64 neon/;
  specialize qw/aom_highbd_smooth_predictor_64x32 neon/;
  specialize qw/aom_highbd_smooth_predictor_64x64 neon/;

  specialize qw/aom_highbd_smooth_v_predictor_4x4 neon/;
  specialize qw/aom_highbd_smooth_v_predictor_4x8 neon/;
  specialize qw/aom_highbd_smooth_v_predictor_8x4 neon/;
  specialize qw/aom_highbd_smooth_v_predictor_8x8 neon/;
  specialize qw/aom_highbd_smooth_v_predictor_8x16 neon/;
  specialize qw/aom_highbd_smooth_v_predictor_16x8 neon/;
  specialize qw/aom_highbd_smooth_v_predictor_16x16 neon/;
  specialize qw/aom_highbd_smooth_v_predictor_16x32 neon/;
  specialize qw/aom_highbd_smooth_v_predictor_32x16 neon/;
  specialize qw/aom_highbd_smooth_v_predictor_32x32 neon/;
  specialize qw/aom_highbd_smooth_v_predictor_32x64 neon/;
  specialize qw/aom_highbd_smooth_v_predictor_64x32 neon/;
  specialize qw/aom_highbd_smooth_v_predictor_64x64 neon/;
  specialize qw/aom_highbd_smooth_h_predictor_4x4 neon/;
  specialize qw/aom_highbd_smooth_h_predictor_4x8 neon/;

  specialize qw/aom_highbd_smooth_h_predictor_8x4 neon/;
  specialize qw/aom_highbd_smooth_h_predictor_8x8 neon/;
  specialize qw/aom_highbd_smooth_h_predictor_8x16 neon/;
  specialize qw/aom_highbd_smooth_h_predictor_16x8 neon/;
  specialize qw/aom_highbd_smooth_h_predictor_16x16 neon/;
  specialize qw/aom_highbd_smooth_h_predictor_16x32 neon/;
  specialize qw/aom_highbd_smooth_h_predictor_32x16 neon/;
  specialize qw/aom_highbd_smooth_h_predictor_32x32 neon/;
  specialize qw/aom_highbd_smooth_h_predictor_32x64 neon/;
  specialize qw/aom_highbd_smooth_h_predictor_64x32 neon/;
  specialize qw/aom_highbd_smooth_h_predictor_64x64 neon/;

  if ((aom_config("CONFIG_REALTIME_ONLY") ne "yes") ||
      (aom_config("CONFIG_AV1_DECODER") eq "yes")) {
    specialize qw/aom_highbd_v_predictor_4x16 neon/;
    specialize qw/aom_highbd_v_predictor_8x32 neon/;
    specialize qw/aom_highbd_v_predictor_16x4 neon/;
    specialize qw/aom_highbd_v_predictor_16x64 neon/;
    specialize qw/aom_highbd_v_predictor_32x8 neon/;
    specialize qw/aom_highbd_v_predictor_64x16 neon/;

    specialize qw/aom_highbd_dc_predictor_4x16 neon/;
    specialize qw/aom_highbd_dc_predictor_8x32 neon/;
    specialize qw/aom_highbd_dc_predictor_16x4 neon/;
    specialize qw/aom_highbd_dc_predictor_16x64 neon/;
    specialize qw/aom_highbd_dc_predictor_32x8 neon/;
    specialize qw/aom_highbd_dc_predictor_64x16 neon/;

    specialize qw/aom_highbd_h_predictor_4x16 neon/;
    specialize qw/aom_highbd_h_predictor_8x32 neon/;
    specialize qw/aom_highbd_h_predictor_16x4 neon/;
    specialize qw/aom_highbd_h_predictor_16x64 neon/;
    specialize qw/aom_highbd_h_predictor_32x8 neon/;
    specialize qw/aom_highbd_h_predictor_64x16 neon/;

    specialize qw/aom_highbd_dc_128_predictor_4x16 neon/;
    specialize qw/aom_highbd_dc_128_predictor_8x32 neon/;
    specialize qw/aom_highbd_dc_128_predictor_16x4 neon/;
    specialize qw/aom_highbd_dc_128_predictor_16x64 neon/;
    specialize qw/aom_highbd_dc_128_predictor_32x8 neon/;
    specialize qw/aom_highbd_dc_128_predictor_64x16 neon/;

    specialize qw/aom_highbd_dc_left_predictor_4x16 neon/;
    specialize qw/aom_highbd_dc_left_predictor_8x32 neon/;
    specialize qw/aom_highbd_dc_left_predictor_16x4 neon/;
    specialize qw/aom_highbd_dc_left_predictor_16x64 neon/;
    specialize qw/aom_highbd_dc_left_predictor_32x8 neon/;
    specialize qw/aom_highbd_dc_left_predictor_64x16 neon/;

    specialize qw/aom_highbd_dc_top_predictor_4x16 neon/;
    specialize qw/aom_highbd_dc_top_predictor_8x32 neon/;
    specialize qw/aom_highbd_dc_top_predictor_16x4 neon/;
    specialize qw/aom_highbd_dc_top_predictor_16x64 neon/;
    specialize qw/aom_highbd_dc_top_predictor_32x8 neon/;
    specialize qw/aom_highbd_dc_top_predictor_64x16 neon/;

    specialize qw/aom_highbd_paeth_predictor_4x16 neon/;
    specialize qw/aom_highbd_paeth_predictor_8x32 neon/;
    specialize qw/aom_highbd_paeth_predictor_16x4 neon/;
    specialize qw/aom_highbd_paeth_predictor_16x64 neon/;
    specialize qw/aom_highbd_paeth_predictor_32x8 neon/;
    specialize qw/aom_highbd_paeth_predictor_64x16 neon/;

    specialize qw/aom_highbd_smooth_predictor_4x16 neon/;
    specialize qw/aom_highbd_smooth_predictor_8x32 neon/;
    specialize qw/aom_highbd_smooth_predictor_16x4 neon/;
    specialize qw/aom_highbd_smooth_predictor_16x64 neon/;
    specialize qw/aom_highbd_smooth_predictor_32x8 neon/;
    specialize qw/aom_highbd_smooth_predictor_64x16 neon/;

    specialize qw/aom_highbd_smooth_v_predictor_4x16 neon/;
    specialize qw/aom_highbd_smooth_v_predictor_8x32 neon/;
    specialize qw/aom_highbd_smooth_v_predictor_16x4 neon/;
    specialize qw/aom_highbd_smooth_v_predictor_16x64 neon/;
    specialize qw/aom_highbd_smooth_v_predictor_32x8 neon/;
    specialize qw/aom_highbd_smooth_v_predictor_64x16 neon/;

    specialize qw/aom_highbd_smooth_h_predictor_4x16 neon/;
    specialize qw/aom_highbd_smooth_h_predictor_8x32 neon/;
    specialize qw/aom_highbd_smooth_h_predictor_16x4 neon/;
    specialize qw/aom_highbd_smooth_h_predictor_16x64 neon/;
    specialize qw/aom_highbd_smooth_h_predictor_32x8 neon/;
    specialize qw/aom_highbd_smooth_h_predictor_64x16 neon/;
  }  # !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
}
#
# Sub Pixel Filters
#
add_proto qw/void aom_convolve_copy/,             "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, int w, int h";
add_proto qw/void aom_convolve8_horiz/,           "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h";
add_proto qw/void aom_convolve8_vert/,            "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h";

specialize qw/aom_convolve_copy       neon                        sse2 avx2/;
specialize qw/aom_convolve8_horiz     neon neon_dotprod neon_i8mm ssse3/, "$avx2_ssse3";
specialize qw/aom_convolve8_vert      neon neon_dotprod neon_i8mm ssse3/, "$avx2_ssse3";

add_proto qw/void aom_scaled_2d/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const InterpKernel *filter, int x0_q4, int x_step_q4, int y0_q4, int y_step_q4, int w, int h";
specialize qw/aom_scaled_2d ssse3 neon neon_dotprod neon_i8mm/;

if (aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
  add_proto qw/void aom_highbd_convolve_copy/, "const uint16_t *src, ptrdiff_t src_stride, uint16_t *dst, ptrdiff_t dst_stride, int w, int h";
  specialize qw/aom_highbd_convolve_copy sse2 avx2 neon/;

  add_proto qw/void aom_highbd_convolve8_horiz/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bd";
  specialize qw/aom_highbd_convolve8_horiz sse2 avx2 neon sve/;

  add_proto qw/void aom_highbd_convolve8_vert/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bd";
  specialize qw/aom_highbd_convolve8_vert sse2 avx2 neon sve/;
}

#
# Loopfilter
#
add_proto qw/void aom_lpf_vertical_14/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh";
specialize qw/aom_lpf_vertical_14 sse2 neon/;

add_proto qw/void aom_lpf_vertical_14_dual/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1";
specialize qw/aom_lpf_vertical_14_dual sse2 neon/;

add_proto qw/void aom_lpf_vertical_14_quad/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0";
specialize qw/aom_lpf_vertical_14_quad avx2 sse2 neon/;

add_proto qw/void aom_lpf_vertical_6/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh";
specialize qw/aom_lpf_vertical_6 sse2 neon/;

add_proto qw/void aom_lpf_vertical_8/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh";
specialize qw/aom_lpf_vertical_8 sse2 neon/;

add_proto qw/void aom_lpf_vertical_8_dual/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1";
specialize qw/aom_lpf_vertical_8_dual sse2 neon/;

add_proto qw/void aom_lpf_vertical_8_quad/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0";
specialize qw/aom_lpf_vertical_8_quad sse2 neon/;

add_proto qw/void aom_lpf_vertical_4/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh";
specialize qw/aom_lpf_vertical_4 sse2 neon/;

add_proto qw/void aom_lpf_vertical_4_dual/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1";
specialize qw/aom_lpf_vertical_4_dual sse2 neon/;

add_proto qw/void aom_lpf_vertical_4_quad/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0";
specialize qw/aom_lpf_vertical_4_quad sse2 neon/;

add_proto qw/void aom_lpf_horizontal_14/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh";
specialize qw/aom_lpf_horizontal_14 sse2 neon/;

add_proto qw/void aom_lpf_horizontal_14_dual/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1";
specialize qw/aom_lpf_horizontal_14_dual sse2 neon/;

add_proto qw/void aom_lpf_horizontal_14_quad/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0";
specialize qw/aom_lpf_horizontal_14_quad sse2 avx2 neon/;

add_proto qw/void aom_lpf_horizontal_6/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh";
specialize qw/aom_lpf_horizontal_6 sse2 neon/;

add_proto qw/void aom_lpf_horizontal_6_dual/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1";
specialize qw/aom_lpf_horizontal_6_dual sse2 neon/;

add_proto qw/void aom_lpf_horizontal_6_quad/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0";
specialize qw/aom_lpf_horizontal_6_quad sse2 avx2 neon/;

add_proto qw/void aom_lpf_horizontal_8/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh";
specialize qw/aom_lpf_horizontal_8 sse2 neon/;

add_proto qw/void aom_lpf_horizontal_8_dual/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1";
specialize qw/aom_lpf_horizontal_8_dual sse2 neon/;

add_proto qw/void aom_lpf_horizontal_8_quad/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0";
specialize qw/aom_lpf_horizontal_8_quad sse2 avx2 neon/;

add_proto qw/void aom_lpf_horizontal_4/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh";
specialize qw/aom_lpf_horizontal_4 sse2 neon/;

add_proto qw/void aom_lpf_horizontal_4_dual/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1";
specialize qw/aom_lpf_horizontal_4_dual sse2 neon/;

add_proto qw/void aom_lpf_horizontal_4_quad/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0";
specialize qw/aom_lpf_horizontal_4_quad sse2 neon/;

add_proto qw/void aom_lpf_vertical_6_dual/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1";
specialize qw/aom_lpf_vertical_6_dual sse2 neon/;

add_proto qw/void aom_lpf_vertical_6_quad/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0";
specialize qw/aom_lpf_vertical_6_quad sse2 neon/;

if (aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
  add_proto qw/void aom_highbd_lpf_vertical_14/, "uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd";
  specialize qw/aom_highbd_lpf_vertical_14 neon sse2/;

  add_proto qw/void aom_highbd_lpf_vertical_14_dual/, "uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd";
  specialize qw/aom_highbd_lpf_vertical_14_dual neon sse2 avx2/;

  add_proto qw/void aom_highbd_lpf_vertical_8/, "uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd";
  specialize qw/aom_highbd_lpf_vertical_8 neon sse2/;

  add_proto qw/void aom_highbd_lpf_vertical_8_dual/, "uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd";
  specialize qw/aom_highbd_lpf_vertical_8_dual neon sse2 avx2/;

  add_proto qw/void aom_highbd_lpf_vertical_6/, "uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd";
  specialize qw/aom_highbd_lpf_vertical_6 neon sse2/;

  add_proto qw/void aom_highbd_lpf_vertical_6_dual/, "uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd";
  specialize qw/aom_highbd_lpf_vertical_6_dual neon sse2/;

  add_proto qw/void aom_highbd_lpf_vertical_4/, "uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd";
  specialize qw/aom_highbd_lpf_vertical_4 neon sse2/;

  add_proto qw/void aom_highbd_lpf_vertical_4_dual/, "uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd";
  specialize qw/aom_highbd_lpf_vertical_4_dual neon sse2 avx2/;

  add_proto qw/void aom_highbd_lpf_horizontal_14/, "uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd";
  specialize qw/aom_highbd_lpf_horizontal_14 neon sse2/;

  add_proto qw/void aom_highbd_lpf_horizontal_14_dual/, "uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1,int bd";
  specialize qw/aom_highbd_lpf_horizontal_14_dual neon sse2 avx2/;

  add_proto qw/void aom_highbd_lpf_horizontal_6/, "uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd";
  specialize qw/aom_highbd_lpf_horizontal_6 neon sse2/;

  add_proto qw/void aom_highbd_lpf_horizontal_6_dual/, "uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd";
  specialize qw/aom_highbd_lpf_horizontal_6_dual neon sse2/;

  add_proto qw/void aom_highbd_lpf_horizontal_8/, "uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd";
  specialize qw/aom_highbd_lpf_horizontal_8 neon sse2/;

  add_proto qw/void aom_highbd_lpf_horizontal_8_dual/, "uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd";
  specialize qw/aom_highbd_lpf_horizontal_8_dual neon sse2 avx2/;

  add_proto qw/void aom_highbd_lpf_horizontal_4/, "uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd";
  specialize qw/aom_highbd_lpf_horizontal_4 neon sse2/;

  add_proto qw/void aom_highbd_lpf_horizontal_4_dual/, "uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd";
  specialize qw/aom_highbd_lpf_horizontal_4_dual neon sse2 avx2/;
}

#
# Encoder functions.
#

#
# Forward transform
#
if (aom_config("CONFIG_AV1_ENCODER") eq "yes"){
    add_proto qw/void aom_fdct4x4/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/aom_fdct4x4 neon sse2/;

    add_proto qw/void aom_fdct4x4_lp/, "const int16_t *input, int16_t *output, int stride";
    specialize qw/aom_fdct4x4_lp neon sse2/;

    if (aom_config("CONFIG_INTERNAL_STATS") eq "yes"){
      # 8x8 DCT transform for psnr-hvs. Unlike other transforms isn't compatible
      # with av1 scan orders, because it does two transposes.
      add_proto qw/void aom_fdct8x8/, "const int16_t *input, tran_low_t *output, int stride";
      specialize qw/aom_fdct8x8 neon sse2/, "$ssse3_x86_64";
      # High bit depth
      if (aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
        add_proto qw/void aom_highbd_fdct8x8/, "const int16_t *input, tran_low_t *output, int stride";
        specialize qw/aom_highbd_fdct8x8 sse2/;
      }
    }
    # FFT/IFFT (float) only used for denoising (and noise power spectral density estimation)
    add_proto qw/void aom_fft2x2_float/, "const float *input, float *temp, float *output";

    add_proto qw/void aom_fft4x4_float/, "const float *input, float *temp, float *output";
    specialize qw/aom_fft4x4_float                  sse2/;

    add_proto qw/void aom_fft8x8_float/, "const float *input, float *temp, float *output";
    specialize qw/aom_fft8x8_float avx2             sse2/;

    add_proto qw/void aom_fft16x16_float/, "const float *input, float *temp, float *output";
    specialize qw/aom_fft16x16_float avx2           sse2/;

    add_proto qw/void aom_fft32x32_float/, "const float *input, float *temp, float *output";
    specialize qw/aom_fft32x32_float avx2           sse2/;

    add_proto qw/void aom_ifft2x2_float/, "const float *input, float *temp, float *output";

    add_proto qw/void aom_ifft4x4_float/, "const float *input, float *temp, float *output";
    specialize qw/aom_ifft4x4_float                 sse2/;

    add_proto qw/void aom_ifft8x8_float/, "const float *input, float *temp, float *output";
    specialize qw/aom_ifft8x8_float avx2            sse2/;

    add_proto qw/void aom_ifft16x16_float/, "const float *input, float *temp, float *output";
    specialize qw/aom_ifft16x16_float avx2          sse2/;

    add_proto qw/void aom_ifft32x32_float/, "const float *input, float *temp, float *output";
    specialize qw/aom_ifft32x32_float avx2          sse2/;
}  # CONFIG_AV1_ENCODER

#
# Quantization
#
if (aom_config("CONFIG_AV1_ENCODER") eq "yes") {
  add_proto qw/void aom_quantize_b/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
  specialize qw/aom_quantize_b sse2 neon avx avx2/, "$ssse3_x86_64";

  add_proto qw/void aom_quantize_b_32x32/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
  specialize qw/aom_quantize_b_32x32 neon avx avx2/, "$ssse3_x86_64";

  add_proto qw/void aom_quantize_b_64x64/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
  specialize qw/aom_quantize_b_64x64 neon ssse3 avx2/;

  if (aom_config("CONFIG_REALTIME_ONLY") ne "yes") {
    add_proto qw/void aom_quantize_b_adaptive/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
    specialize qw/aom_quantize_b_adaptive sse2 avx2/;

    add_proto qw/void aom_quantize_b_32x32_adaptive/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
    specialize qw/aom_quantize_b_32x32_adaptive sse2/;

    add_proto qw/void aom_quantize_b_64x64_adaptive/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
    specialize qw/aom_quantize_b_64x64_adaptive sse2/;
  }
}  # CONFIG_AV1_ENCODER

if (aom_config("CONFIG_AV1_ENCODER") eq "yes" && aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
  add_proto qw/void aom_highbd_quantize_b/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
  specialize qw/aom_highbd_quantize_b sse2 avx2 neon/;

  add_proto qw/void aom_highbd_quantize_b_32x32/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
  specialize qw/aom_highbd_quantize_b_32x32 sse2 avx2 neon/;

  add_proto qw/void aom_highbd_quantize_b_64x64/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
  specialize qw/aom_highbd_quantize_b_64x64 sse2 avx2 neon/;

  if (aom_config("CONFIG_REALTIME_ONLY") ne "yes") {
    add_proto qw/void aom_highbd_quantize_b_adaptive/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
    specialize qw/aom_highbd_quantize_b_adaptive sse2 avx2 neon/;

    add_proto qw/void aom_highbd_quantize_b_32x32_adaptive/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
    specialize qw/aom_highbd_quantize_b_32x32_adaptive sse2 avx2 neon/;

    add_proto qw/void aom_highbd_quantize_b_64x64_adaptive/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
    specialize qw/aom_highbd_quantize_b_64x64_adaptive sse2 neon/;
  }
}  # CONFIG_AV1_ENCODER

#
# Alpha blending with mask
#
add_proto qw/void aom_lowbd_blend_a64_d16_mask/, "uint8_t *dst, uint32_t dst_stride, const CONV_BUF_TYPE *src0, uint32_t src0_stride, const CONV_BUF_TYPE *src1, uint32_t src1_stride, const uint8_t *mask, uint32_t mask_stride, int w, int h, int subw, int subh, ConvolveParams *conv_params";
specialize qw/aom_lowbd_blend_a64_d16_mask sse4_1 avx2 neon/;
add_proto qw/void aom_blend_a64_mask/, "uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, uint32_t mask_stride, int w, int h, int subw, int subh";
add_proto qw/void aom_blend_a64_hmask/, "uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, int w, int h";
add_proto qw/void aom_blend_a64_vmask/, "uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, int w, int h";
specialize "aom_blend_a64_mask", qw/sse4_1 neon avx2/;
specialize "aom_blend_a64_hmask", qw/sse4_1 neon/;
specialize "aom_blend_a64_vmask", qw/sse4_1 neon/;

if (aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
  add_proto qw/void aom_highbd_blend_a64_mask/, "uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, uint32_t mask_stride, int w, int h, int subw, int subh, int bd";
  add_proto qw/void aom_highbd_blend_a64_hmask/, "uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, int w, int h, int bd";
  add_proto qw/void aom_highbd_blend_a64_vmask/, "uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, int w, int h, int bd";
  add_proto qw/void aom_highbd_blend_a64_d16_mask/, "uint8_t *dst, uint32_t dst_stride, const CONV_BUF_TYPE *src0, uint32_t src0_stride, const CONV_BUF_TYPE *src1, uint32_t src1_stride, const uint8_t *mask, uint32_t mask_stride, int w, int h, int subw, int subh, ConvolveParams *conv_params, const int bd";
  specialize "aom_highbd_blend_a64_mask", qw/sse4_1 neon/;
  specialize "aom_highbd_blend_a64_hmask", qw/sse4_1 neon/;
  specialize "aom_highbd_blend_a64_vmask", qw/sse4_1 neon/;
  specialize "aom_highbd_blend_a64_d16_mask", qw/sse4_1 neon avx2/;
}

if (aom_config("CONFIG_AV1_ENCODER") eq "yes") {
  #
  # Block subtraction
  #
  add_proto qw/void aom_subtract_block/, "int rows, int cols, int16_t *diff_ptr, ptrdiff_t diff_stride, const uint8_t *src_ptr, ptrdiff_t src_stride, const uint8_t *pred_ptr, ptrdiff_t pred_stride";
  specialize qw/aom_subtract_block neon sse2 avx2/;

  add_proto qw/int64_t/, "aom_sse", "const uint8_t *a, int a_stride, const uint8_t *b,int b_stride, int width, int height";
  specialize qw/aom_sse sse4_1 avx2 neon neon_dotprod/;

  add_proto qw/void/, "aom_get_blk_sse_sum", "const int16_t *data, int stride, int bw, int bh, int *x_sum, int64_t *x2_sum";
  specialize qw/aom_get_blk_sse_sum sse2 avx2 neon sve/;

  if (aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
    add_proto qw/void aom_highbd_subtract_block/, "int rows, int cols, int16_t *diff_ptr, ptrdiff_t diff_stride, const uint8_t *src_ptr, ptrdiff_t src_stride, const uint8_t *pred_ptr, ptrdiff_t pred_stride";
    specialize qw/aom_highbd_subtract_block sse2 neon/;

    add_proto qw/int64_t/, "aom_highbd_sse", "const uint8_t *a8, int a_stride, const uint8_t *b8,int b_stride, int width, int height";
    specialize qw/aom_highbd_sse sse4_1 avx2 neon sve/;
  }

  #
  # Sum of Squares
  #
  add_proto qw/uint64_t aom_sum_squares_2d_i16/, "const int16_t *src, int stride, int width, int height";
  specialize qw/aom_sum_squares_2d_i16 sse2 avx2 neon sve/;

  add_proto qw/uint64_t aom_sum_squares_i16/, "const int16_t *src, uint32_t N";
  specialize qw/aom_sum_squares_i16 sse2 neon sve/;

  add_proto qw/uint64_t aom_var_2d_u8/, "uint8_t *src, int src_stride, int width, int height";
  specialize qw/aom_var_2d_u8 sse2 avx2 neon neon_dotprod/;

  if (aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
    add_proto qw/uint64_t aom_var_2d_u16/, "uint8_t *src, int src_stride, int width, int height";
    specialize qw/aom_var_2d_u16 sse2 avx2 neon sve/;
  }

  #
  # Single block SAD / Single block Avg SAD
  #
  foreach (@encoder_block_sizes) {
    ($w, $h) = @$_;
    add_proto qw/unsigned int/, "aom_sad${w}x${h}", "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
    if ($h >= 16) {
      add_proto qw/unsigned int/, "aom_sad_skip_${w}x${h}", "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
    }
    add_proto qw/unsigned int/, "aom_sad${w}x${h}_avg", "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
    add_proto qw/unsigned int/, "aom_dist_wtd_sad${w}x${h}_avg", "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const DIST_WTD_COMP_PARAMS *jcp_param";
  }

  add_proto qw/uint64_t aom_sum_sse_2d_i16/, "const int16_t *src, int src_stride, int width, int height, int *sum";
  specialize qw/aom_sum_sse_2d_i16 avx2 neon sse2 sve/;
  specialize qw/aom_sad128x128    avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad128x64     avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad64x128     avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad64x64      avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad64x32      avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad32x64      avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad32x32      avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad32x16      avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad16x32           sse2 neon neon_dotprod/;
  specialize qw/aom_sad16x16           sse2 neon neon_dotprod/;
  specialize qw/aom_sad16x8            sse2 neon neon_dotprod/;
  specialize qw/aom_sad8x16            sse2 neon/;
  specialize qw/aom_sad8x8             sse2 neon/;
  specialize qw/aom_sad8x4             sse2 neon/;
  specialize qw/aom_sad4x8             sse2 neon/;
  specialize qw/aom_sad4x4             sse2 neon/;

  specialize qw/aom_sad4x16            sse2 neon/;
  specialize qw/aom_sad16x4            sse2 neon neon_dotprod/;
  specialize qw/aom_sad8x32            sse2 neon/;
  specialize qw/aom_sad32x8            sse2 neon neon_dotprod/;
  specialize qw/aom_sad16x64           sse2 neon neon_dotprod/;
  specialize qw/aom_sad64x16           sse2 neon neon_dotprod/;

  specialize qw/aom_sad_skip_128x128    avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad_skip_128x64     avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad_skip_64x128     avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad_skip_64x64      avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad_skip_64x32      avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad_skip_32x64      avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad_skip_32x32      avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad_skip_32x16      avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad_skip_16x32           sse2 neon neon_dotprod/;
  specialize qw/aom_sad_skip_16x16           sse2 neon neon_dotprod/;
  specialize qw/aom_sad_skip_16x8            sse2 neon neon_dotprod/;
  specialize qw/aom_sad_skip_8x16            sse2 neon/;

  specialize qw/aom_sad_skip_4x16            sse2 neon/;
  specialize qw/aom_sad_skip_8x32            sse2 neon/;
  specialize qw/aom_sad_skip_16x64           sse2 neon neon_dotprod/;
  specialize qw/aom_sad_skip_64x16           sse2 neon neon_dotprod/;

  specialize qw/aom_sad128x128_avg avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad128x64_avg  avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad64x128_avg  avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad64x64_avg   avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad64x32_avg   avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad32x64_avg   avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad32x32_avg   avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad32x16_avg   avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad16x32_avg        sse2 neon neon_dotprod/;
  specialize qw/aom_sad16x16_avg        sse2 neon neon_dotprod/;
  specialize qw/aom_sad16x8_avg         sse2 neon neon_dotprod/;
  specialize qw/aom_sad8x16_avg         sse2 neon/;
  specialize qw/aom_sad8x8_avg          sse2 neon/;
  specialize qw/aom_sad8x4_avg          sse2 neon/;
  specialize qw/aom_sad4x8_avg          sse2 neon/;
  specialize qw/aom_sad4x4_avg          sse2 neon/;

  specialize qw/aom_sad4x16_avg         sse2 neon/;
  specialize qw/aom_sad16x4_avg         sse2 neon neon_dotprod/;
  specialize qw/aom_sad8x32_avg         sse2 neon/;
  specialize qw/aom_sad32x8_avg         sse2 neon neon_dotprod/;
  specialize qw/aom_sad16x64_avg        sse2 neon neon_dotprod/;
  specialize qw/aom_sad64x16_avg        sse2 neon neon_dotprod/;

  specialize qw/aom_dist_wtd_sad128x128_avg sse2 neon neon_dotprod/;
  specialize qw/aom_dist_wtd_sad128x64_avg  sse2 neon neon_dotprod/;
  specialize qw/aom_dist_wtd_sad64x128_avg  sse2 neon neon_dotprod/;
  specialize qw/aom_dist_wtd_sad64x64_avg   sse2 neon neon_dotprod/;
  specialize qw/aom_dist_wtd_sad64x32_avg   sse2 neon neon_dotprod/;
  specialize qw/aom_dist_wtd_sad32x64_avg   sse2 neon neon_dotprod/;
  specialize qw/aom_dist_wtd_sad32x32_avg   sse2 neon neon_dotprod/;
  specialize qw/aom_dist_wtd_sad32x16_avg   sse2 neon neon_dotprod/;
  specialize qw/aom_dist_wtd_sad16x32_avg   sse2 neon neon_dotprod/;
  specialize qw/aom_dist_wtd_sad16x16_avg   sse2 neon neon_dotprod/;
  specialize qw/aom_dist_wtd_sad16x8_avg    sse2 neon neon_dotprod/;
  specialize qw/aom_dist_wtd_sad8x16_avg    sse2 neon/;
  specialize qw/aom_dist_wtd_sad8x8_avg     sse2 neon/;
  specialize qw/aom_dist_wtd_sad8x4_avg     sse2 neon/;
  specialize qw/aom_dist_wtd_sad4x8_avg     sse2 neon/;
  specialize qw/aom_dist_wtd_sad4x4_avg     sse2 neon/;

  if (aom_config("CONFIG_REALTIME_ONLY") ne "yes") {
    specialize qw/aom_dist_wtd_sad4x16_avg     sse2 neon/;
    specialize qw/aom_dist_wtd_sad16x4_avg     sse2 neon neon_dotprod/;
    specialize qw/aom_dist_wtd_sad8x32_avg     sse2 neon/;
    specialize qw/aom_dist_wtd_sad32x8_avg     sse2 neon neon_dotprod/;
    specialize qw/aom_dist_wtd_sad16x64_avg    sse2 neon neon_dotprod/;
    specialize qw/aom_dist_wtd_sad64x16_avg    sse2 neon neon_dotprod/;
  }

  if (aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
    foreach (@encoder_block_sizes) {
      ($w, $h) = @$_;
      add_proto qw/unsigned int/, "aom_highbd_sad${w}x${h}", "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
      if ($h >= 16) {
        add_proto qw/unsigned int/, "aom_highbd_sad_skip_${w}x${h}", "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
      }
      add_proto qw/unsigned int/, "aom_highbd_sad${w}x${h}_avg", "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
      if ($w != 128 && $h != 128 && $w != 4) {
        specialize "aom_highbd_sad${w}x${h}", qw/sse2/;
        specialize "aom_highbd_sad${w}x${h}_avg", qw/sse2/;
      }
      add_proto qw/unsigned int/, "aom_highbd_dist_wtd_sad${w}x${h}_avg", "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const DIST_WTD_COMP_PARAMS* jcp_param";
    }
    specialize qw/aom_highbd_sad128x128 avx2      neon/;
    specialize qw/aom_highbd_sad128x64  avx2      neon/;
    specialize qw/aom_highbd_sad64x128  avx2      neon/;
    specialize qw/aom_highbd_sad64x64   avx2 sse2 neon/;
    specialize qw/aom_highbd_sad64x32   avx2 sse2 neon/;
    specialize qw/aom_highbd_sad32x64   avx2 sse2 neon/;
    specialize qw/aom_highbd_sad32x32   avx2 sse2 neon/;
    specialize qw/aom_highbd_sad32x16   avx2 sse2 neon/;
    specialize qw/aom_highbd_sad16x32   avx2 sse2 neon/;
    specialize qw/aom_highbd_sad16x16   avx2 sse2 neon/;
    specialize qw/aom_highbd_sad16x8    avx2 sse2 neon/;
    specialize qw/aom_highbd_sad8x16         sse2 neon/;
    specialize qw/aom_highbd_sad8x8          sse2 neon/;
    specialize qw/aom_highbd_sad8x4          sse2 neon/;
    specialize qw/aom_highbd_sad4x8          sse2 neon/;
    specialize qw/aom_highbd_sad4x4          sse2 neon/;

    specialize qw/aom_highbd_sad4x16         sse2 neon/;
    specialize qw/aom_highbd_sad16x4    avx2 sse2 neon/;
    specialize qw/aom_highbd_sad8x32         sse2 neon/;
    specialize qw/aom_highbd_sad32x8    avx2 sse2 neon/;
    specialize qw/aom_highbd_sad16x64   avx2 sse2 neon/;
    specialize qw/aom_highbd_sad64x16   avx2 sse2 neon/;

    specialize qw/aom_highbd_sad_skip_128x128 avx2      neon/;
    specialize qw/aom_highbd_sad_skip_128x64  avx2      neon/;
    specialize qw/aom_highbd_sad_skip_64x128  avx2      neon/;
    specialize qw/aom_highbd_sad_skip_64x64   avx2 sse2 neon/;
    specialize qw/aom_highbd_sad_skip_64x32   avx2 sse2 neon/;
    specialize qw/aom_highbd_sad_skip_32x64   avx2 sse2 neon/;
    specialize qw/aom_highbd_sad_skip_32x32   avx2 sse2 neon/;
    specialize qw/aom_highbd_sad_skip_32x16   avx2 sse2 neon/;
    specialize qw/aom_highbd_sad_skip_16x32   avx2 sse2 neon/;
    specialize qw/aom_highbd_sad_skip_16x16   avx2 sse2 neon/;
    specialize qw/aom_highbd_sad_skip_8x16         sse2 neon/;

    specialize qw/aom_highbd_sad_skip_4x16         sse2 neon/;
    specialize qw/aom_highbd_sad_skip_8x32         sse2 neon/;
    specialize qw/aom_highbd_sad_skip_16x64   avx2 sse2 neon/;
    specialize qw/aom_highbd_sad_skip_64x16   avx2 sse2 neon/;

    specialize qw/aom_highbd_sad128x128_avg avx2      neon/;
    specialize qw/aom_highbd_sad128x64_avg  avx2      neon/;
    specialize qw/aom_highbd_sad64x128_avg  avx2      neon/;
    specialize qw/aom_highbd_sad64x64_avg   avx2 sse2 neon/;
    specialize qw/aom_highbd_sad64x32_avg   avx2 sse2 neon/;
    specialize qw/aom_highbd_sad32x64_avg   avx2 sse2 neon/;
    specialize qw/aom_highbd_sad32x32_avg   avx2 sse2 neon/;
    specialize qw/aom_highbd_sad32x16_avg   avx2 sse2 neon/;
    specialize qw/aom_highbd_sad16x32_avg   avx2 sse2 neon/;
    specialize qw/aom_highbd_sad16x16_avg   avx2 sse2 neon/;
    specialize qw/aom_highbd_sad16x8_avg    avx2 sse2 neon/;
    specialize qw/aom_highbd_sad8x16_avg              neon/;
    specialize qw/aom_highbd_sad8x8_avg               neon/;
    specialize qw/aom_highbd_sad8x4_avg          sse2 neon/;
    specialize qw/aom_highbd_sad4x8_avg          sse2 neon/;
    specialize qw/aom_highbd_sad4x4_avg          sse2 neon/;

    specialize qw/aom_highbd_sad4x16_avg         sse2 neon/;
    specialize qw/aom_highbd_sad8x32_avg         sse2 neon/;
    specialize qw/aom_highbd_sad16x4_avg    avx2 sse2 neon/;
    specialize qw/aom_highbd_sad16x64_avg   avx2 sse2 neon/;
    specialize qw/aom_highbd_sad32x8_avg    avx2 sse2 neon/;
    specialize qw/aom_highbd_sad64x16_avg   avx2 sse2 neon/;
  }
  #
  # Masked SAD
  #
  foreach (@encoder_block_sizes) {
    ($w, $h) = @$_;
    add_proto qw/unsigned int/, "aom_masked_sad${w}x${h}", "const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask";
    specialize "aom_masked_sad${w}x${h}", qw/ssse3 avx2 neon/;
  }

  if (aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
    foreach (@encoder_block_sizes) {
      ($w, $h) = @$_;
      add_proto qw/unsigned int/, "aom_highbd_masked_sad${w}x${h}", "const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask";
      specialize "aom_highbd_masked_sad${w}x${h}", qw/ssse3 avx2 neon/;
    }
  }

  #
  # OBMC SAD
  #
  if (aom_config("CONFIG_REALTIME_ONLY") ne "yes") {
    foreach (@encoder_block_sizes) {
      ($w, $h) = @$_;
      add_proto qw/unsigned int/, "aom_obmc_sad${w}x${h}", "const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask";
      if (! (($w == 128 && $h == 32) || ($w == 32 && $h == 128))) {
        specialize "aom_obmc_sad${w}x${h}", qw/sse4_1 avx2 neon/;
      }
    }

    if (aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
      foreach (@encoder_block_sizes) {
        ($w, $h) = @$_;
        add_proto qw/unsigned int/, "aom_highbd_obmc_sad${w}x${h}", "const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask";
        if (! (($w == 128 && $h == 32) || ($w == 32 && $h == 128))) {
          specialize "aom_highbd_obmc_sad${w}x${h}", qw/sse4_1 avx2 neon/;
        }
      }
    }
  }

  #
  # Multi-block SAD, comparing a reference to N independent blocks
  #
  foreach (@encoder_block_sizes) {
    ($w, $h) = @$_;
    add_proto qw/void/, "aom_sad${w}x${h}x4d", "const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[4], int ref_stride, uint32_t sad_array[4]";
    add_proto qw/void/, "aom_sad${w}x${h}x3d", "const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[4], int ref_stride, uint32_t sad_array[4]";
    if ($h >= 16) {
      add_proto qw/void/, "aom_sad_skip_${w}x${h}x4d", "const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[4], int ref_stride, uint32_t sad_array[4]";
    }
  }

  specialize qw/aom_sad128x128x4d avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad128x64x4d  avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad64x128x4d  avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad64x64x4d   avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad64x32x4d   avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad32x64x4d   avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad32x32x4d   avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad32x16x4d   avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad16x32x4d   avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad16x16x4d   avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad16x8x4d    avx2 sse2 neon neon_dotprod/;

  specialize qw/aom_sad8x16x4d         sse2 neon/;
  specialize qw/aom_sad8x8x4d          sse2 neon/;
  specialize qw/aom_sad8x4x4d          sse2 neon/;
  specialize qw/aom_sad4x8x4d          sse2 neon/;
  specialize qw/aom_sad4x4x4d          sse2 neon/;

  specialize qw/aom_sad64x16x4d   avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad32x8x4d    avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad16x64x4d   avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad16x4x4d    avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad8x32x4d         sse2 neon/;
  specialize qw/aom_sad4x16x4d         sse2 neon/;

  specialize qw/aom_sad_skip_128x128x4d avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad_skip_128x64x4d  avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad_skip_64x128x4d  avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad_skip_64x64x4d   avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad_skip_64x32x4d   avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad_skip_64x16x4d   avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad_skip_32x64x4d   avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad_skip_32x32x4d   avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad_skip_32x16x4d   avx2 sse2 neon neon_dotprod/;

  specialize qw/aom_sad_skip_16x64x4d   avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad_skip_16x32x4d   avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad_skip_16x16x4d   avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad_skip_16x8x4d    avx2 sse2 neon neon_dotprod/;
  specialize qw/aom_sad_skip_8x32x4d         sse2 neon/;
  specialize qw/aom_sad_skip_8x16x4d         sse2 neon/;
  specialize qw/aom_sad_skip_4x16x4d         sse2 neon/;

  specialize qw/aom_sad128x128x3d avx2 neon neon_dotprod/;
  specialize qw/aom_sad128x64x3d  avx2 neon neon_dotprod/;
  specialize qw/aom_sad64x128x3d  avx2 neon neon_dotprod/;
  specialize qw/aom_sad64x64x3d   avx2 neon neon_dotprod/;
  specialize qw/aom_sad64x32x3d   avx2 neon neon_dotprod/;
  specialize qw/aom_sad32x64x3d   avx2 neon neon_dotprod/;
  specialize qw/aom_sad32x32x3d   avx2 neon neon_dotprod/;
  specialize qw/aom_sad32x16x3d   avx2 neon neon_dotprod/;
  specialize qw/aom_sad16x32x3d   avx2 neon neon_dotprod/;
  specialize qw/aom_sad16x16x3d   avx2 neon neon_dotprod/;
  specialize qw/aom_sad16x8x3d    avx2 neon neon_dotprod/;
  specialize qw/aom_sad8x16x3d         neon/;
  specialize qw/aom_sad8x8x3d          neon/;
  specialize qw/aom_sad8x4x3d          neon/;
  specialize qw/aom_sad4x8x3d          neon/;
  specialize qw/aom_sad4x4x3d          neon/;

  specialize qw/aom_sad64x16x3d   avx2 neon neon_dotprod/;
  specialize qw/aom_sad32x8x3d    avx2 neon neon_dotprod/;
  specialize qw/aom_sad16x64x3d   avx2 neon neon_dotprod/;
  specialize qw/aom_sad16x4x3d    avx2 neon neon_dotprod/;
  specialize qw/aom_sad8x32x3d         neon/;
  specialize qw/aom_sad4x16x3d         neon/;

  #
  # Multi-block SAD, comparing a reference to N independent blocks
  #
  if (aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
    foreach (@encoder_block_sizes) {
      ($w, $h) = @$_;
      add_proto qw/void/, "aom_highbd_sad${w}x${h}x4d", "const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[4], int ref_stride, uint32_t sad_array[4]";
      add_proto qw/void/, "aom_highbd_sad${w}x${h}x3d", "const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[4], int ref_stride, uint32_t sad_array[4]";
      if ($h >= 16) {
        add_proto qw/void/, "aom_highbd_sad_skip_${w}x${h}x4d", "const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[4], int ref_stride, uint32_t sad_array[4]";
      }
      if ($w != 128 && $h != 128) {
        specialize "aom_highbd_sad${w}x${h}x4d", qw/sse2/;
      }
    }
    specialize qw/aom_highbd_sad128x128x4d      avx2 neon/;
    specialize qw/aom_highbd_sad128x64x4d       avx2 neon/;
    specialize qw/aom_highbd_sad64x128x4d       avx2 neon/;
    specialize qw/aom_highbd_sad64x64x4d   sse2 avx2 neon/;
    specialize qw/aom_highbd_sad64x32x4d   sse2 avx2 neon/;
    specialize qw/aom_highbd_sad32x64x4d   sse2 avx2 neon/;
    specialize qw/aom_highbd_sad32x32x4d   sse2 avx2 neon/;
    specialize qw/aom_highbd_sad32x16x4d   sse2 avx2 neon/;
    specialize qw/aom_highbd_sad16x32x4d   sse2 avx2 neon/;
    specialize qw/aom_highbd_sad16x16x4d   sse2 avx2 neon/;
    specialize qw/aom_highbd_sad16x8x4d    sse2 avx2 neon/;
    specialize qw/aom_highbd_sad8x16x4d    sse2      neon/;
    specialize qw/aom_highbd_sad8x8x4d     sse2      neon/;
    specialize qw/aom_highbd_sad8x4x4d     sse2      neon/;
    specialize qw/aom_highbd_sad4x8x4d     sse2      neon/;
    specialize qw/aom_highbd_sad4x4x4d     sse2      neon/;

    specialize qw/aom_highbd_sad4x16x4d         sse2 neon/;
    specialize qw/aom_highbd_sad16x4x4d    avx2 sse2 neon/;
    specialize qw/aom_highbd_sad8x32x4d         sse2 neon/;
    specialize qw/aom_highbd_sad32x8x4d    avx2 sse2 neon/;
    specialize qw/aom_highbd_sad16x64x4d   avx2 sse2 neon/;
    specialize qw/aom_highbd_sad64x16x4d   avx2 sse2 neon/;

    specialize qw/aom_highbd_sad_skip_128x128x4d avx2      neon/;
    specialize qw/aom_highbd_sad_skip_128x64x4d  avx2      neon/;
    specialize qw/aom_highbd_sad_skip_64x128x4d  avx2      neon/;
    specialize qw/aom_highbd_sad_skip_64x64x4d   avx2 sse2 neon/;
    specialize qw/aom_highbd_sad_skip_64x32x4d   avx2 sse2 neon/;
    specialize qw/aom_highbd_sad_skip_32x64x4d   avx2 sse2 neon/;
    specialize qw/aom_highbd_sad_skip_32x32x4d   avx2 sse2 neon/;
    specialize qw/aom_highbd_sad_skip_32x16x4d   avx2 sse2 neon/;
    specialize qw/aom_highbd_sad_skip_16x32x4d   avx2 sse2 neon/;
    specialize qw/aom_highbd_sad_skip_16x16x4d   avx2 sse2 neon/;
    specialize qw/aom_highbd_sad_skip_8x16x4d         sse2 neon/;

    specialize qw/aom_highbd_sad_skip_4x16x4d         sse2 neon/;
    specialize qw/aom_highbd_sad_skip_8x32x4d         sse2 neon/;
    specialize qw/aom_highbd_sad_skip_16x64x4d   avx2 sse2 neon/;
    specialize qw/aom_highbd_sad_skip_64x16x4d   avx2 sse2 neon/;

    specialize qw/aom_highbd_sad128x128x3d avx2 neon/;
    specialize qw/aom_highbd_sad128x64x3d  avx2 neon/;
    specialize qw/aom_highbd_sad64x128x3d  avx2 neon/;
    specialize qw/aom_highbd_sad64x64x3d   avx2 neon/;
    specialize qw/aom_highbd_sad64x32x3d   avx2 neon/;
    specialize qw/aom_highbd_sad32x64x3d   avx2 neon/;
    specialize qw/aom_highbd_sad32x32x3d   avx2 neon/;
    specialize qw/aom_highbd_sad32x16x3d   avx2 neon/;
    specialize qw/aom_highbd_sad16x32x3d   avx2 neon/;
    specialize qw/aom_highbd_sad16x16x3d   avx2 neon/;
    specialize qw/aom_highbd_sad16x8x3d    avx2 neon/;
    specialize qw/aom_highbd_sad8x16x3d         neon/;
    specialize qw/aom_highbd_sad8x8x3d          neon/;
    specialize qw/aom_highbd_sad8x4x3d          neon/;
    specialize qw/aom_highbd_sad4x8x3d          neon/;
    specialize qw/aom_highbd_sad4x4x3d          neon/;

    specialize qw/aom_highbd_sad64x16x3d   avx2 neon/;
    specialize qw/aom_highbd_sad32x8x3d    avx2 neon/;
    specialize qw/aom_highbd_sad16x64x3d   avx2 neon/;
    specialize qw/aom_highbd_sad16x4x3d    avx2 neon/;
    specialize qw/aom_highbd_sad8x32x3d         neon/;
    specialize qw/aom_highbd_sad4x16x3d         neon/;
  }
  #
  # Avg
  #
  add_proto qw/unsigned int aom_avg_8x8/, "const uint8_t *, int p";
  specialize qw/aom_avg_8x8 sse2 neon/;

  add_proto qw/unsigned int aom_avg_4x4/, "const uint8_t *, int p";
  specialize qw/aom_avg_4x4 sse2 neon/;

  add_proto qw/void aom_avg_8x8_quad/, "const uint8_t *s, int p, int x16_idx, int y16_idx, int *avg";
  specialize qw/aom_avg_8x8_quad avx2 sse2 neon/;

  add_proto qw/void aom_minmax_8x8/, "const uint8_t *s, int p, const uint8_t *d, int dp, int *min, int *max";
  specialize qw/aom_minmax_8x8 sse2 neon/;

  if (aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
    add_proto qw/unsigned int aom_highbd_avg_8x8/, "const uint8_t *, int p";
    specialize qw/aom_highbd_avg_8x8 neon/;
    add_proto qw/unsigned int aom_highbd_avg_4x4/, "const uint8_t *, int p";
    specialize qw/aom_highbd_avg_4x4 neon/;
    add_proto qw/void aom_highbd_minmax_8x8/, "const uint8_t *s, int p, const uint8_t *d, int dp, int *min, int *max";
    specialize qw/aom_highbd_minmax_8x8 neon/;
  }

  add_proto qw/void aom_int_pro_row/, "int16_t *hbuf, const uint8_t *ref, const int ref_stride, const int width, const int height, int norm_factor";
  specialize qw/aom_int_pro_row avx2 sse2 neon/;

  add_proto qw/void aom_int_pro_col/, "int16_t *vbuf, const uint8_t *ref, const int ref_stride, const int width, const int height, int norm_factor";
  specialize qw/aom_int_pro_col avx2 sse2 neon/;

  add_proto qw/int aom_vector_var/, "const int16_t *ref, const int16_t *src, int bwl";
  specialize qw/aom_vector_var avx2 sse4_1 neon sve/;

  #
  # hamadard transform and satd for implmenting temporal dependency model
  #
  add_proto qw/void aom_hadamard_4x4/, "const int16_t *src_diff, ptrdiff_t src_stride, tran_low_t *coeff";
  specialize qw/aom_hadamard_4x4 sse2 neon/;

  add_proto qw/void aom_hadamard_8x8/, "const int16_t *src_diff, ptrdiff_t src_stride, tran_low_t *coeff";
  specialize qw/aom_hadamard_8x8 sse2 neon/;

  add_proto qw/void aom_hadamard_16x16/, "const int16_t *src_diff, ptrdiff_t src_stride, tran_low_t *coeff";
  specialize qw/aom_hadamard_16x16 avx2 sse2 neon/;

  add_proto qw/void aom_hadamard_32x32/, "const int16_t *src_diff, ptrdiff_t src_stride, tran_low_t *coeff";
  specialize qw/aom_hadamard_32x32 avx2 sse2 neon/;

  add_proto qw/void aom_hadamard_lp_8x8/, "const int16_t *src_diff, ptrdiff_t src_stride, int16_t *coeff";
  specialize qw/aom_hadamard_lp_8x8 sse2 neon/;

  add_proto qw/void aom_hadamard_lp_16x16/, "const int16_t *src_diff, ptrdiff_t src_stride, int16_t *coeff";
  specialize qw/aom_hadamard_lp_16x16 sse2 avx2 neon/;

  add_proto qw/void aom_hadamard_lp_8x8_dual/, "const int16_t *src_diff, ptrdiff_t src_stride, int16_t *coeff";
  specialize qw/aom_hadamard_lp_8x8_dual sse2 avx2 neon/;

  if (aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
    add_proto qw/void aom_highbd_hadamard_8x8/, "const int16_t *src_diff, ptrdiff_t src_stride, tran_low_t *coeff";
    specialize qw/aom_highbd_hadamard_8x8 avx2 neon/;

    add_proto qw/void aom_highbd_hadamard_16x16/, "const int16_t *src_diff, ptrdiff_t src_stride, tran_low_t *coeff";
    specialize qw/aom_highbd_hadamard_16x16 avx2 neon/;

    add_proto qw/void aom_highbd_hadamard_32x32/, "const int16_t *src_diff, ptrdiff_t src_stride, tran_low_t *coeff";
    specialize qw/aom_highbd_hadamard_32x32 avx2 neon/;
  }
  add_proto qw/int aom_satd/, "const tran_low_t *coeff, int length";
  specialize qw/aom_satd neon sse2 avx2/;

  add_proto qw/int aom_satd_lp/, "const int16_t *coeff, int length";
  specialize qw/aom_satd_lp sse2 avx2 neon/;


  #
  # Structured Similarity (SSIM)
  #
  add_proto qw/void aom_ssim_parms_8x8/, "const uint8_t *s, int sp, const uint8_t *r, int rp, uint32_t *sum_s, uint32_t *sum_r, uint32_t *sum_sq_s, uint32_t *sum_sq_r, uint32_t *sum_sxr";
  specialize qw/aom_ssim_parms_8x8/, "$sse2_x86_64";

  if (aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
    add_proto qw/void aom_highbd_ssim_parms_8x8/, "const uint16_t *s, int sp, const uint16_t *r, int rp, uint32_t *sum_s, uint32_t *sum_r, uint32_t *sum_sq_s, uint32_t *sum_sq_r, uint32_t *sum_sxr";
  }
}  # CONFIG_AV1_ENCODER

if (aom_config("CONFIG_AV1_ENCODER") eq "yes") {

  #
  # Specialty Variance
  #
  add_proto qw/void aom_get_var_sse_sum_8x8_quad/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse8x8, int *sum8x8, unsigned int *tot_sse, int *tot_sum, uint32_t *var8x8";
  specialize qw/aom_get_var_sse_sum_8x8_quad        avx2 sse2 neon neon_dotprod/;

  add_proto qw/void aom_get_var_sse_sum_16x16_dual/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse16x16, unsigned int *tot_sse, int *tot_sum, uint32_t *var16x16";
  specialize qw/aom_get_var_sse_sum_16x16_dual        avx2 sse2 neon neon_dotprod/;

  add_proto qw/unsigned int aom_mse16x16/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  add_proto qw/unsigned int aom_mse16x8/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  add_proto qw/unsigned int aom_mse8x16/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  add_proto qw/unsigned int aom_mse8x8/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";

  specialize qw/aom_mse16x16          sse2 avx2 neon neon_dotprod/;
  specialize qw/aom_mse16x8           sse2      neon neon_dotprod/;
  specialize qw/aom_mse8x16           sse2      neon neon_dotprod/;
  specialize qw/aom_mse8x8            sse2      neon neon_dotprod/;

  if (aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
    foreach $bd (8, 10, 12) {
      add_proto qw/unsigned int/, "aom_highbd_${bd}_mse16x16", "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
      add_proto qw/unsigned int/, "aom_highbd_${bd}_mse16x8", "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
      add_proto qw/unsigned int/, "aom_highbd_${bd}_mse8x16", "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
      add_proto qw/unsigned int/, "aom_highbd_${bd}_mse8x8", "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";

      if ($bd eq 8) {
        specialize "aom_highbd_${bd}_mse16x16", qw/sse2 neon neon_dotprod/;
        specialize "aom_highbd_${bd}_mse16x8", qw/neon neon_dotprod/;
        specialize "aom_highbd_${bd}_mse8x16", qw/neon neon_dotprod/;
        specialize "aom_highbd_${bd}_mse8x8", qw/sse2 neon neon_dotprod/;
      } elsif ($bd eq 10) {
        specialize "aom_highbd_${bd}_mse16x16", qw/avx2 sse2 neon sve/;
        specialize "aom_highbd_${bd}_mse16x8", qw/neon sve/;
        specialize "aom_highbd_${bd}_mse8x16", qw/neon sve/;
        specialize "aom_highbd_${bd}_mse8x8", qw/sse2 neon sve/;
      } else {
        specialize "aom_highbd_${bd}_mse16x16", qw/sse2 neon sve/;
        specialize "aom_highbd_${bd}_mse16x8", qw/neon sve/;
        specialize "aom_highbd_${bd}_mse8x16", qw/neon sve/;
        specialize "aom_highbd_${bd}_mse8x8", qw/sse2 neon sve/;
      }

    }
  }

  if (aom_config("CONFIG_REALTIME_ONLY") ne "yes") {
    add_proto qw/unsigned int aom_get_mb_ss/, "const int16_t *";
    specialize qw/aom_get_mb_ss sse2 neon/;
  }

  #
  # Variance / Subpixel Variance / Subpixel Avg Variance
  #
  add_proto qw/uint64_t/, "aom_mse_wxh_16bit", "uint8_t *dst, int dstride,uint16_t *src, int sstride, int w, int h";
  specialize qw/aom_mse_wxh_16bit  sse2 avx2 neon/;

  add_proto qw/uint64_t/, "aom_mse_16xh_16bit", "uint8_t *dst, int dstride,uint16_t *src, int w, int h";
  specialize qw/aom_mse_16xh_16bit sse2 avx2 neon/;

  foreach (@encoder_block_sizes) {
    ($w, $h) = @$_;
    add_proto qw/unsigned int/, "aom_variance${w}x${h}", "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    add_proto qw/uint32_t/, "aom_sub_pixel_variance${w}x${h}", "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    add_proto qw/uint32_t/, "aom_sub_pixel_avg_variance${w}x${h}", "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    add_proto qw/uint32_t/, "aom_dist_wtd_sub_pixel_avg_variance${w}x${h}", "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const DIST_WTD_COMP_PARAMS *jcp_param";
  }
  specialize qw/aom_variance128x128   sse2 avx2 neon neon_dotprod/;
  specialize qw/aom_variance128x64    sse2 avx2 neon neon_dotprod/;
  specialize qw/aom_variance64x128    sse2 avx2 neon neon_dotprod/;
  specialize qw/aom_variance64x64     sse2 avx2 neon neon_dotprod/;
  specialize qw/aom_variance64x32     sse2 avx2 neon neon_dotprod/;
  specialize qw/aom_variance32x64     sse2 avx2 neon neon_dotprod/;
  specialize qw/aom_variance32x32     sse2 avx2 neon neon_dotprod/;
  specialize qw/aom_variance32x16     sse2 avx2 neon neon_dotprod/;
  specialize qw/aom_variance16x32     sse2 avx2 neon neon_dotprod/;
  specialize qw/aom_variance16x16     sse2 avx2 neon neon_dotprod/;
  specialize qw/aom_variance16x8      sse2 avx2 neon neon_dotprod/;
  specialize qw/aom_variance8x16      sse2      neon neon_dotprod/;
  specialize qw/aom_variance8x8       sse2      neon neon_dotprod/;
  specialize qw/aom_variance8x4       sse2      neon neon_dotprod/;
  specialize qw/aom_variance4x8       sse2      neon neon_dotprod/;
  specialize qw/aom_variance4x4       sse2      neon neon_dotprod/;

  specialize qw/aom_sub_pixel_variance128x128   avx2 neon ssse3/;
  specialize qw/aom_sub_pixel_variance128x64    avx2 neon ssse3/;
  specialize qw/aom_sub_pixel_variance64x128    avx2 neon ssse3/;
  specialize qw/aom_sub_pixel_variance64x64     avx2 neon ssse3/;
  specialize qw/aom_sub_pixel_variance64x32     avx2 neon ssse3/;
  specialize qw/aom_sub_pixel_variance32x64     avx2 neon ssse3/;
  specialize qw/aom_sub_pixel_variance32x32     avx2 neon ssse3/;
  specialize qw/aom_sub_pixel_variance32x16     avx2 neon ssse3/;
  specialize qw/aom_sub_pixel_variance16x32     avx2 neon ssse3/;
  specialize qw/aom_sub_pixel_variance16x16     avx2 neon ssse3/;
  specialize qw/aom_sub_pixel_variance16x8      avx2 neon ssse3/;
  specialize qw/aom_sub_pixel_variance8x16           neon ssse3/;
  specialize qw/aom_sub_pixel_variance8x8            neon ssse3/;
  specialize qw/aom_sub_pixel_variance8x4            neon ssse3/;
  specialize qw/aom_sub_pixel_variance4x8            neon ssse3/;
  specialize qw/aom_sub_pixel_variance4x4            neon ssse3/;

  specialize qw/aom_sub_pixel_avg_variance128x128 avx2 neon ssse3/;
  specialize qw/aom_sub_pixel_avg_variance128x64  avx2 neon ssse3/;
  specialize qw/aom_sub_pixel_avg_variance64x128  avx2 neon ssse3/;
  specialize qw/aom_sub_pixel_avg_variance64x64   avx2 neon ssse3/;
  specialize qw/aom_sub_pixel_avg_variance64x32   avx2 neon ssse3/;
  specialize qw/aom_sub_pixel_avg_variance32x64   avx2 neon ssse3/;
  specialize qw/aom_sub_pixel_avg_variance32x32   avx2 neon ssse3/;
  specialize qw/aom_sub_pixel_avg_variance32x16   avx2 neon ssse3/;
  specialize qw/aom_sub_pixel_avg_variance16x32        neon ssse3/;
  specialize qw/aom_sub_pixel_avg_variance16x16        neon ssse3/;
  specialize qw/aom_sub_pixel_avg_variance16x8         neon ssse3/;
  specialize qw/aom_sub_pixel_avg_variance8x16         neon ssse3/;
  specialize qw/aom_sub_pixel_avg_variance8x8          neon ssse3/;
  specialize qw/aom_sub_pixel_avg_variance8x4          neon ssse3/;
  specialize qw/aom_sub_pixel_avg_variance4x8          neon ssse3/;
  specialize qw/aom_sub_pixel_avg_variance4x4          neon ssse3/;

  if (aom_config("CONFIG_REALTIME_ONLY") ne "yes") {
    specialize qw/aom_variance4x16  neon neon_dotprod sse2/;
    specialize qw/aom_variance16x4  neon neon_dotprod sse2 avx2/;
    specialize qw/aom_variance8x32  neon neon_dotprod sse2/;
    specialize qw/aom_variance32x8  neon neon_dotprod sse2 avx2/;
    specialize qw/aom_variance16x64 neon neon_dotprod sse2 avx2/;
    specialize qw/aom_variance64x16 neon neon_dotprod sse2 avx2/;

    specialize qw/aom_sub_pixel_variance4x16 neon ssse3/;
    specialize qw/aom_sub_pixel_variance16x4 neon avx2 ssse3/;
    specialize qw/aom_sub_pixel_variance8x32 neon ssse3/;
    specialize qw/aom_sub_pixel_variance32x8 neon ssse3/;
    specialize qw/aom_sub_pixel_variance16x64 neon avx2 ssse3/;
    specialize qw/aom_sub_pixel_variance64x16 neon ssse3/;
    specialize qw/aom_sub_pixel_avg_variance4x16 neon ssse3/;
    specialize qw/aom_sub_pixel_avg_variance16x4 neon ssse3/;
    specialize qw/aom_sub_pixel_avg_variance8x32 neon ssse3/;
    specialize qw/aom_sub_pixel_avg_variance32x8 neon ssse3/;
    specialize qw/aom_sub_pixel_avg_variance16x64 neon ssse3/;
    specialize qw/aom_sub_pixel_avg_variance64x16 neon ssse3/;

    specialize qw/aom_dist_wtd_sub_pixel_avg_variance4x16  neon ssse3/;
    specialize qw/aom_dist_wtd_sub_pixel_avg_variance16x4  neon ssse3/;
    specialize qw/aom_dist_wtd_sub_pixel_avg_variance8x32  neon ssse3/;
    specialize qw/aom_dist_wtd_sub_pixel_avg_variance32x8  neon ssse3/;
    specialize qw/aom_dist_wtd_sub_pixel_avg_variance16x64 neon ssse3/;
    specialize qw/aom_dist_wtd_sub_pixel_avg_variance64x16 neon ssse3/;
  }

  specialize qw/aom_dist_wtd_sub_pixel_avg_variance64x64 neon ssse3/;
  specialize qw/aom_dist_wtd_sub_pixel_avg_variance64x32 neon ssse3/;
  specialize qw/aom_dist_wtd_sub_pixel_avg_variance32x64 neon ssse3/;
  specialize qw/aom_dist_wtd_sub_pixel_avg_variance32x32 neon ssse3/;
  specialize qw/aom_dist_wtd_sub_pixel_avg_variance32x16 neon ssse3/;
  specialize qw/aom_dist_wtd_sub_pixel_avg_variance16x32 neon ssse3/;
  specialize qw/aom_dist_wtd_sub_pixel_avg_variance16x16 neon ssse3/;
  specialize qw/aom_dist_wtd_sub_pixel_avg_variance16x8  neon ssse3/;
  specialize qw/aom_dist_wtd_sub_pixel_avg_variance8x16  neon ssse3/;
  specialize qw/aom_dist_wtd_sub_pixel_avg_variance8x8   neon ssse3/;
  specialize qw/aom_dist_wtd_sub_pixel_avg_variance8x4   neon ssse3/;
  specialize qw/aom_dist_wtd_sub_pixel_avg_variance4x8   neon ssse3/;
  specialize qw/aom_dist_wtd_sub_pixel_avg_variance4x4   neon ssse3/;

  specialize qw/aom_dist_wtd_sub_pixel_avg_variance128x128  neon ssse3/;
  specialize qw/aom_dist_wtd_sub_pixel_avg_variance128x64   neon ssse3/;
  specialize qw/aom_dist_wtd_sub_pixel_avg_variance64x128   neon ssse3/;

  if (aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
    foreach $bd (8, 10, 12) {
      foreach (@encoder_block_sizes) {
        ($w, $h) = @$_;
        add_proto qw/unsigned int/, "aom_highbd_${bd}_variance${w}x${h}", "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
        add_proto qw/uint32_t/, "aom_highbd_${bd}_sub_pixel_variance${w}x${h}", "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
        add_proto qw/uint32_t/, "aom_highbd_${bd}_sub_pixel_avg_variance${w}x${h}", "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
        add_proto qw/uint32_t/, "aom_highbd_${bd}_dist_wtd_sub_pixel_avg_variance${w}x${h}", "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const DIST_WTD_COMP_PARAMS* jcp_param";
      }
    }

    specialize qw/aom_highbd_12_variance128x128 sse2 neon sve/;
    specialize qw/aom_highbd_12_variance128x64  sse2 neon sve/;
    specialize qw/aom_highbd_12_variance64x128  sse2 neon sve/;
    specialize qw/aom_highbd_12_variance64x64   sse2 neon sve/;
    specialize qw/aom_highbd_12_variance64x32   sse2 neon sve/;
    specialize qw/aom_highbd_12_variance32x64   sse2 neon sve/;
    specialize qw/aom_highbd_12_variance32x32   sse2 neon sve/;
    specialize qw/aom_highbd_12_variance32x16   sse2 neon sve/;
    specialize qw/aom_highbd_12_variance16x32   sse2 neon sve/;
    specialize qw/aom_highbd_12_variance16x16   sse2 neon sve/;
    specialize qw/aom_highbd_12_variance16x8    sse2 neon sve/;
    specialize qw/aom_highbd_12_variance8x16    sse2 neon sve/;
    specialize qw/aom_highbd_12_variance8x8     sse2 neon sve/;
    specialize qw/aom_highbd_12_variance8x4          neon sve/;
    specialize qw/aom_highbd_12_variance4x8          neon sve/;
    specialize qw/aom_highbd_12_variance4x4   sse4_1 neon sve/;

    specialize qw/aom_highbd_10_variance128x128 sse2 avx2 neon sve/;
    specialize qw/aom_highbd_10_variance128x64  sse2 avx2 neon sve/;
    specialize qw/aom_highbd_10_variance64x128  sse2 avx2 neon sve/;
    specialize qw/aom_highbd_10_variance64x64   sse2 avx2 neon sve/;
    specialize qw/aom_highbd_10_variance64x32   sse2 avx2 neon sve/;
    specialize qw/aom_highbd_10_variance32x64   sse2 avx2 neon sve/;
    specialize qw/aom_highbd_10_variance32x32   sse2 avx2 neon sve/;
    specialize qw/aom_highbd_10_variance32x16   sse2 avx2 neon sve/;
    specialize qw/aom_highbd_10_variance16x32   sse2 avx2 neon sve/;
    specialize qw/aom_highbd_10_variance16x16   sse2 avx2 neon sve/;
    specialize qw/aom_highbd_10_variance16x8    sse2 avx2 neon sve/;
    specialize qw/aom_highbd_10_variance8x16    sse2 avx2 neon sve/;
    specialize qw/aom_highbd_10_variance8x8     sse2 avx2 neon sve/;
    specialize qw/aom_highbd_10_variance8x4               neon sve/;
    specialize qw/aom_highbd_10_variance4x8               neon sve/;
    specialize qw/aom_highbd_10_variance4x4   sse4_1      neon sve/;

    specialize qw/aom_highbd_8_variance128x128 sse2 neon sve/;
    specialize qw/aom_highbd_8_variance128x64  sse2 neon sve/;
    specialize qw/aom_highbd_8_variance64x128  sse2 neon sve/;
    specialize qw/aom_highbd_8_variance64x64   sse2 neon sve/;
    specialize qw/aom_highbd_8_variance64x32   sse2 neon sve/;
    specialize qw/aom_highbd_8_variance32x64   sse2 neon sve/;
    specialize qw/aom_highbd_8_variance32x32   sse2 neon sve/;
    specialize qw/aom_highbd_8_variance32x16   sse2 neon sve/;
    specialize qw/aom_highbd_8_variance16x32   sse2 neon sve/;
    specialize qw/aom_highbd_8_variance16x16   sse2 neon sve/;
    specialize qw/aom_highbd_8_variance16x8    sse2 neon sve/;
    specialize qw/aom_highbd_8_variance8x16    sse2 neon sve/;
    specialize qw/aom_highbd_8_variance8x8     sse2 neon sve/;
    specialize qw/aom_highbd_8_variance8x4          neon sve/;
    specialize qw/aom_highbd_8_variance4x8          neon sve/;
    specialize qw/aom_highbd_8_variance4x4   sse4_1 neon sve/;

    if (aom_config("CONFIG_REALTIME_ONLY") ne "yes") {
      foreach $bd (8, 10, 12) {
        my $avx2 = ($bd == 10) ? "avx2" : "";
        specialize "aom_highbd_${bd}_variance64x16" , $avx2, qw/sse2 neon sve/;
        specialize "aom_highbd_${bd}_variance32x8" , $avx2, qw/sse2 neon sve/;
        specialize "aom_highbd_${bd}_variance16x64" , $avx2, qw/sse2 neon sve/;
        specialize "aom_highbd_${bd}_variance16x4" , qw/neon sve/;
        specialize "aom_highbd_${bd}_variance8x32" , $avx2, qw/sse2 neon sve/;
        specialize "aom_highbd_${bd}_variance4x16" , qw/neon sve/;
      }
    }

    specialize qw/aom_highbd_12_sub_pixel_variance128x128 sse2 neon/;
    specialize qw/aom_highbd_12_sub_pixel_variance128x64  sse2 neon/;
    specialize qw/aom_highbd_12_sub_pixel_variance64x128  sse2 neon/;
    specialize qw/aom_highbd_12_sub_pixel_variance64x64   sse2 neon/;
    specialize qw/aom_highbd_12_sub_pixel_variance64x32   sse2 neon/;
    specialize qw/aom_highbd_12_sub_pixel_variance32x64   sse2 neon/;
    specialize qw/aom_highbd_12_sub_pixel_variance32x32   sse2 neon/;
    specialize qw/aom_highbd_12_sub_pixel_variance32x16   sse2 neon/;
    specialize qw/aom_highbd_12_sub_pixel_variance16x32   sse2 neon/;
    specialize qw/aom_highbd_12_sub_pixel_variance16x16   sse2 neon/;
    specialize qw/aom_highbd_12_sub_pixel_variance16x8    sse2 neon/;
    specialize qw/aom_highbd_12_sub_pixel_variance8x16    sse2 neon/;
    specialize qw/aom_highbd_12_sub_pixel_variance8x8     sse2 neon/;
    specialize qw/aom_highbd_12_sub_pixel_variance8x4     sse2 neon/;
    specialize qw/aom_highbd_12_sub_pixel_variance4x8          neon/;
    specialize qw/aom_highbd_12_sub_pixel_variance4x4   sse4_1 neon/;

    specialize qw/aom_highbd_10_sub_pixel_variance128x128 sse2 avx2 neon/;
    specialize qw/aom_highbd_10_sub_pixel_variance128x64  sse2 avx2 neon/;
    specialize qw/aom_highbd_10_sub_pixel_variance64x128  sse2 avx2 neon/;
    specialize qw/aom_highbd_10_sub_pixel_variance64x64   sse2 avx2 neon/;
    specialize qw/aom_highbd_10_sub_pixel_variance64x32   sse2 avx2 neon/;
    specialize qw/aom_highbd_10_sub_pixel_variance32x64   sse2 avx2 neon/;
    specialize qw/aom_highbd_10_sub_pixel_variance32x32   sse2 avx2 neon/;
    specialize qw/aom_highbd_10_sub_pixel_variance32x16   sse2 avx2 neon/;
    specialize qw/aom_highbd_10_sub_pixel_variance16x32   sse2 avx2 neon/;
    specialize qw/aom_highbd_10_sub_pixel_variance16x16   sse2 avx2 neon/;
    specialize qw/aom_highbd_10_sub_pixel_variance16x8    sse2 avx2 neon/;
    specialize qw/aom_highbd_10_sub_pixel_variance8x16    sse2 avx2 neon/;
    specialize qw/aom_highbd_10_sub_pixel_variance8x8     sse2 avx2 neon/;
    specialize qw/aom_highbd_10_sub_pixel_variance8x4     sse2      neon/;
    specialize qw/aom_highbd_10_sub_pixel_variance4x8               neon/;
    specialize qw/aom_highbd_10_sub_pixel_variance4x4   sse4_1      neon/;

    specialize qw/aom_highbd_8_sub_pixel_variance128x128 sse2 neon/;
    specialize qw/aom_highbd_8_sub_pixel_variance128x64  sse2 neon/;
    specialize qw/aom_highbd_8_sub_pixel_variance64x128  sse2 neon/;
    specialize qw/aom_highbd_8_sub_pixel_variance64x64   sse2 neon/;
    specialize qw/aom_highbd_8_sub_pixel_variance64x32   sse2 neon/;
    specialize qw/aom_highbd_8_sub_pixel_variance32x64   sse2 neon/;
    specialize qw/aom_highbd_8_sub_pixel_variance32x32   sse2 neon/;
    specialize qw/aom_highbd_8_sub_pixel_variance32x16   sse2 neon/;
    specialize qw/aom_highbd_8_sub_pixel_variance16x32   sse2 neon/;
    specialize qw/aom_highbd_8_sub_pixel_variance16x16   sse2 neon/;
    specialize qw/aom_highbd_8_sub_pixel_variance16x8    sse2 neon/;
    specialize qw/aom_highbd_8_sub_pixel_variance8x16    sse2 neon/;
    specialize qw/aom_highbd_8_sub_pixel_variance8x8     sse2 neon/;
    specialize qw/aom_highbd_8_sub_pixel_variance8x4     sse2 neon/;
    specialize qw/aom_highbd_8_sub_pixel_variance4x8          neon/;
    specialize qw/aom_highbd_8_sub_pixel_variance4x4   sse4_1 neon/;

    if (aom_config("CONFIG_REALTIME_ONLY") ne "yes") {
      foreach $bd (8, 10, 12) {
        specialize "aom_highbd_${bd}_sub_pixel_variance64x16" , qw/sse2 neon/;
        specialize "aom_highbd_${bd}_sub_pixel_variance32x8" , qw/sse2 neon/;
        specialize "aom_highbd_${bd}_sub_pixel_variance16x64" , qw/sse2 neon/;
        specialize "aom_highbd_${bd}_sub_pixel_variance16x4" , qw/sse2 neon/;
        specialize "aom_highbd_${bd}_sub_pixel_variance8x32" , qw/sse2 neon/;
        specialize "aom_highbd_${bd}_sub_pixel_variance4x16" , qw/neon/;
      }
    }

    specialize qw/aom_highbd_12_sub_pixel_avg_variance128x128      neon/;
    specialize qw/aom_highbd_12_sub_pixel_avg_variance128x64       neon/;
    specialize qw/aom_highbd_12_sub_pixel_avg_variance64x128       neon/;
    specialize qw/aom_highbd_12_sub_pixel_avg_variance64x64   sse2 neon/;
    specialize qw/aom_highbd_12_sub_pixel_avg_variance64x32   sse2 neon/;
    specialize qw/aom_highbd_12_sub_pixel_avg_variance32x64   sse2 neon/;
    specialize qw/aom_highbd_12_sub_pixel_avg_variance32x32   sse2 neon/;
    specialize qw/aom_highbd_12_sub_pixel_avg_variance32x16   sse2 neon/;
    specialize qw/aom_highbd_12_sub_pixel_avg_variance16x32   sse2 neon/;
    specialize qw/aom_highbd_12_sub_pixel_avg_variance16x16   sse2 neon/;
    specialize qw/aom_highbd_12_sub_pixel_avg_variance16x8    sse2 neon/;
    specialize qw/aom_highbd_12_sub_pixel_avg_variance8x16    sse2 neon/;
    specialize qw/aom_highbd_12_sub_pixel_avg_variance8x8     sse2 neon/;
    specialize qw/aom_highbd_12_sub_pixel_avg_variance8x4     sse2 neon/;
    specialize qw/aom_highbd_12_sub_pixel_avg_variance4x8          neon/;
    specialize qw/aom_highbd_12_sub_pixel_avg_variance4x4   sse4_1 neon/;

    specialize qw/aom_highbd_10_sub_pixel_avg_variance128x128      neon/;
    specialize qw/aom_highbd_10_sub_pixel_avg_variance128x64       neon/;
    specialize qw/aom_highbd_10_sub_pixel_avg_variance64x128       neon/;
    specialize qw/aom_highbd_10_sub_pixel_avg_variance64x64   sse2 neon/;
    specialize qw/aom_highbd_10_sub_pixel_avg_variance64x32   sse2 neon/;
    specialize qw/aom_highbd_10_sub_pixel_avg_variance32x64   sse2 neon/;
    specialize qw/aom_highbd_10_sub_pixel_avg_variance32x32   sse2 neon/;
    specialize qw/aom_highbd_10_sub_pixel_avg_variance32x16   sse2 neon/;
    specialize qw/aom_highbd_10_sub_pixel_avg_variance16x32   sse2 neon/;
    specialize qw/aom_highbd_10_sub_pixel_avg_variance16x16   sse2 neon/;
    specialize qw/aom_highbd_10_sub_pixel_avg_variance16x8    sse2 neon/;
    specialize qw/aom_highbd_10_sub_pixel_avg_variance8x16    sse2 neon/;
    specialize qw/aom_highbd_10_sub_pixel_avg_variance8x8     sse2 neon/;
    specialize qw/aom_highbd_10_sub_pixel_avg_variance8x4     sse2 neon/;
    specialize qw/aom_highbd_10_sub_pixel_avg_variance4x8          neon/;
    specialize qw/aom_highbd_10_sub_pixel_avg_variance4x4   sse4_1 neon/;

    specialize qw/aom_highbd_8_sub_pixel_avg_variance128x128      neon/;
    specialize qw/aom_highbd_8_sub_pixel_avg_variance128x64       neon/;
    specialize qw/aom_highbd_8_sub_pixel_avg_variance64x128       neon/;
    specialize qw/aom_highbd_8_sub_pixel_avg_variance64x64   sse2 neon/;
    specialize qw/aom_highbd_8_sub_pixel_avg_variance64x32   sse2 neon/;
    specialize qw/aom_highbd_8_sub_pixel_avg_variance32x64   sse2 neon/;
    specialize qw/aom_highbd_8_sub_pixel_avg_variance32x32   sse2 neon/;
    specialize qw/aom_highbd_8_sub_pixel_avg_variance32x16   sse2 neon/;
    specialize qw/aom_highbd_8_sub_pixel_avg_variance16x32   sse2 neon/;
    specialize qw/aom_highbd_8_sub_pixel_avg_variance16x16   sse2 neon/;
    specialize qw/aom_highbd_8_sub_pixel_avg_variance16x8    sse2 neon/;
    specialize qw/aom_highbd_8_sub_pixel_avg_variance8x16    sse2 neon/;
    specialize qw/aom_highbd_8_sub_pixel_avg_variance8x8     sse2 neon/;
    specialize qw/aom_highbd_8_sub_pixel_avg_variance8x4     sse2 neon/;
    specialize qw/aom_highbd_8_sub_pixel_avg_variance4x8          neon/;
    specialize qw/aom_highbd_8_sub_pixel_avg_variance4x4   sse4_1 neon/;

    if (aom_config("CONFIG_REALTIME_ONLY") ne "yes") {
      foreach $bd (8, 10, 12) {
        specialize "aom_highbd_${bd}_sub_pixel_avg_variance64x16" , qw/sse2 neon/;
        specialize "aom_highbd_${bd}_sub_pixel_avg_variance32x8" , qw/sse2 neon/;
        specialize "aom_highbd_${bd}_sub_pixel_avg_variance16x64" , qw/sse2 neon/;
        specialize "aom_highbd_${bd}_sub_pixel_avg_variance16x4" , qw/sse2 neon/;
        specialize "aom_highbd_${bd}_sub_pixel_avg_variance8x32" , qw/sse2 neon/;
        specialize "aom_highbd_${bd}_sub_pixel_avg_variance4x16" , qw/neon/;
      }
    }

    foreach $bd (8, 10, 12) {
      specialize "aom_highbd_${bd}_dist_wtd_sub_pixel_avg_variance128x128", qw/neon/;
      specialize "aom_highbd_${bd}_dist_wtd_sub_pixel_avg_variance128x64" , qw/neon/;
      specialize "aom_highbd_${bd}_dist_wtd_sub_pixel_avg_variance64x128" , qw/neon/;
      specialize "aom_highbd_${bd}_dist_wtd_sub_pixel_avg_variance64x64"  , qw/neon/;
      specialize "aom_highbd_${bd}_dist_wtd_sub_pixel_avg_variance64x32"  , qw/neon/;
      specialize "aom_highbd_${bd}_dist_wtd_sub_pixel_avg_variance32x64"  , qw/neon/;
      specialize "aom_highbd_${bd}_dist_wtd_sub_pixel_avg_variance32x32"  , qw/neon/;
      specialize "aom_highbd_${bd}_dist_wtd_sub_pixel_avg_variance32x16"  , qw/neon/;
      specialize "aom_highbd_${bd}_dist_wtd_sub_pixel_avg_variance16x32"  , qw/neon/;
      specialize "aom_highbd_${bd}_dist_wtd_sub_pixel_avg_variance16x16"  , qw/neon/;
      specialize "aom_highbd_${bd}_dist_wtd_sub_pixel_avg_variance16x8"   , qw/neon/;
      specialize "aom_highbd_${bd}_dist_wtd_sub_pixel_avg_variance8x16"   , qw/neon/;
      specialize "aom_highbd_${bd}_dist_wtd_sub_pixel_avg_variance8x8"    , qw/neon/;
      specialize "aom_highbd_${bd}_dist_wtd_sub_pixel_avg_variance8x4"    , qw/neon/;
      specialize "aom_highbd_${bd}_dist_wtd_sub_pixel_avg_variance4x8"    , qw/neon/;
      specialize "aom_highbd_${bd}_dist_wtd_sub_pixel_avg_variance4x4"    , qw/neon/;
    }

    if (aom_config("CONFIG_REALTIME_ONLY") ne "yes") {
      foreach $bd (8, 10, 12) {
        specialize "aom_highbd_${bd}_dist_wtd_sub_pixel_avg_variance64x16", qw/neon/;
        specialize "aom_highbd_${bd}_dist_wtd_sub_pixel_avg_variance32x8" , qw/neon/;
        specialize "aom_highbd_${bd}_dist_wtd_sub_pixel_avg_variance16x64", qw/neon/;
        specialize "aom_highbd_${bd}_dist_wtd_sub_pixel_avg_variance16x4" , qw/neon/;
        specialize "aom_highbd_${bd}_dist_wtd_sub_pixel_avg_variance8x32" , qw/neon/;
        specialize "aom_highbd_${bd}_dist_wtd_sub_pixel_avg_variance4x16" , qw/neon/;
      }
    }
  }
  #
  # Masked Variance / Masked Subpixel Variance
  #
  foreach (@encoder_block_sizes) {
    ($w, $h) = @$_;
    add_proto qw/unsigned int/, "aom_masked_sub_pixel_variance${w}x${h}", "const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse";
    specialize "aom_masked_sub_pixel_variance${w}x${h}", qw/ssse3 neon/;
  }

  if (aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
    foreach $bd ("_8_", "_10_", "_12_") {
      foreach (@encoder_block_sizes) {
        ($w, $h) = @$_;
        add_proto qw/unsigned int/, "aom_highbd${bd}masked_sub_pixel_variance${w}x${h}", "const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse";
        specialize "aom_highbd${bd}masked_sub_pixel_variance${w}x${h}", qw/ssse3 neon/;
      }
    }
  }

  #
  # OBMC Variance / OBMC Subpixel Variance
  #
  if (aom_config("CONFIG_REALTIME_ONLY") ne "yes") {
    foreach (@encoder_block_sizes) {
      ($w, $h) = @$_;
      add_proto qw/unsigned int/, "aom_obmc_variance${w}x${h}", "const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse";
      add_proto qw/unsigned int/, "aom_obmc_sub_pixel_variance${w}x${h}", "const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse";
      specialize "aom_obmc_variance${w}x${h}", qw/sse4_1 avx2 neon/;
      specialize "aom_obmc_sub_pixel_variance${w}x${h}", qw/sse4_1 neon/;
    }

    if (aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
      foreach $bd ("_8_", "_10_", "_12_") {
        foreach (@encoder_block_sizes) {
          ($w, $h) = @$_;
          add_proto qw/unsigned int/, "aom_highbd${bd}obmc_variance${w}x${h}", "const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse";
          add_proto qw/unsigned int/, "aom_highbd${bd}obmc_sub_pixel_variance${w}x${h}", "const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse";
          specialize "aom_highbd${bd}obmc_variance${w}x${h}", qw/sse4_1 neon/;
          specialize "aom_highbd${bd}obmc_sub_pixel_variance${w}x${h}", qw/neon/;
        }
      }
    }
  }

  #
  # Comp Avg
  #
  add_proto qw/void aom_comp_avg_pred/, "uint8_t *comp_pred, const uint8_t *pred, int width, int height, const uint8_t *ref, int ref_stride";
  specialize qw/aom_comp_avg_pred avx2 neon/;

  add_proto qw/void aom_dist_wtd_comp_avg_pred/, "uint8_t *comp_pred, const uint8_t *pred, int width, int height, const uint8_t *ref, int ref_stride, const DIST_WTD_COMP_PARAMS *jcp_param";
  specialize qw/aom_dist_wtd_comp_avg_pred ssse3 neon/;

  if (aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
    add_proto qw/void aom_highbd_comp_avg_pred/, "uint8_t *comp_pred8, const uint8_t *pred8, int width, int height, const uint8_t *ref8, int ref_stride";
    specialize qw/aom_highbd_comp_avg_pred neon/;

    add_proto qw/void aom_highbd_dist_wtd_comp_avg_pred/, "uint8_t *comp_pred8, const uint8_t *pred8, int width, int height, const uint8_t *ref8, int ref_stride, const DIST_WTD_COMP_PARAMS *jcp_param";
    specialize qw/aom_highbd_dist_wtd_comp_avg_pred sse2 neon/;

    add_proto qw/uint64_t/, "aom_mse_wxh_16bit_highbd", "uint16_t *dst, int dstride,uint16_t *src, int sstride, int w, int h";
    specialize qw/aom_mse_wxh_16bit_highbd   sse2 avx2 neon sve/;
  }

  add_proto qw/void aom_comp_mask_pred/, "uint8_t *comp_pred, const uint8_t *pred, int width, int height, const uint8_t *ref, int ref_stride, const uint8_t *mask, int mask_stride, int invert_mask";
  specialize qw/aom_comp_mask_pred ssse3 avx2 neon/;

  if (aom_config("CONFIG_AV1_HIGHBITDEPTH") eq "yes") {
    add_proto qw/void aom_highbd_comp_mask_pred/, "uint8_t *comp_pred, const uint8_t *pred8, int width, int height, const uint8_t *ref8, int ref_stride, const uint8_t *mask, int mask_stride, int invert_mask";
    specialize qw/aom_highbd_comp_mask_pred sse2 avx2 neon/;
  }

  # Flow estimation library
  if (aom_config("CONFIG_REALTIME_ONLY") ne "yes") {
    add_proto qw/bool aom_compute_mean_stddev/, "const unsigned char *frame, int stride, int x, int y, double *mean, double *one_over_stddev";
    specialize qw/aom_compute_mean_stddev sse4_1 avx2/;

    add_proto qw/double aom_compute_correlation/, "const unsigned char *frame1, int stride1, int x1, int y1, double mean1, double one_over_stddev1, const unsigned char *frame2, int stride2, int x2, int y2, double mean2, double one_over_stddev2";
    specialize qw/aom_compute_correlation sse4_1 avx2/;

    add_proto qw/void aom_compute_flow_at_point/, "const uint8_t *src, const uint8_t *ref, int x, int y, int width, int height, int stride, double *u, double *v";
    specialize qw/aom_compute_flow_at_point sse4_1 avx2 neon sve/;
  }

}  # CONFIG_AV1_ENCODER

1;
