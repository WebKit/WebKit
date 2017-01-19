/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_TOOLS_SIMPLE_COMMAND_LINE_PARSER_H_
#define WEBRTC_TOOLS_SIMPLE_COMMAND_LINE_PARSER_H_

#include <map>
#include <string>
#include <vector>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/gtest_prod_util.h"

// This is a very basic command line parsing class. We pass the command line
// arguments and their number and the class forms a vector out of these. Than we
// should set up the flags - we provide a name and a string value and map these.
//
// Example use of this class:
// 1. Create a CommandLineParser object.
// 2. Configure the flags you want to support with SetFlag calls.
// 3. Call Init with your program's argc+argv parameters.
// 4. Parse the flags by calling ProcessFlags.
// 5. Get the values of the flags using GetFlag.

namespace webrtc {
namespace test {

class CommandLineParser {
 public:
  CommandLineParser();
  ~CommandLineParser();

  void Init(int argc, char** argv);

  // Prints the entered flags and their values (without --help).
  void PrintEnteredFlags();

  // Processes the vector of command line arguments and puts the value of each
  // flag in the corresponding map entry for this flag's name. We don't process
  // flags which haven't been defined in the map.
  void ProcessFlags();

  // Sets the usage message to be shown if we pass --help.
  void SetUsageMessage(std::string usage_message);

  // prints the usage message.
  void PrintUsageMessage();

  // Set a flag into the map of flag names/values.
  // To set a boolean flag, use "false" as the default flag value.
  // The flag_name should not include the -- prefix.
  void SetFlag(std::string flag_name, std::string default_flag_value);

  // Gets a flag when provided a flag name (name is without the -- prefix).
  // Returns "" if the flag is unknown and "true"/"false" if the flag is a
  // boolean flag.
  std::string GetFlag(std::string flag_name);

 private:
  // The vector of passed command line arguments.
  std::vector<std::string> args_;
  // The map of the flag names/values.
  std::map<std::string, std::string> flags_;
  // The usage message.
  std::string usage_message_;

  // Returns whether the passed flag is standalone or not. By standalone we
  // understand e.g. --standalone (in contrast to --non_standalone=1).
  bool IsStandaloneFlag(std::string flag);

  // Checks whether the flag is in the format --flag_name=flag_value.
  // or just --flag_name.
  bool IsFlagWellFormed(std::string flag);

  // Extracts the flag name from the flag, i.e. return foo for --foo=bar.
  std::string GetCommandLineFlagName(std::string flag);

  // Extracts the flag value from the flag, i.e. return bar for --foo=bar.
  // If the flag has no value (i.e. no equals sign) an empty string is returned.
  std::string GetCommandLineFlagValue(std::string flag);

  FRIEND_TEST_ALL_PREFIXES(CommandLineParserTest, IsStandaloneFlag);
  FRIEND_TEST_ALL_PREFIXES(CommandLineParserTest, IsFlagWellFormed);
  FRIEND_TEST_ALL_PREFIXES(CommandLineParserTest, GetCommandLineFlagName);
  FRIEND_TEST_ALL_PREFIXES(CommandLineParserTest, GetCommandLineFlagValue);

  RTC_DISALLOW_COPY_AND_ASSIGN(CommandLineParser);
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TOOLS_SIMPLE_COMMAND_LINE_PARSER_H_
