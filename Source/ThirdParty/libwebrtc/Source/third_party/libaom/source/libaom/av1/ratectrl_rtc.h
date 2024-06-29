/*
 * Copyright (c) 2021, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AV1_RATECTRL_RTC_H_
#define AOM_AV1_RATECTRL_RTC_H_

#include <cstdint>
#include <memory>

struct AV1_COMP;

namespace aom {

// These constants come from AV1 spec.
static constexpr size_t kAV1MaxLayers = 32;
static constexpr size_t kAV1MaxTemporalLayers = 8;
static constexpr size_t kAV1MaxSpatialLayers = 4;

enum FrameType { kKeyFrame, kInterFrame };

struct AV1RateControlRtcConfig {
 public:
  AV1RateControlRtcConfig();

  int width;
  int height;
  // Flag indicating if the content is screen or not.
  bool is_screen = false;
  // 0-63
  int max_quantizer;
  int min_quantizer;
  int64_t target_bandwidth;
  int64_t buf_initial_sz;
  int64_t buf_optimal_sz;
  int64_t buf_sz;
  int undershoot_pct;
  int overshoot_pct;
  int max_intra_bitrate_pct;
  int max_inter_bitrate_pct;
  int frame_drop_thresh;
  int max_consec_drop;
  double framerate;
  int layer_target_bitrate[kAV1MaxLayers];
  int ts_rate_decimator[kAV1MaxTemporalLayers];
  int aq_mode;
  // Number of spatial layers
  int ss_number_layers;
  // Number of temporal layers
  int ts_number_layers;
  int max_quantizers[kAV1MaxLayers];
  int min_quantizers[kAV1MaxLayers];
  int scaling_factor_num[kAV1MaxSpatialLayers];
  int scaling_factor_den[kAV1MaxSpatialLayers];
};

struct AV1FrameParamsRTC {
  FrameType frame_type;
  int spatial_layer_id;
  int temporal_layer_id;
};

struct AV1LoopfilterLevel {
  int filter_level[2];
  int filter_level_u;
  int filter_level_v;
};

struct AV1CdefInfo {
  int cdef_strength_y;
  int cdef_strength_uv;
  int damping;
};

struct AV1SegmentationData {
  const uint8_t *segmentation_map;
  size_t segmentation_map_size;
  const int *delta_q;
  size_t delta_q_size;
};

enum class FrameDropDecision {
  kOk,    // Frame is encoded.
  kDrop,  // Frame is dropped.
};

class AV1RateControlRTC {
 public:
  static std::unique_ptr<AV1RateControlRTC> Create(
      const AV1RateControlRtcConfig &cfg);
  ~AV1RateControlRTC();

  bool UpdateRateControl(const AV1RateControlRtcConfig &rc_cfg);
  // GetQP() needs to be called after ComputeQP() to get the latest QP
  int GetQP() const;
  // GetLoopfilterLevel() needs to be called after ComputeQP()
  AV1LoopfilterLevel GetLoopfilterLevel() const;
  // GetCdefInfo() needs to be called after ComputeQP()
  AV1CdefInfo GetCdefInfo() const;
  // Returns the segmentation map used for cyclic refresh, based on 4x4 blocks.
  bool GetSegmentationData(AV1SegmentationData *segmentation_data) const;
  // ComputeQP returns the QP if the frame is not dropped (kOk return),
  // otherwise it returns kDrop and subsequent GetQP and PostEncodeUpdate
  // are not to be called (av1_rc_postencode_update_drop_frame is already
  // called via ComputeQP if drop is decided).
  FrameDropDecision ComputeQP(const AV1FrameParamsRTC &frame_params);
  // Feedback to rate control with the size of current encoded frame
  void PostEncodeUpdate(uint64_t encoded_frame_size);

 private:
  AV1RateControlRTC() = default;
  bool InitRateControl(const AV1RateControlRtcConfig &cfg);
  AV1_COMP *cpi_;
  int initial_width_;
  int initial_height_;
};

}  // namespace aom

#endif  // AOM_AV1_RATECTRL_RTC_H_
