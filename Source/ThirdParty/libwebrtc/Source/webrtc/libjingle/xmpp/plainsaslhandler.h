/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_PLAINSASLHANDLER_H_
#define WEBRTC_LIBJINGLE_XMPP_PLAINSASLHANDLER_H_

#include <algorithm>
#include "webrtc/libjingle/xmpp/saslhandler.h"
#include "webrtc/libjingle/xmpp/saslplainmechanism.h"
#include "webrtc/base/cryptstring.h"

namespace buzz {

class PlainSaslHandler : public SaslHandler {
public:
  PlainSaslHandler(const Jid & jid, const rtc::CryptString & password, 
      bool allow_plain) : jid_(jid), password_(password), 
                          allow_plain_(allow_plain) {}
    
  virtual ~PlainSaslHandler() {}

  // Should pick the best method according to this handler
  // returns the empty string if none are suitable
  virtual std::string ChooseBestSaslMechanism(const std::vector<std::string> & mechanisms, bool encrypted) {
  
    if (!encrypted && !allow_plain_) {
      return "";
    }
    
    std::vector<std::string>::const_iterator it = std::find(mechanisms.begin(), mechanisms.end(), "PLAIN");
    if (it == mechanisms.end()) {
      return "";
    }
    else {
      return "PLAIN";
    }
  }

  // Creates a SaslMechanism for the given mechanism name (you own it
  // once you get it).  If not handled, return NULL.
  virtual SaslMechanism * CreateSaslMechanism(const std::string & mechanism) {
    if (mechanism == "PLAIN") {
      return new SaslPlainMechanism(jid_, password_);
    }
    return NULL;
  }
  
private:
  Jid jid_;
  rtc::CryptString password_;
  bool allow_plain_;
};


}

#endif  // WEBRTC_LIBJINGLE_XMPP_PLAINSASLHANDLER_H_
