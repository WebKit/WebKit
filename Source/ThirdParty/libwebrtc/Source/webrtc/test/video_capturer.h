/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_VIDEO_CAPTURER_H_
#define TEST_VIDEO_CAPTURER_H_

#include <stddef.h>

#include <memory>

#include "api/optional.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "media/base/videoadapter.h"
#include "media/base/videosourceinterface.h"
#include "rtc_base/criticalsection.h"

namespace cricket {
class VideoAdapter;
}  // namespace cricket

namespace webrtc {
class Clock;
namespace test {

class VideoCapturer : public rtc::VideoSourceInterface<VideoFrame> {
 public:
  VideoCapturer();
  virtual ~VideoCapturer();

  virtual void Start() = 0;
  virtual void Stop() = 0;

  void AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override;

 protected:
  rtc::Optional<VideoFrame> AdaptFrame(const VideoFrame& frame);
  rtc::VideoSinkWants GetSinkWants();

 private:
  const std::unique_ptr<cricket::VideoAdapter> video_adapter_;
};
}  // namespace test
}  // namespace webrtc

#endif  // TEST_VIDEO_CAPTURER_H_
