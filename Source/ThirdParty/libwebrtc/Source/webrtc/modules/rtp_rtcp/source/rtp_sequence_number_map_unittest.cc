/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_sequence_number_map.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <tuple>
#include <vector>

#include "absl/memory/memory.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/sequence_number_util.h"
#include "rtc_base/random.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
using Info = RtpSequenceNumberMap::Info;

constexpr uint16_t kUint16Max = std::numeric_limits<uint16_t>::max();
constexpr size_t kMaxPossibleMaxEntries = 1 << 15;

// Just a named pair.
struct Association final {
  Association(uint16_t sequence_number, Info info)
      : sequence_number(sequence_number), info(info) {}

  uint16_t sequence_number;
  Info info;
};

class RtpSequenceNumberMapTest : public ::testing::Test {
 protected:
  RtpSequenceNumberMapTest() : random_(1983) {}
  ~RtpSequenceNumberMapTest() override = default;

  Association CreateAssociation(uint16_t sequence_number, uint32_t timestamp) {
    return Association(sequence_number,
                       {timestamp, random_.Rand<bool>(), random_.Rand<bool>()});
  }

  void VerifyAssociations(const RtpSequenceNumberMap& uut,
                          const std::vector<Association>& associations) {
    return VerifyAssociations(uut, associations.begin(), associations.end());
  }

  void VerifyAssociations(
      const RtpSequenceNumberMap& uut,
      std::vector<Association>::const_iterator associations_begin,
      std::vector<Association>::const_iterator associations_end) {
    RTC_DCHECK(associations_begin < associations_end);
    ASSERT_EQ(static_cast<size_t>(associations_end - associations_begin),
              uut.AssociationCountForTesting());
    for (auto association = associations_begin; association != associations_end;
         ++association) {
      EXPECT_EQ(uut.Get(association->sequence_number), association->info);
    }
  }

  // Allows several variations of the same test; definition next to the tests.
  void GetObsoleteSequenceNumberReturnsNullOptMultipleEntriesObsoleted(
      bool with_wrap_around,
      bool last_element_kept);

  // Allows several variations of the same test; definition next to the tests.
  void RepeatedSequenceNumberInvalidatesAll(size_t index_of_repeated);

  // Allows several variations of the same test; definition next to the tests.
  void MaxEntriesReachedAtSameTimeAsObsoletionOfItem(size_t max_entries,
                                                     size_t obsoleted_count);

  Random random_;
};

class RtpSequenceNumberMapTestWithParams
    : public RtpSequenceNumberMapTest,
      public ::testing::WithParamInterface<std::tuple<size_t, uint16_t>> {
 protected:
  RtpSequenceNumberMapTestWithParams() = default;
  ~RtpSequenceNumberMapTestWithParams() override = default;

  std::vector<Association> ProduceRandomAssociationSequence(
      size_t association_count,
      uint16_t first_sequence_number,
      bool allow_obsoletion) {
    std::vector<Association> associations;
    associations.reserve(association_count);

    if (association_count == 0) {
      return associations;
    }

    associations.emplace_back(
        first_sequence_number,
        Info(0, random_.Rand<bool>(), random_.Rand<bool>()));

    for (size_t i = 1; i < association_count; ++i) {
      const uint16_t sequence_number =
          associations[i - 1].sequence_number + random_.Rand(1, 100);
      RTC_DCHECK(allow_obsoletion ||
                 AheadOf(sequence_number, associations[0].sequence_number));

      const uint32_t timestamp =
          associations[i - 1].info.timestamp + random_.Rand(1, 10000);

      associations.emplace_back(
          sequence_number,
          Info(timestamp, random_.Rand<bool>(), random_.Rand<bool>()));
    }

    return associations;
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         RtpSequenceNumberMapTestWithParams,
                         ::testing::Combine(
                             // Association count.
                             ::testing::Values(1, 2, 100),
                             // First sequence number.
                             ::testing::Values(0,
                                               100,
                                               kUint16Max - 100,
                                               kUint16Max - 1,
                                               kUint16Max)));

TEST_F(RtpSequenceNumberMapTest, GetBeforeAssociationsRecordedReturnsNullOpt) {
  RtpSequenceNumberMap uut(kMaxPossibleMaxEntries);
  constexpr uint16_t kArbitrarySequenceNumber = 321;
  EXPECT_FALSE(uut.Get(kArbitrarySequenceNumber));
}

// Version #1 - any old unknown sequence number.
TEST_F(RtpSequenceNumberMapTest, GetUnknownSequenceNumberReturnsNullOpt1) {
  RtpSequenceNumberMap uut(kMaxPossibleMaxEntries);

  constexpr uint16_t kKnownSequenceNumber = 10;
  constexpr uint32_t kArbitrary = 987;
  uut.InsertPacket(kKnownSequenceNumber, {kArbitrary, false, false});

  constexpr uint16_t kUnknownSequenceNumber = kKnownSequenceNumber + 1;
  EXPECT_FALSE(uut.Get(kUnknownSequenceNumber));
}

// Version #2 - intentionally pick a value in the range of currently held
// values, so as to trigger lower_bound / upper_bound.
TEST_F(RtpSequenceNumberMapTest, GetUnknownSequenceNumberReturnsNullOpt2) {
  RtpSequenceNumberMap uut(kMaxPossibleMaxEntries);

  const std::vector<Association> setup = {CreateAssociation(1000, 500),  //
                                          CreateAssociation(1020, 501)};
  for (const Association& association : setup) {
    uut.InsertPacket(association.sequence_number, association.info);
  }

  EXPECT_FALSE(uut.Get(1001));
}

TEST_P(RtpSequenceNumberMapTestWithParams,
       GetKnownSequenceNumberReturnsCorrectValue) {
  RtpSequenceNumberMap uut(kMaxPossibleMaxEntries);

  const size_t association_count = std::get<0>(GetParam());
  const uint16_t first_sequence_number = std::get<1>(GetParam());

  const std::vector<Association> associations =
      ProduceRandomAssociationSequence(association_count, first_sequence_number,
                                       /*allow_obsoletion=*/false);

  for (const Association& association : associations) {
    uut.InsertPacket(association.sequence_number, association.info);
  }

  VerifyAssociations(uut, associations);
}

TEST_F(RtpSequenceNumberMapTest, InsertFrameOnSinglePacketFrame) {
  RtpSequenceNumberMap uut(kMaxPossibleMaxEntries);

  constexpr uint16_t kSequenceNumber = 888;
  constexpr uint32_t kTimestamp = 98765;
  uut.InsertFrame(kSequenceNumber, 1, kTimestamp);

  EXPECT_EQ(uut.Get(kSequenceNumber), Info(kTimestamp, true, true));
}

TEST_F(RtpSequenceNumberMapTest, InsertFrameOnMultiPacketFrameNoWrapAround) {
  RtpSequenceNumberMap uut(kMaxPossibleMaxEntries);

  constexpr uint16_t kFirstSequenceNumber = 0;
  constexpr uint32_t kTimestamp = 98765;
  uut.InsertFrame(kFirstSequenceNumber, 3, kTimestamp);

  EXPECT_EQ(uut.Get(kFirstSequenceNumber + 0), Info(kTimestamp, true, false));
  EXPECT_EQ(uut.Get(kFirstSequenceNumber + 1), Info(kTimestamp, false, false));
  EXPECT_EQ(uut.Get(kFirstSequenceNumber + 2), Info(kTimestamp, false, true));
}

TEST_F(RtpSequenceNumberMapTest, InsertFrameOnMultiPacketFrameWithWrapAround) {
  RtpSequenceNumberMap uut(kMaxPossibleMaxEntries);

  constexpr uint16_t kFirstSequenceNumber = kUint16Max;
  constexpr uint32_t kTimestamp = 98765;
  uut.InsertFrame(kFirstSequenceNumber, 3, kTimestamp);

// Suppress "truncation of constant value" warning; wrap-around is intended.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4309)
#endif
  EXPECT_EQ(uut.Get(static_cast<uint16_t>(kFirstSequenceNumber + 0u)),
            Info(kTimestamp, true, false));
  EXPECT_EQ(uut.Get(static_cast<uint16_t>(kFirstSequenceNumber + 1u)),
            Info(kTimestamp, false, false));
  EXPECT_EQ(uut.Get(static_cast<uint16_t>(kFirstSequenceNumber + 2u)),
            Info(kTimestamp, false, true));
#ifdef _MSC_VER
#pragma warning(pop)
#endif
}

TEST_F(RtpSequenceNumberMapTest,
       GetObsoleteSequenceNumberReturnsNullOptSingleValueObsoleted) {
  RtpSequenceNumberMap uut(kMaxPossibleMaxEntries);

  const std::vector<Association> associations = {
      CreateAssociation(0, 10),       //
      CreateAssociation(0x8000, 20),  //
      CreateAssociation(0x8001u, 30)};

  uut.InsertPacket(associations[0].sequence_number, associations[0].info);

  // First association not yet obsolete, and therefore remembered.
  RTC_DCHECK(AheadOf(associations[1].sequence_number,
                     associations[0].sequence_number));
  uut.InsertPacket(associations[1].sequence_number, associations[1].info);
  VerifyAssociations(uut, {associations[0], associations[1]});

  // Test focus - new entry obsoletes first entry.
  RTC_DCHECK(!AheadOf(associations[2].sequence_number,
                      associations[0].sequence_number));
  uut.InsertPacket(associations[2].sequence_number, associations[2].info);
  VerifyAssociations(uut, {associations[1], associations[2]});
}

void RtpSequenceNumberMapTest::
    GetObsoleteSequenceNumberReturnsNullOptMultipleEntriesObsoleted(
        bool with_wrap_around,
        bool last_element_kept) {
  RtpSequenceNumberMap uut(kMaxPossibleMaxEntries);

  std::vector<Association> associations;
  if (with_wrap_around) {
    associations = {CreateAssociation(kUint16Max - 1, 10),  //
                    CreateAssociation(kUint16Max, 20),      //
                    CreateAssociation(0, 30),               //
                    CreateAssociation(1, 40),               //
                    CreateAssociation(2, 50)};
  } else {
    associations = {CreateAssociation(1, 10),  //
                    CreateAssociation(2, 20),  //
                    CreateAssociation(3, 30),  //
                    CreateAssociation(4, 40),  //
                    CreateAssociation(5, 50)};
  }

  for (auto association : associations) {
    uut.InsertPacket(association.sequence_number, association.info);
  }
  VerifyAssociations(uut, associations);

  // Define a new association that will obsolete either all previous entries,
  // or all previous entries except for the last one, depending on the
  // parameter instantiation of this test.
  RTC_DCHECK_EQ(
      static_cast<uint16_t>(
          associations[associations.size() - 1].sequence_number),
      static_cast<uint16_t>(
          associations[associations.size() - 2].sequence_number + 1u));
  uint16_t new_sequence_number;
  if (last_element_kept) {
    new_sequence_number =
        associations[associations.size() - 1].sequence_number + 0x8000;
    RTC_DCHECK(AheadOf(new_sequence_number,
                       associations[associations.size() - 1].sequence_number));
  } else {
    new_sequence_number =
        associations[associations.size() - 1].sequence_number + 0x8001;
    RTC_DCHECK(!AheadOf(new_sequence_number,
                        associations[associations.size() - 1].sequence_number));
  }
  RTC_DCHECK(!AheadOf(new_sequence_number,
                      associations[associations.size() - 2].sequence_number));

  // Record the new association.
  const Association new_association =
      CreateAssociation(new_sequence_number, 60);
  uut.InsertPacket(new_association.sequence_number, new_association.info);

  // Make sure all obsoleted elements were removed.
  const size_t obsoleted_count =
      associations.size() - (last_element_kept ? 1 : 0);
  for (size_t i = 0; i < obsoleted_count; ++i) {
    EXPECT_FALSE(uut.Get(associations[i].sequence_number));
  }

  // Make sure the expected elements were not removed, and return the
  // expected value.
  if (last_element_kept) {
    EXPECT_EQ(uut.Get(associations.back().sequence_number),
              associations.back().info);
  }
  EXPECT_EQ(uut.Get(new_association.sequence_number), new_association.info);
}

TEST_F(RtpSequenceNumberMapTest,
       GetObsoleteSequenceNumberReturnsNullOptMultipleEntriesObsoleted0) {
  const bool with_wrap_around = false;
  const bool last_element_kept = false;
  GetObsoleteSequenceNumberReturnsNullOptMultipleEntriesObsoleted(
      with_wrap_around, last_element_kept);
}

TEST_F(RtpSequenceNumberMapTest,
       GetObsoleteSequenceNumberReturnsNullOptMultipleEntriesObsoleted1) {
  const bool with_wrap_around = true;
  const bool last_element_kept = false;
  GetObsoleteSequenceNumberReturnsNullOptMultipleEntriesObsoleted(
      with_wrap_around, last_element_kept);
}

TEST_F(RtpSequenceNumberMapTest,
       GetObsoleteSequenceNumberReturnsNullOptMultipleEntriesObsoleted2) {
  const bool with_wrap_around = false;
  const bool last_element_kept = true;
  GetObsoleteSequenceNumberReturnsNullOptMultipleEntriesObsoleted(
      with_wrap_around, last_element_kept);
}

TEST_F(RtpSequenceNumberMapTest,
       GetObsoleteSequenceNumberReturnsNullOptMultipleEntriesObsoleted3) {
  const bool with_wrap_around = true;
  const bool last_element_kept = true;
  GetObsoleteSequenceNumberReturnsNullOptMultipleEntriesObsoleted(
      with_wrap_around, last_element_kept);
}

void RtpSequenceNumberMapTest::RepeatedSequenceNumberInvalidatesAll(
    size_t index_of_repeated) {
  RtpSequenceNumberMap uut(kMaxPossibleMaxEntries);

  const std::vector<Association> setup = {CreateAssociation(100, 500),  //
                                          CreateAssociation(101, 501),  //
                                          CreateAssociation(102, 502)};
  RTC_DCHECK_LT(index_of_repeated, setup.size());
  for (const Association& association : setup) {
    uut.InsertPacket(association.sequence_number, association.info);
  }

  const Association new_association =
      CreateAssociation(setup[index_of_repeated].sequence_number, 503);
  uut.InsertPacket(new_association.sequence_number, new_association.info);

  // All entries from setup invalidated.
  // New entry valid and mapped to new value.
  for (size_t i = 0; i < setup.size(); ++i) {
    if (i == index_of_repeated) {
      EXPECT_EQ(uut.Get(new_association.sequence_number), new_association.info);
    } else {
      EXPECT_FALSE(uut.Get(setup[i].sequence_number));
    }
  }
}

TEST_F(RtpSequenceNumberMapTest,
       RepeatedSequenceNumberInvalidatesAllRepeatFirst) {
  RepeatedSequenceNumberInvalidatesAll(0);
}

TEST_F(RtpSequenceNumberMapTest,
       RepeatedSequenceNumberInvalidatesAllRepeatMiddle) {
  RepeatedSequenceNumberInvalidatesAll(1);
}

TEST_F(RtpSequenceNumberMapTest,
       RepeatedSequenceNumberInvalidatesAllRepeatLast) {
  RepeatedSequenceNumberInvalidatesAll(2);
}

TEST_F(RtpSequenceNumberMapTest,
       SequenceNumberInsideMemorizedRangeInvalidatesAll) {
  RtpSequenceNumberMap uut(kMaxPossibleMaxEntries);

  const std::vector<Association> setup = {CreateAssociation(1000, 500),  //
                                          CreateAssociation(1020, 501),  //
                                          CreateAssociation(1030, 502)};
  for (const Association& association : setup) {
    uut.InsertPacket(association.sequence_number, association.info);
  }

  const Association new_association = CreateAssociation(1010, 503);
  uut.InsertPacket(new_association.sequence_number, new_association.info);

  // All entries from setup invalidated.
  // New entry valid and mapped to new value.
  for (size_t i = 0; i < setup.size(); ++i) {
    EXPECT_FALSE(uut.Get(setup[i].sequence_number));
  }
  EXPECT_EQ(uut.Get(new_association.sequence_number), new_association.info);
}

TEST_F(RtpSequenceNumberMapTest, MaxEntriesObserved) {
  constexpr size_t kMaxEntries = 100;
  RtpSequenceNumberMap uut(kMaxEntries);

  std::vector<Association> associations;
  associations.reserve(kMaxEntries);
  uint32_t timestamp = 789;
  for (size_t i = 0; i < kMaxEntries; ++i) {
    associations.push_back(CreateAssociation(i, ++timestamp));
    uut.InsertPacket(associations[i].sequence_number, associations[i].info);
  }
  VerifyAssociations(uut, associations);  // Sanity.

  const Association new_association =
      CreateAssociation(kMaxEntries, ++timestamp);
  uut.InsertPacket(new_association.sequence_number, new_association.info);
  associations.push_back(new_association);

  // The +1 is for `new_association`.
  const size_t kExpectedAssociationCount = 3 * kMaxEntries / 4 + 1;
  const auto expected_begin =
      std::prev(associations.end(), kExpectedAssociationCount);
  VerifyAssociations(uut, expected_begin, associations.end());
}

void RtpSequenceNumberMapTest::MaxEntriesReachedAtSameTimeAsObsoletionOfItem(
    size_t max_entries,
    size_t obsoleted_count) {
  RtpSequenceNumberMap uut(max_entries);

  std::vector<Association> associations;
  associations.reserve(max_entries);
  uint32_t timestamp = 789;
  for (size_t i = 0; i < max_entries; ++i) {
    associations.push_back(CreateAssociation(i, ++timestamp));
    uut.InsertPacket(associations[i].sequence_number, associations[i].info);
  }
  VerifyAssociations(uut, associations);  // Sanity.

  const uint16_t new_association_sequence_number =
      static_cast<uint16_t>(obsoleted_count) + (1 << 15);
  const Association new_association =
      CreateAssociation(new_association_sequence_number, ++timestamp);
  uut.InsertPacket(new_association.sequence_number, new_association.info);
  associations.push_back(new_association);

  // The +1 is for `new_association`.
  const size_t kExpectedAssociationCount =
      std::min(3 * max_entries / 4, max_entries - obsoleted_count) + 1;
  const auto expected_begin =
      std::prev(associations.end(), kExpectedAssociationCount);
  VerifyAssociations(uut, expected_begin, associations.end());
}

// Version #1 - #(obsoleted entries) < #(entries after paring down below max).
TEST_F(RtpSequenceNumberMapTest,
       MaxEntriesReachedAtSameTimeAsObsoletionOfItem1) {
  constexpr size_t kMaxEntries = 100;
  constexpr size_t kObsoletionTarget = (kMaxEntries / 4) - 1;
  MaxEntriesReachedAtSameTimeAsObsoletionOfItem(kMaxEntries, kObsoletionTarget);
}

// Version #2 - #(obsoleted entries) == #(entries after paring down below max).
TEST_F(RtpSequenceNumberMapTest,
       MaxEntriesReachedAtSameTimeAsObsoletionOfItem2) {
  constexpr size_t kMaxEntries = 100;
  constexpr size_t kObsoletionTarget = kMaxEntries / 4;
  MaxEntriesReachedAtSameTimeAsObsoletionOfItem(kMaxEntries, kObsoletionTarget);
}

// Version #3 - #(obsoleted entries) > #(entries after paring down below max).
TEST_F(RtpSequenceNumberMapTest,
       MaxEntriesReachedAtSameTimeAsObsoletionOfItem3) {
  constexpr size_t kMaxEntries = 100;
  constexpr size_t kObsoletionTarget = (kMaxEntries / 4) + 1;
  MaxEntriesReachedAtSameTimeAsObsoletionOfItem(kMaxEntries, kObsoletionTarget);
}

}  // namespace
}  // namespace webrtc
