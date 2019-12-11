/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TEST_FRAMEGENERATORCAPTURERVIDEOTRACKSOURCE_H_
#define PC_TEST_FRAMEGENERATORCAPTURERVIDEOTRACKSOURCE_H_

#include <memory>

#include "pc/videotracksource.h"
#include "test/frame_generator_capturer.h"

namespace webrtc {

// Implements a VideoTrackSourceInterface to be used for creating VideoTracks.
// The video source is generated using a FrameGeneratorCapturer, specifically
// a SquareGenerator that generates frames with randomly sized and colored
// squares.
class FrameGeneratorCapturerVideoTrackSource : public VideoTrackSource {
 public:
  static const int kDefaultFramesPerSecond = 30;
  static const int kDefaultWidth = 640;
  static const int kDefaultHeight = 480;
  static const int kNumSquaresGenerated = 50;

  struct Config {
    int frames_per_second = kDefaultFramesPerSecond;
    int width = kDefaultWidth;
    int height = kDefaultHeight;
    int num_squares_generated = 50;
  };

  explicit FrameGeneratorCapturerVideoTrackSource(Clock* clock)
      : FrameGeneratorCapturerVideoTrackSource(Config(), clock) {}

  FrameGeneratorCapturerVideoTrackSource(Config config, Clock* clock)
      : VideoTrackSource(false /* remote */) {
    video_capturer_.reset(test::FrameGeneratorCapturer::Create(
        config.width, config.height, absl::nullopt,
        config.num_squares_generated, config.frames_per_second, clock));
  }

  ~FrameGeneratorCapturerVideoTrackSource() = default;

  void Start() {
    video_capturer_->Start();
    SetState(kLive);
  }

  void Stop() {
    video_capturer_->Stop();
    SetState(kMuted);
  }

 protected:
  rtc::VideoSourceInterface<VideoFrame>* source() override {
    return video_capturer_.get();
  }

 private:
  std::unique_ptr<test::FrameGeneratorCapturer> video_capturer_;
};

}  // namespace webrtc

#endif  // PC_TEST_FRAMEGENERATORCAPTURERVIDEOTRACKSOURCE_H_
