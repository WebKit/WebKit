/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "av1/encoder/encoder.h"
#include "av1/encoder/level.h"

#define UNDEFINED_LEVEL                                                 \
  {                                                                     \
    .level = SEQ_LEVEL_MAX, .max_picture_size = 0, .max_h_size = 0,     \
    .max_v_size = 0, .max_display_rate = 0, .max_decode_rate = 0,       \
    .max_header_rate = 0, .main_mbps = 0, .high_mbps = 0, .main_cr = 0, \
    .high_cr = 0, .max_tiles = 0, .max_tile_cols = 0                    \
  }

static const AV1LevelSpec av1_level_defs[SEQ_LEVELS] = {
  { .level = SEQ_LEVEL_2_0,
    .max_picture_size = 147456,
    .max_h_size = 2048,
    .max_v_size = 1152,
    .max_display_rate = 4423680L,
    .max_decode_rate = 5529600L,
    .max_header_rate = 150,
    .main_mbps = 1.5,
    .high_mbps = 0,
    .main_cr = 2.0,
    .high_cr = 0,
    .max_tiles = 8,
    .max_tile_cols = 4 },
  { .level = SEQ_LEVEL_2_1,
    .max_picture_size = 278784,
    .max_h_size = 2816,
    .max_v_size = 1584,
    .max_display_rate = 8363520L,
    .max_decode_rate = 10454400L,
    .max_header_rate = 150,
    .main_mbps = 3.0,
    .high_mbps = 0,
    .main_cr = 2.0,
    .high_cr = 0,
    .max_tiles = 8,
    .max_tile_cols = 4 },
  UNDEFINED_LEVEL,
  UNDEFINED_LEVEL,
  { .level = SEQ_LEVEL_3_0,
    .max_picture_size = 665856,
    .max_h_size = 4352,
    .max_v_size = 2448,
    .max_display_rate = 19975680L,
    .max_decode_rate = 24969600L,
    .max_header_rate = 150,
    .main_mbps = 6.0,
    .high_mbps = 0,
    .main_cr = 2.0,
    .high_cr = 0,
    .max_tiles = 16,
    .max_tile_cols = 6 },
  { .level = SEQ_LEVEL_3_1,
    .max_picture_size = 1065024,
    .max_h_size = 5504,
    .max_v_size = 3096,
    .max_display_rate = 31950720L,
    .max_decode_rate = 39938400L,
    .max_header_rate = 150,
    .main_mbps = 10.0,
    .high_mbps = 0,
    .main_cr = 2.0,
    .high_cr = 0,
    .max_tiles = 16,
    .max_tile_cols = 6 },
  UNDEFINED_LEVEL,
  UNDEFINED_LEVEL,
  { .level = SEQ_LEVEL_4_0,
    .max_picture_size = 2359296,
    .max_h_size = 6144,
    .max_v_size = 3456,
    .max_display_rate = 70778880L,
    .max_decode_rate = 77856768L,
    .max_header_rate = 300,
    .main_mbps = 12.0,
    .high_mbps = 30.0,
    .main_cr = 4.0,
    .high_cr = 4.0,
    .max_tiles = 32,
    .max_tile_cols = 8 },
  { .level = SEQ_LEVEL_4_1,
    .max_picture_size = 2359296,
    .max_h_size = 6144,
    .max_v_size = 3456,
    .max_display_rate = 141557760L,
    .max_decode_rate = 155713536L,
    .max_header_rate = 300,
    .main_mbps = 20.0,
    .high_mbps = 50.0,
    .main_cr = 4.0,
    .high_cr = 4.0,
    .max_tiles = 32,
    .max_tile_cols = 8 },
  UNDEFINED_LEVEL,
  UNDEFINED_LEVEL,
  { .level = SEQ_LEVEL_5_0,
    .max_picture_size = 8912896,
    .max_h_size = 8192,
    .max_v_size = 4352,
    .max_display_rate = 267386880L,
    .max_decode_rate = 273715200L,
    .max_header_rate = 300,
    .main_mbps = 30.0,
    .high_mbps = 100.0,
    .main_cr = 6.0,
    .high_cr = 4.0,
    .max_tiles = 64,
    .max_tile_cols = 8 },
  { .level = SEQ_LEVEL_5_1,
    .max_picture_size = 8912896,
    .max_h_size = 8192,
    .max_v_size = 4352,
    .max_display_rate = 534773760L,
    .max_decode_rate = 547430400L,
    .max_header_rate = 300,
    .main_mbps = 40.0,
    .high_mbps = 160.0,
    .main_cr = 8.0,
    .high_cr = 4.0,
    .max_tiles = 64,
    .max_tile_cols = 8 },
  { .level = SEQ_LEVEL_5_2,
    .max_picture_size = 8912896,
    .max_h_size = 8192,
    .max_v_size = 4352,
    .max_display_rate = 1069547520L,
    .max_decode_rate = 1094860800L,
    .max_header_rate = 300,
    .main_mbps = 60.0,
    .high_mbps = 240.0,
    .main_cr = 8.0,
    .high_cr = 4.0,
    .max_tiles = 64,
    .max_tile_cols = 8 },
  { .level = SEQ_LEVEL_5_3,
    .max_picture_size = 8912896,
    .max_h_size = 8192,
    .max_v_size = 4352,
    .max_display_rate = 1069547520L,
    .max_decode_rate = 1176502272L,
    .max_header_rate = 300,
    .main_mbps = 60.0,
    .high_mbps = 240.0,
    .main_cr = 8.0,
    .high_cr = 4.0,
    .max_tiles = 64,
    .max_tile_cols = 8 },
  { .level = SEQ_LEVEL_6_0,
    .max_picture_size = 35651584,
    .max_h_size = 16384,
    .max_v_size = 8704,
    .max_display_rate = 1069547520L,
    .max_decode_rate = 1176502272L,
    .max_header_rate = 300,
    .main_mbps = 60.0,
    .high_mbps = 240.0,
    .main_cr = 8.0,
    .high_cr = 4.0,
    .max_tiles = 128,
    .max_tile_cols = 16 },
  { .level = SEQ_LEVEL_6_1,
    .max_picture_size = 35651584,
    .max_h_size = 16384,
    .max_v_size = 8704,
    .max_display_rate = 2139095040L,
    .max_decode_rate = 2189721600L,
    .max_header_rate = 300,
    .main_mbps = 100.0,
    .high_mbps = 480.0,
    .main_cr = 8.0,
    .high_cr = 4.0,
    .max_tiles = 128,
    .max_tile_cols = 16 },
  { .level = SEQ_LEVEL_6_2,
    .max_picture_size = 35651584,
    .max_h_size = 16384,
    .max_v_size = 8704,
    .max_display_rate = 4278190080L,
    .max_decode_rate = 4379443200L,
    .max_header_rate = 300,
    .main_mbps = 160.0,
    .high_mbps = 800.0,
    .main_cr = 8.0,
    .high_cr = 4.0,
    .max_tiles = 128,
    .max_tile_cols = 16 },
  { .level = SEQ_LEVEL_6_3,
    .max_picture_size = 35651584,
    .max_h_size = 16384,
    .max_v_size = 8704,
    .max_display_rate = 4278190080L,
    .max_decode_rate = 4706009088L,
    .max_header_rate = 300,
    .main_mbps = 160.0,
    .high_mbps = 800.0,
    .main_cr = 8.0,
    .high_cr = 4.0,
    .max_tiles = 128,
    .max_tile_cols = 16 },
  UNDEFINED_LEVEL,
  UNDEFINED_LEVEL,
  UNDEFINED_LEVEL,
  UNDEFINED_LEVEL,
};

typedef enum {
  LUMA_PIC_SIZE_TOO_LARGE,
  LUMA_PIC_H_SIZE_TOO_LARGE,
  LUMA_PIC_V_SIZE_TOO_LARGE,
  LUMA_PIC_H_SIZE_TOO_SMALL,
  LUMA_PIC_V_SIZE_TOO_SMALL,
  TOO_MANY_TILE_COLUMNS,
  TOO_MANY_TILES,
  TILE_RATE_TOO_HIGH,
  TILE_TOO_LARGE,
  SUPERRES_TILE_WIDTH_TOO_LARGE,
  CROPPED_TILE_WIDTH_TOO_SMALL,
  CROPPED_TILE_HEIGHT_TOO_SMALL,
  TILE_WIDTH_INVALID,
  FRAME_HEADER_RATE_TOO_HIGH,
  DISPLAY_RATE_TOO_HIGH,
  DECODE_RATE_TOO_HIGH,
  CR_TOO_SMALL,
  TILE_SIZE_HEADER_RATE_TOO_HIGH,
  BITRATE_TOO_HIGH,
  DECODER_MODEL_FAIL,

  TARGET_LEVEL_FAIL_IDS,
  TARGET_LEVEL_OK,
} TARGET_LEVEL_FAIL_ID;

static const char *level_fail_messages[TARGET_LEVEL_FAIL_IDS] = {
  "The picture size is too large.",
  "The picture width is too large.",
  "The picture height is too large.",
  "The picture width is too small.",
  "The picture height is too small.",
  "Too many tile columns are used.",
  "Too many tiles are used.",
  "The tile rate is too high.",
  "The tile size is too large.",
  "The superres tile width is too large.",
  "The cropped tile width is less than 8.",
  "The cropped tile height is less than 8.",
  "The tile width is invalid.",
  "The frame header rate is too high.",
  "The display luma sample rate is too high.",
  "The decoded luma sample rate is too high.",
  "The compression ratio is too small.",
  "The product of max tile size and header rate is too high.",
  "The bitrate is too high.",
  "The decoder model fails.",
};

static double get_max_bitrate(const AV1LevelSpec *const level_spec, int tier,
                              BITSTREAM_PROFILE profile) {
  if (level_spec->level < SEQ_LEVEL_4_0) tier = 0;
  const double bitrate_basis =
      (tier ? level_spec->high_mbps : level_spec->main_mbps) * 1e6;
  const double bitrate_profile_factor =
      profile == PROFILE_0 ? 1.0 : (profile == PROFILE_1 ? 2.0 : 3.0);
  return bitrate_basis * bitrate_profile_factor;
}

double av1_get_max_bitrate_for_level(AV1_LEVEL level_index, int tier,
                                     BITSTREAM_PROFILE profile) {
  assert(is_valid_seq_level_idx(level_index));
  return get_max_bitrate(&av1_level_defs[level_index], tier, profile);
}

void av1_get_max_tiles_for_level(AV1_LEVEL level_index, int *const max_tiles,
                                 int *const max_tile_cols) {
  assert(is_valid_seq_level_idx(level_index));
  const AV1LevelSpec *const level_spec = &av1_level_defs[level_index];
  *max_tiles = level_spec->max_tiles;
  *max_tile_cols = level_spec->max_tile_cols;
}

// We assume time t to be valid if and only if t >= 0.0.
// So INVALID_TIME can be defined as anything less than 0.
#define INVALID_TIME (-1.0)

// This corresponds to "free_buffer" in the spec.
static void release_buffer(DECODER_MODEL *const decoder_model, int idx) {
  assert(idx >= 0 && idx < BUFFER_POOL_MAX_SIZE);
  FRAME_BUFFER *const this_buffer = &decoder_model->frame_buffer_pool[idx];
  this_buffer->decoder_ref_count = 0;
  this_buffer->player_ref_count = 0;
  this_buffer->display_index = -1;
  this_buffer->presentation_time = INVALID_TIME;
}

static void initialize_buffer_pool(DECODER_MODEL *const decoder_model) {
  for (int i = 0; i < BUFFER_POOL_MAX_SIZE; ++i) {
    release_buffer(decoder_model, i);
  }
  for (int i = 0; i < REF_FRAMES; ++i) {
    decoder_model->vbi[i] = -1;
  }
}

static int get_free_buffer(DECODER_MODEL *const decoder_model) {
  for (int i = 0; i < BUFFER_POOL_MAX_SIZE; ++i) {
    const FRAME_BUFFER *const this_buffer =
        &decoder_model->frame_buffer_pool[i];
    if (this_buffer->decoder_ref_count == 0 &&
        this_buffer->player_ref_count == 0)
      return i;
  }
  return -1;
}

static void update_ref_buffers(DECODER_MODEL *const decoder_model, int idx,
                               int refresh_frame_flags) {
  FRAME_BUFFER *const this_buffer = &decoder_model->frame_buffer_pool[idx];
  for (int i = 0; i < REF_FRAMES; ++i) {
    if (refresh_frame_flags & (1 << i)) {
      const int pre_idx = decoder_model->vbi[i];
      if (pre_idx != -1) {
        --decoder_model->frame_buffer_pool[pre_idx].decoder_ref_count;
      }
      decoder_model->vbi[i] = idx;
      ++this_buffer->decoder_ref_count;
    }
  }
}

// The time (in seconds) required to decode a frame.
static double time_to_decode_frame(const AV1_COMMON *const cm,
                                   int64_t max_decode_rate) {
  if (cm->show_existing_frame) return 0.0;

  const FRAME_TYPE frame_type = cm->current_frame.frame_type;
  int luma_samples = 0;
  if (frame_type == KEY_FRAME || frame_type == INTRA_ONLY_FRAME) {
    luma_samples = cm->superres_upscaled_width * cm->height;
  } else {
    const int spatial_layer_dimensions_present_flag = 0;
    if (spatial_layer_dimensions_present_flag) {
      assert(0 && "Spatial layer dimensions not supported yet.");
    } else {
      const SequenceHeader *const seq_params = cm->seq_params;
      const int max_frame_width = seq_params->max_frame_width;
      const int max_frame_height = seq_params->max_frame_height;
      luma_samples = max_frame_width * max_frame_height;
    }
  }

  return luma_samples / (double)max_decode_rate;
}

// Release frame buffers that are no longer needed for decode or display.
// It corresponds to "start_decode_at_removal_time" in the spec.
static void release_processed_frames(DECODER_MODEL *const decoder_model,
                                     double removal_time) {
  for (int i = 0; i < BUFFER_POOL_MAX_SIZE; ++i) {
    FRAME_BUFFER *const this_buffer = &decoder_model->frame_buffer_pool[i];
    if (this_buffer->player_ref_count > 0) {
      if (this_buffer->presentation_time >= 0.0 &&
          this_buffer->presentation_time <= removal_time) {
        this_buffer->player_ref_count = 0;
        if (this_buffer->decoder_ref_count == 0) {
          release_buffer(decoder_model, i);
        }
      }
    }
  }
}

static int frames_in_buffer_pool(const DECODER_MODEL *const decoder_model) {
  int frames_in_pool = 0;
  for (int i = 0; i < BUFFER_POOL_MAX_SIZE; ++i) {
    const FRAME_BUFFER *const this_buffer =
        &decoder_model->frame_buffer_pool[i];
    if (this_buffer->decoder_ref_count > 0 ||
        this_buffer->player_ref_count > 0) {
      ++frames_in_pool;
    }
  }
  return frames_in_pool;
}

static double get_presentation_time(const DECODER_MODEL *const decoder_model,
                                    int display_index) {
  if (decoder_model->mode == SCHEDULE_MODE) {
    assert(0 && "SCHEDULE_MODE NOT SUPPORTED");
    return INVALID_TIME;
  } else {
    const double initial_presentation_delay =
        decoder_model->initial_presentation_delay;
    // Can't decide presentation time until the initial presentation delay is
    // known.
    if (initial_presentation_delay < 0.0) return INVALID_TIME;

    return initial_presentation_delay +
           display_index * decoder_model->num_ticks_per_picture *
               decoder_model->display_clock_tick;
  }
}

#define MAX_TIME 1e16
double time_next_buffer_is_free(int num_decoded_frame, int decoder_buffer_delay,
                                const FRAME_BUFFER *frame_buffer_pool,
                                double current_time) {
  if (num_decoded_frame == 0) {
    return (double)decoder_buffer_delay / 90000.0;
  }

  double buf_free_time = MAX_TIME;
  for (int i = 0; i < BUFFER_POOL_MAX_SIZE; ++i) {
    const FRAME_BUFFER *const this_buffer = &frame_buffer_pool[i];
    if (this_buffer->decoder_ref_count == 0) {
      if (this_buffer->player_ref_count == 0) {
        return current_time;
      }
      const double presentation_time = this_buffer->presentation_time;
      if (presentation_time >= 0.0 && presentation_time < buf_free_time) {
        buf_free_time = presentation_time;
      }
    }
  }
  return buf_free_time < MAX_TIME ? buf_free_time : INVALID_TIME;
}
#undef MAX_TIME

static double get_removal_time(int mode, int num_decoded_frame,
                               int decoder_buffer_delay,
                               const FRAME_BUFFER *frame_buffer_pool,
                               double current_time) {
  if (mode == SCHEDULE_MODE) {
    assert(0 && "SCHEDULE_MODE IS NOT SUPPORTED YET");
    return INVALID_TIME;
  } else {
    return time_next_buffer_is_free(num_decoded_frame, decoder_buffer_delay,
                                    frame_buffer_pool, current_time);
  }
}

void av1_decoder_model_print_status(const DECODER_MODEL *const decoder_model) {
  printf(
      "\n status %d, num_frame %3d, num_decoded_frame %3d, "
      "num_shown_frame %3d, current time %6.2f, frames in buffer %2d, "
      "presentation delay %6.2f, total interval %6.2f\n",
      decoder_model->status, decoder_model->num_frame,
      decoder_model->num_decoded_frame, decoder_model->num_shown_frame,
      decoder_model->current_time, frames_in_buffer_pool(decoder_model),
      decoder_model->initial_presentation_delay,
      decoder_model->dfg_interval_queue.total_interval);
  for (int i = 0; i < 10; ++i) {
    const FRAME_BUFFER *const this_buffer =
        &decoder_model->frame_buffer_pool[i];
    printf("buffer %d, decode count %d, display count %d, present time %6.4f\n",
           i, this_buffer->decoder_ref_count, this_buffer->player_ref_count,
           this_buffer->presentation_time);
  }
}

// op_index is the operating point index.
void av1_decoder_model_init(const AV1_COMP *const cpi, AV1_LEVEL level,
                            int op_index, DECODER_MODEL *const decoder_model) {
  decoder_model->status = DECODER_MODEL_OK;
  decoder_model->level = level;

  const AV1_COMMON *const cm = &cpi->common;
  const SequenceHeader *const seq_params = cm->seq_params;
  decoder_model->bit_rate = get_max_bitrate(
      av1_level_defs + level, seq_params->tier[op_index], seq_params->profile);

  // TODO(huisu or anyone): implement SCHEDULE_MODE.
  decoder_model->mode = RESOURCE_MODE;
  decoder_model->encoder_buffer_delay = 20000;
  decoder_model->decoder_buffer_delay = 70000;
  decoder_model->is_low_delay_mode = false;

  decoder_model->first_bit_arrival_time = 0.0;
  decoder_model->last_bit_arrival_time = 0.0;
  decoder_model->coded_bits = 0;

  decoder_model->removal_time = INVALID_TIME;
  decoder_model->presentation_time = INVALID_TIME;
  decoder_model->decode_samples = 0;
  decoder_model->display_samples = 0;
  decoder_model->max_decode_rate = 0.0;
  decoder_model->max_display_rate = 0.0;

  decoder_model->num_frame = -1;
  decoder_model->num_decoded_frame = -1;
  decoder_model->num_shown_frame = -1;
  decoder_model->current_time = 0.0;

  initialize_buffer_pool(decoder_model);

  DFG_INTERVAL_QUEUE *const dfg_interval_queue =
      &decoder_model->dfg_interval_queue;
  dfg_interval_queue->total_interval = 0.0;
  dfg_interval_queue->head = 0;
  dfg_interval_queue->size = 0;

  if (seq_params->timing_info_present) {
    decoder_model->num_ticks_per_picture =
        seq_params->timing_info.num_ticks_per_picture;
    decoder_model->display_clock_tick =
        seq_params->timing_info.num_units_in_display_tick /
        seq_params->timing_info.time_scale;
  } else {
    decoder_model->num_ticks_per_picture = 1;
    decoder_model->display_clock_tick = 1.0 / cpi->framerate;
  }

  decoder_model->initial_display_delay =
      seq_params->op_params[op_index].initial_display_delay;
  decoder_model->initial_presentation_delay = INVALID_TIME;
  decoder_model->decode_rate = av1_level_defs[level].max_decode_rate;
}

DECODER_MODEL_STATUS av1_decoder_model_try_smooth_buf(
    const AV1_COMP *const cpi, size_t coded_bits,
    const DECODER_MODEL *const decoder_model) {
  DECODER_MODEL_STATUS status = DECODER_MODEL_OK;

  if (!decoder_model || decoder_model->status != DECODER_MODEL_OK) {
    return status;
  }

  const AV1_COMMON *const cm = &cpi->common;
  const int show_existing_frame = cm->show_existing_frame;

  size_t cur_coded_bits = decoder_model->coded_bits + coded_bits;
  int num_decoded_frame = decoder_model->num_decoded_frame;
  if (!show_existing_frame) ++num_decoded_frame;

  if (show_existing_frame) {
    return status;
  } else {
    const double removal_time = get_removal_time(
        decoder_model->mode, num_decoded_frame,
        decoder_model->decoder_buffer_delay, decoder_model->frame_buffer_pool,
        decoder_model->current_time);
    if (removal_time < 0.0) {
      status = DECODE_FRAME_BUF_UNAVAILABLE;
      return status;
    }

    // A frame with show_existing_frame being false indicates the end of a DFG.
    // Update the bits arrival time of this DFG.
    const double buffer_delay = (decoder_model->encoder_buffer_delay +
                                 decoder_model->decoder_buffer_delay) /
                                90000.0;
    const double latest_arrival_time = removal_time - buffer_delay;
    const double first_bit_arrival_time =
        AOMMAX(decoder_model->last_bit_arrival_time, latest_arrival_time);
    const double last_bit_arrival_time =
        first_bit_arrival_time +
        (double)cur_coded_bits / decoder_model->bit_rate;
    // Smoothing buffer underflows if the last bit arrives after the removal
    // time.
    if (last_bit_arrival_time > removal_time &&
        !decoder_model->is_low_delay_mode) {
      status = SMOOTHING_BUFFER_UNDERFLOW;
      return status;
    }

    // Check if the smoothing buffer overflows.
    const DFG_INTERVAL_QUEUE *const queue = &decoder_model->dfg_interval_queue;
    if (queue->size >= DFG_INTERVAL_QUEUE_SIZE) {
      assert(0);
    }

    double total_interval = queue->total_interval;
    int qhead = queue->head;
    int qsize = queue->size;
    // Remove the DFGs with removal time earlier than last_bit_arrival_time.
    while (queue->buf[qhead].removal_time <= last_bit_arrival_time &&
           qsize > 0) {
      if (queue->buf[qhead].removal_time - first_bit_arrival_time +
              total_interval >
          1.0) {
        status = SMOOTHING_BUFFER_OVERFLOW;
        return status;
      }
      total_interval -= queue->buf[qhead].last_bit_arrival_time -
                        queue->buf[qhead].first_bit_arrival_time;
      qhead = (qhead + 1) % DFG_INTERVAL_QUEUE_SIZE;
      --qsize;
    }
    total_interval += last_bit_arrival_time - first_bit_arrival_time;
    // The smoothing buffer can hold at most "bit_rate" bits, which is
    // equivalent to 1 second of total interval.
    if (total_interval > 1.0) {
      status = SMOOTHING_BUFFER_OVERFLOW;
      return status;
    }

    return status;
  }
}

void av1_decoder_model_process_frame(const AV1_COMP *const cpi,
                                     size_t coded_bits,
                                     DECODER_MODEL *const decoder_model) {
  if (!decoder_model || decoder_model->status != DECODER_MODEL_OK) return;

  const AV1_COMMON *const cm = &cpi->common;
  const int luma_pic_size = cm->superres_upscaled_width * cm->height;
  const int show_existing_frame = cm->show_existing_frame;
  const int show_frame = cm->show_frame || show_existing_frame;
  ++decoder_model->num_frame;
  if (!show_existing_frame) ++decoder_model->num_decoded_frame;
  if (show_frame) ++decoder_model->num_shown_frame;
  decoder_model->coded_bits += coded_bits;

  int display_idx = -1;
  if (show_existing_frame) {
    display_idx = decoder_model->vbi[cpi->existing_fb_idx_to_show];
    if (display_idx < 0) {
      decoder_model->status = DECODE_EXISTING_FRAME_BUF_EMPTY;
      return;
    }
    if (decoder_model->frame_buffer_pool[display_idx].frame_type == KEY_FRAME) {
      update_ref_buffers(decoder_model, display_idx, 0xFF);
    }
  } else {
    const double removal_time = get_removal_time(
        decoder_model->mode, decoder_model->num_decoded_frame,
        decoder_model->decoder_buffer_delay, decoder_model->frame_buffer_pool,
        decoder_model->current_time);
    if (removal_time < 0.0) {
      decoder_model->status = DECODE_FRAME_BUF_UNAVAILABLE;
      return;
    }

    const int previous_decode_samples = decoder_model->decode_samples;
    const double previous_removal_time = decoder_model->removal_time;
    assert(previous_removal_time < removal_time);
    decoder_model->removal_time = removal_time;
    decoder_model->decode_samples = luma_pic_size;
    const double this_decode_rate =
        previous_decode_samples / (removal_time - previous_removal_time);
    decoder_model->max_decode_rate =
        AOMMAX(decoder_model->max_decode_rate, this_decode_rate);

    // A frame with show_existing_frame being false indicates the end of a DFG.
    // Update the bits arrival time of this DFG.
    const double buffer_delay = (decoder_model->encoder_buffer_delay +
                                 decoder_model->decoder_buffer_delay) /
                                90000.0;
    const double latest_arrival_time = removal_time - buffer_delay;
    decoder_model->first_bit_arrival_time =
        AOMMAX(decoder_model->last_bit_arrival_time, latest_arrival_time);
    decoder_model->last_bit_arrival_time =
        decoder_model->first_bit_arrival_time +
        (double)decoder_model->coded_bits / decoder_model->bit_rate;
    // Smoothing buffer underflows if the last bit arrives after the removal
    // time.
    if (decoder_model->last_bit_arrival_time > removal_time &&
        !decoder_model->is_low_delay_mode) {
      decoder_model->status = SMOOTHING_BUFFER_UNDERFLOW;
      return;
    }
    // Reset the coded bits for the next DFG.
    decoder_model->coded_bits = 0;

    // Check if the smoothing buffer overflows.
    DFG_INTERVAL_QUEUE *const queue = &decoder_model->dfg_interval_queue;
    if (queue->size >= DFG_INTERVAL_QUEUE_SIZE) {
      assert(0);
    }
    const double first_bit_arrival_time = decoder_model->first_bit_arrival_time;
    const double last_bit_arrival_time = decoder_model->last_bit_arrival_time;
    // Remove the DFGs with removal time earlier than last_bit_arrival_time.
    while (queue->buf[queue->head].removal_time <= last_bit_arrival_time &&
           queue->size > 0) {
      if (queue->buf[queue->head].removal_time - first_bit_arrival_time +
              queue->total_interval >
          1.0) {
        decoder_model->status = SMOOTHING_BUFFER_OVERFLOW;
        return;
      }
      queue->total_interval -= queue->buf[queue->head].last_bit_arrival_time -
                               queue->buf[queue->head].first_bit_arrival_time;
      queue->head = (queue->head + 1) % DFG_INTERVAL_QUEUE_SIZE;
      --queue->size;
    }
    // Push current DFG into the queue.
    const int queue_index =
        (queue->head + queue->size++) % DFG_INTERVAL_QUEUE_SIZE;
    queue->buf[queue_index].first_bit_arrival_time = first_bit_arrival_time;
    queue->buf[queue_index].last_bit_arrival_time = last_bit_arrival_time;
    queue->buf[queue_index].removal_time = removal_time;
    queue->total_interval += last_bit_arrival_time - first_bit_arrival_time;
    // The smoothing buffer can hold at most "bit_rate" bits, which is
    // equivalent to 1 second of total interval.
    if (queue->total_interval > 1.0) {
      decoder_model->status = SMOOTHING_BUFFER_OVERFLOW;
      return;
    }

    release_processed_frames(decoder_model, removal_time);
    decoder_model->current_time =
        removal_time + time_to_decode_frame(cm, decoder_model->decode_rate);

    const int cfbi = get_free_buffer(decoder_model);
    if (cfbi < 0) {
      decoder_model->status = DECODE_FRAME_BUF_UNAVAILABLE;
      return;
    }
    const CurrentFrame *const current_frame = &cm->current_frame;
    decoder_model->frame_buffer_pool[cfbi].frame_type =
        cm->current_frame.frame_type;
    display_idx = cfbi;
    update_ref_buffers(decoder_model, cfbi, current_frame->refresh_frame_flags);

    if (decoder_model->initial_presentation_delay < 0.0) {
      // Display can begin after required number of frames have been buffered.
      if (frames_in_buffer_pool(decoder_model) >=
          decoder_model->initial_display_delay - 1) {
        decoder_model->initial_presentation_delay = decoder_model->current_time;
        // Update presentation time for each shown frame in the frame buffer.
        for (int i = 0; i < BUFFER_POOL_MAX_SIZE; ++i) {
          FRAME_BUFFER *const this_buffer =
              &decoder_model->frame_buffer_pool[i];
          if (this_buffer->player_ref_count == 0) continue;
          assert(this_buffer->display_index >= 0);
          this_buffer->presentation_time =
              get_presentation_time(decoder_model, this_buffer->display_index);
        }
      }
    }
  }

  // Display.
  if (show_frame) {
    assert(display_idx >= 0 && display_idx < BUFFER_POOL_MAX_SIZE);
    FRAME_BUFFER *const this_buffer =
        &decoder_model->frame_buffer_pool[display_idx];
    ++this_buffer->player_ref_count;
    this_buffer->display_index = decoder_model->num_shown_frame;
    const double presentation_time =
        get_presentation_time(decoder_model, this_buffer->display_index);
    this_buffer->presentation_time = presentation_time;
    if (presentation_time >= 0.0 &&
        decoder_model->current_time > presentation_time) {
      decoder_model->status = DISPLAY_FRAME_LATE;
      return;
    }

    const int previous_display_samples = decoder_model->display_samples;
    const double previous_presentation_time = decoder_model->presentation_time;
    decoder_model->display_samples = luma_pic_size;
    decoder_model->presentation_time = presentation_time;
    if (presentation_time >= 0.0 && previous_presentation_time >= 0.0) {
      assert(previous_presentation_time < presentation_time);
      const double this_display_rate =
          previous_display_samples /
          (presentation_time - previous_presentation_time);
      decoder_model->max_display_rate =
          AOMMAX(decoder_model->max_display_rate, this_display_rate);
    }
  }
}

void av1_init_level_info(AV1_COMP *cpi) {
  for (int op_index = 0; op_index < MAX_NUM_OPERATING_POINTS; ++op_index) {
    AV1LevelInfo *const this_level_info =
        cpi->ppi->level_params.level_info[op_index];
    if (!this_level_info) continue;
    memset(this_level_info, 0, sizeof(*this_level_info));
    AV1LevelSpec *const level_spec = &this_level_info->level_spec;
    level_spec->level = SEQ_LEVEL_MAX;
    AV1LevelStats *const level_stats = &this_level_info->level_stats;
    level_stats->min_cropped_tile_width = INT_MAX;
    level_stats->min_cropped_tile_height = INT_MAX;
    level_stats->min_frame_width = INT_MAX;
    level_stats->min_frame_height = INT_MAX;
    level_stats->tile_width_is_valid = 1;
    level_stats->min_cr = 1e8;

    FrameWindowBuffer *const frame_window_buffer =
        &this_level_info->frame_window_buffer;
    frame_window_buffer->num = 0;
    frame_window_buffer->start = 0;

    const AV1_COMMON *const cm = &cpi->common;
    const int upscaled_width = cm->superres_upscaled_width;
    const int height = cm->height;
    const int pic_size = upscaled_width * height;
    for (AV1_LEVEL level = SEQ_LEVEL_2_0; level < SEQ_LEVELS; ++level) {
      DECODER_MODEL *const this_model = &this_level_info->decoder_models[level];
      const AV1LevelSpec *const spec = &av1_level_defs[level];
      if (upscaled_width > spec->max_h_size || height > spec->max_v_size ||
          pic_size > spec->max_picture_size) {
        // Turn off decoder model for this level as the frame size already
        // exceeds level constraints.
        this_model->status = DECODER_MODEL_DISABLED;
      } else {
        av1_decoder_model_init(cpi, level, op_index, this_model);
      }
    }
  }
}

static double get_min_cr(const AV1LevelSpec *const level_spec, int tier,
                         int is_still_picture, int64_t decoded_sample_rate) {
  if (is_still_picture) return 0.8;
  if (level_spec->level < SEQ_LEVEL_4_0) tier = 0;
  const double min_cr_basis = tier ? level_spec->high_cr : level_spec->main_cr;
  const double speed_adj =
      (double)decoded_sample_rate / level_spec->max_display_rate;
  return AOMMAX(min_cr_basis * speed_adj, 0.8);
}

double av1_get_min_cr_for_level(AV1_LEVEL level_index, int tier,
                                int is_still_picture) {
  assert(is_valid_seq_level_idx(level_index));
  const AV1LevelSpec *const level_spec = &av1_level_defs[level_index];
  return get_min_cr(level_spec, tier, is_still_picture,
                    level_spec->max_decode_rate);
}

static void get_temporal_parallel_params(int scalability_mode_idc,
                                         int *temporal_parallel_num,
                                         int *temporal_parallel_denom) {
  if (scalability_mode_idc < 0) {
    *temporal_parallel_num = 1;
    *temporal_parallel_denom = 1;
    return;
  }

  // TODO(huisu@): handle scalability cases.
  if (scalability_mode_idc == SCALABILITY_SS) {
    (void)scalability_mode_idc;
  } else {
    (void)scalability_mode_idc;
  }
}

#define MAX_TILE_SIZE (4096 * 2304)
#define MIN_CROPPED_TILE_WIDTH 8
#define MIN_CROPPED_TILE_HEIGHT 8
#define MIN_FRAME_WIDTH 16
#define MIN_FRAME_HEIGHT 16
#define MAX_TILE_SIZE_HEADER_RATE_PRODUCT 588251136

static TARGET_LEVEL_FAIL_ID check_level_constraints(
    const AV1LevelInfo *const level_info, AV1_LEVEL level, int tier,
    int is_still_picture, BITSTREAM_PROFILE profile, int check_bitrate) {
  const DECODER_MODEL *const decoder_model = &level_info->decoder_models[level];
  const DECODER_MODEL_STATUS decoder_model_status = decoder_model->status;
  if (decoder_model_status != DECODER_MODEL_OK &&
      decoder_model_status != DECODER_MODEL_DISABLED) {
    return DECODER_MODEL_FAIL;
  }

  const AV1LevelSpec *const level_spec = &level_info->level_spec;
  const AV1LevelSpec *const target_level_spec = &av1_level_defs[level];
  const AV1LevelStats *const level_stats = &level_info->level_stats;
  TARGET_LEVEL_FAIL_ID fail_id = TARGET_LEVEL_OK;
  do {
    if (level_spec->max_picture_size > target_level_spec->max_picture_size) {
      fail_id = LUMA_PIC_SIZE_TOO_LARGE;
      break;
    }

    if (level_spec->max_h_size > target_level_spec->max_h_size) {
      fail_id = LUMA_PIC_H_SIZE_TOO_LARGE;
      break;
    }

    if (level_spec->max_v_size > target_level_spec->max_v_size) {
      fail_id = LUMA_PIC_V_SIZE_TOO_LARGE;
      break;
    }

    if (level_spec->max_tile_cols > target_level_spec->max_tile_cols) {
      fail_id = TOO_MANY_TILE_COLUMNS;
      break;
    }

    if (level_spec->max_tiles > target_level_spec->max_tiles) {
      fail_id = TOO_MANY_TILES;
      break;
    }

    if (level_spec->max_header_rate > target_level_spec->max_header_rate) {
      fail_id = FRAME_HEADER_RATE_TOO_HIGH;
      break;
    }

    if (decoder_model->max_display_rate >
        (double)target_level_spec->max_display_rate) {
      fail_id = DISPLAY_RATE_TOO_HIGH;
      break;
    }

    // TODO(huisu): we are not using max decode rate calculated by the decoder
    // model because the model in resource availability mode always returns
    // MaxDecodeRate(as in the level definitions) as the max decode rate.
    if (level_spec->max_decode_rate > target_level_spec->max_decode_rate) {
      fail_id = DECODE_RATE_TOO_HIGH;
      break;
    }

    if (level_spec->max_tile_rate > target_level_spec->max_tiles * 120) {
      fail_id = TILE_RATE_TOO_HIGH;
      break;
    }

    if (level_stats->max_tile_size > MAX_TILE_SIZE) {
      fail_id = TILE_TOO_LARGE;
      break;
    }

    if (level_stats->max_superres_tile_width > MAX_TILE_WIDTH) {
      fail_id = SUPERRES_TILE_WIDTH_TOO_LARGE;
      break;
    }

    if (level_stats->min_cropped_tile_width < MIN_CROPPED_TILE_WIDTH) {
      fail_id = CROPPED_TILE_WIDTH_TOO_SMALL;
      break;
    }

    if (level_stats->min_cropped_tile_height < MIN_CROPPED_TILE_HEIGHT) {
      fail_id = CROPPED_TILE_HEIGHT_TOO_SMALL;
      break;
    }

    if (level_stats->min_frame_width < MIN_FRAME_WIDTH) {
      fail_id = LUMA_PIC_H_SIZE_TOO_SMALL;
      break;
    }

    if (level_stats->min_frame_height < MIN_FRAME_HEIGHT) {
      fail_id = LUMA_PIC_V_SIZE_TOO_SMALL;
      break;
    }

    if (!level_stats->tile_width_is_valid) {
      fail_id = TILE_WIDTH_INVALID;
      break;
    }

    const double min_cr = get_min_cr(target_level_spec, tier, is_still_picture,
                                     level_spec->max_decode_rate);
    if (level_stats->min_cr < min_cr) {
      fail_id = CR_TOO_SMALL;
      break;
    }

    if (check_bitrate) {
      // Check average bitrate instead of max_bitrate.
      const double bitrate_limit =
          get_max_bitrate(target_level_spec, tier, profile);
      const double avg_bitrate = level_stats->total_compressed_size * 8.0 /
                                 level_stats->total_time_encoded;
      if (avg_bitrate > bitrate_limit) {
        fail_id = BITRATE_TOO_HIGH;
        break;
      }
    }

    if (target_level_spec->level > SEQ_LEVEL_5_1) {
      int temporal_parallel_num;
      int temporal_parallel_denom;
      const int scalability_mode_idc = -1;
      get_temporal_parallel_params(scalability_mode_idc, &temporal_parallel_num,
                                   &temporal_parallel_denom);
      const int val = level_stats->max_tile_size * level_spec->max_header_rate *
                      temporal_parallel_denom / temporal_parallel_num;
      if (val > MAX_TILE_SIZE_HEADER_RATE_PRODUCT) {
        fail_id = TILE_SIZE_HEADER_RATE_TOO_HIGH;
        break;
      }
    }
  } while (0);

  return fail_id;
}

static void get_tile_stats(const AV1_COMMON *const cm,
                           const TileDataEnc *const tile_data,
                           int *max_tile_size, int *max_superres_tile_width,
                           int *min_cropped_tile_width,
                           int *min_cropped_tile_height,
                           int *tile_width_valid) {
  const int tile_cols = cm->tiles.cols;
  const int tile_rows = cm->tiles.rows;
  const int superres_scale_denominator = cm->superres_scale_denominator;

  *max_tile_size = 0;
  *max_superres_tile_width = 0;
  *min_cropped_tile_width = INT_MAX;
  *min_cropped_tile_height = INT_MAX;
  *tile_width_valid = 1;

  for (int tile_row = 0; tile_row < tile_rows; ++tile_row) {
    for (int tile_col = 0; tile_col < tile_cols; ++tile_col) {
      const TileInfo *const tile_info =
          &tile_data[tile_row * cm->tiles.cols + tile_col].tile_info;
      const int tile_width =
          (tile_info->mi_col_end - tile_info->mi_col_start) * MI_SIZE;
      const int tile_height =
          (tile_info->mi_row_end - tile_info->mi_row_start) * MI_SIZE;
      const int tile_size = tile_width * tile_height;
      *max_tile_size = AOMMAX(*max_tile_size, tile_size);

      const int supperres_tile_width =
          tile_width * superres_scale_denominator / SCALE_NUMERATOR;
      *max_superres_tile_width =
          AOMMAX(*max_superres_tile_width, supperres_tile_width);

      const int cropped_tile_width =
          cm->width - tile_info->mi_col_start * MI_SIZE;
      const int cropped_tile_height =
          cm->height - tile_info->mi_row_start * MI_SIZE;
      *min_cropped_tile_width =
          AOMMIN(*min_cropped_tile_width, cropped_tile_width);
      *min_cropped_tile_height =
          AOMMIN(*min_cropped_tile_height, cropped_tile_height);

      const int is_right_most_tile =
          tile_info->mi_col_end == cm->mi_params.mi_cols;
      if (!is_right_most_tile) {
        if (av1_superres_scaled(cm))
          *tile_width_valid &= tile_width >= 128;
        else
          *tile_width_valid &= tile_width >= 64;
      }
    }
  }
}

static int store_frame_record(int64_t ts_start, int64_t ts_end,
                              size_t encoded_size, int pic_size,
                              int frame_header_count, int tiles, int show_frame,
                              int show_existing_frame,
                              FrameWindowBuffer *const buffer) {
  if (buffer->num < FRAME_WINDOW_SIZE) {
    ++buffer->num;
  } else {
    buffer->start = (buffer->start + 1) % FRAME_WINDOW_SIZE;
  }
  const int new_idx = (buffer->start + buffer->num - 1) % FRAME_WINDOW_SIZE;
  FrameRecord *const record = &buffer->buf[new_idx];
  record->ts_start = ts_start;
  record->ts_end = ts_end;
  record->encoded_size_in_bytes = encoded_size;
  record->pic_size = pic_size;
  record->frame_header_count = frame_header_count;
  record->tiles = tiles;
  record->show_frame = show_frame;
  record->show_existing_frame = show_existing_frame;

  return new_idx;
}

// Count the number of frames encoded in the last "duration" ticks, in display
// time.
static int count_frames(const FrameWindowBuffer *const buffer,
                        int64_t duration) {
  const int current_idx = (buffer->start + buffer->num - 1) % FRAME_WINDOW_SIZE;
  // Assume current frame is shown frame.
  assert(buffer->buf[current_idx].show_frame);

  const int64_t current_time = buffer->buf[current_idx].ts_end;
  const int64_t time_limit = AOMMAX(current_time - duration, 0);
  int num_frames = 1;
  int index = current_idx - 1;
  for (int i = buffer->num - 2; i >= 0; --i, --index, ++num_frames) {
    if (index < 0) index = FRAME_WINDOW_SIZE - 1;
    const FrameRecord *const record = &buffer->buf[index];
    if (!record->show_frame) continue;
    const int64_t ts_start = record->ts_start;
    if (ts_start < time_limit) break;
  }

  return num_frames;
}

// Scan previously encoded frames and update level metrics accordingly.
static void scan_past_frames(const FrameWindowBuffer *const buffer,
                             int num_frames_to_scan,
                             AV1LevelSpec *const level_spec,
                             AV1LevelStats *const level_stats) {
  const int num_frames_in_buffer = buffer->num;
  int index = (buffer->start + num_frames_in_buffer - 1) % FRAME_WINDOW_SIZE;
  int frame_headers = 0;
  int tiles = 0;
  int64_t display_samples = 0;
  int64_t decoded_samples = 0;
  size_t encoded_size_in_bytes = 0;
  for (int i = 0; i < AOMMIN(num_frames_in_buffer, num_frames_to_scan); ++i) {
    const FrameRecord *const record = &buffer->buf[index];
    if (!record->show_existing_frame) {
      frame_headers += record->frame_header_count;
      decoded_samples += record->pic_size;
    }
    if (record->show_frame) {
      display_samples += record->pic_size;
    }
    tiles += record->tiles;
    encoded_size_in_bytes += record->encoded_size_in_bytes;
    --index;
    if (index < 0) index = FRAME_WINDOW_SIZE - 1;
  }
  level_spec->max_header_rate =
      AOMMAX(level_spec->max_header_rate, frame_headers);
  // TODO(huisu): we can now compute max display rate with the decoder model, so
  // these couple of lines can be removed. Keep them here for a while for
  // debugging purpose.
  level_spec->max_display_rate =
      AOMMAX(level_spec->max_display_rate, display_samples);
  level_spec->max_decode_rate =
      AOMMAX(level_spec->max_decode_rate, decoded_samples);
  level_spec->max_tile_rate = AOMMAX(level_spec->max_tile_rate, tiles);
  level_stats->max_bitrate =
      AOMMAX(level_stats->max_bitrate, (int)encoded_size_in_bytes * 8);
}

void av1_update_level_info(AV1_COMP *cpi, size_t size, int64_t ts_start,
                           int64_t ts_end) {
  AV1_COMMON *const cm = &cpi->common;
  const AV1LevelParams *const level_params = &cpi->ppi->level_params;

  const int upscaled_width = cm->superres_upscaled_width;
  const int width = cm->width;
  const int height = cm->height;
  const int tile_cols = cm->tiles.cols;
  const int tile_rows = cm->tiles.rows;
  const int tiles = tile_cols * tile_rows;
  const int luma_pic_size = upscaled_width * height;
  const int frame_header_count = cpi->frame_header_count;
  const int show_frame = cm->show_frame;
  const int show_existing_frame = cm->show_existing_frame;

  int max_tile_size;
  int min_cropped_tile_width;
  int min_cropped_tile_height;
  int max_superres_tile_width;
  int tile_width_is_valid;
  get_tile_stats(cm, cpi->tile_data, &max_tile_size, &max_superres_tile_width,
                 &min_cropped_tile_width, &min_cropped_tile_height,
                 &tile_width_is_valid);

  const double compression_ratio = av1_get_compression_ratio(cm, size);

  const int temporal_layer_id = cm->temporal_layer_id;
  const int spatial_layer_id = cm->spatial_layer_id;
  const SequenceHeader *const seq_params = cm->seq_params;
  const BITSTREAM_PROFILE profile = seq_params->profile;
  const int is_still_picture = seq_params->still_picture;
  // update level_stats
  // TODO(kyslov@) fix the implementation according to buffer model
  for (int i = 0; i < seq_params->operating_points_cnt_minus_1 + 1; ++i) {
    if (!is_in_operating_point(seq_params->operating_point_idc[i],
                               temporal_layer_id, spatial_layer_id) ||
        !((level_params->keep_level_stats >> i) & 1)) {
      continue;
    }

    AV1LevelInfo *const level_info = level_params->level_info[i];
    assert(level_info != NULL);
    AV1LevelStats *const level_stats = &level_info->level_stats;

    level_stats->max_tile_size =
        AOMMAX(level_stats->max_tile_size, max_tile_size);
    level_stats->max_superres_tile_width =
        AOMMAX(level_stats->max_superres_tile_width, max_superres_tile_width);
    level_stats->min_cropped_tile_width =
        AOMMIN(level_stats->min_cropped_tile_width, min_cropped_tile_width);
    level_stats->min_cropped_tile_height =
        AOMMIN(level_stats->min_cropped_tile_height, min_cropped_tile_height);
    level_stats->tile_width_is_valid &= tile_width_is_valid;
    level_stats->min_frame_width = AOMMIN(level_stats->min_frame_width, width);
    level_stats->min_frame_height =
        AOMMIN(level_stats->min_frame_height, height);
    level_stats->min_cr = AOMMIN(level_stats->min_cr, compression_ratio);
    level_stats->total_compressed_size += (double)size;

    // update level_spec
    // TODO(kyslov@) update all spec fields
    AV1LevelSpec *const level_spec = &level_info->level_spec;
    level_spec->max_picture_size =
        AOMMAX(level_spec->max_picture_size, luma_pic_size);
    level_spec->max_h_size =
        AOMMAX(level_spec->max_h_size, cm->superres_upscaled_width);
    level_spec->max_v_size = AOMMAX(level_spec->max_v_size, height);
    level_spec->max_tile_cols = AOMMAX(level_spec->max_tile_cols, tile_cols);
    level_spec->max_tiles = AOMMAX(level_spec->max_tiles, tiles);

    // Store info. of current frame into FrameWindowBuffer.
    FrameWindowBuffer *const buffer = &level_info->frame_window_buffer;
    store_frame_record(ts_start, ts_end, size, luma_pic_size,
                       frame_header_count, tiles, show_frame,
                       show_existing_frame, buffer);
    if (show_frame) {
      // Count the number of frames encoded in the past 1 second.
      const int encoded_frames_in_last_second =
          show_frame ? count_frames(buffer, TICKS_PER_SEC) : 0;
      scan_past_frames(buffer, encoded_frames_in_last_second, level_spec,
                       level_stats);
      level_stats->total_time_encoded +=
          (cpi->time_stamps.prev_ts_end - cpi->time_stamps.prev_ts_start) /
          (double)TICKS_PER_SEC;
    }

    DECODER_MODEL *const decoder_models = level_info->decoder_models;
    for (AV1_LEVEL level = SEQ_LEVEL_2_0; level < SEQ_LEVELS; ++level) {
      av1_decoder_model_process_frame(cpi, size << 3, &decoder_models[level]);
    }

    // Check whether target level is met.
    const AV1_LEVEL target_level = level_params->target_seq_level_idx[i];
    if (target_level < SEQ_LEVELS && cpi->oxcf.strict_level_conformance) {
      assert(is_valid_seq_level_idx(target_level));
      const int tier = seq_params->tier[i];
      const TARGET_LEVEL_FAIL_ID fail_id = check_level_constraints(
          level_info, target_level, tier, is_still_picture, profile, 0);
      if (fail_id != TARGET_LEVEL_OK) {
        const int target_level_major = 2 + (target_level >> 2);
        const int target_level_minor = target_level & 3;
        aom_internal_error(cm->error, AOM_CODEC_ERROR,
                           "Failed to encode to the target level %d_%d. %s",
                           target_level_major, target_level_minor,
                           level_fail_messages[fail_id]);
      }
    }
  }
}

aom_codec_err_t av1_get_seq_level_idx(const SequenceHeader *seq_params,
                                      const AV1LevelParams *level_params,
                                      int *seq_level_idx) {
  const int is_still_picture = seq_params->still_picture;
  const BITSTREAM_PROFILE profile = seq_params->profile;
  for (int op = 0; op < seq_params->operating_points_cnt_minus_1 + 1; ++op) {
    seq_level_idx[op] = (int)SEQ_LEVEL_MAX;
    if (!((level_params->keep_level_stats >> op) & 1)) continue;
    const int tier = seq_params->tier[op];
    const AV1LevelInfo *const level_info = level_params->level_info[op];
    assert(level_info != NULL);
    for (int level = 0; level < SEQ_LEVELS; ++level) {
      if (!is_valid_seq_level_idx(level)) continue;
      const TARGET_LEVEL_FAIL_ID fail_id = check_level_constraints(
          level_info, level, tier, is_still_picture, profile, 1);
      if (fail_id == TARGET_LEVEL_OK) {
        seq_level_idx[op] = level;
        break;
      }
    }
  }

  return AOM_CODEC_OK;
}

aom_codec_err_t av1_get_target_seq_level_idx(const SequenceHeader *seq_params,
                                             const AV1LevelParams *level_params,
                                             int *target_seq_level_idx) {
  for (int op = 0; op < seq_params->operating_points_cnt_minus_1 + 1; ++op) {
    target_seq_level_idx[op] = (int)SEQ_LEVEL_MAX;
    if (!((level_params->keep_level_stats >> op) & 1)) continue;
    target_seq_level_idx[op] = level_params->target_seq_level_idx[op];
  }

  return AOM_CODEC_OK;
}
