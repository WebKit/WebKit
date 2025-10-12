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
// constructs; session descriptions and ICE candidates. The inner "webrtc::"
// objects shouldn't be accessed directly; the intention is that an application
// using the PeerConnection API only creates these objects from strings, and
// them passes them into the PeerConnection.
//
// Though in the future, we're planning to provide an SDP parsing API, with a
// structure more friendly than webrtc::SessionDescription.

#ifndef API_JSEP_H_
#define API_JSEP_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "api/candidate.h"
#include "api/ref_count.h"
#include "api/rtc_error.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class SessionDescription;

struct SdpParseError {
 public:
  // The sdp line that causes the error.
  std::string line;
  // Explains the error.
  std::string description;
};

// Class representation of an ICE candidate.
class RTC_EXPORT IceCandidate final {
 public:
  IceCandidate(absl::string_view sdp_mid,
               int sdp_mline_index,
               const Candidate& candidate);
  ~IceCandidate() = default;

  IceCandidate(const IceCandidate&) = delete;
  IceCandidate& operator=(const IceCandidate&) = delete;

  // Parses an sdp candidate string (only the first line) to construct an
  // IceCandidate instance. If an error occurs, details about the error can
  // optionally be returned via the `error` paramter, and the function returns
  // nullptr.
  static std::unique_ptr<IceCandidate> Create(
      absl::string_view mid,
      int sdp_mline_index,
      absl::string_view sdp,
      SdpParseError* absl_nullable error = nullptr);

  // If present, this is the value of the "a=mid" attribute of the candidate's
  // m= section in SDP, which identifies the m= section.
  // TODO: webrtc:406795492 - string_view.
  std::string sdp_mid() const { return sdp_mid_; }

  // This indicates the index (starting at zero) of m= section this candidate
  // is associated with. Needed when an endpoint doesn't support MIDs.
  int sdp_mline_index() const { return sdp_mline_index_; }

  // Only for use internally.
  const Candidate& candidate() const { return candidate_; }

  // The URL of the ICE server which this candidate was gathered from.
  // TODO: webrtc:406795492 - string_view.
  std::string server_url() const { return candidate_.url(); }

  // Creates a SDP-ized form of this candidate.
  std::string ToString() const;

  // TODO: webrtc:406795492 - Deprecate and remove this method.
  // [[deprecated("Use ToString()")]]
  bool ToString(std::string* out) const {
    if (!out)
      return false;
    *out = ToString();
    return !out->empty();
  }

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const IceCandidate& c) {
    absl::Format(&sink, "IceCandidate: {'%s', %i, '%s'}", c.sdp_mid_.c_str(),
                 c.sdp_mline_index_, c.ToString().c_str());
  }

 private:
  const std::string sdp_mid_;
  const int sdp_mline_index_;
  const Candidate candidate_;
};

// TODO: webrtc:406795492 - Deprecate and eventually remove these types when no
// longer referenced. They're provided here for backwards compatiblity.
using JsepIceCandidate = IceCandidate;
using IceCandidateInterface = IceCandidate;

// Creates an IceCandidate based on SDP string.
// Returns null if the sdp string can't be parsed.
// `error` may be null.
RTC_EXPORT IceCandidate* CreateIceCandidate(const std::string& sdp_mid,
                                            int sdp_mline_index,
                                            const std::string& sdp,
                                            SdpParseError* error);

// Creates an IceCandidate based on a parsed candidate structure.
RTC_EXPORT std::unique_ptr<IceCandidate> CreateIceCandidate(
    const std::string& sdp_mid,
    int sdp_mline_index,
    const Candidate& candidate);

// This class represents a collection of candidates for a specific m= section.
// Used in SessionDescriptionInterface.
class IceCandidateCollection final {
 public:
  IceCandidateCollection() = default;
  explicit IceCandidateCollection(
      std::vector<std::unique_ptr<IceCandidate>>&& candidates)
      : candidates_(std::move(candidates)) {}
  ~IceCandidateCollection() = default;

  // Move constructor is defined so that a vector of IceCandidateCollections
  // can be resized.
  IceCandidateCollection(IceCandidateCollection&& o) = default;

  IceCandidateCollection(const IceCandidateCollection&) = delete;
  IceCandidateCollection& operator=(const IceCandidateCollection&) = delete;

  size_t count() const { return candidates_.size(); }
  bool empty() const { return candidates_.empty(); }
  const IceCandidate* at(size_t index) const;

  // Adds and takes ownership of the IceCandidate.
  void add(std::unique_ptr<IceCandidate> candidate);
  [[deprecated("Use unique_ptr version")]]
  void add(IceCandidate* candidate);

  // Removes the candidate that has a matching address and protocol.
  //
  // Returns the number of candidates that were removed.
  size_t remove(const IceCandidate* candidate);

  const std::vector<std::unique_ptr<IceCandidate>>& candidates() const {
    return candidates_;
  }

  // Returns true if an equivalent `candidate` exist in the collection.
  bool HasCandidate(const IceCandidate* candidate) const;

  IceCandidateCollection Clone() const;

 private:
  std::vector<std::unique_ptr<IceCandidate>> candidates_;
};

// TODO: webrtc:406795492 - Deprecate.
using JsepCandidateCollection = IceCandidateCollection;

// Enum that describes the type of the SessionDescriptionInterface.
// Corresponds to RTCSdpType in the WebRTC specification.
// https://w3c.github.io/webrtc-pc/#dom-rtcsdptype
enum class SdpType {
  kOffer,     // Description must be treated as an SDP offer.
  kPrAnswer,  // Description must be treated as an SDP answer, but not a final
              // answer.
  kAnswer,    // Description must be treated as an SDP final answer, and the
              // offer-answer exchange must be considered complete after
              // receiving this.
  kRollback   // Resets any pending offers and sets signaling state back to
              // stable.
};

// Returns the string form of the given SDP type. String forms are defined in
// SessionDescriptionInterface.
RTC_EXPORT const char* SdpTypeToString(SdpType type);

template <typename Sink>
void AbslStringify(Sink& sink, SdpType sdp_type) {
  sink.Append(SdpTypeToString(sdp_type));
}

// Returns the SdpType from its string form. The string form can be one of the
// constants defined in SessionDescriptionInterface. Passing in any other string
// results in nullopt.
RTC_EXPORT std::optional<SdpType> SdpTypeFromString(
    const std::string& type_str);

// Class representation of an SDP session description.
//
// An instance of this interface is supposed to be owned by one class at a time
// and is therefore not expected to be thread safe.
//
// An instance can be created by CreateSessionDescription.
class RTC_EXPORT SessionDescriptionInterface {
 public:
  // String representations of the supported SDP types.
  static const char kOffer[];
  static const char kPrAnswer[];
  static const char kAnswer[];
  static const char kRollback[];

  virtual ~SessionDescriptionInterface() {}

  // Create a new SessionDescriptionInterface object
  // with the same values as the old object.
  // TODO(bugs.webrtc.org:12215): Remove default implementation
  virtual std::unique_ptr<SessionDescriptionInterface> Clone() const {
    return nullptr;
  }

  // Only for use internally.
  virtual SessionDescription* description() = 0;
  virtual const SessionDescription* description() const = 0;

  // Get the session id and session version, which are defined based on
  // RFC 4566 for the SDP o= line.
  virtual std::string session_id() const = 0;
  virtual std::string session_version() const = 0;

  // Returns the type of this session description as an SdpType. Descriptions of
  // the various types are found in the SdpType documentation.
  // TODO(steveanton): Remove default implementation once Chromium has been
  // updated.
  virtual SdpType GetType() const;

  // kOffer/kPrAnswer/kAnswer
  // TODO(steveanton): Remove this in favor of `GetType` that returns SdpType.
  virtual std::string type() const = 0;

  // Adds the specified candidate to the description.
  //
  // Ownership is not transferred.
  //
  // Returns false if the session description does not have a media section
  // that corresponds to `candidate.sdp_mid()` or
  // `candidate.sdp_mline_index()`.
  virtual bool AddCandidate(const IceCandidate* candidate) = 0;

  // Removes the first matching candidate (at most 1) from the description
  // that meets the `Candidate::MatchesForRemoval()` requirement and matches
  // either the `IceCandidate::sdp_mid()` property or
  // `IceCandidate::sdp_mline_index()`.
  //
  // Returns false if no matching candidate was found (and removed).
  virtual bool RemoveCandidate(const IceCandidate* candidate) = 0;

  // Returns the number of m= sections in the session description.
  virtual size_t number_of_mediasections() const = 0;

  // Returns a collection of all candidates that belong to a certain m=
  // section.
  virtual const IceCandidateCollection* candidates(
      size_t mediasection_index) const = 0;

  // Serializes the description to SDP.
  virtual bool ToString(std::string* out) const = 0;
  template <typename Sink>
  friend void AbslStringify(Sink& sink, const SessionDescriptionInterface& p) {
    sink.Append("\n--- BEGIN SDP ");
    absl::Format(&sink, "%v", p.GetType());
    sink.Append(" ---\n");
    std::string temp;
    if (p.ToString(&temp)) {
      sink.Append(temp);
    } else {
      sink.Append("Error in ToString\n");
    }
    sink.Append("--- END SDP ---\n");
  }
};

// Creates a SessionDescriptionInterface based on the SDP string and the type.
// Returns null if the sdp string can't be parsed or the type is unsupported.
// `error` may be null.
// TODO(https://issues.webrtc.org/360909068): This function is deprecated.
// Please use the functions below which take an SdpType enum instead. Remove
// this once it is no longer used.
[[deprecated("Use version with SdpType argument")]] RTC_EXPORT
    SessionDescriptionInterface*
    CreateSessionDescription(const std::string& type,
                             const std::string& sdp,
                             SdpParseError* error);

// Creates a SessionDescriptionInterface based on the SDP string and the type.
// Returns null if the SDP string cannot be parsed.
// If using the signature with `error_out`, details of the parsing error may be
// written to `error_out` if it is not null.
RTC_EXPORT std::unique_ptr<SessionDescriptionInterface>
CreateSessionDescription(SdpType type, const std::string& sdp);
RTC_EXPORT std::unique_ptr<SessionDescriptionInterface>
CreateSessionDescription(SdpType type,
                         const std::string& sdp,
                         SdpParseError* error_out);

// Creates a SessionDescriptionInterface based on a parsed SDP structure and the
// given type, ID and version.
std::unique_ptr<SessionDescriptionInterface> CreateSessionDescription(
    SdpType type,
    const std::string& session_id,
    const std::string& session_version,
    std::unique_ptr<SessionDescription> description);

// CreateOffer and CreateAnswer callback interface.
class RTC_EXPORT CreateSessionDescriptionObserver : public RefCountInterface {
 public:
  // This callback transfers the ownership of the `desc`.
  // TODO(deadbeef): Make this take an std::unique_ptr<> to avoid confusion
  // around ownership.
  virtual void OnSuccess(SessionDescriptionInterface* desc) = 0;
  // The OnFailure callback takes an RTCError, which consists of an
  // error code and a string.
  // RTCError is non-copyable, so it must be passed using std::move.
  // Earlier versions of the API used a string argument. This version
  // is removed; its functionality was the same as passing
  // error.message.
  virtual void OnFailure(RTCError error) = 0;

 protected:
  ~CreateSessionDescriptionObserver() override = default;
};

// SetLocalDescription and SetRemoteDescription callback interface.
class RTC_EXPORT SetSessionDescriptionObserver : public RefCountInterface {
 public:
  virtual void OnSuccess() = 0;
  // See description in CreateSessionDescriptionObserver for OnFailure.
  virtual void OnFailure(RTCError error) = 0;

 protected:
  ~SetSessionDescriptionObserver() override = default;
};

}  // namespace webrtc

#endif  // API_JSEP_H_
