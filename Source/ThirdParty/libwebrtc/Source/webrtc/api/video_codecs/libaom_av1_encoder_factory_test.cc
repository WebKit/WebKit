/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/libaom_av1_encoder_factory.h"

#include <cstdio>
#include <utility>
#include <vector>

#include "absl/types/variant.h"
#include "api/video/i420_buffer.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_encoder_interface.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "modules/video_coding/codecs/av1/dav1d_decoder.h"
#include "rtc_base/logging.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/frame_reader.h"
#include "test/testsupport/frame_writer.h"

namespace webrtc {
namespace {
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Gt;
using ::testing::IsEmpty;
using ::testing::Lt;
using ::testing::MockFunction;
using ::testing::NotNull;
using Cbr = VideoEncoderInterface::FrameEncodeSettings::Cbr;
using Cqp = VideoEncoderInterface::FrameEncodeSettings::Cqp;
using EncodedData = VideoEncoderInterface::EncodedData;
using EncodeResult = VideoEncoderInterface::EncodeResult;
using EncodeResultCallback = VideoEncoderInterface::EncodeResultCallback;
using FrameType = VideoEncoderInterface::FrameType;

std::unique_ptr<test::FrameReader> CreateFrameReader() {
  return CreateY4mFrameReader(
      test::ResourcePath("reference_video_640x360_30fps", "y4m"),
      test::YuvFrameReaderImpl::RepeatMode::kPingPong);
}

std::string OutPath() {
  std::string res = test::OutputPath();
  res += "frame_dump/";
  RTC_CHECK(test::DirExists(res) || test::CreateDir(res));
  return res;
}

class EncodeResults {
 public:
  EncodeResultCallback Cb() {
    return [&](const EncodeResult& result) { results_.push_back(result); };
  }

  EncodedData* FrameAt(int index) {
    if (index < 0 || index > static_cast<int>(results_.size())) {
      RTC_CHECK(false);
      return nullptr;
    }
    return absl::get_if<EncodedData>(&results_[index]);
  }

 private:
  std::vector<EncodeResult> results_;
};

class Av1Decoder : public DecodedImageCallback {
 public:
  Av1Decoder() : Av1Decoder("") {}

  explicit Av1Decoder(const std::string& name)
      : decoder_(CreateDav1dDecoder()), file_name_(name) {
    decoder_->Configure({});
    decoder_->RegisterDecodeCompleteCallback(this);

    if (!file_name_.empty()) {
      std::string out = OutPath();
      out += file_name_;
      out += "_raw.av1";
      RTC_CHECK(raw_out_file_ = fopen(out.c_str(), "wb"));
      RTC_LOG(LS_INFO) << "Recording bitstream to " << out;
    }
  }

  ~Av1Decoder() {
    if (raw_out_file_) {
      fclose(raw_out_file_);
    }
  }

  // DecodedImageCallback
  int32_t Decoded(VideoFrame& frame) override {
    decode_result_ = std::make_unique<VideoFrame>(std::move(frame));
    return 0;
  }

  VideoFrame Decode(const EncodedData& encoded_data) {
    EncodedImage img;
    img.SetEncodedData(encoded_data.bitstream_data);
    if (raw_out_file_) {
      fwrite(encoded_data.bitstream_data->data(), 1,
             encoded_data.bitstream_data->size(), raw_out_file_);
    }
    decoder_->Decode(img, /*dont_care=*/0);
    VideoFrame res(std::move(*decode_result_));
    return res;
  }

 private:
  std::unique_ptr<VideoDecoder> decoder_;
  std::unique_ptr<VideoFrame> decode_result_;
  std::string file_name_;
  FILE* raw_out_file_ = nullptr;
};

class FrameEncoderSettingsBuilder {
 public:
  FrameEncoderSettingsBuilder& Key() {
    frame_encode_settings_.frame_type = FrameType::kKeyframe;
    return *this;
  }

  FrameEncoderSettingsBuilder& Start() {
    frame_encode_settings_.frame_type = FrameType::kStartFrame;
    return *this;
  }

  FrameEncoderSettingsBuilder& Delta() {
    frame_encode_settings_.frame_type = FrameType::kStartFrame;
    return *this;
  }

  FrameEncoderSettingsBuilder& Rate(
      const absl::variant<Cqp, Cbr>& rate_options) {
    frame_encode_settings_.rate_options = rate_options;
    return *this;
  }

  FrameEncoderSettingsBuilder& T(int id) {
    frame_encode_settings_.temporal_id = id;
    return *this;
  }

  FrameEncoderSettingsBuilder& S(int id) {
    frame_encode_settings_.spatial_id = id;
    return *this;
  }

  FrameEncoderSettingsBuilder& Res(int width, int height) {
    frame_encode_settings_.resolution = {width, height};
    return *this;
  }

  FrameEncoderSettingsBuilder& Ref(const std::vector<int>& ref) {
    frame_encode_settings_.reference_buffers = ref;
    return *this;
  }

  FrameEncoderSettingsBuilder& Upd(int upd) {
    frame_encode_settings_.update_buffer = upd;
    return *this;
  }

  FrameEncoderSettingsBuilder& Cb(EncodeResultCallback cb) {
    frame_encode_settings_.result_callback = std::move(cb);
    return *this;
  }

  operator VideoEncoderInterface::FrameEncodeSettings&&() {
    return std::move(frame_encode_settings_);
  }

 private:
  VideoEncoderInterface::FrameEncodeSettings frame_encode_settings_;
};

using Fb = FrameEncoderSettingsBuilder;

// Since FrameEncodeSettings is move only, initalizer-list initialization won't
// work, so instead a C-style array can be used to do aggregate initialization.
template <int N>
std::vector<VideoEncoderInterface::FrameEncodeSettings> ToVec(
    VideoEncoderInterface::FrameEncodeSettings (&&settings)[N]) {
  return std::vector<VideoEncoderInterface::FrameEncodeSettings>(
      std::make_move_iterator(std::begin(settings)),
      std::make_move_iterator(std::end(settings)));
}

// For reasonable debug printout when an EXPECT fail.
struct Resolution {
  explicit Resolution(const VideoFrame& frame)
      : width(frame.width()), height(frame.height()) {}

  friend void PrintTo(const Resolution& res, std::ostream* os) {
    *os << "(width: " << res.width << " height: " << res.height << ")";
  }

  int width;
  int height;
};

MATCHER_P2(ResolutionIs, width, height, "") {
  return arg.width == width && arg.height == height;
}

double Psnr(const rtc::scoped_refptr<I420BufferInterface>& ref_buffer,
            const VideoFrame& decoded_frame) {
  return I420PSNR(*ref_buffer, *decoded_frame.video_frame_buffer()->ToI420());
}

static constexpr VideoEncoderFactoryInterface::StaticEncoderSettings
    kCbrEncoderSettings{
        .max_encode_dimensions = {.width = 1920, .height = 1080},
        .encoding_format = {.sub_sampling = EncodingFormat::SubSampling::k420,
                            .bit_depth = 8},
        .rc_mode =
            VideoEncoderFactoryInterface::StaticEncoderSettings::Cbr{
                .max_buffer_size = TimeDelta::Millis(1000),
                .target_buffer_size = TimeDelta::Millis(600)},
        .max_number_of_threads = 1,
    };

static constexpr VideoEncoderFactoryInterface::StaticEncoderSettings
    kCqpEncoderSettings{
        .max_encode_dimensions = {.width = 1920, .height = 1080},
        .encoding_format = {.sub_sampling = EncodingFormat::SubSampling::k420,
                            .bit_depth = 8},
        .rc_mode = VideoEncoderFactoryInterface::StaticEncoderSettings::Cqp(),
        .max_number_of_threads = 1,
    };

static constexpr Cbr kCbr{.duration = TimeDelta::Millis(100),
                          .target_bitrate = DataRate::KilobitsPerSec(1000)};

TEST(LibaomAv1EncoderFactory, CodecName) {
  EXPECT_THAT(LibaomAv1EncoderFactory().CodecName(), Eq("AV1"));
}

TEST(LibaomAv1EncoderFactory, CodecSpecifics) {
  EXPECT_THAT(LibaomAv1EncoderFactory().CodecSpecifics(), IsEmpty());
}

TEST(LibaomAv1EncoderFactory, QpRange) {
  const std::pair<int, int> kMinMaxQp = {0, 63};
  EXPECT_THAT(
      LibaomAv1EncoderFactory().GetEncoderCapabilities().rate_control.qp_range,
      Eq(kMinMaxQp));
}

TEST(LibaomAv1Encoder, KeyframeUpdatesSpecifiedBuffer) {
  auto frame_reader = CreateFrameReader();
  auto enc = LibaomAv1EncoderFactory().CreateEncoder(kCbrEncoderSettings, {});
  EncodeResults res;
  Av1Decoder dec;

  auto raw_key = frame_reader->PullFrame();
  auto raw_delta = frame_reader->PullFrame();

  enc->Encode(raw_key, {.presentation_timestamp = Timestamp::Millis(0)},
              ToVec({Fb().Rate(kCbr).Res(640, 360).Upd(5).Key().Cb(res.Cb())}));
  ASSERT_THAT(res.FrameAt(0), NotNull());
  VideoFrame decoded_key = dec.Decode(*res.FrameAt(0));
  EXPECT_THAT(Resolution(decoded_key), ResolutionIs(640, 360));
  EXPECT_THAT(Psnr(raw_key, decoded_key), Gt(40));

  enc->Encode(raw_delta, {.presentation_timestamp = Timestamp::Millis(100)},
              ToVec({Fb().Rate(kCbr).Res(640, 360).Ref({0}).Cb(res.Cb())}));
  ASSERT_THAT(res.FrameAt(1), Eq(nullptr));
}

TEST(LibaomAv1Encoder, MidTemporalUnitKeyframeResetsBuffers) {
  auto frame_reader = CreateFrameReader();
  auto enc = LibaomAv1EncoderFactory().CreateEncoder(kCbrEncoderSettings, {});
  EncodeResults res;
  Av1Decoder dec;

  enc->Encode(
      frame_reader->PullFrame(),
      {.presentation_timestamp = Timestamp::Millis(0)},
      ToVec({Fb().Rate(kCbr).Res(160, 90).S(0).Upd(0).Key().Cb(res.Cb()),
             Fb().Rate(kCbr).Res(320, 180).S(1).Ref({0}).Cb(res.Cb()),
             Fb().Rate(kCbr).Res(640, 360).S(2).Ref({0}).Cb(res.Cb())}));
  ASSERT_THAT(res.FrameAt(2), NotNull());

  enc->Encode(
      frame_reader->PullFrame(),
      {.presentation_timestamp = Timestamp::Millis(100)},
      ToVec({Fb().Rate(kCbr).Res(160, 90).S(0).Upd(0).Ref({0}).Cb(res.Cb()),
             Fb().Rate(kCbr).Res(320, 180).S(1).Upd(1).Key().Cb(res.Cb()),
             Fb().Rate(kCbr).Res(640, 360).S(2).Ref({0}).Cb(res.Cb())}));
  ASSERT_THAT(res.FrameAt(3), Eq(nullptr));
}

TEST(LibaomAv1Encoder, ResolutionSwitching) {
  auto frame_reader = CreateFrameReader();
  auto enc = LibaomAv1EncoderFactory().CreateEncoder(kCbrEncoderSettings, {});
  EncodeResults res;

  rtc::scoped_refptr<I420Buffer> in0 = frame_reader->PullFrame();
  enc->Encode(in0, {.presentation_timestamp = Timestamp::Millis(0)},
              ToVec({Fb().Rate(kCbr).Res(320, 180).Upd(0).Key().Cb(res.Cb())}));

  rtc::scoped_refptr<I420Buffer> in1 = frame_reader->PullFrame();
  enc->Encode(in1, {.presentation_timestamp = Timestamp::Millis(100)},
              ToVec({Fb().Rate(kCbr).Res(640, 360).Ref({0}).Cb(res.Cb())}));

  rtc::scoped_refptr<I420Buffer> in2 = frame_reader->PullFrame();
  enc->Encode(in2, {.presentation_timestamp = Timestamp::Millis(200)},
              ToVec({Fb().Rate(kCbr).Res(160, 90).Ref({0}).Cb(res.Cb())}));

  Av1Decoder dec;
  VideoFrame f0 = dec.Decode(*res.FrameAt(0));
  EXPECT_THAT(Resolution(f0), ResolutionIs(320, 180));
  // TD:
  // EXPECT_THAT(Psnr(in0, f0), Gt(40));

  VideoFrame f1 = dec.Decode(*res.FrameAt(1));
  EXPECT_THAT(Resolution(f1), ResolutionIs(640, 360));
  EXPECT_THAT(Psnr(in1, f1), Gt(40));

  VideoFrame f2 = dec.Decode(*res.FrameAt(2));
  EXPECT_THAT(Resolution(f2), ResolutionIs(160, 90));
  // TD:
  // EXPECT_THAT(Psnr(in2, f2), Gt(40));
}

TEST(LibaomAv1Encoder, InputResolutionSwitching) {
  auto frame_reader = CreateFrameReader();
  auto enc = LibaomAv1EncoderFactory().CreateEncoder(kCbrEncoderSettings, {});
  EncodeResults res;

  rtc::scoped_refptr<I420Buffer> in0 = frame_reader->PullFrame();
  enc->Encode(in0, {.presentation_timestamp = Timestamp::Millis(0)},
              ToVec({Fb().Rate(kCbr).Res(160, 90).Upd(0).Key().Cb(res.Cb())}));

  rtc::scoped_refptr<I420Buffer> in1 = frame_reader->PullFrame(
      /*frame_num=*/nullptr,
      /*resolution=*/{320, 180},
      /*framerate_scale=*/{1, 1});
  enc->Encode(in1, {.presentation_timestamp = Timestamp::Millis(100)},
              ToVec({Fb().Rate(kCbr).Res(160, 90).Ref({0}).Cb(res.Cb())}));

  rtc::scoped_refptr<I420Buffer> in2 = frame_reader->PullFrame(
      /*frame_num=*/nullptr,
      /*resolution=*/{160, 90},
      /*framerate_scale=*/{1, 1});
  enc->Encode(in2, {.presentation_timestamp = Timestamp::Millis(200)},
              ToVec({Fb().Rate(kCbr).Res(160, 90).Ref({0}).Cb(res.Cb())}));

  Av1Decoder dec;
  VideoFrame f0 = dec.Decode(*res.FrameAt(0));
  EXPECT_THAT(Resolution(f0), ResolutionIs(160, 90));
  // TD:
  // EXPECT_THAT(Psnr(in0, f0), Gt(40));

  VideoFrame f1 = dec.Decode(*res.FrameAt(1));
  EXPECT_THAT(Resolution(f1), ResolutionIs(160, 90));
  // TD:
  // EXPECT_THAT(Psnr(in1, f1), Gt(40));

  VideoFrame f2 = dec.Decode(*res.FrameAt(2));
  EXPECT_THAT(Resolution(f2), ResolutionIs(160, 90));
  EXPECT_THAT(Psnr(in2, f2), Gt(40));
}

TEST(LibaomAv1Encoder, TempoSpatial) {
  auto frame_reader = CreateFrameReader();
  auto enc = LibaomAv1EncoderFactory().CreateEncoder(kCbrEncoderSettings, {});
  EncodeResults res;

  const Cbr k10Fps{.duration = TimeDelta::Millis(100),
                   .target_bitrate = DataRate::KilobitsPerSec(500)};
  const Cbr k20Fps{.duration = TimeDelta::Millis(50),
                   .target_bitrate = DataRate::KilobitsPerSec(500)};

  enc->Encode(
      frame_reader->PullFrame(),
      {.presentation_timestamp = Timestamp::Millis(0)},
      ToVec(
          {Fb().Rate(k10Fps).Res(160, 90).S(0).Upd(0).Key().Cb(res.Cb()),
           Fb().Rate(k10Fps).Res(320, 180).S(1).Ref({0}).Upd(1).Cb(res.Cb()),
           Fb().Rate(k20Fps).Res(640, 360).S(2).Ref({1}).Upd(2).Cb(res.Cb())}));

  enc->Encode(frame_reader->PullFrame(),
              {.presentation_timestamp = Timestamp::Millis(50)},
              ToVec({Fb().Rate(k20Fps).Res(640, 360).S(2).Ref({2}).Upd(2).Cb(
                  res.Cb())}));

  rtc::scoped_refptr<I420Buffer> frame = frame_reader->PullFrame();
  enc->Encode(
      frame, {.presentation_timestamp = Timestamp::Millis(100)},
      ToVec(
          {Fb().Rate(k10Fps).Res(160, 90).S(0).Ref({0}).Upd(0).Cb(res.Cb()),
           Fb().Rate(k10Fps).Res(320, 180).S(1).Ref({0, 1}).Upd(1).Cb(res.Cb()),
           Fb().Rate(k20Fps).Res(640, 360).S(2).Ref({1, 2}).Upd(2).Cb(
               res.Cb())}));

  Av1Decoder dec;
  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(0))), ResolutionIs(160, 90));
  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(1))), ResolutionIs(320, 180));
  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(2))), ResolutionIs(640, 360));
  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(3))), ResolutionIs(640, 360));
  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(4))), ResolutionIs(160, 90));
  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(5))), ResolutionIs(320, 180));

  VideoFrame f = dec.Decode(*res.FrameAt(6));
  EXPECT_THAT(Resolution(f), ResolutionIs(640, 360));

  EXPECT_THAT(Psnr(frame, f), Gt(40));
}

TEST(DISABLED_LibaomAv1Encoder, InvertedTempoSpatial) {
  auto frame_reader = CreateFrameReader();
  auto enc = LibaomAv1EncoderFactory().CreateEncoder(kCbrEncoderSettings, {});
  EncodeResults res;

  enc->Encode(
      frame_reader->PullFrame(),
      {.presentation_timestamp = Timestamp::Millis(0)},
      ToVec({Fb().Rate(kCbr).Res(320, 180).S(0).Upd(0).Key().Cb(res.Cb()),
             Fb().Rate(kCbr).Res(640, 360).S(1).Ref({0}).Upd(1).Cb(res.Cb())}));

  enc->Encode(
      frame_reader->PullFrame(),
      {.presentation_timestamp = Timestamp::Millis(100)},
      ToVec({Fb().Rate(kCbr).Res(320, 180).S(0).Ref({0}).Upd(0).Cb(res.Cb())}));

  rtc::scoped_refptr<I420Buffer> frame = frame_reader->PullFrame();
  enc->Encode(
      frame, {.presentation_timestamp = Timestamp::Millis(200)},
      ToVec({Fb().Rate(kCbr).Res(320, 180).S(0).Ref({0}).Upd(0).Cb(res.Cb()),
             Fb().Rate(kCbr).Res(640, 360).S(1).Ref({1, 0}).Upd(1).Cb(
                 res.Cb())}));

  Av1Decoder dec;
  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(0))), ResolutionIs(320, 180));
  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(1))), ResolutionIs(640, 360));
  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(2))), ResolutionIs(320, 180));
  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(3))), ResolutionIs(320, 180));
  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(4))), ResolutionIs(640, 360));
}

TEST(LibaomAv1Encoder, SkipMidLayer) {
  auto frame_reader = CreateFrameReader();
  auto enc = LibaomAv1EncoderFactory().CreateEncoder(kCbrEncoderSettings, {});
  EncodeResults res;

  enc->Encode(
      frame_reader->PullFrame(),
      {.presentation_timestamp = Timestamp::Millis(0)},
      ToVec({Fb().Rate(kCbr).Res(160, 90).S(0).Upd(0).Key().Cb(res.Cb()),
             Fb().Rate(kCbr).Res(320, 180).S(1).Ref({0}).Upd(1).Cb(res.Cb()),
             Fb().Rate(kCbr).Res(640, 360).S(2).Ref({1}).Upd(2).Cb(res.Cb())}));

  enc->Encode(
      frame_reader->PullFrame(),
      {.presentation_timestamp = Timestamp::Millis(100)},
      ToVec({Fb().Rate(kCbr).Res(160, 90).S(0).Ref({0}).Upd(0).Cb(res.Cb()),
             Fb().Rate(kCbr).Res(640, 360).S(2).Ref({2}).Upd(2).Cb(res.Cb())}));

  rtc::scoped_refptr<I420Buffer> frame = frame_reader->PullFrame();
  enc->Encode(
      frame, {.presentation_timestamp = Timestamp::Millis(200)},
      ToVec({Fb().Rate(kCbr).Res(160, 90).S(0).Ref({0}).Upd(0).Cb(res.Cb()),
             Fb().Rate(kCbr).Res(320, 180).S(1).Ref({0, 1}).Upd(1).Cb(res.Cb()),
             Fb().Rate(kCbr).Res(640, 360).S(2).Ref({1, 2}).Upd(2).Cb(
                 res.Cb())}));

  Av1Decoder dec;
  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(0))), ResolutionIs(160, 90));
  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(1))), ResolutionIs(320, 180));
  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(2))), ResolutionIs(640, 360));
  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(3))), ResolutionIs(160, 90));
  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(4))), ResolutionIs(640, 360));
  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(5))), ResolutionIs(160, 90));
  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(6))), ResolutionIs(320, 180));

  VideoFrame f = dec.Decode(*res.FrameAt(7));
  EXPECT_THAT(Resolution(f), ResolutionIs(640, 360));
  EXPECT_THAT(Psnr(frame, f), Gt(40));
}

TEST(LibaomAv1Encoder, L3T1) {
  auto frame_reader = CreateFrameReader();
  auto enc = LibaomAv1EncoderFactory().CreateEncoder(kCbrEncoderSettings, {});
  EncodeResults res;
  Av1Decoder dec;

  enc->Encode(
      frame_reader->PullFrame(),
      {.presentation_timestamp = Timestamp::Millis(0)},
      ToVec({Fb().Rate(kCbr).Res(160, 90).S(0).Upd(0).Key().Cb(res.Cb()),
             Fb().Rate(kCbr).Res(320, 180).S(1).Ref({0}).Upd(1).Cb(res.Cb()),
             Fb().Rate(kCbr).Res(640, 360).S(2).Ref({1}).Upd(2).Cb(res.Cb())}));

  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(0))), ResolutionIs(160, 90));
  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(1))), ResolutionIs(320, 180));
  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(2))), ResolutionIs(640, 360));

  auto tu1_frame = frame_reader->PullFrame();
  enc->Encode(
      tu1_frame, {.presentation_timestamp = Timestamp::Millis(100)},
      ToVec({Fb().Rate(kCbr).Res(160, 90).S(0).Ref({0}).Upd(0).Cb(res.Cb()),
             Fb().Rate(kCbr).Res(320, 180).S(1).Ref({1, 0}).Upd(1).Cb(res.Cb()),
             Fb().Rate(kCbr).Res(640, 360).S(2).Ref({2, 1}).Upd(2).Cb(
                 res.Cb())}));

  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(3))), ResolutionIs(160, 90));
  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(4))), ResolutionIs(320, 180));

  VideoFrame f_tu1 = dec.Decode(*res.FrameAt(5));
  EXPECT_THAT(Resolution(f_tu1), ResolutionIs(640, 360));
  EXPECT_THAT(Psnr(tu1_frame, f_tu1), Gt(40));

  auto tu2_frame = frame_reader->PullFrame();
  enc->Encode(
      tu2_frame, {.presentation_timestamp = Timestamp::Millis(200)},
      ToVec({Fb().Rate(kCbr).Res(160, 90).S(0).Ref({0}).Upd(0).Cb(res.Cb()),
             Fb().Rate(kCbr).Res(320, 180).S(1).Ref({1, 0}).Upd(1).Cb(res.Cb()),
             Fb().Rate(kCbr).Res(640, 360).S(2).Ref({2, 1}).Upd(2).Cb(
                 res.Cb())}));

  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(6))), ResolutionIs(160, 90));
  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(7))), ResolutionIs(320, 180));

  VideoFrame f_tu2 = dec.Decode(*res.FrameAt(8));
  EXPECT_THAT(Resolution(f_tu2), ResolutionIs(640, 360));
  EXPECT_THAT(Psnr(tu2_frame, f_tu2), Gt(40));
}

TEST(LibaomAv1Encoder, L3T1_KEY) {
  auto frame_reader = CreateFrameReader();
  auto enc = LibaomAv1EncoderFactory().CreateEncoder(kCbrEncoderSettings, {});
  EncodeResults res;

  Av1Decoder dec_s0;
  Av1Decoder dec_s1;
  Av1Decoder dec_s2;

  enc->Encode(
      frame_reader->PullFrame(),
      {.presentation_timestamp = Timestamp::Millis(0)},
      ToVec({Fb().Rate(kCbr).Res(160, 90).S(0).Upd(0).Key().Cb(res.Cb()),
             Fb().Rate(kCbr).Res(320, 180).S(1).Ref({0}).Upd(1).Cb(res.Cb()),
             Fb().Rate(kCbr).Res(640, 360).S(2).Ref({1}).Upd(2).Cb(res.Cb())}));

  EXPECT_THAT(Resolution(dec_s0.Decode(*res.FrameAt(0))),
              ResolutionIs(160, 90));

  dec_s1.Decode(*res.FrameAt(0));
  EXPECT_THAT(Resolution(dec_s1.Decode(*res.FrameAt(1))),
              ResolutionIs(320, 180));

  dec_s2.Decode(*res.FrameAt(0));
  dec_s2.Decode(*res.FrameAt(1));
  EXPECT_THAT(Resolution(dec_s2.Decode(*res.FrameAt(2))),
              ResolutionIs(640, 360));

  enc->Encode(
      frame_reader->PullFrame(),
      {.presentation_timestamp = Timestamp::Millis(100)},
      ToVec({Fb().Rate(kCbr).Res(160, 90).S(0).Ref({0}).Upd(0).Cb(res.Cb()),
             Fb().Rate(kCbr).Res(320, 180).S(1).Ref({1}).Upd(1).Cb(res.Cb()),
             Fb().Rate(kCbr).Res(640, 360).S(2).Ref({2}).Upd(2).Cb(res.Cb())}));

  EXPECT_THAT(Resolution(dec_s0.Decode(*res.FrameAt(3))),
              ResolutionIs(160, 90));
  EXPECT_THAT(Resolution(dec_s1.Decode(*res.FrameAt(4))),
              ResolutionIs(320, 180));
  EXPECT_THAT(Resolution(dec_s2.Decode(*res.FrameAt(5))),
              ResolutionIs(640, 360));

  enc->Encode(
      frame_reader->PullFrame(),
      {.presentation_timestamp = Timestamp::Millis(200)},
      ToVec({Fb().Rate(kCbr).Res(160, 90).S(0).Ref({0}).Upd(0).Cb(res.Cb()),
             Fb().Rate(kCbr).Res(320, 180).S(1).Ref({1}).Upd(1).Cb(res.Cb()),
             Fb().Rate(kCbr).Res(640, 360).S(2).Ref({2}).Upd(2).Cb(res.Cb())}));

  EXPECT_THAT(Resolution(dec_s0.Decode(*res.FrameAt(6))),
              ResolutionIs(160, 90));
  EXPECT_THAT(Resolution(dec_s1.Decode(*res.FrameAt(7))),
              ResolutionIs(320, 180));
  EXPECT_THAT(Resolution(dec_s2.Decode(*res.FrameAt(8))),
              ResolutionIs(640, 360));
}

TEST(LibaomAv1Encoder, S3T1) {
  auto frame_reader = CreateFrameReader();
  auto enc = LibaomAv1EncoderFactory().CreateEncoder(kCbrEncoderSettings, {});
  EncodeResults res;

  Av1Decoder dec_s0;
  Av1Decoder dec_s1;
  Av1Decoder dec_s2;

  enc->Encode(
      frame_reader->PullFrame(),
      {.presentation_timestamp = Timestamp::Millis(0)},
      ToVec({Fb().Rate(kCbr).Res(160, 90).S(0).Start().Upd(0).Cb(res.Cb()),
             Fb().Rate(kCbr).Res(320, 180).S(1).Start().Upd(1).Cb(res.Cb()),
             Fb().Rate(kCbr).Res(640, 360).S(2).Start().Upd(2).Cb(res.Cb())}));
  EXPECT_THAT(Resolution(dec_s0.Decode(*res.FrameAt(0))),
              ResolutionIs(160, 90));
  EXPECT_THAT(Resolution(dec_s1.Decode(*res.FrameAt(1))),
              ResolutionIs(320, 180));
  EXPECT_THAT(Resolution(dec_s2.Decode(*res.FrameAt(2))),
              ResolutionIs(640, 360));

  enc->Encode(
      frame_reader->PullFrame(),
      {.presentation_timestamp = Timestamp::Millis(100)},
      ToVec({Fb().Rate(kCbr).Res(160, 90).S(0).Ref({0}).Upd(0).Cb(res.Cb()),
             Fb().Rate(kCbr).Res(320, 180).S(1).Ref({1}).Upd(1).Cb(res.Cb()),
             Fb().Rate(kCbr).Res(640, 360).S(2).Ref({2}).Upd(2).Cb(res.Cb())}));

  EXPECT_THAT(Resolution(dec_s0.Decode(*res.FrameAt(3))),
              ResolutionIs(160, 90));
  EXPECT_THAT(Resolution(dec_s1.Decode(*res.FrameAt(4))),
              ResolutionIs(320, 180));
  EXPECT_THAT(Resolution(dec_s2.Decode(*res.FrameAt(5))),
              ResolutionIs(640, 360));

  enc->Encode(
      frame_reader->PullFrame(),
      {.presentation_timestamp = Timestamp::Millis(200)},
      ToVec({Fb().Rate(kCbr).Res(160, 90).S(0).Ref({0}).Upd(0).Cb(res.Cb()),
             Fb().Rate(kCbr).Res(320, 180).S(1).Ref({1}).Upd(1).Cb(res.Cb()),
             Fb().Rate(kCbr).Res(640, 360).S(2).Ref({2}).Upd(2).Cb(res.Cb())}));

  EXPECT_THAT(Resolution(dec_s0.Decode(*res.FrameAt(6))),
              ResolutionIs(160, 90));
  EXPECT_THAT(Resolution(dec_s1.Decode(*res.FrameAt(7))),
              ResolutionIs(320, 180));
  EXPECT_THAT(Resolution(dec_s2.Decode(*res.FrameAt(8))),
              ResolutionIs(640, 360));
}

TEST(LibaomAv1Encoder, HigherEffortLevelYieldsHigherQualityFrames) {
  auto frame_in = CreateFrameReader()->PullFrame();
  std::pair<int, int> effort_range = LibaomAv1EncoderFactory()
                                         .GetEncoderCapabilities()
                                         .performance.min_max_effort_level;
  // Cbr rc{.duration = TimeDelta::Millis(100),
  //       .target_bitrate = DataRate::KilobitsPerSec(100)};
  absl::optional<double> psnr_last;
  Av1Decoder dec;

  for (int i = effort_range.first; i <= effort_range.second; ++i) {
    auto enc = LibaomAv1EncoderFactory().CreateEncoder(kCbrEncoderSettings, {});
    EncodeResults res;
    enc->Encode(
        frame_in,
        {.presentation_timestamp = Timestamp::Millis(0), .effort_level = i},
        ToVec({Fb().Rate(kCbr).Res(640, 360).Upd(0).Key().Cb(res.Cb())}));
    double psnr = Psnr(frame_in, dec.Decode(*res.FrameAt(0)));
    EXPECT_THAT(psnr, Gt(psnr_last));
    psnr_last = psnr;
  }
}

TEST(LibaomAv1Encoder, KeyframeAndStartrameAreApproximatelyEqual) {
  int max_spatial_layers = LibaomAv1EncoderFactory()
                               .GetEncoderCapabilities()
                               .prediction_constraints.max_spatial_layers;
  const Cbr kRate{.duration = TimeDelta::Millis(100),
                  .target_bitrate = DataRate::KilobitsPerSec(500)};

  for (int sid = 0; sid < max_spatial_layers; ++sid) {
    std::string key_name = "cbr_key_sl_";
    key_name += std::to_string(sid);
    Av1Decoder dec_key(key_name);

    std::string start_name = "cbr_start_sl_";
    start_name += std::to_string(sid);
    Av1Decoder dec_start(start_name);

    auto frame_reader = CreateFrameReader();
    auto enc_key =
        LibaomAv1EncoderFactory().CreateEncoder(kCbrEncoderSettings, {});
    auto enc_start =
        LibaomAv1EncoderFactory().CreateEncoder(kCbrEncoderSettings, {});
    DataSize total_size_key = DataSize::Zero();
    DataSize total_size_start = DataSize::Zero();
    TimeDelta total_duration = TimeDelta::Zero();
    EncodeResults res_key;
    EncodeResults res_start;
    auto frame_in = frame_reader->PullFrame();
    enc_key->Encode(
        frame_in, {.presentation_timestamp = Timestamp::Millis(0)},
        ToVec({Fb().Rate(kRate).Res(640, 360).S(sid).Upd(0).Key().Cb(
            res_key.Cb())}));
    enc_start->Encode(
        frame_in, {.presentation_timestamp = Timestamp::Millis(0)},
        ToVec({Fb().Rate(kRate).Res(640, 360).S(sid).Start().Upd(0).Cb(
            res_start.Cb())}));
    total_size_key +=
        DataSize::Bytes(res_key.FrameAt(0)->bitstream_data->size());
    total_size_start +=
        DataSize::Bytes(res_start.FrameAt(0)->bitstream_data->size());
    total_duration += kRate.duration;
    dec_key.Decode(*res_key.FrameAt(0));
    dec_start.Decode(*res_start.FrameAt(0));

    EXPECT_NEAR(total_size_key.bytes(), total_size_start.bytes(),
                0.1 * total_size_key.bytes());

    for (int f = 1; f < 10; ++f) {
      frame_in = frame_reader->PullFrame();
      enc_key->Encode(
          frame_in, {.presentation_timestamp = Timestamp::Millis(f * 100)},
          ToVec({Fb().Rate(kRate).Res(640, 360).S(sid).Ref({0}).Upd(0).Cb(
              res_key.Cb())}));
      enc_start->Encode(
          frame_in, {.presentation_timestamp = Timestamp::Millis(f * 100)},
          ToVec({Fb().Rate(kRate).Res(640, 360).S(sid).Ref({0}).Upd(0).Cb(
              res_start.Cb())}));
      total_size_key +=
          DataSize::Bytes(res_key.FrameAt(f)->bitstream_data->size());
      total_size_start +=
          DataSize::Bytes(res_start.FrameAt(f)->bitstream_data->size());
      total_duration += kRate.duration;
      dec_key.Decode(*res_key.FrameAt(f));
      dec_start.Decode(*res_start.FrameAt(f));
    }

    double key_encode_kbps = (total_size_key / total_duration).kbps();
    double start_encode_kbps = (total_size_start / total_duration).kbps();

    EXPECT_NEAR(key_encode_kbps, start_encode_kbps, start_encode_kbps * 0.05);
  }
}

TEST(LibaomAv1Encoder, BitrateConsistentAcrossSpatialLayers) {
  int max_spatial_layers = LibaomAv1EncoderFactory()
                               .GetEncoderCapabilities()
                               .prediction_constraints.max_spatial_layers;
  const Cbr kRate{.duration = TimeDelta::Millis(100),
                  .target_bitrate = DataRate::KilobitsPerSec(500)};

  for (int sid = 0; sid < max_spatial_layers; ++sid) {
    std::string out_name = "cbr_sl_";
    out_name += std::to_string(sid);
    Av1Decoder dec(out_name);

    auto frame_reader = CreateFrameReader();
    auto enc = LibaomAv1EncoderFactory().CreateEncoder(kCbrEncoderSettings, {});
    DataSize total_size = DataSize::Zero();
    TimeDelta total_duration = TimeDelta::Zero();
    EncodeResults res;
    enc->Encode(frame_reader->PullFrame(),
                {.presentation_timestamp = Timestamp::Millis(0)},
                ToVec({Fb().Rate(kRate).Res(640, 360).S(sid).Upd(0).Key().Cb(
                    res.Cb())}));
    total_size += DataSize::Bytes(res.FrameAt(0)->bitstream_data->size());
    total_duration += kRate.duration;
    dec.Decode(*res.FrameAt(0));

    for (int f = 1; f < 30; ++f) {
      enc->Encode(
          frame_reader->PullFrame(),
          {.presentation_timestamp = Timestamp::Millis(f * 100)},
          ToVec({Fb().Rate(kRate).Res(640, 360).S(sid).Ref({0}).Upd(0).Cb(
              res.Cb())}));
      total_size += DataSize::Bytes(res.FrameAt(f)->bitstream_data->size());
      total_duration += kRate.duration;
      dec.Decode(*res.FrameAt(f));
    }

    double encode_kbps = (total_size / total_duration).kbps();
    double target_kbps = kRate.target_bitrate.kbps();

    EXPECT_NEAR(encode_kbps, target_kbps, target_kbps * 0.1);
  }
}

TEST(LibaomAv1Encoder, ConstantQp) {
  int max_spatial_layers = LibaomAv1EncoderFactory()
                               .GetEncoderCapabilities()
                               .prediction_constraints.max_spatial_layers;
  constexpr int kQp = 30;
  for (int sid = 0; sid < max_spatial_layers; ++sid) {
    auto enc = LibaomAv1EncoderFactory().CreateEncoder(kCqpEncoderSettings, {});
    std::string out_name = "cqp_sl_";
    out_name += std::to_string(sid);
    Av1Decoder dec(out_name);
    DataSize total_size = DataSize::Zero();
    auto frame_reader = CreateFrameReader();
    EncodeResults res;

    enc->Encode(frame_reader->PullFrame(),
                {.presentation_timestamp = Timestamp::Millis(0)},
                ToVec({Fb().Rate(Cqp{.target_qp = kQp})
                           .Res(640, 360)
                           .S(sid)
                           .Upd(0)
                           .Key()
                           .Cb(res.Cb())}));
    EXPECT_THAT(res.FrameAt(0)->encoded_qp, Eq(kQp));
    total_size += DataSize::Bytes(res.FrameAt(0)->bitstream_data->size());
    dec.Decode(*res.FrameAt(0));

    for (int f = 1; f < 10; ++f) {
      enc->Encode(frame_reader->PullFrame(),
                  {.presentation_timestamp = Timestamp::Millis(f * 100)},
                  ToVec({Fb().Rate(Cqp{.target_qp = kQp - f})
                             .Res(640, 360)
                             .S(sid)
                             .Ref({0})
                             .Upd(0)
                             .Cb(res.Cb())}));
      EXPECT_THAT(res.FrameAt(f)->encoded_qp, Eq(kQp - f));
      dec.Decode(*res.FrameAt(f));
    }
  }
}

}  // namespace
}  // namespace webrtc
