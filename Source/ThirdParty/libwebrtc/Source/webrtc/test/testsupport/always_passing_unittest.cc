/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/gtest.h"

namespace webrtc {

// A test that always passes. Useful when all tests in a executable are
// disabled, since a gtest returns exit code 1 if no tests have executed.
TEST(AlwaysPassingTest, AlwaysPassingTest) {}

}  // namespace webrtc
