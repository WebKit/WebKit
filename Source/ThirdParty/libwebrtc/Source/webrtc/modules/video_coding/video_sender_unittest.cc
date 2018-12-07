/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <vector>

#include "api/test/mock_video_encoder.h"
#include "api/video/i420_buffer.h"
#include "api/video_codecs/vp8_temporal_layers.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "modules/video_coding/include/mock/mock_vcm_callbacks.h"
#include "modules/video_coding/include/video_coding.h"
#include "modules/video_coding/utility/default_video_bitrate_allocator.h"
#include "modules/video_coding/utility/simulcast_rate_allocator.h"
#include "modules/video_coding/video_coding_impl.h"
#include "system_wrappers/include/clock.h"
#include "test/frame_generator.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"
#include "test/video_codec_settings.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Field;
using ::testing::NiceMock;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::FloatEq;
using std::vector;
using webrtc::test::FrameGenerator;

namespace webrtc {
namespace vcm {
namespace {
static const int kDefaultHeight = 720;
static const int kDefaultWidth = 1280;
static const int kMaxNumberOfTemporalLayers = 3;
static const int kNumberOfLayers = 3;
static const int kNumberOfStreams = 3;
static const int kUnusedPayloadType = 10;

struct Vp8StreamInfo {
  float framerate_fps[kMaxNumberOfTemporalLayers];
  int bitrate_kbps[kMaxNumberOfTemporalLayers];
};

MATCHER_P(MatchesVp8StreamInfo, expected, "") {
  bool res = true;
  for (int tl = 0; tl < kMaxNumberOfTemporalLayers; ++tl) {
    if (fabs(expected.framerate_fps[tl] - arg.framerate_fps[tl]) > 0.5) {
      *result_listener << " framerate_fps[" << tl
                       << "] = " << arg.framerate_fps[tl] << " (expected "
                       << expected.framerate_fps[tl] << ") ";
      res = false;
    }
    if (abs(expected.bitrate_kbps[tl] - arg.bitrate_kbps[tl]) > 10) {
      *result_listener << " bitrate_kbps[" << tl
                       << "] = " << arg.bitrate_kbps[tl] << " (expected "
                       << expected.bitrate_kbps[tl] << ") ";
      res = false;
    }
  }
  return res;
}

class EmptyFrameGenerator : public FrameGenerator {
 public:
  EmptyFrameGenerator(int width, int height) : width_(width), height_(height) {}
  VideoFrame* NextFrame() override {
    frame_.reset(new VideoFrame(I420Buffer::Create(width_, height_),
                                webrtc::kVideoRotation_0, 0));
    return frame_.get();
  }

 private:
  const int width_;
  const int height_;
  std::unique_ptr<VideoFrame> frame_;
};

class EncodedImageCallbackImpl : public EncodedImageCallback {
 public:
  explicit EncodedImageCallbackImpl(Clock* clock)
      : clock_(clock), start_time_ms_(clock_->TimeInMilliseconds()) {}

  virtual ~EncodedImageCallbackImpl() {}

  Result OnEncodedImage(const EncodedImage& encoded_image,
                        const CodecSpecificInfo* codec_specific_info,
                        const RTPFragmentationHeader* fragmentation) override {
    assert(codec_specific_info);
    frame_data_.push_back(
        FrameData(encoded_image._length, *codec_specific_info));
    return Result(Result::OK, encoded_image.Timestamp());
  }

  void Reset() {
    frame_data_.clear();
    start_time_ms_ = clock_->TimeInMilliseconds();
  }

  float FramerateFpsWithinTemporalLayer(int temporal_layer) {
    return CountFramesWithinTemporalLayer(temporal_layer) *
           (1000.0 / interval_ms());
  }

  float BitrateKbpsWithinTemporalLayer(int temporal_layer) {
    return SumPayloadBytesWithinTemporalLayer(temporal_layer) * 8.0 /
           interval_ms();
  }

  Vp8StreamInfo CalculateVp8StreamInfo() {
    Vp8StreamInfo info;
    for (int tl = 0; tl < 3; ++tl) {
      info.framerate_fps[tl] = FramerateFpsWithinTemporalLayer(tl);
      info.bitrate_kbps[tl] = BitrateKbpsWithinTemporalLayer(tl);
    }
    return info;
  }

 private:
  struct FrameData {
    FrameData() : payload_size(0) {}

    FrameData(size_t payload_size, const CodecSpecificInfo& codec_specific_info)
        : payload_size(payload_size),
          codec_specific_info(codec_specific_info) {}

    size_t payload_size;
    CodecSpecificInfo codec_specific_info;
  };

  int64_t interval_ms() {
    int64_t diff = (clock_->TimeInMilliseconds() - start_time_ms_);
    EXPECT_GT(diff, 0);
    return diff;
  }

  int CountFramesWithinTemporalLayer(int temporal_layer) {
    int frames = 0;
    for (size_t i = 0; i < frame_data_.size(); ++i) {
      EXPECT_EQ(kVideoCodecVP8, frame_data_[i].codec_specific_info.codecType);
      const uint8_t temporal_idx =
          frame_data_[i].codec_specific_info.codecSpecific.VP8.temporalIdx;
      if (temporal_idx <= temporal_layer || temporal_idx == kNoTemporalIdx)
        frames++;
    }
    return frames;
  }

  size_t SumPayloadBytesWithinTemporalLayer(int temporal_layer) {
    size_t payload_size = 0;
    for (size_t i = 0; i < frame_data_.size(); ++i) {
      EXPECT_EQ(kVideoCodecVP8, frame_data_[i].codec_specific_info.codecType);
      const uint8_t temporal_idx =
          frame_data_[i].codec_specific_info.codecSpecific.VP8.temporalIdx;
      if (temporal_idx <= temporal_layer || temporal_idx == kNoTemporalIdx)
        payload_size += frame_data_[i].payload_size;
    }
    return payload_size;
  }

  Clock* clock_;
  int64_t start_time_ms_;
  vector<FrameData> frame_data_;
};

class TestVideoSender : public ::testing::Test {
 protected:
  // Note: simulated clock starts at 1 seconds, since parts of webrtc use 0 as
  // a special case (e.g. frame rate in media optimization).
  TestVideoSender() : clock_(1000), encoded_frame_callback_(&clock_) {}

  void SetUp() override {
    sender_.reset(new VideoSender(&clock_, &encoded_frame_callback_));
  }

  void AddFrame() {
    assert(generator_.get());
    sender_->AddVideoFrame(*generator_->NextFrame(), nullptr,
                           encoder_ ? absl::optional<VideoEncoder::EncoderInfo>(
                                          encoder_->GetEncoderInfo())
                                    : absl::nullopt);
  }

  SimulatedClock clock_;
  EncodedImageCallbackImpl encoded_frame_callback_;
  // Used by subclassing tests, need to outlive sender_.
  std::unique_ptr<VideoEncoder> encoder_;
  std::unique_ptr<VideoSender> sender_;
  std::unique_ptr<FrameGenerator> generator_;
};

class TestVideoSenderWithMockEncoder : public TestVideoSender {
 public:
  TestVideoSenderWithMockEncoder() {}
  ~TestVideoSenderWithMockEncoder() override {}

 protected:
  void SetUp() override {
    TestVideoSender::SetUp();
    sender_->RegisterExternalEncoder(&encoder_, false);
    webrtc::test::CodecSettings(kVideoCodecVP8, &settings_);
    settings_.numberOfSimulcastStreams = kNumberOfStreams;
    ConfigureStream(kDefaultWidth / 4, kDefaultHeight / 4, 100,
                    &settings_.simulcastStream[0]);
    ConfigureStream(kDefaultWidth / 2, kDefaultHeight / 2, 500,
                    &settings_.simulcastStream[1]);
    ConfigureStream(kDefaultWidth, kDefaultHeight, 1200,
                    &settings_.simulcastStream[2]);
    settings_.plType = kUnusedPayloadType;  // Use the mocked encoder.
    generator_.reset(
        new EmptyFrameGenerator(settings_.width, settings_.height));
    EXPECT_EQ(0, sender_->RegisterSendCodec(&settings_, 1, 1200));
    rate_allocator_.reset(new DefaultVideoBitrateAllocator(settings_));
  }

  void TearDown() override { sender_.reset(); }

  void ExpectIntraRequest(int stream) {
    ExpectEncodeWithFrameTypes(stream, false);
  }

  void ExpectInitialKeyFrames() { ExpectEncodeWithFrameTypes(-1, true); }

  void ExpectEncodeWithFrameTypes(int intra_request_stream, bool first_frame) {
    if (intra_request_stream == -1) {
      // No intra request expected, keyframes on first frame.
      FrameType frame_type = first_frame ? kVideoFrameKey : kVideoFrameDelta;
      EXPECT_CALL(
          encoder_,
          Encode(_, _,
                 Pointee(ElementsAre(frame_type, frame_type, frame_type))))
          .Times(1)
          .WillRepeatedly(Return(0));
      return;
    }
    ASSERT_FALSE(first_frame);
    ASSERT_GE(intra_request_stream, 0);
    ASSERT_LT(intra_request_stream, kNumberOfStreams);
    std::vector<FrameType> frame_types(kNumberOfStreams, kVideoFrameDelta);
    frame_types[intra_request_stream] = kVideoFrameKey;
    EXPECT_CALL(
        encoder_,
        Encode(_, _,
               Pointee(ElementsAreArray(&frame_types[0], frame_types.size()))))
        .Times(1)
        .WillRepeatedly(Return(0));
  }

  static void ConfigureStream(int width,
                              int height,
                              int max_bitrate,
                              SimulcastStream* stream) {
    assert(stream);
    stream->width = width;
    stream->height = height;
    stream->maxBitrate = max_bitrate;
    stream->numberOfTemporalLayers = kNumberOfLayers;
    stream->qpMax = 45;
  }

  VideoCodec settings_;
  NiceMock<MockVideoEncoder> encoder_;
  std::unique_ptr<DefaultVideoBitrateAllocator> rate_allocator_;
};

TEST_F(TestVideoSenderWithMockEncoder, TestIntraRequests) {
  // Initial request should be all keyframes.
  ExpectInitialKeyFrames();
  AddFrame();
  EXPECT_EQ(0, sender_->IntraFrameRequest(0));
  ExpectIntraRequest(0);
  AddFrame();
  ExpectIntraRequest(-1);
  AddFrame();

  EXPECT_EQ(0, sender_->IntraFrameRequest(1));
  ExpectIntraRequest(1);
  AddFrame();
  ExpectIntraRequest(-1);
  AddFrame();

  EXPECT_EQ(0, sender_->IntraFrameRequest(2));
  ExpectIntraRequest(2);
  AddFrame();
  ExpectIntraRequest(-1);
  AddFrame();

  EXPECT_EQ(-1, sender_->IntraFrameRequest(3));
  ExpectIntraRequest(-1);
  AddFrame();
}

TEST_F(TestVideoSenderWithMockEncoder, TestSetRate) {
  // Let actual fps be half of max, so it can be distinguished from default.
  const uint32_t kActualFrameRate = settings_.maxFramerate / 2;
  const int64_t kFrameIntervalMs = 1000 / kActualFrameRate;
  const uint32_t new_bitrate_kbps = settings_.startBitrate + 300;

  // Initial frame rate is taken from config, as we have no data yet.
  VideoBitrateAllocation new_rate_allocation = rate_allocator_->GetAllocation(
      new_bitrate_kbps * 1000, settings_.maxFramerate);
  EXPECT_CALL(encoder_,
              SetRateAllocation(new_rate_allocation, settings_.maxFramerate))
      .Times(1)
      .WillOnce(Return(0));
  sender_->SetChannelParameters(new_bitrate_kbps * 1000, 0, 200,
                                rate_allocator_.get(), nullptr);
  AddFrame();
  clock_.AdvanceTimeMilliseconds(kFrameIntervalMs);

  // Expect no call to encoder_.SetRates if the new bitrate is zero.
  EXPECT_CALL(encoder_, SetRateAllocation(_, _)).Times(0);
  sender_->SetChannelParameters(0, 0, 200, rate_allocator_.get(), nullptr);
  AddFrame();
}

TEST_F(TestVideoSenderWithMockEncoder, TestIntraRequestsInternalCapture) {
  // De-register current external encoder.
  sender_->RegisterExternalEncoder(nullptr, false);
  // Register encoder with internal capture.
  sender_->RegisterExternalEncoder(&encoder_, true);
  EXPECT_EQ(0, sender_->RegisterSendCodec(&settings_, 1, 1200));
  // Initial request should be all keyframes.
  ExpectInitialKeyFrames();
  AddFrame();
  ExpectIntraRequest(0);
  EXPECT_EQ(0, sender_->IntraFrameRequest(0));
  ExpectIntraRequest(1);
  EXPECT_EQ(0, sender_->IntraFrameRequest(1));
  ExpectIntraRequest(2);
  EXPECT_EQ(0, sender_->IntraFrameRequest(2));
  // No requests expected since these indices are out of bounds.
  EXPECT_EQ(-1, sender_->IntraFrameRequest(3));
}

TEST_F(TestVideoSenderWithMockEncoder, TestEncoderParametersForInternalSource) {
  // De-register current external encoder.
  sender_->RegisterExternalEncoder(nullptr, false);
  // Register encoder with internal capture.
  sender_->RegisterExternalEncoder(&encoder_, true);
  EXPECT_EQ(0, sender_->RegisterSendCodec(&settings_, 1, 1200));
  // Update encoder bitrate parameters. We expect that to immediately call
  // SetRates on the encoder without waiting for AddFrame processing.
  const uint32_t new_bitrate_kbps = settings_.startBitrate + 300;
  VideoBitrateAllocation new_rate_allocation = rate_allocator_->GetAllocation(
      new_bitrate_kbps * 1000, settings_.maxFramerate);
  EXPECT_CALL(encoder_, SetRateAllocation(new_rate_allocation, _))
      .Times(1)
      .WillOnce(Return(0));
  sender_->SetChannelParameters(new_bitrate_kbps * 1000, 0, 200,
                                rate_allocator_.get(), nullptr);
}

TEST_F(TestVideoSenderWithMockEncoder,
       NoRedundantSetChannelParameterOrSetRatesCalls) {
  const uint8_t kLossRate = 4;
  const uint8_t kRtt = 200;
  const int64_t kRateStatsWindowMs = 2000;
  const uint32_t kInputFps = 20;
  int64_t start_time = clock_.TimeInMilliseconds();
  // Expect initial call to SetChannelParameters. Rates are initialized through
  // InitEncode and expects no additional call before the framerate (or bitrate)
  // updates.
  sender_->SetChannelParameters(settings_.startBitrate * 1000, kLossRate, kRtt,
                                rate_allocator_.get(), nullptr);
  while (clock_.TimeInMilliseconds() < start_time + kRateStatsWindowMs) {
    AddFrame();
    clock_.AdvanceTimeMilliseconds(1000 / kInputFps);
  }

  // Call to SetChannelParameters with changed bitrate should call encoder
  // SetRates but not encoder SetChannelParameters (that are unchanged).
  uint32_t new_bitrate_bps = 2 * settings_.startBitrate * 1000;
  VideoBitrateAllocation new_rate_allocation =
      rate_allocator_->GetAllocation(new_bitrate_bps, kInputFps);
  EXPECT_CALL(encoder_, SetRateAllocation(new_rate_allocation, kInputFps))
      .Times(1)
      .WillOnce(Return(0));
  sender_->SetChannelParameters(new_bitrate_bps, kLossRate, kRtt,
                                rate_allocator_.get(), nullptr);
  AddFrame();
}

class TestVideoSenderWithVp8 : public TestVideoSender {
 public:
  TestVideoSenderWithVp8()
      : codec_bitrate_kbps_(300), available_bitrate_kbps_(1000) {}

  void SetUp() override {
    TestVideoSender::SetUp();

    const char* input_video = "foreman_cif";
    const int width = 352;
    const int height = 288;
    generator_ = FrameGenerator::CreateFromYuvFile(
        std::vector<std::string>(1, test::ResourcePath(input_video, "yuv")),
        width, height, 1);

    codec_ = MakeVp8VideoCodec(width, height, 3);
    codec_.minBitrate = 10;
    codec_.startBitrate = codec_bitrate_kbps_;
    codec_.maxBitrate = codec_bitrate_kbps_;

    rate_allocator_.reset(new SimulcastRateAllocator(codec_));

    encoder_ = VP8Encoder::Create();
    sender_->RegisterExternalEncoder(encoder_.get(), false);
    EXPECT_EQ(0, sender_->RegisterSendCodec(&codec_, 1, 1200));
  }

  static VideoCodec MakeVp8VideoCodec(int width,
                                      int height,
                                      int temporal_layers) {
    VideoCodec codec;
    webrtc::test::CodecSettings(kVideoCodecVP8, &codec);
    codec.width = width;
    codec.height = height;
    codec.VP8()->numberOfTemporalLayers = temporal_layers;
    return codec;
  }

  void InsertFrames(float framerate, float seconds) {
    for (int i = 0; i < seconds * framerate; ++i) {
      clock_.AdvanceTimeMilliseconds(1000.0f / framerate);
      AddFrame();
      // SetChannelParameters needs to be called frequently to propagate
      // framerate from the media optimization into the encoder.
      // Note: SetChannelParameters fails if less than 2 frames are in the
      // buffer since it will fail to calculate the framerate.
      if (i != 0) {
        EXPECT_EQ(VCM_OK, sender_->SetChannelParameters(
                              available_bitrate_kbps_ * 1000, 0, 200,
                              rate_allocator_.get(), nullptr));
      }
    }
  }

  Vp8StreamInfo SimulateWithFramerate(float framerate) {
    const float short_simulation_interval = 5.0;
    const float long_simulation_interval = 10.0;
    // It appears that this 5 seconds simulation is needed to allow
    // bitrate and framerate to stabilize.
    InsertFrames(framerate, short_simulation_interval);
    encoded_frame_callback_.Reset();

    InsertFrames(framerate, long_simulation_interval);
    return encoded_frame_callback_.CalculateVp8StreamInfo();
  }

 protected:
  VideoCodec codec_;
  int codec_bitrate_kbps_;
  int available_bitrate_kbps_;
  std::unique_ptr<SimulcastRateAllocator> rate_allocator_;
};

#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
#define MAYBE_FixedTemporalLayersStrategy DISABLED_FixedTemporalLayersStrategy
#else
#define MAYBE_FixedTemporalLayersStrategy FixedTemporalLayersStrategy
#endif
TEST_F(TestVideoSenderWithVp8, MAYBE_FixedTemporalLayersStrategy) {
  const int low_b =
      codec_bitrate_kbps_ *
      webrtc::SimulcastRateAllocator::GetTemporalRateAllocation(3, 0);
  const int mid_b =
      codec_bitrate_kbps_ *
      webrtc::SimulcastRateAllocator::GetTemporalRateAllocation(3, 1);
  const int high_b =
      codec_bitrate_kbps_ *
      webrtc::SimulcastRateAllocator::GetTemporalRateAllocation(3, 2);
  {
    Vp8StreamInfo expected = {{7.5, 15.0, 30.0}, {low_b, mid_b, high_b}};
    EXPECT_THAT(SimulateWithFramerate(30.0), MatchesVp8StreamInfo(expected));
  }
  {
    Vp8StreamInfo expected = {{3.75, 7.5, 15.0}, {low_b, mid_b, high_b}};
    EXPECT_THAT(SimulateWithFramerate(15.0), MatchesVp8StreamInfo(expected));
  }
}

}  // namespace
}  // namespace vcm
}  // namespace webrtc
