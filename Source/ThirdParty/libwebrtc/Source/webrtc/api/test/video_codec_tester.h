/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_VIDEO_CODEC_TESTER_H_
#define API_TEST_VIDEO_CODEC_TESTER_H_

#include <memory>
#include <string>

#include "absl/functional/any_invocable.h"
#include "absl/types/optional.h"
#include "api/test/video_codec_stats.h"
#include "api/video/encoded_image.h"
#include "api/video/resolution.h"
#include "api/video/video_frame.h"

namespace webrtc {
namespace test {

// Interface for a video codec tester. The interface provides minimalistic set
// of data structures that enables implementation of decode-only, encode-only
// and encode-decode tests.
class VideoCodecTester {
 public:
  // Pacing settings for codec input.
  struct PacingSettings {
    enum PacingMode {
      // Pacing is not used. Frames are sent to codec back-to-back.
      kNoPacing,
      // Pace with the rate equal to the target video frame rate. Pacing time is
      // derived from RTP timestamp.
      kRealTime,
      // Pace with the explicitly provided rate.
      kConstantRate,
    };
    PacingMode mode = PacingMode::kNoPacing;
    // Pacing rate for `kConstantRate` mode.
    Frequency constant_rate = Frequency::Zero();
  };

  struct DecoderSettings {
    PacingSettings pacing;
    absl::optional<std::string> decoder_input_base_path;
    absl::optional<std::string> decoder_output_base_path;
  };

  struct EncoderSettings {
    PacingSettings pacing;
    absl::optional<std::string> encoder_input_base_path;
    absl::optional<std::string> encoder_output_base_path;
  };

  virtual ~VideoCodecTester() = default;

  // Interface for a raw video frames source.
  class RawVideoSource {
   public:
    virtual ~RawVideoSource() = default;

    // Returns next frame. If no more frames to pull, returns `absl::nullopt`.
    // For analysis and pacing purposes, frame must have RTP timestamp set. The
    // timestamp must represent the target video frame rate and be unique.
    virtual absl::optional<VideoFrame> PullFrame() = 0;

    // Returns early pulled frame with RTP timestamp equal to `timestamp_rtp`.
    virtual VideoFrame GetFrame(uint32_t timestamp_rtp,
                                Resolution resolution) = 0;
  };

  // Interface for a coded video frames source.
  class CodedVideoSource {
   public:
    virtual ~CodedVideoSource() = default;

    // Returns next frame. If no more frames to pull, returns `absl::nullopt`.
    // For analysis and pacing purposes, frame must have RTP timestamp set. The
    // timestamp must represent the target video frame rate and be unique.
    virtual absl::optional<EncodedImage> PullFrame() = 0;
  };

  // Interface for a video encoder.
  class Encoder {
   public:
    using EncodeCallback =
        absl::AnyInvocable<void(const EncodedImage& encoded_frame)>;

    virtual ~Encoder() = default;

    virtual void Initialize() = 0;

    virtual void Encode(const VideoFrame& frame, EncodeCallback callback) = 0;

    virtual void Flush() = 0;
  };

  // Interface for a video decoder.
  class Decoder {
   public:
    using DecodeCallback =
        absl::AnyInvocable<void(const VideoFrame& decoded_frame)>;

    virtual ~Decoder() = default;

    virtual void Initialize() = 0;

    virtual void Decode(const EncodedImage& frame, DecodeCallback callback) = 0;

    virtual void Flush() = 0;
  };

  // Pulls coded video frames from `video_source` and passes them to `decoder`.
  // Returns `VideoCodecTestStats` object that contains collected per-frame
  // metrics.
  virtual std::unique_ptr<VideoCodecStats> RunDecodeTest(
      CodedVideoSource* video_source,
      Decoder* decoder,
      const DecoderSettings& decoder_settings) = 0;

  // Pulls raw video frames from `video_source` and passes them to `encoder`.
  // Returns `VideoCodecTestStats` object that contains collected per-frame
  // metrics.
  virtual std::unique_ptr<VideoCodecStats> RunEncodeTest(
      RawVideoSource* video_source,
      Encoder* encoder,
      const EncoderSettings& encoder_settings) = 0;

  // Pulls raw video frames from `video_source`, passes them to `encoder` and
  // then passes encoded frames to `decoder`. Returns `VideoCodecTestStats`
  // object that contains collected per-frame metrics.
  virtual std::unique_ptr<VideoCodecStats> RunEncodeDecodeTest(
      RawVideoSource* video_source,
      Encoder* encoder,
      Decoder* decoder,
      const EncoderSettings& encoder_settings,
      const DecoderSettings& decoder_settings) = 0;
};

}  // namespace test
}  // namespace webrtc

#endif  // API_TEST_VIDEO_CODEC_TESTER_H_
