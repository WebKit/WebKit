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
#include "call/packet_receiver.h"
#include "call/simulated_network.h"
#include "modules/rtp_rtcp/source/rtp_packet.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_util.h"
#include "rtc_base/task_queue_for_test.h"
#include "test/call_test.h"
#include "test/gtest.h"
#include "test/rtcp_packet_parser.h"
#include "test/video_test_constants.h"

namespace webrtc {
class SsrcEndToEndTest : public test::CallTest {
 public:
  SsrcEndToEndTest() {
    RegisterRtpExtension(
        RtpExtension(RtpExtension::kTransportSequenceNumberUri, 1));
  }

 protected:
  void TestSendsSetSsrcs(size_t num_ssrcs, bool send_single_ssrc_first);
};

TEST_F(SsrcEndToEndTest, ReceiverUsesLocalSsrc) {
  class SyncRtcpObserver : public test::EndToEndTest {
   public:
    SyncRtcpObserver()
        : EndToEndTest(test::VideoTestConstants::kDefaultTimeout) {}

    Action OnReceiveRtcp(rtc::ArrayView<const uint8_t> packet) override {
      test::RtcpPacketParser parser;
      EXPECT_TRUE(parser.Parse(packet));
      EXPECT_EQ(test::VideoTestConstants::kReceiverLocalVideoSsrc,
                parser.sender_ssrc());
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

TEST_F(SsrcEndToEndTest, UnknownRtpPacketTriggersUndemuxablePacketHandler) {
  class PacketInputObserver : public PacketReceiver {
   public:
    explicit PacketInputObserver(PacketReceiver* receiver)
        : receiver_(receiver) {}

    bool Wait() {
      return undemuxable_packet_handler_triggered_.Wait(
          test::VideoTestConstants::kDefaultTimeout);
    }

   private:
    void DeliverRtpPacket(
        MediaType media_type,
        RtpPacketReceived packet,
        OnUndemuxablePacketHandler undemuxable_packet_handler) override {
      PacketReceiver::OnUndemuxablePacketHandler handler =
          [this](const RtpPacketReceived& packet) {
            undemuxable_packet_handler_triggered_.Set();
            // No need to re-attempt deliver the packet.
            return false;
          };
      receiver_->DeliverRtpPacket(media_type, std::move(packet),
                                  std::move(handler));
    }
    void DeliverRtcpPacket(rtc::CopyOnWriteBuffer packet) override {}

    PacketReceiver* receiver_;
    rtc::Event undemuxable_packet_handler_triggered_;
  };

  std::unique_ptr<test::DirectTransport> send_transport;
  std::unique_ptr<test::DirectTransport> receive_transport;
  std::unique_ptr<PacketInputObserver> input_observer;

  SendTask(task_queue(), [this, &send_transport, &receive_transport,
                          &input_observer]() {
    CreateCalls();

    send_transport = std::make_unique<test::DirectTransport>(
        task_queue(),
        std::make_unique<FakeNetworkPipe>(
            Clock::GetRealTimeClock(),
            std::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig())),
        sender_call_.get(), payload_type_map_, GetRegisteredExtensions(),
        GetRegisteredExtensions());
    receive_transport = std::make_unique<test::DirectTransport>(
        task_queue(),
        std::make_unique<FakeNetworkPipe>(
            Clock::GetRealTimeClock(),
            std::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig())),
        receiver_call_.get(), payload_type_map_, GetRegisteredExtensions(),
        GetRegisteredExtensions());
    input_observer =
        std::make_unique<PacketInputObserver>(receiver_call_->Receiver());
    send_transport->SetReceiver(input_observer.get());
    receive_transport->SetReceiver(sender_call_->Receiver());

    CreateSendConfig(1, 0, 0, send_transport.get());
    CreateMatchingReceiveConfigs(receive_transport.get());

    CreateVideoStreams();
    CreateFrameGeneratorCapturer(test::VideoTestConstants::kDefaultFramerate,
                                 test::VideoTestConstants::kDefaultWidth,
                                 test::VideoTestConstants::kDefaultHeight);
    Start();

    receiver_call_->DestroyVideoReceiveStream(video_receive_streams_[0]);
    video_receive_streams_.clear();
  });

  // Wait() waits for a received packet.
  EXPECT_TRUE(input_observer->Wait());

  SendTask(task_queue(), [this, &send_transport, &receive_transport]() {
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
                  bool send_single_ssrc_first,
                  TaskQueueBase* task_queue)
        : EndToEndTest(test::VideoTestConstants::kDefaultTimeout),
          num_ssrcs_(num_ssrcs),
          send_single_ssrc_first_(send_single_ssrc_first),
          ssrcs_to_observe_(num_ssrcs),
          expect_single_ssrc_(send_single_ssrc_first),
          send_stream_(nullptr),
          task_queue_(task_queue) {
      for (size_t i = 0; i < num_ssrcs; ++i)
        valid_ssrcs_[ssrcs[i]] = true;
    }

   private:
    Action OnSendRtp(rtc::ArrayView<const uint8_t> packet) override {
      RtpPacket rtp_packet;
      EXPECT_TRUE(rtp_packet.Parse(packet));

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

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStreamInterface::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      // Set low simulcast bitrates to not have to wait for bandwidth ramp-up.
      encoder_config->max_bitrate_bps = 50000;
      for (auto& layer : encoder_config->simulcast_layers) {
        layer.min_bitrate_bps = 10000;
        layer.target_bitrate_bps = 15000;
        layer.max_bitrate_bps = 20000;
      }
      video_encoder_config_all_streams_ = encoder_config->Copy();
      if (send_single_ssrc_first_)
        encoder_config->number_of_streams = 1;
    }

    void OnVideoStreamsCreated(VideoSendStream* send_stream,
                               const std::vector<VideoReceiveStreamInterface*>&
                                   receive_streams) override {
      send_stream_ = send_stream;
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait()) << "Timed out while waiting for "
                          << (send_single_ssrc_first_ ? "first SSRC."
                                                      : "SSRCs.");

      if (send_single_ssrc_first_) {
        // Set full simulcast and continue with the rest of the SSRCs.
        SendTask(task_queue_, [&]() {
          send_stream_->ReconfigureVideoEncoder(
              std::move(video_encoder_config_all_streams_));
        });
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
    TaskQueueBase* task_queue_;
  } test(test::VideoTestConstants::kVideoSendSsrcs, num_ssrcs,
         send_single_ssrc_first, task_queue());

  RunBaseTest(&test);
}

TEST_F(SsrcEndToEndTest, SendsSetSsrc) {
  TestSendsSetSsrcs(1, false);
}

TEST_F(SsrcEndToEndTest, SendsSetSimulcastSsrcs) {
  TestSendsSetSsrcs(test::VideoTestConstants::kNumSimulcastStreams, false);
}

TEST_F(SsrcEndToEndTest, CanSwitchToUseAllSsrcs) {
  TestSendsSetSsrcs(test::VideoTestConstants::kNumSimulcastStreams, true);
}

TEST_F(SsrcEndToEndTest, DISABLED_RedundantPayloadsTransmittedOnAllSsrcs) {
  class ObserveRedundantPayloads : public test::EndToEndTest {
   public:
    ObserveRedundantPayloads()
        : EndToEndTest(test::VideoTestConstants::kDefaultTimeout),
          ssrcs_to_observe_(test::VideoTestConstants::kNumSimulcastStreams) {
      for (size_t i = 0; i < test::VideoTestConstants::kNumSimulcastStreams;
           ++i) {
        registered_rtx_ssrc_[test::VideoTestConstants::kSendRtxSsrcs[i]] = true;
      }
    }

   private:
    Action OnSendRtp(rtc::ArrayView<const uint8_t> packet) override {
      RtpPacket rtp_packet;
      EXPECT_TRUE(rtp_packet.Parse(packet));

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

    size_t GetNumVideoStreams() const override {
      return test::VideoTestConstants::kNumSimulcastStreams;
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStreamInterface::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      // Set low simulcast bitrates to not have to wait for bandwidth ramp-up.
      encoder_config->max_bitrate_bps = 50000;
      for (auto& layer : encoder_config->simulcast_layers) {
        layer.min_bitrate_bps = 10000;
        layer.target_bitrate_bps = 15000;
        layer.max_bitrate_bps = 20000;
      }
      send_config->rtp.rtx.payload_type =
          test::VideoTestConstants::kSendRtxPayloadType;

      for (size_t i = 0; i < test::VideoTestConstants::kNumSimulcastStreams;
           ++i)
        send_config->rtp.rtx.ssrcs.push_back(
            test::VideoTestConstants::kSendRtxSsrcs[i]);

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
