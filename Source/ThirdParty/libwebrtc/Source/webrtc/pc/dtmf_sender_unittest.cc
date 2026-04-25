/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/dtmf_sender.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "api/dtmf_sender_interface.h"
#include "api/scoped_refptr.h"
#include "api/test/rtc_error_matchers.h"
#include "api/units/time_delta.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/thread.h"
#include "rtc_base/time_utils.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/wait_until.h"

using webrtc::DtmfProviderInterface;
using webrtc::DtmfSender;
using webrtc::DtmfSenderObserverInterface;

// TODO(deadbeef): Even though this test now uses a fake clock, it has a
// generous 3-second timeout for every test case. The timeout could be tuned
// to each test based on the tones sent, instead.
static const int kMaxWaitMs = 3000;

class FakeDtmfObserver : public DtmfSenderObserverInterface {
 public:
  FakeDtmfObserver() : completed_(false) {}

  // Implements DtmfSenderObserverInterface.
  void OnToneChange(const std::string& tone) override {
    tones_from_single_argument_callback_.push_back(tone);
    if (tone.empty()) {
      completed_ = true;
    }
  }
  void OnToneChange(const std::string& tone,
                    const std::string& tone_buffer) override {
    tones_.push_back(tone);
    tones_remaining_ = tone_buffer;
    if (tone.empty()) {
      completed_ = true;
    }
  }

  // getters
  const std::vector<std::string>& tones() const { return tones_; }
  const std::vector<std::string>& tones_from_single_argument_callback() const {
    return tones_from_single_argument_callback_;
  }
  std::string tones_remaining() { return tones_remaining_; }
  bool completed() const { return completed_; }

 private:
  std::vector<std::string> tones_;
  std::vector<std::string> tones_from_single_argument_callback_;
  std::string tones_remaining_;
  bool completed_;
};

class FakeDtmfProvider : public DtmfProviderInterface {
 public:
  struct DtmfInfo {
    DtmfInfo(int code, int duration, int gap)
        : code(code), duration(duration), gap(gap) {}
    int code;
    int duration;
    int gap;
  };

  FakeDtmfProvider() : last_insert_dtmf_call_(0) {}

  // Implements DtmfProviderInterface.
  bool CanInsertDtmf() override { return can_insert_; }

  bool InsertDtmf(int code, int duration) override {
    int gap = 0;
    // TODO(ronghuawu): Make the timer (basically the webrtc::TimeNanos)
    // mockable and use a fake timer in the unit tests.
    if (last_insert_dtmf_call_ > 0) {
      gap = static_cast<int>(webrtc::TimeMillis() - last_insert_dtmf_call_);
    }
    last_insert_dtmf_call_ = webrtc::TimeMillis();

    dtmf_info_queue_.push_back(DtmfInfo(code, duration, gap));
    return true;
  }

  // getter and setter
  const std::vector<DtmfInfo>& dtmf_info_queue() const {
    return dtmf_info_queue_;
  }

  // helper functions
  void SetCanInsertDtmf(bool can_insert) { can_insert_ = can_insert; }

 private:
  bool can_insert_ = false;
  std::vector<DtmfInfo> dtmf_info_queue_;
  int64_t last_insert_dtmf_call_;
};

class DtmfSenderTest : public ::testing::Test {
 protected:
  DtmfSenderTest()
      : observer_(new FakeDtmfObserver()), provider_(new FakeDtmfProvider()) {
    provider_->SetCanInsertDtmf(true);
    dtmf_ = DtmfSender::Create(webrtc::Thread::Current(), provider_.get());
    dtmf_->RegisterObserver(observer_.get());
  }

  ~DtmfSenderTest() override {
    if (dtmf_) {
      dtmf_->UnregisterObserver();
    }
  }

  // Constructs a list of DtmfInfo from `tones`, `duration` and
  // `inter_tone_gap`.
  void GetDtmfInfoFromString(
      const std::string& tones,
      int duration,
      int inter_tone_gap,
      std::vector<FakeDtmfProvider::DtmfInfo>* dtmfs,
      int comma_delay = webrtc::DtmfSender::kDtmfDefaultCommaDelayMs) {
    // Init extra_delay as -inter_tone_gap - duration to ensure the first
    // DtmfInfo's gap field will be 0.
    int extra_delay = -1 * (inter_tone_gap + duration);

    std::string::const_iterator it = tones.begin();
    for (; it != tones.end(); ++it) {
      char tone = *it;
      int code = 0;
      webrtc::GetDtmfCode(tone, &code);
      if (tone == ',') {
        extra_delay = comma_delay;
      } else {
        dtmfs->push_back(FakeDtmfProvider::DtmfInfo(
            code, duration, duration + inter_tone_gap + extra_delay));
        extra_delay = 0;
      }
    }
  }

  void VerifyExpectedState(const std::string& tones,
                           int duration,
                           int inter_tone_gap) {
    EXPECT_EQ(tones, dtmf_->tones());
    EXPECT_EQ(duration, dtmf_->duration());
    EXPECT_EQ(inter_tone_gap, dtmf_->inter_tone_gap());
  }

  // Verify the provider got all the expected calls.
  void VerifyOnProvider(
      const std::string& tones,
      int duration,
      int inter_tone_gap,
      int comma_delay = webrtc::DtmfSender::kDtmfDefaultCommaDelayMs) {
    std::vector<FakeDtmfProvider::DtmfInfo> dtmf_queue_ref;
    GetDtmfInfoFromString(tones, duration, inter_tone_gap, &dtmf_queue_ref,
                          comma_delay);
    VerifyOnProvider(dtmf_queue_ref);
  }

  void VerifyOnProvider(
      const std::vector<FakeDtmfProvider::DtmfInfo>& dtmf_queue_ref) {
    const std::vector<FakeDtmfProvider::DtmfInfo>& dtmf_queue =
        provider_->dtmf_info_queue();
    ASSERT_EQ(dtmf_queue_ref.size(), dtmf_queue.size());
    std::vector<FakeDtmfProvider::DtmfInfo>::const_iterator it_ref =
        dtmf_queue_ref.begin();
    std::vector<FakeDtmfProvider::DtmfInfo>::const_iterator it =
        dtmf_queue.begin();
    while (it_ref != dtmf_queue_ref.end() && it != dtmf_queue.end()) {
      EXPECT_EQ(it_ref->code, it->code);
      EXPECT_EQ(it_ref->duration, it->duration);
      // Allow ~10ms error (can be small since we're using a fake clock).
      EXPECT_GE(it_ref->gap, it->gap - 10);
      EXPECT_LE(it_ref->gap, it->gap + 10);
      ++it_ref;
      ++it;
    }
  }

  // Verify the observer got all the expected callbacks.
  void VerifyOnObserver(const std::string& tones_ref) {
    const std::vector<std::string>& tones = observer_->tones();
    // The observer will get an empty string at the end.
    EXPECT_EQ(tones_ref.size() + 1, tones.size());
    EXPECT_TRUE(tones.back().empty());
    EXPECT_TRUE(observer_->tones_remaining().empty());
    std::string::const_iterator it_ref = tones_ref.begin();
    std::vector<std::string>::const_iterator it = tones.begin();
    while (it_ref != tones_ref.end() && it != tones.end()) {
      EXPECT_EQ(*it_ref, it->at(0));
      ++it_ref;
      ++it;
    }
  }

  webrtc::AutoThread main_thread_;
  std::unique_ptr<FakeDtmfObserver> observer_;
  std::unique_ptr<FakeDtmfProvider> provider_;
  webrtc::scoped_refptr<DtmfSender> dtmf_;
  webrtc::ScopedFakeClock fake_clock_;
};

TEST_F(DtmfSenderTest, CanInsertDtmf) {
  EXPECT_TRUE(dtmf_->CanInsertDtmf());
  provider_->SetCanInsertDtmf(false);
  EXPECT_FALSE(dtmf_->CanInsertDtmf());
}

TEST_F(DtmfSenderTest, InsertDtmf) {
  std::string tones = "@1%a&*$";
  int duration = 100;
  int inter_tone_gap = 50;
  EXPECT_TRUE(dtmf_->InsertDtmf(tones, duration, inter_tone_gap));
  EXPECT_THAT(webrtc::WaitUntil(
                  [&] { return observer_->completed(); }, ::testing::IsTrue(),
                  {.timeout = webrtc::TimeDelta::Millis(kMaxWaitMs),
                   .clock = &fake_clock_}),
              webrtc::IsRtcOk());

  // The unrecognized characters should be ignored.
  std::string known_tones = "1a*";
  VerifyOnProvider(known_tones, duration, inter_tone_gap);
  VerifyOnObserver(known_tones);
}

TEST_F(DtmfSenderTest, InsertDtmfTwice) {
  std::string tones1 = "12";
  std::string tones2 = "ab";
  int duration = 100;
  int inter_tone_gap = 50;
  EXPECT_TRUE(dtmf_->InsertDtmf(tones1, duration, inter_tone_gap));
  VerifyExpectedState(tones1, duration, inter_tone_gap);
  // Wait until the first tone got sent.
  EXPECT_THAT(webrtc::WaitUntil(
                  [&] { return observer_->tones().size(); }, ::testing::Eq(1),
                  {.timeout = webrtc::TimeDelta::Millis(kMaxWaitMs),
                   .clock = &fake_clock_}),
              webrtc::IsRtcOk());
  VerifyExpectedState("2", duration, inter_tone_gap);
  // Insert with another tone buffer.
  EXPECT_TRUE(dtmf_->InsertDtmf(tones2, duration, inter_tone_gap));
  VerifyExpectedState(tones2, duration, inter_tone_gap);
  // Wait until it's completed.
  EXPECT_THAT(webrtc::WaitUntil(
                  [&] { return observer_->completed(); }, ::testing::IsTrue(),
                  {.timeout = webrtc::TimeDelta::Millis(kMaxWaitMs),
                   .clock = &fake_clock_}),
              webrtc::IsRtcOk());

  std::vector<FakeDtmfProvider::DtmfInfo> dtmf_queue_ref;
  GetDtmfInfoFromString("1", duration, inter_tone_gap, &dtmf_queue_ref);
  GetDtmfInfoFromString("ab", duration, inter_tone_gap, &dtmf_queue_ref);
  VerifyOnProvider(dtmf_queue_ref);
  VerifyOnObserver("1ab");
}

TEST_F(DtmfSenderTest, InsertDtmfWhileProviderIsDeleted) {
  std::string tones = "@1%a&*$";
  int duration = 100;
  int inter_tone_gap = 50;
  EXPECT_TRUE(dtmf_->InsertDtmf(tones, duration, inter_tone_gap));
  // Wait until the first tone got sent.
  EXPECT_THAT(webrtc::WaitUntil(
                  [&] { return observer_->tones().size(); }, ::testing::Eq(1),
                  {.timeout = webrtc::TimeDelta::Millis(kMaxWaitMs),
                   .clock = &fake_clock_}),
              webrtc::IsRtcOk());
  // Delete provider.
  dtmf_->OnDtmfProviderDestroyed();
  provider_.reset();
  // The queue should be discontinued so no more tone callbacks.
  fake_clock_.AdvanceTime(webrtc::TimeDelta::Millis(200));
  EXPECT_EQ(1U, observer_->tones().size());
}

TEST_F(DtmfSenderTest, InsertDtmfWhileSenderIsDeleted) {
  std::string tones = "@1%a&*$";
  int duration = 100;
  int inter_tone_gap = 50;
  EXPECT_TRUE(dtmf_->InsertDtmf(tones, duration, inter_tone_gap));
  // Wait until the first tone got sent.
  EXPECT_THAT(webrtc::WaitUntil(
                  [&] { return observer_->tones().size(); }, ::testing::Eq(1),
                  {.timeout = webrtc::TimeDelta::Millis(kMaxWaitMs),
                   .clock = &fake_clock_}),
              webrtc::IsRtcOk());
  // Delete the sender.
  dtmf_ = nullptr;
  // The queue should be discontinued so no more tone callbacks.
  fake_clock_.AdvanceTime(webrtc::TimeDelta::Millis(200));
  EXPECT_EQ(1U, observer_->tones().size());
}

TEST_F(DtmfSenderTest, InsertEmptyTonesToCancelPreviousTask) {
  std::string tones1 = "12";
  std::string tones2 = "";
  int duration = 100;
  int inter_tone_gap = 50;
  EXPECT_TRUE(dtmf_->InsertDtmf(tones1, duration, inter_tone_gap));
  // Wait until the first tone got sent.
  EXPECT_THAT(webrtc::WaitUntil(
                  [&] { return observer_->tones().size(); }, ::testing::Eq(1),
                  {.timeout = webrtc::TimeDelta::Millis(kMaxWaitMs),
                   .clock = &fake_clock_}),
              webrtc::IsRtcOk());
  // Insert with another tone buffer.
  EXPECT_TRUE(dtmf_->InsertDtmf(tones2, duration, inter_tone_gap));
  // Wait until it's completed.
  EXPECT_THAT(webrtc::WaitUntil(
                  [&] { return observer_->completed(); }, ::testing::IsTrue(),
                  {.timeout = webrtc::TimeDelta::Millis(kMaxWaitMs),
                   .clock = &fake_clock_}),
              webrtc::IsRtcOk());

  std::vector<FakeDtmfProvider::DtmfInfo> dtmf_queue_ref;
  GetDtmfInfoFromString("1", duration, inter_tone_gap, &dtmf_queue_ref);
  VerifyOnProvider(dtmf_queue_ref);
  VerifyOnObserver("1");
}

TEST_F(DtmfSenderTest, InsertDtmfWithDefaultCommaDelay) {
  std::string tones = "3,4";
  int duration = 100;
  int inter_tone_gap = 50;
  int default_comma_delay = webrtc::DtmfSender::kDtmfDefaultCommaDelayMs;
  EXPECT_EQ(dtmf_->comma_delay(), default_comma_delay);
  EXPECT_TRUE(dtmf_->InsertDtmf(tones, duration, inter_tone_gap));
  EXPECT_THAT(webrtc::WaitUntil(
                  [&] { return observer_->completed(); }, ::testing::IsTrue(),
                  {.timeout = webrtc::TimeDelta::Millis(kMaxWaitMs),
                   .clock = &fake_clock_}),
              webrtc::IsRtcOk());

  VerifyOnProvider(tones, duration, inter_tone_gap);
  VerifyOnObserver(tones);
  EXPECT_EQ(dtmf_->comma_delay(), default_comma_delay);
}

TEST_F(DtmfSenderTest, InsertDtmfWithNonDefaultCommaDelay) {
  std::string tones = "3,4";
  int duration = 100;
  int inter_tone_gap = 50;
  int default_comma_delay = webrtc::DtmfSender::kDtmfDefaultCommaDelayMs;
  int comma_delay = 500;
  EXPECT_EQ(dtmf_->comma_delay(), default_comma_delay);
  EXPECT_TRUE(dtmf_->InsertDtmf(tones, duration, inter_tone_gap, comma_delay));
  EXPECT_THAT(webrtc::WaitUntil(
                  [&] { return observer_->completed(); }, ::testing::IsTrue(),
                  {.timeout = webrtc::TimeDelta::Millis(kMaxWaitMs),
                   .clock = &fake_clock_}),
              webrtc::IsRtcOk());

  VerifyOnProvider(tones, duration, inter_tone_gap, comma_delay);
  VerifyOnObserver(tones);
  EXPECT_EQ(dtmf_->comma_delay(), comma_delay);
}

TEST_F(DtmfSenderTest, TryInsertDtmfWhenItDoesNotWork) {
  std::string tones = "3,4";
  int duration = 100;
  int inter_tone_gap = 50;
  provider_->SetCanInsertDtmf(false);
  EXPECT_FALSE(dtmf_->InsertDtmf(tones, duration, inter_tone_gap));
}

TEST_F(DtmfSenderTest, InsertDtmfWithInvalidDurationOrGap) {
  std::string tones = "3,4";
  int duration = 40;
  int inter_tone_gap = 50;

  EXPECT_FALSE(dtmf_->InsertDtmf(tones, 6001, inter_tone_gap));
  EXPECT_FALSE(dtmf_->InsertDtmf(tones, 39, inter_tone_gap));
  EXPECT_FALSE(dtmf_->InsertDtmf(tones, duration, 29));
  EXPECT_FALSE(dtmf_->InsertDtmf(tones, duration, inter_tone_gap, 29));

  EXPECT_TRUE(dtmf_->InsertDtmf(tones, duration, inter_tone_gap));
}

TEST_F(DtmfSenderTest, InsertDtmfSendsAfterWait) {
  std::string tones = "ABC";
  int duration = 100;
  int inter_tone_gap = 50;
  EXPECT_TRUE(dtmf_->InsertDtmf(tones, duration, inter_tone_gap));
  VerifyExpectedState("ABC", duration, inter_tone_gap);
  // Wait until the first tone got sent.
  EXPECT_THAT(webrtc::WaitUntil(
                  [&] { return observer_->tones().size(); }, ::testing::Eq(1),
                  {.timeout = webrtc::TimeDelta::Millis(kMaxWaitMs),
                   .clock = &fake_clock_}),
              webrtc::IsRtcOk());
  VerifyExpectedState("BC", duration, inter_tone_gap);
}
