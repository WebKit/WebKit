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
#include "webrtc/libjingle/xmpp/mucroomuniquehangoutidtask.h"
#include "webrtc/base/faketaskrunner.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/sigslot.h"

class MucRoomUniqueHangoutIdListener : public sigslot::has_slots<> {
 public:
  MucRoomUniqueHangoutIdListener() : error_count(0) {}

  void OnResult(buzz::MucRoomUniqueHangoutIdTask* task,
                const std::string& hangout_id) {
    last_hangout_id = hangout_id;
  }

  void OnError(buzz::IqTask* task,
               const buzz::XmlElement* error) {
    ++error_count;
  }

  std::string last_hangout_id;
  int error_count;
};

class MucRoomUniqueHangoutIdTaskTest : public testing::Test {
 public:
  MucRoomUniqueHangoutIdTaskTest() :
      lookup_server_jid("lookup@domain.com"),
      hangout_id("some_hangout_id") {
  }

  virtual void SetUp() {
    runner = new rtc::FakeTaskRunner();
    xmpp_client = new buzz::FakeXmppClient(runner);
    listener = new MucRoomUniqueHangoutIdListener();
  }

  virtual void TearDown() {
    delete listener;
    // delete xmpp_client;  Deleted by deleting runner.
    delete runner;
  }

  rtc::FakeTaskRunner* runner;
  buzz::FakeXmppClient* xmpp_client;
  MucRoomUniqueHangoutIdListener* listener;
  buzz::Jid lookup_server_jid;
  std::string hangout_id;
};

TEST_F(MucRoomUniqueHangoutIdTaskTest, Test) {
  ASSERT_EQ(0U, xmpp_client->sent_stanzas().size());

  buzz::MucRoomUniqueHangoutIdTask* task = new buzz::MucRoomUniqueHangoutIdTask(
      xmpp_client, lookup_server_jid);
  task->SignalResult.connect(listener, &MucRoomUniqueHangoutIdListener::OnResult);
  task->Start();

  std::string expected_iq =
      "<cli:iq type=\"get\" to=\"lookup@domain.com\" id=\"0\" "
          "xmlns:cli=\"jabber:client\">"
        "<uni:unique hangout-id=\"true\" "
          "xmlns:uni=\"http://jabber.org/protocol/muc#unique\"/>"
      "</cli:iq>";

  ASSERT_EQ(1U, xmpp_client->sent_stanzas().size());
  EXPECT_EQ(expected_iq, xmpp_client->sent_stanzas()[0]->Str());

  EXPECT_EQ("", listener->last_hangout_id);

  std::string response_iq =
      "<iq xmlns='jabber:client' from='lookup@domain.com' id='0' type='result'>"
        "<unique hangout-id=\"some_hangout_id\" "
            "xmlns=\"http://jabber.org/protocol/muc#unique\">"
          "muvc-private-chat-00001234-5678-9abc-def0-123456789abc"
        "</unique>"
      "</iq>";

  xmpp_client->HandleStanza(buzz::XmlElement::ForStr(response_iq));

  EXPECT_EQ(hangout_id, listener->last_hangout_id);
  EXPECT_EQ(0, listener->error_count);
}

