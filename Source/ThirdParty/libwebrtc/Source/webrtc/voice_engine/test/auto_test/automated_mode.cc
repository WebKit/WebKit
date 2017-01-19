/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/gtest.h"
#include "webrtc/test/testsupport/fileutils.h"

void InitializeGoogleTest(int* argc, char** argv) {
  // Initialize WebRTC testing framework so paths to resources can be resolved.
  webrtc::test::SetExecutablePath(argv[0]);
  testing::InitGoogleTest(argc, argv);
}

int RunInAutomatedMode() {
  return RUN_ALL_TESTS();
}
