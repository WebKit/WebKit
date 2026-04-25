/*
 * Copyright 2025 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_CORRUPTION_DETECTION_FRAME_INSTRUMENTATION_GENERATOR_IMPL_H_
#define VIDEO_CORRUPTION_DETECTION_FRAME_INSTRUMENTATION_GENERATOR_IMPL_H_

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <queue>

#include "api/environment/environment.h"
#include "api/video/corruption_detection/frame_instrumentation_data.h"
#include "api/video/corruption_detection/frame_instrumentation_generator.h"
#include "api/video/encoded_image.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/scalability_mode.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"
#include "video/corruption_detection/frame_selector.h"
#include "video/corruption_detection/halton_frame_sampler.h"

namespace webrtc {

class FrameInstrumentationGeneratorImpl : public FrameInstrumentationGenerator {
 public:
  FrameInstrumentationGeneratorImpl(
      const Environment* environment,
      VideoCodecType video_codec_type,
      std::optional<ScalabilityMode> scalability_mode);

  FrameInstrumentationGeneratorImpl(const FrameInstrumentationGeneratorImpl&) =
      delete;
  FrameInstrumentationGeneratorImpl& operator=(
      const FrameInstrumentationGeneratorImpl&) = delete;

  ~FrameInstrumentationGeneratorImpl() override = default;

  void OnCapturedFrame(VideoFrame frame) override RTC_LOCKS_EXCLUDED(mutex_);
  std::optional<FrameInstrumentationData> OnEncodedImage(
      const EncodedImage& encoded_image) override RTC_LOCKS_EXCLUDED(mutex_);

  std::optional<int> GetHaltonSequenceIndex(int layer_id) const override
      RTC_LOCKS_EXCLUDED(mutex_);
  void SetHaltonSequenceIndex(int index, int layer_id) override
      RTC_LOCKS_EXCLUDED(mutex_);

 private:
  struct Context {
    HaltonFrameSampler frame_sampler;
    uint32_t rtp_timestamp_of_last_key_frame = 0;
  };

  // Incoming video frames in capture order.
  std::queue<VideoFrame> captured_frames_ RTC_GUARDED_BY(mutex_);
  // Map from spatial or simulcast index to sampling context.
  std::map<int, Context> contexts_ RTC_GUARDED_BY(mutex_);
  const VideoCodecType video_codec_type_;
  const std::unique_ptr<FrameSelector> frame_selector_;
  mutable Mutex mutex_;
};

}  // namespace webrtc

#endif  // VIDEO_CORRUPTION_DETECTION_FRAME_INSTRUMENTATION_GENERATOR_IMPL_H_
