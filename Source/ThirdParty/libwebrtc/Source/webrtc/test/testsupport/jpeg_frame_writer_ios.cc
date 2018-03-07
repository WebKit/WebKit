/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "test/testsupport/frame_writer.h"


namespace webrtc {
namespace test {

JpegFrameWriter::JpegFrameWriter(const std::string& /*output_filename*/) {}

bool JpegFrameWriter::WriteFrame(const VideoFrame& /*input_frame*/,
                                 int /*quality*/) {
  RTC_LOG(LS_WARNING)
      << "Libjpeg isn't available on IOS. Jpeg frame writer is not "
         "supported. No frame will be saved.";
  // Don't fail.
  return true;
}

}  // namespace test
}  // namespace webrtc
