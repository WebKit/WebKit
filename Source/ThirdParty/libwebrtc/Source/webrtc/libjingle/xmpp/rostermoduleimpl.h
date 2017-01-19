/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_XMPPTHREAD_H_
#define WEBRTC_LIBJINGLE_XMPP_XMPPTHREAD_H_

#include <memory>

#include "webrtc/libjingle/xmpp/moduleimpl.h"
#include "webrtc/libjingle/xmpp/rostermodule.h"

namespace buzz {

//! Presence Information
//! This class stores both presence information for outgoing presence and is
//! returned by methods in XmppRosterModule to represent received incoming
//! presence information.  When this class is writeable (non-const) then each
//! update to any property will set the inner xml.  Setting the raw_xml will
//! rederive all of the other properties.
class XmppPresenceImpl : public XmppPresence {
public:
  virtual ~XmppPresenceImpl() {}

  //! The from Jid of for the presence information.
  //! Typically this will be a full Jid with resource specified.  For outgoing
  //! presence this should remain JID_NULL and will be scrubbed from the
  //! stanza when being sent.
  virtual const Jid jid() const;

  //! Is the contact available?
  virtual XmppPresenceAvailable available() const;

  //! Sets if the user is available or not
  virtual XmppReturnStatus set_available(XmppPresenceAvailable available);

  //! The show value of the presence info
  virtual XmppPresenceShow presence_show() const;

  //! Set the presence show value
  virtual XmppReturnStatus set_presence_show(XmppPresenceShow show);

  //! The Priority of the presence info
  virtual int priority() const;

  //! Set the priority of the presence
  virtual XmppReturnStatus set_priority(int priority);

  //! The plain text status of the presence info.
  //! If there are multiple status because of language, this will either be a
  //! status that is not tagged for language or the first available
  virtual const std::string status() const;

  //! Sets the status for the presence info.
  //! If there is more than one status present already then this will remove
  //! them all and replace it with one status element we no specified language
  virtual XmppReturnStatus set_status(const std::string& status);

  //! The connection status
  virtual XmppPresenceConnectionStatus connection_status() const;

  //! The focus obfuscated GAIA id
  virtual const std::string google_user_id() const;

  //! The nickname in the presence
  virtual const std::string nickname() const;

  //! The raw xml of the presence update
  virtual const XmlElement* raw_xml() const;

  //! Sets the raw presence stanza for the presence update
  //! This will cause all other data items in this structure to be rederived
  virtual XmppReturnStatus set_raw_xml(const XmlElement * xml);

private:
  XmppPresenceImpl();

  friend class XmppPresence;
  friend class XmppRosterModuleImpl;

  void CreateRawXmlSkeleton();

  // Store everything in the XML element. If this becomes a perf issue we can
  // cache the data.
  std::unique_ptr<XmlElement> raw_xml_;
};

//! A contact as given by the server
class XmppRosterContactImpl : public XmppRosterContact {
public:
  virtual ~XmppRosterContactImpl() {}

  //! The jid for the contact.
  //! Typically this will be a bare Jid.
  virtual const Jid jid() const;

  //! Sets the jid for the roster contact update
  virtual XmppReturnStatus set_jid(const Jid& jid);

  //! The name (nickname) stored for this contact
  virtual const std::string name() const;

  //! Sets the name
  virtual XmppReturnStatus set_name(const std::string& name);

  //! The Presence subscription state stored on the server for this contact
  //! This is never settable and will be ignored when generating a roster
  //! add/update request
  virtual XmppSubscriptionState subscription_state() const;

  //! The number of Groups applied to this contact
  virtual size_t GetGroupCount() const;

  //! Gets a Group applied to the contact based on index.
  virtual const std::string GetGroup(size_t index) const;

  //! Adds a group to this contact.
  //! This will return a no error if the group is already present.
  virtual XmppReturnStatus AddGroup(const std::string& group);

  //! Removes a group from the contact.
  //! This will return no error if the group isn't there
  virtual XmppReturnStatus RemoveGroup(const std::string& group);

  //! The raw xml for this roster contact
  virtual const XmlElement* raw_xml() const;

  //! Sets the raw presence stanza for the presence update
  //! This will cause all other data items in this structure to be rederived
  virtual XmppReturnStatus set_raw_xml(const XmlElement * xml);

private:
  XmppRosterContactImpl();

  void CreateRawXmlSkeleton();
  void SetXmlFromWire(const XmlElement * xml);
  void ResetGroupCache();

  bool FindGroup(const std::string& group,
                 XmlElement** element,
                 XmlChild** child_before);


  friend class XmppRosterContact;
  friend class XmppRosterModuleImpl;

  int group_count_;
  int group_index_returned_;
  XmlElement * group_returned_;
  std::unique_ptr<XmlElement> raw_xml_;
};

//! An XmppModule for handle roster and presence functionality
class XmppRosterModuleImpl : public XmppModuleImpl,
  public XmppRosterModule, public XmppIqHandler {
public:
  virtual ~XmppRosterModuleImpl();

  IMPLEMENT_XMPPMODULE

  //! Sets the roster handler (callbacks) for the module
  virtual XmppReturnStatus set_roster_handler(XmppRosterHandler * handler);

  //! Gets the roster handler for the module
  virtual XmppRosterHandler* roster_handler();

  // USER PRESENCE STATE -------------------------------------------------------

  //! Gets the aggregate outgoing presence
  //! This object is non-const and be edited directly.  No update is sent
  //! to the server until a Broadcast is sent
  virtual XmppPresence* outgoing_presence();

  //! Broadcasts that the user is available.
  //! Nothing with respect to presence is sent until this is called.
  virtual XmppReturnStatus BroadcastPresence();

  //! Sends a directed presence to a Jid
  //! Note that the client doesn't store where directed presence notifications
  //! have been sent.  The server can keep the appropriate state
  virtual XmppReturnStatus SendDirectedPresence(const XmppPresence* presence,
                                                const Jid& to_jid);

  // INCOMING PRESENCE STATUS --------------------------------------------------

  //! Returns the number of incoming presence data recorded
  virtual size_t GetIncomingPresenceCount();

  //! Returns an incoming presence datum based on index
  virtual const XmppPresence* GetIncomingPresence(size_t index);

  //! Gets the number of presence data for a bare Jid
  //! There may be a datum per resource
  virtual size_t GetIncomingPresenceForJidCount(const Jid& jid);

  //! Returns a single presence data for a Jid based on index
  virtual const XmppPresence* GetIncomingPresenceForJid(const Jid& jid,
                                                        size_t index);

  // ROSTER MANAGEMENT ---------------------------------------------------------

  //! Requests an update of the roster from the server
  //! This must be called to initialize the client side cache of the roster
  //! After this is sent the server should keep this module apprised of any
  //! changes.
  virtual XmppReturnStatus RequestRosterUpdate();

  //! Returns the number of contacts in the roster
  virtual size_t GetRosterContactCount();

  //! Returns a contact by index
  virtual const XmppRosterContact* GetRosterContact(size_t index);

  //! Finds a contact by Jid
  virtual const XmppRosterContact* FindRosterContact(const Jid& jid);

  //! Send a request to the server to add a contact
  //! Note that the contact won't show up in the roster until the server can
  //! respond.  This happens async when the socket is being serviced
  virtual XmppReturnStatus RequestRosterChange(
    const XmppRosterContact* contact);

  //! Request that the server remove a contact
  //! The jabber protocol specifies that the server should also cancel any
  //! subscriptions when this is done.  Like adding, this contact won't be
  //! removed until the server responds.
  virtual XmppReturnStatus RequestRosterRemove(const Jid& jid);

  // SUBSCRIPTION MANAGEMENT ---------------------------------------------------

  //! Request a subscription to presence notifications form a Jid
  virtual XmppReturnStatus RequestSubscription(const Jid& jid);

  //! Cancel a subscription to presence notifications from a Jid
  virtual XmppReturnStatus CancelSubscription(const Jid& jid);

  //! Approve a request to deliver presence notifications to a jid
  virtual XmppReturnStatus ApproveSubscriber(const Jid& jid);

  //! Deny or cancel presence notification deliver to a jid
  virtual XmppReturnStatus CancelSubscriber(const Jid& jid);

  // XmppIqHandler IMPLEMENTATION ----------------------------------------------
  virtual void IqResponse(XmppIqCookie cookie, const XmlElement * stanza);

protected:
  // XmppModuleImpl OVERRIDES --------------------------------------------------
  virtual bool HandleStanza(const XmlElement *);

  // PRIVATE DATA --------------------------------------------------------------
private:
  friend class XmppRosterModule;
  XmppRosterModuleImpl();

  // Helper functions
  void DeleteIncomingPresence();
  void DeleteContacts();
  XmppReturnStatus SendSubscriptionRequest(const Jid& jid,
                                           const std::string& type);
  void InternalSubscriptionRequest(const Jid& jid, const XmlElement* stanza,
                                   XmppSubscriptionRequestType request_type);
  void InternalIncomingPresence(const Jid& jid, const XmlElement* stanza);
  void InternalIncomingPresenceError(const Jid& jid, const XmlElement* stanza);
  void InternalRosterItems(const XmlElement* stanza);

  // Member data
  XmppPresenceImpl outgoing_presence_;
  XmppRosterHandler* roster_handler_;

  typedef std::vector<XmppPresenceImpl*> PresenceVector;
  typedef std::map<Jid, PresenceVector*> JidPresenceVectorMap;
  std::unique_ptr<JidPresenceVectorMap> incoming_presence_map_;
  std::unique_ptr<PresenceVector> incoming_presence_vector_;

  typedef std::vector<XmppRosterContactImpl*> ContactVector;
  std::unique_ptr<ContactVector> contacts_;
};

}

#endif  // WEBRTC_LIBJINGLE_XMPP_XMPPTHREAD_H_
