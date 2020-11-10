/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/nack_module2.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>

#include "system_wrappers/include/clock.h"
#include "test/field_trial.h"
#include "test/gtest.h"
#include "test/run_loop.h"

namespace webrtc {
// TODO(bugs.webrtc.org/11594): Use the use the GlobalSimulatedTimeController
// instead of RunLoop. At the moment we mix use of the Clock and the underlying
// implementation of RunLoop, which is realtime.
class TestNackModule2 : public ::testing::TestWithParam<bool>,
                        public NackSender,
                        public KeyFrameRequestSender {
 protected:
  TestNackModule2()
      : clock_(new SimulatedClock(0)),
        field_trial_(GetParam()
                         ? "WebRTC-ExponentialNackBackoff/enabled:true/"
                         : "WebRTC-ExponentialNackBackoff/enabled:false/"),
        keyframes_requested_(0) {}

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
      RTC_NOTREACHED();
      return false;
    }

    RTC_DCHECK(!waiting_for_send_nack_);

    waiting_for_send_nack_ = true;
    loop_.PostDelayedTask(
        [this]() {
          timed_out_ = true;
          loop_.Quit();
        },
        1000);

    loop_.Run();

    if (timed_out_)
      return false;

    RTC_DCHECK(!waiting_for_send_nack_);
    return true;
  }

  NackModule2& CreateNackModule(
      TimeDelta interval = NackModule2::kUpdateInterval) {
    RTC_DCHECK(!nack_module_.get());
    nack_module_ = std::make_unique<NackModule2>(
        TaskQueueBase::Current(), clock_.get(), this, this, interval);
    nack_module_->UpdateRtt(kDefaultRttMs);
    return *nack_module_.get();
  }

  static constexpr int64_t kDefaultRttMs = 20;
  test::RunLoop loop_;
  std::unique_ptr<SimulatedClock> clock_;
  test::ScopedFieldTrials field_trial_;
  std::unique_ptr<NackModule2> nack_module_;
  std::vector<uint16_t> sent_nacks_;
  int keyframes_requested_;
  bool waiting_for_send_nack_ = false;
  bool timed_out_ = false;
};

TEST_P(TestNackModule2, NackOnePacket) {
  NackModule2& nack_module = CreateNackModule();
  nack_module.OnReceivedPacket(1, false, false);
  nack_module.OnReceivedPacket(3, false, false);
  ASSERT_EQ(1u, sent_nacks_.size());
  EXPECT_EQ(2, sent_nacks_[0]);
}

TEST_P(TestNackModule2, WrappingSeqNum) {
  NackModule2& nack_module = CreateNackModule();
  nack_module.OnReceivedPacket(0xfffe, false, false);
  nack_module.OnReceivedPacket(1, false, false);
  ASSERT_EQ(2u, sent_nacks_.size());
  EXPECT_EQ(0xffff, sent_nacks_[0]);
  EXPECT_EQ(0, sent_nacks_[1]);
}

TEST_P(TestNackModule2, WrappingSeqNumClearToKeyframe) {
  NackModule2& nack_module = CreateNackModule(TimeDelta::Millis(10));
  nack_module.OnReceivedPacket(0xfffe, false, false);
  nack_module.OnReceivedPacket(1, false, false);
  ASSERT_EQ(2u, sent_nacks_.size());
  EXPECT_EQ(0xffff, sent_nacks_[0]);
  EXPECT_EQ(0, sent_nacks_[1]);

  sent_nacks_.clear();
  nack_module.OnReceivedPacket(2, true, false);
  ASSERT_EQ(0u, sent_nacks_.size());

  nack_module.OnReceivedPacket(501, true, false);
  ASSERT_EQ(498u, sent_nacks_.size());
  for (int seq_num = 3; seq_num < 501; ++seq_num)
    EXPECT_EQ(seq_num, sent_nacks_[seq_num - 3]);

  sent_nacks_.clear();
  nack_module.OnReceivedPacket(1001, false, false);
  EXPECT_EQ(499u, sent_nacks_.size());
  for (int seq_num = 502; seq_num < 1001; ++seq_num)
    EXPECT_EQ(seq_num, sent_nacks_[seq_num - 502]);

  sent_nacks_.clear();
  clock_->AdvanceTimeMilliseconds(100);
  ASSERT_TRUE(WaitForSendNack());
  ASSERT_EQ(999u, sent_nacks_.size());
  EXPECT_EQ(0xffff, sent_nacks_[0]);
  EXPECT_EQ(0, sent_nacks_[1]);
  for (int seq_num = 3; seq_num < 501; ++seq_num)
    EXPECT_EQ(seq_num, sent_nacks_[seq_num - 1]);
  for (int seq_num = 502; seq_num < 1001; ++seq_num)
    EXPECT_EQ(seq_num, sent_nacks_[seq_num - 2]);

  // Adding packet 1004 will cause the nack list to reach it's max limit.
  // It will then clear all nacks up to the next keyframe (seq num 2),
  // thus removing 0xffff and 0 from the nack list.
  sent_nacks_.clear();
  nack_module.OnReceivedPacket(1004, false, false);
  ASSERT_EQ(2u, sent_nacks_.size());
  EXPECT_EQ(1002, sent_nacks_[0]);
  EXPECT_EQ(1003, sent_nacks_[1]);

  sent_nacks_.clear();
  clock_->AdvanceTimeMilliseconds(100);
  ASSERT_TRUE(WaitForSendNack());
  ASSERT_EQ(999u, sent_nacks_.size());
  for (int seq_num = 3; seq_num < 501; ++seq_num)
    EXPECT_EQ(seq_num, sent_nacks_[seq_num - 3]);
  for (int seq_num = 502; seq_num < 1001; ++seq_num)
    EXPECT_EQ(seq_num, sent_nacks_[seq_num - 4]);

  // Adding packet 1007 will cause the nack module to overflow again, thus
  // clearing everything up to 501 which is the next keyframe.
  nack_module.OnReceivedPacket(1007, false, false);
  sent_nacks_.clear();
  clock_->AdvanceTimeMilliseconds(100);
  ASSERT_TRUE(WaitForSendNack());
  ASSERT_EQ(503u, sent_nacks_.size());
  for (int seq_num = 502; seq_num < 1001; ++seq_num)
    EXPECT_EQ(seq_num, sent_nacks_[seq_num - 502]);
  EXPECT_EQ(1005, sent_nacks_[501]);
  EXPECT_EQ(1006, sent_nacks_[502]);
}

TEST_P(TestNackModule2, ResendNack) {
  NackModule2& nack_module = CreateNackModule(TimeDelta::Millis(1));
  nack_module.OnReceivedPacket(1, false, false);
  nack_module.OnReceivedPacket(3, false, false);
  size_t expected_nacks_sent = 1;
  ASSERT_EQ(expected_nacks_sent, sent_nacks_.size());
  EXPECT_EQ(2, sent_nacks_[0]);

  if (GetParam()) {
    // Retry has to wait at least 5ms by default.
    nack_module.UpdateRtt(1);
    clock_->AdvanceTimeMilliseconds(4);
    Flush();  // Too early.
    EXPECT_EQ(expected_nacks_sent, sent_nacks_.size());

    clock_->AdvanceTimeMilliseconds(1);
    WaitForSendNack();  // Now allowed.
    EXPECT_EQ(++expected_nacks_sent, sent_nacks_.size());
  } else {
    nack_module.UpdateRtt(1);
    clock_->AdvanceTimeMilliseconds(1);
    WaitForSendNack();  // Fast retransmit allowed.
    EXPECT_EQ(++expected_nacks_sent, sent_nacks_.size());
  }

  // N:th try has to wait b^(N-1) * rtt by default.
  const double b = GetParam() ? 1.25 : 1.0;
  for (int i = 2; i < 10; ++i) {
    // Change RTT, above the 40ms max for exponential backoff.
    TimeDelta rtt = TimeDelta::Millis(160);  // + (i * 10 - 40)
    nack_module.UpdateRtt(rtt.ms());

    // RTT gets capped at 160ms in backoff calculations.
    TimeDelta expected_backoff_delay =
        std::pow(b, i - 1) * std::min(rtt, TimeDelta::Millis(160));

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

TEST_P(TestNackModule2, ResendPacketMaxRetries) {
  NackModule2& nack_module = CreateNackModule(TimeDelta::Millis(1));
  nack_module.OnReceivedPacket(1, false, false);
  nack_module.OnReceivedPacket(3, false, false);
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

TEST_P(TestNackModule2, TooLargeNackList) {
  NackModule2& nack_module = CreateNackModule();
  nack_module.OnReceivedPacket(0, false, false);
  nack_module.OnReceivedPacket(1001, false, false);
  EXPECT_EQ(1000u, sent_nacks_.size());
  EXPECT_EQ(0, keyframes_requested_);
  nack_module.OnReceivedPacket(1003, false, false);
  EXPECT_EQ(1000u, sent_nacks_.size());
  EXPECT_EQ(1, keyframes_requested_);
  nack_module.OnReceivedPacket(1004, false, false);
  EXPECT_EQ(1000u, sent_nacks_.size());
  EXPECT_EQ(1, keyframes_requested_);
}

TEST_P(TestNackModule2, TooLargeNackListWithKeyFrame) {
  NackModule2& nack_module = CreateNackModule();
  nack_module.OnReceivedPacket(0, false, false);
  nack_module.OnReceivedPacket(1, true, false);
  nack_module.OnReceivedPacket(1001, false, false);
  EXPECT_EQ(999u, sent_nacks_.size());
  EXPECT_EQ(0, keyframes_requested_);
  nack_module.OnReceivedPacket(1003, false, false);
  EXPECT_EQ(1000u, sent_nacks_.size());
  EXPECT_EQ(0, keyframes_requested_);
  nack_module.OnReceivedPacket(1005, false, false);
  EXPECT_EQ(1000u, sent_nacks_.size());
  EXPECT_EQ(1, keyframes_requested_);
}

TEST_P(TestNackModule2, ClearUpTo) {
  NackModule2& nack_module = CreateNackModule(TimeDelta::Millis(1));
  nack_module.OnReceivedPacket(0, false, false);
  nack_module.OnReceivedPacket(100, false, false);
  EXPECT_EQ(99u, sent_nacks_.size());

  sent_nacks_.clear();
  clock_->AdvanceTimeMilliseconds(100);
  nack_module.ClearUpTo(50);
  WaitForSendNack();
  ASSERT_EQ(50u, sent_nacks_.size());
  EXPECT_EQ(50, sent_nacks_[0]);
}

TEST_P(TestNackModule2, ClearUpToWrap) {
  NackModule2& nack_module = CreateNackModule();
  nack_module.OnReceivedPacket(0xfff0, false, false);
  nack_module.OnReceivedPacket(0xf, false, false);
  EXPECT_EQ(30u, sent_nacks_.size());

  sent_nacks_.clear();
  clock_->AdvanceTimeMilliseconds(100);
  nack_module.ClearUpTo(0);
  WaitForSendNack();
  ASSERT_EQ(15u, sent_nacks_.size());
  EXPECT_EQ(0, sent_nacks_[0]);
}

TEST_P(TestNackModule2, PacketNackCount) {
  NackModule2& nack_module = CreateNackModule(TimeDelta::Millis(1));
  EXPECT_EQ(0, nack_module.OnReceivedPacket(0, false, false));
  EXPECT_EQ(0, nack_module.OnReceivedPacket(2, false, false));
  EXPECT_EQ(1, nack_module.OnReceivedPacket(1, false, false));

  sent_nacks_.clear();
  nack_module.UpdateRtt(100);
  EXPECT_EQ(0, nack_module.OnReceivedPacket(5, false, false));
  clock_->AdvanceTimeMilliseconds(100);
  WaitForSendNack();
  EXPECT_EQ(4u, sent_nacks_.size());

  clock_->AdvanceTimeMilliseconds(125);
  WaitForSendNack();

  EXPECT_EQ(6u, sent_nacks_.size());

  EXPECT_EQ(3, nack_module.OnReceivedPacket(3, false, false));
  EXPECT_EQ(3, nack_module.OnReceivedPacket(4, false, false));
  EXPECT_EQ(0, nack_module.OnReceivedPacket(4, false, false));
}

TEST_P(TestNackModule2, NackListFullAndNoOverlapWithKeyframes) {
  NackModule2& nack_module = CreateNackModule();
  const int kMaxNackPackets = 1000;
  const unsigned int kFirstGap = kMaxNackPackets - 20;
  const unsigned int kSecondGap = 200;
  uint16_t seq_num = 0;
  nack_module.OnReceivedPacket(seq_num++, true, false);
  seq_num += kFirstGap;
  nack_module.OnReceivedPacket(seq_num++, true, false);
  EXPECT_EQ(kFirstGap, sent_nacks_.size());
  sent_nacks_.clear();
  seq_num += kSecondGap;
  nack_module.OnReceivedPacket(seq_num, true, false);
  EXPECT_EQ(kSecondGap, sent_nacks_.size());
}

TEST_P(TestNackModule2, HandleFecRecoveredPacket) {
  NackModule2& nack_module = CreateNackModule();
  nack_module.OnReceivedPacket(1, false, false);
  nack_module.OnReceivedPacket(4, false, true);
  EXPECT_EQ(0u, sent_nacks_.size());
  nack_module.OnReceivedPacket(5, false, false);
  EXPECT_EQ(2u, sent_nacks_.size());
}

TEST_P(TestNackModule2, SendNackWithoutDelay) {
  NackModule2& nack_module = CreateNackModule();
  nack_module.OnReceivedPacket(0, false, false);
  nack_module.OnReceivedPacket(100, false, false);
  EXPECT_EQ(99u, sent_nacks_.size());
}

INSTANTIATE_TEST_SUITE_P(WithAndWithoutBackoff,
                         TestNackModule2,
                         ::testing::Values(true, false));

class TestNackModule2WithFieldTrial : public ::testing::Test,
                                      public NackSender,
                                      public KeyFrameRequestSender {
 protected:
  TestNackModule2WithFieldTrial()
      : nack_delay_field_trial_("WebRTC-SendNackDelayMs/10/"),
        clock_(new SimulatedClock(0)),
        nack_module_(TaskQueueBase::Current(), clock_.get(), this, this),
        keyframes_requested_(0) {}

  void SendNack(const std::vector<uint16_t>& sequence_numbers,
                bool buffering_allowed) override {
    sent_nacks_.insert(sent_nacks_.end(), sequence_numbers.begin(),
                       sequence_numbers.end());
  }

  void RequestKeyFrame() override { ++keyframes_requested_; }

  test::ScopedFieldTrials nack_delay_field_trial_;
  std::unique_ptr<SimulatedClock> clock_;
  NackModule2 nack_module_;
  std::vector<uint16_t> sent_nacks_;
  int keyframes_requested_;
};

TEST_F(TestNackModule2WithFieldTrial, SendNackWithDelay) {
  nack_module_.OnReceivedPacket(0, false, false);
  nack_module_.OnReceivedPacket(100, false, false);
  EXPECT_EQ(0u, sent_nacks_.size());
  clock_->AdvanceTimeMilliseconds(10);
  nack_module_.OnReceivedPacket(106, false, false);
  EXPECT_EQ(99u, sent_nacks_.size());
  clock_->AdvanceTimeMilliseconds(10);
  nack_module_.OnReceivedPacket(109, false, false);
  EXPECT_EQ(104u, sent_nacks_.size());
}
}  // namespace webrtc
