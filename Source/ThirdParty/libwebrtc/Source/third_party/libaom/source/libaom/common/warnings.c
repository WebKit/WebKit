/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "common/warnings.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aom/aom_encoder.h"
#include "apps/aomenc.h"
#include "common/tools_common.h"

static const char quantizer_warning_string[] =
    "Bad quantizer values. Quantizer values should not be equal, and should "
    "differ by at least 8.";

struct WarningListNode {
  const char *warning_string;
  struct WarningListNode *next_warning;
};

struct WarningList {
  struct WarningListNode *warning_node;
};

static void add_warning(const char *warning_string,
                        struct WarningList *warning_list) {
  struct WarningListNode **node = &warning_list->warning_node;

  struct WarningListNode *new_node = malloc(sizeof(*new_node));
  if (new_node == NULL) {
    fatal("Unable to allocate warning node.");
  }

  new_node->warning_string = warning_string;
  new_node->next_warning = NULL;

  while (*node != NULL) node = &(*node)->next_warning;

  *node = new_node;
}

static void free_warning_list(struct WarningList *warning_list) {
  while (warning_list->warning_node != NULL) {
    struct WarningListNode *const node = warning_list->warning_node;
    warning_list->warning_node = node->next_warning;
    free(node);
  }
}

static int continue_prompt(int num_warnings) {
  int c;
  fprintf(stderr,
          "%d encoder configuration warning(s). Continue? (y to continue) ",
          num_warnings);
  c = getchar();
  return c == 'y';
}

static void check_quantizer(int min_q, int max_q,
                            struct WarningList *warning_list) {
  const int lossless = min_q == 0 && max_q == 0;
  if (!lossless && (min_q == max_q || abs(max_q - min_q) < 8))
    add_warning(quantizer_warning_string, warning_list);
}

void check_encoder_config(int disable_prompt,
                          const struct AvxEncoderConfig *global_config,
                          const struct aom_codec_enc_cfg *stream_config) {
  int num_warnings = 0;
  struct WarningListNode *warning = NULL;
  struct WarningList warning_list = { 0 };
  (void)global_config;
  check_quantizer(stream_config->rc_min_quantizer,
                  stream_config->rc_max_quantizer, &warning_list);
  /* Count and print warnings. */
  for (warning = warning_list.warning_node; warning != NULL;
       warning = warning->next_warning, ++num_warnings) {
    aom_tools_warn("%s", warning->warning_string);
  }

  free_warning_list(&warning_list);

  if (num_warnings) {
    if (!disable_prompt && !continue_prompt(num_warnings)) exit(EXIT_FAILURE);
  }
}
