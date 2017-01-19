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
#include <memory>
#include <sstream>
#include <string>

#include "webrtc/libjingle/xmllite/xmlelement.h"
#include "webrtc/libjingle/xmpp/constants.h"
#include "webrtc/libjingle/xmpp/rostermodule.h"
#include "webrtc/libjingle/xmpp/util_unittest.h"
#include "webrtc/libjingle/xmpp/xmppengine.h"
#include "webrtc/base/gunit.h"

#define TEST_OK(x) EXPECT_EQ((x),XMPP_RETURN_OK)
#define TEST_BADARGUMENT(x) EXPECT_EQ((x),XMPP_RETURN_BADARGUMENT)


namespace buzz {

class RosterModuleTest;

static void
WriteString(std::ostream& os, const std::string& str) {
  os<<str;
}

static void
WriteSubscriptionState(std::ostream& os, XmppSubscriptionState state)
{
  switch (state) {
    case XMPP_SUBSCRIPTION_NONE:
      os<<"none";
      break;
    case XMPP_SUBSCRIPTION_NONE_ASKED:
      os<<"none_asked";
      break;
    case XMPP_SUBSCRIPTION_TO:
      os<<"to";
      break;
    case XMPP_SUBSCRIPTION_FROM:
      os<<"from";
      break;
    case XMPP_SUBSCRIPTION_FROM_ASKED:
      os<<"from_asked";
      break;
    case XMPP_SUBSCRIPTION_BOTH:
      os<<"both";
      break;
    default:
      os<<"unknown";
      break;
  }
}

static void
WriteSubscriptionRequestType(std::ostream& os,
                             XmppSubscriptionRequestType type) {
  switch(type) {
    case XMPP_REQUEST_SUBSCRIBE:
      os<<"subscribe";
      break;
    case XMPP_REQUEST_UNSUBSCRIBE:
      os<<"unsubscribe";
      break;
    case XMPP_REQUEST_SUBSCRIBED:
      os<<"subscribed";
      break;
    case XMPP_REQUEST_UNSUBSCRIBED:
      os<<"unsubscribe";
      break;
    default:
      os<<"unknown";
      break;
  }
}

static void
WritePresenceShow(std::ostream& os, XmppPresenceShow show) {
  switch(show) {
    case XMPP_PRESENCE_AWAY:
      os<<"away";
      break;
    case XMPP_PRESENCE_CHAT:
      os<<"chat";
      break;
    case XMPP_PRESENCE_DND:
      os<<"dnd";
      break;
    case XMPP_PRESENCE_XA:
      os<<"xa";
      break;
    case XMPP_PRESENCE_DEFAULT:
      os<<"[default]";
      break;
    default:
      os<<"[unknown]";
      break;
  }
}

static void
WritePresence(std::ostream& os, const XmppPresence* presence) {
  if (presence == NULL) {
    os<<"NULL";
    return;
  }

  os<<"[Presence jid:";
  WriteString(os, presence->jid().Str());
  os<<" available:"<<presence->available();
  os<<" presence_show:";
  WritePresenceShow(os, presence->presence_show());
  os<<" priority:"<<presence->priority();
  os<<" status:";
  WriteString(os, presence->status());
  os<<"]"<<presence->raw_xml()->Str();
}

static void
WriteContact(std::ostream& os, const XmppRosterContact* contact) {
  if (contact == NULL) {
    os<<"NULL";
    return;
  }

  os<<"[Contact jid:";
  WriteString(os, contact->jid().Str());
  os<<" name:";
  WriteString(os, contact->name());
  os<<" subscription_state:";
  WriteSubscriptionState(os, contact->subscription_state());
  os<<" groups:[";
  for(size_t i=0; i < contact->GetGroupCount(); ++i) {
    os<<(i==0?"":", ");
    WriteString(os, contact->GetGroup(i));
  }
  os<<"]]"<<contact->raw_xml()->Str();
}

//! This session handler saves all calls to a string.  These are events and
//! data delivered form the engine to application code.
class XmppTestRosterHandler : public XmppRosterHandler {
public:
  XmppTestRosterHandler() {}
  virtual ~XmppTestRosterHandler() {}

  virtual void SubscriptionRequest(XmppRosterModule*,
                                   const Jid& requesting_jid,
                                   XmppSubscriptionRequestType type,
                                   const XmlElement* raw_xml) {
    ss_<<"[SubscriptionRequest Jid:" << requesting_jid.Str()<<" type:";
    WriteSubscriptionRequestType(ss_, type);
    ss_<<"]"<<raw_xml->Str();
  }

  //! Some type of presence error has occured
  virtual void SubscriptionError(XmppRosterModule*,
                                 const Jid& from,
                                 const XmlElement* raw_xml) {
    ss_<<"[SubscriptionError from:"<<from.Str()<<"]"<<raw_xml->Str();
  }

  virtual void RosterError(XmppRosterModule*,
                           const XmlElement* raw_xml) {
    ss_<<"[RosterError]"<<raw_xml->Str();
  }

  //! New presence information has come in
  //! The user is notified with the presence object directly.  This info is also
  //! added to the store accessable from the engine.
  virtual void IncomingPresenceChanged(XmppRosterModule*,
                                       const XmppPresence* presence) {
    ss_<<"[IncomingPresenceChanged presence:";
    WritePresence(ss_, presence);
    ss_<<"]";
  }

  //! A contact has changed
  //! This indicates that the data for a contact may have changed.  No
  //! contacts have been added or removed.
  virtual void ContactChanged(XmppRosterModule* roster,
                              const XmppRosterContact* old_contact,
                              size_t index) {
    ss_<<"[ContactChanged old_contact:";
    WriteContact(ss_, old_contact);
    ss_<<" index:"<<index<<" new_contact:";
    WriteContact(ss_, roster->GetRosterContact(index));
    ss_<<"]";
  }

  //! A set of contacts have been added
  //! These contacts may have been added in response to the original roster
  //! request or due to a "roster push" from the server.
  virtual void ContactsAdded(XmppRosterModule* roster,
                             size_t index, size_t number) {
    ss_<<"[ContactsAdded index:"<<index<<" number:"<<number;
    for (size_t i = 0; i < number; ++i) {
      ss_<<" "<<(index+i)<<":";
      WriteContact(ss_, roster->GetRosterContact(index+i));
    }
    ss_<<"]";
  }

  //! A contact has been removed
  //! This contact has been removed form the list.
  virtual void ContactRemoved(XmppRosterModule*,
                              const XmppRosterContact* removed_contact,
                              size_t index) {
    ss_<<"[ContactRemoved old_contact:";
    WriteContact(ss_, removed_contact);
    ss_<<" index:"<<index<<"]";
  }

  std::string Str() {
    return ss_.str();
  }

  std::string StrClear() {
    std::string result = ss_.str();
    ss_.str("");
    return result;
  }

private:
  std::stringstream ss_;
};

//! This is the class that holds all of the unit test code for the
//! roster module
class RosterModuleTest : public testing::Test {
public:
  RosterModuleTest() {}
  static void RunLogin(RosterModuleTest* obj, XmppEngine* engine,
                       XmppTestHandler* handler) {
    // Needs to be similar to XmppEngineTest::RunLogin
  }
};

TEST_F(RosterModuleTest, TestPresence) {
  XmlElement* status = new XmlElement(QN_GOOGLE_PSTN_CONFERENCE_STATUS);
  status->AddAttr(QN_STATUS, STR_PSTN_CONFERENCE_STATUS_CONNECTING);
  XmlElement presence_xml(QN_PRESENCE);
  presence_xml.AddElement(status);
  std::unique_ptr<XmppPresence> presence(XmppPresence::Create());
  presence->set_raw_xml(&presence_xml);
  EXPECT_EQ(presence->connection_status(), XMPP_CONNECTION_STATUS_CONNECTING);
}

TEST_F(RosterModuleTest, TestOutgoingPresence) {
  std::stringstream dump;

  std::unique_ptr<XmppEngine> engine(XmppEngine::Create());
  XmppTestHandler handler(engine.get());
  XmppTestRosterHandler roster_handler;

  std::unique_ptr<XmppRosterModule> roster(XmppRosterModule::Create());
  roster->set_roster_handler(&roster_handler);

  // Configure the roster module
  roster->RegisterEngine(engine.get());

  // Set up callbacks
  engine->SetOutputHandler(&handler);
  engine->AddStanzaHandler(&handler);
  engine->SetSessionHandler(&handler);

  // Set up minimal login info
  engine->SetUser(Jid("david@my-server"));
  // engine->SetPassword("david");

  // Do the whole login handshake
  RunLogin(this, engine.get(), &handler);
  EXPECT_EQ("", handler.OutputActivity());

  // Set some presence and broadcast it
  TEST_OK(roster->outgoing_presence()->
    set_available(XMPP_PRESENCE_AVAILABLE));
  TEST_OK(roster->outgoing_presence()->set_priority(-37));
  TEST_OK(roster->outgoing_presence()->set_presence_show(XMPP_PRESENCE_DND));
  TEST_OK(roster->outgoing_presence()->
    set_status("I'm off to the races!<>&"));
  TEST_OK(roster->BroadcastPresence());

  EXPECT_EQ(roster_handler.StrClear(), "");
  EXPECT_EQ(handler.OutputActivity(),
    "<presence>"
      "<priority>-37</priority>"
      "<show>dnd</show>"
      "<status>I'm off to the races!&lt;&gt;&amp;</status>"
    "</presence>");
  EXPECT_EQ(handler.SessionActivity(), "");

  // Try some more
  TEST_OK(roster->outgoing_presence()->
    set_available(XMPP_PRESENCE_UNAVAILABLE));
  TEST_OK(roster->outgoing_presence()->set_priority(0));
  TEST_OK(roster->outgoing_presence()->set_presence_show(XMPP_PRESENCE_XA));
  TEST_OK(roster->outgoing_presence()->set_status("Gone fishin'"));
  TEST_OK(roster->BroadcastPresence());

  EXPECT_EQ(roster_handler.StrClear(), "");
  EXPECT_EQ(handler.OutputActivity(),
    "<presence type=\"unavailable\">"
      "<show>xa</show>"
      "<status>Gone fishin'</status>"
    "</presence>");
  EXPECT_EQ(handler.SessionActivity(), "");

  // Okay -- we are back on
  TEST_OK(roster->outgoing_presence()->
    set_available(XMPP_PRESENCE_AVAILABLE));
  TEST_BADARGUMENT(roster->outgoing_presence()->set_priority(128));
  TEST_OK(roster->outgoing_presence()->
      set_presence_show(XMPP_PRESENCE_DEFAULT));
  TEST_OK(roster->outgoing_presence()->set_status("Cookin' wit gas"));
  TEST_OK(roster->BroadcastPresence());

  EXPECT_EQ(roster_handler.StrClear(), "");
  EXPECT_EQ(handler.OutputActivity(),
    "<presence>"
      "<status>Cookin' wit gas</status>"
    "</presence>");
  EXPECT_EQ(handler.SessionActivity(), "");

  // Set it via XML
  XmlElement presence_input(QN_PRESENCE);
  presence_input.AddAttr(QN_TYPE, "unavailable");
  presence_input.AddElement(new XmlElement(QN_PRIORITY));
  presence_input.AddText("42", 1);
  presence_input.AddElement(new XmlElement(QN_STATUS));
  presence_input.AddAttr(QN_XML_LANG, "es", 1);
  presence_input.AddText("Hola Amigos!", 1);
  presence_input.AddElement(new XmlElement(QN_STATUS));
  presence_input.AddText("Hey there, friend!", 1);
  TEST_OK(roster->outgoing_presence()->set_raw_xml(&presence_input));
  TEST_OK(roster->BroadcastPresence());

  WritePresence(dump, roster->outgoing_presence());
  EXPECT_EQ(dump.str(),
    "[Presence jid: available:0 presence_show:[default] "
              "priority:42 status:Hey there, friend!]"
    "<cli:presence type=\"unavailable\" xmlns:cli=\"jabber:client\">"
      "<cli:priority>42</cli:priority>"
      "<cli:status xml:lang=\"es\">Hola Amigos!</cli:status>"
      "<cli:status>Hey there, friend!</cli:status>"
    "</cli:presence>");
  dump.str("");
  EXPECT_EQ(roster_handler.StrClear(), "");
  EXPECT_EQ(handler.OutputActivity(),
    "<presence type=\"unavailable\">"
      "<priority>42</priority>"
      "<status xml:lang=\"es\">Hola Amigos!</status>"
      "<status>Hey there, friend!</status>"
    "</presence>");
  EXPECT_EQ(handler.SessionActivity(), "");

  // Construct a directed presence
  std::unique_ptr<XmppPresence> directed_presence(XmppPresence::Create());
  TEST_OK(directed_presence->set_available(XMPP_PRESENCE_AVAILABLE));
  TEST_OK(directed_presence->set_priority(120));
  TEST_OK(directed_presence->set_status("*very* available"));
  TEST_OK(roster->SendDirectedPresence(directed_presence.get(),
                                       Jid("myhoney@honey.net")));

  EXPECT_EQ(roster_handler.StrClear(), "");
  EXPECT_EQ(handler.OutputActivity(),
    "<presence to=\"myhoney@honey.net\">"
      "<priority>120</priority>"
      "<status>*very* available</status>"
    "</presence>");
  EXPECT_EQ(handler.SessionActivity(), "");
}

TEST_F(RosterModuleTest, TestIncomingPresence) {
  std::unique_ptr<XmppEngine> engine(XmppEngine::Create());
  XmppTestHandler handler(engine.get());
  XmppTestRosterHandler roster_handler;

  std::unique_ptr<XmppRosterModule> roster(XmppRosterModule::Create());
  roster->set_roster_handler(&roster_handler);

  // Configure the roster module
  roster->RegisterEngine(engine.get());

  // Set up callbacks
  engine->SetOutputHandler(&handler);
  engine->AddStanzaHandler(&handler);
  engine->SetSessionHandler(&handler);

  // Set up minimal login info
  engine->SetUser(Jid("david@my-server"));
  // engine->SetPassword("david");

  // Do the whole login handshake
  RunLogin(this, engine.get(), &handler);
  EXPECT_EQ("", handler.OutputActivity());

  // Load up with a bunch of data
  std::string input;
  input = "<presence from='maude@example.net/studio' "
                    "to='david@my-server/test'/>"
          "<presence from='walter@example.net/home' "
                    "to='david@my-server/test'>"
            "<priority>-10</priority>"
            "<show>xa</show>"
            "<status>Off bowling</status>"
          "</presence>"
          "<presence from='walter@example.net/alley' "
                    "to='david@my-server/test'>"
            "<priority>20</priority>"
            "<status>Looking for toes...</status>"
          "</presence>"
          "<presence from='donny@example.net/alley' "
                    "to='david@my-server/test'>"
            "<priority>10</priority>"
            "<status>Throwing rocks</status>"
          "</presence>";
  TEST_OK(engine->HandleInput(input.c_str(), input.length()));

  EXPECT_EQ(roster_handler.StrClear(),
    "[IncomingPresenceChanged "
      "presence:[Presence jid:maude@example.net/studio available:1 "
                 "presence_show:[default] priority:0 status:]"
        "<cli:presence from=\"maude@example.net/studio\" "
                      "to=\"david@my-server/test\" "
                      "xmlns:cli=\"jabber:client\"/>]"
    "[IncomingPresenceChanged "
      "presence:[Presence jid:walter@example.net/home available:1 "
                 "presence_show:xa priority:-10 status:Off bowling]"
        "<cli:presence from=\"walter@example.net/home\" "
                      "to=\"david@my-server/test\" "
                      "xmlns:cli=\"jabber:client\">"
          "<cli:priority>-10</cli:priority>"
          "<cli:show>xa</cli:show>"
          "<cli:status>Off bowling</cli:status>"
        "</cli:presence>]"
    "[IncomingPresenceChanged "
      "presence:[Presence jid:walter@example.net/alley available:1 "
                 "presence_show:[default] "
                 "priority:20 status:Looking for toes...]"
        "<cli:presence from=\"walter@example.net/alley\" "
                       "to=\"david@my-server/test\" "
                       "xmlns:cli=\"jabber:client\">"
          "<cli:priority>20</cli:priority>"
          "<cli:status>Looking for toes...</cli:status>"
        "</cli:presence>]"
    "[IncomingPresenceChanged "
      "presence:[Presence jid:donny@example.net/alley available:1 "
                 "presence_show:[default] priority:10 status:Throwing rocks]"
        "<cli:presence from=\"donny@example.net/alley\" "
                      "to=\"david@my-server/test\" "
                      "xmlns:cli=\"jabber:client\">"
          "<cli:priority>10</cli:priority>"
          "<cli:status>Throwing rocks</cli:status>"
        "</cli:presence>]");
  EXPECT_EQ(handler.OutputActivity(), "");
  handler.SessionActivity(); // Ignore the session output

  // Now look at the data structure we've built
  EXPECT_EQ(roster->GetIncomingPresenceCount(), static_cast<size_t>(4));
  EXPECT_EQ(roster->GetIncomingPresenceForJidCount(Jid("maude@example.net")),
            static_cast<size_t>(1));
  EXPECT_EQ(roster->GetIncomingPresenceForJidCount(Jid("walter@example.net")),
            static_cast<size_t>(2));

  const XmppPresence * presence;
  presence = roster->GetIncomingPresenceForJid(Jid("walter@example.net"), 1);

  std::stringstream dump;
  WritePresence(dump, presence);
  EXPECT_EQ(dump.str(),
          "[Presence jid:walter@example.net/alley available:1 "
            "presence_show:[default] priority:20 status:Looking for toes...]"
              "<cli:presence from=\"walter@example.net/alley\" "
                            "to=\"david@my-server/test\" "
                            "xmlns:cli=\"jabber:client\">"
                "<cli:priority>20</cli:priority>"
                "<cli:status>Looking for toes...</cli:status>"
              "</cli:presence>");
  dump.str("");

  // Maude took off...
  input = "<presence from='maude@example.net/studio' "
                    "to='david@my-server/test' "
                    "type='unavailable'>"
            "<status>Stealing my rug back</status>"
            "<priority>-10</priority>"
          "</presence>";
  TEST_OK(engine->HandleInput(input.c_str(), input.length()));

  EXPECT_EQ(roster_handler.StrClear(),
    "[IncomingPresenceChanged "
      "presence:[Presence jid:maude@example.net/studio available:0 "
                 "presence_show:[default] priority:-10 "
                 "status:Stealing my rug back]"
        "<cli:presence from=\"maude@example.net/studio\" "
                      "to=\"david@my-server/test\" type=\"unavailable\" "
                      "xmlns:cli=\"jabber:client\">"
          "<cli:status>Stealing my rug back</cli:status>"
          "<cli:priority>-10</cli:priority>"
        "</cli:presence>]");
  EXPECT_EQ(handler.OutputActivity(), "");
  handler.SessionActivity(); // Ignore the session output
}

TEST_F(RosterModuleTest, TestPresenceSubscription) {
  std::unique_ptr<XmppEngine> engine(XmppEngine::Create());
  XmppTestHandler handler(engine.get());
  XmppTestRosterHandler roster_handler;

  std::unique_ptr<XmppRosterModule> roster(XmppRosterModule::Create());
  roster->set_roster_handler(&roster_handler);

  // Configure the roster module
  roster->RegisterEngine(engine.get());

  // Set up callbacks
  engine->SetOutputHandler(&handler);
  engine->AddStanzaHandler(&handler);
  engine->SetSessionHandler(&handler);

  // Set up minimal login info
  engine->SetUser(Jid("david@my-server"));
  // engine->SetPassword("david");

  // Do the whole login handshake
  RunLogin(this, engine.get(), &handler);
  EXPECT_EQ("", handler.OutputActivity());

  // Test incoming requests
  std::string input;
  input =
    "<presence from='maude@example.net' type='subscribe'/>"
    "<presence from='maude@example.net' type='unsubscribe'/>"
    "<presence from='maude@example.net' type='subscribed'/>"
    "<presence from='maude@example.net' type='unsubscribed'/>";
  TEST_OK(engine->HandleInput(input.c_str(), input.length()));

  EXPECT_EQ(roster_handler.StrClear(),
    "[SubscriptionRequest Jid:maude@example.net type:subscribe]"
      "<cli:presence from=\"maude@example.net\" type=\"subscribe\" "
                    "xmlns:cli=\"jabber:client\"/>"
    "[SubscriptionRequest Jid:maude@example.net type:unsubscribe]"
      "<cli:presence from=\"maude@example.net\" type=\"unsubscribe\" "
                    "xmlns:cli=\"jabber:client\"/>"
    "[SubscriptionRequest Jid:maude@example.net type:subscribed]"
      "<cli:presence from=\"maude@example.net\" type=\"subscribed\" "
                    "xmlns:cli=\"jabber:client\"/>"
    "[SubscriptionRequest Jid:maude@example.net type:unsubscribe]"
      "<cli:presence from=\"maude@example.net\" type=\"unsubscribed\" "
                    "xmlns:cli=\"jabber:client\"/>");
  EXPECT_EQ(handler.OutputActivity(), "");
  handler.SessionActivity(); // Ignore the session output

  TEST_OK(roster->RequestSubscription(Jid("maude@example.net")));
  TEST_OK(roster->CancelSubscription(Jid("maude@example.net")));
  TEST_OK(roster->ApproveSubscriber(Jid("maude@example.net")));
  TEST_OK(roster->CancelSubscriber(Jid("maude@example.net")));

  EXPECT_EQ(roster_handler.StrClear(), "");
  EXPECT_EQ(handler.OutputActivity(),
    "<presence to=\"maude@example.net\" type=\"subscribe\"/>"
    "<presence to=\"maude@example.net\" type=\"unsubscribe\"/>"
    "<presence to=\"maude@example.net\" type=\"subscribed\"/>"
    "<presence to=\"maude@example.net\" type=\"unsubscribed\"/>");
  EXPECT_EQ(handler.SessionActivity(), "");
}

TEST_F(RosterModuleTest, TestRosterReceive) {
  std::unique_ptr<XmppEngine> engine(XmppEngine::Create());
  XmppTestHandler handler(engine.get());
  XmppTestRosterHandler roster_handler;

  std::unique_ptr<XmppRosterModule> roster(XmppRosterModule::Create());
  roster->set_roster_handler(&roster_handler);

  // Configure the roster module
  roster->RegisterEngine(engine.get());

  // Set up callbacks
  engine->SetOutputHandler(&handler);
  engine->AddStanzaHandler(&handler);
  engine->SetSessionHandler(&handler);

  // Set up minimal login info
  engine->SetUser(Jid("david@my-server"));
  // engine->SetPassword("david");

  // Do the whole login handshake
  RunLogin(this, engine.get(), &handler);
  EXPECT_EQ("", handler.OutputActivity());

  // Request a roster update
  TEST_OK(roster->RequestRosterUpdate());

  EXPECT_EQ(roster_handler.StrClear(),"");
  EXPECT_EQ(handler.OutputActivity(),
    "<iq type=\"get\" id=\"2\">"
      "<query xmlns=\"jabber:iq:roster\"/>"
    "</iq>");
  EXPECT_EQ(handler.SessionActivity(), "");

  // Prime the roster with a starting set
  std::string input =
    "<iq to='david@myserver/test' type='result' id='2'>"
      "<query xmlns='jabber:iq:roster'>"
        "<item jid='maude@example.net' "
              "name='Maude Lebowski' "
              "subscription='none' "
              "ask='subscribe'>"
          "<group>Business Partners</group>"
        "</item>"
        "<item jid='walter@example.net' "
              "name='Walter Sobchak' "
              "subscription='both'>"
          "<group>Friends</group>"
          "<group>Bowling Team</group>"
          "<group>Bowling League</group>"
        "</item>"
        "<item jid='donny@example.net' "
              "name='Donny' "
              "subscription='both'>"
          "<group>Friends</group>"
          "<group>Bowling Team</group>"
          "<group>Bowling League</group>"
        "</item>"
        "<item jid='jeffrey@example.net' "
              "name='The Big Lebowski' "
              "subscription='to'>"
          "<group>Business Partners</group>"
        "</item>"
        "<item jid='jesus@example.net' "
              "name='Jesus Quintana' "
              "subscription='from'>"
          "<group>Bowling League</group>"
        "</item>"
      "</query>"
    "</iq>";

  TEST_OK(engine->HandleInput(input.c_str(), input.length()));

  EXPECT_EQ(roster_handler.StrClear(),
    "[ContactsAdded index:0 number:5 "
      "0:[Contact jid:maude@example.net name:Maude Lebowski "
                 "subscription_state:none_asked "
                 "groups:[Business Partners]]"
        "<ros:item jid=\"maude@example.net\" name=\"Maude Lebowski\" "
                  "subscription=\"none\" ask=\"subscribe\" "
                  "xmlns:ros=\"jabber:iq:roster\">"
          "<ros:group>Business Partners</ros:group>"
        "</ros:item> "
      "1:[Contact jid:walter@example.net name:Walter Sobchak "
                 "subscription_state:both "
                 "groups:[Friends, Bowling Team, Bowling League]]"
        "<ros:item jid=\"walter@example.net\" name=\"Walter Sobchak\" "
                  "subscription=\"both\" "
                  "xmlns:ros=\"jabber:iq:roster\">"
          "<ros:group>Friends</ros:group>"
          "<ros:group>Bowling Team</ros:group>"
          "<ros:group>Bowling League</ros:group>"
        "</ros:item> "
      "2:[Contact jid:donny@example.net name:Donny "
                 "subscription_state:both "
                 "groups:[Friends, Bowling Team, Bowling League]]"
        "<ros:item jid=\"donny@example.net\" name=\"Donny\" "
                  "subscription=\"both\" "
                  "xmlns:ros=\"jabber:iq:roster\">"
          "<ros:group>Friends</ros:group>"
          "<ros:group>Bowling Team</ros:group>"
          "<ros:group>Bowling League</ros:group>"
        "</ros:item> "
      "3:[Contact jid:jeffrey@example.net name:The Big Lebowski "
                 "subscription_state:to "
                 "groups:[Business Partners]]"
        "<ros:item jid=\"jeffrey@example.net\" name=\"The Big Lebowski\" "
                  "subscription=\"to\" "
                  "xmlns:ros=\"jabber:iq:roster\">"
          "<ros:group>Business Partners</ros:group>"
        "</ros:item> "
      "4:[Contact jid:jesus@example.net name:Jesus Quintana "
                 "subscription_state:from groups:[Bowling League]]"
        "<ros:item jid=\"jesus@example.net\" name=\"Jesus Quintana\" "
                  "subscription=\"from\" xmlns:ros=\"jabber:iq:roster\">"
          "<ros:group>Bowling League</ros:group>"
        "</ros:item>]");
  EXPECT_EQ(handler.OutputActivity(), "");
  EXPECT_EQ(handler.SessionActivity(), "");

  // Request that someone be added
  std::unique_ptr<XmppRosterContact> contact(XmppRosterContact::Create());
  TEST_OK(contact->set_jid(Jid("brandt@example.net")));
  TEST_OK(contact->set_name("Brandt"));
  TEST_OK(contact->AddGroup("Business Partners"));
  TEST_OK(contact->AddGroup("Watchers"));
  TEST_OK(contact->AddGroup("Friends"));
  TEST_OK(contact->RemoveGroup("Friends")); // Maybe not...
  TEST_OK(roster->RequestRosterChange(contact.get()));

  EXPECT_EQ(roster_handler.StrClear(), "");
  EXPECT_EQ(handler.OutputActivity(),
    "<iq type=\"set\" id=\"3\">"
      "<query xmlns=\"jabber:iq:roster\">"
        "<item jid=\"brandt@example.net\" "
              "name=\"Brandt\">"
          "<group>Business Partners</group>"
          "<group>Watchers</group>"
        "</item>"
      "</query>"
    "</iq>");
  EXPECT_EQ(handler.SessionActivity(), "");

  // Get the push from the server
  input =
    "<iq type='result' to='david@my-server/test' id='3'/>"
    "<iq type='set' id='server_1'>"
      "<query xmlns='jabber:iq:roster'>"
        "<item jid='brandt@example.net' "
              "name='Brandt' "
              "subscription='none'>"
          "<group>Business Partners</group>"
          "<group>Watchers</group>"
        "</item>"
      "</query>"
    "</iq>";
  TEST_OK(engine->HandleInput(input.c_str(), input.length()));

  EXPECT_EQ(roster_handler.StrClear(),
    "[ContactsAdded index:5 number:1 "
      "5:[Contact jid:brandt@example.net name:Brandt "
                 "subscription_state:none "
                 "groups:[Business Partners, Watchers]]"
        "<ros:item jid=\"brandt@example.net\" name=\"Brandt\" "
                  "subscription=\"none\" "
                  "xmlns:ros=\"jabber:iq:roster\">"
          "<ros:group>Business Partners</ros:group>"
          "<ros:group>Watchers</ros:group>"
        "</ros:item>]");
  EXPECT_EQ(handler.OutputActivity(),
    "<iq type=\"result\" id=\"server_1\"/>");
  EXPECT_EQ(handler.SessionActivity(), "");

  // Get a contact update
  input =
    "<iq type='set' id='server_2'>"
      "<query xmlns='jabber:iq:roster'>"
        "<item jid='walter@example.net' "
              "name='Walter Sobchak' "
              "subscription='both'>"
          "<group>Friends</group>"
          "<group>Bowling Team</group>"
          "<group>Bowling League</group>"
          "<group>Not wrong, just an...</group>"
        "</item>"
      "</query>"
    "</iq>";

  TEST_OK(engine->HandleInput(input.c_str(), input.length()));

  EXPECT_EQ(roster_handler.StrClear(),
    "[ContactChanged "
      "old_contact:[Contact jid:walter@example.net name:Walter Sobchak "
                           "subscription_state:both "
                           "groups:[Friends, Bowling Team, Bowling League]]"
        "<ros:item jid=\"walter@example.net\" name=\"Walter Sobchak\" "
                  "subscription=\"both\" xmlns:ros=\"jabber:iq:roster\">"
          "<ros:group>Friends</ros:group>"
          "<ros:group>Bowling Team</ros:group>"
          "<ros:group>Bowling League</ros:group>"
          "</ros:item> "
      "index:1 "
      "new_contact:[Contact jid:walter@example.net name:Walter Sobchak "
                           "subscription_state:both "
                           "groups:[Friends, Bowling Team, Bowling League, "
                                   "Not wrong, just an...]]"
        "<ros:item jid=\"walter@example.net\" name=\"Walter Sobchak\" "
                  "subscription=\"both\" xmlns:ros=\"jabber:iq:roster\">"
          "<ros:group>Friends</ros:group>"
          "<ros:group>Bowling Team</ros:group>"
          "<ros:group>Bowling League</ros:group>"
          "<ros:group>Not wrong, just an...</ros:group>"
        "</ros:item>]");
  EXPECT_EQ(handler.OutputActivity(),
    "<iq type=\"result\" id=\"server_2\"/>");
  EXPECT_EQ(handler.SessionActivity(), "");

  // Remove a contact
  TEST_OK(roster->RequestRosterRemove(Jid("jesus@example.net")));

  EXPECT_EQ(roster_handler.StrClear(), "");
  EXPECT_EQ(handler.OutputActivity(),
    "<iq type=\"set\" id=\"4\">"
      "<query xmlns=\"jabber:iq:roster\" jid=\"jesus@example.net\" "
             "subscription=\"remove\"/>"
    "</iq>");
  EXPECT_EQ(handler.SessionActivity(), "");

  // Response from the server
  input =
    "<iq type='result' to='david@my-server/test' id='4'/>"
    "<iq type='set' id='server_3'>"
      "<query xmlns='jabber:iq:roster'>"
        "<item jid='jesus@example.net' "
              "subscription='remove'>"
        "</item>"
      "</query>"
    "</iq>";
  TEST_OK(engine->HandleInput(input.c_str(), input.length()));

  EXPECT_EQ(roster_handler.StrClear(),
    "[ContactRemoved "
      "old_contact:[Contact jid:jesus@example.net name:Jesus Quintana "
                           "subscription_state:from groups:[Bowling League]]"
        "<ros:item jid=\"jesus@example.net\" name=\"Jesus Quintana\" "
                  "subscription=\"from\" "
                  "xmlns:ros=\"jabber:iq:roster\">"
          "<ros:group>Bowling League</ros:group>"
        "</ros:item> index:4]");
  EXPECT_EQ(handler.OutputActivity(),
    "<iq type=\"result\" id=\"server_3\"/>");
  EXPECT_EQ(handler.SessionActivity(), "");
}

}
