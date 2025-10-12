/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/jsep_session_description.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/candidate.h"
#include "api/jsep.h"
#include "p2p/base/p2p_constants.h"
#include "p2p/base/transport_description.h"
#include "p2p/base/transport_info.h"
#include "pc/media_session.h"  // IWYU pragma: keep
#include "pc/session_description.h"
#include "pc/webrtc_sdp.h"
#include "rtc_base/checks.h"
#include "rtc_base/ip_address.h"
#include "rtc_base/logging.h"
#include "rtc_base/net_helper.h"
#include "rtc_base/net_helpers.h"
#include "rtc_base/socket_address.h"

using webrtc::Candidate;
using ::webrtc::SessionDescription;

namespace webrtc {
namespace {

constexpr char kDummyAddress[] = "0.0.0.0";
constexpr int kDummyPort = 9;

// Update the connection address for the MediaContentDescription based on the
// candidates.
void UpdateConnectionAddress(
    const JsepCandidateCollection& candidate_collection,
    MediaContentDescription* media_desc) {
  int port = kDummyPort;
  std::string ip = kDummyAddress;
  std::string hostname;
  int current_preference = 0;  // Start with lowest preference.
  int current_family = AF_UNSPEC;
  for (size_t i = 0; i < candidate_collection.count(); ++i) {
    const IceCandidate* jsep_candidate = candidate_collection.at(i);
    if (jsep_candidate->candidate().component() !=
        ICE_CANDIDATE_COMPONENT_RTP) {
      continue;
    }
    // Default destination should be UDP only.
    if (jsep_candidate->candidate().protocol() != UDP_PROTOCOL_NAME) {
      continue;
    }
    const int preference = jsep_candidate->candidate().type_preference();
    const int family = jsep_candidate->candidate().address().ipaddr().family();
    // See if this candidate is more preferable then the current one if it's the
    // same family. Or if the current family is IPv4 already so we could safely
    // ignore all IPv6 ones. WebRTC bug 4269.
    // http://code.google.com/p/webrtc/issues/detail?id=4269
    if ((preference <= current_preference && current_family == family) ||
        (current_family == AF_INET && family == AF_INET6)) {
      continue;
    }
    current_preference = preference;
    current_family = family;
    const SocketAddress& candidate_addr = jsep_candidate->candidate().address();
    port = candidate_addr.port();
    ip = candidate_addr.ipaddr().ToString();
    hostname = candidate_addr.hostname();
  }
  SocketAddress connection_addr(ip, port);
  if (IPIsUnspec(connection_addr.ipaddr()) && !hostname.empty()) {
    // When a hostname candidate becomes the (default) connection address,
    // we use the dummy address 0.0.0.0 and port 9 in the c= and the m= lines.
    //
    // We have observed in deployment that with a FQDN in a c= line, SDP parsing
    // could fail in other JSEP implementations. We note that the wildcard
    // addresses (0.0.0.0 or ::) with port 9 are given the exception as the
    // connection address that will not result in an ICE mismatch
    // (draft-ietf-mmusic-ice-sip-sdp). Also, 0.0.0.0 or :: can be used as the
    // connection address in the initial offer or answer with trickle ICE
    // if the offerer or answerer does not want to include the host IP address
    // (draft-ietf-mmusic-trickle-ice-sip), and in particular 0.0.0.0 has been
    // widely deployed for this use without outstanding compatibility issues.
    // Combining the above considerations, we use 0.0.0.0 with port 9 to
    // populate the c= and the m= lines. See `BuildMediaDescription` in
    // webrtc_sdp.cc for the SDP generation with
    // `media_desc->connection_address()`.
    connection_addr = SocketAddress(kDummyAddress, kDummyPort);
  }
  media_desc->set_connection_address(connection_addr);
}
}  // namespace

// TODO(steveanton): Remove this default implementation once Chromium has been
// updated.
SdpType SessionDescriptionInterface::GetType() const {
  std::optional<SdpType> maybe_type = SdpTypeFromString(type());
  if (maybe_type) {
    return *maybe_type;
  } else {
    RTC_LOG(LS_WARNING) << "Default implementation of "
                           "SessionDescriptionInterface::GetType does not "
                           "recognize the result from type(), returning "
                           "kOffer.";
    return SdpType::kOffer;
  }
}

SessionDescriptionInterface* CreateSessionDescription(const std::string& type,
                                                      const std::string& sdp,
                                                      SdpParseError* error) {
  std::optional<SdpType> maybe_type = SdpTypeFromString(type);
  if (!maybe_type) {
    return nullptr;
  }

  return CreateSessionDescription(*maybe_type, sdp, error).release();
}

std::unique_ptr<SessionDescriptionInterface> CreateSessionDescription(
    SdpType type,
    const std::string& sdp) {
  return CreateSessionDescription(type, sdp, nullptr);
}

std::unique_ptr<SessionDescriptionInterface> CreateSessionDescription(
    SdpType type,
    const std::string& sdp,
    SdpParseError* error_out) {
  auto jsep_desc = std::make_unique<JsepSessionDescription>(type);
  if (type != SdpType::kRollback) {
    if (!SdpDeserialize(sdp, jsep_desc.get(), error_out)) {
      return nullptr;
    }
  }
  return std::move(jsep_desc);
}

std::unique_ptr<SessionDescriptionInterface> CreateSessionDescription(
    SdpType type,
    const std::string& session_id,
    const std::string& session_version,
    std::unique_ptr<SessionDescription> description) {
  auto jsep_description = std::make_unique<JsepSessionDescription>(type);
  bool initialize_success = jsep_description->Initialize(
      std::move(description), session_id, session_version);
  RTC_DCHECK(initialize_success);
  return std::move(jsep_description);
}

JsepSessionDescription::JsepSessionDescription(SdpType type) : type_(type) {}

JsepSessionDescription::JsepSessionDescription(const std::string& type) {
  std::optional<SdpType> maybe_type = SdpTypeFromString(type);
  if (maybe_type) {
    type_ = *maybe_type;
  } else {
    RTC_LOG(LS_WARNING)
        << "JsepSessionDescription constructed with invalid type string: "
        << type << ". Assuming it is an offer.";
    type_ = SdpType::kOffer;
  }
}

JsepSessionDescription::JsepSessionDescription(
    SdpType type,
    std::unique_ptr<SessionDescription> description,
    absl::string_view session_id,
    absl::string_view session_version)
    : description_(std::move(description)),
      session_id_(session_id),
      session_version_(session_version),
      type_(type) {
  RTC_DCHECK(description_);
  candidate_collection_.resize(number_of_mediasections());
}

JsepSessionDescription::~JsepSessionDescription() {}

bool JsepSessionDescription::Initialize(
    std::unique_ptr<SessionDescription> description,
    const std::string& session_id,
    const std::string& session_version) {
  if (!description)
    return false;

  session_id_ = session_id;
  session_version_ = session_version;
  description_ = std::move(description);
  candidate_collection_.resize(number_of_mediasections());
  return true;
}

std::unique_ptr<SessionDescriptionInterface> JsepSessionDescription::Clone()
    const {
  auto new_description = std::make_unique<JsepSessionDescription>(type_);
  new_description->session_id_ = session_id_;
  new_description->session_version_ = session_version_;
  if (description_) {
    new_description->description_ = description_->Clone();
  }
  for (const auto& collection : candidate_collection_) {
    new_description->candidate_collection_.push_back(collection.Clone());
  }
  return new_description;
}

bool JsepSessionDescription::AddCandidate(const IceCandidate* candidate) {
  if (!candidate)
    return false;
  size_t mediasection_index = 0;
  if (!GetMediasectionIndex(candidate, &mediasection_index)) {
    return false;
  }
  const std::string& mediasection_mid =
      description_->contents()[mediasection_index].mid();
  const TransportInfo* transport_info =
      description_->GetTransportInfoByName(mediasection_mid);
  if (!transport_info) {
    return false;
  }

  Candidate updated_candidate = candidate->candidate();
  if (updated_candidate.username().empty()) {
    updated_candidate.set_username(transport_info->description.ice_ufrag);
  }
  if (updated_candidate.password().empty()) {
    updated_candidate.set_password(transport_info->description.ice_pwd);
  }

  // Use `mediasection_mid` as the mid for the updated candidate. The
  // `candidate->sdp_mid()` property *should* be the same. However, in some
  // cases specifying an empty mid but a valid index is a way to add a candidate
  // without knowing (or caring about) the mid. This is done in several tests.
  RTC_DCHECK(candidate->sdp_mid().empty() ||
             candidate->sdp_mid() == mediasection_mid)
      << "sdp_mid='" << candidate->sdp_mid() << "' mediasection_mid='"
      << mediasection_mid << "'";
  auto updated_candidate_wrapper = std::make_unique<IceCandidate>(
      mediasection_mid, static_cast<int>(mediasection_index),
      updated_candidate);
  if (!candidate_collection_[mediasection_index].HasCandidate(
          updated_candidate_wrapper.get())) {
    candidate_collection_[mediasection_index].add(
        std::move(updated_candidate_wrapper));
    UpdateConnectionAddress(
        candidate_collection_[mediasection_index],
        description_->contents()[mediasection_index].media_description());
  }

  return true;
}

bool JsepSessionDescription::RemoveCandidate(const IceCandidate* candidate) {
  size_t mediasection_index = 0u;
  if (!GetMediasectionIndex(candidate, &mediasection_index)) {
    return false;
  }
  if (!candidate_collection_[mediasection_index].remove(candidate)) {
    return false;
  }
  UpdateConnectionAddress(
      candidate_collection_[mediasection_index],
      description_->contents()[mediasection_index].media_description());
  return true;
}

size_t JsepSessionDescription::number_of_mediasections() const {
  if (!description_)
    return 0;
  return description_->contents().size();
}

const IceCandidateCollection* JsepSessionDescription::candidates(
    size_t mediasection_index) const {
  if (mediasection_index >= candidate_collection_.size())
    return nullptr;
  return &candidate_collection_[mediasection_index];
}

bool JsepSessionDescription::ToString(std::string* out) const {
  if (!description_ || !out) {
    return false;
  }
  *out = SdpSerialize(*this);
  return !out->empty();
}

bool JsepSessionDescription::IsValidMLineIndex(int index) const {
  RTC_DCHECK(description_);
  return index >= 0 &&
         index < static_cast<int>(description_->contents().size());
}

bool JsepSessionDescription::GetMediasectionIndex(const IceCandidate* candidate,
                                                  size_t* index) const {
  if (!candidate || !index || !description_) {
    return false;
  }

  auto mid = candidate->sdp_mid();
  if (!mid.empty()) {
    *index = GetMediasectionIndex(mid);
  } else {
    // An sdp_mline_index of -1 will be treated as invalid.
    *index = static_cast<size_t>(candidate->sdp_mline_index());
  }
  return IsValidMLineIndex(*index);
}

int JsepSessionDescription::GetMediasectionIndex(absl::string_view mid) const {
  const auto& contents = description_->contents();
  auto it =
      std::find_if(contents.begin(), contents.end(),
                   [&](const auto& content) { return mid == content.mid(); });
  return it == contents.end() ? -1 : std::distance(contents.begin(), it);
}
}  // namespace webrtc
