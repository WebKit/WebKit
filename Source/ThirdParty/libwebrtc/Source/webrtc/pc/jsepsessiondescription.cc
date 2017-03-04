/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/jsepsessiondescription.h"

#include <memory>

#include "webrtc/base/arraysize.h"
#include "webrtc/base/stringencode.h"
#include "webrtc/pc/mediasession.h"
#include "webrtc/pc/webrtcsdp.h"

using cricket::SessionDescription;

namespace webrtc {

static const char* kSupportedTypes[] = {
    JsepSessionDescription::kOffer,
    JsepSessionDescription::kPrAnswer,
    JsepSessionDescription::kAnswer
};

static bool IsTypeSupported(const std::string& type) {
  bool type_supported = false;
  for (size_t i = 0; i < arraysize(kSupportedTypes); ++i) {
    if (kSupportedTypes[i] == type) {
      type_supported = true;
      break;
    }
  }
  return type_supported;
}

const char SessionDescriptionInterface::kOffer[] = "offer";
const char SessionDescriptionInterface::kPrAnswer[] = "pranswer";
const char SessionDescriptionInterface::kAnswer[] = "answer";

const int JsepSessionDescription::kDefaultVideoCodecId = 100;
const char JsepSessionDescription::kDefaultVideoCodecName[] = "VP8";

SessionDescriptionInterface* CreateSessionDescription(const std::string& type,
                                                      const std::string& sdp,
                                                      SdpParseError* error) {
  if (!IsTypeSupported(type)) {
    return NULL;
  }

  JsepSessionDescription* jsep_desc = new JsepSessionDescription(type);
  if (!jsep_desc->Initialize(sdp, error)) {
    delete jsep_desc;
    return NULL;
  }
  return jsep_desc;
}

JsepSessionDescription::JsepSessionDescription(const std::string& type)
    : type_(type) {
}

JsepSessionDescription::~JsepSessionDescription() {}

bool JsepSessionDescription::Initialize(
    cricket::SessionDescription* description,
    const std::string& session_id,
    const std::string& session_version) {
  if (!description)
    return false;

  session_id_ = session_id;
  session_version_ = session_version;
  description_.reset(description);
  candidate_collection_.resize(number_of_mediasections());
  return true;
}

bool JsepSessionDescription::Initialize(const std::string& sdp,
                                        SdpParseError* error) {
  return SdpDeserialize(sdp, this, error);
}

bool JsepSessionDescription::AddCandidate(
    const IceCandidateInterface* candidate) {
  if (!candidate || candidate->sdp_mline_index() < 0)
    return false;
  size_t mediasection_index = 0;
  if (!GetMediasectionIndex(candidate, &mediasection_index)) {
    return false;
  }
  if (mediasection_index >= number_of_mediasections())
    return false;
  const std::string& content_name =
      description_->contents()[mediasection_index].name;
  const cricket::TransportInfo* transport_info =
      description_->GetTransportInfoByName(content_name);
  if (!transport_info) {
    return false;
  }

  cricket::Candidate updated_candidate = candidate->candidate();
  if (updated_candidate.username().empty()) {
    updated_candidate.set_username(transport_info->description.ice_ufrag);
  }
  if (updated_candidate.password().empty()) {
    updated_candidate.set_password(transport_info->description.ice_pwd);
  }

  std::unique_ptr<JsepIceCandidate> updated_candidate_wrapper(
      new JsepIceCandidate(candidate->sdp_mid(),
                           static_cast<int>(mediasection_index),
                           updated_candidate));
  if (!candidate_collection_[mediasection_index].HasCandidate(
          updated_candidate_wrapper.get()))
    candidate_collection_[mediasection_index].add(
        updated_candidate_wrapper.release());

  return true;
}

size_t JsepSessionDescription::RemoveCandidates(
    const std::vector<cricket::Candidate>& candidates) {
  size_t num_removed = 0;
  for (auto& candidate : candidates) {
    int mediasection_index = GetMediasectionIndex(candidate);
    if (mediasection_index < 0) {
      // Not found.
      continue;
    }
    num_removed += candidate_collection_[mediasection_index].remove(candidate);
  }
  return num_removed;
}

size_t JsepSessionDescription::number_of_mediasections() const {
  if (!description_)
    return 0;
  return description_->contents().size();
}

const IceCandidateCollection* JsepSessionDescription::candidates(
    size_t mediasection_index) const {
  if (mediasection_index >= candidate_collection_.size())
    return NULL;
  return &candidate_collection_[mediasection_index];
}

bool JsepSessionDescription::ToString(std::string* out) const {
  if (!description_ || !out) {
    return false;
  }
  *out = SdpSerialize(*this, false);
  return !out->empty();
}

bool JsepSessionDescription::GetMediasectionIndex(
    const IceCandidateInterface* candidate,
    size_t* index) {
  if (!candidate || !index) {
    return false;
  }
  *index = static_cast<size_t>(candidate->sdp_mline_index());
  if (description_ && !candidate->sdp_mid().empty()) {
    bool found = false;
    // Try to match the sdp_mid with content name.
    for (size_t i = 0; i < description_->contents().size(); ++i) {
      if (candidate->sdp_mid() == description_->contents().at(i).name) {
        *index = i;
        found = true;
        break;
      }
    }
    if (!found) {
      // If the sdp_mid is presented but we can't find a match, we consider
      // this as an error.
      return false;
    }
  }
  return true;
}

int JsepSessionDescription::GetMediasectionIndex(
    const cricket::Candidate& candidate) {
  // Find the description with a matching transport name of the candidate.
  const std::string& transport_name = candidate.transport_name();
  for (size_t i = 0; i < description_->contents().size(); ++i) {
    if (transport_name == description_->contents().at(i).name) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

}  // namespace webrtc
