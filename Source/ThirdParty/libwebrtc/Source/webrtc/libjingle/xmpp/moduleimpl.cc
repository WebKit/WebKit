/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/libjingle/xmpp/moduleimpl.h"
#include "webrtc/base/common.h"

namespace buzz {

XmppModuleImpl::XmppModuleImpl() :
  engine_(NULL),
  stanza_handler_(this) {
}

XmppModuleImpl::~XmppModuleImpl()
{
  if (engine_ != NULL) {
    engine_->RemoveStanzaHandler(&stanza_handler_);
    engine_ = NULL;
  }
}

XmppReturnStatus
XmppModuleImpl::RegisterEngine(XmppEngine* engine)
{
  if (NULL == engine || NULL != engine_)
    return XMPP_RETURN_BADARGUMENT;

  engine->AddStanzaHandler(&stanza_handler_);
  engine_ = engine;

  return XMPP_RETURN_OK;
}

XmppEngine*
XmppModuleImpl::engine() {
  ASSERT(NULL != engine_);
  return engine_;
}

}

