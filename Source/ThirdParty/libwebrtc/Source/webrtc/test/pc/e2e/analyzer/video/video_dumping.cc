/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/pc/e2e/analyzer/video/video_dumping.h"

#include <stdio.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/test/video/video_frame_writer.h"
#include "api/video/video_frame.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/clock.h"
#include "test/testsupport/video_frame_writer.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

class VideoFrameIdsWriter final : public test::VideoFrameWriter {
 public:
  explicit VideoFrameIdsWriter(absl::string_view file_name)
      : file_name_(file_name) {
    output_file_ = fopen(file_name_.c_str(), "wb");
    RTC_LOG(LS_INFO) << "Writing VideoFrame IDs into " << file_name_;
    RTC_CHECK(output_file_ != nullptr)
        << "Failed to open file to dump frame ids for writing: " << file_name_;
  }
  ~VideoFrameIdsWriter() override { Close(); }

  bool WriteFrame(const VideoFrame& frame) override {
    RTC_CHECK(output_file_ != nullptr) << "Writer is already closed";
    int chars_written = fprintf(output_file_, "%d\n", frame.id());
    if (chars_written < 2) {
      RTC_LOG(LS_ERROR) << "Failed to write frame id to the output file: "
                        << file_name_;
      return false;
    }
    return true;
  }

  void Close() override {
    if (output_file_ != nullptr) {
      RTC_LOG(LS_INFO) << "Closing file for VideoFrame IDs: " << file_name_;
      fclose(output_file_);
      output_file_ = nullptr;
    }
  }

 private:
  const std::string file_name_;
  FILE* output_file_;
};

// Broadcast received frame to multiple underlying frame writers.
class BroadcastingFrameWriter final : public test::VideoFrameWriter {
 public:
  explicit BroadcastingFrameWriter(
      std::vector<std::unique_ptr<test::VideoFrameWriter>> delegates)
      : delegates_(std::move(delegates)) {}
  ~BroadcastingFrameWriter() override { Close(); }

  bool WriteFrame(const webrtc::VideoFrame& frame) override {
    for (auto& delegate : delegates_) {
      if (!delegate->WriteFrame(frame)) {
        return false;
      }
    }
    return true;
  }

  void Close() override {
    for (auto& delegate : delegates_) {
      delegate->Close();
    }
  }

 private:
  std::vector<std::unique_ptr<test::VideoFrameWriter>> delegates_;
};

}  // namespace

VideoWriter::VideoWriter(test::VideoFrameWriter* video_writer,
                         int sampling_modulo)
    : video_writer_(video_writer), sampling_modulo_(sampling_modulo) {}

void VideoWriter::OnFrame(const VideoFrame& frame) {
  if (frames_counter_++ % sampling_modulo_ != 0) {
    return;
  }
  bool result = video_writer_->WriteFrame(frame);
  RTC_CHECK(result) << "Failed to write frame";
}

std::unique_ptr<test::VideoFrameWriter> CreateVideoFrameWithIdsWriter(
    std::unique_ptr<test::VideoFrameWriter> video_writer_delegate,
    absl::string_view frame_ids_dump_file_name) {
  std::vector<std::unique_ptr<test::VideoFrameWriter>> requested_writers;
  requested_writers.push_back(std::move(video_writer_delegate));
  requested_writers.push_back(
      std::make_unique<VideoFrameIdsWriter>(frame_ids_dump_file_name));
  return std::make_unique<BroadcastingFrameWriter>(
      std::move(requested_writers));
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
