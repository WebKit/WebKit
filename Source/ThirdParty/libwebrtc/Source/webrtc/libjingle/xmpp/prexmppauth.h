/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_PREXMPPAUTH_H_
#define WEBRTC_LIBJINGLE_XMPP_PREXMPPAUTH_H_

#include "webrtc/libjingle/xmpp/saslhandler.h"
#include "webrtc/base/cryptstring.h"
#include "webrtc/base/sigslot.h"

namespace rtc {
  class SocketAddress;
}

namespace buzz {

class Jid;
class SaslMechanism;

class CaptchaChallenge {
 public:
  CaptchaChallenge() : captcha_needed_(false) {}
  CaptchaChallenge(const std::string& token, const std::string& url)
    : captcha_needed_(true), captcha_token_(token), captcha_image_url_(url) {
  }

  bool captcha_needed() const { return captcha_needed_; }
  const std::string& captcha_token() const { return captcha_token_; }

  // This url is relative to the gaia server.  Once we have better tools
  // for cracking URLs, we should probably make this a full URL
  const std::string& captcha_image_url() const { return captcha_image_url_; }

 private:
  bool captcha_needed_;
  std::string captcha_token_;
  std::string captcha_image_url_;
};

class PreXmppAuth : public SaslHandler {
public:
  virtual ~PreXmppAuth() {}

  virtual void StartPreXmppAuth(
    const Jid& jid,
    const rtc::SocketAddress& server,
    const rtc::CryptString& pass,
    const std::string& auth_mechanism,
    const std::string& auth_token) = 0;

  sigslot::signal0<> SignalAuthDone;

  virtual bool IsAuthDone() const = 0;
  virtual bool IsAuthorized() const = 0;
  virtual bool HadError() const = 0;
  virtual int GetError() const = 0;
  virtual CaptchaChallenge GetCaptchaChallenge() const = 0;
  virtual std::string GetAuthMechanism() const = 0;
  virtual std::string GetAuthToken() const = 0;
};

}

#endif  // WEBRTC_LIBJINGLE_XMPP_PREXMPPAUTH_H_
