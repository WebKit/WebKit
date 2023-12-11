/*
 *  Copyright (c) 2020 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VPX_VPX_VPX_EXT_RATECTRL_H_
#define VPX_VPX_VPX_EXT_RATECTRL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "./vpx_integer.h"
#include "./vpx_tpl.h"

/*!\brief Current ABI version number
 *
 * \internal
 * If this file is altered in any way that changes the ABI, this value
 * must be bumped. Examples include, but are not limited to, changing
 * types, removing or reassigning enums, adding/removing/rearranging
 * fields to structures.
 */
#define VPX_EXT_RATECTRL_ABI_VERSION (7)

/*!\brief The control type of the inference API.
 * In VPX_RC_QP mode, the external rate control model determines the
 * quantization parameter (QP) for each frame.
 * In VPX_RC_GOP mode, the external rate control model determines the
 * group of picture (GOP) of the video sequence.
 * In VPX_RC_RDMULT mode, the external rate control model determines the
 * rate-distortion multiplier (rdmult) for the current frame.
 * In VPX_RC_GOP_QP mode, the external rate control model determines
 * both the QP and the GOP.
 * In VPX_RC_GOP_QP_RDMULT mode, the external rate control model determines
 * the QP, GOP and the rdmult.
 */
typedef enum vpx_rc_type {
  VPX_RC_QP = 1 << 0,
  VPX_RC_GOP = 1 << 1,
  VPX_RC_RDMULT = 1 << 2,
  VPX_RC_GOP_QP = VPX_RC_QP | VPX_RC_GOP,
  VPX_RC_GOP_QP_RDMULT = VPX_RC_QP | VPX_RC_GOP | VPX_RC_RDMULT
} vpx_rc_type_t;

/*!\brief The rate control mode for the external rate control model.
 */
typedef enum vpx_ext_rc_mode {
  VPX_RC_QMODE = 0,
  VPX_RC_VBR = 1,
  VPX_RC_CQ = 2,
} vpx_ext_rc_mode_t;

/*!\brief Abstract rate control model handler
 *
 * The encoder will receive the model handler from create_model() defined in
 * vpx_rc_funcs_t.
 */
typedef void *vpx_rc_model_t;

/*!\brief A reserved value for the q index.
 * If the external rate control model returns this value,
 * the encoder will use the default q selected by libvpx's rate control
 * system.
 */
#define VPX_DEFAULT_Q -1

/*!\brief A reserved value for the rdmult.
 * If the external rate control model returns this value,
 * the encoder will use the default rdmult selected by libvpx's rate control
 * system.
 */
#define VPX_DEFAULT_RDMULT -1

/*!\brief Encode frame decision made by the external rate control model
 *
 * The encoder will receive the decision from the external rate control model
 * through get_encodeframe_decision() defined in vpx_rc_funcs_t.
 *
 * If q_index = VPX_DEFAULT_Q, the encoder will use libvpx's default q.
 *
 * If max_frame_size = 0, the encoding ignores max frame size limit.
 * If max_frame_size = -1, the encoding uses VP9's max frame size as the limit.
 * If the encoded frame size is larger than max_frame_size, the frame is
 * recoded to meet the size limit, following VP9's recoding principles.
 */
typedef struct vpx_rc_encodeframe_decision {
  int q_index;        /**< Quantizer step index [0..255]*/
  int max_frame_size; /**< Maximal frame size allowed to encode a frame*/
} vpx_rc_encodeframe_decision_t;

/*!\brief Information for the frame to be encoded.
 *
 * The encoder will send the information to external rate control model through
 * get_encodeframe_decision() defined in vpx_rc_funcs_t.
 *
 */
typedef struct vpx_rc_encodeframe_info {
  /*!
   * 0: Key frame
   * 1: Inter frame
   * 2: Alternate reference frame
   * 3: Overlay frame
   * 4: Golden frame
   */
  int frame_type;
  int show_index;   /**< display index, starts from zero*/
  int coding_index; /**< coding index, starts from zero*/
  /*!
   * index of the current frame in this group of picture, starts from zero.
   */
  int gop_index;
  int ref_frame_coding_indexes[3]; /**< three reference frames' coding indices*/
  /*!
   * The validity of the three reference frames.
   * 0: Invalid
   * 1: Valid
   */
  int ref_frame_valid_list[3];
  /*!
   * The length of the current GOP.
   */
  int gop_size;
  /*!
   * Whether the current GOP uses an alt ref.
   */
  int use_alt_ref;
} vpx_rc_encodeframe_info_t;

/*!\brief Frame coding result
 *
 * The encoder will send the result to the external rate control model through
 * update_encodeframe_result() defined in vpx_rc_funcs_t.
 */
typedef struct vpx_rc_encodeframe_result {
  int64_t sse;         /**< sum of squared error of the reconstructed frame */
  int64_t bit_count;   /**< number of bits spent on coding the frame*/
  int64_t pixel_count; /**< number of pixels in YUV planes of the frame*/
  int actual_encoding_qindex; /**< the actual qindex used to encode the frame*/
} vpx_rc_encodeframe_result_t;

/*!\brief Status returned by rate control callback functions.
 */
typedef enum vpx_rc_status {
  VPX_RC_OK = 0,
  VPX_RC_ERROR = 1,
} vpx_rc_status_t;

/*!\brief First pass frame stats
 * This is a mirror of vp9's FIRSTPASS_STATS except that spatial_layer_id is
 * omitted
 */
typedef struct vpx_rc_frame_stats {
  /*!
   * Frame number in display order, if stats are for a single frame.
   * No real meaning for a collection of frames.
   */
  double frame;
  /*!
   * Weight assigned to this frame (or total weight for the collection of
   * frames) currently based on intra factor and brightness factor. This is used
   * to distribute bits between easier and harder frames.
   */
  double weight;
  /*!
   * Intra prediction error.
   */
  double intra_error;
  /*!
   * Best of intra pred error and inter pred error using last frame as ref.
   */
  double coded_error;
  /*!
   * Best of intra pred error and inter pred error using golden frame as ref.
   */
  double sr_coded_error;
  /*!
   * Estimate the noise energy of the current frame.
   */
  double frame_noise_energy;
  /*!
   * Percentage of blocks with inter pred error < intra pred error.
   */
  double pcnt_inter;
  /*!
   * Percentage of blocks using (inter prediction and) non-zero motion vectors.
   */
  double pcnt_motion;
  /*!
   * Percentage of blocks where golden frame was better than last or intra:
   * inter pred error using golden frame < inter pred error using last frame and
   * inter pred error using golden frame < intra pred error
   */
  double pcnt_second_ref;
  /*!
   * Percentage of blocks where intra and inter prediction errors were very
   * close.
   */
  double pcnt_neutral;
  /*!
   * Percentage of blocks that have intra error < inter error and inter error <
   * LOW_I_THRESH
   * - bit_depth 8: LOW_I_THRESH = 24000
   * - bit_depth 10: LOW_I_THRESH = 24000 << 4
   * - bit_depth 12: LOW_I_THRESH = 24000 << 8
   */
  double pcnt_intra_low;
  /*!
   * Percentage of blocks that have intra error < inter error and intra error <
   * LOW_I_THRESH but inter error >= LOW_I_THRESH LOW_I_THRESH
   * - bit_depth 8: LOW_I_THRESH = 24000
   * - bit_depth 10: LOW_I_THRESH = 24000 << 4
   * - bit_depth 12: LOW_I_THRESH = 24000 << 8
   */
  double pcnt_intra_high;
  /*!
   * Percentage of blocks that have almost no intra error residual
   * (i.e. are in effect completely flat and untextured in the intra
   * domain). In natural videos this is uncommon, but it is much more
   * common in animations, graphics and screen content, so may be used
   * as a signal to detect these types of content.
   */
  double intra_skip_pct;
  /*!
   * Percentage of blocks that have intra error < SMOOTH_INTRA_THRESH
   * - bit_depth 8:  SMOOTH_INTRA_THRESH = 4000
   * - bit_depth 10: SMOOTH_INTRA_THRESH = 4000 << 4
   * - bit_depth 12: SMOOTH_INTRA_THRESH = 4000 << 8
   */
  double intra_smooth_pct;
  /*!
   * Image mask rows top and bottom.
   */
  double inactive_zone_rows;
  /*!
   * Image mask columns at left and right edges.
   */
  double inactive_zone_cols;
  /*!
   * Mean of row motion vectors.
   */
  double MVr;
  /*!
   * Mean of absolute value of row motion vectors.
   */
  double mvr_abs;
  /*!
   * Mean of column motion vectors.
   */
  double MVc;
  /*!
   * Mean of absolute value of column motion vectors.
   */
  double mvc_abs;
  /*!
   * Variance of row motion vectors.
   */
  double MVrv;
  /*!
   * Variance of column motion vectors.
   */
  double MVcv;
  /*!
   * Value in range [-1,1] indicating fraction of row and column motion vectors
   * that point inwards (negative MV value) or outwards (positive MV value).
   * For example, value of 1 indicates, all row/column MVs are inwards.
   */
  double mv_in_out_count;
  /*!
   * Duration of the frame / collection of frames.
   */
  double duration;
  /*!
   * 1.0 if stats are for a single frame, or
   * number of frames whose stats are accumulated.
   */
  double count;
  /*!
   * Number of new mv in a frame.
   */
  double new_mv_count;
} vpx_rc_frame_stats_t;

/*!\brief Collection of first pass frame stats
 */
typedef struct vpx_rc_firstpass_stats {
  /*!
   * Pointer to first pass frame stats.
   * The pointed array of vpx_rc_frame_stats_t should have length equal to
   * number of show frames in the video.
   */
  vpx_rc_frame_stats_t *frame_stats;
  /*!
   * Number of show frames in the video.
   */
  int num_frames;
} vpx_rc_firstpass_stats_t;

/*!\brief Encode config sent to external rate control model
 */
typedef struct vpx_rc_config {
  int frame_width;      /**< frame width */
  int frame_height;     /**< frame height */
  int show_frame_count; /**< number of visible frames in the video */
  int max_gf_interval;  /**< max GOP size in number of show frames */
  int min_gf_interval;  /**< min GOP size in number of show frames */
  /*!
   * Target bitrate in kilobytes per second
   */
  int target_bitrate_kbps;
  int frame_rate_num; /**< numerator of frame rate */
  int frame_rate_den; /**< denominator of frame rate */
  /*!
   * The following fields are only for external rate control models that support
   * different rate control modes.
   */
  vpx_ext_rc_mode_t rc_mode; /**< Q mode or VBR mode */
  int overshoot_percent;     /**< for VBR mode only */
  int undershoot_percent;    /**< for VBR mode only */
} vpx_rc_config_t;

/*!\brief Information passed to the external rate control model to
 * help make GOP decisions.
 */
typedef struct vpx_rc_gop_info {
  /*!
   * Minimum allowed gf interval, fixed for the whole clip.
   * Note that it will be modified to match vp9's level constraints
   * in the encoder.
   * The level constraint is defined in vp9_encoder.c:
   * const Vp9LevelSpec vp9_level_defs[VP9_LEVELS].
   */
  int min_gf_interval;
  /*!
   * Maximum allowed gf interval, fixed for the whole clip.
   */
  int max_gf_interval;
  /*!
   * Minimum allowed gf interval for the current GOP, determined
   * by the encoder.
   */
  int active_min_gf_interval;
  /*!
   * Maximum allowed gf interval for the current GOP, determined
   * by the encoder.
   */
  int active_max_gf_interval;
  /*!
   * Whether to allow the use of alt ref, determined by the encoder.
   * It is fixed for the entire encode.
   * See function "is_altref_enabled" in vp9_encoder.h.
   */
  int allow_alt_ref;
  /*!
   * Is the current frame a key frame.
   */
  int is_key_frame;
  /*!
   * Does the previous gop use alt ref or not.
   */
  int last_gop_use_alt_ref;
  /*!
   * Current frame distance to the last keyframe, e.g., if Nth frame is a key,
   * then the value of the N+1 th frame is 1.
   */
  int frames_since_key;
  /*!
   * Current frame distance to the next keyframe, e.g. if Nth frame is a key,
   * then the value of frame N - 1 is 1.
   */
  int frames_to_key;
  /*!
   * Number of lookahead source frames.
   */
  int lag_in_frames;
  /*!
   * Display index (temporal stamp) of this frame in the whole clip,
   * starts from zero.
   */
  int show_index;
  /*!
   * Coding index of this frame in the whole clip, starts from zero.
   */
  int coding_index;
  /*!
   * The index of the current gop, starts from zero, resets to zero
   * when a keyframe is set.
   */
  int gop_global_index;
} vpx_rc_gop_info_t;

/*!\brief The decision made by the external rate control model to set the
 * group of picture.
 */
typedef struct vpx_rc_gop_decision {
  int gop_coding_frames; /**< The number of frames of this GOP */
  int use_alt_ref;       /**< Whether to use alt ref for this GOP */
} vpx_rc_gop_decision_t;

/*!\brief Create an external rate control model callback prototype
 *
 * This callback is invoked by the encoder to create an external rate control
 * model.
 *
 * \param[in]  priv                Callback's private data
 * \param[in]  ratectrl_config     Pointer to vpx_rc_config_t
 * \param[out] rate_ctrl_model_ptr Pointer to vpx_rc_model_t
 */
typedef vpx_rc_status_t (*vpx_rc_create_model_cb_fn_t)(
    void *priv, const vpx_rc_config_t *ratectrl_config,
    vpx_rc_model_t *rate_ctrl_model_ptr);

/*!\brief Send first pass stats to the external rate control model callback
 * prototype
 *
 * This callback is invoked by the encoder to send first pass stats to the
 * external rate control model.
 *
 * \param[in]  rate_ctrl_model    rate control model
 * \param[in]  first_pass_stats   first pass stats
 */
typedef vpx_rc_status_t (*vpx_rc_send_firstpass_stats_cb_fn_t)(
    vpx_rc_model_t rate_ctrl_model,
    const vpx_rc_firstpass_stats_t *first_pass_stats);

/*!\brief Send TPL stats for the current GOP to the external rate control model
 * callback prototype
 *
 * This callback is invoked by the encoder to send TPL stats for the GOP to the
 * external rate control model.
 *
 * \param[in]  rate_ctrl_model  rate control model
 * \param[in]  tpl_gop_stats    TPL stats for current GOP
 */
typedef vpx_rc_status_t (*vpx_rc_send_tpl_gop_stats_cb_fn_t)(
    vpx_rc_model_t rate_ctrl_model, const VpxTplGopStats *tpl_gop_stats);

/*!\brief Receive encode frame decision callback prototype
 *
 * This callback is invoked by the encoder to receive encode frame decision from
 * the external rate control model.
 *
 * \param[in]  rate_ctrl_model    rate control model
 * \param[in]  encode_frame_info  information of the coding frame
 * \param[out] frame_decision     encode decision of the coding frame
 */
typedef vpx_rc_status_t (*vpx_rc_get_encodeframe_decision_cb_fn_t)(
    vpx_rc_model_t rate_ctrl_model,
    const vpx_rc_encodeframe_info_t *encode_frame_info,
    vpx_rc_encodeframe_decision_t *frame_decision);

/*!\brief Update encode frame result callback prototype
 *
 * This callback is invoked by the encoder to update encode frame result to the
 * external rate control model.
 *
 * \param[in]  rate_ctrl_model     rate control model
 * \param[out] encode_frame_result encode result of the coding frame
 */
typedef vpx_rc_status_t (*vpx_rc_update_encodeframe_result_cb_fn_t)(
    vpx_rc_model_t rate_ctrl_model,
    const vpx_rc_encodeframe_result_t *encode_frame_result);

/*!\brief Get the GOP structure from the external rate control model.
 *
 * This callback is invoked by the encoder to get GOP decisions from
 * the external rate control model.
 *
 * \param[in]  rate_ctrl_model  rate control model
 * \param[in]  gop_info         information collected from the encoder
 * \param[out] gop_decision     GOP decision from the model
 */
typedef vpx_rc_status_t (*vpx_rc_get_gop_decision_cb_fn_t)(
    vpx_rc_model_t rate_ctrl_model, const vpx_rc_gop_info_t *gop_info,
    vpx_rc_gop_decision_t *gop_decision);

/*!\brief Get the frame rdmult from the external rate control model.
 *
 * This callback is invoked by the encoder to get rdmult from
 * the external rate control model.
 *
 * \param[in]  rate_ctrl_model  rate control model
 * \param[in]  frame_info       information collected from the encoder
 * \param[out] rdmult           frame rate-distortion multiplier from the model
 */
typedef vpx_rc_status_t (*vpx_rc_get_frame_rdmult_cb_fn_t)(
    vpx_rc_model_t rate_ctrl_model, const vpx_rc_encodeframe_info_t *frame_info,
    int *rdmult);

/*!\brief Delete the external rate control model callback prototype
 *
 * This callback is invoked by the encoder to delete the external rate control
 * model.
 *
 * \param[in]  rate_ctrl_model     rate control model
 */
typedef vpx_rc_status_t (*vpx_rc_delete_model_cb_fn_t)(
    vpx_rc_model_t rate_ctrl_model);

/*!\brief Callback function set for external rate control.
 *
 * The user can enable external rate control by registering
 * a set of callback functions with the codec control flag
 * VP9E_SET_EXTERNAL_RATE_CONTROL.
 */
typedef struct vpx_rc_funcs {
  /*!
   * The rate control type of this API.
   */
  vpx_rc_type_t rc_type;
  /*!
   * Create an external rate control model.
   */
  vpx_rc_create_model_cb_fn_t create_model;
  /*!
   * Send first pass stats to the external rate control model.
   */
  vpx_rc_send_firstpass_stats_cb_fn_t send_firstpass_stats;
  /*!
   * Send TPL stats for current GOP to the external rate control model.
   */
  vpx_rc_send_tpl_gop_stats_cb_fn_t send_tpl_gop_stats;
  /*!
   * Get encodeframe decision from the external rate control model.
   */
  vpx_rc_get_encodeframe_decision_cb_fn_t get_encodeframe_decision;
  /*!
   * Update encodeframe result to the external rate control model.
   */
  vpx_rc_update_encodeframe_result_cb_fn_t update_encodeframe_result;
  /*!
   * Get GOP decisions from the external rate control model.
   */
  vpx_rc_get_gop_decision_cb_fn_t get_gop_decision;
  /*!
   * Get rdmult for the frame from the external rate control model.
   */
  vpx_rc_get_frame_rdmult_cb_fn_t get_frame_rdmult;
  /*!
   * Delete the external rate control model.
   */
  vpx_rc_delete_model_cb_fn_t delete_model;
  /*!
   * Private data for the external rate control model.
   */
  void *priv;
} vpx_rc_funcs_t;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VPX_VPX_VPX_EXT_RATECTRL_H_
