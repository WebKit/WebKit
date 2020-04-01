/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_ADAPTATION_ENCODE_USAGE_RESOURCE_H_
#define VIDEO_ADAPTATION_ENCODE_USAGE_RESOURCE_H_

#include <memory>
#include <string>

#include "absl/types/optional.h"
#include "call/adaptation/resource.h"
#include "modules/video_coding/utility/quality_scaler.h"
#include "video/adaptation/overuse_frame_detector.h"

namespace webrtc {

// Handles interaction with the OveruseDetector.
// TODO(hbos): Add unittests specific to this class, it is currently only tested
// indirectly by usage in the ResourceAdaptationProcessor (which is only tested
// because of its usage in VideoStreamEncoder); all tests are currently in
// video_stream_encoder_unittest.cc.
// TODO(https://crbug.com/webrtc/11222): Move this class to the
// video/adaptation/ subdirectory.
class EncodeUsageResource : public Resource,
                            public AdaptationObserverInterface {
 public:
  explicit EncodeUsageResource(
      std::unique_ptr<OveruseFrameDetector> overuse_detector);

  void StartCheckForOveruse(CpuOveruseOptions options);
  void StopCheckForOveruse();

  void SetTargetFrameRate(absl::optional<double> target_frame_rate);
  void OnEncodeStarted(const VideoFrame& cropped_frame,
                       int64_t time_when_first_seen_us);
  void OnEncodeCompleted(uint32_t timestamp,
                         int64_t time_sent_in_us,
                         int64_t capture_time_us,
                         absl::optional<int> encode_duration_us);

  // AdaptationObserverInterface implementation.
  // TODO(https://crbug.com/webrtc/11222, 11172): This resource also needs to
  // signal when its stable to support multi-stream aware modules.
  void AdaptUp(AdaptReason reason) override;
  bool AdaptDown(AdaptReason reason) override;

  std::string name() const override { return "EncoderUsageResource"; }

 private:
  int TargetFrameRateAsInt();

  const std::unique_ptr<OveruseFrameDetector> overuse_detector_;
  bool is_started_;
  absl::optional<double> target_frame_rate_;
};

}  // namespace webrtc

#endif  // VIDEO_ADAPTATION_ENCODE_USAGE_RESOURCE_H_
