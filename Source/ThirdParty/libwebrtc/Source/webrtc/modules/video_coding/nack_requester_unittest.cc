/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/nack_requester.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>

#include "system_wrappers/include/clock.h"
#include "test/gtest.h"
#include "test/run_loop.h"
#include "test/scoped_key_value_config.h"

namespace webrtc {
// TODO(bugs.webrtc.org/11594): Use the use the GlobalSimulatedTimeController
// instead of RunLoop. At the moment we mix use of the Clock and the underlying
// implementation of RunLoop, which is realtime.
class TestNackRequester : public ::testing::Test,
                          public NackSender,
                          public KeyFrameRequestSender {
 protected:
  TestNackRequester()
      : clock_(new SimulatedClock(0)), keyframes_requested_(0) {}

  void SetUp() override {}

  void SendNack(const std::vector<uint16_t>& sequence_numbers,
                bool buffering_allowed) override {
    sent_nacks_.insert(sent_nacks_.end(), sequence_numbers.begin(),
                       sequence_numbers.end());
    if (waiting_for_send_nack_) {
      waiting_for_send_nack_ = false;
      loop_.Quit();
    }
  }

  void RequestKeyFrame() override { ++keyframes_requested_; }

  void Flush() {
    // nack_module.Process();
    loop_.Flush();
  }

  bool WaitForSendNack() {
    if (timed_out_) {
      RTC_DCHECK_NOTREACHED();
      return false;
    }

    RTC_DCHECK(!waiting_for_send_nack_);

    waiting_for_send_nack_ = true;
    loop_.task_queue()->PostDelayedTask(
        [this]() {
          timed_out_ = true;
          loop_.Quit();
        },
        TimeDelta::Seconds(1));

    loop_.Run();

    if (timed_out_)
      return false;

    RTC_DCHECK(!waiting_for_send_nack_);
    return true;
  }

  NackRequester& CreateNackModule(
      TimeDelta interval = NackPeriodicProcessor::kUpdateInterval) {
    RTC_DCHECK(!nack_module_.get());
    nack_periodic_processor_ =
        std::make_unique<NackPeriodicProcessor>(interval);
    test::ScopedKeyValueConfig empty_field_trials_;
    nack_module_ = std::make_unique<NackRequester>(
        TaskQueueBase::Current(), nack_periodic_processor_.get(), clock_.get(),
        this, this, empty_field_trials_);
    nack_module_->UpdateRtt(kDefaultRttMs);
    return *nack_module_.get();
  }

  static constexpr int64_t kDefaultRttMs = 20;
  rtc::AutoThread main_thread_;
  test::RunLoop loop_;
  std::unique_ptr<SimulatedClock> clock_;
  std::unique_ptr<NackPeriodicProcessor> nack_periodic_processor_;
  std::unique_ptr<NackRequester> nack_module_;
  std::vector<uint16_t> sent_nacks_;
  int keyframes_requested_;
  bool waiting_for_send_nack_ = false;
  bool timed_out_ = false;
};

TEST_F(TestNackRequester, NackOnePacket) {
  NackRequester& nack_module = CreateNackModule();
  nack_module.OnReceivedPacket(1);
  nack_module.OnReceivedPacket(3);
  ASSERT_EQ(1u, sent_nacks_.size());
  EXPECT_EQ(2, sent_nacks_[0]);
}

TEST_F(TestNackRequester, WrappingSeqNum) {
  NackRequester& nack_module = CreateNackModule();
  nack_module.OnReceivedPacket(0xfffe);
  nack_module.OnReceivedPacket(1);
  ASSERT_EQ(2u, sent_nacks_.size());
  EXPECT_EQ(0xffff, sent_nacks_[0]);
  EXPECT_EQ(0, sent_nacks_[1]);
}

TEST_F(TestNackRequester, ResendNack) {
  NackRequester& nack_module = CreateNackModule(TimeDelta::Millis(1));
  nack_module.OnReceivedPacket(1);
  nack_module.OnReceivedPacket(3);
  size_t expected_nacks_sent = 1;
  ASSERT_EQ(expected_nacks_sent, sent_nacks_.size());
  EXPECT_EQ(2, sent_nacks_[0]);

  nack_module.UpdateRtt(1);
  clock_->AdvanceTimeMilliseconds(1);
  WaitForSendNack();  // Fast retransmit allowed.
  EXPECT_EQ(++expected_nacks_sent, sent_nacks_.size());

  // Each try has to wait rtt by default.
  for (int i = 2; i < 10; ++i) {
    // Change RTT, above the 40ms max for exponential backoff.
    TimeDelta rtt = TimeDelta::Millis(160);  // + (i * 10 - 40)
    nack_module.UpdateRtt(rtt.ms());

    // RTT gets capped at 160ms in backoff calculations.
    TimeDelta expected_backoff_delay =
        (i - 1) * std::min(rtt, TimeDelta::Millis(160));

    // Move to one millisecond before next allowed NACK.
    clock_->AdvanceTimeMilliseconds(expected_backoff_delay.ms() - 1);
    Flush();
    EXPECT_EQ(expected_nacks_sent, sent_nacks_.size());

    // Move to one millisecond after next allowed NACK.
    // After rather than on to avoid rounding errors.
    clock_->AdvanceTimeMilliseconds(2);
    WaitForSendNack();  // Now allowed.
    EXPECT_EQ(++expected_nacks_sent, sent_nacks_.size());
  }

  // Giving up after 10 tries.
  clock_->AdvanceTimeMilliseconds(3000);
  Flush();
  EXPECT_EQ(expected_nacks_sent, sent_nacks_.size());
}

TEST_F(TestNackRequester, ResendPacketMaxRetries) {
  NackRequester& nack_module = CreateNackModule(TimeDelta::Millis(1));
  nack_module.OnReceivedPacket(1);
  nack_module.OnReceivedPacket(3);
  ASSERT_EQ(1u, sent_nacks_.size());
  EXPECT_EQ(2, sent_nacks_[0]);

  int backoff_factor = 1;
  for (size_t retries = 1; retries < 10; ++retries) {
    // Exponential backoff, so that we don't reject NACK because of time.
    clock_->AdvanceTimeMilliseconds(backoff_factor * kDefaultRttMs);
    backoff_factor *= 2;
    WaitForSendNack();
    EXPECT_EQ(retries + 1, sent_nacks_.size());
  }

  clock_->AdvanceTimeMilliseconds(backoff_factor * kDefaultRttMs);
  Flush();
  EXPECT_EQ(10u, sent_nacks_.size());
}

TEST_F(TestNackRequester, TooLargeNackList) {
  NackRequester& nack_module = CreateNackModule();
  nack_module.OnReceivedPacket(0);
  nack_module.OnReceivedPacket(1001);
  EXPECT_EQ(1000u, sent_nacks_.size());
  EXPECT_EQ(0, keyframes_requested_);
  nack_module.OnReceivedPacket(1003);
  EXPECT_EQ(1000u, sent_nacks_.size());
  EXPECT_EQ(1, keyframes_requested_);
  nack_module.OnReceivedPacket(1004);
  EXPECT_EQ(1000u, sent_nacks_.size());
  EXPECT_EQ(1, keyframes_requested_);
}

TEST_F(TestNackRequester, ClearUpTo) {
  NackRequester& nack_module = CreateNackModule(TimeDelta::Millis(1));
  nack_module.OnReceivedPacket(0);
  nack_module.OnReceivedPacket(100);
  EXPECT_EQ(99u, sent_nacks_.size());

  sent_nacks_.clear();
  clock_->AdvanceTimeMilliseconds(100);
  nack_module.ClearUpTo(50);
  WaitForSendNack();
  ASSERT_EQ(50u, sent_nacks_.size());
  EXPECT_EQ(50, sent_nacks_[0]);
}

TEST_F(TestNackRequester, ClearUpToWrap) {
  NackRequester& nack_module = CreateNackModule();
  nack_module.OnReceivedPacket(0xfff0);
  nack_module.OnReceivedPacket(0xf);
  EXPECT_EQ(30u, sent_nacks_.size());

  sent_nacks_.clear();
  clock_->AdvanceTimeMilliseconds(100);
  nack_module.ClearUpTo(0);
  WaitForSendNack();
  ASSERT_EQ(15u, sent_nacks_.size());
  EXPECT_EQ(0, sent_nacks_[0]);
}

TEST_F(TestNackRequester, PacketNackCount) {
  NackRequester& nack_module = CreateNackModule(TimeDelta::Millis(1));
  EXPECT_EQ(0, nack_module.OnReceivedPacket(0));
  EXPECT_EQ(0, nack_module.OnReceivedPacket(2));
  EXPECT_EQ(1, nack_module.OnReceivedPacket(1));

  sent_nacks_.clear();
  nack_module.UpdateRtt(100);
  EXPECT_EQ(0, nack_module.OnReceivedPacket(5));
  clock_->AdvanceTimeMilliseconds(100);
  WaitForSendNack();
  EXPECT_EQ(4u, sent_nacks_.size());

  clock_->AdvanceTimeMilliseconds(125);
  WaitForSendNack();

  EXPECT_EQ(6u, sent_nacks_.size());

  EXPECT_EQ(3, nack_module.OnReceivedPacket(3));
  EXPECT_EQ(3, nack_module.OnReceivedPacket(4));
  EXPECT_EQ(0, nack_module.OnReceivedPacket(4));
}

TEST_F(TestNackRequester, HandleFecRecoveredPacket) {
  NackRequester& nack_module = CreateNackModule();
  nack_module.OnReceivedPacket(1);
  nack_module.OnReceivedPacket(4, /*is_recovered=*/true);
  EXPECT_EQ(0u, sent_nacks_.size());
  nack_module.OnReceivedPacket(5);
  EXPECT_EQ(2u, sent_nacks_.size());
}

TEST_F(TestNackRequester, SendNackWithoutDelay) {
  NackRequester& nack_module = CreateNackModule();
  nack_module.OnReceivedPacket(0);
  nack_module.OnReceivedPacket(100);
  EXPECT_EQ(99u, sent_nacks_.size());
}

class TestNackRequesterWithFieldTrial : public ::testing::Test,
                                        public NackSender,
                                        public KeyFrameRequestSender {
 protected:
  TestNackRequesterWithFieldTrial()
      : nack_delay_field_trial_("WebRTC-SendNackDelayMs/10/"),
        clock_(new SimulatedClock(0)),
        nack_module_(TaskQueueBase::Current(),
                     &nack_periodic_processor_,
                     clock_.get(),
                     this,
                     this,
                     nack_delay_field_trial_),
        keyframes_requested_(0) {}

  void SendNack(const std::vector<uint16_t>& sequence_numbers,
                bool buffering_allowed) override {
    sent_nacks_.insert(sent_nacks_.end(), sequence_numbers.begin(),
                       sequence_numbers.end());
  }

  void RequestKeyFrame() override { ++keyframes_requested_; }

  test::ScopedKeyValueConfig nack_delay_field_trial_;
  rtc::AutoThread main_thread_;
  std::unique_ptr<SimulatedClock> clock_;
  NackPeriodicProcessor nack_periodic_processor_;
  NackRequester nack_module_;
  std::vector<uint16_t> sent_nacks_;
  int keyframes_requested_;
};

TEST_F(TestNackRequesterWithFieldTrial, SendNackWithDelay) {
  nack_module_.OnReceivedPacket(0);
  nack_module_.OnReceivedPacket(100);
  EXPECT_EQ(0u, sent_nacks_.size());
  clock_->AdvanceTimeMilliseconds(10);
  nack_module_.OnReceivedPacket(106);
  EXPECT_EQ(99u, sent_nacks_.size());
  clock_->AdvanceTimeMilliseconds(10);
  nack_module_.OnReceivedPacket(109);
  EXPECT_EQ(104u, sent_nacks_.size());
}
}  // namespace webrtc
