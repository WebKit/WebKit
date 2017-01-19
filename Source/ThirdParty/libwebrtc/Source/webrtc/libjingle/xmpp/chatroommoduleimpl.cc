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
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include "webrtc/libjingle/xmpp/chatroommodule.h"
#include "webrtc/libjingle/xmpp/constants.h"
#include "webrtc/libjingle/xmpp/moduleimpl.h"
#include "webrtc/base/arraysize.h"
#include "webrtc/base/common.h"

namespace buzz {

// forward declarations
class XmppChatroomImpl;
class XmppChatroomMemberImpl;

//! Module that encapsulates multiple chatrooms.
//! Each chatroom is represented by an XmppChatroomImpl instance
class XmppChatroomModuleImpl : public XmppChatroomModule,
  public XmppModuleImpl, public XmppIqHandler {
public:
  IMPLEMENT_XMPPMODULE

   // Creates a chatroom with specified Jid
  XmppChatroomModuleImpl();
  ~XmppChatroomModuleImpl();

  // XmppChatroomModule
  virtual XmppReturnStatus set_chatroom_handler(XmppChatroomHandler* handler);
  virtual XmppChatroomHandler* chatroom_handler();
  virtual XmppReturnStatus set_chatroom_jid(const Jid& chatroom_jid);
  virtual const Jid& chatroom_jid() const;
  virtual XmppReturnStatus set_nickname(const std::string& nickname);
  virtual const std::string& nickname() const;
  virtual const Jid member_jid() const;
  virtual XmppReturnStatus RequestEnterChatroom(const std::string& password,
      const std::string& client_version,
      const std::string& locale);
  virtual XmppReturnStatus RequestExitChatroom();
  virtual XmppReturnStatus RequestConnectionStatusChange(
      XmppPresenceConnectionStatus connection_status);
  virtual size_t GetChatroomMemberCount();
  virtual XmppReturnStatus CreateMemberEnumerator(XmppChatroomMemberEnumerator** enumerator);
  virtual const std::string subject();
  virtual XmppChatroomState state() { return chatroom_state_; }
  virtual XmppReturnStatus SendMessage(const XmlElement& message);

  // XmppModule
  virtual void IqResponse(XmppIqCookie cookie, const XmlElement * pelStanza) {RTC_UNUSED2(cookie, pelStanza);}
  virtual bool HandleStanza(const XmlElement *);

private:
  friend class XmppChatroomMemberEnumeratorImpl;

  XmppReturnStatus ServerChangeMyPresence(const XmlElement& presence);
  XmppReturnStatus ClientChangeMyPresence(XmppChatroomState new_state);
  XmppReturnStatus ChangePresence(XmppChatroomState new_state, const XmlElement* presence, bool isServer);
  XmppReturnStatus ServerChangedOtherPresence(const XmlElement& presence_element);
  XmppChatroomEnteredStatus GetEnterFailureFromXml(const XmlElement* presence);
  XmppChatroomExitedStatus GetExitFailureFromXml(const XmlElement* presence);

  bool CheckEnterChatroomStateOk();

  void FireEnteredStatus(const XmlElement* presence,
                         XmppChatroomEnteredStatus status);
  void FireExitStatus(XmppChatroomExitedStatus status);
  void FireMessageReceived(const XmlElement& message);
  void FireMemberEntered(const XmppChatroomMember* entered_member);
  void FireMemberChanged(const XmppChatroomMember* changed_member);
  void FireMemberExited(const XmppChatroomMember* exited_member);


  typedef std::map<Jid, XmppChatroomMemberImpl*> JidMemberMap;

  XmppChatroomHandler*              chatroom_handler_;
  Jid                               chatroom_jid_;
  std::string                       nickname_;
  XmppChatroomState                 chatroom_state_;
  JidMemberMap                      chatroom_jid_members_;
  int                               chatroom_jid_members_version_;
};


class XmppChatroomMemberImpl : public XmppChatroomMember {
public:
  ~XmppChatroomMemberImpl() {}
  XmppReturnStatus SetPresence(const XmppPresence* presence);

  // XmppChatroomMember
  const Jid member_jid() const;
  const Jid full_jid() const;
  const std::string name() const;
  const XmppPresence* presence() const;

private:
  std::unique_ptr<XmppPresence>  presence_;
};

class XmppChatroomMemberEnumeratorImpl :
        public XmppChatroomMemberEnumerator  {
public:
  XmppChatroomMemberEnumeratorImpl(XmppChatroomModuleImpl::JidMemberMap* chatroom_jid_members,
                                        int* map_version);

  // XmppChatroomMemberEnumerator
  virtual XmppChatroomMember* current();
  virtual bool Next();
  virtual bool Prev();
  virtual bool IsValid();
  virtual bool IsBeforeBeginning();
  virtual bool IsAfterEnd();

private:
  XmppChatroomModuleImpl::JidMemberMap*           map_;
  int                                             map_version_created_;
  int*                                            map_version_;
  XmppChatroomModuleImpl::JidMemberMap::iterator  iterator_;
  bool                                            before_beginning_;
};


// XmppChatroomModuleImpl ------------------------------------------------
XmppChatroomModule *
XmppChatroomModule::Create() {
  return new XmppChatroomModuleImpl();
}

XmppChatroomModuleImpl::XmppChatroomModuleImpl() :
  chatroom_handler_(NULL),
  chatroom_jid_(STR_EMPTY),
  chatroom_state_(XMPP_CHATROOM_STATE_NOT_IN_ROOM),
  chatroom_jid_members_version_(0) {
}

XmppChatroomModuleImpl::~XmppChatroomModuleImpl() {
  JidMemberMap::iterator iterator = chatroom_jid_members_.begin();
  while (iterator != chatroom_jid_members_.end()) {
    delete iterator->second;
    iterator++;
  }
}


bool
XmppChatroomModuleImpl::HandleStanza(const XmlElement* stanza) {
  ASSERT(engine() != NULL);

  // we handle stanzas that are for one of our chatrooms
  Jid from_jid = Jid(stanza->Attr(QN_FROM));
  // see if it's one of our chatrooms
  if (chatroom_jid_ != from_jid.BareJid()) {
    return false; // not one of our chatrooms
  } else {
    // handle presence stanza
    if (stanza->Name() == QN_PRESENCE) {
      if (from_jid == member_jid()) {
        ServerChangeMyPresence(*stanza);
      } else {
        ServerChangedOtherPresence(*stanza);
      }
    } else if (stanza->Name() == QN_MESSAGE) {
      FireMessageReceived(*stanza);
    }
    return true;
  }
}


XmppReturnStatus
XmppChatroomModuleImpl::set_chatroom_handler(XmppChatroomHandler* handler) {
  // Calling with NULL removes the handler.
  chatroom_handler_ = handler;
  return XMPP_RETURN_OK;
}


XmppChatroomHandler*
XmppChatroomModuleImpl::chatroom_handler() {
  return chatroom_handler_;
}

XmppReturnStatus
XmppChatroomModuleImpl::set_chatroom_jid(const Jid& chatroom_jid) {
  if (chatroom_state_ != XMPP_CHATROOM_STATE_NOT_IN_ROOM) {
    return XMPP_RETURN_BADSTATE; // $TODO - this isn't a bad state, it's a bad call,  diff error code?
  }
  if (chatroom_jid != chatroom_jid.BareJid()) {
    // chatroom_jid must be a bare jid
    return XMPP_RETURN_BADARGUMENT;
  }

  chatroom_jid_ = chatroom_jid;
  return XMPP_RETURN_OK;
}

const Jid&
XmppChatroomModuleImpl::chatroom_jid() const {
  return chatroom_jid_;
}

 XmppReturnStatus
 XmppChatroomModuleImpl::set_nickname(const std::string& nickname) {
  if (chatroom_state_ != XMPP_CHATROOM_STATE_NOT_IN_ROOM) {
    return XMPP_RETURN_BADSTATE; // $TODO - this isn't a bad state, it's a bad call,  diff error code?
  }
  nickname_ = nickname;
  return XMPP_RETURN_OK;
 }

 const std::string&
 XmppChatroomModuleImpl::nickname() const {
  return nickname_;
 }

const Jid
XmppChatroomModuleImpl::member_jid() const {
  return Jid(chatroom_jid_.node(), chatroom_jid_.domain(), nickname_);
}


bool
XmppChatroomModuleImpl::CheckEnterChatroomStateOk() {
  if (chatroom_jid_.IsValid() == false) {
    ASSERT(0);
    return false;
  }
  if (nickname_ == STR_EMPTY) {
    ASSERT(0);
    return false;
  }
  return true;
}

std::string GetAttrValueFor(XmppPresenceConnectionStatus connection_status) {
  switch (connection_status) {
    default:
    case XMPP_CONNECTION_STATUS_UNKNOWN:
      return "";
    case XMPP_CONNECTION_STATUS_CONNECTING:
      return STR_PSTN_CONFERENCE_STATUS_CONNECTING;
    case XMPP_CONNECTION_STATUS_CONNECTED:
      return STR_PSTN_CONFERENCE_STATUS_CONNECTED;
  }
}

XmppReturnStatus
XmppChatroomModuleImpl::RequestEnterChatroom(
    const std::string& password,
    const std::string& client_version,
    const std::string& locale) {
  RTC_UNUSED(password);
  if (!engine())
    return XMPP_RETURN_BADSTATE;

  if (chatroom_state_ != XMPP_CHATROOM_STATE_NOT_IN_ROOM)
    return XMPP_RETURN_BADSTATE; // $TODO - this isn't a bad state, it's a bad call,  diff error code?

  if (CheckEnterChatroomStateOk() == false) {
    return XMPP_RETURN_BADSTATE;
  }

  // entering a chatroom is a presence request to the server
  XmlElement element(QN_PRESENCE);
  element.AddAttr(QN_TO, member_jid().Str());

  XmlElement* muc_x = new XmlElement(QN_MUC_X);
  element.AddElement(muc_x);

  if (!client_version.empty()) {
    XmlElement* client_version_element = new XmlElement(QN_CLIENT_VERSION,
                                                        false);
    client_version_element->SetBodyText(client_version);
    muc_x->AddElement(client_version_element);
  }

  if (!locale.empty()) {
    XmlElement* locale_element = new XmlElement(QN_LOCALE, false);

    locale_element->SetBodyText(locale);
    muc_x->AddElement(locale_element);
  }

  XmppReturnStatus status = engine()->SendStanza(&element);
  if (status == XMPP_RETURN_OK) {
    return ClientChangeMyPresence(XMPP_CHATROOM_STATE_REQUESTED_ENTER);
  }
  return status;
}

XmppReturnStatus
XmppChatroomModuleImpl::RequestExitChatroom() {
  if (!engine())
    return XMPP_RETURN_BADSTATE;

  // exiting a chatroom is a presence request to the server
  XmlElement element(QN_PRESENCE);
  element.AddAttr(QN_TO, member_jid().Str());
  element.AddAttr(QN_TYPE, "unavailable");
  XmppReturnStatus status = engine()->SendStanza(&element);
  if (status == XMPP_RETURN_OK &&
      chatroom_state_ == XMPP_CHATROOM_STATE_IN_ROOM) {
    return ClientChangeMyPresence(XMPP_CHATROOM_STATE_REQUESTED_EXIT);
  }
  return status;
}

XmppReturnStatus
XmppChatroomModuleImpl::RequestConnectionStatusChange(
    XmppPresenceConnectionStatus connection_status) {
  if (!engine())
    return XMPP_RETURN_BADSTATE;

  if (chatroom_state_ != XMPP_CHATROOM_STATE_IN_ROOM) {
    // $TODO - this isn't a bad state, it's a bad call,  diff error code?
    return XMPP_RETURN_BADSTATE;
  }

  if (CheckEnterChatroomStateOk() == false) {
    return XMPP_RETURN_BADSTATE;
  }

  // entering a chatroom is a presence request to the server
  XmlElement element(QN_PRESENCE);
  element.AddAttr(QN_TO, member_jid().Str());
  element.AddElement(new XmlElement(QN_MUC_X));
  if (connection_status != XMPP_CONNECTION_STATUS_UNKNOWN) {
    XmlElement* con_status_element =
        new XmlElement(QN_GOOGLE_PSTN_CONFERENCE_STATUS);
    con_status_element->AddAttr(QN_STATUS, GetAttrValueFor(connection_status));
    element.AddElement(con_status_element);
  }
  XmppReturnStatus status = engine()->SendStanza(&element);

  return status;
}

size_t
XmppChatroomModuleImpl::GetChatroomMemberCount() {
  return chatroom_jid_members_.size();
}

XmppReturnStatus
XmppChatroomModuleImpl::CreateMemberEnumerator(XmppChatroomMemberEnumerator** enumerator) {
  *enumerator = new XmppChatroomMemberEnumeratorImpl(&chatroom_jid_members_, &chatroom_jid_members_version_);
  return XMPP_RETURN_OK;
}

const std::string
XmppChatroomModuleImpl::subject() {
  return ""; //NYI
}

XmppReturnStatus
XmppChatroomModuleImpl::SendMessage(const XmlElement& message) {
  XmppReturnStatus xmpp_status = XMPP_RETURN_OK;

  // can only send a message if we're in the room
  if (chatroom_state_ != XMPP_CHATROOM_STATE_IN_ROOM) {
    return XMPP_RETURN_BADSTATE; // $TODO - this isn't a bad state, it's a bad call,  diff error code?
  }

  if (message.Name() != QN_MESSAGE) {
    IFR(XMPP_RETURN_BADARGUMENT);
  }

  const std::string& type = message.Attr(QN_TYPE);
  if (type != "groupchat") {
    IFR(XMPP_RETURN_BADARGUMENT);
  }

  if (message.HasAttr(QN_FROM)) {
    IFR(XMPP_RETURN_BADARGUMENT);
  }

  if (message.Attr(QN_TO) != chatroom_jid_.Str()) {
    IFR(XMPP_RETURN_BADARGUMENT);
  }

  IFR(engine()->SendStanza(&message));

  return xmpp_status;
}

enum TransitionType {
  TRANSITION_TYPE_NONE                 = 0,
  TRANSITION_TYPE_ENTER_SUCCESS        = 1,
  TRANSITION_TYPE_ENTER_FAILURE        = 2,
  TRANSITION_TYPE_EXIT_VOLUNTARILY     = 3,
  TRANSITION_TYPE_EXIT_INVOLUNTARILY   = 4,
};

struct StateTransitionDescription {
  XmppChatroomState old_state;
  XmppChatroomState new_state;
  bool              is_valid_server_transition;
  bool              is_valid_client_transition;
  TransitionType    transition_type;
};

StateTransitionDescription Transitions[] = {
  { XMPP_CHATROOM_STATE_NOT_IN_ROOM,     XMPP_CHATROOM_STATE_REQUESTED_ENTER, false, true,  TRANSITION_TYPE_NONE, },
  { XMPP_CHATROOM_STATE_NOT_IN_ROOM,     XMPP_CHATROOM_STATE_IN_ROOM,         false, false, TRANSITION_TYPE_ENTER_SUCCESS, },
  { XMPP_CHATROOM_STATE_NOT_IN_ROOM,     XMPP_CHATROOM_STATE_REQUESTED_EXIT,  false, false, TRANSITION_TYPE_NONE, },
  { XMPP_CHATROOM_STATE_REQUESTED_ENTER, XMPP_CHATROOM_STATE_NOT_IN_ROOM,     true,  false, TRANSITION_TYPE_ENTER_FAILURE, },
  { XMPP_CHATROOM_STATE_REQUESTED_ENTER, XMPP_CHATROOM_STATE_IN_ROOM,         true,  false, TRANSITION_TYPE_ENTER_SUCCESS, },
  { XMPP_CHATROOM_STATE_REQUESTED_ENTER, XMPP_CHATROOM_STATE_REQUESTED_EXIT,  false, false, TRANSITION_TYPE_NONE, },
  { XMPP_CHATROOM_STATE_IN_ROOM,         XMPP_CHATROOM_STATE_NOT_IN_ROOM,     true,  false, TRANSITION_TYPE_EXIT_INVOLUNTARILY,  },
  { XMPP_CHATROOM_STATE_IN_ROOM,         XMPP_CHATROOM_STATE_REQUESTED_ENTER, false, false, TRANSITION_TYPE_NONE, },
  { XMPP_CHATROOM_STATE_IN_ROOM,         XMPP_CHATROOM_STATE_REQUESTED_EXIT,  false, true,  TRANSITION_TYPE_NONE, },
  { XMPP_CHATROOM_STATE_REQUESTED_EXIT,  XMPP_CHATROOM_STATE_NOT_IN_ROOM,     true,  false, TRANSITION_TYPE_EXIT_VOLUNTARILY, },
  { XMPP_CHATROOM_STATE_REQUESTED_EXIT,  XMPP_CHATROOM_STATE_REQUESTED_ENTER, false, false, TRANSITION_TYPE_NONE, },
  { XMPP_CHATROOM_STATE_REQUESTED_EXIT,  XMPP_CHATROOM_STATE_IN_ROOM,         false, false, TRANSITION_TYPE_NONE, },
};



void
XmppChatroomModuleImpl::FireEnteredStatus(const XmlElement* presence,
                                          XmppChatroomEnteredStatus status) {
  if (chatroom_handler_) {
    std::unique_ptr<XmppPresence> xmpp_presence(XmppPresence::Create());
    xmpp_presence->set_raw_xml(presence);
    chatroom_handler_->ChatroomEnteredStatus(this, xmpp_presence.get(), status);
  }
}

void
XmppChatroomModuleImpl::FireExitStatus(XmppChatroomExitedStatus status) {
  if (chatroom_handler_)
    chatroom_handler_->ChatroomExitedStatus(this, status);
}

void
XmppChatroomModuleImpl::FireMessageReceived(const XmlElement& message) {
  if (chatroom_handler_)
    chatroom_handler_->MessageReceived(this, message);
}

void
XmppChatroomModuleImpl::FireMemberEntered(const XmppChatroomMember* entered_member) {
  if (chatroom_handler_)
    chatroom_handler_->MemberEntered(this, entered_member);
}

void
XmppChatroomModuleImpl::FireMemberChanged(
    const XmppChatroomMember* changed_member) {
  if (chatroom_handler_)
    chatroom_handler_->MemberChanged(this, changed_member);
}

void
XmppChatroomModuleImpl::FireMemberExited(const XmppChatroomMember* exited_member) {
  if (chatroom_handler_)
    chatroom_handler_->MemberExited(this, exited_member);
}


XmppReturnStatus
XmppChatroomModuleImpl::ServerChangedOtherPresence(const XmlElement&
                                                   presence_element) {
  XmppReturnStatus xmpp_status = XMPP_RETURN_OK;
  std::unique_ptr<XmppPresence> presence(XmppPresence::Create());
  IFR(presence->set_raw_xml(&presence_element));

  JidMemberMap::iterator pos = chatroom_jid_members_.find(presence->jid());

  if (pos == chatroom_jid_members_.end()) {
    if (presence->available() == XMPP_PRESENCE_AVAILABLE) {
      XmppChatroomMemberImpl* member = new XmppChatroomMemberImpl();
      member->SetPresence(presence.get());
      chatroom_jid_members_.insert(std::make_pair(member->member_jid(), member));
      chatroom_jid_members_version_++;
      FireMemberEntered(member);
    }
  } else {
    XmppChatroomMemberImpl* member = pos->second;
    if (presence->available() == XMPP_PRESENCE_AVAILABLE) {
      member->SetPresence(presence.get());
      chatroom_jid_members_version_++;
      FireMemberChanged(member);
    }
    else if (presence->available() == XMPP_PRESENCE_UNAVAILABLE) {
      member->SetPresence(presence.get());
      chatroom_jid_members_.erase(pos);
      chatroom_jid_members_version_++;
      FireMemberExited(member);
      delete member;
    }
  }

  return xmpp_status;
}

XmppReturnStatus
XmppChatroomModuleImpl::ClientChangeMyPresence(XmppChatroomState new_state) {
  return ChangePresence(new_state, NULL, false);
}

XmppReturnStatus
XmppChatroomModuleImpl::ServerChangeMyPresence(const XmlElement& presence) {
   XmppChatroomState new_state;

   if (presence.HasAttr(QN_TYPE) == false) {
      new_state = XMPP_CHATROOM_STATE_IN_ROOM;
   } else {
     new_state = XMPP_CHATROOM_STATE_NOT_IN_ROOM;
   }
  return ChangePresence(new_state, &presence, true);

}

XmppReturnStatus
XmppChatroomModuleImpl::ChangePresence(XmppChatroomState new_state,
                                       const XmlElement* presence,
                                       bool isServer) {
  RTC_UNUSED(presence);

  XmppChatroomState old_state = chatroom_state_;

  // do nothing if state hasn't changed
  if (old_state == new_state)
    return XMPP_RETURN_OK;

  // find the right transition description
  StateTransitionDescription* transition_desc = NULL;
  for (size_t i = 0; i < arraysize(Transitions); i++) {
    if (Transitions[i].old_state == old_state &&
        Transitions[i].new_state == new_state) {
        transition_desc = &Transitions[i];
        break;
    }
  }

  if (transition_desc == NULL) {
    ASSERT(0);
    return XMPP_RETURN_BADSTATE;
  }

  // we assert for any invalid transition states, and we'll
  if (isServer) {
    // $TODO send original stanza back to server and log an error?
    // Disable the assert because of b/6133072
    // ASSERT(transition_desc->is_valid_server_transition);
    if (!transition_desc->is_valid_server_transition) {
      return XMPP_RETURN_BADSTATE;
    }
  } else {
    if (transition_desc->is_valid_client_transition == false) {
      ASSERT(0);
      return XMPP_RETURN_BADARGUMENT;
    }
  }

  // set the new state and then fire any notifications to the handler
  chatroom_state_ = new_state;

  switch (transition_desc->transition_type) {
    case TRANSITION_TYPE_ENTER_SUCCESS:
      FireEnteredStatus(presence, XMPP_CHATROOM_ENTERED_SUCCESS);
      break;
    case TRANSITION_TYPE_ENTER_FAILURE:
      FireEnteredStatus(presence, GetEnterFailureFromXml(presence));
      break;
    case TRANSITION_TYPE_EXIT_INVOLUNTARILY:
      FireExitStatus(GetExitFailureFromXml(presence));
      break;
    case TRANSITION_TYPE_EXIT_VOLUNTARILY:
      FireExitStatus(XMPP_CHATROOM_EXITED_REQUESTED);
      break;
    case TRANSITION_TYPE_NONE:
      break;
  }

  return XMPP_RETURN_OK;
}

XmppChatroomEnteredStatus
XmppChatroomModuleImpl::GetEnterFailureFromXml(const XmlElement* presence) {
  XmppChatroomEnteredStatus status = XMPP_CHATROOM_ENTERED_FAILURE_UNSPECIFIED;
  const XmlElement* error = presence->FirstNamed(QN_ERROR);
  if (error != NULL && error->HasAttr(QN_CODE)) {
    int code = atoi(error->Attr(QN_CODE).c_str());
    switch (code) {
      case 401: status = XMPP_CHATROOM_ENTERED_FAILURE_PASSWORD_REQUIRED; break;
      case 403: {
        status = XMPP_CHATROOM_ENTERED_FAILURE_MEMBER_BANNED;
        if (error->FirstNamed(QN_GOOGLE_SESSION_BLOCKED)) {
          status = XMPP_CHATROOM_ENTERED_FAILURE_MEMBER_BLOCKED;
        } else if (error->FirstNamed(QN_GOOGLE_SESSION_BLOCKING)) {
          status = XMPP_CHATROOM_ENTERED_FAILURE_MEMBER_BLOCKING;
        }
        break;
      }
      case 405: status = XMPP_CHATROOM_ENTERED_FAILURE_ROOM_LOCKED; break;
      case 406: status = XMPP_CHATROOM_ENTERED_FAILURE_OUTDATED_CLIENT; break;
      case 407: status = XMPP_CHATROOM_ENTERED_FAILURE_NOT_A_MEMBER; break;
      case 409: status = XMPP_CHATROOM_ENTERED_FAILURE_NICKNAME_CONFLICT; break;
      // http://xmpp.org/extensions/xep-0045.html#enter-maxusers
      case 503: status = XMPP_CHATROOM_ENTERED_FAILURE_MAX_USERS; break;
    }
  }
  return status;
}

XmppChatroomExitedStatus
XmppChatroomModuleImpl::GetExitFailureFromXml(const XmlElement* presence) {
  XmppChatroomExitedStatus status = XMPP_CHATROOM_EXITED_UNSPECIFIED;
  const XmlElement* muc_user = presence->FirstNamed(QN_MUC_USER_X);
  if (muc_user != NULL) {
    const XmlElement* user_status = muc_user->FirstNamed(QN_MUC_USER_STATUS);
    if (user_status != NULL && user_status->HasAttr(QN_CODE)) {
      int code = atoi(user_status->Attr(QN_CODE).c_str());
      switch (code) {
        case 307: status = XMPP_CHATROOM_EXITED_KICKED; break;
        case 322: status = XMPP_CHATROOM_EXITED_NOT_A_MEMBER; break;
        case 332: status = XMPP_CHATROOM_EXITED_SYSTEM_SHUTDOWN; break;
      }
    }
  }
  return status;
}

XmppReturnStatus
XmppChatroomMemberImpl::SetPresence(const XmppPresence* presence) {
  ASSERT(presence != NULL);

  // copy presence
  presence_.reset(XmppPresence::Create());
  presence_->set_raw_xml(presence->raw_xml());
  return XMPP_RETURN_OK;
}

const Jid
XmppChatroomMemberImpl::member_jid() const {
  return presence_->jid();
}

const Jid
XmppChatroomMemberImpl::full_jid() const {
  return Jid("");
}

const std::string
XmppChatroomMemberImpl::name() const {
  return member_jid().resource();
}

const XmppPresence*
XmppChatroomMemberImpl::presence() const {
  return presence_.get();
}


// XmppChatroomMemberEnumeratorImpl --------------------------------------
XmppChatroomMemberEnumeratorImpl::XmppChatroomMemberEnumeratorImpl(
        XmppChatroomModuleImpl::JidMemberMap* map, int* map_version) {
  map_ = map;
  map_version_ = map_version;
  map_version_created_ = *map_version_;
  iterator_ = map->begin();
  before_beginning_ = true;
}

XmppChatroomMember*
XmppChatroomMemberEnumeratorImpl::current() {
  if (IsValid() == false) {
    return NULL;
  } else if (IsBeforeBeginning() || IsAfterEnd()) {
    return NULL;
  } else {
    return iterator_->second;
  }
}

bool
XmppChatroomMemberEnumeratorImpl::Prev() {
  if (IsValid() == false) {
    return false;
  } else if (IsBeforeBeginning()) {
    return false;
  } else if (iterator_ == map_->begin()) {
    before_beginning_ = true;
    return false;
  } else {
    iterator_--;
    return current() != NULL;
  }
}

bool
XmppChatroomMemberEnumeratorImpl::Next() {
  if (IsValid() == false) {
    return false;
  } else if (IsBeforeBeginning()) {
    before_beginning_ = false;
    iterator_ = map_->begin();
    return current() != NULL;
  } else if (IsAfterEnd()) {
    return false;
  } else {
    iterator_++;
    return current() != NULL;
  }
}

bool
XmppChatroomMemberEnumeratorImpl::IsValid() {
  return map_version_created_ == *map_version_;
}

bool
XmppChatroomMemberEnumeratorImpl::IsBeforeBeginning() {
  return before_beginning_;
}

bool
XmppChatroomMemberEnumeratorImpl::IsAfterEnd() {
  return (iterator_ == map_->end());
}



} // namespace buzz
