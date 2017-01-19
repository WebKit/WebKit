/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_JID_H_
#define WEBRTC_LIBJINGLE_XMPP_JID_H_

#include <string>
#include "webrtc/libjingle/xmllite/xmlconstants.h"
#include "webrtc/base/basictypes.h"

namespace buzz {

// The Jid class encapsulates and provides parsing help for Jids. A Jid
// consists of three parts: the node, the domain and the resource, e.g.:
//
// node@domain/resource
//
// The node and resource are both optional. A valid jid is defined to have
// a domain. A bare jid is defined to not have a resource and a full jid
// *does* have a resource.
class Jid {
public:
  explicit Jid();
  explicit Jid(const std::string& jid_string);
  explicit Jid(const std::string& node_name,
               const std::string& domain_name,
               const std::string& resource_name);
  ~Jid();

  const std::string & node() const { return node_name_; }
  const std::string & domain() const { return domain_name_;  }
  const std::string & resource() const { return resource_name_; }

  std::string Str() const;
  Jid BareJid() const;

  bool IsEmpty() const;
  bool IsValid() const;
  bool IsBare() const;
  bool IsFull() const;

  bool BareEquals(const Jid& other) const;
  void CopyFrom(const Jid& jid);
  bool operator==(const Jid& other) const;
  bool operator!=(const Jid& other) const { return !operator==(other); }

  bool operator<(const Jid& other) const { return Compare(other) < 0; };
  bool operator>(const Jid& other) const { return Compare(other) > 0; };

  int Compare(const Jid & other) const;

private:
  void ValidateOrReset();

  static std::string PrepNode(const std::string& node, bool* valid);
  static char PrepNodeAscii(char ch, bool* valid);
  static std::string PrepResource(const std::string& start, bool* valid);
  static char PrepResourceAscii(char ch, bool* valid);
  static std::string PrepDomain(const std::string& domain, bool* valid);
  static void PrepDomain(const std::string& domain,
                         std::string* buf, bool* valid);
  static void PrepDomainLabel(
      std::string::const_iterator start, std::string::const_iterator end,
      std::string* buf, bool* valid);
  static char PrepDomainLabelAscii(char ch, bool *valid);

  std::string node_name_;
  std::string domain_name_;
  std::string resource_name_;
};

}

#endif  // WEBRTC_LIBJINGLE_XMPP_JID_H_
