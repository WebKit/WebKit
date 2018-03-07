/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/stats/rtcstatsreport.h"

#include "api/stats/rtcstats.h"
#include "rtc_base/checks.h"
#include "rtc_base/gunit.h"

namespace webrtc {

class RTCTestStats1 : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCTestStats1(const std::string& id, int64_t timestamp_us)
      : RTCStats(id, timestamp_us),
        integer("integer") {}

  RTCStatsMember<int32_t> integer;
};

WEBRTC_RTCSTATS_IMPL(RTCTestStats1, RTCStats, "test-stats-1",
    &integer);

class RTCTestStats2 : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCTestStats2(const std::string& id, int64_t timestamp_us)
      : RTCStats(id, timestamp_us),
        number("number") {}

  RTCStatsMember<double> number;
};

WEBRTC_RTCSTATS_IMPL(RTCTestStats2, RTCStats, "test-stats-2",
    &number);

class RTCTestStats3 : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCTestStats3(const std::string& id, int64_t timestamp_us)
      : RTCStats(id, timestamp_us),
        string("string") {}

  RTCStatsMember<std::string> string;
};

WEBRTC_RTCSTATS_IMPL(RTCTestStats3, RTCStats, "test-stats-3",
    &string);

TEST(RTCStatsReport, AddAndGetStats) {
  rtc::scoped_refptr<RTCStatsReport> report = RTCStatsReport::Create(1337);
  EXPECT_EQ(report->timestamp_us(), 1337u);
  EXPECT_EQ(report->size(), static_cast<size_t>(0));
  report->AddStats(std::unique_ptr<RTCStats>(new RTCTestStats1("a0", 1)));
  report->AddStats(std::unique_ptr<RTCStats>(new RTCTestStats1("a1", 2)));
  report->AddStats(std::unique_ptr<RTCStats>(new RTCTestStats2("b0", 4)));
  report->AddStats(std::unique_ptr<RTCStats>(new RTCTestStats2("b1", 8)));
  report->AddStats(std::unique_ptr<RTCStats>(new RTCTestStats1("a2", 16)));
  report->AddStats(std::unique_ptr<RTCStats>(new RTCTestStats2("b2", 32)));
  EXPECT_EQ(report->size(), static_cast<size_t>(6));

  EXPECT_EQ(report->Get("missing"), nullptr);
  EXPECT_EQ(report->Get("a0")->id(), "a0");
  EXPECT_EQ(report->Get("b2")->id(), "b2");

  std::vector<const RTCTestStats1*> a = report->GetStatsOfType<RTCTestStats1>();
  EXPECT_EQ(a.size(), static_cast<size_t>(3));
  int64_t mask = 0;
  for (const RTCTestStats1* stats : a)
    mask |= stats->timestamp_us();
  EXPECT_EQ(mask, static_cast<int64_t>(1 | 2 | 16));

  std::vector<const RTCTestStats2*> b = report->GetStatsOfType<RTCTestStats2>();
  EXPECT_EQ(b.size(), static_cast<size_t>(3));
  mask = 0;
  for (const RTCTestStats2* stats : b)
    mask |= stats->timestamp_us();
  EXPECT_EQ(mask, static_cast<int64_t>(4 | 8 | 32));

  EXPECT_EQ(report->GetStatsOfType<RTCTestStats3>().size(),
            static_cast<size_t>(0));
}

TEST(RTCStatsReport, StatsOrder) {
  rtc::scoped_refptr<RTCStatsReport> report = RTCStatsReport::Create(1337);
  EXPECT_EQ(report->timestamp_us(), 1337u);
  report->AddStats(std::unique_ptr<RTCStats>(new RTCTestStats1("C", 2)));
  report->AddStats(std::unique_ptr<RTCStats>(new RTCTestStats1("D", 3)));
  report->AddStats(std::unique_ptr<RTCStats>(new RTCTestStats2("B", 1)));
  report->AddStats(std::unique_ptr<RTCStats>(new RTCTestStats2("A", 0)));
  report->AddStats(std::unique_ptr<RTCStats>(new RTCTestStats2("E", 4)));
  report->AddStats(std::unique_ptr<RTCStats>(new RTCTestStats2("F", 5)));
  report->AddStats(std::unique_ptr<RTCStats>(new RTCTestStats2("G", 6)));
  int64_t i = 0;
  for (const RTCStats& stats : *report) {
    EXPECT_EQ(stats.timestamp_us(), i);
    ++i;
  }
  EXPECT_EQ(i, static_cast<int64_t>(7));
}

TEST(RTCStatsReport, TakeMembersFrom) {
  rtc::scoped_refptr<RTCStatsReport> a = RTCStatsReport::Create(1337);
  EXPECT_EQ(a->timestamp_us(), 1337u);
  a->AddStats(std::unique_ptr<RTCStats>(new RTCTestStats1("B", 1)));
  a->AddStats(std::unique_ptr<RTCStats>(new RTCTestStats1("C", 2)));
  a->AddStats(std::unique_ptr<RTCStats>(new RTCTestStats1("E", 4)));
  rtc::scoped_refptr<RTCStatsReport> b = RTCStatsReport::Create(1338);
  EXPECT_EQ(b->timestamp_us(), 1338u);
  b->AddStats(std::unique_ptr<RTCStats>(new RTCTestStats1("A", 0)));
  b->AddStats(std::unique_ptr<RTCStats>(new RTCTestStats1("D", 3)));
  b->AddStats(std::unique_ptr<RTCStats>(new RTCTestStats1("F", 5)));

  a->TakeMembersFrom(b);
  EXPECT_EQ(b->size(), static_cast<size_t>(0));
  int64_t i = 0;
  for (const RTCStats& stats : *a) {
    EXPECT_EQ(stats.timestamp_us(), i);
    ++i;
  }
  EXPECT_EQ(i, static_cast<int64_t>(6));
}

}  // namespace webrtc
