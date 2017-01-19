/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_SASLCOOKIEMECHANISM_H_
#define WEBRTC_LIBJINGLE_XMPP_SASLCOOKIEMECHANISM_H_

#include "webrtc/libjingle/xmllite/qname.h"
#include "webrtc/libjingle/xmllite/xmlelement.h"
#include "webrtc/libjingle/xmpp/constants.h"
#include "webrtc/libjingle/xmpp/saslmechanism.h"

namespace buzz {

class SaslCookieMechanism : public SaslMechanism {

public:
  SaslCookieMechanism(const std::string & mechanism,
                      const std::string & username,
                      const std::string & cookie,
                      const std::string & token_service)
    : mechanism_(mechanism),
      username_(username),
      cookie_(cookie),
      token_service_(token_service) {}

  SaslCookieMechanism(const std::string & mechanism,
                      const std::string & username,
                      const std::string & cookie)
    : mechanism_(mechanism),
      username_(username),
      cookie_(cookie),
      token_service_("") {}

  virtual std::string GetMechanismName() { return mechanism_; }

  virtual XmlElement * StartSaslAuth() {
    // send initial request
    XmlElement * el = new XmlElement(QN_SASL_AUTH, true);
    el->AddAttr(QN_MECHANISM, mechanism_);
    if (!token_service_.empty()) {
      el->AddAttr(QN_GOOGLE_AUTH_SERVICE, token_service_);
    }

    std::string credential;
    credential.append("\0", 1);
    credential.append(username_);
    credential.append("\0", 1);
    credential.append(cookie_);
    el->AddText(Base64Encode(credential));
    return el;
  }

private:
  std::string mechanism_;
  std::string username_;
  std::string cookie_;
  std::string token_service_;
};

}

#endif  // WEBRTC_LIBJINGLE_XMPP_SASLCOOKIEMECHANISM_H_
