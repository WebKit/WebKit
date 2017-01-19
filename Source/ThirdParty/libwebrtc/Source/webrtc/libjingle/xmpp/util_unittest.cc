/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <iostream>
#include <sstream>
#include <string>
#include "webrtc/libjingle/xmllite/xmlelement.h"
#include "webrtc/libjingle/xmpp/util_unittest.h"
#include "webrtc/libjingle/xmpp/xmppengine.h"
#include "webrtc/base/gunit.h"

namespace buzz {

void XmppTestHandler::WriteOutput(const char * bytes, size_t len) {
  output_ << std::string(bytes, len);
}

void XmppTestHandler::StartTls(const std::string & cname) {
  output_ << "[START-TLS " << cname << "]";
}

void XmppTestHandler::CloseConnection() {
  output_ << "[CLOSED]";
}

void XmppTestHandler::OnStateChange(int state) {
  switch (static_cast<XmppEngine::State>(state)) {
  case XmppEngine::STATE_START:
    session_ << "[START]";
    break;
  case XmppEngine::STATE_OPENING:
    session_ << "[OPENING]";
    break;
  case XmppEngine::STATE_OPEN:
    session_ << "[OPEN]";
    break;
  case XmppEngine::STATE_CLOSED:
    session_ << "[CLOSED]";
    switch (engine_->GetError(NULL)) {
    case XmppEngine::ERROR_NONE:
      // do nothing
      break;
    case XmppEngine::ERROR_XML:
      session_ << "[ERROR-XML]";
      break;
    case XmppEngine::ERROR_STREAM:
      session_ << "[ERROR-STREAM]";
      break;
    case XmppEngine::ERROR_VERSION:
      session_ << "[ERROR-VERSION]";
      break;
    case XmppEngine::ERROR_UNAUTHORIZED:
      session_ << "[ERROR-UNAUTHORIZED]";
      break;
    case XmppEngine::ERROR_TLS:
      session_ << "[ERROR-TLS]";
      break;
    case XmppEngine::ERROR_AUTH:
      session_ << "[ERROR-AUTH]";
      break;
    case XmppEngine::ERROR_BIND:
      session_ << "[ERROR-BIND]";
      break;
    case XmppEngine::ERROR_CONNECTION_CLOSED:
      session_ << "[ERROR-CONNECTION-CLOSED]";
      break;
    case XmppEngine::ERROR_DOCUMENT_CLOSED:
      session_ << "[ERROR-DOCUMENT-CLOSED]";
      break;
    default:
      break;
    }
    break;
  default:
    break;
  }
}

bool XmppTestHandler::HandleStanza(const XmlElement * stanza) {
  stanza_ << stanza->Str();
  return true;
}

std::string XmppTestHandler::OutputActivity() {
  std::string result = output_.str();
  output_.str("");
  return result;
}

std::string XmppTestHandler::SessionActivity() {
  std::string result = session_.str();
  session_.str("");
  return result;
}

std::string XmppTestHandler::StanzaActivity() {
  std::string result = stanza_.str();
  stanza_.str("");
  return result;
}

}  // namespace buzz
