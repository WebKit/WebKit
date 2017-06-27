// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void TestProcess(const std::string& name,
                 const std::vector<base::CommandLine::StringType>& args) {
  base::FilePath exe_dir;
  ASSERT_TRUE(PathService::Get(base::DIR_EXE, &exe_dir));
  base::FilePath test_binary = exe_dir.AppendASCII("boringssl_" + name);
  base::CommandLine cmd(test_binary);

  for (size_t i = 0; i < args.size(); ++i) {
    cmd.AppendArgNative(args[i]);
  }

  std::string output;
  EXPECT_TRUE(base::GetAppOutput(cmd, &output));
  // Account for Windows line endings.
  base::ReplaceSubstringsAfterOffset(&output, 0, "\r\n", "\n");

  const bool ok = output.size() >= 5 &&
                  memcmp("PASS\n", &output[output.size() - 5], 5) == 0 &&
                  (output.size() == 5 || output[output.size() - 6] == '\n');

  EXPECT_TRUE(ok) << output;
}

bool BoringSSLPath(base::FilePath* result) {
  if (!PathService::Get(base::DIR_SOURCE_ROOT, result))
    return false;

  *result = result->Append(FILE_PATH_LITERAL("third_party"));
  *result = result->Append(FILE_PATH_LITERAL("boringssl"));
  *result = result->Append(FILE_PATH_LITERAL("src"));
  return true;
}

}  // anonymous namespace

// Runs all the tests specified in BoringSSL's all_tests.json file to ensure
// that BoringSSL, as built with Chromium's settings, is functional.
TEST(BoringSSL, UnitTests) {
  base::FilePath boringssl_path;
  ASSERT_TRUE(BoringSSLPath(&boringssl_path));

  std::string data;
  ASSERT_TRUE(
      base::ReadFileToString(boringssl_path.Append(FILE_PATH_LITERAL("util"))
                                 .Append(FILE_PATH_LITERAL("all_tests.json")),
                             &data));

  std::unique_ptr<base::Value> value = base::JSONReader::Read(data);
  ASSERT_TRUE(value);

  base::ListValue* tests;
  ASSERT_TRUE(value->GetAsList(&tests));

  for (size_t i = 0; i < tests->GetSize(); i++) {
    SCOPED_TRACE(i);
    base::ListValue* test;
    ASSERT_TRUE(tests->GetList(i, &test));
    ASSERT_FALSE(test->empty());

    std::string name;
    ASSERT_TRUE(test->GetString(0, &name));

    // Skip libdecrepit tests. Chromium does not build libdecrepit.
    if (base::StartsWith(name, "decrepit/", base::CompareCase::SENSITIVE))
      continue;

    // Skip the GTest tests. This wrapper will be removed once all the tests are
    // converted to GTest. See https://crbug.com/boringssl/129
    if (base::EndsWith(name, "crypto_test", base::CompareCase::SENSITIVE) ||
        base::EndsWith(name, "ssl_test", base::CompareCase::SENSITIVE)) {
      continue;
    }

    name = name.substr(name.find_last_of('/') + 1);
    SCOPED_TRACE(name);

    std::vector<base::CommandLine::StringType> args;
    for (size_t j = 1; j < test->GetSize(); j++) {
      base::CommandLine::StringType arg;
      ASSERT_TRUE(test->GetString(j, &arg));

      // If the argument contains a /, assume it is a file path.
      if (arg.find('/') != base::CommandLine::StringType::npos) {
        arg = boringssl_path.Append(arg).value();
      }

      args.push_back(arg);
    }

    TestProcess(name, args);
  }
}
