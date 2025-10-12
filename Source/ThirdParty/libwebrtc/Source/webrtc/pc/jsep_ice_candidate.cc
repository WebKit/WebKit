/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/jsep_ice_candidate.h"

#include <cstddef>
#include <memory>
#include <string>

#include "absl/base/nullability.h"
#include "absl/strings/string_view.h"
#include "api/candidate.h"
#include "api/jsep.h"
#include "pc/webrtc_sdp.h"

// This file contains IceCandidate-related functions that are not
// included in api/jsep_ice_candidate.cc. Some of these link to SDP
// parsing/serializing functions, which some users may not want.
// TODO(bugs.webrtc.org/12330): Merge the two .cc files somehow.

namespace webrtc {

IceCandidate* CreateIceCandidate(const std::string& sdp_mid,
                                 int sdp_mline_index,
                                 const std::string& sdp,
                                 SdpParseError* error) {
  std::unique_ptr<IceCandidate> candidate =
      IceCandidate::Create(sdp_mid, sdp_mline_index, sdp, error);
  if (!candidate) {
    return nullptr;
  }
  return candidate.release();
}

std::unique_ptr<IceCandidate> CreateIceCandidate(const std::string& sdp_mid,
                                                 int sdp_mline_index,
                                                 const Candidate& candidate) {
  return std::make_unique<IceCandidate>(sdp_mid, sdp_mline_index, candidate);
}

// static
std::unique_ptr<IceCandidate> IceCandidate::Create(absl::string_view mid,
                                                   int sdp_mline_index,
                                                   absl::string_view sdp,
                                                   SdpParseError* absl_nullable
                                                       error /*= nullptr*/) {
  Candidate candidate;
  if (!SdpDeserializeCandidate(mid, sdp, &candidate, error)) {
    return nullptr;
  }
  return std::make_unique<IceCandidate>(mid, sdp_mline_index, candidate);
}

IceCandidateCollection IceCandidateCollection::Clone() const {
  IceCandidateCollection new_collection;
  for (const auto& candidate : candidates_) {
    new_collection.candidates_.push_back(std::make_unique<IceCandidate>(
        candidate->sdp_mid(), candidate->sdp_mline_index(),
        candidate->candidate()));
  }
  return new_collection;
}

std::string IceCandidate::ToString() const {
  return SdpSerializeCandidate(*this);
}

}  // namespace webrtc
