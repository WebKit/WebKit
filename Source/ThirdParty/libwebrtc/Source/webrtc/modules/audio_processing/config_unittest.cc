/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/include/config.h"

#include "test/gtest.h"

namespace webrtc {
namespace {

struct MyExperiment {
  static const ConfigOptionID identifier = ConfigOptionID::kMyExperimentForTest;
  static const int kDefaultFactor;
  static const int kDefaultOffset;

  MyExperiment()
    : factor(kDefaultFactor), offset(kDefaultOffset) {}

  MyExperiment(int factor, int offset)
    : factor(factor), offset(offset) {}

  int factor;
  int offset;
};

const int MyExperiment::kDefaultFactor = 1;
const int MyExperiment::kDefaultOffset = 2;

TEST(Config, ReturnsDefaultInstanceIfNotConfigured) {
  Config config;
  const MyExperiment& my_exp = config.Get<MyExperiment>();
  EXPECT_EQ(MyExperiment::kDefaultFactor, my_exp.factor);
  EXPECT_EQ(MyExperiment::kDefaultOffset, my_exp.offset);
}

TEST(Config, ReturnOptionWhenSet) {
  Config config;
  config.Set<MyExperiment>(new MyExperiment(5, 1));
  const MyExperiment& my_exp = config.Get<MyExperiment>();
  EXPECT_EQ(5, my_exp.factor);
  EXPECT_EQ(1, my_exp.offset);
}

TEST(Config, SetNullSetsTheOptionBackToDefault) {
  Config config;
  config.Set<MyExperiment>(new MyExperiment(5, 1));
  config.Set<MyExperiment>(NULL);
  const MyExperiment& my_exp = config.Get<MyExperiment>();
  EXPECT_EQ(MyExperiment::kDefaultFactor, my_exp.factor);
  EXPECT_EQ(MyExperiment::kDefaultOffset, my_exp.offset);
}

struct Algo1_CostFunction {
  static const ConfigOptionID identifier =
      ConfigOptionID::kAlgo1CostFunctionForTest;
  Algo1_CostFunction() {}

  virtual int cost(int x) const {
    return x;
  }

  virtual ~Algo1_CostFunction() {}
};

struct SqrCost : Algo1_CostFunction {
  virtual int cost(int x) const {
    return x*x;
  }
};

TEST(Config, SupportsPolymorphism) {
  Config config;
  config.Set<Algo1_CostFunction>(new SqrCost());
  EXPECT_EQ(25, config.Get<Algo1_CostFunction>().cost(5));
}
}  // namespace
}  // namespace webrtc
