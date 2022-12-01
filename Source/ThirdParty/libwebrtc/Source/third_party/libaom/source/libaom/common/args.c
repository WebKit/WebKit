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

#include "common/args.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "aom/aom_integer.h"
#include "aom_ports/msvc.h"
#include "aom/aom_codec.h"
#include "common/tools_common.h"

static const char kSbSizeWarningString[] =
    "super_block_size has to be 64 or 128.";
static const char kMinpartWarningString[] =
    "min_partition_size has to be smaller or equal to max_partition_size.";
static const char kMaxpartWarningString[] =
    "max_partition_size has to be smaller or equal to super_block_size.";

static char *ignore_front_spaces(const char *str) {
  while (str[0] == ' ' || str[0] == '\t') ++str;
  return (char *)str;
}

static void ignore_end_spaces(char *str) {
  char *end = str + strlen(str);
  while (end > str && (end[0] == ' ' || end[0] == '\t' || end[0] == '\n' ||
                       end[0] == '\r' || end[0] == '\0'))
    --end;
  if (end >= str) end[1] = '\0';
}

int parse_cfg(const char *file, cfg_options_t *config) {
  char line[1024 * 10];
  FILE *f = fopen(file, "r");
  if (!f) return 1;

#define GET_PARAMS(field)          \
  if (strcmp(left, #field) == 0) { \
    config->field = atoi(right);   \
    continue;                      \
  }

  while (fgets(line, sizeof(line) - 1, f)) {
    char *actual_line = ignore_front_spaces(line);
    char *left, *right, *comment;
    size_t length = strlen(actual_line);

    if (length == 0 || actual_line[0] == '#') continue;
    right = strchr(actual_line, '=');
    if (right == NULL) continue;
    right[0] = '\0';

    left = ignore_front_spaces(actual_line);
    right = ignore_front_spaces(right + 1);

    comment = strchr(right, '#');
    if (comment != NULL) comment[0] = '\0';

    ignore_end_spaces(left);
    ignore_end_spaces(right);

    GET_PARAMS(super_block_size)
    GET_PARAMS(max_partition_size)
    GET_PARAMS(min_partition_size)
    GET_PARAMS(disable_ab_partition_type)
    GET_PARAMS(disable_rect_partition_type)
    GET_PARAMS(disable_1to4_partition_type)
    GET_PARAMS(disable_flip_idtx)
    GET_PARAMS(disable_cdef)
    GET_PARAMS(disable_lr)
    GET_PARAMS(disable_obmc)
    GET_PARAMS(disable_warp_motion)
    GET_PARAMS(disable_global_motion)
    GET_PARAMS(disable_dist_wtd_comp)
    GET_PARAMS(disable_diff_wtd_comp)
    GET_PARAMS(disable_inter_intra_comp)
    GET_PARAMS(disable_masked_comp)
    GET_PARAMS(disable_one_sided_comp)
    GET_PARAMS(disable_palette)
    GET_PARAMS(disable_intrabc)
    GET_PARAMS(disable_cfl)
    GET_PARAMS(disable_smooth_intra)
    GET_PARAMS(disable_filter_intra)
    GET_PARAMS(disable_dual_filter)
    GET_PARAMS(disable_intra_angle_delta)
    GET_PARAMS(disable_intra_edge_filter)
    GET_PARAMS(disable_tx_64x64)
    GET_PARAMS(disable_smooth_inter_intra)
    GET_PARAMS(disable_inter_inter_wedge)
    GET_PARAMS(disable_inter_intra_wedge)
    GET_PARAMS(disable_paeth_intra)
    GET_PARAMS(disable_trellis_quant)
    GET_PARAMS(disable_ref_frame_mv)
    GET_PARAMS(reduced_reference_set)
    GET_PARAMS(reduced_tx_type_set)

    fprintf(stderr, "\nInvalid parameter: %s", left);
    exit(-1);
  }

  if (config->super_block_size != 128 && config->super_block_size != 64) {
    fprintf(stderr, "\n%s", kSbSizeWarningString);
    exit(-1);
  }
  if (config->min_partition_size > config->max_partition_size) {
    fprintf(stderr, "\n%s", kMinpartWarningString);
    exit(-1);
  }
  if (config->max_partition_size > config->super_block_size) {
    fprintf(stderr, "\n%s", kMaxpartWarningString);
    exit(-1);
  }

  fclose(f);
  config->init_by_cfg_file = 1;

  return 0;
}

int arg_match(struct arg *arg_, const struct arg_def *def, char **argv) {
  char err_msg[ARG_ERR_MSG_MAX_LEN];
  int ret = arg_match_helper(arg_, def, argv, err_msg);
  if (err_msg[0] != '\0') {
    die("%s", err_msg);
  }
  return ret;
}

const char *arg_next(struct arg *arg) {
  if (arg->argv[0]) arg->argv += arg->argv_step;

  return *arg->argv;
}

char **argv_dup(int argc, const char **argv) {
  char **new_argv = malloc((argc + 1) * sizeof(*argv));
  if (!new_argv) return NULL;

  memcpy(new_argv, argv, argc * sizeof(*argv));
  new_argv[argc] = NULL;
  return new_argv;
}

void arg_show_usage(FILE *fp, const struct arg_def *const *defs) {
  for (; *defs; defs++) {
    const struct arg_def *def = *defs;
    char *short_val = def->has_val ? " <arg>" : "";
    char *long_val = def->has_val ? "=<arg>" : "";
    int n = 0;

    // Short options are indented with two spaces. Long options are indented
    // with 12 spaces.
    if (def->short_name && def->long_name) {
      char *comma = def->has_val ? "," : ",      ";

      n = fprintf(fp, "  -%s%s%s --%s%s", def->short_name, short_val, comma,
                  def->long_name, long_val);
    } else if (def->short_name)
      n = fprintf(fp, "  -%s%s", def->short_name, short_val);
    else if (def->long_name)
      n = fprintf(fp, "            --%s%s", def->long_name, long_val);

    // Descriptions are indented with 40 spaces. If an option is 40 characters
    // or longer, its description starts on the next line.
    if (n < 40)
      for (int i = 0; i < 40 - n; i++) fputc(' ', fp);
    else
      fputs("\n                                        ", fp);
    fprintf(fp, "%s\n", def->desc);

    if (def->enums) {
      const struct arg_enum_list *listptr;

      fprintf(fp, "  %-37s\t  ", "");

      for (listptr = def->enums; listptr->name; listptr++)
        fprintf(fp, "%s%s", listptr->name, listptr[1].name ? ", " : "\n");
    }
  }
}

unsigned int arg_parse_uint(const struct arg *arg) {
  char err_msg[ARG_ERR_MSG_MAX_LEN];
  unsigned int ret = arg_parse_uint_helper(arg, err_msg);
  if (err_msg[0] != '\0') {
    die("%s", err_msg);
  }
  return ret;
}

int arg_parse_int(const struct arg *arg) {
  char err_msg[ARG_ERR_MSG_MAX_LEN];
  int ret = arg_parse_int_helper(arg, err_msg);
  if (err_msg[0] != '\0') {
    die("%s", err_msg);
  }
  return ret;
}

struct aom_rational arg_parse_rational(const struct arg *arg) {
  char err_msg[ARG_ERR_MSG_MAX_LEN];
  struct aom_rational ret = arg_parse_rational_helper(arg, err_msg);
  if (err_msg[0] != '\0') {
    die("%s", err_msg);
  }
  return ret;
}

int arg_parse_enum(const struct arg *arg) {
  char err_msg[ARG_ERR_MSG_MAX_LEN];
  int ret = arg_parse_enum_helper(arg, err_msg);
  if (err_msg[0] != '\0') {
    die("%s", err_msg);
  }
  return ret;
}

int arg_parse_enum_or_int(const struct arg *arg) {
  char err_msg[ARG_ERR_MSG_MAX_LEN];
  int ret = arg_parse_enum_or_int_helper(arg, err_msg);
  if (err_msg[0] != '\0') {
    die("%s", err_msg);
  }
  return ret;
}

// parse a comma separated list of at most n integers
// return the number of elements in the list
int arg_parse_list(const struct arg *arg, int *list, int n) {
  char err_msg[ARG_ERR_MSG_MAX_LEN];
  int ret = arg_parse_list_helper(arg, list, n, err_msg);
  if (err_msg[0] != '\0') {
    die("%s", err_msg);
  }
  return ret;
}
