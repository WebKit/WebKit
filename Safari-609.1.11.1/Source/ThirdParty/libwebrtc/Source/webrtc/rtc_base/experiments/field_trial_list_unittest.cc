/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/field_trial_list.h"

#include "rtc_base/gunit.h"
#include "test/gmock.h"

using testing::ElementsAre;
using testing::IsEmpty;

namespace webrtc {

struct Garment {
  int price = 0;
  TimeDelta age = TimeDelta::Zero();

  // Only needed for testing.
  Garment() = default;
  Garment(int p, TimeDelta a) : price(p), age(a) {}

  bool operator==(const Garment& other) const {
    return price == other.price && age == other.age;
  }
};

TEST(FieldTrialListTest, ParsesListParameter) {
  FieldTrialList<int> my_list("l", {5});
  EXPECT_THAT(my_list.Get(), ElementsAre(5));
  // If one element is invalid the list is unchanged.
  ParseFieldTrial({&my_list}, "l:1|2|hat");
  EXPECT_THAT(my_list.Get(), ElementsAre(5));
  ParseFieldTrial({&my_list}, "l");
  EXPECT_THAT(my_list.Get(), IsEmpty());
  ParseFieldTrial({&my_list}, "l:1|2|3");
  EXPECT_THAT(my_list.Get(), ElementsAre(1, 2, 3));
  ParseFieldTrial({&my_list}, "l:-1");
  EXPECT_THAT(my_list.Get(), ElementsAre(-1));
}

// Normal usage.
TEST(FieldTrialListTest, ParsesStructList) {
  FieldTrialStructList<Garment> my_list(
      {FieldTrialStructMember("age", [](Garment* g) { return &g->age; }),
       FieldTrialStructMember("price", [](Garment* g) { return &g->price; })},
      {{1, TimeDelta::seconds(100)}, {2, TimeDelta::PlusInfinity()}});

  ParseFieldTrial({&my_list},
                  "age:inf|10s|80ms,"
                  "price:10|20|30,"
                  "other_param:asdf");

  ASSERT_THAT(my_list.Get(), ElementsAre(Garment{10, TimeDelta::PlusInfinity()},
                                         Garment{20, TimeDelta::seconds(10)},
                                         Garment{30, TimeDelta::ms(80)}));
}

// One FieldTrialList has the wrong length, so we use the user-provided default
// list.
TEST(FieldTrialListTest, StructListKeepsDefaultWithMismatchingLength) {
  FieldTrialStructList<Garment> my_list(
      {FieldTrialStructMember("wrong_length",
                              [](Garment* g) { return &g->age; }),
       FieldTrialStructMember("price", [](Garment* g) { return &g->price; })},
      {{1, TimeDelta::seconds(100)}, {2, TimeDelta::PlusInfinity()}});

  ParseFieldTrial({&my_list},
                  "wrong_length:3|2|4|3,"
                  "garment:hat|hat|crown,"
                  "price:10|20|30");

  ASSERT_THAT(my_list.Get(),
              ElementsAre(Garment{1, TimeDelta::seconds(100)},
                          Garment{2, TimeDelta::PlusInfinity()}));
}

// One list is missing. We set the values we're given, and the others remain
// as whatever the Garment default constructor set them to.
TEST(FieldTrialListTest, StructListUsesDefaultForMissingList) {
  FieldTrialStructList<Garment> my_list(
      {FieldTrialStructMember("age", [](Garment* g) { return &g->age; }),
       FieldTrialStructMember("price", [](Garment* g) { return &g->price; })},
      {{1, TimeDelta::seconds(100)}, {2, TimeDelta::PlusInfinity()}});

  ParseFieldTrial({&my_list}, "price:10|20|30");

  ASSERT_THAT(my_list.Get(), ElementsAre(Garment{10, TimeDelta::Zero()},
                                         Garment{20, TimeDelta::Zero()},
                                         Garment{30, TimeDelta::Zero()}));
}

// The user haven't provided values for any lists, so we use the default list.
TEST(FieldTrialListTest, StructListUsesDefaultListWithoutValues) {
  FieldTrialStructList<Garment> my_list(
      {FieldTrialStructMember("age", [](Garment* g) { return &g->age; }),
       FieldTrialStructMember("price", [](Garment* g) { return &g->price; })},
      {{1, TimeDelta::seconds(100)}, {2, TimeDelta::PlusInfinity()}});

  ParseFieldTrial({&my_list}, "");

  ASSERT_THAT(my_list.Get(),
              ElementsAre(Garment{1, TimeDelta::seconds(100)},
                          Garment{2, TimeDelta::PlusInfinity()}));
}

// Some lists are provided and all are empty, so we return a empty list.
TEST(FieldTrialListTest, StructListHandlesEmptyLists) {
  FieldTrialStructList<Garment> my_list(
      {FieldTrialStructMember("age", [](Garment* g) { return &g->age; }),
       FieldTrialStructMember("price", [](Garment* g) { return &g->price; })},
      {{1, TimeDelta::seconds(100)}, {2, TimeDelta::PlusInfinity()}});

  ParseFieldTrial({&my_list}, "age,price");

  ASSERT_EQ(my_list.Get().size(), 0u);
}

}  // namespace webrtc
