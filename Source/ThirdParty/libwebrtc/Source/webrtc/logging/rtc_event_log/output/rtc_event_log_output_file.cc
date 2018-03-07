/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <limits>

#include "logging/rtc_event_log/output/rtc_event_log_output_file.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/file_wrapper.h"

namespace webrtc {

// Together with the assumption of no single Write() would ever be called on
// an input with length greater-than-or-equal-to (max(size_t) / 2), this
// guarantees no overflow of the check for remaining file capacity in Write().
// This does *not* apply to files with unlimited size.
const size_t RtcEventLogOutputFile::kMaxReasonableFileSize =
    std::numeric_limits<size_t>::max() / 2;

RtcEventLogOutputFile::RtcEventLogOutputFile(const std::string& file_name)
    : RtcEventLogOutputFile(file_name, RtcEventLog::kUnlimitedOutput) {}

RtcEventLogOutputFile::RtcEventLogOutputFile(const std::string& file_name,
                                             size_t max_size_bytes)
    : max_size_bytes_(max_size_bytes), file_(FileWrapper::Create()) {
  RTC_CHECK_LE(max_size_bytes_, kMaxReasonableFileSize);

  if (!file_->OpenFile(file_name.c_str(), false)) {
    RTC_LOG(LS_ERROR) << "Can't open file. WebRTC event log not started.";
    file_.reset();
    return;
  }
}

RtcEventLogOutputFile::RtcEventLogOutputFile(rtc::PlatformFile file)
    : RtcEventLogOutputFile(file, RtcEventLog::kUnlimitedOutput) {}

RtcEventLogOutputFile::RtcEventLogOutputFile(rtc::PlatformFile file,
                                             size_t max_size_bytes)
    : max_size_bytes_(max_size_bytes), file_(FileWrapper::Create()) {
  RTC_CHECK_LE(max_size_bytes_, kMaxReasonableFileSize);

  FILE* file_handle = rtc::FdopenPlatformFileForWriting(file);
  if (!file_handle) {
    RTC_LOG(LS_ERROR) << "Can't open file. WebRTC event log not started.";
    // Even though we failed to open a FILE*, the file is still open
    // and needs to be closed.
    if (!rtc::ClosePlatformFile(file)) {
      RTC_LOG(LS_ERROR) << "Can't close file.";
    }
    file_.reset();
    return;
  }

  if (!file_->OpenFromFileHandle(file_handle)) {
    RTC_LOG(LS_ERROR) << "Can't open file. WebRTC event log not started.";
    file_.reset();
    return;
  }
}

RtcEventLogOutputFile::~RtcEventLogOutputFile() {
  if (file_) {
    RTC_DCHECK(IsActiveInternal());
    file_->CloseFile();
    file_.reset();
  }
}

bool RtcEventLogOutputFile::IsActive() const {
  return IsActiveInternal();
}

bool RtcEventLogOutputFile::Write(const std::string& output) {
  RTC_DCHECK(IsActiveInternal());
  // No single write may be so big, that it would risk overflowing the
  // calculation of (written_bytes_ + output.length()).
  RTC_DCHECK_LT(output.length(), kMaxReasonableFileSize);

  bool written = false;
  if (max_size_bytes_ == RtcEventLog::kUnlimitedOutput ||
      written_bytes_ + output.length() <= max_size_bytes_) {
    written = file_->Write(output.c_str(), output.size());
    if (!written) {
      RTC_LOG(LS_ERROR) << "FileWrapper failed to write WebRtcEventLog file.";
    }
  } else {
    RTC_LOG(LS_VERBOSE) << "Max file size reached.";
  }

  if (written) {
    written_bytes_ += output.size();
  } else {
    file_->CloseFile();
    file_.reset();
  }

  return written;
}

bool RtcEventLogOutputFile::IsActiveInternal() const {
  return file_ && file_->is_open();
}

}  // namespace webrtc
