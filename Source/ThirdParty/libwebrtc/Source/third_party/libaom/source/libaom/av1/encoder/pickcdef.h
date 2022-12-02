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
#ifndef AOM_AV1_ENCODER_PICKCDEF_H_
#define AOM_AV1_ENCODER_PICKCDEF_H_

#include "av1/common/cdef.h"
#include "av1/encoder/speed_features.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!\enum CDEF_CONTROL
 * \brief This enum controls to which frames CDEF is applied.
 */
typedef enum {
  CDEF_NONE = 0,      /*!< Disable CDEF on all frames. */
  CDEF_ALL = 1,       /*!< Enable CDEF for all frames. */
  CDEF_REFERENCE = 2, /*!< Disable CDEF on non reference frames. */
} CDEF_CONTROL;

/*!\cond */
struct MultiThreadInfo;

#define REDUCED_PRI_STRENGTHS_LVL1 8
#define REDUCED_PRI_STRENGTHS_LVL2 5
#define REDUCED_SEC_STRENGTHS_LVL3 2
#define REDUCED_SEC_STRENGTHS_LVL5 1
#define REDUCED_PRI_STRENGTHS_LVL4 2

#define REDUCED_TOTAL_STRENGTHS_LVL1 \
  (REDUCED_PRI_STRENGTHS_LVL1 * CDEF_SEC_STRENGTHS)
#define REDUCED_TOTAL_STRENGTHS_LVL2 \
  (REDUCED_PRI_STRENGTHS_LVL2 * CDEF_SEC_STRENGTHS)
#define REDUCED_TOTAL_STRENGTHS_LVL3 \
  (REDUCED_PRI_STRENGTHS_LVL2 * REDUCED_SEC_STRENGTHS_LVL3)
#define REDUCED_TOTAL_STRENGTHS_LVL4 \
  (REDUCED_PRI_STRENGTHS_LVL4 * REDUCED_SEC_STRENGTHS_LVL3)
#define REDUCED_TOTAL_STRENGTHS_LVL5 \
  (REDUCED_PRI_STRENGTHS_LVL4 * REDUCED_SEC_STRENGTHS_LVL5)
#define TOTAL_STRENGTHS (CDEF_PRI_STRENGTHS * CDEF_SEC_STRENGTHS)

static const int priconv_lvl1[REDUCED_PRI_STRENGTHS_LVL1] = { 0, 1, 2,  3,
                                                              5, 7, 10, 13 };
static const int priconv_lvl2[REDUCED_PRI_STRENGTHS_LVL2] = { 0, 2, 4, 8, 14 };
static const int priconv_lvl4[REDUCED_PRI_STRENGTHS_LVL4] = { 0, 11 };
static const int priconv_lvl5[REDUCED_PRI_STRENGTHS_LVL4] = { 0, 5 };
static const int secconv_lvl3[REDUCED_SEC_STRENGTHS_LVL3] = { 0, 2 };
static const int secconv_lvl5[REDUCED_SEC_STRENGTHS_LVL5] = { 0 };
static const int nb_cdef_strengths[CDEF_PICK_METHODS] = {
  TOTAL_STRENGTHS,
  REDUCED_TOTAL_STRENGTHS_LVL1,
  REDUCED_TOTAL_STRENGTHS_LVL2,
  REDUCED_TOTAL_STRENGTHS_LVL3,
  REDUCED_TOTAL_STRENGTHS_LVL4,
  REDUCED_TOTAL_STRENGTHS_LVL5,
  TOTAL_STRENGTHS
};

typedef void (*copy_fn_t)(uint16_t *dst, int dstride, const void *src,
                          int src_voffset, int src_hoffset, int sstride,
                          int vsize, int hsize);
typedef uint64_t (*compute_cdef_dist_t)(void *dst, int dstride, uint16_t *src,
                                        cdef_list *dlist, int cdef_count,
                                        BLOCK_SIZE bsize, int coeff_shift,
                                        int row, int col);

/*! \brief CDEF search context.
 */
typedef struct {
  /*!
   * Pointer to the frame buffer holding the source frame
   */
  const YV12_BUFFER_CONFIG *ref;
  /*!
   * Pointer to params related to MB_MODE_INFO arrays and related info
   */
  CommonModeInfoParams *mi_params;
  /*!
   * Info specific to each plane
   */
  struct macroblockd_plane plane[MAX_MB_PLANE];
  /*!
   * Function pointer of copy_fn
   */
  copy_fn_t copy_fn;
  /*!
   * Function pointer of compute_cdef_dist_fn
   */
  compute_cdef_dist_t compute_cdef_dist_fn;
  /*!
   *  Number of strenghts evaluated in CDEF filter search
   */
  int total_strengths;
  /*!
   * Bit-depth dependent shift
   */
  int coeff_shift;
  /*!
   * CDEF damping factor
   */
  int damping;
  /*!
   * Search method used to select CDEF parameters
   */
  int pick_method;
  /*!
   * Number of planes
   */
  int num_planes;
  /*!
   * Log2 of width of the MI unit in pixels. mi_wide_l2[i]
   * indicates the width of the MI unit in pixels for the ith plane
   */
  int mi_wide_l2[MAX_MB_PLANE];
  /*!
   * Log2 of height of the MI unit in pixels. mi_high_l2[i]
   * indicates the height of the MI unit in pixels for the ith plane
   */
  int mi_high_l2[MAX_MB_PLANE];
  /*!
   * Subsampling in x direction. xdec[i] indicates the subsampling
   * for the ith plane
   */
  int xdec[MAX_MB_PLANE];
  /*!
   * Subsampling in y direction. ydec[i] indicates the subsampling
   * for the ith plane
   */
  int ydec[MAX_MB_PLANE];
  /*!
   * bsize[i] indicates the block size of ith plane
   */
  int bsize[MAX_MB_PLANE];
  /*!
   * Number of 64x64 blocks in vertical direction of a frame
   */
  int nvfb;
  /*!
   * Number of 64x64 blocks in horizontal direction of a frame
   */
  int nhfb;
  /*!
   * Pointer to the mean squared error between the CDEF filtered block and the
   * source block. mse[i][j][k] stores the MSE of the ith plane (i=0 corresponds
   * to Y-plane, i=1 corresponds to U and V planes), jth block and kth strength
   * index
   */
  uint64_t (*mse[2])[TOTAL_STRENGTHS];
  /*!
   * Holds the position (in units of mi's) of the cdef filtered
   * block in raster scan order
   */
  int *sb_index;
  /*!
   * Holds the count of cdef filtered blocks
   */
  int sb_count;
} CdefSearchCtx;

static INLINE int sb_all_skip(const CommonModeInfoParams *const mi_params,
                              int mi_row, int mi_col) {
  const int maxr = AOMMIN(mi_params->mi_rows - mi_row, MI_SIZE_64X64);
  const int maxc = AOMMIN(mi_params->mi_cols - mi_col, MI_SIZE_64X64);
  const int stride = mi_params->mi_stride;
  MB_MODE_INFO **mbmi = mi_params->mi_grid_base + mi_row * stride + mi_col;
  for (int r = 0; r < maxr; ++r, mbmi += stride) {
    for (int c = 0; c < maxc; ++c) {
      if (!mbmi[c]->skip_txfm) return 0;
    }
  }
  return 1;
}

// Checks if cdef processing can be skipped for particular sb.
// Inputs:
//   cdef_search_ctx: Pointer to the structure containing parameters related to
//   CDEF search context.
//   fbr: Row index in units of 64x64 block
//   fbc: Column index in units of 64x64 block
// Returns:
//   1/0 will be returned to indicate skip/don't skip cdef processing of sb
//   respectively.
static INLINE int cdef_sb_skip(const CommonModeInfoParams *const mi_params,
                               int fbr, int fbc) {
  const MB_MODE_INFO *const mbmi =
      mi_params->mi_grid_base[MI_SIZE_64X64 * fbr * mi_params->mi_stride +
                              MI_SIZE_64X64 * fbc];
  // No filtering if the entire filter block is skipped.
  if (sb_all_skip(mi_params, fbr * MI_SIZE_64X64, fbc * MI_SIZE_64X64))
    return 1;
  // Skip odd numbered 64x64 block rows(cols) when bsize is BLOCK_128X128,
  // BLOCK_64X128(BLOCK_128X128, BLOCK_128X64) as for such blocks CDEF filtering
  // is done at the corresponding block sizes.
  if (((fbc & 1) &&
       (mbmi->bsize == BLOCK_128X128 || mbmi->bsize == BLOCK_128X64)) ||
      ((fbr & 1) &&
       (mbmi->bsize == BLOCK_128X128 || mbmi->bsize == BLOCK_64X128)))
    return 1;
  return 0;
}

void av1_cdef_mse_calc_block(CdefSearchCtx *cdef_search_ctx, int fbr, int fbc,
                             int sb_count);
/*!\endcond */

/*!\brief AV1 CDEF parameter search
 *
 * \ingroup in_loop_cdef
 *
 * Searches for optimal CDEF parameters for frame
 *
 * \param[in]      mt_info      Pointer to multi-threading parameters
 * \param[in]      frame        Compressed frame buffer
 * \param[in]      ref          Source frame buffer
 * \param[in,out]  cm           Pointer to top level common structure
 * \param[in]      xd           Pointer to common current coding block structure
 * \param[in]      pick_method  The method used to select params
 * \param[in]      rdmult       rd multiplier to use in making param choices
 * \param[in]      skip_cdef_feature Speed feature to skip cdef
 * \param[in]      frames_since_key Number of frames since key frame
 * \param[in]      cdef_control  Parameter that controls CDEF application
 * \param[in]      is_screen_content   Whether it is screen content type
 * \param[in]      non_reference_frame Indicates if current frame is
 * non-reference
 *
 * \return Nothing is returned. Instead, optimal CDEF parameters are stored
 * in the \c cdef_info structure of type \ref CdefInfo inside \c cm:
 * \arg \c cdef_bits: Bits of strength parameters
 * \arg \c nb_cdef_strengths: Number of strength parameters
 * \arg \c cdef_strengths: list of \c nb_cdef_strengths strength parameters
 * for the luma plane.
 * \arg \c uv_cdef_strengths: list of \c nb_cdef_strengths strength parameters
 * for the chroma planes.
 * \arg \c damping_factor: CDEF damping factor.
 *
 */
void av1_cdef_search(struct MultiThreadInfo *mt_info,
                     const YV12_BUFFER_CONFIG *frame,
                     const YV12_BUFFER_CONFIG *ref, AV1_COMMON *cm,
                     MACROBLOCKD *xd, CDEF_PICK_METHOD pick_method, int rdmult,
                     int skip_cdef_feature, int frames_since_key,
                     CDEF_CONTROL cdef_control, const int is_screen_content,
                     int non_reference_frame);

#ifdef __cplusplus
}  // extern "C"
#endif
#endif  // AOM_AV1_ENCODER_PICKCDEF_H_
