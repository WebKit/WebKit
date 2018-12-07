/*
 *  Copyright 2010 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/sessiondescription.h"

#include <utility>

namespace cricket {
namespace {

ContentInfo* FindContentInfoByName(ContentInfos* contents,
                                   const std::string& name) {
  RTC_DCHECK(contents);
  for (ContentInfo& content : *contents) {
    if (content.name == name) {
      return &content;
    }
  }
  return nullptr;
}

}  // namespace

const ContentInfo* FindContentInfoByName(const ContentInfos& contents,
                                         const std::string& name) {
  for (ContentInfos::const_iterator content = contents.begin();
       content != contents.end(); ++content) {
    if (content->name == name) {
      return &(*content);
    }
  }
  return NULL;
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

ContentGroup::ContentGroup(const std::string& semantics)
    : semantics_(semantics) {}

ContentGroup::ContentGroup(const ContentGroup&) = default;
ContentGroup::ContentGroup(ContentGroup&&) = default;
ContentGroup& ContentGroup::operator=(const ContentGroup&) = default;
ContentGroup& ContentGroup::operator=(ContentGroup&&) = default;
ContentGroup::~ContentGroup() = default;

const std::string* ContentGroup::FirstContentName() const {
  return (!content_names_.empty()) ? &(*content_names_.begin()) : NULL;
}

bool ContentGroup::HasContentName(const std::string& content_name) const {
  return (std::find(content_names_.begin(), content_names_.end(),
                    content_name) != content_names_.end());
}

void ContentGroup::AddContentName(const std::string& content_name) {
  if (!HasContentName(content_name)) {
    content_names_.push_back(content_name);
  }
}

bool ContentGroup::RemoveContentName(const std::string& content_name) {
  ContentNames::iterator iter =
      std::find(content_names_.begin(), content_names_.end(), content_name);
  if (iter == content_names_.end()) {
    return false;
  }
  content_names_.erase(iter);
  return true;
}

SessionDescription::SessionDescription() = default;
SessionDescription::SessionDescription(const SessionDescription&) = default;

SessionDescription::~SessionDescription() {
  for (ContentInfos::iterator content = contents_.begin();
       content != contents_.end(); ++content) {
    delete content->description;
  }
}

SessionDescription* SessionDescription::Copy() const {
  SessionDescription* copy = new SessionDescription(*this);
  // Copy all ContentDescriptions.
  for (ContentInfos::iterator content = copy->contents_.begin();
       content != copy->contents().end(); ++content) {
    content->description = content->description->Copy();
  }
  return copy;
}

const ContentInfo* SessionDescription::GetContentByName(
    const std::string& name) const {
  return FindContentInfoByName(contents_, name);
}

ContentInfo* SessionDescription::GetContentByName(const std::string& name) {
  return FindContentInfoByName(&contents_, name);
}

const MediaContentDescription* SessionDescription::GetContentDescriptionByName(
    const std::string& name) const {
  const ContentInfo* cinfo = FindContentInfoByName(contents_, name);
  if (cinfo == NULL) {
    return NULL;
  }

  return cinfo->media_description();
}

MediaContentDescription* SessionDescription::GetContentDescriptionByName(
    const std::string& name) {
  ContentInfo* cinfo = FindContentInfoByName(&contents_, name);
  if (cinfo == NULL) {
    return NULL;
  }

  return cinfo->media_description();
}

const ContentInfo* SessionDescription::FirstContentByType(
    MediaProtocolType type) const {
  return FindContentInfoByType(contents_, type);
}

const ContentInfo* SessionDescription::FirstContent() const {
  return (contents_.empty()) ? NULL : &(*contents_.begin());
}

void SessionDescription::AddContent(const std::string& name,
                                    MediaProtocolType type,
                                    MediaContentDescription* description) {
  ContentInfo content(type);
  content.name = name;
  content.description = description;
  AddContent(&content);
}

void SessionDescription::AddContent(const std::string& name,
                                    MediaProtocolType type,
                                    bool rejected,
                                    MediaContentDescription* description) {
  ContentInfo content(type);
  content.name = name;
  content.rejected = rejected;
  content.description = description;
  AddContent(&content);
}

void SessionDescription::AddContent(const std::string& name,
                                    MediaProtocolType type,
                                    bool rejected,
                                    bool bundle_only,
                                    MediaContentDescription* description) {
  ContentInfo content(type);
  content.name = name;
  content.rejected = rejected;
  content.bundle_only = bundle_only;
  content.description = description;
  AddContent(&content);
}

void SessionDescription::AddContent(ContentInfo* content) {
  if (extmap_allow_mixed()) {
    // Mixed support on session level overrides setting on media level.
    content->description->set_extmap_allow_mixed_enum(
        MediaContentDescription::kSession);
  }
  contents_.push_back(std::move(*content));
}

bool SessionDescription::RemoveContentByName(const std::string& name) {
  for (ContentInfos::iterator content = contents_.begin();
       content != contents_.end(); ++content) {
    if (content->name == name) {
      delete content->description;
      contents_.erase(content);
      return true;
    }
  }

  return false;
}

bool SessionDescription::AddTransportInfo(const TransportInfo& transport_info) {
  if (GetTransportInfoByName(transport_info.content_name) != NULL) {
    return false;
  }
  transport_infos_.push_back(transport_info);
  return true;
}

bool SessionDescription::RemoveTransportInfoByName(const std::string& name) {
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
    const std::string& name) const {
  for (TransportInfos::const_iterator iter = transport_infos_.begin();
       iter != transport_infos_.end(); ++iter) {
    if (iter->content_name == name) {
      return &(*iter);
    }
  }
  return NULL;
}

TransportInfo* SessionDescription::GetTransportInfoByName(
    const std::string& name) {
  for (TransportInfos::iterator iter = transport_infos_.begin();
       iter != transport_infos_.end(); ++iter) {
    if (iter->content_name == name) {
      return &(*iter);
    }
  }
  return NULL;
}

void SessionDescription::RemoveGroupByName(const std::string& name) {
  for (ContentGroups::iterator iter = content_groups_.begin();
       iter != content_groups_.end(); ++iter) {
    if (iter->semantics() == name) {
      content_groups_.erase(iter);
      break;
    }
  }
}

bool SessionDescription::HasGroup(const std::string& name) const {
  for (ContentGroups::const_iterator iter = content_groups_.begin();
       iter != content_groups_.end(); ++iter) {
    if (iter->semantics() == name) {
      return true;
    }
  }
  return false;
}

const ContentGroup* SessionDescription::GetGroupByName(
    const std::string& name) const {
  for (ContentGroups::const_iterator iter = content_groups_.begin();
       iter != content_groups_.end(); ++iter) {
    if (iter->semantics() == name) {
      return &(*iter);
    }
  }
  return NULL;
}

}  // namespace cricket
