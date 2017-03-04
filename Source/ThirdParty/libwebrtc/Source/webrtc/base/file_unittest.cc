/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <limits>
#include <memory>
#include <string>

#include "webrtc/base/gunit.h"
#include "webrtc/base/file.h"
#include "webrtc/test/testsupport/fileutils.h"

#if defined(WEBRTC_WIN)

#include "webrtc/base/win32.h"

#else  // if defined(WEBRTC_WIN)

#include <errno.h>

#endif

namespace rtc {

int LastError() {
#if defined(WEBRTC_WIN)
  return ::GetLastError();
#else
  return errno;
#endif
}

bool VerifyBuffer(uint8_t* buffer, size_t length, uint8_t start_value) {
  for (size_t i = 0; i < length; ++i) {
    uint8_t val = start_value++;
    EXPECT_EQ(val, buffer[i]);
    if (buffer[i] != val)
      return false;
  }
  // Prevent the same buffer from being verified multiple times simply
  // because some operation that should have written to it failed
  memset(buffer, 0, length);
  return true;
}

class FileTest : public ::testing::Test {
 protected:
  std::string path_;
  void SetUp() override {
    path_ = webrtc::test::TempFilename(webrtc::test::OutputPath(), "test_file");
    ASSERT_FALSE(path_.empty());
  }
  void TearDown() override { RemoveFile(path_); }
};

TEST_F(FileTest, DefaultConstructor) {
  File file;
  uint8_t buffer[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

  EXPECT_FALSE(file.IsOpen());
  EXPECT_EQ(0u, file.Write(buffer, 10));
  EXPECT_FALSE(file.Seek(0));
  EXPECT_EQ(0u, file.Read(buffer, 10));
  EXPECT_EQ(0u, file.WriteAt(buffer, 10, 0));
  EXPECT_EQ(0u, file.ReadAt(buffer, 10, 0));
  EXPECT_FALSE(file.Close());
}

TEST_F(FileTest, DoubleClose) {
  File file = File::Open(path_);
  ASSERT_TRUE(file.IsOpen()) << "Error: " << LastError();

  EXPECT_TRUE(file.Close());
  EXPECT_FALSE(file.Close());
}

TEST_F(FileTest, SimpleReadWrite) {
  File file = File::Open(path_);
  ASSERT_TRUE(file.IsOpen()) << "Error: " << LastError();

  uint8_t data[100] = {0};
  uint8_t out[100] = {0};
  for (int i = 0; i < 100; ++i) {
    data[i] = i;
  }

  EXPECT_EQ(10u, file.Write(data, 10));

  EXPECT_TRUE(file.Seek(0));
  EXPECT_EQ(10u, file.Read(out, 10));
  EXPECT_TRUE(VerifyBuffer(out, 10, 0));

  EXPECT_TRUE(file.Seek(0));
  EXPECT_EQ(100u, file.Write(data, 100));

  EXPECT_TRUE(file.Seek(0));
  EXPECT_EQ(100u, file.Read(out, 100));
  EXPECT_TRUE(VerifyBuffer(out, 100, 0));

  EXPECT_TRUE(file.Seek(1));
  EXPECT_EQ(50u, file.Write(data, 50));
  EXPECT_EQ(50u, file.Write(data + 50, 50));

  EXPECT_TRUE(file.Seek(1));
  EXPECT_EQ(100u, file.Read(out, 100));
  EXPECT_TRUE(VerifyBuffer(out, 100, 0));
}

TEST_F(FileTest, ReadWriteClose) {
  File file = File::Open(path_);
  ASSERT_TRUE(file.IsOpen()) << "Error: " << LastError();

  uint8_t data[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  uint8_t out[10] = {0};
  EXPECT_EQ(10u, file.Write(data, 10));
  EXPECT_TRUE(file.Close());

  File file2 = File::Open(path_);
  ASSERT_TRUE(file2.IsOpen()) << "Error: " << LastError();
  EXPECT_EQ(10u, file2.Read(out, 10));
  EXPECT_TRUE(VerifyBuffer(out, 10, 0));
}

TEST_F(FileTest, RandomAccessRead) {
  File file = File::Open(path_);
  ASSERT_TRUE(file.IsOpen()) << "Error: " << LastError();

  uint8_t data[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  uint8_t out[10] = {0};
  EXPECT_EQ(10u, file.Write(data, 10));

  EXPECT_EQ(4u, file.ReadAt(out, 4, 0));
  EXPECT_TRUE(VerifyBuffer(out, 4, 0));

  EXPECT_EQ(4u, file.ReadAt(out, 4, 4));
  EXPECT_TRUE(VerifyBuffer(out, 4, 4));

  EXPECT_EQ(5u, file.ReadAt(out, 5, 5));
  EXPECT_TRUE(VerifyBuffer(out, 5, 5));
}

TEST_F(FileTest, RandomAccessReadWrite) {
  File file = File::Open(path_);
  ASSERT_TRUE(file.IsOpen()) << "Error: " << LastError();

  uint8_t data[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  uint8_t out[10] = {0};
  EXPECT_EQ(10u, file.Write(data, 10));
  EXPECT_TRUE(file.Seek(4));

  EXPECT_EQ(4u, file.WriteAt(data, 4, 4));
  EXPECT_EQ(4u, file.ReadAt(out, 4, 4));
  EXPECT_TRUE(VerifyBuffer(out, 4, 0));

  EXPECT_EQ(2u, file.WriteAt(data, 2, 8));
  EXPECT_EQ(2u, file.ReadAt(out, 2, 8));
  EXPECT_TRUE(VerifyBuffer(out, 2, 0));
}

TEST_F(FileTest, OpenFromPathname) {
  {
    File file = File::Open(Pathname(path_));
    ASSERT_TRUE(file.IsOpen()) << "Error: " << LastError();
  }

  {
    Pathname path(path_);
    File file = File::Open(path);
    ASSERT_TRUE(file.IsOpen()) << "Error: " << LastError();
  }
}

TEST_F(FileTest, CreateFromPathname) {
  {
    File file = File::Create(Pathname(path_));
    ASSERT_TRUE(file.IsOpen()) << "Error: " << LastError();
  }

  {
    Pathname path(path_);
    File file = File::Create(path);
    ASSERT_TRUE(file.IsOpen()) << "Error: " << LastError();
  }
}

TEST_F(FileTest, ShouldBeAbleToRemoveFile) {
  {
    File file = File::Open(Pathname(path_));
    ASSERT_TRUE(file.IsOpen()) << "Error: " << LastError();
  }

  ASSERT_TRUE(File::Remove(Pathname(path_))) << "Error: " << LastError();
}

}  // namespace rtc
