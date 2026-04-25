/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/stats/rtc_stats.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "api/stats/attribute.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "rtc_base/strings/json.h"
#include "stats/test/rtc_test_stats.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

// JSON stores numbers as floating point numbers with 53 significant bits, which
// amounts to about 15.95 decimal digits. Thus, when comparing large numbers
// processed by JSON, that's all the precision we should expect.
const double JSON_EPSILON = 1e-15;

// We do this since Google Test doesn't support relative error.
// This is computed as follows:
// If |a - b| / |a| < EPS, then |a - b| < |a| * EPS, so |a| * EPS is the
// maximum expected error.
double GetExpectedError(const double expected_value) {
  return JSON_EPSILON * fabs(expected_value);
}

}  // namespace

class RTCChildStats : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL(RTCChildStats);

  RTCChildStats(const std::string& id, Timestamp timestamp)
      : RTCStats(id, timestamp) {}

  std::optional<int32_t> child_int;
};

WEBRTC_RTCSTATS_IMPL(RTCChildStats,
                     RTCStats,
                     "child-stats",
                     AttributeInit("childInt", &child_int))

class RTCGrandChildStats : public RTCChildStats {
 public:
  WEBRTC_RTCSTATS_DECL(RTCGrandChildStats);

  RTCGrandChildStats(const std::string& id, Timestamp timestamp)
      : RTCChildStats(id, timestamp) {}

  std::optional<int32_t> grandchild_int;
};

WEBRTC_RTCSTATS_IMPL(RTCGrandChildStats,
                     RTCChildStats,
                     "grandchild-stats",
                     AttributeInit("grandchildInt", &grandchild_int))

TEST(RTCStatsTest, RTCStatsAndAttributes) {
  RTCTestStats stats("testId", Timestamp::Micros(42));
  EXPECT_EQ(stats.id(), "testId");
  EXPECT_EQ(stats.timestamp().us(), static_cast<int64_t>(42));
  std::vector<Attribute> attributes = stats.Attributes();
  EXPECT_EQ(attributes.size(), static_cast<size_t>(16));
  for (const auto& attribute : attributes) {
    EXPECT_FALSE(attribute.has_value());
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

  std::map<std::string, uint64_t> map_string_uint64{{"seven", 8}};
  std::map<std::string, double> map_string_double{{"nine", 10.0}};

  stats.m_sequence_bool = sequence_bool;
  stats.m_sequence_int32 = sequence_int32;
  stats.m_sequence_uint32 = sequence_uint32;
  EXPECT_FALSE(stats.m_sequence_int64.has_value());
  stats.m_sequence_int64 = sequence_int64;
  stats.m_sequence_uint64 = sequence_uint64;
  stats.m_sequence_double = sequence_double;
  stats.m_sequence_string = sequence_string;
  stats.m_map_string_uint64 = map_string_uint64;
  stats.m_map_string_double = map_string_double;
  for (const auto& attribute : attributes) {
    EXPECT_TRUE(attribute.has_value());
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
  EXPECT_EQ(*stats.m_map_string_uint64, map_string_uint64);
  EXPECT_EQ(*stats.m_map_string_double, map_string_double);

  int32_t numbers[] = {4, 8, 15, 16, 23, 42};
  std::vector<int32_t> numbers_sequence(&numbers[0], &numbers[6]);
  stats.m_sequence_int32->clear();
  stats.m_sequence_int32->insert(stats.m_sequence_int32->end(),
                                 numbers_sequence.begin(),
                                 numbers_sequence.end());
  EXPECT_EQ(*stats.m_sequence_int32, numbers_sequence);
}

TEST(RTCStatsTest, EqualityOperator) {
  RTCTestStats empty_stats("testId", Timestamp::Micros(123));
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
  stats_with_all_values.m_map_string_uint64 = std::map<std::string, uint64_t>();
  stats_with_all_values.m_map_string_double = std::map<std::string, double>();
  EXPECT_NE(stats_with_all_values, empty_stats);
  EXPECT_EQ(stats_with_all_values, stats_with_all_values);
  EXPECT_NE(stats_with_all_values.GetAttribute(stats_with_all_values.m_int32),
            stats_with_all_values.GetAttribute(stats_with_all_values.m_uint32));

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
  (*one_member_different[13].m_map_string_uint64)["321"] = 321;
  (*one_member_different[13].m_map_string_double)["321"] = 321.0;
  for (size_t i = 0; i < 14; ++i) {
    EXPECT_NE(stats_with_all_values, one_member_different[i]);
  }

  RTCTestStats empty_stats_different_id("testId2", Timestamp::Micros(123));
  EXPECT_NE(empty_stats, empty_stats_different_id);
  RTCTestStats empty_stats_different_timestamp("testId",
                                               Timestamp::Micros(321));
  EXPECT_EQ(empty_stats, empty_stats_different_timestamp);

  RTCChildStats child("childId", Timestamp::Micros(42));
  RTCGrandChildStats grandchild("grandchildId", Timestamp::Micros(42));
  EXPECT_NE(child, grandchild);

  RTCChildStats stats_with_defined_member("leId", Timestamp::Micros(0));
  stats_with_defined_member.child_int = 0;
  RTCChildStats stats_with_undefined_member("leId", Timestamp::Micros(0));
  EXPECT_NE(stats_with_defined_member, stats_with_undefined_member);
  EXPECT_NE(stats_with_undefined_member, stats_with_defined_member);
}

TEST(RTCStatsTest, RTCStatsGrandChild) {
  RTCGrandChildStats stats("grandchild", Timestamp::Micros(0.0));
  stats.child_int = 1;
  stats.grandchild_int = 2;
  int32_t sum = 0;
  for (const auto& attribute : stats.Attributes()) {
    sum += attribute.get<int32_t>();
  }
  EXPECT_EQ(sum, static_cast<int32_t>(3));

  std::unique_ptr<RTCStats> copy_ptr = stats.copy();
  const RTCGrandChildStats& copy = copy_ptr->cast_to<RTCGrandChildStats>();
  EXPECT_EQ(*copy.child_int, *stats.child_int);
  EXPECT_EQ(*copy.grandchild_int, *stats.grandchild_int);
}

TEST(RTCStatsTest, RTCStatsPrintsValidJson) {
  std::string id = "statsId";
  int timestamp = 42;
  bool m_bool = true;
  int m_int32 = 123;
  int64_t m_int64 = 1234567890123456499L;
  double m_double = 123.4567890123456499;
  std::string m_string = "123";

  std::vector<bool> sequence_bool;
  std::vector<int32_t> sequence_int32;
  sequence_int32.push_back(static_cast<int32_t>(1));
  std::vector<int64_t> sequence_int64;
  sequence_int64.push_back(static_cast<int64_t>(-1234567890123456499L));
  sequence_int64.push_back(static_cast<int64_t>(1));
  sequence_int64.push_back(static_cast<int64_t>(1234567890123456499L));
  std::vector<double> sequence_double;
  sequence_double.push_back(123.4567890123456499);
  sequence_double.push_back(1234567890123.456499);
  std::vector<std::string> sequence_string;
  sequence_string.push_back(std::string("four"));

  std::map<std::string, uint64_t> map_string_uint64{
      {"long", static_cast<uint64_t>(1234567890123456499L)}};
  std::map<std::string, double> map_string_double{
      {"three", 123.4567890123456499}, {"thirteen", 123.4567890123456499}};

  RTCTestStats stats(id, Timestamp::Micros(timestamp));
  stats.m_bool = m_bool;
  stats.m_int32 = m_int32;
  stats.m_int64 = m_int64;
  stats.m_double = m_double;
  stats.m_string = m_string;
  stats.m_sequence_bool = sequence_bool;
  stats.m_sequence_int32 = sequence_int32;
  stats.m_sequence_int64 = sequence_int64;
  stats.m_sequence_double = sequence_double;
  stats.m_sequence_string = sequence_string;
  stats.m_map_string_uint64 = map_string_uint64;
  stats.m_map_string_double = map_string_double;
  std::string json_stats = stats.ToJson();

  Json::CharReaderBuilder builder;
  Json::Value json_output;
  std::unique_ptr<Json::CharReader> json_reader(builder.newCharReader());
  EXPECT_TRUE(json_reader->parse(json_stats.c_str(),
                                 json_stats.c_str() + json_stats.size(),
                                 &json_output, nullptr));

  EXPECT_TRUE(GetStringFromJsonObject(json_output, "id", &id));
  EXPECT_TRUE(GetIntFromJsonObject(json_output, "timestamp", &timestamp));
  EXPECT_TRUE(GetBoolFromJsonObject(json_output, "mBool", &m_bool));
  EXPECT_TRUE(GetIntFromJsonObject(json_output, "mInt32", &m_int32));
  EXPECT_TRUE(GetDoubleFromJsonObject(json_output, "mDouble", &m_double));
  EXPECT_TRUE(GetStringFromJsonObject(json_output, "mString", &m_string));

  Json::Value json_array;

  EXPECT_TRUE(
      GetValueFromJsonObject(json_output, "mSequenceBool", &json_array));
  EXPECT_TRUE(JsonArrayToBoolVector(json_array, &sequence_bool));

  EXPECT_TRUE(
      GetValueFromJsonObject(json_output, "mSequenceInt32", &json_array));
  EXPECT_TRUE(JsonArrayToIntVector(json_array, &sequence_int32));

  EXPECT_TRUE(
      GetValueFromJsonObject(json_output, "mSequenceDouble", &json_array));
  EXPECT_TRUE(JsonArrayToDoubleVector(json_array, &sequence_double));

  EXPECT_TRUE(
      GetValueFromJsonObject(json_output, "mSequenceString", &json_array));
  EXPECT_TRUE(JsonArrayToStringVector(json_array, &sequence_string));

  Json::Value json_map;
  EXPECT_TRUE(
      GetValueFromJsonObject(json_output, "mMapStringDouble", &json_map));
  for (const auto& entry : map_string_double) {
    double double_output = 0.0;
    EXPECT_TRUE(GetDoubleFromJsonObject(json_map, entry.first, &double_output));
    EXPECT_NEAR(double_output, entry.second, GetExpectedError(entry.second));
  }

  EXPECT_EQ(id, stats.id());
  EXPECT_EQ(timestamp, stats.timestamp().us());
  EXPECT_EQ(m_bool, *stats.m_bool);
  EXPECT_EQ(m_int32, *stats.m_int32);
  EXPECT_EQ(m_string, *stats.m_string);
  EXPECT_EQ(sequence_bool, *stats.m_sequence_bool);
  EXPECT_EQ(sequence_int32, *stats.m_sequence_int32);
  EXPECT_EQ(sequence_string, *stats.m_sequence_string);
  EXPECT_EQ(map_string_double, *stats.m_map_string_double);

  EXPECT_NEAR(m_double, *stats.m_double, GetExpectedError(*stats.m_double));

  EXPECT_EQ(sequence_double.size(), stats.m_sequence_double->size());
  for (size_t i = 0; i < stats.m_sequence_double->size(); ++i) {
    EXPECT_NEAR(sequence_double[i], stats.m_sequence_double->at(i),
                GetExpectedError(stats.m_sequence_double->at(i)));
  }

  EXPECT_EQ(map_string_double.size(), stats.m_map_string_double->size());
  for (const auto& entry : map_string_double) {
    auto it = stats.m_map_string_double->find(entry.first);
    EXPECT_NE(it, stats.m_map_string_double->end());
    EXPECT_NEAR(entry.second, it->second, GetExpectedError(it->second));
  }

  // We read mInt64 as double since JSON stores all numbers as doubles, so there
  // is not enough precision to represent large numbers.
  double m_int64_as_double;
  std::vector<double> sequence_int64_as_double;

  EXPECT_TRUE(
      GetDoubleFromJsonObject(json_output, "mInt64", &m_int64_as_double));

  EXPECT_TRUE(
      GetValueFromJsonObject(json_output, "mSequenceInt64", &json_array));
  EXPECT_TRUE(JsonArrayToDoubleVector(json_array, &sequence_int64_as_double));

  double stats_m_int64_as_double = static_cast<double>(*stats.m_int64);
  EXPECT_NEAR(m_int64_as_double, stats_m_int64_as_double,
              GetExpectedError(stats_m_int64_as_double));

  EXPECT_EQ(sequence_int64_as_double.size(), stats.m_sequence_int64->size());
  for (size_t i = 0; i < stats.m_sequence_int64->size(); ++i) {
    const double stats_value_as_double =
        static_cast<double>((*stats.m_sequence_int64)[i]);
    EXPECT_NEAR(sequence_int64_as_double[i], stats_value_as_double,
                GetExpectedError(stats_value_as_double));
  }

  // Similarly, read Uint64 as double
  EXPECT_TRUE(
      GetValueFromJsonObject(json_output, "mMapStringUint64", &json_map));
  for (const auto& entry : map_string_uint64) {
    const double stats_value_as_double =
        static_cast<double>((*stats.m_map_string_uint64)[entry.first]);
    double double_output = 0.0;
    EXPECT_TRUE(GetDoubleFromJsonObject(json_map, entry.first, &double_output));
    EXPECT_NEAR(double_output, stats_value_as_double,
                GetExpectedError(stats_value_as_double));
  }

  // Neither stats.m_uint32 nor stats.m_uint64 are defined, so "mUint64" and
  // "mUint32" should not be part of the generated JSON object.
  int m_uint32;
  int m_uint64;
  EXPECT_FALSE(stats.m_uint32.has_value());
  EXPECT_FALSE(stats.m_uint64.has_value());
  EXPECT_FALSE(GetIntFromJsonObject(json_output, "mUint32", &m_uint32));
  EXPECT_FALSE(GetIntFromJsonObject(json_output, "mUint64", &m_uint64));

  std::cout << stats.ToJson() << std::endl;
}

TEST(RTCStatsTest, IsSequence) {
  RTCTestStats stats("statsId", Timestamp::Micros(42));
  EXPECT_FALSE(stats.GetAttribute(stats.m_bool).is_sequence());
  EXPECT_FALSE(stats.GetAttribute(stats.m_int32).is_sequence());
  EXPECT_FALSE(stats.GetAttribute(stats.m_uint32).is_sequence());
  EXPECT_FALSE(stats.GetAttribute(stats.m_int64).is_sequence());
  EXPECT_FALSE(stats.GetAttribute(stats.m_uint64).is_sequence());
  EXPECT_FALSE(stats.GetAttribute(stats.m_double).is_sequence());
  EXPECT_FALSE(stats.GetAttribute(stats.m_string).is_sequence());
  EXPECT_TRUE(stats.GetAttribute(stats.m_sequence_bool).is_sequence());
  EXPECT_TRUE(stats.GetAttribute(stats.m_sequence_int32).is_sequence());
  EXPECT_TRUE(stats.GetAttribute(stats.m_sequence_uint32).is_sequence());
  EXPECT_TRUE(stats.GetAttribute(stats.m_sequence_int64).is_sequence());
  EXPECT_TRUE(stats.GetAttribute(stats.m_sequence_uint64).is_sequence());
  EXPECT_TRUE(stats.GetAttribute(stats.m_sequence_double).is_sequence());
  EXPECT_TRUE(stats.GetAttribute(stats.m_sequence_string).is_sequence());
  EXPECT_FALSE(stats.GetAttribute(stats.m_map_string_uint64).is_sequence());
  EXPECT_FALSE(stats.GetAttribute(stats.m_map_string_double).is_sequence());
}

TEST(RTCStatsTest, IsString) {
  RTCTestStats stats("statsId", Timestamp::Micros(42));
  EXPECT_TRUE(stats.GetAttribute(stats.m_string).is_string());
  EXPECT_FALSE(stats.GetAttribute(stats.m_bool).is_string());
  EXPECT_FALSE(stats.GetAttribute(stats.m_int32).is_string());
  EXPECT_FALSE(stats.GetAttribute(stats.m_uint32).is_string());
  EXPECT_FALSE(stats.GetAttribute(stats.m_int64).is_string());
  EXPECT_FALSE(stats.GetAttribute(stats.m_uint64).is_string());
  EXPECT_FALSE(stats.GetAttribute(stats.m_double).is_string());
  EXPECT_FALSE(stats.GetAttribute(stats.m_sequence_bool).is_string());
  EXPECT_FALSE(stats.GetAttribute(stats.m_sequence_int32).is_string());
  EXPECT_FALSE(stats.GetAttribute(stats.m_sequence_uint32).is_string());
  EXPECT_FALSE(stats.GetAttribute(stats.m_sequence_int64).is_string());
  EXPECT_FALSE(stats.GetAttribute(stats.m_sequence_uint64).is_string());
  EXPECT_FALSE(stats.GetAttribute(stats.m_sequence_double).is_string());
  EXPECT_FALSE(stats.GetAttribute(stats.m_sequence_string).is_string());
  EXPECT_FALSE(stats.GetAttribute(stats.m_map_string_uint64).is_string());
  EXPECT_FALSE(stats.GetAttribute(stats.m_map_string_double).is_string());
}

TEST(RTCStatsTest, AttributeToString) {
  RTCTestStats stats("statsId", Timestamp::Micros(42));
  stats.m_bool = true;
  EXPECT_EQ("true", stats.GetAttribute(stats.m_bool).ToString());

  stats.m_string = "foo";
  EXPECT_EQ("foo", stats.GetAttribute(stats.m_string).ToString());
  stats.m_int32 = -32;
  EXPECT_EQ("-32", stats.GetAttribute(stats.m_int32).ToString());
  stats.m_uint32 = 32;
  EXPECT_EQ("32", stats.GetAttribute(stats.m_uint32).ToString());
  stats.m_int64 = -64;
  EXPECT_EQ("-64", stats.GetAttribute(stats.m_int64).ToString());
  stats.m_uint64 = 64;
  EXPECT_EQ("64", stats.GetAttribute(stats.m_uint64).ToString());
  stats.m_double = 0.5;
  EXPECT_EQ("0.5", stats.GetAttribute(stats.m_double).ToString());
  stats.m_sequence_bool = {true, false};
  EXPECT_EQ("[true,false]",
            stats.GetAttribute(stats.m_sequence_bool).ToString());
  stats.m_sequence_int32 = {-32, 32};
  EXPECT_EQ("[-32,32]", stats.GetAttribute(stats.m_sequence_int32).ToString());
  stats.m_sequence_uint32 = {64, 32};
  EXPECT_EQ("[64,32]", stats.GetAttribute(stats.m_sequence_uint32).ToString());
  stats.m_sequence_int64 = {-64, 32};
  EXPECT_EQ("[-64,32]", stats.GetAttribute(stats.m_sequence_int64).ToString());
  stats.m_sequence_uint64 = {16, 32};
  EXPECT_EQ("[16,32]", stats.GetAttribute(stats.m_sequence_uint64).ToString());
  stats.m_sequence_double = {0.5, 0.25};
  EXPECT_EQ("[0.5,0.25]",
            stats.GetAttribute(stats.m_sequence_double).ToString());
  stats.m_sequence_string = {"foo", "bar"};
  EXPECT_EQ("[\"foo\",\"bar\"]",
            stats.GetAttribute(stats.m_sequence_string).ToString());
  stats.m_map_string_uint64 = std::map<std::string, uint64_t>();
  stats.m_map_string_uint64->emplace("foo", 32);
  stats.m_map_string_uint64->emplace("bar", 64);
  EXPECT_EQ("{\"bar\":64,\"foo\":32}",
            stats.GetAttribute(stats.m_map_string_uint64).ToString());
  stats.m_map_string_double = std::map<std::string, double>();
  stats.m_map_string_double->emplace("foo", 0.5);
  stats.m_map_string_double->emplace("bar", 0.25);
  EXPECT_EQ("{\"bar\":0.25,\"foo\":0.5}",
            stats.GetAttribute(stats.m_map_string_double).ToString());
}

TEST(RTCStatsTest, SetTimestamp) {
  RTCTestStats test_stats("testId", Timestamp::Micros(123));
  test_stats.set_timestamp(Timestamp::Micros(321));
  EXPECT_EQ(test_stats.timestamp(), Timestamp::Micros(321));
}

// Death tests.
// Disabled on Android because death tests misbehave on Android, see
// base/test/gtest_util.h.
#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

TEST(RTCStatsDeathTest, ValueOfUndefinedMember) {
  RTCTestStats stats("testId", Timestamp::Micros(0));
  EXPECT_FALSE(stats.m_int32.has_value());
  EXPECT_DEATH((void)*stats.m_int32, "");
}

TEST(RTCStatsDeathTest, InvalidCasting) {
  RTCGrandChildStats stats("grandchild", Timestamp::Micros(0.0));
  EXPECT_DEATH(stats.cast_to<RTCChildStats>(), "");
}

#endif  // RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

}  // namespace webrtc
