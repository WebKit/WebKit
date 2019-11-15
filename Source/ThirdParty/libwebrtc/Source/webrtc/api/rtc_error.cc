/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/rtc_error.h"

#include "absl/strings/string_view.h"
#include "rtc_base/arraysize.h"

namespace {

const absl::string_view kRTCErrorTypeNames[] = {
    "NONE",
    "UNSUPPORTED_OPERATION",
    "UNSUPPORTED_PARAMETER",
    "INVALID_PARAMETER",
    "INVALID_RANGE",
    "SYNTAX_ERROR",
    "INVALID_STATE",
    "INVALID_MODIFICATION",
    "NETWORK_ERROR",
    "RESOURCE_EXHAUSTED",
    "INTERNAL_ERROR",
};
static_assert(static_cast<int>(webrtc::RTCErrorType::INTERNAL_ERROR) ==
                  (arraysize(kRTCErrorTypeNames) - 1),
              "kRTCErrorTypeNames must have as many strings as RTCErrorType "
              "has values.");

}  // namespace

namespace webrtc {

RTCError::RTCError(RTCError&& other) = default;
RTCError& RTCError::operator=(RTCError&& other) = default;

// static
RTCError RTCError::OK() {
  return RTCError();
}

const char* RTCError::message() const {
  return message_.c_str();
}

void RTCError::set_message(std::string message) {
  message_ = std::move(message);
}

absl::string_view ToString(RTCErrorType error) {
  int index = static_cast<int>(error);
  return kRTCErrorTypeNames[index];
}

}  // namespace webrtc
