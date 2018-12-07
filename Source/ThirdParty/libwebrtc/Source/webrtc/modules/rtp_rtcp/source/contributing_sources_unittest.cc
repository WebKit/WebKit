/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/contributing_sources.h"

#include "rtc_base/timeutils.h"

#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::UnorderedElementsAre;

constexpr uint32_t kCsrc1 = 111;
constexpr uint32_t kCsrc2 = 222;
constexpr uint32_t kCsrc3 = 333;

}  // namespace

TEST(ContributingSourcesTest, RecordSources) {
  ContributingSources csrcs;
  constexpr uint32_t kCsrcs[] = {kCsrc1, kCsrc2};
  constexpr int64_t kTime1 = 10;
  csrcs.Update(kTime1, kCsrcs, absl::nullopt);
  EXPECT_THAT(
      csrcs.GetSources(kTime1),
      UnorderedElementsAre(RtpSource(kTime1, kCsrc1, RtpSourceType::CSRC),
                           RtpSource(kTime1, kCsrc2, RtpSourceType::CSRC)));
}

TEST(ContributingSourcesTest, UpdateSources) {
  ContributingSources csrcs;
  // TODO(nisse): When migrating to absl::Span, the named constant arrays should
  // be replaced by unnamed literals where they are passed to csrcs.Update(...).
  constexpr uint32_t kCsrcs1[] = {kCsrc1, kCsrc2};
  constexpr uint32_t kCsrcs2[] = {kCsrc2, kCsrc3};
  constexpr int64_t kTime1 = 10;
  constexpr int64_t kTime2 = kTime1 + 5 * rtc::kNumMillisecsPerSec;
  csrcs.Update(kTime1, kCsrcs1, absl::nullopt);
  EXPECT_THAT(
      csrcs.GetSources(kTime1),
      UnorderedElementsAre(RtpSource(kTime1, kCsrc1, RtpSourceType::CSRC),
                           RtpSource(kTime1, kCsrc2, RtpSourceType::CSRC)));
  csrcs.Update(kTime2, kCsrcs2, absl::nullopt);
  EXPECT_THAT(
      csrcs.GetSources(kTime2),
      UnorderedElementsAre(RtpSource(kTime1, kCsrc1, RtpSourceType::CSRC),
                           RtpSource(kTime2, kCsrc2, RtpSourceType::CSRC),
                           RtpSource(kTime2, kCsrc3, RtpSourceType::CSRC)));
}

TEST(ContributingSourcesTest, ReturnRecentOnly) {
  ContributingSources csrcs;
  constexpr uint32_t kCsrcs1[] = {kCsrc1, kCsrc2};
  constexpr uint32_t kCsrcs2[] = {kCsrc2, kCsrc3};
  constexpr int64_t kTime1 = 10;
  constexpr int64_t kTime2 = kTime1 + 5 * rtc::kNumMillisecsPerSec;
  constexpr int64_t kTime3 = kTime1 + 12 * rtc::kNumMillisecsPerSec;
  csrcs.Update(kTime1, kCsrcs1, absl::nullopt);
  EXPECT_THAT(
      csrcs.GetSources(kTime1),
      UnorderedElementsAre(RtpSource(kTime1, kCsrc1, RtpSourceType::CSRC),
                           RtpSource(kTime1, kCsrc2, RtpSourceType::CSRC)));
  csrcs.Update(kTime2, kCsrcs2, absl::nullopt);
  EXPECT_THAT(
      csrcs.GetSources(kTime3),
      UnorderedElementsAre(RtpSource(kTime2, kCsrc2, RtpSourceType::CSRC),
                           RtpSource(kTime2, kCsrc3, RtpSourceType::CSRC)));
}

TEST(ContributingSourcesTest, PurgeOldSources) {
  ContributingSources csrcs;
  constexpr uint32_t kCsrcs1[] = {kCsrc1, kCsrc2};
  constexpr uint32_t kCsrcs2[] = {kCsrc2, kCsrc3};
  constexpr int64_t kTime1 = 10;
  constexpr int64_t kTime2 = kTime1 + 10 * rtc::kNumMillisecsPerSec;
  constexpr int64_t kTime3 = kTime1 + 20 * rtc::kNumMillisecsPerSec;
  csrcs.Update(kTime1, kCsrcs1, absl::nullopt);
  EXPECT_THAT(
      csrcs.GetSources(kTime2),
      UnorderedElementsAre(RtpSource(kTime1, kCsrc1, RtpSourceType::CSRC),
                           RtpSource(kTime1, kCsrc2, RtpSourceType::CSRC)));
  csrcs.Update(kTime2, kCsrcs2, absl::nullopt);
  EXPECT_THAT(
      csrcs.GetSources(kTime2),
      UnorderedElementsAre(RtpSource(kTime1, kCsrc1, RtpSourceType::CSRC),
                           RtpSource(kTime2, kCsrc2, RtpSourceType::CSRC),
                           RtpSource(kTime2, kCsrc3, RtpSourceType::CSRC)));
  csrcs.Update(kTime3, kCsrcs2, absl::nullopt);
  EXPECT_THAT(
      csrcs.GetSources(kTime3),
      UnorderedElementsAre(RtpSource(kTime3, kCsrc2, RtpSourceType::CSRC),
                           RtpSource(kTime3, kCsrc3, RtpSourceType::CSRC)));
  // Query at an earlier time; check that old sources really have been purged
  // and don't reappear.
  EXPECT_THAT(
      csrcs.GetSources(kTime2),
      UnorderedElementsAre(RtpSource(kTime3, kCsrc2, RtpSourceType::CSRC),
                           RtpSource(kTime3, kCsrc3, RtpSourceType::CSRC)));
}

TEST(ContributingSourcesTest, AudioLevel) {
  ContributingSources csrcs;
  constexpr uint32_t kCsrcs[] = {kCsrc1, kCsrc2};
  constexpr int64_t kTime1 = 10;
  csrcs.Update(kTime1, kCsrcs, 47);
  EXPECT_THAT(
      csrcs.GetSources(kTime1),
      UnorderedElementsAre(RtpSource(kTime1, kCsrc1, RtpSourceType::CSRC, 47),
                           RtpSource(kTime1, kCsrc2, RtpSourceType::CSRC, 47)));

  constexpr uint32_t kCsrcsSubset[] = {kCsrc1};
  csrcs.Update(kTime1 + 1, kCsrcsSubset, absl::nullopt);
  EXPECT_THAT(
      csrcs.GetSources(kTime1 + 1),
      UnorderedElementsAre(RtpSource(kTime1 + 1, kCsrc1, RtpSourceType::CSRC),
                           RtpSource(kTime1, kCsrc2, RtpSourceType::CSRC, 47)));
}

}  // namespace webrtc
