/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/test/test_util.h"

#include <assert.h>
#include <math.h>

#include <iomanip>
#include <sstream>

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/video_coding/internal_defines.h"
#include "webrtc/test/testsupport/fileutils.h"

CmdArgs::CmdArgs()
    : codecName("VP8"),
      codecType(webrtc::kVideoCodecVP8),
      width(352),
      height(288),
      rtt(0),
      inputFile(webrtc::test::ResourcePath("foreman_cif", "yuv")),
      outputFile(webrtc::test::OutputPath() +
                 "video_coding_test_output_352x288.yuv") {}

namespace {

void SplitFilename(const std::string& filename,
                   std::string* basename,
                   std::string* extension) {
  assert(basename);
  assert(extension);

  std::string::size_type idx;
  idx = filename.rfind('.');

  if (idx != std::string::npos) {
    *basename = filename.substr(0, idx);
    *extension = filename.substr(idx + 1);
  } else {
    *basename = filename;
    *extension = "";
  }
}

std::string AppendWidthHeightCount(const std::string& filename,
                                   int width,
                                   int height,
                                   int count) {
  std::string basename;
  std::string extension;
  SplitFilename(filename, &basename, &extension);
  std::stringstream ss;
  ss << basename << "_" << count << "." << width << "_" << height << "."
     << extension;
  return ss.str();
}

}  // namespace

FileOutputFrameReceiver::FileOutputFrameReceiver(
    const std::string& base_out_filename,
    uint32_t ssrc)
    : out_filename_(),
      out_file_(NULL),
      timing_file_(NULL),
      width_(0),
      height_(0),
      count_(0) {
  std::string basename;
  std::string extension;
  if (base_out_filename.empty()) {
    basename = webrtc::test::OutputPath() + "rtp_decoded";
    extension = "yuv";
  } else {
    SplitFilename(base_out_filename, &basename, &extension);
  }
  std::stringstream ss;
  ss << basename << "_" << std::hex << std::setw(8) << std::setfill('0') << ssrc
     << "." << extension;
  out_filename_ = ss.str();
}

FileOutputFrameReceiver::~FileOutputFrameReceiver() {
  if (timing_file_ != NULL) {
    fclose(timing_file_);
  }
  if (out_file_ != NULL) {
    fclose(out_file_);
  }
}

int32_t FileOutputFrameReceiver::FrameToRender(
    webrtc::VideoFrame& video_frame) {
  if (timing_file_ == NULL) {
    std::string basename;
    std::string extension;
    SplitFilename(out_filename_, &basename, &extension);
    timing_file_ = fopen((basename + "_renderTiming.txt").c_str(), "w");
    if (timing_file_ == NULL) {
      return -1;
    }
  }
  if (out_file_ == NULL || video_frame.width() != width_ ||
      video_frame.height() != height_) {
    if (out_file_) {
      fclose(out_file_);
    }
    printf("New size: %dx%d\n", video_frame.width(), video_frame.height());
    width_ = video_frame.width();
    height_ = video_frame.height();
    std::string filename_with_width_height =
        AppendWidthHeightCount(out_filename_, width_, height_, count_);
    ++count_;
    out_file_ = fopen(filename_with_width_height.c_str(), "wb");
    if (out_file_ == NULL) {
      return -1;
    }
  }
  fprintf(timing_file_, "%u, %u\n", video_frame.timestamp(),
          webrtc::MaskWord64ToUWord32(video_frame.render_time_ms()));
  if (PrintVideoFrame(video_frame, out_file_) < 0) {
    return -1;
  }
  return 0;
}

webrtc::RtpVideoCodecTypes ConvertCodecType(const char* plname) {
  if (strncmp(plname, "VP8", 3) == 0) {
    return webrtc::kRtpVideoVp8;
  } else {
    // Default value.
    return webrtc::kRtpVideoGeneric;
  }
}
