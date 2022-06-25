/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/file_utils.h"

#include <stdio.h>

#include <algorithm>
#include <fstream>
#include <string>

#include "absl/types/optional.h"
#include "rtc_base/checks.h"
#include "test/gmock.h"
#include "test/gtest.h"

#ifdef WIN32
#define chdir _chdir
#endif

using ::testing::EndsWith;

namespace webrtc {
namespace test {

namespace {

std::string Path(const std::string& path) {
  std::string result = path;
  std::replace(result.begin(), result.end(), '/', *kPathDelimiter);
  return result;
}

// Remove files and directories in a directory non-recursively and writes the
// number of deleted items in |num_deleted_entries|.
void CleanDir(const std::string& dir, size_t* num_deleted_entries) {
  RTC_DCHECK(num_deleted_entries);
  *num_deleted_entries = 0;
  absl::optional<std::vector<std::string>> dir_content = ReadDirectory(dir);
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
class FileUtilsTest : public ::testing::Test {
 protected:
  FileUtilsTest() {}
  ~FileUtilsTest() override {}
  // Runs before the first test
  static void SetUpTestSuite() {
    original_working_dir_ = webrtc::test::WorkingDir();
  }
  void SetUp() override { ASSERT_EQ(chdir(original_working_dir_.c_str()), 0); }
  void TearDown() override {
    ASSERT_EQ(chdir(original_working_dir_.c_str()), 0);
  }

 private:
  static std::string original_working_dir_;
};

std::string FileUtilsTest::original_working_dir_ = "";

// The location will vary depending on where the webrtc checkout is on the
// system, but it should end as described above and be an absolute path.
std::string ExpectedRootDirByPlatform() {
#if defined(WEBRTC_ANDROID)
  return Path("chromium_tests_root/");
#elif defined(WEBRTC_IOS)
  return Path("tmp/");
#else
  return Path("out/");
#endif
}

TEST_F(FileUtilsTest, OutputPathFromUnchangedWorkingDir) {
  std::string expected_end = ExpectedRootDirByPlatform();
  std::string result = webrtc::test::OutputPath();

  ASSERT_THAT(result, EndsWith(expected_end));
}

// Tests with current working directory set to a directory higher up in the
// directory tree than the project root dir.
TEST_F(FileUtilsTest, OutputPathFromRootWorkingDir) {
  ASSERT_EQ(0, chdir(kPathDelimiter));

  std::string expected_end = ExpectedRootDirByPlatform();
  std::string result = webrtc::test::OutputPath();

  ASSERT_THAT(result, EndsWith(expected_end));
}

TEST_F(FileUtilsTest, TempFilename) {
  std::string temp_filename = webrtc::test::TempFilename(
      webrtc::test::OutputPath(), "TempFilenameTest");
  ASSERT_TRUE(webrtc::test::FileExists(temp_filename))
      << "Couldn't find file: " << temp_filename;
  remove(temp_filename.c_str());
}

TEST_F(FileUtilsTest, GenerateTempFilename) {
  std::string temp_filename = webrtc::test::GenerateTempFilename(
      webrtc::test::OutputPath(), "TempFilenameTest");
  ASSERT_FALSE(webrtc::test::FileExists(temp_filename))
      << "File exists: " << temp_filename;
  FILE* file = fopen(temp_filename.c_str(), "wb");
  ASSERT_TRUE(file != NULL) << "Failed to open file: " << temp_filename;
  ASSERT_GT(fprintf(file, "%s", "Dummy data"), 0)
      << "Failed to write to file: " << temp_filename;
  fclose(file);
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
  // This will obviously be different depending on where the webrtc checkout is,
  // so just check something is returned.
  std::string working_dir = webrtc::test::WorkingDir();
  ASSERT_GT(working_dir.length(), 0u);
}

TEST_F(FileUtilsTest, ResourcePathReturnsCorrectPath) {
  std::string result = webrtc::test::ResourcePath(
      Path("video_coding/frame-ethernet-ii"), "pcap");
#if defined(WEBRTC_IOS)
  // iOS bundles resources straight into the bundle root.
  std::string expected_end = Path("/frame-ethernet-ii.pcap");
#else
  // Other platforms: it's a separate dir.
  std::string expected_end =
      Path("resources/video_coding/frame-ethernet-ii.pcap");
#endif

  ASSERT_THAT(result, EndsWith(expected_end));
  ASSERT_TRUE(FileExists(result)) << "Expected " << result
                                  << " to exist; did "
                                     "ResourcePath return an incorrect path?";
}

TEST_F(FileUtilsTest, ResourcePathFromRootWorkingDir) {
  ASSERT_EQ(0, chdir(kPathDelimiter));
  std::string resource = webrtc::test::ResourcePath("whatever", "ext");
#if !defined(WEBRTC_IOS)
  ASSERT_NE(resource.find("resources"), std::string::npos);
#endif
  ASSERT_GT(resource.find("whatever"), 0u);
  ASSERT_GT(resource.find("ext"), 0u);
}

TEST_F(FileUtilsTest, GetFileSizeExistingFile) {
  // Create a file with some dummy data in.
  std::string temp_filename = webrtc::test::TempFilename(
      webrtc::test::OutputPath(), "fileutils_unittest");
  FILE* file = fopen(temp_filename.c_str(), "wb");
  ASSERT_TRUE(file != NULL) << "Failed to open file: " << temp_filename;
  ASSERT_GT(fprintf(file, "%s", "Dummy data"), 0)
      << "Failed to write to file: " << temp_filename;
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
      OutputPath() + Path("TempFileUtilsTestReadDirectory/");
  CreateDir(temp_directory);
  EXPECT_NO_FATAL_FAILURE(CleanDir(temp_directory, &num_deleted_entries));
  EXPECT_TRUE(DirExists(temp_directory));

  // Add a file.
  const std::string temp_filename = temp_directory + "TempFilenameTest";
  WriteStringInFile("test\n", temp_filename);
  EXPECT_TRUE(FileExists(temp_filename));

  // Add an empty directory.
  const std::string temp_subdir = temp_directory + Path("subdir/");
  EXPECT_TRUE(CreateDir(temp_subdir));
  EXPECT_TRUE(DirExists(temp_subdir));

  // Checks.
  absl::optional<std::vector<std::string>> dir_content =
      ReadDirectory(temp_directory);
  EXPECT_TRUE(dir_content);
  EXPECT_EQ(2u, dir_content->size());
  EXPECT_NO_FATAL_FAILURE(CleanDir(temp_directory, &num_deleted_entries));
  EXPECT_EQ(2u, num_deleted_entries);
  EXPECT_TRUE(RemoveDir(temp_directory));
  EXPECT_FALSE(DirExists(temp_directory));
}

TEST_F(FileUtilsTest, DirNameStripsFilename) {
  EXPECT_EQ(Path("/some/path"), DirName(Path("/some/path/file.txt")));
}

TEST_F(FileUtilsTest, DirNameKeepsStrippingRightmostPathComponent) {
  EXPECT_EQ(Path("/some"), DirName(DirName(Path("/some/path/file.txt"))));
}

TEST_F(FileUtilsTest, DirNameDoesntCareIfAPathEndsInPathSeparator) {
  EXPECT_EQ(Path("/some"), DirName(Path("/some/path/")));
}

TEST_F(FileUtilsTest, DirNameStopsAtRoot) {
  EXPECT_EQ(Path("/"), DirName(Path("/")));
}

}  // namespace test
}  // namespace webrtc
