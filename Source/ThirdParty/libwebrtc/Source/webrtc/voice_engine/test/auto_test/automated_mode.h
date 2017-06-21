/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_VOICE_ENGINE_MAIN_TEST_AUTO_TEST_AUTOMATED_MODE_H_
#define SRC_VOICE_ENGINE_MAIN_TEST_AUTO_TEST_AUTOMATED_MODE_H_

namespace webrtc {
namespace voetest {

void InitializeGoogleTest(int* argc, char** argv);
int RunInAutomatedMode();

}  // namespace voetest
}  // namespace webrtc

#endif  // SRC_VOICE_ENGINE_MAIN_TEST_AUTO_TEST_AUTOMATED_MODE_H_
