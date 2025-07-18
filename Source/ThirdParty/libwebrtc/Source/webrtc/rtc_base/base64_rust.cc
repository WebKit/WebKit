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
#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "rtc_base/base64.h"
#include "rtc_base/base64.rs.h"
#include "third_party/rust/chromium_crates_io/vendor/cxx-v1/include/cxx.h"

namespace webrtc {

std::string Base64Encode(absl::string_view data) {
  rust::Slice<const uint8_t> input_slice(
      reinterpret_cast<const uint8_t*>(data.data()), data.size());
  rust::Vec<uint8_t> output_slice;
  rust::String str = rs_base64_encode(input_slice);
  return std::string(str);
}

std::optional<std::string> Base64Decode(absl::string_view data,
                                        Base64DecodeOptions options) {
  rust::Slice<const uint8_t> input_slice(
      reinterpret_cast<const uint8_t*>(data.data()), data.size());
  std::string output;
  if (!rs_base64_decode(input_slice, options, output)) {
    return std::nullopt;
  }
  return output;
}

}  // namespace webrtc
