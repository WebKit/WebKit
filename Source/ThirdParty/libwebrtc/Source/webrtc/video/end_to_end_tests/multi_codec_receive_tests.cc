/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "api/test/simulated_network.h"
#include "api/test/video/function_video_encoder_factory.h"
#include "call/fake_network_pipe.h"
#include "call/simulated_network.h"
#include "modules/include/module_common_types_public.h"
#include "modules/rtp_rtcp/source/rtp_packet.h"
#include "modules/video_coding/codecs/h264/include/h264.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"
#include "rtc_base/task_queue_for_test.h"
#include "test/call_test.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::Contains;

namespace webrtc {
namespace {
constexpr int kWidth = 1280;
constexpr int kHeight = 720;
constexpr int kFps = 30;
constexpr int kFramesToObserve = 10;

uint8_t PayloadNameToPayloadType(const std::string& payload_name) {
  if (payload_name == "VP8") {
    return test::CallTest::kPayloadTypeVP8;
  } else if (payload_name == "VP9") {
    return test::CallTest::kPayloadTypeVP9;
  } else if (payload_name == "H264") {
    return test::CallTest::kPayloadTypeH264;
  } else {
    RTC_NOTREACHED();
    return 0;
  }
}

int RemoveOlderOrEqual(uint32_t timestamp, std::vector<uint32_t>* timestamps) {
  int num_removed = 0;
  while (!timestamps->empty()) {
    auto it = timestamps->begin();
    if (IsNewerTimestamp(*it, timestamp))
      break;

    timestamps->erase(it);
    ++num_removed;
  }
  return num_removed;
}

class FrameObserver : public test::RtpRtcpObserver,
                      public rtc::VideoSinkInterface<VideoFrame> {
 public:
  FrameObserver() : test::RtpRtcpObserver(test::CallTest::kDefaultTimeoutMs) {}

  void Reset(uint8_t expected_payload_type) {
    rtc::CritScope lock(&crit_);
    num_sent_frames_ = 0;
    num_rendered_frames_ = 0;
    expected_payload_type_ = expected_payload_type;
  }

 private:
  // Sends kFramesToObserve.
  Action OnSendRtp(const uint8_t* packet, size_t length) override {
    rtc::CritScope lock(&crit_);

    RtpPacket rtp_packet;
    EXPECT_TRUE(rtp_packet.Parse(packet, length));
    EXPECT_EQ(rtp_packet.Ssrc(), test::CallTest::kVideoSendSsrcs[0]);
    if (rtp_packet.payload_size() == 0)
      return SEND_PACKET;  // Skip padding, may be sent after OnFrame is called.

    if (expected_payload_type_ &&
        rtp_packet.PayloadType() != expected_payload_type_.value()) {
      return DROP_PACKET;  // All frames sent.
    }

    if (!last_timestamp_ || rtp_packet.Timestamp() != *last_timestamp_) {
      // New frame.
      // Sent enough frames?
      if (num_sent_frames_ >= kFramesToObserve)
        return DROP_PACKET;

      ++num_sent_frames_;
      sent_timestamps_.push_back(rtp_packet.Timestamp());
    }

    last_timestamp_ = rtp_packet.Timestamp();
    return SEND_PACKET;
  }

  // Verifies that all sent frames are decoded and rendered.
  void OnFrame(const VideoFrame& rendered_frame) override {
    rtc::CritScope lock(&crit_);
    EXPECT_THAT(sent_timestamps_, Contains(rendered_frame.timestamp()));

    // Remove old timestamps too, only the newest decoded frame is rendered.
    num_rendered_frames_ +=
        RemoveOlderOrEqual(rendered_frame.timestamp(), &sent_timestamps_);

    if (num_rendered_frames_ >= kFramesToObserve) {
      EXPECT_TRUE(sent_timestamps_.empty()) << "All sent frames not decoded.";
      observation_complete_.Set();
    }
  }

  rtc::CriticalSection crit_;
  absl::optional<uint32_t> last_timestamp_;  // Only accessed from pacer thread.
  absl::optional<uint8_t> expected_payload_type_ RTC_GUARDED_BY(crit_);
  int num_sent_frames_ RTC_GUARDED_BY(crit_) = 0;
  int num_rendered_frames_ RTC_GUARDED_BY(crit_) = 0;
  std::vector<uint32_t> sent_timestamps_ RTC_GUARDED_BY(crit_);
};
}  // namespace

class MultiCodecReceiveTest : public test::CallTest {
 public:
  MultiCodecReceiveTest() {
    SendTask(RTC_FROM_HERE, task_queue(), [this]() {
      CreateCalls();

      send_transport_.reset(new test::PacketTransport(
          task_queue(), sender_call_.get(), &observer_,
          test::PacketTransport::kSender, kPayloadTypeMap,
          std::make_unique<FakeNetworkPipe>(
              Clock::GetRealTimeClock(), std::make_unique<SimulatedNetwork>(
                                             BuiltInNetworkBehaviorConfig()))));
      send_transport_->SetReceiver(receiver_call_->Receiver());

      receive_transport_.reset(new test::PacketTransport(
          task_queue(), receiver_call_.get(), &observer_,
          test::PacketTransport::kReceiver, kPayloadTypeMap,
          std::make_unique<FakeNetworkPipe>(
              Clock::GetRealTimeClock(), std::make_unique<SimulatedNetwork>(
                                             BuiltInNetworkBehaviorConfig()))));
      receive_transport_->SetReceiver(sender_call_->Receiver());
    });
  }

  virtual ~MultiCodecReceiveTest() {
    SendTask(RTC_FROM_HERE, task_queue(), [this]() {
      send_transport_.reset();
      receive_transport_.reset();
      DestroyCalls();
    });
  }

  struct CodecConfig {
    std::string payload_name;
    VideoEncoderFactory* encoder_factory;
    VideoDecoderFactory* decoder_factory;
    size_t num_temporal_layers;
  };

  void ConfigureEncoder(const CodecConfig& config);
  void ConfigureDecoders(const std::vector<CodecConfig>& configs);
  void RunTestWithCodecs(const std::vector<CodecConfig>& configs);

 private:
  const std::map<uint8_t, MediaType> kPayloadTypeMap = {
      {CallTest::kPayloadTypeVP8, MediaType::VIDEO},
      {CallTest::kPayloadTypeVP9, MediaType::VIDEO},
      {CallTest::kPayloadTypeH264, MediaType::VIDEO}};
  FrameObserver observer_;
};

void MultiCodecReceiveTest::ConfigureDecoders(
    const std::vector<CodecConfig>& configs) {
  video_receive_configs_[0].decoders.clear();
  // Placing the payload names in a std::set retains the unique names only.
  std::set<std::string> unique_payload_names;
  for (const auto& config : configs)
    if (unique_payload_names.insert(config.payload_name).second) {
      VideoReceiveStream::Decoder decoder = test::CreateMatchingDecoder(
          PayloadNameToPayloadType(config.payload_name), config.payload_name);
      decoder.decoder_factory = config.decoder_factory;

      video_receive_configs_[0].decoders.push_back(decoder);
    }
}

void MultiCodecReceiveTest::ConfigureEncoder(const CodecConfig& config) {
  GetVideoSendConfig()->encoder_settings.encoder_factory =
      config.encoder_factory;
  GetVideoSendConfig()->rtp.payload_name = config.payload_name;
  GetVideoSendConfig()->rtp.payload_type =
      PayloadNameToPayloadType(config.payload_name);
  GetVideoEncoderConfig()->codec_type =
      PayloadStringToCodecType(config.payload_name);
  EXPECT_EQ(1u, GetVideoEncoderConfig()->simulcast_layers.size());
  GetVideoEncoderConfig()->simulcast_layers[0].num_temporal_layers =
      config.num_temporal_layers;
}

void MultiCodecReceiveTest::RunTestWithCodecs(
    const std::vector<CodecConfig>& configs) {
  EXPECT_TRUE(!configs.empty());

  // Create and start call.
  SendTask(RTC_FROM_HERE, task_queue(), [this, &configs]() {
    CreateSendConfig(1, 0, 0, send_transport_.get());
    ConfigureEncoder(configs[0]);
    CreateMatchingReceiveConfigs(receive_transport_.get());
    video_receive_configs_[0].renderer = &observer_;
    // Disable to avoid post-decode frame dropping in VideoRenderFrames.
    video_receive_configs_[0].enable_prerenderer_smoothing = false;
    ConfigureDecoders(configs);
    CreateVideoStreams();
    CreateFrameGeneratorCapturer(kFps, kWidth, kHeight);
    Start();
  });
  EXPECT_TRUE(observer_.Wait()) << "Timed out waiting for frames.";

  for (size_t i = 1; i < configs.size(); ++i) {
    // Recreate VideoSendStream with new config (codec, temporal layers).
    SendTask(RTC_FROM_HERE, task_queue(), [this, i, &configs]() {
      DestroyVideoSendStreams();
      observer_.Reset(PayloadNameToPayloadType(configs[i].payload_name));

      ConfigureEncoder(configs[i]);
      CreateVideoSendStreams();
      GetVideoSendStream()->Start();
      CreateFrameGeneratorCapturer(kFps, kWidth / 2, kHeight / 2);
      ConnectVideoSourcesToStreams();
    });
    EXPECT_TRUE(observer_.Wait()) << "Timed out waiting for frames.";
  }

  SendTask(RTC_FROM_HERE, task_queue(), [this]() {
    Stop();
    DestroyStreams();
  });
}

TEST_F(MultiCodecReceiveTest, SingleStreamReceivesVp8Vp9) {
  test::FunctionVideoEncoderFactory vp8_encoder_factory(
      []() { return VP8Encoder::Create(); });
  test::FunctionVideoEncoderFactory vp9_encoder_factory(
      []() { return VP9Encoder::Create(); });
  test::FunctionVideoDecoderFactory vp8_decoder_factory(
      []() { return VP8Decoder::Create(); });
  test::FunctionVideoDecoderFactory vp9_decoder_factory(
      []() { return VP9Decoder::Create(); });
  RunTestWithCodecs({{"VP8", &vp8_encoder_factory, &vp8_decoder_factory, 1},
                     {"VP9", &vp9_encoder_factory, &vp9_decoder_factory, 1},
                     {"VP8", &vp8_encoder_factory, &vp8_decoder_factory, 1}});
}

TEST_F(MultiCodecReceiveTest, SingleStreamReceivesVp8Vp9WithTl) {
  test::FunctionVideoEncoderFactory vp8_encoder_factory(
      []() { return VP8Encoder::Create(); });
  test::FunctionVideoEncoderFactory vp9_encoder_factory(
      []() { return VP9Encoder::Create(); });
  test::FunctionVideoDecoderFactory vp8_decoder_factory(
      []() { return VP8Decoder::Create(); });
  test::FunctionVideoDecoderFactory vp9_decoder_factory(
      []() { return VP9Decoder::Create(); });
  RunTestWithCodecs({{"VP8", &vp8_encoder_factory, &vp8_decoder_factory, 2},
                     {"VP9", &vp9_encoder_factory, &vp9_decoder_factory, 2},
                     {"VP8", &vp8_encoder_factory, &vp8_decoder_factory, 2}});
}

#if defined(WEBRTC_USE_H264)
TEST_F(MultiCodecReceiveTest, SingleStreamReceivesVp8H264) {
  test::FunctionVideoEncoderFactory vp8_encoder_factory(
      []() { return VP8Encoder::Create(); });
  test::FunctionVideoEncoderFactory h264_encoder_factory(
      []() { return H264Encoder::Create(cricket::VideoCodec("H264")); });
  test::FunctionVideoDecoderFactory vp8_decoder_factory(
      []() { return VP8Decoder::Create(); });
  test::FunctionVideoDecoderFactory h264_decoder_factory(
      []() { return H264Decoder::Create(); });
  RunTestWithCodecs({{"VP8", &vp8_encoder_factory, &vp8_decoder_factory, 1},
                     {"H264", &h264_encoder_factory, &h264_decoder_factory, 1},
                     {"VP8", &vp8_encoder_factory, &vp8_decoder_factory, 1}});
}

TEST_F(MultiCodecReceiveTest, SingleStreamReceivesVp8H264WithTl) {
  test::FunctionVideoEncoderFactory vp8_encoder_factory(
      []() { return VP8Encoder::Create(); });
  test::FunctionVideoEncoderFactory h264_encoder_factory(
      []() { return H264Encoder::Create(cricket::VideoCodec("H264")); });
  test::FunctionVideoDecoderFactory vp8_decoder_factory(
      []() { return VP8Decoder::Create(); });
  test::FunctionVideoDecoderFactory h264_decoder_factory(
      []() { return H264Decoder::Create(); });
  RunTestWithCodecs({{"VP8", &vp8_encoder_factory, &vp8_decoder_factory, 3},
                     {"H264", &h264_encoder_factory, &h264_decoder_factory, 1},
                     {"VP8", &vp8_encoder_factory, &vp8_decoder_factory, 3}});
}

TEST_F(MultiCodecReceiveTest, SingleStreamReceivesVp8Vp9H264) {
  test::FunctionVideoEncoderFactory vp8_encoder_factory(
      []() { return VP8Encoder::Create(); });
  test::FunctionVideoEncoderFactory vp9_encoder_factory(
      []() { return VP9Encoder::Create(); });
  test::FunctionVideoEncoderFactory h264_encoder_factory(
      []() { return H264Encoder::Create(cricket::VideoCodec("H264")); });
  test::FunctionVideoDecoderFactory vp8_decoder_factory(
      []() { return VP8Decoder::Create(); });
  test::FunctionVideoDecoderFactory vp9_decoder_factory(
      []() { return VP9Decoder::Create(); });
  test::FunctionVideoDecoderFactory h264_decoder_factory(
      []() { return H264Decoder::Create(); });
  RunTestWithCodecs({{"VP8", &vp8_encoder_factory, &vp8_decoder_factory, 1},
                     {"VP9", &vp9_encoder_factory, &vp9_decoder_factory, 1},
                     {"H264", &h264_encoder_factory, &h264_decoder_factory, 1},
                     {"VP9", &vp9_encoder_factory, &vp9_decoder_factory, 1}});
}

TEST_F(MultiCodecReceiveTest, SingleStreamReceivesVp8Vp9H264WithTl) {
  test::FunctionVideoEncoderFactory vp8_encoder_factory(
      []() { return VP8Encoder::Create(); });
  test::FunctionVideoEncoderFactory vp9_encoder_factory(
      []() { return VP9Encoder::Create(); });
  test::FunctionVideoEncoderFactory h264_encoder_factory(
      []() { return H264Encoder::Create(cricket::VideoCodec("H264")); });
  test::FunctionVideoDecoderFactory vp8_decoder_factory(
      []() { return VP8Decoder::Create(); });
  test::FunctionVideoDecoderFactory vp9_decoder_factory(
      []() { return VP9Decoder::Create(); });
  test::FunctionVideoDecoderFactory h264_decoder_factory(
      []() { return H264Decoder::Create(); });
  RunTestWithCodecs({{"VP8", &vp8_encoder_factory, &vp8_decoder_factory, 3},
                     {"VP9", &vp9_encoder_factory, &vp9_decoder_factory, 2},
                     {"H264", &h264_encoder_factory, &h264_decoder_factory, 1},
                     {"VP9", &vp9_encoder_factory, &vp9_decoder_factory, 3}});
}
#endif  // defined(WEBRTC_USE_H264)

}  // namespace webrtc
