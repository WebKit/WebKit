/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_XMPPAUTH_H_
#define WEBRTC_LIBJINGLE_XMPP_XMPPAUTH_H_

#include <vector>

#include "webrtc/libjingle/xmpp/jid.h"
#include "webrtc/libjingle/xmpp/prexmppauth.h"
#include "webrtc/libjingle/xmpp/saslhandler.h"
#include "webrtc/base/cryptstring.h"
#include "webrtc/base/sigslot.h"

class XmppAuth: public buzz::PreXmppAuth {
public:
  XmppAuth();
  virtual ~XmppAuth();

  // TODO: Just have one "secret" that is either pass or
  // token?
  virtual void StartPreXmppAuth(const buzz::Jid& jid,
                                const rtc::SocketAddress& server,
                                const rtc::CryptString& pass,
                                const std::string& auth_mechanism,
                                const std::string& auth_token);

  virtual bool IsAuthDone() const { return done_; }
  virtual bool IsAuthorized() const { return true; }
  virtual bool HadError() const { return false; }
  virtual int  GetError() const { return 0; }
  virtual buzz::CaptchaChallenge GetCaptchaChallenge() const {
      return buzz::CaptchaChallenge();
  }
  virtual std::string GetAuthMechanism() const { return auth_mechanism_; }
  virtual std::string GetAuthToken() const { return auth_token_; }

  virtual std::string ChooseBestSaslMechanism(
      const std::vector<std::string>& mechanisms,
      bool encrypted);

  virtual buzz::SaslMechanism * CreateSaslMechanism(
      const std::string& mechanism);

private:
  buzz::Jid jid_;
  rtc::CryptString passwd_;
  std::string auth_mechanism_;
  std::string auth_token_;
  bool done_;
};

#endif  // WEBRTC_LIBJINGLE_XMPP_XMPPAUTH_H_

