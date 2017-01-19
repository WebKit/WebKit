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
#include "webrtc/libjingle/xmpp/jid.h"
#include "webrtc/libjingle/xmpp/pubsubclient.h"
#include "webrtc/base/faketaskrunner.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/sigslot.h"

struct HandledPubSubItem {
  std::string itemid;
  std::string payload;
};

class TestPubSubItemsListener : public sigslot::has_slots<> {
 public:
  TestPubSubItemsListener() : error_count(0) {}

  void OnItems(buzz::PubSubClient*,
               const std::vector<buzz::PubSubItem>& items) {
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

  void OnRequestError(buzz::PubSubClient* client,
                      const buzz::XmlElement* stanza) {
    error_count++;
  }

  void OnPublishResult(buzz::PubSubClient* client,
                       const std::string& task_id,
                       const buzz::XmlElement* item) {
    result_task_id = task_id;
  }

  void OnPublishError(buzz::PubSubClient* client,
                      const std::string& task_id,
                      const buzz::XmlElement* item,
                      const buzz::XmlElement* stanza) {
    error_count++;
    error_task_id = task_id;
  }

  void OnRetractResult(buzz::PubSubClient* client,
                       const std::string& task_id) {
    result_task_id = task_id;
  }

  void OnRetractError(buzz::PubSubClient* client,
                      const std::string& task_id,
                      const buzz::XmlElement* stanza) {
    error_count++;
    error_task_id = task_id;
  }

  std::vector<HandledPubSubItem> items;
  int error_count;
  std::string error_task_id;
  std::string result_task_id;
};

class PubSubClientTest : public testing::Test {
 public:
  PubSubClientTest() :
      pubsubjid("room@domain.com"),
      node("topic"),
      itemid("key") {
    runner.reset(new rtc::FakeTaskRunner());
    xmpp_client = new buzz::FakeXmppClient(runner.get());
    client.reset(new buzz::PubSubClient(xmpp_client, pubsubjid, node));
    listener.reset(new TestPubSubItemsListener());
    client->SignalItems.connect(
        listener.get(), &TestPubSubItemsListener::OnItems);
    client->SignalRequestError.connect(
        listener.get(), &TestPubSubItemsListener::OnRequestError);
    client->SignalPublishResult.connect(
        listener.get(), &TestPubSubItemsListener::OnPublishResult);
    client->SignalPublishError.connect(
        listener.get(), &TestPubSubItemsListener::OnPublishError);
    client->SignalRetractResult.connect(
        listener.get(), &TestPubSubItemsListener::OnRetractResult);
    client->SignalRetractError.connect(
        listener.get(), &TestPubSubItemsListener::OnRetractError);
  }

  std::unique_ptr<rtc::FakeTaskRunner> runner;
  // xmpp_client deleted by deleting runner.
  buzz::FakeXmppClient* xmpp_client;
  std::unique_ptr<buzz::PubSubClient> client;
  std::unique_ptr<TestPubSubItemsListener> listener;
  buzz::Jid pubsubjid;
  std::string node;
  std::string itemid;
};

TEST_F(PubSubClientTest, TestRequest) {
  client->RequestItems();

  std::string expected_iq =
      "<cli:iq type=\"get\" to=\"room@domain.com\" id=\"0\" "
        "xmlns:cli=\"jabber:client\">"
        "<pub:pubsub xmlns:pub=\"http://jabber.org/protocol/pubsub\">"
          "<pub:items node=\"topic\"/>"
        "</pub:pubsub>"
      "</cli:iq>";

  ASSERT_EQ(1U, xmpp_client->sent_stanzas().size());
  EXPECT_EQ(expected_iq, xmpp_client->sent_stanzas()[0]->Str());

  std::string result_iq =
      "<iq xmlns='jabber:client' id='0' type='result' from='room@domain.com'>"
      "  <pubsub xmlns='http://jabber.org/protocol/pubsub'>"
      "    <items node='topic'>"
      "      <item id='key0'>"
      "        <value0a/>"
      "      </item>"
      "      <item id='key1'>"
      "        <value1a/>"
      "      </item>"
      "    </items>"
      "  </pubsub>"
      "</iq>";

  xmpp_client->HandleStanza(buzz::XmlElement::ForStr(result_iq));
  ASSERT_EQ(2U, listener->items.size());
  EXPECT_EQ("key0", listener->items[0].itemid);
  EXPECT_EQ("<pub:value0a xmlns:pub=\"http://jabber.org/protocol/pubsub\"/>",
            listener->items[0].payload);
  EXPECT_EQ("key1", listener->items[1].itemid);
  EXPECT_EQ("<pub:value1a xmlns:pub=\"http://jabber.org/protocol/pubsub\"/>",
            listener->items[1].payload);

  std::string items_message =
      "<message xmlns='jabber:client' from='room@domain.com'>"
      "  <event xmlns='http://jabber.org/protocol/pubsub#event'>"
      "    <items node='topic'>"
      "      <item id='key0'>"
      "        <value0b/>"
      "      </item>"
      "      <item id='key1'>"
      "        <value1b/>"
      "      </item>"
      "    </items>"
      "  </event>"
      "</message>";

  xmpp_client->HandleStanza(buzz::XmlElement::ForStr(items_message));
  ASSERT_EQ(4U, listener->items.size());
  EXPECT_EQ("key0", listener->items[2].itemid);
  EXPECT_EQ("<eve:value0b"
            " xmlns:eve=\"http://jabber.org/protocol/pubsub#event\"/>",
            listener->items[2].payload);
  EXPECT_EQ("key1", listener->items[3].itemid);
  EXPECT_EQ("<eve:value1b"
            " xmlns:eve=\"http://jabber.org/protocol/pubsub#event\"/>",
            listener->items[3].payload);
}

TEST_F(PubSubClientTest, TestRequestError) {
  std::string result_iq =
      "<iq xmlns='jabber:client' id='0' type='error' from='room@domain.com'>"
      "  <error type='auth'>"
      "    <forbidden xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/>"
      "  </error>"
      "</iq>";

  client->RequestItems();
  xmpp_client->HandleStanza(buzz::XmlElement::ForStr(result_iq));
  EXPECT_EQ(1, listener->error_count);
}

TEST_F(PubSubClientTest, TestPublish) {
  buzz::XmlElement* payload =
      new buzz::XmlElement(buzz::QName(buzz::NS_PUBSUB, "value"));

  std::string task_id;
  client->PublishItem(itemid, payload, &task_id);

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

  ASSERT_EQ(1U, xmpp_client->sent_stanzas().size());
  EXPECT_EQ(expected_iq, xmpp_client->sent_stanzas()[0]->Str());

  std::string result_iq =
      "<iq xmlns='jabber:client' id='0' type='result' from='room@domain.com'/>";

  xmpp_client->HandleStanza(buzz::XmlElement::ForStr(result_iq));
  EXPECT_EQ(task_id, listener->result_task_id);
}

TEST_F(PubSubClientTest, TestPublishError) {
  buzz::XmlElement* payload =
      new buzz::XmlElement(buzz::QName(buzz::NS_PUBSUB, "value"));

  std::string task_id;
  client->PublishItem(itemid, payload, &task_id);

  std::string result_iq =
      "<iq xmlns='jabber:client' id='0' type='error' from='room@domain.com'>"
      "  <error type='auth'>"
      "    <forbidden xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/>"
      "  </error>"
      "</iq>";

  xmpp_client->HandleStanza(buzz::XmlElement::ForStr(result_iq));
  EXPECT_EQ(1, listener->error_count);
  EXPECT_EQ(task_id, listener->error_task_id);
}

TEST_F(PubSubClientTest, TestRetract) {
  std::string task_id;
  client->RetractItem(itemid, &task_id);

  std::string expected_iq =
      "<cli:iq type=\"set\" to=\"room@domain.com\" id=\"0\" "
        "xmlns:cli=\"jabber:client\">"
        "<pubsub xmlns=\"http://jabber.org/protocol/pubsub\">"
          "<retract node=\"topic\" notify=\"true\">"
            "<item id=\"key\"/>"
          "</retract>"
        "</pubsub>"
      "</cli:iq>";

  ASSERT_EQ(1U, xmpp_client->sent_stanzas().size());
  EXPECT_EQ(expected_iq, xmpp_client->sent_stanzas()[0]->Str());

  std::string result_iq =
      "<iq xmlns='jabber:client' id='0' type='result' from='room@domain.com'/>";

  xmpp_client->HandleStanza(buzz::XmlElement::ForStr(result_iq));
  EXPECT_EQ(task_id, listener->result_task_id);
}

TEST_F(PubSubClientTest, TestRetractError) {
  std::string task_id;
  client->RetractItem(itemid, &task_id);

  std::string result_iq =
      "<iq xmlns='jabber:client' id='0' type='error' from='room@domain.com'>"
      "  <error type='auth'>"
      "    <forbidden xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/>"
      "  </error>"
      "</iq>";

  xmpp_client->HandleStanza(buzz::XmlElement::ForStr(result_iq));
  EXPECT_EQ(1, listener->error_count);
  EXPECT_EQ(task_id, listener->error_task_id);
}
