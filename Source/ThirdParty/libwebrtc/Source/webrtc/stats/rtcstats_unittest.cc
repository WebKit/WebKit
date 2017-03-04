/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/stats/rtcstats.h"

#include <cstring>

#include "webrtc/base/checks.h"
#include "webrtc/base/gunit.h"
#include "webrtc/stats/test/rtcteststats.h"

namespace webrtc {

class RTCChildStats : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCChildStats(const std::string& id, int64_t timestamp_us)
      : RTCStats(id, timestamp_us),
        child_int("childInt") {}

  RTCStatsMember<int32_t> child_int;
};

WEBRTC_RTCSTATS_IMPL(RTCChildStats, RTCStats, "child-stats",
    &child_int);

class RTCGrandChildStats : public RTCChildStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCGrandChildStats(const std::string& id, int64_t timestamp_us)
      : RTCChildStats(id, timestamp_us),
        grandchild_int("grandchildInt") {}

  RTCStatsMember<int32_t> grandchild_int;
};

WEBRTC_RTCSTATS_IMPL(RTCGrandChildStats, RTCChildStats, "grandchild-stats",
      &grandchild_int);

TEST(RTCStatsTest, RTCStatsAndMembers) {
  RTCTestStats stats("testId", 42);
  EXPECT_EQ(stats.id(), "testId");
  EXPECT_EQ(stats.timestamp_us(), static_cast<int64_t>(42));
  std::vector<const RTCStatsMemberInterface*> members = stats.Members();
  EXPECT_EQ(members.size(), static_cast<size_t>(14));
  for (const RTCStatsMemberInterface* member : members) {
    EXPECT_FALSE(member->is_defined());
  }
  stats.m_bool = true;
  stats.m_int32 = 123;
  stats.m_uint32 = 123;
  stats.m_int64 = 123;
  stats.m_uint64 = 123;
  stats.m_double = 123.0;
  stats.m_string = std::string("123");

  std::vector<bool> sequence_bool;
  sequence_bool.push_back(true);
  std::vector<int32_t> sequence_int32;
  sequence_int32.push_back(static_cast<int32_t>(1));
  std::vector<uint32_t> sequence_uint32;
  sequence_uint32.push_back(static_cast<uint32_t>(2));
  std::vector<int64_t> sequence_int64;
  sequence_int64.push_back(static_cast<int64_t>(3));
  std::vector<uint64_t> sequence_uint64;
  sequence_uint64.push_back(static_cast<uint64_t>(4));
  std::vector<double> sequence_double;
  sequence_double.push_back(5.0);
  std::vector<std::string> sequence_string;
  sequence_string.push_back(std::string("six"));

  stats.m_sequence_bool = sequence_bool;
  stats.m_sequence_int32 = sequence_int32;
  stats.m_sequence_uint32 = sequence_uint32;
  EXPECT_FALSE(stats.m_sequence_int64.is_defined());
  stats.m_sequence_int64 = sequence_int64;
  stats.m_sequence_uint64 = sequence_uint64;
  stats.m_sequence_double = sequence_double;
  stats.m_sequence_string = sequence_string;
  for (const RTCStatsMemberInterface* member : members) {
    EXPECT_TRUE(member->is_defined());
  }
  EXPECT_EQ(*stats.m_bool, true);
  EXPECT_EQ(*stats.m_int32, static_cast<int32_t>(123));
  EXPECT_EQ(*stats.m_uint32, static_cast<uint32_t>(123));
  EXPECT_EQ(*stats.m_int64, static_cast<int64_t>(123));
  EXPECT_EQ(*stats.m_uint64, static_cast<uint64_t>(123));
  EXPECT_EQ(*stats.m_double, 123.0);
  EXPECT_EQ(*stats.m_string, std::string("123"));
  EXPECT_EQ(*stats.m_sequence_bool, sequence_bool);
  EXPECT_EQ(*stats.m_sequence_int32, sequence_int32);
  EXPECT_EQ(*stats.m_sequence_uint32, sequence_uint32);
  EXPECT_EQ(*stats.m_sequence_int64, sequence_int64);
  EXPECT_EQ(*stats.m_sequence_uint64, sequence_uint64);
  EXPECT_EQ(*stats.m_sequence_double, sequence_double);
  EXPECT_EQ(*stats.m_sequence_string, sequence_string);

  int32_t numbers[] = { 4, 8, 15, 16, 23, 42 };
  std::vector<int32_t> numbers_sequence(&numbers[0], &numbers[6]);
  stats.m_sequence_int32->clear();
  stats.m_sequence_int32->insert(stats.m_sequence_int32->end(),
                                 numbers_sequence.begin(),
                                 numbers_sequence.end());
  EXPECT_EQ(*stats.m_sequence_int32, numbers_sequence);
}

TEST(RTCStatsTest, EqualityOperator) {
  RTCTestStats empty_stats("testId", 123);
  EXPECT_EQ(empty_stats, empty_stats);

  RTCTestStats stats_with_all_values = empty_stats;
  stats_with_all_values.m_bool = true;
  stats_with_all_values.m_int32 = 123;
  stats_with_all_values.m_uint32 = 123;
  stats_with_all_values.m_int64 = 123;
  stats_with_all_values.m_uint64 = 123;
  stats_with_all_values.m_double = 123.0;
  stats_with_all_values.m_string = "123";
  stats_with_all_values.m_sequence_bool = std::vector<bool>();
  stats_with_all_values.m_sequence_int32 = std::vector<int32_t>();
  stats_with_all_values.m_sequence_uint32 = std::vector<uint32_t>();
  stats_with_all_values.m_sequence_int64 = std::vector<int64_t>();
  stats_with_all_values.m_sequence_uint64 = std::vector<uint64_t>();
  stats_with_all_values.m_sequence_double = std::vector<double>();
  stats_with_all_values.m_sequence_string = std::vector<std::string>();
  EXPECT_NE(stats_with_all_values, empty_stats);
  EXPECT_EQ(stats_with_all_values, stats_with_all_values);
  EXPECT_NE(stats_with_all_values.m_int32, stats_with_all_values.m_uint32);

  RTCTestStats one_member_different[] = {
    stats_with_all_values, stats_with_all_values, stats_with_all_values,
    stats_with_all_values, stats_with_all_values, stats_with_all_values,
    stats_with_all_values, stats_with_all_values, stats_with_all_values,
    stats_with_all_values, stats_with_all_values, stats_with_all_values,
    stats_with_all_values, stats_with_all_values,
  };
  for (size_t i = 0; i < 14; ++i) {
    EXPECT_EQ(stats_with_all_values, one_member_different[i]);
  }
  one_member_different[0].m_bool = false;
  one_member_different[1].m_int32 = 321;
  one_member_different[2].m_uint32 = 321;
  one_member_different[3].m_int64 = 321;
  one_member_different[4].m_uint64 = 321;
  one_member_different[5].m_double = 321.0;
  one_member_different[6].m_string = "321";
  one_member_different[7].m_sequence_bool->push_back(false);
  one_member_different[8].m_sequence_int32->push_back(321);
  one_member_different[9].m_sequence_uint32->push_back(321);
  one_member_different[10].m_sequence_int64->push_back(321);
  one_member_different[11].m_sequence_uint64->push_back(321);
  one_member_different[12].m_sequence_double->push_back(321.0);
  one_member_different[13].m_sequence_string->push_back("321");
  for (size_t i = 0; i < 14; ++i) {
    EXPECT_NE(stats_with_all_values, one_member_different[i]);
  }

  RTCTestStats empty_stats_different_id("testId2", 123);
  EXPECT_NE(empty_stats, empty_stats_different_id);
  RTCTestStats empty_stats_different_timestamp("testId", 321);
  EXPECT_EQ(empty_stats, empty_stats_different_timestamp);

  RTCChildStats child("childId", 42);
  RTCGrandChildStats grandchild("grandchildId", 42);
  EXPECT_NE(child, grandchild);

  RTCChildStats stats_with_defined_member("leId", 0);
  stats_with_defined_member.child_int = 0;
  RTCChildStats stats_with_undefined_member("leId", 0);
  EXPECT_NE(stats_with_defined_member, stats_with_undefined_member);
  EXPECT_NE(stats_with_undefined_member, stats_with_defined_member);
}

TEST(RTCStatsTest, RTCStatsGrandChild) {
  RTCGrandChildStats stats("grandchild", 0.0);
  stats.child_int = 1;
  stats.grandchild_int = 2;
  int32_t sum = 0;
  for (const RTCStatsMemberInterface* member : stats.Members()) {
    sum += *member->cast_to<const RTCStatsMember<int32_t>>();
  }
  EXPECT_EQ(sum, static_cast<int32_t>(3));

  std::unique_ptr<RTCStats> copy_ptr = stats.copy();
  const RTCGrandChildStats& copy = copy_ptr->cast_to<RTCGrandChildStats>();
  EXPECT_EQ(*copy.child_int, *stats.child_int);
  EXPECT_EQ(*copy.grandchild_int, *stats.grandchild_int);
}

// Death tests.
// Disabled on Android because death tests misbehave on Android, see
// base/test/gtest_util.h.
#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

TEST(RTCStatsDeathTest, ValueOfUndefinedMember) {
  RTCTestStats stats("testId", 0.0);
  EXPECT_FALSE(stats.m_int32.is_defined());
  EXPECT_DEATH(*stats.m_int32, "");
}

TEST(RTCStatsDeathTest, InvalidCasting) {
  RTCGrandChildStats stats("grandchild", 0.0);
  EXPECT_DEATH(stats.cast_to<RTCChildStats>(), "");
}

#endif  // RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

}  // namespace webrtc
