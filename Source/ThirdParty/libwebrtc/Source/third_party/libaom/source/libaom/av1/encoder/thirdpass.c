/*
 * Copyright (c) 2021, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include "av1/encoder/thirdpass.h"

#if CONFIG_THREE_PASS && CONFIG_AV1_DECODER
#include "aom/aom_codec.h"
#include "aom/aomdx.h"
#include "aom_dsp/psnr.h"
#include "aom_mem/aom_mem.h"
#include "av1/av1_iface_common.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/firstpass.h"
#include "av1/common/blockd.h"
#include "common/ivfdec.h"

static void setup_two_pass_stream_input(
    struct AvxInputContext **input_ctx_ptr, const char *input_file_name,
    struct aom_internal_error_info *err_info) {
  FILE *infile;
  infile = fopen(input_file_name, "rb");
  if (!infile) {
    aom_internal_error(err_info, AOM_CODEC_INVALID_PARAM,
                       "Failed to open input file '%s'.", input_file_name);
  }
  struct AvxInputContext *aom_input_ctx = aom_malloc(sizeof(*aom_input_ctx));
  if (!aom_input_ctx) {
    fclose(infile);
    aom_internal_error(err_info, AOM_CODEC_MEM_ERROR,
                       "Failed to allocate memory for third-pass context.");
  }
  memset(aom_input_ctx, 0, sizeof(*aom_input_ctx));
  aom_input_ctx->filename = input_file_name;
  aom_input_ctx->file = infile;

  if (file_is_ivf(aom_input_ctx)) {
    aom_input_ctx->file_type = FILE_TYPE_IVF;
  } else {
    fclose(infile);
    aom_free(aom_input_ctx);
    aom_internal_error(err_info, AOM_CODEC_INVALID_PARAM,
                       "Unrecognized input file type.");
  }
  *input_ctx_ptr = aom_input_ctx;
}

static void init_third_pass(THIRD_PASS_DEC_CTX *ctx) {
  if (!ctx->input_ctx) {
    if (ctx->input_file_name == NULL) {
      aom_internal_error(ctx->err_info, AOM_CODEC_INVALID_PARAM,
                         "No third pass input specified.");
    }
    setup_two_pass_stream_input(&ctx->input_ctx, ctx->input_file_name,
                                ctx->err_info);
  }

  if (!ctx->decoder.iface) {
    aom_codec_iface_t *decoder_iface = &aom_codec_av1_inspect_algo;
    if (aom_codec_dec_init(&ctx->decoder, decoder_iface, NULL, 0)) {
      aom_internal_error(ctx->err_info, AOM_CODEC_ERROR,
                         "Failed to initialize decoder.");
    }
  }
}

// Return 0: success
//        1: cannot read because this is end of file
//       -1: failure to read the frame
static int read_frame(THIRD_PASS_DEC_CTX *ctx) {
  if (!ctx->input_ctx || !ctx->decoder.iface) {
    init_third_pass(ctx);
  }
  if (!ctx->have_frame) {
    if (ivf_read_frame(ctx->input_ctx, &ctx->buf, &ctx->bytes_in_buffer,
                       &ctx->buffer_size, NULL) != 0) {
      if (feof(ctx->input_ctx->file)) {
        return 1;
      } else {
        return -1;
      }
    }
    ctx->frame = ctx->buf;
    ctx->end_frame = ctx->frame + ctx->bytes_in_buffer;
    ctx->have_frame = 1;
  }

  Av1DecodeReturn adr;
  if (aom_codec_decode(&ctx->decoder, ctx->frame,
                       (unsigned int)ctx->bytes_in_buffer,
                       &adr) != AOM_CODEC_OK) {
    aom_internal_error(ctx->err_info, AOM_CODEC_ERROR,
                       "Failed to decode frame for third pass.");
  }
  ctx->this_frame_bits = (int)(adr.buf - ctx->frame) << 3;
  ctx->frame = adr.buf;
  ctx->bytes_in_buffer = ctx->end_frame - ctx->frame;
  if (ctx->frame == ctx->end_frame) ctx->have_frame = 0;
  return 0;
}

static void free_frame_info(THIRD_PASS_FRAME_INFO *frame_info) {
  if (!frame_info) return;
  aom_free(frame_info->mi_info);
  frame_info->mi_info = NULL;
}

// This function gets the information needed from the recently decoded frame,
// via various decoder APIs, and saves the info into ctx->frame_info.
// Return 0: success
//        1: cannot read because this is end of file
//       -1: failure to read the frame
static int get_frame_info(THIRD_PASS_DEC_CTX *ctx) {
  int ret = read_frame(ctx);
  if (ret != 0) return ret;
  int cur = ctx->frame_info_count;

  ctx->frame_info[cur].actual_bits = ctx->this_frame_bits;

  if (cur >= MAX_THIRD_PASS_BUF) {
    aom_internal_error(ctx->err_info, AOM_CODEC_ERROR,
                       "Third pass frame info ran out of available slots.");
  }
  aom_codec_frame_flags_t frame_type_flags = 0;
  if (aom_codec_control(&ctx->decoder, AOMD_GET_FRAME_FLAGS,
                        &frame_type_flags) != AOM_CODEC_OK) {
    aom_internal_error(ctx->err_info, AOM_CODEC_ERROR,
                       "Failed to read frame flags.");
  }
  if (frame_type_flags & AOM_FRAME_IS_KEY) {
    ctx->frame_info[cur].frame_type = KEY_FRAME;
  } else if (frame_type_flags & AOM_FRAME_IS_INTRAONLY) {
    ctx->frame_info[cur].frame_type = INTRA_ONLY_FRAME;
  } else if (frame_type_flags & AOM_FRAME_IS_SWITCH) {
    ctx->frame_info[cur].frame_type = S_FRAME;
  } else {
    ctx->frame_info[cur].frame_type = INTER_FRAME;
  }

  // Get frame width and height
  int frame_size[2];
  if (aom_codec_control(&ctx->decoder, AV1D_GET_FRAME_SIZE, frame_size) !=
      AOM_CODEC_OK) {
    aom_internal_error(ctx->err_info, AOM_CODEC_ERROR,
                       "Failed to read frame size.");
  }

  // Check if we need to re-alloc the mi fields.
  const int mi_cols = (frame_size[0] + 3) >> 2;
  const int mi_rows = (frame_size[1] + 3) >> 2;
  ctx->frame_info[cur].mi_stride = mi_cols;
  ctx->frame_info[cur].mi_rows = mi_rows;
  ctx->frame_info[cur].mi_cols = mi_cols;

  if (ctx->frame_info[cur].width != frame_size[0] ||
      ctx->frame_info[cur].height != frame_size[1] ||
      !ctx->frame_info[cur].mi_info) {
    free_frame_info(&ctx->frame_info[cur]);

    ctx->frame_info[cur].mi_info =
        aom_malloc(mi_cols * mi_rows * sizeof(*ctx->frame_info[cur].mi_info));

    if (!ctx->frame_info[cur].mi_info) {
      aom_internal_error(ctx->err_info, AOM_CODEC_MEM_ERROR,
                         "Failed to allocate mi buffer for the third pass.");
    }
  }

  ctx->frame_info[cur].width = frame_size[0];
  ctx->frame_info[cur].height = frame_size[1];

  // Get frame base q idx
  if (aom_codec_control(&ctx->decoder, AOMD_GET_BASE_Q_IDX,
                        &ctx->frame_info[cur].base_q_idx) != AOM_CODEC_OK) {
    aom_internal_error(ctx->err_info, AOM_CODEC_ERROR,
                       "Failed to read base q index.");
  }

  // Get show existing frame flag
  if (aom_codec_control(&ctx->decoder, AOMD_GET_SHOW_EXISTING_FRAME_FLAG,
                        &ctx->frame_info[cur].is_show_existing_frame) !=
      AOM_CODEC_OK) {
    aom_internal_error(ctx->err_info, AOM_CODEC_ERROR,
                       "Failed to read show existing frame flag.");
  }

  // Get show frame flag
  if (aom_codec_control(&ctx->decoder, AOMD_GET_SHOW_FRAME_FLAG,
                        &ctx->frame_info[cur].is_show_frame) != AOM_CODEC_OK) {
    aom_internal_error(ctx->err_info, AOM_CODEC_ERROR,
                       "Failed to read show frame flag.");
  }

  // Get order hint
  if (aom_codec_control(&ctx->decoder, AOMD_GET_ORDER_HINT,
                        &ctx->frame_info[cur].order_hint) != AOM_CODEC_OK) {
    aom_internal_error(ctx->err_info, AOM_CODEC_ERROR,
                       "Failed to read order hint.");
  }

  // Clear MI info
  for (int mi_row = 0; mi_row < mi_rows; mi_row++) {
    for (int mi_col = 0; mi_col < mi_cols; mi_col++) {
      ctx->frame_info[cur].mi_info[mi_row * mi_cols + mi_col].bsize =
          BLOCK_INVALID;
    }
  }

  // Get relevant information regarding each 4x4 MI
  MB_MODE_INFO cur_mi_info;
  THIRD_PASS_MI_INFO *const this_mi = ctx->frame_info[cur].mi_info;
  for (int mi_row = 0; mi_row < mi_rows; mi_row++) {
    for (int mi_col = 0; mi_col < mi_cols; mi_col++) {
      const int offset = mi_row * mi_cols + mi_col;
      if (this_mi[offset].bsize != BLOCK_INVALID) {
        continue;
      }
      // Get info of this MI
      if (aom_codec_control(&ctx->decoder, AV1D_GET_MI_INFO, mi_row, mi_col,
                            &cur_mi_info) != AOM_CODEC_OK) {
        aom_internal_error(ctx->err_info, AOM_CODEC_ERROR,
                           "Failed to read mi info.");
      }
      const int blk_mi_rows = mi_size_high[cur_mi_info.bsize];
      const int blk_mi_cols = mi_size_wide[cur_mi_info.bsize];

      for (int h = 0; h < blk_mi_rows; h++) {
        for (int w = 0; w < blk_mi_cols; w++) {
          if (h + mi_row >= mi_rows || w + mi_col >= mi_cols) {
            continue;
          }
          const int this_offset = offset + h * mi_cols + w;
          this_mi[this_offset].bsize = cur_mi_info.bsize;
          this_mi[this_offset].partition = cur_mi_info.partition;
          this_mi[this_offset].mi_row_start = mi_row;
          this_mi[this_offset].mi_col_start = mi_col;
          this_mi[this_offset].mv[0] = cur_mi_info.mv[0];
          this_mi[this_offset].mv[1] = cur_mi_info.mv[1];
          this_mi[this_offset].ref_frame[0] = cur_mi_info.ref_frame[0];
          this_mi[this_offset].ref_frame[1] = cur_mi_info.ref_frame[1];
          this_mi[this_offset].pred_mode = cur_mi_info.mode;
        }
      }
    }
  }

  ctx->frame_info_count++;

  return 0;
}

#define USE_SECOND_PASS_FILE 1

#if !USE_SECOND_PASS_FILE
// Parse the frames in the gop and determine the last frame of the current GOP.
// Decode more frames if necessary. The variable max_num is the maximum static
// GOP length if we detect an IPPP structure, and it is expected that max_mum >=
// MAX_GF_INTERVAL.
static void get_current_gop_end(THIRD_PASS_DEC_CTX *ctx, int max_num,
                                int *last_idx) {
  assert(max_num >= MAX_GF_INTERVAL);
  *last_idx = 0;
  int cur_idx = 0;
  int arf_order_hint = -1;
  int num_show_frames = 0;
  while (num_show_frames < max_num) {
    assert(cur_idx < MAX_THIRD_PASS_BUF);
    // Read in from bitstream if needed.
    if (cur_idx >= ctx->frame_info_count) {
      int ret = get_frame_info(ctx);
      if (ret == 1) {
        // At the end of the file, GOP ends in the prev frame.
        if (arf_order_hint >= 0) {
          aom_internal_error(ctx->err_info, AOM_CODEC_ERROR,
                             "Failed to derive GOP length.");
        }
        *last_idx = cur_idx - 1;
        return;
      }
      if (ret < 0) {
        aom_internal_error(ctx->err_info, AOM_CODEC_ERROR,
                           "Failed to read frame for third pass.");
      }
    }

    // TODO(bohanli): verify that fwd_kf works here.
    if (ctx->frame_info[cur_idx].frame_type == KEY_FRAME &&
        ctx->frame_info[cur_idx].is_show_frame) {
      if (cur_idx != 0) {
        // If this is a key frame and is not the first kf in this kf group, we
        // have reached the next key frame. Stop here.
        *last_idx = cur_idx - 1;
        return;
      }
    } else if (!ctx->frame_info[cur_idx].is_show_frame &&
               arf_order_hint == -1) {
      // If this is an arf (the first no show)
      if (num_show_frames <= 1) {
        // This is an arf and we should end the GOP with its overlay.
        arf_order_hint = ctx->frame_info[cur_idx].order_hint;
      } else {
        // There are multiple show frames before the this arf, so we treat the
        // frames previous to this arf as a GOP.
        *last_idx = cur_idx - 1;
        return;
      }
    } else if (arf_order_hint >= 0 && ctx->frame_info[cur_idx].order_hint ==
                                          (unsigned int)arf_order_hint) {
      // If this is the overlay/show existing of the arf
      assert(ctx->frame_info[cur_idx].is_show_frame);
      *last_idx = cur_idx;
      return;
    } else {
      // This frame is part of the GOP.
      if (ctx->frame_info[cur_idx].is_show_frame) num_show_frames++;
    }
    cur_idx++;
  }
  // This is a long IPPP GOP and we will use a length of max_num here.
  assert(arf_order_hint < 0);
  *last_idx = max_num - 1;
  return;
}
#endif

static inline void read_gop_frames(THIRD_PASS_DEC_CTX *ctx) {
  int cur_idx = 0;
  while (cur_idx < ctx->gop_info.num_frames) {
    assert(cur_idx < MAX_THIRD_PASS_BUF);
    // Read in from bitstream if needed.
    if (cur_idx >= ctx->frame_info_count) {
      int ret = get_frame_info(ctx);
      if (ret != 0) {
        aom_internal_error(ctx->err_info, AOM_CODEC_ERROR,
                           "Failed to read frame for third pass.");
      }
    }
    cur_idx++;
  }
  return;
}

void av1_set_gop_third_pass(THIRD_PASS_DEC_CTX *ctx) {
  // Read in future frames in the current GOP.
  read_gop_frames(ctx);

  int gf_len = 0;
  // Check the GOP length against the value read from second_pass_file
  for (int i = 0; i < ctx->gop_info.num_frames; i++) {
    if (ctx->frame_info[i].is_show_frame) gf_len++;
  }

  if (gf_len != ctx->gop_info.gf_length) {
    aom_internal_error(ctx->err_info, AOM_CODEC_ERROR,
                       "Mismatch in third pass GOP length!");
  }
}

void av1_pop_third_pass_info(THIRD_PASS_DEC_CTX *ctx) {
  if (ctx->frame_info_count == 0) {
    aom_internal_error(ctx->err_info, AOM_CODEC_ERROR,
                       "No available frame info for third pass.");
  }
  ctx->frame_info_count--;
  free_frame_info(&ctx->frame_info[0]);
  for (int i = 0; i < ctx->frame_info_count; i++) {
    ctx->frame_info[i] = ctx->frame_info[i + 1];
  }
  ctx->frame_info[ctx->frame_info_count].mi_info = NULL;
}

void av1_init_thirdpass_ctx(AV1_COMMON *cm, THIRD_PASS_DEC_CTX **ctx,
                            const char *file) {
  av1_free_thirdpass_ctx(*ctx);
  CHECK_MEM_ERROR(cm, *ctx, aom_calloc(1, sizeof(**ctx)));
  THIRD_PASS_DEC_CTX *ctx_ptr = *ctx;
  ctx_ptr->input_file_name = file;
  ctx_ptr->prev_gop_end = -1;
  ctx_ptr->err_info = cm->error;
}

void av1_free_thirdpass_ctx(THIRD_PASS_DEC_CTX *ctx) {
  if (ctx == NULL) return;
  if (ctx->decoder.iface) {
    aom_codec_destroy(&ctx->decoder);
  }
  if (ctx->input_ctx && ctx->input_ctx->file) fclose(ctx->input_ctx->file);
  aom_free(ctx->input_ctx);
  if (ctx->buf) free(ctx->buf);
  for (int i = 0; i < MAX_THIRD_PASS_BUF; i++) {
    free_frame_info(&ctx->frame_info[i]);
  }
  aom_free(ctx);
}

void av1_write_second_pass_gop_info(AV1_COMP *cpi) {
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  const GF_GROUP *const gf_group = &cpi->ppi->gf_group;
  const PRIMARY_RATE_CONTROL *const p_rc = &cpi->ppi->p_rc;

  if (oxcf->pass == AOM_RC_SECOND_PASS && oxcf->second_pass_log) {
    // Write the GOP length to a log file.
    av1_open_second_pass_log(cpi, 0);

    THIRD_PASS_GOP_INFO gop_info;

    gop_info.num_frames = gf_group->size;
    gop_info.use_arf = (gf_group->arf_index >= 0);
    gop_info.gf_length = p_rc->baseline_gf_interval;

    size_t count =
        fwrite(&gop_info, sizeof(gop_info), 1, cpi->second_pass_log_stream);
    if (count < 1) {
      aom_internal_error(cpi->common.error, AOM_CODEC_ERROR,
                         "Could not write to second pass log file!");
    }
  }
}

void av1_write_second_pass_per_frame_info(AV1_COMP *cpi, int gf_index) {
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  const GF_GROUP *const gf_group = &cpi->ppi->gf_group;

  if (oxcf->pass == AOM_RC_SECOND_PASS && oxcf->second_pass_log) {
    // write target bitrate
    int bits = gf_group->bit_allocation[gf_index];
    size_t count = fwrite(&bits, sizeof(bits), 1, cpi->second_pass_log_stream);
    if (count < 1) {
      aom_internal_error(cpi->common.error, AOM_CODEC_ERROR,
                         "Could not write to second pass log file!");
    }

    // write sse
    uint64_t sse = 0;
    int pkt_idx = cpi->ppi->output_pkt_list->cnt - 1;
    if (pkt_idx >= 0 &&
        cpi->ppi->output_pkt_list->pkts[pkt_idx].kind == AOM_CODEC_PSNR_PKT) {
      sse = cpi->ppi->output_pkt_list->pkts[pkt_idx].data.psnr.sse[0];
#if CONFIG_INTERNAL_STATS
    } else if (cpi->ppi->b_calculate_psnr) {
      sse = cpi->ppi->total_sq_error[0];
#endif
    } else {
      const YV12_BUFFER_CONFIG *orig = cpi->source;
      const YV12_BUFFER_CONFIG *recon = &cpi->common.cur_frame->buf;
      PSNR_STATS psnr;
#if CONFIG_AV1_HIGHBITDEPTH
      const uint32_t in_bit_depth = cpi->oxcf.input_cfg.input_bit_depth;
      const uint32_t bit_depth = cpi->td.mb.e_mbd.bd;
      aom_calc_highbd_psnr(orig, recon, &psnr, bit_depth, in_bit_depth);
#else
      aom_calc_psnr(orig, recon, &psnr);
#endif
      sse = psnr.sse[0];
    }

    count = fwrite(&sse, sizeof(sse), 1, cpi->second_pass_log_stream);
    if (count < 1) {
      aom_internal_error(cpi->common.error, AOM_CODEC_ERROR,
                         "Could not write to second pass log file!");
    }

    // write bpm_factor
    double factor = cpi->ppi->twopass.bpm_factor;
    count = fwrite(&factor, sizeof(factor), 1, cpi->second_pass_log_stream);
    if (count < 1) {
      aom_internal_error(cpi->common.error, AOM_CODEC_ERROR,
                         "Could not write to second pass log file!");
    }
  }
}
void av1_open_second_pass_log(AV1_COMP *cpi, int is_read) {
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  if (oxcf->second_pass_log == NULL) {
    aom_internal_error(cpi->common.error, AOM_CODEC_INVALID_PARAM,
                       "No second pass log file specified for the third pass!");
  }
  // Read the GOP length from a file.
  if (!cpi->second_pass_log_stream) {
    if (is_read) {
      cpi->second_pass_log_stream = fopen(cpi->oxcf.second_pass_log, "rb");
    } else {
      cpi->second_pass_log_stream = fopen(cpi->oxcf.second_pass_log, "wb");
    }
    if (!cpi->second_pass_log_stream) {
      aom_internal_error(cpi->common.error, AOM_CODEC_ERROR,
                         "Could not open second pass log file!");
    }
  }
}

void av1_close_second_pass_log(AV1_COMP *cpi) {
  if (cpi->second_pass_log_stream) {
    int ret = fclose(cpi->second_pass_log_stream);
    if (ret != 0) {
      aom_internal_error(cpi->common.error, AOM_CODEC_ERROR,
                         "Could not close second pass log file!");
    }
    cpi->second_pass_log_stream = 0;
  }
}

void av1_read_second_pass_gop_info(FILE *second_pass_log_stream,
                                   THIRD_PASS_GOP_INFO *gop_info,
                                   struct aom_internal_error_info *error) {
  size_t count = fread(gop_info, sizeof(*gop_info), 1, second_pass_log_stream);
  if (count < 1) {
    aom_internal_error(error, AOM_CODEC_ERROR,
                       "Could not read from second pass log file!");
  }
}

void av1_read_second_pass_per_frame_info(
    FILE *second_pass_log_stream, THIRD_PASS_FRAME_INFO *frame_info_arr,
    int frame_info_count, struct aom_internal_error_info *error) {
  for (int i = 0; i < frame_info_count; i++) {
    // read target bits
    int bits = 0;
    size_t count = fread(&bits, sizeof(bits), 1, second_pass_log_stream);
    if (count < 1) {
      aom_internal_error(error, AOM_CODEC_ERROR,
                         "Could not read from second pass log file!");
    }
    frame_info_arr[i].bits_allocated = bits;

    // read distortion
    uint64_t sse;
    count = fread(&sse, sizeof(sse), 1, second_pass_log_stream);
    if (count < 1) {
      aom_internal_error(error, AOM_CODEC_ERROR,
                         "Could not read from second pass log file!");
    }
    frame_info_arr[i].sse = sse;

    // read bpm factor
    double factor;
    count = fread(&factor, sizeof(factor), 1, second_pass_log_stream);
    if (count < 1) {
      aom_internal_error(error, AOM_CODEC_ERROR,
                         "Could not read from second pass log file!");
    }
    frame_info_arr[i].bpm_factor = factor;
  }
}

int av1_check_use_arf(THIRD_PASS_DEC_CTX *ctx) {
  if (ctx == NULL) return -1;
  int use_arf = 0;
  for (int i = 0; i < ctx->gop_info.gf_length; i++) {
    if (ctx->frame_info[i].order_hint != 0 &&
        ctx->frame_info[i].is_show_frame == 0) {
      use_arf = 1;
    }
  }
  if (use_arf != ctx->gop_info.use_arf) {
    aom_internal_error(ctx->err_info, AOM_CODEC_ERROR,
                       "Mismatch in third pass GOP length!");
  }
  return use_arf;
}

void av1_get_third_pass_ratio(THIRD_PASS_DEC_CTX *ctx, int fidx, int fheight,
                              int fwidth, double *ratio_h, double *ratio_w) {
  assert(ctx);
  assert(fidx < ctx->frame_info_count);
  const int fheight_second_pass = ctx->frame_info[fidx].height;
  const int fwidth_second_pass = ctx->frame_info[fidx].width;
  assert(fheight_second_pass <= fheight && fwidth_second_pass <= fwidth);

  *ratio_h = (double)fheight / fheight_second_pass;
  *ratio_w = (double)fwidth / fwidth_second_pass;
}

THIRD_PASS_MI_INFO *av1_get_third_pass_mi(THIRD_PASS_DEC_CTX *ctx, int fidx,
                                          int mi_row, int mi_col,
                                          double ratio_h, double ratio_w) {
  assert(ctx);
  assert(fidx < ctx->frame_info_count);

  const int mi_rows_second_pass = ctx->frame_info[fidx].mi_rows;
  const int mi_cols_second_pass = ctx->frame_info[fidx].mi_cols;

  const int mi_row_second_pass =
      clamp((int)round(mi_row / ratio_h), 0, mi_rows_second_pass - 1);
  const int mi_col_second_pass =
      clamp((int)round(mi_col / ratio_w), 0, mi_cols_second_pass - 1);

  const int mi_stride_second_pass = ctx->frame_info[fidx].mi_stride;
  THIRD_PASS_MI_INFO *this_mi = ctx->frame_info[fidx].mi_info +
                                mi_row_second_pass * mi_stride_second_pass +
                                mi_col_second_pass;
  return this_mi;
}

void av1_third_pass_get_adjusted_mi(THIRD_PASS_MI_INFO *third_pass_mi,
                                    double ratio_h, double ratio_w, int *mi_row,
                                    int *mi_col) {
  *mi_row = (int)round(third_pass_mi->mi_row_start * ratio_h);
  *mi_col = (int)round(third_pass_mi->mi_col_start * ratio_w);
}

int_mv av1_get_third_pass_adjusted_mv(THIRD_PASS_MI_INFO *this_mi,
                                      double ratio_h, double ratio_w,
                                      MV_REFERENCE_FRAME frame) {
  assert(this_mi != NULL);
  int_mv cur_mv;
  cur_mv.as_int = INVALID_MV;

  if (frame < LAST_FRAME || frame > ALTREF_FRAME) return cur_mv;

  for (int r = 0; r < 2; r++) {
    if (this_mi->ref_frame[r] == frame) {
      cur_mv.as_mv.row = (int16_t)round(this_mi->mv[r].as_mv.row * ratio_h);
      cur_mv.as_mv.col = (int16_t)round(this_mi->mv[r].as_mv.col * ratio_w);
    }
  }

  return cur_mv;
}

BLOCK_SIZE av1_get_third_pass_adjusted_blk_size(THIRD_PASS_MI_INFO *this_mi,
                                                double ratio_h,
                                                double ratio_w) {
  assert(this_mi != NULL);
  BLOCK_SIZE bsize = BLOCK_INVALID;

  const BLOCK_SIZE bsize_second_pass = this_mi->bsize;
  assert(bsize_second_pass != BLOCK_INVALID);

  const int w_second_pass = block_size_wide[bsize_second_pass];
  const int h_second_pass = block_size_high[bsize_second_pass];

  int part_type;

  if (w_second_pass == h_second_pass) {
    part_type = PARTITION_NONE;
  } else if (w_second_pass / h_second_pass == 2) {
    part_type = PARTITION_HORZ;
  } else if (w_second_pass / h_second_pass == 4) {
    part_type = PARTITION_HORZ_4;
  } else if (h_second_pass / w_second_pass == 2) {
    part_type = PARTITION_VERT;
  } else if (h_second_pass / w_second_pass == 4) {
    part_type = PARTITION_VERT_4;
  } else {
    part_type = PARTITION_INVALID;
  }
  assert(part_type != PARTITION_INVALID);

  const int w = (int)(round(w_second_pass * ratio_w));
  const int h = (int)(round(h_second_pass * ratio_h));

  for (int i = 0; i < SQR_BLOCK_SIZES; i++) {
    const BLOCK_SIZE this_bsize = subsize_lookup[part_type][i];
    if (this_bsize == BLOCK_INVALID) continue;

    const int this_w = block_size_wide[this_bsize];
    const int this_h = block_size_high[this_bsize];

    if (this_w >= w && this_h >= h) {
      // find the smallest block size that contains the mapped block
      bsize = this_bsize;
      break;
    }
  }
  if (bsize == BLOCK_INVALID) {
    // could not find a proper one, just use the largest then.
    bsize = BLOCK_128X128;
  }

  return bsize;
}

PARTITION_TYPE av1_third_pass_get_sb_part_type(THIRD_PASS_DEC_CTX *ctx,
                                               THIRD_PASS_MI_INFO *this_mi) {
  int mi_stride = ctx->frame_info[0].mi_stride;

  int mi_row = this_mi->mi_row_start;
  int mi_col = this_mi->mi_col_start;

  THIRD_PASS_MI_INFO *corner_mi =
      &ctx->frame_info[0].mi_info[mi_row * mi_stride + mi_col];

  return corner_mi->partition;
}

#else   // !(CONFIG_THREE_PASS && CONFIG_AV1_DECODER)
void av1_init_thirdpass_ctx(AV1_COMMON *cm, THIRD_PASS_DEC_CTX **ctx,
                            const char *file) {
  (void)ctx;
  (void)file;
  aom_internal_error(cm->error, AOM_CODEC_ERROR,
                     "To utilize three-pass encoding, libaom must be built "
                     "with CONFIG_THREE_PASS=1 & CONFIG_AV1_DECODER=1.");
}

void av1_free_thirdpass_ctx(THIRD_PASS_DEC_CTX *ctx) { (void)ctx; }

void av1_set_gop_third_pass(THIRD_PASS_DEC_CTX *ctx) { (void)ctx; }

void av1_pop_third_pass_info(THIRD_PASS_DEC_CTX *ctx) { (void)ctx; }

void av1_open_second_pass_log(struct AV1_COMP *cpi, int is_read) {
  (void)cpi;
  (void)is_read;
}

void av1_close_second_pass_log(struct AV1_COMP *cpi) { (void)cpi; }

void av1_write_second_pass_gop_info(struct AV1_COMP *cpi) { (void)cpi; }

void av1_write_second_pass_per_frame_info(struct AV1_COMP *cpi, int gf_index) {
  (void)cpi;
  (void)gf_index;
}

void av1_read_second_pass_gop_info(FILE *second_pass_log_stream,
                                   THIRD_PASS_GOP_INFO *gop_info,
                                   struct aom_internal_error_info *error) {
  (void)second_pass_log_stream;
  (void)gop_info;
  (void)error;
}

void av1_read_second_pass_per_frame_info(
    FILE *second_pass_log_stream, THIRD_PASS_FRAME_INFO *frame_info_arr,
    int frame_info_count, struct aom_internal_error_info *error) {
  (void)second_pass_log_stream;
  (void)frame_info_arr;
  (void)frame_info_count;
  (void)error;
}

int av1_check_use_arf(THIRD_PASS_DEC_CTX *ctx) {
  (void)ctx;
  return 1;
}

void av1_get_third_pass_ratio(THIRD_PASS_DEC_CTX *ctx, int fidx, int fheight,
                              int fwidth, double *ratio_h, double *ratio_w) {
  (void)ctx;
  (void)fidx;
  (void)fheight;
  (void)fwidth;
  (void)ratio_h;
  (void)ratio_w;
}

THIRD_PASS_MI_INFO *av1_get_third_pass_mi(THIRD_PASS_DEC_CTX *ctx, int fidx,
                                          int mi_row, int mi_col,
                                          double ratio_h, double ratio_w) {
  (void)ctx;
  (void)fidx;
  (void)mi_row;
  (void)mi_col;
  (void)ratio_h;
  (void)ratio_w;
  return NULL;
}

int_mv av1_get_third_pass_adjusted_mv(THIRD_PASS_MI_INFO *this_mi,
                                      double ratio_h, double ratio_w,
                                      MV_REFERENCE_FRAME frame) {
  (void)this_mi;
  (void)ratio_h;
  (void)ratio_w;
  (void)frame;
  int_mv mv;
  mv.as_int = INVALID_MV;
  return mv;
}

BLOCK_SIZE av1_get_third_pass_adjusted_blk_size(THIRD_PASS_MI_INFO *this_mi,
                                                double ratio_h,
                                                double ratio_w) {
  (void)this_mi;
  (void)ratio_h;
  (void)ratio_w;
  return BLOCK_INVALID;
}

void av1_third_pass_get_adjusted_mi(THIRD_PASS_MI_INFO *third_pass_mi,
                                    double ratio_h, double ratio_w, int *mi_row,
                                    int *mi_col) {
  (void)third_pass_mi;
  (void)ratio_h;
  (void)ratio_w;
  (void)mi_row;
  (void)mi_col;
}

PARTITION_TYPE av1_third_pass_get_sb_part_type(THIRD_PASS_DEC_CTX *ctx,
                                               THIRD_PASS_MI_INFO *this_mi) {
  (void)ctx;
  (void)this_mi;
  return PARTITION_INVALID;
}
#endif  // CONFIG_THREE_PASS && CONFIG_AV1_DECODER

#if CONFIG_BITRATE_ACCURACY
static void fwrite_and_check(const void *ptr, size_t size, size_t nmemb,
                             FILE *stream,
                             struct aom_internal_error_info *error) {
  size_t count = fwrite(ptr, size, nmemb, stream);
  if (count < nmemb) {
    aom_internal_error(error, AOM_CODEC_ERROR, "fwrite_and_check failed\n");
  }
}

static void fread_and_check(void *ptr, size_t size, size_t nmemb, FILE *stream,
                            struct aom_internal_error_info *error) {
  size_t count = fread(ptr, size, nmemb, stream);
  if (count < nmemb) {
    aom_internal_error(error, AOM_CODEC_ERROR, "fread_and_check failed\n");
  }
}

void av1_pack_tpl_info(TPL_INFO *tpl_info, const GF_GROUP *gf_group,
                       const TplParams *tpl_data) {
  tpl_info->tpl_ready = tpl_data->ready;
  if (tpl_info->tpl_ready) {
    tpl_info->gf_length = gf_group->size;
    for (int i = 0; i < tpl_info->gf_length; ++i) {
      tpl_info->txfm_stats_list[i] = tpl_data->txfm_stats_list[i];
      tpl_info->qstep_ratio_ls[i] = av1_tpl_get_qstep_ratio(tpl_data, i);
      tpl_info->update_type_list[i] = gf_group->update_type[i];
    }
  }
}

void av1_write_tpl_info(const TPL_INFO *tpl_info, FILE *log_stream,
                        struct aom_internal_error_info *error) {
  fwrite_and_check(&tpl_info->tpl_ready, sizeof(tpl_info->tpl_ready), 1,
                   log_stream, error);
  if (tpl_info->tpl_ready) {
    fwrite_and_check(&tpl_info->gf_length, sizeof(tpl_info->gf_length), 1,
                     log_stream, error);
    assert(tpl_info->gf_length <= MAX_LENGTH_TPL_FRAME_STATS);
    fwrite_and_check(&tpl_info->txfm_stats_list,
                     sizeof(tpl_info->txfm_stats_list[0]), tpl_info->gf_length,
                     log_stream, error);
    fwrite_and_check(&tpl_info->qstep_ratio_ls,
                     sizeof(tpl_info->qstep_ratio_ls[0]), tpl_info->gf_length,
                     log_stream, error);
    fwrite_and_check(&tpl_info->update_type_list,
                     sizeof(tpl_info->update_type_list[0]), tpl_info->gf_length,
                     log_stream, error);
  }
}

void av1_read_tpl_info(TPL_INFO *tpl_info, FILE *log_stream,
                       struct aom_internal_error_info *error) {
  av1_zero(*tpl_info);
  fread_and_check(&tpl_info->tpl_ready, sizeof(tpl_info->tpl_ready), 1,
                  log_stream, error);
  if (tpl_info->tpl_ready) {
    fread_and_check(&tpl_info->gf_length, sizeof(tpl_info->gf_length), 1,
                    log_stream, error);
    assert(tpl_info->gf_length <= MAX_LENGTH_TPL_FRAME_STATS);
    fread_and_check(&tpl_info->txfm_stats_list,
                    sizeof(tpl_info->txfm_stats_list[0]), tpl_info->gf_length,
                    log_stream, error);
    fread_and_check(&tpl_info->qstep_ratio_ls,
                    sizeof(tpl_info->qstep_ratio_ls[0]), tpl_info->gf_length,
                    log_stream, error);
    fread_and_check(&tpl_info->update_type_list,
                    sizeof(tpl_info->update_type_list[0]), tpl_info->gf_length,
                    log_stream, error);
  }
}
#endif  // CONFIG_BITRATE_ACCURACY
