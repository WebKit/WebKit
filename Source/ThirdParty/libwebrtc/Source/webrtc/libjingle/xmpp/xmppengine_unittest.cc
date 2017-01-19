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
#include "webrtc/libjingle/xmpp/plainsaslhandler.h"
#include "webrtc/libjingle/xmpp/saslplainmechanism.h"
#include "webrtc/libjingle/xmpp/util_unittest.h"
#include "webrtc/libjingle/xmpp/xmppengine.h"
#include "webrtc/base/common.h"
#include "webrtc/base/gunit.h"

using buzz::Jid;
using buzz::QName;
using buzz::XmlElement;
using buzz::XmppEngine;
using buzz::XmppIqCookie;
using buzz::XmppIqHandler;
using buzz::XmppTestHandler;
using buzz::QN_ID;
using buzz::QN_IQ;
using buzz::QN_TYPE;
using buzz::QN_ROSTER_QUERY;
using buzz::XMPP_RETURN_OK;
using buzz::XMPP_RETURN_BADARGUMENT;

// XmppEngineTestIqHandler
//    This class grabs the response to an IQ stanza and stores it in a string.
class XmppEngineTestIqHandler : public XmppIqHandler {
 public:
  virtual void IqResponse(XmppIqCookie, const XmlElement * stanza) {
    ss_ << stanza->Str();
  }

  std::string IqResponseActivity() {
    std::string result = ss_.str();
    ss_.str("");
    return result;
  }

 private:
  std::stringstream ss_;
};

class XmppEngineTest : public testing::Test {
 public:
  XmppEngine* engine() { return engine_.get(); }
  XmppTestHandler* handler() { return handler_.get(); }
  virtual void SetUp() {
    engine_.reset(XmppEngine::Create());
    handler_.reset(new XmppTestHandler(engine_.get()));

    Jid jid("david@my-server");
    rtc::InsecureCryptStringImpl pass;
    pass.password() = "david";
    engine_->SetSessionHandler(handler_.get());
    engine_->SetOutputHandler(handler_.get());
    engine_->AddStanzaHandler(handler_.get());
    engine_->SetUser(jid);
    engine_->SetSaslHandler(
        new buzz::PlainSaslHandler(jid, rtc::CryptString(pass), true));
  }
  virtual void TearDown() {
    handler_.reset();
    engine_.reset();
  }
  void RunLogin();

 private:
  std::unique_ptr<XmppEngine> engine_;
  std::unique_ptr<XmppTestHandler> handler_;
};

void XmppEngineTest::RunLogin() {
  // Connect
  EXPECT_EQ(XmppEngine::STATE_START, engine()->GetState());
  engine()->Connect();
  EXPECT_EQ(XmppEngine::STATE_OPENING, engine()->GetState());

  EXPECT_EQ("[OPENING]", handler_->SessionActivity());

  EXPECT_EQ("<stream:stream to=\"my-server\" xml:lang=\"*\" version=\"1.0\" "
           "xmlns:stream=\"http://etherx.jabber.org/streams\" "
           "xmlns=\"jabber:client\">\r\n", handler_->OutputActivity());

  std::string input =
    "<stream:stream id=\"a5f2d8c9\" version=\"1.0\" "
    "xmlns:stream=\"http://etherx.jabber.org/streams\" "
    "xmlns=\"jabber:client\">";
  engine()->HandleInput(input.c_str(), input.length());

  input =
    "<stream:features>"
      "<starttls xmlns='urn:ietf:params:xml:ns:xmpp-tls'>"
        "<required/>"
      "</starttls>"
      "<mechanisms xmlns='urn:ietf:params:xml:ns:xmpp-sasl'>"
        "<mechanism>DIGEST-MD5</mechanism>"
        "<mechanism>PLAIN</mechanism>"
      "</mechanisms>"
    "</stream:features>";
  engine()->HandleInput(input.c_str(), input.length());
  EXPECT_EQ("<starttls xmlns=\"urn:ietf:params:xml:ns:xmpp-tls\"/>",
      handler_->OutputActivity());

  EXPECT_EQ("", handler_->SessionActivity());
  EXPECT_EQ("", handler_->StanzaActivity());

  input = "<proceed xmlns='urn:ietf:params:xml:ns:xmpp-tls'/>";
  engine()->HandleInput(input.c_str(), input.length());
  EXPECT_EQ("[START-TLS my-server]"
           "<stream:stream to=\"my-server\" xml:lang=\"*\" "
           "version=\"1.0\" xmlns:stream=\"http://etherx.jabber.org/streams\" "
           "xmlns=\"jabber:client\">\r\n", handler_->OutputActivity());

  EXPECT_EQ("", handler_->SessionActivity());
  EXPECT_EQ("", handler_->StanzaActivity());

  input = "<stream:stream id=\"01234567\" version=\"1.0\" "
          "xmlns:stream=\"http://etherx.jabber.org/streams\" "
          "xmlns=\"jabber:client\">";
  engine()->HandleInput(input.c_str(), input.length());

  input =
    "<stream:features>"
      "<mechanisms xmlns='urn:ietf:params:xml:ns:xmpp-sasl'>"
        "<mechanism>DIGEST-MD5</mechanism>"
        "<mechanism>PLAIN</mechanism>"
      "</mechanisms>"
    "</stream:features>";
  engine()->HandleInput(input.c_str(), input.length());
  EXPECT_EQ("<auth xmlns=\"urn:ietf:params:xml:ns:xmpp-sasl\" "
      "mechanism=\"PLAIN\" "
      "auth:allow-non-google-login=\"true\" "
      "auth:client-uses-full-bind-result=\"true\" "
      "xmlns:auth=\"http://www.google.com/talk/protocol/auth\""
      ">AGRhdmlkAGRhdmlk</auth>",
      handler_->OutputActivity());

  EXPECT_EQ("", handler_->SessionActivity());
  EXPECT_EQ("", handler_->StanzaActivity());

  input = "<success xmlns='urn:ietf:params:xml:ns:xmpp-sasl'/>";
  engine()->HandleInput(input.c_str(), input.length());
  EXPECT_EQ("<stream:stream to=\"my-server\" xml:lang=\"*\" version=\"1.0\" "
      "xmlns:stream=\"http://etherx.jabber.org/streams\" "
      "xmlns=\"jabber:client\">\r\n", handler_->OutputActivity());

  EXPECT_EQ("", handler_->SessionActivity());
  EXPECT_EQ("", handler_->StanzaActivity());

  input = "<stream:stream id=\"01234567\" version=\"1.0\" "
      "xmlns:stream=\"http://etherx.jabber.org/streams\" "
      "xmlns=\"jabber:client\">";
  engine()->HandleInput(input.c_str(), input.length());

  input = "<stream:features>"
          "<bind xmlns='urn:ietf:params:xml:ns:xmpp-bind'/>"
          "<session xmlns='urn:ietf:params:xml:ns:xmpp-session'/>"
          "</stream:features>";
  engine()->HandleInput(input.c_str(), input.length());
  EXPECT_EQ("<iq type=\"set\" id=\"0\">"
           "<bind xmlns=\"urn:ietf:params:xml:ns:xmpp-bind\"/></iq>",
           handler_->OutputActivity());

  EXPECT_EQ("", handler_->SessionActivity());
  EXPECT_EQ("", handler_->StanzaActivity());

  input = "<iq type='result' id='0'>"
          "<bind xmlns='urn:ietf:params:xml:ns:xmpp-bind'><jid>"
          "david@my-server/test</jid></bind></iq>";
  engine()->HandleInput(input.c_str(), input.length());

  EXPECT_EQ("<iq type=\"set\" id=\"1\">"
           "<session xmlns=\"urn:ietf:params:xml:ns:xmpp-session\"/></iq>",
           handler_->OutputActivity());

  EXPECT_EQ("", handler_->SessionActivity());
  EXPECT_EQ("", handler_->StanzaActivity());

  input = "<iq type='result' id='1'/>";
  engine()->HandleInput(input.c_str(), input.length());

  EXPECT_EQ("[OPEN]", handler_->SessionActivity());
  EXPECT_EQ("", handler_->StanzaActivity());
  EXPECT_EQ(Jid("david@my-server/test"), engine()->FullJid());
}

// TestSuccessfulLogin()
//    This function simply tests to see if a login works.  This includes
//    encryption and authentication
TEST_F(XmppEngineTest, TestSuccessfulLoginAndDisconnect) {
  RunLogin();
  engine()->Disconnect();
  EXPECT_EQ("</stream:stream>[CLOSED]", handler()->OutputActivity());
  EXPECT_EQ("[CLOSED]", handler()->SessionActivity());
  EXPECT_EQ("", handler()->StanzaActivity());
}

TEST_F(XmppEngineTest, TestSuccessfulLoginAndConnectionClosed) {
  RunLogin();
  engine()->ConnectionClosed(0);
  EXPECT_EQ("[CLOSED]", handler()->OutputActivity());
  EXPECT_EQ("[CLOSED][ERROR-CONNECTION-CLOSED]", handler()->SessionActivity());
  EXPECT_EQ("", handler()->StanzaActivity());
}


// TestNotXmpp()
//    This tests the error case when connecting to a non XMPP service
TEST_F(XmppEngineTest, TestNotXmpp) {
  // Connect
  engine()->Connect();
  EXPECT_EQ("<stream:stream to=\"my-server\" xml:lang=\"*\" version=\"1.0\" "
          "xmlns:stream=\"http://etherx.jabber.org/streams\" "
          "xmlns=\"jabber:client\">\r\n", handler()->OutputActivity());

  // Send garbage response (courtesy of apache)
  std::string input = "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">";
  engine()->HandleInput(input.c_str(), input.length());

  EXPECT_EQ("[CLOSED]", handler()->OutputActivity());
  EXPECT_EQ("[OPENING][CLOSED][ERROR-XML]", handler()->SessionActivity());
  EXPECT_EQ("", handler()->StanzaActivity());
}

// TestPassthrough()
//    This tests that arbitrary stanzas can be passed to the server through
//    the engine.
TEST_F(XmppEngineTest, TestPassthrough) {
  // Queue up an app stanza
  XmlElement application_stanza(QName("test", "app-stanza"));
  application_stanza.AddText("this-is-a-test");
  engine()->SendStanza(&application_stanza);

  // Do the whole login handshake
  RunLogin();

  EXPECT_EQ("<test:app-stanza xmlns:test=\"test\">this-is-a-test"
          "</test:app-stanza>", handler()->OutputActivity());

  // do another stanza
  XmlElement roster_get(QN_IQ);
  roster_get.AddAttr(QN_TYPE, "get");
  roster_get.AddAttr(QN_ID, engine()->NextId());
  roster_get.AddElement(new XmlElement(QN_ROSTER_QUERY, true));
  engine()->SendStanza(&roster_get);
  EXPECT_EQ("<iq type=\"get\" id=\"2\"><query xmlns=\"jabber:iq:roster\"/>"
          "</iq>", handler()->OutputActivity());

  // now say the server ends the stream
  engine()->HandleInput("</stream:stream>", 16);
  EXPECT_EQ("[CLOSED][ERROR-DOCUMENT-CLOSED]", handler()->SessionActivity());
  EXPECT_EQ("[CLOSED]", handler()->OutputActivity());
  EXPECT_EQ("", handler()->StanzaActivity());
}

// TestIqCallback()
//    This tests the routing of Iq stanzas and responses.
TEST_F(XmppEngineTest, TestIqCallback) {
  XmppEngineTestIqHandler iq_response;
  XmppIqCookie cookie;

  // Do the whole login handshake
  RunLogin();

  // Build an iq request
  XmlElement roster_get(QN_IQ);
  roster_get.AddAttr(QN_TYPE, "get");
  roster_get.AddAttr(QN_ID, engine()->NextId());
  roster_get.AddElement(new XmlElement(QN_ROSTER_QUERY, true));
  engine()->SendIq(&roster_get, &iq_response, &cookie);
  EXPECT_EQ("<iq type=\"get\" id=\"2\"><query xmlns=\"jabber:iq:roster\"/>"
          "</iq>", handler()->OutputActivity());
  EXPECT_EQ("", handler()->SessionActivity());
  EXPECT_EQ("", handler()->StanzaActivity());
  EXPECT_EQ("", iq_response.IqResponseActivity());

  // now say the server responds to the iq
  std::string input = "<iq type='result' id='2'>"
                      "<query xmlns='jabber:iq:roster'><item>foo</item>"
                      "</query></iq>";
  engine()->HandleInput(input.c_str(), input.length());
  EXPECT_EQ("", handler()->OutputActivity());
  EXPECT_EQ("", handler()->SessionActivity());
  EXPECT_EQ("", handler()->StanzaActivity());
  EXPECT_EQ("<cli:iq type=\"result\" id=\"2\" xmlns:cli=\"jabber:client\">"
          "<query xmlns=\"jabber:iq:roster\"><item>foo</item></query>"
          "</cli:iq>", iq_response.IqResponseActivity());

  EXPECT_EQ(XMPP_RETURN_BADARGUMENT, engine()->RemoveIqHandler(cookie, NULL));

  // Do it again with another id to test cancel
  roster_get.SetAttr(QN_ID, engine()->NextId());
  engine()->SendIq(&roster_get, &iq_response, &cookie);
  EXPECT_EQ("<iq type=\"get\" id=\"3\"><query xmlns=\"jabber:iq:roster\"/>"
          "</iq>", handler()->OutputActivity());
  EXPECT_EQ("", handler()->SessionActivity());
  EXPECT_EQ("", handler()->StanzaActivity());
  EXPECT_EQ("", iq_response.IqResponseActivity());

  // cancel the handler this time
  EXPECT_EQ(XMPP_RETURN_OK, engine()->RemoveIqHandler(cookie, NULL));

  // now say the server responds to the iq: the iq handler should not get it.
  input = "<iq type='result' id='3'><query xmlns='jabber:iq:roster'><item>bar"
          "</item></query></iq>";
  engine()->HandleInput(input.c_str(), input.length());
  EXPECT_EQ("<cli:iq type=\"result\" id=\"3\" xmlns:cli=\"jabber:client\">"
          "<query xmlns=\"jabber:iq:roster\"><item>bar</item></query>"
          "</cli:iq>", handler()->StanzaActivity());
  EXPECT_EQ("", iq_response.IqResponseActivity());
  EXPECT_EQ("", handler()->OutputActivity());
  EXPECT_EQ("", handler()->SessionActivity());
}
