/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/testsupport/fileutils.h"

#include <stdio.h>

#include <list>
#include <string>

#include "webrtc/test/gtest.h"

#ifdef WIN32
#define chdir _chdir
static const char* kPathDelimiter = "\\";
#else
static const char* kPathDelimiter = "/";
#endif

static const std::string kResourcesDir = "resources";
static const std::string kTestName = "fileutils_unittest";
static const std::string kExtension = "tmp";

namespace webrtc {

// Test fixture to restore the working directory between each test, since some
// of them change it with chdir during execution (not restored by the
// gtest framework).
class FileUtilsTest : public testing::Test {
 protected:
  FileUtilsTest() {
  }
  virtual ~FileUtilsTest() {}
  // Runs before the first test
  static void SetUpTestCase() {
    original_working_dir_ = webrtc::test::WorkingDir();
  }
  void SetUp() {
    ASSERT_EQ(chdir(original_working_dir_.c_str()), 0);
  }
  void TearDown() {
    ASSERT_EQ(chdir(original_working_dir_.c_str()), 0);
  }
 private:
  static std::string original_working_dir_;
};

std::string FileUtilsTest::original_working_dir_ = "";

// Tests that the project root path is returned for the default working
// directory that is automatically set when the test executable is launched.
// The test is not fully testing the implementation, since we cannot be sure
// of where the executable was launched from.
TEST_F(FileUtilsTest, ProjectRootPath) {
  std::string project_root = webrtc::test::ProjectRootPath();
  // Not very smart, but at least tests something.
  ASSERT_GT(project_root.length(), 0u);
}

// Similar to the above test, but for the output dir
#if defined(WEBRTC_ANDROID)
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
#if defined(WEBRTC_ANDROID)
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
TEST_F(FileUtilsTest, CreateDir) {
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
  ASSERT_NE(resource.find("resources"), std::string::npos);
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

}  // namespace webrtc
