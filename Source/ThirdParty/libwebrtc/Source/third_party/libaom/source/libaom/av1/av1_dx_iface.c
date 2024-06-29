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

#include <stdlib.h>
#include <string.h>

#include "config/aom_config.h"
#include "config/aom_version.h"

#include "aom/internal/aom_codec_internal.h"
#include "aom/internal/aom_image_internal.h"
#include "aom/aomdx.h"
#include "aom/aom_decoder.h"
#include "aom/aom_image.h"
#include "aom_dsp/bitreader_buffer.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_ports/mem.h"
#include "aom_ports/mem_ops.h"
#include "aom_util/aom_pthread.h"
#include "aom_util/aom_thread.h"

#include "av1/common/alloccommon.h"
#include "av1/common/av1_common_int.h"
#include "av1/common/frame_buffers.h"
#include "av1/common/enums.h"
#include "av1/common/obu_util.h"

#include "av1/decoder/decoder.h"
#include "av1/decoder/decodeframe.h"
#include "av1/decoder/dthread.h"
#include "av1/decoder/grain_synthesis.h"
#include "av1/decoder/obu.h"

#include "av1/av1_iface_common.h"

struct aom_codec_alg_priv {
  aom_codec_priv_t base;
  aom_codec_dec_cfg_t cfg;
  aom_codec_stream_info_t si;
  aom_image_t img;
  int img_avail;
  int flushed;
  int invert_tile_order;
  RefCntBuffer *last_show_frame;  // Last output frame buffer
  int byte_alignment;
  int skip_loop_filter;
  int skip_film_grain;
  int decode_tile_row;
  int decode_tile_col;
  unsigned int tile_mode;
  unsigned int ext_tile_debug;
  unsigned int row_mt;
  EXTERNAL_REFERENCES ext_refs;
  unsigned int is_annexb;
  int operating_point;
  int output_all_layers;

  AVxWorker *frame_worker;

  aom_image_t image_with_grain;
  aom_codec_frame_buffer_t grain_image_frame_buffers[MAX_NUM_SPATIAL_LAYERS];
  size_t num_grain_image_frame_buffers;
  int need_resync;  // wait for key/intra-only frame
  // BufferPool that holds all reference frames. Shared by all the FrameWorkers.
  BufferPool *buffer_pool;

  // External frame buffer info to save for AV1 common.
  void *ext_priv;  // Private data associated with the external frame buffers.
  aom_get_frame_buffer_cb_fn_t get_ext_fb_cb;
  aom_release_frame_buffer_cb_fn_t release_ext_fb_cb;

#if CONFIG_INSPECTION
  aom_inspect_cb inspect_cb;
  void *inspect_ctx;
#endif
};

static aom_codec_err_t decoder_init(aom_codec_ctx_t *ctx) {
  // This function only allocates space for the aom_codec_alg_priv_t
  // structure. More memory may be required at the time the stream
  // information becomes known.
  if (!ctx->priv) {
    aom_codec_alg_priv_t *const priv =
        (aom_codec_alg_priv_t *)aom_calloc(1, sizeof(*priv));
    if (priv == NULL) return AOM_CODEC_MEM_ERROR;

    ctx->priv = (aom_codec_priv_t *)priv;
    ctx->priv->init_flags = ctx->init_flags;
    priv->flushed = 0;

    // TODO(tdaede): this should not be exposed to the API
    priv->cfg.allow_lowbitdepth = !FORCE_HIGHBITDEPTH_DECODING;
    if (ctx->config.dec) {
      priv->cfg = *ctx->config.dec;
      ctx->config.dec = &priv->cfg;
    }
    priv->num_grain_image_frame_buffers = 0;
    // Turn row_mt on by default.
    priv->row_mt = 1;

    // Turn on normal tile coding mode by default.
    // 0 is for normal tile coding mode, and 1 is for large scale tile coding
    // mode(refer to lightfield example).
    priv->tile_mode = 0;
    priv->decode_tile_row = -1;
    priv->decode_tile_col = -1;
  }

  return AOM_CODEC_OK;
}

static aom_codec_err_t decoder_destroy(aom_codec_alg_priv_t *ctx) {
  if (ctx->frame_worker != NULL) {
    AVxWorker *const worker = ctx->frame_worker;
    aom_get_worker_interface()->end(worker);
    FrameWorkerData *const frame_worker_data = (FrameWorkerData *)worker->data1;
    if (frame_worker_data != NULL && frame_worker_data->pbi != NULL) {
      AV1Decoder *const pbi = frame_worker_data->pbi;
      aom_free(pbi->common.tpl_mvs);
      pbi->common.tpl_mvs = NULL;
      av1_remove_common(&pbi->common);
      av1_free_cdef_buffers(&pbi->common, &pbi->cdef_worker, &pbi->cdef_sync);
      av1_free_cdef_sync(&pbi->cdef_sync);
      av1_free_restoration_buffers(&pbi->common);
      av1_decoder_remove(pbi);
    }
    aom_free(frame_worker_data);
  }

  if (ctx->buffer_pool) {
    for (size_t i = 0; i < ctx->num_grain_image_frame_buffers; i++) {
      ctx->buffer_pool->release_fb_cb(ctx->buffer_pool->cb_priv,
                                      &ctx->grain_image_frame_buffers[i]);
    }
    av1_free_ref_frame_buffers(ctx->buffer_pool);
    av1_free_internal_frame_buffers(&ctx->buffer_pool->int_frame_buffers);
#if CONFIG_MULTITHREAD
    pthread_mutex_destroy(&ctx->buffer_pool->pool_mutex);
#endif
  }

  aom_free(ctx->frame_worker);
  aom_free(ctx->buffer_pool);
  assert(!ctx->img.self_allocd);
  aom_img_free(&ctx->img);
  aom_free(ctx);
  return AOM_CODEC_OK;
}

static aom_codec_err_t parse_timing_info(struct aom_read_bit_buffer *rb) {
  const uint32_t num_units_in_display_tick =
      aom_rb_read_unsigned_literal(rb, 32);
  const uint32_t time_scale = aom_rb_read_unsigned_literal(rb, 32);
  if (num_units_in_display_tick == 0 || time_scale == 0)
    return AOM_CODEC_UNSUP_BITSTREAM;
  const uint8_t equal_picture_interval = aom_rb_read_bit(rb);
  if (equal_picture_interval) {
    const uint32_t num_ticks_per_picture_minus_1 = aom_rb_read_uvlc(rb);
    if (num_ticks_per_picture_minus_1 == UINT32_MAX) {
      // num_ticks_per_picture_minus_1 cannot be (1 << 32) - 1.
      return AOM_CODEC_UNSUP_BITSTREAM;
    }
  }
  return AOM_CODEC_OK;
}

static aom_codec_err_t parse_decoder_model_info(
    struct aom_read_bit_buffer *rb, int *buffer_delay_length_minus_1) {
  *buffer_delay_length_minus_1 = aom_rb_read_literal(rb, 5);
  const uint32_t num_units_in_decoding_tick =
      aom_rb_read_unsigned_literal(rb, 32);
  const uint8_t buffer_removal_time_length_minus_1 = aom_rb_read_literal(rb, 5);
  const uint8_t frame_presentation_time_length_minus_1 =
      aom_rb_read_literal(rb, 5);
  (void)num_units_in_decoding_tick;
  (void)buffer_removal_time_length_minus_1;
  (void)frame_presentation_time_length_minus_1;
  return AOM_CODEC_OK;
}

static aom_codec_err_t parse_op_parameters_info(
    struct aom_read_bit_buffer *rb, int buffer_delay_length_minus_1) {
  const int n = buffer_delay_length_minus_1 + 1;
  const uint32_t decoder_buffer_delay = aom_rb_read_unsigned_literal(rb, n);
  const uint32_t encoder_buffer_delay = aom_rb_read_unsigned_literal(rb, n);
  const uint8_t low_delay_mode_flag = aom_rb_read_bit(rb);
  (void)decoder_buffer_delay;
  (void)encoder_buffer_delay;
  (void)low_delay_mode_flag;
  return AOM_CODEC_OK;
}

// Parses the operating points (including operating_point_idc, seq_level_idx,
// and seq_tier) and then sets si->number_spatial_layers and
// si->number_temporal_layers based on operating_point_idc[0].
static aom_codec_err_t parse_operating_points(struct aom_read_bit_buffer *rb,
                                              int is_reduced_header,
                                              aom_codec_stream_info_t *si) {
  int operating_point_idc0 = 0;
  if (is_reduced_header) {
    aom_rb_read_literal(rb, LEVEL_BITS);  // level
  } else {
    uint8_t decoder_model_info_present_flag = 0;
    int buffer_delay_length_minus_1 = 0;
    aom_codec_err_t status;
    const uint8_t timing_info_present_flag = aom_rb_read_bit(rb);
    if (timing_info_present_flag) {
      if ((status = parse_timing_info(rb)) != AOM_CODEC_OK) return status;
      decoder_model_info_present_flag = aom_rb_read_bit(rb);
      if (decoder_model_info_present_flag) {
        if ((status = parse_decoder_model_info(
                 rb, &buffer_delay_length_minus_1)) != AOM_CODEC_OK)
          return status;
      }
    }
    const uint8_t initial_display_delay_present_flag = aom_rb_read_bit(rb);
    const uint8_t operating_points_cnt_minus_1 =
        aom_rb_read_literal(rb, OP_POINTS_CNT_MINUS_1_BITS);
    for (int i = 0; i < operating_points_cnt_minus_1 + 1; i++) {
      int operating_point_idc;
      operating_point_idc = aom_rb_read_literal(rb, OP_POINTS_IDC_BITS);
      if (i == 0) operating_point_idc0 = operating_point_idc;
      int seq_level_idx = aom_rb_read_literal(rb, LEVEL_BITS);  // level
      if (seq_level_idx > 7) aom_rb_read_bit(rb);               // tier
      if (decoder_model_info_present_flag) {
        const uint8_t decoder_model_present_for_this_op = aom_rb_read_bit(rb);
        if (decoder_model_present_for_this_op) {
          if ((status = parse_op_parameters_info(
                   rb, buffer_delay_length_minus_1)) != AOM_CODEC_OK)
            return status;
        }
      }
      if (initial_display_delay_present_flag) {
        const uint8_t initial_display_delay_present_for_this_op =
            aom_rb_read_bit(rb);
        if (initial_display_delay_present_for_this_op)
          aom_rb_read_literal(rb, 4);  // initial_display_delay_minus_1
      }
    }
  }

  if (aom_get_num_layers_from_operating_point_idc(
          operating_point_idc0, &si->number_spatial_layers,
          &si->number_temporal_layers) != AOM_CODEC_OK) {
    return AOM_CODEC_ERROR;
  }

  return AOM_CODEC_OK;
}

static aom_codec_err_t decoder_peek_si_internal(const uint8_t *data,
                                                size_t data_sz,
                                                aom_codec_stream_info_t *si,
                                                int *is_intra_only) {
  int intra_only_flag = 0;
  int got_sequence_header = 0;
  int found_keyframe = 0;

  if (data + data_sz <= data || data_sz < 1) return AOM_CODEC_INVALID_PARAM;

  si->w = 0;
  si->h = 0;
  si->is_kf = 0;  // is_kf indicates whether the current packet contains a RAP

  ObuHeader obu_header;
  memset(&obu_header, 0, sizeof(obu_header));
  size_t payload_size = 0;
  size_t bytes_read = 0;
  uint8_t reduced_still_picture_hdr = 0;
  aom_codec_err_t status = aom_read_obu_header_and_size(
      data, data_sz, si->is_annexb, &obu_header, &payload_size, &bytes_read);
  if (status != AOM_CODEC_OK) return status;

  // If the first OBU is a temporal delimiter, skip over it and look at the next
  // OBU in the bitstream
  if (obu_header.type == OBU_TEMPORAL_DELIMITER) {
    // Skip any associated payload (there shouldn't be one, but just in case)
    if (data_sz < bytes_read + payload_size) return AOM_CODEC_CORRUPT_FRAME;
    data += bytes_read + payload_size;
    data_sz -= bytes_read + payload_size;

    status = aom_read_obu_header_and_size(
        data, data_sz, si->is_annexb, &obu_header, &payload_size, &bytes_read);
    if (status != AOM_CODEC_OK) return status;
  }
  while (1) {
    data += bytes_read;
    data_sz -= bytes_read;
    if (data_sz < payload_size) return AOM_CODEC_CORRUPT_FRAME;
    // Check that the selected OBU is a sequence header
    if (obu_header.type == OBU_SEQUENCE_HEADER) {
      // Sanity check on sequence header size
      if (data_sz < 2) return AOM_CODEC_CORRUPT_FRAME;
      // Read a few values from the sequence header payload
      struct aom_read_bit_buffer rb = { data, data + data_sz, 0, NULL, NULL };

      av1_read_profile(&rb);  // profile
      const uint8_t still_picture = aom_rb_read_bit(&rb);
      reduced_still_picture_hdr = aom_rb_read_bit(&rb);

      if (!still_picture && reduced_still_picture_hdr) {
        return AOM_CODEC_UNSUP_BITSTREAM;
      }

      if (parse_operating_points(&rb, reduced_still_picture_hdr, si) !=
          AOM_CODEC_OK) {
        return AOM_CODEC_ERROR;
      }

      int num_bits_width = aom_rb_read_literal(&rb, 4) + 1;
      int num_bits_height = aom_rb_read_literal(&rb, 4) + 1;
      int max_frame_width = aom_rb_read_literal(&rb, num_bits_width) + 1;
      int max_frame_height = aom_rb_read_literal(&rb, num_bits_height) + 1;
      si->w = max_frame_width;
      si->h = max_frame_height;
      got_sequence_header = 1;
    } else if (obu_header.type == OBU_FRAME_HEADER ||
               obu_header.type == OBU_FRAME) {
      if (got_sequence_header && reduced_still_picture_hdr) {
        found_keyframe = 1;
        break;
      } else {
        // make sure we have enough bits to get the frame type out
        if (data_sz < 1) return AOM_CODEC_CORRUPT_FRAME;
        struct aom_read_bit_buffer rb = { data, data + data_sz, 0, NULL, NULL };
        const int show_existing_frame = aom_rb_read_bit(&rb);
        if (!show_existing_frame) {
          const FRAME_TYPE frame_type = (FRAME_TYPE)aom_rb_read_literal(&rb, 2);
          if (frame_type == KEY_FRAME) {
            found_keyframe = 1;
            break;  // Stop here as no further OBUs will change the outcome.
          } else if (frame_type == INTRA_ONLY_FRAME) {
            intra_only_flag = 1;
          }
        }
      }
    }
    // skip past any unread OBU header data
    data += payload_size;
    data_sz -= payload_size;
    if (data_sz == 0) break;  // exit if we're out of OBUs
    status = aom_read_obu_header_and_size(
        data, data_sz, si->is_annexb, &obu_header, &payload_size, &bytes_read);
    if (status != AOM_CODEC_OK) return status;
  }
  if (got_sequence_header && found_keyframe) si->is_kf = 1;
  if (is_intra_only != NULL) *is_intra_only = intra_only_flag;
  return AOM_CODEC_OK;
}

static aom_codec_err_t decoder_peek_si(const uint8_t *data, size_t data_sz,
                                       aom_codec_stream_info_t *si) {
  return decoder_peek_si_internal(data, data_sz, si, NULL);
}

static aom_codec_err_t decoder_get_si(aom_codec_alg_priv_t *ctx,
                                      aom_codec_stream_info_t *si) {
  memcpy(si, &ctx->si, sizeof(*si));

  return AOM_CODEC_OK;
}

static void set_error_detail(aom_codec_alg_priv_t *ctx,
                             const char *const error) {
  ctx->base.err_detail = error;
}

static aom_codec_err_t update_error_state(
    aom_codec_alg_priv_t *ctx, const struct aom_internal_error_info *error) {
  if (error->error_code)
    set_error_detail(ctx, error->has_detail ? error->detail : NULL);

  return error->error_code;
}

static void init_buffer_callbacks(aom_codec_alg_priv_t *ctx) {
  AVxWorker *const worker = ctx->frame_worker;
  FrameWorkerData *const frame_worker_data = (FrameWorkerData *)worker->data1;
  AV1Decoder *const pbi = frame_worker_data->pbi;
  AV1_COMMON *const cm = &pbi->common;
  BufferPool *const pool = cm->buffer_pool;

  cm->cur_frame = NULL;
  cm->features.byte_alignment = ctx->byte_alignment;
  pbi->skip_loop_filter = ctx->skip_loop_filter;
  pbi->skip_film_grain = ctx->skip_film_grain;

  if (ctx->get_ext_fb_cb != NULL && ctx->release_ext_fb_cb != NULL) {
    pool->get_fb_cb = ctx->get_ext_fb_cb;
    pool->release_fb_cb = ctx->release_ext_fb_cb;
    pool->cb_priv = ctx->ext_priv;
  } else {
    pool->get_fb_cb = av1_get_frame_buffer;
    pool->release_fb_cb = av1_release_frame_buffer;

    if (av1_alloc_internal_frame_buffers(&pool->int_frame_buffers))
      aom_internal_error(&pbi->error, AOM_CODEC_MEM_ERROR,
                         "Failed to initialize internal frame buffers");

    pool->cb_priv = &pool->int_frame_buffers;
  }
}

static int frame_worker_hook(void *arg1, void *arg2) {
  FrameWorkerData *const frame_worker_data = (FrameWorkerData *)arg1;
  const uint8_t *data = frame_worker_data->data;
  (void)arg2;

  int result = av1_receive_compressed_data(frame_worker_data->pbi,
                                           frame_worker_data->data_size, &data);
  frame_worker_data->data_end = data;

  if (result != 0) {
    // Check decode result in serial decode.
    frame_worker_data->pbi->need_resync = 1;
  }
  return !result;
}

static aom_codec_err_t init_decoder(aom_codec_alg_priv_t *ctx) {
  const AVxWorkerInterface *const winterface = aom_get_worker_interface();

  ctx->last_show_frame = NULL;
  ctx->need_resync = 1;
  ctx->flushed = 0;

  ctx->buffer_pool = (BufferPool *)aom_calloc(1, sizeof(BufferPool));
  if (ctx->buffer_pool == NULL) return AOM_CODEC_MEM_ERROR;
  ctx->buffer_pool->num_frame_bufs = FRAME_BUFFERS;
  ctx->buffer_pool->frame_bufs = (RefCntBuffer *)aom_calloc(
      ctx->buffer_pool->num_frame_bufs, sizeof(*ctx->buffer_pool->frame_bufs));
  if (ctx->buffer_pool->frame_bufs == NULL) {
    ctx->buffer_pool->num_frame_bufs = 0;
    aom_free(ctx->buffer_pool);
    ctx->buffer_pool = NULL;
    return AOM_CODEC_MEM_ERROR;
  }

#if CONFIG_MULTITHREAD
  if (pthread_mutex_init(&ctx->buffer_pool->pool_mutex, NULL)) {
    aom_free(ctx->buffer_pool->frame_bufs);
    ctx->buffer_pool->frame_bufs = NULL;
    ctx->buffer_pool->num_frame_bufs = 0;
    aom_free(ctx->buffer_pool);
    ctx->buffer_pool = NULL;
    set_error_detail(ctx, "Failed to allocate buffer pool mutex");
    return AOM_CODEC_MEM_ERROR;
  }
#endif

  ctx->frame_worker = (AVxWorker *)aom_malloc(sizeof(*ctx->frame_worker));
  if (ctx->frame_worker == NULL) {
    set_error_detail(ctx, "Failed to allocate frame_worker");
    return AOM_CODEC_MEM_ERROR;
  }

  AVxWorker *const worker = ctx->frame_worker;
  winterface->init(worker);
  worker->thread_name = "aom frameworker";
  worker->data1 = aom_memalign(32, sizeof(FrameWorkerData));
  if (worker->data1 == NULL) {
    winterface->end(worker);
    aom_free(worker);
    ctx->frame_worker = NULL;
    set_error_detail(ctx, "Failed to allocate frame_worker_data");
    return AOM_CODEC_MEM_ERROR;
  }
  FrameWorkerData *frame_worker_data = (FrameWorkerData *)worker->data1;
  frame_worker_data->pbi = av1_decoder_create(ctx->buffer_pool);
  if (frame_worker_data->pbi == NULL) {
    winterface->end(worker);
    aom_free(frame_worker_data);
    aom_free(worker);
    ctx->frame_worker = NULL;
    set_error_detail(ctx, "Failed to allocate frame_worker_data->pbi");
    return AOM_CODEC_MEM_ERROR;
  }
  frame_worker_data->frame_context_ready = 0;
  frame_worker_data->received_frame = 0;
  frame_worker_data->pbi->allow_lowbitdepth = ctx->cfg.allow_lowbitdepth;

  // If decoding in serial mode, FrameWorker thread could create tile worker
  // thread or loopfilter thread.
  frame_worker_data->pbi->max_threads = ctx->cfg.threads;
  frame_worker_data->pbi->inv_tile_order = ctx->invert_tile_order;
  frame_worker_data->pbi->common.tiles.large_scale = ctx->tile_mode;
  frame_worker_data->pbi->is_annexb = ctx->is_annexb;
  frame_worker_data->pbi->dec_tile_row = ctx->decode_tile_row;
  frame_worker_data->pbi->dec_tile_col = ctx->decode_tile_col;
  frame_worker_data->pbi->operating_point = ctx->operating_point;
  frame_worker_data->pbi->output_all_layers = ctx->output_all_layers;
  frame_worker_data->pbi->ext_tile_debug = ctx->ext_tile_debug;
  frame_worker_data->pbi->row_mt = ctx->row_mt;
  frame_worker_data->pbi->is_fwd_kf_present = 0;
  frame_worker_data->pbi->is_arf_frame_present = 0;
  worker->hook = frame_worker_hook;

  init_buffer_callbacks(ctx);

  return AOM_CODEC_OK;
}

static INLINE void check_resync(aom_codec_alg_priv_t *const ctx,
                                const AV1Decoder *const pbi) {
  // Clear resync flag if worker got a key frame or intra only frame.
  if (ctx->need_resync == 1 && pbi->need_resync == 0 &&
      frame_is_intra_only(&pbi->common))
    ctx->need_resync = 0;
}

static aom_codec_err_t decode_one(aom_codec_alg_priv_t *ctx,
                                  const uint8_t **data, size_t data_sz,
                                  void *user_priv) {
  const AVxWorkerInterface *const winterface = aom_get_worker_interface();

  // Determine the stream parameters. Note that we rely on peek_si to
  // validate that we have a buffer that does not wrap around the top
  // of the heap.
  if (!ctx->si.h) {
    int is_intra_only = 0;
    ctx->si.is_annexb = ctx->is_annexb;
    const aom_codec_err_t res =
        decoder_peek_si_internal(*data, data_sz, &ctx->si, &is_intra_only);
    if (res != AOM_CODEC_OK) return res;

    if (!ctx->si.is_kf && !is_intra_only) return AOM_CODEC_ERROR;
  }

  AVxWorker *const worker = ctx->frame_worker;
  FrameWorkerData *const frame_worker_data = (FrameWorkerData *)worker->data1;
  frame_worker_data->data = *data;
  frame_worker_data->data_size = data_sz;
  frame_worker_data->user_priv = user_priv;
  frame_worker_data->received_frame = 1;

  frame_worker_data->pbi->common.tiles.large_scale = ctx->tile_mode;
  frame_worker_data->pbi->dec_tile_row = ctx->decode_tile_row;
  frame_worker_data->pbi->dec_tile_col = ctx->decode_tile_col;
  frame_worker_data->pbi->ext_tile_debug = ctx->ext_tile_debug;
  frame_worker_data->pbi->row_mt = ctx->row_mt;
  frame_worker_data->pbi->ext_refs = ctx->ext_refs;

  frame_worker_data->pbi->is_annexb = ctx->is_annexb;

  worker->had_error = 0;
  winterface->execute(worker);

  // Update data pointer after decode.
  *data = frame_worker_data->data_end;

  if (worker->had_error)
    return update_error_state(ctx, &frame_worker_data->pbi->error);

  check_resync(ctx, frame_worker_data->pbi);

  return AOM_CODEC_OK;
}

static void release_pending_output_frames(aom_codec_alg_priv_t *ctx) {
  // Release any pending output frames from the previous decoder_decode or
  // decoder_inspect call. We need to do this even if the decoder is being
  // flushed or the input arguments are invalid.
  if (ctx->frame_worker) {
    BufferPool *const pool = ctx->buffer_pool;
    lock_buffer_pool(pool);
    AVxWorker *const worker = ctx->frame_worker;
    FrameWorkerData *const frame_worker_data = (FrameWorkerData *)worker->data1;
    struct AV1Decoder *pbi = frame_worker_data->pbi;
    for (size_t j = 0; j < pbi->num_output_frames; j++) {
      decrease_ref_count(pbi->output_frames[j], pool);
    }
    pbi->num_output_frames = 0;
    unlock_buffer_pool(pool);
    for (size_t j = 0; j < ctx->num_grain_image_frame_buffers; j++) {
      pool->release_fb_cb(pool->cb_priv, &ctx->grain_image_frame_buffers[j]);
      ctx->grain_image_frame_buffers[j].data = NULL;
      ctx->grain_image_frame_buffers[j].size = 0;
      ctx->grain_image_frame_buffers[j].priv = NULL;
    }
    ctx->num_grain_image_frame_buffers = 0;
  }
}

// This function enables the inspector to inspect non visible frames.
static aom_codec_err_t decoder_inspect(aom_codec_alg_priv_t *ctx,
                                       const uint8_t *data, size_t data_sz,
                                       void *user_priv) {
  aom_codec_err_t res = AOM_CODEC_OK;

  release_pending_output_frames(ctx);

  /* Sanity checks */
  /* NULL data ptr allowed if data_sz is 0 too */
  if (data == NULL && data_sz == 0) {
    ctx->flushed = 1;
    return AOM_CODEC_OK;
  }
  if (data == NULL || data_sz == 0) return AOM_CODEC_INVALID_PARAM;

  // Reset flushed when receiving a valid frame.
  ctx->flushed = 0;

  const uint8_t *data_start = data;
  const uint8_t *data_end = data + data_sz;

  uint64_t frame_size;
  if (ctx->is_annexb) {
    // read the size of this temporal unit
    size_t length_of_size;
    uint64_t temporal_unit_size;
    if (aom_uleb_decode(data_start, data_sz, &temporal_unit_size,
                        &length_of_size) != 0) {
      return AOM_CODEC_CORRUPT_FRAME;
    }
    data_start += length_of_size;
    if (temporal_unit_size > (size_t)(data_end - data_start))
      return AOM_CODEC_CORRUPT_FRAME;
    data_end = data_start + temporal_unit_size;

    // read the size of this frame unit
    if (aom_uleb_decode(data_start, (size_t)(data_end - data_start),
                        &frame_size, &length_of_size) != 0) {
      return AOM_CODEC_CORRUPT_FRAME;
    }
    data_start += length_of_size;
    if (frame_size > (size_t)(data_end - data_start))
      return AOM_CODEC_CORRUPT_FRAME;
  } else {
    frame_size = (uint64_t)(data_end - data_start);
  }

  if (ctx->frame_worker == NULL) {
    res = init_decoder(ctx);
    if (res != AOM_CODEC_OK) return res;
  }
  FrameWorkerData *const frame_worker_data =
      (FrameWorkerData *)ctx->frame_worker->data1;
  AV1Decoder *const pbi = frame_worker_data->pbi;
  AV1_COMMON *const cm = &pbi->common;
#if CONFIG_INSPECTION
  frame_worker_data->pbi->inspect_cb = ctx->inspect_cb;
  frame_worker_data->pbi->inspect_ctx = ctx->inspect_ctx;
#endif
  res = av1_receive_compressed_data(frame_worker_data->pbi, (size_t)frame_size,
                                    &data_start);
  check_resync(ctx, frame_worker_data->pbi);

  if (ctx->frame_worker->had_error)
    return update_error_state(ctx, &frame_worker_data->pbi->error);

  // Allow extra zero bytes after the frame end
  while (data_start < data_end) {
    const uint8_t marker = data_start[0];
    if (marker) break;
    ++data_start;
  }

  Av1DecodeReturn *data2 = (Av1DecodeReturn *)user_priv;
  data2->idx = -1;
  if (cm->cur_frame) {
    for (int i = 0; i < REF_FRAMES; ++i)
      if (cm->ref_frame_map[i] == cm->cur_frame) data2->idx = i;
  }
  data2->buf = data_start;
  data2->show_existing = cm->show_existing_frame;
  return res;
}

static aom_codec_err_t decoder_decode(aom_codec_alg_priv_t *ctx,
                                      const uint8_t *data, size_t data_sz,
                                      void *user_priv) {
  aom_codec_err_t res = AOM_CODEC_OK;

#if CONFIG_INSPECTION
  if (user_priv != 0) {
    return decoder_inspect(ctx, data, data_sz, user_priv);
  }
#endif

  release_pending_output_frames(ctx);

  /* Sanity checks */
  /* NULL data ptr allowed if data_sz is 0 too */
  if (data == NULL && data_sz == 0) {
    ctx->flushed = 1;
    return AOM_CODEC_OK;
  }
  if (data == NULL || data_sz == 0) return AOM_CODEC_INVALID_PARAM;

  // Reset flushed when receiving a valid frame.
  ctx->flushed = 0;

  // Initialize the decoder worker on the first frame.
  if (ctx->frame_worker == NULL) {
    res = init_decoder(ctx);
    if (res != AOM_CODEC_OK) return res;
  }

  const uint8_t *data_start = data;
  const uint8_t *data_end = data + data_sz;

  if (ctx->is_annexb) {
    // read the size of this temporal unit
    size_t length_of_size;
    uint64_t temporal_unit_size;
    if (aom_uleb_decode(data_start, data_sz, &temporal_unit_size,
                        &length_of_size) != 0) {
      return AOM_CODEC_CORRUPT_FRAME;
    }
    data_start += length_of_size;
    if (temporal_unit_size > (size_t)(data_end - data_start))
      return AOM_CODEC_CORRUPT_FRAME;
    data_end = data_start + temporal_unit_size;
  }

  // Decode in serial mode.
  while (data_start < data_end) {
    uint64_t frame_size;
    if (ctx->is_annexb) {
      // read the size of this frame unit
      size_t length_of_size;
      if (aom_uleb_decode(data_start, (size_t)(data_end - data_start),
                          &frame_size, &length_of_size) != 0) {
        return AOM_CODEC_CORRUPT_FRAME;
      }
      data_start += length_of_size;
      if (frame_size > (size_t)(data_end - data_start))
        return AOM_CODEC_CORRUPT_FRAME;
    } else {
      frame_size = (uint64_t)(data_end - data_start);
    }

    res = decode_one(ctx, &data_start, (size_t)frame_size, user_priv);
    if (res != AOM_CODEC_OK) return res;

    // Allow extra zero bytes after the frame end
    while (data_start < data_end) {
      const uint8_t marker = data_start[0];
      if (marker) break;
      ++data_start;
    }
  }

  return res;
}

typedef struct {
  BufferPool *pool;
  aom_codec_frame_buffer_t *fb;
} AllocCbParam;

static void *AllocWithGetFrameBufferCb(void *priv, size_t size) {
  AllocCbParam *param = (AllocCbParam *)priv;
  if (param->pool->get_fb_cb(param->pool->cb_priv, size, param->fb) < 0)
    return NULL;
  if (param->fb->data == NULL || param->fb->size < size) return NULL;
  return param->fb->data;
}

// If grain_params->apply_grain is false, returns img. Otherwise, adds film
// grain to img, saves the result in grain_img, and returns grain_img.
static aom_image_t *add_grain_if_needed(aom_codec_alg_priv_t *ctx,
                                        aom_image_t *img,
                                        aom_image_t *grain_img,
                                        aom_film_grain_t *grain_params) {
  if (!grain_params->apply_grain) return img;

  const int w_even = ALIGN_POWER_OF_TWO_UNSIGNED(img->d_w, 1);
  const int h_even = ALIGN_POWER_OF_TWO_UNSIGNED(img->d_h, 1);

  BufferPool *const pool = ctx->buffer_pool;
  aom_codec_frame_buffer_t *fb =
      &ctx->grain_image_frame_buffers[ctx->num_grain_image_frame_buffers];
  AllocCbParam param;
  param.pool = pool;
  param.fb = fb;
  if (!aom_img_alloc_with_cb(grain_img, img->fmt, w_even, h_even, 16,
                             AllocWithGetFrameBufferCb, &param)) {
    return NULL;
  }

  grain_img->user_priv = img->user_priv;
  grain_img->fb_priv = fb->priv;
  if (av1_add_film_grain(grain_params, img, grain_img)) {
    pool->release_fb_cb(pool->cb_priv, fb);
    return NULL;
  }

  ctx->num_grain_image_frame_buffers++;
  return grain_img;
}

// Copies and clears the metadata from AV1Decoder.
static void move_decoder_metadata_to_img(AV1Decoder *pbi, aom_image_t *img) {
  if (pbi->metadata && img) {
    assert(!img->metadata);
    img->metadata = pbi->metadata;
    pbi->metadata = NULL;
  }
}

static aom_image_t *decoder_get_frame(aom_codec_alg_priv_t *ctx,
                                      aom_codec_iter_t *iter) {
  aom_image_t *img = NULL;

  if (!iter) {
    return NULL;
  }

  // To avoid having to allocate any extra storage, treat 'iter' as
  // simply a pointer to an integer index
  uintptr_t *index = (uintptr_t *)iter;

  if (ctx->frame_worker == NULL) {
    return NULL;
  }
  const AVxWorkerInterface *const winterface = aom_get_worker_interface();
  AVxWorker *const worker = ctx->frame_worker;
  FrameWorkerData *const frame_worker_data = (FrameWorkerData *)worker->data1;
  AV1Decoder *const pbi = frame_worker_data->pbi;
  pbi->error.error_code = AOM_CODEC_OK;
  pbi->error.has_detail = 0;
  AV1_COMMON *const cm = &pbi->common;
  CommonTileParams *const tiles = &cm->tiles;
  // Wait for the frame from worker thread.
  if (!winterface->sync(worker)) {
    // Decoding failed. Release the worker thread.
    frame_worker_data->received_frame = 0;
    ctx->need_resync = 1;
    // TODO(aomedia:3519): Set an error code. Check if a different error code
    // should be used if ctx->flushed != 1.
    return NULL;
  }
  // Check if worker has received any frames.
  if (frame_worker_data->received_frame == 1) {
    frame_worker_data->received_frame = 0;
    check_resync(ctx, frame_worker_data->pbi);
  }
  YV12_BUFFER_CONFIG *sd;
  aom_film_grain_t *grain_params;
  if (av1_get_raw_frame(frame_worker_data->pbi, *index, &sd, &grain_params) !=
      0) {
    return NULL;
  }
  RefCntBuffer *const output_frame_buf = pbi->output_frames[*index];
  ctx->last_show_frame = output_frame_buf;
  if (ctx->need_resync) return NULL;
  aom_img_remove_metadata(&ctx->img);
  yuvconfig2image(&ctx->img, sd, frame_worker_data->user_priv);
  move_decoder_metadata_to_img(pbi, &ctx->img);

  if (!pbi->ext_tile_debug && tiles->large_scale) {
    *index += 1;  // Advance the iterator to point to the next image
    aom_img_remove_metadata(&ctx->img);
    yuvconfig2image(&ctx->img, &pbi->tile_list_outbuf, NULL);
    move_decoder_metadata_to_img(pbi, &ctx->img);
    img = &ctx->img;
    return img;
  }

  const int num_planes = av1_num_planes(cm);
  if (pbi->ext_tile_debug && tiles->single_tile_decoding &&
      pbi->dec_tile_row >= 0) {
    int tile_width, tile_height;
    if (!av1_get_uniform_tile_size(cm, &tile_width, &tile_height)) {
      return NULL;
    }
    const int tile_row = AOMMIN(pbi->dec_tile_row, tiles->rows - 1);
    const int mi_row = tile_row * tile_height;
    const int ssy = ctx->img.y_chroma_shift;
    int plane;
    ctx->img.planes[0] += mi_row * MI_SIZE * ctx->img.stride[0];
    if (num_planes > 1) {
      for (plane = 1; plane < MAX_MB_PLANE; ++plane) {
        ctx->img.planes[plane] +=
            mi_row * (MI_SIZE >> ssy) * ctx->img.stride[plane];
      }
    }
    ctx->img.d_h =
        AOMMIN(tile_height, cm->mi_params.mi_rows - mi_row) * MI_SIZE;
  }

  if (pbi->ext_tile_debug && tiles->single_tile_decoding &&
      pbi->dec_tile_col >= 0) {
    int tile_width, tile_height;
    if (!av1_get_uniform_tile_size(cm, &tile_width, &tile_height)) {
      return NULL;
    }
    const int tile_col = AOMMIN(pbi->dec_tile_col, tiles->cols - 1);
    const int mi_col = tile_col * tile_width;
    const int ssx = ctx->img.x_chroma_shift;
    const int is_hbd = (ctx->img.fmt & AOM_IMG_FMT_HIGHBITDEPTH) ? 1 : 0;
    int plane;
    ctx->img.planes[0] += mi_col * MI_SIZE * (1 + is_hbd);
    if (num_planes > 1) {
      for (plane = 1; plane < MAX_MB_PLANE; ++plane) {
        ctx->img.planes[plane] += mi_col * (MI_SIZE >> ssx) * (1 + is_hbd);
      }
    }
    ctx->img.d_w = AOMMIN(tile_width, cm->mi_params.mi_cols - mi_col) * MI_SIZE;
  }

  ctx->img.fb_priv = output_frame_buf->raw_frame_buffer.priv;
  img = &ctx->img;
  img->temporal_id = output_frame_buf->temporal_id;
  img->spatial_id = output_frame_buf->spatial_id;
  if (pbi->skip_film_grain) grain_params->apply_grain = 0;
  aom_image_t *res =
      add_grain_if_needed(ctx, img, &ctx->image_with_grain, grain_params);
  if (!res) {
    pbi->error.error_code = AOM_CODEC_CORRUPT_FRAME;
    pbi->error.has_detail = 1;
    snprintf(pbi->error.detail, sizeof(pbi->error.detail),
             "Grain synthesis failed\n");
    return res;
  }
  *index += 1;  // Advance the iterator to point to the next image
  return res;
}

static aom_codec_err_t decoder_set_fb_fn(
    aom_codec_alg_priv_t *ctx, aom_get_frame_buffer_cb_fn_t cb_get,
    aom_release_frame_buffer_cb_fn_t cb_release, void *cb_priv) {
  if (cb_get == NULL || cb_release == NULL) {
    return AOM_CODEC_INVALID_PARAM;
  }
  if (ctx->frame_worker != NULL) {
    // If the decoder has already been initialized, do not accept changes to
    // the frame buffer functions.
    return AOM_CODEC_ERROR;
  }

  ctx->get_ext_fb_cb = cb_get;
  ctx->release_ext_fb_cb = cb_release;
  ctx->ext_priv = cb_priv;
  return AOM_CODEC_OK;
}

static aom_codec_err_t ctrl_set_reference(aom_codec_alg_priv_t *ctx,
                                          va_list args) {
  av1_ref_frame_t *const data = va_arg(args, av1_ref_frame_t *);

  if (data) {
    av1_ref_frame_t *const frame = data;
    YV12_BUFFER_CONFIG sd;
    AVxWorker *const worker = ctx->frame_worker;
    FrameWorkerData *const frame_worker_data = (FrameWorkerData *)worker->data1;
    image2yuvconfig(&frame->img, &sd);
    return av1_set_reference_dec(&frame_worker_data->pbi->common, frame->idx,
                                 frame->use_external_ref, &sd);
  } else {
    return AOM_CODEC_INVALID_PARAM;
  }
}

static aom_codec_err_t ctrl_copy_reference(aom_codec_alg_priv_t *ctx,
                                           va_list args) {
  const av1_ref_frame_t *const frame = va_arg(args, av1_ref_frame_t *);
  if (frame) {
    YV12_BUFFER_CONFIG sd;
    AVxWorker *const worker = ctx->frame_worker;
    FrameWorkerData *const frame_worker_data = (FrameWorkerData *)worker->data1;
    image2yuvconfig(&frame->img, &sd);
    return av1_copy_reference_dec(frame_worker_data->pbi, frame->idx, &sd);
  } else {
    return AOM_CODEC_INVALID_PARAM;
  }
}

static aom_codec_err_t ctrl_get_reference(aom_codec_alg_priv_t *ctx,
                                          va_list args) {
  av1_ref_frame_t *data = va_arg(args, av1_ref_frame_t *);
  if (data) {
    YV12_BUFFER_CONFIG *fb;
    AVxWorker *const worker = ctx->frame_worker;
    FrameWorkerData *const frame_worker_data = (FrameWorkerData *)worker->data1;
    fb = get_ref_frame(&frame_worker_data->pbi->common, data->idx);
    if (fb == NULL) return AOM_CODEC_ERROR;
    yuvconfig2image(&data->img, fb, NULL);
    return AOM_CODEC_OK;
  } else {
    return AOM_CODEC_INVALID_PARAM;
  }
}

static aom_codec_err_t ctrl_get_new_frame_image(aom_codec_alg_priv_t *ctx,
                                                va_list args) {
  aom_image_t *new_img = va_arg(args, aom_image_t *);
  if (new_img) {
    YV12_BUFFER_CONFIG new_frame;
    AVxWorker *const worker = ctx->frame_worker;
    FrameWorkerData *const frame_worker_data = (FrameWorkerData *)worker->data1;

    if (av1_get_frame_to_show(frame_worker_data->pbi, &new_frame) == 0) {
      yuvconfig2image(new_img, &new_frame, NULL);
      return AOM_CODEC_OK;
    } else {
      return AOM_CODEC_ERROR;
    }
  } else {
    return AOM_CODEC_INVALID_PARAM;
  }
}

static aom_codec_err_t ctrl_copy_new_frame_image(aom_codec_alg_priv_t *ctx,
                                                 va_list args) {
  aom_image_t *img = va_arg(args, aom_image_t *);
  if (img) {
    YV12_BUFFER_CONFIG new_frame;
    AVxWorker *const worker = ctx->frame_worker;
    FrameWorkerData *const frame_worker_data = (FrameWorkerData *)worker->data1;

    if (av1_get_frame_to_show(frame_worker_data->pbi, &new_frame) == 0) {
      YV12_BUFFER_CONFIG sd;
      image2yuvconfig(img, &sd);
      return av1_copy_new_frame_dec(&frame_worker_data->pbi->common, &new_frame,
                                    &sd);
    } else {
      return AOM_CODEC_ERROR;
    }
  } else {
    return AOM_CODEC_INVALID_PARAM;
  }
}

static aom_codec_err_t ctrl_get_last_ref_updates(aom_codec_alg_priv_t *ctx,
                                                 va_list args) {
  int *const update_info = va_arg(args, int *);

  if (update_info) {
    if (ctx->frame_worker) {
      AVxWorker *const worker = ctx->frame_worker;
      FrameWorkerData *const frame_worker_data =
          (FrameWorkerData *)worker->data1;
      *update_info =
          frame_worker_data->pbi->common.current_frame.refresh_frame_flags;
      return AOM_CODEC_OK;
    } else {
      return AOM_CODEC_ERROR;
    }
  }

  return AOM_CODEC_INVALID_PARAM;
}

static aom_codec_err_t ctrl_get_last_quantizer(aom_codec_alg_priv_t *ctx,
                                               va_list args) {
  int *const arg = va_arg(args, int *);
  if (arg == NULL) return AOM_CODEC_INVALID_PARAM;
  if (ctx->frame_worker == NULL) return AOM_CODEC_ERROR;
  *arg = ((FrameWorkerData *)ctx->frame_worker->data1)
             ->pbi->common.quant_params.base_qindex;
  return AOM_CODEC_OK;
}

static aom_codec_err_t ctrl_get_fwd_kf_value(aom_codec_alg_priv_t *ctx,
                                             va_list args) {
  int *const arg = va_arg(args, int *);
  if (arg == NULL) return AOM_CODEC_INVALID_PARAM;
  if (ctx->frame_worker == NULL) return AOM_CODEC_ERROR;
  *arg = ((FrameWorkerData *)ctx->frame_worker->data1)->pbi->is_fwd_kf_present;
  return AOM_CODEC_OK;
}

static aom_codec_err_t ctrl_get_altref_present(aom_codec_alg_priv_t *ctx,
                                               va_list args) {
  int *const arg = va_arg(args, int *);
  if (arg == NULL) return AOM_CODEC_INVALID_PARAM;
  if (ctx->frame_worker == NULL) return AOM_CODEC_ERROR;
  *arg =
      ((FrameWorkerData *)ctx->frame_worker->data1)->pbi->is_arf_frame_present;
  return AOM_CODEC_OK;
}

static aom_codec_err_t ctrl_get_frame_flags(aom_codec_alg_priv_t *ctx,
                                            va_list args) {
  int *const arg = va_arg(args, int *);
  if (arg == NULL) return AOM_CODEC_INVALID_PARAM;
  if (ctx->frame_worker == NULL) return AOM_CODEC_ERROR;
  AV1Decoder *pbi = ((FrameWorkerData *)ctx->frame_worker->data1)->pbi;
  *arg = 0;
  switch (pbi->common.current_frame.frame_type) {
    case KEY_FRAME:
      *arg |= AOM_FRAME_IS_KEY;
      *arg |= AOM_FRAME_IS_INTRAONLY;
      if (!pbi->common.show_frame) {
        *arg |= AOM_FRAME_IS_DELAYED_RANDOM_ACCESS_POINT;
      }
      break;
    case INTRA_ONLY_FRAME: *arg |= AOM_FRAME_IS_INTRAONLY; break;
    case S_FRAME: *arg |= AOM_FRAME_IS_SWITCH; break;
  }
  if (pbi->common.features.error_resilient_mode) {
    *arg |= AOM_FRAME_IS_ERROR_RESILIENT;
  }
  return AOM_CODEC_OK;
}

static aom_codec_err_t ctrl_get_tile_info(aom_codec_alg_priv_t *ctx,
                                          va_list args) {
  aom_tile_info *const tile_info = va_arg(args, aom_tile_info *);

  if (tile_info) {
    if (ctx->frame_worker) {
      AVxWorker *const worker = ctx->frame_worker;
      FrameWorkerData *const frame_worker_data =
          (FrameWorkerData *)worker->data1;
      const AV1Decoder *pbi = frame_worker_data->pbi;
      const CommonTileParams *tiles = &pbi->common.tiles;

      int tile_rows = tiles->rows;
      int tile_cols = tiles->cols;

      if (tiles->uniform_spacing) {
        tile_info->tile_rows = 1 << tiles->log2_rows;
        tile_info->tile_columns = 1 << tiles->log2_cols;
      } else {
        tile_info->tile_rows = tile_rows;
        tile_info->tile_columns = tile_cols;
      }

      for (int tile_col = 1; tile_col <= tile_cols; tile_col++) {
        tile_info->tile_widths[tile_col - 1] =
            tiles->col_start_sb[tile_col] - tiles->col_start_sb[tile_col - 1];
      }

      for (int tile_row = 1; tile_row <= tile_rows; tile_row++) {
        tile_info->tile_heights[tile_row - 1] =
            tiles->row_start_sb[tile_row] - tiles->row_start_sb[tile_row - 1];
      }
      tile_info->num_tile_groups = pbi->num_tile_groups;
      return AOM_CODEC_OK;
    } else {
      return AOM_CODEC_ERROR;
    }
  }

  return AOM_CODEC_INVALID_PARAM;
}

static aom_codec_err_t ctrl_get_screen_content_tools_info(
    aom_codec_alg_priv_t *ctx, va_list args) {
  aom_screen_content_tools_info *const sc_info =
      va_arg(args, aom_screen_content_tools_info *);
  if (sc_info) {
    if (ctx->frame_worker) {
      AVxWorker *const worker = ctx->frame_worker;
      FrameWorkerData *const frame_worker_data =
          (FrameWorkerData *)worker->data1;
      const AV1Decoder *pbi = frame_worker_data->pbi;
      sc_info->allow_screen_content_tools =
          pbi->common.features.allow_screen_content_tools;
      sc_info->allow_intrabc = pbi->common.features.allow_intrabc;
      sc_info->force_integer_mv =
          (int)pbi->common.features.cur_frame_force_integer_mv;
      return AOM_CODEC_OK;
    } else {
      return AOM_CODEC_ERROR;
    }
  }
  return AOM_CODEC_INVALID_PARAM;
}

static aom_codec_err_t ctrl_get_still_picture(aom_codec_alg_priv_t *ctx,
                                              va_list args) {
  aom_still_picture_info *const still_picture_info =
      va_arg(args, aom_still_picture_info *);
  if (still_picture_info) {
    if (ctx->frame_worker) {
      AVxWorker *const worker = ctx->frame_worker;
      FrameWorkerData *const frame_worker_data =
          (FrameWorkerData *)worker->data1;
      const AV1Decoder *pbi = frame_worker_data->pbi;
      still_picture_info->is_still_picture = (int)pbi->seq_params.still_picture;
      still_picture_info->is_reduced_still_picture_hdr =
          (int)(pbi->seq_params.reduced_still_picture_hdr);
      return AOM_CODEC_OK;
    } else {
      return AOM_CODEC_ERROR;
    }
  }
  return AOM_CODEC_INVALID_PARAM;
}

static aom_codec_err_t ctrl_get_sb_size(aom_codec_alg_priv_t *ctx,
                                        va_list args) {
  aom_superblock_size_t *const sb_size = va_arg(args, aom_superblock_size_t *);
  if (sb_size) {
    if (ctx->frame_worker) {
      AVxWorker *const worker = ctx->frame_worker;
      FrameWorkerData *const frame_worker_data =
          (FrameWorkerData *)worker->data1;
      const AV1Decoder *pbi = frame_worker_data->pbi;
      if (pbi->seq_params.sb_size == BLOCK_128X128) {
        *sb_size = AOM_SUPERBLOCK_SIZE_128X128;
      } else {
        *sb_size = AOM_SUPERBLOCK_SIZE_64X64;
      }
      return AOM_CODEC_OK;
    } else {
      return AOM_CODEC_ERROR;
    }
  }
  return AOM_CODEC_INVALID_PARAM;
}

static aom_codec_err_t ctrl_get_show_existing_frame_flag(
    aom_codec_alg_priv_t *ctx, va_list args) {
  int *const arg = va_arg(args, int *);
  if (arg == NULL) return AOM_CODEC_INVALID_PARAM;
  if (ctx->frame_worker == NULL) return AOM_CODEC_ERROR;
  *arg = ((FrameWorkerData *)ctx->frame_worker->data1)
             ->pbi->common.show_existing_frame;
  return AOM_CODEC_OK;
}

static aom_codec_err_t ctrl_get_s_frame_info(aom_codec_alg_priv_t *ctx,
                                             va_list args) {
  aom_s_frame_info *const s_frame_info = va_arg(args, aom_s_frame_info *);
  if (s_frame_info) {
    if (ctx->frame_worker) {
      AVxWorker *const worker = ctx->frame_worker;
      FrameWorkerData *const frame_worker_data =
          (FrameWorkerData *)worker->data1;
      const AV1Decoder *pbi = frame_worker_data->pbi;
      s_frame_info->is_s_frame = pbi->sframe_info.is_s_frame;
      s_frame_info->is_s_frame_at_altref =
          pbi->sframe_info.is_s_frame_at_altref;
      return AOM_CODEC_OK;
    } else {
      return AOM_CODEC_ERROR;
    }
  }
  return AOM_CODEC_INVALID_PARAM;
}

static aom_codec_err_t ctrl_get_frame_corrupted(aom_codec_alg_priv_t *ctx,
                                                va_list args) {
  int *corrupted = va_arg(args, int *);

  if (corrupted) {
    if (ctx->frame_worker) {
      AVxWorker *const worker = ctx->frame_worker;
      FrameWorkerData *const frame_worker_data =
          (FrameWorkerData *)worker->data1;
      AV1Decoder *const pbi = frame_worker_data->pbi;
      if (pbi->seen_frame_header && pbi->num_output_frames == 0)
        return AOM_CODEC_ERROR;
      if (ctx->last_show_frame != NULL)
        *corrupted = ctx->last_show_frame->buf.corrupted;
      return AOM_CODEC_OK;
    } else {
      return AOM_CODEC_ERROR;
    }
  }

  return AOM_CODEC_INVALID_PARAM;
}

static aom_codec_err_t ctrl_get_frame_size(aom_codec_alg_priv_t *ctx,
                                           va_list args) {
  int *const frame_size = va_arg(args, int *);

  if (frame_size) {
    if (ctx->frame_worker) {
      AVxWorker *const worker = ctx->frame_worker;
      FrameWorkerData *const frame_worker_data =
          (FrameWorkerData *)worker->data1;
      const AV1_COMMON *const cm = &frame_worker_data->pbi->common;
      frame_size[0] = cm->width;
      frame_size[1] = cm->height;
      return AOM_CODEC_OK;
    } else {
      return AOM_CODEC_ERROR;
    }
  }

  return AOM_CODEC_INVALID_PARAM;
}

static aom_codec_err_t ctrl_get_frame_header_info(aom_codec_alg_priv_t *ctx,
                                                  va_list args) {
  aom_tile_data *const frame_header_info = va_arg(args, aom_tile_data *);

  if (frame_header_info) {
    if (ctx->frame_worker) {
      AVxWorker *const worker = ctx->frame_worker;
      FrameWorkerData *const frame_worker_data =
          (FrameWorkerData *)worker->data1;
      const AV1Decoder *pbi = frame_worker_data->pbi;
      frame_header_info->coded_tile_data_size = pbi->obu_size_hdr.size;
      frame_header_info->coded_tile_data = pbi->obu_size_hdr.data;
      frame_header_info->extra_size = pbi->frame_header_size;
      return AOM_CODEC_OK;
    } else {
      return AOM_CODEC_ERROR;
    }
  }

  return AOM_CODEC_INVALID_PARAM;
}

static aom_codec_err_t ctrl_get_tile_data(aom_codec_alg_priv_t *ctx,
                                          va_list args) {
  aom_tile_data *const tile_data = va_arg(args, aom_tile_data *);

  if (tile_data) {
    if (ctx->frame_worker) {
      AVxWorker *const worker = ctx->frame_worker;
      FrameWorkerData *const frame_worker_data =
          (FrameWorkerData *)worker->data1;
      const AV1Decoder *pbi = frame_worker_data->pbi;
      tile_data->coded_tile_data_size =
          pbi->tile_buffers[pbi->dec_tile_row][pbi->dec_tile_col].size;
      tile_data->coded_tile_data =
          pbi->tile_buffers[pbi->dec_tile_row][pbi->dec_tile_col].data;
      return AOM_CODEC_OK;
    } else {
      return AOM_CODEC_ERROR;
    }
  }

  return AOM_CODEC_INVALID_PARAM;
}

static aom_codec_err_t ctrl_set_ext_ref_ptr(aom_codec_alg_priv_t *ctx,
                                            va_list args) {
  av1_ext_ref_frame_t *const data = va_arg(args, av1_ext_ref_frame_t *);

  if (data) {
    av1_ext_ref_frame_t *const ext_frames = data;
    ctx->ext_refs.num = ext_frames->num;
    for (int i = 0; i < ctx->ext_refs.num; i++) {
      image2yuvconfig(ext_frames->img++, &ctx->ext_refs.refs[i]);
    }
    return AOM_CODEC_OK;
  } else {
    return AOM_CODEC_INVALID_PARAM;
  }
}

static aom_codec_err_t ctrl_get_render_size(aom_codec_alg_priv_t *ctx,
                                            va_list args) {
  int *const render_size = va_arg(args, int *);

  if (render_size) {
    if (ctx->frame_worker) {
      AVxWorker *const worker = ctx->frame_worker;
      FrameWorkerData *const frame_worker_data =
          (FrameWorkerData *)worker->data1;
      const AV1_COMMON *const cm = &frame_worker_data->pbi->common;
      render_size[0] = cm->render_width;
      render_size[1] = cm->render_height;
      return AOM_CODEC_OK;
    } else {
      return AOM_CODEC_ERROR;
    }
  }

  return AOM_CODEC_INVALID_PARAM;
}

static aom_codec_err_t ctrl_get_bit_depth(aom_codec_alg_priv_t *ctx,
                                          va_list args) {
  unsigned int *const bit_depth = va_arg(args, unsigned int *);
  AVxWorker *const worker = ctx->frame_worker;

  if (bit_depth) {
    if (worker) {
      FrameWorkerData *const frame_worker_data =
          (FrameWorkerData *)worker->data1;
      const AV1_COMMON *const cm = &frame_worker_data->pbi->common;
      *bit_depth = cm->seq_params->bit_depth;
      return AOM_CODEC_OK;
    } else {
      return AOM_CODEC_ERROR;
    }
  }

  return AOM_CODEC_INVALID_PARAM;
}

static aom_img_fmt_t get_img_format(int subsampling_x, int subsampling_y,
                                    int use_highbitdepth) {
  aom_img_fmt_t fmt = 0;

  if (subsampling_x == 0 && subsampling_y == 0)
    fmt = AOM_IMG_FMT_I444;
  else if (subsampling_x == 1 && subsampling_y == 0)
    fmt = AOM_IMG_FMT_I422;
  else if (subsampling_x == 1 && subsampling_y == 1)
    fmt = AOM_IMG_FMT_I420;

  if (use_highbitdepth) fmt |= AOM_IMG_FMT_HIGHBITDEPTH;
  return fmt;
}

static aom_codec_err_t ctrl_get_img_format(aom_codec_alg_priv_t *ctx,
                                           va_list args) {
  aom_img_fmt_t *const img_fmt = va_arg(args, aom_img_fmt_t *);
  AVxWorker *const worker = ctx->frame_worker;

  if (img_fmt) {
    if (worker) {
      FrameWorkerData *const frame_worker_data =
          (FrameWorkerData *)worker->data1;
      const AV1_COMMON *const cm = &frame_worker_data->pbi->common;

      *img_fmt = get_img_format(cm->seq_params->subsampling_x,
                                cm->seq_params->subsampling_y,
                                cm->seq_params->use_highbitdepth);
      return AOM_CODEC_OK;
    } else {
      return AOM_CODEC_ERROR;
    }
  }

  return AOM_CODEC_INVALID_PARAM;
}

static aom_codec_err_t ctrl_get_tile_size(aom_codec_alg_priv_t *ctx,
                                          va_list args) {
  unsigned int *const tile_size = va_arg(args, unsigned int *);
  AVxWorker *const worker = ctx->frame_worker;

  if (tile_size) {
    if (worker) {
      FrameWorkerData *const frame_worker_data =
          (FrameWorkerData *)worker->data1;
      const AV1_COMMON *const cm = &frame_worker_data->pbi->common;
      int tile_width, tile_height;
      if (!av1_get_uniform_tile_size(cm, &tile_width, &tile_height)) {
        return AOM_CODEC_CORRUPT_FRAME;
      }
      *tile_size = ((tile_width * MI_SIZE) << 16) + tile_height * MI_SIZE;
      return AOM_CODEC_OK;
    } else {
      return AOM_CODEC_ERROR;
    }
  }
  return AOM_CODEC_INVALID_PARAM;
}

static aom_codec_err_t ctrl_get_tile_count(aom_codec_alg_priv_t *ctx,
                                           va_list args) {
  unsigned int *const tile_count = va_arg(args, unsigned int *);

  if (tile_count) {
    AVxWorker *const worker = ctx->frame_worker;
    if (worker) {
      FrameWorkerData *const frame_worker_data =
          (FrameWorkerData *)worker->data1;
      *tile_count = frame_worker_data->pbi->tile_count_minus_1 + 1;
      return AOM_CODEC_OK;
    } else {
      return AOM_CODEC_ERROR;
    }
  }
  return AOM_CODEC_INVALID_PARAM;
}

static aom_codec_err_t ctrl_get_base_q_idx(aom_codec_alg_priv_t *ctx,
                                           va_list args) {
  int *const arg = va_arg(args, int *);
  if (arg == NULL) return AOM_CODEC_INVALID_PARAM;
  if (ctx->frame_worker == NULL) return AOM_CODEC_ERROR;
  FrameWorkerData *const frame_worker_data =
      (FrameWorkerData *)ctx->frame_worker->data1;
  *arg = frame_worker_data->pbi->common.quant_params.base_qindex;
  return AOM_CODEC_OK;
}

static aom_codec_err_t ctrl_get_show_frame_flag(aom_codec_alg_priv_t *ctx,
                                                va_list args) {
  int *const arg = va_arg(args, int *);
  if (arg == NULL) return AOM_CODEC_INVALID_PARAM;
  if (ctx->frame_worker == NULL) return AOM_CODEC_ERROR;
  FrameWorkerData *const frame_worker_data =
      (FrameWorkerData *)ctx->frame_worker->data1;
  *arg = frame_worker_data->pbi->common.show_frame;
  return AOM_CODEC_OK;
}

static aom_codec_err_t ctrl_get_order_hint(aom_codec_alg_priv_t *ctx,
                                           va_list args) {
  unsigned int *const arg = va_arg(args, unsigned int *);
  if (arg == NULL) return AOM_CODEC_INVALID_PARAM;
  if (ctx->frame_worker == NULL) return AOM_CODEC_ERROR;
  FrameWorkerData *const frame_worker_data =
      (FrameWorkerData *)ctx->frame_worker->data1;
  *arg = frame_worker_data->pbi->common.current_frame.order_hint;
  return AOM_CODEC_OK;
}

static aom_codec_err_t ctrl_get_mi_info(aom_codec_alg_priv_t *ctx,
                                        va_list args) {
  int mi_row = va_arg(args, int);
  int mi_col = va_arg(args, int);
  MB_MODE_INFO *mi = va_arg(args, MB_MODE_INFO *);
  if (mi == NULL) return AOM_CODEC_INVALID_PARAM;
  if (ctx->frame_worker == NULL) return AOM_CODEC_ERROR;
  FrameWorkerData *const frame_worker_data =
      (FrameWorkerData *)ctx->frame_worker->data1;
  if (frame_worker_data == NULL) return AOM_CODEC_ERROR;

  AV1_COMMON *cm = &frame_worker_data->pbi->common;
  const int mi_rows = cm->mi_params.mi_rows;
  const int mi_cols = cm->mi_params.mi_cols;
  const int mi_stride = cm->mi_params.mi_stride;
  const int offset = mi_row * mi_stride + mi_col;

  if (mi_row < 0 || mi_row >= mi_rows || mi_col < 0 || mi_col >= mi_cols) {
    return AOM_CODEC_INVALID_PARAM;
  }

  memcpy(mi, cm->mi_params.mi_grid_base[offset], sizeof(*mi));

  return AOM_CODEC_OK;
}

static aom_codec_err_t ctrl_set_invert_tile_order(aom_codec_alg_priv_t *ctx,
                                                  va_list args) {
  ctx->invert_tile_order = va_arg(args, int);
  return AOM_CODEC_OK;
}

static aom_codec_err_t ctrl_set_byte_alignment(aom_codec_alg_priv_t *ctx,
                                               va_list args) {
  const int legacy_byte_alignment = 0;
  const int min_byte_alignment = 32;
  const int max_byte_alignment = 1024;
  const int byte_alignment = va_arg(args, int);

  if (byte_alignment != legacy_byte_alignment &&
      (byte_alignment < min_byte_alignment ||
       byte_alignment > max_byte_alignment ||
       (byte_alignment & (byte_alignment - 1)) != 0))
    return AOM_CODEC_INVALID_PARAM;

  ctx->byte_alignment = byte_alignment;
  if (ctx->frame_worker) {
    AVxWorker *const worker = ctx->frame_worker;
    FrameWorkerData *const frame_worker_data = (FrameWorkerData *)worker->data1;
    frame_worker_data->pbi->common.features.byte_alignment = byte_alignment;
  }
  return AOM_CODEC_OK;
}

static aom_codec_err_t ctrl_set_skip_loop_filter(aom_codec_alg_priv_t *ctx,
                                                 va_list args) {
  ctx->skip_loop_filter = va_arg(args, int);

  if (ctx->frame_worker) {
    AVxWorker *const worker = ctx->frame_worker;
    FrameWorkerData *const frame_worker_data = (FrameWorkerData *)worker->data1;
    frame_worker_data->pbi->skip_loop_filter = ctx->skip_loop_filter;
  }

  return AOM_CODEC_OK;
}

static aom_codec_err_t ctrl_set_skip_film_grain(aom_codec_alg_priv_t *ctx,
                                                va_list args) {
  ctx->skip_film_grain = va_arg(args, int);

  if (ctx->frame_worker) {
    AVxWorker *const worker = ctx->frame_worker;
    FrameWorkerData *const frame_worker_data = (FrameWorkerData *)worker->data1;
    frame_worker_data->pbi->skip_film_grain = ctx->skip_film_grain;
  }

  return AOM_CODEC_OK;
}

static aom_codec_err_t ctrl_get_accounting(aom_codec_alg_priv_t *ctx,
                                           va_list args) {
#if !CONFIG_ACCOUNTING
  (void)ctx;
  (void)args;
  return AOM_CODEC_INCAPABLE;
#else
  Accounting **acct = va_arg(args, Accounting **);

  if (acct) {
    if (ctx->frame_worker) {
      AVxWorker *const worker = ctx->frame_worker;
      FrameWorkerData *const frame_worker_data =
          (FrameWorkerData *)worker->data1;
      AV1Decoder *pbi = frame_worker_data->pbi;
      *acct = &pbi->accounting;
      return AOM_CODEC_OK;
    } else {
      return AOM_CODEC_ERROR;
    }
  }

  return AOM_CODEC_INVALID_PARAM;
#endif
}

static aom_codec_err_t ctrl_set_decode_tile_row(aom_codec_alg_priv_t *ctx,
                                                va_list args) {
  ctx->decode_tile_row = va_arg(args, int);
  return AOM_CODEC_OK;
}

static aom_codec_err_t ctrl_set_decode_tile_col(aom_codec_alg_priv_t *ctx,
                                                va_list args) {
  ctx->decode_tile_col = va_arg(args, int);
  return AOM_CODEC_OK;
}

static aom_codec_err_t ctrl_set_tile_mode(aom_codec_alg_priv_t *ctx,
                                          va_list args) {
  ctx->tile_mode = va_arg(args, unsigned int);
  return AOM_CODEC_OK;
}

static aom_codec_err_t ctrl_set_is_annexb(aom_codec_alg_priv_t *ctx,
                                          va_list args) {
  ctx->is_annexb = va_arg(args, unsigned int);
  return AOM_CODEC_OK;
}

static aom_codec_err_t ctrl_set_operating_point(aom_codec_alg_priv_t *ctx,
                                                va_list args) {
  ctx->operating_point = va_arg(args, int);
  return AOM_CODEC_OK;
}

static aom_codec_err_t ctrl_set_output_all_layers(aom_codec_alg_priv_t *ctx,
                                                  va_list args) {
  ctx->output_all_layers = va_arg(args, int);
  return AOM_CODEC_OK;
}

static aom_codec_err_t ctrl_set_inspection_callback(aom_codec_alg_priv_t *ctx,
                                                    va_list args) {
#if !CONFIG_INSPECTION
  (void)ctx;
  (void)args;
  return AOM_CODEC_INCAPABLE;
#else
  aom_inspect_init *init = va_arg(args, aom_inspect_init *);
  ctx->inspect_cb = init->inspect_cb;
  ctx->inspect_ctx = init->inspect_ctx;
  return AOM_CODEC_OK;
#endif
}

static aom_codec_err_t ctrl_ext_tile_debug(aom_codec_alg_priv_t *ctx,
                                           va_list args) {
  ctx->ext_tile_debug = va_arg(args, int);
  return AOM_CODEC_OK;
}

static aom_codec_err_t ctrl_set_row_mt(aom_codec_alg_priv_t *ctx,
                                       va_list args) {
  ctx->row_mt = va_arg(args, unsigned int);
  return AOM_CODEC_OK;
}

static aom_codec_ctrl_fn_map_t decoder_ctrl_maps[] = {
  { AV1_COPY_REFERENCE, ctrl_copy_reference },

  // Setters
  { AV1_SET_REFERENCE, ctrl_set_reference },
  { AV1_INVERT_TILE_DECODE_ORDER, ctrl_set_invert_tile_order },
  { AV1_SET_BYTE_ALIGNMENT, ctrl_set_byte_alignment },
  { AV1_SET_SKIP_LOOP_FILTER, ctrl_set_skip_loop_filter },
  { AV1_SET_DECODE_TILE_ROW, ctrl_set_decode_tile_row },
  { AV1_SET_DECODE_TILE_COL, ctrl_set_decode_tile_col },
  { AV1_SET_TILE_MODE, ctrl_set_tile_mode },
  { AV1D_SET_IS_ANNEXB, ctrl_set_is_annexb },
  { AV1D_SET_OPERATING_POINT, ctrl_set_operating_point },
  { AV1D_SET_OUTPUT_ALL_LAYERS, ctrl_set_output_all_layers },
  { AV1_SET_INSPECTION_CALLBACK, ctrl_set_inspection_callback },
  { AV1D_EXT_TILE_DEBUG, ctrl_ext_tile_debug },
  { AV1D_SET_ROW_MT, ctrl_set_row_mt },
  { AV1D_SET_EXT_REF_PTR, ctrl_set_ext_ref_ptr },
  { AV1D_SET_SKIP_FILM_GRAIN, ctrl_set_skip_film_grain },

  // Getters
  { AOMD_GET_FRAME_CORRUPTED, ctrl_get_frame_corrupted },
  { AOMD_GET_LAST_QUANTIZER, ctrl_get_last_quantizer },
  { AOMD_GET_LAST_REF_UPDATES, ctrl_get_last_ref_updates },
  { AV1D_GET_BIT_DEPTH, ctrl_get_bit_depth },
  { AV1D_GET_IMG_FORMAT, ctrl_get_img_format },
  { AV1D_GET_TILE_SIZE, ctrl_get_tile_size },
  { AV1D_GET_TILE_COUNT, ctrl_get_tile_count },
  { AV1D_GET_DISPLAY_SIZE, ctrl_get_render_size },
  { AV1D_GET_FRAME_SIZE, ctrl_get_frame_size },
  { AV1_GET_ACCOUNTING, ctrl_get_accounting },
  { AV1_GET_NEW_FRAME_IMAGE, ctrl_get_new_frame_image },
  { AV1_COPY_NEW_FRAME_IMAGE, ctrl_copy_new_frame_image },
  { AV1_GET_REFERENCE, ctrl_get_reference },
  { AV1D_GET_FRAME_HEADER_INFO, ctrl_get_frame_header_info },
  { AV1D_GET_TILE_DATA, ctrl_get_tile_data },
  { AOMD_GET_FWD_KF_PRESENT, ctrl_get_fwd_kf_value },
  { AOMD_GET_ALTREF_PRESENT, ctrl_get_altref_present },
  { AOMD_GET_FRAME_FLAGS, ctrl_get_frame_flags },
  { AOMD_GET_TILE_INFO, ctrl_get_tile_info },
  { AOMD_GET_SCREEN_CONTENT_TOOLS_INFO, ctrl_get_screen_content_tools_info },
  { AOMD_GET_STILL_PICTURE, ctrl_get_still_picture },
  { AOMD_GET_SB_SIZE, ctrl_get_sb_size },
  { AOMD_GET_SHOW_EXISTING_FRAME_FLAG, ctrl_get_show_existing_frame_flag },
  { AOMD_GET_S_FRAME_INFO, ctrl_get_s_frame_info },
  { AOMD_GET_SHOW_FRAME_FLAG, ctrl_get_show_frame_flag },
  { AOMD_GET_BASE_Q_IDX, ctrl_get_base_q_idx },
  { AOMD_GET_ORDER_HINT, ctrl_get_order_hint },
  { AV1D_GET_MI_INFO, ctrl_get_mi_info },
  CTRL_MAP_END,
};

// This data structure and function are exported in aom/aomdx.h
#ifndef VERSION_STRING
#define VERSION_STRING
#endif
aom_codec_iface_t aom_codec_av1_dx_algo = {
  "AOMedia Project AV1 Decoder" VERSION_STRING,
  AOM_CODEC_INTERNAL_ABI_VERSION,
  AOM_CODEC_CAP_DECODER |
      AOM_CODEC_CAP_EXTERNAL_FRAME_BUFFER,  // aom_codec_caps_t
  decoder_init,                             // aom_codec_init_fn_t
  decoder_destroy,                          // aom_codec_destroy_fn_t
  decoder_ctrl_maps,                        // aom_codec_ctrl_fn_map_t
  {
      // NOLINT
      decoder_peek_si,    // aom_codec_peek_si_fn_t
      decoder_get_si,     // aom_codec_get_si_fn_t
      decoder_decode,     // aom_codec_decode_fn_t
      decoder_get_frame,  // aom_codec_get_frame_fn_t
      decoder_set_fb_fn,  // aom_codec_set_fb_fn_t
  },
  {
      // NOLINT
      0,
      NULL,  // aom_codec_enc_cfg_t
      NULL,  // aom_codec_encode_fn_t
      NULL,  // aom_codec_get_cx_data_fn_t
      NULL,  // aom_codec_enc_config_set_fn_t
      NULL,  // aom_codec_get_global_headers_fn_t
      NULL   // aom_codec_get_preview_frame_fn_t
  },
  NULL  // aom_codec_set_option_fn_t
};

// Decoder interface for inspecting frame data. It uses decoder_inspect instead
// of decoder_decode so it only decodes one frame at a time, whether the frame
// is shown or not.
aom_codec_iface_t aom_codec_av1_inspect_algo = {
  "AOMedia Project AV1 Decoder Inspector" VERSION_STRING,
  AOM_CODEC_INTERNAL_ABI_VERSION,
  AOM_CODEC_CAP_DECODER |
      AOM_CODEC_CAP_EXTERNAL_FRAME_BUFFER,  // aom_codec_caps_t
  decoder_init,                             // aom_codec_init_fn_t
  decoder_destroy,                          // aom_codec_destroy_fn_t
  decoder_ctrl_maps,                        // aom_codec_ctrl_fn_map_t
  {
      // NOLINT
      decoder_peek_si,    // aom_codec_peek_si_fn_t
      decoder_get_si,     // aom_codec_get_si_fn_t
      decoder_inspect,    // aom_codec_decode_fn_t
      decoder_get_frame,  // aom_codec_get_frame_fn_t
      decoder_set_fb_fn,  // aom_codec_set_fb_fn_t
  },
  {
      // NOLINT
      0,
      NULL,  // aom_codec_enc_cfg_t
      NULL,  // aom_codec_encode_fn_t
      NULL,  // aom_codec_get_cx_data_fn_t
      NULL,  // aom_codec_enc_config_set_fn_t
      NULL,  // aom_codec_get_global_headers_fn_t
      NULL   // aom_codec_get_preview_frame_fn_t
  },
  NULL  // aom_codec_set_option_fn_t
};

aom_codec_iface_t *aom_codec_av1_dx(void) { return &aom_codec_av1_dx_algo; }
