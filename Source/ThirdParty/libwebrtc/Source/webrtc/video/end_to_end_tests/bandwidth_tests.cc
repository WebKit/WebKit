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

#include "api/task_queue/task_queue_base.h"
#include "api/test/simulated_network.h"
#include "api/units/time_delta.h"
#include "api/video/builtin_video_bitrate_allocator_factory.h"
#include "api/video/video_bitrate_allocation.h"
#include "call/fake_network_pipe.h"
#include "call/simulated_network.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_impl2.h"
#include "rtc_base/rate_limiter.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/task_queue_for_test.h"
#include "system_wrappers/include/sleep.h"
#include "test/call_test.h"
#include "test/fake_encoder.h"
#include "test/field_trial.h"
#include "test/gtest.h"
#include "test/rtcp_packet_parser.h"
#include "test/rtp_rtcp_observer.h"
#include "test/video_encoder_proxy_factory.h"
#include "test/video_test_constants.h"

namespace webrtc {
namespace {
enum : int {  // The first valid value is 1.
  kAbsSendTimeExtensionId = 1,
  kTransportSequenceNumberId,
};
}  // namespace

class BandwidthEndToEndTest : public test::CallTest {
 public:
  BandwidthEndToEndTest() = default;
};

TEST_F(BandwidthEndToEndTest, ReceiveStreamSendsRemb) {
  class RembObserver : public test::EndToEndTest {
   public:
    RembObserver() : EndToEndTest(test::VideoTestConstants::kDefaultTimeout) {}

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStreamInterface::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      send_config->rtp.extensions.clear();
      send_config->rtp.extensions.push_back(
          RtpExtension(RtpExtension::kAbsSendTimeUri, kAbsSendTimeExtensionId));
    }

    Action OnReceiveRtcp(rtc::ArrayView<const uint8_t> packet) override {
      test::RtcpPacketParser parser;
      EXPECT_TRUE(parser.Parse(packet));

      if (parser.remb()->num_packets() > 0) {
        EXPECT_EQ(test::VideoTestConstants::kReceiverLocalVideoSsrc,
                  parser.remb()->sender_ssrc());
        EXPECT_LT(0U, parser.remb()->bitrate_bps());
        EXPECT_EQ(1U, parser.remb()->ssrcs().size());
        EXPECT_EQ(test::VideoTestConstants::kVideoSendSsrcs[0],
                  parser.remb()->ssrcs()[0]);
        observation_complete_.Set();
      }

      return SEND_PACKET;
    }
    void PerformTest() override {
      EXPECT_TRUE(Wait()) << "Timed out while waiting for a "
                             "receiver RTCP REMB packet to be "
                             "sent.";
    }
  } test;

  RunBaseTest(&test);
}

class BandwidthStatsTest : public test::EndToEndTest {
 public:
  BandwidthStatsTest(bool send_side_bwe, TaskQueueBase* task_queue)
      : EndToEndTest(test::VideoTestConstants::kDefaultTimeout),
        sender_call_(nullptr),
        receiver_call_(nullptr),
        has_seen_pacer_delay_(false),
        send_side_bwe_(send_side_bwe),
        task_queue_(task_queue) {}

  ~BandwidthStatsTest() override {
    // Block until all already posted tasks run to avoid races when such task
    // accesses `this`.
    SendTask(task_queue_, [] {});
  }

  void ModifyVideoConfigs(
      VideoSendStream::Config* send_config,
      std::vector<VideoReceiveStreamInterface::Config>* receive_configs,
      VideoEncoderConfig* encoder_config) override {
    send_config->rtp.extensions.clear();
    if (!send_side_bwe_) {
      send_config->rtp.extensions.push_back(
          RtpExtension(RtpExtension::kAbsSendTimeUri, kAbsSendTimeExtensionId));
    } else {
      send_config->rtp.extensions.push_back(
          RtpExtension(RtpExtension::kTransportSequenceNumberUri,
                       kTransportSequenceNumberId));
    }

    // Force a too high encoder bitrate to make sure we get pacer delay.
    encoder_config->number_of_streams = 1;
    encoder_config->max_bitrate_bps = kMaxBitrateBps * 2;
    encoder_config->simulcast_layers[0].min_bitrate_bps = kMaxBitrateBps * 2;
    encoder_config->simulcast_layers[0].target_bitrate_bps = kMaxBitrateBps * 2;
    encoder_config->simulcast_layers[0].max_bitrate_bps = kMaxBitrateBps * 2;
  }

  void ModifySenderBitrateConfig(BitrateConstraints* bitrate_config) override {
    bitrate_config->max_bitrate_bps = kMaxBitrateBps;
  }

  // Called on the pacer thread.
  Action OnSendRtp(rtc::ArrayView<const uint8_t> packet) override {
    // Stats need to be fetched on the thread where the caller objects were
    // constructed.
    task_queue_->PostTask([this]() {
      if (!sender_call_ || !receiver_call_) {
        return;
      }

      Call::Stats sender_stats = sender_call_->GetStats();
      if (!has_seen_pacer_delay_) {
        has_seen_pacer_delay_ = sender_stats.pacer_delay_ms > 0;
      }

      if (sender_stats.send_bandwidth_bps > 0 && has_seen_pacer_delay_) {
        Call::Stats receiver_stats = receiver_call_->GetStats();
        if (send_side_bwe_ || receiver_stats.recv_bandwidth_bps > 0) {
          observation_complete_.Set();
        }
      }
    });

    return SEND_PACKET;
  }

  void OnCallsCreated(Call* sender_call, Call* receiver_call) override {
    sender_call_ = sender_call;
    receiver_call_ = receiver_call;
  }

  void OnStreamsStopped() override {
    sender_call_ = nullptr;
    receiver_call_ = nullptr;
  }

  void PerformTest() override {
    EXPECT_TRUE(Wait()) << "Timed out while waiting for "
                           "non-zero bandwidth stats.";
  }

 private:
  static const int kMaxBitrateBps = 3000000;
  Call* sender_call_;
  Call* receiver_call_;
  bool has_seen_pacer_delay_;
  const bool send_side_bwe_;
  TaskQueueBase* const task_queue_;
};

TEST_F(BandwidthEndToEndTest, VerifySendSideBweStats) {
  BandwidthStatsTest test(true, task_queue());
  RunBaseTest(&test);
}

TEST_F(BandwidthEndToEndTest, VerifyRecvSideBweStats) {
  BandwidthStatsTest test(false, task_queue());
  RunBaseTest(&test);
}

// Verifies that it's possible to limit the send BWE by sending a REMB.
// This is verified by allowing the send BWE to ramp-up to >1000 kbps,
// then have the test generate a REMB of 500 kbps and verify that the send BWE
// is reduced to exactly 500 kbps. Then a REMB of 1000 kbps is generated and the
// test verifies that the send BWE ramps back up to exactly 1000 kbps.
TEST_F(BandwidthEndToEndTest, RembWithSendSideBwe) {
  class BweObserver : public test::EndToEndTest {
   public:
    explicit BweObserver(TaskQueueBase* task_queue)
        : EndToEndTest(test::VideoTestConstants::kDefaultTimeout),
          sender_call_(nullptr),
          clock_(Clock::GetRealTimeClock()),
          sender_ssrc_(0),
          remb_bitrate_bps_(1000000),
          state_(kWaitForFirstRampUp),
          retransmission_rate_limiter_(clock_, 1000),
          task_queue_(task_queue) {}

    void OnStreamsStopped() override { rtp_rtcp_ = nullptr; }

    void ModifySenderBitrateConfig(
        BitrateConstraints* bitrate_config) override {
      // Set a high start bitrate to reduce the test completion time.
      bitrate_config->start_bitrate_bps = remb_bitrate_bps_;
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStreamInterface::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      ASSERT_EQ(1u, send_config->rtp.ssrcs.size());
      sender_ssrc_ = send_config->rtp.ssrcs[0];

      encoder_config->max_bitrate_bps = 2000000;

      ASSERT_EQ(1u, receive_configs->size());
      remb_sender_local_ssrc_ = (*receive_configs)[0].rtp.local_ssrc;
      remb_sender_remote_ssrc_ = (*receive_configs)[0].rtp.remote_ssrc;
    }

    void OnCallsCreated(Call* sender_call, Call* receiver_call) override {
      RTC_DCHECK(sender_call);
      sender_call_ = sender_call;
      task_queue_->PostTask([this]() { PollStats(); });
    }

    void OnTransportCreated(
        test::PacketTransport* /*to_receiver*/,
        SimulatedNetworkInterface* /*sender_network*/,
        test::PacketTransport* to_sender,
        SimulatedNetworkInterface* /*receiver_network*/) override {
      RtpRtcpInterface::Configuration config;
      config.receiver_only = true;
      config.clock = clock_;
      config.outgoing_transport = to_sender;
      config.retransmission_rate_limiter = &retransmission_rate_limiter_;
      config.local_media_ssrc = remb_sender_local_ssrc_;

      rtp_rtcp_ = ModuleRtpRtcpImpl2::Create(config);
      rtp_rtcp_->SetRemoteSSRC(remb_sender_remote_ssrc_);
      rtp_rtcp_->SetRTCPStatus(RtcpMode::kReducedSize);
    }

    void PollStats() {
      Call::Stats stats = sender_call_->GetStats();
      switch (state_) {
        case kWaitForFirstRampUp:
          if (stats.send_bandwidth_bps >= remb_bitrate_bps_) {
            state_ = kWaitForRemb;
            remb_bitrate_bps_ /= 2;
            rtp_rtcp_->SetRemb(
                remb_bitrate_bps_,
                std::vector<uint32_t>(&sender_ssrc_, &sender_ssrc_ + 1));
            rtp_rtcp_->SendRTCP(kRtcpRr);
          }
          break;

        case kWaitForRemb:
          if (stats.send_bandwidth_bps == remb_bitrate_bps_) {
            state_ = kWaitForSecondRampUp;
            remb_bitrate_bps_ *= 2;
            rtp_rtcp_->SetRemb(
                remb_bitrate_bps_,
                std::vector<uint32_t>(&sender_ssrc_, &sender_ssrc_ + 1));
            rtp_rtcp_->SendRTCP(kRtcpRr);
          }
          break;

        case kWaitForSecondRampUp:
          if (stats.send_bandwidth_bps == remb_bitrate_bps_) {
            observation_complete_.Set();
            return;
          }
          break;
      }

      task_queue_->PostDelayedTask([this] { PollStats(); },
                                   TimeDelta::Seconds(1));
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait())
          << "Timed out while waiting for bitrate to change according to REMB.";
    }

   private:
    enum TestState { kWaitForFirstRampUp, kWaitForRemb, kWaitForSecondRampUp };

    Call* sender_call_;
    Clock* const clock_;
    uint32_t sender_ssrc_;
    uint32_t remb_sender_local_ssrc_ = 0;
    uint32_t remb_sender_remote_ssrc_ = 0;
    int remb_bitrate_bps_;
    std::unique_ptr<ModuleRtpRtcpImpl2> rtp_rtcp_;
    TestState state_;
    RateLimiter retransmission_rate_limiter_;
    TaskQueueBase* const task_queue_;
  } test(task_queue());

  RunBaseTest(&test);
}

TEST_F(BandwidthEndToEndTest, ReportsSetEncoderRates) {
  // If these fields trial are on, we get lower bitrates than expected by this
  // test, due to the packetization overhead and encoder pushback.
  webrtc::test::ScopedFieldTrials field_trials(
      std::string(field_trial::GetFieldTrialString()) +
      "WebRTC-VideoRateControl/bitrate_adjuster:false/");
  class EncoderRateStatsTest : public test::EndToEndTest,
                               public test::FakeEncoder {
   public:
    explicit EncoderRateStatsTest(TaskQueueBase* task_queue)
        : EndToEndTest(test::VideoTestConstants::kDefaultTimeout),
          FakeEncoder(Clock::GetRealTimeClock()),
          task_queue_(task_queue),
          send_stream_(nullptr),
          encoder_factory_(this),
          bitrate_allocator_factory_(
              CreateBuiltinVideoBitrateAllocatorFactory()),
          bitrate_kbps_(0) {}

    void OnVideoStreamsCreated(VideoSendStream* send_stream,
                               const std::vector<VideoReceiveStreamInterface*>&
                                   receive_streams) override {
      send_stream_ = send_stream;
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStreamInterface::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      send_config->encoder_settings.encoder_factory = &encoder_factory_;
      send_config->encoder_settings.bitrate_allocator_factory =
          bitrate_allocator_factory_.get();
      RTC_DCHECK_EQ(1, encoder_config->number_of_streams);
    }

    void SetRates(const RateControlParameters& parameters) override {
      // Make sure not to trigger on any default zero bitrates.
      if (parameters.bitrate.get_sum_bps() == 0)
        return;
      MutexLock lock(&mutex_);
      bitrate_kbps_ = parameters.bitrate.get_sum_kbps();
      observation_complete_.Set();
    }

    void PerformTest() override {
      ASSERT_TRUE(Wait())
          << "Timed out while waiting for encoder SetRates() call.";

      SendTask(task_queue_, [this]() {
        WaitForEncoderTargetBitrateMatchStats();
        send_stream_->Stop();
        WaitForStatsReportZeroTargetBitrate();
        send_stream_->Start();
        WaitForEncoderTargetBitrateMatchStats();
      });
    }

    void WaitForEncoderTargetBitrateMatchStats() {
      for (int i = 0; i < test::VideoTestConstants::kDefaultTimeout.ms(); ++i) {
        VideoSendStream::Stats stats = send_stream_->GetStats();
        {
          MutexLock lock(&mutex_);
          if ((stats.target_media_bitrate_bps + 500) / 1000 ==
              static_cast<int>(bitrate_kbps_)) {
            return;
          }
        }
        SleepMs(1);
      }
      FAIL()
          << "Timed out waiting for stats reporting the currently set bitrate.";
    }

    void WaitForStatsReportZeroTargetBitrate() {
      for (int i = 0; i < test::VideoTestConstants::kDefaultTimeout.ms(); ++i) {
        if (send_stream_->GetStats().target_media_bitrate_bps == 0) {
          return;
        }
        SleepMs(1);
      }
      FAIL() << "Timed out waiting for stats reporting zero bitrate.";
    }

   private:
    TaskQueueBase* const task_queue_;
    Mutex mutex_;
    VideoSendStream* send_stream_;
    test::VideoEncoderProxyFactory encoder_factory_;
    std::unique_ptr<VideoBitrateAllocatorFactory> bitrate_allocator_factory_;
    uint32_t bitrate_kbps_ RTC_GUARDED_BY(mutex_);
  } test(task_queue());

  RunBaseTest(&test);
}
}  // namespace webrtc
