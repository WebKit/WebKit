/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/file_rotating_stream.h"

#include <string.h>

#include <cstdint>
#include <memory>

#include "absl/strings/string_view.h"
#include "rtc_base/arraysize.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

namespace rtc {

namespace {

void CleanupLogDirectory(const FileRotatingStream& stream) {
  for (size_t i = 0; i < stream.GetNumFiles(); ++i) {
    // Ignore return value, not all files are expected to exist.
    webrtc::test::RemoveFile(stream.GetFilePath(i));
  }
}

}  // namespace

#if defined(WEBRTC_ANDROID)
// Fails on Android: https://bugs.chromium.org/p/webrtc/issues/detail?id=4364.
#define MAYBE_FileRotatingStreamTest DISABLED_FileRotatingStreamTest
#else
#define MAYBE_FileRotatingStreamTest FileRotatingStreamTest
#endif

class MAYBE_FileRotatingStreamTest : public ::testing::Test {
 protected:
  static const char* kFilePrefix;
  static const size_t kMaxFileSize;

  void Init(absl::string_view dir_name,
            absl::string_view file_prefix,
            size_t max_file_size,
            size_t num_log_files,
            bool ensure_trailing_delimiter = true) {
    dir_path_ = webrtc::test::OutputPath();

    // Append per-test output path in order to run within gtest parallel.
    dir_path_.append(dir_name.begin(), dir_name.end());
    if (ensure_trailing_delimiter) {
      dir_path_.append(std::string(webrtc::test::kPathDelimiter));
    }
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
    EXPECT_TRUE(stream_->Write(data, data_len));
    EXPECT_TRUE(stream_->Flush());
  }

  // Checks that the stream reads in the expected contents and then returns an
  // end of stream result.
  void VerifyStreamRead(absl::string_view expected_contents,
                        absl::string_view dir_path,
                        absl::string_view file_prefix) {
    size_t expected_length = expected_contents.size();
    FileRotatingStreamReader reader(dir_path, file_prefix);
    EXPECT_EQ(reader.GetSize(), expected_length);
    std::unique_ptr<uint8_t[]> buffer(new uint8_t[expected_length]);
    memset(buffer.get(), 0, expected_length);
    EXPECT_EQ(expected_length, reader.ReadAll(buffer.get(), expected_length));
    EXPECT_EQ(0,
              memcmp(expected_contents.data(), buffer.get(), expected_length));
  }

  void VerifyFileContents(absl::string_view expected_contents,
                          absl::string_view file_path) {
    size_t expected_length = expected_contents.size();
    std::unique_ptr<uint8_t[]> buffer(new uint8_t[expected_length + 1]);
    webrtc::FileWrapper f = webrtc::FileWrapper::OpenReadOnly(file_path);
    ASSERT_TRUE(f.is_open());
    size_t size_read = f.Read(buffer.get(), expected_length + 1);
    EXPECT_EQ(size_read, expected_length);
    EXPECT_EQ(0, memcmp(expected_contents.data(), buffer.get(),
                        std::min(expected_length, size_read)));
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

  EXPECT_FALSE(stream_->IsOpen());
  ASSERT_TRUE(stream_->Open());
  EXPECT_TRUE(stream_->IsOpen());
  stream_->Close();
  EXPECT_FALSE(stream_->IsOpen());
}

// Tests that nothing is written to file when data of length zero is written.
TEST_F(MAYBE_FileRotatingStreamTest, EmptyWrite) {
  Init("FileRotatingStreamTestEmptyWrite", kFilePrefix, kMaxFileSize, 3);

  ASSERT_TRUE(stream_->Open());
  WriteAndFlush("a", 0);

  std::string logfile_path = stream_->GetFilePath(0);
  webrtc::FileWrapper f = webrtc::FileWrapper::OpenReadOnly(logfile_path);
  ASSERT_TRUE(f.is_open());
  char buf[1];
  EXPECT_EQ(0u, f.Read(buf, sizeof(buf)));
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
    VerifyFileContents(message, stream_->GetFilePath(1));
  }
  // Check that exactly three files exist.
  for (size_t i = 0; i < arraysize(messages); ++i) {
    EXPECT_TRUE(webrtc::test::FileExists(stream_->GetFilePath(i)));
  }
  std::string message("d");
  WriteAndFlush(message.c_str(), message.size());
  for (size_t i = 0; i < arraysize(messages); ++i) {
    EXPECT_TRUE(webrtc::test::FileExists(stream_->GetFilePath(i)));
  }
  // TODO(tkchin): Maybe check all the files in the dir.

  // Reopen for read.
  std::string expected_contents("bbccd");
  VerifyStreamRead(expected_contents, dir_path_, kFilePrefix);
}

// Tests that a write operation (with dir name without delimiter) followed by a
// read returns the expected data and writes to the expected files.
TEST_F(MAYBE_FileRotatingStreamTest, WriteWithoutDelimiterAndRead) {
  Init("FileRotatingStreamTestWriteWithoutDelimiterAndRead", kFilePrefix,
       kMaxFileSize, 3,
       /* ensure_trailing_delimiter*/ false);

  ASSERT_TRUE(stream_->Open());
  // The test is set up to create three log files of length 2. Write and check
  // contents.
  std::string messages[3] = {"aa", "bb", "cc"};
  for (size_t i = 0; i < arraysize(messages); ++i) {
    const std::string& message = messages[i];
    WriteAndFlush(message.c_str(), message.size());
  }
  std::string message("d");
  WriteAndFlush(message.c_str(), message.size());

  // Reopen for read.
  std::string expected_contents("bbccd");
  VerifyStreamRead(expected_contents,
                   dir_path_ + std::string(webrtc::test::kPathDelimiter),
                   kFilePrefix);
}

// Tests that a write operation followed by a read (without trailing delimiter)
// returns the expected data and writes to the expected files.
TEST_F(MAYBE_FileRotatingStreamTest, WriteAndReadWithoutDelimiter) {
  Init("FileRotatingStreamTestWriteAndReadWithoutDelimiter", kFilePrefix,
       kMaxFileSize, 3);

  ASSERT_TRUE(stream_->Open());
  // The test is set up to create three log files of length 2. Write and check
  // contents.
  std::string messages[3] = {"aa", "bb", "cc"};
  for (size_t i = 0; i < arraysize(messages); ++i) {
    const std::string& message = messages[i];
    WriteAndFlush(message.c_str(), message.size());
  }
  std::string message("d");
  WriteAndFlush(message.c_str(), message.size());

  // Reopen for read.
  std::string expected_contents("bbccd");
  VerifyStreamRead(expected_contents, dir_path_.substr(0, dir_path_.size() - 1),
                   kFilePrefix);
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
  VerifyFileContents(expected_file_contents, stream_->GetFilePath(0));
  std::string expected_stream_contents("arbaz");
  VerifyStreamRead(expected_stream_contents, dir_path_, kFilePrefix);
}

// Tests that the returned file paths have the right folder and prefix.
TEST_F(MAYBE_FileRotatingStreamTest, GetFilePath) {
  Init("FileRotatingStreamTestGetFilePath", kFilePrefix, kMaxFileSize, 20);
  // dir_path_ includes a trailing delimiter.
  const std::string prefix = dir_path_ + kFilePrefix;
  for (auto i = 0; i < 20; ++i) {
    EXPECT_EQ(0, stream_->GetFilePath(i).compare(0, prefix.size(), prefix));
  }
}

#if defined(WEBRTC_ANDROID)
// Fails on Android: https://bugs.chromium.org/p/webrtc/issues/detail?id=4364.
#define MAYBE_CallSessionFileRotatingStreamTest \
  DISABLED_CallSessionFileRotatingStreamTest
#else
#define MAYBE_CallSessionFileRotatingStreamTest \
  CallSessionFileRotatingStreamTest
#endif

class MAYBE_CallSessionFileRotatingStreamTest : public ::testing::Test {
 protected:
  void Init(absl::string_view dir_name, size_t max_total_log_size) {
    dir_path_ = webrtc::test::OutputPath();

    // Append per-test output path in order to run within gtest parallel.
    dir_path_.append(dir_name.begin(), dir_name.end());
    dir_path_.append(std::string(webrtc::test::kPathDelimiter));
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
    EXPECT_TRUE(stream_->Write(data, data_len));
    EXPECT_TRUE(stream_->Flush());
  }

  // Checks that the stream reads in the expected contents and then returns an
  // end of stream result.
  void VerifyStreamRead(absl::string_view expected_contents,
                        absl::string_view dir_path) {
    size_t expected_length = expected_contents.size();
    CallSessionFileRotatingStreamReader reader(dir_path);
    EXPECT_EQ(reader.GetSize(), expected_length);
    std::unique_ptr<uint8_t[]> buffer(new uint8_t[expected_length]);
    memset(buffer.get(), 0, expected_length);
    EXPECT_EQ(expected_length, reader.ReadAll(buffer.get(), expected_length));
    EXPECT_EQ(0,
              memcmp(expected_contents.data(), buffer.get(), expected_length));
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
  VerifyStreamRead(expected_contents, dir_path_);
}

// Tests that writing and reading to a stream with capacity lesser than 4MB
// behaves correctly.
TEST_F(MAYBE_CallSessionFileRotatingStreamTest, WriteAndReadSmall) {
  Init("CallSessionFileRotatingStreamTestWriteAndReadSmall", 8);

  ASSERT_TRUE(stream_->Open());
  std::string message("123456789");
  WriteAndFlush(message.c_str(), message.size());
  std::string expected_contents("1234789");
  VerifyStreamRead(expected_contents, dir_path_);
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
    EXPECT_TRUE(stream_->Write(buffer.get(), buffer_size));
  }

  const int expected_vals[] = {0, 1, 2, 6, 7};
  const size_t expected_size = buffer_size * arraysize(expected_vals);

  CallSessionFileRotatingStreamReader reader(dir_path_);
  EXPECT_EQ(reader.GetSize(), expected_size);
  std::unique_ptr<uint8_t[]> contents(new uint8_t[expected_size + 1]);
  EXPECT_EQ(reader.ReadAll(contents.get(), expected_size + 1), expected_size);
  for (size_t i = 0; i < arraysize(expected_vals); ++i) {
    const uint8_t* block = contents.get() + i * buffer_size;
    bool match = true;
    for (size_t j = 0; j < buffer_size; j++) {
      if (block[j] != expected_vals[i]) {
        match = false;
        break;
      }
    }
    // EXPECT call at end of loop, to limit the number of messages on failure.
    EXPECT_TRUE(match);
  }
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
    EXPECT_TRUE(stream_->Write(buffer.get(), buffer_size));
  }

  const int expected_vals[] = {0, 1};
  const size_t expected_size = buffer_size * arraysize(expected_vals);

  CallSessionFileRotatingStreamReader reader(dir_path_);
  EXPECT_EQ(reader.GetSize(), expected_size);
  std::unique_ptr<uint8_t[]> contents(new uint8_t[expected_size + 1]);
  EXPECT_EQ(reader.ReadAll(contents.get(), expected_size + 1), expected_size);

  for (size_t i = 0; i < arraysize(expected_vals); ++i) {
    const uint8_t* block = contents.get() + i * buffer_size;
    bool match = true;
    for (size_t j = 0; j < buffer_size; j++) {
      if (block[j] != expected_vals[i]) {
        match = false;
        break;
      }
    }
    // EXPECT call at end of loop, to limit the number of messages on failure.
    EXPECT_TRUE(match);
  }
}

}  // namespace rtc
