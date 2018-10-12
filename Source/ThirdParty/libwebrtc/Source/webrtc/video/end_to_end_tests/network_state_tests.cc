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
#include "system_wrappers/include/sleep.h"
#include "test/call_test.h"
#include "test/fake_encoder.h"
#include "test/gtest.h"
#include "test/video_encoder_proxy_factory.h"

namespace webrtc {
namespace {
constexpr int kSilenceTimeoutMs = 2000;
}

class NetworkStateEndToEndTest : public test::CallTest {
 protected:
  class UnusedTransport : public Transport {
   private:
    bool SendRtp(const uint8_t* packet,
                 size_t length,
                 const PacketOptions& options) override {
      ADD_FAILURE() << "Unexpected RTP sent.";
      return false;
    }

    bool SendRtcp(const uint8_t* packet, size_t length) override {
      ADD_FAILURE() << "Unexpected RTCP sent.";
      return false;
    }
  };
  class RequiredTransport : public Transport {
   public:
    RequiredTransport(bool rtp_required, bool rtcp_required)
        : need_rtp_(rtp_required), need_rtcp_(rtcp_required) {}
    ~RequiredTransport() {
      if (need_rtp_) {
        ADD_FAILURE() << "Expected RTP packet not sent.";
      }
      if (need_rtcp_) {
        ADD_FAILURE() << "Expected RTCP packet not sent.";
      }
    }

   private:
    bool SendRtp(const uint8_t* packet,
                 size_t length,
                 const PacketOptions& options) override {
      rtc::CritScope lock(&crit_);
      need_rtp_ = false;
      return true;
    }

    bool SendRtcp(const uint8_t* packet, size_t length) override {
      rtc::CritScope lock(&crit_);
      need_rtcp_ = false;
      return true;
    }
    bool need_rtp_;
    bool need_rtcp_;
    rtc::CriticalSection crit_;
  };
  void VerifyNewVideoSendStreamsRespectNetworkState(
      MediaType network_to_bring_up,
      VideoEncoder* encoder,
      Transport* transport);
  void VerifyNewVideoReceiveStreamsRespectNetworkState(
      MediaType network_to_bring_up,
      Transport* transport);
};

void NetworkStateEndToEndTest::VerifyNewVideoSendStreamsRespectNetworkState(
    MediaType network_to_bring_up,
    VideoEncoder* encoder,
    Transport* transport) {
  test::VideoEncoderProxyFactory encoder_factory(encoder);

  task_queue_.SendTask([this, network_to_bring_up, &encoder_factory,
                        transport]() {
    CreateSenderCall(Call::Config(send_event_log_.get()));
    sender_call_->SignalChannelNetworkState(network_to_bring_up, kNetworkUp);

    CreateSendConfig(1, 0, 0, transport);
    GetVideoSendConfig()->encoder_settings.encoder_factory = &encoder_factory;
    CreateVideoStreams();
    CreateFrameGeneratorCapturer(kDefaultFramerate, kDefaultWidth,
                                 kDefaultHeight);

    Start();
  });

  SleepMs(kSilenceTimeoutMs);

  task_queue_.SendTask([this]() {
    Stop();
    DestroyStreams();
    DestroyCalls();
  });
}

void NetworkStateEndToEndTest::VerifyNewVideoReceiveStreamsRespectNetworkState(
    MediaType network_to_bring_up,
    Transport* transport) {
  std::unique_ptr<test::DirectTransport> sender_transport;

  task_queue_.SendTask([this, &sender_transport, network_to_bring_up,
                        transport]() {
    CreateCalls();
    receiver_call_->SignalChannelNetworkState(network_to_bring_up, kNetworkUp);
    sender_transport = absl::make_unique<test::DirectTransport>(
        &task_queue_,
        absl::make_unique<FakeNetworkPipe>(
            Clock::GetRealTimeClock(), absl::make_unique<SimulatedNetwork>(
                                           DefaultNetworkSimulationConfig())),
        sender_call_.get(), payload_type_map_);
    sender_transport->SetReceiver(receiver_call_->Receiver());
    CreateSendConfig(1, 0, 0, sender_transport.get());
    CreateMatchingReceiveConfigs(transport);
    CreateVideoStreams();
    CreateFrameGeneratorCapturer(kDefaultFramerate, kDefaultWidth,
                                 kDefaultHeight);
    Start();
  });

  SleepMs(kSilenceTimeoutMs);

  task_queue_.SendTask([this, &sender_transport]() {
    Stop();
    DestroyStreams();
    sender_transport.reset();
    DestroyCalls();
  });
}

TEST_F(NetworkStateEndToEndTest, RespectsNetworkState) {
  // TODO(pbos): Remove accepted downtime packets etc. when signaling network
  // down blocks until no more packets will be sent.

  // Pacer will send from its packet list and then send required padding before
  // checking paused_ again. This should be enough for one round of pacing,
  // otherwise increase.
  static const int kNumAcceptedDowntimeRtp = 5;
  // A single RTCP may be in the pipeline.
  static const int kNumAcceptedDowntimeRtcp = 1;
  class NetworkStateTest : public test::EndToEndTest, public test::FakeEncoder {
   public:
    explicit NetworkStateTest(
        test::SingleThreadedTaskQueueForTesting* task_queue)
        : EndToEndTest(kDefaultTimeoutMs),
          FakeEncoder(Clock::GetRealTimeClock()),
          task_queue_(task_queue),
          encoded_frames_(false, false),
          packet_event_(false, false),
          sender_call_(nullptr),
          receiver_call_(nullptr),
          encoder_factory_(this),
          sender_state_(kNetworkUp),
          sender_rtp_(0),
          sender_padding_(0),
          sender_rtcp_(0),
          receiver_rtcp_(0),
          down_frames_(0) {}

    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      rtc::CritScope lock(&test_crit_);
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));
      if (length == header.headerLength + header.paddingLength)
        ++sender_padding_;
      ++sender_rtp_;
      packet_event_.Set();
      return SEND_PACKET;
    }

    Action OnSendRtcp(const uint8_t* packet, size_t length) override {
      rtc::CritScope lock(&test_crit_);
      ++sender_rtcp_;
      packet_event_.Set();
      return SEND_PACKET;
    }

    Action OnReceiveRtp(const uint8_t* packet, size_t length) override {
      ADD_FAILURE() << "Unexpected receiver RTP, should not be sending.";
      return SEND_PACKET;
    }

    Action OnReceiveRtcp(const uint8_t* packet, size_t length) override {
      rtc::CritScope lock(&test_crit_);
      ++receiver_rtcp_;
      packet_event_.Set();
      return SEND_PACKET;
    }

    void OnCallsCreated(Call* sender_call, Call* receiver_call) override {
      sender_call_ = sender_call;
      receiver_call_ = receiver_call;
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      send_config->encoder_settings.encoder_factory = &encoder_factory_;
    }

    void PerformTest() override {
      EXPECT_TRUE(encoded_frames_.Wait(kDefaultTimeoutMs))
          << "No frames received by the encoder.";

      task_queue_->SendTask([this]() {
        // Wait for packets from both sender/receiver.
        WaitForPacketsOrSilence(false, false);

        // Sender-side network down for audio; there should be no effect on
        // video
        sender_call_->SignalChannelNetworkState(MediaType::AUDIO, kNetworkDown);
        WaitForPacketsOrSilence(false, false);

        // Receiver-side network down for audio; no change expected
        receiver_call_->SignalChannelNetworkState(MediaType::AUDIO,
                                                  kNetworkDown);
        WaitForPacketsOrSilence(false, false);

        // Sender-side network down.
        sender_call_->SignalChannelNetworkState(MediaType::VIDEO, kNetworkDown);
        {
          rtc::CritScope lock(&test_crit_);
          // After network goes down we shouldn't be encoding more frames.
          sender_state_ = kNetworkDown;
        }
        // Wait for receiver-packets and no sender packets.
        WaitForPacketsOrSilence(true, false);

        // Receiver-side network down.
        receiver_call_->SignalChannelNetworkState(MediaType::VIDEO,
                                                  kNetworkDown);
        WaitForPacketsOrSilence(true, true);

        // Network up for audio for both sides; video is still not expected to
        // start
        sender_call_->SignalChannelNetworkState(MediaType::AUDIO, kNetworkUp);
        receiver_call_->SignalChannelNetworkState(MediaType::AUDIO, kNetworkUp);
        WaitForPacketsOrSilence(true, true);

        // Network back up again for both.
        {
          rtc::CritScope lock(&test_crit_);
          // It's OK to encode frames again, as we're about to bring up the
          // network.
          sender_state_ = kNetworkUp;
        }
        sender_call_->SignalChannelNetworkState(MediaType::VIDEO, kNetworkUp);
        receiver_call_->SignalChannelNetworkState(MediaType::VIDEO, kNetworkUp);
        WaitForPacketsOrSilence(false, false);

        // TODO(skvlad): add tests to verify that the audio streams are stopped
        // when the network goes down for audio once the workaround in
        // paced_sender.cc is removed.
      });
    }

    int32_t Encode(const VideoFrame& input_image,
                   const CodecSpecificInfo* codec_specific_info,
                   const std::vector<FrameType>* frame_types) override {
      {
        rtc::CritScope lock(&test_crit_);
        if (sender_state_ == kNetworkDown) {
          ++down_frames_;
          EXPECT_LE(down_frames_, 1)
              << "Encoding more than one frame while network is down.";
          if (down_frames_ > 1)
            encoded_frames_.Set();
        } else {
          encoded_frames_.Set();
        }
      }
      return test::FakeEncoder::Encode(input_image, codec_specific_info,
                                       frame_types);
    }

   private:
    void WaitForPacketsOrSilence(bool sender_down, bool receiver_down) {
      int64_t initial_time_ms = clock_->TimeInMilliseconds();
      int initial_sender_rtp;
      int initial_sender_rtcp;
      int initial_receiver_rtcp;
      {
        rtc::CritScope lock(&test_crit_);
        initial_sender_rtp = sender_rtp_;
        initial_sender_rtcp = sender_rtcp_;
        initial_receiver_rtcp = receiver_rtcp_;
      }
      bool sender_done = false;
      bool receiver_done = false;
      while (!sender_done || !receiver_done) {
        packet_event_.Wait(kSilenceTimeoutMs);
        int64_t time_now_ms = clock_->TimeInMilliseconds();
        rtc::CritScope lock(&test_crit_);
        if (sender_down) {
          ASSERT_LE(sender_rtp_ - initial_sender_rtp - sender_padding_,
                    kNumAcceptedDowntimeRtp)
              << "RTP sent during sender-side downtime.";
          ASSERT_LE(sender_rtcp_ - initial_sender_rtcp,
                    kNumAcceptedDowntimeRtcp)
              << "RTCP sent during sender-side downtime.";
          if (time_now_ms - initial_time_ms >=
              static_cast<int64_t>(kSilenceTimeoutMs)) {
            sender_done = true;
          }
        } else {
          if (sender_rtp_ > initial_sender_rtp + kNumAcceptedDowntimeRtp)
            sender_done = true;
        }
        if (receiver_down) {
          ASSERT_LE(receiver_rtcp_ - initial_receiver_rtcp,
                    kNumAcceptedDowntimeRtcp)
              << "RTCP sent during receiver-side downtime.";
          if (time_now_ms - initial_time_ms >=
              static_cast<int64_t>(kSilenceTimeoutMs)) {
            receiver_done = true;
          }
        } else {
          if (receiver_rtcp_ > initial_receiver_rtcp + kNumAcceptedDowntimeRtcp)
            receiver_done = true;
        }
      }
    }

    test::SingleThreadedTaskQueueForTesting* const task_queue_;
    rtc::CriticalSection test_crit_;
    rtc::Event encoded_frames_;
    rtc::Event packet_event_;
    Call* sender_call_;
    Call* receiver_call_;
    test::VideoEncoderProxyFactory encoder_factory_;
    NetworkState sender_state_ RTC_GUARDED_BY(test_crit_);
    int sender_rtp_ RTC_GUARDED_BY(test_crit_);
    int sender_padding_ RTC_GUARDED_BY(test_crit_);
    int sender_rtcp_ RTC_GUARDED_BY(test_crit_);
    int receiver_rtcp_ RTC_GUARDED_BY(test_crit_);
    int down_frames_ RTC_GUARDED_BY(test_crit_);
  } test(&task_queue_);

  RunBaseTest(&test);
}

TEST_F(NetworkStateEndToEndTest, NewVideoSendStreamsRespectVideoNetworkDown) {
  class UnusedEncoder : public test::FakeEncoder {
   public:
    UnusedEncoder() : FakeEncoder(Clock::GetRealTimeClock()) {}

    int32_t InitEncode(const VideoCodec* config,
                       int32_t number_of_cores,
                       size_t max_payload_size) override {
      EXPECT_GT(config->startBitrate, 0u);
      return 0;
    }
    int32_t Encode(const VideoFrame& input_image,
                   const CodecSpecificInfo* codec_specific_info,
                   const std::vector<FrameType>* frame_types) override {
      ADD_FAILURE() << "Unexpected frame encode.";
      return test::FakeEncoder::Encode(input_image, codec_specific_info,
                                       frame_types);
    }
  };

  UnusedEncoder unused_encoder;
  UnusedTransport unused_transport;
  VerifyNewVideoSendStreamsRespectNetworkState(
      MediaType::AUDIO, &unused_encoder, &unused_transport);
}

TEST_F(NetworkStateEndToEndTest, NewVideoSendStreamsIgnoreAudioNetworkDown) {
  class RequiredEncoder : public test::FakeEncoder {
   public:
    RequiredEncoder()
        : FakeEncoder(Clock::GetRealTimeClock()), encoded_frame_(false) {}
    ~RequiredEncoder() {
      if (!encoded_frame_) {
        ADD_FAILURE() << "Didn't encode an expected frame";
      }
    }
    int32_t Encode(const VideoFrame& input_image,
                   const CodecSpecificInfo* codec_specific_info,
                   const std::vector<FrameType>* frame_types) override {
      encoded_frame_ = true;
      return test::FakeEncoder::Encode(input_image, codec_specific_info,
                                       frame_types);
    }

   private:
    bool encoded_frame_;
  };

  RequiredTransport required_transport(true /*rtp*/, false /*rtcp*/);
  RequiredEncoder required_encoder;
  VerifyNewVideoSendStreamsRespectNetworkState(
      MediaType::VIDEO, &required_encoder, &required_transport);
}

TEST_F(NetworkStateEndToEndTest,
       NewVideoReceiveStreamsRespectVideoNetworkDown) {
  UnusedTransport transport;
  VerifyNewVideoReceiveStreamsRespectNetworkState(MediaType::AUDIO, &transport);
}

TEST_F(NetworkStateEndToEndTest, NewVideoReceiveStreamsIgnoreAudioNetworkDown) {
  RequiredTransport transport(false /*rtp*/, true /*rtcp*/);
  VerifyNewVideoReceiveStreamsRespectNetworkState(MediaType::VIDEO, &transport);
}

}  // namespace webrtc
