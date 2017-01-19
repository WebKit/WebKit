/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_TEST_FAKE_ENCODER_H_
#define WEBRTC_TEST_FAKE_ENCODER_H_

#include <vector>

#include "webrtc/base/criticalsection.h"
#include "webrtc/common_types.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/video_encoder.h"

namespace webrtc {
namespace test {

class FakeEncoder : public VideoEncoder {
 public:
  explicit FakeEncoder(Clock* clock);
  virtual ~FakeEncoder();

  // Sets max bitrate. Not thread-safe, call before registering the encoder.
  void SetMaxBitrate(int max_kbps);

  int32_t InitEncode(const VideoCodec* config,
                     int32_t number_of_cores,
                     size_t max_payload_size) override;
  int32_t Encode(const VideoFrame& input_image,
                 const CodecSpecificInfo* codec_specific_info,
                 const std::vector<FrameType>* frame_types) override;
  int32_t RegisterEncodeCompleteCallback(
      EncodedImageCallback* callback) override;
  int32_t Release() override;
  int32_t SetChannelParameters(uint32_t packet_loss, int64_t rtt) override;
  int32_t SetRates(uint32_t new_target_bitrate, uint32_t framerate) override;
  const char* ImplementationName() const override;

  static const char* kImplementationName;

 protected:
  Clock* const clock_;
  VideoCodec config_;
  EncodedImageCallback* callback_;
  int target_bitrate_kbps_;
  int max_target_bitrate_kbps_;
  int64_t last_encode_time_ms_;
  uint8_t encoded_buffer_[100000];
};

class FakeH264Encoder : public FakeEncoder, public EncodedImageCallback {
 public:
  explicit FakeH264Encoder(Clock* clock);
  virtual ~FakeH264Encoder() {}

  int32_t RegisterEncodeCompleteCallback(
      EncodedImageCallback* callback) override;

  Result OnEncodedImage(const EncodedImage& encodedImage,
                        const CodecSpecificInfo* codecSpecificInfo,
                        const RTPFragmentationHeader* fragments) override;

 private:
  EncodedImageCallback* callback_;
  int idr_counter_;
};

class DelayedEncoder : public test::FakeEncoder {
 public:
  DelayedEncoder(Clock* clock, int delay_ms);
  virtual ~DelayedEncoder() {}

  void SetDelay(int delay_ms);
  int32_t Encode(const VideoFrame& input_image,
                 const CodecSpecificInfo* codec_specific_info,
                 const std::vector<FrameType>* frame_types) override;

 private:
  rtc::CriticalSection lock_;
  int delay_ms_ GUARDED_BY(&lock_);
};
}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TEST_FAKE_ENCODER_H_
