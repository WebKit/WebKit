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
#include "modules/rtp_rtcp/source/rtp_packet.h"
#include "rtc_base/task_queue_for_test.h"
#include "test/call_test.h"
#include "test/gtest.h"
#include "test/rtcp_packet_parser.h"

namespace webrtc {
class SsrcEndToEndTest : public test::CallTest {
 protected:
  void TestSendsSetSsrcs(size_t num_ssrcs, bool send_single_ssrc_first);
};

TEST_F(SsrcEndToEndTest, ReceiverUsesLocalSsrc) {
  class SyncRtcpObserver : public test::EndToEndTest {
   public:
    SyncRtcpObserver() : EndToEndTest(kDefaultTimeoutMs) {}

    Action OnReceiveRtcp(const uint8_t* packet, size_t length) override {
      test::RtcpPacketParser parser;
      EXPECT_TRUE(parser.Parse(packet, length));
      EXPECT_EQ(kReceiverLocalVideoSsrc, parser.sender_ssrc());
      observation_complete_.Set();

      return SEND_PACKET;
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait())
          << "Timed out while waiting for a receiver RTCP packet to be sent.";
    }
  } test;

  RunBaseTest(&test);
}

TEST_F(SsrcEndToEndTest, UnknownRtpPacketGivesUnknownSsrcReturnCode) {
  class PacketInputObserver : public PacketReceiver {
   public:
    explicit PacketInputObserver(PacketReceiver* receiver)
        : receiver_(receiver) {}

    bool Wait() { return delivered_packet_.Wait(kDefaultTimeoutMs); }

   private:
    DeliveryStatus DeliverPacket(MediaType media_type,
                                 rtc::CopyOnWriteBuffer packet,
                                 int64_t packet_time_us) override {
      if (RtpHeaderParser::IsRtcp(packet.cdata(), packet.size())) {
        return receiver_->DeliverPacket(media_type, std::move(packet),
                                        packet_time_us);
      }
      DeliveryStatus delivery_status = receiver_->DeliverPacket(
          media_type, std::move(packet), packet_time_us);
      EXPECT_EQ(DELIVERY_UNKNOWN_SSRC, delivery_status);
      delivered_packet_.Set();
      return delivery_status;
    }

    PacketReceiver* receiver_;
    rtc::Event delivered_packet_;
  };

  std::unique_ptr<test::DirectTransport> send_transport;
  std::unique_ptr<test::DirectTransport> receive_transport;
  std::unique_ptr<PacketInputObserver> input_observer;

  SendTask(
      RTC_FROM_HERE, task_queue(),
      [this, &send_transport, &receive_transport, &input_observer]() {
        CreateCalls();

        send_transport = std::make_unique<test::DirectTransport>(
            task_queue(),
            std::make_unique<FakeNetworkPipe>(
                Clock::GetRealTimeClock(), std::make_unique<SimulatedNetwork>(
                                               BuiltInNetworkBehaviorConfig())),
            sender_call_.get(), payload_type_map_);
        receive_transport = std::make_unique<test::DirectTransport>(
            task_queue(),
            std::make_unique<FakeNetworkPipe>(
                Clock::GetRealTimeClock(), std::make_unique<SimulatedNetwork>(
                                               BuiltInNetworkBehaviorConfig())),
            receiver_call_.get(), payload_type_map_);
        input_observer =
            std::make_unique<PacketInputObserver>(receiver_call_->Receiver());
        send_transport->SetReceiver(input_observer.get());
        receive_transport->SetReceiver(sender_call_->Receiver());

        CreateSendConfig(1, 0, 0, send_transport.get());
        CreateMatchingReceiveConfigs(receive_transport.get());

        CreateVideoStreams();
        CreateFrameGeneratorCapturer(kDefaultFramerate, kDefaultWidth,
                                     kDefaultHeight);
        Start();

        receiver_call_->DestroyVideoReceiveStream(video_receive_streams_[0]);
        video_receive_streams_.clear();
      });

  // Wait() waits for a received packet.
  EXPECT_TRUE(input_observer->Wait());

  SendTask(RTC_FROM_HERE, task_queue(),
           [this, &send_transport, &receive_transport]() {
             Stop();
             DestroyStreams();
             send_transport.reset();
             receive_transport.reset();
             DestroyCalls();
           });
}

void SsrcEndToEndTest::TestSendsSetSsrcs(size_t num_ssrcs,
                                         bool send_single_ssrc_first) {
  class SendsSetSsrcs : public test::EndToEndTest {
   public:
    SendsSetSsrcs(const uint32_t* ssrcs,
                  size_t num_ssrcs,
                  bool send_single_ssrc_first)
        : EndToEndTest(kDefaultTimeoutMs),
          num_ssrcs_(num_ssrcs),
          send_single_ssrc_first_(send_single_ssrc_first),
          ssrcs_to_observe_(num_ssrcs),
          expect_single_ssrc_(send_single_ssrc_first),
          send_stream_(nullptr) {
      for (size_t i = 0; i < num_ssrcs; ++i)
        valid_ssrcs_[ssrcs[i]] = true;
    }

   private:
    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      RtpPacket rtp_packet;
      EXPECT_TRUE(rtp_packet.Parse(packet, length));

      EXPECT_TRUE(valid_ssrcs_[rtp_packet.Ssrc()])
          << "Received unknown SSRC: " << rtp_packet.Ssrc();

      if (!valid_ssrcs_[rtp_packet.Ssrc()])
        observation_complete_.Set();

      if (!is_observed_[rtp_packet.Ssrc()]) {
        is_observed_[rtp_packet.Ssrc()] = true;
        --ssrcs_to_observe_;
        if (expect_single_ssrc_) {
          expect_single_ssrc_ = false;
          observation_complete_.Set();
        }
      }

      if (ssrcs_to_observe_ == 0)
        observation_complete_.Set();

      return SEND_PACKET;
    }

    size_t GetNumVideoStreams() const override { return num_ssrcs_; }

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
      video_encoder_config_all_streams_ = encoder_config->Copy();
      if (send_single_ssrc_first_)
        encoder_config->number_of_streams = 1;
    }

    void OnVideoStreamsCreated(
        VideoSendStream* send_stream,
        const std::vector<VideoReceiveStream*>& receive_streams) override {
      send_stream_ = send_stream;
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait()) << "Timed out while waiting for "
                          << (send_single_ssrc_first_ ? "first SSRC."
                                                      : "SSRCs.");

      if (send_single_ssrc_first_) {
        // Set full simulcast and continue with the rest of the SSRCs.
        send_stream_->ReconfigureVideoEncoder(
            std::move(video_encoder_config_all_streams_));
        EXPECT_TRUE(Wait()) << "Timed out while waiting on additional SSRCs.";
      }
    }

   private:
    std::map<uint32_t, bool> valid_ssrcs_;
    std::map<uint32_t, bool> is_observed_;

    const size_t num_ssrcs_;
    const bool send_single_ssrc_first_;

    size_t ssrcs_to_observe_;
    bool expect_single_ssrc_;

    VideoSendStream* send_stream_;
    VideoEncoderConfig video_encoder_config_all_streams_;
  } test(kVideoSendSsrcs, num_ssrcs, send_single_ssrc_first);

  RunBaseTest(&test);
}

TEST_F(SsrcEndToEndTest, SendsSetSsrc) {
  TestSendsSetSsrcs(1, false);
}

TEST_F(SsrcEndToEndTest, SendsSetSimulcastSsrcs) {
  TestSendsSetSsrcs(kNumSimulcastStreams, false);
}

TEST_F(SsrcEndToEndTest, CanSwitchToUseAllSsrcs) {
  TestSendsSetSsrcs(kNumSimulcastStreams, true);
}

TEST_F(SsrcEndToEndTest, DISABLED_RedundantPayloadsTransmittedOnAllSsrcs) {
  class ObserveRedundantPayloads : public test::EndToEndTest {
   public:
    ObserveRedundantPayloads()
        : EndToEndTest(kDefaultTimeoutMs),
          ssrcs_to_observe_(kNumSimulcastStreams) {
      for (size_t i = 0; i < kNumSimulcastStreams; ++i) {
        registered_rtx_ssrc_[kSendRtxSsrcs[i]] = true;
      }
    }

   private:
    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      RtpPacket rtp_packet;
      EXPECT_TRUE(rtp_packet.Parse(packet, length));

      if (!registered_rtx_ssrc_[rtp_packet.Ssrc()])
        return SEND_PACKET;

      const bool packet_is_redundant_payload = rtp_packet.payload_size() > 0;

      if (!packet_is_redundant_payload)
        return SEND_PACKET;

      if (!observed_redundant_retransmission_[rtp_packet.Ssrc()]) {
        observed_redundant_retransmission_[rtp_packet.Ssrc()] = true;
        if (--ssrcs_to_observe_ == 0)
          observation_complete_.Set();
      }

      return SEND_PACKET;
    }

    size_t GetNumVideoStreams() const override { return kNumSimulcastStreams; }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      // Set low simulcast bitrates to not have to wait for bandwidth ramp-up.
      encoder_config->max_bitrate_bps = 50000;
      for (auto& layer : encoder_config->simulcast_layers) {
        layer.min_bitrate_bps = 10000;
        layer.target_bitrate_bps = 15000;
        layer.max_bitrate_bps = 20000;
      }
      send_config->rtp.rtx.payload_type = kSendRtxPayloadType;

      for (size_t i = 0; i < kNumSimulcastStreams; ++i)
        send_config->rtp.rtx.ssrcs.push_back(kSendRtxSsrcs[i]);

      // Significantly higher than max bitrates for all video streams -> forcing
      // padding to trigger redundant padding on all RTX SSRCs.
      encoder_config->min_transmit_bitrate_bps = 100000;
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait())
          << "Timed out while waiting for redundant payloads on all SSRCs.";
    }

   private:
    size_t ssrcs_to_observe_;
    std::map<uint32_t, bool> observed_redundant_retransmission_;
    std::map<uint32_t, bool> registered_rtx_ssrc_;
  } test;

  RunBaseTest(&test);
}
}  // namespace webrtc
