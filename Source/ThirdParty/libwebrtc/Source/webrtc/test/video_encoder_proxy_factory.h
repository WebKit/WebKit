/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_VIDEO_ENCODER_PROXY_FACTORY_H_
#define TEST_VIDEO_ENCODER_PROXY_FACTORY_H_

#include <memory>
#include <vector>

#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_factory.h"

namespace webrtc {
namespace test {

namespace {
const VideoEncoder::Capabilities kCapabilities(false);
}

// An encoder factory with a single underlying VideoEncoder object,
// intended for test purposes. Each call to CreateVideoEncoder returns
// a proxy for the same encoder, typically an instance of FakeEncoder.
class VideoEncoderProxyFactory final : public VideoEncoderFactory {
 public:
  explicit VideoEncoderProxyFactory(VideoEncoder* encoder)
      : VideoEncoderProxyFactory(encoder, nullptr) {}

  explicit VideoEncoderProxyFactory(VideoEncoder* encoder,
                                    EncoderSelectorInterface* encoder_selector)
      : encoder_(encoder),
        encoder_selector_(encoder_selector),
        num_simultaneous_encoder_instances_(0),
        max_num_simultaneous_encoder_instances_(0) {
    codec_info_.is_hardware_accelerated = false;
    codec_info_.has_internal_source = false;
  }

  // Unused by tests.
  std::vector<SdpVideoFormat> GetSupportedFormats() const override {
    RTC_NOTREACHED();
    return {};
  }

  CodecInfo QueryVideoEncoder(const SdpVideoFormat& format) const override {
    return codec_info_;
  }

  std::unique_ptr<VideoEncoder> CreateVideoEncoder(
      const SdpVideoFormat& format) override {
    ++num_simultaneous_encoder_instances_;
    max_num_simultaneous_encoder_instances_ =
        std::max(max_num_simultaneous_encoder_instances_,
                 num_simultaneous_encoder_instances_);
    return std::make_unique<EncoderProxy>(encoder_, this);
  }

  std::unique_ptr<EncoderSelectorInterface> GetEncoderSelector()
      const override {
    if (encoder_selector_ != nullptr) {
      return std::make_unique<EncoderSelectorProxy>(encoder_selector_);
    }

    return nullptr;
  }

  void SetIsHardwareAccelerated(bool is_hardware_accelerated) {
    codec_info_.is_hardware_accelerated = is_hardware_accelerated;
  }
  void SetHasInternalSource(bool has_internal_source) {
    codec_info_.has_internal_source = has_internal_source;
  }

  int GetMaxNumberOfSimultaneousEncoderInstances() {
    return max_num_simultaneous_encoder_instances_;
  }

 private:
  void OnDestroyVideoEncoder() {
    RTC_CHECK_GT(num_simultaneous_encoder_instances_, 0);
    --num_simultaneous_encoder_instances_;
  }

  // Wrapper class, since CreateVideoEncoder needs to surrender
  // ownership to the object it returns.
  class EncoderProxy final : public VideoEncoder {
   public:
    explicit EncoderProxy(VideoEncoder* encoder,
                          VideoEncoderProxyFactory* encoder_factory)
        : encoder_(encoder), encoder_factory_(encoder_factory) {}
    ~EncoderProxy() { encoder_factory_->OnDestroyVideoEncoder(); }

   private:
    void SetFecControllerOverride(
        FecControllerOverride* fec_controller_override) override {
      encoder_->SetFecControllerOverride(fec_controller_override);
    }

    int32_t Encode(const VideoFrame& input_image,
                   const std::vector<VideoFrameType>* frame_types) override {
      return encoder_->Encode(input_image, frame_types);
    }

    int32_t InitEncode(const VideoCodec* config,
                       const Settings& settings) override {
      return encoder_->InitEncode(config, settings);
    }

    int32_t RegisterEncodeCompleteCallback(
        EncodedImageCallback* callback) override {
      return encoder_->RegisterEncodeCompleteCallback(callback);
    }

    int32_t Release() override { return encoder_->Release(); }

    void SetRates(const RateControlParameters& parameters) override {
      encoder_->SetRates(parameters);
    }

    VideoEncoder::EncoderInfo GetEncoderInfo() const override {
      return encoder_->GetEncoderInfo();
    }

    VideoEncoder* const encoder_;
    VideoEncoderProxyFactory* const encoder_factory_;
  };

  class EncoderSelectorProxy final : public EncoderSelectorInterface {
   public:
    explicit EncoderSelectorProxy(EncoderSelectorInterface* encoder_selector)
        : encoder_selector_(encoder_selector) {}

    void OnCurrentEncoder(const SdpVideoFormat& format) override {
      encoder_selector_->OnCurrentEncoder(format);
    }

    absl::optional<SdpVideoFormat> OnAvailableBitrate(
        const DataRate& rate) override {
      return encoder_selector_->OnAvailableBitrate(rate);
    }

    absl::optional<SdpVideoFormat> OnEncoderBroken() override {
      return encoder_selector_->OnEncoderBroken();
    }

   private:
    EncoderSelectorInterface* const encoder_selector_;
  };

  VideoEncoder* const encoder_;
  EncoderSelectorInterface* const encoder_selector_;
  CodecInfo codec_info_;

  int num_simultaneous_encoder_instances_;
  int max_num_simultaneous_encoder_instances_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_VIDEO_ENCODER_PROXY_FACTORY_H_
