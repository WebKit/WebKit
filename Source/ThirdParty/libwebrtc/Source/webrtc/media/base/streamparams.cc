/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/streamparams.h"

#include <list>

#include "rtc_base/checks.h"
#include "rtc_base/strings/string_builder.h"

namespace cricket {
namespace {
// NOTE: There is no check here for duplicate streams, so check before
// adding.
void AddStream(std::vector<StreamParams>* streams, const StreamParams& stream) {
  streams->push_back(stream);
}
}  // namespace

const char kFecSsrcGroupSemantics[] = "FEC";
const char kFecFrSsrcGroupSemantics[] = "FEC-FR";
const char kFidSsrcGroupSemantics[] = "FID";
const char kSimSsrcGroupSemantics[] = "SIM";

bool GetStream(const StreamParamsVec& streams,
               const StreamSelector& selector,
               StreamParams* stream_out) {
  const StreamParams* found = GetStream(streams, selector);
  if (found && stream_out)
    *stream_out = *found;
  return found != nullptr;
}

MediaStreams::MediaStreams() = default;
MediaStreams::~MediaStreams() = default;

bool MediaStreams::GetAudioStream(const StreamSelector& selector,
                                  StreamParams* stream) {
  return GetStream(audio_, selector, stream);
}

bool MediaStreams::GetVideoStream(const StreamSelector& selector,
                                  StreamParams* stream) {
  return GetStream(video_, selector, stream);
}

bool MediaStreams::GetDataStream(const StreamSelector& selector,
                                 StreamParams* stream) {
  return GetStream(data_, selector, stream);
}

void MediaStreams::CopyFrom(const MediaStreams& streams) {
  audio_ = streams.audio_;
  video_ = streams.video_;
  data_ = streams.data_;
}

void MediaStreams::AddAudioStream(const StreamParams& stream) {
  AddStream(&audio_, stream);
}

void MediaStreams::AddVideoStream(const StreamParams& stream) {
  AddStream(&video_, stream);
}

void MediaStreams::AddDataStream(const StreamParams& stream) {
  AddStream(&data_, stream);
}

bool MediaStreams::RemoveAudioStream(const StreamSelector& selector) {
  return RemoveStream(&audio_, selector);
}

bool MediaStreams::RemoveVideoStream(const StreamSelector& selector) {
  return RemoveStream(&video_, selector);
}

bool MediaStreams::RemoveDataStream(const StreamSelector& selector) {
  return RemoveStream(&data_, selector);
}

static std::string SsrcsToString(const std::vector<uint32_t>& ssrcs) {
  char buf[1024];
  rtc::SimpleStringBuilder sb(buf);
  sb << "ssrcs:[";
  for (std::vector<uint32_t>::const_iterator it = ssrcs.begin();
       it != ssrcs.end(); ++it) {
    if (it != ssrcs.begin()) {
      sb << ",";
    }
    sb << *it;
  }
  sb << "]";
  return sb.str();
}

SsrcGroup::SsrcGroup(const std::string& usage,
                     const std::vector<uint32_t>& ssrcs)
    : semantics(usage), ssrcs(ssrcs) {}
SsrcGroup::SsrcGroup(const SsrcGroup&) = default;
SsrcGroup::SsrcGroup(SsrcGroup&&) = default;
SsrcGroup::~SsrcGroup() = default;

SsrcGroup& SsrcGroup::operator=(const SsrcGroup&) = default;
SsrcGroup& SsrcGroup::operator=(SsrcGroup&&) = default;

bool SsrcGroup::has_semantics(const std::string& semantics_in) const {
  return (semantics == semantics_in && ssrcs.size() > 0);
}

std::string SsrcGroup::ToString() const {
  char buf[1024];
  rtc::SimpleStringBuilder sb(buf);
  sb << "{";
  sb << "semantics:" << semantics << ";";
  sb << SsrcsToString(ssrcs);
  sb << "}";
  return sb.str();
}

StreamParams::StreamParams() = default;
StreamParams::StreamParams(const StreamParams&) = default;
StreamParams::StreamParams(StreamParams&&) = default;
StreamParams::~StreamParams() = default;
StreamParams& StreamParams::operator=(const StreamParams&) = default;
StreamParams& StreamParams::operator=(StreamParams&&) = default;

std::string StreamParams::ToString() const {
  char buf[2 * 1024];
  rtc::SimpleStringBuilder sb(buf);
  sb << "{";
  if (!groupid.empty()) {
    sb << "groupid:" << groupid << ";";
  }
  if (!id.empty()) {
    sb << "id:" << id << ";";
  }
  sb << SsrcsToString(ssrcs) << ";";
  sb << "ssrc_groups:";
  for (std::vector<SsrcGroup>::const_iterator it = ssrc_groups.begin();
       it != ssrc_groups.end(); ++it) {
    if (it != ssrc_groups.begin()) {
      sb << ",";
    }
    sb << it->ToString();
  }
  sb << ";";
  if (!cname.empty()) {
    sb << "cname:" << cname << ";";
  }
  sb << "stream_ids:";
  for (std::vector<std::string>::const_iterator it = stream_ids_.begin();
       it != stream_ids_.end(); ++it) {
    if (it != stream_ids_.begin()) {
      sb << ",";
    }
    sb << *it;
  }
  sb << ";";
  sb << "}";
  return sb.str();
}
void StreamParams::GetPrimarySsrcs(std::vector<uint32_t>* ssrcs) const {
  const SsrcGroup* sim_group = get_ssrc_group(kSimSsrcGroupSemantics);
  if (sim_group == NULL) {
    ssrcs->push_back(first_ssrc());
  } else {
    for (size_t i = 0; i < sim_group->ssrcs.size(); ++i) {
      ssrcs->push_back(sim_group->ssrcs[i]);
    }
  }
}

void StreamParams::GetFidSsrcs(const std::vector<uint32_t>& primary_ssrcs,
                               std::vector<uint32_t>* fid_ssrcs) const {
  for (size_t i = 0; i < primary_ssrcs.size(); ++i) {
    uint32_t fid_ssrc;
    if (GetFidSsrc(primary_ssrcs[i], &fid_ssrc)) {
      fid_ssrcs->push_back(fid_ssrc);
    }
  }
}

bool StreamParams::AddSecondarySsrc(const std::string& semantics,
                                    uint32_t primary_ssrc,
                                    uint32_t secondary_ssrc) {
  if (!has_ssrc(primary_ssrc)) {
    return false;
  }

  ssrcs.push_back(secondary_ssrc);
  std::vector<uint32_t> ssrc_vector;
  ssrc_vector.push_back(primary_ssrc);
  ssrc_vector.push_back(secondary_ssrc);
  SsrcGroup ssrc_group = SsrcGroup(semantics, ssrc_vector);
  ssrc_groups.push_back(ssrc_group);
  return true;
}

bool StreamParams::GetSecondarySsrc(const std::string& semantics,
                                    uint32_t primary_ssrc,
                                    uint32_t* secondary_ssrc) const {
  for (std::vector<SsrcGroup>::const_iterator it = ssrc_groups.begin();
       it != ssrc_groups.end(); ++it) {
    if (it->has_semantics(semantics) && it->ssrcs.size() >= 2 &&
        it->ssrcs[0] == primary_ssrc) {
      *secondary_ssrc = it->ssrcs[1];
      return true;
    }
  }
  return false;
}

std::vector<std::string> StreamParams::stream_ids() const {
  return stream_ids_;
}

void StreamParams::set_stream_ids(const std::vector<std::string>& stream_ids) {
  stream_ids_ = stream_ids;
}

std::string StreamParams::first_stream_id() const {
  return stream_ids_.empty() ? "" : stream_ids_[0];
}

bool IsOneSsrcStream(const StreamParams& sp) {
  if (sp.ssrcs.size() == 1 && sp.ssrc_groups.empty()) {
    return true;
  }
  const SsrcGroup* fid_group = sp.get_ssrc_group(kFidSsrcGroupSemantics);
  const SsrcGroup* fecfr_group = sp.get_ssrc_group(kFecFrSsrcGroupSemantics);
  if (sp.ssrcs.size() == 2) {
    if (fid_group != nullptr && sp.ssrcs == fid_group->ssrcs) {
      return true;
    }
    if (fecfr_group != nullptr && sp.ssrcs == fecfr_group->ssrcs) {
      return true;
    }
  }
  if (sp.ssrcs.size() == 3) {
    if (fid_group == nullptr || fecfr_group == nullptr) {
      return false;
    }
    if (sp.ssrcs[0] != fid_group->ssrcs[0] ||
        sp.ssrcs[0] != fecfr_group->ssrcs[0]) {
      return false;
    }
    // We do not check for FlexFEC over RTX,
    // as this combination is not supported.
    if (sp.ssrcs[1] == fid_group->ssrcs[1] &&
        sp.ssrcs[2] == fecfr_group->ssrcs[1]) {
      return true;
    }
    if (sp.ssrcs[1] == fecfr_group->ssrcs[1] &&
        sp.ssrcs[2] == fid_group->ssrcs[1]) {
      return true;
    }
  }
  return false;
}

static void RemoveFirst(std::list<uint32_t>* ssrcs, uint32_t value) {
  std::list<uint32_t>::iterator it =
      std::find(ssrcs->begin(), ssrcs->end(), value);
  if (it != ssrcs->end()) {
    ssrcs->erase(it);
  }
}

bool IsSimulcastStream(const StreamParams& sp) {
  const SsrcGroup* const sg = sp.get_ssrc_group(kSimSsrcGroupSemantics);
  if (sg == NULL || sg->ssrcs.size() < 2) {
    return false;
  }
  // Start with all StreamParams SSRCs. Remove simulcast SSRCs (from sg) and
  // RTX SSRCs. If we still have SSRCs left, we don't know what they're for.
  // Also we remove first-found SSRCs only. So duplicates should lead to errors.
  std::list<uint32_t> sp_ssrcs(sp.ssrcs.begin(), sp.ssrcs.end());
  for (size_t i = 0; i < sg->ssrcs.size(); ++i) {
    RemoveFirst(&sp_ssrcs, sg->ssrcs[i]);
  }
  for (size_t i = 0; i < sp.ssrc_groups.size(); ++i) {
    const SsrcGroup& group = sp.ssrc_groups[i];
    if (group.semantics.compare(kFidSsrcGroupSemantics) != 0 ||
        group.ssrcs.size() != 2) {
      continue;
    }
    RemoveFirst(&sp_ssrcs, group.ssrcs[1]);
  }
  // If there's SSRCs left that we don't know how to handle, we bail out.
  return sp_ssrcs.size() == 0;
}

}  // namespace cricket
