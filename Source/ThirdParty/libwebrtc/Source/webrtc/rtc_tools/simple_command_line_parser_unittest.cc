/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/simple_command_line_parser.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

class CommandLineParserTest : public ::testing::Test {
 protected:
  void SetUp() override {
    parser_ = new CommandLineParser();

    test_flags_length_ = 3;
    int flag_size = 32;
    test_flags_ = new char*[test_flags_length_];
    for (int i = 0; i < test_flags_length_; ++i) {
      test_flags_[i] = new char[flag_size];
    }
    strncpy(test_flags_[0], "tools_unittest", flag_size);
    strncpy(test_flags_[1], "--foo", flag_size);
    strncpy(test_flags_[2], "--bar=1", flag_size);
  }
  void TearDown() override {
    for (int i = 0; i < test_flags_length_; ++i) {
      delete[] test_flags_[i];
    }
    delete[] test_flags_;
    delete parser_;
  }
  CommandLineParser* parser_;
  // Test flags to emulate a program's argv arguments.
  char** test_flags_;
  int test_flags_length_;
};

TEST_F(CommandLineParserTest, ProcessFlags) {
  // Setup supported flags to parse.
  parser_->SetFlag("foo", "false");
  parser_->SetFlag("foo-foo", "false");  // To test boolean flags defaults.
  parser_->SetFlag("bar", "222");
  parser_->SetFlag("baz", "333");  // To test the default value functionality.

  parser_->Init(test_flags_length_, test_flags_);
  parser_->ProcessFlags();
  EXPECT_EQ("true", parser_->GetFlag("foo"));
  EXPECT_EQ("false", parser_->GetFlag("foo-foo"));
  EXPECT_EQ("1", parser_->GetFlag("bar"));
  EXPECT_EQ("333", parser_->GetFlag("baz"));
  EXPECT_EQ("", parser_->GetFlag("unknown"));
}

TEST_F(CommandLineParserTest, IsStandaloneFlag) {
  EXPECT_TRUE(parser_->IsStandaloneFlag("--foo"));
  EXPECT_TRUE(parser_->IsStandaloneFlag("--foo-foo"));
  EXPECT_FALSE(parser_->IsStandaloneFlag("--foo=1"));
}

TEST_F(CommandLineParserTest, IsFlagWellFormed) {
  EXPECT_TRUE(parser_->IsFlagWellFormed("--foo"));
  EXPECT_TRUE(parser_->IsFlagWellFormed("--foo-foo"));
  EXPECT_TRUE(parser_->IsFlagWellFormed("--bar=1"));
}

TEST_F(CommandLineParserTest, GetCommandLineFlagName) {
  EXPECT_EQ("foo", parser_->GetCommandLineFlagName("--foo"));
  EXPECT_EQ("foo-foo", parser_->GetCommandLineFlagName("--foo-foo"));
  EXPECT_EQ("bar", parser_->GetCommandLineFlagName("--bar=1"));
}

TEST_F(CommandLineParserTest, GetCommandLineFlagValue) {
  EXPECT_EQ("", parser_->GetCommandLineFlagValue("--foo"));
  EXPECT_EQ("", parser_->GetCommandLineFlagValue("--foo-foo"));
  EXPECT_EQ("1", parser_->GetCommandLineFlagValue("--bar=1"));
}

}  // namespace test
}  // namespace webrtc
