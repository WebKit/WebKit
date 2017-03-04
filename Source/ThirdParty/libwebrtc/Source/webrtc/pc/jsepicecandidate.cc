/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/jsepicecandidate.h"

#include <vector>

#include "webrtc/base/stringencode.h"
#include "webrtc/pc/webrtcsdp.h"

namespace webrtc {

IceCandidateInterface* CreateIceCandidate(const std::string& sdp_mid,
                                          int sdp_mline_index,
                                          const std::string& sdp,
                                          SdpParseError* error) {
  JsepIceCandidate* jsep_ice = new JsepIceCandidate(sdp_mid, sdp_mline_index);
  if (!jsep_ice->Initialize(sdp, error)) {
    delete jsep_ice;
    return NULL;
  }
  return jsep_ice;
}

JsepIceCandidate::JsepIceCandidate(const std::string& sdp_mid,
                                   int sdp_mline_index)
    : sdp_mid_(sdp_mid),
      sdp_mline_index_(sdp_mline_index) {
}

JsepIceCandidate::JsepIceCandidate(const std::string& sdp_mid,
                                   int sdp_mline_index,
                                   const cricket::Candidate& candidate)
    : sdp_mid_(sdp_mid),
      sdp_mline_index_(sdp_mline_index),
      candidate_(candidate) {
}

JsepIceCandidate::~JsepIceCandidate() {
}

bool JsepIceCandidate::Initialize(const std::string& sdp, SdpParseError* err) {
  return SdpDeserializeCandidate(sdp, this, err);
}

bool JsepIceCandidate::ToString(std::string* out) const {
  if (!out)
    return false;
  *out = SdpSerializeCandidate(*this);
  return !out->empty();
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
