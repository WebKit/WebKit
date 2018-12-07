/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/simulated_network.h"
#include "api/test/video/function_video_encoder_factory.h"
#include "call/fake_network_pipe.h"
#include "call/simulated_network.h"
#include "modules/rtp_rtcp/source/rtp_utility.h"
#include "modules/video_coding/include/video_coding_defines.h"
#include "rtc_base/strings/string_builder.h"
#include "system_wrappers/include/metrics.h"
#include "system_wrappers/include/sleep.h"
#include "test/call_test.h"
#include "test/fake_encoder.h"
#include "test/gtest.h"
#include "test/rtcp_packet_parser.h"

namespace webrtc {
class StatsEndToEndTest : public test::CallTest {};

TEST_F(StatsEndToEndTest, GetStats) {
  static const int kStartBitrateBps = 3000000;
  static const int kExpectedRenderDelayMs = 20;

  class ReceiveStreamRenderer : public rtc::VideoSinkInterface<VideoFrame> {
   public:
    ReceiveStreamRenderer() {}

   private:
    void OnFrame(const VideoFrame& video_frame) override {}
  };

  class StatsObserver : public test::EndToEndTest,
                        public rtc::VideoSinkInterface<VideoFrame> {
   public:
    StatsObserver()
        : EndToEndTest(kLongTimeoutMs),
          encoder_factory_([]() {
            return absl::make_unique<test::DelayedEncoder>(
                Clock::GetRealTimeClock(), 10);
          }),
          send_stream_(nullptr),
          expected_send_ssrcs_() {}

   private:
    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      // Drop every 25th packet => 4% loss.
      static const int kPacketLossFrac = 25;
      RTPHeader header;
      RtpUtility::RtpHeaderParser parser(packet, length);
      if (parser.Parse(&header) &&
          expected_send_ssrcs_.find(header.ssrc) !=
              expected_send_ssrcs_.end() &&
          header.sequenceNumber % kPacketLossFrac == 0) {
        return DROP_PACKET;
      }
      check_stats_event_.Set();
      return SEND_PACKET;
    }

    Action OnSendRtcp(const uint8_t* packet, size_t length) override {
      check_stats_event_.Set();
      return SEND_PACKET;
    }

    Action OnReceiveRtp(const uint8_t* packet, size_t length) override {
      check_stats_event_.Set();
      return SEND_PACKET;
    }

    Action OnReceiveRtcp(const uint8_t* packet, size_t length) override {
      check_stats_event_.Set();
      return SEND_PACKET;
    }

    void OnFrame(const VideoFrame& video_frame) override {
      // Ensure that we have at least 5ms send side delay.
      SleepMs(5);
    }

    bool CheckReceiveStats() {
      for (size_t i = 0; i < receive_streams_.size(); ++i) {
        VideoReceiveStream::Stats stats = receive_streams_[i]->GetStats();
        EXPECT_EQ(expected_receive_ssrcs_[i], stats.ssrc);

        // Make sure all fields have been populated.
        // TODO(pbos): Use CompoundKey if/when we ever know that all stats are
        // always filled for all receivers.
        receive_stats_filled_["IncomingRate"] |=
            stats.network_frame_rate != 0 || stats.total_bitrate_bps != 0;

        send_stats_filled_["DecoderImplementationName"] |=
            stats.decoder_implementation_name ==
            test::FakeDecoder::kImplementationName;
        receive_stats_filled_["RenderDelayAsHighAsExpected"] |=
            stats.render_delay_ms >= kExpectedRenderDelayMs;

        receive_stats_filled_["FrameCallback"] |= stats.decode_frame_rate != 0;

        receive_stats_filled_["FrameRendered"] |= stats.render_frame_rate != 0;

        receive_stats_filled_["StatisticsUpdated"] |=
            stats.rtcp_stats.packets_lost != 0 ||
            stats.rtcp_stats.extended_highest_sequence_number != 0 ||
            stats.rtcp_stats.fraction_lost != 0 || stats.rtcp_stats.jitter != 0;

        receive_stats_filled_["DataCountersUpdated"] |=
            stats.rtp_stats.transmitted.payload_bytes != 0 ||
            stats.rtp_stats.fec.packets != 0 ||
            stats.rtp_stats.transmitted.header_bytes != 0 ||
            stats.rtp_stats.transmitted.packets != 0 ||
            stats.rtp_stats.transmitted.padding_bytes != 0 ||
            stats.rtp_stats.retransmitted.packets != 0;

        receive_stats_filled_["CodecStats"] |=
            stats.target_delay_ms != 0 || stats.discarded_packets != 0;

        receive_stats_filled_["FrameCounts"] |=
            stats.frame_counts.key_frames != 0 ||
            stats.frame_counts.delta_frames != 0;

        receive_stats_filled_["CName"] |= !stats.c_name.empty();

        receive_stats_filled_["RtcpPacketTypeCount"] |=
            stats.rtcp_packet_type_counts.fir_packets != 0 ||
            stats.rtcp_packet_type_counts.nack_packets != 0 ||
            stats.rtcp_packet_type_counts.pli_packets != 0 ||
            stats.rtcp_packet_type_counts.nack_requests != 0 ||
            stats.rtcp_packet_type_counts.unique_nack_requests != 0;

        assert(stats.current_payload_type == -1 ||
               stats.current_payload_type == kFakeVideoSendPayloadType);
        receive_stats_filled_["IncomingPayloadType"] |=
            stats.current_payload_type == kFakeVideoSendPayloadType;
      }

      return AllStatsFilled(receive_stats_filled_);
    }

    bool CheckSendStats() {
      RTC_DCHECK(send_stream_);
      VideoSendStream::Stats stats = send_stream_->GetStats();

      size_t expected_num_streams =
          kNumSimulcastStreams + expected_send_ssrcs_.size();
      send_stats_filled_["NumStreams"] |=
          stats.substreams.size() == expected_num_streams;

      send_stats_filled_["CpuOveruseMetrics"] |=
          stats.avg_encode_time_ms != 0 && stats.encode_usage_percent != 0;

      send_stats_filled_["EncoderImplementationName"] |=
          stats.encoder_implementation_name ==
          test::FakeEncoder::kImplementationName;

      for (std::map<uint32_t, VideoSendStream::StreamStats>::const_iterator it =
               stats.substreams.begin();
           it != stats.substreams.end(); ++it) {
        if (expected_send_ssrcs_.find(it->first) == expected_send_ssrcs_.end())
          continue;  // Probably RTX.

        send_stats_filled_[CompoundKey("CapturedFrameRate", it->first)] |=
            stats.input_frame_rate != 0;

        const VideoSendStream::StreamStats& stream_stats = it->second;

        send_stats_filled_[CompoundKey("StatisticsUpdated", it->first)] |=
            stream_stats.rtcp_stats.packets_lost != 0 ||
            stream_stats.rtcp_stats.extended_highest_sequence_number != 0 ||
            stream_stats.rtcp_stats.fraction_lost != 0;

        send_stats_filled_[CompoundKey("DataCountersUpdated", it->first)] |=
            stream_stats.rtp_stats.fec.packets != 0 ||
            stream_stats.rtp_stats.transmitted.padding_bytes != 0 ||
            stream_stats.rtp_stats.retransmitted.packets != 0 ||
            stream_stats.rtp_stats.transmitted.packets != 0;

        send_stats_filled_[CompoundKey("BitrateStatisticsObserver.Total",
                                       it->first)] |=
            stream_stats.total_bitrate_bps != 0;

        send_stats_filled_[CompoundKey("BitrateStatisticsObserver.Retransmit",
                                       it->first)] |=
            stream_stats.retransmit_bitrate_bps != 0;

        send_stats_filled_[CompoundKey("FrameCountObserver", it->first)] |=
            stream_stats.frame_counts.delta_frames != 0 ||
            stream_stats.frame_counts.key_frames != 0;

        send_stats_filled_[CompoundKey("OutgoingRate", it->first)] |=
            stats.encode_frame_rate != 0;

        send_stats_filled_[CompoundKey("Delay", it->first)] |=
            stream_stats.avg_delay_ms != 0 || stream_stats.max_delay_ms != 0;

        // TODO(pbos): Use CompoundKey when the test makes sure that all SSRCs
        // report dropped packets.
        send_stats_filled_["RtcpPacketTypeCount"] |=
            stream_stats.rtcp_packet_type_counts.fir_packets != 0 ||
            stream_stats.rtcp_packet_type_counts.nack_packets != 0 ||
            stream_stats.rtcp_packet_type_counts.pli_packets != 0 ||
            stream_stats.rtcp_packet_type_counts.nack_requests != 0 ||
            stream_stats.rtcp_packet_type_counts.unique_nack_requests != 0;
      }

      return AllStatsFilled(send_stats_filled_);
    }

    std::string CompoundKey(const char* name, uint32_t ssrc) {
      rtc::StringBuilder oss;
      oss << name << "_" << ssrc;
      return oss.Release();
    }

    bool AllStatsFilled(const std::map<std::string, bool>& stats_map) {
      for (const auto& stat : stats_map) {
        if (!stat.second)
          return false;
      }
      return true;
    }

    test::PacketTransport* CreateSendTransport(
        test::SingleThreadedTaskQueueForTesting* task_queue,
        Call* sender_call) override {
      BuiltInNetworkBehaviorConfig network_config;
      network_config.loss_percent = 5;
      return new test::PacketTransport(
          task_queue, sender_call, this, test::PacketTransport::kSender,
          payload_type_map_,
          absl::make_unique<FakeNetworkPipe>(
              Clock::GetRealTimeClock(),
              absl::make_unique<SimulatedNetwork>(network_config)));
    }
    void ModifySenderBitrateConfig(
        BitrateConstraints* bitrate_config) override {
      bitrate_config->start_bitrate_bps = kStartBitrateBps;
    }

    // This test use other VideoStream settings than the the default settings
    // implemented in DefaultVideoStreamFactory. Therefore  this test implement
    // its own VideoEncoderConfig::VideoStreamFactoryInterface which is created
    // in ModifyVideoConfigs.
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
        // Set low simulcast bitrates to not have to wait for bandwidth ramp-up.
        for (size_t i = 0; i < encoder_config.number_of_streams; ++i) {
          streams[i].min_bitrate_bps = 10000;
          streams[i].target_bitrate_bps = 15000;
          streams[i].max_bitrate_bps = 20000;
        }
        return streams;
      }
    };

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      encoder_config->video_stream_factory =
          new rtc::RefCountedObject<VideoStreamFactory>();
      send_config->pre_encode_callback = this;  // Used to inject delay.
      expected_cname_ = send_config->rtp.c_name = "SomeCName";

      send_config->rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
      send_config->rtp.rtx.payload_type = kSendRtxPayloadType;

      const std::vector<uint32_t>& ssrcs = send_config->rtp.ssrcs;
      for (size_t i = 0; i < ssrcs.size(); ++i) {
        expected_send_ssrcs_.insert(ssrcs[i]);
        expected_receive_ssrcs_.push_back(
            (*receive_configs)[i].rtp.remote_ssrc);
        (*receive_configs)[i].render_delay_ms = kExpectedRenderDelayMs;
        (*receive_configs)[i].renderer = &receive_stream_renderer_;
        (*receive_configs)[i].rtp.nack.rtp_history_ms = kNackRtpHistoryMs;

        (*receive_configs)[i].rtp.rtx_ssrc = kSendRtxSsrcs[i];
        (*receive_configs)[i]
            .rtp.rtx_associated_payload_types[kSendRtxPayloadType] =
            kFakeVideoSendPayloadType;
      }

      for (size_t i = 0; i < kNumSimulcastStreams; ++i)
        send_config->rtp.rtx.ssrcs.push_back(kSendRtxSsrcs[i]);

      // Use a delayed encoder to make sure we see CpuOveruseMetrics stats that
      // are non-zero.
      send_config->encoder_settings.encoder_factory = &encoder_factory_;
    }

    size_t GetNumVideoStreams() const override { return kNumSimulcastStreams; }

    void OnVideoStreamsCreated(
        VideoSendStream* send_stream,
        const std::vector<VideoReceiveStream*>& receive_streams) override {
      send_stream_ = send_stream;
      receive_streams_ = receive_streams;
    }

    void PerformTest() override {
      Clock* clock = Clock::GetRealTimeClock();
      int64_t now = clock->TimeInMilliseconds();
      int64_t stop_time = now + test::CallTest::kLongTimeoutMs;
      bool receive_ok = false;
      bool send_ok = false;

      while (now < stop_time) {
        if (!receive_ok)
          receive_ok = CheckReceiveStats();
        if (!send_ok)
          send_ok = CheckSendStats();

        if (receive_ok && send_ok)
          return;

        int64_t time_until_timout_ = stop_time - now;
        if (time_until_timout_ > 0)
          check_stats_event_.Wait(time_until_timout_);
        now = clock->TimeInMilliseconds();
      }

      ADD_FAILURE() << "Timed out waiting for filled stats.";
      for (std::map<std::string, bool>::const_iterator it =
               receive_stats_filled_.begin();
           it != receive_stats_filled_.end(); ++it) {
        if (!it->second) {
          ADD_FAILURE() << "Missing receive stats: " << it->first;
        }
      }

      for (std::map<std::string, bool>::const_iterator it =
               send_stats_filled_.begin();
           it != send_stats_filled_.end(); ++it) {
        if (!it->second) {
          ADD_FAILURE() << "Missing send stats: " << it->first;
        }
      }
    }

    test::FunctionVideoEncoderFactory encoder_factory_;
    std::vector<VideoReceiveStream*> receive_streams_;
    std::map<std::string, bool> receive_stats_filled_;

    VideoSendStream* send_stream_;
    std::map<std::string, bool> send_stats_filled_;

    std::vector<uint32_t> expected_receive_ssrcs_;
    std::set<uint32_t> expected_send_ssrcs_;
    std::string expected_cname_;

    rtc::Event check_stats_event_;
    ReceiveStreamRenderer receive_stream_renderer_;
  } test;

  RunBaseTest(&test);
}

TEST_F(StatsEndToEndTest, TimingFramesAreReported) {
  static const int kExtensionId = 5;

  class StatsObserver : public test::EndToEndTest {
   public:
    StatsObserver() : EndToEndTest(kLongTimeoutMs) {}

   private:
    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      send_config->rtp.extensions.clear();
      send_config->rtp.extensions.push_back(
          RtpExtension(RtpExtension::kVideoTimingUri, kExtensionId));
      for (size_t i = 0; i < receive_configs->size(); ++i) {
        (*receive_configs)[i].rtp.extensions.clear();
        (*receive_configs)[i].rtp.extensions.push_back(
            RtpExtension(RtpExtension::kVideoTimingUri, kExtensionId));
      }
    }

    void OnVideoStreamsCreated(
        VideoSendStream* send_stream,
        const std::vector<VideoReceiveStream*>& receive_streams) override {
      receive_streams_ = receive_streams;
    }

    void PerformTest() override {
      // No frames reported initially.
      for (size_t i = 0; i < receive_streams_.size(); ++i) {
        EXPECT_FALSE(receive_streams_[i]->GetStats().timing_frame_info);
      }
      // Wait for at least one timing frame to be sent with 100ms grace period.
      SleepMs(kDefaultTimingFramesDelayMs + 100);
      // Check that timing frames are reported for each stream.
      for (size_t i = 0; i < receive_streams_.size(); ++i) {
        EXPECT_TRUE(receive_streams_[i]->GetStats().timing_frame_info);
      }
    }

    std::vector<VideoReceiveStream*> receive_streams_;
  } test;

  RunBaseTest(&test);
}

TEST_F(StatsEndToEndTest, TestReceivedRtpPacketStats) {
  static const size_t kNumRtpPacketsToSend = 5;
  class ReceivedRtpStatsObserver : public test::EndToEndTest {
   public:
    ReceivedRtpStatsObserver()
        : EndToEndTest(kDefaultTimeoutMs),
          receive_stream_(nullptr),
          sent_rtp_(0) {}

   private:
    void OnVideoStreamsCreated(
        VideoSendStream* send_stream,
        const std::vector<VideoReceiveStream*>& receive_streams) override {
      receive_stream_ = receive_streams[0];
    }

    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      if (sent_rtp_ >= kNumRtpPacketsToSend) {
        VideoReceiveStream::Stats stats = receive_stream_->GetStats();
        if (kNumRtpPacketsToSend == stats.rtp_stats.transmitted.packets) {
          observation_complete_.Set();
        }
        return DROP_PACKET;
      }
      ++sent_rtp_;
      return SEND_PACKET;
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait())
          << "Timed out while verifying number of received RTP packets.";
    }

    VideoReceiveStream* receive_stream_;
    uint32_t sent_rtp_;
  } test;

  RunBaseTest(&test);
}

#if defined(WEBRTC_WIN)
// Disabled due to flakiness on Windows (bugs.webrtc.org/7483).
#define MAYBE_ContentTypeSwitches DISABLED_ContentTypeSwitches
#else
#define MAYBE_ContentTypeSwitches ContentTypeSwitches
#endif
TEST_F(StatsEndToEndTest, MAYBE_ContentTypeSwitches) {
  class StatsObserver : public test::BaseTest,
                        public rtc::VideoSinkInterface<VideoFrame> {
   public:
    StatsObserver() : BaseTest(kLongTimeoutMs), num_frames_received_(0) {}

    bool ShouldCreateReceivers() const override { return true; }

    void OnFrame(const VideoFrame& video_frame) override {
      // The RTT is needed to estimate |ntp_time_ms| which is used by
      // end-to-end delay stats. Therefore, start counting received frames once
      // |ntp_time_ms| is valid.
      if (video_frame.ntp_time_ms() > 0 &&
          Clock::GetRealTimeClock()->CurrentNtpInMilliseconds() >=
              video_frame.ntp_time_ms()) {
        rtc::CritScope lock(&crit_);
        ++num_frames_received_;
      }
    }

    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      if (MinNumberOfFramesReceived())
        observation_complete_.Set();
      return SEND_PACKET;
    }

    bool MinNumberOfFramesReceived() const {
      // Have some room for frames with wrong content type during switch.
      const int kMinRequiredHistogramSamples = 200 + 50;
      rtc::CritScope lock(&crit_);
      return num_frames_received_ > kMinRequiredHistogramSamples;
    }

    // May be called several times.
    void PerformTest() override {
      EXPECT_TRUE(Wait()) << "Timed out waiting for enough packets.";
      // Reset frame counter so next PerformTest() call will do something.
      {
        rtc::CritScope lock(&crit_);
        num_frames_received_ = 0;
      }
    }

    rtc::CriticalSection crit_;
    int num_frames_received_ RTC_GUARDED_BY(&crit_);
  } test;

  metrics::Reset();

  Call::Config send_config(send_event_log_.get());
  test.ModifySenderBitrateConfig(&send_config.bitrate_config);
  Call::Config recv_config(recv_event_log_.get());
  test.ModifyReceiverBitrateConfig(&recv_config.bitrate_config);

  VideoEncoderConfig encoder_config_with_screenshare;

  task_queue_.SendTask([this, &test, &send_config, &recv_config,
                        &encoder_config_with_screenshare]() {
    CreateSenderCall(send_config);
    CreateReceiverCall(recv_config);

    receive_transport_.reset(test.CreateReceiveTransport(&task_queue_));
    send_transport_.reset(
        test.CreateSendTransport(&task_queue_, sender_call_.get()));
    send_transport_->SetReceiver(receiver_call_->Receiver());
    receive_transport_->SetReceiver(sender_call_->Receiver());

    receiver_call_->SignalChannelNetworkState(MediaType::VIDEO, kNetworkUp);
    CreateSendConfig(1, 0, 0, send_transport_.get());
    CreateMatchingReceiveConfigs(receive_transport_.get());

    // Modify send and receive configs.
    GetVideoSendConfig()->rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
    video_receive_configs_[0].rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
    video_receive_configs_[0].renderer = &test;
    // RTT needed for RemoteNtpTimeEstimator for the receive stream.
    video_receive_configs_[0].rtp.rtcp_xr.receiver_reference_time_report = true;
    // Start with realtime video.
    GetVideoEncoderConfig()->content_type =
        VideoEncoderConfig::ContentType::kRealtimeVideo;
    // Second encoder config for the second part of the test uses screenshare
    encoder_config_with_screenshare = GetVideoEncoderConfig()->Copy();
    encoder_config_with_screenshare.content_type =
        VideoEncoderConfig::ContentType::kScreen;

    CreateVideoStreams();
    CreateFrameGeneratorCapturer(kDefaultFramerate, kDefaultWidth,
                                 kDefaultHeight);
    Start();
  });

  test.PerformTest();

  // Replace old send stream.
  task_queue_.SendTask([this, &encoder_config_with_screenshare]() {
    DestroyVideoSendStreams();
    CreateVideoSendStream(encoder_config_with_screenshare);
    SetVideoDegradation(DegradationPreference::BALANCED);
    GetVideoSendStream()->Start();
  });

  // Continue to run test but now with screenshare.
  test.PerformTest();

  task_queue_.SendTask([this]() {
    Stop();
    DestroyStreams();
    send_transport_.reset();
    receive_transport_.reset();
    DestroyCalls();
  });

  // Verify that stats have been updated for both screenshare and video.
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.EndToEndDelayInMs"));
  EXPECT_EQ(1,
            metrics::NumSamples("WebRTC.Video.Screenshare.EndToEndDelayInMs"));
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.EndToEndDelayMaxInMs"));
  EXPECT_EQ(
      1, metrics::NumSamples("WebRTC.Video.Screenshare.EndToEndDelayMaxInMs"));
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.InterframeDelayInMs"));
  EXPECT_EQ(
      1, metrics::NumSamples("WebRTC.Video.Screenshare.InterframeDelayInMs"));
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.InterframeDelayMaxInMs"));
  EXPECT_EQ(1, metrics::NumSamples(
                   "WebRTC.Video.Screenshare.InterframeDelayMaxInMs"));
}

TEST_F(StatsEndToEndTest, VerifyNackStats) {
  static const int kPacketNumberToDrop = 200;
  class NackObserver : public test::EndToEndTest {
   public:
    NackObserver()
        : EndToEndTest(kLongTimeoutMs),
          sent_rtp_packets_(0),
          dropped_rtp_packet_(0),
          dropped_rtp_packet_requested_(false),
          send_stream_(nullptr),
          start_runtime_ms_(-1) {}

   private:
    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      rtc::CritScope lock(&crit_);
      if (++sent_rtp_packets_ == kPacketNumberToDrop) {
        std::unique_ptr<RtpHeaderParser> parser(RtpHeaderParser::Create());
        RTPHeader header;
        EXPECT_TRUE(parser->Parse(packet, length, &header));
        dropped_rtp_packet_ = header.sequenceNumber;
        return DROP_PACKET;
      }
      VerifyStats();
      return SEND_PACKET;
    }

    Action OnReceiveRtcp(const uint8_t* packet, size_t length) override {
      rtc::CritScope lock(&crit_);
      test::RtcpPacketParser rtcp_parser;
      rtcp_parser.Parse(packet, length);
      const std::vector<uint16_t>& nacks = rtcp_parser.nack()->packet_ids();
      if (!nacks.empty() && std::find(nacks.begin(), nacks.end(),
                                      dropped_rtp_packet_) != nacks.end()) {
        dropped_rtp_packet_requested_ = true;
      }
      return SEND_PACKET;
    }

    void VerifyStats() RTC_EXCLUSIVE_LOCKS_REQUIRED(&crit_) {
      if (!dropped_rtp_packet_requested_)
        return;
      int send_stream_nack_packets = 0;
      int receive_stream_nack_packets = 0;
      VideoSendStream::Stats stats = send_stream_->GetStats();
      for (std::map<uint32_t, VideoSendStream::StreamStats>::const_iterator it =
               stats.substreams.begin();
           it != stats.substreams.end(); ++it) {
        const VideoSendStream::StreamStats& stream_stats = it->second;
        send_stream_nack_packets +=
            stream_stats.rtcp_packet_type_counts.nack_packets;
      }
      for (size_t i = 0; i < receive_streams_.size(); ++i) {
        VideoReceiveStream::Stats stats = receive_streams_[i]->GetStats();
        receive_stream_nack_packets +=
            stats.rtcp_packet_type_counts.nack_packets;
      }
      if (send_stream_nack_packets >= 1 && receive_stream_nack_packets >= 1) {
        // NACK packet sent on receive stream and received on sent stream.
        if (MinMetricRunTimePassed())
          observation_complete_.Set();
      }
    }

    bool MinMetricRunTimePassed() {
      int64_t now = Clock::GetRealTimeClock()->TimeInMilliseconds();
      if (start_runtime_ms_ == -1) {
        start_runtime_ms_ = now;
        return false;
      }
      int64_t elapsed_sec = (now - start_runtime_ms_) / 1000;
      return elapsed_sec > metrics::kMinRunTimeInSeconds;
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      send_config->rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
      (*receive_configs)[0].rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
      (*receive_configs)[0].renderer = &fake_renderer_;
    }

    void OnVideoStreamsCreated(
        VideoSendStream* send_stream,
        const std::vector<VideoReceiveStream*>& receive_streams) override {
      send_stream_ = send_stream;
      receive_streams_ = receive_streams;
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait()) << "Timed out waiting for packet to be NACKed.";
    }

    test::FakeVideoRenderer fake_renderer_;
    rtc::CriticalSection crit_;
    uint64_t sent_rtp_packets_;
    uint16_t dropped_rtp_packet_ RTC_GUARDED_BY(&crit_);
    bool dropped_rtp_packet_requested_ RTC_GUARDED_BY(&crit_);
    std::vector<VideoReceiveStream*> receive_streams_;
    VideoSendStream* send_stream_;
    int64_t start_runtime_ms_;
  } test;

  metrics::Reset();
  RunBaseTest(&test);

  EXPECT_EQ(
      1, metrics::NumSamples("WebRTC.Video.UniqueNackRequestsSentInPercent"));
  EXPECT_EQ(1, metrics::NumSamples(
                   "WebRTC.Video.UniqueNackRequestsReceivedInPercent"));
  EXPECT_GT(metrics::MinSample("WebRTC.Video.NackPacketsSentPerMinute"), 0);
}

TEST_F(StatsEndToEndTest, CallReportsRttForSender) {
  static const int kSendDelayMs = 30;
  static const int kReceiveDelayMs = 70;

  std::unique_ptr<test::DirectTransport> sender_transport;
  std::unique_ptr<test::DirectTransport> receiver_transport;

  task_queue_.SendTask([this, &sender_transport, &receiver_transport]() {
    BuiltInNetworkBehaviorConfig config;
    config.queue_delay_ms = kSendDelayMs;
    CreateCalls();
    sender_transport = absl::make_unique<test::DirectTransport>(
        &task_queue_,
        absl::make_unique<FakeNetworkPipe>(
            Clock::GetRealTimeClock(),
            absl::make_unique<SimulatedNetwork>(config)),
        sender_call_.get(), payload_type_map_);
    config.queue_delay_ms = kReceiveDelayMs;
    receiver_transport = absl::make_unique<test::DirectTransport>(
        &task_queue_,
        absl::make_unique<FakeNetworkPipe>(
            Clock::GetRealTimeClock(),
            absl::make_unique<SimulatedNetwork>(config)),
        receiver_call_.get(), payload_type_map_);
    sender_transport->SetReceiver(receiver_call_->Receiver());
    receiver_transport->SetReceiver(sender_call_->Receiver());

    CreateSendConfig(1, 0, 0, sender_transport.get());
    CreateMatchingReceiveConfigs(receiver_transport.get());

    CreateVideoStreams();
    CreateFrameGeneratorCapturer(kDefaultFramerate, kDefaultWidth,
                                 kDefaultHeight);
    Start();
  });

  int64_t start_time_ms = clock_->TimeInMilliseconds();
  while (true) {
    Call::Stats stats = sender_call_->GetStats();
    ASSERT_GE(start_time_ms + kDefaultTimeoutMs, clock_->TimeInMilliseconds())
        << "No RTT stats before timeout!";
    if (stats.rtt_ms != -1) {
      // To avoid failures caused by rounding or minor ntp clock adjustments,
      // relax expectation by 1ms.
      constexpr int kAllowedErrorMs = 1;
      EXPECT_GE(stats.rtt_ms, kSendDelayMs + kReceiveDelayMs - kAllowedErrorMs);
      break;
    }
    SleepMs(10);
  }

  task_queue_.SendTask([this, &sender_transport, &receiver_transport]() {
    Stop();
    DestroyStreams();
    sender_transport.reset();
    receiver_transport.reset();
    DestroyCalls();
  });
}
}  // namespace webrtc
