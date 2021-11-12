/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "api/test/simulated_network.h"
#include "call/fake_network_pipe.h"
#include "call/simulated_network.h"
#include "modules/include/module_common_types_public.h"
#include "modules/rtp_rtcp/source/rtp_packet.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/task_queue_for_test.h"
#include "test/call_test.h"
#include "test/gtest.h"
#include "test/rtcp_packet_parser.h"

namespace webrtc {
namespace {
enum : int {  // The first valid value is 1.
  kTransportSequenceNumberExtensionId = 1,
};
}  // namespace

class RtpRtcpEndToEndTest : public test::CallTest {
 protected:
  void RespectsRtcpMode(RtcpMode rtcp_mode);
  void TestRtpStatePreservation(bool use_rtx, bool provoke_rtcpsr_before_rtp);
};

void RtpRtcpEndToEndTest::RespectsRtcpMode(RtcpMode rtcp_mode) {
  static const int kNumCompoundRtcpPacketsToObserve = 10;
  class RtcpModeObserver : public test::EndToEndTest {
   public:
    explicit RtcpModeObserver(RtcpMode rtcp_mode)
        : EndToEndTest(kDefaultTimeoutMs),
          rtcp_mode_(rtcp_mode),
          sent_rtp_(0),
          sent_rtcp_(0) {}

   private:
    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      MutexLock lock(&mutex_);
      if (++sent_rtp_ % 3 == 0)
        return DROP_PACKET;

      return SEND_PACKET;
    }

    Action OnReceiveRtcp(const uint8_t* packet, size_t length) override {
      MutexLock lock(&mutex_);
      ++sent_rtcp_;
      test::RtcpPacketParser parser;
      EXPECT_TRUE(parser.Parse(packet, length));

      EXPECT_EQ(0, parser.sender_report()->num_packets());

      switch (rtcp_mode_) {
        case RtcpMode::kCompound:
          // TODO(holmer): We shouldn't send transport feedback alone if
          // compound RTCP is negotiated.
          if (parser.receiver_report()->num_packets() == 0 &&
              parser.transport_feedback()->num_packets() == 0) {
            ADD_FAILURE() << "Received RTCP packet without receiver report for "
                             "RtcpMode::kCompound.";
            observation_complete_.Set();
          }

          if (sent_rtcp_ >= kNumCompoundRtcpPacketsToObserve)
            observation_complete_.Set();

          break;
        case RtcpMode::kReducedSize:
          if (parser.receiver_report()->num_packets() == 0)
            observation_complete_.Set();
          break;
        case RtcpMode::kOff:
          RTC_NOTREACHED();
          break;
      }

      return SEND_PACKET;
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      send_config->rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
      (*receive_configs)[0].rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
      (*receive_configs)[0].rtp.rtcp_mode = rtcp_mode_;
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait())
          << (rtcp_mode_ == RtcpMode::kCompound
                  ? "Timed out before observing enough compound packets."
                  : "Timed out before receiving a non-compound RTCP packet.");
    }

    RtcpMode rtcp_mode_;
    Mutex mutex_;
    // Must be protected since RTCP can be sent by both the process thread
    // and the pacer thread.
    int sent_rtp_ RTC_GUARDED_BY(&mutex_);
    int sent_rtcp_ RTC_GUARDED_BY(&mutex_);
  } test(rtcp_mode);

  RunBaseTest(&test);
}

TEST_F(RtpRtcpEndToEndTest, UsesRtcpCompoundMode) {
  RespectsRtcpMode(RtcpMode::kCompound);
}

TEST_F(RtpRtcpEndToEndTest, UsesRtcpReducedSizeMode) {
  RespectsRtcpMode(RtcpMode::kReducedSize);
}

void RtpRtcpEndToEndTest::TestRtpStatePreservation(
    bool use_rtx,
    bool provoke_rtcpsr_before_rtp) {
  // This test uses other VideoStream settings than the the default settings
  // implemented in DefaultVideoStreamFactory. Therefore this test implements
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

      if (encoder_config.number_of_streams > 1) {
        // Lower bitrates so that all streams send initially.
        RTC_DCHECK_EQ(3, encoder_config.number_of_streams);
        for (size_t i = 0; i < encoder_config.number_of_streams; ++i) {
          streams[i].min_bitrate_bps = 10000;
          streams[i].target_bitrate_bps = 15000;
          streams[i].max_bitrate_bps = 20000;
        }
      } else {
        // Use the same total bitrates when sending a single stream to avoid
        // lowering
        // the bitrate estimate and requiring a subsequent rampup.
        streams[0].min_bitrate_bps = 3 * 10000;
        streams[0].target_bitrate_bps = 3 * 15000;
        streams[0].max_bitrate_bps = 3 * 20000;
      }
      return streams;
    }
  };

  class RtpSequenceObserver : public test::RtpRtcpObserver {
   public:
    explicit RtpSequenceObserver(bool use_rtx)
        : test::RtpRtcpObserver(kDefaultTimeoutMs),
          ssrcs_to_observe_(kNumSimulcastStreams) {
      for (size_t i = 0; i < kNumSimulcastStreams; ++i) {
        ssrc_is_rtx_[kVideoSendSsrcs[i]] = false;
        if (use_rtx)
          ssrc_is_rtx_[kSendRtxSsrcs[i]] = true;
      }
    }

    void ResetExpectedSsrcs(size_t num_expected_ssrcs) {
      MutexLock lock(&mutex_);
      ssrc_observed_.clear();
      ssrcs_to_observe_ = num_expected_ssrcs;
    }

   private:
    void ValidateTimestampGap(uint32_t ssrc,
                              uint32_t timestamp,
                              bool only_padding)
        RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
      static const int32_t kMaxTimestampGap = kDefaultTimeoutMs * 90;
      auto timestamp_it = last_observed_timestamp_.find(ssrc);
      if (timestamp_it == last_observed_timestamp_.end()) {
        EXPECT_FALSE(only_padding);
        last_observed_timestamp_[ssrc] = timestamp;
      } else {
        // Verify timestamps are reasonably close.
        uint32_t latest_observed = timestamp_it->second;
        // Wraparound handling is unnecessary here as long as an int variable
        // is used to store the result.
        int32_t timestamp_gap = timestamp - latest_observed;
        EXPECT_LE(std::abs(timestamp_gap), kMaxTimestampGap)
            << "Gap in timestamps (" << latest_observed << " -> " << timestamp
            << ") too large for SSRC: " << ssrc << ".";
        timestamp_it->second = timestamp;
      }
    }

    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      RtpPacket rtp_packet;
      EXPECT_TRUE(rtp_packet.Parse(packet, length));
      const uint32_t ssrc = rtp_packet.Ssrc();
      const int64_t sequence_number =
          seq_numbers_unwrapper_.Unwrap(rtp_packet.SequenceNumber());
      const uint32_t timestamp = rtp_packet.Timestamp();
      const bool only_padding = rtp_packet.payload_size() == 0;

      EXPECT_TRUE(ssrc_is_rtx_.find(ssrc) != ssrc_is_rtx_.end())
          << "Received SSRC that wasn't configured: " << ssrc;

      static const int64_t kMaxSequenceNumberGap = 100;
      std::list<int64_t>* seq_numbers = &last_observed_seq_numbers_[ssrc];
      if (seq_numbers->empty()) {
        seq_numbers->push_back(sequence_number);
      } else {
        // We shouldn't get replays of previous sequence numbers.
        for (int64_t observed : *seq_numbers) {
          EXPECT_NE(observed, sequence_number)
              << "Received sequence number " << sequence_number << " for SSRC "
              << ssrc << " 2nd time.";
        }
        // Verify sequence numbers are reasonably close.
        int64_t latest_observed = seq_numbers->back();
        int64_t sequence_number_gap = sequence_number - latest_observed;
        EXPECT_LE(std::abs(sequence_number_gap), kMaxSequenceNumberGap)
            << "Gap in sequence numbers (" << latest_observed << " -> "
            << sequence_number << ") too large for SSRC: " << ssrc << ".";
        seq_numbers->push_back(sequence_number);
        if (seq_numbers->size() >= kMaxSequenceNumberGap) {
          seq_numbers->pop_front();
        }
      }

      if (!ssrc_is_rtx_[ssrc]) {
        MutexLock lock(&mutex_);
        ValidateTimestampGap(ssrc, timestamp, only_padding);

        // Wait for media packets on all ssrcs.
        if (!ssrc_observed_[ssrc] && !only_padding) {
          ssrc_observed_[ssrc] = true;
          if (--ssrcs_to_observe_ == 0)
            observation_complete_.Set();
        }
      }

      return SEND_PACKET;
    }

    Action OnSendRtcp(const uint8_t* packet, size_t length) override {
      test::RtcpPacketParser rtcp_parser;
      rtcp_parser.Parse(packet, length);
      if (rtcp_parser.sender_report()->num_packets() > 0) {
        uint32_t ssrc = rtcp_parser.sender_report()->sender_ssrc();
        uint32_t rtcp_timestamp = rtcp_parser.sender_report()->rtp_timestamp();

        MutexLock lock(&mutex_);
        ValidateTimestampGap(ssrc, rtcp_timestamp, false);
      }
      return SEND_PACKET;
    }

    SequenceNumberUnwrapper seq_numbers_unwrapper_;
    std::map<uint32_t, std::list<int64_t>> last_observed_seq_numbers_;
    std::map<uint32_t, uint32_t> last_observed_timestamp_;
    std::map<uint32_t, bool> ssrc_is_rtx_;

    Mutex mutex_;
    size_t ssrcs_to_observe_ RTC_GUARDED_BY(mutex_);
    std::map<uint32_t, bool> ssrc_observed_ RTC_GUARDED_BY(mutex_);
  } observer(use_rtx);

  std::unique_ptr<test::PacketTransport> send_transport;
  std::unique_ptr<test::PacketTransport> receive_transport;

  VideoEncoderConfig one_stream;

  SendTask(
      RTC_FROM_HERE, task_queue(),
      [this, &observer, &send_transport, &receive_transport, &one_stream,
       use_rtx]() {
        CreateCalls();

        send_transport = std::make_unique<test::PacketTransport>(
            task_queue(), sender_call_.get(), &observer,
            test::PacketTransport::kSender, payload_type_map_,
            std::make_unique<FakeNetworkPipe>(
                Clock::GetRealTimeClock(),
                std::make_unique<SimulatedNetwork>(
                    BuiltInNetworkBehaviorConfig())));
        receive_transport = std::make_unique<test::PacketTransport>(
            task_queue(), nullptr, &observer, test::PacketTransport::kReceiver,
            payload_type_map_,
            std::make_unique<FakeNetworkPipe>(
                Clock::GetRealTimeClock(),
                std::make_unique<SimulatedNetwork>(
                    BuiltInNetworkBehaviorConfig())));
        send_transport->SetReceiver(receiver_call_->Receiver());
        receive_transport->SetReceiver(sender_call_->Receiver());

        CreateSendConfig(kNumSimulcastStreams, 0, 0, send_transport.get());

        if (use_rtx) {
          for (size_t i = 0; i < kNumSimulcastStreams; ++i) {
            GetVideoSendConfig()->rtp.rtx.ssrcs.push_back(kSendRtxSsrcs[i]);
          }
          GetVideoSendConfig()->rtp.rtx.payload_type = kSendRtxPayloadType;
        }

        GetVideoEncoderConfig()->video_stream_factory =
            rtc::make_ref_counted<VideoStreamFactory>();
        // Use the same total bitrates when sending a single stream to avoid
        // lowering the bitrate estimate and requiring a subsequent rampup.
        one_stream = GetVideoEncoderConfig()->Copy();
        // one_stream.streams.resize(1);
        one_stream.number_of_streams = 1;
        CreateMatchingReceiveConfigs(receive_transport.get());

        CreateVideoStreams();
        CreateFrameGeneratorCapturer(30, 1280, 720);

        Start();
      });

  EXPECT_TRUE(observer.Wait())
      << "Timed out waiting for all SSRCs to send packets.";

  // Test stream resetting more than once to make sure that the state doesn't
  // get set once (this could be due to using std::map::insert for instance).
  for (size_t i = 0; i < 3; ++i) {
    SendTask(RTC_FROM_HERE, task_queue(), [&]() {
      DestroyVideoSendStreams();

      // Re-create VideoSendStream with only one stream.
      CreateVideoSendStream(one_stream);
      GetVideoSendStream()->Start();
      if (provoke_rtcpsr_before_rtp) {
        // Rapid Resync Request forces sending RTCP Sender Report back.
        // Using this request speeds up this test because then there is no need
        // to wait for a second for periodic Sender Report.
        rtcp::RapidResyncRequest force_send_sr_back_request;
        rtc::Buffer packet = force_send_sr_back_request.Build();
        static_cast<webrtc::test::DirectTransport*>(receive_transport.get())
            ->SendRtcp(packet.data(), packet.size());
      }
      CreateFrameGeneratorCapturer(30, 1280, 720);
    });

    observer.ResetExpectedSsrcs(1);
    EXPECT_TRUE(observer.Wait()) << "Timed out waiting for single RTP packet.";

    // Reconfigure back to use all streams.
    SendTask(RTC_FROM_HERE, task_queue(), [this]() {
      GetVideoSendStream()->ReconfigureVideoEncoder(
          GetVideoEncoderConfig()->Copy());
    });
    observer.ResetExpectedSsrcs(kNumSimulcastStreams);
    EXPECT_TRUE(observer.Wait())
        << "Timed out waiting for all SSRCs to send packets.";

    // Reconfigure down to one stream.
    SendTask(RTC_FROM_HERE, task_queue(), [this, &one_stream]() {
      GetVideoSendStream()->ReconfigureVideoEncoder(one_stream.Copy());
    });
    observer.ResetExpectedSsrcs(1);
    EXPECT_TRUE(observer.Wait()) << "Timed out waiting for single RTP packet.";

    // Reconfigure back to use all streams.
    SendTask(RTC_FROM_HERE, task_queue(), [this]() {
      GetVideoSendStream()->ReconfigureVideoEncoder(
          GetVideoEncoderConfig()->Copy());
    });
    observer.ResetExpectedSsrcs(kNumSimulcastStreams);
    EXPECT_TRUE(observer.Wait())
        << "Timed out waiting for all SSRCs to send packets.";
  }

  SendTask(RTC_FROM_HERE, task_queue(),
           [this, &send_transport, &receive_transport]() {
             Stop();
             DestroyStreams();
             send_transport.reset();
             receive_transport.reset();
             DestroyCalls();
           });
}

TEST_F(RtpRtcpEndToEndTest, RestartingSendStreamPreservesRtpState) {
  TestRtpStatePreservation(false, false);
}

TEST_F(RtpRtcpEndToEndTest, RestartingSendStreamPreservesRtpStatesWithRtx) {
  TestRtpStatePreservation(true, false);
}

TEST_F(RtpRtcpEndToEndTest,
       RestartingSendStreamKeepsRtpAndRtcpTimestampsSynced) {
  TestRtpStatePreservation(true, true);
}

// See https://bugs.chromium.org/p/webrtc/issues/detail?id=9648.
TEST_F(RtpRtcpEndToEndTest, DISABLED_TestFlexfecRtpStatePreservation) {
  class RtpSequenceObserver : public test::RtpRtcpObserver {
   public:
    RtpSequenceObserver()
        : test::RtpRtcpObserver(kDefaultTimeoutMs),
          num_flexfec_packets_sent_(0) {}

    void ResetPacketCount() {
      MutexLock lock(&mutex_);
      num_flexfec_packets_sent_ = 0;
    }

   private:
    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      MutexLock lock(&mutex_);

      RtpPacket rtp_packet;
      EXPECT_TRUE(rtp_packet.Parse(packet, length));
      const uint16_t sequence_number = rtp_packet.SequenceNumber();
      const uint32_t timestamp = rtp_packet.Timestamp();
      const uint32_t ssrc = rtp_packet.Ssrc();

      if (ssrc == kVideoSendSsrcs[0] || ssrc == kSendRtxSsrcs[0]) {
        return SEND_PACKET;
      }
      EXPECT_EQ(kFlexfecSendSsrc, ssrc) << "Unknown SSRC sent.";

      ++num_flexfec_packets_sent_;

      // If this is the first packet, we have nothing to compare to.
      if (!last_observed_sequence_number_) {
        last_observed_sequence_number_.emplace(sequence_number);
        last_observed_timestamp_.emplace(timestamp);

        return SEND_PACKET;
      }

      // Verify continuity and monotonicity of RTP sequence numbers.
      EXPECT_EQ(static_cast<uint16_t>(*last_observed_sequence_number_ + 1),
                sequence_number);
      last_observed_sequence_number_.emplace(sequence_number);

      // Timestamps should be non-decreasing...
      const bool timestamp_is_same_or_newer =
          timestamp == *last_observed_timestamp_ ||
          IsNewerTimestamp(timestamp, *last_observed_timestamp_);
      EXPECT_TRUE(timestamp_is_same_or_newer);
      // ...but reasonably close in time.
      const int k10SecondsInRtpTimestampBase = 10 * kVideoPayloadTypeFrequency;
      EXPECT_TRUE(IsNewerTimestamp(
          *last_observed_timestamp_ + k10SecondsInRtpTimestampBase, timestamp));
      last_observed_timestamp_.emplace(timestamp);

      // Pass test when enough packets have been let through.
      if (num_flexfec_packets_sent_ >= 10) {
        observation_complete_.Set();
      }

      return SEND_PACKET;
    }

    absl::optional<uint16_t> last_observed_sequence_number_
        RTC_GUARDED_BY(mutex_);
    absl::optional<uint32_t> last_observed_timestamp_ RTC_GUARDED_BY(mutex_);
    size_t num_flexfec_packets_sent_ RTC_GUARDED_BY(mutex_);
    Mutex mutex_;
  } observer;

  static constexpr int kFrameMaxWidth = 320;
  static constexpr int kFrameMaxHeight = 180;
  static constexpr int kFrameRate = 15;

  std::unique_ptr<test::PacketTransport> send_transport;
  std::unique_ptr<test::PacketTransport> receive_transport;
  test::FunctionVideoEncoderFactory encoder_factory(
      []() { return VP8Encoder::Create(); });

  SendTask(RTC_FROM_HERE, task_queue(), [&]() {
    CreateCalls();

    BuiltInNetworkBehaviorConfig lossy_delayed_link;
    lossy_delayed_link.loss_percent = 2;
    lossy_delayed_link.queue_delay_ms = 50;

    send_transport = std::make_unique<test::PacketTransport>(
        task_queue(), sender_call_.get(), &observer,
        test::PacketTransport::kSender, payload_type_map_,
        std::make_unique<FakeNetworkPipe>(
            Clock::GetRealTimeClock(),
            std::make_unique<SimulatedNetwork>(lossy_delayed_link)));
    send_transport->SetReceiver(receiver_call_->Receiver());

    BuiltInNetworkBehaviorConfig flawless_link;
    receive_transport = std::make_unique<test::PacketTransport>(
        task_queue(), nullptr, &observer, test::PacketTransport::kReceiver,
        payload_type_map_,
        std::make_unique<FakeNetworkPipe>(
            Clock::GetRealTimeClock(),
            std::make_unique<SimulatedNetwork>(flawless_link)));
    receive_transport->SetReceiver(sender_call_->Receiver());

    // For reduced flakyness, we use a real VP8 encoder together with NACK
    // and RTX.
    const int kNumVideoStreams = 1;
    const int kNumFlexfecStreams = 1;
    CreateSendConfig(kNumVideoStreams, 0, kNumFlexfecStreams,
                     send_transport.get());

    GetVideoSendConfig()->encoder_settings.encoder_factory = &encoder_factory;
    GetVideoSendConfig()->rtp.payload_name = "VP8";
    GetVideoSendConfig()->rtp.payload_type = kVideoSendPayloadType;
    GetVideoSendConfig()->rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
    GetVideoSendConfig()->rtp.rtx.ssrcs.push_back(kSendRtxSsrcs[0]);
    GetVideoSendConfig()->rtp.rtx.payload_type = kSendRtxPayloadType;
    GetVideoEncoderConfig()->codec_type = kVideoCodecVP8;

    CreateMatchingReceiveConfigs(receive_transport.get());
    video_receive_configs_[0].rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
    video_receive_configs_[0].rtp.rtx_ssrc = kSendRtxSsrcs[0];
    video_receive_configs_[0]
        .rtp.rtx_associated_payload_types[kSendRtxPayloadType] =
        kVideoSendPayloadType;

    // The matching FlexFEC receive config is not created by
    // CreateMatchingReceiveConfigs since this is not a test::BaseTest.
    // Set up the receive config manually instead.
    FlexfecReceiveStream::Config flexfec_receive_config(
        receive_transport.get());
    flexfec_receive_config.payload_type =
        GetVideoSendConfig()->rtp.flexfec.payload_type;
    flexfec_receive_config.rtp.remote_ssrc =
        GetVideoSendConfig()->rtp.flexfec.ssrc;
    flexfec_receive_config.protected_media_ssrcs =
        GetVideoSendConfig()->rtp.flexfec.protected_media_ssrcs;
    flexfec_receive_config.rtp.local_ssrc = kReceiverLocalVideoSsrc;
    flexfec_receive_config.rtp.transport_cc = true;
    flexfec_receive_config.rtp.extensions.emplace_back(
        RtpExtension::kTransportSequenceNumberUri,
        kTransportSequenceNumberExtensionId);
    flexfec_receive_configs_.push_back(flexfec_receive_config);

    CreateFlexfecStreams();
    CreateVideoStreams();

    // RTCP might be disabled if the network is "down".
    sender_call_->SignalChannelNetworkState(MediaType::VIDEO, kNetworkUp);
    receiver_call_->SignalChannelNetworkState(MediaType::VIDEO, kNetworkUp);

    CreateFrameGeneratorCapturer(kFrameRate, kFrameMaxWidth, kFrameMaxHeight);

    Start();
  });

  // Initial test.
  EXPECT_TRUE(observer.Wait()) << "Timed out waiting for packets.";

  SendTask(RTC_FROM_HERE, task_queue(), [this, &observer]() {
    // Ensure monotonicity when the VideoSendStream is restarted.
    Stop();
    observer.ResetPacketCount();
    Start();
  });

  EXPECT_TRUE(observer.Wait()) << "Timed out waiting for packets.";

  SendTask(RTC_FROM_HERE, task_queue(), [this, &observer]() {
    // Ensure monotonicity when the VideoSendStream is recreated.
    DestroyVideoSendStreams();
    observer.ResetPacketCount();
    CreateVideoSendStreams();
    GetVideoSendStream()->Start();
    CreateFrameGeneratorCapturer(kFrameRate, kFrameMaxWidth, kFrameMaxHeight);
  });

  EXPECT_TRUE(observer.Wait()) << "Timed out waiting for packets.";

  // Cleanup.
  SendTask(RTC_FROM_HERE, task_queue(),
           [this, &send_transport, &receive_transport]() {
             Stop();
             DestroyStreams();
             send_transport.reset();
             receive_transport.reset();
             DestroyCalls();
           });
}
}  // namespace webrtc
