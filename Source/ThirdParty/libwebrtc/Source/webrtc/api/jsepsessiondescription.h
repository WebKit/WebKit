/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Implements the SessionDescriptionInterface.

#ifndef WEBRTC_API_JSEPSESSIONDESCRIPTION_H_
#define WEBRTC_API_JSEPSESSIONDESCRIPTION_H_

#include <memory>
#include <string>
#include <vector>

#include "webrtc/api/jsep.h"
#include "webrtc/api/jsepicecandidate.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/p2p/base/candidate.h"

namespace cricket {
class SessionDescription;
}

namespace webrtc {

class JsepSessionDescription : public SessionDescriptionInterface {
 public:
  explicit JsepSessionDescription(const std::string& type);
  virtual ~JsepSessionDescription();

  // |error| can be NULL if don't care about the failure reason.
  bool Initialize(const std::string& sdp, SdpParseError* error);

  // Takes ownership of |description|.
  bool Initialize(cricket::SessionDescription* description,
      const std::string& session_id,
      const std::string& session_version);

  virtual cricket::SessionDescription* description() {
    return description_.get();
  }
  virtual const cricket::SessionDescription* description() const {
    return description_.get();
  }
  virtual std::string session_id() const {
    return session_id_;
  }
  virtual std::string session_version() const {
    return session_version_;
  }
  virtual std::string type() const {
    return type_;
  }
  // Allow changing the type. Used for testing.
  void set_type(const std::string& type) { type_ = type; }
  virtual bool AddCandidate(const IceCandidateInterface* candidate);
  virtual size_t RemoveCandidates(
      const std::vector<cricket::Candidate>& candidates);
  virtual size_t number_of_mediasections() const;
  virtual const IceCandidateCollection* candidates(
      size_t mediasection_index) const;
  virtual bool ToString(std::string* out) const;

  static const int kDefaultVideoCodecId;
  static const char kDefaultVideoCodecName[];

 private:
  std::unique_ptr<cricket::SessionDescription> description_;
  std::string session_id_;
  std::string session_version_;
  std::string type_;
  std::vector<JsepCandidateCollection> candidate_collection_;

  bool GetMediasectionIndex(const IceCandidateInterface* candidate,
                            size_t* index);
  int GetMediasectionIndex(const cricket::Candidate& candidate);

  RTC_DISALLOW_COPY_AND_ASSIGN(JsepSessionDescription);
};

}  // namespace webrtc

#endif  // WEBRTC_API_JSEPSESSIONDESCRIPTION_H_
