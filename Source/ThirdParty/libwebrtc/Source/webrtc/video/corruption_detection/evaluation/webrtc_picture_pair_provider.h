/*
 * Copyright 2025 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_CORRUPTION_DETECTION_EVALUATION_WEBRTC_PICTURE_PAIR_PROVIDER_H_
#define VIDEO_CORRUPTION_DETECTION_EVALUATION_WEBRTC_PICTURE_PAIR_PROVIDER_H_

#include <cstdint>
#include <memory>
#include <optional>

#include "api/environment/environment.h"
#include "api/sequence_checker.h"
#include "api/units/data_rate.h"
#include "api/video/encoded_image.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/thread_annotations.h"
#include "test/testsupport/frame_reader.h"
#include "video/corruption_detection/evaluation/picture_pair_provider.h"
#include "video/corruption_detection/evaluation/test_clip.h"

namespace webrtc {

// Provides a picture pair where one of the pairs is the original raw frame and
// the other pair is the corresponding compressed frame that has gone through an
// encoding/decoding pipeline as implemented in this class. The compressed frame
// is obtained through the standard VideoEncoder and VideoDecoder instances.
//
// This particular class is meant to only be used with the built-in software
// encoders/decoders which are all synchronous implementations. Hence, only
// works for synchronous encoders/decoders. If the class is to be expanded to
// include asynchronous encoders/decoders, see the guidance in the .cc file.
class WebRtcEncoderDecoderPicturePairProvider : public PicturePairProvider,
                                                public EncodedImageCallback,
                                                public DecodedImageCallback {
 public:
  WebRtcEncoderDecoderPicturePairProvider(
      VideoCodecType type,
      std::unique_ptr<VideoEncoderFactory> encoder_factory,
      std::unique_ptr<VideoDecoderFactory> decoder_factory);

  ~WebRtcEncoderDecoderPicturePairProvider() override;

  bool Configure(const TestClip& clip, DataRate bitrate) override;

  std::optional<OriginalCompressedPicturePair> GetNextPicturePair() override;

 private:
  bool ConfigureEncoderSettings(const TestClip& clip, DataRate bitrate);
  bool InitializeEncoder();
  void SetEncoderRate(const TestClip& clip, DataRate bitrate);
  bool InitializeDecoder();
  bool InitializeFrameGenerator(const TestClip& clip);

  // Callback methods
  // Overridden from EncodedImageCallback.
  Result OnEncodedImage(const EncodedImage& encoded_image,
                        const CodecSpecificInfo*) override;
  void OnDroppedFrame(DropReason reason) override;

  // Overridden from DecodedImageCallback.
  int32_t Decoded(VideoFrame& decoded_image) override;

  // Variables
  const Environment env_;
  const VideoCodecType type_;

  // Because of how `BuiltinVideoEncoderFactory` is constructed, this class
  // needs to take ownership of the encoder factory.
  const std::unique_ptr<VideoEncoderFactory> encoder_factory_;
  const std::unique_ptr<VideoEncoder> encoder_;
  const std::unique_ptr<VideoDecoder> decoder_;

  RTC_NO_UNIQUE_ADDRESS SequenceChecker sequence_checker_;
  std::optional<EncodedImage> encoded_image_ RTC_GUARDED_BY(sequence_checker_);
  std::optional<VideoFrame> decoded_image_ RTC_GUARDED_BY(sequence_checker_);
  std::optional<uint8_t> qp_ RTC_GUARDED_BY(sequence_checker_);

  bool is_initialized_ = false;
  VideoCodec codec_config_;
  bool is_keyframe_ RTC_GUARDED_BY(sequence_checker_) = true;
  uint32_t rtp_timestamp_ RTC_GUARDED_BY(sequence_checker_) = 0;
  uint32_t rtp_timestamp_interval_ RTC_GUARDED_BY(sequence_checker_) = 0;

  std::unique_ptr<test::FrameReader> frame_generator_;
};

}  // namespace webrtc

#endif  // VIDEO_CORRUPTION_DETECTION_EVALUATION_WEBRTC_PICTURE_PAIR_PROVIDER_H_
