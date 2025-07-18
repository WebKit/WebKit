/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Based on Chromium's
// https://source.chromium.org/chromium/chromium/src/+/main:testing/android/native_test/native_test_util.cc
// and Angle's
// https://source.chromium.org/chromium/chromium/src/+/main:third_party/angle/src/tests/test_utils/runner/android/AngleNativeTest.cpp

#include "test/android/native_test_util.h"

#include <android/log.h>
#include <stdio.h>

#include <optional>
#include <string>

#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"

namespace webrtc {
namespace test {
namespace android {

namespace {

const char kLogTag[] = "webrtc";

std::optional<std::string> ReadFileToString(const char* path) {
  FILE* file = fopen(path, "rb");
  if (!file) {
    AndroidLog(ANDROID_LOG_ERROR, "Failed to open %s\n", path);
    return std::nullopt;
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    return std::nullopt;
  }

  auto size = ftell(file);
  // Check that `size` fits in a 32-bit int, to avoid any issues with overflow
  // in the subsequent casts. (For example, consider the case where long is >32
  // bits while size_t is 32 bits.) We're not expecting the command line to be
  // larger than 1 GB, anyway.
  if (size < 0 || size > 1'000'000'000) {
    AndroidLog(ANDROID_LOG_ERROR,
               "Expected size of %s between 0 and 1 GB, got %ld bytes\n", path,
               size);
    return std::nullopt;
  }

  std::string contents;
  contents.resize(size);

  fseek(file, 0, SEEK_SET);

  if (fread(contents.data(), 1, size, file) != static_cast<size_t>(size)) {
    return std::nullopt;
  }

  if (ferror(file)) {
    return std::nullopt;
  }

  return contents;
}

}  // namespace

// Writes printf() style string to Android's logger where |priority| is one of
// the levels defined in <android/log.h>.
void AndroidLog(int priority, const char* format, ...) {
  va_list args;
  va_start(args, format);
  __android_log_vprint(priority, kLogTag, format, args);
  va_end(args);
}

std::string ASCIIJavaStringToUTF8(JNIEnv* env, jstring str) {
  if (!str) {
    return "";
  }

  const jsize length = env->GetStringLength(str);
  if (!length) {
    return "";
  }

  // JNI's GetStringUTFChars() returns strings in Java "modified" UTF8, so
  // instead get the String in UTF16. As the input is ASCII, drop the higher
  // bytes.
  const jchar* jchars = env->GetStringChars(str, NULL);
  const char16_t* chars = reinterpret_cast<const char16_t*>(jchars);
  std::string out(chars, chars + length);
  env->ReleaseStringChars(str, jchars);
  return out;
}

void ParseArgsFromString(const std::string& command_line,
                         std::vector<std::string>* args) {
  std::vector<absl::string_view> v =
      absl::StrSplit(command_line, absl::ByAsciiWhitespace());
  for (absl::string_view arg : v) {
    args->push_back(std::string(arg));
  }

  // TODO(webrtc:42223878): Implement tokenization that handle quotes and
  // escaped quotes (along the lines of the previous chromium code):
  //
  // base::StringTokenizer tokenizer(command_line, base::kWhitespaceASCII);
  // tokenizer.set_quote_chars("\"");
  // while (tokenizer.GetNext()) {
  //   std::string token;
  //   base::RemoveChars(tokenizer.token(), "\"", &token);
  //   args->push_back(token);
  // }
}

void ParseArgsFromCommandLineFile(const char* path,
                                  std::vector<std::string>* args) {
  std::optional<std::string> command_line_string = ReadFileToString(path);
  if (command_line_string.has_value()) {
    ParseArgsFromString(*command_line_string, args);
  }
}

int ArgsToArgv(const std::vector<std::string>& args, std::vector<char*>* argv) {
  // We need to pass in a non-const char**.
  int argc = args.size();

  argv->resize(argc + 1);
  for (int i = 0; i < argc; ++i) {
    (*argv)[i] = const_cast<char*>(args[i].c_str());
  }
  (*argv)[argc] = NULL;  // argv must be NULL terminated.

  return argc;
}

}  // namespace android
}  // namespace test
}  // namespace webrtc
