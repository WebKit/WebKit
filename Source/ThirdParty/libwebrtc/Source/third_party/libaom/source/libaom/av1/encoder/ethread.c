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

#include "av1/common/warped_motion.h"
#include "av1/common/thread_common.h"

#include "av1/encoder/bitstream.h"
#include "av1/encoder/encodeframe.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/encoder_alloc.h"
#include "av1/encoder/encodeframe_utils.h"
#include "av1/encoder/ethread.h"
#if !CONFIG_REALTIME_ONLY
#include "av1/encoder/firstpass.h"
#endif
#include "av1/encoder/global_motion.h"
#include "av1/encoder/global_motion_facade.h"
#include "av1/encoder/intra_mode_search_utils.h"
#include "av1/encoder/rdopt.h"
#include "aom_dsp/aom_dsp_common.h"
#include "av1/encoder/temporal_filter.h"
#include "av1/encoder/tpl_model.h"

static AOM_INLINE void accumulate_rd_opt(ThreadData *td, ThreadData *td_t) {
  td->rd_counts.compound_ref_used_flag |=
      td_t->rd_counts.compound_ref_used_flag;
  td->rd_counts.skip_mode_used_flag |= td_t->rd_counts.skip_mode_used_flag;

  for (int i = 0; i < TX_SIZES_ALL; i++) {
    for (int j = 0; j < TX_TYPES; j++)
      td->rd_counts.tx_type_used[i][j] += td_t->rd_counts.tx_type_used[i][j];
  }

  for (int i = 0; i < BLOCK_SIZES_ALL; i++) {
    for (int j = 0; j < 2; j++) {
      td->rd_counts.obmc_used[i][j] += td_t->rd_counts.obmc_used[i][j];
    }
  }

  for (int i = 0; i < 2; i++) {
    td->rd_counts.warped_used[i] += td_t->rd_counts.warped_used[i];
  }

  td->rd_counts.seg_tmp_pred_cost[0] += td_t->rd_counts.seg_tmp_pred_cost[0];
  td->rd_counts.seg_tmp_pred_cost[1] += td_t->rd_counts.seg_tmp_pred_cost[1];

  td->rd_counts.newmv_or_intra_blocks += td_t->rd_counts.newmv_or_intra_blocks;
}

static AOM_INLINE void update_delta_lf_for_row_mt(AV1_COMP *cpi) {
  AV1_COMMON *cm = &cpi->common;
  MACROBLOCKD *xd = &cpi->td.mb.e_mbd;
  const int mib_size = cm->seq_params->mib_size;
  const int frame_lf_count =
      av1_num_planes(cm) > 1 ? FRAME_LF_COUNT : FRAME_LF_COUNT - 2;
  for (int row = 0; row < cm->tiles.rows; row++) {
    for (int col = 0; col < cm->tiles.cols; col++) {
      TileDataEnc *tile_data = &cpi->tile_data[row * cm->tiles.cols + col];
      const TileInfo *const tile_info = &tile_data->tile_info;
      for (int mi_row = tile_info->mi_row_start; mi_row < tile_info->mi_row_end;
           mi_row += mib_size) {
        if (mi_row == tile_info->mi_row_start)
          av1_reset_loop_filter_delta(xd, av1_num_planes(cm));
        for (int mi_col = tile_info->mi_col_start;
             mi_col < tile_info->mi_col_end; mi_col += mib_size) {
          const int idx_str = cm->mi_params.mi_stride * mi_row + mi_col;
          MB_MODE_INFO **mi = cm->mi_params.mi_grid_base + idx_str;
          MB_MODE_INFO *mbmi = mi[0];
          if (mbmi->skip_txfm == 1 &&
              (mbmi->bsize == cm->seq_params->sb_size)) {
            for (int lf_id = 0; lf_id < frame_lf_count; ++lf_id)
              mbmi->delta_lf[lf_id] = xd->delta_lf[lf_id];
            mbmi->delta_lf_from_base = xd->delta_lf_from_base;
          } else {
            if (cm->delta_q_info.delta_lf_multi) {
              for (int lf_id = 0; lf_id < frame_lf_count; ++lf_id)
                xd->delta_lf[lf_id] = mbmi->delta_lf[lf_id];
            } else {
              xd->delta_lf_from_base = mbmi->delta_lf_from_base;
            }
          }
        }
      }
    }
  }
}

void av1_row_mt_sync_read_dummy(AV1EncRowMultiThreadSync *row_mt_sync, int r,
                                int c) {
  (void)row_mt_sync;
  (void)r;
  (void)c;
  return;
}

void av1_row_mt_sync_write_dummy(AV1EncRowMultiThreadSync *row_mt_sync, int r,
                                 int c, int cols) {
  (void)row_mt_sync;
  (void)r;
  (void)c;
  (void)cols;
  return;
}

void av1_row_mt_sync_read(AV1EncRowMultiThreadSync *row_mt_sync, int r, int c) {
#if CONFIG_MULTITHREAD
  const int nsync = row_mt_sync->sync_range;

  if (r) {
    pthread_mutex_t *const mutex = &row_mt_sync->mutex_[r - 1];
    pthread_mutex_lock(mutex);

    while (c > row_mt_sync->num_finished_cols[r - 1] - nsync -
                   row_mt_sync->intrabc_extra_top_right_sb_delay) {
      pthread_cond_wait(&row_mt_sync->cond_[r - 1], mutex);
    }
    pthread_mutex_unlock(mutex);
  }
#else
  (void)row_mt_sync;
  (void)r;
  (void)c;
#endif  // CONFIG_MULTITHREAD
}

void av1_row_mt_sync_write(AV1EncRowMultiThreadSync *row_mt_sync, int r, int c,
                           int cols) {
#if CONFIG_MULTITHREAD
  const int nsync = row_mt_sync->sync_range;
  int cur;
  // Only signal when there are enough encoded blocks for next row to run.
  int sig = 1;

  if (c < cols - 1) {
    cur = c;
    if (c % nsync) sig = 0;
  } else {
    cur = cols + nsync + row_mt_sync->intrabc_extra_top_right_sb_delay;
  }

  if (sig) {
    pthread_mutex_lock(&row_mt_sync->mutex_[r]);

    row_mt_sync->num_finished_cols[r] = cur;

    pthread_cond_signal(&row_mt_sync->cond_[r]);
    pthread_mutex_unlock(&row_mt_sync->mutex_[r]);
  }
#else
  (void)row_mt_sync;
  (void)r;
  (void)c;
  (void)cols;
#endif  // CONFIG_MULTITHREAD
}

// Allocate memory for row synchronization
static void row_mt_sync_mem_alloc(AV1EncRowMultiThreadSync *row_mt_sync,
                                  AV1_COMMON *cm, int rows) {
#if CONFIG_MULTITHREAD
  int i;

  CHECK_MEM_ERROR(cm, row_mt_sync->mutex_,
                  aom_malloc(sizeof(*row_mt_sync->mutex_) * rows));
  if (row_mt_sync->mutex_) {
    for (i = 0; i < rows; ++i) {
      pthread_mutex_init(&row_mt_sync->mutex_[i], NULL);
    }
  }

  CHECK_MEM_ERROR(cm, row_mt_sync->cond_,
                  aom_malloc(sizeof(*row_mt_sync->cond_) * rows));
  if (row_mt_sync->cond_) {
    for (i = 0; i < rows; ++i) {
      pthread_cond_init(&row_mt_sync->cond_[i], NULL);
    }
  }
#endif  // CONFIG_MULTITHREAD

  CHECK_MEM_ERROR(cm, row_mt_sync->num_finished_cols,
                  aom_malloc(sizeof(*row_mt_sync->num_finished_cols) * rows));

  row_mt_sync->rows = rows;
  // Set up nsync.
  row_mt_sync->sync_range = 1;
}

// Deallocate row based multi-threading synchronization related mutex and data
static void row_mt_sync_mem_dealloc(AV1EncRowMultiThreadSync *row_mt_sync) {
  if (row_mt_sync != NULL) {
#if CONFIG_MULTITHREAD
    int i;

    if (row_mt_sync->mutex_ != NULL) {
      for (i = 0; i < row_mt_sync->rows; ++i) {
        pthread_mutex_destroy(&row_mt_sync->mutex_[i]);
      }
      aom_free(row_mt_sync->mutex_);
    }
    if (row_mt_sync->cond_ != NULL) {
      for (i = 0; i < row_mt_sync->rows; ++i) {
        pthread_cond_destroy(&row_mt_sync->cond_[i]);
      }
      aom_free(row_mt_sync->cond_);
    }
#endif  // CONFIG_MULTITHREAD
    aom_free(row_mt_sync->num_finished_cols);

    // clear the structure as the source of this call may be dynamic change
    // in tiles in which case this call will be followed by an _alloc()
    // which may fail.
    av1_zero(*row_mt_sync);
  }
}

static void row_mt_mem_alloc(AV1_COMP *cpi, int max_rows, int max_cols,
                             int alloc_row_ctx) {
  struct AV1Common *cm = &cpi->common;
  AV1EncRowMultiThreadInfo *const enc_row_mt = &cpi->mt_info.enc_row_mt;
  const int tile_cols = cm->tiles.cols;
  const int tile_rows = cm->tiles.rows;
  int tile_col, tile_row;

  // Allocate memory for row based multi-threading
  for (tile_row = 0; tile_row < tile_rows; tile_row++) {
    for (tile_col = 0; tile_col < tile_cols; tile_col++) {
      int tile_index = tile_row * tile_cols + tile_col;
      TileDataEnc *const this_tile = &cpi->tile_data[tile_index];

      row_mt_sync_mem_alloc(&this_tile->row_mt_sync, cm, max_rows);

      this_tile->row_ctx = NULL;
      if (alloc_row_ctx) {
        assert(max_cols > 0);
        const int num_row_ctx = AOMMAX(1, (max_cols - 1));
        CHECK_MEM_ERROR(cm, this_tile->row_ctx,
                        (FRAME_CONTEXT *)aom_memalign(
                            16, num_row_ctx * sizeof(*this_tile->row_ctx)));
      }
    }
  }
  enc_row_mt->allocated_tile_cols = tile_cols;
  enc_row_mt->allocated_tile_rows = tile_rows;
  enc_row_mt->allocated_rows = max_rows;
  enc_row_mt->allocated_cols = max_cols - 1;
}

void av1_row_mt_mem_dealloc(AV1_COMP *cpi) {
  AV1EncRowMultiThreadInfo *const enc_row_mt = &cpi->mt_info.enc_row_mt;
  const int tile_cols = enc_row_mt->allocated_tile_cols;
  const int tile_rows = enc_row_mt->allocated_tile_rows;
  int tile_col, tile_row;

  // Free row based multi-threading sync memory
  for (tile_row = 0; tile_row < tile_rows; tile_row++) {
    for (tile_col = 0; tile_col < tile_cols; tile_col++) {
      int tile_index = tile_row * tile_cols + tile_col;
      TileDataEnc *const this_tile = &cpi->tile_data[tile_index];

      row_mt_sync_mem_dealloc(&this_tile->row_mt_sync);

      if (cpi->oxcf.algo_cfg.cdf_update_mode) aom_free(this_tile->row_ctx);
    }
  }
  enc_row_mt->allocated_rows = 0;
  enc_row_mt->allocated_cols = 0;
  enc_row_mt->allocated_tile_cols = 0;
  enc_row_mt->allocated_tile_rows = 0;
}

static AOM_INLINE void assign_tile_to_thread(int *thread_id_to_tile_id,
                                             int num_tiles, int num_workers) {
  int tile_id = 0;
  int i;

  for (i = 0; i < num_workers; i++) {
    thread_id_to_tile_id[i] = tile_id++;
    if (tile_id == num_tiles) tile_id = 0;
  }
}

static AOM_INLINE int get_next_job(TileDataEnc *const tile_data,
                                   int *current_mi_row, int mib_size) {
  AV1EncRowMultiThreadSync *const row_mt_sync = &tile_data->row_mt_sync;
  const int mi_row_end = tile_data->tile_info.mi_row_end;

  if (row_mt_sync->next_mi_row < mi_row_end) {
    *current_mi_row = row_mt_sync->next_mi_row;
    row_mt_sync->num_threads_working++;
    row_mt_sync->next_mi_row += mib_size;
    return 1;
  }
  return 0;
}

static AOM_INLINE void switch_tile_and_get_next_job(
    AV1_COMMON *const cm, TileDataEnc *const tile_data, int *cur_tile_id,
    int *current_mi_row, int *end_of_frame, int is_firstpass,
    const BLOCK_SIZE fp_block_size) {
  const int tile_cols = cm->tiles.cols;
  const int tile_rows = cm->tiles.rows;

  int tile_id = -1;  // Stores the tile ID with minimum proc done
  int max_mis_to_encode = 0;
  int min_num_threads_working = INT_MAX;

  for (int tile_row = 0; tile_row < tile_rows; tile_row++) {
    for (int tile_col = 0; tile_col < tile_cols; tile_col++) {
      int tile_index = tile_row * tile_cols + tile_col;
      TileDataEnc *const this_tile = &tile_data[tile_index];
      AV1EncRowMultiThreadSync *const row_mt_sync = &this_tile->row_mt_sync;

#if CONFIG_REALTIME_ONLY
      int num_b_rows_in_tile =
          av1_get_sb_rows_in_tile(cm, &this_tile->tile_info);
      int num_b_cols_in_tile =
          av1_get_sb_cols_in_tile(cm, &this_tile->tile_info);
#else
      int num_b_rows_in_tile =
          is_firstpass
              ? av1_get_unit_rows_in_tile(&this_tile->tile_info, fp_block_size)
              : av1_get_sb_rows_in_tile(cm, &this_tile->tile_info);
      int num_b_cols_in_tile =
          is_firstpass
              ? av1_get_unit_cols_in_tile(&this_tile->tile_info, fp_block_size)
              : av1_get_sb_cols_in_tile(cm, &this_tile->tile_info);
#endif
      int theoretical_limit_on_threads =
          AOMMIN((num_b_cols_in_tile + 1) >> 1, num_b_rows_in_tile);
      int num_threads_working = row_mt_sync->num_threads_working;

      if (num_threads_working < theoretical_limit_on_threads) {
        int num_mis_to_encode =
            this_tile->tile_info.mi_row_end - row_mt_sync->next_mi_row;

        // Tile to be processed by this thread is selected on the basis of
        // availability of jobs:
        // 1) If jobs are available, tile to be processed is chosen on the
        // basis of minimum number of threads working for that tile. If two or
        // more tiles have same number of threads working for them, then the
        // tile with maximum number of jobs available will be chosen.
        // 2) If no jobs are available, then end_of_frame is reached.
        if (num_mis_to_encode > 0) {
          if (num_threads_working < min_num_threads_working) {
            min_num_threads_working = num_threads_working;
            max_mis_to_encode = 0;
          }
          if (num_threads_working == min_num_threads_working &&
              num_mis_to_encode > max_mis_to_encode) {
            tile_id = tile_index;
            max_mis_to_encode = num_mis_to_encode;
          }
        }
      }
    }
  }
  if (tile_id == -1) {
    *end_of_frame = 1;
  } else {
    // Update the current tile id to the tile id that will be processed next,
    // which will be the least processed tile.
    *cur_tile_id = tile_id;
    const int unit_height = mi_size_high[fp_block_size];
    get_next_job(&tile_data[tile_id], current_mi_row,
                 is_firstpass ? unit_height : cm->seq_params->mib_size);
  }
}

#if !CONFIG_REALTIME_ONLY
static int fp_enc_row_mt_worker_hook(void *arg1, void *unused) {
  EncWorkerData *const thread_data = (EncWorkerData *)arg1;
  AV1_COMP *const cpi = thread_data->cpi;
  AV1_COMMON *const cm = &cpi->common;
  int thread_id = thread_data->thread_id;
  AV1EncRowMultiThreadInfo *const enc_row_mt = &cpi->mt_info.enc_row_mt;
  int cur_tile_id = enc_row_mt->thread_id_to_tile_id[thread_id];
#if CONFIG_MULTITHREAD
  pthread_mutex_t *enc_row_mt_mutex_ = enc_row_mt->mutex_;
#endif
  (void)unused;

  assert(cur_tile_id != -1);

  const BLOCK_SIZE fp_block_size = cpi->fp_block_size;
  const int unit_height = mi_size_high[fp_block_size];
  int end_of_frame = 0;
  while (1) {
    int current_mi_row = -1;
#if CONFIG_MULTITHREAD
    pthread_mutex_lock(enc_row_mt_mutex_);
#endif
    if (!get_next_job(&cpi->tile_data[cur_tile_id], &current_mi_row,
                      unit_height)) {
      // No jobs are available for the current tile. Query for the status of
      // other tiles and get the next job if available
      switch_tile_and_get_next_job(cm, cpi->tile_data, &cur_tile_id,
                                   &current_mi_row, &end_of_frame, 1,
                                   fp_block_size);
    }
#if CONFIG_MULTITHREAD
    pthread_mutex_unlock(enc_row_mt_mutex_);
#endif
    if (end_of_frame == 1) break;

    TileDataEnc *const this_tile = &cpi->tile_data[cur_tile_id];
    AV1EncRowMultiThreadSync *const row_mt_sync = &this_tile->row_mt_sync;
    ThreadData *td = thread_data->td;

    assert(current_mi_row != -1 &&
           current_mi_row < this_tile->tile_info.mi_row_end);

    const int unit_height_log2 = mi_size_high_log2[fp_block_size];
    av1_first_pass_row(cpi, td, this_tile, current_mi_row >> unit_height_log2,
                       fp_block_size);
#if CONFIG_MULTITHREAD
    pthread_mutex_lock(enc_row_mt_mutex_);
#endif
    row_mt_sync->num_threads_working--;
#if CONFIG_MULTITHREAD
    pthread_mutex_unlock(enc_row_mt_mutex_);
#endif
  }

  return 1;
}
#endif

static int enc_row_mt_worker_hook(void *arg1, void *unused) {
  EncWorkerData *const thread_data = (EncWorkerData *)arg1;
  AV1_COMP *const cpi = thread_data->cpi;
  AV1_COMMON *const cm = &cpi->common;
  int thread_id = thread_data->thread_id;
  AV1EncRowMultiThreadInfo *const enc_row_mt = &cpi->mt_info.enc_row_mt;
  int cur_tile_id = enc_row_mt->thread_id_to_tile_id[thread_id];
#if CONFIG_MULTITHREAD
  pthread_mutex_t *enc_row_mt_mutex_ = enc_row_mt->mutex_;
#endif
  (void)unused;

  assert(cur_tile_id != -1);

  const BLOCK_SIZE fp_block_size = cpi->fp_block_size;
  int end_of_frame = 0;

  // When master thread does not have a valid job to process, xd->tile_ctx
  // is not set and it contains NULL pointer. This can result in NULL pointer
  // access violation if accessed beyond the encode stage. Hence, updating
  // thread_data->td->mb.e_mbd.tile_ctx is initialized with common frame
  // context to avoid NULL pointer access in subsequent stages.
  thread_data->td->mb.e_mbd.tile_ctx = cm->fc;
  while (1) {
    int current_mi_row = -1;
#if CONFIG_MULTITHREAD
    pthread_mutex_lock(enc_row_mt_mutex_);
#endif
    if (!get_next_job(&cpi->tile_data[cur_tile_id], &current_mi_row,
                      cm->seq_params->mib_size)) {
      // No jobs are available for the current tile. Query for the status of
      // other tiles and get the next job if available
      switch_tile_and_get_next_job(cm, cpi->tile_data, &cur_tile_id,
                                   &current_mi_row, &end_of_frame, 0,
                                   fp_block_size);
    }
#if CONFIG_MULTITHREAD
    pthread_mutex_unlock(enc_row_mt_mutex_);
#endif
    if (end_of_frame == 1) break;

    TileDataEnc *const this_tile = &cpi->tile_data[cur_tile_id];
    AV1EncRowMultiThreadSync *const row_mt_sync = &this_tile->row_mt_sync;
    const TileInfo *const tile_info = &this_tile->tile_info;
    const int tile_row = tile_info->tile_row;
    const int tile_col = tile_info->tile_col;
    ThreadData *td = thread_data->td;

    assert(current_mi_row != -1 && current_mi_row <= tile_info->mi_row_end);

    td->mb.e_mbd.tile_ctx = td->tctx;
    td->mb.tile_pb_ctx = &this_tile->tctx;
    td->abs_sum_level = 0;

    if (this_tile->allow_update_cdf) {
      td->mb.row_ctx = this_tile->row_ctx;
      if (current_mi_row == tile_info->mi_row_start)
        memcpy(td->mb.e_mbd.tile_ctx, &this_tile->tctx, sizeof(FRAME_CONTEXT));
    } else {
      memcpy(td->mb.e_mbd.tile_ctx, &this_tile->tctx, sizeof(FRAME_CONTEXT));
    }

    av1_init_above_context(&cm->above_contexts, av1_num_planes(cm), tile_row,
                           &td->mb.e_mbd);

    cfl_init(&td->mb.e_mbd.cfl, cm->seq_params);
    if (td->mb.txfm_search_info.mb_rd_record != NULL) {
      av1_crc32c_calculator_init(
          &td->mb.txfm_search_info.mb_rd_record->crc_calculator);
    }

    av1_encode_sb_row(cpi, td, tile_row, tile_col, current_mi_row);
#if CONFIG_MULTITHREAD
    pthread_mutex_lock(enc_row_mt_mutex_);
#endif
    this_tile->abs_sum_level += td->abs_sum_level;
    row_mt_sync->num_threads_working--;
#if CONFIG_MULTITHREAD
    pthread_mutex_unlock(enc_row_mt_mutex_);
#endif
  }

  return 1;
}

static int enc_worker_hook(void *arg1, void *unused) {
  EncWorkerData *const thread_data = (EncWorkerData *)arg1;
  AV1_COMP *const cpi = thread_data->cpi;
  const AV1_COMMON *const cm = &cpi->common;
  const int tile_cols = cm->tiles.cols;
  const int tile_rows = cm->tiles.rows;
  int t;

  (void)unused;

  for (t = thread_data->start; t < tile_rows * tile_cols;
       t += cpi->mt_info.num_workers) {
    int tile_row = t / tile_cols;
    int tile_col = t % tile_cols;

    TileDataEnc *const this_tile =
        &cpi->tile_data[tile_row * cm->tiles.cols + tile_col];
    thread_data->td->mb.e_mbd.tile_ctx = &this_tile->tctx;
    thread_data->td->mb.tile_pb_ctx = &this_tile->tctx;
    av1_encode_tile(cpi, thread_data->td, tile_row, tile_col);
  }

  return 1;
}

void av1_init_frame_mt(AV1_PRIMARY *ppi, AV1_COMP *cpi) {
  cpi->mt_info.workers = ppi->p_mt_info.workers;
  cpi->mt_info.num_workers = ppi->p_mt_info.num_workers;
  cpi->mt_info.tile_thr_data = ppi->p_mt_info.tile_thr_data;
  int i;
  for (i = MOD_FP; i < NUM_MT_MODULES; i++) {
    cpi->mt_info.num_mod_workers[i] =
        AOMMIN(cpi->mt_info.num_workers, ppi->p_mt_info.num_mod_workers[i]);
  }
}

void av1_init_cdef_worker(AV1_COMP *cpi) {
  // The allocation is done only for level 0 parallel frames. No change
  // in config is supported in the middle of a parallel encode set, since the
  // rest of the MT modules also do not support dynamic change of config.
  if (cpi->ppi->gf_group.frame_parallel_level[cpi->gf_frame_index] > 0) return;
  PrimaryMultiThreadInfo *const p_mt_info = &cpi->ppi->p_mt_info;
  int num_cdef_workers = av1_get_num_mod_workers_for_alloc(p_mt_info, MOD_CDEF);

  av1_alloc_cdef_buffers(&cpi->common, &p_mt_info->cdef_worker,
                         &cpi->mt_info.cdef_sync, num_cdef_workers, 1);
  cpi->mt_info.cdef_worker = p_mt_info->cdef_worker;
}

#if !CONFIG_REALTIME_ONLY
void av1_init_lr_mt_buffers(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  AV1LrSync *lr_sync = &cpi->mt_info.lr_row_sync;
  if (lr_sync->sync_range) {
    int num_lr_workers =
        av1_get_num_mod_workers_for_alloc(&cpi->ppi->p_mt_info, MOD_LR);
    if (cpi->ppi->gf_group.frame_parallel_level[cpi->gf_frame_index] > 0)
      return;
    lr_sync->lrworkerdata[num_lr_workers - 1].rst_tmpbuf = cm->rst_tmpbuf;
    lr_sync->lrworkerdata[num_lr_workers - 1].rlbs = cm->rlbs;
  }
}
#endif

#if CONFIG_MULTITHREAD
void av1_init_mt_sync(AV1_COMP *cpi, int is_first_pass) {
  AV1_COMMON *const cm = &cpi->common;
  MultiThreadInfo *const mt_info = &cpi->mt_info;

  // Initialize enc row MT object.
  if (is_first_pass || cpi->oxcf.row_mt == 1) {
    AV1EncRowMultiThreadInfo *enc_row_mt = &mt_info->enc_row_mt;
    if (enc_row_mt->mutex_ == NULL) {
      CHECK_MEM_ERROR(cm, enc_row_mt->mutex_,
                      aom_malloc(sizeof(*(enc_row_mt->mutex_))));
      if (enc_row_mt->mutex_) pthread_mutex_init(enc_row_mt->mutex_, NULL);
    }
  }

  if (!is_first_pass) {
    // Initialize global motion MT object.
    AV1GlobalMotionSync *gm_sync = &mt_info->gm_sync;
    if (gm_sync->mutex_ == NULL) {
      CHECK_MEM_ERROR(cm, gm_sync->mutex_,
                      aom_malloc(sizeof(*(gm_sync->mutex_))));
      if (gm_sync->mutex_) pthread_mutex_init(gm_sync->mutex_, NULL);
    }
#if !CONFIG_REALTIME_ONLY
    // Initialize temporal filtering MT object.
    AV1TemporalFilterSync *tf_sync = &mt_info->tf_sync;
    if (tf_sync->mutex_ == NULL) {
      CHECK_MEM_ERROR(cm, tf_sync->mutex_,
                      aom_malloc(sizeof(*tf_sync->mutex_)));
      if (tf_sync->mutex_) pthread_mutex_init(tf_sync->mutex_, NULL);
    }
#endif  // !CONFIG_REALTIME_ONLY
        // Initialize CDEF MT object.
    AV1CdefSync *cdef_sync = &mt_info->cdef_sync;
    if (cdef_sync->mutex_ == NULL) {
      CHECK_MEM_ERROR(cm, cdef_sync->mutex_,
                      aom_malloc(sizeof(*(cdef_sync->mutex_))));
      if (cdef_sync->mutex_) pthread_mutex_init(cdef_sync->mutex_, NULL);
    }

    // Initialize loop filter MT object.
    AV1LfSync *lf_sync = &mt_info->lf_row_sync;
    // Number of superblock rows
    const int sb_rows =
        CEIL_POWER_OF_TWO(cm->height >> MI_SIZE_LOG2, MAX_MIB_SIZE_LOG2);
    PrimaryMultiThreadInfo *const p_mt_info = &cpi->ppi->p_mt_info;
    int num_lf_workers = av1_get_num_mod_workers_for_alloc(p_mt_info, MOD_LPF);

    if (!lf_sync->sync_range || sb_rows != lf_sync->rows ||
        num_lf_workers > lf_sync->num_workers) {
      av1_loop_filter_dealloc(lf_sync);
      av1_loop_filter_alloc(lf_sync, cm, sb_rows, cm->width, num_lf_workers);
    }

#if !CONFIG_REALTIME_ONLY
    if (is_restoration_used(cm)) {
      // Initialize loop restoration MT object.
      AV1LrSync *lr_sync = &mt_info->lr_row_sync;
      int rst_unit_size;
      if (cm->width * cm->height > 352 * 288)
        rst_unit_size = RESTORATION_UNITSIZE_MAX;
      else
        rst_unit_size = (RESTORATION_UNITSIZE_MAX >> 1);
      int num_rows_lr = av1_lr_count_units_in_tile(rst_unit_size, cm->height);
      int num_lr_workers = av1_get_num_mod_workers_for_alloc(p_mt_info, MOD_LR);
      if (!lr_sync->sync_range || num_rows_lr > lr_sync->rows ||
          num_lr_workers > lr_sync->num_workers ||
          MAX_MB_PLANE > lr_sync->num_planes) {
        av1_loop_restoration_dealloc(lr_sync, num_lr_workers);
        av1_loop_restoration_alloc(lr_sync, cm, num_lr_workers, num_rows_lr,
                                   MAX_MB_PLANE, cm->width);
      }
    }
#endif

    // Initialization of pack bitstream MT object.
    AV1EncPackBSSync *pack_bs_sync = &mt_info->pack_bs_sync;
    if (pack_bs_sync->mutex_ == NULL) {
      CHECK_MEM_ERROR(cm, pack_bs_sync->mutex_,
                      aom_malloc(sizeof(*pack_bs_sync->mutex_)));
      if (pack_bs_sync->mutex_) pthread_mutex_init(pack_bs_sync->mutex_, NULL);
    }
  }
}
#endif  // CONFIG_MULTITHREAD

// Computes the number of workers to be considered while allocating memory for a
// multi-threaded module under FPMT.
int av1_get_num_mod_workers_for_alloc(PrimaryMultiThreadInfo *const p_mt_info,
                                      MULTI_THREADED_MODULES mod_name) {
  int num_mod_workers = p_mt_info->num_mod_workers[mod_name];
  if (p_mt_info->num_mod_workers[MOD_FRAME_ENC] > 1) {
    // TODO(anyone): Change num_mod_workers to num_mod_workers[MOD_FRAME_ENC].
    // As frame parallel jobs will only perform multi-threading for the encode
    // stage, we can limit the allocations according to num_enc_workers per
    // frame parallel encode(a.k.a num_mod_workers[MOD_FRAME_ENC]).
    num_mod_workers = p_mt_info->num_workers;
  }
  return num_mod_workers;
}

void av1_init_tile_thread_data(AV1_PRIMARY *ppi, int is_first_pass) {
  PrimaryMultiThreadInfo *const p_mt_info = &ppi->p_mt_info;

  assert(p_mt_info->workers != NULL);
  assert(p_mt_info->tile_thr_data != NULL);

  int num_workers = p_mt_info->num_workers;
  int num_enc_workers = av1_get_num_mod_workers_for_alloc(p_mt_info, MOD_ENC);
  for (int i = num_workers - 1; i >= 0; i--) {
    EncWorkerData *const thread_data = &p_mt_info->tile_thr_data[i];

    if (i > 0) {
      // Allocate thread data.
      AOM_CHECK_MEM_ERROR(&ppi->error, thread_data->td,
                          aom_memalign(32, sizeof(*thread_data->td)));
      av1_zero(*thread_data->td);
      thread_data->original_td = thread_data->td;

      // Set up shared coeff buffers.
      av1_setup_shared_coeff_buffer(
          &ppi->seq_params, &thread_data->td->shared_coeff_buf, &ppi->error);
      AOM_CHECK_MEM_ERROR(
          &ppi->error, thread_data->td->tmp_conv_dst,
          aom_memalign(32, MAX_SB_SIZE * MAX_SB_SIZE *
                               sizeof(*thread_data->td->tmp_conv_dst)));

      if (i < p_mt_info->num_mod_workers[MOD_FP]) {
        // Set up firstpass PICK_MODE_CONTEXT.
        thread_data->td->firstpass_ctx = av1_alloc_pmc(
            ppi->cpi, BLOCK_16X16, &thread_data->td->shared_coeff_buf);
      }

      if (!is_first_pass && i < num_enc_workers) {
        // Set up sms_tree.
        av1_setup_sms_tree(ppi->cpi, thread_data->td);

        for (int x = 0; x < 2; x++)
          for (int y = 0; y < 2; y++)
            AOM_CHECK_MEM_ERROR(
                &ppi->error, thread_data->td->hash_value_buffer[x][y],
                (uint32_t *)aom_malloc(
                    AOM_BUFFER_SIZE_FOR_BLOCK_HASH *
                    sizeof(*thread_data->td->hash_value_buffer[0][0])));

        // Allocate frame counters in thread data.
        AOM_CHECK_MEM_ERROR(&ppi->error, thread_data->td->counts,
                            aom_calloc(1, sizeof(*thread_data->td->counts)));

        // Allocate buffers used by palette coding mode.
        AOM_CHECK_MEM_ERROR(
            &ppi->error, thread_data->td->palette_buffer,
            aom_memalign(16, sizeof(*thread_data->td->palette_buffer)));

        // The buffers 'tmp_pred_bufs[]', 'comp_rd_buffer' and 'obmc_buffer' are
        // used in inter frames to store intermediate inter mode prediction
        // results and are not required for allintra encoding mode. Hence, the
        // memory allocations for these buffers are avoided for allintra
        // encoding mode.
        if (ppi->cpi->oxcf.kf_cfg.key_freq_max != 0) {
          alloc_obmc_buffers(&thread_data->td->obmc_buffer, &ppi->error);

          alloc_compound_type_rd_buffers(&ppi->error,
                                         &thread_data->td->comp_rd_buffer);

          for (int j = 0; j < 2; ++j) {
            AOM_CHECK_MEM_ERROR(
                &ppi->error, thread_data->td->tmp_pred_bufs[j],
                aom_memalign(32,
                             2 * MAX_MB_PLANE * MAX_SB_SQUARE *
                                 sizeof(*thread_data->td->tmp_pred_bufs[j])));
          }
        }

        if (is_gradient_caching_for_hog_enabled(ppi->cpi)) {
          const int plane_types = PLANE_TYPES >> ppi->seq_params.monochrome;
          AOM_CHECK_MEM_ERROR(
              &ppi->error, thread_data->td->pixel_gradient_info,
              aom_malloc(sizeof(*thread_data->td->pixel_gradient_info) *
                         plane_types * MAX_SB_SQUARE));
        }

        if (is_src_var_for_4x4_sub_blocks_caching_enabled(ppi->cpi)) {
          const BLOCK_SIZE sb_size = ppi->cpi->common.seq_params->sb_size;
          const int mi_count_in_sb =
              mi_size_wide[sb_size] * mi_size_high[sb_size];

          AOM_CHECK_MEM_ERROR(
              &ppi->error, thread_data->td->src_var_info_of_4x4_sub_blocks,
              aom_malloc(
                  sizeof(*thread_data->td->src_var_info_of_4x4_sub_blocks) *
                  mi_count_in_sb));
        }

        if (ppi->cpi->sf.part_sf.partition_search_type == VAR_BASED_PARTITION) {
          const int num_64x64_blocks =
              (ppi->seq_params.sb_size == BLOCK_64X64) ? 1 : 4;
          AOM_CHECK_MEM_ERROR(
              &ppi->error, thread_data->td->vt64x64,
              aom_malloc(sizeof(*thread_data->td->vt64x64) * num_64x64_blocks));
        }
      }
    }

    if (!is_first_pass && ppi->cpi->oxcf.row_mt == 1 && i < num_enc_workers) {
      if (i == 0) {
        for (int j = 0; j < ppi->num_fp_contexts; j++) {
          AOM_CHECK_MEM_ERROR(&ppi->error, ppi->parallel_cpi[j]->td.tctx,
                              (FRAME_CONTEXT *)aom_memalign(
                                  16, sizeof(*ppi->parallel_cpi[j]->td.tctx)));
        }
      } else {
        AOM_CHECK_MEM_ERROR(
            &ppi->error, thread_data->td->tctx,
            (FRAME_CONTEXT *)aom_memalign(16, sizeof(*thread_data->td->tctx)));
      }
    }
  }
}

void av1_create_workers(AV1_PRIMARY *ppi, int num_workers) {
  PrimaryMultiThreadInfo *const p_mt_info = &ppi->p_mt_info;
  const AVxWorkerInterface *const winterface = aom_get_worker_interface();

  AOM_CHECK_MEM_ERROR(&ppi->error, p_mt_info->workers,
                      aom_malloc(num_workers * sizeof(*p_mt_info->workers)));

  AOM_CHECK_MEM_ERROR(
      &ppi->error, p_mt_info->tile_thr_data,
      aom_calloc(num_workers, sizeof(*p_mt_info->tile_thr_data)));

  for (int i = num_workers - 1; i >= 0; i--) {
    AVxWorker *const worker = &p_mt_info->workers[i];
    EncWorkerData *const thread_data = &p_mt_info->tile_thr_data[i];

    winterface->init(worker);
    worker->thread_name = "aom enc worker";

    thread_data->thread_id = i;
    // Set the starting tile for each thread.
    thread_data->start = i;

    if (i > 0) {
      // Create threads
      if (!winterface->reset(worker))
        aom_internal_error(&ppi->error, AOM_CODEC_ERROR,
                           "Tile encoder thread creation failed");
    }
    winterface->sync(worker);

    ++p_mt_info->num_workers;
  }
}

// This function returns 1 if frame parallel encode is supported for
// the current configuration. Returns 0 otherwise.
static AOM_INLINE int is_fpmt_config(AV1_PRIMARY *ppi, AV1EncoderConfig *oxcf) {
  // FPMT is enabled for AOM_Q and AOM_VBR.
  // TODO(Tarun): Test and enable resize config.
  if (oxcf->rc_cfg.mode == AOM_CBR || oxcf->rc_cfg.mode == AOM_CQ) {
    return 0;
  }
  if (ppi->use_svc) {
    return 0;
  }
  if (oxcf->tile_cfg.enable_large_scale_tile) {
    return 0;
  }
  if (oxcf->dec_model_cfg.timing_info_present) {
    return 0;
  }
  if (oxcf->mode != GOOD) {
    return 0;
  }
  if (oxcf->tool_cfg.error_resilient_mode) {
    return 0;
  }
  if (oxcf->resize_cfg.resize_mode) {
    return 0;
  }
  if (oxcf->pass != AOM_RC_SECOND_PASS) {
    return 0;
  }
  if (oxcf->max_threads < 2) {
    return 0;
  }
  if (!oxcf->fp_mt) {
    return 0;
  }

  return 1;
}

int av1_check_fpmt_config(AV1_PRIMARY *const ppi,
                          AV1EncoderConfig *const oxcf) {
  if (is_fpmt_config(ppi, oxcf)) return 1;
  // Reset frame parallel configuration for unsupported config
  if (ppi->num_fp_contexts > 1) {
    for (int i = 1; i < ppi->num_fp_contexts; i++) {
      // Release the previously-used frame-buffer
      if (ppi->parallel_cpi[i]->common.cur_frame != NULL) {
        --ppi->parallel_cpi[i]->common.cur_frame->ref_count;
        ppi->parallel_cpi[i]->common.cur_frame = NULL;
      }
    }

    int cur_gf_index = ppi->cpi->gf_frame_index;
    int reset_size = AOMMAX(0, ppi->gf_group.size - cur_gf_index);
    av1_zero_array(&ppi->gf_group.frame_parallel_level[cur_gf_index],
                   reset_size);
    av1_zero_array(&ppi->gf_group.is_frame_non_ref[cur_gf_index], reset_size);
    av1_zero_array(&ppi->gf_group.src_offset[cur_gf_index], reset_size);
    memset(&ppi->gf_group.skip_frame_refresh[cur_gf_index][0], INVALID_IDX,
           sizeof(ppi->gf_group.skip_frame_refresh[cur_gf_index][0]) *
               reset_size * REF_FRAMES);
    memset(&ppi->gf_group.skip_frame_as_ref[cur_gf_index], INVALID_IDX,
           sizeof(ppi->gf_group.skip_frame_as_ref[cur_gf_index]) * reset_size);
    ppi->num_fp_contexts = 1;
  }
  return 0;
}

// A large value for threads used to compute the max num_enc_workers
// possible for each resolution.
#define MAX_THREADS 100

// Computes the max number of enc workers possible for each resolution.
static AOM_INLINE int compute_max_num_enc_workers(
    CommonModeInfoParams *const mi_params, int mib_size_log2) {
  int num_sb_rows = CEIL_POWER_OF_TWO(mi_params->mi_rows, mib_size_log2);
  int num_sb_cols = CEIL_POWER_OF_TWO(mi_params->mi_cols, mib_size_log2);

  return AOMMIN((num_sb_cols + 1) >> 1, num_sb_rows);
}

// Computes the number of frame parallel(fp) contexts to be created
// based on the number of max_enc_workers.
int av1_compute_num_fp_contexts(AV1_PRIMARY *ppi, AV1EncoderConfig *oxcf) {
  ppi->p_mt_info.num_mod_workers[MOD_FRAME_ENC] = 0;
  if (!av1_check_fpmt_config(ppi, oxcf)) {
    return 1;
  }
  int max_num_enc_workers = compute_max_num_enc_workers(
      &ppi->cpi->common.mi_params, ppi->cpi->common.seq_params->mib_size_log2);
  // Scaling factors and rounding factors used to tune worker_per_frame
  // computation.
  int rounding_factor[2] = { 2, 4 };
  int scaling_factor[2] = { 4, 8 };
  int is_480p_or_lesser =
      AOMMIN(oxcf->frm_dim_cfg.width, oxcf->frm_dim_cfg.height) <= 480;
  int is_sb_64 = 0;
  if (ppi->cpi != NULL)
    is_sb_64 = ppi->cpi->common.seq_params->sb_size == BLOCK_64X64;
  // A parallel frame encode has at least 1/4th the
  // theoretical limit of max enc workers in default case. For resolutions
  // larger than 480p, if SB size is 64x64, optimal performance is obtained with
  // limit of 1/8.
  int index = (!is_480p_or_lesser && is_sb_64) ? 1 : 0;
  int workers_per_frame =
      AOMMAX(1, (max_num_enc_workers + rounding_factor[index]) /
                    scaling_factor[index]);
  int max_threads = oxcf->max_threads;
  int num_fp_contexts = max_threads / workers_per_frame;
  // Based on empirical results, FPMT gains with multi-tile are significant when
  // more parallel frames are available. Use FPMT with multi-tile encode only
  // when sufficient threads are available for parallel encode of
  // MAX_PARALLEL_FRAMES frames.
  if (oxcf->tile_cfg.tile_columns > 0 || oxcf->tile_cfg.tile_rows > 0) {
    if (num_fp_contexts < MAX_PARALLEL_FRAMES) num_fp_contexts = 1;
  }

  num_fp_contexts = AOMMAX(1, AOMMIN(num_fp_contexts, MAX_PARALLEL_FRAMES));
  // Limit recalculated num_fp_contexts to ppi->num_fp_contexts.
  num_fp_contexts = (ppi->num_fp_contexts == 1)
                        ? num_fp_contexts
                        : AOMMIN(num_fp_contexts, ppi->num_fp_contexts);
  if (num_fp_contexts > 1) {
    ppi->p_mt_info.num_mod_workers[MOD_FRAME_ENC] =
        AOMMIN(max_num_enc_workers * num_fp_contexts, oxcf->max_threads);
  }
  return num_fp_contexts;
}

// Computes the number of workers to process each of the parallel frames.
static AOM_INLINE int compute_num_workers_per_frame(
    const int num_workers, const int parallel_frame_count) {
  // Number of level 2 workers per frame context (floor division).
  int workers_per_frame = (num_workers / parallel_frame_count);
  return workers_per_frame;
}

// Prepare level 1 workers. This function is only called for
// parallel_frame_count > 1. This function populates the mt_info structure of
// frame level contexts appropriately by dividing the total number of available
// workers amongst the frames as level 2 workers. It also populates the hook and
// data members of level 1 workers.
static AOM_INLINE void prepare_fpmt_workers(AV1_PRIMARY *ppi,
                                            AV1_COMP_DATA *first_cpi_data,
                                            AVxWorkerHook hook,
                                            int parallel_frame_count) {
  assert(parallel_frame_count <= ppi->num_fp_contexts &&
         parallel_frame_count > 1);

  PrimaryMultiThreadInfo *const p_mt_info = &ppi->p_mt_info;
  int num_workers = p_mt_info->num_workers;

  int frame_idx = 0;
  int i = 0;
  while (i < num_workers) {
    // Assign level 1 worker
    AVxWorker *frame_worker = p_mt_info->p_workers[frame_idx] =
        &p_mt_info->workers[i];
    AV1_COMP *cur_cpi = ppi->parallel_cpi[frame_idx];
    MultiThreadInfo *mt_info = &cur_cpi->mt_info;
    AV1_COMMON *const cm = &cur_cpi->common;
    const int num_planes = av1_num_planes(cm);

    // Assign start of level 2 worker pool
    mt_info->workers = &p_mt_info->workers[i];
    mt_info->tile_thr_data = &p_mt_info->tile_thr_data[i];
    // Assign number of workers for each frame in the parallel encode set.
    mt_info->num_workers = compute_num_workers_per_frame(
        num_workers - i, parallel_frame_count - frame_idx);
    for (int j = MOD_FP; j < NUM_MT_MODULES; j++) {
      mt_info->num_mod_workers[j] =
          AOMMIN(mt_info->num_workers, ppi->p_mt_info.num_mod_workers[j]);
    }
    if (ppi->p_mt_info.cdef_worker != NULL) {
      mt_info->cdef_worker = &ppi->p_mt_info.cdef_worker[i];

      // Back up the original cdef_worker pointers.
      mt_info->restore_state_buf.cdef_srcbuf = mt_info->cdef_worker->srcbuf;
      for (int plane = 0; plane < num_planes; plane++)
        mt_info->restore_state_buf.cdef_colbuf[plane] =
            mt_info->cdef_worker->colbuf[plane];
    }
#if !CONFIG_REALTIME_ONLY
    if (is_restoration_used(cm)) {
      // Back up the original LR buffers before update.
      int idx = i + mt_info->num_workers - 1;
      mt_info->restore_state_buf.rst_tmpbuf =
          mt_info->lr_row_sync.lrworkerdata[idx].rst_tmpbuf;
      mt_info->restore_state_buf.rlbs =
          mt_info->lr_row_sync.lrworkerdata[idx].rlbs;

      // Update LR buffers.
      mt_info->lr_row_sync.lrworkerdata[idx].rst_tmpbuf = cm->rst_tmpbuf;
      mt_info->lr_row_sync.lrworkerdata[idx].rlbs = cm->rlbs;
    }
#endif

    // At this stage, the thread specific CDEF buffers for the current frame's
    // 'common' and 'cdef_sync' only need to be allocated. 'cdef_worker' has
    // already been allocated across parallel frames.
    av1_alloc_cdef_buffers(cm, &p_mt_info->cdef_worker, &mt_info->cdef_sync,
                           p_mt_info->num_workers, 0);

    frame_worker->hook = hook;
    frame_worker->data1 = cur_cpi;
    frame_worker->data2 = (frame_idx == 0)
                              ? first_cpi_data
                              : &ppi->parallel_frames_data[frame_idx - 1];
    frame_idx++;
    i += mt_info->num_workers;
  }
  p_mt_info->p_num_workers = parallel_frame_count;
}

// Launch level 1 workers to perform frame parallel encode.
static AOM_INLINE void launch_fpmt_workers(AV1_PRIMARY *ppi) {
  const AVxWorkerInterface *const winterface = aom_get_worker_interface();
  int num_workers = ppi->p_mt_info.p_num_workers;

  for (int i = num_workers - 1; i >= 0; i--) {
    AVxWorker *const worker = ppi->p_mt_info.p_workers[i];
    if (i == 0)
      winterface->execute(worker);
    else
      winterface->launch(worker);
  }
}

// Synchronize level 1 workers.
static AOM_INLINE void sync_fpmt_workers(AV1_PRIMARY *ppi) {
  const AVxWorkerInterface *const winterface = aom_get_worker_interface();
  int num_workers = ppi->p_mt_info.p_num_workers;
  int had_error = 0;
  // Points to error in the earliest display order frame in the parallel set.
  const struct aom_internal_error_info *error;

  // Encoding ends.
  for (int i = num_workers - 1; i >= 0; i--) {
    AVxWorker *const worker = ppi->p_mt_info.p_workers[i];
    if (!winterface->sync(worker)) {
      had_error = 1;
      error = ((AV1_COMP *)worker->data1)->common.error;
    }
  }

  if (had_error)
    aom_internal_error(&ppi->error, error->error_code, "%s", error->detail);
}

// Restore worker states after parallel encode.
static AOM_INLINE void restore_workers_after_fpmt(AV1_PRIMARY *ppi,
                                                  int parallel_frame_count) {
  assert(parallel_frame_count <= ppi->num_fp_contexts &&
         parallel_frame_count > 1);
  (void)parallel_frame_count;

  PrimaryMultiThreadInfo *const p_mt_info = &ppi->p_mt_info;
  int num_workers = p_mt_info->num_workers;

  int frame_idx = 0;
  int i = 0;
  while (i < num_workers) {
    AV1_COMP *cur_cpi = ppi->parallel_cpi[frame_idx];
    MultiThreadInfo *mt_info = &cur_cpi->mt_info;
    const AV1_COMMON *const cm = &cur_cpi->common;
    const int num_planes = av1_num_planes(cm);

    // Restore the original cdef_worker pointers.
    if (ppi->p_mt_info.cdef_worker != NULL) {
      mt_info->cdef_worker->srcbuf = mt_info->restore_state_buf.cdef_srcbuf;
      for (int plane = 0; plane < num_planes; plane++)
        mt_info->cdef_worker->colbuf[plane] =
            mt_info->restore_state_buf.cdef_colbuf[plane];
    }
#if !CONFIG_REALTIME_ONLY
    if (is_restoration_used(cm)) {
      // Restore the original LR buffers.
      int idx = i + mt_info->num_workers - 1;
      mt_info->lr_row_sync.lrworkerdata[idx].rst_tmpbuf =
          mt_info->restore_state_buf.rst_tmpbuf;
      mt_info->lr_row_sync.lrworkerdata[idx].rlbs =
          mt_info->restore_state_buf.rlbs;
    }
#endif

    frame_idx++;
    i += mt_info->num_workers;
  }
}

static int get_compressed_data_hook(void *arg1, void *arg2) {
  AV1_COMP *cpi = (AV1_COMP *)arg1;
  AV1_COMP_DATA *cpi_data = (AV1_COMP_DATA *)arg2;
  int status = av1_get_compressed_data(cpi, cpi_data);

  // AOM_CODEC_OK(0) means no error.
  return !status;
}

// This function encodes the raw frame data for each frame in parallel encode
// set, and outputs the frame bit stream to the designated buffers.
int av1_compress_parallel_frames(AV1_PRIMARY *const ppi,
                                 AV1_COMP_DATA *const first_cpi_data) {
  // Bitmask for the frame buffers referenced by cpi->scaled_ref_buf
  // corresponding to frames in the current parallel encode set.
  int ref_buffers_used_map = 0;
  int frames_in_parallel_set = av1_init_parallel_frame_context(
      first_cpi_data, ppi, &ref_buffers_used_map);
  prepare_fpmt_workers(ppi, first_cpi_data, get_compressed_data_hook,
                       frames_in_parallel_set);
  launch_fpmt_workers(ppi);
  sync_fpmt_workers(ppi);
  restore_workers_after_fpmt(ppi, frames_in_parallel_set);

  // Release cpi->scaled_ref_buf corresponding to frames in the current parallel
  // encode set.
  for (int i = 0; i < frames_in_parallel_set; ++i) {
    av1_release_scaled_references_fpmt(ppi->parallel_cpi[i]);
  }
  av1_decrement_ref_counts_fpmt(ppi->cpi->common.buffer_pool,
                                ref_buffers_used_map);
  return AOM_CODEC_OK;
}

static AOM_INLINE void launch_workers(MultiThreadInfo *const mt_info,
                                      int num_workers) {
  const AVxWorkerInterface *const winterface = aom_get_worker_interface();
  for (int i = num_workers - 1; i >= 0; i--) {
    AVxWorker *const worker = &mt_info->workers[i];
    if (i == 0)
      winterface->execute(worker);
    else
      winterface->launch(worker);
  }
}

static AOM_INLINE void sync_enc_workers(MultiThreadInfo *const mt_info,
                                        AV1_COMMON *const cm, int num_workers) {
  const AVxWorkerInterface *const winterface = aom_get_worker_interface();
  int had_error = 0;

  // Encoding ends.
  for (int i = num_workers - 1; i > 0; i--) {
    AVxWorker *const worker = &mt_info->workers[i];
    had_error |= !winterface->sync(worker);
  }

  if (had_error)
    aom_internal_error(cm->error, AOM_CODEC_ERROR,
                       "Failed to encode tile data");
}

static AOM_INLINE void accumulate_counters_enc_workers(AV1_COMP *cpi,
                                                       int num_workers) {
  for (int i = num_workers - 1; i >= 0; i--) {
    AVxWorker *const worker = &cpi->mt_info.workers[i];
    EncWorkerData *const thread_data = (EncWorkerData *)worker->data1;
    cpi->intrabc_used |= thread_data->td->intrabc_used;
    cpi->deltaq_used |= thread_data->td->deltaq_used;
    // Accumulate rtc counters.
    if (!frame_is_intra_only(&cpi->common))
      av1_accumulate_rtc_counters(cpi, &thread_data->td->mb);
    if (thread_data->td != &cpi->td) {
      // Keep these conditional expressions in sync with the corresponding ones
      // in prepare_enc_workers().
      if (cpi->sf.inter_sf.mv_cost_upd_level != INTERNAL_COST_UPD_OFF) {
        aom_free(thread_data->td->mb.mv_costs);
      }
      if (cpi->sf.intra_sf.dv_cost_upd_level != INTERNAL_COST_UPD_OFF) {
        aom_free(thread_data->td->mb.dv_costs);
      }
    }
    av1_dealloc_mb_data(&cpi->common, &thread_data->td->mb);

    // Accumulate counters.
    if (i > 0) {
      av1_accumulate_frame_counts(&cpi->counts, thread_data->td->counts);
      accumulate_rd_opt(&cpi->td, thread_data->td);
      cpi->td.mb.txfm_search_info.txb_split_count +=
          thread_data->td->mb.txfm_search_info.txb_split_count;
#if CONFIG_SPEED_STATS
      cpi->td.mb.txfm_search_info.tx_search_count +=
          thread_data->td->mb.txfm_search_info.tx_search_count;
#endif  // CONFIG_SPEED_STATS
    }

    av1_free_pc_tree_recursive(thread_data->td->rt_pc_root,
                               av1_num_planes(&cpi->common), 0, 0);
  }
}

static AOM_INLINE void prepare_enc_workers(AV1_COMP *cpi, AVxWorkerHook hook,
                                           int num_workers) {
  MultiThreadInfo *const mt_info = &cpi->mt_info;
  AV1_COMMON *const cm = &cpi->common;
  for (int i = num_workers - 1; i >= 0; i--) {
    AVxWorker *const worker = &mt_info->workers[i];
    EncWorkerData *const thread_data = &mt_info->tile_thr_data[i];

    worker->hook = hook;
    worker->data1 = thread_data;
    worker->data2 = NULL;

    thread_data->thread_id = i;
    // Set the starting tile for each thread.
    thread_data->start = i;

    thread_data->cpi = cpi;
    if (i == 0) {
      thread_data->td = &cpi->td;
    } else {
      thread_data->td = thread_data->original_td;
    }

    thread_data->td->intrabc_used = 0;
    thread_data->td->deltaq_used = 0;
    thread_data->td->abs_sum_level = 0;
    thread_data->td->rd_counts.seg_tmp_pred_cost[0] = 0;
    thread_data->td->rd_counts.seg_tmp_pred_cost[1] = 0;

    // Before encoding a frame, copy the thread data from cpi.
    if (thread_data->td != &cpi->td) {
      thread_data->td->mb = cpi->td.mb;
      thread_data->td->rd_counts = cpi->td.rd_counts;
      thread_data->td->mb.obmc_buffer = thread_data->td->obmc_buffer;

      for (int x = 0; x < 2; x++) {
        for (int y = 0; y < 2; y++) {
          memcpy(thread_data->td->hash_value_buffer[x][y],
                 cpi->td.mb.intrabc_hash_info.hash_value_buffer[x][y],
                 AOM_BUFFER_SIZE_FOR_BLOCK_HASH *
                     sizeof(*thread_data->td->hash_value_buffer[0][0]));
          thread_data->td->mb.intrabc_hash_info.hash_value_buffer[x][y] =
              thread_data->td->hash_value_buffer[x][y];
        }
      }
      // Keep these conditional expressions in sync with the corresponding ones
      // in accumulate_counters_enc_workers().
      if (cpi->sf.inter_sf.mv_cost_upd_level != INTERNAL_COST_UPD_OFF) {
        CHECK_MEM_ERROR(cm, thread_data->td->mb.mv_costs,
                        (MvCosts *)aom_malloc(sizeof(MvCosts)));
        memcpy(thread_data->td->mb.mv_costs, cpi->td.mb.mv_costs,
               sizeof(MvCosts));
      }
      if (cpi->sf.intra_sf.dv_cost_upd_level != INTERNAL_COST_UPD_OFF) {
        // Reset dv_costs to NULL for worker threads when dv cost update is
        // enabled so that only dv_cost_upd_level needs to be checked before the
        // aom_free() call for the same.
        thread_data->td->mb.dv_costs = NULL;
        if (av1_need_dv_costs(cpi)) {
          CHECK_MEM_ERROR(cm, thread_data->td->mb.dv_costs,
                          (IntraBCMVCosts *)aom_malloc(sizeof(IntraBCMVCosts)));
          memcpy(thread_data->td->mb.dv_costs, cpi->td.mb.dv_costs,
                 sizeof(IntraBCMVCosts));
        }
      }
    }
    av1_alloc_mb_data(cpi, &thread_data->td->mb);

    // Reset rtc counters.
    av1_init_rtc_counters(&thread_data->td->mb);

    if (thread_data->td->counts != &cpi->counts) {
      memcpy(thread_data->td->counts, &cpi->counts, sizeof(cpi->counts));
    }

    if (i > 0) {
      thread_data->td->mb.palette_buffer = thread_data->td->palette_buffer;
      thread_data->td->mb.comp_rd_buffer = thread_data->td->comp_rd_buffer;
      thread_data->td->mb.tmp_conv_dst = thread_data->td->tmp_conv_dst;
      for (int j = 0; j < 2; ++j) {
        thread_data->td->mb.tmp_pred_bufs[j] =
            thread_data->td->tmp_pred_bufs[j];
      }
      thread_data->td->mb.pixel_gradient_info =
          thread_data->td->pixel_gradient_info;

      thread_data->td->mb.src_var_info_of_4x4_sub_blocks =
          thread_data->td->src_var_info_of_4x4_sub_blocks;

      thread_data->td->mb.e_mbd.tmp_conv_dst = thread_data->td->mb.tmp_conv_dst;
      for (int j = 0; j < 2; ++j) {
        thread_data->td->mb.e_mbd.tmp_obmc_bufs[j] =
            thread_data->td->mb.tmp_pred_bufs[j];
      }
    }

    // Preallocate the pc_tree for realtime coding to reduce the cost of memory
    // allocation.
    thread_data->td->rt_pc_root =
        cpi->sf.rt_sf.use_nonrd_pick_mode
            ? av1_alloc_pc_tree_node(cm->seq_params->sb_size)
            : NULL;
  }
}

#if !CONFIG_REALTIME_ONLY
static AOM_INLINE void fp_prepare_enc_workers(AV1_COMP *cpi, AVxWorkerHook hook,
                                              int num_workers) {
  AV1_COMMON *const cm = &cpi->common;
  MultiThreadInfo *const mt_info = &cpi->mt_info;
  for (int i = num_workers - 1; i >= 0; i--) {
    AVxWorker *const worker = &mt_info->workers[i];
    EncWorkerData *const thread_data = &mt_info->tile_thr_data[i];

    worker->hook = hook;
    worker->data1 = thread_data;
    worker->data2 = NULL;

    thread_data->thread_id = i;
    // Set the starting tile for each thread.
    thread_data->start = i;

    thread_data->cpi = cpi;
    if (i == 0) {
      thread_data->td = &cpi->td;
    } else {
      thread_data->td = thread_data->original_td;
    }

    // Before encoding a frame, copy the thread data from cpi.
    if (thread_data->td != &cpi->td) {
      thread_data->td->mb = cpi->td.mb;
      // Keep this conditional expression in sync with the corresponding one
      // in av1_fp_encode_tiles_row_mt().
      if (cpi->sf.inter_sf.mv_cost_upd_level != INTERNAL_COST_UPD_OFF) {
        CHECK_MEM_ERROR(cm, thread_data->td->mb.mv_costs,
                        (MvCosts *)aom_malloc(sizeof(MvCosts)));
        memcpy(thread_data->td->mb.mv_costs, cpi->td.mb.mv_costs,
               sizeof(MvCosts));
      }
    }

    av1_alloc_mb_data(cpi, &thread_data->td->mb);
  }
}
#endif

// Computes the number of workers for row multi-threading of encoding stage
static AOM_INLINE int compute_num_enc_row_mt_workers(AV1_COMMON *const cm,
                                                     int max_threads) {
  TileInfo tile_info;
  const int tile_cols = cm->tiles.cols;
  const int tile_rows = cm->tiles.rows;
  int total_num_threads_row_mt = 0;
  for (int row = 0; row < tile_rows; row++) {
    for (int col = 0; col < tile_cols; col++) {
      av1_tile_init(&tile_info, cm, row, col);
      const int num_sb_rows_in_tile = av1_get_sb_rows_in_tile(cm, &tile_info);
      const int num_sb_cols_in_tile = av1_get_sb_cols_in_tile(cm, &tile_info);
      total_num_threads_row_mt +=
          AOMMIN((num_sb_cols_in_tile + 1) >> 1, num_sb_rows_in_tile);
    }
  }
  return AOMMIN(max_threads, total_num_threads_row_mt);
}

// Computes the number of workers for tile multi-threading of encoding stage
static AOM_INLINE int compute_num_enc_tile_mt_workers(AV1_COMMON *const cm,
                                                      int max_threads) {
  const int tile_cols = cm->tiles.cols;
  const int tile_rows = cm->tiles.rows;
  return AOMMIN(max_threads, tile_cols * tile_rows);
}

// Find max worker of all MT stages
int av1_get_max_num_workers(const AV1_COMP *cpi) {
  int max_num_workers = 0;
  for (int i = MOD_FP; i < NUM_MT_MODULES; i++)
    max_num_workers =
        AOMMAX(cpi->ppi->p_mt_info.num_mod_workers[i], max_num_workers);
  assert(max_num_workers >= 1);
  return AOMMIN(max_num_workers, cpi->oxcf.max_threads);
}

// Computes the number of workers for encoding stage (row/tile multi-threading)
int av1_compute_num_enc_workers(AV1_COMP *cpi, int max_workers) {
  if (max_workers <= 1) return 1;
  if (cpi->oxcf.row_mt)
    return compute_num_enc_row_mt_workers(&cpi->common, max_workers);
  else
    return compute_num_enc_tile_mt_workers(&cpi->common, max_workers);
}

void av1_encode_tiles_mt(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  MultiThreadInfo *const mt_info = &cpi->mt_info;
  const int tile_cols = cm->tiles.cols;
  const int tile_rows = cm->tiles.rows;
  int num_workers = mt_info->num_mod_workers[MOD_ENC];

  assert(IMPLIES(cpi->tile_data == NULL,
                 cpi->allocated_tiles < tile_cols * tile_rows));
  if (cpi->allocated_tiles < tile_cols * tile_rows) av1_alloc_tile_data(cpi);

  av1_init_tile_data(cpi);
  num_workers = AOMMIN(num_workers, mt_info->num_workers);

  prepare_enc_workers(cpi, enc_worker_hook, num_workers);
  launch_workers(&cpi->mt_info, num_workers);
  sync_enc_workers(&cpi->mt_info, cm, num_workers);
  accumulate_counters_enc_workers(cpi, num_workers);
}

// Accumulate frame counts. FRAME_COUNTS consist solely of 'unsigned int'
// members, so we treat it as an array, and sum over the whole length.
void av1_accumulate_frame_counts(FRAME_COUNTS *acc_counts,
                                 const FRAME_COUNTS *counts) {
  unsigned int *const acc = (unsigned int *)acc_counts;
  const unsigned int *const cnt = (const unsigned int *)counts;

  const unsigned int n_counts = sizeof(FRAME_COUNTS) / sizeof(unsigned int);

  for (unsigned int i = 0; i < n_counts; i++) acc[i] += cnt[i];
}

// Computes the maximum number of sb_rows for row multi-threading of encoding
// stage
static AOM_INLINE void compute_max_sb_rows_cols(AV1_COMP *cpi, int *max_sb_rows,
                                                int *max_sb_cols) {
  AV1_COMMON *const cm = &cpi->common;
  const int tile_cols = cm->tiles.cols;
  const int tile_rows = cm->tiles.rows;
  for (int row = 0; row < tile_rows; row++) {
    for (int col = 0; col < tile_cols; col++) {
      const int tile_index = row * cm->tiles.cols + col;
      const TileInfo *const tile_info = &cpi->tile_data[tile_index].tile_info;
      const int num_sb_rows_in_tile = av1_get_sb_rows_in_tile(cm, tile_info);
      const int num_sb_cols_in_tile = av1_get_sb_cols_in_tile(cm, tile_info);
      *max_sb_rows = AOMMAX(*max_sb_rows, num_sb_rows_in_tile);
      *max_sb_cols = AOMMAX(*max_sb_cols, num_sb_cols_in_tile);
    }
  }
}

#if !CONFIG_REALTIME_ONLY
// Computes the number of workers for firstpass stage (row/tile multi-threading)
int av1_fp_compute_num_enc_workers(AV1_COMP *cpi) {
  AV1_COMMON *cm = &cpi->common;
  const int tile_cols = cm->tiles.cols;
  const int tile_rows = cm->tiles.rows;
  int total_num_threads_row_mt = 0;
  TileInfo tile_info;

  if (cpi->oxcf.max_threads <= 1) return 1;

  for (int row = 0; row < tile_rows; row++) {
    for (int col = 0; col < tile_cols; col++) {
      av1_tile_init(&tile_info, cm, row, col);
      const int num_mb_rows_in_tile =
          av1_get_unit_rows_in_tile(&tile_info, cpi->fp_block_size);
      const int num_mb_cols_in_tile =
          av1_get_unit_cols_in_tile(&tile_info, cpi->fp_block_size);
      total_num_threads_row_mt +=
          AOMMIN((num_mb_cols_in_tile + 1) >> 1, num_mb_rows_in_tile);
    }
  }
  return AOMMIN(cpi->oxcf.max_threads, total_num_threads_row_mt);
}

// Computes the maximum number of mb_rows for row multi-threading of firstpass
// stage
static AOM_INLINE int fp_compute_max_mb_rows(const AV1_COMMON *const cm,
                                             const TileDataEnc *const tile_data,
                                             const BLOCK_SIZE fp_block_size) {
  const int tile_cols = cm->tiles.cols;
  const int tile_rows = cm->tiles.rows;
  int max_mb_rows = 0;
  for (int row = 0; row < tile_rows; row++) {
    for (int col = 0; col < tile_cols; col++) {
      const int tile_index = row * cm->tiles.cols + col;
      const TileInfo *const tile_info = &tile_data[tile_index].tile_info;
      const int num_mb_rows_in_tile =
          av1_get_unit_rows_in_tile(tile_info, fp_block_size);
      max_mb_rows = AOMMAX(max_mb_rows, num_mb_rows_in_tile);
    }
  }
  return max_mb_rows;
}
#endif

void av1_encode_tiles_row_mt(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  MultiThreadInfo *const mt_info = &cpi->mt_info;
  AV1EncRowMultiThreadInfo *const enc_row_mt = &mt_info->enc_row_mt;
  const int tile_cols = cm->tiles.cols;
  const int tile_rows = cm->tiles.rows;
  int *thread_id_to_tile_id = enc_row_mt->thread_id_to_tile_id;
  int max_sb_rows = 0, max_sb_cols = 0;
  int num_workers = mt_info->num_mod_workers[MOD_ENC];

  assert(IMPLIES(cpi->tile_data == NULL,
                 cpi->allocated_tiles < tile_cols * tile_rows));
  if (cpi->allocated_tiles < tile_cols * tile_rows) {
    av1_row_mt_mem_dealloc(cpi);
    av1_alloc_tile_data(cpi);
  }

  av1_init_tile_data(cpi);

  compute_max_sb_rows_cols(cpi, &max_sb_rows, &max_sb_cols);

  if (enc_row_mt->allocated_tile_cols != tile_cols ||
      enc_row_mt->allocated_tile_rows != tile_rows ||
      enc_row_mt->allocated_rows != max_sb_rows ||
      enc_row_mt->allocated_cols != (max_sb_cols - 1)) {
    av1_row_mt_mem_dealloc(cpi);
    row_mt_mem_alloc(cpi, max_sb_rows, max_sb_cols,
                     cpi->oxcf.algo_cfg.cdf_update_mode);
  }

  memset(thread_id_to_tile_id, -1,
         sizeof(*thread_id_to_tile_id) * MAX_NUM_THREADS);

  for (int tile_row = 0; tile_row < tile_rows; tile_row++) {
    for (int tile_col = 0; tile_col < tile_cols; tile_col++) {
      int tile_index = tile_row * tile_cols + tile_col;
      TileDataEnc *const this_tile = &cpi->tile_data[tile_index];
      AV1EncRowMultiThreadSync *const row_mt_sync = &this_tile->row_mt_sync;

      // Initialize num_finished_cols to -1 for all rows.
      memset(row_mt_sync->num_finished_cols, -1,
             sizeof(*row_mt_sync->num_finished_cols) * max_sb_rows);
      row_mt_sync->next_mi_row = this_tile->tile_info.mi_row_start;
      row_mt_sync->num_threads_working = 0;
      row_mt_sync->intrabc_extra_top_right_sb_delay =
          av1_get_intrabc_extra_top_right_sb_delay(cm);

      av1_inter_mode_data_init(this_tile);
      av1_zero_above_context(cm, &cpi->td.mb.e_mbd,
                             this_tile->tile_info.mi_col_start,
                             this_tile->tile_info.mi_col_end, tile_row);
    }
  }

  num_workers = AOMMIN(num_workers, mt_info->num_workers);

  assign_tile_to_thread(thread_id_to_tile_id, tile_cols * tile_rows,
                        num_workers);
  prepare_enc_workers(cpi, enc_row_mt_worker_hook, num_workers);
  launch_workers(&cpi->mt_info, num_workers);
  sync_enc_workers(&cpi->mt_info, cm, num_workers);
  if (cm->delta_q_info.delta_lf_present_flag) update_delta_lf_for_row_mt(cpi);
  accumulate_counters_enc_workers(cpi, num_workers);
}

#if !CONFIG_REALTIME_ONLY
void av1_fp_encode_tiles_row_mt(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  MultiThreadInfo *const mt_info = &cpi->mt_info;
  AV1EncRowMultiThreadInfo *const enc_row_mt = &mt_info->enc_row_mt;
  const int tile_cols = cm->tiles.cols;
  const int tile_rows = cm->tiles.rows;
  int *thread_id_to_tile_id = enc_row_mt->thread_id_to_tile_id;
  int num_workers = 0;
  int max_mb_rows = 0;

  assert(IMPLIES(cpi->tile_data == NULL,
                 cpi->allocated_tiles < tile_cols * tile_rows));
  if (cpi->allocated_tiles < tile_cols * tile_rows) {
    av1_row_mt_mem_dealloc(cpi);
    av1_alloc_tile_data(cpi);
  }

  av1_init_tile_data(cpi);

  const BLOCK_SIZE fp_block_size = cpi->fp_block_size;
  max_mb_rows = fp_compute_max_mb_rows(cm, cpi->tile_data, fp_block_size);

  // For pass = 1, compute the no. of workers needed. For single-pass encode
  // (pass = 0), no. of workers are already computed.
  if (mt_info->num_mod_workers[MOD_FP] == 0)
    num_workers = av1_fp_compute_num_enc_workers(cpi);
  else
    num_workers = mt_info->num_mod_workers[MOD_FP];

  if (enc_row_mt->allocated_tile_cols != tile_cols ||
      enc_row_mt->allocated_tile_rows != tile_rows ||
      enc_row_mt->allocated_rows != max_mb_rows) {
    av1_row_mt_mem_dealloc(cpi);
    row_mt_mem_alloc(cpi, max_mb_rows, -1, 0);
  }

  memset(thread_id_to_tile_id, -1,
         sizeof(*thread_id_to_tile_id) * MAX_NUM_THREADS);

  for (int tile_row = 0; tile_row < tile_rows; tile_row++) {
    for (int tile_col = 0; tile_col < tile_cols; tile_col++) {
      int tile_index = tile_row * tile_cols + tile_col;
      TileDataEnc *const this_tile = &cpi->tile_data[tile_index];
      AV1EncRowMultiThreadSync *const row_mt_sync = &this_tile->row_mt_sync;

      // Initialize num_finished_cols to -1 for all rows.
      memset(row_mt_sync->num_finished_cols, -1,
             sizeof(*row_mt_sync->num_finished_cols) * max_mb_rows);
      row_mt_sync->next_mi_row = this_tile->tile_info.mi_row_start;
      row_mt_sync->num_threads_working = 0;

      // intraBC mode is not evaluated during first-pass encoding. Hence, no
      // additional top-right delay is required.
      row_mt_sync->intrabc_extra_top_right_sb_delay = 0;
    }
  }

  num_workers = AOMMIN(num_workers, mt_info->num_workers);
  assign_tile_to_thread(thread_id_to_tile_id, tile_cols * tile_rows,
                        num_workers);
  fp_prepare_enc_workers(cpi, fp_enc_row_mt_worker_hook, num_workers);
  launch_workers(&cpi->mt_info, num_workers);
  sync_enc_workers(&cpi->mt_info, cm, num_workers);
  for (int i = num_workers - 1; i >= 0; i--) {
    EncWorkerData *const thread_data = &cpi->mt_info.tile_thr_data[i];
    if (thread_data->td != &cpi->td) {
      // Keep this conditional expression in sync with the corresponding one
      // in fp_prepare_enc_workers().
      if (cpi->sf.inter_sf.mv_cost_upd_level != INTERNAL_COST_UPD_OFF) {
        aom_free(thread_data->td->mb.mv_costs);
      }
      assert(!thread_data->td->mb.dv_costs);
    }
    av1_dealloc_mb_data(cm, &thread_data->td->mb);
  }
}

void av1_tpl_row_mt_sync_read_dummy(AV1TplRowMultiThreadSync *tpl_mt_sync,
                                    int r, int c) {
  (void)tpl_mt_sync;
  (void)r;
  (void)c;
  return;
}

void av1_tpl_row_mt_sync_write_dummy(AV1TplRowMultiThreadSync *tpl_mt_sync,
                                     int r, int c, int cols) {
  (void)tpl_mt_sync;
  (void)r;
  (void)c;
  (void)cols;
  return;
}

void av1_tpl_row_mt_sync_read(AV1TplRowMultiThreadSync *tpl_row_mt_sync, int r,
                              int c) {
#if CONFIG_MULTITHREAD
  int nsync = tpl_row_mt_sync->sync_range;

  if (r) {
    pthread_mutex_t *const mutex = &tpl_row_mt_sync->mutex_[r - 1];
    pthread_mutex_lock(mutex);

    while (c > tpl_row_mt_sync->num_finished_cols[r - 1] - nsync)
      pthread_cond_wait(&tpl_row_mt_sync->cond_[r - 1], mutex);
    pthread_mutex_unlock(mutex);
  }
#else
  (void)tpl_row_mt_sync;
  (void)r;
  (void)c;
#endif  // CONFIG_MULTITHREAD
}

void av1_tpl_row_mt_sync_write(AV1TplRowMultiThreadSync *tpl_row_mt_sync, int r,
                               int c, int cols) {
#if CONFIG_MULTITHREAD
  int nsync = tpl_row_mt_sync->sync_range;
  int cur;
  // Only signal when there are enough encoded blocks for next row to run.
  int sig = 1;

  if (c < cols - 1) {
    cur = c;
    if (c % nsync) sig = 0;
  } else {
    cur = cols + nsync;
  }

  if (sig) {
    pthread_mutex_lock(&tpl_row_mt_sync->mutex_[r]);

    tpl_row_mt_sync->num_finished_cols[r] = cur;

    pthread_cond_signal(&tpl_row_mt_sync->cond_[r]);
    pthread_mutex_unlock(&tpl_row_mt_sync->mutex_[r]);
  }
#else
  (void)tpl_row_mt_sync;
  (void)r;
  (void)c;
  (void)cols;
#endif  // CONFIG_MULTITHREAD
}

// Each worker calls tpl_worker_hook() and computes the tpl data.
static int tpl_worker_hook(void *arg1, void *unused) {
  (void)unused;
  EncWorkerData *thread_data = (EncWorkerData *)arg1;
  AV1_COMP *cpi = thread_data->cpi;
  AV1_COMMON *cm = &cpi->common;
  MACROBLOCK *x = &thread_data->td->mb;
  MACROBLOCKD *xd = &x->e_mbd;
  TplTxfmStats *tpl_txfm_stats = &thread_data->td->tpl_txfm_stats;
  CommonModeInfoParams *mi_params = &cm->mi_params;
  BLOCK_SIZE bsize = convert_length_to_bsize(cpi->ppi->tpl_data.tpl_bsize_1d);
  TX_SIZE tx_size = max_txsize_lookup[bsize];
  int mi_height = mi_size_high[bsize];
  int num_active_workers = cpi->ppi->tpl_data.tpl_mt_sync.num_threads_working;

  av1_init_tpl_txfm_stats(tpl_txfm_stats);

  for (int mi_row = thread_data->start * mi_height; mi_row < mi_params->mi_rows;
       mi_row += num_active_workers * mi_height) {
    // Motion estimation row boundary
    av1_set_mv_row_limits(mi_params, &x->mv_limits, mi_row, mi_height,
                          cpi->oxcf.border_in_pixels);
    xd->mb_to_top_edge = -GET_MV_SUBPEL(mi_row * MI_SIZE);
    xd->mb_to_bottom_edge =
        GET_MV_SUBPEL((mi_params->mi_rows - mi_height - mi_row) * MI_SIZE);
    av1_mc_flow_dispenser_row(cpi, tpl_txfm_stats, x, mi_row, bsize, tx_size);
  }
  return 1;
}

// Deallocate tpl synchronization related mutex and data.
void av1_tpl_dealloc(AV1TplRowMultiThreadSync *tpl_sync) {
  assert(tpl_sync != NULL);

#if CONFIG_MULTITHREAD
  if (tpl_sync->mutex_ != NULL) {
    for (int i = 0; i < tpl_sync->rows; ++i)
      pthread_mutex_destroy(&tpl_sync->mutex_[i]);
    aom_free(tpl_sync->mutex_);
  }
  if (tpl_sync->cond_ != NULL) {
    for (int i = 0; i < tpl_sync->rows; ++i)
      pthread_cond_destroy(&tpl_sync->cond_[i]);
    aom_free(tpl_sync->cond_);
  }
#endif  // CONFIG_MULTITHREAD

  aom_free(tpl_sync->num_finished_cols);
  // clear the structure as the source of this call may be a resize in which
  // case this call will be followed by an _alloc() which may fail.
  av1_zero(*tpl_sync);
}

// Allocate memory for tpl row synchronization.
void av1_tpl_alloc(AV1TplRowMultiThreadSync *tpl_sync, AV1_COMMON *cm,
                   int mb_rows) {
  tpl_sync->rows = mb_rows;
#if CONFIG_MULTITHREAD
  {
    CHECK_MEM_ERROR(cm, tpl_sync->mutex_,
                    aom_malloc(sizeof(*tpl_sync->mutex_) * mb_rows));
    if (tpl_sync->mutex_) {
      for (int i = 0; i < mb_rows; ++i)
        pthread_mutex_init(&tpl_sync->mutex_[i], NULL);
    }

    CHECK_MEM_ERROR(cm, tpl_sync->cond_,
                    aom_malloc(sizeof(*tpl_sync->cond_) * mb_rows));
    if (tpl_sync->cond_) {
      for (int i = 0; i < mb_rows; ++i)
        pthread_cond_init(&tpl_sync->cond_[i], NULL);
    }
  }
#endif  // CONFIG_MULTITHREAD
  CHECK_MEM_ERROR(cm, tpl_sync->num_finished_cols,
                  aom_malloc(sizeof(*tpl_sync->num_finished_cols) * mb_rows));

  // Set up nsync.
  tpl_sync->sync_range = 1;
}

// Each worker is prepared by assigning the hook function and individual thread
// data.
static AOM_INLINE void prepare_tpl_workers(AV1_COMP *cpi, AVxWorkerHook hook,
                                           int num_workers) {
  MultiThreadInfo *mt_info = &cpi->mt_info;
  for (int i = num_workers - 1; i >= 0; i--) {
    AVxWorker *worker = &mt_info->workers[i];
    EncWorkerData *thread_data = &mt_info->tile_thr_data[i];

    worker->hook = hook;
    worker->data1 = thread_data;
    worker->data2 = NULL;

    thread_data->thread_id = i;
    // Set the starting tile for each thread.
    thread_data->start = i;

    thread_data->cpi = cpi;
    if (i == 0) {
      thread_data->td = &cpi->td;
    } else {
      thread_data->td = thread_data->original_td;
    }

    // Before encoding a frame, copy the thread data from cpi.
    if (thread_data->td != &cpi->td) {
      thread_data->td->mb = cpi->td.mb;
      // OBMC buffers are used only to init MS params and remain unused when
      // called from tpl, hence set the buffers to defaults.
      av1_init_obmc_buffer(&thread_data->td->mb.obmc_buffer);
      thread_data->td->mb.tmp_conv_dst = thread_data->td->tmp_conv_dst;
      thread_data->td->mb.e_mbd.tmp_conv_dst = thread_data->td->mb.tmp_conv_dst;
    }
  }
}

// Accumulate transform stats after tpl.
static void tpl_accumulate_txfm_stats(ThreadData *main_td,
                                      const MultiThreadInfo *mt_info,
                                      int num_workers) {
  TplTxfmStats *accumulated_stats = &main_td->tpl_txfm_stats;
  for (int i = num_workers - 1; i >= 0; i--) {
    AVxWorker *const worker = &mt_info->workers[i];
    EncWorkerData *const thread_data = (EncWorkerData *)worker->data1;
    ThreadData *td = thread_data->td;
    if (td != main_td) {
      const TplTxfmStats *tpl_txfm_stats = &td->tpl_txfm_stats;
      av1_accumulate_tpl_txfm_stats(tpl_txfm_stats, accumulated_stats);
    }
  }
}

// Implements multi-threading for tpl.
void av1_mc_flow_dispenser_mt(AV1_COMP *cpi) {
  AV1_COMMON *cm = &cpi->common;
  CommonModeInfoParams *mi_params = &cm->mi_params;
  MultiThreadInfo *mt_info = &cpi->mt_info;
  TplParams *tpl_data = &cpi->ppi->tpl_data;
  AV1TplRowMultiThreadSync *tpl_sync = &tpl_data->tpl_mt_sync;
  int mb_rows = mi_params->mb_rows;
  int num_workers =
      AOMMIN(mt_info->num_mod_workers[MOD_TPL], mt_info->num_workers);

  if (mb_rows != tpl_sync->rows) {
    av1_tpl_dealloc(tpl_sync);
    av1_tpl_alloc(tpl_sync, cm, mb_rows);
  }
  tpl_sync->num_threads_working = num_workers;

  // Initialize cur_mb_col to -1 for all MB rows.
  memset(tpl_sync->num_finished_cols, -1,
         sizeof(*tpl_sync->num_finished_cols) * mb_rows);

  prepare_tpl_workers(cpi, tpl_worker_hook, num_workers);
  launch_workers(&cpi->mt_info, num_workers);
  sync_enc_workers(&cpi->mt_info, cm, num_workers);
  tpl_accumulate_txfm_stats(&cpi->td, &cpi->mt_info, num_workers);
}

// Deallocate memory for temporal filter multi-thread synchronization.
void av1_tf_mt_dealloc(AV1TemporalFilterSync *tf_sync) {
  assert(tf_sync != NULL);
#if CONFIG_MULTITHREAD
  if (tf_sync->mutex_ != NULL) {
    pthread_mutex_destroy(tf_sync->mutex_);
    aom_free(tf_sync->mutex_);
  }
#endif  // CONFIG_MULTITHREAD
  tf_sync->next_tf_row = 0;
}

// Checks if a job is available. If job is available,
// populates next_tf_row and returns 1, else returns 0.
static AOM_INLINE int tf_get_next_job(AV1TemporalFilterSync *tf_mt_sync,
                                      int *current_mb_row, int mb_rows) {
  int do_next_row = 0;
#if CONFIG_MULTITHREAD
  pthread_mutex_t *tf_mutex_ = tf_mt_sync->mutex_;
  pthread_mutex_lock(tf_mutex_);
#endif
  if (tf_mt_sync->next_tf_row < mb_rows) {
    *current_mb_row = tf_mt_sync->next_tf_row;
    tf_mt_sync->next_tf_row++;
    do_next_row = 1;
  }
#if CONFIG_MULTITHREAD
  pthread_mutex_unlock(tf_mutex_);
#endif
  return do_next_row;
}

// Hook function for each thread in temporal filter multi-threading.
static int tf_worker_hook(void *arg1, void *unused) {
  (void)unused;
  EncWorkerData *thread_data = (EncWorkerData *)arg1;
  AV1_COMP *cpi = thread_data->cpi;
  ThreadData *td = thread_data->td;
  TemporalFilterCtx *tf_ctx = &cpi->tf_ctx;
  AV1TemporalFilterSync *tf_sync = &cpi->mt_info.tf_sync;
  const struct scale_factors *scale = &cpi->tf_ctx.sf;
  const int num_planes = av1_num_planes(&cpi->common);
  assert(num_planes >= 1 && num_planes <= MAX_MB_PLANE);

  MACROBLOCKD *mbd = &td->mb.e_mbd;
  uint8_t *input_buffer[MAX_MB_PLANE];
  MB_MODE_INFO **input_mb_mode_info;
  tf_save_state(mbd, &input_mb_mode_info, input_buffer, num_planes);
  tf_setup_macroblockd(mbd, &td->tf_data, scale);

  int current_mb_row = -1;

  while (tf_get_next_job(tf_sync, &current_mb_row, tf_ctx->mb_rows))
    av1_tf_do_filtering_row(cpi, td, current_mb_row);

  tf_restore_state(mbd, input_mb_mode_info, input_buffer, num_planes);

  return 1;
}

// Assigns temporal filter hook function and thread data to each worker.
static void prepare_tf_workers(AV1_COMP *cpi, AVxWorkerHook hook,
                               int num_workers, int is_highbitdepth) {
  MultiThreadInfo *mt_info = &cpi->mt_info;
  mt_info->tf_sync.next_tf_row = 0;
  for (int i = num_workers - 1; i >= 0; i--) {
    AVxWorker *worker = &mt_info->workers[i];
    EncWorkerData *thread_data = &mt_info->tile_thr_data[i];

    worker->hook = hook;
    worker->data1 = thread_data;
    worker->data2 = NULL;

    thread_data->thread_id = i;
    // Set the starting tile for each thread.
    thread_data->start = i;

    thread_data->cpi = cpi;
    if (i == 0) {
      thread_data->td = &cpi->td;
    } else {
      thread_data->td = thread_data->original_td;
    }

    // Before encoding a frame, copy the thread data from cpi.
    if (thread_data->td != &cpi->td) {
      thread_data->td->mb = cpi->td.mb;
      // OBMC buffers are used only to init MS params and remain unused when
      // called from tf, hence set the buffers to defaults.
      av1_init_obmc_buffer(&thread_data->td->mb.obmc_buffer);
      if (!tf_alloc_and_reset_data(&thread_data->td->tf_data,
                                   cpi->tf_ctx.num_pels, is_highbitdepth)) {
        aom_internal_error(cpi->common.error, AOM_CODEC_MEM_ERROR,
                           "Error allocating temporal filter data");
      }
    }
  }
}

// Deallocate thread specific data for temporal filter.
static void tf_dealloc_thread_data(AV1_COMP *cpi, int num_workers,
                                   int is_highbitdepth) {
  MultiThreadInfo *mt_info = &cpi->mt_info;
  for (int i = num_workers - 1; i >= 0; i--) {
    EncWorkerData *thread_data = &mt_info->tile_thr_data[i];
    ThreadData *td = thread_data->td;
    if (td != &cpi->td) tf_dealloc_data(&td->tf_data, is_highbitdepth);
  }
}

// Accumulate sse and sum after temporal filtering.
static void tf_accumulate_frame_diff(AV1_COMP *cpi, int num_workers) {
  FRAME_DIFF *total_diff = &cpi->td.tf_data.diff;
  for (int i = num_workers - 1; i >= 0; i--) {
    AVxWorker *const worker = &cpi->mt_info.workers[i];
    EncWorkerData *const thread_data = (EncWorkerData *)worker->data1;
    ThreadData *td = thread_data->td;
    FRAME_DIFF *diff = &td->tf_data.diff;
    if (td != &cpi->td) {
      total_diff->sse += diff->sse;
      total_diff->sum += diff->sum;
    }
  }
}

// Implements multi-threading for temporal filter.
void av1_tf_do_filtering_mt(AV1_COMP *cpi) {
  AV1_COMMON *cm = &cpi->common;
  MultiThreadInfo *mt_info = &cpi->mt_info;
  const int is_highbitdepth = cpi->tf_ctx.is_highbitdepth;

  int num_workers =
      AOMMIN(mt_info->num_mod_workers[MOD_TF], mt_info->num_workers);

  prepare_tf_workers(cpi, tf_worker_hook, num_workers, is_highbitdepth);
  launch_workers(mt_info, num_workers);
  sync_enc_workers(mt_info, cm, num_workers);
  tf_accumulate_frame_diff(cpi, num_workers);
  tf_dealloc_thread_data(cpi, num_workers, is_highbitdepth);
}

// Checks if a job is available in the current direction. If a job is available,
// frame_idx will be populated and returns 1, else returns 0.
static AOM_INLINE int get_next_gm_job(AV1_COMP *cpi, int *frame_idx,
                                      int cur_dir) {
  GlobalMotionInfo *gm_info = &cpi->gm_info;
  JobInfo *job_info = &cpi->mt_info.gm_sync.job_info;

  int total_refs = gm_info->num_ref_frames[cur_dir];
  int8_t cur_frame_to_process = job_info->next_frame_to_process[cur_dir];

  if (cur_frame_to_process < total_refs && !job_info->early_exit[cur_dir]) {
    *frame_idx = gm_info->reference_frames[cur_dir][cur_frame_to_process].frame;
    job_info->next_frame_to_process[cur_dir] += 1;
    return 1;
  }
  return 0;
}

// Switches the current direction and calls the function get_next_gm_job() if
// the speed feature 'prune_ref_frame_for_gm_search' is not set.
static AOM_INLINE void switch_direction(AV1_COMP *cpi, int *frame_idx,
                                        int *cur_dir) {
  if (cpi->sf.gm_sf.prune_ref_frame_for_gm_search) return;
  // Switch the direction and get next job
  *cur_dir = !(*cur_dir);
  get_next_gm_job(cpi, frame_idx, *(cur_dir));
}

// Initializes inliers, num_inliers and segment_map.
static AOM_INLINE void init_gm_thread_data(
    const GlobalMotionInfo *gm_info, GlobalMotionThreadData *thread_data) {
  for (int m = 0; m < RANSAC_NUM_MOTIONS; m++) {
    MotionModel motion_params = thread_data->params_by_motion[m];
    av1_zero(motion_params.params);
    motion_params.num_inliers = 0;
  }

  av1_zero_array(thread_data->segment_map,
                 gm_info->segment_map_w * gm_info->segment_map_h);
}

// Hook function for each thread in global motion multi-threading.
static int gm_mt_worker_hook(void *arg1, void *unused) {
  (void)unused;

  EncWorkerData *thread_data = (EncWorkerData *)arg1;
  AV1_COMP *cpi = thread_data->cpi;
  GlobalMotionInfo *gm_info = &cpi->gm_info;
  MultiThreadInfo *mt_info = &cpi->mt_info;
  JobInfo *job_info = &mt_info->gm_sync.job_info;
  int thread_id = thread_data->thread_id;
  GlobalMotionThreadData *gm_thread_data =
      &mt_info->gm_sync.thread_data[thread_id];
  int cur_dir = job_info->thread_id_to_dir[thread_id];
#if CONFIG_MULTITHREAD
  pthread_mutex_t *gm_mt_mutex_ = mt_info->gm_sync.mutex_;
#endif

  while (1) {
    int ref_buf_idx = -1;
    int ref_frame_idx = -1;

#if CONFIG_MULTITHREAD
    pthread_mutex_lock(gm_mt_mutex_);
#endif

    // Populates ref_buf_idx(the reference frame type) for which global motion
    // estimation will be done.
    if (!get_next_gm_job(cpi, &ref_buf_idx, cur_dir)) {
      // No jobs are available for the current direction. Switch
      // to other direction and get the next job, if available.
      switch_direction(cpi, &ref_buf_idx, &cur_dir);
    }

    // 'ref_frame_idx' holds the index of the current reference frame type in
    // gm_info->reference_frames. job_info->next_frame_to_process will be
    // incremented in get_next_gm_job() and hence subtracting by 1.
    ref_frame_idx = job_info->next_frame_to_process[cur_dir] - 1;

#if CONFIG_MULTITHREAD
    pthread_mutex_unlock(gm_mt_mutex_);
#endif

    if (ref_buf_idx == -1) break;

    init_gm_thread_data(gm_info, gm_thread_data);

    // Compute global motion for the given ref_buf_idx.
    av1_compute_gm_for_valid_ref_frames(
        cpi, gm_info->ref_buf, ref_buf_idx, gm_info->num_src_corners,
        gm_info->src_corners, gm_info->src_buffer,
        gm_thread_data->params_by_motion, gm_thread_data->segment_map,
        gm_info->segment_map_w, gm_info->segment_map_h);

#if CONFIG_MULTITHREAD
    pthread_mutex_lock(gm_mt_mutex_);
#endif
    assert(ref_frame_idx != -1);
    // If global motion w.r.t. current ref frame is
    // INVALID/TRANSLATION/IDENTITY, skip the evaluation of global motion w.r.t
    // the remaining ref frames in that direction. The below exit is disabled
    // when ref frame distance w.r.t. current frame is zero. E.g.:
    // source_alt_ref_frame w.r.t. ARF frames.
    if (cpi->sf.gm_sf.prune_ref_frame_for_gm_search &&
        gm_info->reference_frames[cur_dir][ref_frame_idx].distance != 0 &&
        cpi->common.global_motion[ref_buf_idx].wmtype != ROTZOOM)
      job_info->early_exit[cur_dir] = 1;

#if CONFIG_MULTITHREAD
    pthread_mutex_unlock(gm_mt_mutex_);
#endif
  }
  return 1;
}

// Assigns global motion hook function and thread data to each worker.
static AOM_INLINE void prepare_gm_workers(AV1_COMP *cpi, AVxWorkerHook hook,
                                          int num_workers) {
  MultiThreadInfo *mt_info = &cpi->mt_info;
  for (int i = num_workers - 1; i >= 0; i--) {
    AVxWorker *worker = &mt_info->workers[i];
    EncWorkerData *thread_data = &mt_info->tile_thr_data[i];

    worker->hook = hook;
    worker->data1 = thread_data;
    worker->data2 = NULL;

    thread_data->thread_id = i;
    // Set the starting tile for each thread.
    thread_data->start = i;

    thread_data->cpi = cpi;
    if (i == 0) {
      thread_data->td = &cpi->td;
    } else {
      thread_data->td = thread_data->original_td;
    }
  }
}

// Assigns available threads to past/future direction.
static AOM_INLINE void assign_thread_to_dir(int8_t *thread_id_to_dir,
                                            int num_workers) {
  int8_t frame_dir_idx = 0;

  for (int i = 0; i < num_workers; i++) {
    thread_id_to_dir[i] = frame_dir_idx++;
    if (frame_dir_idx == MAX_DIRECTIONS) frame_dir_idx = 0;
  }
}

// Computes number of workers for global motion multi-threading.
static AOM_INLINE int compute_gm_workers(const AV1_COMP *cpi) {
  int total_refs =
      cpi->gm_info.num_ref_frames[0] + cpi->gm_info.num_ref_frames[1];
  int num_gm_workers = cpi->sf.gm_sf.prune_ref_frame_for_gm_search
                           ? AOMMIN(MAX_DIRECTIONS, total_refs)
                           : total_refs;
  num_gm_workers = AOMMIN(num_gm_workers, cpi->mt_info.num_workers);
  return (num_gm_workers);
}

// Frees the memory allocated for each worker in global motion multi-threading.
void av1_gm_dealloc(AV1GlobalMotionSync *gm_sync_data) {
  if (gm_sync_data->thread_data != NULL) {
    for (int j = 0; j < gm_sync_data->allocated_workers; j++) {
      GlobalMotionThreadData *thread_data = &gm_sync_data->thread_data[j];
      aom_free(thread_data->segment_map);

      for (int m = 0; m < RANSAC_NUM_MOTIONS; m++)
        aom_free(thread_data->params_by_motion[m].inliers);
    }
    aom_free(gm_sync_data->thread_data);
  }
}

// Allocates memory for inliers and segment_map for each worker in global motion
// multi-threading.
static AOM_INLINE void gm_alloc(AV1_COMP *cpi, int num_workers) {
  AV1_COMMON *cm = &cpi->common;
  AV1GlobalMotionSync *gm_sync = &cpi->mt_info.gm_sync;
  GlobalMotionInfo *gm_info = &cpi->gm_info;

  gm_sync->allocated_workers = num_workers;
  gm_sync->allocated_width = cpi->source->y_width;
  gm_sync->allocated_height = cpi->source->y_height;

  CHECK_MEM_ERROR(cm, gm_sync->thread_data,
                  aom_malloc(sizeof(*gm_sync->thread_data) * num_workers));

  for (int i = 0; i < num_workers; i++) {
    GlobalMotionThreadData *thread_data = &gm_sync->thread_data[i];
    CHECK_MEM_ERROR(
        cm, thread_data->segment_map,
        aom_malloc(sizeof(*thread_data->segment_map) * gm_info->segment_map_w *
                   gm_info->segment_map_h));

    for (int m = 0; m < RANSAC_NUM_MOTIONS; m++) {
      CHECK_MEM_ERROR(
          cm, thread_data->params_by_motion[m].inliers,
          aom_malloc(sizeof(*thread_data->params_by_motion[m].inliers) * 2 *
                     MAX_CORNERS));
    }
  }
}

// Implements multi-threading for global motion.
void av1_global_motion_estimation_mt(AV1_COMP *cpi) {
  AV1GlobalMotionSync *gm_sync = &cpi->mt_info.gm_sync;
  JobInfo *job_info = &gm_sync->job_info;

  av1_zero(*job_info);

  int num_workers = compute_gm_workers(cpi);

  if (num_workers > gm_sync->allocated_workers ||
      cpi->source->y_width != gm_sync->allocated_width ||
      cpi->source->y_height != gm_sync->allocated_height) {
    av1_gm_dealloc(gm_sync);
    gm_alloc(cpi, num_workers);
  }

  assign_thread_to_dir(job_info->thread_id_to_dir, num_workers);
  prepare_gm_workers(cpi, gm_mt_worker_hook, num_workers);
  launch_workers(&cpi->mt_info, num_workers);
  sync_enc_workers(&cpi->mt_info, &cpi->common, num_workers);
}
#endif  // !CONFIG_REALTIME_ONLY

// Compare and order tiles based on absolute sum of tx coeffs.
static int compare_tile_order(const void *a, const void *b) {
  const PackBSTileOrder *const tile_a = (const PackBSTileOrder *)a;
  const PackBSTileOrder *const tile_b = (const PackBSTileOrder *)b;

  if (tile_a->abs_sum_level > tile_b->abs_sum_level)
    return -1;
  else if (tile_a->abs_sum_level == tile_b->abs_sum_level)
    return (tile_a->tile_idx > tile_b->tile_idx ? 1 : -1);
  else
    return 1;
}

// Get next tile index to be processed for pack bitstream
static AOM_INLINE int get_next_pack_bs_tile_idx(
    AV1EncPackBSSync *const pack_bs_sync, const int num_tiles) {
  assert(pack_bs_sync->next_job_idx <= num_tiles);
  if (pack_bs_sync->next_job_idx == num_tiles) return -1;

  return pack_bs_sync->pack_bs_tile_order[pack_bs_sync->next_job_idx++]
      .tile_idx;
}

// Calculates bitstream chunk size based on total buffer size and tile or tile
// group size.
static AOM_INLINE size_t get_bs_chunk_size(int tg_or_tile_size,
                                           const int frame_or_tg_size,
                                           size_t *remain_buf_size,
                                           size_t max_buf_size,
                                           int is_last_chunk) {
  size_t this_chunk_size;
  assert(*remain_buf_size > 0);
  if (is_last_chunk) {
    this_chunk_size = *remain_buf_size;
    *remain_buf_size = 0;
  } else {
    const uint64_t size_scale = (uint64_t)max_buf_size * tg_or_tile_size;
    this_chunk_size = (size_t)(size_scale / frame_or_tg_size);
    *remain_buf_size -= this_chunk_size;
    assert(*remain_buf_size > 0);
  }
  assert(this_chunk_size > 0);
  return this_chunk_size;
}

// Initializes params required for pack bitstream tile.
static void init_tile_pack_bs_params(AV1_COMP *const cpi, uint8_t *const dst,
                                     struct aom_write_bit_buffer *saved_wb,
                                     PackBSParams *const pack_bs_params_arr,
                                     uint8_t obu_extn_header) {
  MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;
  AV1_COMMON *const cm = &cpi->common;
  const CommonTileParams *const tiles = &cm->tiles;
  const int num_tiles = tiles->cols * tiles->rows;
  // Fixed size tile groups for the moment
  const int num_tg_hdrs = cpi->num_tg;
  // Tile group size in terms of number of tiles.
  const int tg_size_in_tiles = (num_tiles + num_tg_hdrs - 1) / num_tg_hdrs;
  uint8_t *tile_dst = dst;
  uint8_t *tile_data_curr = dst;
  // Max tile group count can not be more than MAX_TILES.
  int tg_size_mi[MAX_TILES] = { 0 };  // Size of tile group in mi units
  int tile_idx;
  int tg_idx = 0;
  int tile_count_in_tg = 0;
  int new_tg = 1;

  // Populate pack bitstream params of all tiles.
  for (tile_idx = 0; tile_idx < num_tiles; tile_idx++) {
    const TileInfo *const tile_info = &cpi->tile_data[tile_idx].tile_info;
    PackBSParams *const pack_bs_params = &pack_bs_params_arr[tile_idx];
    // Calculate tile size in mi units.
    const int tile_size_mi = (tile_info->mi_col_end - tile_info->mi_col_start) *
                             (tile_info->mi_row_end - tile_info->mi_row_start);
    int is_last_tile_in_tg = 0;
    tile_count_in_tg++;
    if (tile_count_in_tg == tg_size_in_tiles || tile_idx == (num_tiles - 1))
      is_last_tile_in_tg = 1;

    // Populate pack bitstream params of this tile.
    pack_bs_params->curr_tg_hdr_size = 0;
    pack_bs_params->obu_extn_header = obu_extn_header;
    pack_bs_params->saved_wb = saved_wb;
    pack_bs_params->obu_header_size = 0;
    pack_bs_params->is_last_tile_in_tg = is_last_tile_in_tg;
    pack_bs_params->new_tg = new_tg;
    pack_bs_params->tile_col = tile_info->tile_col;
    pack_bs_params->tile_row = tile_info->tile_row;
    pack_bs_params->tile_size_mi = tile_size_mi;
    tg_size_mi[tg_idx] += tile_size_mi;

    if (new_tg) new_tg = 0;
    if (is_last_tile_in_tg) {
      tile_count_in_tg = 0;
      new_tg = 1;
      tg_idx++;
    }
  }

  assert(cpi->available_bs_size > 0);
  size_t tg_buf_size[MAX_TILES] = { 0 };
  size_t max_buf_size = cpi->available_bs_size;
  size_t remain_buf_size = max_buf_size;
  const int frame_size_mi = cm->mi_params.mi_rows * cm->mi_params.mi_cols;

  tile_idx = 0;
  // Prepare obu, tile group and frame header of each tile group.
  for (tg_idx = 0; tg_idx < cpi->num_tg; tg_idx++) {
    PackBSParams *const pack_bs_params = &pack_bs_params_arr[tile_idx];
    int is_last_tg = tg_idx == cpi->num_tg - 1;
    // Prorate bitstream buffer size based on tile group size and available
    // buffer size. This buffer will be used to store headers and tile data.
    tg_buf_size[tg_idx] =
        get_bs_chunk_size(tg_size_mi[tg_idx], frame_size_mi, &remain_buf_size,
                          max_buf_size, is_last_tg);

    pack_bs_params->dst = tile_dst;
    pack_bs_params->tile_data_curr = tile_dst;

    // Write obu, tile group and frame header at first tile in the tile
    // group.
    av1_write_obu_tg_tile_headers(cpi, xd, pack_bs_params, tile_idx);
    tile_dst += tg_buf_size[tg_idx];

    // Exclude headers from tile group buffer size.
    tg_buf_size[tg_idx] -= pack_bs_params->curr_tg_hdr_size;
    tile_idx += tg_size_in_tiles;
  }

  tg_idx = 0;
  // Calculate bitstream buffer size of each tile in the tile group.
  for (tile_idx = 0; tile_idx < num_tiles; tile_idx++) {
    PackBSParams *const pack_bs_params = &pack_bs_params_arr[tile_idx];

    if (pack_bs_params->new_tg) {
      max_buf_size = tg_buf_size[tg_idx];
      remain_buf_size = max_buf_size;
    }

    // Prorate bitstream buffer size of this tile based on tile size and
    // available buffer size. For this proration, header size is not accounted.
    const size_t tile_buf_size = get_bs_chunk_size(
        pack_bs_params->tile_size_mi, tg_size_mi[tg_idx], &remain_buf_size,
        max_buf_size, pack_bs_params->is_last_tile_in_tg);
    pack_bs_params->tile_buf_size = tile_buf_size;

    // Update base address of bitstream buffer for tile and tile group.
    if (pack_bs_params->new_tg) {
      tile_dst = pack_bs_params->dst;
      tile_data_curr = pack_bs_params->tile_data_curr;
      // Account header size in first tile of a tile group.
      pack_bs_params->tile_buf_size += pack_bs_params->curr_tg_hdr_size;
    } else {
      pack_bs_params->dst = tile_dst;
      pack_bs_params->tile_data_curr = tile_data_curr;
    }

    if (pack_bs_params->is_last_tile_in_tg) tg_idx++;
    tile_dst += pack_bs_params->tile_buf_size;
  }
}

// Worker hook function of pack bitsteam multithreading.
static int pack_bs_worker_hook(void *arg1, void *arg2) {
  EncWorkerData *const thread_data = (EncWorkerData *)arg1;
  PackBSParams *const pack_bs_params = (PackBSParams *)arg2;
  AV1_COMP *const cpi = thread_data->cpi;
  AV1_COMMON *const cm = &cpi->common;
  AV1EncPackBSSync *const pack_bs_sync = &cpi->mt_info.pack_bs_sync;
  const CommonTileParams *const tiles = &cm->tiles;
  const int num_tiles = tiles->cols * tiles->rows;

  while (1) {
#if CONFIG_MULTITHREAD
    pthread_mutex_lock(pack_bs_sync->mutex_);
#endif
    const int tile_idx = get_next_pack_bs_tile_idx(pack_bs_sync, num_tiles);
#if CONFIG_MULTITHREAD
    pthread_mutex_unlock(pack_bs_sync->mutex_);
#endif
    if (tile_idx == -1) break;
    TileDataEnc *this_tile = &cpi->tile_data[tile_idx];
    thread_data->td->mb.e_mbd.tile_ctx = &this_tile->tctx;

    av1_pack_tile_info(cpi, thread_data->td, &pack_bs_params[tile_idx]);
  }

  return 1;
}

// Prepares thread data and workers of pack bitsteam multithreading.
static void prepare_pack_bs_workers(AV1_COMP *const cpi,
                                    PackBSParams *const pack_bs_params,
                                    AVxWorkerHook hook, const int num_workers) {
  MultiThreadInfo *const mt_info = &cpi->mt_info;
  for (int i = num_workers - 1; i >= 0; i--) {
    AVxWorker *worker = &mt_info->workers[i];
    EncWorkerData *const thread_data = &mt_info->tile_thr_data[i];
    if (i == 0) {
      thread_data->td = &cpi->td;
    } else {
      thread_data->td = thread_data->original_td;
    }

    if (thread_data->td != &cpi->td) thread_data->td->mb = cpi->td.mb;

    thread_data->cpi = cpi;
    thread_data->start = i;
    thread_data->thread_id = i;
    av1_reset_pack_bs_thread_data(thread_data->td);

    worker->hook = hook;
    worker->data1 = thread_data;
    worker->data2 = pack_bs_params;
  }

  AV1_COMMON *const cm = &cpi->common;
  AV1EncPackBSSync *const pack_bs_sync = &mt_info->pack_bs_sync;
  const uint16_t num_tiles = cm->tiles.rows * cm->tiles.cols;
  pack_bs_sync->next_job_idx = 0;

  PackBSTileOrder *const pack_bs_tile_order = pack_bs_sync->pack_bs_tile_order;
  // Reset tile order data of pack bitstream
  av1_zero_array(pack_bs_tile_order, num_tiles);

  // Populate pack bitstream tile order structure
  for (uint16_t tile_idx = 0; tile_idx < num_tiles; tile_idx++) {
    pack_bs_tile_order[tile_idx].abs_sum_level =
        cpi->tile_data[tile_idx].abs_sum_level;
    pack_bs_tile_order[tile_idx].tile_idx = tile_idx;
  }

  // Sort tiles in descending order based on tile area.
  qsort(pack_bs_tile_order, num_tiles, sizeof(*pack_bs_tile_order),
        compare_tile_order);
}

// Accumulates data after pack bitsteam processing.
static void accumulate_pack_bs_data(
    AV1_COMP *const cpi, const PackBSParams *const pack_bs_params_arr,
    uint8_t *const dst, uint32_t *total_size, const FrameHeaderInfo *fh_info,
    int *const largest_tile_id, unsigned int *max_tile_size,
    uint32_t *const obu_header_size, uint8_t **tile_data_start,
    const int num_workers) {
  const AV1_COMMON *const cm = &cpi->common;
  const CommonTileParams *const tiles = &cm->tiles;
  const int tile_count = tiles->cols * tiles->rows;
  // Fixed size tile groups for the moment
  size_t curr_tg_data_size = 0;
  int is_first_tg = 1;
  uint8_t *curr_tg_start = dst;
  size_t src_offset = 0;
  size_t dst_offset = 0;

  for (int tile_idx = 0; tile_idx < tile_count; tile_idx++) {
    // PackBSParams stores all parameters required to pack tile and header
    // info.
    const PackBSParams *const pack_bs_params = &pack_bs_params_arr[tile_idx];
    uint32_t tile_size = 0;

    if (pack_bs_params->new_tg) {
      curr_tg_start = dst + *total_size;
      curr_tg_data_size = pack_bs_params->curr_tg_hdr_size;
      *tile_data_start += pack_bs_params->curr_tg_hdr_size;
      *obu_header_size = pack_bs_params->obu_header_size;
    }
    curr_tg_data_size +=
        pack_bs_params->buf.size + (pack_bs_params->is_last_tile_in_tg ? 0 : 4);

    if (pack_bs_params->buf.size > *max_tile_size) {
      *largest_tile_id = tile_idx;
      *max_tile_size = (unsigned int)pack_bs_params->buf.size;
    }
    tile_size +=
        (uint32_t)pack_bs_params->buf.size + *pack_bs_params->total_size;

    // Pack all the chunks of tile bitstreams together
    if (tile_idx != 0) memmove(dst + dst_offset, dst + src_offset, tile_size);

    if (pack_bs_params->is_last_tile_in_tg)
      av1_write_last_tile_info(
          cpi, fh_info, pack_bs_params->saved_wb, &curr_tg_data_size,
          curr_tg_start, &tile_size, tile_data_start, largest_tile_id,
          &is_first_tg, *obu_header_size, pack_bs_params->obu_extn_header);
    src_offset += pack_bs_params->tile_buf_size;
    dst_offset += tile_size;
    *total_size += tile_size;
  }

  // Accumulate thread data
  MultiThreadInfo *const mt_info = &cpi->mt_info;
  for (int idx = num_workers - 1; idx >= 0; idx--) {
    ThreadData const *td = mt_info->tile_thr_data[idx].td;
    av1_accumulate_pack_bs_thread_data(cpi, td);
  }
}

void av1_write_tile_obu_mt(
    AV1_COMP *const cpi, uint8_t *const dst, uint32_t *total_size,
    struct aom_write_bit_buffer *saved_wb, uint8_t obu_extn_header,
    const FrameHeaderInfo *fh_info, int *const largest_tile_id,
    unsigned int *max_tile_size, uint32_t *const obu_header_size,
    uint8_t **tile_data_start, const int num_workers) {
  MultiThreadInfo *const mt_info = &cpi->mt_info;

  PackBSParams pack_bs_params[MAX_TILES];
  uint32_t tile_size[MAX_TILES] = { 0 };

  for (int tile_idx = 0; tile_idx < MAX_TILES; tile_idx++)
    pack_bs_params[tile_idx].total_size = &tile_size[tile_idx];

  init_tile_pack_bs_params(cpi, dst, saved_wb, pack_bs_params, obu_extn_header);
  prepare_pack_bs_workers(cpi, pack_bs_params, pack_bs_worker_hook,
                          num_workers);
  launch_workers(mt_info, num_workers);
  sync_enc_workers(mt_info, &cpi->common, num_workers);
  accumulate_pack_bs_data(cpi, pack_bs_params, dst, total_size, fh_info,
                          largest_tile_id, max_tile_size, obu_header_size,
                          tile_data_start, num_workers);
}

// Deallocate memory for CDEF search multi-thread synchronization.
void av1_cdef_mt_dealloc(AV1CdefSync *cdef_sync) {
  (void)cdef_sync;
  assert(cdef_sync != NULL);
#if CONFIG_MULTITHREAD
  if (cdef_sync->mutex_ != NULL) {
    pthread_mutex_destroy(cdef_sync->mutex_);
    aom_free(cdef_sync->mutex_);
  }
#endif  // CONFIG_MULTITHREAD
}

// Updates the row and column indices of the next job to be processed.
// Also updates end_of_frame flag when the processing of all blocks is complete.
static void update_next_job_info(AV1CdefSync *cdef_sync, int nvfb, int nhfb) {
  cdef_sync->fbc++;
  if (cdef_sync->fbc == nhfb) {
    cdef_sync->fbr++;
    if (cdef_sync->fbr == nvfb) {
      cdef_sync->end_of_frame = 1;
    } else {
      cdef_sync->fbc = 0;
    }
  }
}

// Initializes cdef_sync parameters.
static AOM_INLINE void cdef_reset_job_info(AV1CdefSync *cdef_sync) {
#if CONFIG_MULTITHREAD
  if (cdef_sync->mutex_) pthread_mutex_init(cdef_sync->mutex_, NULL);
#endif  // CONFIG_MULTITHREAD
  cdef_sync->end_of_frame = 0;
  cdef_sync->fbr = 0;
  cdef_sync->fbc = 0;
}

// Checks if a job is available. If job is available,
// populates next job information and returns 1, else returns 0.
static AOM_INLINE int cdef_get_next_job(AV1CdefSync *cdef_sync,
                                        CdefSearchCtx *cdef_search_ctx,
                                        int *cur_fbr, int *cur_fbc,
                                        int *sb_count) {
#if CONFIG_MULTITHREAD
  pthread_mutex_lock(cdef_sync->mutex_);
#endif  // CONFIG_MULTITHREAD
  int do_next_block = 0;
  const int nvfb = cdef_search_ctx->nvfb;
  const int nhfb = cdef_search_ctx->nhfb;

  // If a block is skip, do not process the block and
  // check the skip condition for the next block.
  while ((!cdef_sync->end_of_frame) &&
         (cdef_sb_skip(cdef_search_ctx->mi_params, cdef_sync->fbr,
                       cdef_sync->fbc))) {
    update_next_job_info(cdef_sync, nvfb, nhfb);
  }

  // Populates information needed for current job and update the row,
  // column indices of the next block to be processed.
  if (cdef_sync->end_of_frame == 0) {
    do_next_block = 1;
    *cur_fbr = cdef_sync->fbr;
    *cur_fbc = cdef_sync->fbc;
    *sb_count = cdef_search_ctx->sb_count;
    cdef_search_ctx->sb_count++;
    update_next_job_info(cdef_sync, nvfb, nhfb);
  }
#if CONFIG_MULTITHREAD
  pthread_mutex_unlock(cdef_sync->mutex_);
#endif  // CONFIG_MULTITHREAD
  return do_next_block;
}

// Hook function for each thread in CDEF search multi-threading.
static int cdef_filter_block_worker_hook(void *arg1, void *arg2) {
  AV1CdefSync *const cdef_sync = (AV1CdefSync *)arg1;
  CdefSearchCtx *cdef_search_ctx = (CdefSearchCtx *)arg2;
  int cur_fbr, cur_fbc, sb_count;
  while (cdef_get_next_job(cdef_sync, cdef_search_ctx, &cur_fbr, &cur_fbc,
                           &sb_count)) {
    av1_cdef_mse_calc_block(cdef_search_ctx, cur_fbr, cur_fbc, sb_count);
  }
  return 1;
}

// Assigns CDEF search hook function and thread data to each worker.
static void prepare_cdef_workers(MultiThreadInfo *mt_info,
                                 CdefSearchCtx *cdef_search_ctx,
                                 AVxWorkerHook hook, int num_workers) {
  for (int i = num_workers - 1; i >= 0; i--) {
    AVxWorker *worker = &mt_info->workers[i];
    worker->hook = hook;
    worker->data1 = &mt_info->cdef_sync;
    worker->data2 = cdef_search_ctx;
  }
}

// Implements multi-threading for CDEF search.
void av1_cdef_mse_calc_frame_mt(AV1_COMMON *cm, MultiThreadInfo *mt_info,
                                CdefSearchCtx *cdef_search_ctx) {
  AV1CdefSync *cdef_sync = &mt_info->cdef_sync;
  const int num_workers = mt_info->num_mod_workers[MOD_CDEF_SEARCH];

  cdef_reset_job_info(cdef_sync);
  prepare_cdef_workers(mt_info, cdef_search_ctx, cdef_filter_block_worker_hook,
                       num_workers);
  launch_workers(mt_info, num_workers);
  sync_enc_workers(mt_info, cm, num_workers);
}

// Computes num_workers for temporal filter multi-threading.
static AOM_INLINE int compute_num_tf_workers(AV1_COMP *cpi) {
  // For single-pass encode, using no. of workers as per tf block size was not
  // found to improve speed. Hence the thread assignment for single-pass encode
  // is kept based on compute_num_enc_workers().
  if (cpi->oxcf.pass < AOM_RC_SECOND_PASS)
    return (av1_compute_num_enc_workers(cpi, cpi->oxcf.max_threads));

  if (cpi->oxcf.max_threads <= 1) return 1;

  const int frame_height = cpi->common.height;
  const BLOCK_SIZE block_size = TF_BLOCK_SIZE;
  const int mb_height = block_size_high[block_size];
  const int mb_rows = get_num_blocks(frame_height, mb_height);
  return AOMMIN(cpi->oxcf.max_threads, mb_rows);
}

// Computes num_workers for tpl multi-threading.
static AOM_INLINE int compute_num_tpl_workers(AV1_COMP *cpi) {
  return av1_compute_num_enc_workers(cpi, cpi->oxcf.max_threads);
}

// Computes num_workers for loop filter multi-threading.
static AOM_INLINE int compute_num_lf_workers(AV1_COMP *cpi) {
  return av1_compute_num_enc_workers(cpi, cpi->oxcf.max_threads);
}

// Computes num_workers for cdef multi-threading.
static AOM_INLINE int compute_num_cdef_workers(AV1_COMP *cpi) {
  return av1_compute_num_enc_workers(cpi, cpi->oxcf.max_threads);
}

// Computes num_workers for loop-restoration multi-threading.
static AOM_INLINE int compute_num_lr_workers(AV1_COMP *cpi) {
  return av1_compute_num_enc_workers(cpi, cpi->oxcf.max_threads);
}

// Computes num_workers for pack bitstream multi-threading.
static AOM_INLINE int compute_num_pack_bs_workers(AV1_COMP *cpi) {
  if (cpi->oxcf.max_threads <= 1) return 1;
  return compute_num_enc_tile_mt_workers(&cpi->common, cpi->oxcf.max_threads);
}

int compute_num_mod_workers(AV1_COMP *cpi, MULTI_THREADED_MODULES mod_name) {
  int num_mod_workers = 0;
  switch (mod_name) {
    case MOD_FP:
      if (cpi->oxcf.pass >= AOM_RC_SECOND_PASS)
        num_mod_workers = 0;
      else
        num_mod_workers =
            av1_compute_num_enc_workers(cpi, cpi->oxcf.max_threads);
      break;
    case MOD_TF: num_mod_workers = compute_num_tf_workers(cpi); break;
    case MOD_TPL: num_mod_workers = compute_num_tpl_workers(cpi); break;
    case MOD_GME: num_mod_workers = 1; break;
    case MOD_ENC:
      num_mod_workers = av1_compute_num_enc_workers(cpi, cpi->oxcf.max_threads);
      break;
    case MOD_LPF: num_mod_workers = compute_num_lf_workers(cpi); break;
    case MOD_CDEF_SEARCH:
      num_mod_workers = compute_num_cdef_workers(cpi);
      break;
    case MOD_CDEF: num_mod_workers = compute_num_cdef_workers(cpi); break;
    case MOD_LR: num_mod_workers = compute_num_lr_workers(cpi); break;
    case MOD_PACK_BS: num_mod_workers = compute_num_pack_bs_workers(cpi); break;
    case MOD_FRAME_ENC:
      num_mod_workers = cpi->ppi->p_mt_info.num_mod_workers[MOD_FRAME_ENC];
      break;
    default: assert(0); break;
  }
  return (num_mod_workers);
}
// Computes the number of workers for each MT modules in the encoder
void av1_compute_num_workers_for_mt(AV1_COMP *cpi) {
  for (int i = MOD_FP; i < NUM_MT_MODULES; i++)
    cpi->ppi->p_mt_info.num_mod_workers[i] =
        compute_num_mod_workers(cpi, (MULTI_THREADED_MODULES)i);
}
