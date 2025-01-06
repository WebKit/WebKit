/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "test/av1_txfm_test.h"

#include <stdio.h>

#include <memory>
#include <new>

namespace libaom_test {

const char *tx_type_name[] = {
  "DCT_DCT",
  "ADST_DCT",
  "DCT_ADST",
  "ADST_ADST",
  "FLIPADST_DCT",
  "DCT_FLIPADST",
  "FLIPADST_FLIPADST",
  "ADST_FLIPADST",
  "FLIPADST_ADST",
  "IDTX",
  "V_DCT",
  "H_DCT",
  "V_ADST",
  "H_ADST",
  "V_FLIPADST",
  "H_FLIPADST",
};

int get_txfm1d_size(TX_SIZE tx_size) { return tx_size_wide[tx_size]; }

void get_txfm1d_type(TX_TYPE txfm2d_type, TYPE_TXFM *type0, TYPE_TXFM *type1) {
  switch (txfm2d_type) {
    case DCT_DCT:
      *type0 = TYPE_DCT;
      *type1 = TYPE_DCT;
      break;
    case ADST_DCT:
      *type0 = TYPE_ADST;
      *type1 = TYPE_DCT;
      break;
    case DCT_ADST:
      *type0 = TYPE_DCT;
      *type1 = TYPE_ADST;
      break;
    case ADST_ADST:
      *type0 = TYPE_ADST;
      *type1 = TYPE_ADST;
      break;
    case FLIPADST_DCT:
      *type0 = TYPE_ADST;
      *type1 = TYPE_DCT;
      break;
    case DCT_FLIPADST:
      *type0 = TYPE_DCT;
      *type1 = TYPE_ADST;
      break;
    case FLIPADST_FLIPADST:
      *type0 = TYPE_ADST;
      *type1 = TYPE_ADST;
      break;
    case ADST_FLIPADST:
      *type0 = TYPE_ADST;
      *type1 = TYPE_ADST;
      break;
    case FLIPADST_ADST:
      *type0 = TYPE_ADST;
      *type1 = TYPE_ADST;
      break;
    case IDTX:
      *type0 = TYPE_IDTX;
      *type1 = TYPE_IDTX;
      break;
    case H_DCT:
      *type0 = TYPE_IDTX;
      *type1 = TYPE_DCT;
      break;
    case V_DCT:
      *type0 = TYPE_DCT;
      *type1 = TYPE_IDTX;
      break;
    case H_ADST:
      *type0 = TYPE_IDTX;
      *type1 = TYPE_ADST;
      break;
    case V_ADST:
      *type0 = TYPE_ADST;
      *type1 = TYPE_IDTX;
      break;
    case H_FLIPADST:
      *type0 = TYPE_IDTX;
      *type1 = TYPE_ADST;
      break;
    case V_FLIPADST:
      *type0 = TYPE_ADST;
      *type1 = TYPE_IDTX;
      break;
    default:
      *type0 = TYPE_DCT;
      *type1 = TYPE_DCT;
      assert(0);
      break;
  }
}

double Sqrt2 = pow(2, 0.5);
double invSqrt2 = 1 / pow(2, 0.5);

static double dct_matrix(double n, double k, int size) {
  return cos(PI * (2 * n + 1) * k / (2 * size));
}

void reference_dct_1d(const double *in, double *out, int size) {
  for (int k = 0; k < size; ++k) {
    out[k] = 0;
    for (int n = 0; n < size; ++n) {
      out[k] += in[n] * dct_matrix(n, k, size);
    }
    if (k == 0) out[k] = out[k] * invSqrt2;
  }
}

void reference_idct_1d(const double *in, double *out, int size) {
  for (int k = 0; k < size; ++k) {
    out[k] = 0;
    for (int n = 0; n < size; ++n) {
      if (n == 0)
        out[k] += invSqrt2 * in[n] * dct_matrix(k, n, size);
      else
        out[k] += in[n] * dct_matrix(k, n, size);
    }
  }
}

// TODO(any): Copied from the old 'fadst4' (same as the new 'av1_fadst4'
// function). Should be replaced by a proper reference function that takes
// 'double' input & output.
static void fadst4_new(const tran_low_t *input, tran_low_t *output) {
  tran_high_t x0, x1, x2, x3;
  tran_high_t s0, s1, s2, s3, s4, s5, s6, s7;

  x0 = input[0];
  x1 = input[1];
  x2 = input[2];
  x3 = input[3];

  if (!(x0 | x1 | x2 | x3)) {
    output[0] = output[1] = output[2] = output[3] = 0;
    return;
  }

  s0 = sinpi_1_9 * x0;
  s1 = sinpi_4_9 * x0;
  s2 = sinpi_2_9 * x1;
  s3 = sinpi_1_9 * x1;
  s4 = sinpi_3_9 * x2;
  s5 = sinpi_4_9 * x3;
  s6 = sinpi_2_9 * x3;
  s7 = x0 + x1 - x3;

  x0 = s0 + s2 + s5;
  x1 = sinpi_3_9 * s7;
  x2 = s1 - s3 + s6;
  x3 = s4;

  s0 = x0 + x3;
  s1 = x1;
  s2 = x2 - x3;
  s3 = x2 - x0 + x3;

  // 1-D transform scaling factor is sqrt(2).
  output[0] = (tran_low_t)fdct_round_shift(s0);
  output[1] = (tran_low_t)fdct_round_shift(s1);
  output[2] = (tran_low_t)fdct_round_shift(s2);
  output[3] = (tran_low_t)fdct_round_shift(s3);
}

void reference_adst_1d(const double *in, double *out, int size) {
  if (size == 4) {  // Special case.
    tran_low_t int_input[4];
    for (int i = 0; i < 4; ++i) {
      int_input[i] = static_cast<tran_low_t>(round(in[i]));
    }
    tran_low_t int_output[4];
    fadst4_new(int_input, int_output);
    for (int i = 0; i < 4; ++i) {
      out[i] = int_output[i];
    }
    return;
  }

  for (int k = 0; k < size; ++k) {
    out[k] = 0;
    for (int n = 0; n < size; ++n) {
      out[k] += in[n] * sin(PI * (2 * n + 1) * (2 * k + 1) / (4 * size));
    }
  }
}

static void reference_idtx_1d(const double *in, double *out, int size) {
  double scale = 0;
  if (size == 4)
    scale = Sqrt2;
  else if (size == 8)
    scale = 2;
  else if (size == 16)
    scale = 2 * Sqrt2;
  else if (size == 32)
    scale = 4;
  else if (size == 64)
    scale = 4 * Sqrt2;
  for (int k = 0; k < size; ++k) {
    out[k] = in[k] * scale;
  }
}

void reference_hybrid_1d(double *in, double *out, int size, int type) {
  if (type == TYPE_DCT)
    reference_dct_1d(in, out, size);
  else if (type == TYPE_ADST)
    reference_adst_1d(in, out, size);
  else
    reference_idtx_1d(in, out, size);
}

double get_amplification_factor(TX_TYPE tx_type, TX_SIZE tx_size) {
  TXFM_2D_FLIP_CFG fwd_txfm_flip_cfg;
  av1_get_fwd_txfm_cfg(tx_type, tx_size, &fwd_txfm_flip_cfg);
  const int tx_width = tx_size_wide[fwd_txfm_flip_cfg.tx_size];
  const int tx_height = tx_size_high[fwd_txfm_flip_cfg.tx_size];
  const int8_t *shift = fwd_txfm_flip_cfg.shift;
  const int amplify_bit = shift[0] + shift[1] + shift[2];
  double amplify_factor =
      amplify_bit >= 0 ? (1 << amplify_bit) : (1.0 / (1 << -amplify_bit));

  // For rectangular transforms, we need to multiply by an extra factor.
  const int rect_type = get_rect_tx_log_ratio(tx_width, tx_height);
  if (abs(rect_type) == 1) {
    amplify_factor *= pow(2, 0.5);
  }
  return amplify_factor;
}

void reference_hybrid_2d(double *in, double *out, TX_TYPE tx_type,
                         TX_SIZE tx_size) {
  // Get transform type and size of each dimension.
  TYPE_TXFM type0;
  TYPE_TXFM type1;
  get_txfm1d_type(tx_type, &type0, &type1);
  const int tx_width = tx_size_wide[tx_size];
  const int tx_height = tx_size_high[tx_size];

  std::unique_ptr<double[]> temp_in(
      new (std::nothrow) double[AOMMAX(tx_width, tx_height)]);
  std::unique_ptr<double[]> temp_out(
      new (std::nothrow) double[AOMMAX(tx_width, tx_height)]);
  std::unique_ptr<double[]> out_interm(
      new (std::nothrow) double[tx_width * tx_height]);
  ASSERT_NE(temp_in, nullptr);
  ASSERT_NE(temp_out, nullptr);
  ASSERT_NE(out_interm, nullptr);

  // Transform columns.
  for (int c = 0; c < tx_width; ++c) {
    for (int r = 0; r < tx_height; ++r) {
      temp_in[r] = in[r * tx_width + c];
    }
    reference_hybrid_1d(temp_in.get(), temp_out.get(), tx_height, type0);
    for (int r = 0; r < tx_height; ++r) {
      out_interm[r * tx_width + c] = temp_out[r];
    }
  }

  // Transform rows.
  for (int r = 0; r < tx_height; ++r) {
    reference_hybrid_1d(out_interm.get() + r * tx_width, temp_out.get(),
                        tx_width, type1);
    for (int c = 0; c < tx_width; ++c) {
      out[c * tx_height + r] = temp_out[c];
    }
  }

  // These transforms use an approximate 2D DCT transform, by only keeping the
  // top-left quarter of the coefficients, and repacking them in the first
  // quarter indices.
  // TODO(urvang): Refactor this code.
  if (tx_width == 64 && tx_height == 64) {  // tx_size == TX_64X64
    // Zero out top-right 32x32 area.
    for (int col = 0; col < 32; ++col) {
      memset(out + col * 64 + 32, 0, 32 * sizeof(*out));
    }
    // Zero out the bottom 64x32 area.
    memset(out + 32 * 64, 0, 32 * 64 * sizeof(*out));
    // Re-pack non-zero coeffs in the first 32x32 indices.
    for (int col = 1; col < 32; ++col) {
      memcpy(out + col * 32, out + col * 64, 32 * sizeof(*out));
    }
  } else if (tx_width == 32 && tx_height == 64) {  // tx_size == TX_32X64
    // Zero out right 32x32 area.
    for (int col = 0; col < 32; ++col) {
      memset(out + col * 64 + 32, 0, 32 * sizeof(*out));
    }
    // Re-pack non-zero coeffs in the first 32x32 indices.
    for (int col = 1; col < 32; ++col) {
      memcpy(out + col * 32, out + col * 64, 32 * sizeof(*out));
    }
  } else if (tx_width == 64 && tx_height == 32) {  // tx_size == TX_64X32
    // Zero out the bottom 32x32 area.
    memset(out + 32 * 32, 0, 32 * 32 * sizeof(*out));
    // Note: no repacking needed here.
  } else if (tx_width == 16 && tx_height == 64) {  // tx_size == TX_16X64
    // Note: no repacking needed here.
    // Zero out right 32x16 area.
    for (int col = 0; col < 16; ++col) {
      memset(out + col * 64 + 32, 0, 32 * sizeof(*out));
    }
    // Re-pack non-zero coeffs in the first 32x16 indices.
    for (int col = 1; col < 16; ++col) {
      memcpy(out + col * 32, out + col * 64, 32 * sizeof(*out));
    }
  } else if (tx_width == 64 && tx_height == 16) {  // tx_size == TX_64X16
    // Zero out the bottom 16x32 area.
    memset(out + 16 * 32, 0, 16 * 32 * sizeof(*out));
  }

  // Apply appropriate scale.
  const double amplify_factor = get_amplification_factor(tx_type, tx_size);
  for (int c = 0; c < tx_width; ++c) {
    for (int r = 0; r < tx_height; ++r) {
      out[c * tx_height + r] *= amplify_factor;
    }
  }
}

template <typename Type>
void fliplr(Type *dest, int width, int height, int stride) {
  for (int r = 0; r < height; ++r) {
    for (int c = 0; c < width / 2; ++c) {
      const Type tmp = dest[r * stride + c];
      dest[r * stride + c] = dest[r * stride + width - 1 - c];
      dest[r * stride + width - 1 - c] = tmp;
    }
  }
}

template <typename Type>
void flipud(Type *dest, int width, int height, int stride) {
  for (int c = 0; c < width; ++c) {
    for (int r = 0; r < height / 2; ++r) {
      const Type tmp = dest[r * stride + c];
      dest[r * stride + c] = dest[(height - 1 - r) * stride + c];
      dest[(height - 1 - r) * stride + c] = tmp;
    }
  }
}

template <typename Type>
void fliplrud(Type *dest, int width, int height, int stride) {
  for (int r = 0; r < height / 2; ++r) {
    for (int c = 0; c < width; ++c) {
      const Type tmp = dest[r * stride + c];
      dest[r * stride + c] = dest[(height - 1 - r) * stride + width - 1 - c];
      dest[(height - 1 - r) * stride + width - 1 - c] = tmp;
    }
  }
}

template void fliplr<double>(double *dest, int width, int height, int stride);
template void flipud<double>(double *dest, int width, int height, int stride);
template void fliplrud<double>(double *dest, int width, int height, int stride);

int bd_arr[BD_NUM] = { 8, 10, 12 };

int8_t low_range_arr[BD_NUM] = { 18, 32, 32 };
int8_t high_range_arr[BD_NUM] = { 32, 32, 32 };

void txfm_stage_range_check(const int8_t *stage_range, int stage_num,
                            int8_t cos_bit, int low_range, int high_range) {
  for (int i = 0; i < stage_num; ++i) {
    EXPECT_LE(stage_range[i], low_range);
    ASSERT_LE(stage_range[i] + cos_bit, high_range) << "stage = " << i;
  }
  for (int i = 0; i < stage_num - 1; ++i) {
    // make sure there is no overflow while doing half_btf()
    ASSERT_LE(stage_range[i + 1] + cos_bit, high_range) << "stage = " << i;
  }
}
}  // namespace libaom_test
