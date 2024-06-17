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

#include <assert.h>
#include <limits.h>
#include <stdio.h>

#include "config/av1_rtcd.h"
#include "config/aom_dsp_rtcd.h"
#include "config/aom_scale_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/aom_timer.h"
#include "aom_scale/aom_scale.h"
#include "aom_util/aom_pthread.h"
#include "aom_util/aom_thread.h"

#include "av1/common/alloccommon.h"
#include "av1/common/av1_common_int.h"
#include "av1/common/av1_loopfilter.h"
#include "av1/common/quant_common.h"
#include "av1/common/reconinter.h"
#include "av1/common/reconintra.h"

#include "av1/decoder/decodeframe.h"
#include "av1/decoder/decoder.h"
#include "av1/decoder/detokenize.h"
#include "av1/decoder/obu.h"

static void initialize_dec(void) {
  av1_rtcd();
  aom_dsp_rtcd();
  aom_scale_rtcd();
  av1_init_intra_predictors();
  av1_init_wedge_masks();
}

static void dec_set_mb_mi(CommonModeInfoParams *mi_params, int width,
                          int height, BLOCK_SIZE min_partition_size) {
  (void)min_partition_size;
  // Ensure that the decoded width and height are both multiples of
  // 8 luma pixels (note: this may only be a multiple of 4 chroma pixels if
  // subsampling is used).
  // This simplifies the implementation of various experiments,
  // eg. cdef, which operates on units of 8x8 luma pixels.
  const int aligned_width = ALIGN_POWER_OF_TWO(width, 3);
  const int aligned_height = ALIGN_POWER_OF_TWO(height, 3);

  mi_params->mi_cols = aligned_width >> MI_SIZE_LOG2;
  mi_params->mi_rows = aligned_height >> MI_SIZE_LOG2;
  mi_params->mi_stride = calc_mi_size(mi_params->mi_cols);

  mi_params->mb_cols = ROUND_POWER_OF_TWO(mi_params->mi_cols, 2);
  mi_params->mb_rows = ROUND_POWER_OF_TWO(mi_params->mi_rows, 2);
  mi_params->MBs = mi_params->mb_rows * mi_params->mb_cols;

  mi_params->mi_alloc_bsize = BLOCK_4X4;
  mi_params->mi_alloc_stride = mi_params->mi_stride;

  assert(mi_size_wide[mi_params->mi_alloc_bsize] ==
         mi_size_high[mi_params->mi_alloc_bsize]);
}

static void dec_setup_mi(CommonModeInfoParams *mi_params) {
  const int mi_grid_size =
      mi_params->mi_stride * calc_mi_size(mi_params->mi_rows);
  memset(mi_params->mi_grid_base, 0,
         mi_grid_size * sizeof(*mi_params->mi_grid_base));
}

static void dec_free_mi(CommonModeInfoParams *mi_params) {
  aom_free(mi_params->mi_alloc);
  mi_params->mi_alloc = NULL;
  mi_params->mi_alloc_size = 0;
  aom_free(mi_params->mi_grid_base);
  mi_params->mi_grid_base = NULL;
  mi_params->mi_grid_size = 0;
  aom_free(mi_params->tx_type_map);
  mi_params->tx_type_map = NULL;
}

AV1Decoder *av1_decoder_create(BufferPool *const pool) {
  AV1Decoder *volatile const pbi = aom_memalign(32, sizeof(*pbi));
  if (!pbi) return NULL;
  av1_zero(*pbi);

  AV1_COMMON *volatile const cm = &pbi->common;
  cm->seq_params = &pbi->seq_params;
  cm->error = &pbi->error;

  // The jmp_buf is valid only for the duration of the function that calls
  // setjmp(). Therefore, this function must reset the 'setjmp' field to 0
  // before it returns.
  if (setjmp(pbi->error.jmp)) {
    pbi->error.setjmp = 0;
    av1_decoder_remove(pbi);
    return NULL;
  }

  pbi->error.setjmp = 1;

  CHECK_MEM_ERROR(cm, cm->fc,
                  (FRAME_CONTEXT *)aom_memalign(32, sizeof(*cm->fc)));
  CHECK_MEM_ERROR(
      cm, cm->default_frame_context,
      (FRAME_CONTEXT *)aom_memalign(32, sizeof(*cm->default_frame_context)));
  memset(cm->fc, 0, sizeof(*cm->fc));
  memset(cm->default_frame_context, 0, sizeof(*cm->default_frame_context));

  pbi->need_resync = 1;
  initialize_dec();

  // Initialize the references to not point to any frame buffers.
  for (int i = 0; i < REF_FRAMES; i++) {
    cm->ref_frame_map[i] = NULL;
  }

  cm->current_frame.frame_number = 0;
  pbi->decoding_first_frame = 1;
  pbi->common.buffer_pool = pool;

  cm->seq_params->bit_depth = AOM_BITS_8;

  cm->mi_params.free_mi = dec_free_mi;
  cm->mi_params.setup_mi = dec_setup_mi;
  cm->mi_params.set_mb_mi = dec_set_mb_mi;

  av1_loop_filter_init(cm);

  av1_qm_init(&cm->quant_params, av1_num_planes(cm));
  av1_loop_restoration_precal();

#if CONFIG_ACCOUNTING
  pbi->acct_enabled = 1;
  aom_accounting_init(&pbi->accounting);
#endif

  pbi->error.setjmp = 0;

  aom_get_worker_interface()->init(&pbi->lf_worker);
  pbi->lf_worker.thread_name = "aom lf worker";

  return pbi;
}

void av1_dealloc_dec_jobs(struct AV1DecTileMTData *tile_mt_info) {
  if (tile_mt_info != NULL) {
#if CONFIG_MULTITHREAD
    if (tile_mt_info->job_mutex != NULL) {
      pthread_mutex_destroy(tile_mt_info->job_mutex);
      aom_free(tile_mt_info->job_mutex);
    }
#endif
    aom_free(tile_mt_info->job_queue);
    // clear the structure as the source of this call may be a resize in which
    // case this call will be followed by an _alloc() which may fail.
    av1_zero(*tile_mt_info);
  }
}

void av1_dec_free_cb_buf(AV1Decoder *pbi) {
  aom_free(pbi->cb_buffer_base);
  pbi->cb_buffer_base = NULL;
  pbi->cb_buffer_alloc_size = 0;
}

void av1_decoder_remove(AV1Decoder *pbi) {
  int i;

  if (!pbi) return;

  // Free the tile list output buffer.
  aom_free_frame_buffer(&pbi->tile_list_outbuf);

  aom_get_worker_interface()->end(&pbi->lf_worker);
  aom_free(pbi->lf_worker.data1);

  if (pbi->thread_data) {
    for (int worker_idx = 1; worker_idx < pbi->num_workers; worker_idx++) {
      DecWorkerData *const thread_data = pbi->thread_data + worker_idx;
      if (thread_data->td != NULL) {
        av1_free_mc_tmp_buf(thread_data->td);
        aom_free(thread_data->td);
      }
    }
    aom_free(pbi->thread_data);
  }
  aom_free(pbi->dcb.xd.seg_mask);

  for (i = 0; i < pbi->num_workers; ++i) {
    AVxWorker *const worker = &pbi->tile_workers[i];
    aom_get_worker_interface()->end(worker);
  }
#if CONFIG_MULTITHREAD
  if (pbi->row_mt_mutex_ != NULL) {
    pthread_mutex_destroy(pbi->row_mt_mutex_);
    aom_free(pbi->row_mt_mutex_);
  }
  if (pbi->row_mt_cond_ != NULL) {
    pthread_cond_destroy(pbi->row_mt_cond_);
    aom_free(pbi->row_mt_cond_);
  }
#endif
  for (i = 0; i < pbi->allocated_tiles; i++) {
    TileDataDec *const tile_data = pbi->tile_data + i;
    av1_dec_row_mt_dealloc(&tile_data->dec_row_mt_sync);
  }
  aom_free(pbi->tile_data);
  aom_free(pbi->tile_workers);

  if (pbi->num_workers > 0) {
    av1_loop_filter_dealloc(&pbi->lf_row_sync);
    av1_loop_restoration_dealloc(&pbi->lr_row_sync);
    av1_dealloc_dec_jobs(&pbi->tile_mt_info);
  }

  av1_dec_free_cb_buf(pbi);
#if CONFIG_ACCOUNTING
  aom_accounting_clear(&pbi->accounting);
#endif
  av1_free_mc_tmp_buf(&pbi->td);
  aom_img_metadata_array_free(pbi->metadata);
  av1_remove_common(&pbi->common);
  aom_free(pbi);
}

void av1_visit_palette(AV1Decoder *const pbi, MACROBLOCKD *const xd,
                       aom_reader *r, palette_visitor_fn_t visit) {
  if (!is_inter_block(xd->mi[0])) {
    for (int plane = 0; plane < AOMMIN(2, av1_num_planes(&pbi->common));
         ++plane) {
      if (plane == 0 || xd->is_chroma_ref) {
        if (xd->mi[0]->palette_mode_info.palette_size[plane])
          visit(xd, plane, r);
      } else {
        assert(xd->mi[0]->palette_mode_info.palette_size[plane] == 0);
      }
    }
  }
}

static int equal_dimensions(const YV12_BUFFER_CONFIG *a,
                            const YV12_BUFFER_CONFIG *b) {
  return a->y_height == b->y_height && a->y_width == b->y_width &&
         a->uv_height == b->uv_height && a->uv_width == b->uv_width;
}

aom_codec_err_t av1_copy_reference_dec(AV1Decoder *pbi, int idx,
                                       YV12_BUFFER_CONFIG *sd) {
  AV1_COMMON *cm = &pbi->common;
  const int num_planes = av1_num_planes(cm);

  const YV12_BUFFER_CONFIG *const cfg = get_ref_frame(cm, idx);
  if (cfg == NULL) {
    aom_internal_error(&pbi->error, AOM_CODEC_ERROR, "No reference frame");
    return AOM_CODEC_ERROR;
  }
  if (!equal_dimensions(cfg, sd))
    aom_internal_error(&pbi->error, AOM_CODEC_ERROR,
                       "Incorrect buffer dimensions");
  else
    aom_yv12_copy_frame(cfg, sd, num_planes);

  return pbi->error.error_code;
}

static int equal_dimensions_and_border(const YV12_BUFFER_CONFIG *a,
                                       const YV12_BUFFER_CONFIG *b) {
  return a->y_height == b->y_height && a->y_width == b->y_width &&
         a->uv_height == b->uv_height && a->uv_width == b->uv_width &&
         a->y_stride == b->y_stride && a->uv_stride == b->uv_stride &&
         a->border == b->border &&
         (a->flags & YV12_FLAG_HIGHBITDEPTH) ==
             (b->flags & YV12_FLAG_HIGHBITDEPTH);
}

aom_codec_err_t av1_set_reference_dec(AV1_COMMON *cm, int idx,
                                      int use_external_ref,
                                      YV12_BUFFER_CONFIG *sd) {
  const int num_planes = av1_num_planes(cm);
  YV12_BUFFER_CONFIG *ref_buf = NULL;

  // Get the destination reference buffer.
  ref_buf = get_ref_frame(cm, idx);

  if (ref_buf == NULL) {
    aom_internal_error(cm->error, AOM_CODEC_ERROR, "No reference frame");
    return AOM_CODEC_ERROR;
  }

  if (!use_external_ref) {
    if (!equal_dimensions(ref_buf, sd)) {
      aom_internal_error(cm->error, AOM_CODEC_ERROR,
                         "Incorrect buffer dimensions");
    } else {
      // Overwrite the reference frame buffer.
      aom_yv12_copy_frame(sd, ref_buf, num_planes);
    }
  } else {
    if (!equal_dimensions_and_border(ref_buf, sd)) {
      aom_internal_error(cm->error, AOM_CODEC_ERROR,
                         "Incorrect buffer dimensions");
    } else {
      // Overwrite the reference frame buffer pointers.
      // Once we no longer need the external reference buffer, these pointers
      // are restored.
      ref_buf->store_buf_adr[0] = ref_buf->y_buffer;
      ref_buf->store_buf_adr[1] = ref_buf->u_buffer;
      ref_buf->store_buf_adr[2] = ref_buf->v_buffer;
      ref_buf->y_buffer = sd->y_buffer;
      ref_buf->u_buffer = sd->u_buffer;
      ref_buf->v_buffer = sd->v_buffer;
      ref_buf->use_external_reference_buffers = 1;
    }
  }

  return cm->error->error_code;
}

aom_codec_err_t av1_copy_new_frame_dec(AV1_COMMON *cm,
                                       YV12_BUFFER_CONFIG *new_frame,
                                       YV12_BUFFER_CONFIG *sd) {
  const int num_planes = av1_num_planes(cm);

  if (!equal_dimensions_and_border(new_frame, sd))
    aom_internal_error(cm->error, AOM_CODEC_ERROR,
                       "Incorrect buffer dimensions");
  else
    aom_yv12_copy_frame(new_frame, sd, num_planes);

  return cm->error->error_code;
}

static void release_current_frame(AV1Decoder *pbi) {
  AV1_COMMON *const cm = &pbi->common;
  BufferPool *const pool = cm->buffer_pool;

  cm->cur_frame->buf.corrupted = 1;
  lock_buffer_pool(pool);
  decrease_ref_count(cm->cur_frame, pool);
  unlock_buffer_pool(pool);
  cm->cur_frame = NULL;
}

// If any buffer updating is signaled it should be done here.
// Consumes a reference to cm->cur_frame.
//
// This functions returns void. It reports failure by setting
// pbi->error.error_code.
static void update_frame_buffers(AV1Decoder *pbi, int frame_decoded) {
  int ref_index = 0, mask;
  AV1_COMMON *const cm = &pbi->common;
  BufferPool *const pool = cm->buffer_pool;

  if (frame_decoded) {
    lock_buffer_pool(pool);

    // In ext-tile decoding, the camera frame header is only decoded once. So,
    // we don't update the references here.
    if (!pbi->camera_frame_header_ready) {
      // The following for loop needs to release the reference stored in
      // cm->ref_frame_map[ref_index] before storing a reference to
      // cm->cur_frame in cm->ref_frame_map[ref_index].
      for (mask = cm->current_frame.refresh_frame_flags; mask; mask >>= 1) {
        if (mask & 1) {
          decrease_ref_count(cm->ref_frame_map[ref_index], pool);
          cm->ref_frame_map[ref_index] = cm->cur_frame;
          ++cm->cur_frame->ref_count;
        }
        ++ref_index;
      }
    }

    if (cm->show_existing_frame || cm->show_frame) {
      if (pbi->output_all_layers) {
        // Append this frame to the output queue
        if (pbi->num_output_frames >= MAX_NUM_SPATIAL_LAYERS) {
          // We can't store the new frame anywhere, so drop it and return an
          // error
          cm->cur_frame->buf.corrupted = 1;
          decrease_ref_count(cm->cur_frame, pool);
          pbi->error.error_code = AOM_CODEC_UNSUP_BITSTREAM;
        } else {
          pbi->output_frames[pbi->num_output_frames] = cm->cur_frame;
          pbi->num_output_frames++;
        }
      } else {
        // Replace any existing output frame
        assert(pbi->num_output_frames == 0 || pbi->num_output_frames == 1);
        if (pbi->num_output_frames > 0) {
          decrease_ref_count(pbi->output_frames[0], pool);
        }
        pbi->output_frames[0] = cm->cur_frame;
        pbi->num_output_frames = 1;
      }
    } else {
      decrease_ref_count(cm->cur_frame, pool);
    }

    unlock_buffer_pool(pool);
  } else {
    // Nothing was decoded, so just drop this frame buffer
    lock_buffer_pool(pool);
    decrease_ref_count(cm->cur_frame, pool);
    unlock_buffer_pool(pool);
  }
  cm->cur_frame = NULL;

  if (!pbi->camera_frame_header_ready) {
    // Invalidate these references until the next frame starts.
    for (ref_index = 0; ref_index < INTER_REFS_PER_FRAME; ref_index++) {
      cm->remapped_ref_idx[ref_index] = INVALID_IDX;
    }
  }
}

int av1_receive_compressed_data(AV1Decoder *pbi, size_t size,
                                const uint8_t **psource) {
  AV1_COMMON *volatile const cm = &pbi->common;
  const uint8_t *source = *psource;
  pbi->error.error_code = AOM_CODEC_OK;
  pbi->error.has_detail = 0;

  if (size == 0) {
    // This is used to signal that we are missing frames.
    // We do not know if the missing frame(s) was supposed to update
    // any of the reference buffers, but we act conservative and
    // mark only the last buffer as corrupted.
    //
    // TODO(jkoleszar): Error concealment is undefined and non-normative
    // at this point, but if it becomes so, [0] may not always be the correct
    // thing to do here.
    RefCntBuffer *ref_buf = get_ref_frame_buf(cm, LAST_FRAME);
    if (ref_buf != NULL) ref_buf->buf.corrupted = 1;
  }

  if (assign_cur_frame_new_fb(cm) == NULL) {
    pbi->error.error_code = AOM_CODEC_MEM_ERROR;
    return 1;
  }

  // The jmp_buf is valid only for the duration of the function that calls
  // setjmp(). Therefore, this function must reset the 'setjmp' field to 0
  // before it returns.
  if (setjmp(pbi->error.jmp)) {
    const AVxWorkerInterface *const winterface = aom_get_worker_interface();
    int i;

    pbi->error.setjmp = 0;

    // Synchronize all threads immediately as a subsequent decode call may
    // cause a resize invalidating some allocations.
    winterface->sync(&pbi->lf_worker);
    for (i = 0; i < pbi->num_workers; ++i) {
      winterface->sync(&pbi->tile_workers[i]);
    }

    release_current_frame(pbi);
    return -1;
  }

  pbi->error.setjmp = 1;

  int frame_decoded =
      aom_decode_frame_from_obus(pbi, source, source + size, psource);

  if (frame_decoded < 0) {
    assert(pbi->error.error_code != AOM_CODEC_OK);
    release_current_frame(pbi);
    pbi->error.setjmp = 0;
    return 1;
  }

#if TXCOEFF_TIMER
  cm->cum_txcoeff_timer += cm->txcoeff_timer;
  fprintf(stderr,
          "txb coeff block number: %d, frame time: %ld, cum time %ld in us\n",
          cm->txb_count, cm->txcoeff_timer, cm->cum_txcoeff_timer);
  cm->txcoeff_timer = 0;
  cm->txb_count = 0;
#endif

  // Note: At this point, this function holds a reference to cm->cur_frame
  // in the buffer pool. This reference is consumed by update_frame_buffers().
  update_frame_buffers(pbi, frame_decoded);

  if (frame_decoded) {
    pbi->decoding_first_frame = 0;
  }

  if (pbi->error.error_code != AOM_CODEC_OK) {
    pbi->error.setjmp = 0;
    return 1;
  }

  if (!cm->show_existing_frame) {
    if (cm->seg.enabled) {
      if (cm->prev_frame &&
          (cm->mi_params.mi_rows == cm->prev_frame->mi_rows) &&
          (cm->mi_params.mi_cols == cm->prev_frame->mi_cols)) {
        cm->last_frame_seg_map = cm->prev_frame->seg_map;
      } else {
        cm->last_frame_seg_map = NULL;
      }
    }
  }

  // Update progress in frame parallel decode.
  pbi->error.setjmp = 0;

  return 0;
}

// Get the frame at a particular index in the output queue
int av1_get_raw_frame(AV1Decoder *pbi, size_t index, YV12_BUFFER_CONFIG **sd,
                      aom_film_grain_t **grain_params) {
  if (index >= pbi->num_output_frames) return -1;
  *sd = &pbi->output_frames[index]->buf;
  *grain_params = &pbi->output_frames[index]->film_grain_params;
  return 0;
}

// Get the highest-spatial-layer output
// TODO(rachelbarker): What should this do?
int av1_get_frame_to_show(AV1Decoder *pbi, YV12_BUFFER_CONFIG *frame) {
  if (pbi->num_output_frames == 0) return -1;

  *frame = pbi->output_frames[pbi->num_output_frames - 1]->buf;
  return 0;
}
