/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/libjingle/xmllite/xmlnsstack.h"

#include <iostream>
#include <sstream>
#include <string>

#include "webrtc/libjingle/xmllite/xmlconstants.h"
#include "webrtc/base/common.h"
#include "webrtc/base/gunit.h"

using buzz::NS_XML;
using buzz::NS_XMLNS;
using buzz::QName;
using buzz::XmlnsStack;

TEST(XmlnsStackTest, TestBuiltin) {
  XmlnsStack stack;

  EXPECT_EQ(std::string(NS_XML), stack.NsForPrefix("xml").first);
  EXPECT_EQ(std::string(NS_XMLNS), stack.NsForPrefix("xmlns").first);
  EXPECT_EQ("", stack.NsForPrefix("").first);

  EXPECT_EQ("xml", stack.PrefixForNs(NS_XML, false).first);
  EXPECT_EQ("xmlns", stack.PrefixForNs(NS_XMLNS, false).first);
  EXPECT_EQ("", stack.PrefixForNs("", false).first);
  EXPECT_EQ("", stack.PrefixForNs("", true).first);
}

TEST(XmlnsStackTest, TestNsForPrefix) {
 XmlnsStack stack;
  stack.AddXmlns("pre1", "ns1");
  stack.AddXmlns("pre2", "ns2");
  stack.AddXmlns("pre1", "ns3");
  stack.AddXmlns("", "ns4");

  EXPECT_EQ("ns3", stack.NsForPrefix("pre1").first);
  EXPECT_TRUE(stack.NsForPrefix("pre1").second);
  EXPECT_EQ("ns2", stack.NsForPrefix("pre2").first);
  EXPECT_EQ("ns4", stack.NsForPrefix("").first);
  EXPECT_EQ("", stack.NsForPrefix("pre3").first);
  EXPECT_FALSE(stack.NsForPrefix("pre3").second);
}

TEST(XmlnsStackTest, TestPrefixForNs) {
  XmlnsStack stack;
  stack.AddXmlns("pre1", "ns1");
  stack.AddXmlns("pre2", "ns2");
  stack.AddXmlns("pre1", "ns3");
  stack.AddXmlns("pre3", "ns2");
  stack.AddXmlns("pre4", "ns4");
  stack.AddXmlns("", "ns4");

  EXPECT_EQ("", stack.PrefixForNs("ns1", false).first);
  EXPECT_FALSE(stack.PrefixForNs("ns1", false).second);
  EXPECT_EQ("", stack.PrefixForNs("ns1", true).first);
  EXPECT_FALSE(stack.PrefixForNs("ns1", true).second);
  EXPECT_EQ("pre3", stack.PrefixForNs("ns2", false).first);
  EXPECT_TRUE(stack.PrefixForNs("ns2", false).second);
  EXPECT_EQ("pre3", stack.PrefixForNs("ns2", true).first);
  EXPECT_TRUE(stack.PrefixForNs("ns2", true).second);
  EXPECT_EQ("pre1", stack.PrefixForNs("ns3", false).first);
  EXPECT_EQ("pre1", stack.PrefixForNs("ns3", true).first);
  EXPECT_EQ("", stack.PrefixForNs("ns4", false).first);
  EXPECT_TRUE(stack.PrefixForNs("ns4", false).second);
  EXPECT_EQ("pre4", stack.PrefixForNs("ns4", true).first);
  EXPECT_EQ("", stack.PrefixForNs("ns5", false).first);
  EXPECT_FALSE(stack.PrefixForNs("ns5", false).second);
  EXPECT_EQ("", stack.PrefixForNs("ns5", true).first);
  EXPECT_EQ("", stack.PrefixForNs("", false).first);
  EXPECT_EQ("", stack.PrefixForNs("", true).first);

  stack.AddXmlns("", "ns6");
  EXPECT_EQ("", stack.PrefixForNs("ns6", false).first);
  EXPECT_TRUE(stack.PrefixForNs("ns6", false).second);
  EXPECT_EQ("", stack.PrefixForNs("ns6", true).first);
  EXPECT_FALSE(stack.PrefixForNs("ns6", true).second);
}

TEST(XmlnsStackTest, TestFrames) {
  XmlnsStack stack;
  stack.PushFrame();
  stack.AddXmlns("pre1", "ns1");
  stack.AddXmlns("pre2", "ns2");

  stack.PushFrame();
  stack.AddXmlns("pre1", "ns3");
  stack.AddXmlns("pre3", "ns2");
  stack.AddXmlns("pre4", "ns4");

  stack.PushFrame();
  stack.PushFrame();
  stack.AddXmlns("", "ns4");

  // basic test
  EXPECT_EQ("ns3", stack.NsForPrefix("pre1").first);
  EXPECT_EQ("ns2", stack.NsForPrefix("pre2").first);
  EXPECT_EQ("ns2", stack.NsForPrefix("pre3").first);
  EXPECT_EQ("ns4", stack.NsForPrefix("pre4").first);
  EXPECT_EQ("", stack.NsForPrefix("pre5").first);
  EXPECT_FALSE(stack.NsForPrefix("pre5").second);
  EXPECT_EQ("ns4", stack.NsForPrefix("").first);
  EXPECT_TRUE(stack.NsForPrefix("").second);

  // pop the default xmlns definition
  stack.PopFrame();
  EXPECT_EQ("ns3", stack.NsForPrefix("pre1").first);
  EXPECT_EQ("ns2", stack.NsForPrefix("pre2").first);
  EXPECT_EQ("ns2", stack.NsForPrefix("pre3").first);
  EXPECT_EQ("ns4", stack.NsForPrefix("pre4").first);
  EXPECT_EQ("", stack.NsForPrefix("pre5").first);
  EXPECT_FALSE(stack.NsForPrefix("pre5").second);
  EXPECT_EQ("", stack.NsForPrefix("").first);
  EXPECT_TRUE(stack.NsForPrefix("").second);

  // pop empty frame (nop)
  stack.PopFrame();
  EXPECT_EQ("ns3", stack.NsForPrefix("pre1").first);
  EXPECT_EQ("ns2", stack.NsForPrefix("pre2").first);
  EXPECT_EQ("ns2", stack.NsForPrefix("pre3").first);
  EXPECT_EQ("ns4", stack.NsForPrefix("pre4").first);
  EXPECT_EQ("", stack.NsForPrefix("pre5").first);
  EXPECT_FALSE(stack.NsForPrefix("pre5").second);
  EXPECT_EQ("", stack.NsForPrefix("").first);
  EXPECT_TRUE(stack.NsForPrefix("").second);

  // pop frame with three defs
  stack.PopFrame();
  EXPECT_EQ("ns1", stack.NsForPrefix("pre1").first);
  EXPECT_EQ("ns2", stack.NsForPrefix("pre2").first);
  EXPECT_EQ("", stack.NsForPrefix("pre3").first);
  EXPECT_FALSE(stack.NsForPrefix("pre3").second);
  EXPECT_EQ("", stack.NsForPrefix("pre4").first);
  EXPECT_FALSE(stack.NsForPrefix("pre4").second);
  EXPECT_EQ("", stack.NsForPrefix("pre5").first);
  EXPECT_FALSE(stack.NsForPrefix("pre5").second);
  EXPECT_EQ("", stack.NsForPrefix("").first);
  EXPECT_TRUE(stack.NsForPrefix("").second);

  // pop frame with last two defs
  stack.PopFrame();
  EXPECT_FALSE(stack.NsForPrefix("pre1").second);
  EXPECT_FALSE(stack.NsForPrefix("pre2").second);
  EXPECT_FALSE(stack.NsForPrefix("pre3").second);
  EXPECT_FALSE(stack.NsForPrefix("pre4").second);
  EXPECT_FALSE(stack.NsForPrefix("pre5").second);
  EXPECT_TRUE(stack.NsForPrefix("").second);
  EXPECT_EQ("", stack.NsForPrefix("pre1").first);
  EXPECT_EQ("", stack.NsForPrefix("pre2").first);
  EXPECT_EQ("", stack.NsForPrefix("pre3").first);
  EXPECT_EQ("", stack.NsForPrefix("pre4").first);
  EXPECT_EQ("", stack.NsForPrefix("pre5").first);
  EXPECT_EQ("", stack.NsForPrefix("").first);
}

TEST(XmlnsStackTest, TestAddNewPrefix) {
  XmlnsStack stack;

  // builtin namespaces cannot be added
  EXPECT_FALSE(stack.AddNewPrefix("", true).second);
  EXPECT_FALSE(stack.AddNewPrefix("", false).second);
  EXPECT_FALSE(stack.AddNewPrefix(NS_XML, true).second);
  EXPECT_FALSE(stack.AddNewPrefix(NS_XML, false).second);
  EXPECT_FALSE(stack.AddNewPrefix(NS_XMLNS, true).second);
  EXPECT_FALSE(stack.AddNewPrefix(NS_XMLNS, false).second);

  // namespaces already added cannot be added again.
  EXPECT_EQ("foo", stack.AddNewPrefix("http://a.b.com/foo.htm", true).first);
  EXPECT_EQ("bare", stack.AddNewPrefix("http://a.b.com/bare", false).first);
  EXPECT_EQ("z", stack.AddNewPrefix("z", false).first);
  EXPECT_FALSE(stack.AddNewPrefix("http://a.b.com/foo.htm", true).second);
  EXPECT_FALSE(stack.AddNewPrefix("http://a.b.com/bare", true).second);
  EXPECT_FALSE(stack.AddNewPrefix("z", true).second);
  EXPECT_FALSE(stack.AddNewPrefix("http://a.b.com/foo.htm", false).second);
  EXPECT_FALSE(stack.AddNewPrefix("http://a.b.com/bare", false).second);
  EXPECT_FALSE(stack.AddNewPrefix("z", false).second);

  // default namespace usable by non-attributes only
  stack.AddXmlns("", "http://my/default");
  EXPECT_FALSE(stack.AddNewPrefix("http://my/default", false).second);
  EXPECT_EQ("def", stack.AddNewPrefix("http://my/default", true).first);

  // namespace cannot start with 'xml'
  EXPECT_EQ("ns", stack.AddNewPrefix("http://a.b.com/xmltest", true).first);
  EXPECT_EQ("ns2", stack.AddNewPrefix("xmlagain", false).first);

  // verify added namespaces are still defined
  EXPECT_EQ("http://a.b.com/foo.htm", stack.NsForPrefix("foo").first);
  EXPECT_TRUE(stack.NsForPrefix("foo").second);
  EXPECT_EQ("http://a.b.com/bare", stack.NsForPrefix("bare").first);
  EXPECT_TRUE(stack.NsForPrefix("bare").second);
  EXPECT_EQ("z", stack.NsForPrefix("z").first);
  EXPECT_TRUE(stack.NsForPrefix("z").second);
  EXPECT_EQ("http://my/default", stack.NsForPrefix("").first);
  EXPECT_TRUE(stack.NsForPrefix("").second);
  EXPECT_EQ("http://my/default", stack.NsForPrefix("def").first);
  EXPECT_TRUE(stack.NsForPrefix("def").second);
  EXPECT_EQ("http://a.b.com/xmltest", stack.NsForPrefix("ns").first);
  EXPECT_TRUE(stack.NsForPrefix("ns").second);
  EXPECT_EQ("xmlagain", stack.NsForPrefix("ns2").first);
  EXPECT_TRUE(stack.NsForPrefix("ns2").second);
}

TEST(XmlnsStackTest, TestFormatQName) {
  XmlnsStack stack;
  stack.AddXmlns("pre1", "ns1");
  stack.AddXmlns("pre2", "ns2");
  stack.AddXmlns("pre1", "ns3");
  stack.AddXmlns("", "ns4");

  EXPECT_EQ("zip",
      stack.FormatQName(QName("ns1", "zip"), false));  // no match
  EXPECT_EQ("pre2:abracadabra",
      stack.FormatQName(QName("ns2", "abracadabra"), false));
  EXPECT_EQ("pre1:a",
      stack.FormatQName(QName("ns3", "a"), false));
  EXPECT_EQ("simple",
      stack.FormatQName(QName("ns4", "simple"), false));
  EXPECT_EQ("root",
      stack.FormatQName(QName("", "root"), false));  // no match

  EXPECT_EQ("zip",
      stack.FormatQName(QName("ns1", "zip"), true));  // no match
  EXPECT_EQ("pre2:abracadabra",
      stack.FormatQName(QName("ns2", "abracadabra"), true));
  EXPECT_EQ("pre1:a",
      stack.FormatQName(QName("ns3", "a"), true));
  EXPECT_EQ("simple",
      stack.FormatQName(QName("ns4", "simple"), true));  // no match
  EXPECT_EQ("root",
      stack.FormatQName(QName("", "root"), true));
}
