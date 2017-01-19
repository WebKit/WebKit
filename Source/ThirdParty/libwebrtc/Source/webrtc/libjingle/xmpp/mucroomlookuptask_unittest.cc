/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>
#include <vector>

#include "webrtc/libjingle/xmllite/xmlelement.h"
#include "webrtc/libjingle/xmpp/constants.h"
#include "webrtc/libjingle/xmpp/fakexmppclient.h"
#include "webrtc/libjingle/xmpp/mucroomlookuptask.h"
#include "webrtc/base/faketaskrunner.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/sigslot.h"

class MucRoomLookupListener : public sigslot::has_slots<> {
 public:
  MucRoomLookupListener() : error_count(0) {}

  void OnResult(buzz::MucRoomLookupTask* task,
                const buzz::MucRoomInfo& room) {
    last_room = room;
  }

  void OnError(buzz::IqTask* task,
               const buzz::XmlElement* error) {
    ++error_count;
  }

  buzz::MucRoomInfo last_room;
  int error_count;
};

class MucRoomLookupTaskTest : public testing::Test {
 public:
  MucRoomLookupTaskTest() :
      lookup_server_jid("lookup@domain.com"),
      room_jid("muc-jid-ponies@domain.com"),
      room_name("ponies"),
      room_domain("domain.com"),
      room_full_name("ponies@domain.com"),
      hangout_id("some_hangout_id") {
  }

  virtual void SetUp() {
    runner = new rtc::FakeTaskRunner();
    xmpp_client = new buzz::FakeXmppClient(runner);
    listener = new MucRoomLookupListener();
  }

  virtual void TearDown() {
    delete listener;
    // delete xmpp_client;  Deleted by deleting runner.
    delete runner;
  }

  rtc::FakeTaskRunner* runner;
  buzz::FakeXmppClient* xmpp_client;
  MucRoomLookupListener* listener;
  buzz::Jid lookup_server_jid;
  buzz::Jid room_jid;
  std::string room_name;
  std::string room_domain;
  std::string room_full_name;
  std::string hangout_id;
};

TEST_F(MucRoomLookupTaskTest, TestLookupName) {
  ASSERT_EQ(0U, xmpp_client->sent_stanzas().size());

  buzz::MucRoomLookupTask* task =
      buzz::MucRoomLookupTask::CreateLookupTaskForRoomName(
          xmpp_client, lookup_server_jid, room_name, room_domain);
  task->SignalResult.connect(listener, &MucRoomLookupListener::OnResult);
  task->Start();

  std::string expected_iq =
      "<cli:iq type=\"set\" to=\"lookup@domain.com\" id=\"0\" "
        "xmlns:cli=\"jabber:client\">"
        "<query xmlns=\"jabber:iq:search\">"
          "<room-name>ponies</room-name>"
          "<room-domain>domain.com</room-domain>"
        "</query>"
      "</cli:iq>";

  ASSERT_EQ(1U, xmpp_client->sent_stanzas().size());
  EXPECT_EQ(expected_iq, xmpp_client->sent_stanzas()[0]->Str());

  EXPECT_EQ("", listener->last_room.name);

  std::string response_iq =
      "<iq xmlns='jabber:client' from='lookup@domain.com' id='0' type='result'>"
      "  <query xmlns='jabber:iq:search'>"
      "    <item jid='muc-jid-ponies@domain.com'>"
      "      <room-name>ponies</room-name>"
      "      <room-domain>domain.com</room-domain>"
      "    </item>"
      "  </query>"
      "</iq>";

  xmpp_client->HandleStanza(buzz::XmlElement::ForStr(response_iq));

  EXPECT_EQ(room_name, listener->last_room.name);
  EXPECT_EQ(room_domain, listener->last_room.domain);
  EXPECT_EQ(room_jid, listener->last_room.jid);
  EXPECT_EQ(room_full_name, listener->last_room.full_name());
  EXPECT_EQ(0, listener->error_count);
}

TEST_F(MucRoomLookupTaskTest, TestLookupHangoutId) {
  ASSERT_EQ(0U, xmpp_client->sent_stanzas().size());

  buzz::MucRoomLookupTask* task = buzz::MucRoomLookupTask::CreateLookupTaskForHangoutId(
      xmpp_client, lookup_server_jid, hangout_id);
  task->SignalResult.connect(listener, &MucRoomLookupListener::OnResult);
  task->Start();

  std::string expected_iq =
      "<cli:iq type=\"set\" to=\"lookup@domain.com\" id=\"0\" "
        "xmlns:cli=\"jabber:client\">"
        "<query xmlns=\"jabber:iq:search\">"
          "<hangout-id>some_hangout_id</hangout-id>"
        "</query>"
      "</cli:iq>";

  ASSERT_EQ(1U, xmpp_client->sent_stanzas().size());
  EXPECT_EQ(expected_iq, xmpp_client->sent_stanzas()[0]->Str());

  EXPECT_EQ("", listener->last_room.name);

  std::string response_iq =
      "<iq xmlns='jabber:client' from='lookup@domain.com' id='0' type='result'>"
      "  <query xmlns='jabber:iq:search'>"
      "    <item jid='muc-jid-ponies@domain.com'>"
      "      <room-name>some_hangout_id</room-name>"
      "      <room-domain>domain.com</room-domain>"
      "    </item>"
      "  </query>"
      "</iq>";

  xmpp_client->HandleStanza(buzz::XmlElement::ForStr(response_iq));

  EXPECT_EQ(hangout_id, listener->last_room.name);
  EXPECT_EQ(room_domain, listener->last_room.domain);
  EXPECT_EQ(room_jid, listener->last_room.jid);
  EXPECT_EQ(0, listener->error_count);
}

TEST_F(MucRoomLookupTaskTest, TestError) {
  buzz::MucRoomLookupTask* task = buzz::MucRoomLookupTask::CreateLookupTaskForRoomName(
      xmpp_client, lookup_server_jid, room_name, room_domain);
  task->SignalError.connect(listener, &MucRoomLookupListener::OnError);
  task->Start();

  std::string error_iq =
      "<iq xmlns='jabber:client' id='0' type='error'"
      "  from='lookup@domain.com'>"
      "</iq>";

  EXPECT_EQ(0, listener->error_count);
  xmpp_client->HandleStanza(buzz::XmlElement::ForStr(error_iq));
  EXPECT_EQ(1, listener->error_count);
}

TEST_F(MucRoomLookupTaskTest, TestBadJid) {
  buzz::MucRoomLookupTask* task = buzz::MucRoomLookupTask::CreateLookupTaskForRoomName(
      xmpp_client, lookup_server_jid, room_name, room_domain);
  task->SignalError.connect(listener, &MucRoomLookupListener::OnError);
  task->Start();

  std::string response_iq =
      "<iq xmlns='jabber:client' from='lookup@domain.com' id='0' type='result'>"
      "  <query xmlns='jabber:iq:search'>"
      "    <item/>"
      "  </query>"
      "</iq>";

  EXPECT_EQ(0, listener->error_count);
  xmpp_client->HandleStanza(buzz::XmlElement::ForStr(response_iq));
  EXPECT_EQ(1, listener->error_count);
}
