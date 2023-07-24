/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/experiments/struct_parameters_parser.h"

#include "rtc_base/gunit.h"

namespace webrtc {
namespace {
struct DummyConfig {
  bool enabled = false;
  double factor = 0.5;
  int retries = 5;
  unsigned size = 3;
  bool ping = 0;
  absl::optional<TimeDelta> duration;
  absl::optional<TimeDelta> latency = TimeDelta::Millis(100);
  std::unique_ptr<StructParametersParser> Parser();
};

std::unique_ptr<StructParametersParser> DummyConfig::Parser() {
  // The empty comments ensures that each pair is on a separate line.
  return StructParametersParser::Create("e", &enabled,   //
                                        "f", &factor,    //
                                        "r", &retries,   //
                                        "s", &size,      //
                                        "p", &ping,      //
                                        "d", &duration,  //
                                        "l", &latency);
}
}  // namespace

TEST(StructParametersParserTest, ParsesValidParameters) {
  DummyConfig exp;
  exp.Parser()->Parse("e:1,f:-1.7,r:2,s:7,p:1,d:8,l:,");
  EXPECT_TRUE(exp.enabled);
  EXPECT_EQ(exp.factor, -1.7);
  EXPECT_EQ(exp.retries, 2);
  EXPECT_EQ(exp.size, 7u);
  EXPECT_EQ(exp.ping, true);
  EXPECT_EQ(exp.duration.value().ms(), 8);
  EXPECT_FALSE(exp.latency);
}

TEST(StructParametersParserTest, UsesDefaults) {
  DummyConfig exp;
  exp.Parser()->Parse("");
  EXPECT_FALSE(exp.enabled);
  EXPECT_EQ(exp.factor, 0.5);
  EXPECT_EQ(exp.retries, 5);
  EXPECT_EQ(exp.size, 3u);
  EXPECT_EQ(exp.ping, false);
}

TEST(StructParametersParserTest, EncodeAll) {
  DummyConfig exp;
  auto encoded = exp.Parser()->Encode();
  // All parameters are encoded.
  EXPECT_EQ(encoded, "e:false,f:0.5,r:5,s:3,p:false,d:,l:100 ms");
}

}  // namespace webrtc
