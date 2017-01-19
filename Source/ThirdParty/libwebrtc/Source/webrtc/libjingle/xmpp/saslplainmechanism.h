/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_SASLPLAINMECHANISM_H_
#define WEBRTC_LIBJINGLE_XMPP_SASLPLAINMECHANISM_H_

#include "webrtc/libjingle/xmpp/saslmechanism.h"
#include "webrtc/base/cryptstring.h"

namespace buzz {

class SaslPlainMechanism : public SaslMechanism {

public:
  SaslPlainMechanism(const buzz::Jid user_jid, const rtc::CryptString & password) :
    user_jid_(user_jid), password_(password) {}

  virtual std::string GetMechanismName() { return "PLAIN"; }

  virtual XmlElement * StartSaslAuth() {
    // send initial request
    XmlElement * el = new XmlElement(QN_SASL_AUTH, true);
    el->AddAttr(QN_MECHANISM, "PLAIN");

    rtc::FormatCryptString credential;
    credential.Append("\0", 1);
    credential.Append(user_jid_.node());
    credential.Append("\0", 1);
    credential.Append(&password_);
    el->AddText(Base64EncodeFromArray(credential.GetData(), credential.GetLength()));
    return el;
  }

private:
  Jid user_jid_;
  rtc::CryptString password_;
};

}

#endif  // WEBRTC_LIBJINGLE_XMPP_SASLPLAINMECHANISM_H_
