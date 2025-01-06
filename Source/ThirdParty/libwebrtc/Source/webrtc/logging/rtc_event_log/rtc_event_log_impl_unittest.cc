/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/rtc_event_log_impl.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "api/rtc_event_log/rtc_event.h"
#include "api/rtc_event_log_output.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/encoder/rtc_event_log_encoder.h"
#include "rtc_base/checks.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {
namespace {

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Property;
using ::testing::Ref;
using ::testing::Return;

class MockEventEncoder : public RtcEventLogEncoder {
 public:
  MOCK_METHOD(std::string,
              EncodeLogStart,
              (int64_t /*timestamp_us*/, int64_t /*utc_time_us*/),
              (override));
  MOCK_METHOD(std::string,
              EncodeLogEnd,
              (int64_t /*timestamp_us*/),
              (override));
  MOCK_METHOD(std::string, OnEncode, (const RtcEvent&), ());
  std::string EncodeBatch(
      std::deque<std::unique_ptr<RtcEvent>>::const_iterator a,
      std::deque<std::unique_ptr<RtcEvent>>::const_iterator b) override {
    std::string result;
    while (a != b) {
      result += OnEncode(**a);
      ++a;
    }
    return result;
  }
};

class FakeOutput : public RtcEventLogOutput {
 public:
  explicit FakeOutput(std::string& written_data)
      : written_data_(written_data) {}
  bool IsActive() const { return is_active_; }
  bool Write(absl::string_view data) override {
    RTC_DCHECK(is_active_);
    if (fails_write_) {
      is_active_ = false;
      fails_write_ = false;
      return false;
    } else {
      written_data_.append(std::string(data));
      return true;
    }
  }
  void Flush() override {}

  void FailsNextWrite() { fails_write_ = true; }

 private:
  std::string& written_data_;
  bool is_active_ = true;
  bool fails_write_ = false;
};

class FakeEvent : public RtcEvent {
 public:
  Type GetType() const override { return RtcEvent::Type::FakeEvent; }
  bool IsConfigEvent() const override { return false; }
};

class FakeConfigEvent : public RtcEvent {
 public:
  Type GetType() const override { return RtcEvent::Type::FakeEvent; }
  bool IsConfigEvent() const override { return true; }
};

class RtcEventLogImplTest : public ::testing::Test {
 public:
  static constexpr size_t kMaxEventsInHistory = 2;
  static constexpr size_t kMaxEventsInConfigHistory = 5;
  static constexpr TimeDelta kOutputPeriod = TimeDelta::Seconds(2);
  static constexpr Timestamp kStartTime = Timestamp::Seconds(1);

  GlobalSimulatedTimeController time_controller_{kStartTime};
  std::string written_data_;  // This must be destroyed after the event_log_.
  std::unique_ptr<MockEventEncoder> encoder_ =
      std::make_unique<MockEventEncoder>();
  MockEventEncoder* encoder_ptr_ = encoder_.get();
  std::unique_ptr<FakeOutput> output_ =
      std::make_unique<FakeOutput>(written_data_);
  FakeOutput* output_ptr_ = output_.get();
  RtcEventLogImpl event_log_{std::move(encoder_),
                             time_controller_.GetTaskQueueFactory(),
                             kMaxEventsInHistory, kMaxEventsInConfigHistory};
};

TEST_F(RtcEventLogImplTest, WritesHeaderAndEventsAndTrailer) {
  EXPECT_CALL(*encoder_ptr_, EncodeLogStart(kStartTime.us(), kStartTime.us()))
      .WillOnce(Return("start"));
  EXPECT_CALL(*encoder_ptr_, EncodeLogEnd(kStartTime.us()))
      .WillOnce(Return("stop"));
  EXPECT_CALL(*encoder_ptr_, OnEncode(Property(&RtcEvent::IsConfigEvent, true)))
      .WillOnce(Return("config"));
  EXPECT_CALL(*encoder_ptr_,
              OnEncode(Property(&RtcEvent::IsConfigEvent, false)))
      .WillOnce(Return("event"));
  event_log_.StartLogging(std::move(output_), kOutputPeriod.ms());
  event_log_.Log(std::make_unique<FakeEvent>());
  event_log_.Log(std::make_unique<FakeConfigEvent>());
  event_log_.StopLogging();
  Mock::VerifyAndClearExpectations(encoder_ptr_);
  EXPECT_TRUE(written_data_ == "startconfigeventstop" ||
              written_data_ == "starteventconfigstop");
}

TEST_F(RtcEventLogImplTest, KeepsMostRecentEventsOnStart) {
  auto e2 = std::make_unique<FakeEvent>();
  RtcEvent* e2_ptr = e2.get();
  auto e3 = std::make_unique<FakeEvent>();
  RtcEvent* e3_ptr = e3.get();
  event_log_.Log(std::make_unique<FakeEvent>());
  event_log_.Log(std::move(e2));
  event_log_.Log(std::move(e3));
  InSequence s;
  EXPECT_CALL(*encoder_ptr_, OnEncode(Ref(*e2_ptr)));
  EXPECT_CALL(*encoder_ptr_, OnEncode(Ref(*e3_ptr)));
  event_log_.StartLogging(std::move(output_), kOutputPeriod.ms());
  time_controller_.AdvanceTime(TimeDelta::Zero());
  Mock::VerifyAndClearExpectations(encoder_ptr_);
}

TEST_F(RtcEventLogImplTest, KeepsConfigEventsOnStart) {
  // The config-history is supposed to be unbounded and never overflow.
  auto c1 = std::make_unique<FakeConfigEvent>();
  RtcEvent* c1_ptr = c1.get();
  auto c2 = std::make_unique<FakeConfigEvent>();
  RtcEvent* c2_ptr = c2.get();
  auto c3 = std::make_unique<FakeConfigEvent>();
  RtcEvent* c3_ptr = c3.get();
  event_log_.Log(std::move(c1));
  event_log_.Log(std::move(c2));
  event_log_.Log(std::move(c3));
  InSequence s;
  EXPECT_CALL(*encoder_ptr_, OnEncode(Ref(*c1_ptr)));
  EXPECT_CALL(*encoder_ptr_, OnEncode(Ref(*c2_ptr)));
  EXPECT_CALL(*encoder_ptr_, OnEncode(Ref(*c3_ptr)));
  event_log_.StartLogging(std::move(output_), kOutputPeriod.ms());
  time_controller_.AdvanceTime(TimeDelta::Zero());
  Mock::VerifyAndClearExpectations(encoder_ptr_);
}

TEST_F(RtcEventLogImplTest, EncodesEventsOnHistoryFullPostponesLastEncode) {
  auto e1 = std::make_unique<FakeEvent>();
  RtcEvent* e1_ptr = e1.get();
  auto e2 = std::make_unique<FakeEvent>();
  RtcEvent* e2_ptr = e2.get();
  auto e3 = std::make_unique<FakeEvent>();
  RtcEvent* e3_ptr = e3.get();
  event_log_.StartLogging(std::move(output_), kOutputPeriod.ms());
  time_controller_.AdvanceTime(TimeDelta::Zero());
  event_log_.Log(std::move(e1));
  event_log_.Log(std::move(e2));
  // Overflows and causes immediate output, and eventual output after the output
  // period has passed.
  event_log_.Log(std::move(e3));
  InSequence s;
  EXPECT_CALL(*encoder_ptr_, OnEncode(Ref(*e1_ptr)));
  EXPECT_CALL(*encoder_ptr_, OnEncode(Ref(*e2_ptr)));
  time_controller_.AdvanceTime(TimeDelta::Zero());
  EXPECT_CALL(*encoder_ptr_, OnEncode(Ref(*e3_ptr)));
  time_controller_.AdvanceTime(kOutputPeriod);
  Mock::VerifyAndClearExpectations(encoder_ptr_);
}

TEST_F(RtcEventLogImplTest, RewritesAllConfigEventsOnlyOnRestart) {
  auto c1 = std::make_unique<FakeConfigEvent>();
  RtcEvent* c1_ptr = c1.get();
  auto c2 = std::make_unique<FakeConfigEvent>();
  RtcEvent* c2_ptr = c2.get();
  auto c3 = std::make_unique<FakeConfigEvent>();
  RtcEvent* c3_ptr = c3.get();
  auto c4 = std::make_unique<FakeConfigEvent>();
  RtcEvent* c4_ptr = c4.get();
  auto c5 = std::make_unique<FakeConfigEvent>();
  RtcEvent* c5_ptr = c5.get();
  // Keep the config event before start.
  event_log_.Log(std::move(c1));
  // Start logging.
  event_log_.StartLogging(std::make_unique<FakeOutput>(written_data_),
                          kOutputPeriod.ms());
  event_log_.Log(std::move(c2));
  event_log_.Log(std::move(c3));
  event_log_.StopLogging();
  time_controller_.AdvanceTime(TimeDelta::Zero());
  // Restart logging.
  event_log_.StartLogging(std::make_unique<FakeOutput>(written_data_),
                          kOutputPeriod.ms());
  event_log_.Log(std::move(c4));
  event_log_.Log(std::move(c5));
  event_log_.Log(std::make_unique<FakeEvent>());
  event_log_.StopLogging();
  time_controller_.AdvanceTime(TimeDelta::Zero());
  InSequence s;
  // Rewrite all config events in sequence on next restart.
  EXPECT_CALL(*encoder_ptr_, OnEncode(Ref(*c1_ptr)));
  EXPECT_CALL(*encoder_ptr_, OnEncode(Ref(*c2_ptr)));
  EXPECT_CALL(*encoder_ptr_, OnEncode(Ref(*c3_ptr)));
  EXPECT_CALL(*encoder_ptr_, OnEncode(Ref(*c4_ptr)));
  EXPECT_CALL(*encoder_ptr_, OnEncode(Ref(*c5_ptr)));
  EXPECT_CALL(*encoder_ptr_,
              OnEncode(Property(&RtcEvent::IsConfigEvent, false)))
      .Times(0);
  event_log_.StartLogging(std::move(output_), kOutputPeriod.ms());
  time_controller_.AdvanceTime(TimeDelta::Zero());
  Mock::VerifyAndClearExpectations(encoder_ptr_);
}

TEST_F(RtcEventLogImplTest, SchedulesWriteAfterOutputDurationPassed) {
  event_log_.StartLogging(std::move(output_), kOutputPeriod.ms());
  event_log_.Log(std::make_unique<FakeConfigEvent>());
  event_log_.Log(std::make_unique<FakeEvent>());
  EXPECT_CALL(*encoder_ptr_,
              OnEncode(Property(&RtcEvent::IsConfigEvent, true)));
  EXPECT_CALL(*encoder_ptr_,
              OnEncode(Property(&RtcEvent::IsConfigEvent, false)));
  time_controller_.AdvanceTime(kOutputPeriod);
  Mock::VerifyAndClearExpectations(encoder_ptr_);
}

TEST_F(RtcEventLogImplTest, DoNotDropEventsIfHistoryFullAfterStarted) {
  constexpr size_t kNumberOfEvents = 10 * kMaxEventsInHistory;

  event_log_.StartLogging(std::move(output_), kOutputPeriod.ms());
  event_log_.Log(std::make_unique<FakeConfigEvent>());
  for (size_t i = 0; i < kNumberOfEvents; i++) {
    event_log_.Log(std::make_unique<FakeEvent>());
  }
  EXPECT_CALL(*encoder_ptr_,
              OnEncode(Property(&RtcEvent::IsConfigEvent, true)));
  EXPECT_CALL(*encoder_ptr_,
              OnEncode(Property(&RtcEvent::IsConfigEvent, false)))
      .Times(kNumberOfEvents);
  time_controller_.AdvanceTime(kOutputPeriod);
  Mock::VerifyAndClearExpectations(encoder_ptr_);
}

TEST_F(RtcEventLogImplTest, StopOutputOnWriteFailure) {
  constexpr size_t kNumberOfEvents = 10;
  constexpr size_t kFailsWriteOnEventsCount = 5;

  size_t number_of_encoded_events = 0;
  EXPECT_CALL(*encoder_ptr_, OnEncode(_))
      .WillRepeatedly(Invoke([this, &number_of_encoded_events]() {
        ++number_of_encoded_events;
        if (number_of_encoded_events == kFailsWriteOnEventsCount) {
          output_ptr_->FailsNextWrite();
        }
        return std::string();
      }));

  event_log_.StartLogging(std::move(output_), kOutputPeriod.ms());

  // Fails `RtcEventLogOutput` on the last event.
  for (size_t i = 0; i < kFailsWriteOnEventsCount; i++) {
    event_log_.Log(std::make_unique<FakeEvent>());
  }
  time_controller_.AdvanceTime(kOutputPeriod);
  // Expect that the remainder events are not encoded.
  for (size_t i = kFailsWriteOnEventsCount; i < kNumberOfEvents; i++) {
    event_log_.Log(std::make_unique<FakeEvent>());
  }
  time_controller_.AdvanceTime(kOutputPeriod);
  event_log_.StopLogging();

  EXPECT_EQ(number_of_encoded_events, kFailsWriteOnEventsCount);

  Mock::VerifyAndClearExpectations(encoder_ptr_);
}

}  // namespace
}  // namespace webrtc
