/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/libjingle/xmllite/xmlelement.h"
#include "webrtc/libjingle/xmpp/constants.h"
#include "webrtc/libjingle/xmpp/saslmechanism.h"
#include "webrtc/base/base64.h"

using rtc::Base64;

namespace buzz {

XmlElement *
SaslMechanism::StartSaslAuth() {
  return new XmlElement(QN_SASL_AUTH, true);
}

XmlElement *
SaslMechanism::HandleSaslChallenge(const XmlElement * challenge) {
  return new XmlElement(QN_SASL_ABORT, true);
}

void
SaslMechanism::HandleSaslSuccess(const XmlElement * success) {
}

void
SaslMechanism::HandleSaslFailure(const XmlElement * failure) {
}

std::string
SaslMechanism::Base64Encode(const std::string & plain) {
  return Base64::Encode(plain);
}

std::string
SaslMechanism::Base64Decode(const std::string & encoded) {
  return Base64::Decode(encoded, Base64::DO_LAX);
}

std::string
SaslMechanism::Base64EncodeFromArray(const char * plain, size_t length) {
  std::string result;
  Base64::EncodeFromArray(plain, length, &result);
  return result;
}

}
