/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/jsepicecandidate.h"

#include <algorithm>
#include <utility>

namespace webrtc {

std::string JsepIceCandidate::sdp_mid() const {
  return sdp_mid_;
}

int JsepIceCandidate::sdp_mline_index() const {
  return sdp_mline_index_;
}

const cricket::Candidate& JsepIceCandidate::candidate() const {
  return candidate_;
}

std::string JsepIceCandidate::server_url() const {
  return candidate_.url();
}

JsepCandidateCollection::JsepCandidateCollection() = default;

JsepCandidateCollection::JsepCandidateCollection(JsepCandidateCollection&& o)
    : candidates_(std::move(o.candidates_)) {}

size_t JsepCandidateCollection::count() const {
  return candidates_.size();
}

void JsepCandidateCollection::add(JsepIceCandidate* candidate) {
  candidates_.push_back(candidate);
}

const IceCandidateInterface* JsepCandidateCollection::at(size_t index) const {
  return candidates_[index];
}

JsepCandidateCollection::~JsepCandidateCollection() {
  for (std::vector<JsepIceCandidate*>::iterator it = candidates_.begin();
       it != candidates_.end(); ++it) {
    delete *it;
  }
}

bool JsepCandidateCollection::HasCandidate(
    const IceCandidateInterface* candidate) const {
  bool ret = false;
  for (std::vector<JsepIceCandidate*>::const_iterator it = candidates_.begin();
       it != candidates_.end(); ++it) {
    if ((*it)->sdp_mid() == candidate->sdp_mid() &&
        (*it)->sdp_mline_index() == candidate->sdp_mline_index() &&
        (*it)->candidate().IsEquivalent(candidate->candidate())) {
      ret = true;
      break;
    }
  }
  return ret;
}

size_t JsepCandidateCollection::remove(const cricket::Candidate& candidate) {
  auto iter = std::find_if(candidates_.begin(), candidates_.end(),
                           [candidate](JsepIceCandidate* c) {
                             return candidate.MatchesForRemoval(c->candidate());
                           });
  if (iter != candidates_.end()) {
    delete *iter;
    candidates_.erase(iter);
    return 1;
  }
  return 0;
}

}  // namespace webrtc
