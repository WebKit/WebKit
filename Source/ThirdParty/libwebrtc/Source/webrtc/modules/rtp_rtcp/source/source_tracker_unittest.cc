/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/source_tracker.h"

#include <algorithm>
#include <list>
#include <random>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/rtp_headers.h"
#include "api/rtp_packet_info.h"
#include "api/rtp_packet_infos.h"
#include "system_wrappers/include/ntp_time.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {
namespace {

using ::testing::Combine;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::TestWithParam;
using ::testing::Values;

constexpr size_t kPacketInfosCountMax = 5;

// Simple "guaranteed to be correct" re-implementation of `SourceTracker` for
// dual-implementation testing purposes.
class ExpectedSourceTracker {
 public:
  explicit ExpectedSourceTracker(Clock* clock) : clock_(clock) {}

  void OnFrameDelivered(const RtpPacketInfos& packet_infos) {
    const Timestamp now = clock_->CurrentTime();

    for (const auto& packet_info : packet_infos) {
      RtpSource::Extensions extensions = {
          packet_info.audio_level(), packet_info.absolute_capture_time(),
          packet_info.local_capture_clock_offset()};

      for (const auto& csrc : packet_info.csrcs()) {
        entries_.emplace_front(now, csrc, RtpSourceType::CSRC,
                               packet_info.rtp_timestamp(), extensions);
      }

      entries_.emplace_front(now, packet_info.ssrc(), RtpSourceType::SSRC,
                             packet_info.rtp_timestamp(), extensions);
    }

    PruneEntries(now);
  }

  std::vector<RtpSource> GetSources() const {
    PruneEntries(clock_->CurrentTime());

    return std::vector<RtpSource>(entries_.begin(), entries_.end());
  }

 private:
  void PruneEntries(Timestamp now) const {
    const Timestamp prune = now - TimeDelta::Seconds(10);

    std::set<std::pair<RtpSourceType, uint32_t>> seen;

    auto it = entries_.begin();
    auto end = entries_.end();
    while (it != end) {
      auto next = it;
      ++next;

      auto key = std::make_pair(it->source_type(), it->source_id());
      if (!seen.insert(key).second || it->timestamp() < prune) {
        entries_.erase(it);
      }

      it = next;
    }
  }

  Clock* const clock_;

  mutable std::list<RtpSource> entries_;
};

class SourceTrackerRandomTest
    : public TestWithParam<std::tuple<uint32_t, uint32_t>> {
 protected:
  SourceTrackerRandomTest()
      : ssrcs_count_(std::get<0>(GetParam())),
        csrcs_count_(std::get<1>(GetParam())),
        generator_(42) {}

  RtpPacketInfos GeneratePacketInfos() {
    size_t count = std::uniform_int_distribution<size_t>(
        1, kPacketInfosCountMax)(generator_);

    RtpPacketInfos::vector_type packet_infos;
    for (size_t i = 0; i < count; ++i) {
      packet_infos
          .emplace_back(GenerateSsrc(), GenerateCsrcs(), GenerateRtpTimestamp(),
                        GenerateReceiveTime())
          .set_audio_level(GenerateAudioLevel())
          .set_absolute_capture_time(GenerateAbsoluteCaptureTime())
          .set_local_capture_clock_offset(GenerateLocalCaptureClockOffset());
    }

    return RtpPacketInfos(std::move(packet_infos));
  }

  TimeDelta GenerateClockAdvanceTime() {
    double roll = std::uniform_real_distribution<double>(0.0, 1.0)(generator_);

    if (roll < 0.05) {
      return TimeDelta::Zero();
    }

    if (roll < 0.08) {
      return SourceTracker::kTimeout - TimeDelta::Millis(1);
    }

    if (roll < 0.11) {
      return SourceTracker::kTimeout;
    }

    if (roll < 0.19) {
      return TimeDelta::Millis(std::uniform_int_distribution<int64_t>(
          SourceTracker::kTimeout.ms(),
          SourceTracker::kTimeout.ms() * 1000)(generator_));
    }

    return TimeDelta::Millis(std::uniform_int_distribution<int64_t>(
        1, SourceTracker::kTimeout.ms() - 1)(generator_));
  }

 private:
  uint32_t GenerateSsrc() {
    return std::uniform_int_distribution<uint32_t>(1, ssrcs_count_)(generator_);
  }

  std::vector<uint32_t> GenerateCsrcs() {
    std::vector<uint32_t> csrcs;
    for (size_t i = 1; i <= csrcs_count_ && csrcs.size() < kRtpCsrcSize; ++i) {
      if (std::bernoulli_distribution(0.5)(generator_)) {
        csrcs.push_back(i);
      }
    }

    return csrcs;
  }

  uint32_t GenerateRtpTimestamp() {
    return std::uniform_int_distribution<uint32_t>()(generator_);
  }

  absl::optional<uint8_t> GenerateAudioLevel() {
    if (std::bernoulli_distribution(0.25)(generator_)) {
      return absl::nullopt;
    }

    // Workaround for std::uniform_int_distribution<uint8_t> not being allowed.
    return static_cast<uint8_t>(
        std::uniform_int_distribution<uint16_t>()(generator_));
  }

  absl::optional<AbsoluteCaptureTime> GenerateAbsoluteCaptureTime() {
    if (std::bernoulli_distribution(0.25)(generator_)) {
      return absl::nullopt;
    }

    AbsoluteCaptureTime value;

    value.absolute_capture_timestamp =
        std::uniform_int_distribution<uint64_t>()(generator_);

    if (std::bernoulli_distribution(0.5)(generator_)) {
      value.estimated_capture_clock_offset = absl::nullopt;
    } else {
      value.estimated_capture_clock_offset =
          std::uniform_int_distribution<int64_t>()(generator_);
    }

    return value;
  }

  absl::optional<TimeDelta> GenerateLocalCaptureClockOffset() {
    if (std::bernoulli_distribution(0.5)(generator_)) {
      return absl::nullopt;
    }
    return TimeDelta::Millis(
        UQ32x32ToInt64Ms(std::uniform_int_distribution<int64_t>()(generator_)));
  }

  Timestamp GenerateReceiveTime() {
    return Timestamp::Micros(
        std::uniform_int_distribution<int64_t>()(generator_));
  }

 protected:
  GlobalSimulatedTimeController time_controller_{Timestamp::Seconds(1000)};

 private:
  const uint32_t ssrcs_count_;
  const uint32_t csrcs_count_;

  std::mt19937 generator_;
};

}  // namespace

TEST_P(SourceTrackerRandomTest, RandomOperations) {
  constexpr size_t kIterationsCount = 200;

  SourceTracker actual_tracker(time_controller_.GetClock());
  ExpectedSourceTracker expected_tracker(time_controller_.GetClock());

  ASSERT_THAT(actual_tracker.GetSources(), IsEmpty());
  ASSERT_THAT(expected_tracker.GetSources(), IsEmpty());

  for (size_t i = 0; i < kIterationsCount; ++i) {
    RtpPacketInfos packet_infos = GeneratePacketInfos();

    actual_tracker.OnFrameDelivered(packet_infos);
    expected_tracker.OnFrameDelivered(packet_infos);

    time_controller_.AdvanceTime(GenerateClockAdvanceTime());
    ASSERT_THAT(actual_tracker.GetSources(),
                ElementsAreArray(expected_tracker.GetSources()));
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         SourceTrackerRandomTest,
                         Combine(/*ssrcs_count_=*/Values(1, 2, 4),
                                 /*csrcs_count_=*/Values(0, 1, 3, 7)));

TEST(SourceTrackerTest, StartEmpty) {
  GlobalSimulatedTimeController time_controller(Timestamp::Seconds(1000));
  SourceTracker tracker(time_controller.GetClock());

  EXPECT_THAT(tracker.GetSources(), IsEmpty());
}

TEST(SourceTrackerTest, OnFrameDeliveredRecordsSourcesDistinctSsrcs) {
  constexpr uint32_t kSsrc1 = 10;
  constexpr uint32_t kSsrc2 = 11;
  constexpr uint32_t kCsrcs0 = 20;
  constexpr uint32_t kCsrcs1 = 21;
  constexpr uint32_t kCsrcs2 = 22;
  constexpr uint32_t kRtpTimestamp0 = 40;
  constexpr uint32_t kRtpTimestamp1 = 50;
  constexpr absl::optional<uint8_t> kAudioLevel0 = 50;
  constexpr absl::optional<uint8_t> kAudioLevel1 = 20;
  constexpr absl::optional<AbsoluteCaptureTime> kAbsoluteCaptureTime =
      AbsoluteCaptureTime{/*absolute_capture_timestamp=*/12,
                          /*estimated_capture_clock_offset=*/absl::nullopt};
  constexpr absl::optional<TimeDelta> kLocalCaptureClockOffset = absl::nullopt;
  constexpr Timestamp kReceiveTime0 = Timestamp::Millis(60);
  constexpr Timestamp kReceiveTime1 = Timestamp::Millis(70);

  GlobalSimulatedTimeController time_controller(Timestamp::Seconds(1000));
  SourceTracker tracker(time_controller.GetClock());

  tracker.OnFrameDelivered(RtpPacketInfos(
      {RtpPacketInfo(kSsrc1, {kCsrcs0, kCsrcs1}, kRtpTimestamp0, kReceiveTime0)
           .set_audio_level(kAudioLevel0)
           .set_absolute_capture_time(kAbsoluteCaptureTime)
           .set_local_capture_clock_offset(kLocalCaptureClockOffset),
       RtpPacketInfo(kSsrc2, {kCsrcs2}, kRtpTimestamp1, kReceiveTime1)
           .set_audio_level(kAudioLevel1)
           .set_absolute_capture_time(kAbsoluteCaptureTime)
           .set_local_capture_clock_offset(kLocalCaptureClockOffset)}));

  Timestamp timestamp = time_controller.GetClock()->CurrentTime();
  constexpr RtpSource::Extensions extensions0 = {
      .audio_level = kAudioLevel0,
      .absolute_capture_time = kAbsoluteCaptureTime,
      .local_capture_clock_offset = kLocalCaptureClockOffset};
  constexpr RtpSource::Extensions extensions1 = {
      .audio_level = kAudioLevel1,
      .absolute_capture_time = kAbsoluteCaptureTime,
      .local_capture_clock_offset = kLocalCaptureClockOffset};

  time_controller.AdvanceTime(TimeDelta::Zero());

  EXPECT_THAT(tracker.GetSources(),
              ElementsAre(RtpSource(timestamp, kSsrc2, RtpSourceType::SSRC,
                                    kRtpTimestamp1, extensions1),
                          RtpSource(timestamp, kCsrcs2, RtpSourceType::CSRC,
                                    kRtpTimestamp1, extensions1),
                          RtpSource(timestamp, kSsrc1, RtpSourceType::SSRC,
                                    kRtpTimestamp0, extensions0),
                          RtpSource(timestamp, kCsrcs1, RtpSourceType::CSRC,
                                    kRtpTimestamp0, extensions0),
                          RtpSource(timestamp, kCsrcs0, RtpSourceType::CSRC,
                                    kRtpTimestamp0, extensions0)));
}

TEST(SourceTrackerTest, OnFrameDeliveredRecordsSourcesSameSsrc) {
  constexpr uint32_t kSsrc = 10;
  constexpr uint32_t kCsrcs0 = 20;
  constexpr uint32_t kCsrcs1 = 21;
  constexpr uint32_t kCsrcs2 = 22;
  constexpr uint32_t kRtpTimestamp0 = 40;
  constexpr uint32_t kRtpTimestamp1 = 45;
  constexpr uint32_t kRtpTimestamp2 = 50;
  constexpr absl::optional<uint8_t> kAudioLevel0 = 50;
  constexpr absl::optional<uint8_t> kAudioLevel1 = 20;
  constexpr absl::optional<uint8_t> kAudioLevel2 = 10;
  constexpr absl::optional<AbsoluteCaptureTime> kAbsoluteCaptureTime =
      AbsoluteCaptureTime{/*absolute_capture_timestamp=*/12,
                          /*estimated_capture_clock_offset=*/absl::nullopt};
  constexpr absl::optional<TimeDelta> kLocalCaptureClockOffset = absl::nullopt;
  constexpr Timestamp kReceiveTime0 = Timestamp::Millis(60);
  constexpr Timestamp kReceiveTime1 = Timestamp::Millis(70);
  constexpr Timestamp kReceiveTime2 = Timestamp::Millis(80);

  GlobalSimulatedTimeController time_controller(Timestamp::Seconds(1000));
  SourceTracker tracker(time_controller.GetClock());

  tracker.OnFrameDelivered(RtpPacketInfos({
      RtpPacketInfo(kSsrc, {kCsrcs0, kCsrcs1}, kRtpTimestamp0, kReceiveTime0)
          .set_audio_level(kAudioLevel0)
          .set_absolute_capture_time(kAbsoluteCaptureTime)
          .set_local_capture_clock_offset(kLocalCaptureClockOffset),
      RtpPacketInfo(kSsrc, {kCsrcs2}, kRtpTimestamp1, kReceiveTime1)
          .set_audio_level(kAudioLevel1)
          .set_absolute_capture_time(kAbsoluteCaptureTime)
          .set_local_capture_clock_offset(kLocalCaptureClockOffset),
      RtpPacketInfo(kSsrc, {kCsrcs0}, kRtpTimestamp2, kReceiveTime2)
          .set_audio_level(kAudioLevel2)
          .set_absolute_capture_time(kAbsoluteCaptureTime)
          .set_local_capture_clock_offset(kLocalCaptureClockOffset),
  }));

  time_controller.AdvanceTime(TimeDelta::Zero());
  Timestamp timestamp = time_controller.GetClock()->CurrentTime();
  constexpr RtpSource::Extensions extensions0 = {
      .audio_level = kAudioLevel0,
      .absolute_capture_time = kAbsoluteCaptureTime,
      .local_capture_clock_offset = kLocalCaptureClockOffset};
  constexpr RtpSource::Extensions extensions1 = {
      .audio_level = kAudioLevel1,
      .absolute_capture_time = kAbsoluteCaptureTime,
      .local_capture_clock_offset = kLocalCaptureClockOffset};
  constexpr RtpSource::Extensions extensions2 = {
      .audio_level = kAudioLevel2,
      .absolute_capture_time = kAbsoluteCaptureTime,
      .local_capture_clock_offset = kLocalCaptureClockOffset};

  EXPECT_THAT(tracker.GetSources(),
              ElementsAre(RtpSource(timestamp, kSsrc, RtpSourceType::SSRC,
                                    kRtpTimestamp2, extensions2),
                          RtpSource(timestamp, kCsrcs0, RtpSourceType::CSRC,
                                    kRtpTimestamp2, extensions2),
                          RtpSource(timestamp, kCsrcs2, RtpSourceType::CSRC,
                                    kRtpTimestamp1, extensions1),
                          RtpSource(timestamp, kCsrcs1, RtpSourceType::CSRC,
                                    kRtpTimestamp0, extensions0)));
}

TEST(SourceTrackerTest, OnFrameDeliveredUpdatesSources) {
  constexpr uint32_t kSsrc1 = 10;
  constexpr uint32_t kSsrc2 = 11;
  constexpr uint32_t kCsrcs0 = 20;
  constexpr uint32_t kCsrcs1 = 21;
  constexpr uint32_t kCsrcs2 = 22;
  constexpr uint32_t kRtpTimestamp0 = 40;
  constexpr uint32_t kRtpTimestamp1 = 41;
  constexpr uint32_t kRtpTimestamp2 = 42;
  constexpr absl::optional<uint8_t> kAudioLevel0 = 50;
  constexpr absl::optional<uint8_t> kAudioLevel1 = absl::nullopt;
  constexpr absl::optional<uint8_t> kAudioLevel2 = 10;
  constexpr absl::optional<AbsoluteCaptureTime> kAbsoluteCaptureTime0 =
      AbsoluteCaptureTime{12, 34};
  constexpr absl::optional<AbsoluteCaptureTime> kAbsoluteCaptureTime1 =
      AbsoluteCaptureTime{56, 78};
  constexpr absl::optional<AbsoluteCaptureTime> kAbsoluteCaptureTime2 =
      AbsoluteCaptureTime{89, 90};
  constexpr absl::optional<TimeDelta> kLocalCaptureClockOffset0 =
      TimeDelta::Millis(123);
  constexpr absl::optional<TimeDelta> kLocalCaptureClockOffset1 =
      TimeDelta::Millis(456);
  constexpr absl::optional<TimeDelta> kLocalCaptureClockOffset2 =
      TimeDelta::Millis(789);
  constexpr Timestamp kReceiveTime0 = Timestamp::Millis(60);
  constexpr Timestamp kReceiveTime1 = Timestamp::Millis(61);
  constexpr Timestamp kReceiveTime2 = Timestamp::Millis(62);

  constexpr RtpSource::Extensions extensions0 = {
      .audio_level = kAudioLevel0,
      .absolute_capture_time = kAbsoluteCaptureTime0,
      .local_capture_clock_offset = kLocalCaptureClockOffset0};
  constexpr RtpSource::Extensions extensions1 = {
      .audio_level = kAudioLevel1,
      .absolute_capture_time = kAbsoluteCaptureTime1,
      .local_capture_clock_offset = kLocalCaptureClockOffset1};
  constexpr RtpSource::Extensions extensions2 = {
      .audio_level = kAudioLevel2,
      .absolute_capture_time = kAbsoluteCaptureTime2,
      .local_capture_clock_offset = kLocalCaptureClockOffset2};

  GlobalSimulatedTimeController time_controller(Timestamp::Seconds(1000));
  SourceTracker tracker(time_controller.GetClock());

  tracker.OnFrameDelivered(RtpPacketInfos(
      {RtpPacketInfo(kSsrc1, {kCsrcs0, kCsrcs1}, kRtpTimestamp0, kReceiveTime0)
           .set_audio_level(kAudioLevel0)
           .set_absolute_capture_time(kAbsoluteCaptureTime0)
           .set_local_capture_clock_offset(kLocalCaptureClockOffset0)}));

  time_controller.AdvanceTime(TimeDelta::Zero());
  Timestamp timestamp_0 = time_controller.GetClock()->CurrentTime();
  EXPECT_THAT(tracker.GetSources(),
              ElementsAre(RtpSource(timestamp_0, kSsrc1, RtpSourceType::SSRC,
                                    kRtpTimestamp0, extensions0),
                          RtpSource(timestamp_0, kCsrcs1, RtpSourceType::CSRC,
                                    kRtpTimestamp0, extensions0),
                          RtpSource(timestamp_0, kCsrcs0, RtpSourceType::CSRC,
                                    kRtpTimestamp0, extensions0)));

  // Deliver packets with updated sources.

  time_controller.AdvanceTime(TimeDelta::Millis(17));
  tracker.OnFrameDelivered(RtpPacketInfos(
      {RtpPacketInfo(kSsrc1, {kCsrcs0, kCsrcs2}, kRtpTimestamp1, kReceiveTime1)
           .set_audio_level(kAudioLevel1)
           .set_absolute_capture_time(kAbsoluteCaptureTime1)
           .set_local_capture_clock_offset(kLocalCaptureClockOffset1)}));

  time_controller.AdvanceTime(TimeDelta::Zero());
  Timestamp timestamp_1 = time_controller.GetClock()->CurrentTime();

  EXPECT_THAT(tracker.GetSources(),
              ElementsAre(RtpSource(timestamp_1, kSsrc1, RtpSourceType::SSRC,
                                    kRtpTimestamp1, extensions1),
                          RtpSource(timestamp_1, kCsrcs2, RtpSourceType::CSRC,
                                    kRtpTimestamp1, extensions1),
                          RtpSource(timestamp_1, kCsrcs0, RtpSourceType::CSRC,
                                    kRtpTimestamp1, extensions1),
                          RtpSource(timestamp_0, kCsrcs1, RtpSourceType::CSRC,
                                    kRtpTimestamp0, extensions0)));

  // Deliver more packets with update csrcs and a new ssrc.
  time_controller.AdvanceTime(TimeDelta::Millis(17));

  tracker.OnFrameDelivered(RtpPacketInfos(
      {RtpPacketInfo(kSsrc2, {kCsrcs0}, kRtpTimestamp2, kReceiveTime2)
           .set_audio_level(kAudioLevel2)
           .set_absolute_capture_time(kAbsoluteCaptureTime2)
           .set_local_capture_clock_offset(kLocalCaptureClockOffset2)}));

  time_controller.AdvanceTime(TimeDelta::Zero());
  Timestamp timestamp_2 = time_controller.GetClock()->CurrentTime();

  EXPECT_THAT(tracker.GetSources(),
              ElementsAre(RtpSource(timestamp_2, kSsrc2, RtpSourceType::SSRC,
                                    kRtpTimestamp2, extensions2),
                          RtpSource(timestamp_2, kCsrcs0, RtpSourceType::CSRC,
                                    kRtpTimestamp2, extensions2),
                          RtpSource(timestamp_1, kSsrc1, RtpSourceType::SSRC,
                                    kRtpTimestamp1, extensions1),
                          RtpSource(timestamp_1, kCsrcs2, RtpSourceType::CSRC,
                                    kRtpTimestamp1, extensions1),
                          RtpSource(timestamp_0, kCsrcs1, RtpSourceType::CSRC,
                                    kRtpTimestamp0, extensions0)));
}

TEST(SourceTrackerTest, TimedOutSourcesAreRemoved) {
  constexpr uint32_t kSsrc = 10;
  constexpr uint32_t kCsrcs0 = 20;
  constexpr uint32_t kCsrcs1 = 21;
  constexpr uint32_t kCsrcs2 = 22;
  constexpr uint32_t kRtpTimestamp0 = 40;
  constexpr uint32_t kRtpTimestamp1 = 41;
  constexpr absl::optional<uint8_t> kAudioLevel0 = 50;
  constexpr absl::optional<uint8_t> kAudioLevel1 = absl::nullopt;
  constexpr absl::optional<AbsoluteCaptureTime> kAbsoluteCaptureTime0 =
      AbsoluteCaptureTime{12, 34};
  constexpr absl::optional<AbsoluteCaptureTime> kAbsoluteCaptureTime1 =
      AbsoluteCaptureTime{56, 78};
  constexpr absl::optional<TimeDelta> kLocalCaptureClockOffset0 =
      TimeDelta::Millis(123);
  constexpr absl::optional<TimeDelta> kLocalCaptureClockOffset1 =
      TimeDelta::Millis(456);
  constexpr Timestamp kReceiveTime0 = Timestamp::Millis(60);
  constexpr Timestamp kReceiveTime1 = Timestamp::Millis(61);

  GlobalSimulatedTimeController time_controller(Timestamp::Seconds(1000));
  SourceTracker tracker(time_controller.GetClock());

  tracker.OnFrameDelivered(RtpPacketInfos(
      {RtpPacketInfo(kSsrc, {kCsrcs0, kCsrcs1}, kRtpTimestamp0, kReceiveTime0)
           .set_audio_level(kAudioLevel0)
           .set_absolute_capture_time(kAbsoluteCaptureTime0)
           .set_local_capture_clock_offset(kLocalCaptureClockOffset0)}));

  time_controller.AdvanceTime(TimeDelta::Millis(17));

  tracker.OnFrameDelivered(RtpPacketInfos(
      {RtpPacketInfo(kSsrc, {kCsrcs0, kCsrcs2}, kRtpTimestamp1, kReceiveTime1)
           .set_audio_level(kAudioLevel1)
           .set_absolute_capture_time(kAbsoluteCaptureTime1)
           .set_local_capture_clock_offset(kLocalCaptureClockOffset1)}));

  Timestamp timestamp_1 = time_controller.GetClock()->CurrentTime();

  time_controller.AdvanceTime(SourceTracker::kTimeout);

  constexpr RtpSource::Extensions extensions1 = {
      .audio_level = kAudioLevel1,
      .absolute_capture_time = kAbsoluteCaptureTime1,
      .local_capture_clock_offset = kLocalCaptureClockOffset1};

  EXPECT_THAT(tracker.GetSources(),
              ElementsAre(RtpSource(timestamp_1, kSsrc, RtpSourceType::SSRC,
                                    kRtpTimestamp1, extensions1),
                          RtpSource(timestamp_1, kCsrcs2, RtpSourceType::CSRC,
                                    kRtpTimestamp1, extensions1),
                          RtpSource(timestamp_1, kCsrcs0, RtpSourceType::CSRC,
                                    kRtpTimestamp1, extensions1)));
}

}  // namespace webrtc
