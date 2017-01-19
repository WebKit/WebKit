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
#include <sstream>
#include <string>
#include "webrtc/libjingle/xmllite/xmlelement.h"
#include "webrtc/libjingle/xmpp/xmppstanzaparser.h"
#include "webrtc/base/common.h"
#include "webrtc/base/gunit.h"

using buzz::QName;
using buzz::XmlElement;
using buzz::XmppStanzaParser;
using buzz::XmppStanzaParseHandler;

class XmppStanzaParserTestHandler : public XmppStanzaParseHandler {
 public:
  virtual void StartStream(const XmlElement * element) {
    ss_ << "START" << element->Str();
  }
  virtual void Stanza(const XmlElement * element) {
    ss_ << "STANZA" << element->Str();
  }
  virtual void EndStream() {
    ss_ << "END";
  }
  virtual void XmlError() {
    ss_ << "ERROR";
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


TEST(XmppStanzaParserTest, TestTrivial) {
  XmppStanzaParserTestHandler handler;
  XmppStanzaParser parser(&handler);
  std::string fragment;

  fragment = "<trivial/>";
  parser.Parse(fragment.c_str(), fragment.length(), false);
  EXPECT_EQ("START<trivial/>END", handler.StrClear());
}

TEST(XmppStanzaParserTest, TestStanzaAtATime) {
  XmppStanzaParserTestHandler handler;
  XmppStanzaParser parser(&handler);
  std::string fragment;

  fragment = "<stream:stream id='abc' xmlns='j:c' xmlns:stream='str'>";
  parser.Parse(fragment.c_str(), fragment.length(), false);
  EXPECT_EQ("START<stream:stream id=\"abc\" xmlns=\"j:c\" "
      "xmlns:stream=\"str\"/>", handler.StrClear());

  fragment = "<message type='foo'><body>hello</body></message>";
  parser.Parse(fragment.c_str(), fragment.length(), false);
  EXPECT_EQ("STANZA<c:message type=\"foo\" xmlns:c=\"j:c\">"
      "<c:body>hello</c:body></c:message>", handler.StrClear());

  fragment = " SOME TEXT TO IGNORE ";
  parser.Parse(fragment.c_str(), fragment.length(), false);
  EXPECT_EQ("", handler.StrClear());

  fragment = "<iq type='set' id='123'><abc xmlns='def'/></iq>";
  parser.Parse(fragment.c_str(), fragment.length(), false);
  EXPECT_EQ("STANZA<c:iq type=\"set\" id=\"123\" xmlns:c=\"j:c\">"
      "<abc xmlns=\"def\"/></c:iq>", handler.StrClear());

  fragment = "</stream:stream>";
  parser.Parse(fragment.c_str(), fragment.length(), false);
  EXPECT_EQ("END", handler.StrClear());
}

TEST(XmppStanzaParserTest, TestFragmentedStanzas) {
  XmppStanzaParserTestHandler handler;
  XmppStanzaParser parser(&handler);
  std::string fragment;

  fragment = "<stream:stream id='abc' xmlns='j:c' xml";
  parser.Parse(fragment.c_str(), fragment.length(), false);
  EXPECT_EQ("", handler.StrClear());

  fragment = "ns:stream='str'><message type='foo'><body>hel";
  parser.Parse(fragment.c_str(), fragment.length(), false);
  EXPECT_EQ("START<stream:stream id=\"abc\" xmlns=\"j:c\" "
      "xmlns:stream=\"str\"/>", handler.StrClear());

  fragment = "lo</body></message> IGNORE ME <iq type='set' id='123'>"
      "<abc xmlns='def'/></iq></st";
  parser.Parse(fragment.c_str(), fragment.length(), false);
  EXPECT_EQ("STANZA<c:message type=\"foo\" xmlns:c=\"j:c\">"
      "<c:body>hello</c:body></c:message>STANZA<c:iq type=\"set\" id=\"123\" "
      "xmlns:c=\"j:c\"><abc xmlns=\"def\"/></c:iq>", handler.StrClear());

  fragment = "ream:stream>";
  parser.Parse(fragment.c_str(), fragment.length(), false);
  EXPECT_EQ("END", handler.StrClear());
}

TEST(XmppStanzaParserTest, TestReset) {
  XmppStanzaParserTestHandler handler;
  XmppStanzaParser parser(&handler);
  std::string fragment;

  fragment = "<stream:stream id='abc' xmlns='j:c' xml";
  parser.Parse(fragment.c_str(), fragment.length(), false);
  EXPECT_EQ("", handler.StrClear());

  parser.Reset();
  fragment = "<stream:stream id='abc' xmlns='j:c' xml";
  parser.Parse(fragment.c_str(), fragment.length(), false);
  EXPECT_EQ("", handler.StrClear());

  fragment = "ns:stream='str'><message type='foo'><body>hel";
  parser.Parse(fragment.c_str(), fragment.length(), false);
  EXPECT_EQ("START<stream:stream id=\"abc\" xmlns=\"j:c\" "
      "xmlns:stream=\"str\"/>", handler.StrClear());
  parser.Reset();

  fragment = "<stream:stream id='abc' xmlns='j:c' xmlns:stream='str'>";
  parser.Parse(fragment.c_str(), fragment.length(), false);
  EXPECT_EQ("START<stream:stream id=\"abc\" xmlns=\"j:c\" "
      "xmlns:stream=\"str\"/>", handler.StrClear());

  fragment = "<message type='foo'><body>hello</body></message>";
  parser.Parse(fragment.c_str(), fragment.length(), false);
  EXPECT_EQ("STANZA<c:message type=\"foo\" xmlns:c=\"j:c\">"
      "<c:body>hello</c:body></c:message>", handler.StrClear());
}

TEST(XmppStanzaParserTest, TestError) {
  XmppStanzaParserTestHandler handler;
  XmppStanzaParser parser(&handler);
  std::string fragment;

  fragment = "<-foobar/>";
  parser.Parse(fragment.c_str(), fragment.length(), false);
  EXPECT_EQ("ERROR", handler.StrClear());

  parser.Reset();
  fragment = "<stream:stream/>";
  parser.Parse(fragment.c_str(), fragment.length(), false);
  EXPECT_EQ("ERROR", handler.StrClear());
  parser.Reset();

  fragment = "ns:stream='str'><message type='foo'><body>hel";
  parser.Parse(fragment.c_str(), fragment.length(), false);
  EXPECT_EQ("ERROR", handler.StrClear());
  parser.Reset();

  fragment = "<stream:stream xmlns:stream='st' xmlns='jc'>"
      "<foo/><bar><st:foobar/></bar>";
  parser.Parse(fragment.c_str(), fragment.length(), false);
  EXPECT_EQ("START<stream:stream xmlns:stream=\"st\" xmlns=\"jc\"/>STANZA"
      "<jc:foo xmlns:jc=\"jc\"/>ERROR", handler.StrClear());
}
