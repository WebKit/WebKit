/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains declarations of interfaces that wrap SDP-related
// constructs; session descriptions and ICE candidates. The inner "cricket::"
// objects shouldn't be accessed directly; the intention is that an application
// using the PeerConnection API only creates these objects from strings, and
// them passes them into the PeerConnection.
//
// Though in the future, we're planning to provide an SDP parsing API, with a
// structure more friendly than cricket::SessionDescription.

#ifndef WEBRTC_API_JSEP_H_
#define WEBRTC_API_JSEP_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "webrtc/base/export.h"
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
//
// An instance of this interface is supposed to be owned by one class at
// a time and is therefore not expected to be thread safe.
//
// An instance can be created by CreateIceCandidate.
class IceCandidateInterface {
 public:
  virtual ~IceCandidateInterface() {}
  // If present, this is the value of the "a=mid" attribute of the candidate's
  // m= section in SDP, which identifies the m= section.
  virtual std::string sdp_mid() const = 0;
  // This indicates the index (starting at zero) of m= section this candidate
  // is assocated with. Needed when an endpoint doesn't support MIDs.
  virtual int sdp_mline_index() const = 0;
  // Only for use internally.
  virtual const cricket::Candidate& candidate() const = 0;
  // The URL of the ICE server which this candidate was gathered from.
  // TODO(zhihuang): Remove the default implementation once the subclasses
  // implement this method.
  virtual std::string server_url() const { return ""; }
  // Creates a SDP-ized form of this candidate.
  virtual bool ToString(std::string* out) const = 0;
};

// Creates a IceCandidateInterface based on SDP string.
// Returns null if the sdp string can't be parsed.
// |error| may be null.
WEBRTC_DYLIB_EXPORT IceCandidateInterface* CreateIceCandidate(const std::string& sdp_mid,
                                          int sdp_mline_index,
                                          const std::string& sdp,
                                          SdpParseError* error);

// This class represents a collection of candidates for a specific m= section.
// Used in SessionDescriptionInterface.
class IceCandidateCollection {
 public:
  virtual ~IceCandidateCollection() {}
  virtual size_t count() const = 0;
  // Returns true if an equivalent |candidate| exist in the collection.
  virtual bool HasCandidate(const IceCandidateInterface* candidate) const = 0;
  virtual const IceCandidateInterface* at(size_t index) const = 0;
};

// Class representation of an SDP session description.
//
// An instance of this interface is supposed to be owned by one class at a time
// and is therefore not expected to be thread safe.
//
// An instance can be created by CreateSessionDescription.
class WEBRTC_DYLIB_EXPORT SessionDescriptionInterface {
 public:
  // Supported types:
  static const char kOffer[];
  static const char kPrAnswer[];
  static const char kAnswer[];

  virtual ~SessionDescriptionInterface() {}

  // Only for use internally.
  virtual cricket::SessionDescription* description() = 0;
  virtual const cricket::SessionDescription* description() const = 0;

  // Get the session id and session version, which are defined based on
  // RFC 4566 for the SDP o= line.
  virtual std::string session_id() const = 0;
  virtual std::string session_version() const = 0;

  // kOffer/kPrAnswer/kAnswer
  virtual std::string type() const = 0;

  // Adds the specified candidate to the description.
  //
  // Ownership is not transferred.
  //
  // Returns false if the session description does not have a media section
  // that corresponds to |candidate.sdp_mid()| or
  // |candidate.sdp_mline_index()|.
  virtual bool AddCandidate(const IceCandidateInterface* candidate) = 0;

  // Removes the candidates from the description, if found.
  //
  // Returns the number of candidates removed.
  virtual size_t RemoveCandidates(
      const std::vector<cricket::Candidate>&) { return 0; }

  // Returns the number of m= sections in the session description.
  virtual size_t number_of_mediasections() const = 0;

  // Returns a collection of all candidates that belong to a certain m=
  // section.
  virtual const IceCandidateCollection* candidates(
      size_t mediasection_index) const = 0;

  // Serializes the description to SDP.
  virtual bool ToString(std::string* out) const = 0;
};

// Creates a SessionDescriptionInterface based on the SDP string and the type.
// Returns null if the sdp string can't be parsed or the type is unsupported.
// |error| may be null.
WEBRTC_DYLIB_EXPORT SessionDescriptionInterface* CreateSessionDescription(const std::string& type,
                                                      const std::string& sdp,
                                                      SdpParseError* error);

// CreateOffer and CreateAnswer callback interface.
class CreateSessionDescriptionObserver : public rtc::RefCountInterface {
 public:
  // This callback transfers the ownership of the |desc|.
  // TODO(deadbeef): Make this take an std::unique_ptr<> to avoid confusion
  // around ownership.
  virtual void OnSuccess(SessionDescriptionInterface* desc) = 0;
  virtual void OnFailure(const std::string& error) = 0;

 protected:
  ~CreateSessionDescriptionObserver() {}
};

// SetLocalDescription and SetRemoteDescription callback interface.
class SetSessionDescriptionObserver : public rtc::RefCountInterface {
 public:
  virtual void OnSuccess() = 0;
  virtual void OnFailure(const std::string& error) = 0;

 protected:
  ~SetSessionDescriptionObserver() {}
};

}  // namespace webrtc

#endif  // WEBRTC_API_JSEP_H_
