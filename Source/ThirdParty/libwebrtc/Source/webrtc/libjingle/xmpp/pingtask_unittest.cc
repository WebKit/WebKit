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
#include "webrtc/libjingle/xmpp/pingtask.h"
#include "webrtc/base/faketaskrunner.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/sigslot.h"

class PingTaskTest;

class PingXmppClient : public buzz::FakeXmppClient {
 public:
  PingXmppClient(rtc::TaskParent* parent, PingTaskTest* tst) :
      FakeXmppClient(parent), test(tst) {
  }

  buzz::XmppReturnStatus SendStanza(const buzz::XmlElement* stanza);

 private:
  PingTaskTest* test;
};

class PingTaskTest : public testing::Test, public sigslot::has_slots<> {
 public:
  PingTaskTest() : respond_to_pings(true), timed_out(false) {
  }

  virtual void SetUp() {
    runner = new rtc::FakeTaskRunner();
    xmpp_client = new PingXmppClient(runner, this);
  }

  virtual void TearDown() {
    // delete xmpp_client;  Deleted by deleting runner.
    delete runner;
  }

  void ConnectTimeoutSignal(buzz::PingTask* task) {
    task->SignalTimeout.connect(this, &PingTaskTest::OnPingTimeout);
  }

  void OnPingTimeout() {
    timed_out = true;
  }

  rtc::FakeTaskRunner* runner;
  PingXmppClient* xmpp_client;
  bool respond_to_pings;
  bool timed_out;
};

buzz::XmppReturnStatus PingXmppClient::SendStanza(
    const buzz::XmlElement* stanza) {
  buzz::XmppReturnStatus result = FakeXmppClient::SendStanza(stanza);
  if (test->respond_to_pings && (stanza->FirstNamed(buzz::QN_PING) != NULL)) {
    std::string ping_response =
        "<iq xmlns=\'jabber:client\' id='0' type='result'/>";
    HandleStanza(buzz::XmlElement::ForStr(ping_response));
  }
  return result;
}

TEST_F(PingTaskTest, TestSuccess) {
  uint32_t ping_period_millis = 100;
  buzz::PingTask* task = new buzz::PingTask(xmpp_client,
      rtc::Thread::Current(),
      ping_period_millis, ping_period_millis / 10);
  ConnectTimeoutSignal(task);
  task->Start();
  unsigned int expected_ping_count = 5U;
  EXPECT_EQ_WAIT(xmpp_client->sent_stanzas().size(), expected_ping_count,
                 ping_period_millis * (expected_ping_count + 1));
  EXPECT_FALSE(task->IsDone());
  EXPECT_FALSE(timed_out);
}

TEST_F(PingTaskTest, TestTimeout) {
  respond_to_pings = false;
  uint32_t ping_timeout_millis = 200;
  buzz::PingTask* task = new buzz::PingTask(xmpp_client,
      rtc::Thread::Current(),
      ping_timeout_millis * 10, ping_timeout_millis);
  ConnectTimeoutSignal(task);
  task->Start();
  WAIT(false, ping_timeout_millis / 2);
  EXPECT_FALSE(timed_out);
  EXPECT_TRUE_WAIT(timed_out, ping_timeout_millis * 2);
}
