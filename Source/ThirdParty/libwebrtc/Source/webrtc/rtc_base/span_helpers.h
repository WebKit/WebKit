/*
 *  Copyright 2026 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SPAN_HELPERS_H_
#define RTC_BASE_SPAN_HELPERS_H_

#include <cstdint>
#include <span>
#include <string>

#include "absl/strings/string_view.h"

namespace webrtc {

// Converters between std::span<uint8_t> and std::span<char>.
// These are deliberately not templated; we don't think that we need
// more generic versions of these, and the need for them could be a code
// smell indicating the need for defining whether variables are intended
// to be text (char) or binary blobs (uint8_t).

// If there turns out to be a need for many more variants, templates can
// be introduced in the future.
inline std::span<char> AsWritableCharSpan(std::span<uint8_t> span) {
  return std::span<char>(reinterpret_cast<char*>(span.data()), span.size());
}

inline std::span<const char> AsCharSpan(std::span<const uint8_t> span) {
  return std::span<const char>(reinterpret_cast<const char*>(span.data()),
                               span.size());
}

inline std::span<uint8_t> AsWritableUint8Span(std::span<char> span) {
  return std::span<uint8_t>(reinterpret_cast<uint8_t*>(span.data()),
                            span.size());
}

inline std::span<const uint8_t> AsUint8Span(std::span<const char> span) {
  return std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(span.data()),
                                  span.size());
}

inline std::span<const uint8_t> AsUint8Span(absl::string_view s) {
  return std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(s.data()),
                                  s.size());
}

inline std::span<const uint8_t> AsUint8Span(const std::string& s) {
  return AsUint8Span(absl::string_view(s));
}

inline absl::string_view AsStringView(std::span<const uint8_t> span) {
  return absl::string_view(reinterpret_cast<const char*>(span.data()),
                           span.size());
}

}  // namespace webrtc

#endif  // RTC_BASE_SPAN_HELPERS_H_
