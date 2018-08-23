/*
 *  Copyright 2013 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/transportdescription.h"

#include "p2p/base/p2pconstants.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/stringutils.h"

namespace cricket {

bool StringToConnectionRole(const std::string& role_str, ConnectionRole* role) {
  const char* const roles[] = {
      CONNECTIONROLE_ACTIVE_STR, CONNECTIONROLE_PASSIVE_STR,
      CONNECTIONROLE_ACTPASS_STR, CONNECTIONROLE_HOLDCONN_STR};

  for (size_t i = 0; i < arraysize(roles); ++i) {
    if (_stricmp(roles[i], role_str.c_str()) == 0) {
      *role = static_cast<ConnectionRole>(CONNECTIONROLE_ACTIVE + i);
      return true;
    }
  }
  return false;
}

bool ConnectionRoleToString(const ConnectionRole& role, std::string* role_str) {
  switch (role) {
    case cricket::CONNECTIONROLE_ACTIVE:
      *role_str = cricket::CONNECTIONROLE_ACTIVE_STR;
      break;
    case cricket::CONNECTIONROLE_ACTPASS:
      *role_str = cricket::CONNECTIONROLE_ACTPASS_STR;
      break;
    case cricket::CONNECTIONROLE_PASSIVE:
      *role_str = cricket::CONNECTIONROLE_PASSIVE_STR;
      break;
    case cricket::CONNECTIONROLE_HOLDCONN:
      *role_str = cricket::CONNECTIONROLE_HOLDCONN_STR;
      break;
    default:
      return false;
  }
  return true;
}

TransportDescription::TransportDescription()
    : ice_mode(ICEMODE_FULL), connection_role(CONNECTIONROLE_NONE) {}

TransportDescription::TransportDescription(
    const std::vector<std::string>& transport_options,
    const std::string& ice_ufrag,
    const std::string& ice_pwd,
    IceMode ice_mode,
    ConnectionRole role,
    const rtc::SSLFingerprint* identity_fingerprint)
    : transport_options(transport_options),
      ice_ufrag(ice_ufrag),
      ice_pwd(ice_pwd),
      ice_mode(ice_mode),
      connection_role(role),
      identity_fingerprint(CopyFingerprint(identity_fingerprint)) {}

TransportDescription::TransportDescription(const std::string& ice_ufrag,
                                           const std::string& ice_pwd)
    : ice_ufrag(ice_ufrag),
      ice_pwd(ice_pwd),
      ice_mode(ICEMODE_FULL),
      connection_role(CONNECTIONROLE_NONE) {}

TransportDescription::TransportDescription(const TransportDescription& from)
    : transport_options(from.transport_options),
      ice_ufrag(from.ice_ufrag),
      ice_pwd(from.ice_pwd),
      ice_mode(from.ice_mode),
      connection_role(from.connection_role),
      identity_fingerprint(CopyFingerprint(from.identity_fingerprint.get())) {}

TransportDescription::~TransportDescription() = default;

TransportDescription& TransportDescription::operator=(
    const TransportDescription& from) {
  // Self-assignment
  if (this == &from)
    return *this;

  transport_options = from.transport_options;
  ice_ufrag = from.ice_ufrag;
  ice_pwd = from.ice_pwd;
  ice_mode = from.ice_mode;
  connection_role = from.connection_role;

  identity_fingerprint.reset(CopyFingerprint(from.identity_fingerprint.get()));
  return *this;
}

}  // namespace cricket
