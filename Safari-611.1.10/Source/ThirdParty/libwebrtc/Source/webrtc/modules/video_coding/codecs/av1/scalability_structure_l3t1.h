/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_VIDEO_CODING_CODECS_AV1_SCALABILITY_STRUCTURE_L3T1_H_
#define MODULES_VIDEO_CODING_CODECS_AV1_SCALABILITY_STRUCTURE_L3T1_H_

#include <vector>

#include "absl/types/optional.h"
#include "api/transport/rtp/dependency_descriptor.h"
#include "common_video/generic_frame_descriptor/generic_frame_info.h"
#include "modules/video_coding/codecs/av1/scalable_video_controller.h"

namespace webrtc {

// S2     0-0-0-
//        | | |
// S1     0-0-0-...
//        | | |
// S0     0-0-0-
// Time-> 0 1 2
class ScalabilityStructureL3T1 : public ScalableVideoController {
 public:
  ~ScalabilityStructureL3T1() override;

  StreamLayersConfig StreamConfig() const override;
  FrameDependencyStructure DependencyStructure() const override;

  std::vector<LayerFrameConfig> NextFrameConfig(bool restart) override;
  absl::optional<GenericFrameInfo> OnEncodeDone(
      LayerFrameConfig config) override;
  void OnRatesUpdated(const VideoBitrateAllocation& bitrates) override;

 private:
  enum FramePattern {
    kKeyFrame,
    kDeltaFrame,
  };
  static constexpr int kNumSpatialLayers = 3;

  FramePattern next_pattern_ = kKeyFrame;
  bool use_temporal_dependency_[kNumSpatialLayers] = {false, false, false};
  std::bitset<32> active_decode_targets_ = 0b111;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_AV1_SCALABILITY_STRUCTURE_L3T1_H_
