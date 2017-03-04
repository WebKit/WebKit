/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/rtcerror.h"

#include "webrtc/base/arraysize.h"

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

RTCError::RTCError(RTCError&& other)
    : type_(other.type_), have_string_message_(other.have_string_message_) {
  if (have_string_message_) {
    new (&string_message_) std::string(std::move(other.string_message_));
  } else {
    static_message_ = other.static_message_;
  }
}

RTCError& RTCError::operator=(RTCError&& other) {
  type_ = other.type_;
  if (other.have_string_message_) {
    set_message(std::move(other.string_message_));
  } else {
    set_message(other.static_message_);
  }
  return *this;
}

RTCError::~RTCError() {
  // If we hold a message string that was built, rather than a static string,
  // we need to delete it.
  if (have_string_message_) {
    string_message_.~basic_string();
  }
}

// static
RTCError RTCError::OK() {
  return RTCError();
}

const char* RTCError::message() const {
  if (have_string_message_) {
    return string_message_.c_str();
  } else {
    return static_message_;
  }
}

void RTCError::set_message(const char* message) {
  if (have_string_message_) {
    string_message_.~basic_string();
    have_string_message_ = false;
  }
  static_message_ = message;
}

void RTCError::set_message(std::string&& message) {
  if (!have_string_message_) {
    new (&string_message_) std::string(std::move(message));
    have_string_message_ = true;
  } else {
    string_message_ = message;
  }
}

std::ostream& operator<<(std::ostream& stream, RTCErrorType error) {
  int index = static_cast<int>(error);
  return stream << kRTCErrorTypeNames[index];
}

}  // namespace webrtc
