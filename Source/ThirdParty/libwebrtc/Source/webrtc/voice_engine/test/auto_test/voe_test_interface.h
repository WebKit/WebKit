/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  Interface for starting test
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_TEST_INTERFACE_H
#define WEBRTC_VOICE_ENGINE_VOE_TEST_INTERFACE_H

#include "webrtc/common_types.h"

namespace voetest {
// TODO(andrew): Using directives not permitted.
using namespace webrtc;

// TestType enumerator
enum TestType {
  Invalid = -1,
  Standard = 0,

  Stress = 2,

  CPU = 4
};

// Main test function
int runAutoTest(TestType testType);

}  // namespace voetest
#endif // WEBRTC_VOICE_ENGINE_VOE_TEST_INTERFACE_H
