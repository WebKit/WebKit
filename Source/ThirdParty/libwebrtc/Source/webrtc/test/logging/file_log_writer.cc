/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/logging/file_log_writer.h"

#include <memory>

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace webrtc_impl {

FileLogWriter::FileLogWriter(std::string file_path)
    : out_(std::fopen(file_path.c_str(), "wb")) {
  RTC_CHECK(out_ != nullptr)
      << "Failed to open file: '" << file_path << "' for writing.";
}

FileLogWriter::~FileLogWriter() {
  std::fclose(out_);
}

bool FileLogWriter::IsActive() const {
  return true;
}

bool FileLogWriter::Write(const std::string& value) {
  // We don't expect the write to fail. If it does, we don't want to risk
  // silently ignoring it.
  RTC_CHECK_EQ(std::fwrite(value.data(), 1, value.size(), out_), value.size())
      << "fwrite failed unexpectedly: " << errno;
  return true;
}

void FileLogWriter::Flush() {
  RTC_CHECK_EQ(fflush(out_), 0) << "fflush failed unexpectedly: " << errno;
}

}  // namespace webrtc_impl

FileLogWriterFactory::FileLogWriterFactory(std::string base_path)
    : base_path_(base_path) {
  for (size_t i = 0; i < base_path.size(); ++i) {
    if (base_path[i] == '/')
      test::CreateDir(base_path.substr(0, i));
  }
}

FileLogWriterFactory::~FileLogWriterFactory() {}

std::unique_ptr<RtcEventLogOutput> FileLogWriterFactory::Create(
    std::string filename) {
  return std::make_unique<webrtc_impl::FileLogWriter>(base_path_ + filename);
}
}  // namespace webrtc
