/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/platform_file.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

namespace rtc {

void FillWithDummyDataAndClose(FILE* const file, const std::string& filename) {
  EXPECT_GT(fprintf(file, "%s", "Dummy data"), 0)
      << "Failed to write to file: " << filename;
  fclose(file);
}

TEST(PlatformFileTest, CreateWriteAndDelete) {
  const std::string filename = webrtc::test::GenerateTempFilename(
      webrtc::test::OutputPath(), ".testfile");
  const PlatformFile fd = rtc::CreatePlatformFile(filename);
  ASSERT_NE(fd, rtc::kInvalidPlatformFileValue)
      << "Failed to create file descriptor for file: " << filename;
  FILE* const file = rtc::FdopenPlatformFile(fd, "w");
  ASSERT_TRUE(file != nullptr) << "Failed to open file: " << filename;
  FillWithDummyDataAndClose(file, filename);
  webrtc::test::RemoveFile(filename);
}

TEST(PlatformFileTest, OpenExistingWriteAndDelete) {
  const std::string filename = webrtc::test::GenerateTempFilename(
      webrtc::test::OutputPath(), ".testfile");

  // Create file with dummy data.
  FILE* file = fopen(filename.c_str(), "wb");
  ASSERT_TRUE(file != nullptr) << "Failed to open file: " << filename;
  FillWithDummyDataAndClose(file, filename);

  // Open it for write, write and delete.
  const PlatformFile fd = rtc::OpenPlatformFile(filename);
  ASSERT_NE(fd, rtc::kInvalidPlatformFileValue)
      << "Failed to open file descriptor for file: " << filename;
  file = rtc::FdopenPlatformFile(fd, "w");
  ASSERT_TRUE(file != nullptr) << "Failed to open file: " << filename;
  FillWithDummyDataAndClose(file, filename);
  webrtc::test::RemoveFile(filename);
}

TEST(PlatformFileTest, OpenExistingReadOnlyAndDelete) {
  const std::string filename = webrtc::test::GenerateTempFilename(
      webrtc::test::OutputPath(), ".testfile");

  // Create file with dummy data.
  FILE* file = fopen(filename.c_str(), "wb");
  ASSERT_TRUE(file != nullptr) << "Failed to open file: " << filename;
  FillWithDummyDataAndClose(file, filename);

  // Open it for read, read and delete.
  const PlatformFile fd = rtc::OpenPlatformFileReadOnly(filename);
  ASSERT_NE(fd, rtc::kInvalidPlatformFileValue)
      << "Failed to open file descriptor for file: " << filename;
  file = rtc::FdopenPlatformFile(fd, "r");
  ASSERT_TRUE(file != nullptr) << "Failed to open file: " << filename;

  int buf[]{0};
  ASSERT_GT(fread(&buf, 1, 1, file), 0u)
      << "Failed to read from file: " << filename;
  fclose(file);
  webrtc::test::RemoveFile(filename);
}

}  // namespace rtc
