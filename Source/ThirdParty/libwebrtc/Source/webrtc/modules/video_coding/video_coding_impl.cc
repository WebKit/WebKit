/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/video_coding_impl.h"

#include <algorithm>
#include <utility>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/common_types.h"
#include "webrtc/common_video/include/video_bitrate_allocator.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/video_coding/codecs/vp8/temporal_layers.h"
#include "webrtc/modules/video_coding/encoded_frame.h"
#include "webrtc/modules/video_coding/include/video_codec_initializer.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/modules/video_coding/jitter_buffer.h"
#include "webrtc/modules/video_coding/packet.h"
#include "webrtc/modules/video_coding/timing.h"
#include "webrtc/system_wrappers/include/clock.h"

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
  EncodedImageCallback* callback_ GUARDED_BY(cs_);
};

class VideoCodingModuleImpl : public VideoCodingModule {
 public:
  VideoCodingModuleImpl(Clock* clock,
                        EventFactory* event_factory,
                        NackSender* nack_sender,
                        KeyFrameRequestSender* keyframe_request_sender,
                        EncodedImageCallback* pre_decode_image_callback)
      : VideoCodingModule(),
        sender_(clock, &post_encode_callback_, nullptr),
        timing_(new VCMTiming(clock)),
        receiver_(clock,
                  event_factory,
                  pre_decode_image_callback,
                  timing_.get(),
                  nack_sender,
                  keyframe_request_sender) {}

  virtual ~VideoCodingModuleImpl() {}

  int64_t TimeUntilNextProcess() override {
    int64_t sender_time = sender_.TimeUntilNextProcess();
    int64_t receiver_time = receiver_.TimeUntilNextProcess();
    RTC_DCHECK_GE(sender_time, 0);
    RTC_DCHECK_GE(receiver_time, 0);
    return VCM_MIN(sender_time, receiver_time);
  }

  void Process() override {
    sender_.Process();
    receiver_.Process();
  }

  int32_t RegisterSendCodec(const VideoCodec* sendCodec,
                            uint32_t numberOfCores,
                            uint32_t maxPayloadSize) override {
    if (sendCodec != nullptr && sendCodec->codecType == kVideoCodecVP8) {
      // Set up a rate allocator and temporal layers factory for this vp8
      // instance. The codec impl will have a raw pointer to the TL factory,
      // and will call it when initializing. Since this can happen
      // asynchronously keep the instance alive until destruction or until a
      // new send codec is registered.
      VideoCodec vp8_codec = *sendCodec;
      std::unique_ptr<TemporalLayersFactory> tl_factory(
          new TemporalLayersFactory());
      vp8_codec.VP8()->tl_factory = tl_factory.get();
      rate_allocator_ = VideoCodecInitializer::CreateBitrateAllocator(
          vp8_codec, std::move(tl_factory));
      return sender_.RegisterSendCodec(&vp8_codec, numberOfCores,
                                       maxPayloadSize);
    }
    return sender_.RegisterSendCodec(sendCodec, numberOfCores, maxPayloadSize);
  }

  int32_t RegisterExternalEncoder(VideoEncoder* externalEncoder,
                                  uint8_t payloadType,
                                  bool internalSource) override {
    sender_.RegisterExternalEncoder(externalEncoder, payloadType,
                                    internalSource);
    return 0;
  }

  int Bitrate(unsigned int* bitrate) const override {
    return sender_.Bitrate(bitrate);
  }

  int FrameRate(unsigned int* framerate) const override {
    return sender_.FrameRate(framerate);
  }

  int32_t SetChannelParameters(uint32_t target_bitrate,  // bits/s.
                               uint8_t lossRate,
                               int64_t rtt) override {
    return sender_.SetChannelParameters(target_bitrate, lossRate, rtt,
                                        rate_allocator_.get(), nullptr);
  }

  int32_t RegisterProtectionCallback(
      VCMProtectionCallback* protection) override {
    return sender_.RegisterProtectionCallback(protection);
  }

  int32_t SetVideoProtection(VCMVideoProtection videoProtection,
                             bool enable) override {
    // TODO(pbos): Remove enable from receive-side protection modes as well.
    return receiver_.SetVideoProtection(videoProtection, enable);
  }

  int32_t AddVideoFrame(const VideoFrame& videoFrame,
                        const CodecSpecificInfo* codecSpecificInfo) override {
    return sender_.AddVideoFrame(videoFrame, codecSpecificInfo);
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

  int32_t RegisterReceiveStatisticsCallback(
      VCMReceiveStatisticsCallback* receiveStats) override {
    return receiver_.RegisterReceiveStatisticsCallback(receiveStats);
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

  int32_t SetMinimumPlayoutDelay(uint32_t minPlayoutDelayMs) override {
    return receiver_.SetMinimumPlayoutDelay(minPlayoutDelayMs);
  }

  int32_t SetRenderDelay(uint32_t timeMS) override {
    return receiver_.SetRenderDelay(timeMS);
  }

  int32_t Delay() const override { return receiver_.Delay(); }

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

  void SetDecodeErrorMode(VCMDecodeErrorMode decode_error_mode) override {
    return receiver_.SetDecodeErrorMode(decode_error_mode);
  }

  int SetMinReceiverDelay(int desired_delay_ms) override {
    return receiver_.SetMinReceiverDelay(desired_delay_ms);
  }

  int32_t SetReceiveChannelParameters(int64_t rtt) override {
    return receiver_.SetReceiveChannelParameters(rtt);
  }

  void RegisterPostEncodeImageCallback(
      EncodedImageCallback* observer) override {
    post_encode_callback_.Register(observer);
  }

  void TriggerDecoderShutdown() override { receiver_.TriggerDecoderShutdown(); }

 private:
  rtc::ThreadChecker construction_thread_;
  EncodedImageCallbackWrapper post_encode_callback_;
  vcm::VideoSender sender_;
  std::unique_ptr<VideoBitrateAllocator> rate_allocator_;
  std::unique_ptr<VCMTiming> timing_;
  vcm::VideoReceiver receiver_;
};
}  // namespace

void VideoCodingModule::Codec(VideoCodecType codecType, VideoCodec* codec) {
  VCMCodecDataBase::Codec(codecType, codec);
}

// DEPRECATED.  Create method for current interface, will be removed when the
// new jitter buffer is in place.
VideoCodingModule* VideoCodingModule::Create(Clock* clock,
                                             EventFactory* event_factory) {
  RTC_DCHECK(clock);
  RTC_DCHECK(event_factory);
  return new VideoCodingModuleImpl(clock, event_factory, nullptr, nullptr,
                                   nullptr);
}

}  // namespace webrtc
