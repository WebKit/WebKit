/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/video_codec.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/functional/any_invocable.h"
#include "api/test/create_video_codec_tester.h"
#include "api/test/metrics/global_metrics_logger_and_exporter.h"
#include "api/test/video_codec_tester.h"
#include "api/test/videocodec_test_stats.h"
#include "api/units/data_rate.h"
#include "api/units/frequency.h"
#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video/resolution.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_encoder.h"
#include "media/engine/internal_decoder_factory.h"
#include "media/engine/internal_encoder_factory.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "modules/video_coding/svc/scalability_mode_util.h"
#if defined(WEBRTC_ANDROID)
#include "modules/video_coding/codecs/test/android_codec_factory_helper.h"
#endif
#include "rtc_base/logging.h"
#include "test/gtest.h"
#include "test/test_flags.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/frame_reader.h"

namespace webrtc {
namespace test {

namespace {
using ::testing::Combine;
using ::testing::Values;
using PacingMode = VideoCodecTester::PacingSettings::PacingMode;

struct VideoInfo {
  std::string name;
  Resolution resolution;
  Frequency framerate;
};

struct LayerId {
  int spatial_idx;
  int temporal_idx;

  bool operator==(const LayerId& o) const {
    return spatial_idx == o.spatial_idx && temporal_idx == o.temporal_idx;
  }

  bool operator<(const LayerId& o) const {
    if (spatial_idx < o.spatial_idx)
      return true;
    if (spatial_idx == o.spatial_idx && temporal_idx < o.temporal_idx)
      return true;
    return false;
  }
};

struct EncodingSettings {
  ScalabilityMode scalability_mode;
  struct LayerSettings {
    Resolution resolution;
    Frequency framerate;
    DataRate bitrate;
  };
  std::map<LayerId, LayerSettings> layer_settings;

  bool IsSameSettings(const EncodingSettings& other) const {
    if (scalability_mode != other.scalability_mode) {
      return false;
    }

    for (auto [layer_id, layer] : layer_settings) {
      const auto& other_layer = other.layer_settings.at(layer_id);
      if (layer.resolution != other_layer.resolution) {
        return false;
      }
    }

    return true;
  }

  bool IsSameRate(const EncodingSettings& other) const {
    for (auto [layer_id, layer] : layer_settings) {
      const auto& other_layer = other.layer_settings.at(layer_id);
      if (layer.bitrate != other_layer.bitrate ||
          layer.framerate != other_layer.framerate) {
        return false;
      }
    }

    return true;
  }
};

const VideoInfo kFourPeople_1280x720_30 = {
    .name = "FourPeople_1280x720_30",
    .resolution = {.width = 1280, .height = 720},
    .framerate = Frequency::Hertz(30)};

class TestRawVideoSource : public VideoCodecTester::RawVideoSource {
 public:
  static constexpr Frequency k90kHz = Frequency::Hertz(90000);

  TestRawVideoSource(VideoInfo video_info,
                     const std::map<int, EncodingSettings>& frame_settings,
                     int num_frames)
      : video_info_(video_info),
        frame_settings_(frame_settings),
        num_frames_(num_frames),
        frame_num_(0),
        // Start with non-zero timestamp to force using frame RTP timestamps in
        // IvfFrameWriter.
        timestamp_rtp_(90000) {
    // Ensure settings for the first frame are provided.
    RTC_CHECK_GT(frame_settings_.size(), 0u);
    RTC_CHECK_EQ(frame_settings_.begin()->first, 0);

    frame_reader_ = CreateYuvFrameReader(
        ResourcePath(video_info_.name, "yuv"), video_info_.resolution,
        YuvFrameReaderImpl::RepeatMode::kPingPong);
    RTC_CHECK(frame_reader_);
  }

  // Pulls next frame. Frame RTP timestamp is set accordingly to
  // `EncodingSettings::framerate`.
  absl::optional<VideoFrame> PullFrame() override {
    if (frame_num_ >= num_frames_) {
      return absl::nullopt;  // End of stream.
    }

    const EncodingSettings& encoding_settings =
        std::prev(frame_settings_.upper_bound(frame_num_))->second;

    Resolution resolution =
        encoding_settings.layer_settings.begin()->second.resolution;
    Frequency framerate =
        encoding_settings.layer_settings.begin()->second.framerate;

    int pulled_frame;
    auto buffer = frame_reader_->PullFrame(
        &pulled_frame, resolution,
        {.num = static_cast<int>(framerate.millihertz()),
         .den = static_cast<int>(video_info_.framerate.millihertz())});
    RTC_CHECK(buffer) << "Cannot pull frame " << frame_num_;

    auto frame = VideoFrame::Builder()
                     .set_video_frame_buffer(buffer)
                     .set_timestamp_rtp(timestamp_rtp_)
                     .set_timestamp_us((timestamp_rtp_ / k90kHz).us())
                     .build();

    pulled_frames_[timestamp_rtp_] = pulled_frame;
    timestamp_rtp_ += k90kHz / framerate;
    ++frame_num_;

    return frame;
  }

  // Reads frame specified by `timestamp_rtp`, scales it to `resolution` and
  // returns. Frame with the given `timestamp_rtp` is expected to be pulled
  // before.
  VideoFrame GetFrame(uint32_t timestamp_rtp, Resolution resolution) override {
    RTC_CHECK(pulled_frames_.find(timestamp_rtp) != pulled_frames_.end())
        << "Frame with RTP timestamp " << timestamp_rtp
        << " was not pulled before";
    auto buffer =
        frame_reader_->ReadFrame(pulled_frames_[timestamp_rtp], resolution);
    return VideoFrame::Builder()
        .set_video_frame_buffer(buffer)
        .set_timestamp_rtp(timestamp_rtp)
        .build();
  }

 protected:
  VideoInfo video_info_;
  std::unique_ptr<FrameReader> frame_reader_;
  const std::map<int, EncodingSettings>& frame_settings_;
  int num_frames_;
  int frame_num_;
  uint32_t timestamp_rtp_;
  std::map<uint32_t, int> pulled_frames_;
};

class TestEncoder : public VideoCodecTester::Encoder,
                    public EncodedImageCallback {
 public:
  TestEncoder(std::unique_ptr<VideoEncoder> encoder,
              const std::string codec_type,
              const std::map<int, EncodingSettings>& frame_settings)
      : encoder_(std::move(encoder)),
        codec_type_(codec_type),
        frame_settings_(frame_settings),
        frame_num_(0) {
    // Ensure settings for the first frame is provided.
    RTC_CHECK_GT(frame_settings_.size(), 0u);
    RTC_CHECK_EQ(frame_settings_.begin()->first, 0);

    encoder_->RegisterEncodeCompleteCallback(this);
  }

  void Initialize() override {
    const EncodingSettings& first_frame_settings = frame_settings_.at(0);
    Configure(first_frame_settings);
    SetRates(first_frame_settings);
  }

  void Encode(const VideoFrame& frame, EncodeCallback callback) override {
    {
      MutexLock lock(&mutex_);
      callbacks_[frame.timestamp()] = std::move(callback);
    }

    if (auto fs = frame_settings_.find(frame_num_);
        fs != frame_settings_.begin() && fs != frame_settings_.end()) {
      if (!fs->second.IsSameSettings(std::prev(fs)->second)) {
        Configure(fs->second);
      } else if (!fs->second.IsSameRate(std::prev(fs)->second)) {
        SetRates(fs->second);
      }
    }

    encoder_->Encode(frame, nullptr);
    ++frame_num_;
  }

  void Flush() override {
    // TODO(webrtc:14852): For codecs which buffer frames we need a to
    // flush them to get last frames. Add such functionality to VideoEncoder
    // API. On Android it will map directly to `MediaCodec.flush()`.
    encoder_->Release();
  }

  VideoEncoder* encoder() { return encoder_.get(); }

 protected:
  Result OnEncodedImage(const EncodedImage& encoded_image,
                        const CodecSpecificInfo* codec_specific_info) override {
    MutexLock lock(&mutex_);
    auto cb = callbacks_.find(encoded_image.RtpTimestamp());
    RTC_CHECK(cb != callbacks_.end());
    cb->second(encoded_image);

    callbacks_.erase(callbacks_.begin(), cb);
    return Result(Result::Error::OK);
  }

  void Configure(const EncodingSettings& es) {
    VideoCodec vc;
    const EncodingSettings::LayerSettings& layer_settings =
        es.layer_settings.begin()->second;
    vc.width = layer_settings.resolution.width;
    vc.height = layer_settings.resolution.height;
    const DataRate& bitrate = layer_settings.bitrate;
    vc.startBitrate = bitrate.kbps();
    vc.maxBitrate = bitrate.kbps();
    vc.minBitrate = 0;
    vc.maxFramerate = static_cast<uint32_t>(layer_settings.framerate.hertz());
    vc.active = true;
    vc.qpMax = 63;
    vc.numberOfSimulcastStreams = 0;
    vc.mode = webrtc::VideoCodecMode::kRealtimeVideo;
    vc.SetFrameDropEnabled(true);
    vc.SetScalabilityMode(es.scalability_mode);

    vc.codecType = PayloadStringToCodecType(codec_type_);
    if (vc.codecType == kVideoCodecVP8) {
      *(vc.VP8()) = VideoEncoder::GetDefaultVp8Settings();
    } else if (vc.codecType == kVideoCodecVP9) {
      *(vc.VP9()) = VideoEncoder::GetDefaultVp9Settings();
    } else if (vc.codecType == kVideoCodecH264) {
      *(vc.H264()) = VideoEncoder::GetDefaultH264Settings();
    }

    VideoEncoder::Settings ves(
        VideoEncoder::Capabilities(/*loss_notification=*/false),
        /*number_of_cores=*/1,
        /*max_payload_size=*/1440);

    int result = encoder_->InitEncode(&vc, ves);
    ASSERT_EQ(result, WEBRTC_VIDEO_CODEC_OK);

    SetRates(es);
  }

  void SetRates(const EncodingSettings& es) {
    VideoEncoder::RateControlParameters rc;
    int num_spatial_layers =
        ScalabilityModeToNumSpatialLayers(es.scalability_mode);
    int num_temporal_layers =
        ScalabilityModeToNumSpatialLayers(es.scalability_mode);
    for (int sidx = 0; sidx < num_spatial_layers; ++sidx) {
      for (int tidx = 0; tidx < num_temporal_layers; ++tidx) {
        auto layer_settings =
            es.layer_settings.find({.spatial_idx = sidx, .temporal_idx = tidx});
        RTC_CHECK(layer_settings != es.layer_settings.end())
            << "Bitrate for layer S=" << sidx << " T=" << tidx << " is not set";
        rc.bitrate.SetBitrate(sidx, tidx, layer_settings->second.bitrate.bps());
      }
    }

    rc.framerate_fps =
        es.layer_settings.begin()->second.framerate.millihertz() / 1000.0;
    encoder_->SetRates(rc);
  }

  std::unique_ptr<VideoEncoder> encoder_;
  const std::string codec_type_;
  const std::map<int, EncodingSettings>& frame_settings_;
  int frame_num_;
  std::map<uint32_t, EncodeCallback> callbacks_ RTC_GUARDED_BY(mutex_);
  Mutex mutex_;
};

class TestDecoder : public VideoCodecTester::Decoder,
                    public DecodedImageCallback {
 public:
  TestDecoder(std::unique_ptr<VideoDecoder> decoder,
              const std::string codec_type)
      : decoder_(std::move(decoder)), codec_type_(codec_type) {
    decoder_->RegisterDecodeCompleteCallback(this);
  }

  void Initialize() override {
    VideoDecoder::Settings ds;
    ds.set_codec_type(PayloadStringToCodecType(codec_type_));
    ds.set_number_of_cores(1);
    ds.set_max_render_resolution({1280, 720});

    bool result = decoder_->Configure(ds);
    ASSERT_TRUE(result);
  }

  void Decode(const EncodedImage& frame, DecodeCallback callback) override {
    {
      MutexLock lock(&mutex_);
      callbacks_[frame.RtpTimestamp()] = std::move(callback);
    }

    decoder_->Decode(frame, /*render_time_ms=*/0);
  }

  void Flush() override {
    // TODO(webrtc:14852): For codecs which buffer frames we need a to
    // flush them to get last frames. Add such functionality to VideoDecoder
    // API. On Android it will map directly to `MediaCodec.flush()`.
    decoder_->Release();
  }

  VideoDecoder* decoder() { return decoder_.get(); }

 protected:
  int Decoded(VideoFrame& decoded_frame) override {
    MutexLock lock(&mutex_);
    auto cb = callbacks_.find(decoded_frame.timestamp());
    RTC_CHECK(cb != callbacks_.end());
    cb->second(decoded_frame);

    callbacks_.erase(callbacks_.begin(), cb);
    return WEBRTC_VIDEO_CODEC_OK;
  }

  std::unique_ptr<VideoDecoder> decoder_;
  const std::string codec_type_;
  std::map<uint32_t, DecodeCallback> callbacks_ RTC_GUARDED_BY(mutex_);
  Mutex mutex_;
};

std::unique_ptr<TestRawVideoSource> CreateVideoSource(
    const VideoInfo& video,
    const std::map<int, EncodingSettings>& frame_settings,
    int num_frames) {
  return std::make_unique<TestRawVideoSource>(video, frame_settings,
                                              num_frames);
}

std::unique_ptr<TestEncoder> CreateEncoder(
    std::string type,
    std::string impl,
    const std::map<int, EncodingSettings>& frame_settings) {
  std::unique_ptr<VideoEncoderFactory> factory;
  if (impl == "builtin") {
    factory = std::make_unique<InternalEncoderFactory>();
  } else if (impl == "mediacodec") {
#if defined(WEBRTC_ANDROID)
    InitializeAndroidObjects();
    factory = CreateAndroidEncoderFactory();
#endif
  }
  std::unique_ptr<VideoEncoder> encoder =
      factory->CreateVideoEncoder(SdpVideoFormat(type));
  if (encoder == nullptr) {
    return nullptr;
  }
  return std::make_unique<TestEncoder>(std::move(encoder), type,
                                       frame_settings);
}

std::unique_ptr<TestDecoder> CreateDecoder(std::string type, std::string impl) {
  std::unique_ptr<VideoDecoderFactory> factory;
  if (impl == "builtin") {
    factory = std::make_unique<InternalDecoderFactory>();
  } else if (impl == "mediacodec") {
#if defined(WEBRTC_ANDROID)
    InitializeAndroidObjects();
    factory = CreateAndroidDecoderFactory();
#endif
  }
  std::unique_ptr<VideoDecoder> decoder =
      factory->CreateVideoDecoder(SdpVideoFormat(type));
  if (decoder == nullptr) {
    return nullptr;
  }
  return std::make_unique<TestDecoder>(std::move(decoder), type);
}

void SetTargetRates(const std::map<int, EncodingSettings>& frame_settings,
                    std::vector<VideoCodecStats::Frame>& frames) {
  for (VideoCodecStats::Frame& f : frames) {
    const EncodingSettings& encoding_settings =
        std::prev(frame_settings.upper_bound(f.frame_num))->second;
    LayerId layer_id = {.spatial_idx = f.spatial_idx,
                        .temporal_idx = f.temporal_idx};
    RTC_CHECK(encoding_settings.layer_settings.find(layer_id) !=
              encoding_settings.layer_settings.end())
        << "Frame frame_num=" << f.frame_num
        << " belongs to spatial_idx=" << f.spatial_idx
        << " temporal_idx=" << f.temporal_idx
        << " but settings for this layer are not provided.";
    const EncodingSettings::LayerSettings& layer_settings =
        encoding_settings.layer_settings.at(layer_id);
    f.target_bitrate = layer_settings.bitrate;
    f.target_framerate = layer_settings.framerate;
  }
}

std::string TestOutputPath() {
  std::string output_path =
      OutputPath() +
      ::testing::UnitTest::GetInstance()->current_test_info()->name();
  std::string output_dir = DirName(output_path);
  bool result = CreateDir(output_dir);
  RTC_CHECK(result) << "Cannot create " << output_dir;
  return output_path;
}
}  // namespace

std::unique_ptr<VideoCodecStats> RunEncodeDecodeTest(
    std::string codec_type,
    std::string codec_impl,
    const VideoInfo& video_info,
    const std::map<int, EncodingSettings>& frame_settings,
    int num_frames,
    bool save_codec_input,
    bool save_codec_output) {
  std::unique_ptr<TestRawVideoSource> video_source =
      CreateVideoSource(video_info, frame_settings, num_frames);

  std::unique_ptr<TestEncoder> encoder =
      CreateEncoder(codec_type, codec_impl, frame_settings);
  if (encoder == nullptr) {
    return nullptr;
  }

  std::unique_ptr<TestDecoder> decoder = CreateDecoder(codec_type, codec_impl);
  if (decoder == nullptr) {
    // If platform decoder is not available try built-in one.
    if (codec_impl == "builtin") {
      return nullptr;
    }

    decoder = CreateDecoder(codec_type, "builtin");
    if (decoder == nullptr) {
      return nullptr;
    }
  }

  RTC_LOG(LS_INFO) << "Encoder implementation: "
                   << encoder->encoder()->GetEncoderInfo().implementation_name;
  RTC_LOG(LS_INFO) << "Decoder implementation: "
                   << decoder->decoder()->GetDecoderInfo().implementation_name;

  VideoCodecTester::EncoderSettings encoder_settings;
  encoder_settings.pacing.mode =
      encoder->encoder()->GetEncoderInfo().is_hardware_accelerated
          ? PacingMode::kRealTime
          : PacingMode::kNoPacing;

  VideoCodecTester::DecoderSettings decoder_settings;
  decoder_settings.pacing.mode =
      decoder->decoder()->GetDecoderInfo().is_hardware_accelerated
          ? PacingMode::kRealTime
          : PacingMode::kNoPacing;

  std::string output_path = TestOutputPath();
  if (save_codec_input) {
    encoder_settings.encoder_input_base_path = output_path + "_enc_input";
    decoder_settings.decoder_input_base_path = output_path + "_dec_input";
  }
  if (save_codec_output) {
    encoder_settings.encoder_output_base_path = output_path + "_enc_output";
    decoder_settings.decoder_output_base_path = output_path + "_dec_output";
  }

  std::unique_ptr<VideoCodecTester> tester = CreateVideoCodecTester();
  return tester->RunEncodeDecodeTest(video_source.get(), encoder.get(),
                                     decoder.get(), encoder_settings,
                                     decoder_settings);
}

std::unique_ptr<VideoCodecStats> RunEncodeTest(
    std::string codec_type,
    std::string codec_impl,
    const VideoInfo& video_info,
    const std::map<int, EncodingSettings>& frame_settings,
    int num_frames,
    bool save_codec_input,
    bool save_codec_output) {
  std::unique_ptr<TestRawVideoSource> video_source =
      CreateVideoSource(video_info, frame_settings, num_frames);

  std::unique_ptr<TestEncoder> encoder =
      CreateEncoder(codec_type, codec_impl, frame_settings);
  if (encoder == nullptr) {
    return nullptr;
  }

  RTC_LOG(LS_INFO) << "Encoder implementation: "
                   << encoder->encoder()->GetEncoderInfo().implementation_name;

  VideoCodecTester::EncoderSettings encoder_settings;
  encoder_settings.pacing.mode =
      encoder->encoder()->GetEncoderInfo().is_hardware_accelerated
          ? PacingMode::kRealTime
          : PacingMode::kNoPacing;

  std::string output_path = TestOutputPath();
  if (save_codec_input) {
    encoder_settings.encoder_input_base_path = output_path + "_enc_input";
  }
  if (save_codec_output) {
    encoder_settings.encoder_output_base_path = output_path + "_enc_output";
  }

  std::unique_ptr<VideoCodecTester> tester = CreateVideoCodecTester();
  return tester->RunEncodeTest(video_source.get(), encoder.get(),
                               encoder_settings);
}

class SpatialQualityTest : public ::testing::TestWithParam<
                               std::tuple</*codec_type=*/std::string,
                                          /*codec_impl=*/std::string,
                                          VideoInfo,
                                          std::tuple</*width=*/int,
                                                     /*height=*/int,
                                                     /*framerate_fps=*/double,
                                                     /*bitrate_kbps=*/int,
                                                     /*min_psnr=*/double>>> {
 public:
  static std::string TestParamsToString(
      const ::testing::TestParamInfo<SpatialQualityTest::ParamType>& info) {
    auto [codec_type, codec_impl, video_info, coding_settings] = info.param;
    auto [width, height, framerate_fps, bitrate_kbps, psnr] = coding_settings;
    return std::string(codec_type + codec_impl + video_info.name +
                       std::to_string(width) + "x" + std::to_string(height) +
                       "p" +
                       std::to_string(static_cast<int>(1000 * framerate_fps)) +
                       "mhz" + std::to_string(bitrate_kbps) + "kbps");
  }
};

TEST_P(SpatialQualityTest, SpatialQuality) {
  auto [codec_type, codec_impl, video_info, coding_settings] = GetParam();
  auto [width, height, framerate_fps, bitrate_kbps, psnr] = coding_settings;

  std::map<int, EncodingSettings> frame_settings = {
      {0,
       {.scalability_mode = ScalabilityMode::kL1T1,
        .layer_settings = {
            {LayerId{.spatial_idx = 0, .temporal_idx = 0},
             {.resolution = {.width = width, .height = height},
              .framerate = Frequency::MilliHertz(1000 * framerate_fps),
              .bitrate = DataRate::KilobitsPerSec(bitrate_kbps)}}}}}};

  int duration_s = 10;
  int num_frames = duration_s * framerate_fps;

  std::unique_ptr<VideoCodecStats> stats = RunEncodeDecodeTest(
      codec_type, codec_impl, video_info, frame_settings, num_frames,
      /*save_codec_input=*/false, /*save_codec_output=*/false);

  VideoCodecStats::Stream stream;
  if (stats != nullptr) {
    std::vector<VideoCodecStats::Frame> frames = stats->Slice();
    SetTargetRates(frame_settings, frames);
    stream = stats->Aggregate(frames);
    if (absl::GetFlag(FLAGS_webrtc_quick_perf_test)) {
      EXPECT_GE(stream.psnr.y.GetAverage(), psnr);
    }
  }

  stream.LogMetrics(
      GetGlobalMetricsLogger(),
      ::testing::UnitTest::GetInstance()->current_test_info()->name(),
      /*metadata=*/
      {{"codec_type", codec_type},
       {"codec_impl", codec_impl},
       {"video_name", video_info.name}});
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SpatialQualityTest,
    Combine(Values("AV1", "VP9", "VP8", "H264", "H265"),
#if defined(WEBRTC_ANDROID)
            Values("builtin", "mediacodec"),
#else
            Values("builtin"),
#endif
            Values(kFourPeople_1280x720_30),
            Values(std::make_tuple(320, 180, 30, 32, 28),
                   std::make_tuple(320, 180, 30, 64, 30),
                   std::make_tuple(320, 180, 30, 128, 33),
                   std::make_tuple(320, 180, 30, 256, 36),
                   std::make_tuple(640, 360, 30, 128, 31),
                   std::make_tuple(640, 360, 30, 256, 33),
                   std::make_tuple(640, 360, 30, 384, 35),
                   std::make_tuple(640, 360, 30, 512, 36),
                   std::make_tuple(1280, 720, 30, 256, 32),
                   std::make_tuple(1280, 720, 30, 512, 34),
                   std::make_tuple(1280, 720, 30, 1024, 37),
                   std::make_tuple(1280, 720, 30, 2048, 39))),
    SpatialQualityTest::TestParamsToString);

class BitrateAdaptationTest
    : public ::testing::TestWithParam<
          std::tuple</*codec_type=*/std::string,
                     /*codec_impl=*/std::string,
                     VideoInfo,
                     std::pair</*bitrate_kbps=*/int, /*bitrate_kbps=*/int>>> {
 public:
  static std::string TestParamsToString(
      const ::testing::TestParamInfo<BitrateAdaptationTest::ParamType>& info) {
    auto [codec_type, codec_impl, video_info, bitrate_kbps] = info.param;
    return std::string(codec_type + codec_impl + video_info.name +
                       std::to_string(bitrate_kbps.first) + "kbps" +
                       std::to_string(bitrate_kbps.second) + "kbps");
  }
};

TEST_P(BitrateAdaptationTest, BitrateAdaptation) {
  auto [codec_type, codec_impl, video_info, bitrate_kbps] = GetParam();

  int duration_s = 10;  // Duration of fixed rate interval.
  int first_frame = duration_s * video_info.framerate.millihertz() / 1000;
  int num_frames = 2 * duration_s * video_info.framerate.millihertz() / 1000;

  std::map<int, EncodingSettings> frame_settings = {
      {0,
       {.layer_settings = {{LayerId{.spatial_idx = 0, .temporal_idx = 0},
                            {.resolution = {.width = 640, .height = 360},
                             .framerate = video_info.framerate,
                             .bitrate = DataRate::KilobitsPerSec(
                                 bitrate_kbps.first)}}}}},
      {first_frame,
       {.layer_settings = {
            {LayerId{.spatial_idx = 0, .temporal_idx = 0},
             {.resolution = {.width = 640, .height = 360},
              .framerate = video_info.framerate,
              .bitrate = DataRate::KilobitsPerSec(bitrate_kbps.second)}}}}}};

  std::unique_ptr<VideoCodecStats> stats = RunEncodeTest(
      codec_type, codec_impl, video_info, frame_settings, num_frames,
      /*save_codec_input=*/false, /*save_codec_output=*/false);

  VideoCodecStats::Stream stream;
  if (stats != nullptr) {
    std::vector<VideoCodecStats::Frame> frames =
        stats->Slice(VideoCodecStats::Filter{.first_frame = first_frame});
    SetTargetRates(frame_settings, frames);
    stream = stats->Aggregate(frames);
    if (absl::GetFlag(FLAGS_webrtc_quick_perf_test)) {
      EXPECT_NEAR(stream.bitrate_mismatch_pct.GetAverage(), 0, 10);
      EXPECT_NEAR(stream.framerate_mismatch_pct.GetAverage(), 0, 10);
    }
  }

  stream.LogMetrics(
      GetGlobalMetricsLogger(),
      ::testing::UnitTest::GetInstance()->current_test_info()->name(),
      /*metadata=*/
      {{"codec_type", codec_type},
       {"codec_impl", codec_impl},
       {"video_name", video_info.name},
       {"rate_profile", std::to_string(bitrate_kbps.first) + "," +
                            std::to_string(bitrate_kbps.second)}});
}

INSTANTIATE_TEST_SUITE_P(All,
                         BitrateAdaptationTest,
                         Combine(Values("AV1", "VP9", "VP8", "H264", "H265"),
#if defined(WEBRTC_ANDROID)
                                 Values("builtin", "mediacodec"),
#else
                                 Values("builtin"),
#endif
                                 Values(kFourPeople_1280x720_30),
                                 Values(std::pair(1024, 512),
                                        std::pair(512, 1024))),
                         BitrateAdaptationTest::TestParamsToString);

class FramerateAdaptationTest
    : public ::testing::TestWithParam<std::tuple</*codec_type=*/std::string,
                                                 /*codec_impl=*/std::string,
                                                 VideoInfo,
                                                 std::pair<double, double>>> {
 public:
  static std::string TestParamsToString(
      const ::testing::TestParamInfo<FramerateAdaptationTest::ParamType>&
          info) {
    auto [codec_type, codec_impl, video_info, framerate_fps] = info.param;
    return std::string(
        codec_type + codec_impl + video_info.name +
        std::to_string(static_cast<int>(1000 * framerate_fps.first)) + "mhz" +
        std::to_string(static_cast<int>(1000 * framerate_fps.second)) + "mhz");
  }
};

TEST_P(FramerateAdaptationTest, FramerateAdaptation) {
  auto [codec_type, codec_impl, video_info, framerate_fps] = GetParam();

  int duration_s = 10;  // Duration of fixed rate interval.
  int first_frame = static_cast<int>(duration_s * framerate_fps.first);
  int num_frames = static_cast<int>(
      duration_s * (framerate_fps.first + framerate_fps.second));

  std::map<int, EncodingSettings> frame_settings = {
      {0,
       {.layer_settings = {{LayerId{.spatial_idx = 0, .temporal_idx = 0},
                            {.resolution = {.width = 640, .height = 360},
                             .framerate = Frequency::MilliHertz(
                                 1000 * framerate_fps.first),
                             .bitrate = DataRate::KilobitsPerSec(512)}}}}},
      {first_frame,
       {.layer_settings = {
            {LayerId{.spatial_idx = 0, .temporal_idx = 0},
             {.resolution = {.width = 640, .height = 360},
              .framerate = Frequency::MilliHertz(1000 * framerate_fps.second),
              .bitrate = DataRate::KilobitsPerSec(512)}}}}}};

  std::unique_ptr<VideoCodecStats> stats = RunEncodeTest(
      codec_type, codec_impl, video_info, frame_settings, num_frames,
      /*save_codec_input=*/false, /*save_codec_output=*/false);

  VideoCodecStats::Stream stream;
  if (stats != nullptr) {
    std::vector<VideoCodecStats::Frame> frames =
        stats->Slice(VideoCodecStats::Filter{.first_frame = first_frame});
    SetTargetRates(frame_settings, frames);
    stream = stats->Aggregate(frames);
    if (absl::GetFlag(FLAGS_webrtc_quick_perf_test)) {
      EXPECT_NEAR(stream.bitrate_mismatch_pct.GetAverage(), 0, 10);
      EXPECT_NEAR(stream.framerate_mismatch_pct.GetAverage(), 0, 10);
    }
  }

  stream.LogMetrics(
      GetGlobalMetricsLogger(),
      ::testing::UnitTest::GetInstance()->current_test_info()->name(),
      /*metadata=*/
      {{"codec_type", codec_type},
       {"codec_impl", codec_impl},
       {"video_name", video_info.name},
       {"rate_profile", std::to_string(framerate_fps.first) + "," +
                            std::to_string(framerate_fps.second)}});
}

INSTANTIATE_TEST_SUITE_P(All,
                         FramerateAdaptationTest,
                         Combine(Values("AV1", "VP9", "VP8", "H264", "H265"),
#if defined(WEBRTC_ANDROID)
                                 Values("builtin", "mediacodec"),
#else
                                 Values("builtin"),
#endif
                                 Values(kFourPeople_1280x720_30),
                                 Values(std::pair(30, 15), std::pair(15, 30))),
                         FramerateAdaptationTest::TestParamsToString);

}  // namespace test

}  // namespace webrtc
