/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/rtcerror.h"

#include "rtc_base/arraysize.h"

namespace {

static const char* const kRTCErrorTypeNames[] = {
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

// TODO(jonasolsson): Change to use absl::string_view when it's available.
std::string ToString(RTCErrorType error) {
  int index = static_cast<int>(error);
  return std::string(kRTCErrorTypeNames[index]);
}

}  // namespace webrtc
