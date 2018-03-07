/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/video_capturer.h"

#include "rtc_base/basictypes.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {
namespace test {
VideoCapturer::VideoCapturer() : video_adapter_(new cricket::VideoAdapter()) {}
VideoCapturer::~VideoCapturer() {}

rtc::Optional<VideoFrame> VideoCapturer::AdaptFrame(const VideoFrame& frame) {
  int cropped_width = 0;
  int cropped_height = 0;
  int out_width = 0;
  int out_height = 0;

  if (!video_adapter_->AdaptFrameResolution(
          frame.width(), frame.height(), frame.timestamp_us() * 1000,
          &cropped_width, &cropped_height, &out_width, &out_height)) {
    // Drop frame in order to respect frame rate constraint.
    return rtc::Optional<VideoFrame>();
  }

  rtc::Optional<VideoFrame> out_frame;
  if (out_height != frame.height() || out_width != frame.width()) {
    // Video adapter has requested a down-scale. Allocate a new buffer and
    // return scaled version.
    rtc::scoped_refptr<I420Buffer> scaled_buffer =
        I420Buffer::Create(out_width, out_height);
    scaled_buffer->ScaleFrom(*frame.video_frame_buffer()->ToI420());
    out_frame.emplace(
        VideoFrame(scaled_buffer, kVideoRotation_0, frame.timestamp_us()));
  } else {
    // No adaptations needed, just return the frame as is.
    out_frame.emplace(frame);
  }

  return out_frame;
}

void VideoCapturer::AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame>* sink,
                                    const rtc::VideoSinkWants& wants) {
  video_adapter_->OnResolutionFramerateRequest(
      wants.target_pixel_count, wants.max_pixel_count, wants.max_framerate_fps);
}

}  // namespace test
}  // namespace webrtc
