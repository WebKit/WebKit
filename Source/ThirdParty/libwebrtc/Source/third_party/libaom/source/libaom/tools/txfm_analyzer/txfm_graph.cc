/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "tools/txfm_analyzer/txfm_graph.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

typedef struct Node Node;

void get_fun_name(char *str_fun_name, int str_buf_size, const TYPE_TXFM type,
                  const int txfm_size) {
  if (type == TYPE_DCT)
    snprintf(str_fun_name, str_buf_size, "fdct%d_new", txfm_size);
  else if (type == TYPE_ADST)
    snprintf(str_fun_name, str_buf_size, "fadst%d_new", txfm_size);
  else if (type == TYPE_IDCT)
    snprintf(str_fun_name, str_buf_size, "idct%d_new", txfm_size);
  else if (type == TYPE_IADST)
    snprintf(str_fun_name, str_buf_size, "iadst%d_new", txfm_size);
}

void get_txfm_type_name(char *str_fun_name, int str_buf_size,
                        const TYPE_TXFM type, const int txfm_size) {
  if (type == TYPE_DCT)
    snprintf(str_fun_name, str_buf_size, "TXFM_TYPE_DCT%d", txfm_size);
  else if (type == TYPE_ADST)
    snprintf(str_fun_name, str_buf_size, "TXFM_TYPE_ADST%d", txfm_size);
  else if (type == TYPE_IDCT)
    snprintf(str_fun_name, str_buf_size, "TXFM_TYPE_DCT%d", txfm_size);
  else if (type == TYPE_IADST)
    snprintf(str_fun_name, str_buf_size, "TXFM_TYPE_ADST%d", txfm_size);
}

void get_hybrid_2d_type_name(char *buf, int buf_size, const TYPE_TXFM type0,
                             const TYPE_TXFM type1, const int txfm_size0,
                             const int txfm_size1) {
  if (type0 == TYPE_DCT && type1 == TYPE_DCT)
    snprintf(buf, buf_size, "_dct_dct_%dx%d", txfm_size1, txfm_size0);
  else if (type0 == TYPE_DCT && type1 == TYPE_ADST)
    snprintf(buf, buf_size, "_dct_adst_%dx%d", txfm_size1, txfm_size0);
  else if (type0 == TYPE_ADST && type1 == TYPE_ADST)
    snprintf(buf, buf_size, "_adst_adst_%dx%d", txfm_size1, txfm_size0);
  else if (type0 == TYPE_ADST && type1 == TYPE_DCT)
    snprintf(buf, buf_size, "_adst_dct_%dx%d", txfm_size1, txfm_size0);
}

TYPE_TXFM get_inv_type(TYPE_TXFM type) {
  if (type == TYPE_DCT)
    return TYPE_IDCT;
  else if (type == TYPE_ADST)
    return TYPE_IADST;
  else if (type == TYPE_IDCT)
    return TYPE_DCT;
  else if (type == TYPE_IADST)
    return TYPE_ADST;
  else
    return TYPE_LAST;
}

void reference_dct_1d(double *in, double *out, int size) {
  const double kInvSqrt2 = 0.707106781186547524400844362104;
  for (int k = 0; k < size; k++) {
    out[k] = 0;  // initialize out[k]
    for (int n = 0; n < size; n++) {
      out[k] += in[n] * cos(PI * (2 * n + 1) * k / (2 * size));
    }
    if (k == 0) out[k] = out[k] * kInvSqrt2;
  }
}

void reference_dct_2d(double *in, double *out, int size) {
  double *tempOut = new double[size * size];
  // dct each row: in -> out
  for (int r = 0; r < size; r++) {
    reference_dct_1d(in + r * size, out + r * size, size);
  }

  for (int r = 0; r < size; r++) {
    // out ->tempOut
    for (int c = 0; c < size; c++) {
      tempOut[r * size + c] = out[c * size + r];
    }
  }
  for (int r = 0; r < size; r++) {
    reference_dct_1d(tempOut + r * size, out + r * size, size);
  }
  delete[] tempOut;
}

void reference_adst_1d(double *in, double *out, int size) {
  for (int k = 0; k < size; k++) {
    out[k] = 0;  // initialize out[k]
    for (int n = 0; n < size; n++) {
      out[k] += in[n] * sin(PI * (2 * n + 1) * (2 * k + 1) / (4 * size));
    }
  }
}

void reference_hybrid_2d(double *in, double *out, int size, int type0,
                         int type1) {
  double *tempOut = new double[size * size];
  // dct each row: in -> out
  for (int r = 0; r < size; r++) {
    if (type0 == TYPE_DCT)
      reference_dct_1d(in + r * size, out + r * size, size);
    else
      reference_adst_1d(in + r * size, out + r * size, size);
  }

  for (int r = 0; r < size; r++) {
    // out ->tempOut
    for (int c = 0; c < size; c++) {
      tempOut[r * size + c] = out[c * size + r];
    }
  }
  for (int r = 0; r < size; r++) {
    if (type1 == TYPE_DCT)
      reference_dct_1d(tempOut + r * size, out + r * size, size);
    else
      reference_adst_1d(tempOut + r * size, out + r * size, size);
  }
  delete[] tempOut;
}

void reference_hybrid_2d_new(double *in, double *out, int size0, int size1,
                             int type0, int type1) {
  double *tempOut = new double[size0 * size1];
  // dct each row: in -> out
  for (int r = 0; r < size1; r++) {
    if (type0 == TYPE_DCT)
      reference_dct_1d(in + r * size0, out + r * size0, size0);
    else
      reference_adst_1d(in + r * size0, out + r * size0, size0);
  }

  for (int r = 0; r < size1; r++) {
    // out ->tempOut
    for (int c = 0; c < size0; c++) {
      tempOut[c * size1 + r] = out[r * size0 + c];
    }
  }
  for (int r = 0; r < size0; r++) {
    if (type1 == TYPE_DCT)
      reference_dct_1d(tempOut + r * size1, out + r * size1, size1);
    else
      reference_adst_1d(tempOut + r * size1, out + r * size1, size1);
  }
  delete[] tempOut;
}

unsigned int get_max_bit(unsigned int x) {
  int max_bit = -1;
  while (x) {
    x = x >> 1;
    max_bit++;
  }
  return max_bit;
}

unsigned int bitwise_reverse(unsigned int x, int max_bit) {
  x = ((x >> 16) & 0x0000ffff) | ((x & 0x0000ffff) << 16);
  x = ((x >> 8) & 0x00ff00ff) | ((x & 0x00ff00ff) << 8);
  x = ((x >> 4) & 0x0f0f0f0f) | ((x & 0x0f0f0f0f) << 4);
  x = ((x >> 2) & 0x33333333) | ((x & 0x33333333) << 2);
  x = ((x >> 1) & 0x55555555) | ((x & 0x55555555) << 1);
  x = x >> (31 - max_bit);
  return x;
}

int get_idx(int ri, int ci, int cSize) { return ri * cSize + ci; }

void add_node(Node *node, int stage_num, int node_num, int stage_idx,
              int node_idx, int in, double w) {
  int outIdx = get_idx(stage_idx, node_idx, node_num);
  int inIdx = get_idx(stage_idx - 1, in, node_num);
  int idx = node[outIdx].inNodeNum;
  if (idx < 2) {
    node[outIdx].inNode[idx] = &node[inIdx];
    node[outIdx].inNodeIdx[idx] = in;
    node[outIdx].inWeight[idx] = w;
    idx++;
    node[outIdx].inNodeNum = idx;
  } else {
    printf("Error: inNode is full");
  }
}

void connect_node(Node *node, int stage_num, int node_num, int stage_idx,
                  int node_idx, int in0, double w0, int in1, double w1) {
  int outIdx = get_idx(stage_idx, node_idx, node_num);
  int inIdx0 = get_idx(stage_idx - 1, in0, node_num);
  int inIdx1 = get_idx(stage_idx - 1, in1, node_num);

  int idx = 0;
  // if(w0 != 0) {
  node[outIdx].inNode[idx] = &node[inIdx0];
  node[outIdx].inNodeIdx[idx] = in0;
  node[outIdx].inWeight[idx] = w0;
  idx++;
  //}

  // if(w1 != 0) {
  node[outIdx].inNode[idx] = &node[inIdx1];
  node[outIdx].inNodeIdx[idx] = in1;
  node[outIdx].inWeight[idx] = w1;
  idx++;
  //}

  node[outIdx].inNodeNum = idx;
}

void propagate(Node *node, int stage_num, int node_num, int stage_idx) {
  for (int ni = 0; ni < node_num; ni++) {
    int outIdx = get_idx(stage_idx, ni, node_num);
    node[outIdx].value = 0;
    for (int k = 0; k < node[outIdx].inNodeNum; k++) {
      node[outIdx].value +=
          node[outIdx].inNode[k]->value * node[outIdx].inWeight[k];
    }
  }
}

int64_t round_shift(int64_t value, int bit) {
  if (bit > 0) {
    if (value < 0) {
      return -round_shift(-value, bit);
    } else {
      return (value + (1 << (bit - 1))) >> bit;
    }
  } else {
    return value << (-bit);
  }
}

void round_shift_array(int32_t *arr, int size, int bit) {
  if (bit == 0) {
    return;
  } else {
    for (int i = 0; i < size; i++) {
      arr[i] = round_shift(arr[i], bit);
    }
  }
}

void graph_reset_visited(Node *node, int stage_num, int node_num) {
  for (int si = 0; si < stage_num; si++) {
    for (int ni = 0; ni < node_num; ni++) {
      int idx = get_idx(si, ni, node_num);
      node[idx].visited = 0;
    }
  }
}

void estimate_value(Node *node, int stage_num, int node_num, int stage_idx,
                    int node_idx, int estimate_bit) {
  if (stage_idx > 0) {
    int outIdx = get_idx(stage_idx, node_idx, node_num);
    int64_t out = 0;
    node[outIdx].value = 0;
    for (int k = 0; k < node[outIdx].inNodeNum; k++) {
      int64_t w = round(node[outIdx].inWeight[k] * (1 << estimate_bit));
      int64_t v = round(node[outIdx].inNode[k]->value);
      out += v * w;
    }
    node[outIdx].value = round_shift(out, estimate_bit);
  }
}

void amplify_value(Node *node, int stage_num, int node_num, int stage_idx,
                   int node_idx, int amplify_bit) {
  int outIdx = get_idx(stage_idx, node_idx, node_num);
  node[outIdx].value = round_shift(round(node[outIdx].value), -amplify_bit);
}

void propagate_estimate_amlify(Node *node, int stage_num, int node_num,
                               int stage_idx, int amplify_bit,
                               int estimate_bit) {
  for (int ni = 0; ni < node_num; ni++) {
    estimate_value(node, stage_num, node_num, stage_idx, ni, estimate_bit);
    amplify_value(node, stage_num, node_num, stage_idx, ni, amplify_bit);
  }
}

void init_graph(Node *node, int stage_num, int node_num) {
  for (int si = 0; si < stage_num; si++) {
    for (int ni = 0; ni < node_num; ni++) {
      int outIdx = get_idx(si, ni, node_num);
      node[outIdx].stageIdx = si;
      node[outIdx].nodeIdx = ni;
      node[outIdx].value = 0;
      node[outIdx].inNodeNum = 0;
      if (si >= 1) {
        connect_node(node, stage_num, node_num, si, ni, ni, 1, ni, 0);
      }
    }
  }
}

void gen_B_graph(Node *node, int stage_num, int node_num, int stage_idx,
                 int node_idx, int N, int star) {
  for (int i = 0; i < N / 2; i++) {
    int out = node_idx + i;
    int in1 = node_idx + N - 1 - i;
    if (star == 1) {
      connect_node(node, stage_num, node_num, stage_idx + 1, out, out, -1, in1,
                   1);
    } else {
      connect_node(node, stage_num, node_num, stage_idx + 1, out, out, 1, in1,
                   1);
    }
  }
  for (int i = N / 2; i < N; i++) {
    int out = node_idx + i;
    int in1 = node_idx + N - 1 - i;
    if (star == 1) {
      connect_node(node, stage_num, node_num, stage_idx + 1, out, out, 1, in1,
                   1);
    } else {
      connect_node(node, stage_num, node_num, stage_idx + 1, out, out, -1, in1,
                   1);
    }
  }
}

void gen_P_graph(Node *node, int stage_num, int node_num, int stage_idx,
                 int node_idx, int N) {
  int max_bit = get_max_bit(N - 1);
  for (int i = 0; i < N; i++) {
    int out = node_idx + bitwise_reverse(i, max_bit);
    int in = node_idx + i;
    connect_node(node, stage_num, node_num, stage_idx + 1, out, in, 1, in, 0);
  }
}

void gen_type1_graph(Node *node, int stage_num, int node_num, int stage_idx,
                     int node_idx, int N) {
  int max_bit = get_max_bit(N);
  for (int ni = 0; ni < N / 2; ni++) {
    int ai = bitwise_reverse(N + ni, max_bit);
    int out = node_idx + ni;
    int in1 = node_idx + N - ni - 1;
    connect_node(node, stage_num, node_num, stage_idx + 1, out, out,
                 sin(PI * ai / (2 * 2 * N)), in1, cos(PI * ai / (2 * 2 * N)));
  }
  for (int ni = N / 2; ni < N; ni++) {
    int ai = bitwise_reverse(N + ni, max_bit);
    int out = node_idx + ni;
    int in1 = node_idx + N - ni - 1;
    connect_node(node, stage_num, node_num, stage_idx + 1, out, out,
                 cos(PI * ai / (2 * 2 * N)), in1, -sin(PI * ai / (2 * 2 * N)));
  }
}

void gen_type2_graph(Node *node, int stage_num, int node_num, int stage_idx,
                     int node_idx, int N) {
  for (int ni = 0; ni < N / 4; ni++) {
    int out = node_idx + ni;
    connect_node(node, stage_num, node_num, stage_idx + 1, out, out, 1, out, 0);
  }

  for (int ni = N / 4; ni < N / 2; ni++) {
    int out = node_idx + ni;
    int in1 = node_idx + N - ni - 1;
    connect_node(node, stage_num, node_num, stage_idx + 1, out, out,
                 -cos(PI / 4), in1, cos(-PI / 4));
  }

  for (int ni = N / 2; ni < N * 3 / 4; ni++) {
    int out = node_idx + ni;
    int in1 = node_idx + N - ni - 1;
    connect_node(node, stage_num, node_num, stage_idx + 1, out, out,
                 cos(-PI / 4), in1, cos(PI / 4));
  }

  for (int ni = N * 3 / 4; ni < N; ni++) {
    int out = node_idx + ni;
    connect_node(node, stage_num, node_num, stage_idx + 1, out, out, 1, out, 0);
  }
}

void gen_type3_graph(Node *node, int stage_num, int node_num, int stage_idx,
                     int node_idx, int idx, int N) {
  // TODO(angiebird): Simplify and clarify this function

  int i = 2 * N / (1 << (idx / 2));
  int max_bit =
      get_max_bit(i / 2) - 1;  // the max_bit counts on i/2 instead of N here
  int N_over_i = 2 << (idx / 2);

  for (int nj = 0; nj < N / 2; nj += N_over_i) {
    int j = nj / (N_over_i);
    int kj = bitwise_reverse(i / 4 + j, max_bit);
    // printf("kj = %d\n", kj);

    // I_N/2i   --- 0
    int offset = nj;
    for (int ni = 0; ni < N_over_i / 4; ni++) {
      int out = node_idx + offset + ni;
      int in = out;
      connect_node(node, stage_num, node_num, stage_idx + 1, out, in, 1, in, 0);
    }

    // -C_Kj/i --- S_Kj/i
    offset += N_over_i / 4;
    for (int ni = 0; ni < N_over_i / 4; ni++) {
      int out = node_idx + offset + ni;
      int in0 = out;
      double w0 = -cos(kj * PI / i);
      int in1 = N - (offset + ni) - 1 + node_idx;
      double w1 = sin(kj * PI / i);
      connect_node(node, stage_num, node_num, stage_idx + 1, out, in0, w0, in1,
                   w1);
    }

    // S_kj/i  --- -C_Kj/i
    offset += N_over_i / 4;
    for (int ni = 0; ni < N_over_i / 4; ni++) {
      int out = node_idx + offset + ni;
      int in0 = out;
      double w0 = -sin(kj * PI / i);
      int in1 = N - (offset + ni) - 1 + node_idx;
      double w1 = -cos(kj * PI / i);
      connect_node(node, stage_num, node_num, stage_idx + 1, out, in0, w0, in1,
                   w1);
    }

    // I_N/2i   --- 0
    offset += N_over_i / 4;
    for (int ni = 0; ni < N_over_i / 4; ni++) {
      int out = node_idx + offset + ni;
      int in = out;
      connect_node(node, stage_num, node_num, stage_idx + 1, out, in, 1, in, 0);
    }
  }

  for (int nj = N / 2; nj < N; nj += N_over_i) {
    int j = nj / N_over_i;
    int kj = bitwise_reverse(i / 4 + j, max_bit);

    // I_N/2i --- 0
    int offset = nj;
    for (int ni = 0; ni < N_over_i / 4; ni++) {
      int out = node_idx + offset + ni;
      int in = out;
      connect_node(node, stage_num, node_num, stage_idx + 1, out, in, 1, in, 0);
    }

    // C_kj/i --- -S_Kj/i
    offset += N_over_i / 4;
    for (int ni = 0; ni < N_over_i / 4; ni++) {
      int out = node_idx + offset + ni;
      int in0 = out;
      double w0 = cos(kj * PI / i);
      int in1 = N - (offset + ni) - 1 + node_idx;
      double w1 = -sin(kj * PI / i);
      connect_node(node, stage_num, node_num, stage_idx + 1, out, in0, w0, in1,
                   w1);
    }

    // S_kj/i --- C_Kj/i
    offset += N_over_i / 4;
    for (int ni = 0; ni < N_over_i / 4; ni++) {
      int out = node_idx + offset + ni;
      int in0 = out;
      double w0 = sin(kj * PI / i);
      int in1 = N - (offset + ni) - 1 + node_idx;
      double w1 = cos(kj * PI / i);
      connect_node(node, stage_num, node_num, stage_idx + 1, out, in0, w0, in1,
                   w1);
    }

    // I_N/2i --- 0
    offset += N_over_i / 4;
    for (int ni = 0; ni < N_over_i / 4; ni++) {
      int out = node_idx + offset + ni;
      int in = out;
      connect_node(node, stage_num, node_num, stage_idx + 1, out, in, 1, in, 0);
    }
  }
}

void gen_type4_graph(Node *node, int stage_num, int node_num, int stage_idx,
                     int node_idx, int idx, int N) {
  int B_size = 1 << ((idx + 1) / 2);
  for (int ni = 0; ni < N; ni += B_size) {
    gen_B_graph(node, stage_num, node_num, stage_idx, node_idx + ni, B_size,
                (ni / B_size) % 2);
  }
}

void gen_R_graph(Node *node, int stage_num, int node_num, int stage_idx,
                 int node_idx, int N) {
  int max_idx = 2 * (get_max_bit(N) + 1) - 3;
  for (int idx = 0; idx < max_idx; idx++) {
    int s = stage_idx + max_idx - idx - 1;
    if (idx == 0) {
      // type 1
      gen_type1_graph(node, stage_num, node_num, s, node_idx, N);
    } else if (idx == max_idx - 1) {
      // type 2
      gen_type2_graph(node, stage_num, node_num, s, node_idx, N);
    } else if ((idx + 1) % 2 == 0) {
      // type 4
      gen_type4_graph(node, stage_num, node_num, s, node_idx, idx, N);
    } else if ((idx + 1) % 2 == 1) {
      // type 3
      gen_type3_graph(node, stage_num, node_num, s, node_idx, idx, N);
    } else {
      printf("check gen_R_graph()\n");
    }
  }
}

void gen_DCT_graph(Node *node, int stage_num, int node_num, int stage_idx,
                   int node_idx, int N) {
  if (N > 2) {
    gen_B_graph(node, stage_num, node_num, stage_idx, node_idx, N, 0);
    gen_DCT_graph(node, stage_num, node_num, stage_idx + 1, node_idx, N / 2);
    gen_R_graph(node, stage_num, node_num, stage_idx + 1, node_idx + N / 2,
                N / 2);
  } else {
    // generate dct_2
    connect_node(node, stage_num, node_num, stage_idx + 1, node_idx, node_idx,
                 cos(PI / 4), node_idx + 1, cos(PI / 4));
    connect_node(node, stage_num, node_num, stage_idx + 1, node_idx + 1,
                 node_idx + 1, -cos(PI / 4), node_idx, cos(PI / 4));
  }
}

int get_dct_stage_num(int size) { return 2 * get_max_bit(size); }

void gen_DCT_graph_1d(Node *node, int stage_num, int node_num, int stage_idx,
                      int node_idx, int dct_node_num) {
  gen_DCT_graph(node, stage_num, node_num, stage_idx, node_idx, dct_node_num);
  int dct_stage_num = get_dct_stage_num(dct_node_num);
  gen_P_graph(node, stage_num, node_num, stage_idx + dct_stage_num - 2,
              node_idx, dct_node_num);
}

void gen_adst_B_graph(Node *node, int stage_num, int node_num, int stage_idx,
                      int node_idx, int adst_idx) {
  int size = 1 << (adst_idx + 1);
  for (int ni = 0; ni < size / 2; ni++) {
    int nOut = node_idx + ni;
    int nIn = nOut + size / 2;
    connect_node(node, stage_num, node_num, stage_idx + 1, nOut, nOut, 1, nIn,
                 1);
    // printf("nOut: %d nIn: %d\n", nOut, nIn);
  }
  for (int ni = size / 2; ni < size; ni++) {
    int nOut = node_idx + ni;
    int nIn = nOut - size / 2;
    connect_node(node, stage_num, node_num, stage_idx + 1, nOut, nOut, -1, nIn,
                 1);
    // printf("ndctOut: %d nIn: %d\n", nOut, nIn);
  }
}

void gen_adst_U_graph(Node *node, int stage_num, int node_num, int stage_idx,
                      int node_idx, int adst_idx, int adst_node_num) {
  int size = 1 << (adst_idx + 1);
  for (int ni = 0; ni < adst_node_num; ni += size) {
    gen_adst_B_graph(node, stage_num, node_num, stage_idx, node_idx + ni,
                     adst_idx);
  }
}

void gen_adst_T_graph(Node *node, int stage_num, int node_num, int stage_idx,
                      int node_idx, double freq) {
  connect_node(node, stage_num, node_num, stage_idx + 1, node_idx, node_idx,
               cos(freq * PI), node_idx + 1, sin(freq * PI));
  connect_node(node, stage_num, node_num, stage_idx + 1, node_idx + 1,
               node_idx + 1, -cos(freq * PI), node_idx, sin(freq * PI));
}

void gen_adst_E_graph(Node *node, int stage_num, int node_num, int stage_idx,
                      int node_idx, int adst_idx) {
  int size = 1 << (adst_idx);
  for (int i = 0; i < size / 2; i++) {
    int ni = i * 2;
    double fi = (1 + 4 * i) * 1.0 / (1 << (adst_idx + 1));
    gen_adst_T_graph(node, stage_num, node_num, stage_idx, node_idx + ni, fi);
  }
}

void gen_adst_V_graph(Node *node, int stage_num, int node_num, int stage_idx,
                      int node_idx, int adst_idx, int adst_node_num) {
  int size = 1 << (adst_idx);
  for (int i = 0; i < adst_node_num / size; i++) {
    if (i % 2 == 1) {
      int ni = i * size;
      gen_adst_E_graph(node, stage_num, node_num, stage_idx, node_idx + ni,
                       adst_idx);
    }
  }
}
void gen_adst_VJ_graph(Node *node, int stage_num, int node_num, int stage_idx,
                       int node_idx, int adst_node_num) {
  for (int i = 0; i < adst_node_num / 2; i++) {
    int ni = i * 2;
    double fi = (1 + 4 * i) * 1.0 / (4 * adst_node_num);
    gen_adst_T_graph(node, stage_num, node_num, stage_idx, node_idx + ni, fi);
  }
}
void gen_adst_Q_graph(Node *node, int stage_num, int node_num, int stage_idx,
                      int node_idx, int adst_node_num) {
  // reverse order when idx is 1, 3, 5, 7 ...
  // example of adst_node_num = 8:
  //   0 1 2 3 4 5 6 7
  // --> 0 7 2 5 4 3 6 1
  for (int ni = 0; ni < adst_node_num; ni++) {
    if (ni % 2 == 0) {
      int out = node_idx + ni;
      connect_node(node, stage_num, node_num, stage_idx + 1, out, out, 1, out,
                   0);
    } else {
      int out = node_idx + ni;
      int in = node_idx + adst_node_num - ni;
      connect_node(node, stage_num, node_num, stage_idx + 1, out, in, 1, in, 0);
    }
  }
}
void gen_adst_Ibar_graph(Node *node, int stage_num, int node_num, int stage_idx,
                         int node_idx, int adst_node_num) {
  // reverse order
  // 0 1 2 3 --> 3 2 1 0
  for (int ni = 0; ni < adst_node_num; ni++) {
    int out = node_idx + ni;
    int in = node_idx + adst_node_num - ni - 1;
    connect_node(node, stage_num, node_num, stage_idx + 1, out, in, 1, in, 0);
  }
}

int get_Q_out2in(int adst_node_num, int out) {
  int in;
  if (out % 2 == 0) {
    in = out;
  } else {
    in = adst_node_num - out;
  }
  return in;
}

int get_Ibar_out2in(int adst_node_num, int out) {
  return adst_node_num - out - 1;
}

void gen_adst_IbarQ_graph(Node *node, int stage_num, int node_num,
                          int stage_idx, int node_idx, int adst_node_num) {
  // in -> Ibar -> Q -> out
  for (int ni = 0; ni < adst_node_num; ni++) {
    int out = node_idx + ni;
    int in = node_idx +
             get_Ibar_out2in(adst_node_num, get_Q_out2in(adst_node_num, ni));
    connect_node(node, stage_num, node_num, stage_idx + 1, out, in, 1, in, 0);
  }
}

void gen_adst_D_graph(Node *node, int stage_num, int node_num, int stage_idx,
                      int node_idx, int adst_node_num) {
  // reverse order
  for (int ni = 0; ni < adst_node_num; ni++) {
    int out = node_idx + ni;
    int in = out;
    if (ni % 2 == 0) {
      connect_node(node, stage_num, node_num, stage_idx + 1, out, in, 1, in, 0);
    } else {
      connect_node(node, stage_num, node_num, stage_idx + 1, out, in, -1, in,
                   0);
    }
  }
}

int get_hadamard_idx(int x, int adst_node_num) {
  int max_bit = get_max_bit(adst_node_num - 1);
  x = bitwise_reverse(x, max_bit);

  // gray code
  int c = x & 1;
  int p = x & 1;
  int y = c;

  for (int i = 1; i <= max_bit; i++) {
    p = c;
    c = (x >> i) & 1;
    y += (c ^ p) << i;
  }
  return y;
}

void gen_adst_Ht_graph(Node *node, int stage_num, int node_num, int stage_idx,
                       int node_idx, int adst_node_num) {
  for (int ni = 0; ni < adst_node_num; ni++) {
    int out = node_idx + ni;
    int in = node_idx + get_hadamard_idx(ni, adst_node_num);
    connect_node(node, stage_num, node_num, stage_idx + 1, out, in, 1, in, 0);
  }
}

void gen_adst_HtD_graph(Node *node, int stage_num, int node_num, int stage_idx,
                        int node_idx, int adst_node_num) {
  for (int ni = 0; ni < adst_node_num; ni++) {
    int out = node_idx + ni;
    int in = node_idx + get_hadamard_idx(ni, adst_node_num);
    double inW;
    if (ni % 2 == 0)
      inW = 1;
    else
      inW = -1;
    connect_node(node, stage_num, node_num, stage_idx + 1, out, in, inW, in, 0);
  }
}

int get_adst_stage_num(int adst_node_num) {
  return 2 * get_max_bit(adst_node_num) + 2;
}

int gen_iadst_graph(Node *node, int stage_num, int node_num, int stage_idx,
                    int node_idx, int adst_node_num) {
  int max_bit = get_max_bit(adst_node_num);
  int si = 0;
  gen_adst_IbarQ_graph(node, stage_num, node_num, stage_idx + si, node_idx,
                       adst_node_num);
  si++;
  gen_adst_VJ_graph(node, stage_num, node_num, stage_idx + si, node_idx,
                    adst_node_num);
  si++;
  for (int adst_idx = max_bit - 1; adst_idx >= 1; adst_idx--) {
    gen_adst_U_graph(node, stage_num, node_num, stage_idx + si, node_idx,
                     adst_idx, adst_node_num);
    si++;
    gen_adst_V_graph(node, stage_num, node_num, stage_idx + si, node_idx,
                     adst_idx, adst_node_num);
    si++;
  }
  gen_adst_HtD_graph(node, stage_num, node_num, stage_idx + si, node_idx,
                     adst_node_num);
  si++;
  return si + 1;
}

int gen_adst_graph(Node *node, int stage_num, int node_num, int stage_idx,
                   int node_idx, int adst_node_num) {
  int hybrid_stage_num = get_hybrid_stage_num(TYPE_ADST, adst_node_num);
  // generate a adst tempNode
  Node *tempNode = new Node[hybrid_stage_num * adst_node_num];
  init_graph(tempNode, hybrid_stage_num, adst_node_num);
  int si = gen_iadst_graph(tempNode, hybrid_stage_num, adst_node_num, 0, 0,
                           adst_node_num);

  // tempNode's inverse graph to node[stage_idx][node_idx]
  gen_inv_graph(tempNode, hybrid_stage_num, adst_node_num, node, stage_num,
                node_num, stage_idx, node_idx);
  delete[] tempNode;
  return si;
}

void connect_layer_2d(Node *node, int stage_num, int node_num, int stage_idx,
                      int node_idx, int dct_node_num) {
  for (int first = 0; first < dct_node_num; first++) {
    for (int second = 0; second < dct_node_num; second++) {
      // int sIn = stage_idx;
      int sOut = stage_idx + 1;
      int nIn = node_idx + first * dct_node_num + second;
      int nOut = node_idx + second * dct_node_num + first;

      // printf("sIn: %d nIn: %d sOut: %d nOut: %d\n", sIn, nIn, sOut, nOut);

      connect_node(node, stage_num, node_num, sOut, nOut, nIn, 1, nIn, 0);
    }
  }
}

void connect_layer_2d_new(Node *node, int stage_num, int node_num,
                          int stage_idx, int node_idx, int dct_node_num0,
                          int dct_node_num1) {
  for (int i = 0; i < dct_node_num1; i++) {
    for (int j = 0; j < dct_node_num0; j++) {
      // int sIn = stage_idx;
      int sOut = stage_idx + 1;
      int nIn = node_idx + i * dct_node_num0 + j;
      int nOut = node_idx + j * dct_node_num1 + i;

      // printf("sIn: %d nIn: %d sOut: %d nOut: %d\n", sIn, nIn, sOut, nOut);

      connect_node(node, stage_num, node_num, sOut, nOut, nIn, 1, nIn, 0);
    }
  }
}

void gen_DCT_graph_2d(Node *node, int stage_num, int node_num, int stage_idx,
                      int node_idx, int dct_node_num) {
  int dct_stage_num = get_dct_stage_num(dct_node_num);
  // put 2 layers of dct_node_num DCTs on the graph
  for (int ni = 0; ni < dct_node_num; ni++) {
    gen_DCT_graph_1d(node, stage_num, node_num, stage_idx,
                     node_idx + ni * dct_node_num, dct_node_num);
    gen_DCT_graph_1d(node, stage_num, node_num, stage_idx + dct_stage_num,
                     node_idx + ni * dct_node_num, dct_node_num);
  }
  // connect first layer and second layer
  connect_layer_2d(node, stage_num, node_num, stage_idx + dct_stage_num - 1,
                   node_idx, dct_node_num);
}

int get_hybrid_stage_num(int type, int hybrid_node_num) {
  if (type == TYPE_DCT || type == TYPE_IDCT) {
    return get_dct_stage_num(hybrid_node_num);
  } else if (type == TYPE_ADST || type == TYPE_IADST) {
    return get_adst_stage_num(hybrid_node_num);
  }
  return 0;
}

int get_hybrid_2d_stage_num(int type0, int type1, int hybrid_node_num) {
  int stage_num = 0;
  stage_num += get_hybrid_stage_num(type0, hybrid_node_num);
  stage_num += get_hybrid_stage_num(type1, hybrid_node_num);
  return stage_num;
}

int get_hybrid_2d_stage_num_new(int type0, int type1, int hybrid_node_num0,
                                int hybrid_node_num1) {
  int stage_num = 0;
  stage_num += get_hybrid_stage_num(type0, hybrid_node_num0);
  stage_num += get_hybrid_stage_num(type1, hybrid_node_num1);
  return stage_num;
}

int get_hybrid_amplify_factor(int type, int hybrid_node_num) {
  return get_max_bit(hybrid_node_num) - 1;
}

void gen_hybrid_graph_1d(Node *node, int stage_num, int node_num, int stage_idx,
                         int node_idx, int hybrid_node_num, int type) {
  if (type == TYPE_DCT) {
    gen_DCT_graph_1d(node, stage_num, node_num, stage_idx, node_idx,
                     hybrid_node_num);
  } else if (type == TYPE_ADST) {
    gen_adst_graph(node, stage_num, node_num, stage_idx, node_idx,
                   hybrid_node_num);
  } else if (type == TYPE_IDCT) {
    int hybrid_stage_num = get_hybrid_stage_num(type, hybrid_node_num);
    // generate a dct tempNode
    Node *tempNode = new Node[hybrid_stage_num * hybrid_node_num];
    init_graph(tempNode, hybrid_stage_num, hybrid_node_num);
    gen_DCT_graph_1d(tempNode, hybrid_stage_num, hybrid_node_num, 0, 0,
                     hybrid_node_num);

    // tempNode's inverse graph to node[stage_idx][node_idx]
    gen_inv_graph(tempNode, hybrid_stage_num, hybrid_node_num, node, stage_num,
                  node_num, stage_idx, node_idx);
    delete[] tempNode;
  } else if (type == TYPE_IADST) {
    int hybrid_stage_num = get_hybrid_stage_num(type, hybrid_node_num);
    // generate a adst tempNode
    Node *tempNode = new Node[hybrid_stage_num * hybrid_node_num];
    init_graph(tempNode, hybrid_stage_num, hybrid_node_num);
    gen_adst_graph(tempNode, hybrid_stage_num, hybrid_node_num, 0, 0,
                   hybrid_node_num);

    // tempNode's inverse graph to node[stage_idx][node_idx]
    gen_inv_graph(tempNode, hybrid_stage_num, hybrid_node_num, node, stage_num,
                  node_num, stage_idx, node_idx);
    delete[] tempNode;
  }
}

void gen_hybrid_graph_2d(Node *node, int stage_num, int node_num, int stage_idx,
                         int node_idx, int hybrid_node_num, int type0,
                         int type1) {
  int hybrid_stage_num = get_hybrid_stage_num(type0, hybrid_node_num);

  for (int ni = 0; ni < hybrid_node_num; ni++) {
    gen_hybrid_graph_1d(node, stage_num, node_num, stage_idx,
                        node_idx + ni * hybrid_node_num, hybrid_node_num,
                        type0);
    gen_hybrid_graph_1d(node, stage_num, node_num, stage_idx + hybrid_stage_num,
                        node_idx + ni * hybrid_node_num, hybrid_node_num,
                        type1);
  }

  // connect first layer and second layer
  connect_layer_2d(node, stage_num, node_num, stage_idx + hybrid_stage_num - 1,
                   node_idx, hybrid_node_num);
}

void gen_hybrid_graph_2d_new(Node *node, int stage_num, int node_num,
                             int stage_idx, int node_idx, int hybrid_node_num0,
                             int hybrid_node_num1, int type0, int type1) {
  int hybrid_stage_num0 = get_hybrid_stage_num(type0, hybrid_node_num0);

  for (int ni = 0; ni < hybrid_node_num1; ni++) {
    gen_hybrid_graph_1d(node, stage_num, node_num, stage_idx,
                        node_idx + ni * hybrid_node_num0, hybrid_node_num0,
                        type0);
  }
  for (int ni = 0; ni < hybrid_node_num0; ni++) {
    gen_hybrid_graph_1d(
        node, stage_num, node_num, stage_idx + hybrid_stage_num0,
        node_idx + ni * hybrid_node_num1, hybrid_node_num1, type1);
  }

  // connect first layer and second layer
  connect_layer_2d_new(node, stage_num, node_num,
                       stage_idx + hybrid_stage_num0 - 1, node_idx,
                       hybrid_node_num0, hybrid_node_num1);
}

void gen_inv_graph(Node *node, int stage_num, int node_num, Node *invNode,
                   int inv_stage_num, int inv_node_num, int inv_stage_idx,
                   int inv_node_idx) {
  // clean up inNodeNum in invNode because of add_node
  for (int si = 1 + inv_stage_idx; si < inv_stage_idx + stage_num; si++) {
    for (int ni = inv_node_idx; ni < inv_node_idx + node_num; ni++) {
      int idx = get_idx(si, ni, inv_node_num);
      invNode[idx].inNodeNum = 0;
    }
  }
  // generate inverse graph of node on invNode
  for (int si = 1; si < stage_num; si++) {
    for (int ni = 0; ni < node_num; ni++) {
      int invSi = stage_num - si;
      int idx = get_idx(si, ni, node_num);
      for (int k = 0; k < node[idx].inNodeNum; k++) {
        int invNi = node[idx].inNodeIdx[k];
        add_node(invNode, inv_stage_num, inv_node_num, invSi + inv_stage_idx,
                 invNi + inv_node_idx, ni + inv_node_idx,
                 node[idx].inWeight[k]);
      }
    }
  }
}
