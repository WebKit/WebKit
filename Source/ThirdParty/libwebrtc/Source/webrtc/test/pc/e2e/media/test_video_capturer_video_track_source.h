/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_MEDIA_TEST_VIDEO_CAPTURER_VIDEO_TRACK_SOURCE_H_
#define TEST_PC_E2E_MEDIA_TEST_VIDEO_CAPTURER_VIDEO_TRACK_SOURCE_H_

#include <memory>
#include <utility>

#include "api/video/video_frame.h"
#include "api/video/video_source_interface.h"
#include "pc/video_track_source.h"
#include "test/test_video_capturer.h"

namespace webrtc {
namespace webrtc_pc_e2e {

class TestVideoCapturerVideoTrackSource : public VideoTrackSource {
 public:
  TestVideoCapturerVideoTrackSource(
      std::unique_ptr<test::TestVideoCapturer> video_capturer,
      bool is_screencast)
      : VideoTrackSource(/*remote=*/false),
        video_capturer_(std::move(video_capturer)),
        is_screencast_(is_screencast) {}

  ~TestVideoCapturerVideoTrackSource() = default;

  void Start() { SetState(kLive); }

  void Stop() { SetState(kMuted); }

  bool is_screencast() const override { return is_screencast_; }

 protected:
  rtc::VideoSourceInterface<VideoFrame>* source() override {
    return video_capturer_.get();
  }

 private:
  std::unique_ptr<test::TestVideoCapturer> video_capturer_;
  const bool is_screencast_;
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_MEDIA_TEST_VIDEO_CAPTURER_VIDEO_TRACK_SOURCE_H_
