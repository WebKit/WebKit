/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_SESSIONDESCRIPTION_H_
#define P2P_BASE_SESSIONDESCRIPTION_H_

#include <string>
#include <vector>

#include "p2p/base/transportinfo.h"
#include "rtc_base/constructormagic.h"

namespace cricket {

// Describes a session content. Individual content types inherit from
// this class.  Analagous to a <jingle><content><description> or
// <session><description>.
class ContentDescription {
 public:
  virtual ~ContentDescription() {}
  virtual ContentDescription* Copy() const = 0;
};

// Analagous to a <jingle><content> or <session><description>.
// name = name of <content name="...">
// type = xmlns of <content>
struct ContentInfo {
  ContentInfo() {}
  ContentInfo(const std::string& name,
              const std::string& type,
              ContentDescription* description)
      : name(name), type(type), description(description) {}
  ContentInfo(const std::string& name,
              const std::string& type,
              bool rejected,
              ContentDescription* description) :
      name(name), type(type), rejected(rejected), description(description) {}
  ContentInfo(const std::string& name,
              const std::string& type,
              bool rejected,
              bool bundle_only,
              ContentDescription* description)
      : name(name),
        type(type),
        rejected(rejected),
        bundle_only(bundle_only),
        description(description) {}
  std::string name;
  std::string type;
  bool rejected = false;
  bool bundle_only = false;
  ContentDescription* description = nullptr;
};

typedef std::vector<std::string> ContentNames;

// This class provides a mechanism to aggregate different media contents into a
// group. This group can also be shared with the peers in a pre-defined format.
// GroupInfo should be populated only with the |content_name| of the
// MediaDescription.
class ContentGroup {
 public:
  explicit ContentGroup(const std::string& semantics);
  ContentGroup(const ContentGroup&);
  ContentGroup(ContentGroup&&);
  ContentGroup& operator=(const ContentGroup&);
  ContentGroup& operator=(ContentGroup&&);
  ~ContentGroup();

  const std::string& semantics() const { return semantics_; }
  const ContentNames& content_names() const { return content_names_; }

  const std::string* FirstContentName() const;
  bool HasContentName(const std::string& content_name) const;
  void AddContentName(const std::string& content_name);
  bool RemoveContentName(const std::string& content_name);

 private:
  std::string semantics_;
  ContentNames content_names_;
};

typedef std::vector<ContentInfo> ContentInfos;
typedef std::vector<ContentGroup> ContentGroups;

const ContentInfo* FindContentInfoByName(
    const ContentInfos& contents, const std::string& name);
const ContentInfo* FindContentInfoByType(
    const ContentInfos& contents, const std::string& type);

// Describes a collection of contents, each with its own name and
// type.  Analogous to a <jingle> or <session> stanza.  Assumes that
// contents are unique be name, but doesn't enforce that.
class SessionDescription {
 public:
  SessionDescription();
  explicit SessionDescription(const ContentInfos& contents);
  SessionDescription(const ContentInfos& contents, const ContentGroups& groups);
  SessionDescription(const ContentInfos& contents,
                     const TransportInfos& transports,
                     const ContentGroups& groups);
  ~SessionDescription();

  SessionDescription* Copy() const;

  // Content accessors.
  const ContentInfos& contents() const { return contents_; }
  ContentInfos& contents() { return contents_; }
  const ContentInfo* GetContentByName(const std::string& name) const;
  ContentInfo* GetContentByName(const std::string& name);
  const ContentDescription* GetContentDescriptionByName(
      const std::string& name) const;
  ContentDescription* GetContentDescriptionByName(const std::string& name);
  const ContentInfo* FirstContentByType(const std::string& type) const;
  const ContentInfo* FirstContent() const;

  // Content mutators.
  // Adds a content to this description. Takes ownership of ContentDescription*.
  void AddContent(const std::string& name,
                  const std::string& type,
                  ContentDescription* description);
  void AddContent(const std::string& name,
                  const std::string& type,
                  bool rejected,
                  ContentDescription* description);
  void AddContent(const std::string& name,
                  const std::string& type,
                  bool rejected,
                  bool bundle_only,
                  ContentDescription* description);
  bool RemoveContentByName(const std::string& name);

  // Transport accessors.
  const TransportInfos& transport_infos() const { return transport_infos_; }
  TransportInfos& transport_infos() { return transport_infos_; }
  const TransportInfo* GetTransportInfoByName(
      const std::string& name) const;
  TransportInfo* GetTransportInfoByName(const std::string& name);
  const TransportDescription* GetTransportDescriptionByName(
      const std::string& name) const {
    const TransportInfo* tinfo = GetTransportInfoByName(name);
    return tinfo ? &tinfo->description : NULL;
  }

  // Transport mutators.
  void set_transport_infos(const TransportInfos& transport_infos) {
    transport_infos_ = transport_infos;
  }
  // Adds a TransportInfo to this description.
  // Returns false if a TransportInfo with the same name already exists.
  bool AddTransportInfo(const TransportInfo& transport_info);
  bool RemoveTransportInfoByName(const std::string& name);

  // Group accessors.
  const ContentGroups& groups() const { return content_groups_; }
  const ContentGroup* GetGroupByName(const std::string& name) const;
  bool HasGroup(const std::string& name) const;

  // Group mutators.
  void AddGroup(const ContentGroup& group) { content_groups_.push_back(group); }
  // Remove the first group with the same semantics specified by |name|.
  void RemoveGroupByName(const std::string& name);

  // Global attributes.
  void set_msid_supported(bool supported) { msid_supported_ = supported; }
  bool msid_supported() const { return msid_supported_; }

 private:
  SessionDescription(const SessionDescription&);

  ContentInfos contents_;
  TransportInfos transport_infos_;
  ContentGroups content_groups_;
  bool msid_supported_ = true;
};

// Indicates whether a ContentDescription was an offer or an answer, as
// described in http://www.ietf.org/rfc/rfc3264.txt.
enum ContentAction { CA_OFFER, CA_PRANSWER, CA_ANSWER };

// Indicates whether a ContentDescription was sent by the local client
// or received from the remote client.
enum ContentSource {
  CS_LOCAL, CS_REMOTE
};

}  // namespace cricket

#endif  // P2P_BASE_SESSIONDESCRIPTION_H_
