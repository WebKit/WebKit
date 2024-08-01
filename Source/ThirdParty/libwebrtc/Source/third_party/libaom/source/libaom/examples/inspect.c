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

// Inspect Decoder
// ================
//
// This is a simple decoder loop that writes JSON stats to stdout. This tool
// can also be compiled with Emscripten and used as a library.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#include "config/aom_config.h"

#include "aom/aom_decoder.h"
#include "aom/aomdx.h"
#include "av1/common/av1_common_int.h"

#if CONFIG_ACCOUNTING
#include "av1/decoder/accounting.h"
#endif

#include "av1/decoder/inspection.h"
#include "common/args.h"
#include "common/tools_common.h"
#include "common/video_common.h"
#include "common/video_reader.h"

// Max JSON buffer size.
const int MAX_BUFFER = 1024 * 1024 * 256;

typedef enum {
  ACCOUNTING_LAYER = 1,
  BLOCK_SIZE_LAYER = 1 << 1,
  TRANSFORM_SIZE_LAYER = 1 << 2,
  TRANSFORM_TYPE_LAYER = 1 << 3,
  MODE_LAYER = 1 << 4,
  SKIP_LAYER = 1 << 5,
  FILTER_LAYER = 1 << 6,
  CDEF_LAYER = 1 << 7,
  REFERENCE_FRAME_LAYER = 1 << 8,
  MOTION_VECTORS_LAYER = 1 << 9,
  UV_MODE_LAYER = 1 << 10,
  CFL_LAYER = 1 << 11,
  DUAL_FILTER_LAYER = 1 << 12,
  Q_INDEX_LAYER = 1 << 13,
  SEGMENT_ID_LAYER = 1 << 14,
  MOTION_MODE_LAYER = 1 << 15,
  COMPOUND_TYPE_LAYER = 1 << 16,
  INTRABC_LAYER = 1 << 17,
  PALETTE_LAYER = 1 << 18,
  UV_PALETTE_LAYER = 1 << 19,
  ALL_LAYERS = (1 << 20) - 1
} LayerType;

static LayerType layers = 0;

static int stop_after = 0;
static int compress = 0;

static const arg_def_t limit_arg =
    ARG_DEF(NULL, "limit", 1, "Stop decoding after n frames");
static const arg_def_t dump_all_arg = ARG_DEF("A", "all", 0, "Dump All");
static const arg_def_t compress_arg =
    ARG_DEF("x", "compress", 0, "Compress JSON using RLE");
static const arg_def_t dump_accounting_arg =
    ARG_DEF("a", "accounting", 0, "Dump Accounting");
static const arg_def_t dump_block_size_arg =
    ARG_DEF("bs", "blockSize", 0, "Dump Block Size");
static const arg_def_t dump_motion_vectors_arg =
    ARG_DEF("mv", "motionVectors", 0, "Dump Motion Vectors");
static const arg_def_t dump_transform_size_arg =
    ARG_DEF("ts", "transformSize", 0, "Dump Transform Size");
static const arg_def_t dump_transform_type_arg =
    ARG_DEF("tt", "transformType", 0, "Dump Transform Type");
static const arg_def_t dump_mode_arg = ARG_DEF("m", "mode", 0, "Dump Mode");
static const arg_def_t dump_motion_mode_arg =
    ARG_DEF("mm", "motion_mode", 0, "Dump Motion Modes");
static const arg_def_t dump_compound_type_arg =
    ARG_DEF("ct", "compound_type", 0, "Dump Compound Types");
static const arg_def_t dump_uv_mode_arg =
    ARG_DEF("uvm", "uv_mode", 0, "Dump UV Intra Prediction Modes");
static const arg_def_t dump_skip_arg = ARG_DEF("s", "skip", 0, "Dump Skip");
static const arg_def_t dump_filter_arg =
    ARG_DEF("f", "filter", 0, "Dump Filter");
static const arg_def_t dump_cdef_arg = ARG_DEF("c", "cdef", 0, "Dump CDEF");
static const arg_def_t dump_cfl_arg =
    ARG_DEF("cfl", "chroma_from_luma", 0, "Dump Chroma from Luma Alphas");
static const arg_def_t dump_dual_filter_type_arg =
    ARG_DEF("df", "dualFilterType", 0, "Dump Dual Filter Type");
static const arg_def_t dump_reference_frame_arg =
    ARG_DEF("r", "referenceFrame", 0, "Dump Reference Frame");
static const arg_def_t dump_delta_q_arg =
    ARG_DEF("dq", "delta_q", 0, "Dump QIndex");
static const arg_def_t dump_seg_id_arg =
    ARG_DEF("si", "seg_id", 0, "Dump Segment ID");
static const arg_def_t dump_intrabc_arg =
    ARG_DEF("ibc", "intrabc", 0, "Dump If IntraBC Is Used");
static const arg_def_t dump_palette_arg =
    ARG_DEF("plt", "palette", 0, "Dump Palette Size");
static const arg_def_t dump_uv_palette_arg =
    ARG_DEF("uvp", "uv_palette", 0, "Dump UV Palette Size");
static const arg_def_t usage_arg = ARG_DEF("h", "help", 0, "Help");
static const arg_def_t skip_non_transform_arg = ARG_DEF(
    "snt", "skip_non_transform", 1, "Skip is counted as a non transform.");
static const arg_def_t combined_arg =
    ARG_DEF("comb", "combined", 1, "combinining parameters into one output.");

int combined_parm_list[15];
int combined_parm_count = 0;

static const arg_def_t *main_args[] = { &limit_arg,
                                        &dump_all_arg,
                                        &compress_arg,
#if CONFIG_ACCOUNTING
                                        &dump_accounting_arg,
#endif
                                        &dump_block_size_arg,
                                        &dump_transform_size_arg,
                                        &dump_transform_type_arg,
                                        &dump_mode_arg,
                                        &dump_uv_mode_arg,
                                        &dump_motion_mode_arg,
                                        &dump_compound_type_arg,
                                        &dump_skip_arg,
                                        &dump_filter_arg,
                                        &dump_cdef_arg,
                                        &dump_dual_filter_type_arg,
                                        &dump_cfl_arg,
                                        &dump_reference_frame_arg,
                                        &dump_motion_vectors_arg,
                                        &dump_delta_q_arg,
                                        &dump_seg_id_arg,
                                        &dump_intrabc_arg,
                                        &dump_palette_arg,
                                        &dump_uv_palette_arg,
                                        &usage_arg,
                                        &skip_non_transform_arg,
                                        &combined_arg,
                                        NULL };
#define ENUM(name) \
  { #name, name }
#define LAST_ENUM \
  { NULL, 0 }
typedef struct map_entry {
  const char *name;
  int value;
} map_entry;

const map_entry refs_map[] = {
  ENUM(INTRA_FRAME),   ENUM(LAST_FRAME),   ENUM(LAST2_FRAME),
  ENUM(LAST3_FRAME),   ENUM(GOLDEN_FRAME), ENUM(BWDREF_FRAME),
  ENUM(ALTREF2_FRAME), ENUM(ALTREF_FRAME), LAST_ENUM
};

const map_entry block_size_map[] = {
  ENUM(BLOCK_4X4),     ENUM(BLOCK_4X8),    ENUM(BLOCK_8X4),
  ENUM(BLOCK_8X8),     ENUM(BLOCK_8X16),   ENUM(BLOCK_16X8),
  ENUM(BLOCK_16X16),   ENUM(BLOCK_16X32),  ENUM(BLOCK_32X16),
  ENUM(BLOCK_32X32),   ENUM(BLOCK_32X64),  ENUM(BLOCK_64X32),
  ENUM(BLOCK_64X64),   ENUM(BLOCK_64X128), ENUM(BLOCK_128X64),
  ENUM(BLOCK_128X128), ENUM(BLOCK_4X16),   ENUM(BLOCK_16X4),
  ENUM(BLOCK_8X32),    ENUM(BLOCK_32X8),   ENUM(BLOCK_16X64),
  ENUM(BLOCK_64X16),   LAST_ENUM
};

#define TX_SKIP -1

const map_entry tx_size_map[] = {
  ENUM(TX_4X4),   ENUM(TX_8X8),   ENUM(TX_16X16), ENUM(TX_32X32),
  ENUM(TX_64X64), ENUM(TX_4X8),   ENUM(TX_8X4),   ENUM(TX_8X16),
  ENUM(TX_16X8),  ENUM(TX_16X32), ENUM(TX_32X16), ENUM(TX_32X64),
  ENUM(TX_64X32), ENUM(TX_4X16),  ENUM(TX_16X4),  ENUM(TX_8X32),
  ENUM(TX_32X8),  ENUM(TX_16X64), ENUM(TX_64X16), LAST_ENUM
};

const map_entry tx_type_map[] = { ENUM(DCT_DCT),
                                  ENUM(ADST_DCT),
                                  ENUM(DCT_ADST),
                                  ENUM(ADST_ADST),
                                  ENUM(FLIPADST_DCT),
                                  ENUM(DCT_FLIPADST),
                                  ENUM(FLIPADST_FLIPADST),
                                  ENUM(ADST_FLIPADST),
                                  ENUM(FLIPADST_ADST),
                                  ENUM(IDTX),
                                  ENUM(V_DCT),
                                  ENUM(H_DCT),
                                  ENUM(V_ADST),
                                  ENUM(H_ADST),
                                  ENUM(V_FLIPADST),
                                  ENUM(H_FLIPADST),
                                  LAST_ENUM };
const map_entry dual_filter_map[] = { ENUM(REG_REG),       ENUM(REG_SMOOTH),
                                      ENUM(REG_SHARP),     ENUM(SMOOTH_REG),
                                      ENUM(SMOOTH_SMOOTH), ENUM(SMOOTH_SHARP),
                                      ENUM(SHARP_REG),     ENUM(SHARP_SMOOTH),
                                      ENUM(SHARP_SHARP),   LAST_ENUM };

const map_entry prediction_mode_map[] = {
  ENUM(DC_PRED),     ENUM(V_PRED),        ENUM(H_PRED),
  ENUM(D45_PRED),    ENUM(D135_PRED),     ENUM(D113_PRED),
  ENUM(D157_PRED),   ENUM(D203_PRED),     ENUM(D67_PRED),
  ENUM(SMOOTH_PRED), ENUM(SMOOTH_V_PRED), ENUM(SMOOTH_H_PRED),
  ENUM(PAETH_PRED),  ENUM(NEARESTMV),     ENUM(NEARMV),
  ENUM(GLOBALMV),    ENUM(NEWMV),         ENUM(NEAREST_NEARESTMV),
  ENUM(NEAR_NEARMV), ENUM(NEAREST_NEWMV), ENUM(NEW_NEARESTMV),
  ENUM(NEAR_NEWMV),  ENUM(NEW_NEARMV),    ENUM(GLOBAL_GLOBALMV),
  ENUM(NEW_NEWMV),   ENUM(INTRA_INVALID), LAST_ENUM
};

const map_entry motion_mode_map[] = { ENUM(SIMPLE_TRANSLATION),
                                      ENUM(OBMC_CAUSAL),    // 2-sided OBMC
                                      ENUM(WARPED_CAUSAL),  // 2-sided WARPED
                                      LAST_ENUM };

const map_entry compound_type_map[] = { ENUM(COMPOUND_AVERAGE),
                                        ENUM(COMPOUND_WEDGE),
                                        ENUM(COMPOUND_DIFFWTD), LAST_ENUM };

const map_entry uv_prediction_mode_map[] = {
  ENUM(UV_DC_PRED),       ENUM(UV_V_PRED),
  ENUM(UV_H_PRED),        ENUM(UV_D45_PRED),
  ENUM(UV_D135_PRED),     ENUM(UV_D113_PRED),
  ENUM(UV_D157_PRED),     ENUM(UV_D203_PRED),
  ENUM(UV_D67_PRED),      ENUM(UV_SMOOTH_PRED),
  ENUM(UV_SMOOTH_V_PRED), ENUM(UV_SMOOTH_H_PRED),
  ENUM(UV_PAETH_PRED),    ENUM(UV_CFL_PRED),
  ENUM(UV_MODE_INVALID),  LAST_ENUM
};
#define NO_SKIP 0
#define SKIP 1

const map_entry skip_map[] = { ENUM(SKIP), ENUM(NO_SKIP), LAST_ENUM };

const map_entry intrabc_map[] = { { "INTRABC", 1 },
                                  { "NO_INTRABC", 0 },
                                  LAST_ENUM };

const map_entry palette_map[] = {
  { "ZERO_COLORS", 0 },  { "TWO_COLORS", 2 },   { "THREE_COLORS", 3 },
  { "FOUR_COLORS", 4 },  { "FIVE_COLORS", 5 },  { "SIX_COLORS", 6 },
  { "SEVEN_COLORS", 7 }, { "EIGHT_COLORS", 8 }, LAST_ENUM
};

const map_entry config_map[] = { ENUM(MI_SIZE), LAST_ENUM };

static const char *exec_name;

struct parm_offset {
  char parm[60];
  char offset;
};
struct parm_offset parm_offsets[] = {
  { "blockSize", offsetof(insp_mi_data, bsize) },
  { "transformSize", offsetof(insp_mi_data, tx_size) },
  { "transformType", offsetof(insp_mi_data, tx_type) },
  { "dualFilterType", offsetof(insp_mi_data, dual_filter_type) },
  { "mode", offsetof(insp_mi_data, mode) },
  { "uv_mode", offsetof(insp_mi_data, uv_mode) },
  { "motion_mode", offsetof(insp_mi_data, motion_mode) },
  { "compound_type", offsetof(insp_mi_data, compound_type) },
  { "referenceFrame", offsetof(insp_mi_data, ref_frame) },
  { "skip", offsetof(insp_mi_data, skip) },
};
int parm_count = sizeof(parm_offsets) / sizeof(parm_offsets[0]);

static int convert_to_indices(char *str, int *indices, int maxCount,
                              int *count) {
  *count = 0;
  do {
    char *comma = strchr(str, ',');
    int length = (comma ? (int)(comma - str) : (int)strlen(str));
    int i;
    for (i = 0; i < parm_count; ++i) {
      if (!strncmp(str, parm_offsets[i].parm, length)) {
        break;
      }
    }
    if (i == parm_count) return 0;
    indices[(*count)++] = i;
    if (*count > maxCount) return 0;
    str += length + 1;
  } while (strlen(str) > 0);
  return 1;
}

insp_frame_data frame_data;
int frame_count = 0;
int decoded_frame_count = 0;
aom_codec_ctx_t codec;
AvxVideoReader *reader = NULL;
const AvxVideoInfo *info = NULL;
aom_image_t *img = NULL;

static void on_frame_decoded_dump(char *json) {
#ifdef __EMSCRIPTEN__
  EM_ASM_({ Module.on_frame_decoded_json($0); }, json);
#else
  printf("%s", json);
#endif
}

// Writing out the JSON buffer using snprintf is very slow, especially when
// compiled with emscripten, these functions speed things up quite a bit.
static int put_str(char *buffer, const char *str) {
  int i;
  for (i = 0; str[i] != '\0'; i++) {
    buffer[i] = str[i];
  }
  return i;
}

static int put_str_with_escape(char *buffer, const char *str) {
  int i;
  int j = 0;
  for (i = 0; str[i] != '\0'; i++) {
    if (str[i] < ' ') {
      continue;
    } else if (str[i] == '"' || str[i] == '\\') {
      buffer[j++] = '\\';
    }
    buffer[j++] = str[i];
  }
  return j;
}

static int put_num(char *buffer, char prefix, int num, char suffix) {
  int i = 0;
  char *buf = buffer;
  int is_neg = 0;
  if (prefix) {
    buf[i++] = prefix;
  }
  if (num == 0) {
    buf[i++] = '0';
  } else {
    if (num < 0) {
      num = -num;
      is_neg = 1;
    }
    int s = i;
    while (num != 0) {
      buf[i++] = '0' + (num % 10);
      num = num / 10;
    }
    if (is_neg) {
      buf[i++] = '-';
    }
    int e = i - 1;
    while (s < e) {
      int t = buf[s];
      buf[s] = buf[e];
      buf[e] = t;
      s++;
      e--;
    }
  }
  if (suffix) {
    buf[i++] = suffix;
  }
  return i;
}

static int put_map(char *buffer, const map_entry *map) {
  char *buf = buffer;
  const map_entry *entry = map;
  while (entry->name != NULL) {
    *(buf++) = '"';
    buf += put_str(buf, entry->name);
    *(buf++) = '"';
    buf += put_num(buf, ':', entry->value, 0);
    entry++;
    if (entry->name != NULL) {
      *(buf++) = ',';
    }
  }
  return (int)(buf - buffer);
}

#if 0
static int put_reference_frame(char *buffer) {
  const int mi_rows = frame_data.mi_rows;
  const int mi_cols = frame_data.mi_cols;
  char *buf = buffer;
  int r, c, t;
  buf += put_str(buf, "  \"referenceFrameMap\": {");
  buf += put_map(buf, refs_map);
  buf += put_str(buf, "},\n");
  buf += put_str(buf, "  \"referenceFrame\": [");
  for (r = 0; r < mi_rows; ++r) {
    *(buf++) = '[';
    for (c = 0; c < mi_cols; ++c) {
      insp_mi_data *mi = &frame_data.mi_grid[r * mi_cols + c];
      buf += put_num(buf, '[', mi->ref_frame[0], 0);
      buf += put_num(buf, ',', mi->ref_frame[1], ']');
      if (compress) {  // RLE
        for (t = c + 1; t < mi_cols; ++t) {
          insp_mi_data *next_mi = &frame_data.mi_grid[r * mi_cols + t];
          if (mi->ref_frame[0] != next_mi->ref_frame[0] ||
              mi->ref_frame[1] != next_mi->ref_frame[1]) {
            break;
          }
        }
        if (t - c > 1) {
          *(buf++) = ',';
          buf += put_num(buf, '[', t - c - 1, ']');
          c = t - 1;
        }
      }
      if (c < mi_cols - 1) *(buf++) = ',';
    }
    *(buf++) = ']';
    if (r < mi_rows - 1) *(buf++) = ',';
  }
  buf += put_str(buf, "],\n");
  return (int)(buf - buffer);
}
#endif

static int put_motion_vectors(char *buffer) {
  const int mi_rows = frame_data.mi_rows;
  const int mi_cols = frame_data.mi_cols;
  char *buf = buffer;
  int r, c, t;
  buf += put_str(buf, "  \"motionVectors\": [");
  for (r = 0; r < mi_rows; ++r) {
    *(buf++) = '[';
    for (c = 0; c < mi_cols; ++c) {
      insp_mi_data *mi = &frame_data.mi_grid[r * mi_cols + c];
      buf += put_num(buf, '[', mi->mv[0].col, 0);
      buf += put_num(buf, ',', mi->mv[0].row, 0);
      buf += put_num(buf, ',', mi->mv[1].col, 0);
      buf += put_num(buf, ',', mi->mv[1].row, ']');
      if (compress) {  // RLE
        for (t = c + 1; t < mi_cols; ++t) {
          insp_mi_data *next_mi = &frame_data.mi_grid[r * mi_cols + t];
          if (mi->mv[0].col != next_mi->mv[0].col ||
              mi->mv[0].row != next_mi->mv[0].row ||
              mi->mv[1].col != next_mi->mv[1].col ||
              mi->mv[1].row != next_mi->mv[1].row) {
            break;
          }
        }
        if (t - c > 1) {
          *(buf++) = ',';
          buf += put_num(buf, '[', t - c - 1, ']');
          c = t - 1;
        }
      }
      if (c < mi_cols - 1) *(buf++) = ',';
    }
    *(buf++) = ']';
    if (r < mi_rows - 1) *(buf++) = ',';
  }
  buf += put_str(buf, "],\n");
  return (int)(buf - buffer);
}

static int put_combined(char *buffer) {
  const int mi_rows = frame_data.mi_rows;
  const int mi_cols = frame_data.mi_cols;
  char *buf = buffer;
  int r, c, p;
  buf += put_str(buf, "  \"");
  for (p = 0; p < combined_parm_count; ++p) {
    if (p) buf += put_str(buf, "&");
    buf += put_str(buf, parm_offsets[combined_parm_list[p]].parm);
  }
  buf += put_str(buf, "\": [");
  for (r = 0; r < mi_rows; ++r) {
    *(buf++) = '[';
    for (c = 0; c < mi_cols; ++c) {
      insp_mi_data *mi = &frame_data.mi_grid[r * mi_cols + c];
      *(buf++) = '[';
      for (p = 0; p < combined_parm_count; ++p) {
        if (p) *(buf++) = ',';
        int16_t *v = (int16_t *)(((int8_t *)mi) +
                                 parm_offsets[combined_parm_list[p]].offset);
        buf += put_num(buf, 0, v[0], 0);
      }
      *(buf++) = ']';
      if (c < mi_cols - 1) *(buf++) = ',';
    }
    *(buf++) = ']';
    if (r < mi_rows - 1) *(buf++) = ',';
  }
  buf += put_str(buf, "],\n");
  return (int)(buf - buffer);
}

static int put_block_info(char *buffer, const map_entry *map, const char *name,
                          size_t offset, int len) {
  const int mi_rows = frame_data.mi_rows;
  const int mi_cols = frame_data.mi_cols;
  char *buf = buffer;
  int r, c, t, i;
  if (compress && len == 1) {
    die("Can't encode scalars as arrays when RLE compression is enabled.");
  }
  if (map) {
    buf += snprintf(buf, MAX_BUFFER, "  \"%sMap\": {", name);
    buf += put_map(buf, map);
    buf += put_str(buf, "},\n");
  }
  buf += snprintf(buf, MAX_BUFFER, "  \"%s\": [", name);
  for (r = 0; r < mi_rows; ++r) {
    *(buf++) = '[';
    for (c = 0; c < mi_cols; ++c) {
      insp_mi_data *mi = &frame_data.mi_grid[r * mi_cols + c];
      int16_t *v = (int16_t *)(((int8_t *)mi) + offset);
      if (len == 0) {
        buf += put_num(buf, 0, v[0], 0);
      } else {
        buf += put_str(buf, "[");
        for (i = 0; i < len; i++) {
          buf += put_num(buf, 0, v[i], 0);
          if (i < len - 1) {
            buf += put_str(buf, ",");
          }
        }
        buf += put_str(buf, "]");
      }
      if (compress) {  // RLE
        for (t = c + 1; t < mi_cols; ++t) {
          insp_mi_data *next_mi = &frame_data.mi_grid[r * mi_cols + t];
          int16_t *nv = (int16_t *)(((int8_t *)next_mi) + offset);
          int same = 0;
          if (len == 0) {
            same = v[0] == nv[0];
          } else {
            for (i = 0; i < len; i++) {
              same = v[i] == nv[i];
              if (!same) {
                break;
              }
            }
          }
          if (!same) {
            break;
          }
        }
        if (t - c > 1) {
          *(buf++) = ',';
          buf += put_num(buf, '[', t - c - 1, ']');
          c = t - 1;
        }
      }
      if (c < mi_cols - 1) *(buf++) = ',';
    }
    *(buf++) = ']';
    if (r < mi_rows - 1) *(buf++) = ',';
  }
  buf += put_str(buf, "],\n");
  return (int)(buf - buffer);
}

#if CONFIG_ACCOUNTING
static int put_accounting(char *buffer) {
  char *buf = buffer;
  int i;
  const Accounting *accounting = frame_data.accounting;
  if (accounting == NULL) {
    printf("XXX\n");
    return 0;
  }
  const int num_syms = accounting->syms.num_syms;
  const int num_strs = accounting->syms.dictionary.num_strs;
  buf += put_str(buf, "  \"symbolsMap\": [");
  for (i = 0; i < num_strs; i++) {
    buf += snprintf(buf, MAX_BUFFER, "\"%s\"",
                    accounting->syms.dictionary.strs[i]);
    if (i < num_strs - 1) *(buf++) = ',';
  }
  buf += put_str(buf, "],\n");
  buf += put_str(buf, "  \"symbols\": [\n    ");
  AccountingSymbolContext context;
  context.x = -2;
  context.y = -2;
  AccountingSymbol *sym;
  for (i = 0; i < num_syms; i++) {
    sym = &accounting->syms.syms[i];
    if (memcmp(&context, &sym->context, sizeof(AccountingSymbolContext)) != 0) {
      buf += put_num(buf, '[', sym->context.x, 0);
      buf += put_num(buf, ',', sym->context.y, ']');
    } else {
      buf += put_num(buf, '[', sym->id, 0);
      buf += put_num(buf, ',', sym->bits, 0);
      buf += put_num(buf, ',', sym->samples, ']');
    }
    context = sym->context;
    if (i < num_syms - 1) *(buf++) = ',';
  }
  buf += put_str(buf, "],\n");
  return (int)(buf - buffer);
}
#endif

int skip_non_transform = 0;

static void inspect(void *pbi, void *data) {
  /* Fetch frame data. */
  ifd_inspect(&frame_data, pbi, skip_non_transform);

  // Show existing frames just show a reference buffer we've already decoded.
  // There's no information to show.
  if (frame_data.show_existing_frame) return;

  (void)data;
  // We allocate enough space and hope we don't write out of bounds. Totally
  // unsafe but this speeds things up, especially when compiled to Javascript.
  char *buffer = aom_malloc(MAX_BUFFER);
  if (!buffer) {
    fprintf(stderr, "Error allocating inspect info buffer\n");
    abort();
  }
  char *buf = buffer;
  buf += put_str(buf, "{\n");
  if (layers & BLOCK_SIZE_LAYER) {
    buf += put_block_info(buf, block_size_map, "blockSize",
                          offsetof(insp_mi_data, bsize), 0);
  }
  if (layers & TRANSFORM_SIZE_LAYER) {
    buf += put_block_info(buf, tx_size_map, "transformSize",
                          offsetof(insp_mi_data, tx_size), 0);
  }
  if (layers & TRANSFORM_TYPE_LAYER) {
    buf += put_block_info(buf, tx_type_map, "transformType",
                          offsetof(insp_mi_data, tx_type), 0);
  }
  if (layers & DUAL_FILTER_LAYER) {
    buf += put_block_info(buf, dual_filter_map, "dualFilterType",
                          offsetof(insp_mi_data, dual_filter_type), 0);
  }
  if (layers & MODE_LAYER) {
    buf += put_block_info(buf, prediction_mode_map, "mode",
                          offsetof(insp_mi_data, mode), 0);
  }
  if (layers & UV_MODE_LAYER) {
    buf += put_block_info(buf, uv_prediction_mode_map, "uv_mode",
                          offsetof(insp_mi_data, uv_mode), 0);
  }
  if (layers & MOTION_MODE_LAYER) {
    buf += put_block_info(buf, motion_mode_map, "motion_mode",
                          offsetof(insp_mi_data, motion_mode), 0);
  }
  if (layers & COMPOUND_TYPE_LAYER) {
    buf += put_block_info(buf, compound_type_map, "compound_type",
                          offsetof(insp_mi_data, compound_type), 0);
  }
  if (layers & SKIP_LAYER) {
    buf +=
        put_block_info(buf, skip_map, "skip", offsetof(insp_mi_data, skip), 0);
  }
  if (layers & FILTER_LAYER) {
    buf +=
        put_block_info(buf, NULL, "filter", offsetof(insp_mi_data, filter), 2);
  }
  if (layers & CDEF_LAYER) {
    buf += put_block_info(buf, NULL, "cdef_level",
                          offsetof(insp_mi_data, cdef_level), 0);
    buf += put_block_info(buf, NULL, "cdef_strength",
                          offsetof(insp_mi_data, cdef_strength), 0);
  }
  if (layers & CFL_LAYER) {
    buf += put_block_info(buf, NULL, "cfl_alpha_idx",
                          offsetof(insp_mi_data, cfl_alpha_idx), 0);
    buf += put_block_info(buf, NULL, "cfl_alpha_sign",
                          offsetof(insp_mi_data, cfl_alpha_sign), 0);
  }
  if (layers & Q_INDEX_LAYER) {
    buf += put_block_info(buf, NULL, "delta_q",
                          offsetof(insp_mi_data, current_qindex), 0);
  }
  if (layers & SEGMENT_ID_LAYER) {
    buf += put_block_info(buf, NULL, "seg_id",
                          offsetof(insp_mi_data, segment_id), 0);
  }
  if (layers & MOTION_VECTORS_LAYER) {
    buf += put_motion_vectors(buf);
  }
  if (layers & INTRABC_LAYER) {
    buf += put_block_info(buf, intrabc_map, "intrabc",
                          offsetof(insp_mi_data, intrabc), 0);
  }
  if (layers & PALETTE_LAYER) {
    buf += put_block_info(buf, palette_map, "palette",
                          offsetof(insp_mi_data, palette), 0);
  }
  if (layers & UV_PALETTE_LAYER) {
    buf += put_block_info(buf, palette_map, "uv_palette",
                          offsetof(insp_mi_data, uv_palette), 0);
  }
  if (combined_parm_count > 0) buf += put_combined(buf);
  if (layers & REFERENCE_FRAME_LAYER) {
    buf += put_block_info(buf, refs_map, "referenceFrame",
                          offsetof(insp_mi_data, ref_frame), 2);
  }
#if CONFIG_ACCOUNTING
  if (layers & ACCOUNTING_LAYER) {
    buf += put_accounting(buf);
  }
#endif
  buf +=
      snprintf(buf, MAX_BUFFER, "  \"frame\": %d,\n", frame_data.frame_number);
  buf += snprintf(buf, MAX_BUFFER, "  \"showFrame\": %d,\n",
                  frame_data.show_frame);
  buf += snprintf(buf, MAX_BUFFER, "  \"frameType\": %d,\n",
                  frame_data.frame_type);
  buf += snprintf(buf, MAX_BUFFER, "  \"baseQIndex\": %d,\n",
                  frame_data.base_qindex);
  buf += snprintf(buf, MAX_BUFFER, "  \"tileCols\": %d,\n",
                  frame_data.tile_mi_cols);
  buf += snprintf(buf, MAX_BUFFER, "  \"tileRows\": %d,\n",
                  frame_data.tile_mi_rows);
  buf += snprintf(buf, MAX_BUFFER, "  \"deltaQPresentFlag\": %d,\n",
                  frame_data.delta_q_present_flag);
  buf += snprintf(buf, MAX_BUFFER, "  \"deltaQRes\": %d,\n",
                  frame_data.delta_q_res);
  buf += put_str(buf, "  \"config\": {");
  buf += put_map(buf, config_map);
  buf += put_str(buf, "},\n");
  buf += put_str(buf, "  \"configString\": \"");
  buf += put_str_with_escape(buf, aom_codec_build_config());
  buf += put_str(buf, "\"\n");
  decoded_frame_count++;
  buf += put_str(buf, "},\n");
  *(buf++) = 0;
  on_frame_decoded_dump(buffer);
  aom_free(buffer);
}

static void ifd_init_cb(void) {
  aom_inspect_init ii;
  ii.inspect_cb = inspect;
  ii.inspect_ctx = NULL;
  aom_codec_control(&codec, AV1_SET_INSPECTION_CALLBACK, &ii);
}

EMSCRIPTEN_KEEPALIVE int open_file(char *file);

EMSCRIPTEN_KEEPALIVE
int open_file(char *file) {
  if (file == NULL) {
    // The JS analyzer puts the .ivf file at this location.
    file = "/tmp/input.ivf";
  }
  reader = aom_video_reader_open(file);
  if (!reader) die("Failed to open %s for reading.", file);
  info = aom_video_reader_get_info(reader);
  aom_codec_iface_t *decoder = get_aom_decoder_by_fourcc(info->codec_fourcc);
  if (!decoder) die("Unknown input codec.");
  fprintf(stderr, "Using %s\n", aom_codec_iface_name(decoder));
  if (aom_codec_dec_init(&codec, decoder, NULL, 0))
    die("Failed to initialize decoder.");
  ifd_init(&frame_data, info->frame_width, info->frame_height);
  ifd_init_cb();
  return EXIT_SUCCESS;
}

Av1DecodeReturn adr;
int have_frame = 0;
const unsigned char *frame;
const unsigned char *end_frame;
size_t frame_size = 0;
struct av1_ref_frame ref_dec;

EMSCRIPTEN_KEEPALIVE int read_frame(void);

EMSCRIPTEN_KEEPALIVE
int read_frame(void) {
  img = NULL;

  // This loop skips over any frames that are show_existing_frames,  as
  // there is nothing to analyze.
  do {
    if (!have_frame) {
      if (!aom_video_reader_read_frame(reader)) return EXIT_FAILURE;
      frame = aom_video_reader_get_frame(reader, &frame_size);

      have_frame = 1;
      end_frame = frame + frame_size;
    }

    if (aom_codec_decode(&codec, frame, (unsigned int)frame_size, &adr) !=
        AOM_CODEC_OK) {
      die_codec(&codec, "Failed to decode frame.");
    }

    frame = adr.buf;
    frame_size = end_frame - frame;
    if (frame == end_frame) have_frame = 0;
  } while (adr.show_existing);

  int got_any_frames = 0;
  aom_image_t *frame_img;
  ref_dec.idx = adr.idx;

  // ref_dec.idx is the index to the reference buffer idx to AV1_GET_REFERENCE
  // if its -1 the decoder didn't update any reference buffer and the only
  // way to see the frame is aom_codec_get_frame.
  if (ref_dec.idx == -1) {
    aom_codec_iter_t iter = NULL;
    img = frame_img = aom_codec_get_frame(&codec, &iter);
    ++frame_count;
    got_any_frames = 1;
  } else if (!aom_codec_control(&codec, AV1_GET_REFERENCE, &ref_dec)) {
    img = frame_img = &ref_dec.img;
    ++frame_count;
    got_any_frames = 1;
  }
  if (!got_any_frames) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

EMSCRIPTEN_KEEPALIVE const char *get_aom_codec_build_config(void);

EMSCRIPTEN_KEEPALIVE
const char *get_aom_codec_build_config(void) {
  return aom_codec_build_config();
}

EMSCRIPTEN_KEEPALIVE int get_bit_depth(void);

EMSCRIPTEN_KEEPALIVE
int get_bit_depth(void) { return img->bit_depth; }

EMSCRIPTEN_KEEPALIVE int get_bits_per_sample(void);

EMSCRIPTEN_KEEPALIVE
int get_bits_per_sample(void) { return img->bps; }

EMSCRIPTEN_KEEPALIVE int get_image_format(void);

EMSCRIPTEN_KEEPALIVE
int get_image_format(void) { return img->fmt; }

EMSCRIPTEN_KEEPALIVE unsigned char *get_plane(int plane);

EMSCRIPTEN_KEEPALIVE
unsigned char *get_plane(int plane) { return img->planes[plane]; }

EMSCRIPTEN_KEEPALIVE int get_plane_stride(int plane);

EMSCRIPTEN_KEEPALIVE
int get_plane_stride(int plane) { return img->stride[plane]; }

EMSCRIPTEN_KEEPALIVE int get_plane_width(int plane);

EMSCRIPTEN_KEEPALIVE
int get_plane_width(int plane) { return aom_img_plane_width(img, plane); }

EMSCRIPTEN_KEEPALIVE int get_plane_height(int plane);

EMSCRIPTEN_KEEPALIVE
int get_plane_height(int plane) { return aom_img_plane_height(img, plane); }

EMSCRIPTEN_KEEPALIVE int get_frame_width(void);

EMSCRIPTEN_KEEPALIVE
int get_frame_width(void) { return info->frame_width; }

EMSCRIPTEN_KEEPALIVE int get_frame_height(void);

EMSCRIPTEN_KEEPALIVE
int get_frame_height(void) { return info->frame_height; }

static void parse_args(char **argv) {
  char **argi, **argj;
  struct arg arg;
  (void)dump_accounting_arg;
  (void)dump_cdef_arg;
  for (argi = argj = argv; (*argj = *argi); argi += arg.argv_step) {
    arg.argv_step = 1;
    if (arg_match(&arg, &dump_block_size_arg, argi)) layers |= BLOCK_SIZE_LAYER;
#if CONFIG_ACCOUNTING
    else if (arg_match(&arg, &dump_accounting_arg, argi))
      layers |= ACCOUNTING_LAYER;
#endif
    else if (arg_match(&arg, &dump_transform_size_arg, argi))
      layers |= TRANSFORM_SIZE_LAYER;
    else if (arg_match(&arg, &dump_transform_type_arg, argi))
      layers |= TRANSFORM_TYPE_LAYER;
    else if (arg_match(&arg, &dump_mode_arg, argi))
      layers |= MODE_LAYER;
    else if (arg_match(&arg, &dump_uv_mode_arg, argi))
      layers |= UV_MODE_LAYER;
    else if (arg_match(&arg, &dump_motion_mode_arg, argi))
      layers |= MOTION_MODE_LAYER;
    else if (arg_match(&arg, &dump_compound_type_arg, argi))
      layers |= COMPOUND_TYPE_LAYER;
    else if (arg_match(&arg, &dump_skip_arg, argi))
      layers |= SKIP_LAYER;
    else if (arg_match(&arg, &dump_filter_arg, argi))
      layers |= FILTER_LAYER;
    else if (arg_match(&arg, &dump_cdef_arg, argi))
      layers |= CDEF_LAYER;
    else if (arg_match(&arg, &dump_cfl_arg, argi))
      layers |= CFL_LAYER;
    else if (arg_match(&arg, &dump_reference_frame_arg, argi))
      layers |= REFERENCE_FRAME_LAYER;
    else if (arg_match(&arg, &dump_motion_vectors_arg, argi))
      layers |= MOTION_VECTORS_LAYER;
    else if (arg_match(&arg, &dump_dual_filter_type_arg, argi))
      layers |= DUAL_FILTER_LAYER;
    else if (arg_match(&arg, &dump_delta_q_arg, argi))
      layers |= Q_INDEX_LAYER;
    else if (arg_match(&arg, &dump_seg_id_arg, argi))
      layers |= SEGMENT_ID_LAYER;
    else if (arg_match(&arg, &dump_intrabc_arg, argi))
      layers |= INTRABC_LAYER;
    else if (arg_match(&arg, &dump_palette_arg, argi))
      layers |= PALETTE_LAYER;
    else if (arg_match(&arg, &dump_uv_palette_arg, argi))
      layers |= UV_PALETTE_LAYER;
    else if (arg_match(&arg, &dump_all_arg, argi))
      layers |= ALL_LAYERS;
    else if (arg_match(&arg, &compress_arg, argi))
      compress = 1;
    else if (arg_match(&arg, &usage_arg, argi))
      usage_exit();
    else if (arg_match(&arg, &limit_arg, argi))
      stop_after = arg_parse_uint(&arg);
    else if (arg_match(&arg, &skip_non_transform_arg, argi))
      skip_non_transform = arg_parse_uint(&arg);
    else if (arg_match(&arg, &combined_arg, argi))
      convert_to_indices(
          (char *)arg.val, combined_parm_list,
          sizeof(combined_parm_list) / sizeof(combined_parm_list[0]),
          &combined_parm_count);
    else
      argj++;
  }
}

static const char *exec_name;

void usage_exit(void) {
  fprintf(stderr, "Usage: %s src_filename <options>\n", exec_name);
  fprintf(stderr, "\nOptions:\n");
  arg_show_usage(stderr, main_args);
  exit(EXIT_FAILURE);
}

EMSCRIPTEN_KEEPALIVE
int main(int argc, char **argv) {
  exec_name = argv[0];
  parse_args(argv);
  if (argc >= 2) {
    open_file(argv[1]);
    printf("[\n");
    while (1) {
      if (stop_after && (decoded_frame_count >= stop_after)) break;
      if (read_frame()) break;
    }
    printf("null\n");
    printf("]");
  } else {
    usage_exit();
  }
}

EMSCRIPTEN_KEEPALIVE void quit(void);

EMSCRIPTEN_KEEPALIVE
void quit(void) {
  if (aom_codec_destroy(&codec)) die_codec(&codec, "Failed to destroy codec");
  aom_video_reader_close(reader);
}

EMSCRIPTEN_KEEPALIVE void set_layers(LayerType v);

EMSCRIPTEN_KEEPALIVE
void set_layers(LayerType v) { layers = v; }

EMSCRIPTEN_KEEPALIVE void set_compress(int v);

EMSCRIPTEN_KEEPALIVE
void set_compress(int v) { compress = v; }
