/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_TEST_VIDEO_CODEC_TEST_UTILS_H_
#define API_VIDEO_CODECS_TEST_VIDEO_CODEC_TEST_UTILS_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <set>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "absl/strings/match.h"
#include "api/environment/environment.h"
#include "api/scoped_refptr.h"
#include "api/test/frame_generator_interface.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/video/encoded_image.h"
#include "api/video/resolution.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_buffer.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_decoder_factory_template.h"
#include "api/video_codecs/video_decoder_factory_template_dav1d_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_open_h264_adapter.h"
#include "api/video_codecs/video_encoder_builders.h"
#include "api/video_codecs/video_encoder_builders_for_test.h"
#include "api/video_codecs/video_encoder_factory_interface.h"
#include "api/video_codecs/video_encoder_interface.h"
#include "api/video_codecs/video_encoding_general.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "test/gmock.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/frame_reader.h"
#include "test/testsupport/y4m_frame_generator.h"

namespace webrtc {

using Cbr = VideoEncoderInterface::FrameEncodeSettings::Cbr;
using Cqp = VideoEncoderInterface::FrameEncodeSettings::Cqp;
using EncodedData = VideoEncoderInterface::EncodedData;
using FrameType = VideoEncoderInterface::FrameType;
using TemporalUnitSettings = VideoEncoderInterface::TemporalUnitSettings;
using EncOut = TestEncodedOutput;
using Fb = FrameEncodeSettingsBuilderForTest;
using FactoryCreator = std::unique_ptr<VideoEncoderFactoryInterface> (*)();

template <int N>
std::vector<VideoEncoderInterface::FrameEncodeSettings> ToVec(
    VideoEncoderInterface::FrameEncodeSettings (&&settings)[N]) {
  return std::vector<VideoEncoderInterface::FrameEncodeSettings>(
      std::make_move_iterator(std::begin(settings)),
      std::make_move_iterator(std::end(settings)));
}

constexpr Resolution kDefaultResolution = {.width = 640, .height = 360};

inline Resolution GetResolution(const VideoFrame& frame) {
  return {.width = frame.width(), .height = frame.height()};
}

MATCHER(HasBitstreamAndMetaData, "") {
  return !arg.bitstream.empty() && std::holds_alternative<EncodedData>(arg.res);
}

MATCHER_P(QpIs, qp, "") {
  if (const EncodedData* ed = std::get_if<EncodedData>(&arg.res)) {
    return ed->encoded_qp == qp;
  }
  return false;
}

inline double Psnr(const scoped_refptr<I420BufferInterface>& ref_buffer,
                   const VideoFrame& decoded_frame) {
  return I420PSNR(*ref_buffer, *decoded_frame.video_frame_buffer()->ToI420());
}

inline std::unique_ptr<test::FrameReader> CreateFrameReader() {
  return CreateY4mFrameReader(
      test::ResourcePath("reference_video_640x360_30fps", "y4m"),
      test::YuvFrameReaderImpl::RepeatMode::kPingPong);
}

inline std::unique_ptr<test::FrameGeneratorInterface> CreateFrameGenerator() {
  return std::make_unique<test::Y4mFrameGenerator>(
      test::ResourcePath("reference_video_640x360_30fps", "y4m"),
      test::Y4mFrameGenerator::RepeatMode::kPingPong);
}

inline std::unique_ptr<VideoDecoderFactory> CreateTestDecoderFactory() {
  return std::make_unique<VideoDecoderFactoryTemplate<
      LibvpxVp8DecoderTemplateAdapter, LibvpxVp9DecoderTemplateAdapter,
      OpenH264DecoderTemplateAdapter, Dav1dDecoderTemplateAdapter>>();
}

class TestDecoder : public DecodedImageCallback {
 public:
  TestDecoder(const Environment& env,
              VideoDecoderFactory* factory,
              const std::string& codec_name) {
    for (const auto& format : factory->GetSupportedFormats()) {
      if (absl::EqualsIgnoreCase(format.name, codec_name)) {
        decoder_ = factory->Create(env, format);
        break;
      }
    }
    if (decoder_) {
      decoder_->Configure({});
      decoder_->RegisterDecodeCompleteCallback(this);
    }
  }

  bool IsSupported() const { return decoder_ != nullptr; }

  // DecodedImageCallback
  int32_t Decoded(VideoFrame& frame) override {
    decode_result_ = std::make_unique<VideoFrame>(std::move(frame));
    decoded_event_.Set();
    return 0;
  }

  VideoFrame Decode(std::span<const uint8_t> bitstream_data) {
    RTC_CHECK(decoder_);
    EncodedImage img;
    img.SetEncodedData(EncodedImageBuffer::Create(bitstream_data.data(),
                                                  bitstream_data.size()));
    decoded_event_.Reset();
    decoder_->Decode(img, /*dont_care=*/0);
    RTC_CHECK(decoded_event_.Wait(TimeDelta::Seconds(5)));
    RTC_CHECK(decode_result_);
    VideoFrame res(std::move(*decode_result_));
    decode_result_.reset();
    return res;
  }

 private:
  std::unique_ptr<VideoDecoder> decoder_;
  std::unique_ptr<VideoFrame> decode_result_;
  Event decoded_event_;
};

struct TestConfig {
  VideoEncoderFactoryInterface::StaticEncoderSettings static_settings;
  std::variant<VideoEncoderInterface::FrameEncodeSettings::Cbr,
               VideoEncoderInterface::FrameEncodeSettings::Cqp>
      rate_options;
};

inline void ConfigureRate(FrameEncodeSettingsBuilderForTest& fb,
                          const std::variant<Cbr, Cqp>& rate_options) {
  std::visit(
      [&fb](const auto& rate) -> void {
        using T = std::decay_t<decltype(rate)>;
        if constexpr (std::is_same_v<
                          T, VideoEncoderInterface::FrameEncodeSettings::Cbr>) {
          fb.Cbr(rate);
        } else if constexpr (std::is_same_v<T, VideoEncoderInterface::
                                                   FrameEncodeSettings::Cqp>) {
          fb.Cqp(rate.target_qp);
        }
      },
      rate_options);
}

inline VideoEncoderInterface::FrameEncodeSettings BuildSettings(
    FrameEncodeSettingsBuilderForTest&& fb,
    const std::variant<Cbr, Cqp>& rate_options) {
  ConfigureRate(fb, rate_options);
  return fb.Build();
}

inline TestConfig CreateTestConfig(
    const VideoEncoderFactoryInterface::Capabilities& capabilities) {
  const auto& rc_modes = capabilities.bitrate_control().rc_modes();
  RTC_DCHECK(!rc_modes.empty()) << "Encoder must support at least one RC mode.";

  TestConfig config;
  if (rc_modes.contains(VideoEncoderFactoryInterface::RateControlMode::kCqp)) {
    config.static_settings =
        StaticEncoderSettingsBuilder()
            .MaxEncodeDimensions({.width = 640, .height = 360})
            .EncodingFormat({.sub_sampling = EncodingFormat::SubSampling::k420,
                             .bit_depth = 8})
            .CqpRcMode()
            .MaxNumberOfThreads(1)
            .Build();
    config.rate_options = VideoEncoderInterface::FrameEncodeSettings::Cqp{
        .target_qp = std::clamp(20, capabilities.bitrate_control().min_qp(),
                                capabilities.bitrate_control().max_qp())};
  } else {
    config.static_settings =
        StaticEncoderSettingsBuilder()
            .MaxEncodeDimensions({.width = 640, .height = 360})
            .EncodingFormat({.sub_sampling = EncodingFormat::SubSampling::k420,
                             .bit_depth = 8})
            .CbrRcMode(TimeDelta::Millis(1000), TimeDelta::Millis(600),
                       /*max_intra_bitrate_factor=*/3.0)
            .MaxNumberOfThreads(1)
            .Build();
    config.rate_options = VideoEncoderInterface::FrameEncodeSettings::Cbr{
        .duration = TimeDelta::Millis(100),
        .target_bitrate = DataRate::KilobitsPerSec(1000)};
  }
  return config;
}

}  // namespace webrtc

#endif  // API_VIDEO_CODECS_TEST_VIDEO_CODEC_TEST_UTILS_H_
