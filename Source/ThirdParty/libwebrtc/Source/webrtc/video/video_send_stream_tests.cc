/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <algorithm>  // max
#include <memory>
#include <vector>

#include "api/test/simulated_network.h"
#include "api/video/encoded_image.h"
#include "call/call.h"
#include "call/fake_network_pipe.h"
#include "call/rtp_transport_controller_send.h"
#include "call/simulated_network.h"
#include "modules/rtp_rtcp/include/rtp_header_parser.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "modules/rtp_rtcp/source/rtcp_sender.h"
#include "modules/rtp_rtcp/source/rtp_format_vp9.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"
#include "rtc_base/bind.h"
#include "rtc_base/checks.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/event.h"
#include "rtc_base/experiments/alr_experiment.h"
#include "rtc_base/logging.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/rate_limiter.h"
#include "rtc_base/timeutils.h"
#include "system_wrappers/include/sleep.h"
#include "test/call_test.h"
#include "test/configurable_frame_size_encoder.h"
#include "test/fake_encoder.h"
#include "test/fake_texture_frame.h"
#include "test/field_trial.h"
#include "test/frame_generator.h"
#include "test/frame_generator_capturer.h"
#include "test/frame_utils.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/null_transport.h"
#include "test/rtcp_packet_parser.h"
#include "test/testsupport/perf_test.h"
#include "test/video_encoder_proxy_factory.h"

#include "call/video_send_stream.h"
#include "video/send_statistics_proxy.h"
#include "video/transport_adapter.h"
#include "video/video_send_stream.h"

namespace webrtc {
namespace test {
class VideoSendStreamPeer {
 public:
  explicit VideoSendStreamPeer(webrtc::VideoSendStream* base_class_stream)
      : internal_stream_(
            static_cast<internal::VideoSendStream*>(base_class_stream)) {}
  absl::optional<float> GetPacingFactorOverride() const {
    return internal_stream_->GetPacingFactorOverride();
  }

 private:
  internal::VideoSendStream const* const internal_stream_;
};
}  // namespace test

namespace {
constexpr int64_t kRtcpIntervalMs = 1000;

enum VideoFormat {
  kGeneric,
  kVP8,
};
}  // namespace

void ExpectEqualFramesVector(const std::vector<VideoFrame>& frames1,
                             const std::vector<VideoFrame>& frames2);
VideoFrame CreateVideoFrame(int width, int height, uint8_t data);

class VideoSendStreamTest : public test::CallTest,
                            public testing::WithParamInterface<std::string> {
 public:
  VideoSendStreamTest() : field_trial_(GetParam()) {}

 protected:
  void TestNackRetransmission(uint32_t retransmit_ssrc,
                              uint8_t retransmit_payload_type);
  void TestPacketFragmentationSize(VideoFormat format, bool with_fec);

  void TestVp9NonFlexMode(uint8_t num_temporal_layers,
                          uint8_t num_spatial_layers);

  void TestRequestSourceRotateVideo(bool support_orientation_ext);

 private:
  test::ScopedFieldTrials field_trial_;
};

INSTANTIATE_TEST_CASE_P(Default, VideoSendStreamTest, ::testing::Values(""));

INSTANTIATE_TEST_CASE_P(
    TaskQueueTrial,
    VideoSendStreamTest,
    ::testing::Values("WebRTC-TaskQueueCongestionControl/Enabled/"));

TEST_P(VideoSendStreamTest, CanStartStartedStream) {
  task_queue_.SendTask([this]() {
    CreateSenderCall();

    test::NullTransport transport;
    CreateSendConfig(1, 0, 0, &transport);
    CreateVideoStreams();
    GetVideoSendStream()->Start();
    GetVideoSendStream()->Start();
    DestroyStreams();
    DestroyCalls();
  });
}

TEST_P(VideoSendStreamTest, CanStopStoppedStream) {
  task_queue_.SendTask([this]() {
    CreateSenderCall();

    test::NullTransport transport;
    CreateSendConfig(1, 0, 0, &transport);
    CreateVideoStreams();
    GetVideoSendStream()->Stop();
    GetVideoSendStream()->Stop();
    DestroyStreams();
    DestroyCalls();
  });
}

TEST_P(VideoSendStreamTest, SupportsCName) {
  static std::string kCName = "PjQatC14dGfbVwGPUOA9IH7RlsFDbWl4AhXEiDsBizo=";
  class CNameObserver : public test::SendTest {
   public:
    CNameObserver() : SendTest(kDefaultTimeoutMs) {}

   private:
    Action OnSendRtcp(const uint8_t* packet, size_t length) override {
      test::RtcpPacketParser parser;
      EXPECT_TRUE(parser.Parse(packet, length));
      if (parser.sdes()->num_packets() > 0) {
        EXPECT_EQ(1u, parser.sdes()->chunks().size());
        EXPECT_EQ(kCName, parser.sdes()->chunks()[0].cname);

        observation_complete_.Set();
      }

      return SEND_PACKET;
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      send_config->rtp.c_name = kCName;
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait()) << "Timed out while waiting for RTCP with CNAME.";
    }
  } test;

  RunBaseTest(&test);
}

TEST_P(VideoSendStreamTest, SupportsAbsoluteSendTime) {
  class AbsoluteSendTimeObserver : public test::SendTest {
   public:
    AbsoluteSendTimeObserver() : SendTest(kDefaultTimeoutMs) {
      EXPECT_TRUE(parser_->RegisterRtpHeaderExtension(
          kRtpExtensionAbsoluteSendTime, test::kAbsSendTimeExtensionId));
    }

    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));

      EXPECT_FALSE(header.extension.hasTransmissionTimeOffset);
      EXPECT_TRUE(header.extension.hasAbsoluteSendTime);
      EXPECT_EQ(header.extension.transmissionTimeOffset, 0);
      if (header.extension.absoluteSendTime != 0) {
        // Wait for at least one packet with a non-zero send time. The send time
        // is a 16-bit value derived from the system clock, and it is valid
        // for a packet to have a zero send time. To tell that from an
        // unpopulated value we'll wait for a packet with non-zero send time.
        observation_complete_.Set();
      } else {
        RTC_LOG(LS_WARNING)
            << "Got a packet with zero absoluteSendTime, waiting"
               " for another packet...";
      }

      return SEND_PACKET;
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      send_config->rtp.extensions.clear();
      send_config->rtp.extensions.push_back(RtpExtension(
          RtpExtension::kAbsSendTimeUri, test::kAbsSendTimeExtensionId));
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait()) << "Timed out while waiting for single RTP packet.";
    }
  } test;

  RunBaseTest(&test);
}

TEST_P(VideoSendStreamTest, SupportsTransmissionTimeOffset) {
  static const int kEncodeDelayMs = 5;
  class TransmissionTimeOffsetObserver : public test::SendTest {
   public:
    TransmissionTimeOffsetObserver()
        : SendTest(kDefaultTimeoutMs), encoder_factory_([]() {
            return absl::make_unique<test::DelayedEncoder>(
                Clock::GetRealTimeClock(), kEncodeDelayMs);
          }) {
      EXPECT_TRUE(parser_->RegisterRtpHeaderExtension(
          kRtpExtensionTransmissionTimeOffset, test::kTOffsetExtensionId));
    }

   private:
    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));

      EXPECT_TRUE(header.extension.hasTransmissionTimeOffset);
      EXPECT_FALSE(header.extension.hasAbsoluteSendTime);
      EXPECT_GT(header.extension.transmissionTimeOffset, 0);
      EXPECT_EQ(header.extension.absoluteSendTime, 0u);
      observation_complete_.Set();

      return SEND_PACKET;
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      send_config->encoder_settings.encoder_factory = &encoder_factory_;
      send_config->rtp.extensions.clear();
      send_config->rtp.extensions.push_back(RtpExtension(
          RtpExtension::kTimestampOffsetUri, test::kTOffsetExtensionId));
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait()) << "Timed out while waiting for a single RTP packet.";
    }

    test::FunctionVideoEncoderFactory encoder_factory_;
  } test;

  RunBaseTest(&test);
}

TEST_P(VideoSendStreamTest, SupportsTransportWideSequenceNumbers) {
  static const uint8_t kExtensionId = test::kTransportSequenceNumberExtensionId;
  class TransportWideSequenceNumberObserver : public test::SendTest {
   public:
    TransportWideSequenceNumberObserver()
        : SendTest(kDefaultTimeoutMs), encoder_factory_([]() {
            return absl::make_unique<test::FakeEncoder>(
                Clock::GetRealTimeClock());
          }) {
      EXPECT_TRUE(parser_->RegisterRtpHeaderExtension(
          kRtpExtensionTransportSequenceNumber, kExtensionId));
    }

   private:
    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));

      EXPECT_TRUE(header.extension.hasTransportSequenceNumber);
      EXPECT_FALSE(header.extension.hasTransmissionTimeOffset);
      EXPECT_FALSE(header.extension.hasAbsoluteSendTime);

      observation_complete_.Set();

      return SEND_PACKET;
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      send_config->encoder_settings.encoder_factory = &encoder_factory_;
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait()) << "Timed out while waiting for a single RTP packet.";
    }

    test::FunctionVideoEncoderFactory encoder_factory_;
  } test;

  RunBaseTest(&test);
}

TEST_P(VideoSendStreamTest, SupportsVideoRotation) {
  class VideoRotationObserver : public test::SendTest {
   public:
    VideoRotationObserver() : SendTest(kDefaultTimeoutMs) {
      EXPECT_TRUE(parser_->RegisterRtpHeaderExtension(
          kRtpExtensionVideoRotation, test::kVideoRotationExtensionId));
    }

    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));
      // Only the last packet of the frame is required to have the extension.
      if (!header.markerBit)
        return SEND_PACKET;
      EXPECT_TRUE(header.extension.hasVideoRotation);
      EXPECT_EQ(kVideoRotation_90, header.extension.videoRotation);
      observation_complete_.Set();
      return SEND_PACKET;
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      send_config->rtp.extensions.clear();
      send_config->rtp.extensions.push_back(RtpExtension(
          RtpExtension::kVideoRotationUri, test::kVideoRotationExtensionId));
    }

    void OnFrameGeneratorCapturerCreated(
        test::FrameGeneratorCapturer* frame_generator_capturer) override {
      frame_generator_capturer->SetFakeRotation(kVideoRotation_90);
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait()) << "Timed out while waiting for single RTP packet.";
    }
  } test;

  RunBaseTest(&test);
}

TEST_P(VideoSendStreamTest, SupportsVideoContentType) {
  class VideoContentTypeObserver : public test::SendTest {
   public:
    VideoContentTypeObserver()
        : SendTest(kDefaultTimeoutMs), first_frame_sent_(false) {
      EXPECT_TRUE(parser_->RegisterRtpHeaderExtension(
          kRtpExtensionVideoContentType, test::kVideoContentTypeExtensionId));
    }

    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));
      // Only the last packet of the key-frame must have extension.
      if (!header.markerBit || first_frame_sent_)
        return SEND_PACKET;
      // First marker bit seen means that the first frame is sent.
      first_frame_sent_ = true;
      EXPECT_TRUE(header.extension.hasVideoContentType);
      EXPECT_TRUE(videocontenttypehelpers::IsScreenshare(
          header.extension.videoContentType));
      observation_complete_.Set();
      return SEND_PACKET;
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      send_config->rtp.extensions.clear();
      send_config->rtp.extensions.push_back(
          RtpExtension(RtpExtension::kVideoContentTypeUri,
                       test::kVideoContentTypeExtensionId));
      encoder_config->content_type = VideoEncoderConfig::ContentType::kScreen;
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait()) << "Timed out while waiting for single RTP packet.";
    }

   private:
    bool first_frame_sent_;
  } test;

  RunBaseTest(&test);
}

TEST_P(VideoSendStreamTest, SupportsVideoTimingFrames) {
  class VideoTimingObserver : public test::SendTest {
   public:
    VideoTimingObserver() : SendTest(kDefaultTimeoutMs) {
      EXPECT_TRUE(parser_->RegisterRtpHeaderExtension(
          kRtpExtensionVideoTiming, test::kVideoTimingExtensionId));
    }

    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));
      // Only the last packet of the frame must have extension.
      if (!header.markerBit)
        return SEND_PACKET;
      EXPECT_TRUE(header.extension.has_video_timing);
      observation_complete_.Set();
      return SEND_PACKET;
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      send_config->rtp.extensions.clear();
      send_config->rtp.extensions.push_back(RtpExtension(
          RtpExtension::kVideoTimingUri, test::kVideoTimingExtensionId));
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait()) << "Timed out while waiting for timing frames.";
    }
  } test;

  RunBaseTest(&test);
}

class FakeReceiveStatistics : public ReceiveStatisticsProvider {
 public:
  FakeReceiveStatistics(uint32_t send_ssrc,
                        uint32_t last_sequence_number,
                        uint32_t cumulative_lost,
                        uint8_t fraction_lost) {
    stat_.SetMediaSsrc(send_ssrc);
    stat_.SetExtHighestSeqNum(last_sequence_number);
    stat_.SetCumulativeLost(cumulative_lost);
    stat_.SetFractionLost(fraction_lost);
  }

  std::vector<rtcp::ReportBlock> RtcpReportBlocks(size_t max_blocks) override {
    EXPECT_GE(max_blocks, 1u);
    return {stat_};
  }

 private:
  rtcp::ReportBlock stat_;
};

class UlpfecObserver : public test::EndToEndTest {
 public:
  UlpfecObserver(bool header_extensions_enabled,
                 bool use_nack,
                 bool expect_red,
                 bool expect_ulpfec,
                 const std::string& codec,
                 VideoEncoderFactory* encoder_factory)
      : EndToEndTest(kTimeoutMs),
        encoder_factory_(encoder_factory),
        payload_name_(codec),
        use_nack_(use_nack),
        expect_red_(expect_red),
        expect_ulpfec_(expect_ulpfec),
        sent_media_(false),
        sent_ulpfec_(false),
        header_extensions_enabled_(header_extensions_enabled) {}

  // Some of the test cases are expected to time out and thus we are using
  // a shorter timeout window than the default here.
  static constexpr size_t kTimeoutMs = 10000;

 private:
  Action OnSendRtp(const uint8_t* packet, size_t length) override {
    RTPHeader header;
    EXPECT_TRUE(parser_->Parse(packet, length, &header));

    int encapsulated_payload_type = -1;
    if (header.payloadType == VideoSendStreamTest::kRedPayloadType) {
      EXPECT_TRUE(expect_red_);
      encapsulated_payload_type = static_cast<int>(packet[header.headerLength]);
      if (encapsulated_payload_type !=
          VideoSendStreamTest::kFakeVideoSendPayloadType) {
        EXPECT_EQ(VideoSendStreamTest::kUlpfecPayloadType,
                  encapsulated_payload_type);
      }
    } else {
      EXPECT_EQ(VideoSendStreamTest::kFakeVideoSendPayloadType,
                header.payloadType);
      if (static_cast<size_t>(header.headerLength + header.paddingLength) <
          length) {
        // Not padding-only, media received outside of RED.
        EXPECT_FALSE(expect_red_);
        sent_media_ = true;
      }
    }

    if (header_extensions_enabled_) {
      EXPECT_TRUE(header.extension.hasAbsoluteSendTime);
      uint32_t kHalf24BitsSpace = 0xFFFFFF / 2;
      if (header.extension.absoluteSendTime <= kHalf24BitsSpace &&
          prev_header_.extension.absoluteSendTime > kHalf24BitsSpace) {
        // 24 bits wrap.
        EXPECT_GT(prev_header_.extension.absoluteSendTime,
                  header.extension.absoluteSendTime);
      } else {
        EXPECT_GE(header.extension.absoluteSendTime,
                  prev_header_.extension.absoluteSendTime);
      }
      EXPECT_TRUE(header.extension.hasTransportSequenceNumber);
      uint16_t seq_num_diff = header.extension.transportSequenceNumber -
                              prev_header_.extension.transportSequenceNumber;
      EXPECT_EQ(1, seq_num_diff);
    }

    if (encapsulated_payload_type != -1) {
      if (encapsulated_payload_type ==
          VideoSendStreamTest::kUlpfecPayloadType) {
        EXPECT_TRUE(expect_ulpfec_);
        sent_ulpfec_ = true;
      } else {
        sent_media_ = true;
      }
    }

    if (sent_media_ && sent_ulpfec_) {
      observation_complete_.Set();
    }

    prev_header_ = header;

    return SEND_PACKET;
  }

  test::PacketTransport* CreateSendTransport(
      test::SingleThreadedTaskQueueForTesting* task_queue,
      Call* sender_call) override {
    // At low RTT (< kLowRttNackMs) -> NACK only, no FEC.
    // Configure some network delay.
    const int kNetworkDelayMs = 100;
    BuiltInNetworkBehaviorConfig config;
    config.loss_percent = 5;
    config.queue_delay_ms = kNetworkDelayMs;
    return new test::PacketTransport(
        task_queue, sender_call, this, test::PacketTransport::kSender,
        VideoSendStreamTest::payload_type_map_,
        absl::make_unique<FakeNetworkPipe>(
            Clock::GetRealTimeClock(),
            absl::make_unique<SimulatedNetwork>(config)));
  }

  void ModifyVideoConfigs(
      VideoSendStream::Config* send_config,
      std::vector<VideoReceiveStream::Config>* receive_configs,
      VideoEncoderConfig* encoder_config) override {
    if (use_nack_) {
      send_config->rtp.nack.rtp_history_ms =
          (*receive_configs)[0].rtp.nack.rtp_history_ms =
              VideoSendStreamTest::kNackRtpHistoryMs;
    }
    send_config->encoder_settings.encoder_factory = encoder_factory_;
    send_config->rtp.payload_name = payload_name_;
    send_config->rtp.ulpfec.red_payload_type =
        VideoSendStreamTest::kRedPayloadType;
    send_config->rtp.ulpfec.ulpfec_payload_type =
        VideoSendStreamTest::kUlpfecPayloadType;
    EXPECT_FALSE(send_config->rtp.extensions.empty());
    if (!header_extensions_enabled_) {
      send_config->rtp.extensions.clear();
    } else {
      send_config->rtp.extensions.push_back(RtpExtension(
          RtpExtension::kAbsSendTimeUri, test::kAbsSendTimeExtensionId));
    }
    encoder_config->codec_type = PayloadStringToCodecType(payload_name_);
    (*receive_configs)[0].rtp.red_payload_type =
        send_config->rtp.ulpfec.red_payload_type;
    (*receive_configs)[0].rtp.ulpfec_payload_type =
        send_config->rtp.ulpfec.ulpfec_payload_type;
  }

  void PerformTest() override {
    EXPECT_EQ(expect_ulpfec_, Wait())
        << "Timed out waiting for ULPFEC and/or media packets.";
  }

  VideoEncoderFactory* encoder_factory_;
  std::string payload_name_;
  const bool use_nack_;
  const bool expect_red_;
  const bool expect_ulpfec_;
  bool sent_media_;
  bool sent_ulpfec_;
  bool header_extensions_enabled_;
  RTPHeader prev_header_;
};

TEST_P(VideoSendStreamTest, SupportsUlpfecWithExtensions) {
  test::FunctionVideoEncoderFactory encoder_factory(
      []() { return VP8Encoder::Create(); });
  UlpfecObserver test(true, false, true, true, "VP8", &encoder_factory);
  RunBaseTest(&test);
}

TEST_P(VideoSendStreamTest, SupportsUlpfecWithoutExtensions) {
  test::FunctionVideoEncoderFactory encoder_factory(
      []() { return VP8Encoder::Create(); });
  UlpfecObserver test(false, false, true, true, "VP8", &encoder_factory);
  RunBaseTest(&test);
}

class VideoSendStreamWithoutUlpfecTest : public test::CallTest {
 protected:
  VideoSendStreamWithoutUlpfecTest()
      : field_trial_("WebRTC-DisableUlpFecExperiment/Enabled/") {}

  test::ScopedFieldTrials field_trial_;
};

TEST_F(VideoSendStreamWithoutUlpfecTest, NoUlpfecIfDisabledThroughFieldTrial) {
  test::FunctionVideoEncoderFactory encoder_factory(
      []() { return VP8Encoder::Create(); });
  UlpfecObserver test(false, false, false, false, "VP8", &encoder_factory);
  RunBaseTest(&test);
}

// The FEC scheme used is not efficient for H264, so we should not use RED/FEC
// since we'll still have to re-request FEC packets, effectively wasting
// bandwidth since the receiver has to wait for FEC retransmissions to determine
// that the received state is actually decodable.
TEST_P(VideoSendStreamTest, DoesNotUtilizeUlpfecForH264WithNackEnabled) {
  test::FunctionVideoEncoderFactory encoder_factory([]() {
    return absl::make_unique<test::FakeH264Encoder>(Clock::GetRealTimeClock());
  });
  UlpfecObserver test(false, true, false, false, "H264", &encoder_factory);
  RunBaseTest(&test);
}

// Without retransmissions FEC for H264 is fine.
TEST_P(VideoSendStreamTest, DoesUtilizeUlpfecForH264WithoutNackEnabled) {
  test::FunctionVideoEncoderFactory encoder_factory([]() {
    return absl::make_unique<test::FakeH264Encoder>(Clock::GetRealTimeClock());
  });
  UlpfecObserver test(false, false, true, true, "H264", &encoder_factory);
  RunBaseTest(&test);
}

// Disabled as flaky, see https://crbug.com/webrtc/7285 for details.
TEST_P(VideoSendStreamTest, DISABLED_DoesUtilizeUlpfecForVp8WithNackEnabled) {
  test::FunctionVideoEncoderFactory encoder_factory(
      []() { return VP8Encoder::Create(); });
  UlpfecObserver test(false, true, true, true, "VP8", &encoder_factory);
  RunBaseTest(&test);
}

#if defined(RTC_ENABLE_VP9)
// Disabled as flaky, see https://crbug.com/webrtc/7285 for details.
TEST_P(VideoSendStreamTest, DISABLED_DoesUtilizeUlpfecForVp9WithNackEnabled) {
  test::FunctionVideoEncoderFactory encoder_factory(
      []() { return VP9Encoder::Create(); });
  UlpfecObserver test(false, true, true, true, "VP9", &encoder_factory);
  RunBaseTest(&test);
}
#endif  // defined(RTC_ENABLE_VP9)

TEST_P(VideoSendStreamTest, SupportsUlpfecWithMultithreadedH264) {
  test::FunctionVideoEncoderFactory encoder_factory([]() {
    return absl::make_unique<test::MultithreadedFakeH264Encoder>(
        Clock::GetRealTimeClock());
  });
  UlpfecObserver test(false, false, true, true, "H264", &encoder_factory);
  RunBaseTest(&test);
}

// TODO(brandtr): Move these FlexFEC tests when we have created
// FlexfecSendStream.
class FlexfecObserver : public test::EndToEndTest {
 public:
  FlexfecObserver(bool header_extensions_enabled,
                  bool use_nack,
                  const std::string& codec,
                  VideoEncoderFactory* encoder_factory,
                  size_t num_video_streams)
      : EndToEndTest(VideoSendStreamTest::kDefaultTimeoutMs),
        encoder_factory_(encoder_factory),
        payload_name_(codec),
        use_nack_(use_nack),
        sent_media_(false),
        sent_flexfec_(false),
        header_extensions_enabled_(header_extensions_enabled),
        num_video_streams_(num_video_streams) {}

  size_t GetNumFlexfecStreams() const override { return 1; }
  size_t GetNumVideoStreams() const override { return num_video_streams_; }

 private:
  Action OnSendRtp(const uint8_t* packet, size_t length) override {
    RTPHeader header;
    EXPECT_TRUE(parser_->Parse(packet, length, &header));

    if (header.payloadType == VideoSendStreamTest::kFlexfecPayloadType) {
      EXPECT_EQ(VideoSendStreamTest::kFlexfecSendSsrc, header.ssrc);
      sent_flexfec_ = true;
    } else {
      EXPECT_EQ(VideoSendStreamTest::kFakeVideoSendPayloadType,
                header.payloadType);
      EXPECT_THAT(testing::make_tuple(VideoSendStreamTest::kVideoSendSsrcs,
                                      num_video_streams_),
                  testing::Contains(header.ssrc));
      sent_media_ = true;
    }

    if (header_extensions_enabled_) {
      EXPECT_TRUE(header.extension.hasAbsoluteSendTime);
      EXPECT_TRUE(header.extension.hasTransmissionTimeOffset);
      EXPECT_TRUE(header.extension.hasTransportSequenceNumber);
    }

    if (sent_media_ && sent_flexfec_) {
      observation_complete_.Set();
    }

    return SEND_PACKET;
  }

  test::PacketTransport* CreateSendTransport(
      test::SingleThreadedTaskQueueForTesting* task_queue,
      Call* sender_call) override {
    // At low RTT (< kLowRttNackMs) -> NACK only, no FEC.
    // Therefore we need some network delay.
    const int kNetworkDelayMs = 100;
    BuiltInNetworkBehaviorConfig config;
    config.loss_percent = 5;
    config.queue_delay_ms = kNetworkDelayMs;
    return new test::PacketTransport(
        task_queue, sender_call, this, test::PacketTransport::kSender,
        VideoSendStreamTest::payload_type_map_,
        absl::make_unique<FakeNetworkPipe>(
            Clock::GetRealTimeClock(),
            absl::make_unique<SimulatedNetwork>(config)));
  }

  void ModifyVideoConfigs(
      VideoSendStream::Config* send_config,
      std::vector<VideoReceiveStream::Config>* receive_configs,
      VideoEncoderConfig* encoder_config) override {
    if (use_nack_) {
      send_config->rtp.nack.rtp_history_ms =
          (*receive_configs)[0].rtp.nack.rtp_history_ms =
              VideoSendStreamTest::kNackRtpHistoryMs;
    }
    send_config->encoder_settings.encoder_factory = encoder_factory_;
    send_config->rtp.payload_name = payload_name_;
    if (header_extensions_enabled_) {
      send_config->rtp.extensions.push_back(RtpExtension(
          RtpExtension::kAbsSendTimeUri, test::kAbsSendTimeExtensionId));
      send_config->rtp.extensions.push_back(RtpExtension(
          RtpExtension::kTimestampOffsetUri, test::kTOffsetExtensionId));
    } else {
      send_config->rtp.extensions.clear();
    }
    encoder_config->codec_type = PayloadStringToCodecType(payload_name_);
  }

  void PerformTest() override {
    EXPECT_TRUE(Wait())
        << "Timed out waiting for FlexFEC and/or media packets.";
  }

  VideoEncoderFactory* encoder_factory_;
  std::string payload_name_;
  const bool use_nack_;
  bool sent_media_;
  bool sent_flexfec_;
  const bool header_extensions_enabled_;
  const size_t num_video_streams_;
};

TEST_P(VideoSendStreamTest, SupportsFlexfecVp8) {
  test::FunctionVideoEncoderFactory encoder_factory(
      []() { return VP8Encoder::Create(); });
  FlexfecObserver test(false, false, "VP8", &encoder_factory, 1);
  RunBaseTest(&test);
}

TEST_P(VideoSendStreamTest, SupportsFlexfecSimulcastVp8) {
  test::FunctionVideoEncoderFactory encoder_factory(
      []() { return VP8Encoder::Create(); });
  FlexfecObserver test(false, false, "VP8", &encoder_factory, 2);
  RunBaseTest(&test);
}

TEST_P(VideoSendStreamTest, SupportsFlexfecWithNackVp8) {
  test::FunctionVideoEncoderFactory encoder_factory(
      []() { return VP8Encoder::Create(); });
  FlexfecObserver test(false, true, "VP8", &encoder_factory, 1);
  RunBaseTest(&test);
}

TEST_P(VideoSendStreamTest, SupportsFlexfecWithRtpExtensionsVp8) {
  test::FunctionVideoEncoderFactory encoder_factory(
      []() { return VP8Encoder::Create(); });
  FlexfecObserver test(true, false, "VP8", &encoder_factory, 1);
  RunBaseTest(&test);
}

#if defined(RTC_ENABLE_VP9)
TEST_P(VideoSendStreamTest, SupportsFlexfecVp9) {
  test::FunctionVideoEncoderFactory encoder_factory(
      []() { return VP9Encoder::Create(); });
  FlexfecObserver test(false, false, "VP9", &encoder_factory, 1);
  RunBaseTest(&test);
}

TEST_P(VideoSendStreamTest, SupportsFlexfecWithNackVp9) {
  test::FunctionVideoEncoderFactory encoder_factory(
      []() { return VP9Encoder::Create(); });
  FlexfecObserver test(false, true, "VP9", &encoder_factory, 1);
  RunBaseTest(&test);
}
#endif  // defined(RTC_ENABLE_VP9)

TEST_P(VideoSendStreamTest, SupportsFlexfecH264) {
  test::FunctionVideoEncoderFactory encoder_factory([]() {
    return absl::make_unique<test::FakeH264Encoder>(Clock::GetRealTimeClock());
  });
  FlexfecObserver test(false, false, "H264", &encoder_factory, 1);
  RunBaseTest(&test);
}

TEST_P(VideoSendStreamTest, SupportsFlexfecWithNackH264) {
  test::FunctionVideoEncoderFactory encoder_factory([]() {
    return absl::make_unique<test::FakeH264Encoder>(Clock::GetRealTimeClock());
  });
  FlexfecObserver test(false, true, "H264", &encoder_factory, 1);
  RunBaseTest(&test);
}

TEST_P(VideoSendStreamTest, SupportsFlexfecWithMultithreadedH264) {
  test::FunctionVideoEncoderFactory encoder_factory([]() {
    return absl::make_unique<test::MultithreadedFakeH264Encoder>(
        Clock::GetRealTimeClock());
  });

  FlexfecObserver test(false, false, "H264", &encoder_factory, 1);
  RunBaseTest(&test);
}

void VideoSendStreamTest::TestNackRetransmission(
    uint32_t retransmit_ssrc,
    uint8_t retransmit_payload_type) {
  class NackObserver : public test::SendTest {
   public:
    explicit NackObserver(uint32_t retransmit_ssrc,
                          uint8_t retransmit_payload_type)
        : SendTest(kDefaultTimeoutMs),
          send_count_(0),
          retransmit_count_(0),
          retransmit_ssrc_(retransmit_ssrc),
          retransmit_payload_type_(retransmit_payload_type) {}

   private:
    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));

      // NACK packets two times at some arbitrary points.
      const int kNackedPacketsAtOnceCount = 3;
      const int kRetransmitTarget = kNackedPacketsAtOnceCount * 2;

      // Skip padding packets because they will never be retransmitted.
      if (header.paddingLength + header.headerLength == length) {
        return SEND_PACKET;
      }

      ++send_count_;

      // NACK packets at arbitrary points.
      if (send_count_ == 5 || send_count_ == 25) {
        nacked_sequence_numbers_.insert(
            nacked_sequence_numbers_.end(),
            non_padding_sequence_numbers_.end() - kNackedPacketsAtOnceCount,
            non_padding_sequence_numbers_.end());

        RTCPSender rtcp_sender(false, Clock::GetRealTimeClock(), nullptr,
                               nullptr, nullptr, transport_adapter_.get(),
                               kRtcpIntervalMs);

        rtcp_sender.SetRTCPStatus(RtcpMode::kReducedSize);
        rtcp_sender.SetRemoteSSRC(kVideoSendSsrcs[0]);

        RTCPSender::FeedbackState feedback_state;

        EXPECT_EQ(0, rtcp_sender.SendRTCP(
                         feedback_state, kRtcpNack,
                         static_cast<int>(nacked_sequence_numbers_.size()),
                         &nacked_sequence_numbers_.front()));
      }

      uint16_t sequence_number = header.sequenceNumber;
      if (header.ssrc == retransmit_ssrc_ &&
          retransmit_ssrc_ != kVideoSendSsrcs[0]) {
        // Not kVideoSendSsrcs[0], assume correct RTX packet. Extract sequence
        // number.
        const uint8_t* rtx_header = packet + header.headerLength;
        sequence_number = (rtx_header[0] << 8) + rtx_header[1];
      }

      auto found = std::find(nacked_sequence_numbers_.begin(),
                             nacked_sequence_numbers_.end(), sequence_number);
      if (found != nacked_sequence_numbers_.end()) {
        nacked_sequence_numbers_.erase(found);

        if (++retransmit_count_ == kRetransmitTarget) {
          EXPECT_EQ(retransmit_ssrc_, header.ssrc);
          EXPECT_EQ(retransmit_payload_type_, header.payloadType);
          observation_complete_.Set();
        }
      } else {
        non_padding_sequence_numbers_.push_back(sequence_number);
      }

      return SEND_PACKET;
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      transport_adapter_.reset(
          new internal::TransportAdapter(send_config->send_transport));
      transport_adapter_->Enable();
      send_config->rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
      send_config->rtp.rtx.payload_type = retransmit_payload_type_;
      if (retransmit_ssrc_ != kVideoSendSsrcs[0])
        send_config->rtp.rtx.ssrcs.push_back(retransmit_ssrc_);
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait()) << "Timed out while waiting for NACK retransmission.";
    }

    std::unique_ptr<internal::TransportAdapter> transport_adapter_;
    int send_count_;
    int retransmit_count_;
    uint32_t retransmit_ssrc_;
    uint8_t retransmit_payload_type_;
    std::vector<uint16_t> nacked_sequence_numbers_;
    std::vector<uint16_t> non_padding_sequence_numbers_;
  } test(retransmit_ssrc, retransmit_payload_type);

  RunBaseTest(&test);
}

TEST_P(VideoSendStreamTest, RetransmitsNack) {
  // Normal NACKs should use the send SSRC.
  TestNackRetransmission(kVideoSendSsrcs[0], kFakeVideoSendPayloadType);
}

TEST_P(VideoSendStreamTest, RetransmitsNackOverRtx) {
  // NACKs over RTX should use a separate SSRC.
  TestNackRetransmission(kSendRtxSsrcs[0], kSendRtxPayloadType);
}

void VideoSendStreamTest::TestPacketFragmentationSize(VideoFormat format,
                                                      bool with_fec) {
  // Use a fake encoder to output a frame of every size in the range [90, 290],
  // for each size making sure that the exact number of payload bytes received
  // is correct and that packets are fragmented to respect max packet size.
  static const size_t kMaxPacketSize = 128;
  static const size_t start = 90;
  static const size_t stop = 290;

  // Observer that verifies that the expected number of packets and bytes
  // arrive for each frame size, from start_size to stop_size.
  class FrameFragmentationTest : public test::SendTest {
   public:
    FrameFragmentationTest(size_t max_packet_size,
                           size_t start_size,
                           size_t stop_size,
                           bool test_generic_packetization,
                           bool use_fec)
        : SendTest(kLongTimeoutMs),
          encoder_(stop),
          encoder_factory_(&encoder_),
          max_packet_size_(max_packet_size),
          stop_size_(stop_size),
          test_generic_packetization_(test_generic_packetization),
          use_fec_(use_fec),
          packet_count_(0),
          packets_lost_(0),
          last_packet_count_(0),
          last_packets_lost_(0),
          accumulated_size_(0),
          accumulated_payload_(0),
          fec_packet_received_(false),
          current_size_rtp_(start_size),
          current_size_frame_(static_cast<int>(start_size)) {
      // Fragmentation required, this test doesn't make sense without it.
      encoder_.SetFrameSize(start_size);
      RTC_DCHECK_GT(stop_size, max_packet_size);
      if (!test_generic_packetization_)
        encoder_.SetCodecType(kVideoCodecVP8);
    }

   private:
    Action OnSendRtp(const uint8_t* packet, size_t size) override {
      size_t length = size;
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));

      EXPECT_LE(length, max_packet_size_);

      if (use_fec_) {
        uint8_t payload_type = packet[header.headerLength];
        bool is_fec = header.payloadType == kRedPayloadType &&
                      payload_type == kUlpfecPayloadType;
        if (is_fec) {
          fec_packet_received_ = true;
          return SEND_PACKET;
        }
      }

      accumulated_size_ += length;

      if (use_fec_)
        TriggerLossReport(header);

      if (test_generic_packetization_) {
        size_t overhead = header.headerLength + header.paddingLength;
        // Only remove payload header and RED header if the packet actually
        // contains payload.
        if (length > overhead) {
          overhead += (1 /* Generic header */);
          if (use_fec_)
            overhead += 1;  // RED for FEC header.
        }
        EXPECT_GE(length, overhead);
        accumulated_payload_ += length - overhead;
      }

      // Marker bit set indicates last packet of a frame.
      if (header.markerBit) {
        if (use_fec_ && accumulated_payload_ == current_size_rtp_ - 1) {
          // With FEC enabled, frame size is incremented asynchronously, so
          // "old" frames one byte too small may arrive. Accept, but don't
          // increase expected frame size.
          accumulated_size_ = 0;
          accumulated_payload_ = 0;
          return SEND_PACKET;
        }

        EXPECT_GE(accumulated_size_, current_size_rtp_);
        if (test_generic_packetization_) {
          EXPECT_EQ(current_size_rtp_, accumulated_payload_);
        }

        // Last packet of frame; reset counters.
        accumulated_size_ = 0;
        accumulated_payload_ = 0;
        if (current_size_rtp_ == stop_size_) {
          // Done! (Don't increase size again, might arrive more @ stop_size).
          observation_complete_.Set();
        } else {
          // Increase next expected frame size. If testing with FEC, make sure
          // a FEC packet has been received for this frame size before
          // proceeding, to make sure that redundancy packets don't exceed
          // size limit.
          if (!use_fec_) {
            ++current_size_rtp_;
          } else if (fec_packet_received_) {
            fec_packet_received_ = false;
            ++current_size_rtp_;

            rtc::CritScope lock(&mutex_);
            ++current_size_frame_;
          }
        }
      }

      return SEND_PACKET;
    }

    void TriggerLossReport(const RTPHeader& header) {
      // Send lossy receive reports to trigger FEC enabling.
      const int kLossPercent = 5;
      if (++packet_count_ % (100 / kLossPercent) == 0) {
        packets_lost_++;
        int loss_delta = packets_lost_ - last_packets_lost_;
        int packets_delta = packet_count_ - last_packet_count_;
        last_packet_count_ = packet_count_;
        last_packets_lost_ = packets_lost_;
        uint8_t loss_ratio =
            static_cast<uint8_t>(loss_delta * 255 / packets_delta);
        FakeReceiveStatistics lossy_receive_stats(
            kVideoSendSsrcs[0], header.sequenceNumber,
            packets_lost_,  // Cumulative lost.
            loss_ratio);    // Loss percent.
        RTCPSender rtcp_sender(false, Clock::GetRealTimeClock(),
                               &lossy_receive_stats, nullptr, nullptr,
                               transport_adapter_.get(), kRtcpIntervalMs);

        rtcp_sender.SetRTCPStatus(RtcpMode::kReducedSize);
        rtcp_sender.SetRemoteSSRC(kVideoSendSsrcs[0]);

        RTCPSender::FeedbackState feedback_state;

        EXPECT_EQ(0, rtcp_sender.SendRTCP(feedback_state, kRtcpRr));
      }
    }

    void UpdateConfiguration() {
      rtc::CritScope lock(&mutex_);
      // Increase frame size for next encoded frame, in the context of the
      // encoder thread.
      if (!use_fec_ && current_size_frame_ < static_cast<int32_t>(stop_size_)) {
        ++current_size_frame_;
      }
      encoder_.SetFrameSize(static_cast<size_t>(current_size_frame_));
    }
    void ModifySenderBitrateConfig(
        BitrateConstraints* bitrate_config) override {
      const int kMinBitrateBps = 30000;
      bitrate_config->min_bitrate_bps = kMinBitrateBps;
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      transport_adapter_.reset(
          new internal::TransportAdapter(send_config->send_transport));
      transport_adapter_->Enable();
      if (use_fec_) {
        send_config->rtp.ulpfec.red_payload_type = kRedPayloadType;
        send_config->rtp.ulpfec.ulpfec_payload_type = kUlpfecPayloadType;
      }

      if (!test_generic_packetization_)
        send_config->rtp.payload_name = "VP8";

      send_config->encoder_settings.encoder_factory = &encoder_factory_;
      send_config->rtp.max_packet_size = kMaxPacketSize;
      encoder_.RegisterPostEncodeCallback([this]() { UpdateConfiguration(); });

      // Make sure there is at least one extension header, to make the RTP
      // header larger than the base length of 12 bytes.
      EXPECT_FALSE(send_config->rtp.extensions.empty());

      // Setup screen content disables frame dropping which makes this easier.
      EXPECT_EQ(1u, encoder_config->simulcast_layers.size());
      encoder_config->simulcast_layers[0].num_temporal_layers = 2;
      encoder_config->content_type = VideoEncoderConfig::ContentType::kScreen;
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait()) << "Timed out while observing incoming RTP packets.";
    }

    std::unique_ptr<internal::TransportAdapter> transport_adapter_;
    test::ConfigurableFrameSizeEncoder encoder_;
    test::VideoEncoderProxyFactory encoder_factory_;

    const size_t max_packet_size_;
    const size_t stop_size_;
    const bool test_generic_packetization_;
    const bool use_fec_;

    uint32_t packet_count_;
    uint32_t packets_lost_;
    uint32_t last_packet_count_;
    uint32_t last_packets_lost_;
    size_t accumulated_size_;
    size_t accumulated_payload_;
    bool fec_packet_received_;

    size_t current_size_rtp_;
    rtc::CriticalSection mutex_;
    int current_size_frame_ RTC_GUARDED_BY(mutex_);
  };

  // Don't auto increment if FEC is used; continue sending frame size until
  // a FEC packet has been received.
  FrameFragmentationTest test(kMaxPacketSize, start, stop, format == kGeneric,
                              with_fec);

  RunBaseTest(&test);
}

// TODO(sprang): Is there any way of speeding up these tests?
TEST_P(VideoSendStreamTest, FragmentsGenericAccordingToMaxPacketSize) {
  TestPacketFragmentationSize(kGeneric, false);
}

TEST_P(VideoSendStreamTest, FragmentsGenericAccordingToMaxPacketSizeWithFec) {
  TestPacketFragmentationSize(kGeneric, true);
}

TEST_P(VideoSendStreamTest, FragmentsVp8AccordingToMaxPacketSize) {
  TestPacketFragmentationSize(kVP8, false);
}

TEST_P(VideoSendStreamTest, FragmentsVp8AccordingToMaxPacketSizeWithFec) {
  TestPacketFragmentationSize(kVP8, true);
}

// The test will go through a number of phases.
// 1. Start sending packets.
// 2. As soon as the RTP stream has been detected, signal a low REMB value to
//    suspend the stream.
// 3. Wait until |kSuspendTimeFrames| have been captured without seeing any RTP
//    packets.
// 4. Signal a high REMB and then wait for the RTP stream to start again.
//    When the stream is detected again, and the stats show that the stream
//    is no longer suspended, the test ends.
TEST_P(VideoSendStreamTest, SuspendBelowMinBitrate) {
  static const int kSuspendTimeFrames = 60;  // Suspend for 2 seconds @ 30 fps.

  class RembObserver : public test::SendTest,
                       public rtc::VideoSinkInterface<VideoFrame> {
   public:
    RembObserver()
        : SendTest(kDefaultTimeoutMs),
          clock_(Clock::GetRealTimeClock()),
          stream_(nullptr),
          test_state_(kBeforeSuspend),
          rtp_count_(0),
          last_sequence_number_(0),
          suspended_frame_count_(0),
          low_remb_bps_(0),
          high_remb_bps_(0) {}

   private:
    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      rtc::CritScope lock(&crit_);
      ++rtp_count_;
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));
      last_sequence_number_ = header.sequenceNumber;

      if (test_state_ == kBeforeSuspend) {
        // The stream has started. Try to suspend it.
        SendRtcpFeedback(low_remb_bps_);
        test_state_ = kDuringSuspend;
      } else if (test_state_ == kDuringSuspend) {
        if (header.paddingLength == 0) {
          // Received non-padding packet during suspension period. Reset the
          // counter.
          suspended_frame_count_ = 0;
        }
        SendRtcpFeedback(0);  // REMB is only sent if value is > 0.
      } else if (test_state_ == kWaitingForPacket) {
        if (header.paddingLength == 0) {
          // Non-padding packet observed. Test is almost complete. Will just
          // have to wait for the stats to change.
          test_state_ = kWaitingForStats;
        }
        SendRtcpFeedback(0);  // REMB is only sent if value is > 0.
      } else if (test_state_ == kWaitingForStats) {
        VideoSendStream::Stats stats = stream_->GetStats();
        if (stats.suspended == false) {
          // Stats flipped to false. Test is complete.
          observation_complete_.Set();
        }
        SendRtcpFeedback(0);  // REMB is only sent if value is > 0.
      }

      return SEND_PACKET;
    }

    // This method implements the rtc::VideoSinkInterface. This is called when
    // a frame is provided to the VideoSendStream.
    void OnFrame(const VideoFrame& video_frame) override {
      rtc::CritScope lock(&crit_);
      if (test_state_ == kDuringSuspend &&
          ++suspended_frame_count_ > kSuspendTimeFrames) {
        VideoSendStream::Stats stats = stream_->GetStats();
        EXPECT_TRUE(stats.suspended);
        SendRtcpFeedback(high_remb_bps_);
        test_state_ = kWaitingForPacket;
      }
    }

    void set_low_remb_bps(int value) {
      rtc::CritScope lock(&crit_);
      low_remb_bps_ = value;
    }

    void set_high_remb_bps(int value) {
      rtc::CritScope lock(&crit_);
      high_remb_bps_ = value;
    }

    void OnVideoStreamsCreated(
        VideoSendStream* send_stream,
        const std::vector<VideoReceiveStream*>& receive_streams) override {
      stream_ = send_stream;
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      RTC_DCHECK_EQ(1, encoder_config->number_of_streams);
      transport_adapter_.reset(
          new internal::TransportAdapter(send_config->send_transport));
      transport_adapter_->Enable();
      send_config->rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
      send_config->pre_encode_callback = this;
      send_config->suspend_below_min_bitrate = true;
      int min_bitrate_bps =
          test::DefaultVideoStreamFactory::kDefaultMinBitratePerStream[0];
      set_low_remb_bps(min_bitrate_bps - 10000);
      int threshold_window = std::max(min_bitrate_bps / 10, 20000);
      ASSERT_GT(encoder_config->max_bitrate_bps,
                min_bitrate_bps + threshold_window + 5000);
      set_high_remb_bps(min_bitrate_bps + threshold_window + 5000);
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait()) << "Timed out during suspend-below-min-bitrate test.";
    }

    enum TestState {
      kBeforeSuspend,
      kDuringSuspend,
      kWaitingForPacket,
      kWaitingForStats
    };

    virtual void SendRtcpFeedback(int remb_value)
        RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_) {
      FakeReceiveStatistics receive_stats(kVideoSendSsrcs[0],
                                          last_sequence_number_, rtp_count_, 0);
      RTCPSender rtcp_sender(false, clock_, &receive_stats, nullptr, nullptr,
                             transport_adapter_.get(), kRtcpIntervalMs);

      rtcp_sender.SetRTCPStatus(RtcpMode::kReducedSize);
      rtcp_sender.SetRemoteSSRC(kVideoSendSsrcs[0]);
      if (remb_value > 0) {
        rtcp_sender.SetRemb(remb_value, std::vector<uint32_t>());
      }
      RTCPSender::FeedbackState feedback_state;
      EXPECT_EQ(0, rtcp_sender.SendRTCP(feedback_state, kRtcpRr));
    }

    std::unique_ptr<internal::TransportAdapter> transport_adapter_;
    Clock* const clock_;
    VideoSendStream* stream_;

    rtc::CriticalSection crit_;
    TestState test_state_ RTC_GUARDED_BY(crit_);
    int rtp_count_ RTC_GUARDED_BY(crit_);
    int last_sequence_number_ RTC_GUARDED_BY(crit_);
    int suspended_frame_count_ RTC_GUARDED_BY(crit_);
    int low_remb_bps_ RTC_GUARDED_BY(crit_);
    int high_remb_bps_ RTC_GUARDED_BY(crit_);
  } test;

  RunBaseTest(&test);
}

// This test that padding stops being send after a while if the Camera stops
// producing video frames and that padding resumes if the camera restarts.
TEST_P(VideoSendStreamTest, NoPaddingWhenVideoIsMuted) {
  class NoPaddingWhenVideoIsMuted : public test::SendTest {
   public:
    NoPaddingWhenVideoIsMuted()
        : SendTest(kDefaultTimeoutMs),
          clock_(Clock::GetRealTimeClock()),
          last_packet_time_ms_(-1),
          capturer_(nullptr) {}

   private:
    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      rtc::CritScope lock(&crit_);
      last_packet_time_ms_ = clock_->TimeInMilliseconds();

      RTPHeader header;
      parser_->Parse(packet, length, &header);
      const bool only_padding =
          header.headerLength + header.paddingLength == length;

      if (test_state_ == kBeforeStopCapture) {
        // Packets are flowing, stop camera.
        capturer_->Stop();
        test_state_ = kWaitingForPadding;
      } else if (test_state_ == kWaitingForPadding && only_padding) {
        // We're still getting padding, after stopping camera.
        test_state_ = kWaitingForNoPackets;
      } else if (test_state_ == kWaitingForMediaAfterCameraRestart &&
                 !only_padding) {
        // Media packets are flowing again, stop camera a second time.
        capturer_->Stop();
        test_state_ = kWaitingForPaddingAfterCameraStopsAgain;
      } else if (test_state_ == kWaitingForPaddingAfterCameraStopsAgain &&
                 only_padding) {
        // Padding is still flowing, test ok.
        observation_complete_.Set();
      }
      return SEND_PACKET;
    }

    Action OnSendRtcp(const uint8_t* packet, size_t length) override {
      rtc::CritScope lock(&crit_);
      const int kNoPacketsThresholdMs = 2000;
      if (test_state_ == kWaitingForNoPackets &&
          (last_packet_time_ms_ > 0 &&
           clock_->TimeInMilliseconds() - last_packet_time_ms_ >
               kNoPacketsThresholdMs)) {
        // No packets seen for |kNoPacketsThresholdMs|, restart camera.
        capturer_->Start();
        test_state_ = kWaitingForMediaAfterCameraRestart;
      }
      return SEND_PACKET;
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      // Make sure padding is sent if encoder is not producing media.
      encoder_config->min_transmit_bitrate_bps = 50000;
    }

    void OnFrameGeneratorCapturerCreated(
        test::FrameGeneratorCapturer* frame_generator_capturer) override {
      rtc::CritScope lock(&crit_);
      capturer_ = frame_generator_capturer;
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait())
          << "Timed out while waiting for RTP packets to stop being sent.";
    }

    enum TestState {
      kBeforeStopCapture,
      kWaitingForPadding,
      kWaitingForNoPackets,
      kWaitingForMediaAfterCameraRestart,
      kWaitingForPaddingAfterCameraStopsAgain
    };

    TestState test_state_ = kBeforeStopCapture;
    Clock* const clock_;
    std::unique_ptr<internal::TransportAdapter> transport_adapter_;
    rtc::CriticalSection crit_;
    int64_t last_packet_time_ms_ RTC_GUARDED_BY(crit_);
    test::FrameGeneratorCapturer* capturer_ RTC_GUARDED_BY(crit_);
  } test;

  RunBaseTest(&test);
}

TEST_P(VideoSendStreamTest, PaddingIsPrimarilyRetransmissions) {
  const int kCapacityKbps = 10000;  // 10 Mbps
  class PaddingIsPrimarilyRetransmissions : public test::EndToEndTest {
   public:
    PaddingIsPrimarilyRetransmissions()
        : EndToEndTest(kDefaultTimeoutMs),
          clock_(Clock::GetRealTimeClock()),
          padding_length_(0),
          total_length_(0),
          call_(nullptr) {}

   private:
    void OnCallsCreated(Call* sender_call, Call* receiver_call) override {
      call_ = sender_call;
    }

    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      rtc::CritScope lock(&crit_);

      RTPHeader header;
      parser_->Parse(packet, length, &header);
      padding_length_ += header.paddingLength;
      total_length_ += length;
      return SEND_PACKET;
    }

    test::PacketTransport* CreateSendTransport(
        test::SingleThreadedTaskQueueForTesting* task_queue,
        Call* sender_call) override {
      const int kNetworkDelayMs = 50;
      BuiltInNetworkBehaviorConfig config;
      config.loss_percent = 10;
      config.link_capacity_kbps = kCapacityKbps;
      config.queue_delay_ms = kNetworkDelayMs;
      return new test::PacketTransport(
          task_queue, sender_call, this, test::PacketTransport::kSender,
          payload_type_map_,
          absl::make_unique<FakeNetworkPipe>(
              Clock::GetRealTimeClock(),
              absl::make_unique<SimulatedNetwork>(config)));
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      // Turn on RTX.
      send_config->rtp.rtx.payload_type = kFakeVideoSendPayloadType;
      send_config->rtp.rtx.ssrcs.push_back(kVideoSendSsrcs[0]);
    }

    void PerformTest() override {
      // TODO(isheriff): Some platforms do not ramp up as expected to full
      // capacity due to packet scheduling delays. Fix that before getting
      // rid of this.
      SleepMs(5000);
      {
        rtc::CritScope lock(&crit_);
        // Expect padding to be a small percentage of total bytes sent.
        EXPECT_LT(padding_length_, .1 * total_length_);
      }
    }

    rtc::CriticalSection crit_;
    Clock* const clock_;
    size_t padding_length_ RTC_GUARDED_BY(crit_);
    size_t total_length_ RTC_GUARDED_BY(crit_);
    Call* call_;
  } test;

  RunBaseTest(&test);
}

// This test first observes "high" bitrate use at which point it sends a REMB to
// indicate that it should be lowered significantly. The test then observes that
// the bitrate observed is sinking well below the min-transmit-bitrate threshold
// to verify that the min-transmit bitrate respects incoming REMB.
//
// Note that the test starts at "high" bitrate and does not ramp up to "higher"
// bitrate since no receiver block or remb is sent in the initial phase.
TEST_P(VideoSendStreamTest, MinTransmitBitrateRespectsRemb) {
  static const int kMinTransmitBitrateBps = 400000;
  static const int kHighBitrateBps = 150000;
  static const int kRembBitrateBps = 80000;
  static const int kRembRespectedBitrateBps = 100000;
  class BitrateObserver : public test::SendTest {
   public:
    BitrateObserver()
        : SendTest(kDefaultTimeoutMs),
          retranmission_rate_limiter_(Clock::GetRealTimeClock(), 1000),
          stream_(nullptr),
          bitrate_capped_(false) {}

   private:
    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      if (RtpHeaderParser::IsRtcp(packet, length))
        return DROP_PACKET;

      RTPHeader header;
      if (!parser_->Parse(packet, length, &header))
        return DROP_PACKET;
      RTC_DCHECK(stream_);
      VideoSendStream::Stats stats = stream_->GetStats();
      if (!stats.substreams.empty()) {
        EXPECT_EQ(1u, stats.substreams.size());
        int total_bitrate_bps =
            stats.substreams.begin()->second.total_bitrate_bps;
        test::PrintResult("bitrate_stats_", "min_transmit_bitrate_low_remb",
                          "bitrate_bps", static_cast<size_t>(total_bitrate_bps),
                          "bps", false);
        if (total_bitrate_bps > kHighBitrateBps) {
          rtp_rtcp_->SetRemb(kRembBitrateBps,
                             std::vector<uint32_t>(1, header.ssrc));
          rtp_rtcp_->Process();
          bitrate_capped_ = true;
        } else if (bitrate_capped_ &&
                   total_bitrate_bps < kRembRespectedBitrateBps) {
          observation_complete_.Set();
        }
      }
      // Packets don't have to be delivered since the test is the receiver.
      return DROP_PACKET;
    }

    void OnVideoStreamsCreated(
        VideoSendStream* send_stream,
        const std::vector<VideoReceiveStream*>& receive_streams) override {
      stream_ = send_stream;
      RtpRtcp::Configuration config;
      config.outgoing_transport = feedback_transport_.get();
      config.retransmission_rate_limiter = &retranmission_rate_limiter_;
      rtp_rtcp_.reset(RtpRtcp::CreateRtpRtcp(config));
      rtp_rtcp_->SetRTCPStatus(RtcpMode::kReducedSize);
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      feedback_transport_.reset(
          new internal::TransportAdapter(send_config->send_transport));
      feedback_transport_->Enable();
      encoder_config->min_transmit_bitrate_bps = kMinTransmitBitrateBps;
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait())
          << "Timeout while waiting for low bitrate stats after REMB.";
    }

    std::unique_ptr<RtpRtcp> rtp_rtcp_;
    std::unique_ptr<internal::TransportAdapter> feedback_transport_;
    RateLimiter retranmission_rate_limiter_;
    VideoSendStream* stream_;
    bool bitrate_capped_;
  } test;

  RunBaseTest(&test);
}

TEST_P(VideoSendStreamTest, ChangingNetworkRoute) {
  static const int kStartBitrateBps = 300000;
  static const int kNewMaxBitrateBps = 1234567;
  static const uint8_t kExtensionId = test::kTransportSequenceNumberExtensionId;
  class ChangingNetworkRouteTest : public test::EndToEndTest {
   public:
    explicit ChangingNetworkRouteTest(
        test::SingleThreadedTaskQueueForTesting* task_queue)
        : EndToEndTest(test::CallTest::kDefaultTimeoutMs),
          task_queue_(task_queue),
          call_(nullptr) {
      EXPECT_TRUE(parser_->RegisterRtpHeaderExtension(
          kRtpExtensionTransportSequenceNumber, kExtensionId));
    }

    void OnCallsCreated(Call* sender_call, Call* receiver_call) override {
      call_ = sender_call;
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      send_config->rtp.extensions.clear();
      send_config->rtp.extensions.push_back(RtpExtension(
          RtpExtension::kTransportSequenceNumberUri, kExtensionId));
      (*receive_configs)[0].rtp.extensions = send_config->rtp.extensions;
      (*receive_configs)[0].rtp.transport_cc = true;
    }

    void ModifyAudioConfigs(
        AudioSendStream::Config* send_config,
        std::vector<AudioReceiveStream::Config>* receive_configs) override {
      send_config->rtp.extensions.clear();
      send_config->rtp.extensions.push_back(RtpExtension(
          RtpExtension::kTransportSequenceNumberUri, kExtensionId));
      (*receive_configs)[0].rtp.extensions.clear();
      (*receive_configs)[0].rtp.extensions = send_config->rtp.extensions;
      (*receive_configs)[0].rtp.transport_cc = true;
    }

    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      if (call_->GetStats().send_bandwidth_bps > kStartBitrateBps) {
        observation_complete_.Set();
      }

      return SEND_PACKET;
    }

    void PerformTest() override {
      rtc::NetworkRoute new_route;
      new_route.connected = true;
      new_route.local_network_id = 10;
      new_route.remote_network_id = 20;
      BitrateConstraints bitrate_config;

      task_queue_->SendTask([this, &new_route, &bitrate_config]() {
        call_->GetTransportControllerSend()->OnNetworkRouteChanged("transport",
                                                                   new_route);
        bitrate_config.start_bitrate_bps = kStartBitrateBps;
        call_->GetTransportControllerSend()->SetSdpBitrateParameters(
            bitrate_config);
      });

      EXPECT_TRUE(Wait())
          << "Timed out while waiting for start bitrate to be exceeded.";

      task_queue_->SendTask([this, &new_route, &bitrate_config]() {
        bitrate_config.start_bitrate_bps = -1;
        bitrate_config.max_bitrate_bps = kNewMaxBitrateBps;
        call_->GetTransportControllerSend()->SetSdpBitrateParameters(
            bitrate_config);
        // TODO(holmer): We should set the last sent packet id here and verify
        // that we correctly ignore any packet loss reported prior to that id.
        ++new_route.local_network_id;
        call_->GetTransportControllerSend()->OnNetworkRouteChanged("transport",
                                                                   new_route);
        EXPECT_GE(call_->GetStats().send_bandwidth_bps, kStartBitrateBps);
      });
    }

   private:
    test::SingleThreadedTaskQueueForTesting* const task_queue_;
    Call* call_;
  } test(&task_queue_);

  RunBaseTest(&test);
}

TEST_P(VideoSendStreamTest, ChangingTransportOverhead) {
  class ChangingTransportOverheadTest : public test::EndToEndTest {
   public:
    explicit ChangingTransportOverheadTest(
        test::SingleThreadedTaskQueueForTesting* task_queue)
        : EndToEndTest(test::CallTest::kDefaultTimeoutMs),
          task_queue_(task_queue),
          call_(nullptr),
          packets_sent_(0),
          transport_overhead_(0) {}

    void OnCallsCreated(Call* sender_call, Call* receiver_call) override {
      call_ = sender_call;
    }

    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      EXPECT_LE(length, kMaxRtpPacketSize);
      rtc::CritScope cs(&lock_);
      if (++packets_sent_ < 100)
        return SEND_PACKET;
      observation_complete_.Set();
      return SEND_PACKET;
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      send_config->rtp.max_packet_size = kMaxRtpPacketSize;
    }

    void PerformTest() override {
      task_queue_->SendTask([this]() {
        transport_overhead_ = 100;
        call_->GetTransportControllerSend()->OnTransportOverheadChanged(
            transport_overhead_);
      });

      EXPECT_TRUE(Wait());

      {
        rtc::CritScope cs(&lock_);
        packets_sent_ = 0;
      }

      task_queue_->SendTask([this]() {
        transport_overhead_ = 500;
        call_->GetTransportControllerSend()->OnTransportOverheadChanged(
            transport_overhead_);
      });

      EXPECT_TRUE(Wait());
    }

   private:
    test::SingleThreadedTaskQueueForTesting* const task_queue_;
    Call* call_;
    rtc::CriticalSection lock_;
    int packets_sent_ RTC_GUARDED_BY(lock_);
    int transport_overhead_;
    const size_t kMaxRtpPacketSize = 1000;
  } test(&task_queue_);

  RunBaseTest(&test);
}

// Test class takes takes as argument a switch selecting if type switch should
// occur and a function pointer to reset the send stream. This is necessary
// since you cannot change the content type of a VideoSendStream, you need to
// recreate it. Stopping and recreating the stream can only be done on the main
// thread and in the context of VideoSendStreamTest (not BaseTest).
template <typename T>
class MaxPaddingSetTest : public test::SendTest {
 public:
  static const uint32_t kMinTransmitBitrateBps = 400000;
  static const uint32_t kActualEncodeBitrateBps = 40000;
  static const uint32_t kMinPacketsToSend = 50;

  explicit MaxPaddingSetTest(bool test_switch_content_type, T* stream_reset_fun)
      : SendTest(test::CallTest::kDefaultTimeoutMs),
        call_(nullptr),
        send_stream_(nullptr),
        send_stream_config_(nullptr),
        packets_sent_(0),
        running_without_padding_(test_switch_content_type),
        stream_resetter_(stream_reset_fun) {
    RTC_DCHECK(stream_resetter_);
  }

  void OnVideoStreamsCreated(
      VideoSendStream* send_stream,
      const std::vector<VideoReceiveStream*>& receive_streams) override {
    rtc::CritScope lock(&crit_);
    send_stream_ = send_stream;
  }

  void ModifyVideoConfigs(
      VideoSendStream::Config* send_config,
      std::vector<VideoReceiveStream::Config>* receive_configs,
      VideoEncoderConfig* encoder_config) override {
    RTC_DCHECK_EQ(1, encoder_config->number_of_streams);
    if (RunningWithoutPadding()) {
      encoder_config->min_transmit_bitrate_bps = 0;
      encoder_config->content_type =
          VideoEncoderConfig::ContentType::kRealtimeVideo;
    } else {
      encoder_config->min_transmit_bitrate_bps = kMinTransmitBitrateBps;
      encoder_config->content_type = VideoEncoderConfig::ContentType::kScreen;
    }
    send_stream_config_ = send_config->Copy();
    encoder_config_ = encoder_config->Copy();
  }

  void OnCallsCreated(Call* sender_call, Call* receiver_call) override {
    call_ = sender_call;
  }

  Action OnSendRtp(const uint8_t* packet, size_t length) override {
    rtc::CritScope lock(&crit_);
    if (running_without_padding_)
      EXPECT_EQ(0, call_->GetStats().max_padding_bitrate_bps);

    // Wait until at least kMinPacketsToSend frames have been encoded, so that
    // we have reliable data.
    if (++packets_sent_ < kMinPacketsToSend)
      return SEND_PACKET;

    if (running_without_padding_) {
      // We've sent kMinPacketsToSend packets with default configuration, switch
      // to enabling screen content and setting min transmit bitrate.
      // Note that we need to recreate the stream if changing content type.
      packets_sent_ = 0;
      encoder_config_.min_transmit_bitrate_bps = kMinTransmitBitrateBps;
      encoder_config_.content_type = VideoEncoderConfig::ContentType::kScreen;
      running_without_padding_ = false;
      content_switch_event_.Set();
      return SEND_PACKET;
    }

    // Make sure the pacer has been configured with a min transmit bitrate.
    if (call_->GetStats().max_padding_bitrate_bps > 0)
      observation_complete_.Set();

    return SEND_PACKET;
  }

  void PerformTest() override {
    if (RunningWithoutPadding()) {
      ASSERT_TRUE(
          content_switch_event_.Wait(test::CallTest::kDefaultTimeoutMs));
      (*stream_resetter_)(send_stream_config_, encoder_config_);
    }

    ASSERT_TRUE(Wait()) << "Timed out waiting for a valid padding bitrate.";
  }

 private:
  bool RunningWithoutPadding() const {
    rtc::CritScope lock(&crit_);
    return running_without_padding_;
  }

  rtc::CriticalSection crit_;
  rtc::Event content_switch_event_;
  Call* call_;
  VideoSendStream* send_stream_ RTC_GUARDED_BY(crit_);
  VideoSendStream::Config send_stream_config_;
  VideoEncoderConfig encoder_config_;
  uint32_t packets_sent_ RTC_GUARDED_BY(crit_);
  bool running_without_padding_;
  T* const stream_resetter_;
};

TEST_P(VideoSendStreamTest, RespectsMinTransmitBitrate) {
  auto reset_fun = [](const VideoSendStream::Config& send_stream_config,
                      const VideoEncoderConfig& encoder_config) {};
  MaxPaddingSetTest<decltype(reset_fun)> test(false, &reset_fun);
  RunBaseTest(&test);
}

TEST_P(VideoSendStreamTest, RespectsMinTransmitBitrateAfterContentSwitch) {
  // Function for removing and recreating the send stream with a new config.
  auto reset_fun = [this](const VideoSendStream::Config& send_stream_config,
                          const VideoEncoderConfig& encoder_config) {
    task_queue_.SendTask([this, &send_stream_config, &encoder_config]() {
      Stop();
      DestroyVideoSendStreams();
      SetVideoSendConfig(send_stream_config);
      SetVideoEncoderConfig(encoder_config);
      CreateVideoSendStreams();
      SetVideoDegradation(DegradationPreference::MAINTAIN_RESOLUTION);
      Start();
    });
  };
  MaxPaddingSetTest<decltype(reset_fun)> test(true, &reset_fun);
  RunBaseTest(&test);
}

// This test verifies that new frame sizes reconfigures encoders even though not
// (yet) sending. The purpose of this is to permit encoding as quickly as
// possible once we start sending. Likely the frames being input are from the
// same source that will be sent later, which just means that we're ready
// earlier.
TEST_P(VideoSendStreamTest,
       EncoderReconfigureOnResolutionChangeWhenNotSending) {
  class EncoderObserver : public test::FakeEncoder {
   public:
    EncoderObserver()
        : FakeEncoder(Clock::GetRealTimeClock()),
          number_of_initializations_(0),
          last_initialized_frame_width_(0),
          last_initialized_frame_height_(0) {}

    void WaitForResolution(int width, int height) {
      {
        rtc::CritScope lock(&crit_);
        if (last_initialized_frame_width_ == width &&
            last_initialized_frame_height_ == height) {
          return;
        }
      }
      EXPECT_TRUE(
          init_encode_called_.Wait(VideoSendStreamTest::kDefaultTimeoutMs));
      {
        rtc::CritScope lock(&crit_);
        EXPECT_EQ(width, last_initialized_frame_width_);
        EXPECT_EQ(height, last_initialized_frame_height_);
      }
    }

   private:
    int32_t InitEncode(const VideoCodec* config,
                       int32_t number_of_cores,
                       size_t max_payload_size) override {
      rtc::CritScope lock(&crit_);
      last_initialized_frame_width_ = config->width;
      last_initialized_frame_height_ = config->height;
      ++number_of_initializations_;
      init_encode_called_.Set();
      return FakeEncoder::InitEncode(config, number_of_cores, max_payload_size);
    }

    int32_t Encode(const VideoFrame& input_image,
                   const CodecSpecificInfo* codec_specific_info,
                   const std::vector<FrameType>* frame_types) override {
      ADD_FAILURE()
          << "Unexpected Encode call since the send stream is not started";
      return 0;
    }

    rtc::CriticalSection crit_;
    rtc::Event init_encode_called_;
    size_t number_of_initializations_ RTC_GUARDED_BY(&crit_);
    int last_initialized_frame_width_ RTC_GUARDED_BY(&crit_);
    int last_initialized_frame_height_ RTC_GUARDED_BY(&crit_);
  };

  test::NullTransport transport;
  EncoderObserver encoder;
  test::VideoEncoderProxyFactory encoder_factory(&encoder);

  task_queue_.SendTask([this, &transport, &encoder_factory]() {
    CreateSenderCall();
    CreateSendConfig(1, 0, 0, &transport);
    GetVideoSendConfig()->encoder_settings.encoder_factory = &encoder_factory;
    CreateVideoStreams();
    CreateFrameGeneratorCapturer(kDefaultFramerate, kDefaultWidth,
                                 kDefaultHeight);
    frame_generator_capturer_->Start();
  });

  encoder.WaitForResolution(kDefaultWidth, kDefaultHeight);

  task_queue_.SendTask([this]() {
    frame_generator_capturer_->ChangeResolution(kDefaultWidth * 2,
                                                kDefaultHeight * 2);
  });

  encoder.WaitForResolution(kDefaultWidth * 2, kDefaultHeight * 2);

  task_queue_.SendTask([this]() {
    DestroyStreams();
    DestroyCalls();
  });
}

TEST_P(VideoSendStreamTest, CanReconfigureToUseStartBitrateAbovePreviousMax) {
  class StartBitrateObserver : public test::FakeEncoder {
   public:
    StartBitrateObserver()
        : FakeEncoder(Clock::GetRealTimeClock()),
          start_bitrate_kbps_(0) {}
    int32_t InitEncode(const VideoCodec* config,
                       int32_t number_of_cores,
                       size_t max_payload_size) override {
      rtc::CritScope lock(&crit_);
      start_bitrate_kbps_ = config->startBitrate;
      start_bitrate_changed_.Set();
      return FakeEncoder::InitEncode(config, number_of_cores, max_payload_size);
    }

    int32_t SetRates(uint32_t new_target_bitrate, uint32_t framerate) override {
      rtc::CritScope lock(&crit_);
      start_bitrate_kbps_ = new_target_bitrate;
      start_bitrate_changed_.Set();
      return FakeEncoder::SetRates(new_target_bitrate, framerate);
    }

    int GetStartBitrateKbps() const {
      rtc::CritScope lock(&crit_);
      return start_bitrate_kbps_;
    }

    bool WaitForStartBitrate() {
      return start_bitrate_changed_.Wait(
          VideoSendStreamTest::kDefaultTimeoutMs);
    }

   private:
    rtc::CriticalSection crit_;
    rtc::Event start_bitrate_changed_;
    int start_bitrate_kbps_ RTC_GUARDED_BY(crit_);
  };

  CreateSenderCall();

  test::NullTransport transport;
  CreateSendConfig(1, 0, 0, &transport);

  BitrateConstraints bitrate_config;
  bitrate_config.start_bitrate_bps =
      2 * GetVideoEncoderConfig()->max_bitrate_bps;
  sender_call_->GetTransportControllerSend()->SetSdpBitrateParameters(
      bitrate_config);

  StartBitrateObserver encoder;
  test::VideoEncoderProxyFactory encoder_factory(&encoder);
  // Since this test does not use a capturer, set |internal_source| = true.
  // Encoder configuration is otherwise updated on the next video frame.
  encoder_factory.SetHasInternalSource(true);
  GetVideoSendConfig()->encoder_settings.encoder_factory = &encoder_factory;

  CreateVideoStreams();

  EXPECT_TRUE(encoder.WaitForStartBitrate());
  EXPECT_EQ(GetVideoEncoderConfig()->max_bitrate_bps / 1000,
            encoder.GetStartBitrateKbps());

  GetVideoEncoderConfig()->max_bitrate_bps =
      2 * bitrate_config.start_bitrate_bps;
  GetVideoSendStream()->ReconfigureVideoEncoder(
      GetVideoEncoderConfig()->Copy());

  // New bitrate should be reconfigured above the previous max. As there's no
  // network connection this shouldn't be flaky, as no bitrate should've been
  // reported in between.
  EXPECT_TRUE(encoder.WaitForStartBitrate());
  EXPECT_EQ(bitrate_config.start_bitrate_bps / 1000,
            encoder.GetStartBitrateKbps());

  DestroyStreams();
}

class StartStopBitrateObserver : public test::FakeEncoder {
 public:
  StartStopBitrateObserver() : FakeEncoder(Clock::GetRealTimeClock()) {}
  int32_t InitEncode(const VideoCodec* config,
                     int32_t number_of_cores,
                     size_t max_payload_size) override {
    rtc::CritScope lock(&crit_);
    encoder_init_.Set();
    return FakeEncoder::InitEncode(config, number_of_cores, max_payload_size);
  }

  int32_t SetRateAllocation(const VideoBitrateAllocation& bitrate,
                            uint32_t framerate) override {
    rtc::CritScope lock(&crit_);
    bitrate_kbps_ = bitrate.get_sum_kbps();
    bitrate_changed_.Set();
    return FakeEncoder::SetRateAllocation(bitrate, framerate);
  }

  bool WaitForEncoderInit() {
    return encoder_init_.Wait(VideoSendStreamTest::kDefaultTimeoutMs);
  }

  bool WaitBitrateChanged(bool non_zero) {
    do {
      absl::optional<int> bitrate_kbps;
      {
        rtc::CritScope lock(&crit_);
        bitrate_kbps = bitrate_kbps_;
      }
      if (!bitrate_kbps)
        continue;

      if ((non_zero && *bitrate_kbps > 0) ||
          (!non_zero && *bitrate_kbps == 0)) {
        return true;
      }
    } while (bitrate_changed_.Wait(VideoSendStreamTest::kDefaultTimeoutMs));
    return false;
  }

 private:
  rtc::CriticalSection crit_;
  rtc::Event encoder_init_;
  rtc::Event bitrate_changed_;
  absl::optional<int> bitrate_kbps_ RTC_GUARDED_BY(crit_);
};

// This test that if the encoder use an internal source, VideoEncoder::SetRates
// will be called with zero bitrate during initialization and that
// VideoSendStream::Stop also triggers VideoEncoder::SetRates Start to be called
// with zero bitrate.
TEST_P(VideoSendStreamTest, VideoSendStreamStopSetEncoderRateToZero) {
  test::NullTransport transport;
  StartStopBitrateObserver encoder;
  test::VideoEncoderProxyFactory encoder_factory(&encoder);
  encoder_factory.SetHasInternalSource(true);
  test::FrameForwarder forwarder;

  task_queue_.SendTask([this, &transport, &encoder_factory, &forwarder]() {
    CreateSenderCall();
    CreateSendConfig(1, 0, 0, &transport);

    sender_call_->SignalChannelNetworkState(MediaType::VIDEO, kNetworkUp);
    GetVideoSendConfig()->encoder_settings.encoder_factory = &encoder_factory;

    CreateVideoStreams();
    // Inject a frame, to force encoder creation.
    GetVideoSendStream()->Start();
    GetVideoSendStream()->SetSource(&forwarder,
                                    DegradationPreference::DISABLED);
    forwarder.IncomingCapturedFrame(CreateVideoFrame(640, 480, 4));
  });

  EXPECT_TRUE(encoder.WaitForEncoderInit());

  task_queue_.SendTask([this]() { GetVideoSendStream()->Start(); });
  EXPECT_TRUE(encoder.WaitBitrateChanged(true));

  task_queue_.SendTask([this]() { GetVideoSendStream()->Stop(); });
  EXPECT_TRUE(encoder.WaitBitrateChanged(false));

  task_queue_.SendTask([this]() { GetVideoSendStream()->Start(); });
  EXPECT_TRUE(encoder.WaitBitrateChanged(true));

  task_queue_.SendTask([this]() {
    DestroyStreams();
    DestroyCalls();
  });
}

// Tests that when the encoder uses an internal source, the VideoEncoder will
// be updated with a new bitrate when turning the VideoSendStream on/off with
// VideoSendStream::UpdateActiveSimulcastLayers, and when the VideoStreamEncoder
// is reconfigured with new active layers.
TEST_P(VideoSendStreamTest, VideoSendStreamUpdateActiveSimulcastLayers) {
  test::NullTransport transport;
  StartStopBitrateObserver encoder;
  test::VideoEncoderProxyFactory encoder_factory(&encoder);
  encoder_factory.SetHasInternalSource(true);
  test::FrameForwarder forwarder;

  task_queue_.SendTask([this, &transport, &encoder_factory, &forwarder]() {
    CreateSenderCall();
    // Create two simulcast streams.
    CreateSendConfig(2, 0, 0, &transport);

    sender_call_->SignalChannelNetworkState(MediaType::VIDEO, kNetworkUp);
    GetVideoSendConfig()->encoder_settings.encoder_factory = &encoder_factory;

    CreateVideoStreams();

    // Inject a frame, to force encoder creation.
    GetVideoSendStream()->Start();
    GetVideoSendStream()->SetSource(&forwarder,
                                    DegradationPreference::DISABLED);
    forwarder.IncomingCapturedFrame(CreateVideoFrame(640, 480, 4));
  });

  EXPECT_TRUE(encoder.WaitForEncoderInit());

  // When we turn on the simulcast layers it will update the BitrateAllocator,
  // which in turn updates the VideoEncoder's bitrate.
  task_queue_.SendTask([this]() {
    GetVideoSendStream()->UpdateActiveSimulcastLayers({true, true});
  });
  EXPECT_TRUE(encoder.WaitBitrateChanged(true));

  GetVideoEncoderConfig()->simulcast_layers[0].active = true;
  GetVideoEncoderConfig()->simulcast_layers[1].active = false;
  task_queue_.SendTask([this]() {
    GetVideoSendStream()->ReconfigureVideoEncoder(
        GetVideoEncoderConfig()->Copy());
  });
  // TODO(bugs.webrtc.org/8807): Currently we require a hard reconfiguration to
  // update the VideoBitrateAllocator and BitrateAllocator of which layers are
  // active. Once the change is made for a "soft" reconfiguration we can remove
  // the expecation for an encoder init. We can also test that bitrate changes
  // when just updating individual active layers, which should change the
  // bitrate set to the video encoder.
  EXPECT_TRUE(encoder.WaitForEncoderInit());
  EXPECT_TRUE(encoder.WaitBitrateChanged(true));

  // Turning off both simulcast layers should trigger a bitrate change of 0.
  GetVideoEncoderConfig()->simulcast_layers[0].active = false;
  GetVideoEncoderConfig()->simulcast_layers[1].active = false;
  task_queue_.SendTask([this]() {
    GetVideoSendStream()->UpdateActiveSimulcastLayers({false, false});
  });
  EXPECT_TRUE(encoder.WaitBitrateChanged(false));

  task_queue_.SendTask([this]() {
    DestroyStreams();
    DestroyCalls();
  });
}
TEST_P(VideoSendStreamTest, CapturesTextureAndVideoFrames) {
  class FrameObserver : public rtc::VideoSinkInterface<VideoFrame> {
   public:
    void OnFrame(const VideoFrame& video_frame) override {
      output_frames_.push_back(video_frame);
      output_frame_event_.Set();
    }

    void WaitOutputFrame() {
      const int kWaitFrameTimeoutMs = 3000;
      EXPECT_TRUE(output_frame_event_.Wait(kWaitFrameTimeoutMs))
          << "Timeout while waiting for output frames.";
    }

    const std::vector<VideoFrame>& output_frames() const {
      return output_frames_;
    }

   private:
    // Delivered output frames.
    std::vector<VideoFrame> output_frames_;

    // Indicate an output frame has arrived.
    rtc::Event output_frame_event_;
  };

  test::NullTransport transport;
  FrameObserver observer;
  std::vector<VideoFrame> input_frames;

  task_queue_.SendTask([this, &transport, &observer, &input_frames]() {
    // Initialize send stream.
    CreateSenderCall();

    CreateSendConfig(1, 0, 0, &transport);
    GetVideoSendConfig()->pre_encode_callback = &observer;
    CreateVideoStreams();

    // Prepare five input frames. Send ordinary VideoFrame and texture frames
    // alternatively.
    int width = 168;
    int height = 132;

    input_frames.push_back(test::FakeNativeBuffer::CreateFrame(
        width, height, 1, 1, kVideoRotation_0));
    input_frames.push_back(test::FakeNativeBuffer::CreateFrame(
        width, height, 2, 2, kVideoRotation_0));
    input_frames.push_back(CreateVideoFrame(width, height, 3));
    input_frames.push_back(CreateVideoFrame(width, height, 4));
    input_frames.push_back(test::FakeNativeBuffer::CreateFrame(
        width, height, 5, 5, kVideoRotation_0));

    GetVideoSendStream()->Start();
    test::FrameForwarder forwarder;
    GetVideoSendStream()->SetSource(&forwarder,
                                    DegradationPreference::MAINTAIN_FRAMERATE);
    for (size_t i = 0; i < input_frames.size(); i++) {
      forwarder.IncomingCapturedFrame(input_frames[i]);
      // Wait until the output frame is received before sending the next input
      // frame. Or the previous input frame may be replaced without delivering.
      observer.WaitOutputFrame();
    }
    GetVideoSendStream()->Stop();
    GetVideoSendStream()->SetSource(nullptr,
                                    DegradationPreference::MAINTAIN_FRAMERATE);
  });

  // Test if the input and output frames are the same. render_time_ms and
  // timestamp are not compared because capturer sets those values.
  ExpectEqualFramesVector(input_frames, observer.output_frames());

  task_queue_.SendTask([this]() {
    DestroyStreams();
    DestroyCalls();
  });
}

void ExpectEqualFramesVector(const std::vector<VideoFrame>& frames1,
                             const std::vector<VideoFrame>& frames2) {
  EXPECT_EQ(frames1.size(), frames2.size());
  for (size_t i = 0; i < std::min(frames1.size(), frames2.size()); ++i)
    // Compare frame buffers, since we don't care about differing timestamps.
    EXPECT_TRUE(test::FrameBufsEqual(frames1[i].video_frame_buffer(),
                                     frames2[i].video_frame_buffer()));
}

VideoFrame CreateVideoFrame(int width, int height, uint8_t data) {
  const int kSizeY = width * height * 2;
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[kSizeY]);
  memset(buffer.get(), data, kSizeY);
  VideoFrame frame(I420Buffer::Create(width, height), kVideoRotation_0, data);
  frame.set_timestamp(data);
  // Use data as a ms timestamp.
  frame.set_timestamp_us(data * rtc::kNumMicrosecsPerMillisec);
  return frame;
}

TEST_P(VideoSendStreamTest, EncoderIsProperlyInitializedAndDestroyed) {
  class EncoderStateObserver : public test::SendTest, public VideoEncoder {
   public:
    explicit EncoderStateObserver(
        test::SingleThreadedTaskQueueForTesting* task_queue)
        : SendTest(kDefaultTimeoutMs),
          task_queue_(task_queue),
          stream_(nullptr),
          initialized_(false),
          callback_registered_(false),
          num_releases_(0),
          released_(false),
          encoder_factory_(this) {}

    bool IsReleased() {
      rtc::CritScope lock(&crit_);
      return released_;
    }

    bool IsReadyForEncode() {
      rtc::CritScope lock(&crit_);
      return initialized_ && callback_registered_;
    }

    size_t num_releases() {
      rtc::CritScope lock(&crit_);
      return num_releases_;
    }

   private:
    int32_t InitEncode(const VideoCodec* codecSettings,
                       int32_t numberOfCores,
                       size_t maxPayloadSize) override {
      rtc::CritScope lock(&crit_);
      EXPECT_FALSE(initialized_);
      initialized_ = true;
      released_ = false;
      return 0;
    }

    int32_t Encode(const VideoFrame& inputImage,
                   const CodecSpecificInfo* codecSpecificInfo,
                   const std::vector<FrameType>* frame_types) override {
      EXPECT_TRUE(IsReadyForEncode());

      observation_complete_.Set();
      return 0;
    }

    int32_t RegisterEncodeCompleteCallback(
        EncodedImageCallback* callback) override {
      rtc::CritScope lock(&crit_);
      EXPECT_TRUE(initialized_);
      callback_registered_ = true;
      return 0;
    }

    int32_t Release() override {
      rtc::CritScope lock(&crit_);
      EXPECT_TRUE(IsReadyForEncode());
      EXPECT_FALSE(released_);
      initialized_ = false;
      callback_registered_ = false;
      released_ = true;
      ++num_releases_;
      return 0;
    }

    int32_t SetRates(uint32_t newBitRate, uint32_t frameRate) override {
      EXPECT_TRUE(IsReadyForEncode());
      return 0;
    }

    void OnVideoStreamsCreated(
        VideoSendStream* send_stream,
        const std::vector<VideoReceiveStream*>& receive_streams) override {
      stream_ = send_stream;
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      send_config->encoder_settings.encoder_factory = &encoder_factory_;
      encoder_config_ = encoder_config->Copy();
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait()) << "Timed out while waiting for Encode.";

      task_queue_->SendTask([this]() {
        EXPECT_EQ(0u, num_releases());
        stream_->ReconfigureVideoEncoder(std::move(encoder_config_));
        EXPECT_EQ(0u, num_releases());
        stream_->Stop();
        // Encoder should not be released before destroying the VideoSendStream.
        EXPECT_FALSE(IsReleased());
        EXPECT_TRUE(IsReadyForEncode());
        stream_->Start();
      });

      // Sanity check, make sure we still encode frames with this encoder.
      EXPECT_TRUE(Wait()) << "Timed out while waiting for Encode.";
    }

    test::SingleThreadedTaskQueueForTesting* const task_queue_;
    rtc::CriticalSection crit_;
    VideoSendStream* stream_;
    bool initialized_ RTC_GUARDED_BY(crit_);
    bool callback_registered_ RTC_GUARDED_BY(crit_);
    size_t num_releases_ RTC_GUARDED_BY(crit_);
    bool released_ RTC_GUARDED_BY(crit_);
    test::VideoEncoderProxyFactory encoder_factory_;
    VideoEncoderConfig encoder_config_;
  } test_encoder(&task_queue_);

  RunBaseTest(&test_encoder);

  EXPECT_TRUE(test_encoder.IsReleased());
  EXPECT_EQ(1u, test_encoder.num_releases());
}

TEST_P(VideoSendStreamTest, EncoderSetupPropagatesCommonEncoderConfigValues) {
  class VideoCodecConfigObserver : public test::SendTest,
                                   public test::FakeEncoder {
   public:
    VideoCodecConfigObserver()
        : SendTest(kDefaultTimeoutMs),
          FakeEncoder(Clock::GetRealTimeClock()),
          num_initializations_(0),
          stream_(nullptr),
          encoder_factory_(this) {}

   private:
    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      send_config->encoder_settings.encoder_factory = &encoder_factory_;
      encoder_config->max_bitrate_bps = kFirstMaxBitrateBps;
      encoder_config_ = encoder_config->Copy();
    }

    void OnVideoStreamsCreated(
        VideoSendStream* send_stream,
        const std::vector<VideoReceiveStream*>& receive_streams) override {
      stream_ = send_stream;
    }

    int32_t InitEncode(const VideoCodec* config,
                       int32_t number_of_cores,
                       size_t max_payload_size) override {
      if (num_initializations_ == 0) {
        // Verify default values.
        EXPECT_EQ(kFirstMaxBitrateBps / 1000, config->maxBitrate);
      } else {
        // Verify that changed values are propagated.
        EXPECT_EQ(kSecondMaxBitrateBps / 1000, config->maxBitrate);
      }
      ++num_initializations_;
      init_encode_event_.Set();
      return FakeEncoder::InitEncode(config, number_of_cores, max_payload_size);
    }

    void PerformTest() override {
      EXPECT_TRUE(init_encode_event_.Wait(kDefaultTimeoutMs));
      EXPECT_EQ(1u, num_initializations_) << "VideoEncoder not initialized.";

      encoder_config_.max_bitrate_bps = kSecondMaxBitrateBps;
      stream_->ReconfigureVideoEncoder(std::move(encoder_config_));
      EXPECT_TRUE(init_encode_event_.Wait(kDefaultTimeoutMs));
      EXPECT_EQ(2u, num_initializations_)
          << "ReconfigureVideoEncoder did not reinitialize the encoder with "
             "new encoder settings.";
    }

    const uint32_t kFirstMaxBitrateBps = 1000000;
    const uint32_t kSecondMaxBitrateBps = 2000000;

    rtc::Event init_encode_event_;
    size_t num_initializations_;
    VideoSendStream* stream_;
    test::VideoEncoderProxyFactory encoder_factory_;
    VideoEncoderConfig encoder_config_;
  } test;

  RunBaseTest(&test);
}

static const size_t kVideoCodecConfigObserverNumberOfTemporalLayers = 3;
template <typename T>
class VideoCodecConfigObserver : public test::SendTest,
                                 public test::FakeEncoder {
 public:
  VideoCodecConfigObserver(VideoCodecType video_codec_type,
                           const char* codec_name)
      : SendTest(VideoSendStreamTest::kDefaultTimeoutMs),
        FakeEncoder(Clock::GetRealTimeClock()),
        video_codec_type_(video_codec_type),
        codec_name_(codec_name),
        num_initializations_(0),
        stream_(nullptr),
        encoder_factory_(this) {
    InitCodecSpecifics();
  }

 private:
  void ModifyVideoConfigs(
      VideoSendStream::Config* send_config,
      std::vector<VideoReceiveStream::Config>* receive_configs,
      VideoEncoderConfig* encoder_config) override {
    send_config->encoder_settings.encoder_factory = &encoder_factory_;
    send_config->rtp.payload_name = codec_name_;

    encoder_config->codec_type = video_codec_type_;
    encoder_config->encoder_specific_settings = GetEncoderSpecificSettings();
    EXPECT_EQ(1u, encoder_config->simulcast_layers.size());
    encoder_config->simulcast_layers[0].num_temporal_layers =
        kVideoCodecConfigObserverNumberOfTemporalLayers;
    encoder_config_ = encoder_config->Copy();
  }

  void OnVideoStreamsCreated(
      VideoSendStream* send_stream,
      const std::vector<VideoReceiveStream*>& receive_streams) override {
    stream_ = send_stream;
  }

  int32_t InitEncode(const VideoCodec* config,
                     int32_t number_of_cores,
                     size_t max_payload_size) override {
    EXPECT_EQ(video_codec_type_, config->codecType);
    VerifyCodecSpecifics(*config);
    ++num_initializations_;
    init_encode_event_.Set();
    return FakeEncoder::InitEncode(config, number_of_cores, max_payload_size);
  }

  void InitCodecSpecifics();
  void VerifyCodecSpecifics(const VideoCodec& config) const;
  rtc::scoped_refptr<VideoEncoderConfig::EncoderSpecificSettings>
  GetEncoderSpecificSettings() const;

  void PerformTest() override {
    EXPECT_TRUE(
        init_encode_event_.Wait(VideoSendStreamTest::kDefaultTimeoutMs));
    ASSERT_EQ(1u, num_initializations_) << "VideoEncoder not initialized.";

    // Change encoder settings to actually trigger reconfiguration.
    encoder_settings_.frameDroppingOn = !encoder_settings_.frameDroppingOn;
    encoder_config_.encoder_specific_settings = GetEncoderSpecificSettings();
    stream_->ReconfigureVideoEncoder(std::move(encoder_config_));
    ASSERT_TRUE(
        init_encode_event_.Wait(VideoSendStreamTest::kDefaultTimeoutMs));
    EXPECT_EQ(2u, num_initializations_)
        << "ReconfigureVideoEncoder did not reinitialize the encoder with "
           "new encoder settings.";
  }

  int32_t Encode(const VideoFrame& input_image,
                 const CodecSpecificInfo* codec_specific_info,
                 const std::vector<FrameType>* frame_types) override {
    // Silently skip the encode, FakeEncoder::Encode doesn't produce VP8.
    return 0;
  }

  T encoder_settings_;
  const VideoCodecType video_codec_type_;
  const char* const codec_name_;
  rtc::Event init_encode_event_;
  size_t num_initializations_;
  VideoSendStream* stream_;
  test::VideoEncoderProxyFactory encoder_factory_;
  VideoEncoderConfig encoder_config_;
};

template <>
void VideoCodecConfigObserver<VideoCodecH264>::InitCodecSpecifics() {
  encoder_settings_ = VideoEncoder::GetDefaultH264Settings();
}

template <>
void VideoCodecConfigObserver<VideoCodecH264>::VerifyCodecSpecifics(
    const VideoCodec& config) const {
  EXPECT_EQ(
      0, memcmp(&config.H264(), &encoder_settings_, sizeof(encoder_settings_)));
}

template <>
rtc::scoped_refptr<VideoEncoderConfig::EncoderSpecificSettings>
VideoCodecConfigObserver<VideoCodecH264>::GetEncoderSpecificSettings() const {
  return new rtc::RefCountedObject<
      VideoEncoderConfig::H264EncoderSpecificSettings>(encoder_settings_);
}

template <>
void VideoCodecConfigObserver<VideoCodecVP8>::InitCodecSpecifics() {
  encoder_settings_ = VideoEncoder::GetDefaultVp8Settings();
}

template <>
void VideoCodecConfigObserver<VideoCodecVP8>::VerifyCodecSpecifics(
    const VideoCodec& config) const {
  // Check that the number of temporal layers has propagated properly to
  // VideoCodec.
  EXPECT_EQ(kVideoCodecConfigObserverNumberOfTemporalLayers,
            config.VP8().numberOfTemporalLayers);

  for (unsigned char i = 0; i < config.numberOfSimulcastStreams; ++i) {
    EXPECT_EQ(kVideoCodecConfigObserverNumberOfTemporalLayers,
              config.simulcastStream[i].numberOfTemporalLayers);
  }

  // Set expected temporal layers as they should have been set when
  // reconfiguring the encoder and not match the set config.
  VideoCodecVP8 encoder_settings = encoder_settings_;
  encoder_settings.numberOfTemporalLayers =
      kVideoCodecConfigObserverNumberOfTemporalLayers;
  EXPECT_EQ(
      0, memcmp(&config.VP8(), &encoder_settings, sizeof(encoder_settings_)));
}

template <>
rtc::scoped_refptr<VideoEncoderConfig::EncoderSpecificSettings>
VideoCodecConfigObserver<VideoCodecVP8>::GetEncoderSpecificSettings() const {
  return new rtc::RefCountedObject<
      VideoEncoderConfig::Vp8EncoderSpecificSettings>(encoder_settings_);
}

template <>
void VideoCodecConfigObserver<VideoCodecVP9>::InitCodecSpecifics() {
  encoder_settings_ = VideoEncoder::GetDefaultVp9Settings();
}

template <>
void VideoCodecConfigObserver<VideoCodecVP9>::VerifyCodecSpecifics(
    const VideoCodec& config) const {
  // Check that the number of temporal layers has propagated properly to
  // VideoCodec.
  EXPECT_EQ(kVideoCodecConfigObserverNumberOfTemporalLayers,
            config.VP9().numberOfTemporalLayers);

  for (unsigned char i = 0; i < config.numberOfSimulcastStreams; ++i) {
    EXPECT_EQ(kVideoCodecConfigObserverNumberOfTemporalLayers,
              config.simulcastStream[i].numberOfTemporalLayers);
  }

  // Set expected temporal layers as they should have been set when
  // reconfiguring the encoder and not match the set config.
  VideoCodecVP9 encoder_settings = encoder_settings_;
  encoder_settings.numberOfTemporalLayers =
      kVideoCodecConfigObserverNumberOfTemporalLayers;
  EXPECT_EQ(
      0, memcmp(&(config.VP9()), &encoder_settings, sizeof(encoder_settings_)));
}

template <>
rtc::scoped_refptr<VideoEncoderConfig::EncoderSpecificSettings>
VideoCodecConfigObserver<VideoCodecVP9>::GetEncoderSpecificSettings() const {
  return new rtc::RefCountedObject<
      VideoEncoderConfig::Vp9EncoderSpecificSettings>(encoder_settings_);
}

TEST_P(VideoSendStreamTest, EncoderSetupPropagatesVp8Config) {
  VideoCodecConfigObserver<VideoCodecVP8> test(kVideoCodecVP8, "VP8");
  RunBaseTest(&test);
}

TEST_P(VideoSendStreamTest, EncoderSetupPropagatesVp9Config) {
  VideoCodecConfigObserver<VideoCodecVP9> test(kVideoCodecVP9, "VP9");
  RunBaseTest(&test);
}

TEST_P(VideoSendStreamTest, EncoderSetupPropagatesH264Config) {
  VideoCodecConfigObserver<VideoCodecH264> test(kVideoCodecH264, "H264");
  RunBaseTest(&test);
}

TEST_P(VideoSendStreamTest, RtcpSenderReportContainsMediaBytesSent) {
  class RtcpSenderReportTest : public test::SendTest {
   public:
    RtcpSenderReportTest()
        : SendTest(kDefaultTimeoutMs),
          rtp_packets_sent_(0),
          media_bytes_sent_(0) {}

   private:
    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      rtc::CritScope lock(&crit_);
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));
      ++rtp_packets_sent_;
      media_bytes_sent_ += length - header.headerLength - header.paddingLength;
      return SEND_PACKET;
    }

    Action OnSendRtcp(const uint8_t* packet, size_t length) override {
      rtc::CritScope lock(&crit_);
      test::RtcpPacketParser parser;
      EXPECT_TRUE(parser.Parse(packet, length));

      if (parser.sender_report()->num_packets() > 0) {
        // Only compare sent media bytes if SenderPacketCount matches the
        // number of sent rtp packets (a new rtp packet could be sent before
        // the rtcp packet).
        if (parser.sender_report()->sender_octet_count() > 0 &&
            parser.sender_report()->sender_packet_count() ==
                rtp_packets_sent_) {
          EXPECT_EQ(media_bytes_sent_,
                    parser.sender_report()->sender_octet_count());
          observation_complete_.Set();
        }
      }

      return SEND_PACKET;
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait()) << "Timed out while waiting for RTCP sender report.";
    }

    rtc::CriticalSection crit_;
    size_t rtp_packets_sent_ RTC_GUARDED_BY(&crit_);
    size_t media_bytes_sent_ RTC_GUARDED_BY(&crit_);
  } test;

  RunBaseTest(&test);
}

TEST_P(VideoSendStreamTest, TranslatesTwoLayerScreencastToTargetBitrate) {
  static const int kScreencastMaxTargetBitrateDeltaKbps = 1;

  class VideoStreamFactory
      : public VideoEncoderConfig::VideoStreamFactoryInterface {
   public:
    VideoStreamFactory() {}

   private:
    std::vector<VideoStream> CreateEncoderStreams(
        int width,
        int height,
        const VideoEncoderConfig& encoder_config) override {
      std::vector<VideoStream> streams =
          test::CreateVideoStreams(width, height, encoder_config);
      RTC_CHECK_GT(streams[0].max_bitrate_bps,
                   kScreencastMaxTargetBitrateDeltaKbps);
      streams[0].target_bitrate_bps =
          streams[0].max_bitrate_bps -
          kScreencastMaxTargetBitrateDeltaKbps * 1000;
      return streams;
    }
  };

  class ScreencastTargetBitrateTest : public test::SendTest,
                                      public test::FakeEncoder {
   public:
    ScreencastTargetBitrateTest()
        : SendTest(kDefaultTimeoutMs),
          test::FakeEncoder(Clock::GetRealTimeClock()),
          encoder_factory_(this) {}

   private:
    int32_t InitEncode(const VideoCodec* config,
                       int32_t number_of_cores,
                       size_t max_payload_size) override {
      EXPECT_EQ(config->numberOfSimulcastStreams, 1);
      EXPECT_EQ(static_cast<unsigned int>(kScreencastMaxTargetBitrateDeltaKbps),
                config->simulcastStream[0].maxBitrate -
                    config->simulcastStream[0].targetBitrate);
      observation_complete_.Set();
      return test::FakeEncoder::InitEncode(config, number_of_cores,
                                           max_payload_size);
    }
    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      send_config->encoder_settings.encoder_factory = &encoder_factory_;
      EXPECT_EQ(1u, encoder_config->number_of_streams);
      encoder_config->video_stream_factory =
          new rtc::RefCountedObject<VideoStreamFactory>();
      EXPECT_EQ(1u, encoder_config->simulcast_layers.size());
      encoder_config->simulcast_layers[0].num_temporal_layers = 2;
      encoder_config->content_type = VideoEncoderConfig::ContentType::kScreen;
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait())
          << "Timed out while waiting for the encoder to be initialized.";
    }
    test::VideoEncoderProxyFactory encoder_factory_;
  } test;

  RunBaseTest(&test);
}

TEST_P(VideoSendStreamTest, ReconfigureBitratesSetsEncoderBitratesCorrectly) {
  // These are chosen to be "kind of odd" to not be accidentally checked against
  // default values.
  static const int kMinBitrateKbps = 137;
  static const int kStartBitrateKbps = 345;
  static const int kLowerMaxBitrateKbps = 312;
  static const int kMaxBitrateKbps = 413;
  static const int kIncreasedStartBitrateKbps = 451;
  static const int kIncreasedMaxBitrateKbps = 597;
  class EncoderBitrateThresholdObserver : public test::SendTest,
                                          public test::FakeEncoder {
   public:
    explicit EncoderBitrateThresholdObserver(
        test::SingleThreadedTaskQueueForTesting* task_queue)
        : SendTest(kDefaultTimeoutMs),
          FakeEncoder(Clock::GetRealTimeClock()),
          task_queue_(task_queue),
          target_bitrate_(0),
          num_initializations_(0),
          call_(nullptr),
          send_stream_(nullptr),
          encoder_factory_(this) {}

   private:
    int32_t InitEncode(const VideoCodec* codecSettings,
                       int32_t numberOfCores,
                       size_t maxPayloadSize) override {
      EXPECT_GE(codecSettings->startBitrate, codecSettings->minBitrate);
      EXPECT_LE(codecSettings->startBitrate, codecSettings->maxBitrate);
      if (num_initializations_ == 0) {
        EXPECT_EQ(static_cast<unsigned int>(kMinBitrateKbps),
                  codecSettings->minBitrate);
        EXPECT_EQ(static_cast<unsigned int>(kStartBitrateKbps),
                  codecSettings->startBitrate);
        EXPECT_EQ(static_cast<unsigned int>(kMaxBitrateKbps),
                  codecSettings->maxBitrate);
        observation_complete_.Set();
      } else if (num_initializations_ == 1) {
        EXPECT_EQ(static_cast<unsigned int>(kLowerMaxBitrateKbps),
                  codecSettings->maxBitrate);
        // The start bitrate should be kept (-1) and capped to the max bitrate.
        // Since this is not an end-to-end call no receiver should have been
        // returning a REMB that could lower this estimate.
        EXPECT_EQ(codecSettings->startBitrate, codecSettings->maxBitrate);
      } else if (num_initializations_ == 2) {
        EXPECT_EQ(static_cast<unsigned int>(kIncreasedMaxBitrateKbps),
                  codecSettings->maxBitrate);
        // The start bitrate will be whatever the rate BitRateController
        // has currently configured but in the span of the set max and min
        // bitrate.
      }
      ++num_initializations_;
      init_encode_event_.Set();

      return FakeEncoder::InitEncode(codecSettings, numberOfCores,
                                     maxPayloadSize);
    }

    int32_t SetRateAllocation(const VideoBitrateAllocation& bitrate,
                              uint32_t frameRate) override {
      {
        rtc::CritScope lock(&crit_);
        if (target_bitrate_ == bitrate.get_sum_kbps()) {
          return FakeEncoder::SetRateAllocation(bitrate, frameRate);
        }
        target_bitrate_ = bitrate.get_sum_kbps();
      }
      bitrate_changed_event_.Set();
      return FakeEncoder::SetRateAllocation(bitrate, frameRate);
    }

    void WaitForSetRates(uint32_t expected_bitrate) {
      EXPECT_TRUE(
          bitrate_changed_event_.Wait(VideoSendStreamTest::kDefaultTimeoutMs))
          << "Timed out while waiting encoder rate to be set.";
      rtc::CritScope lock(&crit_);
      EXPECT_EQ(expected_bitrate, target_bitrate_);
    }

    void ModifySenderBitrateConfig(
        BitrateConstraints* bitrate_config) override {
      bitrate_config->min_bitrate_bps = kMinBitrateKbps * 1000;
      bitrate_config->start_bitrate_bps = kStartBitrateKbps * 1000;
      bitrate_config->max_bitrate_bps = kMaxBitrateKbps * 1000;
    }

    class VideoStreamFactory
        : public VideoEncoderConfig::VideoStreamFactoryInterface {
     public:
      explicit VideoStreamFactory(int min_bitrate_bps)
          : min_bitrate_bps_(min_bitrate_bps) {}

     private:
      std::vector<VideoStream> CreateEncoderStreams(
          int width,
          int height,
          const VideoEncoderConfig& encoder_config) override {
        std::vector<VideoStream> streams =
            test::CreateVideoStreams(width, height, encoder_config);
        streams[0].min_bitrate_bps = min_bitrate_bps_;
        return streams;
      }

      const int min_bitrate_bps_;
    };

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      send_config->encoder_settings.encoder_factory = &encoder_factory_;
      // Set bitrates lower/higher than min/max to make sure they are properly
      // capped.
      encoder_config->max_bitrate_bps = kMaxBitrateKbps * 1000;
      // Create a new StreamFactory to be able to set
      // |VideoStream.min_bitrate_bps|.
      encoder_config->video_stream_factory =
          new rtc::RefCountedObject<VideoStreamFactory>(kMinBitrateKbps * 1000);
      encoder_config_ = encoder_config->Copy();
    }

    void OnCallsCreated(Call* sender_call, Call* receiver_call) override {
      call_ = sender_call;
    }

    void OnVideoStreamsCreated(
        VideoSendStream* send_stream,
        const std::vector<VideoReceiveStream*>& receive_streams) override {
      send_stream_ = send_stream;
    }

    void PerformTest() override {
      ASSERT_TRUE(
          init_encode_event_.Wait(VideoSendStreamTest::kDefaultTimeoutMs))
          << "Timed out while waiting for encoder to be configured.";
      WaitForSetRates(kStartBitrateKbps);
      BitrateConstraints bitrate_config;
      bitrate_config.start_bitrate_bps = kIncreasedStartBitrateKbps * 1000;
      bitrate_config.max_bitrate_bps = kIncreasedMaxBitrateKbps * 1000;
      task_queue_->SendTask([this, &bitrate_config]() {
        call_->GetTransportControllerSend()->SetSdpBitrateParameters(
            bitrate_config);
      });
      // Encoder rate is capped by EncoderConfig max_bitrate_bps.
      WaitForSetRates(kMaxBitrateKbps);
      encoder_config_.max_bitrate_bps = kLowerMaxBitrateKbps * 1000;
      send_stream_->ReconfigureVideoEncoder(encoder_config_.Copy());
      ASSERT_TRUE(
          init_encode_event_.Wait(VideoSendStreamTest::kDefaultTimeoutMs));
      EXPECT_EQ(2, num_initializations_)
          << "Encoder should have been reconfigured with the new value.";
      WaitForSetRates(kLowerMaxBitrateKbps);

      encoder_config_.max_bitrate_bps = kIncreasedMaxBitrateKbps * 1000;
      send_stream_->ReconfigureVideoEncoder(encoder_config_.Copy());
      ASSERT_TRUE(
          init_encode_event_.Wait(VideoSendStreamTest::kDefaultTimeoutMs));
      EXPECT_EQ(3, num_initializations_)
          << "Encoder should have been reconfigured with the new value.";
      // Expected target bitrate is the start bitrate set in the call to
      // call_->GetTransportControllerSend()->SetSdpBitrateParameters.
      WaitForSetRates(kIncreasedStartBitrateKbps);
    }

    test::SingleThreadedTaskQueueForTesting* const task_queue_;
    rtc::Event init_encode_event_;
    rtc::Event bitrate_changed_event_;
    rtc::CriticalSection crit_;
    uint32_t target_bitrate_ RTC_GUARDED_BY(&crit_);

    int num_initializations_;
    webrtc::Call* call_;
    webrtc::VideoSendStream* send_stream_;
    test::VideoEncoderProxyFactory encoder_factory_;
    webrtc::VideoEncoderConfig encoder_config_;
  } test(&task_queue_);

  RunBaseTest(&test);
}

TEST_P(VideoSendStreamTest, ReportsSentResolution) {
  static const size_t kNumStreams = 3;
  // Unusual resolutions to make sure that they are the ones being reported.
  static const struct {
    int width;
    int height;
  } kEncodedResolution[kNumStreams] = {{241, 181}, {300, 121}, {121, 221}};
  class ScreencastTargetBitrateTest : public test::SendTest,
                                      public test::FakeEncoder {
   public:
    ScreencastTargetBitrateTest()
        : SendTest(kDefaultTimeoutMs),
          test::FakeEncoder(Clock::GetRealTimeClock()),
          send_stream_(nullptr),
          encoder_factory_(this) {}

   private:
    int32_t Encode(const VideoFrame& input_image,
                   const CodecSpecificInfo* codecSpecificInfo,
                   const std::vector<FrameType>* frame_types) override {
      CodecSpecificInfo specifics;
      specifics.codecType = kVideoCodecGeneric;

      uint8_t buffer[16] = {0};
      EncodedImage encoded(buffer, sizeof(buffer), sizeof(buffer));
      encoded.SetTimestamp(input_image.timestamp());
      encoded.capture_time_ms_ = input_image.render_time_ms();

      for (size_t i = 0; i < kNumStreams; ++i) {
        encoded._frameType = (*frame_types)[i];
        encoded._encodedWidth = kEncodedResolution[i].width;
        encoded._encodedHeight = kEncodedResolution[i].height;
        encoded.SetSpatialIndex(i);
        EncodedImageCallback* callback;
        {
          rtc::CritScope cs(&crit_sect_);
          callback = callback_;
        }
        RTC_DCHECK(callback);
        if (callback->OnEncodedImage(encoded, &specifics, nullptr).error !=
            EncodedImageCallback::Result::OK) {
          return -1;
        }
      }

      observation_complete_.Set();
      return 0;
    }
    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      send_config->encoder_settings.encoder_factory = &encoder_factory_;
      EXPECT_EQ(kNumStreams, encoder_config->number_of_streams);
    }

    size_t GetNumVideoStreams() const override { return kNumStreams; }

    void PerformTest() override {
      EXPECT_TRUE(Wait())
          << "Timed out while waiting for the encoder to send one frame.";
      VideoSendStream::Stats stats = send_stream_->GetStats();

      for (size_t i = 0; i < kNumStreams; ++i) {
        ASSERT_TRUE(stats.substreams.find(kVideoSendSsrcs[i]) !=
                    stats.substreams.end())
            << "No stats for SSRC: " << kVideoSendSsrcs[i]
            << ", stats should exist as soon as frames have been encoded.";
        VideoSendStream::StreamStats ssrc_stats =
            stats.substreams[kVideoSendSsrcs[i]];
        EXPECT_EQ(kEncodedResolution[i].width, ssrc_stats.width);
        EXPECT_EQ(kEncodedResolution[i].height, ssrc_stats.height);
      }
    }

    void OnVideoStreamsCreated(
        VideoSendStream* send_stream,
        const std::vector<VideoReceiveStream*>& receive_streams) override {
      send_stream_ = send_stream;
    }

    VideoSendStream* send_stream_;
    test::VideoEncoderProxyFactory encoder_factory_;
  } test;

  RunBaseTest(&test);
}

#if defined(RTC_ENABLE_VP9)
class Vp9HeaderObserver : public test::SendTest {
 public:
  Vp9HeaderObserver()
      : SendTest(VideoSendStreamTest::kLongTimeoutMs),
        encoder_factory_([]() { return VP9Encoder::Create(); }),
        vp9_settings_(VideoEncoder::GetDefaultVp9Settings()),
        packets_sent_(0),
        frames_sent_(0),
        expected_width_(0),
        expected_height_(0) {}

  virtual void ModifyVideoConfigsHook(
      VideoSendStream::Config* send_config,
      std::vector<VideoReceiveStream::Config>* receive_configs,
      VideoEncoderConfig* encoder_config) {}

  virtual void InspectHeader(const RTPVideoHeaderVP9& vp9) = 0;

 private:
  const int kVp9PayloadType = test::CallTest::kVideoSendPayloadType;

  void ModifyVideoConfigs(
      VideoSendStream::Config* send_config,
      std::vector<VideoReceiveStream::Config>* receive_configs,
      VideoEncoderConfig* encoder_config) override {
    send_config->encoder_settings.encoder_factory = &encoder_factory_;
    send_config->rtp.payload_name = "VP9";
    send_config->rtp.payload_type = kVp9PayloadType;
    ModifyVideoConfigsHook(send_config, receive_configs, encoder_config);
    encoder_config->encoder_specific_settings = new rtc::RefCountedObject<
        VideoEncoderConfig::Vp9EncoderSpecificSettings>(vp9_settings_);
    EXPECT_EQ(1u, encoder_config->number_of_streams);
    EXPECT_EQ(1u, encoder_config->simulcast_layers.size());
    encoder_config->simulcast_layers[0].num_temporal_layers =
        vp9_settings_.numberOfTemporalLayers;
    encoder_config_ = encoder_config->Copy();
  }

  void ModifyVideoCaptureStartResolution(int* width,
                                         int* height,
                                         int* frame_rate) override {
    expected_width_ = *width;
    expected_height_ = *height;
  }

  void PerformTest() override {
    EXPECT_TRUE(Wait()) << "Test timed out waiting for VP9 packet, num frames "
                        << frames_sent_;
  }

  Action OnSendRtp(const uint8_t* packet, size_t length) override {
    RTPHeader header;
    EXPECT_TRUE(parser_->Parse(packet, length, &header));

    EXPECT_EQ(kVp9PayloadType, header.payloadType);
    const uint8_t* payload = packet + header.headerLength;
    size_t payload_length = length - header.headerLength - header.paddingLength;

    bool new_packet = packets_sent_ == 0 ||
                      IsNewerSequenceNumber(header.sequenceNumber,
                                            last_header_.sequenceNumber);
    if (payload_length > 0 && new_packet) {
      RtpDepacketizer::ParsedPayload parsed;
      RtpDepacketizerVp9 depacketizer;
      EXPECT_TRUE(depacketizer.Parse(&parsed, payload, payload_length));
      EXPECT_EQ(VideoCodecType::kVideoCodecVP9, parsed.video_header().codec);
      // Verify common fields for all configurations.
      const auto& vp9_header =
          absl::get<RTPVideoHeaderVP9>(parsed.video_header().video_type_header);
      VerifyCommonHeader(vp9_header);
      CompareConsecutiveFrames(header, parsed.video_header());
      // Verify configuration specific settings.
      InspectHeader(vp9_header);

      ++packets_sent_;
      if (header.markerBit) {
        ++frames_sent_;
      }
      last_header_ = header;
      last_vp9_ = vp9_header;
    }
    return SEND_PACKET;
  }

 protected:
  bool ContinuousPictureId(const RTPVideoHeaderVP9& vp9) const {
    if (last_vp9_.picture_id > vp9.picture_id) {
      return vp9.picture_id == 0;  // Wrap.
    } else {
      return vp9.picture_id == last_vp9_.picture_id + 1;
    }
  }

  void VerifySpatialIdxWithinFrame(const RTPVideoHeaderVP9& vp9) const {
    bool new_layer = vp9.spatial_idx != last_vp9_.spatial_idx;
    EXPECT_EQ(new_layer, vp9.beginning_of_frame);
    EXPECT_EQ(new_layer, last_vp9_.end_of_frame);
    EXPECT_EQ(new_layer ? last_vp9_.spatial_idx + 1 : last_vp9_.spatial_idx,
              vp9.spatial_idx);
  }

  void VerifyFixedTemporalLayerStructure(const RTPVideoHeaderVP9& vp9,
                                         uint8_t num_layers) const {
    switch (num_layers) {
      case 0:
        VerifyTemporalLayerStructure0(vp9);
        break;
      case 1:
        VerifyTemporalLayerStructure1(vp9);
        break;
      case 2:
        VerifyTemporalLayerStructure2(vp9);
        break;
      case 3:
        VerifyTemporalLayerStructure3(vp9);
        break;
      default:
        RTC_NOTREACHED();
    }
  }

  void VerifyTemporalLayerStructure0(const RTPVideoHeaderVP9& vp9) const {
    EXPECT_EQ(kNoTl0PicIdx, vp9.tl0_pic_idx);
    EXPECT_EQ(kNoTemporalIdx, vp9.temporal_idx);  // no tid
    EXPECT_FALSE(vp9.temporal_up_switch);
  }

  void VerifyTemporalLayerStructure1(const RTPVideoHeaderVP9& vp9) const {
    EXPECT_NE(kNoTl0PicIdx, vp9.tl0_pic_idx);
    EXPECT_EQ(0, vp9.temporal_idx);  // 0,0,0,...
    EXPECT_FALSE(vp9.temporal_up_switch);
  }

  void VerifyTemporalLayerStructure2(const RTPVideoHeaderVP9& vp9) const {
    EXPECT_NE(kNoTl0PicIdx, vp9.tl0_pic_idx);
    EXPECT_GE(vp9.temporal_idx, 0);  // 0,1,0,1,... (tid reset on I-frames).
    EXPECT_LE(vp9.temporal_idx, 1);
    EXPECT_EQ(vp9.temporal_idx > 0, vp9.temporal_up_switch);
    if (IsNewPictureId(vp9)) {
      uint8_t expected_tid =
          (!vp9.inter_pic_predicted || last_vp9_.temporal_idx == 1) ? 0 : 1;
      EXPECT_EQ(expected_tid, vp9.temporal_idx);
    }
  }

  void VerifyTemporalLayerStructure3(const RTPVideoHeaderVP9& vp9) const {
    EXPECT_NE(kNoTl0PicIdx, vp9.tl0_pic_idx);
    EXPECT_GE(vp9.temporal_idx, 0);  // 0,2,1,2,... (tid reset on I-frames).
    EXPECT_LE(vp9.temporal_idx, 2);
    if (IsNewPictureId(vp9) && vp9.inter_pic_predicted) {
      EXPECT_NE(vp9.temporal_idx, last_vp9_.temporal_idx);
      switch (vp9.temporal_idx) {
        case 0:
          EXPECT_EQ(2, last_vp9_.temporal_idx);
          EXPECT_FALSE(vp9.temporal_up_switch);
          break;
        case 1:
          EXPECT_EQ(2, last_vp9_.temporal_idx);
          EXPECT_TRUE(vp9.temporal_up_switch);
          break;
        case 2:
          EXPECT_LT(last_vp9_.temporal_idx, 2);
          EXPECT_TRUE(vp9.temporal_up_switch);
          break;
      }
    }
  }

  void VerifyTl0Idx(const RTPVideoHeaderVP9& vp9) const {
    if (vp9.tl0_pic_idx == kNoTl0PicIdx)
      return;

    uint8_t expected_tl0_idx = last_vp9_.tl0_pic_idx;
    if (vp9.temporal_idx == 0)
      ++expected_tl0_idx;
    EXPECT_EQ(expected_tl0_idx, vp9.tl0_pic_idx);
  }

  bool IsNewPictureId(const RTPVideoHeaderVP9& vp9) const {
    return frames_sent_ > 0 && (vp9.picture_id != last_vp9_.picture_id);
  }

  // Flexible mode (F=1):    Non-flexible mode (F=0):
  //
  //      +-+-+-+-+-+-+-+-+     +-+-+-+-+-+-+-+-+
  //      |I|P|L|F|B|E|V|-|     |I|P|L|F|B|E|V|-|
  //      +-+-+-+-+-+-+-+-+     +-+-+-+-+-+-+-+-+
  // I:   |M| PICTURE ID  |  I: |M| PICTURE ID  |
  //      +-+-+-+-+-+-+-+-+     +-+-+-+-+-+-+-+-+
  // M:   | EXTENDED PID  |  M: | EXTENDED PID  |
  //      +-+-+-+-+-+-+-+-+     +-+-+-+-+-+-+-+-+
  // L:   |  T  |U|  S  |D|  L: |  T  |U|  S  |D|
  //      +-+-+-+-+-+-+-+-+     +-+-+-+-+-+-+-+-+
  // P,F: | P_DIFF    |X|N|     |   TL0PICIDX   |
  //      +-+-+-+-+-+-+-+-+     +-+-+-+-+-+-+-+-+
  // X:   |EXTENDED P_DIFF|  V: | SS  ..        |
  //      +-+-+-+-+-+-+-+-+     +-+-+-+-+-+-+-+-+
  // V:   | SS  ..        |
  //      +-+-+-+-+-+-+-+-+
  void VerifyCommonHeader(const RTPVideoHeaderVP9& vp9) const {
    EXPECT_EQ(kMaxTwoBytePictureId, vp9.max_picture_id);       // M:1
    EXPECT_NE(kNoPictureId, vp9.picture_id);                   // I:1
    EXPECT_EQ(vp9_settings_.flexibleMode, vp9.flexible_mode);  // F

    if (vp9_settings_.numberOfSpatialLayers > 1) {
      EXPECT_LT(vp9.spatial_idx, vp9_settings_.numberOfSpatialLayers);
    } else if (vp9_settings_.numberOfTemporalLayers > 1) {
      EXPECT_EQ(vp9.spatial_idx, 0);
    } else {
      EXPECT_EQ(vp9.spatial_idx, kNoSpatialIdx);
    }

    if (vp9_settings_.numberOfTemporalLayers > 1) {
      EXPECT_LT(vp9.temporal_idx, vp9_settings_.numberOfTemporalLayers);
    } else if (vp9_settings_.numberOfSpatialLayers > 1) {
      EXPECT_EQ(vp9.temporal_idx, 0);
    } else {
      EXPECT_EQ(vp9.temporal_idx, kNoTemporalIdx);
    }

    if (vp9.ss_data_available)  // V
      VerifySsData(vp9);

    if (frames_sent_ == 0)
      EXPECT_FALSE(vp9.inter_pic_predicted);  // P

    if (!vp9.inter_pic_predicted) {
      EXPECT_TRUE(vp9.temporal_idx == 0 || vp9.temporal_idx == kNoTemporalIdx);
      EXPECT_FALSE(vp9.temporal_up_switch);
    }
  }

  // Scalability structure (SS).
  //
  //      +-+-+-+-+-+-+-+-+
  // V:   | N_S |Y|G|-|-|-|
  //      +-+-+-+-+-+-+-+-+
  // Y:   |    WIDTH      |  N_S + 1 times
  //      +-+-+-+-+-+-+-+-+
  //      |    HEIGHT     |
  //      +-+-+-+-+-+-+-+-+
  // G:   |      N_G      |
  //      +-+-+-+-+-+-+-+-+
  // N_G: |  T  |U| R |-|-|  N_G times
  //      +-+-+-+-+-+-+-+-+
  //      |    P_DIFF     |  R times
  //      +-+-+-+-+-+-+-+-+
  void VerifySsData(const RTPVideoHeaderVP9& vp9) const {
    EXPECT_TRUE(vp9.ss_data_available);             // V
    EXPECT_EQ(vp9_settings_.numberOfSpatialLayers,  // N_S + 1
              vp9.num_spatial_layers);
    EXPECT_TRUE(vp9.spatial_layer_resolution_present);  // Y:1
    int expected_width = expected_width_;
    int expected_height = expected_height_;
    for (int i = static_cast<int>(vp9.num_spatial_layers) - 1; i >= 0; --i) {
      EXPECT_EQ(expected_width, vp9.width[i]);    // WIDTH
      EXPECT_EQ(expected_height, vp9.height[i]);  // HEIGHT
      expected_width /= 2;
      expected_height /= 2;
    }
  }

  void CompareConsecutiveFrames(const RTPHeader& header,
                                const RTPVideoHeader& video) const {
    const auto& vp9_header =
        absl::get<RTPVideoHeaderVP9>(video.video_type_header);

    bool new_frame = packets_sent_ == 0 ||
                     IsNewerTimestamp(header.timestamp, last_header_.timestamp);
    EXPECT_EQ(new_frame, video.is_first_packet_in_frame);
    if (!new_frame) {
      EXPECT_FALSE(last_header_.markerBit);
      EXPECT_EQ(last_header_.timestamp, header.timestamp);
      EXPECT_EQ(last_vp9_.picture_id, vp9_header.picture_id);
      EXPECT_EQ(last_vp9_.temporal_idx, vp9_header.temporal_idx);
      EXPECT_EQ(last_vp9_.tl0_pic_idx, vp9_header.tl0_pic_idx);
      VerifySpatialIdxWithinFrame(vp9_header);
      return;
    }
    // New frame.
    EXPECT_TRUE(vp9_header.beginning_of_frame);

    // Compare with last packet in previous frame.
    if (frames_sent_ == 0)
      return;
    EXPECT_TRUE(last_vp9_.end_of_frame);
    EXPECT_TRUE(last_header_.markerBit);
    EXPECT_TRUE(ContinuousPictureId(vp9_header));
    VerifyTl0Idx(vp9_header);
  }

  test::FunctionVideoEncoderFactory encoder_factory_;
  VideoCodecVP9 vp9_settings_;
  webrtc::VideoEncoderConfig encoder_config_;
  RTPHeader last_header_;
  RTPVideoHeaderVP9 last_vp9_;
  size_t packets_sent_;
  size_t frames_sent_;
  int expected_width_;
  int expected_height_;
};

TEST_P(VideoSendStreamTest, Vp9NonFlexMode_1Tl1SLayers) {
  const uint8_t kNumTemporalLayers = 1;
  const uint8_t kNumSpatialLayers = 1;
  TestVp9NonFlexMode(kNumTemporalLayers, kNumSpatialLayers);
}

TEST_P(VideoSendStreamTest, Vp9NonFlexMode_2Tl1SLayers) {
  const uint8_t kNumTemporalLayers = 2;
  const uint8_t kNumSpatialLayers = 1;
  TestVp9NonFlexMode(kNumTemporalLayers, kNumSpatialLayers);
}

TEST_P(VideoSendStreamTest, Vp9NonFlexMode_3Tl1SLayers) {
  const uint8_t kNumTemporalLayers = 3;
  const uint8_t kNumSpatialLayers = 1;
  TestVp9NonFlexMode(kNumTemporalLayers, kNumSpatialLayers);
}

TEST_P(VideoSendStreamTest, Vp9NonFlexMode_1Tl2SLayers) {
  const uint8_t kNumTemporalLayers = 1;
  const uint8_t kNumSpatialLayers = 2;
  TestVp9NonFlexMode(kNumTemporalLayers, kNumSpatialLayers);
}

TEST_P(VideoSendStreamTest, Vp9NonFlexMode_2Tl2SLayers) {
  const uint8_t kNumTemporalLayers = 2;
  const uint8_t kNumSpatialLayers = 2;
  TestVp9NonFlexMode(kNumTemporalLayers, kNumSpatialLayers);
}

TEST_P(VideoSendStreamTest, Vp9NonFlexMode_3Tl2SLayers) {
  const uint8_t kNumTemporalLayers = 3;
  const uint8_t kNumSpatialLayers = 2;
  TestVp9NonFlexMode(kNumTemporalLayers, kNumSpatialLayers);
}

void VideoSendStreamTest::TestVp9NonFlexMode(uint8_t num_temporal_layers,
                                             uint8_t num_spatial_layers) {
  static const size_t kNumFramesToSend = 100;
  // Set to < kNumFramesToSend and coprime to length of temporal layer
  // structures to verify temporal id reset on key frame.
  static const int kKeyFrameInterval = 31;

  static const int kWidth = kMinVp9SpatialLayerWidth;
  static const int kHeight = kMinVp9SpatialLayerHeight;
  static const float kGoodBitsPerPixel = 0.1f;
  class NonFlexibleMode : public Vp9HeaderObserver {
   public:
    NonFlexibleMode(uint8_t num_temporal_layers, uint8_t num_spatial_layers)
        : num_temporal_layers_(num_temporal_layers),
          num_spatial_layers_(num_spatial_layers),
          l_field_(num_temporal_layers > 1 || num_spatial_layers > 1) {}

    void ModifyVideoConfigsHook(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      encoder_config->codec_type = kVideoCodecVP9;
      int bitrate_bps = 0;
      for (int sl_idx = 0; sl_idx < num_spatial_layers_; ++sl_idx) {
        const int width = kWidth << sl_idx;
        const int height = kHeight << sl_idx;
        const float bpp = kGoodBitsPerPixel / (1 << sl_idx);
        bitrate_bps += static_cast<int>(width * height * bpp * 30);
      }
      encoder_config->max_bitrate_bps = bitrate_bps * 2;

      vp9_settings_.flexibleMode = false;
      vp9_settings_.frameDroppingOn = false;
      vp9_settings_.keyFrameInterval = kKeyFrameInterval;
      vp9_settings_.numberOfTemporalLayers = num_temporal_layers_;
      vp9_settings_.numberOfSpatialLayers = num_spatial_layers_;
    }

    void ModifyVideoCaptureStartResolution(int* width,
                                           int* height,
                                           int* frame_rate) override {
      expected_width_ = kWidth << (num_spatial_layers_ - 1);
      expected_height_ = kHeight << (num_spatial_layers_ - 1);
      *width = expected_width_;
      *height = expected_height_;
    }

    void InspectHeader(const RTPVideoHeaderVP9& vp9) override {
      bool ss_data_expected =
          !vp9.inter_pic_predicted && vp9.beginning_of_frame &&
          (vp9.spatial_idx == 0 || vp9.spatial_idx == kNoSpatialIdx);
      EXPECT_EQ(ss_data_expected, vp9.ss_data_available);
      if (num_spatial_layers_ > 1) {
        EXPECT_EQ(vp9.spatial_idx > 0, vp9.inter_layer_predicted);
      } else {
        EXPECT_FALSE(vp9.inter_layer_predicted);
      }

      EXPECT_EQ(!vp9.inter_pic_predicted,
                frames_sent_ % kKeyFrameInterval == 0);

      if (IsNewPictureId(vp9)) {
        if (num_temporal_layers_ == 1 && num_spatial_layers_ == 1) {
          EXPECT_EQ(kNoSpatialIdx, vp9.spatial_idx);
        } else {
          EXPECT_EQ(0, vp9.spatial_idx);
        }
        if (num_spatial_layers_ > 1)
          EXPECT_EQ(num_spatial_layers_ - 1, last_vp9_.spatial_idx);
      }

      VerifyFixedTemporalLayerStructure(vp9,
                                        l_field_ ? num_temporal_layers_ : 0);

      if (frames_sent_ > kNumFramesToSend)
        observation_complete_.Set();
    }
    const uint8_t num_temporal_layers_;
    const uint8_t num_spatial_layers_;
    const bool l_field_;
  } test(num_temporal_layers, num_spatial_layers);

  RunBaseTest(&test);
}

TEST_P(VideoSendStreamTest, Vp9NonFlexModeSmallResolution) {
  static const size_t kNumFramesToSend = 50;
  static const int kWidth = 4;
  static const int kHeight = 4;
  class NonFlexibleModeResolution : public Vp9HeaderObserver {
    void ModifyVideoConfigsHook(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      encoder_config->codec_type = kVideoCodecVP9;
      vp9_settings_.flexibleMode = false;
      vp9_settings_.numberOfTemporalLayers = 1;
      vp9_settings_.numberOfSpatialLayers = 1;

      EXPECT_EQ(1u, encoder_config->number_of_streams);
    }

    void InspectHeader(const RTPVideoHeaderVP9& vp9_header) override {
      if (frames_sent_ > kNumFramesToSend)
        observation_complete_.Set();
    }

    void ModifyVideoCaptureStartResolution(int* width,
                                           int* height,
                                           int* frame_rate) override {
      expected_width_ = kWidth;
      expected_height_ = kHeight;
      *width = kWidth;
      *height = kHeight;
    }
  } test;

  RunBaseTest(&test);
}

#if defined(WEBRTC_ANDROID)
// Crashes on Android; bugs.webrtc.org/7401
#define MAYBE_Vp9FlexModeRefCount DISABLED_Vp9FlexModeRefCount
#else
// TODO(webrtc:9270): Support of flexible mode is temporarily disabled. Enable
// the test after webrtc:9270 is implemented.
#define MAYBE_Vp9FlexModeRefCount DISABLED_Vp9FlexModeRefCount
// #define MAYBE_Vp9FlexModeRefCount Vp9FlexModeRefCount
#endif
TEST_P(VideoSendStreamTest, MAYBE_Vp9FlexModeRefCount) {
  class FlexibleMode : public Vp9HeaderObserver {
    void ModifyVideoConfigsHook(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      encoder_config->codec_type = kVideoCodecVP9;
      encoder_config->content_type = VideoEncoderConfig::ContentType::kScreen;
      vp9_settings_.flexibleMode = true;
      vp9_settings_.numberOfTemporalLayers = 1;
      vp9_settings_.numberOfSpatialLayers = 2;
    }

    void InspectHeader(const RTPVideoHeaderVP9& vp9_header) override {
      EXPECT_TRUE(vp9_header.flexible_mode);
      EXPECT_EQ(kNoTl0PicIdx, vp9_header.tl0_pic_idx);
      if (vp9_header.inter_pic_predicted) {
        EXPECT_GT(vp9_header.num_ref_pics, 0u);
        observation_complete_.Set();
      }
    }
  } test;

  RunBaseTest(&test);
}
#endif  // defined(RTC_ENABLE_VP9)

void VideoSendStreamTest::TestRequestSourceRotateVideo(
    bool support_orientation_ext) {
  CreateSenderCall();

  test::NullTransport transport;
  CreateSendConfig(1, 0, 0, &transport);
  GetVideoSendConfig()->rtp.extensions.clear();
  if (support_orientation_ext) {
    GetVideoSendConfig()->rtp.extensions.push_back(
        RtpExtension(RtpExtension::kVideoRotationUri, 1));
  }

  CreateVideoStreams();
  test::FrameForwarder forwarder;
  GetVideoSendStream()->SetSource(&forwarder,
                                  DegradationPreference::MAINTAIN_FRAMERATE);

  EXPECT_TRUE(forwarder.sink_wants().rotation_applied !=
              support_orientation_ext);

  DestroyStreams();
}

TEST_P(VideoSendStreamTest,
       RequestSourceRotateIfVideoOrientationExtensionNotSupported) {
  TestRequestSourceRotateVideo(false);
}

TEST_P(VideoSendStreamTest,
       DoNotRequestsRotationIfVideoOrientationExtensionSupported) {
  TestRequestSourceRotateVideo(true);
}

TEST_P(VideoSendStreamTest, EncoderConfigMaxFramerateReportedToSource) {
  static const int kMaxFps = 22;
  class FpsObserver : public test::SendTest,
                      public test::FrameGeneratorCapturer::SinkWantsObserver {
   public:
    FpsObserver() : SendTest(kDefaultTimeoutMs) {}

    void OnFrameGeneratorCapturerCreated(
        test::FrameGeneratorCapturer* frame_generator_capturer) override {
      frame_generator_capturer->SetSinkWantsObserver(this);
    }

    void OnSinkWantsChanged(rtc::VideoSinkInterface<VideoFrame>* sink,
                            const rtc::VideoSinkWants& wants) override {
      if (wants.max_framerate_fps == kMaxFps)
        observation_complete_.Set();
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      encoder_config->simulcast_layers[0].max_framerate = kMaxFps;
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait()) << "Timed out while waiting for fps to be reported.";
    }
  } test;

  RunBaseTest(&test);
}

// This test verifies that overhead is removed from the bandwidth estimate by
// testing that the maximum possible target payload rate is smaller than the
// maximum bandwidth estimate by the overhead rate.
TEST_P(VideoSendStreamTest, RemoveOverheadFromBandwidth) {
  test::ScopedFieldTrials override_field_trials(
      "WebRTC-SendSideBwe-WithOverhead/Enabled/");
  class RemoveOverheadFromBandwidthTest : public test::EndToEndTest,
                                          public test::FakeEncoder {
   public:
    explicit RemoveOverheadFromBandwidthTest(
        test::SingleThreadedTaskQueueForTesting* task_queue)
        : EndToEndTest(test::CallTest::kDefaultTimeoutMs),
          FakeEncoder(Clock::GetRealTimeClock()),
          task_queue_(task_queue),
          encoder_factory_(this),
          call_(nullptr),
          max_bitrate_bps_(0),
          first_packet_sent_(false) {}

    int32_t SetRateAllocation(const VideoBitrateAllocation& bitrate,
                              uint32_t frameRate) override {
      rtc::CritScope lock(&crit_);
      // Wait for the first sent packet so that videosendstream knows
      // rtp_overhead.
      if (first_packet_sent_) {
        max_bitrate_bps_ = bitrate.get_sum_bps();
        bitrate_changed_event_.Set();
      }
      return FakeEncoder::SetRateAllocation(bitrate, frameRate);
    }

    void OnCallsCreated(Call* sender_call, Call* receiver_call) override {
      call_ = sender_call;
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      send_config->rtp.max_packet_size = 1200;
      send_config->encoder_settings.encoder_factory = &encoder_factory_;
      EXPECT_FALSE(send_config->rtp.extensions.empty());
    }

    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      rtc::CritScope lock(&crit_);
      first_packet_sent_ = true;
      return SEND_PACKET;
    }

    void PerformTest() override {
      BitrateConstraints bitrate_config;
      constexpr int kStartBitrateBps = 60000;
      constexpr int kMaxBitrateBps = 60000;
      constexpr int kMinBitrateBps = 10000;
      bitrate_config.start_bitrate_bps = kStartBitrateBps;
      bitrate_config.max_bitrate_bps = kMaxBitrateBps;
      bitrate_config.min_bitrate_bps = kMinBitrateBps;
      task_queue_->SendTask([this, &bitrate_config]() {
        call_->GetTransportControllerSend()->SetSdpBitrateParameters(
            bitrate_config);
        call_->GetTransportControllerSend()->OnTransportOverheadChanged(40);
      });

      // At a bitrate of 60kbps with a packet size of 1200B video and an
      // overhead of 40B per packet video produces 2240bps overhead.
      // So the encoder BW should be set to 57760bps.
      EXPECT_TRUE(
          bitrate_changed_event_.Wait(VideoSendStreamTest::kDefaultTimeoutMs));
      {
        rtc::CritScope lock(&crit_);
        EXPECT_LE(max_bitrate_bps_, 57760u);
      }
    }

   private:
    test::SingleThreadedTaskQueueForTesting* const task_queue_;
    test::VideoEncoderProxyFactory encoder_factory_;
    Call* call_;
    rtc::CriticalSection crit_;
    uint32_t max_bitrate_bps_ RTC_GUARDED_BY(&crit_);
    bool first_packet_sent_ RTC_GUARDED_BY(&crit_);
    rtc::Event bitrate_changed_event_;
  } test(&task_queue_);
  RunBaseTest(&test);
}

TEST_P(VideoSendStreamTest, SendsKeepAlive) {
  const int kTimeoutMs = 50;  // Really short timeout for testing.

  class KeepaliveObserver : public test::SendTest {
   public:
    KeepaliveObserver() : SendTest(kDefaultTimeoutMs) {}

    void OnRtpTransportControllerSendCreated(
        RtpTransportControllerSend* controller) override {
      RtpKeepAliveConfig config;
      config.timeout_interval_ms = kTimeoutMs;
      config.payload_type = CallTest::kDefaultKeepalivePayloadType;
      controller->SetKeepAliveConfig(config);
    }

   private:
    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));

      if (header.payloadType != CallTest::kDefaultKeepalivePayloadType) {
        // The video stream has started. Stop it now.
        if (capturer_)
          capturer_->Stop();
      } else {
        observation_complete_.Set();
      }

      return SEND_PACKET;
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait()) << "Timed out while waiting for keep-alive packet.";
    }

    void OnFrameGeneratorCapturerCreated(
        test::FrameGeneratorCapturer* frame_generator_capturer) override {
      capturer_ = frame_generator_capturer;
    }

    test::FrameGeneratorCapturer* capturer_ = nullptr;
  } test;

  RunBaseTest(&test);
}

class PacingFactorObserver : public test::SendTest {
 public:
  PacingFactorObserver(bool configure_send_side,
                       absl::optional<float> expected_pacing_factor)
      : test::SendTest(VideoSendStreamTest::kDefaultTimeoutMs),
        configure_send_side_(configure_send_side),
        expected_pacing_factor_(expected_pacing_factor) {}

  void ModifyVideoConfigs(
      VideoSendStream::Config* send_config,
      std::vector<VideoReceiveStream::Config>* receive_configs,
      VideoEncoderConfig* encoder_config) override {
    // Check if send-side bwe extension is already present, and remove it if
    // it is not desired.
    bool has_send_side = false;
    for (auto it = send_config->rtp.extensions.begin();
         it != send_config->rtp.extensions.end(); ++it) {
      if (it->uri == RtpExtension::kTransportSequenceNumberUri) {
        if (configure_send_side_) {
          has_send_side = true;
        } else {
          send_config->rtp.extensions.erase(it);
        }
        break;
      }
    }

    if (configure_send_side_ && !has_send_side) {
      // Want send side, not present by default, so add it.
      send_config->rtp.extensions.emplace_back(
          RtpExtension::kTransportSequenceNumberUri,
          RtpExtension::kTransportSequenceNumberDefaultId);
    }

    // ALR only enabled for screenshare.
    encoder_config->content_type = VideoEncoderConfig::ContentType::kScreen;
  }

  void OnVideoStreamsCreated(
      VideoSendStream* send_stream,
      const std::vector<VideoReceiveStream*>& receive_streams) override {
    auto internal_send_peer = test::VideoSendStreamPeer(send_stream);
    // Video streams created, check that pacing factor is correctly configured.
    EXPECT_EQ(expected_pacing_factor_,
              internal_send_peer.GetPacingFactorOverride());
    observation_complete_.Set();
  }

  void PerformTest() override {
    EXPECT_TRUE(Wait()) << "Timed out while waiting for stream creation.";
  }

 private:
  const bool configure_send_side_;
  const absl::optional<float> expected_pacing_factor_;
};

std::string GetAlrProbingExperimentString() {
  return std::string(
             AlrExperimentSettings::kScreenshareProbingBweExperimentName) +
         "/1.0,2875,80,40,-60,3/";
}
const float kAlrProbingExperimentPaceMultiplier = 1.0f;

TEST_P(VideoSendStreamTest, AlrConfiguredWhenSendSideOn) {
  test::ScopedFieldTrials alr_experiment(GetAlrProbingExperimentString());
  // Send-side bwe on, use pacing factor from |kAlrProbingExperiment| above.
  PacingFactorObserver test_with_send_side(true,
                                           kAlrProbingExperimentPaceMultiplier);
  RunBaseTest(&test_with_send_side);
}

TEST_P(VideoSendStreamTest, AlrNotConfiguredWhenSendSideOff) {
  test::ScopedFieldTrials alr_experiment(GetAlrProbingExperimentString());
  // Send-side bwe off, use configuration should not be overridden.
  PacingFactorObserver test_without_send_side(false, absl::nullopt);
  RunBaseTest(&test_without_send_side);
}

// Test class takes as argument a function pointer to reset the send
// stream and call OnVideoStreamsCreated. This is necessary since you cannot
// change the content type of a VideoSendStream, you need to recreate it.
// Stopping and recreating the stream can only be done on the main thread and in
// the context of VideoSendStreamTest (not BaseTest). The test switches from
// realtime to screenshare and back.
template <typename T>
class ContentSwitchTest : public test::SendTest {
 public:
  enum class StreamState {
    kBeforeSwitch = 0,
    kInScreenshare = 1,
    kAfterSwitchBack = 2,
  };
  static const uint32_t kMinPacketsToSend = 50;

  explicit ContentSwitchTest(T* stream_reset_fun)
      : SendTest(test::CallTest::kDefaultTimeoutMs),
        call_(nullptr),
        state_(StreamState::kBeforeSwitch),
        send_stream_(nullptr),
        send_stream_config_(nullptr),
        packets_sent_(0),
        stream_resetter_(stream_reset_fun) {
    RTC_DCHECK(stream_resetter_);
  }

  void OnVideoStreamsCreated(
      VideoSendStream* send_stream,
      const std::vector<VideoReceiveStream*>& receive_streams) override {
    rtc::CritScope lock(&crit_);
    send_stream_ = send_stream;
  }

  void ModifyVideoConfigs(
      VideoSendStream::Config* send_config,
      std::vector<VideoReceiveStream::Config>* receive_configs,
      VideoEncoderConfig* encoder_config) override {
    RTC_DCHECK_EQ(1, encoder_config->number_of_streams);
    encoder_config->min_transmit_bitrate_bps = 0;
    encoder_config->content_type =
        VideoEncoderConfig::ContentType::kRealtimeVideo;
    send_stream_config_ = send_config->Copy();
    encoder_config_ = encoder_config->Copy();
  }

  void OnCallsCreated(Call* sender_call, Call* receiver_call) override {
    call_ = sender_call;
  }

  Action OnSendRtp(const uint8_t* packet, size_t length) override {
    rtc::CritScope lock(&crit_);

    auto internal_send_peer = test::VideoSendStreamPeer(send_stream_);
    float pacing_factor =
        internal_send_peer.GetPacingFactorOverride().value_or(0.0f);
    float expected_pacing_factor = PacedSender::kDefaultPaceMultiplier;
    if (send_stream_->GetStats().content_type ==
        webrtc::VideoContentType::SCREENSHARE) {
      expected_pacing_factor = 1.0f;  // Currently used pacing factor in ALR.
    }

    EXPECT_NEAR(expected_pacing_factor, pacing_factor, 1e-6);

    // Wait until at least kMinPacketsToSend packets to be sent, so that
    // some frames would be encoded.
    if (++packets_sent_ < kMinPacketsToSend)
      return SEND_PACKET;

    if (state_ != StreamState::kAfterSwitchBack) {
      // We've sent kMinPacketsToSend packets, switch the content type and move
      // move to the next state.
      // Note that we need to recreate the stream if changing content type.
      packets_sent_ = 0;
      if (encoder_config_.content_type ==
          VideoEncoderConfig::ContentType::kRealtimeVideo) {
        encoder_config_.content_type = VideoEncoderConfig::ContentType::kScreen;
      } else {
        encoder_config_.content_type =
            VideoEncoderConfig::ContentType::kRealtimeVideo;
      }
      switch (state_) {
        case StreamState::kBeforeSwitch:
          state_ = StreamState::kInScreenshare;
          break;
        case StreamState::kInScreenshare:
          state_ = StreamState::kAfterSwitchBack;
          break;
        case StreamState::kAfterSwitchBack:
          RTC_NOTREACHED();
          break;
      }
      content_switch_event_.Set();
      return SEND_PACKET;
    }

    observation_complete_.Set();
    return SEND_PACKET;
  }

  void PerformTest() override {
    while (GetStreamState() != StreamState::kAfterSwitchBack) {
      ASSERT_TRUE(
          content_switch_event_.Wait(test::CallTest::kDefaultTimeoutMs));
      (*stream_resetter_)(send_stream_config_, encoder_config_, this);
    }

    ASSERT_TRUE(Wait())
        << "Timed out waiting for a frame sent after switch back";
  }

 private:
  StreamState GetStreamState() {
    rtc::CritScope lock(&crit_);
    return state_;
  }

  rtc::CriticalSection crit_;
  rtc::Event content_switch_event_;
  Call* call_;
  StreamState state_ RTC_GUARDED_BY(crit_);
  VideoSendStream* send_stream_ RTC_GUARDED_BY(crit_);
  VideoSendStream::Config send_stream_config_;
  VideoEncoderConfig encoder_config_;
  uint32_t packets_sent_ RTC_GUARDED_BY(crit_);
  T* stream_resetter_;
};

TEST_P(VideoSendStreamTest, SwitchesToScreenshareAndBack) {
  auto reset_fun = [this](const VideoSendStream::Config& send_stream_config,
                          const VideoEncoderConfig& encoder_config,
                          test::BaseTest* test) {
    task_queue_.SendTask([this, &send_stream_config, &encoder_config, &test]() {
      Stop();
      DestroyVideoSendStreams();
      SetVideoSendConfig(send_stream_config);
      SetVideoEncoderConfig(encoder_config);
      CreateVideoSendStreams();
      SetVideoDegradation(DegradationPreference::MAINTAIN_RESOLUTION);
      test->OnVideoStreamsCreated(GetVideoSendStream(), video_receive_streams_);
      Start();
    });
  };
  ContentSwitchTest<decltype(reset_fun)> test(&reset_fun);
  RunBaseTest(&test);
}

}  // namespace webrtc
