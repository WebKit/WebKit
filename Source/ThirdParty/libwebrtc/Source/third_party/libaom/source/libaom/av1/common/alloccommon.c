/*
 *
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "config/aom_config.h"

#include "aom_mem/aom_mem.h"
#include "aom_scale/yv12config.h"
#include "aom_util/aom_pthread.h"

#include "av1/common/alloccommon.h"
#include "av1/common/av1_common_int.h"
#include "av1/common/blockd.h"
#include "av1/common/cdef_block.h"
#include "av1/common/entropymode.h"
#include "av1/common/entropymv.h"
#include "av1/common/enums.h"
#include "av1/common/restoration.h"
#include "av1/common/thread_common.h"

int av1_get_MBs(int width, int height) {
  const int aligned_width = ALIGN_POWER_OF_TWO(width, 3);
  const int aligned_height = ALIGN_POWER_OF_TWO(height, 3);
  const int mi_cols = aligned_width >> MI_SIZE_LOG2;
  const int mi_rows = aligned_height >> MI_SIZE_LOG2;

  const int mb_cols = ROUND_POWER_OF_TWO(mi_cols, 2);
  const int mb_rows = ROUND_POWER_OF_TWO(mi_rows, 2);
  return mb_rows * mb_cols;
}

void av1_free_ref_frame_buffers(BufferPool *pool) {
  int i;

  for (i = 0; i < pool->num_frame_bufs; ++i) {
    if (pool->frame_bufs[i].ref_count > 0 &&
        pool->frame_bufs[i].raw_frame_buffer.data != NULL) {
      pool->release_fb_cb(pool->cb_priv, &pool->frame_bufs[i].raw_frame_buffer);
      pool->frame_bufs[i].raw_frame_buffer.data = NULL;
      pool->frame_bufs[i].raw_frame_buffer.size = 0;
      pool->frame_bufs[i].raw_frame_buffer.priv = NULL;
      pool->frame_bufs[i].ref_count = 0;
    }
    aom_free(pool->frame_bufs[i].mvs);
    pool->frame_bufs[i].mvs = NULL;
    aom_free(pool->frame_bufs[i].seg_map);
    pool->frame_bufs[i].seg_map = NULL;
    aom_free_frame_buffer(&pool->frame_bufs[i].buf);
  }
  aom_free(pool->frame_bufs);
  pool->frame_bufs = NULL;
  pool->num_frame_bufs = 0;
}

static INLINE void free_cdef_linebuf_conditional(
    AV1_COMMON *const cm, const size_t *new_linebuf_size) {
  CdefInfo *cdef_info = &cm->cdef_info;
  for (int plane = 0; plane < MAX_MB_PLANE; plane++) {
    if (new_linebuf_size[plane] != cdef_info->allocated_linebuf_size[plane]) {
      aom_free(cdef_info->linebuf[plane]);
      cdef_info->linebuf[plane] = NULL;
    }
  }
}

static INLINE void free_cdef_bufs_conditional(AV1_COMMON *const cm,
                                              uint16_t **colbuf,
                                              uint16_t **srcbuf,
                                              const size_t *new_colbuf_size,
                                              const size_t new_srcbuf_size) {
  CdefInfo *cdef_info = &cm->cdef_info;
  if (new_srcbuf_size != cdef_info->allocated_srcbuf_size) {
    aom_free(*srcbuf);
    *srcbuf = NULL;
  }
  for (int plane = 0; plane < MAX_MB_PLANE; plane++) {
    if (new_colbuf_size[plane] != cdef_info->allocated_colbuf_size[plane]) {
      aom_free(colbuf[plane]);
      colbuf[plane] = NULL;
    }
  }
}

static INLINE void free_cdef_bufs(uint16_t **colbuf, uint16_t **srcbuf) {
  aom_free(*srcbuf);
  *srcbuf = NULL;
  for (int plane = 0; plane < MAX_MB_PLANE; plane++) {
    aom_free(colbuf[plane]);
    colbuf[plane] = NULL;
  }
}

static INLINE void free_cdef_row_sync(AV1CdefRowSync **cdef_row_mt,
                                      const int num_mi_rows) {
  if (*cdef_row_mt == NULL) return;
#if CONFIG_MULTITHREAD
  for (int row_idx = 0; row_idx < num_mi_rows; row_idx++) {
    if ((*cdef_row_mt)[row_idx].row_mutex_ != NULL) {
      pthread_mutex_destroy((*cdef_row_mt)[row_idx].row_mutex_);
      aom_free((*cdef_row_mt)[row_idx].row_mutex_);
    }
    if ((*cdef_row_mt)[row_idx].row_cond_ != NULL) {
      pthread_cond_destroy((*cdef_row_mt)[row_idx].row_cond_);
      aom_free((*cdef_row_mt)[row_idx].row_cond_);
    }
  }
#else
  (void)num_mi_rows;
#endif  // CONFIG_MULTITHREAD
  aom_free(*cdef_row_mt);
  *cdef_row_mt = NULL;
}

void av1_free_cdef_buffers(AV1_COMMON *const cm,
                           AV1CdefWorkerData **cdef_worker,
                           AV1CdefSync *cdef_sync) {
  CdefInfo *cdef_info = &cm->cdef_info;
  const int num_mi_rows = cdef_info->allocated_mi_rows;

  for (int plane = 0; plane < MAX_MB_PLANE; plane++) {
    aom_free(cdef_info->linebuf[plane]);
    cdef_info->linebuf[plane] = NULL;
  }
  // De-allocation of column buffer & source buffer (worker_0).
  free_cdef_bufs(cdef_info->colbuf, &cdef_info->srcbuf);

  free_cdef_row_sync(&cdef_sync->cdef_row_mt, num_mi_rows);

  if (cdef_info->allocated_num_workers < 2) return;
  if (*cdef_worker != NULL) {
    for (int idx = cdef_info->allocated_num_workers - 1; idx >= 1; idx--) {
      // De-allocation of column buffer & source buffer for remaining workers.
      free_cdef_bufs((*cdef_worker)[idx].colbuf, &(*cdef_worker)[idx].srcbuf);
    }
    aom_free(*cdef_worker);
    *cdef_worker = NULL;
  }
}

static INLINE void alloc_cdef_linebuf(AV1_COMMON *const cm, uint16_t **linebuf,
                                      const int num_planes) {
  CdefInfo *cdef_info = &cm->cdef_info;
  for (int plane = 0; plane < num_planes; plane++) {
    if (linebuf[plane] == NULL)
      CHECK_MEM_ERROR(cm, linebuf[plane],
                      aom_malloc(cdef_info->allocated_linebuf_size[plane]));
  }
}

static INLINE void alloc_cdef_bufs(AV1_COMMON *const cm, uint16_t **colbuf,
                                   uint16_t **srcbuf, const int num_planes) {
  CdefInfo *cdef_info = &cm->cdef_info;
  if (*srcbuf == NULL)
    CHECK_MEM_ERROR(cm, *srcbuf,
                    aom_memalign(16, cdef_info->allocated_srcbuf_size));

  for (int plane = 0; plane < num_planes; plane++) {
    if (colbuf[plane] == NULL)
      CHECK_MEM_ERROR(cm, colbuf[plane],
                      aom_malloc(cdef_info->allocated_colbuf_size[plane]));
  }
}

static INLINE void alloc_cdef_row_sync(AV1_COMMON *const cm,
                                       AV1CdefRowSync **cdef_row_mt,
                                       const int num_mi_rows) {
  if (*cdef_row_mt != NULL) return;

  CHECK_MEM_ERROR(cm, *cdef_row_mt,
                  aom_calloc(num_mi_rows, sizeof(**cdef_row_mt)));
#if CONFIG_MULTITHREAD
  for (int row_idx = 0; row_idx < num_mi_rows; row_idx++) {
    CHECK_MEM_ERROR(cm, (*cdef_row_mt)[row_idx].row_mutex_,
                    aom_malloc(sizeof(*(*cdef_row_mt)[row_idx].row_mutex_)));
    pthread_mutex_init((*cdef_row_mt)[row_idx].row_mutex_, NULL);

    CHECK_MEM_ERROR(cm, (*cdef_row_mt)[row_idx].row_cond_,
                    aom_malloc(sizeof(*(*cdef_row_mt)[row_idx].row_cond_)));
    pthread_cond_init((*cdef_row_mt)[row_idx].row_cond_, NULL);
  }
#endif  // CONFIG_MULTITHREAD
}

void av1_alloc_cdef_buffers(AV1_COMMON *const cm,
                            AV1CdefWorkerData **cdef_worker,
                            AV1CdefSync *cdef_sync, int num_workers,
                            int init_worker) {
  const int num_planes = av1_num_planes(cm);
  size_t new_linebuf_size[MAX_MB_PLANE] = { 0 };
  size_t new_colbuf_size[MAX_MB_PLANE] = { 0 };
  size_t new_srcbuf_size = 0;
  CdefInfo *const cdef_info = &cm->cdef_info;
  // Check for configuration change
  const int num_mi_rows =
      (cm->mi_params.mi_rows + MI_SIZE_64X64 - 1) / MI_SIZE_64X64;
  const int is_num_workers_changed =
      cdef_info->allocated_num_workers != num_workers;
  const int is_cdef_enabled =
      cm->seq_params->enable_cdef && !cm->tiles.single_tile_decoding;

  // num-bufs=3 represents ping-pong buffers for top linebuf,
  // followed by bottom linebuf.
  // ping-pong is to avoid top linebuf over-write by consecutive row.
  int num_bufs = 3;
  if (num_workers > 1)
    num_bufs = (cm->mi_params.mi_rows + MI_SIZE_64X64 - 1) / MI_SIZE_64X64;

  if (is_cdef_enabled) {
    // Calculate src buffer size
    new_srcbuf_size = sizeof(*cdef_info->srcbuf) * CDEF_INBUF_SIZE;
    for (int plane = 0; plane < num_planes; plane++) {
      const int shift =
          plane == AOM_PLANE_Y ? 0 : cm->seq_params->subsampling_x;
      // Calculate top and bottom line buffer size
      const int luma_stride =
          ALIGN_POWER_OF_TWO(cm->mi_params.mi_cols << MI_SIZE_LOG2, 4);
      new_linebuf_size[plane] = sizeof(*cdef_info->linebuf) * num_bufs *
                                (CDEF_VBORDER << 1) * (luma_stride >> shift);
      // Calculate column buffer size
      const int block_height =
          (CDEF_BLOCKSIZE << (MI_SIZE_LOG2 - shift)) * 2 * CDEF_VBORDER;
      new_colbuf_size[plane] =
          sizeof(*cdef_info->colbuf[plane]) * block_height * CDEF_HBORDER;
    }
  }

  // Free src, line and column buffers for worker 0 in case of reallocation
  free_cdef_linebuf_conditional(cm, new_linebuf_size);
  free_cdef_bufs_conditional(cm, cdef_info->colbuf, &cdef_info->srcbuf,
                             new_colbuf_size, new_srcbuf_size);

  // The flag init_worker indicates if cdef_worker has to be allocated for the
  // frame. This is passed as 1 always from decoder. At encoder side, it is 0
  // when called for parallel frames during FPMT (where cdef_worker is shared
  // across parallel frames) and 1 otherwise.
  if (*cdef_worker != NULL && init_worker) {
    if (is_num_workers_changed) {
      // Free src and column buffers for remaining workers in case of change in
      // num_workers
      for (int idx = cdef_info->allocated_num_workers - 1; idx >= 1; idx--)
        free_cdef_bufs((*cdef_worker)[idx].colbuf, &(*cdef_worker)[idx].srcbuf);

      aom_free(*cdef_worker);
      *cdef_worker = NULL;
    } else if (num_workers > 1) {
      // Free src and column buffers for remaining workers in case of
      // reallocation
      for (int idx = num_workers - 1; idx >= 1; idx--)
        free_cdef_bufs_conditional(cm, (*cdef_worker)[idx].colbuf,
                                   &(*cdef_worker)[idx].srcbuf, new_colbuf_size,
                                   new_srcbuf_size);
    }
  }

  if (cdef_info->allocated_mi_rows != num_mi_rows)
    free_cdef_row_sync(&cdef_sync->cdef_row_mt, cdef_info->allocated_mi_rows);

  // Store allocated sizes for reallocation
  cdef_info->allocated_srcbuf_size = new_srcbuf_size;
  av1_copy(cdef_info->allocated_colbuf_size, new_colbuf_size);
  av1_copy(cdef_info->allocated_linebuf_size, new_linebuf_size);
  // Store configuration to check change in configuration
  cdef_info->allocated_mi_rows = num_mi_rows;
  cdef_info->allocated_num_workers = num_workers;

  if (!is_cdef_enabled) return;

  // Memory allocation of column buffer & source buffer (worker_0).
  alloc_cdef_bufs(cm, cdef_info->colbuf, &cdef_info->srcbuf, num_planes);
  alloc_cdef_linebuf(cm, cdef_info->linebuf, num_planes);

  if (num_workers < 2) return;

  if (init_worker) {
    if (*cdef_worker == NULL)
      CHECK_MEM_ERROR(cm, *cdef_worker,
                      aom_calloc(num_workers, sizeof(**cdef_worker)));

    // Memory allocation of column buffer & source buffer for remaining workers.
    for (int idx = num_workers - 1; idx >= 1; idx--)
      alloc_cdef_bufs(cm, (*cdef_worker)[idx].colbuf,
                      &(*cdef_worker)[idx].srcbuf, num_planes);
  }

  alloc_cdef_row_sync(cm, &cdef_sync->cdef_row_mt,
                      cdef_info->allocated_mi_rows);
}

// Allocate buffers which are independent of restoration_unit_size
void av1_alloc_restoration_buffers(AV1_COMMON *cm, bool is_sgr_enabled) {
  const int num_planes = av1_num_planes(cm);

  if (cm->rst_tmpbuf == NULL && is_sgr_enabled) {
    CHECK_MEM_ERROR(cm, cm->rst_tmpbuf,
                    (int32_t *)aom_memalign(16, RESTORATION_TMPBUF_SIZE));
  }

  if (cm->rlbs == NULL) {
    CHECK_MEM_ERROR(cm, cm->rlbs, aom_malloc(sizeof(RestorationLineBuffers)));
  }

  // For striped loop restoration, we divide each plane into "stripes",
  // of height 64 luma pixels but with an offset by RESTORATION_UNIT_OFFSET
  // luma pixels to match the output from CDEF. We will need to store 2 *
  // RESTORATION_CTX_VERT lines of data for each stripe.
  int mi_h = cm->mi_params.mi_rows;
  const int ext_h = RESTORATION_UNIT_OFFSET + (mi_h << MI_SIZE_LOG2);
  const int num_stripes = (ext_h + 63) / 64;

  // Now we need to allocate enough space to store the line buffers for the
  // stripes
  const int frame_w = cm->superres_upscaled_width;
  const int use_highbd = cm->seq_params->use_highbitdepth;

  for (int p = 0; p < num_planes; ++p) {
    const int is_uv = p > 0;
    const int ss_x = is_uv && cm->seq_params->subsampling_x;
    const int plane_w = ((frame_w + ss_x) >> ss_x) + 2 * RESTORATION_EXTRA_HORZ;
    const int stride = ALIGN_POWER_OF_TWO(plane_w, 5);
    const int buf_size = num_stripes * stride * RESTORATION_CTX_VERT
                         << use_highbd;
    RestorationStripeBoundaries *boundaries = &cm->rst_info[p].boundaries;

    if (buf_size != boundaries->stripe_boundary_size ||
        boundaries->stripe_boundary_above == NULL ||
        boundaries->stripe_boundary_below == NULL) {
      aom_free(boundaries->stripe_boundary_above);
      aom_free(boundaries->stripe_boundary_below);

      CHECK_MEM_ERROR(cm, boundaries->stripe_boundary_above,
                      (uint8_t *)aom_memalign(32, buf_size));
      CHECK_MEM_ERROR(cm, boundaries->stripe_boundary_below,
                      (uint8_t *)aom_memalign(32, buf_size));

      boundaries->stripe_boundary_size = buf_size;
    }
    boundaries->stripe_boundary_stride = stride;
  }
}

void av1_free_restoration_buffers(AV1_COMMON *cm) {
  int p;
  for (p = 0; p < MAX_MB_PLANE; ++p)
    av1_free_restoration_struct(&cm->rst_info[p]);
  aom_free(cm->rst_tmpbuf);
  cm->rst_tmpbuf = NULL;
  aom_free(cm->rlbs);
  cm->rlbs = NULL;
  for (p = 0; p < MAX_MB_PLANE; ++p) {
    RestorationStripeBoundaries *boundaries = &cm->rst_info[p].boundaries;
    aom_free(boundaries->stripe_boundary_above);
    aom_free(boundaries->stripe_boundary_below);
    boundaries->stripe_boundary_above = NULL;
    boundaries->stripe_boundary_below = NULL;
  }

  aom_free_frame_buffer(&cm->rst_frame);
}

void av1_free_above_context_buffers(CommonContexts *above_contexts) {
  int i;
  const int num_planes = above_contexts->num_planes;

  for (int tile_row = 0; tile_row < above_contexts->num_tile_rows; tile_row++) {
    for (i = 0; i < num_planes; i++) {
      if (above_contexts->entropy[i] == NULL) break;
      aom_free(above_contexts->entropy[i][tile_row]);
      above_contexts->entropy[i][tile_row] = NULL;
    }
    if (above_contexts->partition != NULL) {
      aom_free(above_contexts->partition[tile_row]);
      above_contexts->partition[tile_row] = NULL;
    }

    if (above_contexts->txfm != NULL) {
      aom_free(above_contexts->txfm[tile_row]);
      above_contexts->txfm[tile_row] = NULL;
    }
  }
  for (i = 0; i < num_planes; i++) {
    aom_free(above_contexts->entropy[i]);
    above_contexts->entropy[i] = NULL;
  }
  aom_free(above_contexts->partition);
  above_contexts->partition = NULL;

  aom_free(above_contexts->txfm);
  above_contexts->txfm = NULL;

  above_contexts->num_tile_rows = 0;
  above_contexts->num_mi_cols = 0;
  above_contexts->num_planes = 0;
}

void av1_free_context_buffers(AV1_COMMON *cm) {
  if (cm->mi_params.free_mi != NULL) cm->mi_params.free_mi(&cm->mi_params);

  av1_free_above_context_buffers(&cm->above_contexts);
}

int av1_alloc_above_context_buffers(CommonContexts *above_contexts,
                                    int num_tile_rows, int num_mi_cols,
                                    int num_planes) {
  const int aligned_mi_cols =
      ALIGN_POWER_OF_TWO(num_mi_cols, MAX_MIB_SIZE_LOG2);

  // Allocate above context buffers
  above_contexts->num_tile_rows = num_tile_rows;
  above_contexts->num_mi_cols = aligned_mi_cols;
  above_contexts->num_planes = num_planes;
  for (int plane_idx = 0; plane_idx < num_planes; plane_idx++) {
    above_contexts->entropy[plane_idx] = (ENTROPY_CONTEXT **)aom_calloc(
        num_tile_rows, sizeof(above_contexts->entropy[0]));
    if (!above_contexts->entropy[plane_idx]) return 1;
  }

  above_contexts->partition = (PARTITION_CONTEXT **)aom_calloc(
      num_tile_rows, sizeof(above_contexts->partition));
  if (!above_contexts->partition) return 1;

  above_contexts->txfm =
      (TXFM_CONTEXT **)aom_calloc(num_tile_rows, sizeof(above_contexts->txfm));
  if (!above_contexts->txfm) return 1;

  for (int tile_row = 0; tile_row < num_tile_rows; tile_row++) {
    for (int plane_idx = 0; plane_idx < num_planes; plane_idx++) {
      above_contexts->entropy[plane_idx][tile_row] =
          (ENTROPY_CONTEXT *)aom_calloc(
              aligned_mi_cols, sizeof(*above_contexts->entropy[0][tile_row]));
      if (!above_contexts->entropy[plane_idx][tile_row]) return 1;
    }

    above_contexts->partition[tile_row] = (PARTITION_CONTEXT *)aom_calloc(
        aligned_mi_cols, sizeof(*above_contexts->partition[tile_row]));
    if (!above_contexts->partition[tile_row]) return 1;

    above_contexts->txfm[tile_row] = (TXFM_CONTEXT *)aom_calloc(
        aligned_mi_cols, sizeof(*above_contexts->txfm[tile_row]));
    if (!above_contexts->txfm[tile_row]) return 1;
  }

  return 0;
}

// Allocate the dynamically allocated arrays in 'mi_params' assuming
// 'mi_params->set_mb_mi()' was already called earlier to initialize the rest of
// the struct members.
static int alloc_mi(CommonModeInfoParams *mi_params) {
  const int aligned_mi_rows = calc_mi_size(mi_params->mi_rows);
  const int mi_grid_size = mi_params->mi_stride * aligned_mi_rows;
  const int alloc_size_1d = mi_size_wide[mi_params->mi_alloc_bsize];
  const int alloc_mi_size =
      mi_params->mi_alloc_stride * (aligned_mi_rows / alloc_size_1d);

  if (mi_params->mi_alloc_size < alloc_mi_size ||
      mi_params->mi_grid_size < mi_grid_size) {
    mi_params->free_mi(mi_params);

    mi_params->mi_alloc =
        aom_calloc(alloc_mi_size, sizeof(*mi_params->mi_alloc));
    if (!mi_params->mi_alloc) return 1;
    mi_params->mi_alloc_size = alloc_mi_size;

    mi_params->mi_grid_base = (MB_MODE_INFO **)aom_calloc(
        mi_grid_size, sizeof(*mi_params->mi_grid_base));
    if (!mi_params->mi_grid_base) return 1;

    mi_params->tx_type_map =
        aom_calloc(mi_grid_size, sizeof(*mi_params->tx_type_map));
    if (!mi_params->tx_type_map) return 1;
    mi_params->mi_grid_size = mi_grid_size;
  }

  return 0;
}

int av1_alloc_context_buffers(AV1_COMMON *cm, int width, int height,
                              BLOCK_SIZE min_partition_size) {
  CommonModeInfoParams *const mi_params = &cm->mi_params;
  mi_params->set_mb_mi(mi_params, width, height, min_partition_size);
  if (alloc_mi(mi_params)) goto fail;
  return 0;

fail:
  // clear the mi_* values to force a realloc on resync
  mi_params->set_mb_mi(mi_params, 0, 0, BLOCK_4X4);
  av1_free_context_buffers(cm);
  return 1;
}

void av1_remove_common(AV1_COMMON *cm) {
  av1_free_context_buffers(cm);

  aom_free(cm->fc);
  cm->fc = NULL;
  aom_free(cm->default_frame_context);
  cm->default_frame_context = NULL;
}

void av1_init_mi_buffers(CommonModeInfoParams *mi_params) {
  mi_params->setup_mi(mi_params);
}
