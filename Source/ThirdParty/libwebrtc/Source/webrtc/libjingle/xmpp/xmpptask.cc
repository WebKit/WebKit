/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/libjingle/xmpp/constants.h"
#include "webrtc/libjingle/xmpp/xmppclient.h"
#include "webrtc/libjingle/xmpp/xmppengine.h"
#include "webrtc/libjingle/xmpp/xmpptask.h"

namespace buzz {

XmppClientInterface::XmppClientInterface() {
}

XmppClientInterface::~XmppClientInterface() {
}

XmppTask::XmppTask(XmppTaskParentInterface* parent,
                   XmppEngine::HandlerLevel level)
    : XmppTaskBase(parent), stopped_(false) {
#if !defined(NDEBUG)
  debug_force_timeout_ = false;
#endif

  id_ = GetClient()->NextId();
  GetClient()->AddXmppTask(this, level);
  GetClient()->SignalDisconnected.connect(this, &XmppTask::OnDisconnect);
}

XmppTask::~XmppTask() {
  StopImpl();
}

void XmppTask::StopImpl() {
  while (NextStanza() != NULL) {}
  if (!stopped_) {
    GetClient()->RemoveXmppTask(this);
    GetClient()->SignalDisconnected.disconnect(this);
    stopped_ = true;
  }
}

XmppReturnStatus XmppTask::SendStanza(const XmlElement* stanza) {
  if (stopped_)
    return XMPP_RETURN_BADSTATE;
  return GetClient()->SendStanza(stanza);
}

XmppReturnStatus XmppTask::SendStanzaError(const XmlElement* element_original,
                                           XmppStanzaError code,
                                           const std::string& text) {
  if (stopped_)
    return XMPP_RETURN_BADSTATE;
  return GetClient()->SendStanzaError(element_original, code, text);
}

void XmppTask::Stop() {
  StopImpl();
  Task::Stop();
}

void XmppTask::OnDisconnect() {
  Error();
}

void XmppTask::QueueStanza(const XmlElement* stanza) {
#if !defined(NDEBUG)
  if (debug_force_timeout_)
    return;
#endif

  stanza_queue_.push_back(new XmlElement(*stanza));
  Wake();
}

const XmlElement* XmppTask::NextStanza() {
  XmlElement* result = NULL;
  if (!stanza_queue_.empty()) {
    result = stanza_queue_.front();
    stanza_queue_.pop_front();
  }
  next_stanza_.reset(result);
  return result;
}

XmlElement* XmppTask::MakeIq(const std::string& type,
                             const buzz::Jid& to,
                             const std::string& id) {
  XmlElement* result = new XmlElement(QN_IQ);
  if (!type.empty())
    result->AddAttr(QN_TYPE, type);
  if (!to.IsEmpty())
    result->AddAttr(QN_TO, to.Str());
  if (!id.empty())
    result->AddAttr(QN_ID, id);
  return result;
}

XmlElement* XmppTask::MakeIqResult(const XmlElement * query) {
  XmlElement* result = new XmlElement(QN_IQ);
  result->AddAttr(QN_TYPE, STR_RESULT);
  if (query->HasAttr(QN_FROM)) {
    result->AddAttr(QN_TO, query->Attr(QN_FROM));
  }
  result->AddAttr(QN_ID, query->Attr(QN_ID));
  return result;
}

bool XmppTask::MatchResponseIq(const XmlElement* stanza,
                               const Jid& to,
                               const std::string& id) {
  if (stanza->Name() != QN_IQ)
    return false;

  if (stanza->Attr(QN_ID) != id)
    return false;

  return MatchStanzaFrom(stanza, to);
}

bool XmppTask::MatchStanzaFrom(const XmlElement* stanza,
                               const Jid& to) {
  Jid from(stanza->Attr(QN_FROM));
  if (from == to)
    return true;

  // We address the server as "", check if we are doing so here.
  if (!to.IsEmpty())
    return false;

  // It is legal for the server to identify itself with "domain" or
  // "myself@domain"
  Jid me = GetClient()->jid();
  return (from == Jid(me.domain())) || (from == me.BareJid());
}

bool XmppTask::MatchRequestIq(const XmlElement* stanza,
                              const std::string& type,
                              const QName& qn) {
  if (stanza->Name() != QN_IQ)
    return false;

  if (stanza->Attr(QN_TYPE) != type)
    return false;

  if (stanza->FirstNamed(qn) == NULL)
    return false;

  return true;
}

}
