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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <string.h>

#include "tools/txfm_analyzer/txfm_graph.h"

typedef enum CODE_TYPE {
  CODE_TYPE_C,
  CODE_TYPE_SSE2,
  CODE_TYPE_SSE4_1
} CODE_TYPE;

int get_cos_idx(double value, int mod) {
  return round(acos(fabs(value)) / PI * mod);
}

char *cos_text_arr(double value, int mod, char *text, int size) {
  int num = get_cos_idx(value, mod);
  if (value < 0) {
    snprintf(text, size, "-cospi[%2d]", num);
  } else {
    snprintf(text, size, " cospi[%2d]", num);
  }

  if (num == 0)
    printf("v: %f -> %d/%d v==-1 is %d\n", value, num, mod, value == -1);

  return text;
}

char *cos_text_sse2(double w0, double w1, int mod, char *text, int size) {
  int idx0 = get_cos_idx(w0, mod);
  int idx1 = get_cos_idx(w1, mod);
  char p[] = "p";
  char n[] = "m";
  char *sgn0 = w0 < 0 ? n : p;
  char *sgn1 = w1 < 0 ? n : p;
  snprintf(text, size, "cospi_%s%02d_%s%02d", sgn0, idx0, sgn1, idx1);
  return text;
}

char *cos_text_sse4_1(double w, int mod, char *text, int size) {
  int idx = get_cos_idx(w, mod);
  char p[] = "p";
  char n[] = "m";
  char *sgn = w < 0 ? n : p;
  snprintf(text, size, "cospi_%s%02d", sgn, idx);
  return text;
}

void node_to_code_c(Node *node, const char *buf0, const char *buf1) {
  int cnt = 0;
  for (int i = 0; i < 2; i++) {
    if (fabs(node->inWeight[i]) == 1 || fabs(node->inWeight[i]) == 0) cnt++;
  }
  if (cnt == 2) {
    int cnt2 = 0;
    printf("  %s[%d] =", buf1, node->nodeIdx);
    for (int i = 0; i < 2; i++) {
      if (fabs(node->inWeight[i]) == 1) {
        cnt2++;
      }
    }
    if (cnt2 == 2) {
      printf(" apply_value(");
    }
    int cnt1 = 0;
    for (int i = 0; i < 2; i++) {
      if (node->inWeight[i] == 1) {
        if (cnt1 > 0)
          printf(" + %s[%d]", buf0, node->inNodeIdx[i]);
        else
          printf(" %s[%d]", buf0, node->inNodeIdx[i]);
        cnt1++;
      } else if (node->inWeight[i] == -1) {
        if (cnt1 > 0)
          printf(" - %s[%d]", buf0, node->inNodeIdx[i]);
        else
          printf("-%s[%d]", buf0, node->inNodeIdx[i]);
        cnt1++;
      }
    }
    if (cnt2 == 2) {
      printf(", stage_range[stage])");
    }
    printf(";\n");
  } else {
    char w0[100];
    char w1[100];
    printf(
        "  %s[%d] = half_btf(%s, %s[%d], %s, %s[%d], "
        "cos_bit);\n",
        buf1, node->nodeIdx, cos_text_arr(node->inWeight[0], COS_MOD, w0, 100),
        buf0, node->inNodeIdx[0],
        cos_text_arr(node->inWeight[1], COS_MOD, w1, 100), buf0,
        node->inNodeIdx[1]);
  }
}

void gen_code_c(Node *node, int stage_num, int node_num, TYPE_TXFM type) {
  char *fun_name = new char[100];
  get_fun_name(fun_name, 100, type, node_num);

  printf("\n");
  printf(
      "void av1_%s(const int32_t *input, int32_t *output, int8_t cos_bit, "
      "const int8_t* stage_range) "
      "{\n",
      fun_name);
  printf("  assert(output != input);\n");
  printf("  const int32_t size = %d;\n", node_num);
  printf("  const int32_t *cospi = cospi_arr(cos_bit);\n");
  printf("\n");

  printf("  int32_t stage = 0;\n");
  printf("  int32_t *bf0, *bf1;\n");
  printf("  int32_t step[%d];\n", node_num);

  const char *buf0 = "bf0";
  const char *buf1 = "bf1";
  const char *input = "input";

  int si = 0;
  printf("\n");
  printf("  // stage %d;\n", si);
  printf("  apply_range(stage, input, %s, size, stage_range[stage]);\n", input);

  si = 1;
  printf("\n");
  printf("  // stage %d;\n", si);
  printf("  stage++;\n");
  if (si % 2 == (stage_num - 1) % 2) {
    printf("  %s = output;\n", buf1);
  } else {
    printf("  %s = step;\n", buf1);
  }

  for (int ni = 0; ni < node_num; ni++) {
    int idx = get_idx(si, ni, node_num);
    node_to_code_c(node + idx, input, buf1);
  }

  printf("  range_check_buf(stage, input, bf1, size, stage_range[stage]);\n");

  for (int si = 2; si < stage_num; si++) {
    printf("\n");
    printf("  // stage %d\n", si);
    printf("  stage++;\n");
    if (si % 2 == (stage_num - 1) % 2) {
      printf("  %s = step;\n", buf0);
      printf("  %s = output;\n", buf1);
    } else {
      printf("  %s = output;\n", buf0);
      printf("  %s = step;\n", buf1);
    }

    // computation code
    for (int ni = 0; ni < node_num; ni++) {
      int idx = get_idx(si, ni, node_num);
      node_to_code_c(node + idx, buf0, buf1);
    }

    if (si != stage_num - 1) {
      printf(
          "  range_check_buf(stage, input, bf1, size, stage_range[stage]);\n");
    }
  }
  printf("  apply_range(stage, input, output, size, stage_range[stage]);\n");
  printf("}\n");
}

void single_node_to_code_sse2(Node *node, const char *buf0, const char *buf1) {
  printf("  %s[%2d] =", buf1, node->nodeIdx);
  if (node->inWeight[0] == 1 && node->inWeight[1] == 1) {
    printf(" _mm_adds_epi16(%s[%d], %s[%d])", buf0, node->inNodeIdx[0], buf0,
           node->inNodeIdx[1]);
  } else if (node->inWeight[0] == 1 && node->inWeight[1] == -1) {
    printf(" _mm_subs_epi16(%s[%d], %s[%d])", buf0, node->inNodeIdx[0], buf0,
           node->inNodeIdx[1]);
  } else if (node->inWeight[0] == -1 && node->inWeight[1] == 1) {
    printf(" _mm_subs_epi16(%s[%d], %s[%d])", buf0, node->inNodeIdx[1], buf0,
           node->inNodeIdx[0]);
  } else if (node->inWeight[0] == 1 && node->inWeight[1] == 0) {
    printf(" %s[%d]", buf0, node->inNodeIdx[0]);
  } else if (node->inWeight[0] == 0 && node->inWeight[1] == 1) {
    printf(" %s[%d]", buf0, node->inNodeIdx[1]);
  } else if (node->inWeight[0] == -1 && node->inWeight[1] == 0) {
    printf(" _mm_subs_epi16(__zero, %s[%d])", buf0, node->inNodeIdx[0]);
  } else if (node->inWeight[0] == 0 && node->inWeight[1] == -1) {
    printf(" _mm_subs_epi16(__zero, %s[%d])", buf0, node->inNodeIdx[1]);
  }
  printf(";\n");
}

void pair_node_to_code_sse2(Node *node, Node *partnerNode, const char *buf0,
                            const char *buf1) {
  char temp0[100];
  char temp1[100];
  // btf_16_sse2_type0(w0, w1, in0, in1, out0, out1)
  if (node->inNodeIdx[0] != partnerNode->inNodeIdx[0])
    printf("  btf_16_sse2(%s, %s, %s[%d], %s[%d], %s[%d], %s[%d]);\n",
           cos_text_sse2(node->inWeight[0], node->inWeight[1], COS_MOD, temp0,
                         100),
           cos_text_sse2(partnerNode->inWeight[1], partnerNode->inWeight[0],
                         COS_MOD, temp1, 100),
           buf0, node->inNodeIdx[0], buf0, node->inNodeIdx[1], buf1,
           node->nodeIdx, buf1, partnerNode->nodeIdx);
  else
    printf("  btf_16_sse2(%s, %s, %s[%d], %s[%d], %s[%d], %s[%d]);\n",
           cos_text_sse2(node->inWeight[0], node->inWeight[1], COS_MOD, temp0,
                         100),
           cos_text_sse2(partnerNode->inWeight[0], partnerNode->inWeight[1],
                         COS_MOD, temp1, 100),
           buf0, node->inNodeIdx[0], buf0, node->inNodeIdx[1], buf1,
           node->nodeIdx, buf1, partnerNode->nodeIdx);
}

Node *get_partner_node(Node *node) {
  int diff = node->inNode[1]->nodeIdx - node->nodeIdx;
  return node + diff;
}

void node_to_code_sse2(Node *node, const char *buf0, const char *buf1) {
  int cnt = 0;
  int cnt1 = 0;
  if (node->visited == 0) {
    node->visited = 1;
    for (int i = 0; i < 2; i++) {
      if (fabs(node->inWeight[i]) == 1 || fabs(node->inWeight[i]) == 0) cnt++;
      if (fabs(node->inWeight[i]) == 1) cnt1++;
    }
    if (cnt == 2) {
      if (cnt1 == 2) {
        // has a partner
        Node *partnerNode = get_partner_node(node);
        partnerNode->visited = 1;
        single_node_to_code_sse2(node, buf0, buf1);
        single_node_to_code_sse2(partnerNode, buf0, buf1);
      } else {
        single_node_to_code_sse2(node, buf0, buf1);
      }
    } else {
      Node *partnerNode = get_partner_node(node);
      partnerNode->visited = 1;
      pair_node_to_code_sse2(node, partnerNode, buf0, buf1);
    }
  }
}

void gen_cospi_list_sse2(Node *node, int stage_num, int node_num) {
  int visited[65][65][2][2];
  memset(visited, 0, sizeof(visited));
  char text[100];
  char text1[100];
  char text2[100];
  int size = 100;
  printf("\n");
  for (int si = 1; si < stage_num; si++) {
    for (int ni = 0; ni < node_num; ni++) {
      int idx = get_idx(si, ni, node_num);
      int cnt = 0;
      Node *node0 = node + idx;
      if (node0->visited == 0) {
        node0->visited = 1;
        for (int i = 0; i < 2; i++) {
          if (fabs(node0->inWeight[i]) == 1 || fabs(node0->inWeight[i]) == 0)
            cnt++;
        }
        if (cnt != 2) {
          {
            double w0 = node0->inWeight[0];
            double w1 = node0->inWeight[1];
            int idx0 = get_cos_idx(w0, COS_MOD);
            int idx1 = get_cos_idx(w1, COS_MOD);
            int sgn0 = w0 < 0 ? 1 : 0;
            int sgn1 = w1 < 0 ? 1 : 0;

            if (!visited[idx0][idx1][sgn0][sgn1]) {
              visited[idx0][idx1][sgn0][sgn1] = 1;
              printf("  __m128i %s = pair_set_epi16(%s, %s);\n",
                     cos_text_sse2(w0, w1, COS_MOD, text, size),
                     cos_text_arr(w0, COS_MOD, text1, size),
                     cos_text_arr(w1, COS_MOD, text2, size));
            }
          }
          Node *node1 = get_partner_node(node0);
          node1->visited = 1;
          if (node1->inNode[0]->nodeIdx != node0->inNode[0]->nodeIdx) {
            double w0 = node1->inWeight[0];
            double w1 = node1->inWeight[1];
            int idx0 = get_cos_idx(w0, COS_MOD);
            int idx1 = get_cos_idx(w1, COS_MOD);
            int sgn0 = w0 < 0 ? 1 : 0;
            int sgn1 = w1 < 0 ? 1 : 0;

            if (!visited[idx1][idx0][sgn1][sgn0]) {
              visited[idx1][idx0][sgn1][sgn0] = 1;
              printf("  __m128i %s = pair_set_epi16(%s, %s);\n",
                     cos_text_sse2(w1, w0, COS_MOD, text, size),
                     cos_text_arr(w1, COS_MOD, text1, size),
                     cos_text_arr(w0, COS_MOD, text2, size));
            }
          } else {
            double w0 = node1->inWeight[0];
            double w1 = node1->inWeight[1];
            int idx0 = get_cos_idx(w0, COS_MOD);
            int idx1 = get_cos_idx(w1, COS_MOD);
            int sgn0 = w0 < 0 ? 1 : 0;
            int sgn1 = w1 < 0 ? 1 : 0;

            if (!visited[idx0][idx1][sgn0][sgn1]) {
              visited[idx0][idx1][sgn0][sgn1] = 1;
              printf("  __m128i %s = pair_set_epi16(%s, %s);\n",
                     cos_text_sse2(w0, w1, COS_MOD, text, size),
                     cos_text_arr(w0, COS_MOD, text1, size),
                     cos_text_arr(w1, COS_MOD, text2, size));
            }
          }
        }
      }
    }
  }
}

void gen_code_sse2(Node *node, int stage_num, int node_num, TYPE_TXFM type) {
  char *fun_name = new char[100];
  get_fun_name(fun_name, 100, type, node_num);

  printf("\n");
  printf(
      "void %s_sse2(const __m128i *input, __m128i *output, int8_t cos_bit) "
      "{\n",
      fun_name);

  printf("  const int32_t* cospi = cospi_arr(cos_bit);\n");
  printf("  const __m128i __zero = _mm_setzero_si128();\n");
  printf("  const __m128i __rounding = _mm_set1_epi32(1 << (cos_bit - 1));\n");

  graph_reset_visited(node, stage_num, node_num);
  gen_cospi_list_sse2(node, stage_num, node_num);
  graph_reset_visited(node, stage_num, node_num);
  for (int si = 1; si < stage_num; si++) {
    char in[100];
    char out[100];
    printf("\n");
    printf("  // stage %d\n", si);
    if (si == 1)
      snprintf(in, 100, "%s", "input");
    else
      snprintf(in, 100, "x%d", si - 1);
    if (si == stage_num - 1) {
      snprintf(out, 100, "%s", "output");
    } else {
      snprintf(out, 100, "x%d", si);
      printf("  __m128i %s[%d];\n", out, node_num);
    }
    // computation code
    for (int ni = 0; ni < node_num; ni++) {
      int idx = get_idx(si, ni, node_num);
      node_to_code_sse2(node + idx, in, out);
    }
  }

  printf("}\n");
}
void gen_cospi_list_sse4_1(Node *node, int stage_num, int node_num) {
  int visited[65][2];
  memset(visited, 0, sizeof(visited));
  char text[100];
  char text1[100];
  int size = 100;
  printf("\n");
  for (int si = 1; si < stage_num; si++) {
    for (int ni = 0; ni < node_num; ni++) {
      int idx = get_idx(si, ni, node_num);
      Node *node0 = node + idx;
      if (node0->visited == 0) {
        int cnt = 0;
        node0->visited = 1;
        for (int i = 0; i < 2; i++) {
          if (fabs(node0->inWeight[i]) == 1 || fabs(node0->inWeight[i]) == 0)
            cnt++;
        }
        if (cnt != 2) {
          for (int i = 0; i < 2; i++) {
            if (fabs(node0->inWeight[i]) != 1 &&
                fabs(node0->inWeight[i]) != 0) {
              double w = node0->inWeight[i];
              int idx = get_cos_idx(w, COS_MOD);
              int sgn = w < 0 ? 1 : 0;

              if (!visited[idx][sgn]) {
                visited[idx][sgn] = 1;
                printf("  __m128i %s = _mm_set1_epi32(%s);\n",
                       cos_text_sse4_1(w, COS_MOD, text, size),
                       cos_text_arr(w, COS_MOD, text1, size));
              }
            }
          }
          Node *node1 = get_partner_node(node0);
          node1->visited = 1;
        }
      }
    }
  }
}

void single_node_to_code_sse4_1(Node *node, const char *buf0,
                                const char *buf1) {
  printf("  %s[%2d] =", buf1, node->nodeIdx);
  if (node->inWeight[0] == 1 && node->inWeight[1] == 1) {
    printf(" _mm_add_epi32(%s[%d], %s[%d])", buf0, node->inNodeIdx[0], buf0,
           node->inNodeIdx[1]);
  } else if (node->inWeight[0] == 1 && node->inWeight[1] == -1) {
    printf(" _mm_sub_epi32(%s[%d], %s[%d])", buf0, node->inNodeIdx[0], buf0,
           node->inNodeIdx[1]);
  } else if (node->inWeight[0] == -1 && node->inWeight[1] == 1) {
    printf(" _mm_sub_epi32(%s[%d], %s[%d])", buf0, node->inNodeIdx[1], buf0,
           node->inNodeIdx[0]);
  } else if (node->inWeight[0] == 1 && node->inWeight[1] == 0) {
    printf(" %s[%d]", buf0, node->inNodeIdx[0]);
  } else if (node->inWeight[0] == 0 && node->inWeight[1] == 1) {
    printf(" %s[%d]", buf0, node->inNodeIdx[1]);
  } else if (node->inWeight[0] == -1 && node->inWeight[1] == 0) {
    printf(" _mm_sub_epi32(__zero, %s[%d])", buf0, node->inNodeIdx[0]);
  } else if (node->inWeight[0] == 0 && node->inWeight[1] == -1) {
    printf(" _mm_sub_epi32(__zero, %s[%d])", buf0, node->inNodeIdx[1]);
  }
  printf(";\n");
}

void pair_node_to_code_sse4_1(Node *node, Node *partnerNode, const char *buf0,
                              const char *buf1) {
  char temp0[100];
  char temp1[100];
  if (node->inWeight[0] * partnerNode->inWeight[0] < 0) {
    /* type0
     * cos  sin
     * sin -cos
     */
    // btf_32_sse2_type0(w0, w1, in0, in1, out0, out1)
    // out0 = w0*in0 + w1*in1
    // out1 = -w0*in1 + w1*in0
    printf(
        "  btf_32_type0_sse4_1_new(%s, %s, %s[%d], %s[%d], %s[%d], %s[%d], "
        "__rounding, cos_bit);\n",
        cos_text_sse4_1(node->inWeight[0], COS_MOD, temp0, 100),
        cos_text_sse4_1(node->inWeight[1], COS_MOD, temp1, 100), buf0,
        node->inNodeIdx[0], buf0, node->inNodeIdx[1], buf1, node->nodeIdx, buf1,
        partnerNode->nodeIdx);
  } else {
    /* type1
     *  cos sin
     * -sin cos
     */
    // btf_32_sse2_type1(w0, w1, in0, in1, out0, out1)
    // out0 = w0*in0 + w1*in1
    // out1 = w0*in1 - w1*in0
    printf(
        "  btf_32_type1_sse4_1_new(%s, %s, %s[%d], %s[%d], %s[%d], %s[%d], "
        "__rounding, cos_bit);\n",
        cos_text_sse4_1(node->inWeight[0], COS_MOD, temp0, 100),
        cos_text_sse4_1(node->inWeight[1], COS_MOD, temp1, 100), buf0,
        node->inNodeIdx[0], buf0, node->inNodeIdx[1], buf1, node->nodeIdx, buf1,
        partnerNode->nodeIdx);
  }
}

void node_to_code_sse4_1(Node *node, const char *buf0, const char *buf1) {
  int cnt = 0;
  int cnt1 = 0;
  if (node->visited == 0) {
    node->visited = 1;
    for (int i = 0; i < 2; i++) {
      if (fabs(node->inWeight[i]) == 1 || fabs(node->inWeight[i]) == 0) cnt++;
      if (fabs(node->inWeight[i]) == 1) cnt1++;
    }
    if (cnt == 2) {
      if (cnt1 == 2) {
        // has a partner
        Node *partnerNode = get_partner_node(node);
        partnerNode->visited = 1;
        single_node_to_code_sse4_1(node, buf0, buf1);
        single_node_to_code_sse4_1(partnerNode, buf0, buf1);
      } else {
        single_node_to_code_sse2(node, buf0, buf1);
      }
    } else {
      Node *partnerNode = get_partner_node(node);
      partnerNode->visited = 1;
      pair_node_to_code_sse4_1(node, partnerNode, buf0, buf1);
    }
  }
}

void gen_code_sse4_1(Node *node, int stage_num, int node_num, TYPE_TXFM type) {
  char *fun_name = new char[100];
  get_fun_name(fun_name, 100, type, node_num);

  printf("\n");
  printf(
      "void %s_sse4_1(const __m128i *input, __m128i *output, int8_t cos_bit) "
      "{\n",
      fun_name);

  printf("  const int32_t* cospi = cospi_arr(cos_bit);\n");
  printf("  const __m128i __zero = _mm_setzero_si128();\n");
  printf("  const __m128i __rounding = _mm_set1_epi32(1 << (cos_bit - 1));\n");

  graph_reset_visited(node, stage_num, node_num);
  gen_cospi_list_sse4_1(node, stage_num, node_num);
  graph_reset_visited(node, stage_num, node_num);
  for (int si = 1; si < stage_num; si++) {
    char in[100];
    char out[100];
    printf("\n");
    printf("  // stage %d\n", si);
    if (si == 1)
      snprintf(in, 100, "%s", "input");
    else
      snprintf(in, 100, "x%d", si - 1);
    if (si == stage_num - 1) {
      snprintf(out, 100, "%s", "output");
    } else {
      snprintf(out, 100, "x%d", si);
      printf("  __m128i %s[%d];\n", out, node_num);
    }
    // computation code
    for (int ni = 0; ni < node_num; ni++) {
      int idx = get_idx(si, ni, node_num);
      node_to_code_sse4_1(node + idx, in, out);
    }
  }

  printf("}\n");
}

void gen_hybrid_code(CODE_TYPE code_type, TYPE_TXFM txfm_type, int node_num) {
  int stage_num = get_hybrid_stage_num(txfm_type, node_num);

  Node *node = new Node[node_num * stage_num];
  init_graph(node, stage_num, node_num);

  gen_hybrid_graph_1d(node, stage_num, node_num, 0, 0, node_num, txfm_type);

  switch (code_type) {
    case CODE_TYPE_C: gen_code_c(node, stage_num, node_num, txfm_type); break;
    case CODE_TYPE_SSE2:
      gen_code_sse2(node, stage_num, node_num, txfm_type);
      break;
    case CODE_TYPE_SSE4_1:
      gen_code_sse4_1(node, stage_num, node_num, txfm_type);
      break;
  }

  delete[] node;
}

int main(int argc, char **argv) {
  CODE_TYPE code_type = CODE_TYPE_SSE4_1;
  for (int txfm_type = TYPE_DCT; txfm_type < TYPE_LAST; txfm_type++) {
    for (int node_num = 4; node_num <= 64; node_num *= 2) {
      gen_hybrid_code(code_type, (TYPE_TXFM)txfm_type, node_num);
    }
  }
  return 0;
}
