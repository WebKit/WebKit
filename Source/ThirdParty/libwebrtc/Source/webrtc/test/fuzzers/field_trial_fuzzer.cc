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
#include <cstdint>

#include "absl/strings/string_view.h"
#include "api/field_trials.h"

namespace webrtc {

void FuzzOneInput(const uint8_t* data, size_t size) {
  // FieldTrials constructor crashes on invalid input.
  // FieldTrials::Create validates input and returns nullptr when it is invalid,
  // but should never crash.
  FieldTrials::Create(
      absl::string_view(reinterpret_cast<const char*>(data), size));
}

}  // namespace webrtc
