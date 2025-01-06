/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/mathutils.h"
#include "aom_dsp/noise_model.h"
#include "aom_dsp/noise_util.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"
#include "aom_scale/yv12config.h"

#define kLowPolyNumParams 3

static const int kMaxLag = 4;

// Defines a function that can be used to obtain the mean of a block for the
// provided data type (uint8_t, or uint16_t)
#define GET_BLOCK_MEAN(INT_TYPE, suffix)                                    \
  static double get_block_mean_##suffix(const INT_TYPE *data, int w, int h, \
                                        int stride, int x_o, int y_o,       \
                                        int block_size) {                   \
    const int max_h = AOMMIN(h - y_o, block_size);                          \
    const int max_w = AOMMIN(w - x_o, block_size);                          \
    double block_mean = 0;                                                  \
    for (int y = 0; y < max_h; ++y) {                                       \
      for (int x = 0; x < max_w; ++x) {                                     \
        block_mean += data[(y_o + y) * stride + x_o + x];                   \
      }                                                                     \
    }                                                                       \
    return block_mean / (max_w * max_h);                                    \
  }

GET_BLOCK_MEAN(uint8_t, lowbd)
GET_BLOCK_MEAN(uint16_t, highbd)

static inline double get_block_mean(const uint8_t *data, int w, int h,
                                    int stride, int x_o, int y_o,
                                    int block_size, int use_highbd) {
  if (use_highbd)
    return get_block_mean_highbd((const uint16_t *)data, w, h, stride, x_o, y_o,
                                 block_size);
  return get_block_mean_lowbd(data, w, h, stride, x_o, y_o, block_size);
}

// Defines a function that can be used to obtain the variance of a block
// for the provided data type (uint8_t, or uint16_t)
#define GET_NOISE_VAR(INT_TYPE, suffix)                                  \
  static double get_noise_var_##suffix(                                  \
      const INT_TYPE *data, const INT_TYPE *denoised, int stride, int w, \
      int h, int x_o, int y_o, int block_size_x, int block_size_y) {     \
    const int max_h = AOMMIN(h - y_o, block_size_y);                     \
    const int max_w = AOMMIN(w - x_o, block_size_x);                     \
    double noise_var = 0;                                                \
    double noise_mean = 0;                                               \
    for (int y = 0; y < max_h; ++y) {                                    \
      for (int x = 0; x < max_w; ++x) {                                  \
        double noise = (double)data[(y_o + y) * stride + x_o + x] -      \
                       denoised[(y_o + y) * stride + x_o + x];           \
        noise_mean += noise;                                             \
        noise_var += noise * noise;                                      \
      }                                                                  \
    }                                                                    \
    noise_mean /= (max_w * max_h);                                       \
    return noise_var / (max_w * max_h) - noise_mean * noise_mean;        \
  }

GET_NOISE_VAR(uint8_t, lowbd)
GET_NOISE_VAR(uint16_t, highbd)

static inline double get_noise_var(const uint8_t *data, const uint8_t *denoised,
                                   int w, int h, int stride, int x_o, int y_o,
                                   int block_size_x, int block_size_y,
                                   int use_highbd) {
  if (use_highbd)
    return get_noise_var_highbd((const uint16_t *)data,
                                (const uint16_t *)denoised, w, h, stride, x_o,
                                y_o, block_size_x, block_size_y);
  return get_noise_var_lowbd(data, denoised, w, h, stride, x_o, y_o,
                             block_size_x, block_size_y);
}

static void equation_system_clear(aom_equation_system_t *eqns) {
  const int n = eqns->n;
  memset(eqns->A, 0, sizeof(*eqns->A) * n * n);
  memset(eqns->x, 0, sizeof(*eqns->x) * n);
  memset(eqns->b, 0, sizeof(*eqns->b) * n);
}

static void equation_system_copy(aom_equation_system_t *dst,
                                 const aom_equation_system_t *src) {
  const int n = dst->n;
  memcpy(dst->A, src->A, sizeof(*dst->A) * n * n);
  memcpy(dst->x, src->x, sizeof(*dst->x) * n);
  memcpy(dst->b, src->b, sizeof(*dst->b) * n);
}

static int equation_system_init(aom_equation_system_t *eqns, int n) {
  eqns->A = (double *)aom_malloc(sizeof(*eqns->A) * n * n);
  eqns->b = (double *)aom_malloc(sizeof(*eqns->b) * n);
  eqns->x = (double *)aom_malloc(sizeof(*eqns->x) * n);
  eqns->n = n;
  if (!eqns->A || !eqns->b || !eqns->x) {
    fprintf(stderr, "Failed to allocate system of equations of size %d\n", n);
    aom_free(eqns->A);
    aom_free(eqns->b);
    aom_free(eqns->x);
    memset(eqns, 0, sizeof(*eqns));
    return 0;
  }
  equation_system_clear(eqns);
  return 1;
}

static int equation_system_solve(aom_equation_system_t *eqns) {
  const int n = eqns->n;
  double *b = (double *)aom_malloc(sizeof(*b) * n);
  double *A = (double *)aom_malloc(sizeof(*A) * n * n);
  int ret = 0;
  if (A == NULL || b == NULL) {
    fprintf(stderr, "Unable to allocate temp values of size %dx%d\n", n, n);
    aom_free(b);
    aom_free(A);
    return 0;
  }
  memcpy(A, eqns->A, sizeof(*eqns->A) * n * n);
  memcpy(b, eqns->b, sizeof(*eqns->b) * n);
  ret = linsolve(n, A, eqns->n, b, eqns->x);
  aom_free(b);
  aom_free(A);

  if (ret == 0) {
    return 0;
  }
  return 1;
}

static void equation_system_add(aom_equation_system_t *dest,
                                aom_equation_system_t *src) {
  const int n = dest->n;
  int i, j;
  for (i = 0; i < n; ++i) {
    for (j = 0; j < n; ++j) {
      dest->A[i * n + j] += src->A[i * n + j];
    }
    dest->b[i] += src->b[i];
  }
}

static void equation_system_free(aom_equation_system_t *eqns) {
  if (!eqns) return;
  aom_free(eqns->A);
  aom_free(eqns->b);
  aom_free(eqns->x);
  memset(eqns, 0, sizeof(*eqns));
}

static void noise_strength_solver_clear(aom_noise_strength_solver_t *solver) {
  equation_system_clear(&solver->eqns);
  solver->num_equations = 0;
  solver->total = 0;
}

static void noise_strength_solver_add(aom_noise_strength_solver_t *dest,
                                      aom_noise_strength_solver_t *src) {
  equation_system_add(&dest->eqns, &src->eqns);
  dest->num_equations += src->num_equations;
  dest->total += src->total;
}

// Return the number of coefficients required for the given parameters
static int num_coeffs(const aom_noise_model_params_t params) {
  const int n = 2 * params.lag + 1;
  switch (params.shape) {
    case AOM_NOISE_SHAPE_DIAMOND: return params.lag * (params.lag + 1);
    case AOM_NOISE_SHAPE_SQUARE: return (n * n) / 2;
  }
  return 0;
}

static int noise_state_init(aom_noise_state_t *state, int n, int bit_depth) {
  const int kNumBins = 20;
  if (!equation_system_init(&state->eqns, n)) {
    fprintf(stderr, "Failed initialization noise state with size %d\n", n);
    return 0;
  }
  state->ar_gain = 1.0;
  state->num_observations = 0;
  return aom_noise_strength_solver_init(&state->strength_solver, kNumBins,
                                        bit_depth);
}

static void set_chroma_coefficient_fallback_soln(aom_equation_system_t *eqns) {
  const double kTolerance = 1e-6;
  const int last = eqns->n - 1;
  // Set all of the AR coefficients to zero, but try to solve for correlation
  // with the luma channel
  memset(eqns->x, 0, sizeof(*eqns->x) * eqns->n);
  if (fabs(eqns->A[last * eqns->n + last]) > kTolerance) {
    eqns->x[last] = eqns->b[last] / eqns->A[last * eqns->n + last];
  }
}

int aom_noise_strength_lut_init(aom_noise_strength_lut_t *lut, int num_points) {
  if (!lut) return 0;
  if (num_points <= 0) return 0;
  lut->num_points = 0;
  lut->points = (double(*)[2])aom_malloc(num_points * sizeof(*lut->points));
  if (!lut->points) return 0;
  lut->num_points = num_points;
  memset(lut->points, 0, sizeof(*lut->points) * num_points);
  return 1;
}

void aom_noise_strength_lut_free(aom_noise_strength_lut_t *lut) {
  if (!lut) return;
  aom_free(lut->points);
  memset(lut, 0, sizeof(*lut));
}

double aom_noise_strength_lut_eval(const aom_noise_strength_lut_t *lut,
                                   double x) {
  int i = 0;
  // Constant extrapolation for x <  x_0.
  if (x < lut->points[0][0]) return lut->points[0][1];
  for (i = 0; i < lut->num_points - 1; ++i) {
    if (x >= lut->points[i][0] && x <= lut->points[i + 1][0]) {
      const double a =
          (x - lut->points[i][0]) / (lut->points[i + 1][0] - lut->points[i][0]);
      return lut->points[i + 1][1] * a + lut->points[i][1] * (1.0 - a);
    }
  }
  // Constant extrapolation for x > x_{n-1}
  return lut->points[lut->num_points - 1][1];
}

static double noise_strength_solver_get_bin_index(
    const aom_noise_strength_solver_t *solver, double value) {
  const double val =
      fclamp(value, solver->min_intensity, solver->max_intensity);
  const double range = solver->max_intensity - solver->min_intensity;
  return (solver->num_bins - 1) * (val - solver->min_intensity) / range;
}

static double noise_strength_solver_get_value(
    const aom_noise_strength_solver_t *solver, double x) {
  const double bin = noise_strength_solver_get_bin_index(solver, x);
  const int bin_i0 = (int)floor(bin);
  const int bin_i1 = AOMMIN(solver->num_bins - 1, bin_i0 + 1);
  const double a = bin - bin_i0;
  return (1.0 - a) * solver->eqns.x[bin_i0] + a * solver->eqns.x[bin_i1];
}

void aom_noise_strength_solver_add_measurement(
    aom_noise_strength_solver_t *solver, double block_mean, double noise_std) {
  const double bin = noise_strength_solver_get_bin_index(solver, block_mean);
  const int bin_i0 = (int)floor(bin);
  const int bin_i1 = AOMMIN(solver->num_bins - 1, bin_i0 + 1);
  const double a = bin - bin_i0;
  const int n = solver->num_bins;
  solver->eqns.A[bin_i0 * n + bin_i0] += (1.0 - a) * (1.0 - a);
  solver->eqns.A[bin_i1 * n + bin_i0] += a * (1.0 - a);
  solver->eqns.A[bin_i1 * n + bin_i1] += a * a;
  solver->eqns.A[bin_i0 * n + bin_i1] += a * (1.0 - a);
  solver->eqns.b[bin_i0] += (1.0 - a) * noise_std;
  solver->eqns.b[bin_i1] += a * noise_std;
  solver->total += noise_std;
  solver->num_equations++;
}

int aom_noise_strength_solver_solve(aom_noise_strength_solver_t *solver) {
  // Add regularization proportional to the number of constraints
  const int n = solver->num_bins;
  const double kAlpha = 2.0 * (double)(solver->num_equations) / n;
  int result = 0;
  double mean = 0;

  // Do this in a non-destructive manner so it is not confusing to the caller
  double *old_A = solver->eqns.A;
  double *A = (double *)aom_malloc(sizeof(*A) * n * n);
  if (!A) {
    fprintf(stderr, "Unable to allocate copy of A\n");
    return 0;
  }
  memcpy(A, old_A, sizeof(*A) * n * n);

  for (int i = 0; i < n; ++i) {
    const int i_lo = AOMMAX(0, i - 1);
    const int i_hi = AOMMIN(n - 1, i + 1);
    A[i * n + i_lo] -= kAlpha;
    A[i * n + i] += 2 * kAlpha;
    A[i * n + i_hi] -= kAlpha;
  }

  // Small regularization to give average noise strength
  mean = solver->total / solver->num_equations;
  for (int i = 0; i < n; ++i) {
    A[i * n + i] += 1.0 / 8192.;
    solver->eqns.b[i] += mean / 8192.;
  }
  solver->eqns.A = A;
  result = equation_system_solve(&solver->eqns);
  solver->eqns.A = old_A;

  aom_free(A);
  return result;
}

int aom_noise_strength_solver_init(aom_noise_strength_solver_t *solver,
                                   int num_bins, int bit_depth) {
  if (!solver) return 0;
  memset(solver, 0, sizeof(*solver));
  solver->num_bins = num_bins;
  solver->min_intensity = 0;
  solver->max_intensity = (1 << bit_depth) - 1;
  solver->total = 0;
  solver->num_equations = 0;
  return equation_system_init(&solver->eqns, num_bins);
}

void aom_noise_strength_solver_free(aom_noise_strength_solver_t *solver) {
  if (!solver) return;
  equation_system_free(&solver->eqns);
}

double aom_noise_strength_solver_get_center(
    const aom_noise_strength_solver_t *solver, int i) {
  const double range = solver->max_intensity - solver->min_intensity;
  const int n = solver->num_bins;
  return ((double)i) / (n - 1) * range + solver->min_intensity;
}

// Computes the residual if a point were to be removed from the lut. This is
// calculated as the area between the output of the solver and the line segment
// that would be formed between [x_{i - 1}, x_{i + 1}).
static void update_piecewise_linear_residual(
    const aom_noise_strength_solver_t *solver,
    const aom_noise_strength_lut_t *lut, double *residual, int start, int end) {
  const double dx = 255. / solver->num_bins;
  for (int i = AOMMAX(start, 1); i < AOMMIN(end, lut->num_points - 1); ++i) {
    const int lower = AOMMAX(0, (int)floor(noise_strength_solver_get_bin_index(
                                    solver, lut->points[i - 1][0])));
    const int upper = AOMMIN(solver->num_bins - 1,
                             (int)ceil(noise_strength_solver_get_bin_index(
                                 solver, lut->points[i + 1][0])));
    double r = 0;
    for (int j = lower; j <= upper; ++j) {
      const double x = aom_noise_strength_solver_get_center(solver, j);
      if (x < lut->points[i - 1][0]) continue;
      if (x >= lut->points[i + 1][0]) continue;
      const double y = solver->eqns.x[j];
      const double a = (x - lut->points[i - 1][0]) /
                       (lut->points[i + 1][0] - lut->points[i - 1][0]);
      const double estimate_y =
          lut->points[i - 1][1] * (1.0 - a) + lut->points[i + 1][1] * a;
      r += fabs(y - estimate_y);
    }
    residual[i] = r * dx;
  }
}

int aom_noise_strength_solver_fit_piecewise(
    const aom_noise_strength_solver_t *solver, int max_output_points,
    aom_noise_strength_lut_t *lut) {
  // The tolerance is normalized to be give consistent results between
  // different bit-depths.
  const double kTolerance = solver->max_intensity * 0.00625 / 255.0;
  if (!aom_noise_strength_lut_init(lut, solver->num_bins)) {
    fprintf(stderr, "Failed to init lut\n");
    return 0;
  }
  for (int i = 0; i < solver->num_bins; ++i) {
    lut->points[i][0] = aom_noise_strength_solver_get_center(solver, i);
    lut->points[i][1] = solver->eqns.x[i];
  }
  if (max_output_points < 0) {
    max_output_points = solver->num_bins;
  }

  double *residual = (double *)aom_malloc(solver->num_bins * sizeof(*residual));
  if (!residual) {
    aom_noise_strength_lut_free(lut);
    return 0;
  }
  memset(residual, 0, sizeof(*residual) * solver->num_bins);

  update_piecewise_linear_residual(solver, lut, residual, 0, solver->num_bins);

  // Greedily remove points if there are too many or if it doesn't hurt local
  // approximation (never remove the end points)
  while (lut->num_points > 2) {
    int min_index = 1;
    for (int j = 1; j < lut->num_points - 1; ++j) {
      if (residual[j] < residual[min_index]) {
        min_index = j;
      }
    }
    const double dx =
        lut->points[min_index + 1][0] - lut->points[min_index - 1][0];
    const double avg_residual = residual[min_index] / dx;
    if (lut->num_points <= max_output_points && avg_residual > kTolerance) {
      break;
    }

    const int num_remaining = lut->num_points - min_index - 1;
    memmove(lut->points + min_index, lut->points + min_index + 1,
            sizeof(lut->points[0]) * num_remaining);
    lut->num_points--;

    update_piecewise_linear_residual(solver, lut, residual, min_index - 1,
                                     min_index + 1);
  }
  aom_free(residual);
  return 1;
}

int aom_flat_block_finder_init(aom_flat_block_finder_t *block_finder,
                               int block_size, int bit_depth, int use_highbd) {
  const int n = block_size * block_size;
  aom_equation_system_t eqns;
  double *AtA_inv = 0;
  double *A = 0;
  int x = 0, y = 0, i = 0, j = 0;
  block_finder->A = NULL;
  block_finder->AtA_inv = NULL;

  if (!equation_system_init(&eqns, kLowPolyNumParams)) {
    fprintf(stderr, "Failed to init equation system for block_size=%d\n",
            block_size);
    return 0;
  }

  AtA_inv = (double *)aom_malloc(kLowPolyNumParams * kLowPolyNumParams *
                                 sizeof(*AtA_inv));
  A = (double *)aom_malloc(kLowPolyNumParams * n * sizeof(*A));
  if (AtA_inv == NULL || A == NULL) {
    fprintf(stderr, "Failed to alloc A or AtA_inv for block_size=%d\n",
            block_size);
    aom_free(AtA_inv);
    aom_free(A);
    equation_system_free(&eqns);
    return 0;
  }

  block_finder->A = A;
  block_finder->AtA_inv = AtA_inv;
  block_finder->block_size = block_size;
  block_finder->normalization = (1 << bit_depth) - 1;
  block_finder->use_highbd = use_highbd;

  for (y = 0; y < block_size; ++y) {
    const double yd = ((double)y - block_size / 2.) / (block_size / 2.);
    for (x = 0; x < block_size; ++x) {
      const double xd = ((double)x - block_size / 2.) / (block_size / 2.);
      const double coords[3] = { yd, xd, 1 };
      const int row = y * block_size + x;
      A[kLowPolyNumParams * row + 0] = yd;
      A[kLowPolyNumParams * row + 1] = xd;
      A[kLowPolyNumParams * row + 2] = 1;

      for (i = 0; i < kLowPolyNumParams; ++i) {
        for (j = 0; j < kLowPolyNumParams; ++j) {
          eqns.A[kLowPolyNumParams * i + j] += coords[i] * coords[j];
        }
      }
    }
  }

  // Lazy inverse using existing equation solver.
  for (i = 0; i < kLowPolyNumParams; ++i) {
    memset(eqns.b, 0, sizeof(*eqns.b) * kLowPolyNumParams);
    eqns.b[i] = 1;
    equation_system_solve(&eqns);

    for (j = 0; j < kLowPolyNumParams; ++j) {
      AtA_inv[j * kLowPolyNumParams + i] = eqns.x[j];
    }
  }
  equation_system_free(&eqns);
  return 1;
}

void aom_flat_block_finder_free(aom_flat_block_finder_t *block_finder) {
  if (!block_finder) return;
  aom_free(block_finder->A);
  aom_free(block_finder->AtA_inv);
  memset(block_finder, 0, sizeof(*block_finder));
}

void aom_flat_block_finder_extract_block(
    const aom_flat_block_finder_t *block_finder, const uint8_t *const data,
    int w, int h, int stride, int offsx, int offsy, double *plane,
    double *block) {
  const int block_size = block_finder->block_size;
  const int n = block_size * block_size;
  const double *A = block_finder->A;
  const double *AtA_inv = block_finder->AtA_inv;
  double plane_coords[kLowPolyNumParams];
  double AtA_inv_b[kLowPolyNumParams];
  int xi, yi, i;

  if (block_finder->use_highbd) {
    const uint16_t *const data16 = (const uint16_t *const)data;
    for (yi = 0; yi < block_size; ++yi) {
      const int y = clamp(offsy + yi, 0, h - 1);
      for (xi = 0; xi < block_size; ++xi) {
        const int x = clamp(offsx + xi, 0, w - 1);
        block[yi * block_size + xi] =
            ((double)data16[y * stride + x]) / block_finder->normalization;
      }
    }
  } else {
    for (yi = 0; yi < block_size; ++yi) {
      const int y = clamp(offsy + yi, 0, h - 1);
      for (xi = 0; xi < block_size; ++xi) {
        const int x = clamp(offsx + xi, 0, w - 1);
        block[yi * block_size + xi] =
            ((double)data[y * stride + x]) / block_finder->normalization;
      }
    }
  }
  multiply_mat(block, A, AtA_inv_b, 1, n, kLowPolyNumParams);
  multiply_mat(AtA_inv, AtA_inv_b, plane_coords, kLowPolyNumParams,
               kLowPolyNumParams, 1);
  multiply_mat(A, plane_coords, plane, n, kLowPolyNumParams, 1);

  for (i = 0; i < n; ++i) {
    block[i] -= plane[i];
  }
}

typedef struct {
  int index;
  float score;
} index_and_score_t;

static int compare_scores(const void *a, const void *b) {
  const float diff =
      ((index_and_score_t *)a)->score - ((index_and_score_t *)b)->score;
  if (diff < 0)
    return -1;
  else if (diff > 0)
    return 1;
  return 0;
}

int aom_flat_block_finder_run(const aom_flat_block_finder_t *block_finder,
                              const uint8_t *const data, int w, int h,
                              int stride, uint8_t *flat_blocks) {
  // The gradient-based features used in this code are based on:
  //  A. Kokaram, D. Kelly, H. Denman and A. Crawford, "Measuring noise
  //  correlation for improved video denoising," 2012 19th, ICIP.
  // The thresholds are more lenient to allow for correct grain modeling
  // if extreme cases.
  const int block_size = block_finder->block_size;
  const int n = block_size * block_size;
  const double kTraceThreshold = 0.15 / (32 * 32);
  const double kRatioThreshold = 1.25;
  const double kNormThreshold = 0.08 / (32 * 32);
  const double kVarThreshold = 0.005 / (double)n;
  const int num_blocks_w = (w + block_size - 1) / block_size;
  const int num_blocks_h = (h + block_size - 1) / block_size;
  int num_flat = 0;
  double *plane = (double *)aom_malloc(n * sizeof(*plane));
  double *block = (double *)aom_malloc(n * sizeof(*block));
  index_and_score_t *scores = (index_and_score_t *)aom_malloc(
      num_blocks_w * num_blocks_h * sizeof(*scores));
  if (plane == NULL || block == NULL || scores == NULL) {
    fprintf(stderr, "Failed to allocate memory for block of size %d\n", n);
    aom_free(plane);
    aom_free(block);
    aom_free(scores);
    return -1;
  }

#ifdef NOISE_MODEL_LOG_SCORE
  fprintf(stderr, "score = [");
#endif
  for (int by = 0; by < num_blocks_h; ++by) {
    for (int bx = 0; bx < num_blocks_w; ++bx) {
      // Compute gradient covariance matrix.
      aom_flat_block_finder_extract_block(block_finder, data, w, h, stride,
                                          bx * block_size, by * block_size,
                                          plane, block);
      double Gxx = 0, Gxy = 0, Gyy = 0;
      double mean = 0;
      double var = 0;

      for (int yi = 1; yi < block_size - 1; ++yi) {
        for (int xi = 1; xi < block_size - 1; ++xi) {
          const double gx = (block[yi * block_size + xi + 1] -
                             block[yi * block_size + xi - 1]) /
                            2;
          const double gy = (block[yi * block_size + xi + block_size] -
                             block[yi * block_size + xi - block_size]) /
                            2;
          Gxx += gx * gx;
          Gxy += gx * gy;
          Gyy += gy * gy;

          const double value = block[yi * block_size + xi];
          mean += value;
          var += value * value;
        }
      }
      mean /= (block_size - 2) * (block_size - 2);

      // Normalize gradients by block_size.
      Gxx /= ((block_size - 2) * (block_size - 2));
      Gxy /= ((block_size - 2) * (block_size - 2));
      Gyy /= ((block_size - 2) * (block_size - 2));
      var = var / ((block_size - 2) * (block_size - 2)) - mean * mean;

      {
        const double trace = Gxx + Gyy;
        const double det = Gxx * Gyy - Gxy * Gxy;
        const double e1 = (trace + sqrt(trace * trace - 4 * det)) / 2.;
        const double e2 = (trace - sqrt(trace * trace - 4 * det)) / 2.;
        const double norm = e1;  // Spectral norm
        const double ratio = (e1 / AOMMAX(e2, 1e-6));
        const int is_flat = (trace < kTraceThreshold) &&
                            (ratio < kRatioThreshold) &&
                            (norm < kNormThreshold) && (var > kVarThreshold);
        // The following weights are used to combine the above features to give
        // a sigmoid score for flatness. If the input was normalized to [0,100]
        // the magnitude of these values would be close to 1 (e.g., weights
        // corresponding to variance would be a factor of 10000x smaller).
        // The weights are given in the following order:
        //    [{var}, {ratio}, {trace}, {norm}, offset]
        // with one of the most discriminative being simply the variance.
        const double weights[5] = { -6682, -0.2056, 13087, -12434, 2.5694 };
        double sum_weights = weights[0] * var + weights[1] * ratio +
                             weights[2] * trace + weights[3] * norm +
                             weights[4];
        // clamp the value to [-25.0, 100.0] to prevent overflow
        sum_weights = fclamp(sum_weights, -25.0, 100.0);
        const float score = (float)(1.0 / (1 + exp(-sum_weights)));
        flat_blocks[by * num_blocks_w + bx] = is_flat ? 255 : 0;
        scores[by * num_blocks_w + bx].score = var > kVarThreshold ? score : 0;
        scores[by * num_blocks_w + bx].index = by * num_blocks_w + bx;
#ifdef NOISE_MODEL_LOG_SCORE
        fprintf(stderr, "%g %g %g %g %g %d ", score, var, ratio, trace, norm,
                is_flat);
#endif
        num_flat += is_flat;
      }
    }
#ifdef NOISE_MODEL_LOG_SCORE
    fprintf(stderr, "\n");
#endif
  }
#ifdef NOISE_MODEL_LOG_SCORE
  fprintf(stderr, "];\n");
#endif
  // Find the top-scored blocks (most likely to be flat) and set the flat blocks
  // be the union of the thresholded results and the top 10th percentile of the
  // scored results.
  qsort(scores, num_blocks_w * num_blocks_h, sizeof(*scores), &compare_scores);
  const int top_nth_percentile = num_blocks_w * num_blocks_h * 90 / 100;
  const float score_threshold = scores[top_nth_percentile].score;
  for (int i = 0; i < num_blocks_w * num_blocks_h; ++i) {
    if (scores[i].score >= score_threshold) {
      num_flat += flat_blocks[scores[i].index] == 0;
      flat_blocks[scores[i].index] |= 1;
    }
  }
  aom_free(block);
  aom_free(plane);
  aom_free(scores);
  return num_flat;
}

int aom_noise_model_init(aom_noise_model_t *model,
                         const aom_noise_model_params_t params) {
  const int n = num_coeffs(params);
  const int lag = params.lag;
  const int bit_depth = params.bit_depth;
  int x = 0, y = 0, i = 0, c = 0;

  memset(model, 0, sizeof(*model));
  if (params.lag < 1) {
    fprintf(stderr, "Invalid noise param: lag = %d must be >= 1\n", params.lag);
    return 0;
  }
  if (params.lag > kMaxLag) {
    fprintf(stderr, "Invalid noise param: lag = %d must be <= %d\n", params.lag,
            kMaxLag);
    return 0;
  }
  if (!(params.bit_depth == 8 || params.bit_depth == 10 ||
        params.bit_depth == 12)) {
    return 0;
  }

  memcpy(&model->params, &params, sizeof(params));
  for (c = 0; c < 3; ++c) {
    if (!noise_state_init(&model->combined_state[c], n + (c > 0), bit_depth)) {
      fprintf(stderr, "Failed to allocate noise state for channel %d\n", c);
      aom_noise_model_free(model);
      return 0;
    }
    if (!noise_state_init(&model->latest_state[c], n + (c > 0), bit_depth)) {
      fprintf(stderr, "Failed to allocate noise state for channel %d\n", c);
      aom_noise_model_free(model);
      return 0;
    }
  }
  model->n = n;
  model->coords = (int(*)[2])aom_malloc(sizeof(*model->coords) * n);
  if (!model->coords) {
    aom_noise_model_free(model);
    return 0;
  }

  for (y = -lag; y <= 0; ++y) {
    const int max_x = y == 0 ? -1 : lag;
    for (x = -lag; x <= max_x; ++x) {
      switch (params.shape) {
        case AOM_NOISE_SHAPE_DIAMOND:
          if (abs(x) <= y + lag) {
            model->coords[i][0] = x;
            model->coords[i][1] = y;
            ++i;
          }
          break;
        case AOM_NOISE_SHAPE_SQUARE:
          model->coords[i][0] = x;
          model->coords[i][1] = y;
          ++i;
          break;
        default:
          fprintf(stderr, "Invalid shape\n");
          aom_noise_model_free(model);
          return 0;
      }
    }
  }
  assert(i == n);
  return 1;
}

void aom_noise_model_free(aom_noise_model_t *model) {
  int c = 0;
  if (!model) return;

  aom_free(model->coords);
  for (c = 0; c < 3; ++c) {
    equation_system_free(&model->latest_state[c].eqns);
    equation_system_free(&model->combined_state[c].eqns);

    equation_system_free(&model->latest_state[c].strength_solver.eqns);
    equation_system_free(&model->combined_state[c].strength_solver.eqns);
  }
  memset(model, 0, sizeof(*model));
}

// Extracts the neighborhood defined by coords around point (x, y) from
// the difference between the data and denoised images. Also extracts the
// entry (possibly downsampled) for (x, y) in the alt_data (e.g., luma).
#define EXTRACT_AR_ROW(INT_TYPE, suffix)                                   \
  static double extract_ar_row_##suffix(                                   \
      int(*coords)[2], int num_coords, const INT_TYPE *const data,         \
      const INT_TYPE *const denoised, int stride, int sub_log2[2],         \
      const INT_TYPE *const alt_data, const INT_TYPE *const alt_denoised,  \
      int alt_stride, int x, int y, double *buffer) {                      \
    for (int i = 0; i < num_coords; ++i) {                                 \
      const int x_i = x + coords[i][0], y_i = y + coords[i][1];            \
      buffer[i] =                                                          \
          (double)data[y_i * stride + x_i] - denoised[y_i * stride + x_i]; \
    }                                                                      \
    const double val =                                                     \
        (double)data[y * stride + x] - denoised[y * stride + x];           \
                                                                           \
    if (alt_data && alt_denoised) {                                        \
      double avg_data = 0, avg_denoised = 0;                               \
      int num_samples = 0;                                                 \
      for (int dy_i = 0; dy_i < (1 << sub_log2[1]); dy_i++) {              \
        const int y_up = (y << sub_log2[1]) + dy_i;                        \
        for (int dx_i = 0; dx_i < (1 << sub_log2[0]); dx_i++) {            \
          const int x_up = (x << sub_log2[0]) + dx_i;                      \
          avg_data += alt_data[y_up * alt_stride + x_up];                  \
          avg_denoised += alt_denoised[y_up * alt_stride + x_up];          \
          num_samples++;                                                   \
        }                                                                  \
      }                                                                    \
      buffer[num_coords] = (avg_data - avg_denoised) / num_samples;        \
    }                                                                      \
    return val;                                                            \
  }

EXTRACT_AR_ROW(uint8_t, lowbd)
EXTRACT_AR_ROW(uint16_t, highbd)

static int add_block_observations(
    aom_noise_model_t *noise_model, int c, const uint8_t *const data,
    const uint8_t *const denoised, int w, int h, int stride, int sub_log2[2],
    const uint8_t *const alt_data, const uint8_t *const alt_denoised,
    int alt_stride, const uint8_t *const flat_blocks, int block_size,
    int num_blocks_w, int num_blocks_h) {
  const int lag = noise_model->params.lag;
  const int num_coords = noise_model->n;
  const double normalization = (1 << noise_model->params.bit_depth) - 1;
  double *A = noise_model->latest_state[c].eqns.A;
  double *b = noise_model->latest_state[c].eqns.b;
  double *buffer = (double *)aom_malloc(sizeof(*buffer) * (num_coords + 1));
  const int n = noise_model->latest_state[c].eqns.n;

  if (!buffer) {
    fprintf(stderr, "Unable to allocate buffer of size %d\n", num_coords + 1);
    return 0;
  }
  for (int by = 0; by < num_blocks_h; ++by) {
    const int y_o = by * (block_size >> sub_log2[1]);
    for (int bx = 0; bx < num_blocks_w; ++bx) {
      const int x_o = bx * (block_size >> sub_log2[0]);
      if (!flat_blocks[by * num_blocks_w + bx]) {
        continue;
      }
      int y_start =
          (by > 0 && flat_blocks[(by - 1) * num_blocks_w + bx]) ? 0 : lag;
      int x_start =
          (bx > 0 && flat_blocks[by * num_blocks_w + bx - 1]) ? 0 : lag;
      int y_end = AOMMIN((h >> sub_log2[1]) - by * (block_size >> sub_log2[1]),
                         block_size >> sub_log2[1]);
      int x_end = AOMMIN(
          (w >> sub_log2[0]) - bx * (block_size >> sub_log2[0]) - lag,
          (bx + 1 < num_blocks_w && flat_blocks[by * num_blocks_w + bx + 1])
              ? (block_size >> sub_log2[0])
              : ((block_size >> sub_log2[0]) - lag));
      for (int y = y_start; y < y_end; ++y) {
        for (int x = x_start; x < x_end; ++x) {
          const double val =
              noise_model->params.use_highbd
                  ? extract_ar_row_highbd(noise_model->coords, num_coords,
                                          (const uint16_t *const)data,
                                          (const uint16_t *const)denoised,
                                          stride, sub_log2,
                                          (const uint16_t *const)alt_data,
                                          (const uint16_t *const)alt_denoised,
                                          alt_stride, x + x_o, y + y_o, buffer)
                  : extract_ar_row_lowbd(noise_model->coords, num_coords, data,
                                         denoised, stride, sub_log2, alt_data,
                                         alt_denoised, alt_stride, x + x_o,
                                         y + y_o, buffer);
          for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
              A[i * n + j] +=
                  (buffer[i] * buffer[j]) / (normalization * normalization);
            }
            b[i] += (buffer[i] * val) / (normalization * normalization);
          }
          noise_model->latest_state[c].num_observations++;
        }
      }
    }
  }
  aom_free(buffer);
  return 1;
}

static void add_noise_std_observations(
    aom_noise_model_t *noise_model, int c, const double *coeffs,
    const uint8_t *const data, const uint8_t *const denoised, int w, int h,
    int stride, int sub_log2[2], const uint8_t *const alt_data, int alt_stride,
    const uint8_t *const flat_blocks, int block_size, int num_blocks_w,
    int num_blocks_h) {
  const int num_coords = noise_model->n;
  aom_noise_strength_solver_t *noise_strength_solver =
      &noise_model->latest_state[c].strength_solver;

  const aom_noise_strength_solver_t *noise_strength_luma =
      &noise_model->latest_state[0].strength_solver;
  const double luma_gain = noise_model->latest_state[0].ar_gain;
  const double noise_gain = noise_model->latest_state[c].ar_gain;
  for (int by = 0; by < num_blocks_h; ++by) {
    const int y_o = by * (block_size >> sub_log2[1]);
    for (int bx = 0; bx < num_blocks_w; ++bx) {
      const int x_o = bx * (block_size >> sub_log2[0]);
      if (!flat_blocks[by * num_blocks_w + bx]) {
        continue;
      }
      const int num_samples_h =
          AOMMIN((h >> sub_log2[1]) - by * (block_size >> sub_log2[1]),
                 block_size >> sub_log2[1]);
      const int num_samples_w =
          AOMMIN((w >> sub_log2[0]) - bx * (block_size >> sub_log2[0]),
                 (block_size >> sub_log2[0]));
      // Make sure that we have a reasonable amount of samples to consider the
      // block
      if (num_samples_w * num_samples_h > block_size) {
        const double block_mean = get_block_mean(
            alt_data ? alt_data : data, w, h, alt_data ? alt_stride : stride,
            x_o << sub_log2[0], y_o << sub_log2[1], block_size,
            noise_model->params.use_highbd);
        const double noise_var = get_noise_var(
            data, denoised, stride, w >> sub_log2[0], h >> sub_log2[1], x_o,
            y_o, block_size >> sub_log2[0], block_size >> sub_log2[1],
            noise_model->params.use_highbd);
        // We want to remove the part of the noise that came from being
        // correlated with luma. Note that the noise solver for luma must
        // have already been run.
        const double luma_strength =
            c > 0 ? luma_gain * noise_strength_solver_get_value(
                                    noise_strength_luma, block_mean)
                  : 0;
        const double corr = c > 0 ? coeffs[num_coords] : 0;
        // Chroma noise:
        //    N(0, noise_var) = N(0, uncorr_var) + corr * N(0, luma_strength^2)
        // The uncorrelated component:
        //   uncorr_var = noise_var - (corr * luma_strength)^2
        // But don't allow fully correlated noise (hence the max), since the
        // synthesis cannot model it.
        const double uncorr_std = sqrt(
            AOMMAX(noise_var / 16, noise_var - pow(corr * luma_strength, 2)));
        // After we've removed correlation with luma, undo the gain that will
        // come from running the IIR filter.
        const double adjusted_strength = uncorr_std / noise_gain;
        aom_noise_strength_solver_add_measurement(
            noise_strength_solver, block_mean, adjusted_strength);
      }
    }
  }
}

// Return true if the noise estimate appears to be different from the combined
// (multi-frame) estimate. The difference is measured by checking whether the
// AR coefficients have diverged (using a threshold on normalized cross
// correlation), or whether the noise strength has changed.
static int is_noise_model_different(aom_noise_model_t *const noise_model) {
  // These thresholds are kind of arbitrary and will likely need further tuning
  // (or exported as parameters). The threshold on noise strength is a weighted
  // difference between the noise strength histograms
  const double kCoeffThreshold = 0.9;
  const double kStrengthThreshold =
      0.005 * (1 << (noise_model->params.bit_depth - 8));
  for (int c = 0; c < 1; ++c) {
    const double corr =
        aom_normalized_cross_correlation(noise_model->latest_state[c].eqns.x,
                                         noise_model->combined_state[c].eqns.x,
                                         noise_model->combined_state[c].eqns.n);
    if (corr < kCoeffThreshold) return 1;

    const double dx =
        1.0 / noise_model->latest_state[c].strength_solver.num_bins;

    const aom_equation_system_t *latest_eqns =
        &noise_model->latest_state[c].strength_solver.eqns;
    const aom_equation_system_t *combined_eqns =
        &noise_model->combined_state[c].strength_solver.eqns;
    double diff = 0;
    double total_weight = 0;
    for (int j = 0; j < latest_eqns->n; ++j) {
      double weight = 0;
      for (int i = 0; i < latest_eqns->n; ++i) {
        weight += latest_eqns->A[i * latest_eqns->n + j];
      }
      weight = sqrt(weight);
      diff += weight * fabs(latest_eqns->x[j] - combined_eqns->x[j]);
      total_weight += weight;
    }
    if (diff * dx / total_weight > kStrengthThreshold) return 1;
  }
  return 0;
}

static int ar_equation_system_solve(aom_noise_state_t *state, int is_chroma) {
  const int ret = equation_system_solve(&state->eqns);
  state->ar_gain = 1.0;
  if (!ret) return ret;

  // Update the AR gain from the equation system as it will be used to fit
  // the noise strength as a function of intensity.  In the Yule-Walker
  // equations, the diagonal should be the variance of the correlated noise.
  // In the case of the least squares estimate, there will be some variability
  // in the diagonal. So use the mean of the diagonal as the estimate of
  // overall variance (this works for least squares or Yule-Walker formulation).
  double var = 0;
  const int n = state->eqns.n;
  for (int i = 0; i < (state->eqns.n - is_chroma); ++i) {
    var += state->eqns.A[i * n + i] / state->num_observations;
  }
  var /= (n - is_chroma);

  // Keep track of E(Y^2) = <b, x> + E(X^2)
  // In the case that we are using chroma and have an estimate of correlation
  // with luma we adjust that estimate slightly to remove the correlated bits by
  // subtracting out the last column of a scaled by our correlation estimate
  // from b. E(y^2) = <b - A(:, end)*x(end), x>
  double sum_covar = 0;
  for (int i = 0; i < state->eqns.n - is_chroma; ++i) {
    double bi = state->eqns.b[i];
    if (is_chroma) {
      bi -= state->eqns.A[i * n + (n - 1)] * state->eqns.x[n - 1];
    }
    sum_covar += (bi * state->eqns.x[i]) / state->num_observations;
  }
  // Now, get an estimate of the variance of uncorrelated noise signal and use
  // it to determine the gain of the AR filter.
  const double noise_var = AOMMAX(var - sum_covar, 1e-6);
  state->ar_gain = AOMMAX(1, sqrt(AOMMAX(var / noise_var, 1e-6)));
  return ret;
}

aom_noise_status_t aom_noise_model_update(
    aom_noise_model_t *const noise_model, const uint8_t *const data[3],
    const uint8_t *const denoised[3], int w, int h, int stride[3],
    int chroma_sub_log2[2], const uint8_t *const flat_blocks, int block_size) {
  const int num_blocks_w = (w + block_size - 1) / block_size;
  const int num_blocks_h = (h + block_size - 1) / block_size;
  int y_model_different = 0;
  int num_blocks = 0;
  int i = 0, channel = 0;

  if (block_size <= 1) {
    fprintf(stderr, "block_size = %d must be > 1\n", block_size);
    return AOM_NOISE_STATUS_INVALID_ARGUMENT;
  }

  if (block_size < noise_model->params.lag * 2 + 1) {
    fprintf(stderr, "block_size = %d must be >= %d\n", block_size,
            noise_model->params.lag * 2 + 1);
    return AOM_NOISE_STATUS_INVALID_ARGUMENT;
  }

  // Clear the latest equation system
  for (i = 0; i < 3; ++i) {
    equation_system_clear(&noise_model->latest_state[i].eqns);
    noise_model->latest_state[i].num_observations = 0;
    noise_strength_solver_clear(&noise_model->latest_state[i].strength_solver);
  }

  // Check that we have enough flat blocks
  for (i = 0; i < num_blocks_h * num_blocks_w; ++i) {
    if (flat_blocks[i]) {
      num_blocks++;
    }
  }

  if (num_blocks <= 1) {
    fprintf(stderr, "Not enough flat blocks to update noise estimate\n");
    return AOM_NOISE_STATUS_INSUFFICIENT_FLAT_BLOCKS;
  }

  for (channel = 0; channel < 3; ++channel) {
    int no_subsampling[2] = { 0, 0 };
    const uint8_t *alt_data = channel > 0 ? data[0] : 0;
    const uint8_t *alt_denoised = channel > 0 ? denoised[0] : 0;
    int *sub = channel > 0 ? chroma_sub_log2 : no_subsampling;
    const int is_chroma = channel != 0;
    if (!data[channel] || !denoised[channel]) break;
    if (!add_block_observations(noise_model, channel, data[channel],
                                denoised[channel], w, h, stride[channel], sub,
                                alt_data, alt_denoised, stride[0], flat_blocks,
                                block_size, num_blocks_w, num_blocks_h)) {
      fprintf(stderr, "Adding block observation failed\n");
      return AOM_NOISE_STATUS_INTERNAL_ERROR;
    }

    if (!ar_equation_system_solve(&noise_model->latest_state[channel],
                                  is_chroma)) {
      if (is_chroma) {
        set_chroma_coefficient_fallback_soln(
            &noise_model->latest_state[channel].eqns);
      } else {
        fprintf(stderr, "Solving latest noise equation system failed %d!\n",
                channel);
        return AOM_NOISE_STATUS_INTERNAL_ERROR;
      }
    }

    add_noise_std_observations(
        noise_model, channel, noise_model->latest_state[channel].eqns.x,
        data[channel], denoised[channel], w, h, stride[channel], sub, alt_data,
        stride[0], flat_blocks, block_size, num_blocks_w, num_blocks_h);

    if (!aom_noise_strength_solver_solve(
            &noise_model->latest_state[channel].strength_solver)) {
      fprintf(stderr, "Solving latest noise strength failed!\n");
      return AOM_NOISE_STATUS_INTERNAL_ERROR;
    }

    // Check noise characteristics and return if error.
    if (channel == 0 &&
        noise_model->combined_state[channel].strength_solver.num_equations >
            0 &&
        is_noise_model_different(noise_model)) {
      y_model_different = 1;
    }

    // Don't update the combined stats if the y model is different.
    if (y_model_different) continue;

    noise_model->combined_state[channel].num_observations +=
        noise_model->latest_state[channel].num_observations;
    equation_system_add(&noise_model->combined_state[channel].eqns,
                        &noise_model->latest_state[channel].eqns);
    if (!ar_equation_system_solve(&noise_model->combined_state[channel],
                                  is_chroma)) {
      if (is_chroma) {
        set_chroma_coefficient_fallback_soln(
            &noise_model->combined_state[channel].eqns);
      } else {
        fprintf(stderr, "Solving combined noise equation system failed %d!\n",
                channel);
        return AOM_NOISE_STATUS_INTERNAL_ERROR;
      }
    }

    noise_strength_solver_add(
        &noise_model->combined_state[channel].strength_solver,
        &noise_model->latest_state[channel].strength_solver);

    if (!aom_noise_strength_solver_solve(
            &noise_model->combined_state[channel].strength_solver)) {
      fprintf(stderr, "Solving combined noise strength failed!\n");
      return AOM_NOISE_STATUS_INTERNAL_ERROR;
    }
  }

  return y_model_different ? AOM_NOISE_STATUS_DIFFERENT_NOISE_TYPE
                           : AOM_NOISE_STATUS_OK;
}

void aom_noise_model_save_latest(aom_noise_model_t *noise_model) {
  for (int c = 0; c < 3; c++) {
    equation_system_copy(&noise_model->combined_state[c].eqns,
                         &noise_model->latest_state[c].eqns);
    equation_system_copy(&noise_model->combined_state[c].strength_solver.eqns,
                         &noise_model->latest_state[c].strength_solver.eqns);
    noise_model->combined_state[c].strength_solver.num_equations =
        noise_model->latest_state[c].strength_solver.num_equations;
    noise_model->combined_state[c].num_observations =
        noise_model->latest_state[c].num_observations;
    noise_model->combined_state[c].ar_gain =
        noise_model->latest_state[c].ar_gain;
  }
}

int aom_noise_model_get_grain_parameters(aom_noise_model_t *const noise_model,
                                         aom_film_grain_t *film_grain) {
  if (noise_model->params.lag > 3) {
    fprintf(stderr, "params.lag = %d > 3\n", noise_model->params.lag);
    return 0;
  }
  uint16_t random_seed = film_grain->random_seed;
  memset(film_grain, 0, sizeof(*film_grain));
  film_grain->random_seed = random_seed;

  film_grain->apply_grain = 1;
  film_grain->update_parameters = 1;

  film_grain->ar_coeff_lag = noise_model->params.lag;

  // Convert the scaling functions to 8 bit values
  aom_noise_strength_lut_t scaling_points[3];
  if (!aom_noise_strength_solver_fit_piecewise(
          &noise_model->combined_state[0].strength_solver, 14,
          scaling_points + 0)) {
    return 0;
  }
  if (!aom_noise_strength_solver_fit_piecewise(
          &noise_model->combined_state[1].strength_solver, 10,
          scaling_points + 1)) {
    aom_noise_strength_lut_free(scaling_points + 0);
    return 0;
  }
  if (!aom_noise_strength_solver_fit_piecewise(
          &noise_model->combined_state[2].strength_solver, 10,
          scaling_points + 2)) {
    aom_noise_strength_lut_free(scaling_points + 0);
    aom_noise_strength_lut_free(scaling_points + 1);
    return 0;
  }

  // Both the domain and the range of the scaling functions in the film_grain
  // are normalized to 8-bit (e.g., they are implicitly scaled during grain
  // synthesis).
  const double strength_divisor = 1 << (noise_model->params.bit_depth - 8);
  double max_scaling_value = 1e-4;
  for (int c = 0; c < 3; ++c) {
    for (int i = 0; i < scaling_points[c].num_points; ++i) {
      scaling_points[c].points[i][0] =
          AOMMIN(255, scaling_points[c].points[i][0] / strength_divisor);
      scaling_points[c].points[i][1] =
          AOMMIN(255, scaling_points[c].points[i][1] / strength_divisor);
      max_scaling_value =
          AOMMAX(scaling_points[c].points[i][1], max_scaling_value);
    }
  }

  // Scaling_shift values are in the range [8,11]
  const int max_scaling_value_log2 =
      clamp((int)floor(log2(max_scaling_value) + 1), 2, 5);
  film_grain->scaling_shift = 5 + (8 - max_scaling_value_log2);

  const double scale_factor = 1 << (8 - max_scaling_value_log2);
  film_grain->num_y_points = scaling_points[0].num_points;
  film_grain->num_cb_points = scaling_points[1].num_points;
  film_grain->num_cr_points = scaling_points[2].num_points;

  int(*film_grain_scaling[3])[2] = {
    film_grain->scaling_points_y,
    film_grain->scaling_points_cb,
    film_grain->scaling_points_cr,
  };
  for (int c = 0; c < 3; c++) {
    for (int i = 0; i < scaling_points[c].num_points; ++i) {
      film_grain_scaling[c][i][0] = (int)(scaling_points[c].points[i][0] + 0.5);
      film_grain_scaling[c][i][1] = clamp(
          (int)(scale_factor * scaling_points[c].points[i][1] + 0.5), 0, 255);
    }
  }
  aom_noise_strength_lut_free(scaling_points + 0);
  aom_noise_strength_lut_free(scaling_points + 1);
  aom_noise_strength_lut_free(scaling_points + 2);

  // Convert the ar_coeffs into 8-bit values
  const int n_coeff = noise_model->combined_state[0].eqns.n;
  double max_coeff = 1e-4, min_coeff = -1e-4;
  double y_corr[2] = { 0, 0 };
  double avg_luma_strength = 0;
  for (int c = 0; c < 3; c++) {
    aom_equation_system_t *eqns = &noise_model->combined_state[c].eqns;
    for (int i = 0; i < n_coeff; ++i) {
      max_coeff = AOMMAX(max_coeff, eqns->x[i]);
      min_coeff = AOMMIN(min_coeff, eqns->x[i]);
    }
    // Since the correlation between luma/chroma was computed in an already
    // scaled space, we adjust it in the un-scaled space.
    aom_noise_strength_solver_t *solver =
        &noise_model->combined_state[c].strength_solver;
    // Compute a weighted average of the strength for the channel.
    double average_strength = 0, total_weight = 0;
    for (int i = 0; i < solver->eqns.n; ++i) {
      double w = 0;
      for (int j = 0; j < solver->eqns.n; ++j) {
        w += solver->eqns.A[i * solver->eqns.n + j];
      }
      w = sqrt(w);
      average_strength += solver->eqns.x[i] * w;
      total_weight += w;
    }
    if (total_weight == 0)
      average_strength = 1;
    else
      average_strength /= total_weight;
    if (c == 0) {
      avg_luma_strength = average_strength;
    } else {
      y_corr[c - 1] = avg_luma_strength * eqns->x[n_coeff] / average_strength;
      max_coeff = AOMMAX(max_coeff, y_corr[c - 1]);
      min_coeff = AOMMIN(min_coeff, y_corr[c - 1]);
    }
  }
  // Shift value: AR coeffs range (values 6-9)
  // 6: [-2, 2),  7: [-1, 1), 8: [-0.5, 0.5), 9: [-0.25, 0.25)
  film_grain->ar_coeff_shift =
      clamp(7 - (int)AOMMAX(1 + floor(log2(max_coeff)), ceil(log2(-min_coeff))),
            6, 9);
  double scale_ar_coeff = 1 << film_grain->ar_coeff_shift;
  int *ar_coeffs[3] = {
    film_grain->ar_coeffs_y,
    film_grain->ar_coeffs_cb,
    film_grain->ar_coeffs_cr,
  };
  for (int c = 0; c < 3; ++c) {
    aom_equation_system_t *eqns = &noise_model->combined_state[c].eqns;
    for (int i = 0; i < n_coeff; ++i) {
      ar_coeffs[c][i] =
          clamp((int)round(scale_ar_coeff * eqns->x[i]), -128, 127);
    }
    if (c > 0) {
      ar_coeffs[c][n_coeff] =
          clamp((int)round(scale_ar_coeff * y_corr[c - 1]), -128, 127);
    }
  }

  // At the moment, the noise modeling code assumes that the chroma scaling
  // functions are a function of luma.
  film_grain->cb_mult = 128;       // 8 bits
  film_grain->cb_luma_mult = 192;  // 8 bits
  film_grain->cb_offset = 256;     // 9 bits

  film_grain->cr_mult = 128;       // 8 bits
  film_grain->cr_luma_mult = 192;  // 8 bits
  film_grain->cr_offset = 256;     // 9 bits

  film_grain->chroma_scaling_from_luma = 0;
  film_grain->grain_scale_shift = 0;
  film_grain->overlap_flag = 1;
  return 1;
}

static void pointwise_multiply(const float *a, float *b, int n) {
  for (int i = 0; i < n; ++i) {
    b[i] *= a[i];
  }
}

static float *get_half_cos_window(int block_size) {
  float *window_function =
      (float *)aom_malloc(block_size * block_size * sizeof(*window_function));
  if (!window_function) return NULL;
  for (int y = 0; y < block_size; ++y) {
    const double cos_yd = cos((.5 + y) * PI / block_size - PI / 2);
    for (int x = 0; x < block_size; ++x) {
      const double cos_xd = cos((.5 + x) * PI / block_size - PI / 2);
      window_function[y * block_size + x] = (float)(cos_yd * cos_xd);
    }
  }
  return window_function;
}

#define DITHER_AND_QUANTIZE(INT_TYPE, suffix)                               \
  static void dither_and_quantize_##suffix(                                 \
      float *result, int result_stride, INT_TYPE *denoised, int w, int h,   \
      int stride, int chroma_sub_w, int chroma_sub_h, int block_size,       \
      float block_normalization) {                                          \
    for (int y = 0; y < (h >> chroma_sub_h); ++y) {                         \
      for (int x = 0; x < (w >> chroma_sub_w); ++x) {                       \
        const int result_idx =                                              \
            (y + (block_size >> chroma_sub_h)) * result_stride + x +        \
            (block_size >> chroma_sub_w);                                   \
        INT_TYPE new_val = (INT_TYPE)AOMMIN(                                \
            AOMMAX(result[result_idx] * block_normalization + 0.5f, 0),     \
            block_normalization);                                           \
        const float err =                                                   \
            -(((float)new_val) / block_normalization - result[result_idx]); \
        denoised[y * stride + x] = new_val;                                 \
        if (x + 1 < (w >> chroma_sub_w)) {                                  \
          result[result_idx + 1] += err * 7.0f / 16.0f;                     \
        }                                                                   \
        if (y + 1 < (h >> chroma_sub_h)) {                                  \
          if (x > 0) {                                                      \
            result[result_idx + result_stride - 1] += err * 3.0f / 16.0f;   \
          }                                                                 \
          result[result_idx + result_stride] += err * 5.0f / 16.0f;         \
          if (x + 1 < (w >> chroma_sub_w)) {                                \
            result[result_idx + result_stride + 1] += err * 1.0f / 16.0f;   \
          }                                                                 \
        }                                                                   \
      }                                                                     \
    }                                                                       \
  }

DITHER_AND_QUANTIZE(uint8_t, lowbd)
DITHER_AND_QUANTIZE(uint16_t, highbd)

int aom_wiener_denoise_2d(const uint8_t *const data[3], uint8_t *denoised[3],
                          int w, int h, int stride[3], int chroma_sub[2],
                          float *noise_psd[3], int block_size, int bit_depth,
                          int use_highbd) {
  float *plane = NULL, *block = NULL, *window_full = NULL,
        *window_chroma = NULL;
  double *block_d = NULL, *plane_d = NULL;
  struct aom_noise_tx_t *tx_full = NULL;
  struct aom_noise_tx_t *tx_chroma = NULL;
  const int num_blocks_w = (w + block_size - 1) / block_size;
  const int num_blocks_h = (h + block_size - 1) / block_size;
  const int result_stride = (num_blocks_w + 2) * block_size;
  const int result_height = (num_blocks_h + 2) * block_size;
  float *result = NULL;
  int init_success = 1;
  aom_flat_block_finder_t block_finder_full;
  aom_flat_block_finder_t block_finder_chroma;
  const float kBlockNormalization = (float)((1 << bit_depth) - 1);
  if (chroma_sub[0] != chroma_sub[1]) {
    fprintf(stderr,
            "aom_wiener_denoise_2d doesn't handle different chroma "
            "subsampling\n");
    return 0;
  }
  init_success &= aom_flat_block_finder_init(&block_finder_full, block_size,
                                             bit_depth, use_highbd);
  result = (float *)aom_malloc((num_blocks_h + 2) * block_size * result_stride *
                               sizeof(*result));
  plane = (float *)aom_malloc(block_size * block_size * sizeof(*plane));
  block =
      (float *)aom_memalign(32, 2 * block_size * block_size * sizeof(*block));
  block_d = (double *)aom_malloc(block_size * block_size * sizeof(*block_d));
  plane_d = (double *)aom_malloc(block_size * block_size * sizeof(*plane_d));
  window_full = get_half_cos_window(block_size);
  tx_full = aom_noise_tx_malloc(block_size);

  if (chroma_sub[0] != 0) {
    init_success &= aom_flat_block_finder_init(&block_finder_chroma,
                                               block_size >> chroma_sub[0],
                                               bit_depth, use_highbd);
    window_chroma = get_half_cos_window(block_size >> chroma_sub[0]);
    tx_chroma = aom_noise_tx_malloc(block_size >> chroma_sub[0]);
  } else {
    window_chroma = window_full;
    tx_chroma = tx_full;
  }

  init_success &= (tx_full != NULL) && (tx_chroma != NULL) && (plane != NULL) &&
                  (plane_d != NULL) && (block != NULL) && (block_d != NULL) &&
                  (window_full != NULL) && (window_chroma != NULL) &&
                  (result != NULL);
  for (int c = init_success ? 0 : 3; c < 3; ++c) {
    float *window_function = c == 0 ? window_full : window_chroma;
    aom_flat_block_finder_t *block_finder = &block_finder_full;
    const int chroma_sub_h = c > 0 ? chroma_sub[1] : 0;
    const int chroma_sub_w = c > 0 ? chroma_sub[0] : 0;
    struct aom_noise_tx_t *tx =
        (c > 0 && chroma_sub[0] > 0) ? tx_chroma : tx_full;
    if (!data[c] || !denoised[c]) continue;
    if (c > 0 && chroma_sub[0] != 0) {
      block_finder = &block_finder_chroma;
    }
    memset(result, 0, sizeof(*result) * result_stride * result_height);
    // Do overlapped block processing (half overlapped). The block rows can
    // easily be done in parallel
    for (int offsy = 0; offsy < (block_size >> chroma_sub_h);
         offsy += (block_size >> chroma_sub_h) / 2) {
      for (int offsx = 0; offsx < (block_size >> chroma_sub_w);
           offsx += (block_size >> chroma_sub_w) / 2) {
        // Pad the boundary when processing each block-set.
        for (int by = -1; by < num_blocks_h; ++by) {
          for (int bx = -1; bx < num_blocks_w; ++bx) {
            const int pixels_per_block =
                (block_size >> chroma_sub_w) * (block_size >> chroma_sub_h);
            aom_flat_block_finder_extract_block(
                block_finder, data[c], w >> chroma_sub_w, h >> chroma_sub_h,
                stride[c], bx * (block_size >> chroma_sub_w) + offsx,
                by * (block_size >> chroma_sub_h) + offsy, plane_d, block_d);
            for (int j = 0; j < pixels_per_block; ++j) {
              block[j] = (float)block_d[j];
              plane[j] = (float)plane_d[j];
            }
            pointwise_multiply(window_function, block, pixels_per_block);
            aom_noise_tx_forward(tx, block);
            aom_noise_tx_filter(tx, noise_psd[c]);
            aom_noise_tx_inverse(tx, block);

            // Apply window function to the plane approximation (we will apply
            // it to the sum of plane + block when composing the results).
            pointwise_multiply(window_function, plane, pixels_per_block);

            for (int y = 0; y < (block_size >> chroma_sub_h); ++y) {
              const int y_result =
                  y + (by + 1) * (block_size >> chroma_sub_h) + offsy;
              for (int x = 0; x < (block_size >> chroma_sub_w); ++x) {
                const int x_result =
                    x + (bx + 1) * (block_size >> chroma_sub_w) + offsx;
                result[y_result * result_stride + x_result] +=
                    (block[y * (block_size >> chroma_sub_w) + x] +
                     plane[y * (block_size >> chroma_sub_w) + x]) *
                    window_function[y * (block_size >> chroma_sub_w) + x];
              }
            }
          }
        }
      }
    }
    if (use_highbd) {
      dither_and_quantize_highbd(result, result_stride, (uint16_t *)denoised[c],
                                 w, h, stride[c], chroma_sub_w, chroma_sub_h,
                                 block_size, kBlockNormalization);
    } else {
      dither_and_quantize_lowbd(result, result_stride, denoised[c], w, h,
                                stride[c], chroma_sub_w, chroma_sub_h,
                                block_size, kBlockNormalization);
    }
  }
  aom_free(result);
  aom_free(plane);
  aom_free(block);
  aom_free(plane_d);
  aom_free(block_d);
  aom_free(window_full);

  aom_noise_tx_free(tx_full);

  aom_flat_block_finder_free(&block_finder_full);
  if (chroma_sub[0] != 0) {
    aom_flat_block_finder_free(&block_finder_chroma);
    aom_free(window_chroma);
    aom_noise_tx_free(tx_chroma);
  }
  return init_success;
}

struct aom_denoise_and_model_t {
  int block_size;
  int bit_depth;
  float noise_level;

  // Size of current denoised buffer and flat_block buffer
  int width;
  int height;
  int y_stride;
  int uv_stride;
  int num_blocks_w;
  int num_blocks_h;

  // Buffers for image and noise_psd allocated on the fly
  float *noise_psd[3];
  uint8_t *denoised[3];
  uint8_t *flat_blocks;

  aom_flat_block_finder_t flat_block_finder;
  aom_noise_model_t noise_model;
};

struct aom_denoise_and_model_t *aom_denoise_and_model_alloc(int bit_depth,
                                                            int block_size,
                                                            float noise_level) {
  struct aom_denoise_and_model_t *ctx =
      (struct aom_denoise_and_model_t *)aom_malloc(
          sizeof(struct aom_denoise_and_model_t));
  if (!ctx) {
    fprintf(stderr, "Unable to allocate denoise_and_model struct\n");
    return NULL;
  }
  memset(ctx, 0, sizeof(*ctx));

  ctx->block_size = block_size;
  ctx->noise_level = noise_level;
  ctx->bit_depth = bit_depth;

  ctx->noise_psd[0] =
      (float *)aom_malloc(sizeof(*ctx->noise_psd[0]) * block_size * block_size);
  ctx->noise_psd[1] =
      (float *)aom_malloc(sizeof(*ctx->noise_psd[1]) * block_size * block_size);
  ctx->noise_psd[2] =
      (float *)aom_malloc(sizeof(*ctx->noise_psd[2]) * block_size * block_size);
  if (!ctx->noise_psd[0] || !ctx->noise_psd[1] || !ctx->noise_psd[2]) {
    fprintf(stderr, "Unable to allocate noise PSD buffers\n");
    aom_denoise_and_model_free(ctx);
    return NULL;
  }
  return ctx;
}

void aom_denoise_and_model_free(struct aom_denoise_and_model_t *ctx) {
  aom_free(ctx->flat_blocks);
  for (int i = 0; i < 3; ++i) {
    aom_free(ctx->denoised[i]);
    aom_free(ctx->noise_psd[i]);
  }
  aom_noise_model_free(&ctx->noise_model);
  aom_flat_block_finder_free(&ctx->flat_block_finder);
  aom_free(ctx);
}

static int denoise_and_model_realloc_if_necessary(
    struct aom_denoise_and_model_t *ctx, const YV12_BUFFER_CONFIG *sd) {
  if (ctx->width == sd->y_width && ctx->height == sd->y_height &&
      ctx->y_stride == sd->y_stride && ctx->uv_stride == sd->uv_stride)
    return 1;
  const int use_highbd = (sd->flags & YV12_FLAG_HIGHBITDEPTH) != 0;
  const int block_size = ctx->block_size;

  ctx->width = sd->y_width;
  ctx->height = sd->y_height;
  ctx->y_stride = sd->y_stride;
  ctx->uv_stride = sd->uv_stride;

  for (int i = 0; i < 3; ++i) {
    aom_free(ctx->denoised[i]);
    ctx->denoised[i] = NULL;
  }
  aom_free(ctx->flat_blocks);
  ctx->flat_blocks = NULL;

  ctx->denoised[0] =
      (uint8_t *)aom_malloc((sd->y_stride * sd->y_height) << use_highbd);
  ctx->denoised[1] =
      (uint8_t *)aom_malloc((sd->uv_stride * sd->uv_height) << use_highbd);
  ctx->denoised[2] =
      (uint8_t *)aom_malloc((sd->uv_stride * sd->uv_height) << use_highbd);
  if (!ctx->denoised[0] || !ctx->denoised[1] || !ctx->denoised[2]) {
    fprintf(stderr, "Unable to allocate denoise buffers\n");
    return 0;
  }
  ctx->num_blocks_w = (sd->y_width + ctx->block_size - 1) / ctx->block_size;
  ctx->num_blocks_h = (sd->y_height + ctx->block_size - 1) / ctx->block_size;
  ctx->flat_blocks =
      (uint8_t *)aom_malloc(ctx->num_blocks_w * ctx->num_blocks_h);
  if (!ctx->flat_blocks) {
    fprintf(stderr, "Unable to allocate flat_blocks buffer\n");
    return 0;
  }

  aom_flat_block_finder_free(&ctx->flat_block_finder);
  if (!aom_flat_block_finder_init(&ctx->flat_block_finder, ctx->block_size,
                                  ctx->bit_depth, use_highbd)) {
    fprintf(stderr, "Unable to init flat block finder\n");
    return 0;
  }

  const aom_noise_model_params_t params = { AOM_NOISE_SHAPE_SQUARE, 3,
                                            ctx->bit_depth, use_highbd };
  aom_noise_model_free(&ctx->noise_model);
  if (!aom_noise_model_init(&ctx->noise_model, params)) {
    fprintf(stderr, "Unable to init noise model\n");
    return 0;
  }

  // Simply use a flat PSD (although we could use the flat blocks to estimate
  // PSD) those to estimate an actual noise PSD)
  const float y_noise_level =
      aom_noise_psd_get_default_value(ctx->block_size, ctx->noise_level);
  const float uv_noise_level = aom_noise_psd_get_default_value(
      ctx->block_size >> sd->subsampling_x, ctx->noise_level);
  for (int i = 0; i < block_size * block_size; ++i) {
    ctx->noise_psd[0][i] = y_noise_level;
    ctx->noise_psd[1][i] = ctx->noise_psd[2][i] = uv_noise_level;
  }
  return 1;
}

// TODO(aomedia:3151): Handle a monochrome image (sd->u_buffer and sd->v_buffer
// are null pointers) correctly.
int aom_denoise_and_model_run(struct aom_denoise_and_model_t *ctx,
                              const YV12_BUFFER_CONFIG *sd,
                              aom_film_grain_t *film_grain, int apply_denoise) {
  const int block_size = ctx->block_size;
  const int use_highbd = (sd->flags & YV12_FLAG_HIGHBITDEPTH) != 0;
  uint8_t *raw_data[3] = {
    use_highbd ? (uint8_t *)CONVERT_TO_SHORTPTR(sd->y_buffer) : sd->y_buffer,
    use_highbd ? (uint8_t *)CONVERT_TO_SHORTPTR(sd->u_buffer) : sd->u_buffer,
    use_highbd ? (uint8_t *)CONVERT_TO_SHORTPTR(sd->v_buffer) : sd->v_buffer,
  };
  const uint8_t *const data[3] = { raw_data[0], raw_data[1], raw_data[2] };
  int strides[3] = { sd->y_stride, sd->uv_stride, sd->uv_stride };
  int chroma_sub_log2[2] = { sd->subsampling_x, sd->subsampling_y };

  if (!denoise_and_model_realloc_if_necessary(ctx, sd)) {
    fprintf(stderr, "Unable to realloc buffers\n");
    return 0;
  }

  aom_flat_block_finder_run(&ctx->flat_block_finder, data[0], sd->y_width,
                            sd->y_height, strides[0], ctx->flat_blocks);

  if (!aom_wiener_denoise_2d(data, ctx->denoised, sd->y_width, sd->y_height,
                             strides, chroma_sub_log2, ctx->noise_psd,
                             block_size, ctx->bit_depth, use_highbd)) {
    fprintf(stderr, "Unable to denoise image\n");
    return 0;
  }

  const aom_noise_status_t status = aom_noise_model_update(
      &ctx->noise_model, data, (const uint8_t *const *)ctx->denoised,
      sd->y_width, sd->y_height, strides, chroma_sub_log2, ctx->flat_blocks,
      block_size);
  int have_noise_estimate = 0;
  if (status == AOM_NOISE_STATUS_OK) {
    have_noise_estimate = 1;
  } else if (status == AOM_NOISE_STATUS_DIFFERENT_NOISE_TYPE) {
    aom_noise_model_save_latest(&ctx->noise_model);
    have_noise_estimate = 1;
  } else {
    // Unable to update noise model; proceed if we have a previous estimate.
    have_noise_estimate =
        (ctx->noise_model.combined_state[0].strength_solver.num_equations > 0);
  }

  film_grain->apply_grain = 0;
  if (have_noise_estimate) {
    if (!aom_noise_model_get_grain_parameters(&ctx->noise_model, film_grain)) {
      fprintf(stderr, "Unable to get grain parameters.\n");
      return 0;
    }
    if (!film_grain->random_seed) {
      film_grain->random_seed = 7391;
    }
    if (apply_denoise) {
      memcpy(raw_data[0], ctx->denoised[0],
             (strides[0] * sd->y_height) << use_highbd);
      if (!sd->monochrome) {
        memcpy(raw_data[1], ctx->denoised[1],
               (strides[1] * sd->uv_height) << use_highbd);
        memcpy(raw_data[2], ctx->denoised[2],
               (strides[2] * sd->uv_height) << use_highbd);
      }
    }
  }
  return 1;
}
