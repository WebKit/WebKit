/*
 *  Copyright 2013 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/base/transportdescription.h"

#include "webrtc/base/arraysize.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/p2p/base/p2pconstants.h"

namespace cricket {

bool StringToConnectionRole(const std::string& role_str, ConnectionRole* role) {
  const char* const roles[] = {
      CONNECTIONROLE_ACTIVE_STR,
      CONNECTIONROLE_PASSIVE_STR,
      CONNECTIONROLE_ACTPASS_STR,
      CONNECTIONROLE_HOLDCONN_STR
  };

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

}  // namespace cricket
