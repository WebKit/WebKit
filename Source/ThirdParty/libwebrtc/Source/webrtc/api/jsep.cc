/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/jsep.h"

namespace webrtc {

std::string IceCandidateInterface::server_url() const {
  return "";
}

size_t SessionDescriptionInterface::RemoveCandidates(
    const std::vector<cricket::Candidate>& candidates) {
  return 0;
}

void CreateSessionDescriptionObserver::OnFailure(RTCError error) {
  OnFailure(error.message());
}

void CreateSessionDescriptionObserver::OnFailure(const std::string& error) {
  OnFailure(RTCError(RTCErrorType::INTERNAL_ERROR, std::string(error)));
}

void SetSessionDescriptionObserver::OnFailure(RTCError error) {
  std::string message(error.message());
  OnFailure(message);
}

void SetSessionDescriptionObserver::OnFailure(const std::string& error) {
  OnFailure(RTCError(RTCErrorType::INTERNAL_ERROR, std::string(error)));
}

const char SessionDescriptionInterface::kOffer[] = "offer";
const char SessionDescriptionInterface::kPrAnswer[] = "pranswer";
const char SessionDescriptionInterface::kAnswer[] = "answer";

const char* SdpTypeToString(SdpType type) {
  switch (type) {
    case SdpType::kOffer:
      return SessionDescriptionInterface::kOffer;
    case SdpType::kPrAnswer:
      return SessionDescriptionInterface::kPrAnswer;
    case SdpType::kAnswer:
      return SessionDescriptionInterface::kAnswer;
  }
  return "";
}

absl::optional<SdpType> SdpTypeFromString(const std::string& type_str) {
  if (type_str == SessionDescriptionInterface::kOffer) {
    return SdpType::kOffer;
  } else if (type_str == SessionDescriptionInterface::kPrAnswer) {
    return SdpType::kPrAnswer;
  } else if (type_str == SessionDescriptionInterface::kAnswer) {
    return SdpType::kAnswer;
  } else {
    return absl::nullopt;
  }
}

}  // namespace webrtc
