/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Implements the IceCandidateInterface.

#ifndef WEBRTC_API_JSEPICECANDIDATE_H_
#define WEBRTC_API_JSEPICECANDIDATE_H_

#include <string>

#include "webrtc/api/jsep.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/p2p/base/candidate.h"

namespace webrtc {

class JsepIceCandidate : public IceCandidateInterface {
 public:
  JsepIceCandidate(const std::string& sdp_mid, int sdp_mline_index);
  JsepIceCandidate(const std::string& sdp_mid, int sdp_mline_index,
                   const cricket::Candidate& candidate);
  ~JsepIceCandidate();
  // |error| can be NULL if don't care about the failure reason.
  bool Initialize(const std::string& sdp, SdpParseError* err);
  void SetCandidate(const cricket::Candidate& candidate) {
    candidate_ = candidate;
  }

  virtual std::string sdp_mid() const { return sdp_mid_; }
  virtual int sdp_mline_index() const { return sdp_mline_index_; }
  virtual const cricket::Candidate& candidate() const {
    return candidate_;
  }

  virtual bool ToString(std::string* out) const;

 private:
  std::string sdp_mid_;
  int sdp_mline_index_;
  cricket::Candidate candidate_;

  RTC_DISALLOW_COPY_AND_ASSIGN(JsepIceCandidate);
};

// Implementation of IceCandidateCollection.
// This implementation stores JsepIceCandidates.
class JsepCandidateCollection : public IceCandidateCollection {
 public:
  JsepCandidateCollection() {}
  // Move constructor is defined so that a vector of JsepCandidateCollections
  // can be resized.
  JsepCandidateCollection(JsepCandidateCollection&& o)
      : candidates_(std::move(o.candidates_)) {}
  ~JsepCandidateCollection();
  virtual size_t count() const {
    return candidates_.size();
  }
  virtual bool HasCandidate(const IceCandidateInterface* candidate) const;
  // Adds and takes ownership of the JsepIceCandidate.
  virtual void add(JsepIceCandidate* candidate) {
    candidates_.push_back(candidate);
  }
  virtual const IceCandidateInterface* at(size_t index) const {
    return candidates_[index];
  }
  // Removes the candidate that has a matching address and protocol.
  // Returns the number of candidates that were removed.
  size_t remove(const cricket::Candidate& candidate);

 private:
  std::vector<JsepIceCandidate*> candidates_;

  RTC_DISALLOW_COPY_AND_ASSIGN(JsepCandidateCollection);
};

}  // namespace webrtc

#endif  // WEBRTC_API_JSEPICECANDIDATE_H_
