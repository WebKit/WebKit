/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/fake_encoder.h"

#include <algorithm>

#include "webrtc/base/checks.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/system_wrappers/include/sleep.h"
#include "webrtc/test/gtest.h"

namespace webrtc {
namespace test {

FakeEncoder::FakeEncoder(Clock* clock)
    : clock_(clock),
      callback_(NULL),
      target_bitrate_kbps_(0),
      max_target_bitrate_kbps_(-1),
      last_encode_time_ms_(0) {
  // Generate some arbitrary not-all-zero data
  for (size_t i = 0; i < sizeof(encoded_buffer_); ++i) {
    encoded_buffer_[i] = static_cast<uint8_t>(i);
  }
}

FakeEncoder::~FakeEncoder() {}

void FakeEncoder::SetMaxBitrate(int max_kbps) {
  RTC_DCHECK_GE(max_kbps, -1);  // max_kbps == -1 disables it.
  max_target_bitrate_kbps_ = max_kbps;
}

int32_t FakeEncoder::InitEncode(const VideoCodec* config,
                                int32_t number_of_cores,
                                size_t max_payload_size) {
  config_ = *config;
  target_bitrate_kbps_ = config_.startBitrate;
  return 0;
}

int32_t FakeEncoder::Encode(const VideoFrame& input_image,
                            const CodecSpecificInfo* codec_specific_info,
                            const std::vector<FrameType>* frame_types) {
  RTC_DCHECK_GT(config_.maxFramerate, 0);
  int64_t time_since_last_encode_ms = 1000 / config_.maxFramerate;
  int64_t time_now_ms = clock_->TimeInMilliseconds();
  const bool first_encode = last_encode_time_ms_ == 0;
  if (!first_encode) {
    // For all frames but the first we can estimate the display time by looking
    // at the display time of the previous frame.
    time_since_last_encode_ms = time_now_ms - last_encode_time_ms_;
  }
  if (time_since_last_encode_ms > 3 * 1000 / config_.maxFramerate) {
    // Rudimentary check to make sure we don't widely overshoot bitrate target
    // when resuming encoding after a suspension.
    time_since_last_encode_ms = 3 * 1000 / config_.maxFramerate;
  }

  size_t bits_available =
      static_cast<size_t>(target_bitrate_kbps_ * time_since_last_encode_ms);
  size_t min_bits = static_cast<size_t>(
      config_.simulcastStream[0].minBitrate * time_since_last_encode_ms);
  if (bits_available < min_bits)
    bits_available = min_bits;
  size_t max_bits =
      static_cast<size_t>(max_target_bitrate_kbps_ * time_since_last_encode_ms);
  if (max_bits > 0 && max_bits < bits_available)
    bits_available = max_bits;
  last_encode_time_ms_ = time_now_ms;

  RTC_DCHECK_GT(config_.numberOfSimulcastStreams, 0);
  for (unsigned char i = 0; i < config_.numberOfSimulcastStreams; ++i) {
    CodecSpecificInfo specifics;
    memset(&specifics, 0, sizeof(specifics));
    specifics.codecType = kVideoCodecGeneric;
    specifics.codecSpecific.generic.simulcast_idx = i;
    size_t min_stream_bits = static_cast<size_t>(
        config_.simulcastStream[i].minBitrate * time_since_last_encode_ms);
    size_t max_stream_bits = static_cast<size_t>(
        config_.simulcastStream[i].maxBitrate * time_since_last_encode_ms);
    size_t stream_bits = (bits_available > max_stream_bits) ? max_stream_bits :
        bits_available;
    size_t stream_bytes = (stream_bits + 7) / 8;
    if (first_encode) {
      // The first frame is a key frame and should be larger.
      // TODO(holmer): The FakeEncoder should store the bits_available between
      // encodes so that it can compensate for oversized frames.
      stream_bytes *= 10;
    }
    if (stream_bytes > sizeof(encoded_buffer_))
      stream_bytes = sizeof(encoded_buffer_);

    // Always encode something on the first frame.
    if (min_stream_bits > bits_available && i > 0)
      continue;
    EncodedImage encoded(
        encoded_buffer_, stream_bytes, sizeof(encoded_buffer_));
    encoded._timeStamp = input_image.timestamp();
    encoded.capture_time_ms_ = input_image.render_time_ms();
    encoded._frameType = (*frame_types)[i];
    encoded._encodedWidth = config_.simulcastStream[i].width;
    encoded._encodedHeight = config_.simulcastStream[i].height;
    encoded.rotation_ = input_image.rotation();
    RTC_DCHECK(callback_ != NULL);
    specifics.codec_name = ImplementationName();
    if (callback_->OnEncodedImage(encoded, &specifics, NULL).error !=
        EncodedImageCallback::Result::OK) {
      return -1;
    }
    bits_available -= std::min(encoded._length * 8, bits_available);
  }
  return 0;
}

int32_t FakeEncoder::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  callback_ = callback;
  return 0;
}

int32_t FakeEncoder::Release() { return 0; }

int32_t FakeEncoder::SetChannelParameters(uint32_t packet_loss, int64_t rtt) {
  return 0;
}

int32_t FakeEncoder::SetRates(uint32_t new_target_bitrate, uint32_t framerate) {
  target_bitrate_kbps_ = new_target_bitrate;
  return 0;
}

const char* FakeEncoder::kImplementationName = "fake_encoder";
const char* FakeEncoder::ImplementationName() const {
  return kImplementationName;
}

FakeH264Encoder::FakeH264Encoder(Clock* clock)
    : FakeEncoder(clock), callback_(NULL), idr_counter_(0) {
  FakeEncoder::RegisterEncodeCompleteCallback(this);
}

int32_t FakeH264Encoder::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  callback_ = callback;
  return 0;
}

EncodedImageCallback::Result FakeH264Encoder::OnEncodedImage(
    const EncodedImage& encoded_image,
    const CodecSpecificInfo* codec_specific_info,
    const RTPFragmentationHeader* fragments) {
  const size_t kSpsSize = 8;
  const size_t kPpsSize = 11;
  const int kIdrFrequency = 10;
  RTPFragmentationHeader fragmentation;
  if (idr_counter_++ % kIdrFrequency == 0 &&
      encoded_image._length > kSpsSize + kPpsSize + 1) {
    const size_t kNumSlices = 3;
    fragmentation.VerifyAndAllocateFragmentationHeader(kNumSlices);
    fragmentation.fragmentationOffset[0] = 0;
    fragmentation.fragmentationLength[0] = kSpsSize;
    fragmentation.fragmentationOffset[1] = kSpsSize;
    fragmentation.fragmentationLength[1] = kPpsSize;
    fragmentation.fragmentationOffset[2] = kSpsSize + kPpsSize;
    fragmentation.fragmentationLength[2] =
        encoded_image._length - (kSpsSize + kPpsSize);
    const size_t kSpsNalHeader = 0x67;
    const size_t kPpsNalHeader = 0x68;
    const size_t kIdrNalHeader = 0x65;
    encoded_image._buffer[fragmentation.fragmentationOffset[0]] = kSpsNalHeader;
    encoded_image._buffer[fragmentation.fragmentationOffset[1]] = kPpsNalHeader;
    encoded_image._buffer[fragmentation.fragmentationOffset[2]] = kIdrNalHeader;
  } else {
    const size_t kNumSlices = 1;
    fragmentation.VerifyAndAllocateFragmentationHeader(kNumSlices);
    fragmentation.fragmentationOffset[0] = 0;
    fragmentation.fragmentationLength[0] = encoded_image._length;
    const size_t kNalHeader = 0x41;
    encoded_image._buffer[fragmentation.fragmentationOffset[0]] = kNalHeader;
  }
  uint8_t value = 0;
  int fragment_counter = 0;
  for (size_t i = 0; i < encoded_image._length; ++i) {
    if (fragment_counter == fragmentation.fragmentationVectorSize ||
        i != fragmentation.fragmentationOffset[fragment_counter]) {
      encoded_image._buffer[i] = value++;
    } else {
      ++fragment_counter;
    }
  }
  CodecSpecificInfo specifics;
  memset(&specifics, 0, sizeof(specifics));
  specifics.codecType = kVideoCodecH264;
  return callback_->OnEncodedImage(encoded_image, &specifics, &fragmentation);
}

DelayedEncoder::DelayedEncoder(Clock* clock, int delay_ms)
    : test::FakeEncoder(clock),
      delay_ms_(delay_ms) {}

void DelayedEncoder::SetDelay(int delay_ms) {
  rtc::CritScope lock(&lock_);
  delay_ms_ = delay_ms;
}

int32_t DelayedEncoder::Encode(const VideoFrame& input_image,
                               const CodecSpecificInfo* codec_specific_info,
                               const std::vector<FrameType>* frame_types) {
  int delay_ms = 0;
  {
    rtc::CritScope lock(&lock_);
    delay_ms = delay_ms_;
  }
  SleepMs(delay_ms);
  return FakeEncoder::Encode(input_image, codec_specific_info, frame_types);
}
}  // namespace test
}  // namespace webrtc
