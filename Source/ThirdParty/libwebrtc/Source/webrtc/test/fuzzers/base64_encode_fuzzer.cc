/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string>

#include "rtc_base/base64.h"
#include "rtc_base/checks.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {

void FuzzOneInput(FuzzDataHelper fuzz_data) {
  std::span<const uint8_t> raw = fuzz_data.ReadRemaining();
  std::optional<std::string> decoded_encoded_data =
      Base64Decode(Base64Encode(raw));
  RTC_CHECK(decoded_encoded_data.has_value());
  RTC_CHECK_EQ(
      std::memcmp(raw.data(), decoded_encoded_data->data(), raw.size()), 0);
}

}  // namespace webrtc
