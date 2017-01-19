/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <vector>
#include "webrtc/libjingle/xmpp/constants.h"
#include "webrtc/libjingle/xmpp/xmppengineimpl.h"
#include "webrtc/base/common.h"

namespace buzz {

class XmppIqEntry {
  XmppIqEntry(const std::string & id, const std::string & to,
               XmppEngine * pxce, XmppIqHandler * iq_handler) :
    id_(id),
    to_(to),
    engine_(pxce),
    iq_handler_(iq_handler) {
  }

private:
  friend class XmppEngineImpl;

  const std::string id_;
  const std::string to_;
  XmppEngine * const engine_;
  XmppIqHandler * const iq_handler_;
};


XmppReturnStatus
XmppEngineImpl::SendIq(const XmlElement * element, XmppIqHandler * iq_handler,
  XmppIqCookie* cookie) {
  if (state_ == STATE_CLOSED)
    return XMPP_RETURN_BADSTATE;
  if (NULL == iq_handler)
    return XMPP_RETURN_BADARGUMENT;
  if (!element || element->Name() != QN_IQ)
    return XMPP_RETURN_BADARGUMENT;

  const std::string& type = element->Attr(QN_TYPE);
  if (type != "get" && type != "set")
    return XMPP_RETURN_BADARGUMENT;

  if (!element->HasAttr(QN_ID))
    return XMPP_RETURN_BADARGUMENT;
  const std::string& id = element->Attr(QN_ID);

  XmppIqEntry * iq_entry = new XmppIqEntry(id,
                                              element->Attr(QN_TO),
                                              this, iq_handler);
  iq_entries_->push_back(iq_entry);
  SendStanza(element);

  if (cookie)
    *cookie = iq_entry;

  return XMPP_RETURN_OK;
}


XmppReturnStatus
XmppEngineImpl::RemoveIqHandler(XmppIqCookie cookie,
    XmppIqHandler ** iq_handler) {

  std::vector<XmppIqEntry*, std::allocator<XmppIqEntry*> >::iterator pos;

  pos = std::find(iq_entries_->begin(),
                  iq_entries_->end(),
                  reinterpret_cast<XmppIqEntry*>(cookie));

  if (pos == iq_entries_->end())
    return XMPP_RETURN_BADARGUMENT;

  XmppIqEntry* entry = *pos;
  iq_entries_->erase(pos);
  if (iq_handler)
    *iq_handler = entry->iq_handler_;
  delete entry;

  return XMPP_RETURN_OK;
}

void
XmppEngineImpl::DeleteIqCookies() {
  for (size_t i = 0; i < iq_entries_->size(); i += 1) {
    XmppIqEntry * iq_entry_ = (*iq_entries_)[i];
    (*iq_entries_)[i] = NULL;
    delete iq_entry_;
  }
  iq_entries_->clear();
}

static void
AecImpl(XmlElement * error_element, const QName & name,
        const char * type, const char * code) {
  error_element->AddElement(new XmlElement(QN_ERROR));
  error_element->AddAttr(QN_CODE, code, 1);
  error_element->AddAttr(QN_TYPE, type, 1);
  error_element->AddElement(new XmlElement(name, true), 1);
}


static void
AddErrorCode(XmlElement * error_element, XmppStanzaError code) {
  switch (code) {
    case XSE_BAD_REQUEST:
      AecImpl(error_element, QN_STANZA_BAD_REQUEST, "modify", "400");
      break;
    case XSE_CONFLICT:
      AecImpl(error_element, QN_STANZA_CONFLICT, "cancel", "409");
      break;
    case XSE_FEATURE_NOT_IMPLEMENTED:
      AecImpl(error_element, QN_STANZA_FEATURE_NOT_IMPLEMENTED,
              "cancel", "501");
      break;
    case XSE_FORBIDDEN:
      AecImpl(error_element, QN_STANZA_FORBIDDEN, "auth", "403");
      break;
    case XSE_GONE:
      AecImpl(error_element, QN_STANZA_GONE, "modify", "302");
      break;
    case XSE_INTERNAL_SERVER_ERROR:
      AecImpl(error_element, QN_STANZA_INTERNAL_SERVER_ERROR, "wait", "500");
      break;
    case XSE_ITEM_NOT_FOUND:
      AecImpl(error_element, QN_STANZA_ITEM_NOT_FOUND, "cancel", "404");
      break;
    case XSE_JID_MALFORMED:
      AecImpl(error_element, QN_STANZA_JID_MALFORMED, "modify", "400");
      break;
    case XSE_NOT_ACCEPTABLE:
      AecImpl(error_element, QN_STANZA_NOT_ACCEPTABLE, "cancel", "406");
      break;
    case XSE_NOT_ALLOWED:
      AecImpl(error_element, QN_STANZA_NOT_ALLOWED, "cancel", "405");
      break;
    case XSE_PAYMENT_REQUIRED:
      AecImpl(error_element, QN_STANZA_PAYMENT_REQUIRED, "auth", "402");
      break;
    case XSE_RECIPIENT_UNAVAILABLE:
      AecImpl(error_element, QN_STANZA_RECIPIENT_UNAVAILABLE, "wait", "404");
      break;
    case XSE_REDIRECT:
      AecImpl(error_element, QN_STANZA_REDIRECT, "modify", "302");
      break;
    case XSE_REGISTRATION_REQUIRED:
      AecImpl(error_element, QN_STANZA_REGISTRATION_REQUIRED, "auth", "407");
      break;
    case XSE_SERVER_NOT_FOUND:
      AecImpl(error_element, QN_STANZA_REMOTE_SERVER_NOT_FOUND,
              "cancel", "404");
      break;
    case XSE_SERVER_TIMEOUT:
      AecImpl(error_element, QN_STANZA_REMOTE_SERVER_TIMEOUT, "wait", "502");
      break;
    case XSE_RESOURCE_CONSTRAINT:
      AecImpl(error_element, QN_STANZA_RESOURCE_CONSTRAINT, "wait", "500");
      break;
    case XSE_SERVICE_UNAVAILABLE:
      AecImpl(error_element, QN_STANZA_SERVICE_UNAVAILABLE, "cancel", "503");
      break;
    case XSE_SUBSCRIPTION_REQUIRED:
      AecImpl(error_element, QN_STANZA_SUBSCRIPTION_REQUIRED, "auth", "407");
      break;
    case XSE_UNDEFINED_CONDITION:
      AecImpl(error_element, QN_STANZA_UNDEFINED_CONDITION, "wait", "500");
      break;
    case XSE_UNEXPECTED_REQUEST:
      AecImpl(error_element, QN_STANZA_UNEXPECTED_REQUEST, "wait", "400");
      break;
  }
}


XmppReturnStatus
XmppEngineImpl::SendStanzaError(const XmlElement * element_original,
                                XmppStanzaError code,
                                const std::string & text) {

  if (state_ == STATE_CLOSED)
    return XMPP_RETURN_BADSTATE;

  XmlElement error_element(element_original->Name());
  error_element.AddAttr(QN_TYPE, "error");

  // copy attrs, copy 'from' to 'to' and strip 'from'
  for (const XmlAttr * attribute = element_original->FirstAttr();
       attribute; attribute = attribute->NextAttr()) {
    QName name = attribute->Name();
    if (name == QN_TO)
      continue; // no need to put a from attr.  Server will stamp stanza
    else if (name == QN_FROM)
      name = QN_TO;
    else if (name == QN_TYPE)
      continue;
    error_element.AddAttr(name, attribute->Value());
  }

  // copy children
  for (const XmlChild * child = element_original->FirstChild();
       child;
       child = child->NextChild()) {
    if (child->IsText()) {
      error_element.AddText(child->AsText()->Text());
    } else {
      error_element.AddElement(new XmlElement(*(child->AsElement())));
    }
  }

  // add error information
  AddErrorCode(&error_element, code);
  if (text != STR_EMPTY) {
    XmlElement * text_element = new XmlElement(QN_STANZA_TEXT, true);
    text_element->AddText(text);
    error_element.AddElement(text_element);
  }

  SendStanza(&error_element);

  return XMPP_RETURN_OK;
}


bool
XmppEngineImpl::HandleIqResponse(const XmlElement * element) {
  if (iq_entries_->empty())
    return false;
  if (element->Name() != QN_IQ)
    return false;
  std::string type = element->Attr(QN_TYPE);
  if (type != "result" && type != "error")
    return false;
  if (!element->HasAttr(QN_ID))
    return false;
  std::string id = element->Attr(QN_ID);
  std::string from = element->Attr(QN_FROM);

  for (std::vector<XmppIqEntry *>::iterator it = iq_entries_->begin();
       it != iq_entries_->end(); it += 1) {
    XmppIqEntry * iq_entry = *it;
    if (iq_entry->id_ == id && iq_entry->to_ == from) {
      iq_entries_->erase(it);
      iq_entry->iq_handler_->IqResponse(iq_entry, element);
      delete iq_entry;
      return true;
    }
  }

  return false;
}

}
