/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_VIDEO_STREAM_ENCODER_OBSERVER_H_
#define API_VIDEO_VIDEO_STREAM_ENCODER_OBSERVER_H_

#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_config.h"

namespace webrtc {

// TODO(nisse): Used for the OnSendEncodedImage callback below. The callback
// wants metadata such as size, encode timing, qp, but doesn't need actual
// encoded data. So use some other type to represent that.
class EncodedImage;

// Broken out into a base class, with public inheritance below, only to ease
// unit testing of the internal class OveruseFrameDetector.
class CpuOveruseMetricsObserver {
 public:
  virtual ~CpuOveruseMetricsObserver() = default;
  virtual void OnEncodedFrameTimeMeasured(int encode_duration_ms,
                                          int encode_usage_percent) = 0;
};

class VideoStreamEncoderObserver : public CpuOveruseMetricsObserver {
 public:
  // Number of resolution and framerate reductions (unset if disabled).
  struct AdaptationSteps {
    AdaptationSteps();
    absl::optional<int> num_resolution_reductions = 0;
    absl::optional<int> num_framerate_reductions = 0;
  };

  // TODO(nisse): There are too many enums to represent this. Besides
  // this one, see AdaptationObserverInterface::AdaptReason and
  // WebRtcVideoChannel::AdaptReason.
  enum class AdaptationReason {
    kNone,  // Used for reset of counters.
    kCpu,
    kQuality,
  };

  // TODO(nisse): Duplicates enum EncodedImageCallback::DropReason.
  enum class DropReason {
    kSource,
    kEncoderQueue,
    kEncoder,
    kMediaOptimization
  };

  ~VideoStreamEncoderObserver() override = default;

  virtual void OnIncomingFrame(int width, int height) = 0;

  // TODO(nisse): Merge into one callback per encoded frame.
  using CpuOveruseMetricsObserver::OnEncodedFrameTimeMeasured;
  virtual void OnSendEncodedImage(const EncodedImage& encoded_image,
                                  const CodecSpecificInfo* codec_info) = 0;

  virtual void OnEncoderImplementationChanged(
      const std::string& implementation_name) = 0;

  virtual void OnFrameDropped(DropReason reason) = 0;

  // Used to indicate change in content type, which may require a change in
  // how stats are collected and set the configured preferred media bitrate.
  virtual void OnEncoderReconfigured(
      const VideoEncoderConfig& encoder_config,
      const std::vector<VideoStream>& streams) = 0;

  virtual void OnAdaptationChanged(AdaptationReason reason,
                                   const AdaptationSteps& cpu_steps,
                                   const AdaptationSteps& quality_steps) = 0;
  virtual void OnMinPixelLimitReached() = 0;
  virtual void OnInitialQualityResolutionAdaptDown() = 0;

  virtual void OnSuspendChange(bool is_suspended) = 0;

  // TODO(nisse): VideoStreamEncoder wants to query the stats, which makes this
  // not a pure observer. GetInputFrameRate is needed for the cpu adaptation, so
  // can be deleted if that responsibility is moved out to a VideoStreamAdaptor
  // class.
  virtual int GetInputFrameRate() const = 0;
};

}  // namespace webrtc
#endif  // API_VIDEO_VIDEO_STREAM_ENCODER_OBSERVER_H_
