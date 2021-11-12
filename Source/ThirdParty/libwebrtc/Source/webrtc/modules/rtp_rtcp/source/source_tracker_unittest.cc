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
#include "test/gmock.h"
#include "test/gtest.h"

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
    const int64_t now_ms = clock_->TimeInMilliseconds();

    for (const auto& packet_info : packet_infos) {
      RtpSource::Extensions extensions = {packet_info.audio_level(),
                                          packet_info.absolute_capture_time()};

      for (const auto& csrc : packet_info.csrcs()) {
        entries_.emplace_front(now_ms, csrc, RtpSourceType::CSRC,
                               packet_info.rtp_timestamp(), extensions);
      }

      entries_.emplace_front(now_ms, packet_info.ssrc(), RtpSourceType::SSRC,
                             packet_info.rtp_timestamp(), extensions);
    }

    PruneEntries(now_ms);
  }

  std::vector<RtpSource> GetSources() const {
    PruneEntries(clock_->TimeInMilliseconds());

    return std::vector<RtpSource>(entries_.begin(), entries_.end());
  }

 private:
  void PruneEntries(int64_t now_ms) const {
    const int64_t prune_ms = now_ms - 10000;  // 10 seconds

    std::set<std::pair<RtpSourceType, uint32_t>> seen;

    auto it = entries_.begin();
    auto end = entries_.end();
    while (it != end) {
      auto next = it;
      ++next;

      auto key = std::make_pair(it->source_type(), it->source_id());
      if (!seen.insert(key).second || it->timestamp_ms() < prune_ms) {
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
      packet_infos.emplace_back(GenerateSsrc(), GenerateCsrcs(),
                                GenerateRtpTimestamp(), GenerateAudioLevel(),
                                GenerateAbsoluteCaptureTime(),
                                GenerateReceiveTime());
    }

    return RtpPacketInfos(std::move(packet_infos));
  }

  int64_t GenerateClockAdvanceTimeMilliseconds() {
    double roll = std::uniform_real_distribution<double>(0.0, 1.0)(generator_);

    if (roll < 0.05) {
      return 0;
    }

    if (roll < 0.08) {
      return SourceTracker::kTimeoutMs - 1;
    }

    if (roll < 0.11) {
      return SourceTracker::kTimeoutMs;
    }

    if (roll < 0.19) {
      return std::uniform_int_distribution<int64_t>(
          SourceTracker::kTimeoutMs,
          SourceTracker::kTimeoutMs * 1000)(generator_);
    }

    return std::uniform_int_distribution<int64_t>(
        1, SourceTracker::kTimeoutMs - 1)(generator_);
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

  Timestamp GenerateReceiveTime() {
    return Timestamp::Micros(
        std::uniform_int_distribution<int64_t>()(generator_));
  }

  const uint32_t ssrcs_count_;
  const uint32_t csrcs_count_;

  std::mt19937 generator_;
};

}  // namespace

TEST_P(SourceTrackerRandomTest, RandomOperations) {
  constexpr size_t kIterationsCount = 200;

  SimulatedClock clock(1000000000000ULL);
  SourceTracker actual_tracker(&clock);
  ExpectedSourceTracker expected_tracker(&clock);

  ASSERT_THAT(actual_tracker.GetSources(), IsEmpty());
  ASSERT_THAT(expected_tracker.GetSources(), IsEmpty());

  for (size_t i = 0; i < kIterationsCount; ++i) {
    RtpPacketInfos packet_infos = GeneratePacketInfos();

    actual_tracker.OnFrameDelivered(packet_infos);
    expected_tracker.OnFrameDelivered(packet_infos);

    clock.AdvanceTimeMilliseconds(GenerateClockAdvanceTimeMilliseconds());

    ASSERT_THAT(actual_tracker.GetSources(),
                ElementsAreArray(expected_tracker.GetSources()));
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         SourceTrackerRandomTest,
                         Combine(/*ssrcs_count_=*/Values(1, 2, 4),
                                 /*csrcs_count_=*/Values(0, 1, 3, 7)));

TEST(SourceTrackerTest, StartEmpty) {
  SimulatedClock clock(1000000000000ULL);
  SourceTracker tracker(&clock);

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
  constexpr Timestamp kReceiveTime0 = Timestamp::Millis(60);
  constexpr Timestamp kReceiveTime1 = Timestamp::Millis(70);

  SimulatedClock clock(1000000000000ULL);
  SourceTracker tracker(&clock);

  tracker.OnFrameDelivered(RtpPacketInfos(
      {RtpPacketInfo(kSsrc1, {kCsrcs0, kCsrcs1}, kRtpTimestamp0, kAudioLevel0,
                     kAbsoluteCaptureTime, kReceiveTime0),
       RtpPacketInfo(kSsrc2, {kCsrcs2}, kRtpTimestamp1, kAudioLevel1,
                     kAbsoluteCaptureTime, kReceiveTime1)}));

  int64_t timestamp_ms = clock.TimeInMilliseconds();
  constexpr RtpSource::Extensions extensions0 = {kAudioLevel0,
                                                 kAbsoluteCaptureTime};
  constexpr RtpSource::Extensions extensions1 = {kAudioLevel1,
                                                 kAbsoluteCaptureTime};

  EXPECT_THAT(tracker.GetSources(),
              ElementsAre(RtpSource(timestamp_ms, kSsrc2, RtpSourceType::SSRC,
                                    kRtpTimestamp1, extensions1),
                          RtpSource(timestamp_ms, kCsrcs2, RtpSourceType::CSRC,
                                    kRtpTimestamp1, extensions1),
                          RtpSource(timestamp_ms, kSsrc1, RtpSourceType::SSRC,
                                    kRtpTimestamp0, extensions0),
                          RtpSource(timestamp_ms, kCsrcs1, RtpSourceType::CSRC,
                                    kRtpTimestamp0, extensions0),
                          RtpSource(timestamp_ms, kCsrcs0, RtpSourceType::CSRC,
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
  constexpr Timestamp kReceiveTime0 = Timestamp::Millis(60);
  constexpr Timestamp kReceiveTime1 = Timestamp::Millis(70);
  constexpr Timestamp kReceiveTime2 = Timestamp::Millis(80);

  SimulatedClock clock(1000000000000ULL);
  SourceTracker tracker(&clock);

  tracker.OnFrameDelivered(RtpPacketInfos(
      {RtpPacketInfo(kSsrc, {kCsrcs0, kCsrcs1}, kRtpTimestamp0, kAudioLevel0,
                     kAbsoluteCaptureTime, kReceiveTime0),
       RtpPacketInfo(kSsrc, {kCsrcs2}, kRtpTimestamp1, kAudioLevel1,
                     kAbsoluteCaptureTime, kReceiveTime1),
       RtpPacketInfo(kSsrc, {kCsrcs0}, kRtpTimestamp2, kAudioLevel2,
                     kAbsoluteCaptureTime, kReceiveTime2)}));

  int64_t timestamp_ms = clock.TimeInMilliseconds();
  constexpr RtpSource::Extensions extensions0 = {kAudioLevel0,
                                                 kAbsoluteCaptureTime};
  constexpr RtpSource::Extensions extensions1 = {kAudioLevel1,
                                                 kAbsoluteCaptureTime};
  constexpr RtpSource::Extensions extensions2 = {kAudioLevel2,
                                                 kAbsoluteCaptureTime};

  EXPECT_THAT(tracker.GetSources(),
              ElementsAre(RtpSource(timestamp_ms, kSsrc, RtpSourceType::SSRC,
                                    kRtpTimestamp2, extensions2),
                          RtpSource(timestamp_ms, kCsrcs0, RtpSourceType::CSRC,
                                    kRtpTimestamp2, extensions2),
                          RtpSource(timestamp_ms, kCsrcs2, RtpSourceType::CSRC,
                                    kRtpTimestamp1, extensions1),
                          RtpSource(timestamp_ms, kCsrcs1, RtpSourceType::CSRC,
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
  constexpr Timestamp kReceiveTime0 = Timestamp::Millis(60);
  constexpr Timestamp kReceiveTime1 = Timestamp::Millis(61);
  constexpr Timestamp kReceiveTime2 = Timestamp::Millis(62);

  constexpr RtpSource::Extensions extensions0 = {kAudioLevel0,
                                                 kAbsoluteCaptureTime0};
  constexpr RtpSource::Extensions extensions1 = {kAudioLevel1,
                                                 kAbsoluteCaptureTime1};
  constexpr RtpSource::Extensions extensions2 = {kAudioLevel2,
                                                 kAbsoluteCaptureTime2};

  SimulatedClock clock(1000000000000ULL);
  SourceTracker tracker(&clock);

  tracker.OnFrameDelivered(RtpPacketInfos(
      {RtpPacketInfo(kSsrc1, {kCsrcs0, kCsrcs1}, kRtpTimestamp0, kAudioLevel0,
                     kAbsoluteCaptureTime0, kReceiveTime0)}));

  int64_t timestamp_ms_0 = clock.TimeInMilliseconds();
  EXPECT_THAT(
      tracker.GetSources(),
      ElementsAre(RtpSource(timestamp_ms_0, kSsrc1, RtpSourceType::SSRC,
                            kRtpTimestamp0, extensions0),
                  RtpSource(timestamp_ms_0, kCsrcs1, RtpSourceType::CSRC,
                            kRtpTimestamp0, extensions0),
                  RtpSource(timestamp_ms_0, kCsrcs0, RtpSourceType::CSRC,
                            kRtpTimestamp0, extensions0)));

  // Deliver packets with updated sources.

  clock.AdvanceTimeMilliseconds(17);
  tracker.OnFrameDelivered(RtpPacketInfos(
      {RtpPacketInfo(kSsrc1, {kCsrcs0, kCsrcs2}, kRtpTimestamp1, kAudioLevel1,
                     kAbsoluteCaptureTime1, kReceiveTime1)}));

  int64_t timestamp_ms_1 = clock.TimeInMilliseconds();

  EXPECT_THAT(
      tracker.GetSources(),
      ElementsAre(RtpSource(timestamp_ms_1, kSsrc1, RtpSourceType::SSRC,
                            kRtpTimestamp1, extensions1),
                  RtpSource(timestamp_ms_1, kCsrcs2, RtpSourceType::CSRC,
                            kRtpTimestamp1, extensions1),
                  RtpSource(timestamp_ms_1, kCsrcs0, RtpSourceType::CSRC,
                            kRtpTimestamp1, extensions1),
                  RtpSource(timestamp_ms_0, kCsrcs1, RtpSourceType::CSRC,
                            kRtpTimestamp0, extensions0)));

  // Deliver more packets with update csrcs and a new ssrc.
  clock.AdvanceTimeMilliseconds(17);
  tracker.OnFrameDelivered(RtpPacketInfos(
      {RtpPacketInfo(kSsrc2, {kCsrcs0}, kRtpTimestamp2, kAudioLevel2,
                     kAbsoluteCaptureTime2, kReceiveTime2)}));

  int64_t timestamp_ms_2 = clock.TimeInMilliseconds();

  EXPECT_THAT(
      tracker.GetSources(),
      ElementsAre(RtpSource(timestamp_ms_2, kSsrc2, RtpSourceType::SSRC,
                            kRtpTimestamp2, extensions2),
                  RtpSource(timestamp_ms_2, kCsrcs0, RtpSourceType::CSRC,
                            kRtpTimestamp2, extensions2),
                  RtpSource(timestamp_ms_1, kSsrc1, RtpSourceType::SSRC,
                            kRtpTimestamp1, extensions1),
                  RtpSource(timestamp_ms_1, kCsrcs2, RtpSourceType::CSRC,
                            kRtpTimestamp1, extensions1),
                  RtpSource(timestamp_ms_0, kCsrcs1, RtpSourceType::CSRC,
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
  constexpr Timestamp kReceiveTime0 = Timestamp::Millis(60);
  constexpr Timestamp kReceiveTime1 = Timestamp::Millis(61);

  SimulatedClock clock(1000000000000ULL);
  SourceTracker tracker(&clock);

  tracker.OnFrameDelivered(RtpPacketInfos(
      {RtpPacketInfo(kSsrc, {kCsrcs0, kCsrcs1}, kRtpTimestamp0, kAudioLevel0,
                     kAbsoluteCaptureTime0, kReceiveTime0)}));

  clock.AdvanceTimeMilliseconds(17);

  tracker.OnFrameDelivered(RtpPacketInfos(
      {RtpPacketInfo(kSsrc, {kCsrcs0, kCsrcs2}, kRtpTimestamp1, kAudioLevel1,
                     kAbsoluteCaptureTime1, kReceiveTime1)}));

  int64_t timestamp_ms_1 = clock.TimeInMilliseconds();

  clock.AdvanceTimeMilliseconds(SourceTracker::kTimeoutMs);

  constexpr RtpSource::Extensions extensions1 = {kAudioLevel1,
                                                 kAbsoluteCaptureTime1};

  EXPECT_THAT(
      tracker.GetSources(),
      ElementsAre(RtpSource(timestamp_ms_1, kSsrc, RtpSourceType::SSRC,
                            kRtpTimestamp1, extensions1),
                  RtpSource(timestamp_ms_1, kCsrcs2, RtpSourceType::CSRC,
                            kRtpTimestamp1, extensions1),
                  RtpSource(timestamp_ms_1, kCsrcs0, RtpSourceType::CSRC,
                            kRtpTimestamp1, extensions1)));
}

}  // namespace webrtc
