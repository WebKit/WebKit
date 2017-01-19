/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <string>

#include "webrtc/libjingle/xmllite/qname.h"
#include "webrtc/libjingle/xmllite/xmlelement.h"
#include "webrtc/libjingle/xmpp/constants.h"
#include "webrtc/libjingle/xmpp/fakexmppclient.h"
#include "webrtc/libjingle/xmpp/iqtask.h"
#include "webrtc/libjingle/xmpp/jid.h"
#include "webrtc/libjingle/xmpp/pubsubtasks.h"
#include "webrtc/base/faketaskrunner.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/sigslot.h"

struct HandledPubSubItem {
  std::string itemid;
  std::string payload;
};

class TestPubSubTasksListener : public sigslot::has_slots<> {
 public:
  TestPubSubTasksListener() : result_count(0), error_count(0) {}

  void OnReceiveUpdate(buzz::PubSubReceiveTask* task,
                       const std::vector<buzz::PubSubItem>& items) {
    OnItems(items);
  }

  void OnRequestResult(buzz::PubSubRequestTask* task,
                       const std::vector<buzz::PubSubItem>& items) {
    OnItems(items);
  }

  void OnItems(const std::vector<buzz::PubSubItem>& items) {
    for (std::vector<buzz::PubSubItem>::const_iterator item = items.begin();
         item != items.end(); ++item) {
      HandledPubSubItem handled_item;
      handled_item.itemid = item->itemid;
      if (item->elem->FirstElement() != NULL) {
        handled_item.payload = item->elem->FirstElement()->Str();
      }
      this->items.push_back(handled_item);
    }
  }

  void OnPublishResult(buzz::PubSubPublishTask* task) {
    ++result_count;
  }

  void OnRetractResult(buzz::PubSubRetractTask* task) {
    ++result_count;
  }

  void OnError(buzz::IqTask* task, const buzz::XmlElement* stanza) {
    ++error_count;
  }

  std::vector<HandledPubSubItem> items;
  int result_count;
  int error_count;
};

class PubSubTasksTest : public testing::Test {
 public:
  PubSubTasksTest() :
      pubsubjid("room@domain.com"),
      node("topic"),
      itemid("key") {
    runner.reset(new rtc::FakeTaskRunner());
    client = new buzz::FakeXmppClient(runner.get());
    listener.reset(new TestPubSubTasksListener());
  }

  std::unique_ptr<rtc::FakeTaskRunner> runner;
  // Client deleted by deleting runner.
  buzz::FakeXmppClient* client;
  std::unique_ptr<TestPubSubTasksListener> listener;
  buzz::Jid pubsubjid;
  std::string node;
  std::string itemid;
};

TEST_F(PubSubTasksTest, TestRequest) {
  buzz::PubSubRequestTask* task =
      new buzz::PubSubRequestTask(client, pubsubjid, node);
  task->SignalResult.connect(
      listener.get(), &TestPubSubTasksListener::OnRequestResult);
  task->Start();

  std::string expected_iq =
      "<cli:iq type=\"get\" to=\"room@domain.com\" id=\"0\" "
        "xmlns:cli=\"jabber:client\">"
        "<pub:pubsub xmlns:pub=\"http://jabber.org/protocol/pubsub\">"
          "<pub:items node=\"topic\"/>"
        "</pub:pubsub>"
      "</cli:iq>";

  ASSERT_EQ(1U, client->sent_stanzas().size());
  EXPECT_EQ(expected_iq, client->sent_stanzas()[0]->Str());

  std::string result_iq =
      "<iq xmlns='jabber:client' id='0' type='result' from='room@domain.com'>"
      "  <pubsub xmlns='http://jabber.org/protocol/pubsub'>"
      "    <items node='topic'>"
      "      <item id='key0'>"
      "        <value0/>"
      "      </item>"
      "      <item id='key1'>"
      "        <value1/>"
      "      </item>"
      "    </items>"
      "  </pubsub>"
      "</iq>";

  client->HandleStanza(buzz::XmlElement::ForStr(result_iq));

  ASSERT_EQ(2U, listener->items.size());
  EXPECT_EQ("key0", listener->items[0].itemid);
  EXPECT_EQ("<pub:value0 xmlns:pub=\"http://jabber.org/protocol/pubsub\"/>",
            listener->items[0].payload);
  EXPECT_EQ("key1", listener->items[1].itemid);
  EXPECT_EQ("<pub:value1 xmlns:pub=\"http://jabber.org/protocol/pubsub\"/>",
            listener->items[1].payload);
}

TEST_F(PubSubTasksTest, TestRequestError) {
  std::string result_iq =
      "<iq xmlns='jabber:client' id='0' type='error' from='room@domain.com'>"
      "  <error type='auth'>"
      "    <forbidden xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/>"
      "  </error>"
      "</iq>";

  buzz::PubSubRequestTask* task =
      new buzz::PubSubRequestTask(client, pubsubjid, node);
  task->SignalResult.connect(
      listener.get(), &TestPubSubTasksListener::OnRequestResult);
  task->SignalError.connect(
      listener.get(), &TestPubSubTasksListener::OnError);
  task->Start();
  client->HandleStanza(buzz::XmlElement::ForStr(result_iq));

  EXPECT_EQ(0, listener->result_count);
  EXPECT_EQ(1, listener->error_count);
}

TEST_F(PubSubTasksTest, TestReceive) {
  std::string items_message =
      "<message xmlns='jabber:client' from='room@domain.com'>"
      "  <event xmlns='http://jabber.org/protocol/pubsub#event'>"
      "    <items node='topic'>"
      "      <item id='key0'>"
      "        <value0/>"
      "      </item>"
      "      <item id='key1'>"
      "        <value1/>"
      "      </item>"
      "    </items>"
      "  </event>"
      "</message>";

  buzz::PubSubReceiveTask* task =
      new buzz::PubSubReceiveTask(client, pubsubjid, node);
  task->SignalUpdate.connect(
      listener.get(), &TestPubSubTasksListener::OnReceiveUpdate);
  task->Start();
  client->HandleStanza(buzz::XmlElement::ForStr(items_message));

  ASSERT_EQ(2U, listener->items.size());
  EXPECT_EQ("key0", listener->items[0].itemid);
  EXPECT_EQ(
      "<eve:value0 xmlns:eve=\"http://jabber.org/protocol/pubsub#event\"/>",
      listener->items[0].payload);
  EXPECT_EQ("key1", listener->items[1].itemid);
  EXPECT_EQ(
      "<eve:value1 xmlns:eve=\"http://jabber.org/protocol/pubsub#event\"/>",
      listener->items[1].payload);
}

TEST_F(PubSubTasksTest, TestPublish) {
  buzz::XmlElement* payload =
      new buzz::XmlElement(buzz::QName(buzz::NS_PUBSUB, "value"));
  std::string expected_iq =
      "<cli:iq type=\"set\" to=\"room@domain.com\" id=\"0\" "
        "xmlns:cli=\"jabber:client\">"
        "<pubsub xmlns=\"http://jabber.org/protocol/pubsub\">"
          "<publish node=\"topic\">"
            "<item id=\"key\">"
              "<value/>"
            "</item>"
          "</publish>"
        "</pubsub>"
      "</cli:iq>";

  std::vector<buzz::XmlElement*> children;
  children.push_back(payload);
  buzz::PubSubPublishTask* task =
      new buzz::PubSubPublishTask(client, pubsubjid, node, itemid, children);
  task->SignalResult.connect(
      listener.get(), &TestPubSubTasksListener::OnPublishResult);
  task->Start();

  ASSERT_EQ(1U, client->sent_stanzas().size());
  EXPECT_EQ(expected_iq, client->sent_stanzas()[0]->Str());

  std::string result_iq =
      "<iq xmlns='jabber:client' id='0' type='result' from='room@domain.com'/>";

  client->HandleStanza(buzz::XmlElement::ForStr(result_iq));

  EXPECT_EQ(1, listener->result_count);
  EXPECT_EQ(0, listener->error_count);
}

TEST_F(PubSubTasksTest, TestPublishError) {
  buzz::XmlElement* payload =
      new buzz::XmlElement(buzz::QName(buzz::NS_PUBSUB, "value"));

  std::vector<buzz::XmlElement*> children;
  children.push_back(payload);
  buzz::PubSubPublishTask* task =
      new buzz::PubSubPublishTask(client, pubsubjid, node, itemid, children);
  task->SignalResult.connect(
      listener.get(), &TestPubSubTasksListener::OnPublishResult);
  task->SignalError.connect(
      listener.get(), &TestPubSubTasksListener::OnError);
  task->Start();

  std::string result_iq =
      "<iq xmlns='jabber:client' id='0' type='error' from='room@domain.com'>"
      "  <error type='auth'>"
      "    <forbidden xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/>"
      "  </error>"
      "</iq>";

  client->HandleStanza(buzz::XmlElement::ForStr(result_iq));

  EXPECT_EQ(0, listener->result_count);
  EXPECT_EQ(1, listener->error_count);
}

TEST_F(PubSubTasksTest, TestRetract) {
  buzz::PubSubRetractTask* task =
      new buzz::PubSubRetractTask(client, pubsubjid, node, itemid);
  task->SignalResult.connect(
      listener.get(), &TestPubSubTasksListener::OnRetractResult);
  task->SignalError.connect(
      listener.get(), &TestPubSubTasksListener::OnError);
  task->Start();

  std::string expected_iq =
      "<cli:iq type=\"set\" to=\"room@domain.com\" id=\"0\" "
        "xmlns:cli=\"jabber:client\">"
        "<pubsub xmlns=\"http://jabber.org/protocol/pubsub\">"
          "<retract node=\"topic\" notify=\"true\">"
            "<item id=\"key\"/>"
          "</retract>"
        "</pubsub>"
      "</cli:iq>";

  ASSERT_EQ(1U, client->sent_stanzas().size());
  EXPECT_EQ(expected_iq, client->sent_stanzas()[0]->Str());

  std::string result_iq =
      "<iq xmlns='jabber:client' id='0' type='result' from='room@domain.com'/>";

  client->HandleStanza(buzz::XmlElement::ForStr(result_iq));

  EXPECT_EQ(1, listener->result_count);
  EXPECT_EQ(0, listener->error_count);
}
