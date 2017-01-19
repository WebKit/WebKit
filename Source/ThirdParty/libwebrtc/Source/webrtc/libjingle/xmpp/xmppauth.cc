/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/libjingle/xmpp/xmppauth.h"

#include <algorithm>

#include "webrtc/libjingle/xmpp/constants.h"
#include "webrtc/libjingle/xmpp/saslcookiemechanism.h"
#include "webrtc/libjingle/xmpp/saslplainmechanism.h"

XmppAuth::XmppAuth() : done_(false) {
}

XmppAuth::~XmppAuth() {
}

void XmppAuth::StartPreXmppAuth(const buzz::Jid& jid,
                                const rtc::SocketAddress& server,
                                const rtc::CryptString& pass,
                                const std::string& auth_mechanism,
                                const std::string& auth_token) {
  jid_ = jid;
  passwd_ = pass;
  auth_mechanism_ = auth_mechanism;
  auth_token_ = auth_token;
  done_ = true;

  SignalAuthDone();
}

static bool contains(const std::vector<std::string>& strings,
                     const std::string& string) {
  return std::find(strings.begin(), strings.end(), string) != strings.end();
}

std::string XmppAuth::ChooseBestSaslMechanism(
    const std::vector<std::string>& mechanisms,
    bool encrypted) {
  // First try Oauth2.
  if (GetAuthMechanism() == buzz::AUTH_MECHANISM_OAUTH2 &&
      contains(mechanisms, buzz::AUTH_MECHANISM_OAUTH2)) {
    return buzz::AUTH_MECHANISM_OAUTH2;
  }

  // A token is the weakest auth - 15s, service-limited, so prefer it.
  if (GetAuthMechanism() == buzz::AUTH_MECHANISM_GOOGLE_TOKEN &&
      contains(mechanisms, buzz::AUTH_MECHANISM_GOOGLE_TOKEN)) {
    return buzz::AUTH_MECHANISM_GOOGLE_TOKEN;
  }

  // A cookie is the next weakest - 14 days.
  if (GetAuthMechanism() == buzz::AUTH_MECHANISM_GOOGLE_COOKIE &&
      contains(mechanisms, buzz::AUTH_MECHANISM_GOOGLE_COOKIE)) {
    return buzz::AUTH_MECHANISM_GOOGLE_COOKIE;
  }

  // As a last resort, use plain authentication.
  if (contains(mechanisms, buzz::AUTH_MECHANISM_PLAIN)) {
    return buzz::AUTH_MECHANISM_PLAIN;
  }

  // No good mechanism found
  return "";
}

buzz::SaslMechanism* XmppAuth::CreateSaslMechanism(
    const std::string& mechanism) {
  if (mechanism == buzz::AUTH_MECHANISM_OAUTH2) {
    return new buzz::SaslCookieMechanism(
        mechanism, jid_.Str(), auth_token_, "oauth2");
  } else if (mechanism == buzz::AUTH_MECHANISM_GOOGLE_TOKEN) {
    return new buzz::SaslCookieMechanism(mechanism, jid_.Str(), auth_token_);
  // } else if (mechanism == buzz::AUTH_MECHANISM_GOOGLE_COOKIE) {
  //   return new buzz::SaslCookieMechanism(mechanism, jid.Str(), sid_);
  } else if (mechanism == buzz::AUTH_MECHANISM_PLAIN) {
    return new buzz::SaslPlainMechanism(jid_, passwd_);
  } else {
    return NULL;
  }
}
