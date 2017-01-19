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
#include "webrtc/base/cryptstring.h"
#include "webrtc/base/gunit.h"
#include "webrtc/typedefs.h"

using buzz::Jid;
using buzz::QName;
using buzz::XmlElement;
using buzz::XmppEngine;
using buzz::XmppTestHandler;

enum XlttStage {
  XLTT_STAGE_CONNECT = 0,
  XLTT_STAGE_STREAMSTART,
  XLTT_STAGE_TLS_FEATURES,
  XLTT_STAGE_TLS_PROCEED,
  XLTT_STAGE_ENCRYPTED_START,
  XLTT_STAGE_AUTH_FEATURES,
  XLTT_STAGE_AUTH_SUCCESS,
  XLTT_STAGE_AUTHENTICATED_START,
  XLTT_STAGE_BIND_FEATURES,
  XLTT_STAGE_BIND_SUCCESS,
  XLTT_STAGE_SESSION_SUCCESS,
};

class XmppLoginTaskTest : public testing::Test {
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
  void RunPartialLogin(XlttStage startstage, XlttStage endstage);
  void SetTlsOptions(buzz::TlsOptions option);

 private:
  std::unique_ptr<XmppEngine> engine_;
  std::unique_ptr<XmppTestHandler> handler_;
};

void XmppLoginTaskTest::SetTlsOptions(buzz::TlsOptions option) {
  engine_->SetTls(option);
}
void XmppLoginTaskTest::RunPartialLogin(XlttStage startstage,
                                        XlttStage endstage) {
  std::string input;

  switch (startstage) {
    case XLTT_STAGE_CONNECT: {
      engine_->Connect();
      XmlElement appStanza(QName("test", "app-stanza"));
      appStanza.AddText("this-is-a-test");
      engine_->SendStanza(&appStanza);

      EXPECT_EQ("<stream:stream to=\"my-server\" xml:lang=\"*\" "
          "version=\"1.0\" xmlns:stream=\"http://etherx.jabber.org/streams\" "
          "xmlns=\"jabber:client\">\r\n", handler_->OutputActivity());
      EXPECT_EQ("[OPENING]", handler_->SessionActivity());
      EXPECT_EQ("", handler_->StanzaActivity());
      if (endstage == XLTT_STAGE_CONNECT)
        return;
      FALLTHROUGH();
    }

    case XLTT_STAGE_STREAMSTART: {
      input = "<stream:stream id=\"a5f2d8c9\" version=\"1.0\" "
          "xmlns:stream=\"http://etherx.jabber.org/streams\" "
          "xmlns=\"jabber:client\">";
      engine_->HandleInput(input.c_str(), input.length());
      EXPECT_EQ("", handler_->StanzaActivity());
      EXPECT_EQ("", handler_->SessionActivity());
      EXPECT_EQ("", handler_->OutputActivity());
      if (endstage == XLTT_STAGE_STREAMSTART)
        return;
      FALLTHROUGH();
    }

    case XLTT_STAGE_TLS_FEATURES: {
      input = "<stream:features>"
        "<starttls xmlns='urn:ietf:params:xml:ns:xmpp-tls'/>"
       "</stream:features>";
      engine_->HandleInput(input.c_str(), input.length());
      EXPECT_EQ("<starttls xmlns=\"urn:ietf:params:xml:ns:xmpp-tls\"/>",
          handler_->OutputActivity());
      EXPECT_EQ("", handler_->StanzaActivity());
      EXPECT_EQ("", handler_->SessionActivity());
      if (endstage == XLTT_STAGE_TLS_FEATURES)
        return;
      FALLTHROUGH();
    }

    case XLTT_STAGE_TLS_PROCEED: {
      input = std::string("<proceed xmlns='urn:ietf:params:xml:ns:xmpp-tls'/>");
      engine_->HandleInput(input.c_str(), input.length());
      EXPECT_EQ("[START-TLS my-server]"
          "<stream:stream to=\"my-server\" xml:lang=\"*\" "
          "version=\"1.0\" xmlns:stream=\"http://etherx.jabber.org/streams\" "
          "xmlns=\"jabber:client\">\r\n", handler_->OutputActivity());
      EXPECT_EQ("", handler_->StanzaActivity());
      EXPECT_EQ("", handler_->SessionActivity());
      if (endstage == XLTT_STAGE_TLS_PROCEED)
        return;
      FALLTHROUGH();
    }

    case XLTT_STAGE_ENCRYPTED_START: {
      input = std::string("<stream:stream id=\"01234567\" version=\"1.0\" "
          "xmlns:stream=\"http://etherx.jabber.org/streams\" "
          "xmlns=\"jabber:client\">");
      engine_->HandleInput(input.c_str(), input.length());
      EXPECT_EQ("", handler_->StanzaActivity());
      EXPECT_EQ("", handler_->SessionActivity());
      EXPECT_EQ("", handler_->OutputActivity());
      if (endstage == XLTT_STAGE_ENCRYPTED_START)
        return;
      FALLTHROUGH();
    }

    case XLTT_STAGE_AUTH_FEATURES: {
      input = "<stream:features>"
        "<mechanisms xmlns='urn:ietf:params:xml:ns:xmpp-sasl'>"
          "<mechanism>DIGEST-MD5</mechanism>"
          "<mechanism>PLAIN</mechanism>"
        "</mechanisms>"
       "</stream:features>";
      engine_->HandleInput(input.c_str(), input.length());
      EXPECT_EQ("<auth xmlns=\"urn:ietf:params:xml:ns:xmpp-sasl\" "
          "mechanism=\"PLAIN\" "
          "auth:allow-non-google-login=\"true\" "
          "auth:client-uses-full-bind-result=\"true\" "
          "xmlns:auth=\"http://www.google.com/talk/protocol/auth\""
          ">AGRhdmlkAGRhdmlk</auth>",
          handler_->OutputActivity());
      EXPECT_EQ("", handler_->StanzaActivity());
      EXPECT_EQ("", handler_->SessionActivity());
      if (endstage == XLTT_STAGE_AUTH_FEATURES)
        return;
      FALLTHROUGH();
    }

    case XLTT_STAGE_AUTH_SUCCESS: {
      input = "<success xmlns='urn:ietf:params:xml:ns:xmpp-sasl'/>";
      engine_->HandleInput(input.c_str(), input.length());
      EXPECT_EQ("<stream:stream to=\"my-server\" xml:lang=\"*\" "
          "version=\"1.0\" xmlns:stream=\"http://etherx.jabber.org/streams\" "
          "xmlns=\"jabber:client\">\r\n", handler_->OutputActivity());
      EXPECT_EQ("", handler_->StanzaActivity());
      EXPECT_EQ("", handler_->SessionActivity());
      if (endstage == XLTT_STAGE_AUTH_SUCCESS)
        return;
      FALLTHROUGH();
    }

    case XLTT_STAGE_AUTHENTICATED_START: {
      input = std::string("<stream:stream id=\"01234567\" version=\"1.0\" "
          "xmlns:stream=\"http://etherx.jabber.org/streams\" "
          "xmlns=\"jabber:client\">");
      engine_->HandleInput(input.c_str(), input.length());
      EXPECT_EQ("", handler_->StanzaActivity());
      EXPECT_EQ("", handler_->SessionActivity());
      EXPECT_EQ("", handler_->OutputActivity());
      if (endstage == XLTT_STAGE_AUTHENTICATED_START)
        return;
      FALLTHROUGH();
    }

    case XLTT_STAGE_BIND_FEATURES: {
      input = "<stream:features>"
          "<bind xmlns='urn:ietf:params:xml:ns:xmpp-bind'/>"
          "<session xmlns='urn:ietf:params:xml:ns:xmpp-session'/>"
        "</stream:features>";
      engine_->HandleInput(input.c_str(), input.length());
      EXPECT_EQ("<iq type=\"set\" id=\"0\">"
          "<bind xmlns=\"urn:ietf:params:xml:ns:xmpp-bind\"/></iq>",
          handler_->OutputActivity());
      EXPECT_EQ("", handler_->StanzaActivity());
      EXPECT_EQ("", handler_->SessionActivity());
      if (endstage == XLTT_STAGE_BIND_FEATURES)
        return;
      FALLTHROUGH();
    }

    case XLTT_STAGE_BIND_SUCCESS: {
      input = "<iq type='result' id='0'>"
          "<bind xmlns='urn:ietf:params:xml:ns:xmpp-bind'>"
          "<jid>david@my-server/test</jid></bind></iq>";
      engine_->HandleInput(input.c_str(), input.length());
      EXPECT_EQ("<iq type=\"set\" id=\"1\">"
          "<session xmlns=\"urn:ietf:params:xml:ns:xmpp-session\"/></iq>",
          handler_->OutputActivity());
      EXPECT_EQ("", handler_->StanzaActivity());
      EXPECT_EQ("", handler_->SessionActivity());
      if (endstage == XLTT_STAGE_BIND_SUCCESS)
        return;
      FALLTHROUGH();
    }

    case XLTT_STAGE_SESSION_SUCCESS: {
      input = "<iq type='result' id='1'/>";
      engine_->HandleInput(input.c_str(), input.length());
      EXPECT_EQ("<test:app-stanza xmlns:test=\"test\">this-is-a-test"
          "</test:app-stanza>", handler_->OutputActivity());
      EXPECT_EQ("[OPEN]", handler_->SessionActivity());
      EXPECT_EQ("", handler_->StanzaActivity());
      if (endstage == XLTT_STAGE_SESSION_SUCCESS)
        return;
      FALLTHROUGH();
    }
    default:
      break;
  }
}

TEST_F(XmppLoginTaskTest, TestUtf8Good) {
  RunPartialLogin(XLTT_STAGE_CONNECT, XLTT_STAGE_CONNECT);

  std::string input = "<?xml version='1.0' encoding='UTF-8'?>"
      "<stream:stream id=\"a5f2d8c9\" version=\"1.0\" "
      "xmlns:stream=\"http://etherx.jabber.org/streams\" "
      "xmlns=\"jabber:client\">";
  engine()->HandleInput(input.c_str(), input.length());
  EXPECT_EQ("", handler()->OutputActivity());
  EXPECT_EQ("", handler()->SessionActivity());
  EXPECT_EQ("", handler()->StanzaActivity());
}

TEST_F(XmppLoginTaskTest, TestNonUtf8Bad) {
  RunPartialLogin(XLTT_STAGE_CONNECT, XLTT_STAGE_CONNECT);

  std::string input = "<?xml version='1.0' encoding='ISO-8859-1'?>"
      "<stream:stream id=\"a5f2d8c9\" version=\"1.0\" "
      "xmlns:stream=\"http://etherx.jabber.org/streams\" "
      "xmlns=\"jabber:client\">";
  engine()->HandleInput(input.c_str(), input.length());
  EXPECT_EQ("[CLOSED]", handler()->OutputActivity());
  EXPECT_EQ("[CLOSED][ERROR-XML]", handler()->SessionActivity());
  EXPECT_EQ("", handler()->StanzaActivity());
}

TEST_F(XmppLoginTaskTest, TestNoFeatures) {
  RunPartialLogin(XLTT_STAGE_CONNECT, XLTT_STAGE_STREAMSTART);

  std::string input = "<iq type='get' id='1'/>";
  engine()->HandleInput(input.c_str(), input.length());

  EXPECT_EQ("[CLOSED]", handler()->OutputActivity());
  EXPECT_EQ("[CLOSED][ERROR-VERSION]", handler()->SessionActivity());
  EXPECT_EQ("", handler()->StanzaActivity());
}

TEST_F(XmppLoginTaskTest, TestTlsRequiredNotPresent) {
  RunPartialLogin(XLTT_STAGE_CONNECT, XLTT_STAGE_STREAMSTART);

  std::string input = "<stream:features>"
      "<mechanisms xmlns='urn:ietf:params:xml:ns:xmpp-sasl'>"
        "<mechanism>DIGEST-MD5</mechanism>"
        "<mechanism>PLAIN</mechanism>"
      "</mechanisms>"
     "</stream:features>";
  engine()->HandleInput(input.c_str(), input.length());

  EXPECT_EQ("[CLOSED]", handler()->OutputActivity());
  EXPECT_EQ("[CLOSED][ERROR-TLS]", handler()->SessionActivity());
  EXPECT_EQ("", handler()->StanzaActivity());
}

TEST_F(XmppLoginTaskTest, TestTlsRequeiredAndPresent) {
  RunPartialLogin(XLTT_STAGE_CONNECT, XLTT_STAGE_STREAMSTART);

  std::string input = "<stream:features>"
      "<starttls xmlns='urn:ietf:params:xml:ns:xmpp-tls'>"
        "<required/>"
      "</starttls>"
      "<mechanisms xmlns='urn:ietf:params:xml:ns:xmpp-sasl'>"
        "<mechanism>X-GOOGLE-TOKEN</mechanism>"
        "<mechanism>PLAIN</mechanism>"
        "<mechanism>X-OAUTH2</mechanism>"
      "</mechanisms>"
     "</stream:features>";
  engine()->HandleInput(input.c_str(), input.length());

  EXPECT_EQ("<starttls xmlns=\"urn:ietf:params:xml:ns:xmpp-tls\"/>",
      handler()->OutputActivity());
  EXPECT_EQ("", handler()->SessionActivity());
  EXPECT_EQ("", handler()->StanzaActivity());
}

TEST_F(XmppLoginTaskTest, TestTlsEnabledNotPresent) {
  SetTlsOptions(buzz::TLS_ENABLED);
  RunPartialLogin(XLTT_STAGE_CONNECT, XLTT_STAGE_STREAMSTART);

  std::string input = "<stream:features>"
      "<mechanisms xmlns='urn:ietf:params:xml:ns:xmpp-sasl'>"
        "<mechanism>DIGEST-MD5</mechanism>"
        "<mechanism>PLAIN</mechanism>"
      "</mechanisms>"
     "</stream:features>";
  engine()->HandleInput(input.c_str(), input.length());

  EXPECT_EQ("<auth xmlns=\"urn:ietf:params:xml:ns:xmpp-sasl\" "
      "mechanism=\"PLAIN\" auth:allow-non-google-login=\"true\" "
      "auth:client-uses-full-bind-result=\"true\" "
      "xmlns:auth=\"http://www.google.com/talk/protocol/auth\""
      ">AGRhdmlkAGRhdmlk</auth>", handler()->OutputActivity());
  EXPECT_EQ("", handler()->SessionActivity());
  EXPECT_EQ("", handler()->StanzaActivity());
}

TEST_F(XmppLoginTaskTest, TestTlsEnabledAndPresent) {
  SetTlsOptions(buzz::TLS_ENABLED);
  RunPartialLogin(XLTT_STAGE_CONNECT, XLTT_STAGE_STREAMSTART);

  std::string input = "<stream:features>"
      "<mechanisms xmlns='urn:ietf:params:xml:ns:xmpp-sasl'>"
        "<mechanism>X-GOOGLE-TOKEN</mechanism>"
        "<mechanism>PLAIN</mechanism>"
        "<mechanism>X-OAUTH2</mechanism>"
      "</mechanisms>"
      "</stream:features>";
  engine()->HandleInput(input.c_str(), input.length());

  EXPECT_EQ("<auth xmlns=\"urn:ietf:params:xml:ns:xmpp-sasl\" "
      "mechanism=\"PLAIN\" auth:allow-non-google-login=\"true\" "
      "auth:client-uses-full-bind-result=\"true\" "
      "xmlns:auth=\"http://www.google.com/talk/protocol/auth\""
      ">AGRhdmlkAGRhdmlk</auth>", handler()->OutputActivity());
  EXPECT_EQ("", handler()->SessionActivity());
  EXPECT_EQ("", handler()->StanzaActivity());
}

TEST_F(XmppLoginTaskTest, TestTlsDisabledNotPresent) {
  SetTlsOptions(buzz::TLS_DISABLED);
  RunPartialLogin(XLTT_STAGE_CONNECT, XLTT_STAGE_STREAMSTART);

    std::string input = "<stream:features>"
      "<mechanisms xmlns='urn:ietf:params:xml:ns:xmpp-sasl'>"
        "<mechanism>DIGEST-MD5</mechanism>"
        "<mechanism>PLAIN</mechanism>"
      "</mechanisms>"
     "</stream:features>";
  engine()->HandleInput(input.c_str(), input.length());

  EXPECT_EQ("<auth xmlns=\"urn:ietf:params:xml:ns:xmpp-sasl\" "
      "mechanism=\"PLAIN\" auth:allow-non-google-login=\"true\" "
      "auth:client-uses-full-bind-result=\"true\" "
      "xmlns:auth=\"http://www.google.com/talk/protocol/auth\""
      ">AGRhdmlkAGRhdmlk</auth>", handler()->OutputActivity());
  EXPECT_EQ("", handler()->SessionActivity());
  EXPECT_EQ("", handler()->StanzaActivity());
}

TEST_F(XmppLoginTaskTest, TestTlsDisabledAndPresent) {
  SetTlsOptions(buzz::TLS_DISABLED);
  RunPartialLogin(XLTT_STAGE_CONNECT, XLTT_STAGE_STREAMSTART);

  std::string input = "<stream:features>"
      "<mechanisms xmlns='urn:ietf:params:xml:ns:xmpp-sasl'>"
        "<mechanism>X-GOOGLE-TOKEN</mechanism>"
        "<mechanism>PLAIN</mechanism>"
        "<mechanism>X-OAUTH2</mechanism>"
      "</mechanisms>"
      "</stream:features>";
  engine()->HandleInput(input.c_str(), input.length());

  EXPECT_EQ("<auth xmlns=\"urn:ietf:params:xml:ns:xmpp-sasl\" "
      "mechanism=\"PLAIN\" auth:allow-non-google-login=\"true\" "
      "auth:client-uses-full-bind-result=\"true\" "
      "xmlns:auth=\"http://www.google.com/talk/protocol/auth\""
      ">AGRhdmlkAGRhdmlk</auth>", handler()->OutputActivity());
  EXPECT_EQ("", handler()->SessionActivity());
  EXPECT_EQ("", handler()->StanzaActivity());
}

TEST_F(XmppLoginTaskTest, TestTlsFailure) {
  RunPartialLogin(XLTT_STAGE_CONNECT, XLTT_STAGE_TLS_FEATURES);

  std::string input = "<failure xmlns='urn:ietf:params:xml:ns:xmpp-tls'/>";
  engine()->HandleInput(input.c_str(), input.length());

  EXPECT_EQ("[CLOSED]", handler()->OutputActivity());
  EXPECT_EQ("[CLOSED][ERROR-TLS]", handler()->SessionActivity());
  EXPECT_EQ("", handler()->StanzaActivity());
}

TEST_F(XmppLoginTaskTest, TestTlsBadStream) {
  RunPartialLogin(XLTT_STAGE_CONNECT, XLTT_STAGE_TLS_PROCEED);

  std::string input = "<wrongtag>";
  engine()->HandleInput(input.c_str(), input.length());

  EXPECT_EQ("[CLOSED]", handler()->OutputActivity());
  EXPECT_EQ("[CLOSED][ERROR-VERSION]", handler()->SessionActivity());
  EXPECT_EQ("", handler()->StanzaActivity());
}

TEST_F(XmppLoginTaskTest, TestMissingSaslPlain) {
  RunPartialLogin(XLTT_STAGE_CONNECT, XLTT_STAGE_ENCRYPTED_START);

  std::string input = "<stream:features>"
        "<mechanisms xmlns='urn:ietf:params:xml:ns:xmpp-sasl'>"
          "<mechanism>DIGEST-MD5</mechanism>"
        "</mechanisms>"
       "</stream:features>";
  engine()->HandleInput(input.c_str(), input.length());

  EXPECT_EQ("[CLOSED]", handler()->OutputActivity());
  EXPECT_EQ("[CLOSED][ERROR-AUTH]", handler()->SessionActivity());
  EXPECT_EQ("", handler()->StanzaActivity());
}

TEST_F(XmppLoginTaskTest, TestWrongPassword) {
  RunPartialLogin(XLTT_STAGE_CONNECT, XLTT_STAGE_AUTH_FEATURES);

  std::string input = "<failure xmlns='urn:ietf:params:xml:ns:xmpp-sasl'/>";
  engine()->HandleInput(input.c_str(), input.length());

  EXPECT_EQ("[CLOSED]", handler()->OutputActivity());
  EXPECT_EQ("[CLOSED][ERROR-UNAUTHORIZED]", handler()->SessionActivity());
  EXPECT_EQ("", handler()->StanzaActivity());
}

TEST_F(XmppLoginTaskTest, TestAuthBadStream) {
  RunPartialLogin(XLTT_STAGE_CONNECT, XLTT_STAGE_AUTH_SUCCESS);

  std::string input = "<wrongtag>";
  engine()->HandleInput(input.c_str(), input.length());

  EXPECT_EQ("[CLOSED]", handler()->OutputActivity());
  EXPECT_EQ("[CLOSED][ERROR-VERSION]", handler()->SessionActivity());
  EXPECT_EQ("", handler()->StanzaActivity());
}

TEST_F(XmppLoginTaskTest, TestMissingBindFeature) {
  RunPartialLogin(XLTT_STAGE_CONNECT, XLTT_STAGE_AUTHENTICATED_START);

  std::string input = "<stream:features>"
          "<session xmlns='urn:ietf:params:xml:ns:xmpp-session'/>"
        "</stream:features>";
  engine()->HandleInput(input.c_str(), input.length());

  EXPECT_EQ("[CLOSED]", handler()->OutputActivity());
  EXPECT_EQ("[CLOSED][ERROR-BIND]", handler()->SessionActivity());
}

TEST_F(XmppLoginTaskTest, TestMissingSessionFeature) {
  RunPartialLogin(XLTT_STAGE_CONNECT, XLTT_STAGE_AUTHENTICATED_START);

  std::string input = "<stream:features>"
          "<bind xmlns='urn:ietf:params:xml:ns:xmpp-bind'/>"
        "</stream:features>";
  engine()->HandleInput(input.c_str(), input.length());

  EXPECT_EQ("[CLOSED]", handler()->OutputActivity());
  EXPECT_EQ("[CLOSED][ERROR-BIND]", handler()->SessionActivity());
  EXPECT_EQ("", handler()->StanzaActivity());
}

/* TODO: Handle this case properly inside XmppLoginTask.
TEST_F(XmppLoginTaskTest, TestBindFailure1) {
  // check wrong JID
  RunPartialLogin(XLTT_STAGE_CONNECT, XLTT_STAGE_BIND_FEATURES);

  std::string input = "<iq type='result' id='0'>"
      "<bind xmlns='urn:ietf:params:xml:ns:xmpp-bind'>"
      "<jid>davey@my-server/test</jid></bind></iq>";
  engine()->HandleInput(input.c_str(), input.length());

  EXPECT_EQ("[CLOSED]", handler()->OutputActivity());
  EXPECT_EQ("[CLOSED][ERROR-BIND]", handler()->SessionActivity());
  EXPECT_EQ("", handler()->StanzaActivity());
}
*/

TEST_F(XmppLoginTaskTest, TestBindFailure2) {
  // check missing JID
  RunPartialLogin(XLTT_STAGE_CONNECT, XLTT_STAGE_BIND_FEATURES);

  std::string input = "<iq type='result' id='0'>"
      "<bind xmlns='urn:ietf:params:xml:ns:xmpp-bind'/></iq>";
  engine()->HandleInput(input.c_str(), input.length());

  EXPECT_EQ("[CLOSED]", handler()->OutputActivity());
  EXPECT_EQ("[CLOSED][ERROR-BIND]", handler()->SessionActivity());
  EXPECT_EQ("", handler()->StanzaActivity());
}

TEST_F(XmppLoginTaskTest, TestBindFailure3) {
  // check plain failure
  RunPartialLogin(XLTT_STAGE_CONNECT, XLTT_STAGE_BIND_FEATURES);

  std::string input = "<iq type='error' id='0'/>";
  engine()->HandleInput(input.c_str(), input.length());

  EXPECT_EQ("[CLOSED]", handler()->OutputActivity());
  EXPECT_EQ("[CLOSED][ERROR-BIND]", handler()->SessionActivity());
  EXPECT_EQ("", handler()->StanzaActivity());
}

TEST_F(XmppLoginTaskTest, TestBindFailure4) {
  // check wrong id to ignore
  RunPartialLogin(XLTT_STAGE_CONNECT, XLTT_STAGE_BIND_FEATURES);

  std::string input = "<iq type='error' id='1'/>";
  engine()->HandleInput(input.c_str(), input.length());

  // continue after an ignored iq
  RunPartialLogin(XLTT_STAGE_BIND_SUCCESS, XLTT_STAGE_SESSION_SUCCESS);
}

TEST_F(XmppLoginTaskTest, TestSessionFailurePlain1) {
  RunPartialLogin(XLTT_STAGE_CONNECT, XLTT_STAGE_BIND_SUCCESS);

  std::string input = "<iq type='error' id='1'/>";
  engine()->HandleInput(input.c_str(), input.length());

  EXPECT_EQ("[CLOSED]", handler()->OutputActivity());
  EXPECT_EQ("[CLOSED][ERROR-BIND]", handler()->SessionActivity());
}

TEST_F(XmppLoginTaskTest, TestSessionFailurePlain2) {
  RunPartialLogin(XLTT_STAGE_CONNECT, XLTT_STAGE_BIND_SUCCESS);

  // check reverse iq to ignore
  // TODO: consider queueing or passing through?
  std::string input = "<iq type='get' id='1'/>";
  engine()->HandleInput(input.c_str(), input.length());

  EXPECT_EQ("", handler()->OutputActivity());
  EXPECT_EQ("", handler()->SessionActivity());

  // continue after an ignored iq
  RunPartialLogin(XLTT_STAGE_SESSION_SUCCESS, XLTT_STAGE_SESSION_SUCCESS);
}

TEST_F(XmppLoginTaskTest, TestBadXml) {
  int errorKind = 0;
  for (XlttStage stage = XLTT_STAGE_CONNECT;
      stage <= XLTT_STAGE_SESSION_SUCCESS;
      stage = static_cast<XlttStage>(stage + 1)) {
    RunPartialLogin(XLTT_STAGE_CONNECT, stage);

    std::string input;
    switch (errorKind++ % 5) {
      case 0: input = "&syntax;"; break;
      case 1: input = "<nons:iq/>"; break;
      case 2: input = "<iq a='b' a='dupe'/>"; break;
      case 3: input = "<>"; break;
      case 4: input = "<iq a='<wrong>'>"; break;
    }

    engine()->HandleInput(input.c_str(), input.length());

    EXPECT_EQ("[CLOSED]", handler()->OutputActivity());
    EXPECT_EQ("[CLOSED][ERROR-XML]", handler()->SessionActivity());

    TearDown();
    SetUp();
  }
}

TEST_F(XmppLoginTaskTest, TestStreamError) {
  for (XlttStage stage = XLTT_STAGE_CONNECT;
      stage <= XLTT_STAGE_SESSION_SUCCESS;
      stage = static_cast<XlttStage>(stage + 1)) {
    switch (stage) {
      case XLTT_STAGE_CONNECT:
      case XLTT_STAGE_TLS_PROCEED:
      case XLTT_STAGE_AUTH_SUCCESS:
        continue;
      default:
        break;
    }

    RunPartialLogin(XLTT_STAGE_CONNECT, stage);

    std::string input = "<stream:error>"
        "<xml-not-well-formed xmlns='urn:ietf:params:xml:ns:xmpp-streams'/>"
        "<text xml:lang='en' xmlns='urn:ietf:params:xml:ns:xmpp-streams'>"
        "Some special application diagnostic information!"
        "</text>"
        "<escape-your-data xmlns='application-ns'/>"
        "</stream:error>";

    engine()->HandleInput(input.c_str(), input.length());

    EXPECT_EQ("[CLOSED]", handler()->OutputActivity());
    EXPECT_EQ("[CLOSED][ERROR-STREAM]", handler()->SessionActivity());

    EXPECT_EQ("<str:error xmlns:str=\"http://etherx.jabber.org/streams\">"
        "<xml-not-well-formed xmlns=\"urn:ietf:params:xml:ns:xmpp-streams\"/>"
        "<text xml:lang=\"en\" xmlns=\"urn:ietf:params:xml:ns:xmpp-streams\">"
        "Some special application diagnostic information!"
        "</text>"
        "<escape-your-data xmlns=\"application-ns\"/>"
        "</str:error>", engine()->GetStreamError()->Str());

    TearDown();
    SetUp();
  }
}

