/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/fakevideorenderer.h"

namespace cricket {

FakeVideoRenderer::FakeVideoRenderer() = default;

void FakeVideoRenderer::OnFrame(const webrtc::VideoFrame& frame) {
  rtc::CritScope cs(&crit_);
  // TODO(zhurunz) Check with VP8 team to see if we can remove this
  // tolerance on Y values. Some unit tests produce Y values close
  // to 16 rather than close to zero, for supposedly black frames.
  // Largest value observed is 34, e.g., running
  // PeerConnectionIntegrationTest.SendAndReceive16To9AspectRatio.
  black_frame_ = CheckFrameColorYuv(0, 48, 128, 128, 128, 128, &frame);
  // Treat unexpected frame size as error.
  ++num_rendered_frames_;
  width_ = frame.width();
  height_ = frame.height();
  rotation_ = frame.rotation();
  timestamp_us_ = frame.timestamp_us();
}

}  // namespace cricket
