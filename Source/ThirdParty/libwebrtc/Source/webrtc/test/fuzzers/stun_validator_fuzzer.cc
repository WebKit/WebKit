/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stddef.h>
#include <stdint.h>

#include "absl/strings/string_view.h"
#include "api/transport/stun.h"
#include "rtc_base/span_helpers.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {
void FuzzOneInput(FuzzDataHelper fuzz_data) {
  absl::string_view message = fuzz_data.ReadString();

  std::span<const uint8_t> data = AsUint8Span(message);
  StunMessage::ValidateFingerprint(data);
  StunMessage::ValidateMessageIntegrityForTesting("", data);
}
}  // namespace webrtc
