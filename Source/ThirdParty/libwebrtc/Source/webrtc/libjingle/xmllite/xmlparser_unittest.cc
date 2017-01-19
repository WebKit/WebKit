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
#include "webrtc/libjingle/xmllite/qname.h"
#include "webrtc/libjingle/xmllite/xmlparser.h"
#include "webrtc/base/common.h"
#include "webrtc/base/gunit.h"

using buzz::QName;
using buzz::XmlParser;
using buzz::XmlParseContext;
using buzz::XmlParseHandler;

class XmlParserTestHandler : public XmlParseHandler {
 public:
  virtual void StartElement(XmlParseContext * pctx,
                            const char * name, const char ** atts) {
    ss_ << "START (" << pctx->ResolveQName(name, false).Merged();
    while (*atts) {
      ss_ << ", " << pctx->ResolveQName(*atts, true).Merged()
          << "='" << *(atts+1) << "'";
      atts += 2;
    }
    ss_ << ") ";
  }
  virtual void EndElement(XmlParseContext * pctx, const char * name) {
    RTC_UNUSED(pctx);
    RTC_UNUSED(name);
    ss_ << "END ";
  }
  virtual void CharacterData(XmlParseContext * pctx,
                             const char * text, int len) {
    RTC_UNUSED(pctx);
    ss_ << "TEXT (" << std::string(text, len) << ") ";
  }
  virtual void Error(XmlParseContext * pctx, XML_Error code) {
    RTC_UNUSED(pctx);
    ss_ << "ERROR (" << static_cast<int>(code) << ") ";
  }
  virtual ~XmlParserTestHandler() {
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


TEST(XmlParserTest, TestTrivial) {
  XmlParserTestHandler handler;
  XmlParser::ParseXml(&handler, "<testing/>");
  EXPECT_EQ("START (testing) END ", handler.Str());
}

TEST(XmlParserTest, TestAttributes) {
  {
    XmlParserTestHandler handler;
    XmlParser::ParseXml(&handler, "<testing a='b'/>");
    EXPECT_EQ("START (testing, a='b') END ", handler.Str());
  }
  {
    XmlParserTestHandler handler;
    XmlParser::ParseXml(&handler, "<testing e='' long='some text'/>");
    EXPECT_EQ("START (testing, e='', long='some text') END ", handler.Str());
  }
}

TEST(XmlParserTest, TestNesting) {
  {
    XmlParserTestHandler handler;
    XmlParser::ParseXml(&handler,
        "<top><first/><second><third></third></second></top>");
    EXPECT_EQ("START (top) START (first) END START (second) START (third) "
        "END END END ", handler.Str());
  }
  {
    XmlParserTestHandler handler;
    XmlParser::ParseXml(&handler, "<top><fifth><deeper><and><deeper/></and>"
        "<sibling><leaf/></sibling></deeper></fifth><first/><second>"
        "<third></third></second></top>");
    EXPECT_EQ("START (top) START (fifth) START (deeper) START (and) START "
            "(deeper) END END START (sibling) START (leaf) END END END "
            "END START (first) END START (second) START (third) END END END ",
            handler.Str());
  }
}

TEST(XmlParserTest, TestXmlDecl) {
  {
    XmlParserTestHandler handler;
    XmlParser::ParseXml(&handler, "<?xml version=\"1.0\"?><testing/>");
    EXPECT_EQ("START (testing) END ", handler.Str());
  }
  {
    XmlParserTestHandler handler;
    XmlParser::ParseXml(&handler,
        "<?xml version=\"1.0\" encoding=\"utf-8\"?><testing/>");
    EXPECT_EQ("START (testing) END ", handler.Str());
  }
  {
    XmlParserTestHandler handler;
    XmlParser::ParseXml(&handler,
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<testing/>");
    EXPECT_EQ("START (testing) END ", handler.Str());
  }
}

TEST(XmlParserTest, TestNamespace) {
  {
    XmlParserTestHandler handler;
    XmlParser::ParseXml(&handler, "<top xmlns='my-namespace' a='b'/>");
    EXPECT_EQ("START (my-namespace:top, xmlns='my-namespace', a='b') END ",
        handler.Str());
  }
  {
    XmlParserTestHandler handler;
    XmlParser::ParseXml(&handler, "<foo:top xmlns:foo='my-namespace' "
          "a='b' foo:c='d'/>");
    EXPECT_EQ("START (my-namespace:top, "
        "http://www.w3.org/2000/xmlns/:foo='my-namespace', "
        "a='b', my-namespace:c='d') END ", handler.Str());
  }
  {
    XmlParserTestHandler handler;
    XmlParser::ParseXml(&handler, "<top><nested xmlns='my-namespace'><leaf/>"
        "</nested><sibling/></top>");
    EXPECT_EQ("START (top) START (my-namespace:nested, xmlns='my-namespace') "
        "START (my-namespace:leaf) END END START (sibling) END END ",
        handler.Str());
  }
}

TEST(XmlParserTest, TestIncremental) {
  XmlParserTestHandler handler;
  XmlParser parser(&handler);
  std::string fragment;

  fragment = "<stream:stream";
  parser.Parse(fragment.c_str(), fragment.length(), false);
  EXPECT_EQ("", handler.StrClear());

  fragment = " id=\"abcdefg\" xmlns=\"";
  parser.Parse(fragment.c_str(), fragment.length(), false);
  EXPECT_EQ("", handler.StrClear());

  fragment = "j:c\" xmlns:stream='hm";
  parser.Parse(fragment.c_str(), fragment.length(), false);
  EXPECT_EQ("", handler.StrClear());

  fragment = "ph'><test";
  parser.Parse(fragment.c_str(), fragment.length(), false);
  EXPECT_EQ("START (hmph:stream, id='abcdefg', xmlns='j:c', "
      "http://www.w3.org/2000/xmlns/:stream='hmph') ", handler.StrClear());

  fragment = "ing/><again/>abracad";
  parser.Parse(fragment.c_str(), fragment.length(), false);
  EXPECT_EQ("START (j:c:testing) END START (j:c:again) END TEXT (abracad) ",
      handler.StrClear());

  fragment = "abra</stream:";
  parser.Parse(fragment.c_str(), fragment.length(), false);
  EXPECT_EQ("TEXT (abra) ", handler.StrClear());

  fragment = "stream>";
  parser.Parse(fragment.c_str(), fragment.length(), false);
  EXPECT_EQ("END ", handler.StrClear());
}

TEST(XmlParserTest, TestReset) {
  {
    XmlParserTestHandler handler;
    XmlParser parser(&handler);
    std::string fragment;

    fragment = "<top><first/><second><third></third>";
    parser.Parse(fragment.c_str(), fragment.length(), false);
    EXPECT_EQ("START (top) START (first) END START (second) START (third) END ",
        handler.StrClear());

    parser.Reset();
    fragment = "<tip><first/><second><third></third>";
    parser.Parse(fragment.c_str(), fragment.length(), false);
    EXPECT_EQ("START (tip) START (first) END START (second) START (third) END ",
        handler.StrClear());
  }
  {
    XmlParserTestHandler handler;
    XmlParser parser(&handler);
    std::string fragment;

    fragment = "<top xmlns='m'>";
    parser.Parse(fragment.c_str(), fragment.length(), false);
    EXPECT_EQ("START (m:top, xmlns='m') ", handler.StrClear());

    fragment = "<testing/><frag";
    parser.Parse(fragment.c_str(), fragment.length(), false);
    EXPECT_EQ("START (m:testing) END ", handler.StrClear());

    parser.Reset();
    fragment = "<testing><fragment/";
    parser.Parse(fragment.c_str(), fragment.length(), false);
    EXPECT_EQ("START (testing) ", handler.StrClear());

    fragment = ">";
    parser.Parse(fragment.c_str(), fragment.length(), false);
    EXPECT_EQ("START (fragment) END ", handler.StrClear());
  }
}

TEST(XmlParserTest, TestError) {
  {
    XmlParserTestHandler handler;
    XmlParser::ParseXml(&handler, "junk");
    EXPECT_EQ("ERROR (2) ", handler.Str());
  }
  {
    XmlParserTestHandler handler;
    XmlParser::ParseXml(&handler, "<top/> garbage ");
    EXPECT_EQ("START (top) END ERROR (9) ", handler.Str());
  }
  {
    XmlParserTestHandler handler;
    XmlParser::ParseXml(&handler, "<-hm->");
    EXPECT_EQ("ERROR (4) ", handler.Str());
  }
  {
    XmlParserTestHandler handler;
    XmlParser::ParseXml(&handler, "<hello>&foobar;</hello>");
    EXPECT_EQ("START (hello) ERROR (11) ", handler.Str());
  }
  {
    XmlParserTestHandler handler;
    XmlParser::ParseXml(&handler,
        "<!DOCTYPE HTML PUBLIC \"foobar\" \"barfoo\">");
    EXPECT_EQ("ERROR (3) ", handler.Str());
  }
  {
    // XmlParser requires utf-8
    XmlParserTestHandler handler;
    XmlParser::ParseXml(&handler,
        "<?xml version=\"1.0\" encoding=\"iso-8859-1\"?><test/>");
    EXPECT_EQ("ERROR (19) ", handler.Str());
  }
  {
    // XmlParser requires version 1.0
    XmlParserTestHandler handler;
    XmlParser::ParseXml(&handler,
        "<?xml version=\"2.0\"?><test/>");
    EXPECT_EQ("ERROR (2) ", handler.Str());
  }
  {
    // XmlParser requires standalone documents
    XmlParserTestHandler handler;
    XmlParser::ParseXml(&handler,
        "<?xml version=\"1.0\" standalone=\"no\"?><test/>");
    EXPECT_EQ("ERROR (2) ", handler.Str());
  }
  {
    // XmlParser doesn't like empty namespace URIs
    XmlParserTestHandler handler;
    XmlParser::ParseXml(&handler,
        "<test xmlns:foo='' foo:bar='huh?'>");
    EXPECT_EQ("ERROR (2) ", handler.Str());
  }
}
