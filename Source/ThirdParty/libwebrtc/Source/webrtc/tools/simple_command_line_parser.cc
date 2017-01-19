/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/tools/simple_command_line_parser.h"

#include <stdio.h>
#include <stdlib.h>

#include <string>

namespace webrtc {
namespace test {

using std::string;

CommandLineParser::CommandLineParser() {}
CommandLineParser::~CommandLineParser() {}

void CommandLineParser::Init(int argc, char** argv) {
  args_ = std::vector<std::string> (argv + 1, argv + argc);
}

bool CommandLineParser::IsStandaloneFlag(std::string flag) {
  return flag.find("=") == string::npos;
}

bool CommandLineParser::IsFlagWellFormed(std::string flag) {
  size_t dash_pos = flag.find("--");
  size_t equal_pos = flag.find("=");
  if (dash_pos != 0) {
    fprintf(stderr, "Wrong switch format: %s\n", flag.c_str());
    fprintf(stderr, "Flag doesn't start with --\n");
    return false;
  }
  size_t flag_length = flag.length() - 1;

  // We use 3 here because we assume that the flags are in the format
  // --flag_name=flag_value, thus -- are at positions 0 and 1 and we should have
  // at least one symbol for the flag name.
  if (equal_pos > 0 && (equal_pos < 3 || equal_pos == flag_length)) {
    fprintf(stderr, "Wrong switch format: %s\n", flag.c_str());
    fprintf(stderr, "Wrong placement of =\n");
    return false;
  }
  return true;
}

std::string CommandLineParser::GetCommandLineFlagName(std::string flag) {
  size_t dash_pos = flag.find("--");
  size_t equal_pos = flag.find("=");
  if (equal_pos == string::npos) {
    return flag.substr(dash_pos + 2);
  } else {
    return flag.substr(dash_pos + 2, equal_pos - 2);
  }
}

std::string CommandLineParser::GetCommandLineFlagValue(std::string flag) {
  size_t equal_pos = flag.find("=");
  if (equal_pos == string::npos) {
    return "";
  } else {
    return flag.substr(equal_pos + 1);
  }
}

void CommandLineParser::PrintEnteredFlags() {
  std::map<std::string, std::string>::iterator flag_iter;
  fprintf(stdout, "You have entered:\n");
  for (flag_iter = flags_.begin(); flag_iter != flags_.end(); ++flag_iter) {
    if (flag_iter->first != "help") {
      fprintf(stdout, "%s=%s, ", flag_iter->first.c_str(),
              flag_iter->second.c_str());
    }
  }
  fprintf(stdout, "\n");
}

void CommandLineParser::ProcessFlags() {
  std::map<std::string, std::string>::iterator flag_iter;
  std::vector<std::string>::iterator iter;
  for (iter = args_.begin(); iter != args_.end(); ++iter) {
    if (!IsFlagWellFormed(*iter)) {
      // Ignore badly formated flags.
      continue;
    }
    std::string flag_name = GetCommandLineFlagName(*iter);
    flag_iter = flags_.find(flag_name);
    if (flag_iter == flags_.end()) {
      // Ignore unknown flags.
      fprintf(stdout, "Flag '%s' is not recognized\n", flag_name.c_str());
      continue;
    }
    if (IsStandaloneFlag(*iter)) {
      flags_[flag_name] = "true";
    } else {
      flags_[flag_name] = GetCommandLineFlagValue(*iter);
    }
  }
}

void CommandLineParser::SetUsageMessage(std::string usage_message) {
  usage_message_ = usage_message;
}

void CommandLineParser::PrintUsageMessage() {
  fprintf(stdout, "%s", usage_message_.c_str());
}

void CommandLineParser::SetFlag(std::string flag_name,
                                std::string default_flag_value) {
  flags_[flag_name] = default_flag_value;
}

std::string CommandLineParser::GetFlag(std::string flag_name) {
  std::map<std::string, std::string>::iterator flag_iter;
  flag_iter = flags_.find(flag_name);
  // If no such flag.
  if (flag_iter == flags_.end()) {
    return "";
  }
  return flag_iter->second;
}

}  // namespace test
}  // namespace webrtc
