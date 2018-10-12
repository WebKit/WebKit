/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/fake_encoder.h"

#include <string.h>

#include <algorithm>
#include <memory>

#include "common_types.h"  // NOLINT(build/include)
#include "modules/video_coding/codecs/vp8/include/vp8_temporal_layers.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "rtc_base/checks.h"
#include "system_wrappers/include/sleep.h"

namespace webrtc {
namespace test {

const int kKeyframeSizeFactor = 10;

FakeEncoder::FakeEncoder(Clock* clock)
    : clock_(clock),
      callback_(nullptr),
      configured_input_framerate_(-1),
      max_target_bitrate_kbps_(-1),
      pending_keyframe_(true),
      debt_bytes_(0) {
  // Generate some arbitrary not-all-zero data
  for (size_t i = 0; i < sizeof(encoded_buffer_); ++i) {
    encoded_buffer_[i] = static_cast<uint8_t>(i);
  }
  for (bool& used : used_layers_) {
    used = false;
  }
}

void FakeEncoder::SetMaxBitrate(int max_kbps) {
  RTC_DCHECK_GE(max_kbps, -1);  // max_kbps == -1 disables it.
  rtc::CritScope cs(&crit_sect_);
  max_target_bitrate_kbps_ = max_kbps;
}

int32_t FakeEncoder::InitEncode(const VideoCodec* config,
                                int32_t number_of_cores,
                                size_t max_payload_size) {
  rtc::CritScope cs(&crit_sect_);
  config_ = *config;
  target_bitrate_.SetBitrate(0, 0, config_.startBitrate * 1000);
  configured_input_framerate_ = config_.maxFramerate;
  pending_keyframe_ = true;
  last_frame_info_ = FrameInfo();
  return 0;
}

int32_t FakeEncoder::Encode(const VideoFrame& input_image,
                            const CodecSpecificInfo* codec_specific_info,
                            const std::vector<FrameType>* frame_types) {
  unsigned char max_framerate;
  unsigned char num_simulcast_streams;
  SimulcastStream simulcast_streams[kMaxSimulcastStreams];
  EncodedImageCallback* callback;
  VideoBitrateAllocation target_bitrate;
  int framerate;
  VideoCodecMode mode;
  bool keyframe;
  {
    rtc::CritScope cs(&crit_sect_);
    max_framerate = config_.maxFramerate;
    num_simulcast_streams = config_.numberOfSimulcastStreams;
    for (int i = 0; i < num_simulcast_streams; ++i) {
      simulcast_streams[i] = config_.simulcastStream[i];
    }
    callback = callback_;
    target_bitrate = target_bitrate_;
    mode = config_.mode;
    if (configured_input_framerate_ > 0) {
      framerate = configured_input_framerate_;
    } else {
      framerate = max_framerate;
    }
    keyframe = pending_keyframe_;
    pending_keyframe_ = false;
  }

  FrameInfo frame_info =
      NextFrame(frame_types, keyframe, num_simulcast_streams, target_bitrate,
                simulcast_streams, framerate);
  for (uint8_t i = 0; i < frame_info.layers.size(); ++i) {
    if (frame_info.layers[i].size == 0) {
      // Drop this temporal layer.
      continue;
    }

    CodecSpecificInfo specifics;
    memset(&specifics, 0, sizeof(specifics));
    specifics.codecType = kVideoCodecGeneric;
    std::unique_ptr<uint8_t[]> encoded_buffer(
        new uint8_t[frame_info.layers[i].size]);
    memcpy(encoded_buffer.get(), encoded_buffer_, frame_info.layers[i].size);
    EncodedImage encoded(encoded_buffer.get(), frame_info.layers[i].size,
                         sizeof(encoded_buffer_));
    encoded.SetTimestamp(input_image.timestamp());
    encoded.capture_time_ms_ = input_image.render_time_ms();
    encoded._frameType =
        frame_info.keyframe ? kVideoFrameKey : kVideoFrameDelta;
    encoded._encodedWidth = simulcast_streams[i].width;
    encoded._encodedHeight = simulcast_streams[i].height;
    encoded.rotation_ = input_image.rotation();
    encoded.content_type_ = (mode == VideoCodecMode::kScreensharing)
                                ? VideoContentType::SCREENSHARE
                                : VideoContentType::UNSPECIFIED;
    encoded.SetSpatialIndex(i);
    specifics.codec_name = ImplementationName();
    if (callback->OnEncodedImage(encoded, &specifics, nullptr).error !=
        EncodedImageCallback::Result::OK) {
      return -1;
    }
  }
  return 0;
}

FakeEncoder::FrameInfo FakeEncoder::NextFrame(
    const std::vector<FrameType>* frame_types,
    bool keyframe,
    uint8_t num_simulcast_streams,
    const VideoBitrateAllocation& target_bitrate,
    SimulcastStream simulcast_streams[kMaxSimulcastStreams],
    int framerate) {
  FrameInfo frame_info;
  frame_info.keyframe = keyframe;

  if (frame_types) {
    for (FrameType frame_type : *frame_types) {
      if (frame_type == kVideoFrameKey) {
        frame_info.keyframe = true;
        break;
      }
    }
  }

  for (uint8_t i = 0; i < num_simulcast_streams; ++i) {
    if (target_bitrate.GetBitrate(i, 0) > 0) {
      int temporal_id = last_frame_info_.layers.size() > i
                            ? ++last_frame_info_.layers[i].temporal_id %
                                  simulcast_streams[i].numberOfTemporalLayers
                            : 0;
      frame_info.layers.emplace_back(0, temporal_id);
    }
  }

  if (last_frame_info_.layers.size() < frame_info.layers.size()) {
    // A new keyframe is needed since a new layer will be added.
    frame_info.keyframe = true;
  }

  for (uint8_t i = 0; i < frame_info.layers.size(); ++i) {
    FrameInfo::SpatialLayer& layer_info = frame_info.layers[i];
    if (frame_info.keyframe) {
      layer_info.temporal_id = 0;
      size_t avg_frame_size =
          (target_bitrate.GetBitrate(i, 0) + 7) / (8 * framerate);

      // The first frame is a key frame and should be larger.
      // Store the overshoot bytes and distribute them over the coming frames,
      // so that we on average meet the bitrate target.
      debt_bytes_ += (kKeyframeSizeFactor - 1) * avg_frame_size;
      layer_info.size = kKeyframeSizeFactor * avg_frame_size;
    } else {
      size_t avg_frame_size =
          (target_bitrate.GetBitrate(i, layer_info.temporal_id) + 7) /
          (8 * framerate);
      layer_info.size = avg_frame_size;
      if (debt_bytes_ > 0) {
        // Pay at most half of the frame size for old debts.
        size_t payment_size = std::min(avg_frame_size / 2, debt_bytes_);
        debt_bytes_ -= payment_size;
        layer_info.size -= payment_size;
      }
    }
  }
  last_frame_info_ = frame_info;
  return frame_info;
}

int32_t FakeEncoder::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  rtc::CritScope cs(&crit_sect_);
  callback_ = callback;
  return 0;
}

int32_t FakeEncoder::Release() {
  return 0;
}

int32_t FakeEncoder::SetChannelParameters(uint32_t packet_loss, int64_t rtt) {
  return 0;
}

int32_t FakeEncoder::SetRateAllocation(
    const VideoBitrateAllocation& rate_allocation,
    uint32_t framerate) {
  rtc::CritScope cs(&crit_sect_);
  target_bitrate_ = rate_allocation;
  int allocated_bitrate_kbps = target_bitrate_.get_sum_kbps();

  // Scale bitrate allocation to not exceed the given max target bitrate.
  if (max_target_bitrate_kbps_ > 0 &&
      allocated_bitrate_kbps > max_target_bitrate_kbps_) {
    for (uint8_t spatial_idx = 0; spatial_idx < kMaxSpatialLayers;
         ++spatial_idx) {
      for (uint8_t temporal_idx = 0; temporal_idx < kMaxTemporalStreams;
           ++temporal_idx) {
        if (target_bitrate_.HasBitrate(spatial_idx, temporal_idx)) {
          uint32_t bitrate =
              target_bitrate_.GetBitrate(spatial_idx, temporal_idx);
          bitrate = static_cast<uint32_t>(
              (bitrate * int64_t{max_target_bitrate_kbps_}) /
              allocated_bitrate_kbps);
          target_bitrate_.SetBitrate(spatial_idx, temporal_idx, bitrate);
        }
      }
    }
  }

  configured_input_framerate_ = framerate;
  return 0;
}

const char* FakeEncoder::kImplementationName = "fake_encoder";
const char* FakeEncoder::ImplementationName() const {
  return kImplementationName;
}

int FakeEncoder::GetConfiguredInputFramerate() const {
  rtc::CritScope cs(&crit_sect_);
  return configured_input_framerate_;
}

FakeH264Encoder::FakeH264Encoder(Clock* clock)
    : FakeEncoder(clock), callback_(nullptr), idr_counter_(0) {
  FakeEncoder::RegisterEncodeCompleteCallback(this);
}

int32_t FakeH264Encoder::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  rtc::CritScope cs(&local_crit_sect_);
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
  EncodedImageCallback* callback;
  int current_idr_counter;
  {
    rtc::CritScope cs(&local_crit_sect_);
    callback = callback_;
    current_idr_counter = idr_counter_;
    ++idr_counter_;
  }
  RTPFragmentationHeader fragmentation;
  if (current_idr_counter % kIdrFrequency == 0 &&
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
  specifics.codecSpecific.H264.packetization_mode =
      H264PacketizationMode::NonInterleaved;
  RTC_DCHECK(callback);
  return callback->OnEncodedImage(encoded_image, &specifics, &fragmentation);
}

DelayedEncoder::DelayedEncoder(Clock* clock, int delay_ms)
    : test::FakeEncoder(clock), delay_ms_(delay_ms) {
  // The encoder could be created on a different thread than
  // it is being used on.
  sequence_checker_.Detach();
}

void DelayedEncoder::SetDelay(int delay_ms) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  delay_ms_ = delay_ms;
}

int32_t DelayedEncoder::Encode(const VideoFrame& input_image,
                               const CodecSpecificInfo* codec_specific_info,
                               const std::vector<FrameType>* frame_types) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);

  SleepMs(delay_ms_);

  return FakeEncoder::Encode(input_image, codec_specific_info, frame_types);
}

MultithreadedFakeH264Encoder::MultithreadedFakeH264Encoder(Clock* clock)
    : test::FakeH264Encoder(clock),
      current_queue_(0),
      queue1_(nullptr),
      queue2_(nullptr) {
  // The encoder could be created on a different thread than
  // it is being used on.
  sequence_checker_.Detach();
}

int32_t MultithreadedFakeH264Encoder::InitEncode(const VideoCodec* config,
                                                 int32_t number_of_cores,
                                                 size_t max_payload_size) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);

  queue1_.reset(new rtc::TaskQueue("Queue 1"));
  queue2_.reset(new rtc::TaskQueue("Queue 2"));

  return FakeH264Encoder::InitEncode(config, number_of_cores, max_payload_size);
}

class MultithreadedFakeH264Encoder::EncodeTask : public rtc::QueuedTask {
 public:
  EncodeTask(MultithreadedFakeH264Encoder* encoder,
             const VideoFrame& input_image,
             const CodecSpecificInfo* codec_specific_info,
             const std::vector<FrameType>* frame_types)
      : encoder_(encoder),
        input_image_(input_image),
        codec_specific_info_(),
        frame_types_(*frame_types) {
    if (codec_specific_info)
      codec_specific_info_ = *codec_specific_info;
  }

 private:
  bool Run() override {
    encoder_->EncodeCallback(input_image_, &codec_specific_info_,
                             &frame_types_);
    return true;
  }

  MultithreadedFakeH264Encoder* const encoder_;
  VideoFrame input_image_;
  CodecSpecificInfo codec_specific_info_;
  std::vector<FrameType> frame_types_;
};

int32_t MultithreadedFakeH264Encoder::Encode(
    const VideoFrame& input_image,
    const CodecSpecificInfo* codec_specific_info,
    const std::vector<FrameType>* frame_types) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);

  std::unique_ptr<rtc::TaskQueue>& queue =
      (current_queue_++ % 2 == 0) ? queue1_ : queue2_;

  if (!queue) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  queue->PostTask(std::unique_ptr<rtc::QueuedTask>(
      new EncodeTask(this, input_image, codec_specific_info, frame_types)));

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t MultithreadedFakeH264Encoder::EncodeCallback(
    const VideoFrame& input_image,
    const CodecSpecificInfo* codec_specific_info,
    const std::vector<FrameType>* frame_types) {
  return FakeH264Encoder::Encode(input_image, codec_specific_info, frame_types);
}

int32_t MultithreadedFakeH264Encoder::Release() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);

  queue1_.reset();
  queue2_.reset();

  return FakeH264Encoder::Release();
}

}  // namespace test
}  // namespace webrtc
