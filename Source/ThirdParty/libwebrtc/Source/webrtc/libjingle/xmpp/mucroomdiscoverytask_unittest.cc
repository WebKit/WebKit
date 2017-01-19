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
#include "webrtc/libjingle/xmpp/mucroomdiscoverytask.h"
#include "webrtc/base/faketaskrunner.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/sigslot.h"

class MucRoomDiscoveryListener : public sigslot::has_slots<> {
 public:
  MucRoomDiscoveryListener() : error_count(0) {}

  void OnResult(buzz::MucRoomDiscoveryTask* task,
                bool exists,
                const std::string& name,
                const std::string& conversation_id,
                const std::set<std::string>& features,
                const std::map<std::string, std::string>& extended_info) {
    last_exists = exists;
    last_name = name;
    last_conversation_id = conversation_id;
    last_features = features;
    last_extended_info = extended_info;
  }

  void OnError(buzz::IqTask* task,
               const buzz::XmlElement* error) {
    ++error_count;
  }

  bool last_exists;
  std::string last_name;
  std::string last_conversation_id;
  std::set<std::string> last_features;
  std::map<std::string, std::string> last_extended_info;
  int error_count;
};

class MucRoomDiscoveryTaskTest : public testing::Test {
 public:
  MucRoomDiscoveryTaskTest() :
      room_jid("muc-jid-ponies@domain.com"),
      room_name("ponies"),
      conversation_id("test_conversation_id") {
  }

  virtual void SetUp() {
    runner = new rtc::FakeTaskRunner();
    xmpp_client = new buzz::FakeXmppClient(runner);
    listener = new MucRoomDiscoveryListener();
  }

  virtual void TearDown() {
    delete listener;
    // delete xmpp_client;  Deleted by deleting runner.
    delete runner;
  }

  rtc::FakeTaskRunner* runner;
  buzz::FakeXmppClient* xmpp_client;
  MucRoomDiscoveryListener* listener;
  buzz::Jid room_jid;
  std::string room_name;
  std::string conversation_id;
};

TEST_F(MucRoomDiscoveryTaskTest, TestDiscovery) {
  ASSERT_EQ(0U, xmpp_client->sent_stanzas().size());

  buzz::MucRoomDiscoveryTask* task = new buzz::MucRoomDiscoveryTask(
      xmpp_client, room_jid);
  task->SignalResult.connect(listener, &MucRoomDiscoveryListener::OnResult);
  task->Start();

  std::string expected_iq =
      "<cli:iq type=\"get\" to=\"muc-jid-ponies@domain.com\" id=\"0\" "
        "xmlns:cli=\"jabber:client\">"
        "<info:query xmlns:info=\"http://jabber.org/protocol/disco#info\"/>"
      "</cli:iq>";

  ASSERT_EQ(1U, xmpp_client->sent_stanzas().size());
  EXPECT_EQ(expected_iq, xmpp_client->sent_stanzas()[0]->Str());

  EXPECT_EQ("", listener->last_name);
  EXPECT_EQ("", listener->last_conversation_id);

  std::string response_iq =
      "<iq xmlns='jabber:client'"
      "    from='muc-jid-ponies@domain.com' id='0' type='result'>"
      "  <info:query xmlns:info='http://jabber.org/protocol/disco#info'>"
      "    <info:identity name='ponies'>"
      "      <han:conversation-id xmlns:han='google:muc#hangout'>"
      "test_conversation_id</han:conversation-id>"
      "    </info:identity>"
      "    <info:feature var='feature1'/>"
      "    <info:feature var='feature2'/>"
      "    <data:x xmlns:data='jabber:x:data'>"
      "      <data:field var='var1' data:value='value1' />"
      "      <data:field var='var2' data:value='value2' />"
      "    </data:x>"
      "  </info:query>"
      "</iq>";

  xmpp_client->HandleStanza(buzz::XmlElement::ForStr(response_iq));

  EXPECT_EQ(true, listener->last_exists);
  EXPECT_EQ(room_name, listener->last_name);
  EXPECT_EQ(conversation_id, listener->last_conversation_id);
  EXPECT_EQ(2U, listener->last_features.size());
  EXPECT_EQ(1U, listener->last_features.count("feature1"));
  EXPECT_EQ(2U, listener->last_extended_info.size());
  EXPECT_EQ("value1", listener->last_extended_info["var1"]);
  EXPECT_EQ(0, listener->error_count);
}

TEST_F(MucRoomDiscoveryTaskTest, TestMissingName) {
  buzz::MucRoomDiscoveryTask* task = new buzz::MucRoomDiscoveryTask(
      xmpp_client, room_jid);
  task->SignalError.connect(listener, &MucRoomDiscoveryListener::OnError);
  task->Start();

  std::string error_iq =
      "<iq xmlns='jabber:client'"
      "    from='muc-jid-ponies@domain.com' id='0' type='result'>"
      "  <info:query xmlns:info='http://jabber.org/protocol/disco#info'>"
      "    <info:identity />"
      "  </info:query>"
      "</iq>";
  EXPECT_EQ(0, listener->error_count);
  xmpp_client->HandleStanza(buzz::XmlElement::ForStr(error_iq));
  EXPECT_EQ(0, listener->error_count);
}
