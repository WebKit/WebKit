/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Interfaces matching the draft-ietf-rtcweb-jsep-01.

#ifndef WEBRTC_API_JSEP_H_
#define WEBRTC_API_JSEP_H_

#include <string>
#include <vector>

#include "webrtc/base/basictypes.h"
#include "webrtc/base/refcount.h"

namespace cricket {
class Candidate;
class SessionDescription;
}  // namespace cricket

namespace webrtc {

struct SdpParseError {
 public:
  // The sdp line that causes the error.
  std::string line;
  // Explains the error.
  std::string description;
};

// Class representation of an ICE candidate.
// An instance of this interface is supposed to be owned by one class at
// a time and is therefore not expected to be thread safe.
class IceCandidateInterface {
 public:
  virtual ~IceCandidateInterface() {}
  /// If present, this contains the identierfier of the "media stream
  // identification" as defined in [RFC 3388] for m-line this candidate is
  // assocated with.
  virtual std::string sdp_mid() const = 0;
  // This indeicates the index (starting at zero) of m-line in the SDP this
  // candidate is assocated with.
  virtual int sdp_mline_index() const = 0;
  virtual const cricket::Candidate& candidate() const = 0;
  // Creates a SDP-ized form of this candidate.
  virtual bool ToString(std::string* out) const = 0;
};

// Creates a IceCandidateInterface based on SDP string.
// Returns NULL if the sdp string can't be parsed.
// |error| can be NULL if doesn't care about the failure reason.
IceCandidateInterface* CreateIceCandidate(const std::string& sdp_mid,
                                          int sdp_mline_index,
                                          const std::string& sdp,
                                          SdpParseError* error);

// This class represents a collection of candidates for a specific m-line.
// This class is used in SessionDescriptionInterface to represent all known
// candidates for a certain m-line.
class IceCandidateCollection {
 public:
  virtual ~IceCandidateCollection() {}
  virtual size_t count() const = 0;
  // Returns true if an equivalent |candidate| exist in the collection.
  virtual bool HasCandidate(const IceCandidateInterface* candidate) const = 0;
  virtual const IceCandidateInterface* at(size_t index) const = 0;
};

// Class representation of a Session description.
// An instance of this interface is supposed to be owned by one class at
// a time and is therefore not expected to be thread safe.
class SessionDescriptionInterface {
 public:
  // Supported types:
  static const char kOffer[];
  static const char kPrAnswer[];
  static const char kAnswer[];

  virtual ~SessionDescriptionInterface() {}
  virtual cricket::SessionDescription* description() = 0;
  virtual const cricket::SessionDescription* description() const = 0;
  // Get the session id and session version, which are defined based on
  // RFC 4566 for the SDP o= line.
  virtual std::string session_id() const = 0;
  virtual std::string session_version() const = 0;
  virtual std::string type() const = 0;
  // Adds the specified candidate to the description.
  // Ownership is not transferred.
  // Returns false if the session description does not have a media section that
  // corresponds to the |candidate| label.
  virtual bool AddCandidate(const IceCandidateInterface* candidate) = 0;
  // Removes the candidates from the description.
  // Returns the number of candidates removed.
  virtual size_t RemoveCandidates(
      const std::vector<cricket::Candidate>& candidates) { return 0; }

  // Returns the number of m- lines in the session description.
  virtual size_t number_of_mediasections() const = 0;
  // Returns a collection of all candidates that belong to a certain m-line
  virtual const IceCandidateCollection* candidates(
      size_t mediasection_index) const = 0;
  // Serializes the description to SDP.
  virtual bool ToString(std::string* out) const = 0;
};

// Creates a SessionDescriptionInterface based on SDP string and the type.
// Returns NULL if the sdp string can't be parsed or the type is unsupported.
// |error| can be NULL if doesn't care about the failure reason.
SessionDescriptionInterface* CreateSessionDescription(const std::string& type,
                                                      const std::string& sdp,
                                                      SdpParseError* error);

// Jsep CreateOffer and CreateAnswer callback interface.
class CreateSessionDescriptionObserver : public rtc::RefCountInterface {
 public:
  // The implementation of the CreateSessionDescriptionObserver takes
  // the ownership of the |desc|.
  virtual void OnSuccess(SessionDescriptionInterface* desc) = 0;
  virtual void OnFailure(const std::string& error) = 0;

 protected:
  ~CreateSessionDescriptionObserver() {}
};

// Jsep SetLocalDescription and SetRemoteDescription callback interface.
class SetSessionDescriptionObserver : public rtc::RefCountInterface {
 public:
  virtual void OnSuccess() = 0;
  virtual void OnFailure(const std::string& error) = 0;

 protected:
  ~SetSessionDescriptionObserver() {}
};

}  // namespace webrtc

#endif  // WEBRTC_API_JSEP_H_
