/*
 *  Copyright 2010 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/session_description.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "p2p/base/transport_info.h"
#include "rtc_base/checks.h"
#include "rtc_base/strings/str_join.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {
namespace {

ContentInfo* FindContentInfoByName(ContentInfos* contents,
                                   absl::string_view name) {
  RTC_DCHECK(contents);
  for (ContentInfo& content : *contents) {
    if (content.mid() == name) {
      return &content;
    }
  }
  return nullptr;
}

const ContentInfo* FindContentInfoByName(const ContentInfos& contents,
                                         absl::string_view name) {
  for (ContentInfos::const_iterator content = contents.begin();
       content != contents.end(); ++content) {
    if (content->mid() == name) {
      return &(*content);
    }
  }
  return nullptr;
}

const ContentInfo* FindContentInfoByType(const ContentInfos& contents,
                                         MediaProtocolType type) {
  for (const auto& content : contents) {
    if (content.type == type) {
      return &content;
    }
  }
  return nullptr;
}

}  // namespace

ContentGroup::ContentGroup(std::string semantics)
    : semantics_(std::move(semantics)) {}

ContentGroup::ContentGroup(const ContentGroup&) = default;
ContentGroup::ContentGroup(ContentGroup&&) = default;
ContentGroup& ContentGroup::operator=(const ContentGroup&) = default;
ContentGroup& ContentGroup::operator=(ContentGroup&&) = default;
ContentGroup::~ContentGroup() = default;

const std::string* ContentGroup::FirstContentName() const {
  return (!content_names_.empty()) ? &(*content_names_.begin()) : nullptr;
}

bool ContentGroup::HasContentName(absl::string_view content_name) const {
  return absl::c_linear_search(content_names_, content_name);
}

void ContentGroup::AddContentName(absl::string_view content_name) {
  if (!HasContentName(content_name)) {
    content_names_.emplace_back(content_name);
  }
}

bool ContentGroup::RemoveContentName(absl::string_view content_name) {
  ContentNames::iterator iter = absl::c_find(content_names_, content_name);
  if (iter == content_names_.end()) {
    return false;
  }
  content_names_.erase(iter);
  return true;
}

std::string ContentGroup::ToString() const {
  StringBuilder acc;
  acc << semantics_ << "(";
  if (!content_names_.empty()) {
    acc << StrJoin(content_names_, " ");
  }
  acc << ")";
  return acc.Release();
}

SessionDescription::SessionDescription() = default;
SessionDescription::SessionDescription(const SessionDescription&) = default;

SessionDescription::~SessionDescription() {}

std::unique_ptr<SessionDescription> SessionDescription::Clone() const {
  // Copy using the private copy constructor.
  // This will clone the descriptions using ContentInfo's copy constructor.
  return absl::WrapUnique(new SessionDescription(*this));
}

const ContentInfo* SessionDescription::GetContentByName(
    absl::string_view name) const {
  return FindContentInfoByName(contents_, name);
}

ContentInfo* SessionDescription::GetContentByName(absl::string_view name) {
  return FindContentInfoByName(&contents_, name);
}

const MediaContentDescription* SessionDescription::GetContentDescriptionByName(
    absl::string_view name) const {
  const ContentInfo* cinfo = FindContentInfoByName(contents_, name);
  if (cinfo == nullptr) {
    return nullptr;
  }

  return cinfo->media_description();
}

MediaContentDescription* SessionDescription::GetContentDescriptionByName(
    absl::string_view name) {
  ContentInfo* cinfo = FindContentInfoByName(&contents_, name);
  if (cinfo == nullptr) {
    return nullptr;
  }

  return cinfo->media_description();
}

const ContentInfo* SessionDescription::FirstContentByType(
    MediaProtocolType type) const {
  return FindContentInfoByType(contents_, type);
}

const ContentInfo* SessionDescription::FirstContent() const {
  return (contents_.empty()) ? nullptr : &(*contents_.begin());
}

void SessionDescription::AddContent(
    absl::string_view name,
    MediaProtocolType type,
    std::unique_ptr<MediaContentDescription> description) {
  AddContent(ContentInfo(type, name, std::move(description)));
}

void SessionDescription::AddContent(
    absl::string_view name,
    MediaProtocolType type,
    bool rejected,
    std::unique_ptr<MediaContentDescription> description) {
  AddContent(ContentInfo(type, name, std::move(description), rejected));
}

void SessionDescription::AddContent(
    absl::string_view name,
    MediaProtocolType type,
    bool rejected,
    bool bundle_only,
    std::unique_ptr<MediaContentDescription> description) {
  AddContent(
      ContentInfo(type, name, std::move(description), rejected, bundle_only));
}

void SessionDescription::AddContent(ContentInfo&& content) {
  if (extmap_allow_mixed()) {
    // Mixed support on session level overrides setting on media level.
    content.media_description()->set_extmap_allow_mixed_enum(
        MediaContentDescription::kSession);
  }
  contents_.push_back(std::move(content));
}

bool SessionDescription::RemoveContentByName(absl::string_view name) {
  for (ContentInfos::iterator content = contents_.begin();
       content != contents_.end(); ++content) {
    if (content->mid() == name) {
      contents_.erase(content);
      return true;
    }
  }

  return false;
}

void SessionDescription::AddTransportInfo(const TransportInfo& transport_info) {
  transport_infos_.push_back(transport_info);
}

bool SessionDescription::RemoveTransportInfoByName(absl::string_view name) {
  for (TransportInfos::iterator transport_info = transport_infos_.begin();
       transport_info != transport_infos_.end(); ++transport_info) {
    if (transport_info->content_name == name) {
      transport_infos_.erase(transport_info);
      return true;
    }
  }
  return false;
}

const TransportInfo* SessionDescription::GetTransportInfoByName(
    absl::string_view name) const {
  for (TransportInfos::const_iterator iter = transport_infos_.begin();
       iter != transport_infos_.end(); ++iter) {
    if (iter->content_name == name) {
      return &(*iter);
    }
  }
  return nullptr;
}

TransportInfo* SessionDescription::GetTransportInfoByName(
    absl::string_view name) {
  for (TransportInfos::iterator iter = transport_infos_.begin();
       iter != transport_infos_.end(); ++iter) {
    if (iter->content_name == name) {
      return &(*iter);
    }
  }
  return nullptr;
}

void SessionDescription::RemoveGroupByName(absl::string_view name) {
  for (ContentGroups::iterator iter = content_groups_.begin();
       iter != content_groups_.end(); ++iter) {
    if (iter->semantics() == name) {
      content_groups_.erase(iter);
      break;
    }
  }
}

bool SessionDescription::HasGroup(absl::string_view name) const {
  for (ContentGroups::const_iterator iter = content_groups_.begin();
       iter != content_groups_.end(); ++iter) {
    if (iter->semantics() == name) {
      return true;
    }
  }
  return false;
}

const ContentGroup* SessionDescription::GetGroupByName(
    absl::string_view name) const {
  for (ContentGroups::const_iterator iter = content_groups_.begin();
       iter != content_groups_.end(); ++iter) {
    if (iter->semantics() == name) {
      return &(*iter);
    }
  }
  return nullptr;
}

std::vector<const ContentGroup*> SessionDescription::GetGroupsByName(
    absl::string_view name) const {
  std::vector<const ContentGroup*> content_groups;
  for (const ContentGroup& content_group : content_groups_) {
    if (content_group.semantics() == name) {
      content_groups.push_back(&content_group);
    }
  }
  return content_groups;
}

ContentInfo::~ContentInfo() {}

// Copy operator.
ContentInfo::ContentInfo(const ContentInfo& o)
    : type(o.type),
      rejected(o.rejected),
      bundle_only(o.bundle_only),
      mid_(o.mid_),
      description_(o.description_->Clone()) {}

const MediaContentDescription* ContentInfo::media_description() const {
  return description_.get();
}

MediaContentDescription* ContentInfo::media_description() {
  return description_.get();
}

}  // namespace webrtc
