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

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>

#include "api/scoped_refptr.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/resolution.h"
#include "api/video/video_frame_buffer.h"
#include "api/video_codecs/video_codec.h"

namespace webrtc {
// NOTE: This class is still under development and may change without notice.
class VideoEncoderInterface {
 public:
  virtual ~VideoEncoderInterface() = default;
  enum class FrameType { kKeyframe, kStartFrame, kDeltaFrame };

  struct EncodingError {};
  struct EncodedData {
    FrameType frame_type;
    int encoded_qp;
  };
  using EncodeResult = std::variant<EncodingError, EncodedData>;

  struct FrameOutput {
    virtual ~FrameOutput() = default;
    virtual std::span<uint8_t> GetBitstreamOutputBuffer(DataSize size) = 0;
    virtual void EncodeComplete(const EncodeResult& encode_result) = 0;
  };

  class TemporalUnitSettings {
   public:
    constexpr TemporalUnitSettings()
        : presentation_timestamp_(Timestamp::Zero()) {}
    constexpr explicit TemporalUnitSettings(Timestamp timestamp)
        : presentation_timestamp_(timestamp) {}
    constexpr TemporalUnitSettings(VideoCodecMode content_hint,
                                   Timestamp timestamp)
        : content_hint_(content_hint), presentation_timestamp_(timestamp) {}

    VideoCodecMode content_hint() const { return content_hint_; }
    void set_content_hint(VideoCodecMode content_hint) {
      content_hint_ = content_hint;
    }

    Timestamp presentation_timestamp() const { return presentation_timestamp_; }
    void set_presentation_timestamp(Timestamp timestamp) {
      presentation_timestamp_ = timestamp;
    }

   private:
    VideoCodecMode content_hint_ = VideoCodecMode::kRealtimeVideo;
    Timestamp presentation_timestamp_;
  };

  class FrameEncodeSettings {
   public:
    struct Cbr {
      TimeDelta duration;
      DataRate target_bitrate;
    };

    struct Cqp {
      int target_qp;
    };

    FrameEncodeSettings() = default;

    FrameEncodeSettings(const FrameEncodeSettings&) = delete;
    FrameEncodeSettings& operator=(const FrameEncodeSettings&) = delete;

    FrameEncodeSettings(FrameEncodeSettings&&) = default;
    FrameEncodeSettings& operator=(FrameEncodeSettings&&) = default;

    const std::variant<Cqp, Cbr>& rate_options() const { return rate_options_; }
    void set_cqp_options(int qp) { rate_options_ = Cqp{.target_qp = qp}; }
    void set_cbr_options(TimeDelta duration, DataRate target_bitrate) {
      rate_options_ =
          Cbr{.duration = duration, .target_bitrate = target_bitrate};
    }

    FrameType frame_type() const { return frame_type_; }
    void set_frame_type(FrameType type) { frame_type_ = type; }

    int temporal_id() const { return temporal_id_; }
    void set_temporal_id(int id) { temporal_id_ = id; }

    int spatial_id() const { return spatial_id_; }
    void set_spatial_id(int id) { spatial_id_ = id; }

    Resolution resolution() const { return resolution_; }
    void set_resolution(Resolution res) { resolution_ = res; }

    std::span<const int> reference_buffers() const {
      return std::span<const int>(reference_buffers_);
    }
    void set_reference_buffers(std::span<const int> refs) {
      reference_buffers_.assign(refs.begin(), refs.end());
    }

    std::optional<int> update_buffer() const { return update_buffer_; }
    void set_update_buffer(std::optional<int> buf) { update_buffer_ = buf; }

    int effort_level() const { return effort_level_; }
    void set_effort_level(int level) { effort_level_ = level; }

    FrameOutput* frame_output() const { return frame_output_.get(); }
    std::unique_ptr<FrameOutput> release_frame_output() {
      return std::move(frame_output_);
    }
    void set_frame_output(std::unique_ptr<FrameOutput> output) {
      frame_output_ = std::move(output);
    }

   private:
    std::variant<Cqp, Cbr> rate_options_;
    FrameType frame_type_ = FrameType::kDeltaFrame;
    int temporal_id_ = 0;
    int spatial_id_ = 0;
    Resolution resolution_;
    std::vector<int> reference_buffers_;
    std::optional<int> update_buffer_;
    int effort_level_ = 0;
    std::unique_ptr<FrameOutput> frame_output_;
  };

  virtual void Encode(scoped_refptr<VideoFrameBuffer> frame_buffer,
                      const TemporalUnitSettings& settings,
                      std::vector<FrameEncodeSettings> frame_settings) = 0;
};

}  // namespace webrtc
#endif  // API_VIDEO_CODECS_VIDEO_ENCODER_INTERFACE_H_
