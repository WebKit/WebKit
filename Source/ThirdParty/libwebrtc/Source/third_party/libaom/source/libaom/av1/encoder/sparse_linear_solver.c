/*
 * Copyright (c) 2021, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include "av1/common/av1_common_int.h"
#include "av1/encoder/sparse_linear_solver.h"
#include "config/aom_config.h"
#include "aom_mem/aom_mem.h"
#include "av1/common/alloccommon.h"

#if CONFIG_OPTICAL_FLOW_API
/*
 * Input:
 * rows: array of row positions
 * cols: array of column positions
 * values: array of element values
 * num_elem: total number of elements in the matrix
 * num_rows: number of rows in the matrix
 * num_cols: number of columns in the matrix
 *
 * Output:
 * sm: pointer to the sparse matrix to be initialized
 *
 * Return: 0  - success
 *         -1 - failed
 */
int av1_init_sparse_mtx(const int *rows, const int *cols, const double *values,
                        int num_elem, int num_rows, int num_cols,
                        SPARSE_MTX *sm) {
  sm->n_elem = num_elem;
  sm->n_rows = num_rows;
  sm->n_cols = num_cols;
  if (num_elem == 0) {
    sm->row_pos = NULL;
    sm->col_pos = NULL;
    sm->value = NULL;
    return 0;
  }
  sm->row_pos = aom_calloc(num_elem, sizeof(*sm->row_pos));
  sm->col_pos = aom_calloc(num_elem, sizeof(*sm->col_pos));
  sm->value = aom_calloc(num_elem, sizeof(*sm->value));

  if (!sm->row_pos || !sm->col_pos || !sm->value) {
    av1_free_sparse_mtx_elems(sm);
    return -1;
  }

  memcpy(sm->row_pos, rows, num_elem * sizeof(*sm->row_pos));
  memcpy(sm->col_pos, cols, num_elem * sizeof(*sm->col_pos));
  memcpy(sm->value, values, num_elem * sizeof(*sm->value));

  return 0;
}

/*
 * Combines two sparse matrices (allocating new space).
 *
 * Input:
 * sm1, sm2: matrices to be combined
 * row_offset1, row_offset2: row offset of each matrix in the new matrix
 * col_offset1, col_offset2: column offset of each matrix in the new matrix
 * new_n_rows, new_n_cols: number of rows and columns in the new matrix
 *
 * Output:
 * sm: the combined matrix
 *
 * Return: 0  - success
 *         -1 - failed
 */
int av1_init_combine_sparse_mtx(const SPARSE_MTX *sm1, const SPARSE_MTX *sm2,
                                SPARSE_MTX *sm, int row_offset1,
                                int col_offset1, int row_offset2,
                                int col_offset2, int new_n_rows,
                                int new_n_cols) {
  sm->n_elem = sm1->n_elem + sm2->n_elem;
  sm->n_cols = new_n_cols;
  sm->n_rows = new_n_rows;

  if (sm->n_elem == 0) {
    sm->row_pos = NULL;
    sm->col_pos = NULL;
    sm->value = NULL;
    return 0;
  }

  sm->row_pos = aom_calloc(sm->n_elem, sizeof(*sm->row_pos));
  sm->col_pos = aom_calloc(sm->n_elem, sizeof(*sm->col_pos));
  sm->value = aom_calloc(sm->n_elem, sizeof(*sm->value));

  if (!sm->row_pos || !sm->col_pos || !sm->value) {
    av1_free_sparse_mtx_elems(sm);
    return -1;
  }

  for (int i = 0; i < sm1->n_elem; i++) {
    sm->row_pos[i] = sm1->row_pos[i] + row_offset1;
    sm->col_pos[i] = sm1->col_pos[i] + col_offset1;
  }
  memcpy(sm->value, sm1->value, sm1->n_elem * sizeof(*sm1->value));
  int n_elem1 = sm1->n_elem;
  for (int i = 0; i < sm2->n_elem; i++) {
    sm->row_pos[n_elem1 + i] = sm2->row_pos[i] + row_offset2;
    sm->col_pos[n_elem1 + i] = sm2->col_pos[i] + col_offset2;
  }
  memcpy(sm->value + n_elem1, sm2->value, sm2->n_elem * sizeof(*sm2->value));
  return 0;
}

void av1_free_sparse_mtx_elems(SPARSE_MTX *sm) {
  sm->n_cols = 0;
  sm->n_rows = 0;
  if (sm->n_elem != 0) {
    aom_free(sm->row_pos);
    aom_free(sm->col_pos);
    aom_free(sm->value);
  }
  sm->n_elem = 0;
}

/*
 * Calculate matrix and vector multiplication: A*b
 *
 * Input:
 * sm: matrix A
 * srcv: the vector b to be multiplied to
 * dstl: the length of vectors
 *
 * Output:
 * dstv: pointer to the resulting vector
 */
void av1_mtx_vect_multi_right(const SPARSE_MTX *sm, const double *srcv,
                              double *dstv, int dstl) {
  memset(dstv, 0, sizeof(*dstv) * dstl);
  for (int i = 0; i < sm->n_elem; i++) {
    dstv[sm->row_pos[i]] += srcv[sm->col_pos[i]] * sm->value[i];
  }
}
/*
 * Calculate matrix and vector multiplication: b*A
 *
 * Input:
 * sm: matrix A
 * srcv: the vector b to be multiplied to
 * dstl: the length of vectors
 *
 * Output:
 * dstv: pointer to the resulting vector
 */
void av1_mtx_vect_multi_left(const SPARSE_MTX *sm, const double *srcv,
                             double *dstv, int dstl) {
  memset(dstv, 0, sizeof(*dstv) * dstl);
  for (int i = 0; i < sm->n_elem; i++) {
    dstv[sm->col_pos[i]] += srcv[sm->row_pos[i]] * sm->value[i];
  }
}

/*
 * Calculate inner product of two vectors
 *
 * Input:
 * src1, scr2: the vectors to be multiplied
 * src1l: length of the vectors
 *
 * Output:
 * the inner product
 */
double av1_vect_vect_multi(const double *src1, int src1l, const double *src2) {
  double result = 0;
  for (int i = 0; i < src1l; i++) {
    result += src1[i] * src2[i];
  }
  return result;
}

/*
 * Multiply each element in the matrix sm with a constant c
 */
void av1_constant_multiply_sparse_matrix(SPARSE_MTX *sm, double c) {
  for (int i = 0; i < sm->n_elem; i++) {
    sm->value[i] *= c;
  }
}

static INLINE void free_solver_local_buf(double *buf1, double *buf2,
                                         double *buf3, double *buf4,
                                         double *buf5, double *buf6,
                                         double *buf7) {
  aom_free(buf1);
  aom_free(buf2);
  aom_free(buf3);
  aom_free(buf4);
  aom_free(buf5);
  aom_free(buf6);
  aom_free(buf7);
}

/*
 * Solve for Ax = b
 * no requirement on A
 *
 * Input:
 * A: the sparse matrix
 * b: the vector b
 * bl: length of b
 * x: the vector x
 *
 * Output:
 * x: pointer to the solution vector
 *
 * Return: 0  - success
 *         -1 - failed
 */
int av1_bi_conjugate_gradient_sparse(const SPARSE_MTX *A, const double *b,
                                     int bl, double *x) {
  double *r = NULL, *r_hat = NULL, *p = NULL, *p_hat = NULL, *Ap = NULL,
         *p_hatA = NULL, *x_hat = NULL;
  double alpha, beta, rtr, r_norm_2;
  double denormtemp;

  // initialize
  r = aom_calloc(bl, sizeof(*r));
  r_hat = aom_calloc(bl, sizeof(*r_hat));
  p = aom_calloc(bl, sizeof(*p));
  p_hat = aom_calloc(bl, sizeof(*p_hat));
  Ap = aom_calloc(bl, sizeof(*Ap));
  p_hatA = aom_calloc(bl, sizeof(*p_hatA));
  x_hat = aom_calloc(bl, sizeof(*x_hat));
  if (!r || !r_hat || !p || !p_hat || !Ap || !p_hatA || !x_hat) {
    free_solver_local_buf(r, r_hat, p, p_hat, Ap, p_hatA, x_hat);
    return -1;
  }

  int i;
  for (i = 0; i < bl; i++) {
    r[i] = b[i];
    r_hat[i] = b[i];
    p[i] = r[i];
    p_hat[i] = r_hat[i];
    x[i] = 0;
    x_hat[i] = 0;
  }
  r_norm_2 = av1_vect_vect_multi(r_hat, bl, r);
  for (int k = 0; k < MAX_CG_SP_ITER; k++) {
    rtr = r_norm_2;
    av1_mtx_vect_multi_right(A, p, Ap, bl);
    av1_mtx_vect_multi_left(A, p_hat, p_hatA, bl);

    denormtemp = av1_vect_vect_multi(p_hat, bl, Ap);
    if (denormtemp < 1e-10) break;
    alpha = rtr / denormtemp;
    r_norm_2 = 0;
    for (i = 0; i < bl; i++) {
      x[i] += alpha * p[i];
      x_hat[i] += alpha * p_hat[i];
      r[i] -= alpha * Ap[i];
      r_hat[i] -= alpha * p_hatA[i];
      r_norm_2 += r_hat[i] * r[i];
    }
    if (sqrt(r_norm_2) < 1e-2) {
      break;
    }
    if (rtr < 1e-10) break;
    beta = r_norm_2 / rtr;
    for (i = 0; i < bl; i++) {
      p[i] = r[i] + beta * p[i];
      p_hat[i] = r_hat[i] + beta * p_hat[i];
    }
  }
  // free
  free_solver_local_buf(r, r_hat, p, p_hat, Ap, p_hatA, x_hat);
  return 0;
}

/*
 * Solve for Ax = b when A is symmetric and positive definite
 *
 * Input:
 * A: the sparse matrix
 * b: the vector b
 * bl: length of b
 * x: the vector x
 *
 * Output:
 * x: pointer to the solution vector
 *
 * Return: 0  - success
 *         -1 - failed
 */
int av1_conjugate_gradient_sparse(const SPARSE_MTX *A, const double *b, int bl,
                                  double *x) {
  double *r = NULL, *p = NULL, *Ap = NULL;
  double alpha, beta, rtr, r_norm_2;
  double denormtemp;

  // initialize
  r = aom_calloc(bl, sizeof(*r));
  p = aom_calloc(bl, sizeof(*p));
  Ap = aom_calloc(bl, sizeof(*Ap));
  if (!r || !p || !Ap) {
    free_solver_local_buf(r, p, Ap, NULL, NULL, NULL, NULL);
    return -1;
  }

  int i;
  for (i = 0; i < bl; i++) {
    r[i] = b[i];
    p[i] = r[i];
    x[i] = 0;
  }
  r_norm_2 = av1_vect_vect_multi(r, bl, r);
  int k;
  for (k = 0; k < MAX_CG_SP_ITER; k++) {
    rtr = r_norm_2;
    av1_mtx_vect_multi_right(A, p, Ap, bl);
    denormtemp = av1_vect_vect_multi(p, bl, Ap);
    if (denormtemp < 1e-10) break;
    alpha = rtr / denormtemp;
    r_norm_2 = 0;
    for (i = 0; i < bl; i++) {
      x[i] += alpha * p[i];
      r[i] -= alpha * Ap[i];
      r_norm_2 += r[i] * r[i];
    }
    if (r_norm_2 < 1e-8 * bl) break;
    if (rtr < 1e-10) break;
    beta = r_norm_2 / rtr;
    for (i = 0; i < bl; i++) {
      p[i] = r[i] + beta * p[i];
    }
  }
  // free
  free_solver_local_buf(r, p, Ap, NULL, NULL, NULL, NULL);

  return 0;
}

/*
 * Solve for Ax = b using Jacobi method
 *
 * Input:
 * A: the sparse matrix
 * b: the vector b
 * bl: length of b
 * x: the vector x
 *
 * Output:
 * x: pointer to the solution vector
 *
 * Return: 0  - success
 *         -1 - failed
 */
int av1_jacobi_sparse(const SPARSE_MTX *A, const double *b, int bl, double *x) {
  double *diags = NULL, *Rx = NULL, *x_last = NULL, *x_cur = NULL,
         *tempx = NULL;
  double resi2;

  diags = aom_calloc(bl, sizeof(*diags));
  Rx = aom_calloc(bl, sizeof(*Rx));
  x_last = aom_calloc(bl, sizeof(*x_last));
  x_cur = aom_calloc(bl, sizeof(*x_cur));

  if (!diags || !Rx || !x_last || !x_cur) {
    free_solver_local_buf(diags, Rx, x_last, x_cur, NULL, NULL, NULL);
    return -1;
  }

  int i;
  memset(x_last, 0, sizeof(*x_last) * bl);
  // get the diagonals of A
  memset(diags, 0, sizeof(*diags) * bl);
  for (int c = 0; c < A->n_elem; c++) {
    if (A->row_pos[c] != A->col_pos[c]) continue;
    diags[A->row_pos[c]] = A->value[c];
  }
  int k;
  for (k = 0; k < MAX_CG_SP_ITER; k++) {
    // R = A - diag(diags)
    // get R*x_last
    memset(Rx, 0, sizeof(*Rx) * bl);
    for (int c = 0; c < A->n_elem; c++) {
      if (A->row_pos[c] == A->col_pos[c]) continue;
      Rx[A->row_pos[c]] += x_last[A->col_pos[c]] * A->value[c];
    }
    resi2 = 0;
    for (i = 0; i < bl; i++) {
      x_cur[i] = (b[i] - Rx[i]) / diags[i];
      resi2 += (x_last[i] - x_cur[i]) * (x_last[i] - x_cur[i]);
    }
    if (resi2 <= 1e-10 * bl) break;
    // swap last & cur buffer ptrs
    tempx = x_last;
    x_last = x_cur;
    x_cur = tempx;
  }
  printf("\n numiter: %d\n", k);
  for (i = 0; i < bl; i++) {
    x[i] = x_cur[i];
  }
  free_solver_local_buf(diags, Rx, x_last, x_cur, NULL, NULL, NULL);
  return 0;
}

/*
 * Solve for Ax = b using Steepest descent method
 *
 * Input:
 * A: the sparse matrix
 * b: the vector b
 * bl: length of b
 * x: the vector x
 *
 * Output:
 * x: pointer to the solution vector
 *
 * Return: 0  - success
 *         -1 - failed
 */
int av1_steepest_descent_sparse(const SPARSE_MTX *A, const double *b, int bl,
                                double *x) {
  double *d = NULL, *Ad = NULL, *Ax = NULL;
  double resi2, resi2_last, dAd, temp;

  d = aom_calloc(bl, sizeof(*d));
  Ax = aom_calloc(bl, sizeof(*Ax));
  Ad = aom_calloc(bl, sizeof(*Ad));

  if (!d || !Ax || !Ad) {
    free_solver_local_buf(d, Ax, Ad, NULL, NULL, NULL, NULL);
    return -1;
  }

  int i;
  // initialize with 0s
  resi2 = 0;
  for (i = 0; i < bl; i++) {
    x[i] = 0;
    d[i] = b[i];
    resi2 += d[i] * d[i] / bl;
  }
  int k;
  for (k = 0; k < MAX_CG_SP_ITER; k++) {
    // get A*x_last
    av1_mtx_vect_multi_right(A, d, Ad, bl);
    dAd = resi2 * bl / av1_vect_vect_multi(d, bl, Ad);
    for (i = 0; i < bl; i++) {
      temp = dAd * d[i];
      x[i] = x[i] + temp;
    }
    av1_mtx_vect_multi_right(A, x, Ax, bl);
    resi2_last = resi2;
    resi2 = 0;
    for (i = 0; i < bl; i++) {
      d[i] = b[i] - Ax[i];
      resi2 += d[i] * d[i] / bl;
    }
    if (resi2 <= 1e-8) break;
    if (resi2_last - resi2 < 1e-8) {
      break;
    }
  }
  free_solver_local_buf(d, Ax, Ad, NULL, NULL, NULL, NULL);

  return 0;
}

#endif  // CONFIG_OPTICAL_FLOW_API
