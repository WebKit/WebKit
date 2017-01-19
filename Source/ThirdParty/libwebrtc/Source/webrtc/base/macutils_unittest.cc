/*
 *  Copyright 2009 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/gunit.h"
#include "webrtc/base/macutils.h"

TEST(MacUtilsTest, GetOsVersionName) {
  rtc::MacOSVersionName ver = rtc::GetOSVersionName();
  LOG(LS_INFO) << "GetOsVersionName " << ver;
  EXPECT_NE(rtc::kMacOSUnknown, ver);
}
