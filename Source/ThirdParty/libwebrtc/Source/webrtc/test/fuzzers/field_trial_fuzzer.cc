/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstddef>

#include "api/field_trials.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {

void FuzzOneInput(FuzzDataHelper fuzz_data) {
  // FieldTrials constructor crashes on invalid input.
  // FieldTrials::Create validates input and returns nullptr when it is invalid,
  // but should never crash.
  FieldTrials::Create(fuzz_data.ReadString());
}

}  // namespace webrtc
