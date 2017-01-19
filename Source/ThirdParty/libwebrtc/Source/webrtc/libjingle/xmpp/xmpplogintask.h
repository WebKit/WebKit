/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_LOGINTASK_H_
#define WEBRTC_LIBJINGLE_XMPP_LOGINTASK_H_

#include <memory>
#include <string>
#include <vector>

#include "webrtc/libjingle/xmpp/jid.h"
#include "webrtc/libjingle/xmpp/xmppengine.h"
#include "webrtc/base/logging.h"

namespace buzz {

class XmlElement;
class XmppEngineImpl;
class SaslMechanism;


// TODO: Rename to LoginTask.
class XmppLoginTask {

public:
  XmppLoginTask(XmppEngineImpl *pctx);
  ~XmppLoginTask();

  bool IsDone()
    { return state_ == LOGINSTATE_DONE; }
  void IncomingStanza(const XmlElement * element, bool isStart);
  void OutgoingStanza(const XmlElement *element);
  void set_allow_non_google_login(bool b)
    { allowNonGoogleLogin_ = b; }

private:
  enum LoginTaskState {
    LOGINSTATE_INIT = 0,
    LOGINSTATE_STREAMSTART_SENT,
    LOGINSTATE_STARTED_XMPP,
    LOGINSTATE_TLS_INIT,
    LOGINSTATE_AUTH_INIT,
    LOGINSTATE_BIND_INIT,
    LOGINSTATE_TLS_REQUESTED,
    LOGINSTATE_SASL_RUNNING,
    LOGINSTATE_BIND_REQUESTED,
    LOGINSTATE_SESSION_REQUESTED,
    LOGINSTATE_DONE,
  };

  const XmlElement * NextStanza();
  bool Advance();
  bool HandleStartStream(const XmlElement * element);
  bool HandleFeatures(const XmlElement * element);
  const XmlElement * GetFeature(const QName & name);
  bool Failure(XmppEngine::Error reason);
  void FlushQueuedStanzas();

  XmppEngineImpl * pctx_;
  bool authNeeded_;
  bool allowNonGoogleLogin_;
  LoginTaskState state_;
  const XmlElement * pelStanza_;
  bool isStart_;
  std::string iqId_;
  std::unique_ptr<XmlElement> pelFeatures_;
  Jid fullJid_;
  std::string streamId_;
  std::unique_ptr<std::vector<XmlElement *> > pvecQueuedStanzas_;

  std::unique_ptr<SaslMechanism> sasl_mech_;

#if !defined(NDEBUG)
  static const rtc::ConstantLabel LOGINTASK_STATES[];
#endif
};

}

#endif  //  WEBRTC_LIBJINGLE_XMPP_LOGINTASK_H_
