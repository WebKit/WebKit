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

#include "aom/aom_image.h"
#include "config/aom_config.h"
#include "config/aom_scale_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/txfm_common.h"
#include "aom_mem/aom_mem.h"
#include "aom_util/aom_pthread.h"
#include "aom_util/aom_thread.h"
#include "av1/common/av1_loopfilter.h"
#include "av1/common/blockd.h"
#include "av1/common/cdef.h"
#include "av1/common/entropymode.h"
#include "av1/common/enums.h"
#include "av1/common/thread_common.h"
#include "av1/common/reconinter.h"
#include "av1/common/reconintra.h"
#include "av1/common/restoration.h"

// Set up nsync by width.
static INLINE int get_sync_range(int width) {
  // nsync numbers are picked by testing. For example, for 4k
  // video, using 4 gives best performance.
  if (width < 640)
    return 1;
  else if (width <= 1280)
    return 2;
  else if (width <= 4096)
    return 4;
  else
    return 8;
}

static INLINE int get_lr_sync_range(int width) {
#if 0
  // nsync numbers are picked by testing. For example, for 4k
  // video, using 4 gives best performance.
  if (width < 640)
    return 1;
  else if (width <= 1280)
    return 2;
  else if (width <= 4096)
    return 4;
  else
    return 8;
#else
  (void)width;
  return 1;
#endif
}

// Allocate memory for lf row synchronization
void av1_loop_filter_alloc(AV1LfSync *lf_sync, AV1_COMMON *cm, int rows,
                           int width, int num_workers) {
  lf_sync->rows = rows;
#if CONFIG_MULTITHREAD
  {
    int i, j;

    for (j = 0; j < MAX_MB_PLANE; j++) {
      CHECK_MEM_ERROR(cm, lf_sync->mutex_[j],
                      aom_malloc(sizeof(*(lf_sync->mutex_[j])) * rows));
      if (lf_sync->mutex_[j]) {
        for (i = 0; i < rows; ++i) {
          pthread_mutex_init(&lf_sync->mutex_[j][i], NULL);
        }
      }

      CHECK_MEM_ERROR(cm, lf_sync->cond_[j],
                      aom_malloc(sizeof(*(lf_sync->cond_[j])) * rows));
      if (lf_sync->cond_[j]) {
        for (i = 0; i < rows; ++i) {
          pthread_cond_init(&lf_sync->cond_[j][i], NULL);
        }
      }
    }

    CHECK_MEM_ERROR(cm, lf_sync->job_mutex,
                    aom_malloc(sizeof(*(lf_sync->job_mutex))));
    if (lf_sync->job_mutex) {
      pthread_mutex_init(lf_sync->job_mutex, NULL);
    }
  }
#endif  // CONFIG_MULTITHREAD
  CHECK_MEM_ERROR(cm, lf_sync->lfdata,
                  aom_malloc(num_workers * sizeof(*(lf_sync->lfdata))));
  lf_sync->num_workers = num_workers;

  for (int j = 0; j < MAX_MB_PLANE; j++) {
    CHECK_MEM_ERROR(cm, lf_sync->cur_sb_col[j],
                    aom_malloc(sizeof(*(lf_sync->cur_sb_col[j])) * rows));
  }
  CHECK_MEM_ERROR(
      cm, lf_sync->job_queue,
      aom_malloc(sizeof(*(lf_sync->job_queue)) * rows * MAX_MB_PLANE * 2));
  // Set up nsync.
  lf_sync->sync_range = get_sync_range(width);
}

// Deallocate lf synchronization related mutex and data
void av1_loop_filter_dealloc(AV1LfSync *lf_sync) {
  if (lf_sync != NULL) {
    int j;
#if CONFIG_MULTITHREAD
    int i;
    for (j = 0; j < MAX_MB_PLANE; j++) {
      if (lf_sync->mutex_[j] != NULL) {
        for (i = 0; i < lf_sync->rows; ++i) {
          pthread_mutex_destroy(&lf_sync->mutex_[j][i]);
        }
        aom_free(lf_sync->mutex_[j]);
      }
      if (lf_sync->cond_[j] != NULL) {
        for (i = 0; i < lf_sync->rows; ++i) {
          pthread_cond_destroy(&lf_sync->cond_[j][i]);
        }
        aom_free(lf_sync->cond_[j]);
      }
    }
    if (lf_sync->job_mutex != NULL) {
      pthread_mutex_destroy(lf_sync->job_mutex);
      aom_free(lf_sync->job_mutex);
    }
#endif  // CONFIG_MULTITHREAD
    aom_free(lf_sync->lfdata);
    for (j = 0; j < MAX_MB_PLANE; j++) {
      aom_free(lf_sync->cur_sb_col[j]);
    }

    aom_free(lf_sync->job_queue);
    // clear the structure as the source of this call may be a resize in which
    // case this call will be followed by an _alloc() which may fail.
    av1_zero(*lf_sync);
  }
}

void av1_alloc_cdef_sync(AV1_COMMON *const cm, AV1CdefSync *cdef_sync,
                         int num_workers) {
  if (num_workers < 1) return;
#if CONFIG_MULTITHREAD
  if (cdef_sync->mutex_ == NULL) {
    CHECK_MEM_ERROR(cm, cdef_sync->mutex_,
                    aom_malloc(sizeof(*(cdef_sync->mutex_))));
    if (cdef_sync->mutex_) pthread_mutex_init(cdef_sync->mutex_, NULL);
  }
#else
  (void)cm;
  (void)cdef_sync;
#endif  // CONFIG_MULTITHREAD
}

void av1_free_cdef_sync(AV1CdefSync *cdef_sync) {
  if (cdef_sync == NULL) return;
#if CONFIG_MULTITHREAD
  if (cdef_sync->mutex_ != NULL) {
    pthread_mutex_destroy(cdef_sync->mutex_);
    aom_free(cdef_sync->mutex_);
  }
#endif  // CONFIG_MULTITHREAD
}

static INLINE void cdef_row_mt_sync_read(AV1CdefSync *const cdef_sync,
                                         int row) {
  if (!row) return;
#if CONFIG_MULTITHREAD
  AV1CdefRowSync *const cdef_row_mt = cdef_sync->cdef_row_mt;
  pthread_mutex_lock(cdef_row_mt[row - 1].row_mutex_);
  while (cdef_row_mt[row - 1].is_row_done != 1)
    pthread_cond_wait(cdef_row_mt[row - 1].row_cond_,
                      cdef_row_mt[row - 1].row_mutex_);
  cdef_row_mt[row - 1].is_row_done = 0;
  pthread_mutex_unlock(cdef_row_mt[row - 1].row_mutex_);
#else
  (void)cdef_sync;
#endif  // CONFIG_MULTITHREAD
}

static INLINE void cdef_row_mt_sync_write(AV1CdefSync *const cdef_sync,
                                          int row) {
#if CONFIG_MULTITHREAD
  AV1CdefRowSync *const cdef_row_mt = cdef_sync->cdef_row_mt;
  pthread_mutex_lock(cdef_row_mt[row].row_mutex_);
  pthread_cond_signal(cdef_row_mt[row].row_cond_);
  cdef_row_mt[row].is_row_done = 1;
  pthread_mutex_unlock(cdef_row_mt[row].row_mutex_);
#else
  (void)cdef_sync;
  (void)row;
#endif  // CONFIG_MULTITHREAD
}

static INLINE void sync_read(AV1LfSync *const lf_sync, int r, int c,
                             int plane) {
#if CONFIG_MULTITHREAD
  const int nsync = lf_sync->sync_range;

  if (r && !(c & (nsync - 1))) {
    pthread_mutex_t *const mutex = &lf_sync->mutex_[plane][r - 1];
    pthread_mutex_lock(mutex);

    while (c > lf_sync->cur_sb_col[plane][r - 1] - nsync) {
      pthread_cond_wait(&lf_sync->cond_[plane][r - 1], mutex);
    }
    pthread_mutex_unlock(mutex);
  }
#else
  (void)lf_sync;
  (void)r;
  (void)c;
  (void)plane;
#endif  // CONFIG_MULTITHREAD
}

static INLINE void sync_write(AV1LfSync *const lf_sync, int r, int c,
                              const int sb_cols, int plane) {
#if CONFIG_MULTITHREAD
  const int nsync = lf_sync->sync_range;
  int cur;
  // Only signal when there are enough filtered SB for next row to run.
  int sig = 1;

  if (c < sb_cols - 1) {
    cur = c;
    if (c % nsync) sig = 0;
  } else {
    cur = sb_cols + nsync;
  }

  if (sig) {
    pthread_mutex_lock(&lf_sync->mutex_[plane][r]);

    // When a thread encounters an error, cur_sb_col[plane][r] is set to maximum
    // column number. In this case, the AOMMAX operation here ensures that
    // cur_sb_col[plane][r] is not overwritten with a smaller value thus
    // preventing the infinite waiting of threads in the relevant sync_read()
    // function.
    lf_sync->cur_sb_col[plane][r] = AOMMAX(lf_sync->cur_sb_col[plane][r], cur);

    pthread_cond_broadcast(&lf_sync->cond_[plane][r]);
    pthread_mutex_unlock(&lf_sync->mutex_[plane][r]);
  }
#else
  (void)lf_sync;
  (void)r;
  (void)c;
  (void)sb_cols;
  (void)plane;
#endif  // CONFIG_MULTITHREAD
}

// One job of row loopfiltering.
void av1_thread_loop_filter_rows(
    const YV12_BUFFER_CONFIG *const frame_buffer, AV1_COMMON *const cm,
    struct macroblockd_plane *planes, MACROBLOCKD *xd, int mi_row, int plane,
    int dir, int lpf_opt_level, AV1LfSync *const lf_sync,
    struct aom_internal_error_info *error_info,
    AV1_DEBLOCKING_PARAMETERS *params_buf, TX_SIZE *tx_buf,
    int num_mis_in_lpf_unit_height_log2) {
  // TODO(aomedia:3276): Pass error_info to the low-level functions as required
  // in future to handle error propagation.
  (void)error_info;
  const int sb_cols =
      CEIL_POWER_OF_TWO(cm->mi_params.mi_cols, MAX_MIB_SIZE_LOG2);
  const int r = mi_row >> num_mis_in_lpf_unit_height_log2;
  int mi_col, c;

  const bool joint_filter_chroma = (lpf_opt_level == 2) && plane > AOM_PLANE_Y;
  const int num_planes = joint_filter_chroma ? 2 : 1;
  assert(IMPLIES(joint_filter_chroma, plane == AOM_PLANE_U));

  if (dir == 0) {
    for (mi_col = 0; mi_col < cm->mi_params.mi_cols; mi_col += MAX_MIB_SIZE) {
      c = mi_col >> MAX_MIB_SIZE_LOG2;

      av1_setup_dst_planes(planes, cm->seq_params->sb_size, frame_buffer,
                           mi_row, mi_col, plane, plane + num_planes);
      if (lpf_opt_level) {
        if (plane == AOM_PLANE_Y) {
          av1_filter_block_plane_vert_opt(cm, xd, &planes[plane], mi_row,
                                          mi_col, params_buf, tx_buf,
                                          num_mis_in_lpf_unit_height_log2);
        } else {
          av1_filter_block_plane_vert_opt_chroma(
              cm, xd, &planes[plane], mi_row, mi_col, params_buf, tx_buf, plane,
              joint_filter_chroma, num_mis_in_lpf_unit_height_log2);
        }
      } else {
        av1_filter_block_plane_vert(cm, xd, plane, &planes[plane], mi_row,
                                    mi_col);
      }
      if (lf_sync != NULL) {
        sync_write(lf_sync, r, c, sb_cols, plane);
      }
    }
  } else if (dir == 1) {
    for (mi_col = 0; mi_col < cm->mi_params.mi_cols; mi_col += MAX_MIB_SIZE) {
      c = mi_col >> MAX_MIB_SIZE_LOG2;

      if (lf_sync != NULL) {
        // Wait for vertical edge filtering of the top-right block to be
        // completed
        sync_read(lf_sync, r, c, plane);

        // Wait for vertical edge filtering of the right block to be completed
        sync_read(lf_sync, r + 1, c, plane);
      }

#if CONFIG_MULTITHREAD
      if (lf_sync && lf_sync->num_workers > 1) {
        pthread_mutex_lock(lf_sync->job_mutex);
        const bool lf_mt_exit = lf_sync->lf_mt_exit;
        pthread_mutex_unlock(lf_sync->job_mutex);
        // Exit in case any worker has encountered an error.
        if (lf_mt_exit) return;
      }
#endif

      av1_setup_dst_planes(planes, cm->seq_params->sb_size, frame_buffer,
                           mi_row, mi_col, plane, plane + num_planes);
      if (lpf_opt_level) {
        if (plane == AOM_PLANE_Y) {
          av1_filter_block_plane_horz_opt(cm, xd, &planes[plane], mi_row,
                                          mi_col, params_buf, tx_buf,
                                          num_mis_in_lpf_unit_height_log2);
        } else {
          av1_filter_block_plane_horz_opt_chroma(
              cm, xd, &planes[plane], mi_row, mi_col, params_buf, tx_buf, plane,
              joint_filter_chroma, num_mis_in_lpf_unit_height_log2);
        }
      } else {
        av1_filter_block_plane_horz(cm, xd, plane, &planes[plane], mi_row,
                                    mi_col);
      }
    }
  }
}

void av1_set_vert_loop_filter_done(AV1_COMMON *cm, AV1LfSync *lf_sync,
                                   int num_mis_in_lpf_unit_height_log2) {
  int plane, sb_row;
  const int sb_cols =
      CEIL_POWER_OF_TWO(cm->mi_params.mi_cols, num_mis_in_lpf_unit_height_log2);
  const int sb_rows =
      CEIL_POWER_OF_TWO(cm->mi_params.mi_rows, num_mis_in_lpf_unit_height_log2);

  // In case of loopfilter row-multithreading, the worker on an SB row waits for
  // the vertical edge filtering of the right and top-right SBs. Hence, in case
  // a thread (main/worker) encounters an error, update that vertical
  // loopfiltering of every SB row in the frame is complete in order to avoid
  // dependent workers waiting indefinitely.
  for (sb_row = 0; sb_row < sb_rows; ++sb_row)
    for (plane = 0; plane < MAX_MB_PLANE; ++plane)
      sync_write(lf_sync, sb_row, sb_cols - 1, sb_cols, plane);
}

static AOM_INLINE void sync_lf_workers(AVxWorker *const workers,
                                       AV1_COMMON *const cm, int num_workers) {
  const AVxWorkerInterface *const winterface = aom_get_worker_interface();
  int had_error = workers[0].had_error;
  struct aom_internal_error_info error_info;

  // Read the error_info of main thread.
  if (had_error) {
    AVxWorker *const worker = &workers[0];
    error_info = ((LFWorkerData *)worker->data2)->error_info;
  }

  // Wait till all rows are finished.
  for (int i = num_workers - 1; i > 0; --i) {
    AVxWorker *const worker = &workers[i];
    if (!winterface->sync(worker)) {
      had_error = 1;
      error_info = ((LFWorkerData *)worker->data2)->error_info;
    }
  }
  if (had_error) aom_internal_error_copy(cm->error, &error_info);
}

// Row-based multi-threaded loopfilter hook
static int loop_filter_row_worker(void *arg1, void *arg2) {
  AV1LfSync *const lf_sync = (AV1LfSync *)arg1;
  LFWorkerData *const lf_data = (LFWorkerData *)arg2;
  AV1LfMTInfo *cur_job_info;

#if CONFIG_MULTITHREAD
  pthread_mutex_t *job_mutex_ = lf_sync->job_mutex;
#endif

  struct aom_internal_error_info *const error_info = &lf_data->error_info;

  // The jmp_buf is valid only for the duration of the function that calls
  // setjmp(). Therefore, this function must reset the 'setjmp' field to 0
  // before it returns.
  if (setjmp(error_info->jmp)) {
    error_info->setjmp = 0;
#if CONFIG_MULTITHREAD
    pthread_mutex_lock(job_mutex_);
    lf_sync->lf_mt_exit = true;
    pthread_mutex_unlock(job_mutex_);
#endif
    av1_set_vert_loop_filter_done(lf_data->cm, lf_sync, MAX_MIB_SIZE_LOG2);
    return 0;
  }
  error_info->setjmp = 1;

  while ((cur_job_info = get_lf_job_info(lf_sync)) != NULL) {
    const int lpf_opt_level = cur_job_info->lpf_opt_level;
    av1_thread_loop_filter_rows(
        lf_data->frame_buffer, lf_data->cm, lf_data->planes, lf_data->xd,
        cur_job_info->mi_row, cur_job_info->plane, cur_job_info->dir,
        lpf_opt_level, lf_sync, error_info, lf_data->params_buf,
        lf_data->tx_buf, MAX_MIB_SIZE_LOG2);
  }
  error_info->setjmp = 0;
  return 1;
}

static void loop_filter_rows_mt(YV12_BUFFER_CONFIG *frame, AV1_COMMON *cm,
                                MACROBLOCKD *xd, int start, int stop,
                                const int planes_to_lf[MAX_MB_PLANE],
                                AVxWorker *workers, int num_workers,
                                AV1LfSync *lf_sync, int lpf_opt_level) {
  const AVxWorkerInterface *const winterface = aom_get_worker_interface();
  int i;
  loop_filter_frame_mt_init(cm, start, stop, planes_to_lf, num_workers, lf_sync,
                            lpf_opt_level, MAX_MIB_SIZE_LOG2);

  // Set up loopfilter thread data.
  for (i = num_workers - 1; i >= 0; --i) {
    AVxWorker *const worker = &workers[i];
    LFWorkerData *const lf_data = &lf_sync->lfdata[i];

    worker->hook = loop_filter_row_worker;
    worker->data1 = lf_sync;
    worker->data2 = lf_data;

    // Loopfilter data
    loop_filter_data_reset(lf_data, frame, cm, xd);

    // Start loopfiltering
    worker->had_error = 0;
    if (i == 0) {
      winterface->execute(worker);
    } else {
      winterface->launch(worker);
    }
  }

  sync_lf_workers(workers, cm, num_workers);
}

static void loop_filter_rows(YV12_BUFFER_CONFIG *frame, AV1_COMMON *cm,
                             MACROBLOCKD *xd, int start, int stop,
                             const int planes_to_lf[MAX_MB_PLANE],
                             int lpf_opt_level) {
  // Filter top rows of all planes first, in case the output can be partially
  // reconstructed row by row.
  int mi_row, plane, dir;

  AV1_DEBLOCKING_PARAMETERS params_buf[MAX_MIB_SIZE];
  TX_SIZE tx_buf[MAX_MIB_SIZE];
  for (mi_row = start; mi_row < stop; mi_row += MAX_MIB_SIZE) {
    for (plane = 0; plane < MAX_MB_PLANE; ++plane) {
      if (skip_loop_filter_plane(planes_to_lf, plane, lpf_opt_level)) {
        continue;
      }

      for (dir = 0; dir < 2; ++dir) {
        av1_thread_loop_filter_rows(frame, cm, xd->plane, xd, mi_row, plane,
                                    dir, lpf_opt_level, /*lf_sync=*/NULL,
                                    xd->error_info, params_buf, tx_buf,
                                    MAX_MIB_SIZE_LOG2);
      }
    }
  }
}

void av1_loop_filter_frame_mt(YV12_BUFFER_CONFIG *frame, AV1_COMMON *cm,
                              MACROBLOCKD *xd, int plane_start, int plane_end,
                              int partial_frame, AVxWorker *workers,
                              int num_workers, AV1LfSync *lf_sync,
                              int lpf_opt_level) {
  int start_mi_row, end_mi_row, mi_rows_to_filter;
  int planes_to_lf[MAX_MB_PLANE];

  if (!check_planes_to_loop_filter(&cm->lf, planes_to_lf, plane_start,
                                   plane_end))
    return;

  start_mi_row = 0;
  mi_rows_to_filter = cm->mi_params.mi_rows;
  if (partial_frame && cm->mi_params.mi_rows > 8) {
    start_mi_row = cm->mi_params.mi_rows >> 1;
    start_mi_row &= 0xfffffff8;
    mi_rows_to_filter = AOMMAX(cm->mi_params.mi_rows / 8, 8);
  }
  end_mi_row = start_mi_row + mi_rows_to_filter;
  av1_loop_filter_frame_init(cm, plane_start, plane_end);

  if (num_workers > 1) {
    // Enqueue and execute loopfiltering jobs.
    loop_filter_rows_mt(frame, cm, xd, start_mi_row, end_mi_row, planes_to_lf,
                        workers, num_workers, lf_sync, lpf_opt_level);
  } else {
    // Directly filter in the main thread.
    loop_filter_rows(frame, cm, xd, start_mi_row, end_mi_row, planes_to_lf,
                     lpf_opt_level);
  }
}

static INLINE void lr_sync_read(void *const lr_sync, int r, int c, int plane) {
#if CONFIG_MULTITHREAD
  AV1LrSync *const loop_res_sync = (AV1LrSync *)lr_sync;
  const int nsync = loop_res_sync->sync_range;

  if (r && !(c & (nsync - 1))) {
    pthread_mutex_t *const mutex = &loop_res_sync->mutex_[plane][r - 1];
    pthread_mutex_lock(mutex);

    while (c > loop_res_sync->cur_sb_col[plane][r - 1] - nsync) {
      pthread_cond_wait(&loop_res_sync->cond_[plane][r - 1], mutex);
    }
    pthread_mutex_unlock(mutex);
  }
#else
  (void)lr_sync;
  (void)r;
  (void)c;
  (void)plane;
#endif  // CONFIG_MULTITHREAD
}

static INLINE void lr_sync_write(void *const lr_sync, int r, int c,
                                 const int sb_cols, int plane) {
#if CONFIG_MULTITHREAD
  AV1LrSync *const loop_res_sync = (AV1LrSync *)lr_sync;
  const int nsync = loop_res_sync->sync_range;
  int cur;
  // Only signal when there are enough filtered SB for next row to run.
  int sig = 1;

  if (c < sb_cols - 1) {
    cur = c;
    if (c % nsync) sig = 0;
  } else {
    cur = sb_cols + nsync;
  }

  if (sig) {
    pthread_mutex_lock(&loop_res_sync->mutex_[plane][r]);

    // When a thread encounters an error, cur_sb_col[plane][r] is set to maximum
    // column number. In this case, the AOMMAX operation here ensures that
    // cur_sb_col[plane][r] is not overwritten with a smaller value thus
    // preventing the infinite waiting of threads in the relevant sync_read()
    // function.
    loop_res_sync->cur_sb_col[plane][r] =
        AOMMAX(loop_res_sync->cur_sb_col[plane][r], cur);

    pthread_cond_broadcast(&loop_res_sync->cond_[plane][r]);
    pthread_mutex_unlock(&loop_res_sync->mutex_[plane][r]);
  }
#else
  (void)lr_sync;
  (void)r;
  (void)c;
  (void)sb_cols;
  (void)plane;
#endif  // CONFIG_MULTITHREAD
}

// Allocate memory for loop restoration row synchronization
void av1_loop_restoration_alloc(AV1LrSync *lr_sync, AV1_COMMON *cm,
                                int num_workers, int num_rows_lr,
                                int num_planes, int width) {
  lr_sync->rows = num_rows_lr;
  lr_sync->num_planes = num_planes;
#if CONFIG_MULTITHREAD
  {
    int i, j;

    for (j = 0; j < num_planes; j++) {
      CHECK_MEM_ERROR(cm, lr_sync->mutex_[j],
                      aom_malloc(sizeof(*(lr_sync->mutex_[j])) * num_rows_lr));
      if (lr_sync->mutex_[j]) {
        for (i = 0; i < num_rows_lr; ++i) {
          pthread_mutex_init(&lr_sync->mutex_[j][i], NULL);
        }
      }

      CHECK_MEM_ERROR(cm, lr_sync->cond_[j],
                      aom_malloc(sizeof(*(lr_sync->cond_[j])) * num_rows_lr));
      if (lr_sync->cond_[j]) {
        for (i = 0; i < num_rows_lr; ++i) {
          pthread_cond_init(&lr_sync->cond_[j][i], NULL);
        }
      }
    }

    CHECK_MEM_ERROR(cm, lr_sync->job_mutex,
                    aom_malloc(sizeof(*(lr_sync->job_mutex))));
    if (lr_sync->job_mutex) {
      pthread_mutex_init(lr_sync->job_mutex, NULL);
    }
  }
#endif  // CONFIG_MULTITHREAD
  CHECK_MEM_ERROR(cm, lr_sync->lrworkerdata,
                  aom_calloc(num_workers, sizeof(*(lr_sync->lrworkerdata))));
  lr_sync->num_workers = num_workers;

  for (int worker_idx = 0; worker_idx < num_workers; ++worker_idx) {
    if (worker_idx < num_workers - 1) {
      CHECK_MEM_ERROR(cm, lr_sync->lrworkerdata[worker_idx].rst_tmpbuf,
                      (int32_t *)aom_memalign(16, RESTORATION_TMPBUF_SIZE));
      CHECK_MEM_ERROR(cm, lr_sync->lrworkerdata[worker_idx].rlbs,
                      aom_malloc(sizeof(RestorationLineBuffers)));

    } else {
      lr_sync->lrworkerdata[worker_idx].rst_tmpbuf = cm->rst_tmpbuf;
      lr_sync->lrworkerdata[worker_idx].rlbs = cm->rlbs;
    }
  }

  for (int j = 0; j < num_planes; j++) {
    CHECK_MEM_ERROR(
        cm, lr_sync->cur_sb_col[j],
        aom_malloc(sizeof(*(lr_sync->cur_sb_col[j])) * num_rows_lr));
  }
  CHECK_MEM_ERROR(
      cm, lr_sync->job_queue,
      aom_malloc(sizeof(*(lr_sync->job_queue)) * num_rows_lr * num_planes));
  // Set up nsync.
  lr_sync->sync_range = get_lr_sync_range(width);
}

// Deallocate loop restoration synchronization related mutex and data
void av1_loop_restoration_dealloc(AV1LrSync *lr_sync) {
  if (lr_sync != NULL) {
    int j;
#if CONFIG_MULTITHREAD
    int i;
    for (j = 0; j < MAX_MB_PLANE; j++) {
      if (lr_sync->mutex_[j] != NULL) {
        for (i = 0; i < lr_sync->rows; ++i) {
          pthread_mutex_destroy(&lr_sync->mutex_[j][i]);
        }
        aom_free(lr_sync->mutex_[j]);
      }
      if (lr_sync->cond_[j] != NULL) {
        for (i = 0; i < lr_sync->rows; ++i) {
          pthread_cond_destroy(&lr_sync->cond_[j][i]);
        }
        aom_free(lr_sync->cond_[j]);
      }
    }
    if (lr_sync->job_mutex != NULL) {
      pthread_mutex_destroy(lr_sync->job_mutex);
      aom_free(lr_sync->job_mutex);
    }
#endif  // CONFIG_MULTITHREAD
    for (j = 0; j < MAX_MB_PLANE; j++) {
      aom_free(lr_sync->cur_sb_col[j]);
    }

    aom_free(lr_sync->job_queue);

    if (lr_sync->lrworkerdata) {
      for (int worker_idx = 0; worker_idx < lr_sync->num_workers - 1;
           worker_idx++) {
        LRWorkerData *const workerdata_data =
            lr_sync->lrworkerdata + worker_idx;

        aom_free(workerdata_data->rst_tmpbuf);
        aom_free(workerdata_data->rlbs);
      }
      aom_free(lr_sync->lrworkerdata);
    }

    // clear the structure as the source of this call may be a resize in which
    // case this call will be followed by an _alloc() which may fail.
    av1_zero(*lr_sync);
  }
}

static void enqueue_lr_jobs(AV1LrSync *lr_sync, AV1LrStruct *lr_ctxt,
                            AV1_COMMON *cm) {
  FilterFrameCtxt *ctxt = lr_ctxt->ctxt;

  const int num_planes = av1_num_planes(cm);
  AV1LrMTInfo *lr_job_queue = lr_sync->job_queue;
  int32_t lr_job_counter[2], num_even_lr_jobs = 0;
  lr_sync->jobs_enqueued = 0;
  lr_sync->jobs_dequeued = 0;

  for (int plane = 0; plane < num_planes; plane++) {
    if (cm->rst_info[plane].frame_restoration_type == RESTORE_NONE) continue;
    num_even_lr_jobs =
        num_even_lr_jobs + ((ctxt[plane].rsi->vert_units + 1) >> 1);
  }
  lr_job_counter[0] = 0;
  lr_job_counter[1] = num_even_lr_jobs;

  for (int plane = 0; plane < num_planes; plane++) {
    if (cm->rst_info[plane].frame_restoration_type == RESTORE_NONE) continue;
    const int is_uv = plane > 0;
    const int ss_y = is_uv && cm->seq_params->subsampling_y;
    const int unit_size = ctxt[plane].rsi->restoration_unit_size;
    const int plane_h = ctxt[plane].plane_h;
    const int ext_size = unit_size * 3 / 2;

    int y0 = 0, i = 0;
    while (y0 < plane_h) {
      int remaining_h = plane_h - y0;
      int h = (remaining_h < ext_size) ? remaining_h : unit_size;

      RestorationTileLimits limits;
      limits.v_start = y0;
      limits.v_end = y0 + h;
      assert(limits.v_end <= plane_h);
      // Offset upwards to align with the restoration processing stripe
      const int voffset = RESTORATION_UNIT_OFFSET >> ss_y;
      limits.v_start = AOMMAX(0, limits.v_start - voffset);
      if (limits.v_end < plane_h) limits.v_end -= voffset;

      assert(lr_job_counter[0] <= num_even_lr_jobs);

      lr_job_queue[lr_job_counter[i & 1]].lr_unit_row = i;
      lr_job_queue[lr_job_counter[i & 1]].plane = plane;
      lr_job_queue[lr_job_counter[i & 1]].v_start = limits.v_start;
      lr_job_queue[lr_job_counter[i & 1]].v_end = limits.v_end;
      lr_job_queue[lr_job_counter[i & 1]].sync_mode = i & 1;
      if ((i & 1) == 0) {
        lr_job_queue[lr_job_counter[i & 1]].v_copy_start =
            limits.v_start + RESTORATION_BORDER;
        lr_job_queue[lr_job_counter[i & 1]].v_copy_end =
            limits.v_end - RESTORATION_BORDER;
        if (i == 0) {
          assert(limits.v_start == 0);
          lr_job_queue[lr_job_counter[i & 1]].v_copy_start = 0;
        }
        if (i == (ctxt[plane].rsi->vert_units - 1)) {
          assert(limits.v_end == plane_h);
          lr_job_queue[lr_job_counter[i & 1]].v_copy_end = plane_h;
        }
      } else {
        lr_job_queue[lr_job_counter[i & 1]].v_copy_start =
            AOMMAX(limits.v_start - RESTORATION_BORDER, 0);
        lr_job_queue[lr_job_counter[i & 1]].v_copy_end =
            AOMMIN(limits.v_end + RESTORATION_BORDER, plane_h);
      }
      lr_job_counter[i & 1]++;
      lr_sync->jobs_enqueued++;

      y0 += h;
      ++i;
    }
  }
}

static AV1LrMTInfo *get_lr_job_info(AV1LrSync *lr_sync) {
  AV1LrMTInfo *cur_job_info = NULL;

#if CONFIG_MULTITHREAD
  pthread_mutex_lock(lr_sync->job_mutex);

  if (!lr_sync->lr_mt_exit && lr_sync->jobs_dequeued < lr_sync->jobs_enqueued) {
    cur_job_info = lr_sync->job_queue + lr_sync->jobs_dequeued;
    lr_sync->jobs_dequeued++;
  }

  pthread_mutex_unlock(lr_sync->job_mutex);
#else
  (void)lr_sync;
#endif

  return cur_job_info;
}

static void set_loop_restoration_done(AV1LrSync *const lr_sync,
                                      FilterFrameCtxt *const ctxt) {
  for (int plane = 0; plane < MAX_MB_PLANE; ++plane) {
    if (ctxt[plane].rsi->frame_restoration_type == RESTORE_NONE) continue;
    int y0 = 0, row_number = 0;
    const int unit_size = ctxt[plane].rsi->restoration_unit_size;
    const int plane_h = ctxt[plane].plane_h;
    const int ext_size = unit_size * 3 / 2;
    const int hnum_rest_units = ctxt[plane].rsi->horz_units;
    while (y0 < plane_h) {
      const int remaining_h = plane_h - y0;
      const int h = (remaining_h < ext_size) ? remaining_h : unit_size;
      lr_sync_write(lr_sync, row_number, hnum_rest_units - 1, hnum_rest_units,
                    plane);
      y0 += h;
      ++row_number;
    }
  }
}

// Implement row loop restoration for each thread.
static int loop_restoration_row_worker(void *arg1, void *arg2) {
  AV1LrSync *const lr_sync = (AV1LrSync *)arg1;
  LRWorkerData *lrworkerdata = (LRWorkerData *)arg2;
  AV1LrStruct *lr_ctxt = (AV1LrStruct *)lrworkerdata->lr_ctxt;
  FilterFrameCtxt *ctxt = lr_ctxt->ctxt;
  int lr_unit_row;
  int plane;
  int plane_w;
#if CONFIG_MULTITHREAD
  pthread_mutex_t *job_mutex_ = lr_sync->job_mutex;
#endif
  struct aom_internal_error_info *const error_info = &lrworkerdata->error_info;

  // The jmp_buf is valid only for the duration of the function that calls
  // setjmp(). Therefore, this function must reset the 'setjmp' field to 0
  // before it returns.
  if (setjmp(error_info->jmp)) {
    error_info->setjmp = 0;
#if CONFIG_MULTITHREAD
    pthread_mutex_lock(job_mutex_);
    lr_sync->lr_mt_exit = true;
    pthread_mutex_unlock(job_mutex_);
#endif
    // In case of loop restoration multithreading, the worker on an even lr
    // block row waits for the completion of the filtering of the top-right and
    // bottom-right blocks. Hence, in case a thread (main/worker) encounters an
    // error, update that filtering of every row in the frame is complete in
    // order to avoid the dependent workers from waiting indefinitely.
    set_loop_restoration_done(lr_sync, lr_ctxt->ctxt);
    return 0;
  }
  error_info->setjmp = 1;

  typedef void (*copy_fun)(const YV12_BUFFER_CONFIG *src_ybc,
                           YV12_BUFFER_CONFIG *dst_ybc, int hstart, int hend,
                           int vstart, int vend);
  static const copy_fun copy_funs[MAX_MB_PLANE] = {
    aom_yv12_partial_coloc_copy_y, aom_yv12_partial_coloc_copy_u,
    aom_yv12_partial_coloc_copy_v
  };

  while (1) {
    AV1LrMTInfo *cur_job_info = get_lr_job_info(lr_sync);
    if (cur_job_info != NULL) {
      RestorationTileLimits limits;
      sync_read_fn_t on_sync_read;
      sync_write_fn_t on_sync_write;
      limits.v_start = cur_job_info->v_start;
      limits.v_end = cur_job_info->v_end;
      lr_unit_row = cur_job_info->lr_unit_row;
      plane = cur_job_info->plane;
      plane_w = ctxt[plane].plane_w;

      // sync_mode == 1 implies only sync read is required in LR Multi-threading
      // sync_mode == 0 implies only sync write is required.
      on_sync_read =
          cur_job_info->sync_mode == 1 ? lr_sync_read : av1_lr_sync_read_dummy;
      on_sync_write = cur_job_info->sync_mode == 0 ? lr_sync_write
                                                   : av1_lr_sync_write_dummy;

      av1_foreach_rest_unit_in_row(
          &limits, plane_w, lr_ctxt->on_rest_unit, lr_unit_row,
          ctxt[plane].rsi->restoration_unit_size, ctxt[plane].rsi->horz_units,
          ctxt[plane].rsi->vert_units, plane, &ctxt[plane],
          lrworkerdata->rst_tmpbuf, lrworkerdata->rlbs, on_sync_read,
          on_sync_write, lr_sync, error_info);

      copy_funs[plane](lr_ctxt->dst, lr_ctxt->frame, 0, plane_w,
                       cur_job_info->v_copy_start, cur_job_info->v_copy_end);

      if (lrworkerdata->do_extend_border) {
        aom_extend_frame_borders_plane_row(lr_ctxt->frame, plane,
                                           cur_job_info->v_copy_start,
                                           cur_job_info->v_copy_end);
      }
    } else {
      break;
    }
  }
  error_info->setjmp = 0;
  return 1;
}

static AOM_INLINE void sync_lr_workers(AVxWorker *const workers,
                                       AV1_COMMON *const cm, int num_workers) {
  const AVxWorkerInterface *const winterface = aom_get_worker_interface();
  int had_error = workers[0].had_error;
  struct aom_internal_error_info error_info;

  // Read the error_info of main thread.
  if (had_error) {
    AVxWorker *const worker = &workers[0];
    error_info = ((LRWorkerData *)worker->data2)->error_info;
  }

  // Wait till all rows are finished.
  for (int i = num_workers - 1; i > 0; --i) {
    AVxWorker *const worker = &workers[i];
    if (!winterface->sync(worker)) {
      had_error = 1;
      error_info = ((LRWorkerData *)worker->data2)->error_info;
    }
  }
  if (had_error) aom_internal_error_copy(cm->error, &error_info);
}

static void foreach_rest_unit_in_planes_mt(AV1LrStruct *lr_ctxt,
                                           AVxWorker *workers, int num_workers,
                                           AV1LrSync *lr_sync, AV1_COMMON *cm,
                                           int do_extend_border) {
  FilterFrameCtxt *ctxt = lr_ctxt->ctxt;

  const int num_planes = av1_num_planes(cm);

  const AVxWorkerInterface *const winterface = aom_get_worker_interface();
  int num_rows_lr = 0;

  for (int plane = 0; plane < num_planes; plane++) {
    if (cm->rst_info[plane].frame_restoration_type == RESTORE_NONE) continue;

    const int plane_h = ctxt[plane].plane_h;
    const int unit_size = cm->rst_info[plane].restoration_unit_size;

    num_rows_lr = AOMMAX(num_rows_lr, av1_lr_count_units(unit_size, plane_h));
  }

  int i;
  assert(MAX_MB_PLANE == 3);

  if (!lr_sync->sync_range || num_rows_lr > lr_sync->rows ||
      num_workers > lr_sync->num_workers || num_planes > lr_sync->num_planes) {
    av1_loop_restoration_dealloc(lr_sync);
    av1_loop_restoration_alloc(lr_sync, cm, num_workers, num_rows_lr,
                               num_planes, cm->width);
  }
  lr_sync->lr_mt_exit = false;

  // Initialize cur_sb_col to -1 for all SB rows.
  for (i = 0; i < num_planes; i++) {
    memset(lr_sync->cur_sb_col[i], -1,
           sizeof(*(lr_sync->cur_sb_col[i])) * num_rows_lr);
  }

  enqueue_lr_jobs(lr_sync, lr_ctxt, cm);

  // Set up looprestoration thread data.
  for (i = num_workers - 1; i >= 0; --i) {
    AVxWorker *const worker = &workers[i];
    lr_sync->lrworkerdata[i].lr_ctxt = (void *)lr_ctxt;
    lr_sync->lrworkerdata[i].do_extend_border = do_extend_border;
    worker->hook = loop_restoration_row_worker;
    worker->data1 = lr_sync;
    worker->data2 = &lr_sync->lrworkerdata[i];

    // Start loop restoration
    worker->had_error = 0;
    if (i == 0) {
      winterface->execute(worker);
    } else {
      winterface->launch(worker);
    }
  }

  sync_lr_workers(workers, cm, num_workers);
}

void av1_loop_restoration_filter_frame_mt(YV12_BUFFER_CONFIG *frame,
                                          AV1_COMMON *cm, int optimized_lr,
                                          AVxWorker *workers, int num_workers,
                                          AV1LrSync *lr_sync, void *lr_ctxt,
                                          int do_extend_border) {
  assert(!cm->features.all_lossless);

  const int num_planes = av1_num_planes(cm);

  AV1LrStruct *loop_rest_ctxt = (AV1LrStruct *)lr_ctxt;

  av1_loop_restoration_filter_frame_init(loop_rest_ctxt, frame, cm,
                                         optimized_lr, num_planes);

  foreach_rest_unit_in_planes_mt(loop_rest_ctxt, workers, num_workers, lr_sync,
                                 cm, do_extend_border);
}

// Initializes cdef_sync parameters.
static AOM_INLINE void reset_cdef_job_info(AV1CdefSync *const cdef_sync) {
  cdef_sync->end_of_frame = 0;
  cdef_sync->fbr = 0;
  cdef_sync->fbc = 0;
  cdef_sync->cdef_mt_exit = false;
}

static AOM_INLINE void launch_cdef_workers(AVxWorker *const workers,
                                           int num_workers) {
  const AVxWorkerInterface *const winterface = aom_get_worker_interface();
  for (int i = num_workers - 1; i >= 0; i--) {
    AVxWorker *const worker = &workers[i];
    worker->had_error = 0;
    if (i == 0)
      winterface->execute(worker);
    else
      winterface->launch(worker);
  }
}

static AOM_INLINE void sync_cdef_workers(AVxWorker *const workers,
                                         AV1_COMMON *const cm,
                                         int num_workers) {
  const AVxWorkerInterface *const winterface = aom_get_worker_interface();
  int had_error = workers[0].had_error;
  struct aom_internal_error_info error_info;

  // Read the error_info of main thread.
  if (had_error) {
    AVxWorker *const worker = &workers[0];
    error_info = ((AV1CdefWorkerData *)worker->data2)->error_info;
  }

  // Wait till all rows are finished.
  for (int i = num_workers - 1; i > 0; --i) {
    AVxWorker *const worker = &workers[i];
    if (!winterface->sync(worker)) {
      had_error = 1;
      error_info = ((AV1CdefWorkerData *)worker->data2)->error_info;
    }
  }
  if (had_error) aom_internal_error_copy(cm->error, &error_info);
}

// Updates the row index of the next job to be processed.
// Also updates end_of_frame flag when the processing of all rows is complete.
static void update_cdef_row_next_job_info(AV1CdefSync *const cdef_sync,
                                          const int nvfb) {
  cdef_sync->fbr++;
  if (cdef_sync->fbr == nvfb) {
    cdef_sync->end_of_frame = 1;
  }
}

// Checks if a job is available. If job is available,
// populates next job information and returns 1, else returns 0.
static AOM_INLINE int get_cdef_row_next_job(AV1CdefSync *const cdef_sync,
                                            volatile int *cur_fbr,
                                            const int nvfb) {
#if CONFIG_MULTITHREAD
  pthread_mutex_lock(cdef_sync->mutex_);
#endif  // CONFIG_MULTITHREAD
  int do_next_row = 0;
  // Populates information needed for current job and update the row
  // index of the next row to be processed.
  if (!cdef_sync->cdef_mt_exit && cdef_sync->end_of_frame == 0) {
    do_next_row = 1;
    *cur_fbr = cdef_sync->fbr;
    update_cdef_row_next_job_info(cdef_sync, nvfb);
  }
#if CONFIG_MULTITHREAD
  pthread_mutex_unlock(cdef_sync->mutex_);
#endif  // CONFIG_MULTITHREAD
  return do_next_row;
}

static void set_cdef_init_fb_row_done(AV1CdefSync *const cdef_sync, int nvfb) {
  for (int fbr = 0; fbr < nvfb; fbr++) cdef_row_mt_sync_write(cdef_sync, fbr);
}

// Hook function for each thread in CDEF multi-threading.
static int cdef_sb_row_worker_hook(void *arg1, void *arg2) {
  AV1CdefSync *const cdef_sync = (AV1CdefSync *)arg1;
  AV1CdefWorkerData *const cdef_worker = (AV1CdefWorkerData *)arg2;
  AV1_COMMON *cm = cdef_worker->cm;
  const int nvfb = (cm->mi_params.mi_rows + MI_SIZE_64X64 - 1) / MI_SIZE_64X64;

#if CONFIG_MULTITHREAD
  pthread_mutex_t *job_mutex_ = cdef_sync->mutex_;
#endif
  struct aom_internal_error_info *const error_info = &cdef_worker->error_info;

  // The jmp_buf is valid only for the duration of the function that calls
  // setjmp(). Therefore, this function must reset the 'setjmp' field to 0
  // before it returns.
  if (setjmp(error_info->jmp)) {
    error_info->setjmp = 0;
#if CONFIG_MULTITHREAD
    pthread_mutex_lock(job_mutex_);
    cdef_sync->cdef_mt_exit = true;
    pthread_mutex_unlock(job_mutex_);
#endif
    // In case of cdef row-multithreading, the worker on a filter block row
    // (fbr) waits for the line buffers (top and bottom) copy of the above row.
    // Hence, in case a thread (main/worker) encounters an error before copying
    // of the line buffers, update that line buffer copy is complete in order to
    // avoid dependent workers waiting indefinitely.
    set_cdef_init_fb_row_done(cdef_sync, nvfb);
    return 0;
  }
  error_info->setjmp = 1;

  volatile int cur_fbr;
  const int num_planes = av1_num_planes(cm);
  while (get_cdef_row_next_job(cdef_sync, &cur_fbr, nvfb)) {
    MACROBLOCKD *xd = cdef_worker->xd;
    av1_cdef_fb_row(cm, xd, cdef_worker->linebuf, cdef_worker->colbuf,
                    cdef_worker->srcbuf, cur_fbr,
                    cdef_worker->cdef_init_fb_row_fn, cdef_sync, error_info);
    if (cdef_worker->do_extend_border) {
      for (int plane = 0; plane < num_planes; ++plane) {
        const YV12_BUFFER_CONFIG *ybf = &cm->cur_frame->buf;
        const int is_uv = plane > 0;
        const int mi_high = MI_SIZE_LOG2 - xd->plane[plane].subsampling_y;
        const int unit_height = MI_SIZE_64X64 << mi_high;
        const int v_start = cur_fbr * unit_height;
        const int v_end =
            AOMMIN(v_start + unit_height, ybf->crop_heights[is_uv]);
        aom_extend_frame_borders_plane_row(ybf, plane, v_start, v_end);
      }
    }
  }
  error_info->setjmp = 0;
  return 1;
}

// Assigns CDEF hook function and thread data to each worker.
static void prepare_cdef_frame_workers(
    AV1_COMMON *const cm, MACROBLOCKD *xd, AV1CdefWorkerData *const cdef_worker,
    AVxWorkerHook hook, AVxWorker *const workers, AV1CdefSync *const cdef_sync,
    int num_workers, cdef_init_fb_row_t cdef_init_fb_row_fn,
    int do_extend_border) {
  const int num_planes = av1_num_planes(cm);

  cdef_worker[0].srcbuf = cm->cdef_info.srcbuf;
  for (int plane = 0; plane < num_planes; plane++)
    cdef_worker[0].colbuf[plane] = cm->cdef_info.colbuf[plane];
  for (int i = num_workers - 1; i >= 0; i--) {
    AVxWorker *const worker = &workers[i];
    cdef_worker[i].cm = cm;
    cdef_worker[i].xd = xd;
    cdef_worker[i].cdef_init_fb_row_fn = cdef_init_fb_row_fn;
    cdef_worker[i].do_extend_border = do_extend_border;
    for (int plane = 0; plane < num_planes; plane++)
      cdef_worker[i].linebuf[plane] = cm->cdef_info.linebuf[plane];

    worker->hook = hook;
    worker->data1 = cdef_sync;
    worker->data2 = &cdef_worker[i];
  }
}

// Initializes row-level parameters for CDEF frame.
void av1_cdef_init_fb_row_mt(const AV1_COMMON *const cm,
                             const MACROBLOCKD *const xd,
                             CdefBlockInfo *const fb_info,
                             uint16_t **const linebuf, uint16_t *const src,
                             struct AV1CdefSyncData *const cdef_sync, int fbr) {
  const int num_planes = av1_num_planes(cm);
  const int nvfb = (cm->mi_params.mi_rows + MI_SIZE_64X64 - 1) / MI_SIZE_64X64;
  const int luma_stride =
      ALIGN_POWER_OF_TWO(cm->mi_params.mi_cols << MI_SIZE_LOG2, 4);

  // for the current filter block, it's top left corner mi structure (mi_tl)
  // is first accessed to check whether the top and left boundaries are
  // frame boundaries. Then bottom-left and top-right mi structures are
  // accessed to check whether the bottom and right boundaries
  // (respectively) are frame boundaries.
  //
  // Note that we can't just check the bottom-right mi structure - eg. if
  // we're at the right-hand edge of the frame but not the bottom, then
  // the bottom-right mi is NULL but the bottom-left is not.
  fb_info->frame_boundary[TOP] = (MI_SIZE_64X64 * fbr == 0) ? 1 : 0;
  if (fbr != nvfb - 1)
    fb_info->frame_boundary[BOTTOM] =
        (MI_SIZE_64X64 * (fbr + 1) == cm->mi_params.mi_rows) ? 1 : 0;
  else
    fb_info->frame_boundary[BOTTOM] = 1;

  fb_info->src = src;
  fb_info->damping = cm->cdef_info.cdef_damping;
  fb_info->coeff_shift = AOMMAX(cm->seq_params->bit_depth - 8, 0);
  av1_zero(fb_info->dir);
  av1_zero(fb_info->var);

  for (int plane = 0; plane < num_planes; plane++) {
    const int stride = luma_stride >> xd->plane[plane].subsampling_x;
    uint16_t *top_linebuf = &linebuf[plane][0];
    uint16_t *bot_linebuf = &linebuf[plane][nvfb * CDEF_VBORDER * stride];
    {
      const int mi_high_l2 = MI_SIZE_LOG2 - xd->plane[plane].subsampling_y;
      const int top_offset = MI_SIZE_64X64 * (fbr + 1) << mi_high_l2;
      const int bot_offset = MI_SIZE_64X64 * (fbr + 1) << mi_high_l2;

      if (fbr != nvfb - 1)  // if (fbr != 0)  // top line buffer copy
        av1_cdef_copy_sb8_16(
            cm, &top_linebuf[(fbr + 1) * CDEF_VBORDER * stride], stride,
            xd->plane[plane].dst.buf, top_offset - CDEF_VBORDER, 0,
            xd->plane[plane].dst.stride, CDEF_VBORDER, stride);
      if (fbr != nvfb - 1)  // bottom line buffer copy
        av1_cdef_copy_sb8_16(cm, &bot_linebuf[fbr * CDEF_VBORDER * stride],
                             stride, xd->plane[plane].dst.buf, bot_offset, 0,
                             xd->plane[plane].dst.stride, CDEF_VBORDER, stride);
    }

    fb_info->top_linebuf[plane] = &linebuf[plane][fbr * CDEF_VBORDER * stride];
    fb_info->bot_linebuf[plane] =
        &linebuf[plane]
                [nvfb * CDEF_VBORDER * stride + (fbr * CDEF_VBORDER * stride)];
  }

  cdef_row_mt_sync_write(cdef_sync, fbr);
  cdef_row_mt_sync_read(cdef_sync, fbr);
}

// Implements multi-threading for CDEF.
// Perform CDEF on input frame.
// Inputs:
//   frame: Pointer to input frame buffer.
//   cm: Pointer to common structure.
//   xd: Pointer to common current coding block structure.
// Returns:
//   Nothing will be returned.
void av1_cdef_frame_mt(AV1_COMMON *const cm, MACROBLOCKD *const xd,
                       AV1CdefWorkerData *const cdef_worker,
                       AVxWorker *const workers, AV1CdefSync *const cdef_sync,
                       int num_workers, cdef_init_fb_row_t cdef_init_fb_row_fn,
                       int do_extend_border) {
  YV12_BUFFER_CONFIG *frame = &cm->cur_frame->buf;
  const int num_planes = av1_num_planes(cm);

  av1_setup_dst_planes(xd->plane, cm->seq_params->sb_size, frame, 0, 0, 0,
                       num_planes);

  reset_cdef_job_info(cdef_sync);
  prepare_cdef_frame_workers(cm, xd, cdef_worker, cdef_sb_row_worker_hook,
                             workers, cdef_sync, num_workers,
                             cdef_init_fb_row_fn, do_extend_border);
  launch_cdef_workers(workers, num_workers);
  sync_cdef_workers(workers, cm, num_workers);
}

int av1_get_intrabc_extra_top_right_sb_delay(const AV1_COMMON *cm) {
  // No additional top-right delay when intraBC tool is not enabled.
  if (!av1_allow_intrabc(cm)) return 0;
  // Due to the hardware constraints on processing the intraBC tool with row
  // multithreading, a top-right delay of 3 superblocks of size 128x128 or 5
  // superblocks of size 64x64 is mandated. However, a minimum top-right delay
  // of 1 superblock is assured with 'sync_range'. Hence return only the
  // additional superblock delay when the intraBC tool is enabled.
  return cm->seq_params->sb_size == BLOCK_128X128 ? 2 : 4;
}
