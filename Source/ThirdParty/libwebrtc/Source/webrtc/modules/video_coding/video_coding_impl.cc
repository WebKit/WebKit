/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/video_coding_impl.h"

#include <algorithm>
#include <utility>

#include "api/video/builtin_video_bitrate_allocator_factory.h"
#include "api/video/video_bitrate_allocator.h"
#include "common_types.h"  // NOLINT(build/include)
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "modules/video_coding/encoded_frame.h"
#include "modules/video_coding/include/video_codec_initializer.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/jitter_buffer.h"
#include "modules/video_coding/packet.h"
#include "modules/video_coding/timing.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/thread_checker.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace vcm {

int64_t VCMProcessTimer::Period() const {
  return _periodMs;
}

int64_t VCMProcessTimer::TimeUntilProcess() const {
  const int64_t time_since_process = _clock->TimeInMilliseconds() - _latestMs;
  const int64_t time_until_process = _periodMs - time_since_process;
  return std::max<int64_t>(time_until_process, 0);
}

void VCMProcessTimer::Processed() {
  _latestMs = _clock->TimeInMilliseconds();
}
}  // namespace vcm

namespace {
// This wrapper provides a way to modify the callback without the need to expose
// a register method all the way down to the function calling it.
class EncodedImageCallbackWrapper : public EncodedImageCallback {
 public:
  EncodedImageCallbackWrapper() : callback_(nullptr) {}

  virtual ~EncodedImageCallbackWrapper() {}

  void Register(EncodedImageCallback* callback) {
    rtc::CritScope lock(&cs_);
    callback_ = callback;
  }

  virtual Result OnEncodedImage(const EncodedImage& encoded_image,
                                const CodecSpecificInfo* codec_specific_info,
                                const RTPFragmentationHeader* fragmentation) {
    rtc::CritScope lock(&cs_);
    if (callback_) {
      return callback_->OnEncodedImage(encoded_image, codec_specific_info,
                                       fragmentation);
    }
    return Result(Result::ERROR_SEND_FAILED);
  }

 private:
  rtc::CriticalSection cs_;
  EncodedImageCallback* callback_ RTC_GUARDED_BY(cs_);
};

class VideoCodingModuleImpl : public VideoCodingModule {
 public:
  VideoCodingModuleImpl(Clock* clock,
                        NackSender* nack_sender,
                        KeyFrameRequestSender* keyframe_request_sender)
      : VideoCodingModule(),
        sender_(clock, &post_encode_callback_),
        rate_allocator_factory_(CreateBuiltinVideoBitrateAllocatorFactory()),
        timing_(new VCMTiming(clock)),
        receiver_(clock, timing_.get(), nack_sender, keyframe_request_sender) {}

  virtual ~VideoCodingModuleImpl() {}

  int64_t TimeUntilNextProcess() override {
    int64_t receiver_time = receiver_.TimeUntilNextProcess();
    RTC_DCHECK_GE(receiver_time, 0);
    return receiver_time;
  }

  void Process() override { receiver_.Process(); }

  int32_t RegisterSendCodec(const VideoCodec* sendCodec,
                            uint32_t numberOfCores,
                            uint32_t maxPayloadSize) override {
    if (sendCodec != nullptr && ((sendCodec->codecType == kVideoCodecVP8) ||
                                 (sendCodec->codecType == kVideoCodecH264))) {
      // Set up a rate allocator and temporal layers factory for this codec
      // instance. The codec impl will have a raw pointer to the TL factory,
      // and will call it when initializing. Since this can happen
      // asynchronously keep the instance alive until destruction or until a
      // new send codec is registered.
      VideoCodec codec = *sendCodec;
      rate_allocator_ =
          rate_allocator_factory_->CreateVideoBitrateAllocator(codec);
      return sender_.RegisterSendCodec(&codec, numberOfCores, maxPayloadSize);
    }
    return sender_.RegisterSendCodec(sendCodec, numberOfCores, maxPayloadSize);
  }

  int32_t RegisterExternalEncoder(VideoEncoder* externalEncoder,
                                  uint8_t /* payloadType */,
                                  bool internalSource) override {
    sender_.RegisterExternalEncoder(externalEncoder, internalSource);
    return 0;
  }

  int32_t SetChannelParameters(uint32_t target_bitrate,  // bits/s.
                               uint8_t lossRate,
                               int64_t rtt) override {
    return sender_.SetChannelParameters(target_bitrate, lossRate, rtt,
                                        rate_allocator_.get(), nullptr);
  }

  int32_t SetVideoProtection(VCMVideoProtection videoProtection,
                             bool enable) override {
    // TODO(pbos): Remove enable from receive-side protection modes as well.
    return receiver_.SetVideoProtection(videoProtection, enable);
  }

  int32_t AddVideoFrame(const VideoFrame& videoFrame,
                        const CodecSpecificInfo* codecSpecificInfo) override {
    return sender_.AddVideoFrame(videoFrame, codecSpecificInfo, absl::nullopt);
  }

  int32_t IntraFrameRequest(size_t stream_index) override {
    return sender_.IntraFrameRequest(stream_index);
  }

  int32_t EnableFrameDropper(bool enable) override {
    return sender_.EnableFrameDropper(enable);
  }

  int32_t RegisterReceiveCodec(const VideoCodec* receiveCodec,
                               int32_t numberOfCores,
                               bool requireKeyFrame) override {
    return receiver_.RegisterReceiveCodec(receiveCodec, numberOfCores,
                                          requireKeyFrame);
  }

  void RegisterExternalDecoder(VideoDecoder* externalDecoder,
                               uint8_t payloadType) override {
    receiver_.RegisterExternalDecoder(externalDecoder, payloadType);
  }

  int32_t RegisterReceiveCallback(
      VCMReceiveCallback* receiveCallback) override {
    RTC_DCHECK(construction_thread_.CalledOnValidThread());
    return receiver_.RegisterReceiveCallback(receiveCallback);
  }

  int32_t RegisterFrameTypeCallback(
      VCMFrameTypeCallback* frameTypeCallback) override {
    return receiver_.RegisterFrameTypeCallback(frameTypeCallback);
  }

  int32_t RegisterPacketRequestCallback(
      VCMPacketRequestCallback* callback) override {
    RTC_DCHECK(construction_thread_.CalledOnValidThread());
    return receiver_.RegisterPacketRequestCallback(callback);
  }

  int32_t Decode(uint16_t maxWaitTimeMs) override {
    return receiver_.Decode(maxWaitTimeMs);
  }

  int32_t IncomingPacket(const uint8_t* incomingPayload,
                         size_t payloadLength,
                         const WebRtcRTPHeader& rtpInfo) override {
    return receiver_.IncomingPacket(incomingPayload, payloadLength, rtpInfo);
  }

  int SetReceiverRobustnessMode(ReceiverRobustness robustnessMode,
                                VCMDecodeErrorMode errorMode) override {
    return receiver_.SetReceiverRobustnessMode(robustnessMode, errorMode);
  }

  void SetNackSettings(size_t max_nack_list_size,
                       int max_packet_age_to_nack,
                       int max_incomplete_time_ms) override {
    return receiver_.SetNackSettings(max_nack_list_size, max_packet_age_to_nack,
                                     max_incomplete_time_ms);
  }

  void RegisterPostEncodeImageCallback(
      EncodedImageCallback* observer) override {
    post_encode_callback_.Register(observer);
  }

 private:
  rtc::ThreadChecker construction_thread_;
  EncodedImageCallbackWrapper post_encode_callback_;
  vcm::VideoSender sender_;
  const std::unique_ptr<VideoBitrateAllocatorFactory> rate_allocator_factory_;
  std::unique_ptr<VideoBitrateAllocator> rate_allocator_;
  const std::unique_ptr<VCMTiming> timing_;
  vcm::VideoReceiver receiver_;
};
}  // namespace

// DEPRECATED.  Create method for current interface, will be removed when the
// new jitter buffer is in place.
VideoCodingModule* VideoCodingModule::Create(Clock* clock) {
  RTC_DCHECK(clock);
  return new VideoCodingModuleImpl(clock, nullptr, nullptr);
}

}  // namespace webrtc
