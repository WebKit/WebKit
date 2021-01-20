/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/unique_id_generator.h"

#include <string>
#include <vector>

#include "absl/algorithm/container.h"
#include "api/array_view.h"
#include "rtc_base/gunit.h"
#include "rtc_base/helpers.h"
#include "test/gmock.h"

using ::testing::IsEmpty;
using ::testing::Test;

namespace rtc {

template <typename Generator>
class UniqueIdGeneratorTest : public Test {};

using test_types = ::testing::Types<UniqueNumberGenerator<uint8_t>,
                                    UniqueNumberGenerator<uint16_t>,
                                    UniqueNumberGenerator<uint32_t>,
                                    UniqueNumberGenerator<int>,
                                    UniqueRandomIdGenerator,
                                    UniqueStringGenerator>;

TYPED_TEST_SUITE(UniqueIdGeneratorTest, test_types);

TYPED_TEST(UniqueIdGeneratorTest, ElementsDoNotRepeat) {
  typedef TypeParam Generator;
  const size_t num_elements = 255;
  Generator generator;
  std::vector<typename Generator::value_type> values;
  for (size_t i = 0; i < num_elements; i++) {
    values.push_back(generator());
  }

  EXPECT_EQ(num_elements, values.size());
  // Use a set to check uniqueness.
  std::set<typename Generator::value_type> set(values.begin(), values.end());
  EXPECT_EQ(values.size(), set.size()) << "Returned values were not unique.";
}

TYPED_TEST(UniqueIdGeneratorTest, KnownElementsAreNotGenerated) {
  typedef TypeParam Generator;
  const size_t num_elements = 100;
  rtc::InitRandom(0);
  Generator generator1;
  std::vector<typename Generator::value_type> known_values;
  for (size_t i = 0; i < num_elements; i++) {
    known_values.push_back(generator1());
  }
  EXPECT_EQ(num_elements, known_values.size());

  rtc::InitRandom(0);
  Generator generator2(known_values);

  std::vector<typename Generator::value_type> values;
  for (size_t i = 0; i < num_elements; i++) {
    values.push_back(generator2());
  }
  EXPECT_THAT(values, ::testing::SizeIs(num_elements));
  absl::c_sort(values);
  absl::c_sort(known_values);
  std::vector<typename Generator::value_type> intersection;
  absl::c_set_intersection(values, known_values,
                           std::back_inserter(intersection));
  EXPECT_THAT(intersection, IsEmpty());
}

TYPED_TEST(UniqueIdGeneratorTest, AddedElementsAreNotGenerated) {
  typedef TypeParam Generator;
  const size_t num_elements = 100;
  rtc::InitRandom(0);
  Generator generator1;
  std::vector<typename Generator::value_type> known_values;
  for (size_t i = 0; i < num_elements; i++) {
    known_values.push_back(generator1());
  }
  EXPECT_EQ(num_elements, known_values.size());

  rtc::InitRandom(0);
  Generator generator2;

  for (const typename Generator::value_type& value : known_values) {
    generator2.AddKnownId(value);
  }

  std::vector<typename Generator::value_type> values;
  for (size_t i = 0; i < num_elements; i++) {
    values.push_back(generator2());
  }
  EXPECT_THAT(values, ::testing::SizeIs(num_elements));
  absl::c_sort(values);
  absl::c_sort(known_values);
  std::vector<typename Generator::value_type> intersection;
  absl::c_set_intersection(values, known_values,
                           std::back_inserter(intersection));
  EXPECT_THAT(intersection, IsEmpty());
}

TYPED_TEST(UniqueIdGeneratorTest, AddKnownIdOnNewIdReturnsTrue) {
  typedef TypeParam Generator;

  rtc::InitRandom(0);
  Generator generator1;
  const typename Generator::value_type id = generator1();

  rtc::InitRandom(0);
  Generator generator2;
  EXPECT_TRUE(generator2.AddKnownId(id));
}

TYPED_TEST(UniqueIdGeneratorTest, AddKnownIdCalledAgainForSameIdReturnsFalse) {
  typedef TypeParam Generator;

  rtc::InitRandom(0);
  Generator generator1;
  const typename Generator::value_type id = generator1();

  rtc::InitRandom(0);
  Generator generator2;
  ASSERT_TRUE(generator2.AddKnownId(id));
  EXPECT_FALSE(generator2.AddKnownId(id));
}

TYPED_TEST(UniqueIdGeneratorTest,
           AddKnownIdOnIdProvidedAsKnownToCtorReturnsFalse) {
  typedef TypeParam Generator;

  rtc::InitRandom(0);
  Generator generator1;
  const typename Generator::value_type id = generator1();
  std::vector<typename Generator::value_type> known_values = {id};

  rtc::InitRandom(0);
  Generator generator2(known_values);
  EXPECT_FALSE(generator2.AddKnownId(id));
}

}  // namespace rtc
