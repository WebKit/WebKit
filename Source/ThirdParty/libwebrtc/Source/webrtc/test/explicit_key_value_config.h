/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_EXPLICIT_KEY_VALUE_CONFIG_H_
#define TEST_EXPLICIT_KEY_VALUE_CONFIG_H_

#include "api/field_trials.h"

namespace webrtc {
namespace test {

// TODO: bugs.webrtc.org/42220378 - Use `FieldTrials` directly.
using ExplicitKeyValueConfig = FieldTrials;

}  // namespace test
}  // namespace webrtc

#endif  // TEST_EXPLICIT_KEY_VALUE_CONFIG_H_
