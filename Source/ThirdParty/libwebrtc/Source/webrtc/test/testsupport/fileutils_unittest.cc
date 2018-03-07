/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/fileutils.h"

#include <stdio.h>

#include <fstream>
#include <iostream>
#include <list>
#include <string>

#include "api/optional.h"
#include "rtc_base/checks.h"
#include "rtc_base/pathutils.h"
#include "test/gtest.h"

#ifdef WIN32
#define chdir _chdir
static const char* kPathDelimiter = "\\";
#else
static const char* kPathDelimiter = "/";
#endif

static const char kTestName[] = "fileutils_unittest";
static const char kExtension[] = "tmp";

namespace webrtc {
namespace test {

namespace {

// Remove files and directories in a directory non-recursively and writes the
// number of deleted items in |num_deleted_entries|.
void CleanDir(const std::string& dir, size_t* num_deleted_entries) {
  RTC_DCHECK(num_deleted_entries);
  *num_deleted_entries = 0;
  rtc::Optional<std::vector<std::string>> dir_content = ReadDirectory(dir);
  EXPECT_TRUE(dir_content);
  for (const auto& entry : *dir_content) {
    if (DirExists(entry)) {
      EXPECT_TRUE(RemoveDir(entry));
      (*num_deleted_entries)++;
    } else if (FileExists(entry)) {
      EXPECT_TRUE(RemoveFile(entry));
      (*num_deleted_entries)++;
    } else {
      FAIL();
    }
  }
}

void WriteStringInFile(const std::string& what, const std::string& file_path) {
  std::ofstream out(file_path);
  out << what;
  out.close();
}

}  // namespace

// Test fixture to restore the working directory between each test, since some
// of them change it with chdir during execution (not restored by the
// gtest framework).
class FileUtilsTest : public testing::Test {
 protected:
  FileUtilsTest() {
  }
  ~FileUtilsTest() override {}
  // Runs before the first test
  static void SetUpTestCase() {
    original_working_dir_ = webrtc::test::WorkingDir();
  }
  void SetUp() override {
    ASSERT_EQ(chdir(original_working_dir_.c_str()), 0);
  }
  void TearDown() override {
    ASSERT_EQ(chdir(original_working_dir_.c_str()), 0);
  }
 private:
  static std::string original_working_dir_;
};

std::string FileUtilsTest::original_working_dir_ = "";

// Tests that the project output dir path is returned for the default working
// directory that is automatically set when the test executable is launched.
// The test is not fully testing the implementation, since we cannot be sure
// of where the executable was launched from.
#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
#define MAYBE_OutputPathFromUnchangedWorkingDir \
  DISABLED_OutputPathFromUnchangedWorkingDir
#else
#define MAYBE_OutputPathFromUnchangedWorkingDir \
  OutputPathFromUnchangedWorkingDir
#endif
TEST_F(FileUtilsTest, MAYBE_OutputPathFromUnchangedWorkingDir) {
  std::string path = webrtc::test::OutputPath();
  std::string expected_end = "out";
  expected_end = kPathDelimiter + expected_end + kPathDelimiter;
  ASSERT_EQ(path.length() - expected_end.length(), path.find(expected_end));
}

// Tests with current working directory set to a directory higher up in the
// directory tree than the project root dir.
#if defined(WEBRTC_ANDROID) || defined(WIN32) || defined(WEBRTC_IOS)
#define MAYBE_OutputPathFromRootWorkingDir DISABLED_OutputPathFromRootWorkingDir
#else
#define MAYBE_OutputPathFromRootWorkingDir OutputPathFromRootWorkingDir
#endif
TEST_F(FileUtilsTest, MAYBE_OutputPathFromRootWorkingDir) {
  ASSERT_EQ(0, chdir(kPathDelimiter));
  ASSERT_EQ("./", webrtc::test::OutputPath());
}

TEST_F(FileUtilsTest, TempFilename) {
  std::string temp_filename = webrtc::test::TempFilename(
      webrtc::test::OutputPath(), "TempFilenameTest");
  ASSERT_TRUE(webrtc::test::FileExists(temp_filename))
      << "Couldn't find file: " << temp_filename;
  remove(temp_filename.c_str());
}

// Only tests that the code executes
#if defined(WEBRTC_IOS)
#define MAYBE_CreateDir DISABLED_CreateDir
#else
#define MAYBE_CreateDir CreateDir
#endif
TEST_F(FileUtilsTest, MAYBE_CreateDir) {
  std::string directory = "fileutils-unittest-empty-dir";
  // Make sure it's removed if a previous test has failed:
  remove(directory.c_str());
  ASSERT_TRUE(webrtc::test::CreateDir(directory));
  remove(directory.c_str());
}

TEST_F(FileUtilsTest, WorkingDirReturnsValue) {
  // Hard to cover all platforms. Just test that it returns something without
  // crashing:
  std::string working_dir = webrtc::test::WorkingDir();
  ASSERT_GT(working_dir.length(), 0u);
}

// Due to multiple platforms, it is hard to make a complete test for
// ResourcePath. Manual testing has been performed by removing files and
// verified the result confirms with the specified documentation for the
// function.
TEST_F(FileUtilsTest, ResourcePathReturnsValue) {
  std::string resource = webrtc::test::ResourcePath(kTestName, kExtension);
  ASSERT_GT(resource.find(kTestName), 0u);
  ASSERT_GT(resource.find(kExtension), 0u);
}

TEST_F(FileUtilsTest, ResourcePathFromRootWorkingDir) {
  ASSERT_EQ(0, chdir(kPathDelimiter));
  std::string resource = webrtc::test::ResourcePath(kTestName, kExtension);
#if !defined(WEBRTC_IOS)
  ASSERT_NE(resource.find("resources"), std::string::npos);
#endif
  ASSERT_GT(resource.find(kTestName), 0u);
  ASSERT_GT(resource.find(kExtension), 0u);
}

TEST_F(FileUtilsTest, GetFileSizeExistingFile) {
  // Create a file with some dummy data in.
  std::string temp_filename = webrtc::test::TempFilename(
      webrtc::test::OutputPath(), "fileutils_unittest");
  FILE* file = fopen(temp_filename.c_str(), "wb");
  ASSERT_TRUE(file != NULL) << "Failed to open file: " << temp_filename;
  ASSERT_GT(fprintf(file, "%s",  "Dummy data"), 0) <<
      "Failed to write to file: " << temp_filename;
  fclose(file);
  ASSERT_GT(webrtc::test::GetFileSize(std::string(temp_filename.c_str())), 0u);
  remove(temp_filename.c_str());
}

TEST_F(FileUtilsTest, GetFileSizeNonExistingFile) {
  ASSERT_EQ(0u, webrtc::test::GetFileSize("non-existing-file.tmp"));
}

TEST_F(FileUtilsTest, DirExists) {
  // Check that an existing directory is recognized as such.
  ASSERT_TRUE(webrtc::test::DirExists(webrtc::test::OutputPath()))
      << "Existing directory not found";

  // Check that a non-existing directory is recognized as such.
  std::string directory = "direxists-unittest-non_existing-dir";
  ASSERT_FALSE(webrtc::test::DirExists(directory))
      << "Non-existing directory found";

  // Check that an existing file is not recognized as an existing directory.
  std::string temp_filename = webrtc::test::TempFilename(
      webrtc::test::OutputPath(), "TempFilenameTest");
  ASSERT_TRUE(webrtc::test::FileExists(temp_filename))
      << "Couldn't find file: " << temp_filename;
  ASSERT_FALSE(webrtc::test::DirExists(temp_filename))
      << "Existing file recognized as existing directory";
  remove(temp_filename.c_str());
}

TEST_F(FileUtilsTest, WriteReadDeleteFilesAndDirs) {
  size_t num_deleted_entries;

  // Create an empty temporary directory for this test.
  const std::string temp_directory =
      OutputPath() + "TempFileUtilsTestReadDirectory" + kPathDelimiter;
  CreateDir(temp_directory);
  EXPECT_NO_FATAL_FAILURE(CleanDir(temp_directory, &num_deleted_entries));
  EXPECT_TRUE(DirExists(temp_directory));

  // Add a file.
  const std::string temp_filename = temp_directory + "TempFilenameTest";
  WriteStringInFile("test\n", temp_filename);
  EXPECT_TRUE(FileExists(temp_filename));

  // Add an empty directory.
  const std::string temp_subdir = temp_directory + "subdir" + kPathDelimiter;
  EXPECT_TRUE(CreateDir(temp_subdir));
  EXPECT_TRUE(DirExists(temp_subdir));

  // Checks.
  rtc::Optional<std::vector<std::string>> dir_content =
      ReadDirectory(temp_directory);
  EXPECT_TRUE(dir_content);
  EXPECT_EQ(2u, dir_content->size());
  EXPECT_NO_FATAL_FAILURE(CleanDir(temp_directory, &num_deleted_entries));
  EXPECT_EQ(2u, num_deleted_entries);
  EXPECT_TRUE(RemoveDir(temp_directory));
  EXPECT_FALSE(DirExists(temp_directory));
}

}  // namespace test
}  // namespace webrtc
