/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_MODULE_H_
#define WEBRTC_LIBJINGLE_XMPP_MODULE_H_

#include "webrtc/libjingle/xmpp/xmppengine.h"

namespace buzz {

class XmppEngine;

//! This is the base class for extension modules.
//! An engine is registered with the module and the module then hooks the
//! appropriate parts of the engine to implement that set of features.  It is
//! important to unregister modules before destructing the engine.
class XmppModule {
public:
  virtual ~XmppModule() {}

  //! Register the engine with the module.  Only one engine can be associated
  //! with a module at a time.  This method will return an error if there is
  //! already an engine registered.
  virtual XmppReturnStatus RegisterEngine(XmppEngine* engine) = 0;
};

}
#endif  // WEBRTC_LIBJINGLE_XMPP_MODULE_H_
