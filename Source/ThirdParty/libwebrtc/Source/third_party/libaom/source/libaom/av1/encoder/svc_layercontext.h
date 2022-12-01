/*
 *  Copyright (c) 2019, Alliance for Open Media. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AOM_AV1_ENCODER_SVC_LAYERCONTEXT_H_
#define AOM_AV1_ENCODER_SVC_LAYERCONTEXT_H_

#include "av1/encoder/aq_cyclicrefresh.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/ratectrl.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * \brief The stucture of quantities related to each spatial and temporal layer.
 * \ingroup SVC
 */
typedef struct {
  /*!\cond */
  RATE_CONTROL rc;
  PRIMARY_RATE_CONTROL p_rc;
  int framerate_factor;
  int64_t layer_target_bitrate;
  int scaling_factor_num;
  int scaling_factor_den;
  int64_t target_bandwidth;
  int64_t spatial_layer_target_bandwidth;
  double framerate;
  int avg_frame_size;
  int max_q;
  int min_q;
  int frames_from_key_frame;
  /*!\endcond */

  /*!
   * Cyclic refresh parameters (aq-mode=3), that need to be updated per-frame.
   */
  int sb_index;
  /*!
   * Segmentation map
   */
  int8_t *map;
  /*!
   * Number of blocks on segment 1
   */
  int actual_num_seg1_blocks;

  /*!
   * Number of blocks on segment 2
   */
  int actual_num_seg2_blocks;
  /*!
   * Counter used to detect scene change.
   */
  int counter_encode_maxq_scene_change;

  /*!
   * Speed settings for each layer.
   */
  uint8_t speed;
  /*!
   * GF group index.
   */
  unsigned char group_index;
  /*!
   * If current layer is key frame.
   */
  int is_key_frame;
  /*!
   * Maximum motion magnitude of previous encoded layer.
   */
  int max_mv_magnitude;
} LAYER_CONTEXT;

/*!
 * \brief The stucture of SVC.
 * \ingroup SVC
 */
typedef struct SVC {
  /*!\cond */
  int spatial_layer_id;
  int temporal_layer_id;
  int number_spatial_layers;
  int number_temporal_layers;
  int set_ref_frame_config;
  int non_reference_frame;
  int use_flexible_mode;
  int ksvc_fixed_mode;
  int ref_frame_comp[3];
  /*!\endcond */

  /*!
   * LAST_FRAME (0), LAST2_FRAME(1), LAST3_FRAME(2), GOLDEN_FRAME(3),
   * BWDREF_FRAME(4), ALTREF2_FRAME(5), ALTREF_FRAME(6).
   */
  int reference[INTER_REFS_PER_FRAME];
  /*!\cond */
  int ref_idx[INTER_REFS_PER_FRAME];
  int refresh[REF_FRAMES];
  int gld_idx_1layer;
  double base_framerate;
  unsigned int current_superframe;
  unsigned int buffer_time_index[REF_FRAMES];
  unsigned char buffer_spatial_layer[REF_FRAMES];
  int skip_mvsearch_last;
  int skip_mvsearch_gf;
  int skip_mvsearch_altref;
  int spatial_layer_fb[REF_FRAMES];
  int temporal_layer_fb[REF_FRAMES];
  int num_encoded_top_layer;
  int first_layer_denoise;
  int high_source_sad_superframe;
  /*!\endcond */

  /*!
   * Layer context used for rate control in CBR mode.
   */
  LAYER_CONTEXT layer_context[AOM_MAX_LAYERS];

  /*!
   * EIGHTTAP_SMOOTH or BILINEAR
   */
  InterpFilter downsample_filter_type[AOM_MAX_SS_LAYERS];

  /*!
   * Downsample_filter_phase: = 0 will do sub-sampling (no weighted average),
   * = 8 will center the target pixel and get a symmetric averaging filter.
   */
  int downsample_filter_phase[AOM_MAX_SS_LAYERS];

  /*!
   * Force zero-mv in mode search for the spatial/inter-layer reference.
   */
  int force_zero_mode_spatial_ref;
} SVC;

struct AV1_COMP;

/*!\brief Initialize layer context data from init_config().
 *
 * \ingroup SVC
 * \callgraph
 * \callergraph
 *
 * \param[in]       cpi  Top level encoder structure
 *
 * \return  Nothing returned. Set cpi->svc.
 */
void av1_init_layer_context(struct AV1_COMP *const cpi);

/*!\brief Update the layer context from a change_config() call.
 *
 * \ingroup SVC
 * \callgraph
 * \callergraph
 *
 * \param[in]       cpi  Top level encoder structure
 * \param[in]       target_bandwidth  Total target bandwidth
 *
 * \return  Nothing returned. Buffer level for each layer is set.
 */
void av1_update_layer_context_change_config(struct AV1_COMP *const cpi,
                                            const int64_t target_bandwidth);

/*!\brief Prior to encoding the frame, update framerate-related quantities
          for the current temporal layer.
 *
 * \ingroup SVC
 * \callgraph
 * \callergraph
 *
 * \param[in]       cpi  Top level encoder structure
 *
 * \return  Nothing returned. Frame related quantities for current temporal
 layer are updated.
 */
void av1_update_temporal_layer_framerate(struct AV1_COMP *const cpi);

/*!\brief Prior to encoding the frame, set the layer context, for the current
 layer to be encoded, to the cpi struct.
 *
 * \ingroup SVC
 * \callgraph
 * \callergraph
 *
 * \param[in]       cpi  Top level encoder structure
 *
 * \return  Nothing returned. Layer context for current layer is set.
 */
void av1_restore_layer_context(struct AV1_COMP *const cpi);

/*!\brief Save the layer context after encoding the frame.
 *
 * \ingroup SVC
 * \callgraph
 * \callergraph
 *
 * \param[in]       cpi  Top level encoder structure
 *
 * \return  Nothing returned.
 */
void av1_save_layer_context(struct AV1_COMP *const cpi);

/*!\brief Free the memory used for cyclic refresh in layer context.
 *
 * \ingroup SVC
 * \callgraph
 * \callergraph
 *
 * \param[in]       cpi  Top level encoder structure
 *
 * \return  Nothing returned.
 */
void av1_free_svc_cyclic_refresh(struct AV1_COMP *const cpi);

/*!\brief Reset on key frame: reset counters, references and buffer updates.
 *
 * \ingroup SVC
 * \callgraph
 * \callergraph
 *
 * \param[in]       cpi  Top level encoder structure
 * \param[in]       is_key  Whether current layer is key frame
 *
 * \return  Nothing returned.
 */
void av1_svc_reset_temporal_layers(struct AV1_COMP *const cpi, int is_key);

/*!\brief Before encoding, set resolutions and allocate compressor data.
 *
 * \ingroup SVC
 * \callgraph
 * \callergraph
 *
 * \param[in]       cpi  Top level encoder structure
 *
 * \return  Nothing returned.
 */
void av1_one_pass_cbr_svc_start_layer(struct AV1_COMP *const cpi);

/*!\brief Get primary reference frame for current layer
 *
 * \ingroup SVC
 * \callgraph
 * \callergraph
 *
 * \param[in]       cpi  Top level encoder structure
 *
 * \return  The primary reference frame for current layer.
 */
int av1_svc_primary_ref_frame(const struct AV1_COMP *const cpi);

/*!\brief Get resolution for current layer.
 *
 * \ingroup SVC
 * \param[in]       width_org    Original width, unscaled
 * \param[in]       height_org   Original height, unscaled
 * \param[in]       num          Numerator for the scale ratio
 * \param[in]       den          Denominator for the scale ratio
 * \param[in]       width_out    Output width, scaled for current layer
 * \param[in]       height_out   Output height, scaled for current layer
 *
 * \return Nothing is returned. Instead the scaled width and height are set.
 */
void av1_get_layer_resolution(const int width_org, const int height_org,
                              const int num, const int den, int *width_out,
                              int *height_out);

void av1_set_svc_fixed_mode(struct AV1_COMP *const cpi);

void av1_svc_check_reset_layer_rc_flag(struct AV1_COMP *const cpi);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_ENCODER_SVC_LAYERCONTEXT_H_
