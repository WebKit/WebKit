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
#include "call/fake_network_pipe.h"
#include "call/simulated_network.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "system_wrappers/include/sleep.h"
#include "test/call_test.h"
#include "test/field_trial.h"
#include "test/function_video_encoder_factory.h"
#include "test/gtest.h"
#include "test/rtcp_packet_parser.h"

namespace webrtc {
class RetransmissionEndToEndTest : public test::CallTest {
 public:
  RetransmissionEndToEndTest() = default;

 protected:
  void DecodesRetransmittedFrame(bool enable_rtx, bool enable_red);
  void ReceivesPliAndRecovers(int rtp_history_ms);
};

TEST_F(RetransmissionEndToEndTest, ReceivesAndRetransmitsNack) {
  static const int kNumberOfNacksToObserve = 2;
  static const int kLossBurstSize = 2;
  static const int kPacketsBetweenLossBursts = 9;
  class NackObserver : public test::EndToEndTest {
   public:
    NackObserver()
        : EndToEndTest(kLongTimeoutMs),
          sent_rtp_packets_(0),
          packets_left_to_drop_(0),
          nacks_left_(kNumberOfNacksToObserve) {}

   private:
    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      rtc::CritScope lock(&crit_);
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));

      // Never drop retransmitted packets.
      if (dropped_packets_.find(header.sequenceNumber) !=
          dropped_packets_.end()) {
        retransmitted_packets_.insert(header.sequenceNumber);
        return SEND_PACKET;
      }

      if (nacks_left_ <= 0 &&
          retransmitted_packets_.size() == dropped_packets_.size()) {
        observation_complete_.Set();
      }

      ++sent_rtp_packets_;

      // Enough NACKs received, stop dropping packets.
      if (nacks_left_ <= 0)
        return SEND_PACKET;

      // Check if it's time for a new loss burst.
      if (sent_rtp_packets_ % kPacketsBetweenLossBursts == 0)
        packets_left_to_drop_ = kLossBurstSize;

      // Never drop padding packets as those won't be retransmitted.
      if (packets_left_to_drop_ > 0 && header.paddingLength == 0) {
        --packets_left_to_drop_;
        dropped_packets_.insert(header.sequenceNumber);
        return DROP_PACKET;
      }

      return SEND_PACKET;
    }

    Action OnReceiveRtcp(const uint8_t* packet, size_t length) override {
      rtc::CritScope lock(&crit_);
      test::RtcpPacketParser parser;
      EXPECT_TRUE(parser.Parse(packet, length));
      nacks_left_ -= parser.nack()->num_packets();
      return SEND_PACKET;
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      send_config->rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
      (*receive_configs)[0].rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait())
          << "Timed out waiting for packets to be NACKed, retransmitted and "
             "rendered.";
    }

    rtc::CriticalSection crit_;
    std::set<uint16_t> dropped_packets_;
    std::set<uint16_t> retransmitted_packets_;
    uint64_t sent_rtp_packets_;
    int packets_left_to_drop_;
    int nacks_left_ RTC_GUARDED_BY(&crit_);
  } test;

  RunBaseTest(&test);
}

TEST_F(RetransmissionEndToEndTest, ReceivesNackAndRetransmitsAudio) {
  class NackObserver : public test::EndToEndTest {
   public:
    NackObserver()
        : EndToEndTest(kLongTimeoutMs),
          local_ssrc_(0),
          remote_ssrc_(0),
          receive_transport_(nullptr) {}

   private:
    size_t GetNumVideoStreams() const override { return 0; }
    size_t GetNumAudioStreams() const override { return 1; }

    test::PacketTransport* CreateReceiveTransport(
        test::SingleThreadedTaskQueueForTesting* task_queue) override {
      test::PacketTransport* receive_transport = new test::PacketTransport(
          task_queue, nullptr, this, test::PacketTransport::kReceiver,
          payload_type_map_,
          absl::make_unique<FakeNetworkPipe>(
              Clock::GetRealTimeClock(),
              absl::make_unique<SimulatedNetwork>(
                  DefaultNetworkSimulationConfig())));
      receive_transport_ = receive_transport;
      return receive_transport;
    }

    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));

      if (!sequence_number_to_retransmit_) {
        sequence_number_to_retransmit_ = header.sequenceNumber;

        // Don't ask for retransmission straight away, may be deduped in pacer.
      } else if (header.sequenceNumber == *sequence_number_to_retransmit_) {
        observation_complete_.Set();
      } else {
        // Send a NACK as often as necessary until retransmission is received.
        rtcp::Nack nack;
        nack.SetSenderSsrc(local_ssrc_);
        nack.SetMediaSsrc(remote_ssrc_);
        uint16_t nack_list[] = {*sequence_number_to_retransmit_};
        nack.SetPacketIds(nack_list, 1);
        rtc::Buffer buffer = nack.Build();

        EXPECT_TRUE(receive_transport_->SendRtcp(buffer.data(), buffer.size()));
      }

      return SEND_PACKET;
    }

    void ModifyAudioConfigs(
        AudioSendStream::Config* send_config,
        std::vector<AudioReceiveStream::Config>* receive_configs) override {
      send_config->rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
      (*receive_configs)[0].rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
      local_ssrc_ = (*receive_configs)[0].rtp.local_ssrc;
      remote_ssrc_ = (*receive_configs)[0].rtp.remote_ssrc;
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait())
          << "Timed out waiting for packets to be NACKed, retransmitted and "
             "rendered.";
    }

    uint32_t local_ssrc_;
    uint32_t remote_ssrc_;
    Transport* receive_transport_;
    absl::optional<uint16_t> sequence_number_to_retransmit_;
  } test;

  RunBaseTest(&test);
}

TEST_F(RetransmissionEndToEndTest,
       StopSendingKeyframeRequestsForInactiveStream) {
  class KeyframeRequestObserver : public test::EndToEndTest {
   public:
    explicit KeyframeRequestObserver(
        test::SingleThreadedTaskQueueForTesting* task_queue)
        : clock_(Clock::GetRealTimeClock()), task_queue_(task_queue) {}

    void OnVideoStreamsCreated(
        VideoSendStream* send_stream,
        const std::vector<VideoReceiveStream*>& receive_streams) override {
      RTC_DCHECK_EQ(1, receive_streams.size());
      send_stream_ = send_stream;
      receive_stream_ = receive_streams[0];
    }

    void PerformTest() override {
      bool frame_decoded = false;
      int64_t start_time = clock_->TimeInMilliseconds();
      while (clock_->TimeInMilliseconds() - start_time <= 5000) {
        if (receive_stream_->GetStats().frames_decoded > 0) {
          frame_decoded = true;
          break;
        }
        SleepMs(100);
      }
      ASSERT_TRUE(frame_decoded);
      task_queue_->SendTask([this]() { send_stream_->Stop(); });
      SleepMs(10000);
      ASSERT_EQ(
          1U, receive_stream_->GetStats().rtcp_packet_type_counts.pli_packets);
    }

   private:
    Clock* clock_;
    VideoSendStream* send_stream_;
    VideoReceiveStream* receive_stream_;
    test::SingleThreadedTaskQueueForTesting* const task_queue_;
  } test(&task_queue_);

  RunBaseTest(&test);
}

void RetransmissionEndToEndTest::ReceivesPliAndRecovers(int rtp_history_ms) {
  static const int kPacketsToDrop = 1;

  class PliObserver : public test::EndToEndTest,
                      public rtc::VideoSinkInterface<VideoFrame> {
   public:
    explicit PliObserver(int rtp_history_ms)
        : EndToEndTest(kLongTimeoutMs),
          rtp_history_ms_(rtp_history_ms),
          nack_enabled_(rtp_history_ms > 0),
          highest_dropped_timestamp_(0),
          frames_to_drop_(0),
          received_pli_(false) {}

   private:
    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      rtc::CritScope lock(&crit_);
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));

      // Drop all retransmitted packets to force a PLI.
      if (header.timestamp <= highest_dropped_timestamp_)
        return DROP_PACKET;

      if (frames_to_drop_ > 0) {
        highest_dropped_timestamp_ = header.timestamp;
        --frames_to_drop_;
        return DROP_PACKET;
      }

      return SEND_PACKET;
    }

    Action OnReceiveRtcp(const uint8_t* packet, size_t length) override {
      rtc::CritScope lock(&crit_);
      test::RtcpPacketParser parser;
      EXPECT_TRUE(parser.Parse(packet, length));
      if (!nack_enabled_)
        EXPECT_EQ(0, parser.nack()->num_packets());
      if (parser.pli()->num_packets() > 0)
        received_pli_ = true;
      return SEND_PACKET;
    }

    void OnFrame(const VideoFrame& video_frame) override {
      rtc::CritScope lock(&crit_);
      if (received_pli_ &&
          video_frame.timestamp() > highest_dropped_timestamp_) {
        observation_complete_.Set();
      }
      if (!received_pli_)
        frames_to_drop_ = kPacketsToDrop;
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      send_config->rtp.nack.rtp_history_ms = rtp_history_ms_;
      (*receive_configs)[0].rtp.nack.rtp_history_ms = rtp_history_ms_;
      (*receive_configs)[0].renderer = this;
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait()) << "Timed out waiting for PLI to be "
                             "received and a frame to be "
                             "rendered afterwards.";
    }

    rtc::CriticalSection crit_;
    int rtp_history_ms_;
    bool nack_enabled_;
    uint32_t highest_dropped_timestamp_ RTC_GUARDED_BY(&crit_);
    int frames_to_drop_ RTC_GUARDED_BY(&crit_);
    bool received_pli_ RTC_GUARDED_BY(&crit_);
  } test(rtp_history_ms);

  RunBaseTest(&test);
}

TEST_F(RetransmissionEndToEndTest, ReceivesPliAndRecoversWithNack) {
  ReceivesPliAndRecovers(1000);
}

TEST_F(RetransmissionEndToEndTest, ReceivesPliAndRecoversWithoutNack) {
  ReceivesPliAndRecovers(0);
}
// This test drops second RTP packet with a marker bit set, makes sure it's
// retransmitted and renders. Retransmission SSRCs are also checked.
void RetransmissionEndToEndTest::DecodesRetransmittedFrame(bool enable_rtx,
                                                           bool enable_red) {
  static const int kDroppedFrameNumber = 10;
  class RetransmissionObserver : public test::EndToEndTest,
                                 public rtc::VideoSinkInterface<VideoFrame> {
   public:
    RetransmissionObserver(bool enable_rtx, bool enable_red)
        : EndToEndTest(kDefaultTimeoutMs),
          payload_type_(GetPayloadType(false, enable_red)),
          retransmission_ssrc_(enable_rtx ? kSendRtxSsrcs[0]
                                          : kVideoSendSsrcs[0]),
          retransmission_payload_type_(GetPayloadType(enable_rtx, enable_red)),
          encoder_factory_([]() { return VP8Encoder::Create(); }),
          marker_bits_observed_(0),
          retransmitted_timestamp_(0) {}

   private:
    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      rtc::CritScope lock(&crit_);
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));

      // Ignore padding-only packets over RTX.
      if (header.payloadType != payload_type_) {
        EXPECT_EQ(retransmission_ssrc_, header.ssrc);
        if (length == header.headerLength + header.paddingLength)
          return SEND_PACKET;
      }

      if (header.timestamp == retransmitted_timestamp_) {
        EXPECT_EQ(retransmission_ssrc_, header.ssrc);
        EXPECT_EQ(retransmission_payload_type_, header.payloadType);
        return SEND_PACKET;
      }

      // Found the final packet of the frame to inflict loss to, drop this and
      // expect a retransmission.
      if (header.payloadType == payload_type_ && header.markerBit &&
          ++marker_bits_observed_ == kDroppedFrameNumber) {
        // This should be the only dropped packet.
        EXPECT_EQ(0u, retransmitted_timestamp_);
        retransmitted_timestamp_ = header.timestamp;
        if (std::find(rendered_timestamps_.begin(), rendered_timestamps_.end(),
                      retransmitted_timestamp_) != rendered_timestamps_.end()) {
          // Frame was rendered before last packet was scheduled for sending.
          // This is extremly rare but possible scenario because prober able to
          // resend packet before it was send.
          // TODO(danilchap): Remove this corner case when prober would not be
          // able to sneak in between packet saved to history for resending and
          // pacer notified about existance of that packet for sending.
          // See https://bugs.chromium.org/p/webrtc/issues/detail?id=5540 for
          // details.
          observation_complete_.Set();
        }
        return DROP_PACKET;
      }

      return SEND_PACKET;
    }

    void OnFrame(const VideoFrame& frame) override {
      EXPECT_EQ(kVideoRotation_90, frame.rotation());
      {
        rtc::CritScope lock(&crit_);
        if (frame.timestamp() == retransmitted_timestamp_)
          observation_complete_.Set();
        rendered_timestamps_.push_back(frame.timestamp());
      }
      orig_renderer_->OnFrame(frame);
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      send_config->rtp.nack.rtp_history_ms = kNackRtpHistoryMs;

      // Insert ourselves into the rendering pipeline.
      RTC_DCHECK(!orig_renderer_);
      orig_renderer_ = (*receive_configs)[0].renderer;
      RTC_DCHECK(orig_renderer_);
      (*receive_configs)[0].disable_prerenderer_smoothing = true;
      (*receive_configs)[0].renderer = this;

      (*receive_configs)[0].rtp.nack.rtp_history_ms = kNackRtpHistoryMs;

      if (payload_type_ == kRedPayloadType) {
        send_config->rtp.ulpfec.ulpfec_payload_type = kUlpfecPayloadType;
        send_config->rtp.ulpfec.red_payload_type = kRedPayloadType;
        if (retransmission_ssrc_ == kSendRtxSsrcs[0])
          send_config->rtp.ulpfec.red_rtx_payload_type = kRtxRedPayloadType;
        (*receive_configs)[0].rtp.ulpfec_payload_type =
            send_config->rtp.ulpfec.ulpfec_payload_type;
        (*receive_configs)[0].rtp.red_payload_type =
            send_config->rtp.ulpfec.red_payload_type;
      }

      if (retransmission_ssrc_ == kSendRtxSsrcs[0]) {
        send_config->rtp.rtx.ssrcs.push_back(kSendRtxSsrcs[0]);
        send_config->rtp.rtx.payload_type = kSendRtxPayloadType;
        (*receive_configs)[0].rtp.rtx_ssrc = kSendRtxSsrcs[0];
        (*receive_configs)[0]
            .rtp.rtx_associated_payload_types[(payload_type_ == kRedPayloadType)
                                                  ? kRtxRedPayloadType
                                                  : kSendRtxPayloadType] =
            payload_type_;
      }
      // Configure encoding and decoding with VP8, since generic packetization
      // doesn't support FEC with NACK.
      RTC_DCHECK_EQ(1, (*receive_configs)[0].decoders.size());
      send_config->encoder_settings.encoder_factory = &encoder_factory_;
      send_config->rtp.payload_name = "VP8";
      encoder_config->codec_type = kVideoCodecVP8;
      (*receive_configs)[0].decoders[0].video_format = SdpVideoFormat("VP8");
    }

    void OnFrameGeneratorCapturerCreated(
        test::FrameGeneratorCapturer* frame_generator_capturer) override {
      frame_generator_capturer->SetFakeRotation(kVideoRotation_90);
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait())
          << "Timed out while waiting for retransmission to render.";
    }

    int GetPayloadType(bool use_rtx, bool use_fec) {
      if (use_fec) {
        if (use_rtx)
          return kRtxRedPayloadType;
        return kRedPayloadType;
      }
      if (use_rtx)
        return kSendRtxPayloadType;
      return kFakeVideoSendPayloadType;
    }

    rtc::CriticalSection crit_;
    rtc::VideoSinkInterface<VideoFrame>* orig_renderer_ = nullptr;
    const int payload_type_;
    const uint32_t retransmission_ssrc_;
    const int retransmission_payload_type_;
    test::FunctionVideoEncoderFactory encoder_factory_;
    const std::string payload_name_;
    int marker_bits_observed_;
    uint32_t retransmitted_timestamp_ RTC_GUARDED_BY(&crit_);
    std::vector<uint32_t> rendered_timestamps_ RTC_GUARDED_BY(&crit_);
  } test(enable_rtx, enable_red);

  RunBaseTest(&test);
}

TEST_F(RetransmissionEndToEndTest, DecodesRetransmittedFrame) {
  DecodesRetransmittedFrame(false, false);
}

TEST_F(RetransmissionEndToEndTest, DecodesRetransmittedFrameOverRtx) {
  DecodesRetransmittedFrame(true, false);
}

TEST_F(RetransmissionEndToEndTest, DecodesRetransmittedFrameByRed) {
  DecodesRetransmittedFrame(false, true);
}

TEST_F(RetransmissionEndToEndTest, DecodesRetransmittedFrameByRedOverRtx) {
  DecodesRetransmittedFrame(true, true);
}

}  // namespace webrtc
