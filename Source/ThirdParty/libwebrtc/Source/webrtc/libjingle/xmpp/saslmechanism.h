/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_SASLMECHANISM_H_
#define WEBRTC_LIBJINGLE_XMPP_SASLMECHANISM_H_

#include <string>

namespace buzz {

class XmlElement;


// Defines a mechnanism to do SASL authentication.
// Subclass instances should have a self-contained way to present
// credentials.
class SaslMechanism {

public:
  
  // Intended to be subclassed
  virtual ~SaslMechanism() {}

  // Should return the name of the SASL mechanism, e.g., "PLAIN"
  virtual std::string GetMechanismName() = 0;

  // Should generate the initial "auth" request.  Default is just <auth/>.
  virtual XmlElement * StartSaslAuth();

  // Should respond to a SASL "<challenge>" request.  Default is
  // to abort (for mechanisms that do not do challenge-response)
  virtual XmlElement * HandleSaslChallenge(const XmlElement * challenge);

  // Notification of a SASL "<success>".  Sometimes information
  // is passed on success.
  virtual void HandleSaslSuccess(const XmlElement * success);

  // Notification of a SASL "<failure>".  Sometimes information
  // for the user is passed on failure.
  virtual void HandleSaslFailure(const XmlElement * failure);

protected:
  static std::string Base64Encode(const std::string & plain);
  static std::string Base64Decode(const std::string & encoded);
  static std::string Base64EncodeFromArray(const char * plain, size_t length);
};

}

#endif  // WEBRTC_LIBJINGLE_XMPP_SASLMECHANISM_H_
