/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/adaptation/test/fake_frame_rate_provider.h"

#include "test/gmock.h"

using ::testing::Return;

namespace webrtc {

FakeFrameRateProvider::FakeFrameRateProvider() {
  set_fps(0);
}

void FakeFrameRateProvider::set_fps(int fps) {
  EXPECT_CALL(*this, GetInputFrameRate()).WillRepeatedly(Return(fps));
}

}  // namespace webrtc
