/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_MODULEIMPL_H_
#define WEBRTC_LIBJINGLE_XMPP_MODULEIMPL_H_

#include "webrtc/libjingle/xmpp/module.h"
#include "webrtc/libjingle/xmpp/xmppengine.h"

namespace buzz {

//! This is the base implementation class for extension modules.
//! An engine is registered with the module and the module then hooks the
//! appropriate parts of the engine to implement that set of features.  It is
//! important to unregister modules before destructing the engine.
class XmppModuleImpl {
protected:
  XmppModuleImpl();
  virtual ~XmppModuleImpl();

  //! Register the engine with the module.  Only one engine can be associated
  //! with a module at a time.  This method will return an error if there is
  //! already an engine registered.
  XmppReturnStatus RegisterEngine(XmppEngine* engine);

  //! Gets the engine that this module is attached to.
  XmppEngine* engine();

  //! Process the given stanza.
  //! The module must return true if it has handled the stanza.
  //! A false return value causes the stanza to be passed on to
  //! the next registered handler.
  virtual bool HandleStanza(const XmlElement *) { return false; };

private:

  //! The ModuleSessionHelper nested class allows the Module
  //! to hook into and get stanzas and events from the engine.
  class ModuleStanzaHandler : public XmppStanzaHandler {
    friend class XmppModuleImpl;

    ModuleStanzaHandler(XmppModuleImpl* module) :
      module_(module) {
    }

    bool HandleStanza(const XmlElement* stanza) {
      return module_->HandleStanza(stanza);
    }

    XmppModuleImpl* module_;
  };

  friend class ModuleStanzaHandler;

  XmppEngine* engine_;
  ModuleStanzaHandler stanza_handler_;
};


// This macro will implement the XmppModule interface for a class
// that derives from both XmppModuleImpl and XmppModule
#define IMPLEMENT_XMPPMODULE \
  XmppReturnStatus RegisterEngine(XmppEngine* engine) { \
    return XmppModuleImpl::RegisterEngine(engine); \
  }

}

#endif  // WEBRTC_LIBJINGLE_XMPP_MODULEIMPL_H_
