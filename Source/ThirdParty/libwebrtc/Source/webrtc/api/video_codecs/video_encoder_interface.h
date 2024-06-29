/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_VIDEO_ENCODER_INTERFACE_H_
#define API_VIDEO_CODECS_VIDEO_ENCODER_INTERFACE_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/encoded_image.h"
#include "api/video/resolution.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoding_general.h"
#include "rtc_base/numerics/rational.h"

namespace webrtc {
// NOTE: This class is still under development and may change without notice.
class VideoEncoderInterface {
 public:
  virtual ~VideoEncoderInterface() = default;
  enum class FrameType { kKeyframe, kStartFrame, kDeltaFrame };

  struct TemporalUnitSettings {
    VideoCodecMode content_hint = VideoCodecMode::kRealtimeVideo;
    Timestamp presentation_timestamp;
    int effort_level = 0;
  };

  // Results from calling Encode. Called once for each configured frame.
  struct EncodingError {};
  struct EncodedData {
    rtc::scoped_refptr<EncodedImageBufferInterface> bitstream_data;
    FrameType frame_type;
    int encoded_qp;
  };
  using EncodeResult = absl::variant<EncodingError, EncodedData>;
  using EncodeResultCallback =
      absl::AnyInvocable<void(const EncodeResult& result) &&>;

  struct FrameEncodeSettings {
    struct Cbr {
      TimeDelta duration;
      DataRate target_bitrate;
    };

    struct Cqp {
      int target_qp;
    };

    absl::variant<Cqp, Cbr> rate_options;

    FrameType frame_type = FrameType::kDeltaFrame;
    int temporal_id = 0;
    int spatial_id = 0;
    Resolution resolution;
    std::vector<int> reference_buffers;
    absl::optional<int> update_buffer;

    EncodeResultCallback result_callback;
  };

  virtual void Encode(rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_buffer,
                      const TemporalUnitSettings& settings,
                      std::vector<FrameEncodeSettings> frame_settings) = 0;
};

}  // namespace webrtc
#endif  // API_VIDEO_CODECS_VIDEO_ENCODER_INTERFACE_H_
