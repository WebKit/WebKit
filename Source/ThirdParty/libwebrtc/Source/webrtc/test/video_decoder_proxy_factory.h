/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_VIDEO_DECODER_PROXY_FACTORY_H_
#define TEST_VIDEO_DECODER_PROXY_FACTORY_H_

#include <memory>
#include <vector>

#include "absl/memory/memory.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_decoder_factory.h"

namespace webrtc {
namespace test {

// An decoder factory with a single underlying VideoDecoder object, intended for
// test purposes. Each call to CreateVideoDecoder returns a proxy for the same
// decoder, typically an instance of FakeDecoder or MockEncoder.
class VideoDecoderProxyFactory final : public VideoDecoderFactory {
 public:
  explicit VideoDecoderProxyFactory(VideoDecoder* decoder)
      : decoder_(decoder) {}

  // Unused by tests.
  std::vector<SdpVideoFormat> GetSupportedFormats() const override {
    RTC_NOTREACHED();
    return {};
  }

  std::unique_ptr<VideoDecoder> CreateVideoDecoder(
      const SdpVideoFormat& format) override {
    return absl::make_unique<DecoderProxy>(decoder_);
  }

 private:
  // Wrapper class, since CreateVideoDecoder needs to surrender
  // ownership to the object it returns.
  class DecoderProxy final : public VideoDecoder {
   public:
    explicit DecoderProxy(VideoDecoder* decoder) : decoder_(decoder) {}

   private:
    int32_t Decode(const EncodedImage& input_image,
                   bool missing_frames,
                   const CodecSpecificInfo* codec_specific_info,
                   int64_t render_time_ms) override {
      return decoder_->Decode(input_image, missing_frames, codec_specific_info,
                              render_time_ms);
    }
    int32_t InitDecode(const VideoCodec* config,
                       int32_t number_of_cores) override {
      return decoder_->InitDecode(config, number_of_cores);
    }
    int32_t RegisterDecodeCompleteCallback(
        DecodedImageCallback* callback) override {
      return decoder_->RegisterDecodeCompleteCallback(callback);
    }
    int32_t Release() override { return decoder_->Release(); }
    bool PrefersLateDecoding() const { return decoder_->PrefersLateDecoding(); }
    const char* ImplementationName() const override {
      return decoder_->ImplementationName();
    }

    VideoDecoder* const decoder_;
  };

  VideoDecoder* const decoder_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_VIDEO_DECODER_PROXY_FACTORY_H_
