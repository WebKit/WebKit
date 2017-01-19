/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_ROSTERMODULE_H_
#define WEBRTC_LIBJINGLE_XMPP_ROSTERMODULE_H_

#include "webrtc/libjingle/xmpp/module.h"

namespace buzz {

class XmppRosterModule;

// The main way you initialize and use the module would be like this:
//    XmppRosterModule *roster_module = XmppRosterModule::Create();
//    roster_module->RegisterEngine(engine);
//    roster_module->BroadcastPresence();
//    roster_module->RequestRosterUpdate();

//! This enum captures the valid values for the show attribute in a presence
//! stanza
enum XmppPresenceShow
{
  XMPP_PRESENCE_CHAT = 0,
  XMPP_PRESENCE_DEFAULT = 1,
  XMPP_PRESENCE_AWAY = 2,
  XMPP_PRESENCE_XA = 3,
  XMPP_PRESENCE_DND = 4,
};

//! These are the valid subscription states in a roster contact.  This
//! represents the combination of the subscription and ask attributes
enum XmppSubscriptionState
{
  XMPP_SUBSCRIPTION_NONE = 0,
  XMPP_SUBSCRIPTION_NONE_ASKED = 1,
  XMPP_SUBSCRIPTION_TO = 2,
  XMPP_SUBSCRIPTION_FROM = 3,
  XMPP_SUBSCRIPTION_FROM_ASKED = 4,
  XMPP_SUBSCRIPTION_BOTH = 5,
};

//! These represent the valid types of presence stanzas for managing
//! subscriptions
enum XmppSubscriptionRequestType
{
  XMPP_REQUEST_SUBSCRIBE = 0,
  XMPP_REQUEST_UNSUBSCRIBE = 1,
  XMPP_REQUEST_SUBSCRIBED = 2,
  XMPP_REQUEST_UNSUBSCRIBED = 3,
};

enum XmppPresenceAvailable {
  XMPP_PRESENCE_UNAVAILABLE = 0,
  XMPP_PRESENCE_AVAILABLE   = 1,
  XMPP_PRESENCE_ERROR       = 2,
};

enum XmppPresenceConnectionStatus {
  XMPP_CONNECTION_STATUS_UNKNOWN    = 0,
  // Status set by the server while the user is being rung.
  XMPP_CONNECTION_STATUS_CONNECTING = 1,
  // Status set by the client when the user has accepted the ring but before
  // the client has joined the call.
  XMPP_CONNECTION_STATUS_JOINING    = 2,
  // Status set by the client as part of joining the call.
  XMPP_CONNECTION_STATUS_CONNECTED  = 3,
  XMPP_CONNECTION_STATUS_HANGUP     = 4,
};

//! Presence Information
//! This class stores both presence information for outgoing presence and is
//! returned by methods in XmppRosterModule to represent recieved incoming
//! presence information.  When this class is writeable (non-const) then each
//! update to any property will set the inner xml.  Setting the raw_xml will
//! rederive all of the other properties.
class XmppPresence {
public:
  virtual ~XmppPresence() {}

  //! Create a new Presence
  //! This is typically only used when sending a directed presence
  static XmppPresence* Create();

  //! The Jid of for the presence information.
  //! Typically this will be a full Jid with resource specified.
  virtual const Jid jid() const = 0;

  //! Is the contact available?
  virtual XmppPresenceAvailable available() const = 0;

  //! Sets if the user is available or not
  virtual XmppReturnStatus set_available(XmppPresenceAvailable available) = 0;

  //! The show value of the presence info
  virtual XmppPresenceShow presence_show() const = 0;

  //! Set the presence show value
  virtual XmppReturnStatus set_presence_show(XmppPresenceShow show) = 0;

  //! The Priority of the presence info
  virtual int priority() const = 0;

  //! Set the priority of the presence
  virtual XmppReturnStatus set_priority(int priority) = 0;

  //! The plain text status of the presence info.
  //! If there are multiple status because of language, this will either be a
  //! status that is not tagged for language or the first available
  virtual const std::string status() const = 0;

  //! Sets the status for the presence info.
  //! If there is more than one status present already then this will remove
  //! them all and replace it with one status element we no specified language
  virtual XmppReturnStatus set_status(const std::string& status) = 0;

  //! The connection status
  virtual XmppPresenceConnectionStatus connection_status() const = 0;

  //! The focus obfuscated GAIA id
  virtual const std::string google_user_id() const = 0;

  //! The nickname in the presence
  virtual const std::string nickname() const = 0;

  //! The raw xml of the presence update
  virtual const XmlElement* raw_xml() const = 0;

  //! Sets the raw presence stanza for the presence update
  //! This will cause all other data items in this structure to be rederived
  virtual XmppReturnStatus set_raw_xml(const XmlElement * xml) = 0;
};

//! A contact as given by the server
class XmppRosterContact {
public:
  virtual ~XmppRosterContact() {}

  //! Create a new roster contact
  //! This is typically only used when doing a roster update/add
  static XmppRosterContact* Create();

  //! The jid for the contact.
  //! Typically this will be a bare Jid.
  virtual const Jid jid() const = 0;

  //! Sets the jid for the roster contact update
  virtual XmppReturnStatus set_jid(const Jid& jid) = 0;

  //! The name (nickname) stored for this contact
  virtual const std::string name() const = 0;

  //! Sets the name
  virtual XmppReturnStatus set_name(const std::string& name) = 0;

  //! The Presence subscription state stored on the server for this contact
  //! This is never settable and will be ignored when generating a roster
  //! add/update request
  virtual XmppSubscriptionState subscription_state() const = 0;

  //! The number of Groups applied to this contact
  virtual size_t GetGroupCount() const = 0;

  //! Gets a Group applied to the contact based on index.
  //! range
  virtual const std::string GetGroup(size_t index) const = 0;

  //! Adds a group to this contact.
  //! This will return a bad argument error if the group is already there.
  virtual XmppReturnStatus AddGroup(const std::string& group) = 0;

  //! Removes a group from the contact.
  //! This will return an error if the group cannot be found in the group list.
  virtual XmppReturnStatus RemoveGroup(const std::string& group) = 0;

  //! The raw xml for this roster contact
  virtual const XmlElement* raw_xml() const = 0;

  //! Sets the raw presence stanza for the contact update/add
  //! This will cause all other data items in this structure to be rederived
  virtual XmppReturnStatus set_raw_xml(const XmlElement * xml) = 0;
};

//! The XmppRosterHandler is an interface for callbacks from the module
class XmppRosterHandler {
public:
  virtual ~XmppRosterHandler() {}

  //! A request for a subscription has come in.
  //! Typically, the UI will ask the user if it is okay to let the requester
  //! get presence notifications for the user.  The response is send back
  //! by calling ApproveSubscriber or CancelSubscriber.
  virtual void SubscriptionRequest(XmppRosterModule* roster,
                                   const Jid& requesting_jid,
                                   XmppSubscriptionRequestType type,
                                   const XmlElement* raw_xml) = 0;

  //! Some type of presence error has occured
  virtual void SubscriptionError(XmppRosterModule* roster,
                                 const Jid& from,
                                 const XmlElement* raw_xml) = 0;

  virtual void RosterError(XmppRosterModule* roster,
                           const XmlElement* raw_xml) = 0;

  //! New presence information has come in
  //! The user is notified with the presence object directly.  This info is also
  //! added to the store accessable from the engine.
  virtual void IncomingPresenceChanged(XmppRosterModule* roster,
                                       const XmppPresence* presence) = 0;

  //! A contact has changed
  //! This indicates that the data for a contact may have changed.  No
  //! contacts have been added or removed.
  virtual void ContactChanged(XmppRosterModule* roster,
                              const XmppRosterContact* old_contact,
                              size_t index) = 0;

  //! A set of contacts have been added
  //! These contacts may have been added in response to the original roster
  //! request or due to a "roster push" from the server.
  virtual void ContactsAdded(XmppRosterModule* roster,
                             size_t index, size_t number) = 0;

  //! A contact has been removed
  //! This contact has been removed form the list.
  virtual void ContactRemoved(XmppRosterModule* roster,
                              const XmppRosterContact* removed_contact,
                              size_t index) = 0;

};

//! An XmppModule for handle roster and presence functionality
class XmppRosterModule : public XmppModule {
public:
  //! Creates a new XmppRosterModule
  static XmppRosterModule * Create();
  virtual ~XmppRosterModule() {}

  //! Sets the roster handler (callbacks) for the module
  virtual XmppReturnStatus set_roster_handler(XmppRosterHandler * handler) = 0;

  //! Gets the roster handler for the module
  virtual XmppRosterHandler* roster_handler() = 0;

  // USER PRESENCE STATE -------------------------------------------------------

  //! Gets the aggregate outgoing presence
  //! This object is non-const and be edited directly.  No update is sent
  //! to the server until a Broadcast is sent
  virtual XmppPresence* outgoing_presence() = 0;

  //! Broadcasts that the user is available.
  //! Nothing with respect to presence is sent until this is called.
  virtual XmppReturnStatus BroadcastPresence() = 0;

  //! Sends a directed presence to a Jid
  //! Note that the client doesn't store where directed presence notifications
  //! have been sent.  The server can keep the appropriate state
  virtual XmppReturnStatus SendDirectedPresence(const XmppPresence* presence,
                                                const Jid& to_jid) = 0;

  // INCOMING PRESENCE STATUS --------------------------------------------------

  //! Returns the number of incoming presence data recorded
  virtual size_t GetIncomingPresenceCount() = 0;

  //! Returns an incoming presence datum based on index
  virtual const XmppPresence* GetIncomingPresence(size_t index) = 0;

  //! Gets the number of presence data for a bare Jid
  //! There may be a datum per resource
  virtual size_t GetIncomingPresenceForJidCount(const Jid& jid) = 0;

  //! Returns a single presence data for a Jid based on index
  virtual const XmppPresence* GetIncomingPresenceForJid(const Jid& jid,
                                                        size_t index) = 0;

  // ROSTER MANAGEMENT ---------------------------------------------------------

  //! Requests an update of the roster from the server
  //! This must be called to initialize the client side cache of the roster
  //! After this is sent the server should keep this module apprised of any
  //! changes.
  virtual XmppReturnStatus RequestRosterUpdate() = 0;

  //! Returns the number of contacts in the roster
  virtual size_t GetRosterContactCount() = 0;

  //! Returns a contact by index
  virtual const XmppRosterContact* GetRosterContact(size_t index) = 0;

  //! Finds a contact by Jid
  virtual const XmppRosterContact* FindRosterContact(const Jid& jid) = 0;

  //! Send a request to the server to add a contact
  //! Note that the contact won't show up in the roster until the server can
  //! respond.  This happens async when the socket is being serviced
  virtual XmppReturnStatus RequestRosterChange(
    const XmppRosterContact* contact) = 0;

  //! Request that the server remove a contact
  //! The jabber protocol specifies that the server should also cancel any
  //! subscriptions when this is done.  Like adding, this contact won't be
  //! removed until the server responds.
  virtual XmppReturnStatus RequestRosterRemove(const Jid& jid) = 0;

  // SUBSCRIPTION MANAGEMENT ---------------------------------------------------

  //! Request a subscription to presence notifications form a Jid
  virtual XmppReturnStatus RequestSubscription(const Jid& jid) = 0;

  //! Cancel a subscription to presence notifications from a Jid
  virtual XmppReturnStatus CancelSubscription(const Jid& jid) = 0;

  //! Approve a request to deliver presence notifications to a jid
  virtual XmppReturnStatus ApproveSubscriber(const Jid& jid) = 0;

  //! Deny or cancel presence notification deliver to a jid
  virtual XmppReturnStatus CancelSubscriber(const Jid& jid) = 0;
};

}

#endif  // WEBRTC_LIBJINGLE_XMPP_ROSTERMODULE_H_
