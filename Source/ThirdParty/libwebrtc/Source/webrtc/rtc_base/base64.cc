/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/base64.h"

#include <optional>
#include <string>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/strings/ascii.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"

namespace webrtc {

namespace {

bool IsStrictBase64(absl::string_view data) {
  // Strict base64 must be a multiple of 4 bytes and have no whitespace.
  return data.size() % 4 == 0 && absl::c_none_of(data, absl::ascii_isspace);
}
}  // namespace

std::string Base64Encode(absl::string_view data) {
  return absl::Base64Escape(data);
}

std::optional<std::string> Base64Decode(absl::string_view data,
                                        Base64DecodeOptions options) {
  // absl::Base64Unescape is forgiving. Return nullopt if the input is not
  // strict.
  if (options == Base64DecodeOptions::kStrict && !IsStrictBase64(data)) {
    return std::nullopt;
  }

  std::string dest;
  return absl::Base64Unescape(data, &dest) ? std::make_optional(std::move(dest))
                                           : std::nullopt;
}

}  // namespace webrtc
