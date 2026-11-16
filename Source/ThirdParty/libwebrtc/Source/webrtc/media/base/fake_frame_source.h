/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_BASE_FAKE_FRAME_SOURCE_H_
#define MEDIA_BASE_FAKE_FRAME_SOURCE_H_

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/video_frame.h"
#include "api/video/video_rotation.h"

namespace webrtc {

class FakeFrameSource {
 public:
  FakeFrameSource(int width,
                  int height,
                  TimeDelta interval,
                  Timestamp timestamp);

  VideoRotation GetRotation() const;
  void SetRotation(VideoRotation rotation);

  VideoFrame GetFrame();
  VideoFrame GetFrameRotationApplied();

  // Override configuration.
  VideoFrame GetFrame(int width,
                      int height,
                      VideoRotation rotation,
                      TimeDelta interval);

 private:
  const int width_;
  const int height_;
  const TimeDelta interval_;

  VideoRotation rotation_ = kVideoRotation_0;
  Timestamp next_timestamp_;
};

}  //  namespace webrtc


#endif  // MEDIA_BASE_FAKE_FRAME_SOURCE_H_
