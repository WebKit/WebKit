/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/filerotatingstream.h"
#include "rtc_base/fileutils.h"
#include "rtc_base/gunit.h"
#include "rtc_base/pathutils.h"
#include "test/testsupport/fileutils.h"

namespace rtc {

namespace {

void CleanupLogDirectory(const FileRotatingStream& stream) {
  for (size_t i = 0; i < stream.GetNumFiles(); ++i) {
    // Ignore return value, not all files are expected to exist.
    webrtc::test::RemoveFile(stream.GetFilePath(i));
  }
}

}  // namespace

#if defined (WEBRTC_ANDROID)
// Fails on Android: https://bugs.chromium.org/p/webrtc/issues/detail?id=4364.
#define MAYBE_FileRotatingStreamTest DISABLED_FileRotatingStreamTest
#else
#define MAYBE_FileRotatingStreamTest FileRotatingStreamTest
#endif

class MAYBE_FileRotatingStreamTest : public ::testing::Test {
 protected:
  static const char* kFilePrefix;
  static const size_t kMaxFileSize;

  void Init(const std::string& dir_name,
            const std::string& file_prefix,
            size_t max_file_size,
            size_t num_log_files) {
    dir_path_ = webrtc::test::OutputPath();

    // Append per-test output path in order to run within gtest parallel.
    dir_path_.append(dir_name);
    dir_path_.push_back(Pathname::DefaultFolderDelimiter());
    ASSERT_TRUE(webrtc::test::CreateDir(dir_path_));
    stream_.reset(new FileRotatingStream(dir_path_, file_prefix, max_file_size,
                                         num_log_files));
  }

  void TearDown() override {
    // On windows, open files can't be removed.
    stream_->Close();
    CleanupLogDirectory(*stream_);
    EXPECT_TRUE(webrtc::test::RemoveDir(dir_path_));

    stream_.reset();
  }

  // Writes the data to the stream and flushes it.
  void WriteAndFlush(const void* data, const size_t data_len) {
    EXPECT_EQ(SR_SUCCESS, stream_->WriteAll(data, data_len, nullptr, nullptr));
    EXPECT_TRUE(stream_->Flush());
  }

  // Checks that the stream reads in the expected contents and then returns an
  // end of stream result.
  void VerifyStreamRead(const char* expected_contents,
                        const size_t expected_length,
                        const std::string& dir_path,
                        const char* file_prefix) {
    std::unique_ptr<FileRotatingStream> stream;
    stream.reset(new FileRotatingStream(dir_path, file_prefix));
    ASSERT_TRUE(stream->Open());
    size_t read = 0;
    size_t stream_size = 0;
    EXPECT_TRUE(stream->GetSize(&stream_size));
    std::unique_ptr<uint8_t[]> buffer(new uint8_t[expected_length]);
    EXPECT_EQ(SR_SUCCESS,
              stream->ReadAll(buffer.get(), expected_length, &read, nullptr));
    EXPECT_EQ(0, memcmp(expected_contents, buffer.get(), expected_length));
    EXPECT_EQ(SR_EOS, stream->ReadAll(buffer.get(), 1, nullptr, nullptr));
    EXPECT_EQ(stream_size, read);
  }

  void VerifyFileContents(const char* expected_contents,
                          const size_t expected_length,
                          const std::string& file_path) {
    std::unique_ptr<uint8_t[]> buffer(new uint8_t[expected_length]);
    FileStream stream;
    ASSERT_TRUE(stream.Open(file_path, "r", nullptr));
    EXPECT_EQ(rtc::SR_SUCCESS,
              stream.ReadAll(buffer.get(), expected_length, nullptr, nullptr));
    EXPECT_EQ(0, memcmp(expected_contents, buffer.get(), expected_length));
    size_t file_size = 0;
    EXPECT_TRUE(stream.GetSize(&file_size));
    EXPECT_EQ(file_size, expected_length);
  }

  std::unique_ptr<FileRotatingStream> stream_;
  std::string dir_path_;
};

const char* MAYBE_FileRotatingStreamTest::kFilePrefix =
    "FileRotatingStreamTest";
const size_t MAYBE_FileRotatingStreamTest::kMaxFileSize = 2;

// Tests that stream state is correct before and after Open / Close.
TEST_F(MAYBE_FileRotatingStreamTest, State) {
  Init("FileRotatingStreamTestState", kFilePrefix, kMaxFileSize, 3);

  EXPECT_EQ(SS_CLOSED, stream_->GetState());
  ASSERT_TRUE(stream_->Open());
  EXPECT_EQ(SS_OPEN, stream_->GetState());
  stream_->Close();
  EXPECT_EQ(SS_CLOSED, stream_->GetState());
}

// Tests that nothing is written to file when data of length zero is written.
TEST_F(MAYBE_FileRotatingStreamTest, EmptyWrite) {
  Init("FileRotatingStreamTestEmptyWrite", kFilePrefix, kMaxFileSize, 3);

  ASSERT_TRUE(stream_->Open());
  WriteAndFlush("a", 0);

  std::string logfile_path = stream_->GetFilePath(0);
  FileStream stream;
  ASSERT_TRUE(stream.Open(logfile_path, "r", nullptr));
  size_t file_size = 0;
  EXPECT_TRUE(stream.GetSize(&file_size));
  EXPECT_EQ(0u, file_size);
}

// Tests that a write operation followed by a read returns the expected data
// and writes to the expected files.
TEST_F(MAYBE_FileRotatingStreamTest, WriteAndRead) {
  Init("FileRotatingStreamTestWriteAndRead", kFilePrefix, kMaxFileSize, 3);

  ASSERT_TRUE(stream_->Open());
  // The test is set up to create three log files of length 2. Write and check
  // contents.
  std::string messages[3] = {"aa", "bb", "cc"};
  for (size_t i = 0; i < arraysize(messages); ++i) {
    const std::string& message = messages[i];
    WriteAndFlush(message.c_str(), message.size());
    // Since the max log size is 2, we will be causing rotation. Read from the
    // next file.
    VerifyFileContents(message.c_str(), message.size(),
                       stream_->GetFilePath(1));
  }
  // Check that exactly three files exist.
  for (size_t i = 0; i < arraysize(messages); ++i) {
    EXPECT_TRUE(Filesystem::IsFile(stream_->GetFilePath(i)));
  }
  std::string message("d");
  WriteAndFlush(message.c_str(), message.size());
  for (size_t i = 0; i < arraysize(messages); ++i) {
    EXPECT_TRUE(Filesystem::IsFile(stream_->GetFilePath(i)));
  }
  // TODO(tkchin): Maybe check all the files in the dir.

  // Reopen for read.
  std::string expected_contents("bbccd");
  VerifyStreamRead(expected_contents.c_str(), expected_contents.size(),
                   dir_path_, kFilePrefix);
}

// Tests that writing data greater than the total capacity of the files
// overwrites the files correctly and is read correctly after.
TEST_F(MAYBE_FileRotatingStreamTest, WriteOverflowAndRead) {
  Init("FileRotatingStreamTestWriteOverflowAndRead", kFilePrefix, kMaxFileSize,
       3);
  ASSERT_TRUE(stream_->Open());
  // This should cause overflow across all three files, such that the first file
  // we wrote to also gets overwritten.
  std::string message("foobarbaz");
  WriteAndFlush(message.c_str(), message.size());
  std::string expected_file_contents("z");
  VerifyFileContents(expected_file_contents.c_str(),
                     expected_file_contents.size(), stream_->GetFilePath(0));
  std::string expected_stream_contents("arbaz");
  VerifyStreamRead(expected_stream_contents.c_str(),
                   expected_stream_contents.size(), dir_path_, kFilePrefix);
}

// Tests that the returned file paths have the right folder and prefix.
TEST_F(MAYBE_FileRotatingStreamTest, GetFilePath) {
  Init("FileRotatingStreamTestGetFilePath", kFilePrefix, kMaxFileSize, 20);
  for (auto i = 0; i < 20; ++i) {
    Pathname path(stream_->GetFilePath(i));
    EXPECT_EQ(0, path.folder().compare(dir_path_));
    EXPECT_EQ(0, path.filename().compare(0, strlen(kFilePrefix), kFilePrefix));
  }
}

#if defined (WEBRTC_ANDROID)
// Fails on Android: https://bugs.chromium.org/p/webrtc/issues/detail?id=4364.
#define MAYBE_CallSessionFileRotatingStreamTest \
    DISABLED_CallSessionFileRotatingStreamTest
#else
#define MAYBE_CallSessionFileRotatingStreamTest \
    CallSessionFileRotatingStreamTest
#endif

class MAYBE_CallSessionFileRotatingStreamTest : public ::testing::Test {
 protected:
  void Init(const std::string& dir_name, size_t max_total_log_size) {
    dir_path_ = webrtc::test::OutputPath();

    // Append per-test output path in order to run within gtest parallel.
    dir_path_.append(dir_name);
    dir_path_.push_back(Pathname::DefaultFolderDelimiter());
    ASSERT_TRUE(webrtc::test::CreateDir(dir_path_));
    stream_.reset(
        new CallSessionFileRotatingStream(dir_path_, max_total_log_size));
  }

  void TearDown() override {
    // On windows, open files can't be removed.
    stream_->Close();
    CleanupLogDirectory(*stream_);
    EXPECT_TRUE(webrtc::test::RemoveDir(dir_path_));

    stream_.reset();
  }

  // Writes the data to the stream and flushes it.
  void WriteAndFlush(const void* data, const size_t data_len) {
    EXPECT_EQ(SR_SUCCESS, stream_->WriteAll(data, data_len, nullptr, nullptr));
    EXPECT_TRUE(stream_->Flush());
  }

  // Checks that the stream reads in the expected contents and then returns an
  // end of stream result.
  void VerifyStreamRead(const char* expected_contents,
                        const size_t expected_length,
                        const std::string& dir_path) {
    std::unique_ptr<CallSessionFileRotatingStream> stream(
        new CallSessionFileRotatingStream(dir_path));
    ASSERT_TRUE(stream->Open());
    size_t read = 0;
    size_t stream_size = 0;
    EXPECT_TRUE(stream->GetSize(&stream_size));
    std::unique_ptr<uint8_t[]> buffer(new uint8_t[expected_length]);
    EXPECT_EQ(SR_SUCCESS,
              stream->ReadAll(buffer.get(), expected_length, &read, nullptr));
    EXPECT_EQ(0, memcmp(expected_contents, buffer.get(), expected_length));
    EXPECT_EQ(SR_EOS, stream->ReadAll(buffer.get(), 1, nullptr, nullptr));
    EXPECT_EQ(stream_size, read);
  }

  std::unique_ptr<CallSessionFileRotatingStream> stream_;
  std::string dir_path_;
};

// Tests that writing and reading to a stream with the smallest possible
// capacity works.
TEST_F(MAYBE_CallSessionFileRotatingStreamTest, WriteAndReadSmallest) {
  Init("CallSessionFileRotatingStreamTestWriteAndReadSmallest", 4);

  ASSERT_TRUE(stream_->Open());
  std::string message("abcde");
  WriteAndFlush(message.c_str(), message.size());
  std::string expected_contents("abe");
  VerifyStreamRead(expected_contents.c_str(), expected_contents.size(),
                   dir_path_);
}

// Tests that writing and reading to a stream with capacity lesser than 4MB
// behaves correctly.
TEST_F(MAYBE_CallSessionFileRotatingStreamTest, WriteAndReadSmall) {
  Init("CallSessionFileRotatingStreamTestWriteAndReadSmall", 8);

  ASSERT_TRUE(stream_->Open());
  std::string message("123456789");
  WriteAndFlush(message.c_str(), message.size());
  std::string expected_contents("1234789");
  VerifyStreamRead(expected_contents.c_str(), expected_contents.size(),
                   dir_path_);
}

// Tests that writing and reading to a stream with capacity greater than 4MB
// behaves correctly.
TEST_F(MAYBE_CallSessionFileRotatingStreamTest, WriteAndReadLarge) {
  Init("CallSessionFileRotatingStreamTestWriteAndReadLarge", 6 * 1024 * 1024);

  ASSERT_TRUE(stream_->Open());
  const size_t buffer_size = 1024 * 1024;
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[buffer_size]);
  for (int i = 0; i < 8; i++) {
    memset(buffer.get(), i, buffer_size);
    EXPECT_EQ(SR_SUCCESS,
              stream_->WriteAll(buffer.get(), buffer_size, nullptr, nullptr));
  }

  stream_.reset(new CallSessionFileRotatingStream(dir_path_));
  ASSERT_TRUE(stream_->Open());
  std::unique_ptr<uint8_t[]> expected_buffer(new uint8_t[buffer_size]);
  int expected_vals[] = {0, 1, 2, 6, 7};
  for (size_t i = 0; i < arraysize(expected_vals); ++i) {
    memset(expected_buffer.get(), expected_vals[i], buffer_size);
    EXPECT_EQ(SR_SUCCESS,
              stream_->ReadAll(buffer.get(), buffer_size, nullptr, nullptr));
    EXPECT_EQ(0, memcmp(buffer.get(), expected_buffer.get(), buffer_size));
  }
  EXPECT_EQ(SR_EOS, stream_->ReadAll(buffer.get(), 1, nullptr, nullptr));
}

// Tests that writing and reading to a stream where only the first file is
// written to behaves correctly.
TEST_F(MAYBE_CallSessionFileRotatingStreamTest, WriteAndReadFirstHalf) {
  Init("CallSessionFileRotatingStreamTestWriteAndReadFirstHalf",
       6 * 1024 * 1024);
  ASSERT_TRUE(stream_->Open());
  const size_t buffer_size = 1024 * 1024;
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[buffer_size]);
  for (int i = 0; i < 2; i++) {
    memset(buffer.get(), i, buffer_size);
    EXPECT_EQ(SR_SUCCESS,
              stream_->WriteAll(buffer.get(), buffer_size, nullptr, nullptr));
  }

  stream_.reset(new CallSessionFileRotatingStream(dir_path_));
  ASSERT_TRUE(stream_->Open());
  std::unique_ptr<uint8_t[]> expected_buffer(new uint8_t[buffer_size]);
  int expected_vals[] = {0, 1};
  for (size_t i = 0; i < arraysize(expected_vals); ++i) {
    memset(expected_buffer.get(), expected_vals[i], buffer_size);
    EXPECT_EQ(SR_SUCCESS,
              stream_->ReadAll(buffer.get(), buffer_size, nullptr, nullptr));
    EXPECT_EQ(0, memcmp(buffer.get(), expected_buffer.get(), buffer_size));
  }
  EXPECT_EQ(SR_EOS, stream_->ReadAll(buffer.get(), 1, nullptr, nullptr));
}

}  // namespace rtc
