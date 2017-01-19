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
#include <sstream>
#include <string>
#include <vector>
#include "webrtc/libjingle/xmpp/constants.h"
#include "webrtc/libjingle/xmpp/rostermoduleimpl.h"
#include "webrtc/base/common.h"
#include "webrtc/base/stringencode.h"

namespace buzz {

// enum prase and persist helpers ----------------------------------------------
static bool
StringToPresenceShow(const std::string& input, XmppPresenceShow* show) {
  // If this becomes a perf issue we can use a hash or a map here
  if (STR_SHOW_AWAY == input)
    *show = XMPP_PRESENCE_AWAY;
  else if (STR_SHOW_DND == input)
    *show = XMPP_PRESENCE_DND;
  else if (STR_SHOW_XA == input)
    *show = XMPP_PRESENCE_XA;
  else if (STR_SHOW_CHAT == input)
    *show = XMPP_PRESENCE_CHAT;
  else if (STR_EMPTY == input)
    *show = XMPP_PRESENCE_DEFAULT;
  else
    return false;

  return true;
}

static bool
PresenceShowToString(XmppPresenceShow show, const char** output) {
  switch(show) {
    case XMPP_PRESENCE_AWAY:
      *output = STR_SHOW_AWAY;
      return true;
    case XMPP_PRESENCE_CHAT:
      *output = STR_SHOW_CHAT;
      return true;
    case XMPP_PRESENCE_XA:
      *output = STR_SHOW_XA;
      return true;
    case XMPP_PRESENCE_DND:
      *output = STR_SHOW_DND;
      return true;
    case XMPP_PRESENCE_DEFAULT:
      *output = STR_EMPTY;
      return true;
  }

  *output = STR_EMPTY;
  return false;
}

static bool
StringToSubscriptionState(const std::string& subscription,
                          const std::string& ask,
                          XmppSubscriptionState* state)
{
  if (ask == "subscribe")
  {
    if (subscription == "none") {
      *state = XMPP_SUBSCRIPTION_NONE_ASKED;
      return true;
    }
    if (subscription == "from") {
      *state = XMPP_SUBSCRIPTION_FROM_ASKED;
      return true;
    }
  } else if (ask == STR_EMPTY)
  {
    if (subscription == "none") {
      *state = XMPP_SUBSCRIPTION_NONE;
      return true;
    }
    if (subscription == "from") {
      *state = XMPP_SUBSCRIPTION_FROM;
      return true;
    }
    if (subscription == "to") {
      *state = XMPP_SUBSCRIPTION_TO;
      return true;
    }
    if (subscription == "both") {
      *state = XMPP_SUBSCRIPTION_BOTH;
      return true;
    }
  }

  return false;
}

static bool
StringToSubscriptionRequestType(const std::string& string,
                                XmppSubscriptionRequestType* type)
{
  if (string == "subscribe")
    *type = XMPP_REQUEST_SUBSCRIBE;
  else if (string == "unsubscribe")
    *type = XMPP_REQUEST_UNSUBSCRIBE;
  else if (string == "subscribed")
    *type = XMPP_REQUEST_SUBSCRIBED;
  else if (string == "unsubscribed")
    *type = XMPP_REQUEST_UNSUBSCRIBED;
  else
    return false;
  return true;
}

// XmppPresenceImpl class ------------------------------------------------------
XmppPresence*
XmppPresence::Create() {
  return new XmppPresenceImpl();
}

XmppPresenceImpl::XmppPresenceImpl() {
}

const Jid
XmppPresenceImpl::jid() const {
  if (!raw_xml_)
    return Jid();

  return Jid(raw_xml_->Attr(QN_FROM));
}

XmppPresenceAvailable
XmppPresenceImpl::available() const {
  if (!raw_xml_)
    return XMPP_PRESENCE_UNAVAILABLE;

  if (raw_xml_->Attr(QN_TYPE) == "unavailable")
    return XMPP_PRESENCE_UNAVAILABLE;
  else if (raw_xml_->Attr(QN_TYPE) == "error")
    return XMPP_PRESENCE_ERROR;
  else
    return XMPP_PRESENCE_AVAILABLE;
}

XmppReturnStatus
XmppPresenceImpl::set_available(XmppPresenceAvailable available) {
  if (!raw_xml_)
    CreateRawXmlSkeleton();

  if (available == XMPP_PRESENCE_AVAILABLE)
    raw_xml_->ClearAttr(QN_TYPE);
  else if (available == XMPP_PRESENCE_UNAVAILABLE)
    raw_xml_->SetAttr(QN_TYPE, "unavailable");
  else if (available == XMPP_PRESENCE_ERROR)
    raw_xml_->SetAttr(QN_TYPE, "error");
  return XMPP_RETURN_OK;
}

XmppPresenceShow
XmppPresenceImpl::presence_show() const {
  if (!raw_xml_)
    return XMPP_PRESENCE_DEFAULT;

  XmppPresenceShow show = XMPP_PRESENCE_DEFAULT;
  StringToPresenceShow(raw_xml_->TextNamed(QN_SHOW), &show);
  return show;
}

XmppReturnStatus
XmppPresenceImpl::set_presence_show(XmppPresenceShow show) {
  if (!raw_xml_)
    CreateRawXmlSkeleton();

  const char* show_string;

  if(!PresenceShowToString(show, &show_string))
    return XMPP_RETURN_BADARGUMENT;

  raw_xml_->ClearNamedChildren(QN_SHOW);

  if (show!=XMPP_PRESENCE_DEFAULT) {
    raw_xml_->AddElement(new XmlElement(QN_SHOW));
    raw_xml_->AddText(show_string, 1);
  }

  return XMPP_RETURN_OK;
}

int
XmppPresenceImpl::priority() const {
  if (!raw_xml_)
    return 0;

  int raw_priority = 0;
  if (!rtc::FromString(raw_xml_->TextNamed(QN_PRIORITY), &raw_priority))
    raw_priority = 0;
  if (raw_priority < -128)
    raw_priority = -128;
  if (raw_priority > 127)
    raw_priority = 127;

  return raw_priority;
}

XmppReturnStatus
XmppPresenceImpl::set_priority(int priority) {
  if (!raw_xml_)
    CreateRawXmlSkeleton();

  if (priority < -128 || priority > 127)
    return XMPP_RETURN_BADARGUMENT;

  raw_xml_->ClearNamedChildren(QN_PRIORITY);
  if (0 != priority) {
    std::string priority_string;
    if (rtc::ToString(priority, &priority_string)) {
      raw_xml_->AddElement(new XmlElement(QN_PRIORITY));
      raw_xml_->AddText(priority_string, 1);
    }
  }

  return XMPP_RETURN_OK;
}

const std::string
XmppPresenceImpl::status() const {
  if (!raw_xml_)
    return STR_EMPTY;

  XmlElement* status_element;
  XmlElement* element;

  // Search for a status element with no xml:lang attribute on it.  if we can't
  // find that then just return the first status element in the stanza.
  for (status_element = element = raw_xml_->FirstNamed(QN_STATUS);
       element;
       element = element->NextNamed(QN_STATUS)) {
    if (!element->HasAttr(QN_XML_LANG)) {
      status_element = element;
      break;
    }
  }

  if (status_element) {
    return status_element->BodyText();
  }

  return STR_EMPTY;
}

XmppReturnStatus
XmppPresenceImpl::set_status(const std::string& status) {
  if (!raw_xml_)
    CreateRawXmlSkeleton();

  raw_xml_->ClearNamedChildren(QN_STATUS);

  if (status != STR_EMPTY) {
    raw_xml_->AddElement(new XmlElement(QN_STATUS));
    raw_xml_->AddText(status, 1);
  }

  return XMPP_RETURN_OK;
}

XmppPresenceConnectionStatus
XmppPresenceImpl::connection_status() const {
  if (!raw_xml_)
      return XMPP_CONNECTION_STATUS_UNKNOWN;

  XmlElement* con = raw_xml_->FirstNamed(QN_GOOGLE_PSTN_CONFERENCE_STATUS);
  if (con) {
    std::string status = con->Attr(QN_ATTR_STATUS);
    if (status == STR_PSTN_CONFERENCE_STATUS_CONNECTING)
      return XMPP_CONNECTION_STATUS_CONNECTING;
    else if (status == STR_PSTN_CONFERENCE_STATUS_CONNECTED)
      return XMPP_CONNECTION_STATUS_CONNECTED;
    else if (status == STR_PSTN_CONFERENCE_STATUS_JOINING)
            return XMPP_CONNECTION_STATUS_JOINING;
    else if (status == STR_PSTN_CONFERENCE_STATUS_HANGUP)
        return XMPP_CONNECTION_STATUS_HANGUP;
  }

  return XMPP_CONNECTION_STATUS_CONNECTED;
}

const std::string
XmppPresenceImpl::google_user_id() const {
  if (!raw_xml_)
    return std::string();

  XmlElement* muc_user_x = raw_xml_->FirstNamed(QN_MUC_USER_X);
  if (muc_user_x) {
    XmlElement* muc_user_item = muc_user_x->FirstNamed(QN_MUC_USER_ITEM);
    if (muc_user_item) {
      return muc_user_item->Attr(QN_GOOGLE_USER_ID);
    }
  }

  return std::string();
}

const std::string
XmppPresenceImpl::nickname() const {
  if (!raw_xml_)
    return std::string();

  XmlElement* nickname = raw_xml_->FirstNamed(QN_NICKNAME);
  if (nickname) {
    return nickname->BodyText();
  }

  return std::string();
}

const XmlElement*
XmppPresenceImpl::raw_xml() const {
  if (!raw_xml_)
    const_cast<XmppPresenceImpl*>(this)->CreateRawXmlSkeleton();
  return raw_xml_.get();
}

XmppReturnStatus
XmppPresenceImpl::set_raw_xml(const XmlElement * xml) {
  if (!xml ||
      xml->Name() != QN_PRESENCE)
    return XMPP_RETURN_BADARGUMENT;

  raw_xml_.reset(new XmlElement(*xml));
  return XMPP_RETURN_OK;
}

void
XmppPresenceImpl::CreateRawXmlSkeleton() {
  raw_xml_.reset(new XmlElement(QN_PRESENCE));
}

// XmppRosterContactImpl -------------------------------------------------------
XmppRosterContact*
XmppRosterContact::Create() {
  return new XmppRosterContactImpl();
}

XmppRosterContactImpl::XmppRosterContactImpl() {
  ResetGroupCache();
}

void
XmppRosterContactImpl::SetXmlFromWire(const XmlElement* xml) {
  ResetGroupCache();
  if (xml)
    raw_xml_.reset(new XmlElement(*xml));
  else
    raw_xml_.reset(NULL);
}

void
XmppRosterContactImpl::ResetGroupCache() {
  group_count_ = -1;
  group_index_returned_ = -1;
  group_returned_ = NULL;
}

const Jid
XmppRosterContactImpl::jid() const {
  return Jid(raw_xml_->Attr(QN_JID));
}

XmppReturnStatus
XmppRosterContactImpl::set_jid(const Jid& jid)
{
  if (!raw_xml_)
    CreateRawXmlSkeleton();

  if (!jid.IsValid())
    return XMPP_RETURN_BADARGUMENT;

  raw_xml_->SetAttr(QN_JID, jid.Str());

  return XMPP_RETURN_OK;
}

const std::string
XmppRosterContactImpl::name() const {
  return raw_xml_->Attr(QN_NAME);
}

XmppReturnStatus
XmppRosterContactImpl::set_name(const std::string& name) {
  if (!raw_xml_)
    CreateRawXmlSkeleton();

  if (name == STR_EMPTY)
    raw_xml_->ClearAttr(QN_NAME);
  else
    raw_xml_->SetAttr(QN_NAME, name);

  return XMPP_RETURN_OK;
}

XmppSubscriptionState
XmppRosterContactImpl::subscription_state() const {
  if (!raw_xml_)
    return XMPP_SUBSCRIPTION_NONE;

  XmppSubscriptionState state = XMPP_SUBSCRIPTION_NONE;

  if (StringToSubscriptionState(raw_xml_->Attr(QN_SUBSCRIPTION),
                                raw_xml_->Attr(QN_ASK),
                                &state))
    return state;

  return XMPP_SUBSCRIPTION_NONE;
}

size_t
XmppRosterContactImpl::GetGroupCount() const {
  if (!raw_xml_)
    return 0;

  if (-1 == group_count_) {
    XmlElement *group_element = raw_xml_->FirstNamed(QN_ROSTER_GROUP);
    int group_count = 0;
    while(group_element) {
      group_count++;
      group_element = group_element->NextNamed(QN_ROSTER_GROUP);
    }

    ASSERT(group_count > 0); // protect the cast
    XmppRosterContactImpl * me = const_cast<XmppRosterContactImpl*>(this);
    me->group_count_ = group_count;
  }

  return group_count_;
}

const std::string
XmppRosterContactImpl::GetGroup(size_t index) const {
  if (index >= GetGroupCount())
    return STR_EMPTY;

  // We cache the last group index and element that we returned.  This way
  // going through the groups in order is order n and not n^2.  This could be
  // enhanced if necessary by starting at the cached value if the index asked
  // is after the cached one.
  if (group_index_returned_ >= 0 &&
      index == static_cast<size_t>(group_index_returned_) + 1)
  {
    XmppRosterContactImpl * me = const_cast<XmppRosterContactImpl*>(this);
    me->group_returned_ = group_returned_->NextNamed(QN_ROSTER_GROUP);
    ASSERT(group_returned_ != NULL);
    me->group_index_returned_++;
  } else if (group_index_returned_ < 0 ||
             static_cast<size_t>(group_index_returned_) != index) {
    XmlElement * group_element = raw_xml_->FirstNamed(QN_ROSTER_GROUP);
    size_t group_index = 0;
    while(group_index < index) {
      ASSERT(group_element != NULL);
      group_index++;
      group_element = group_element->NextNamed(QN_ROSTER_GROUP);
    }

    XmppRosterContactImpl * me = const_cast<XmppRosterContactImpl*>(this);
    me->group_index_returned_ = static_cast<int>(group_index);
    me->group_returned_ = group_element;
  }

  return group_returned_->BodyText();
}

XmppReturnStatus
XmppRosterContactImpl::AddGroup(const std::string& group) {
  if (group == STR_EMPTY)
    return XMPP_RETURN_BADARGUMENT;

  if (!raw_xml_)
    CreateRawXmlSkeleton();

  if (FindGroup(group, NULL, NULL))
    return XMPP_RETURN_OK;

  raw_xml_->AddElement(new XmlElement(QN_ROSTER_GROUP));
  raw_xml_->AddText(group, 1);
  ++group_count_;

  return XMPP_RETURN_OK;
}

XmppReturnStatus
XmppRosterContactImpl::RemoveGroup(const std::string& group) {
  if (group == STR_EMPTY)
    return XMPP_RETURN_BADARGUMENT;

  if (!raw_xml_)
    return XMPP_RETURN_OK;

  XmlChild * child_before;
  if (FindGroup(group, NULL, &child_before)) {
    raw_xml_->RemoveChildAfter(child_before);
    ResetGroupCache();
  }
  return XMPP_RETURN_OK;
}

bool
XmppRosterContactImpl::FindGroup(const std::string& group,
                                 XmlElement** element,
                                 XmlChild** child_before) {
  XmlChild * prev_child = NULL;
  XmlChild * next_child;
  XmlChild * child;
  for (child = raw_xml_->FirstChild(); child; child = next_child) {
    next_child = child->NextChild();
    if (!child->IsText() &&
        child->AsElement()->Name() == QN_ROSTER_GROUP &&
        child->AsElement()->BodyText() == group) {
      if (element)
        *element = child->AsElement();
      if (child_before)
        *child_before = prev_child;
      return true;
    }
    prev_child = child;
  }

  return false;
}

const XmlElement*
XmppRosterContactImpl::raw_xml() const {
  if (!raw_xml_)
    const_cast<XmppRosterContactImpl*>(this)->CreateRawXmlSkeleton();
  return raw_xml_.get();
}

XmppReturnStatus
XmppRosterContactImpl::set_raw_xml(const XmlElement* xml) {
  if (!xml ||
      xml->Name() != QN_ROSTER_ITEM ||
      xml->HasAttr(QN_SUBSCRIPTION) ||
      xml->HasAttr(QN_ASK))
    return XMPP_RETURN_BADARGUMENT;

  ResetGroupCache();

  raw_xml_.reset(new XmlElement(*xml));

  return XMPP_RETURN_OK;
}

void
XmppRosterContactImpl::CreateRawXmlSkeleton() {
  raw_xml_.reset(new XmlElement(QN_ROSTER_ITEM));
}

// XmppRosterModuleImpl --------------------------------------------------------
XmppRosterModule *
XmppRosterModule::Create() {
  return new XmppRosterModuleImpl();
}

XmppRosterModuleImpl::XmppRosterModuleImpl() :
  roster_handler_(NULL),
  incoming_presence_map_(new JidPresenceVectorMap()),
  incoming_presence_vector_(new PresenceVector()),
  contacts_(new ContactVector()) {

}

XmppRosterModuleImpl::~XmppRosterModuleImpl() {
  DeleteIncomingPresence();
  DeleteContacts();
}

XmppReturnStatus
XmppRosterModuleImpl::set_roster_handler(XmppRosterHandler * handler) {
  roster_handler_ = handler;
  return XMPP_RETURN_OK;
}

XmppRosterHandler*
XmppRosterModuleImpl::roster_handler() {
  return roster_handler_;
}

XmppPresence*
XmppRosterModuleImpl::outgoing_presence() {
  return &outgoing_presence_;
}

XmppReturnStatus
XmppRosterModuleImpl::BroadcastPresence() {
  // Scrub the outgoing presence
  const XmlElement* element = outgoing_presence_.raw_xml();

  ASSERT(!element->HasAttr(QN_TO) &&
         !element->HasAttr(QN_FROM) &&
          (element->Attr(QN_TYPE) == STR_EMPTY ||
           element->Attr(QN_TYPE) == "unavailable"));

  if (!engine())
    return XMPP_RETURN_BADSTATE;

  return engine()->SendStanza(element);
}

XmppReturnStatus
XmppRosterModuleImpl::SendDirectedPresence(const XmppPresence* presence,
                                           const Jid& to_jid) {
  if (!presence)
    return XMPP_RETURN_BADARGUMENT;

  if (!engine())
    return XMPP_RETURN_BADSTATE;

  XmlElement element(*(presence->raw_xml()));

  if (element.Name() != QN_PRESENCE ||
      element.HasAttr(QN_TO) ||
      element.HasAttr(QN_FROM))
    return XMPP_RETURN_BADARGUMENT;

  if (element.HasAttr(QN_TYPE)) {
    if (element.Attr(QN_TYPE) != STR_EMPTY &&
        element.Attr(QN_TYPE) != "unavailable") {
      return XMPP_RETURN_BADARGUMENT;
    }
  }

  element.SetAttr(QN_TO, to_jid.Str());

  return engine()->SendStanza(&element);
}

size_t
XmppRosterModuleImpl::GetIncomingPresenceCount() {
  return incoming_presence_vector_->size();
}

const XmppPresence*
XmppRosterModuleImpl::GetIncomingPresence(size_t index) {
  if (index >= incoming_presence_vector_->size())
    return NULL;
  return (*incoming_presence_vector_)[index];
}

size_t
XmppRosterModuleImpl::GetIncomingPresenceForJidCount(const Jid& jid)
{
  // find the vector in the map
  JidPresenceVectorMap::iterator pos;
  pos = incoming_presence_map_->find(jid);
  if (pos == incoming_presence_map_->end())
    return 0;

  ASSERT(pos->second != NULL);

  return pos->second->size();
}

const XmppPresence*
XmppRosterModuleImpl::GetIncomingPresenceForJid(const Jid& jid,
                                                size_t index) {
  JidPresenceVectorMap::iterator pos;
  pos = incoming_presence_map_->find(jid);
  if (pos == incoming_presence_map_->end())
    return NULL;

  ASSERT(pos->second != NULL);

  if (index >= pos->second->size())
    return NULL;

  return (*pos->second)[index];
}

XmppReturnStatus
XmppRosterModuleImpl::RequestRosterUpdate() {
  if (!engine())
    return XMPP_RETURN_BADSTATE;

  XmlElement roster_get(QN_IQ);
  roster_get.AddAttr(QN_TYPE, "get");
  roster_get.AddAttr(QN_ID, engine()->NextId());
  roster_get.AddElement(new XmlElement(QN_ROSTER_QUERY, true));
  return engine()->SendIq(&roster_get, this, NULL);
}

size_t
XmppRosterModuleImpl::GetRosterContactCount() {
  return contacts_->size();
}

const XmppRosterContact*
XmppRosterModuleImpl::GetRosterContact(size_t index) {
  if (index >= contacts_->size())
    return NULL;
  return (*contacts_)[index];
}

class RosterPredicate {
public:
  explicit RosterPredicate(const Jid& jid) : jid_(jid) {
  }

  bool operator() (XmppRosterContactImpl *& contact) {
    return contact->jid() == jid_;
  }

private:
  Jid jid_;
};

const XmppRosterContact*
XmppRosterModuleImpl::FindRosterContact(const Jid& jid) {
  ContactVector::iterator pos;

  pos = std::find_if(contacts_->begin(),
                     contacts_->end(),
                     RosterPredicate(jid));
  if (pos == contacts_->end())
    return NULL;

  return *pos;
}

XmppReturnStatus
XmppRosterModuleImpl::RequestRosterChange(
  const XmppRosterContact* contact) {
  if (!contact)
    return XMPP_RETURN_BADARGUMENT;

  Jid jid = contact->jid();

  if (!jid.IsValid())
    return XMPP_RETURN_BADARGUMENT;

  if (!engine())
    return XMPP_RETURN_BADSTATE;

  const XmlElement* contact_xml = contact->raw_xml();
  if (contact_xml->Name() != QN_ROSTER_ITEM ||
      contact_xml->HasAttr(QN_SUBSCRIPTION) ||
      contact_xml->HasAttr(QN_ASK))
    return XMPP_RETURN_BADARGUMENT;

  XmlElement roster_add(QN_IQ);
  roster_add.AddAttr(QN_TYPE, "set");
  roster_add.AddAttr(QN_ID, engine()->NextId());
  roster_add.AddElement(new XmlElement(QN_ROSTER_QUERY, true));
  roster_add.AddElement(new XmlElement(*contact_xml), 1);

  return engine()->SendIq(&roster_add, this, NULL);
}

XmppReturnStatus
XmppRosterModuleImpl::RequestRosterRemove(const Jid& jid) {
  if (!jid.IsValid())
    return XMPP_RETURN_BADARGUMENT;

  if (!engine())
    return XMPP_RETURN_BADSTATE;

  XmlElement roster_add(QN_IQ);
  roster_add.AddAttr(QN_TYPE, "set");
  roster_add.AddAttr(QN_ID, engine()->NextId());
  roster_add.AddElement(new XmlElement(QN_ROSTER_QUERY, true));
  roster_add.AddAttr(QN_JID, jid.Str(), 1);
  roster_add.AddAttr(QN_SUBSCRIPTION, "remove", 1);

  return engine()->SendIq(&roster_add, this, NULL);
}

XmppReturnStatus
XmppRosterModuleImpl::RequestSubscription(const Jid& jid) {
  return SendSubscriptionRequest(jid, "subscribe");
}

XmppReturnStatus
XmppRosterModuleImpl::CancelSubscription(const Jid& jid) {
  return SendSubscriptionRequest(jid, "unsubscribe");
}

XmppReturnStatus
XmppRosterModuleImpl::ApproveSubscriber(const Jid& jid) {
  return SendSubscriptionRequest(jid, "subscribed");
}

XmppReturnStatus
XmppRosterModuleImpl::CancelSubscriber(const Jid& jid) {
  return SendSubscriptionRequest(jid, "unsubscribed");
}

void
XmppRosterModuleImpl::IqResponse(XmppIqCookie, const XmlElement * stanza) {
  // The only real Iq response that we expect to recieve are initial roster
  // population
  if (stanza->Attr(QN_TYPE) == "error")
  {
    if (roster_handler_)
      roster_handler_->RosterError(this, stanza);

    return;
  }

  ASSERT(stanza->Attr(QN_TYPE) == "result");

  InternalRosterItems(stanza);
}

bool
XmppRosterModuleImpl::HandleStanza(const XmlElement * stanza)
{
  ASSERT(engine() != NULL);

  // There are two types of stanzas that we care about: presence and roster push
  // Iqs
  if (stanza->Name() == QN_PRESENCE) {
    const std::string&  jid_string = stanza->Attr(QN_FROM);
    Jid jid(jid_string);

    if (!jid.IsValid())
      return false; // if the Jid isn't valid, don't process

    const std::string& type = stanza->Attr(QN_TYPE);
    XmppSubscriptionRequestType request_type;
    if (StringToSubscriptionRequestType(type, &request_type))
      InternalSubscriptionRequest(jid, stanza, request_type);
    else if (type == "unavailable" || type == STR_EMPTY)
      InternalIncomingPresence(jid, stanza);
    else if (type == "error")
      InternalIncomingPresenceError(jid, stanza);
    else
      return false;

    return true;
  } else if (stanza->Name() == QN_IQ) {
    const XmlElement * roster_query = stanza->FirstNamed(QN_ROSTER_QUERY);
    if (!roster_query || stanza->Attr(QN_TYPE) != "set")
      return false;

    InternalRosterItems(stanza);

    // respond to the IQ
    XmlElement result(QN_IQ);
    result.AddAttr(QN_TYPE, "result");
    result.AddAttr(QN_TO, stanza->Attr(QN_FROM));
    result.AddAttr(QN_ID, stanza->Attr(QN_ID));

    engine()->SendStanza(&result);
    return true;
  }

  return false;
}

void
XmppRosterModuleImpl::DeleteIncomingPresence() {
  // Clear out the vector of all presence notifications
  {
    PresenceVector::iterator pos;
    for (pos = incoming_presence_vector_->begin();
         pos < incoming_presence_vector_->end();
         ++pos) {
      XmppPresenceImpl * presence = *pos;
      *pos = NULL;
      delete presence;
    }
    incoming_presence_vector_->clear();
  }

  // Clear out all of the small presence vectors per Jid
  {
    JidPresenceVectorMap::iterator pos;
    for (pos = incoming_presence_map_->begin();
         pos != incoming_presence_map_->end();
         ++pos) {
      PresenceVector* presence_vector = pos->second;
      pos->second = NULL;
      delete presence_vector;
    }
    incoming_presence_map_->clear();
  }
}

void
XmppRosterModuleImpl::DeleteContacts() {
  ContactVector::iterator pos;
  for (pos = contacts_->begin();
       pos < contacts_->end();
       ++pos) {
    XmppRosterContact* contact = *pos;
    *pos = NULL;
    delete contact;
  }
  contacts_->clear();
}

XmppReturnStatus
XmppRosterModuleImpl::SendSubscriptionRequest(const Jid& jid,
                                              const std::string& type) {
  if (!jid.IsValid())
    return XMPP_RETURN_BADARGUMENT;

  if (!engine())
    return XMPP_RETURN_BADSTATE;

  XmlElement presence_request(QN_PRESENCE);
  presence_request.AddAttr(QN_TO, jid.Str());
  presence_request.AddAttr(QN_TYPE, type);

  return engine()->SendStanza(&presence_request);
}


void
XmppRosterModuleImpl::InternalSubscriptionRequest(const Jid& jid,
                                                  const XmlElement* stanza,
                                                  XmppSubscriptionRequestType
                                                    request_type) {
  if (roster_handler_)
    roster_handler_->SubscriptionRequest(this, jid, request_type, stanza);
}

class PresencePredicate {
public:
  explicit PresencePredicate(const Jid& jid) : jid_(jid) {
  }

  bool operator() (XmppPresenceImpl *& contact) {
    return contact->jid() == jid_;
  }

private:
  Jid jid_;
};

void
XmppRosterModuleImpl::InternalIncomingPresence(const Jid& jid,
                                               const XmlElement* stanza) {
  bool added = false;
  Jid bare_jid = jid.BareJid();

  // First add the presence to the map
  JidPresenceVectorMap::iterator pos;
  pos = incoming_presence_map_->find(jid.BareJid());
  if (pos == incoming_presence_map_->end()) {
    // Insert a new entry into the map.  Get the position of this new entry
    pos = (incoming_presence_map_->insert(
            std::make_pair(bare_jid, new PresenceVector()))).first;
  }

  PresenceVector * presence_vector = pos->second;
  ASSERT(presence_vector != NULL);

  // Try to find this jid in the bare jid bucket
  PresenceVector::iterator presence_pos;
  XmppPresenceImpl* presence;
  presence_pos = std::find_if(presence_vector->begin(),
                              presence_vector->end(),
                              PresencePredicate(jid));

  // Update/add it to the bucket
  if (presence_pos == presence_vector->end()) {
    presence = new XmppPresenceImpl();
    if (XMPP_RETURN_OK == presence->set_raw_xml(stanza)) {
      added = true;
      presence_vector->push_back(presence);
    } else {
      delete presence;
      presence = NULL;
    }
  } else {
    presence = *presence_pos;
    presence->set_raw_xml(stanza);
  }

  // now add to the comprehensive vector
  if (added)
    incoming_presence_vector_->push_back(presence);

  // Call back to the user with the changed presence information
  if (roster_handler_)
    roster_handler_->IncomingPresenceChanged(this, presence);
}


void
XmppRosterModuleImpl::InternalIncomingPresenceError(const Jid& jid,
                                                    const XmlElement* stanza) {
  if (roster_handler_)
    roster_handler_->SubscriptionError(this, jid, stanza);
}

void
XmppRosterModuleImpl::InternalRosterItems(const XmlElement* stanza) {
  const XmlElement* result_data = stanza->FirstNamed(QN_ROSTER_QUERY);
  if (!result_data)
    return; // unknown stuff in result!

  bool all_new = contacts_->empty();

  for (const XmlElement* roster_item = result_data->FirstNamed(QN_ROSTER_ITEM);
       roster_item;
       roster_item = roster_item->NextNamed(QN_ROSTER_ITEM))
  {
    const std::string& jid_string = roster_item->Attr(QN_JID);
    Jid jid(jid_string);
    if (!jid.IsValid())
      continue;

    // This algorithm is N^2 on the number of incoming contacts after the
    // initial load. There is no way to do this faster without allowing
    // duplicates, introducing more data structures or write a custom data
    // structure.  We'll see if this becomes a perf problem and fix it if it
    // does.
    ContactVector::iterator pos = contacts_->end();

    if (!all_new) {
      pos = std::find_if(contacts_->begin(),
                         contacts_->end(),
                         RosterPredicate(jid));
    }

    if (pos != contacts_->end()) { // Update/remove a current contact
      if (roster_item->Attr(QN_SUBSCRIPTION) == "remove") {
        XmppRosterContact* contact = *pos;
        contacts_->erase(pos);
        if (roster_handler_)
          roster_handler_->ContactRemoved(this, contact,
            std::distance(contacts_->begin(), pos));
        delete contact;
      } else {
        XmppRosterContact* old_contact = *pos;
        *pos = new XmppRosterContactImpl();
        (*pos)->SetXmlFromWire(roster_item);
        if (roster_handler_)
          roster_handler_->ContactChanged(this, old_contact,
            std::distance(contacts_->begin(), pos));
        delete old_contact;
      }
    } else { // Add a new contact
      XmppRosterContactImpl* contact = new XmppRosterContactImpl();
      contact->SetXmlFromWire(roster_item);
      contacts_->push_back(contact);
      if (roster_handler_ && !all_new)
        roster_handler_->ContactsAdded(this, contacts_->size() - 1, 1);
    }
  }

  // Send a consolidated update if all contacts are new
  if (roster_handler_ && all_new)
    roster_handler_->ContactsAdded(this, 0, contacts_->size());
}

}
