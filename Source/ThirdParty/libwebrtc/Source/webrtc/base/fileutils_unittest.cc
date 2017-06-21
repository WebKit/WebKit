/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "webrtc/base/fileutils.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/pathutils.h"
#include "webrtc/base/stream.h"

namespace rtc {

#if defined (WEBRTC_ANDROID)
// Fails on Android: https://bugs.chromium.org/p/webrtc/issues/detail?id=4364.
#define MAYBE_FilesystemTest DISABLED_FilesystemTest
#else
#define MAYBE_FilesystemTest FilesystemTest
#endif

// Make sure we can get a temp folder for the later tests.
TEST(MAYBE_FilesystemTest, GetTemporaryFolder) {
  Pathname path;
  EXPECT_TRUE(Filesystem::GetTemporaryFolder(path, true, nullptr));
}

}  // namespace rtc
