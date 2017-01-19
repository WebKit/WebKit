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
#include "webrtc/libjingle/xmpp/mucroomconfigtask.h"
#include "webrtc/base/faketaskrunner.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/sigslot.h"

class MucRoomConfigListener : public sigslot::has_slots<> {
 public:
  MucRoomConfigListener() : result_count(0), error_count(0) {}

  void OnResult(buzz::MucRoomConfigTask*) {
    ++result_count;
  }

  void OnError(buzz::IqTask* task,
               const buzz::XmlElement* error) {
    ++error_count;
  }

  int result_count;
  int error_count;
};

class MucRoomConfigTaskTest : public testing::Test {
 public:
  MucRoomConfigTaskTest() :
      room_jid("muc-jid-ponies@domain.com"),
      room_name("ponies") {
  }

  virtual void SetUp() {
    runner = new rtc::FakeTaskRunner();
    xmpp_client = new buzz::FakeXmppClient(runner);
    listener = new MucRoomConfigListener();
  }

  virtual void TearDown() {
    delete listener;
    // delete xmpp_client;  Deleted by deleting runner.
    delete runner;
  }

  rtc::FakeTaskRunner* runner;
  buzz::FakeXmppClient* xmpp_client;
  MucRoomConfigListener* listener;
  buzz::Jid room_jid;
  std::string room_name;
};

TEST_F(MucRoomConfigTaskTest, TestConfigEnterprise) {
  ASSERT_EQ(0U, xmpp_client->sent_stanzas().size());

  std::vector<std::string> room_features;
  room_features.push_back("feature1");
  room_features.push_back("feature2");
  buzz::MucRoomConfigTask* task = new buzz::MucRoomConfigTask(
      xmpp_client, room_jid, "ponies", room_features);
  EXPECT_EQ(room_jid, task->room_jid());

  task->SignalResult.connect(listener, &MucRoomConfigListener::OnResult);
  task->Start();

  std::string expected_iq =
      "<cli:iq type=\"set\" to=\"muc-jid-ponies@domain.com\" id=\"0\" "
        "xmlns:cli=\"jabber:client\">"
        "<query xmlns=\"http://jabber.org/protocol/muc#owner\">"
          "<x xmlns=\"jabber:x:data\" type=\"form\">"
            "<field var=\"muc#roomconfig_roomname\" type=\"text-single\">"
              "<value>ponies</value>"
            "</field>"
            "<field var=\"muc#roomconfig_features\" type=\"list-multi\">"
              "<value>feature1</value>"
              "<value>feature2</value>"
            "</field>"
          "</x>"
        "</query>"
      "</cli:iq>";

  ASSERT_EQ(1U, xmpp_client->sent_stanzas().size());
  EXPECT_EQ(expected_iq, xmpp_client->sent_stanzas()[0]->Str());

  EXPECT_EQ(0, listener->result_count);
  EXPECT_EQ(0, listener->error_count);

  std::string response_iq =
      "<iq xmlns='jabber:client' id='0' type='result'"
      "  from='muc-jid-ponies@domain.com'>"
      "</iq>";

  xmpp_client->HandleStanza(buzz::XmlElement::ForStr(response_iq));

  EXPECT_EQ(1, listener->result_count);
  EXPECT_EQ(0, listener->error_count);
}

TEST_F(MucRoomConfigTaskTest, TestError) {
  std::vector<std::string> room_features;
  buzz::MucRoomConfigTask* task = new buzz::MucRoomConfigTask(
      xmpp_client, room_jid, "ponies", room_features);
  task->SignalError.connect(listener, &MucRoomConfigListener::OnError);
  task->Start();

  std::string error_iq =
      "<iq xmlns='jabber:client' id='0' type='error'"
      " from='muc-jid-ponies@domain.com'>"
      "</iq>";

  xmpp_client->HandleStanza(buzz::XmlElement::ForStr(error_iq));

  EXPECT_EQ(0, listener->result_count);
  EXPECT_EQ(1, listener->error_count);
}
