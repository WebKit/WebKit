/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_SASLHANDLER_H_
#define WEBRTC_LIBJINGLE_XMPP_SASLHANDLER_H_

#include <string>
#include <vector>

namespace buzz {

class XmlElement;
class SaslMechanism;

// Creates mechanisms to deal with a given mechanism
class SaslHandler {

public:
  
  // Intended to be subclassed
  virtual ~SaslHandler() {}

  // Should pick the best method according to this handler
  // returns the empty string if none are suitable
  virtual std::string ChooseBestSaslMechanism(const std::vector<std::string> & mechanisms, bool encrypted) = 0;

  // Creates a SaslMechanism for the given mechanism name (you own it
  // once you get it).
  // If not handled, return NULL.
  virtual SaslMechanism * CreateSaslMechanism(const std::string & mechanism) = 0;
};

}

#endif  // WEBRTC_LIBJINGLE_XMPP_SASLHANDLER_H_
