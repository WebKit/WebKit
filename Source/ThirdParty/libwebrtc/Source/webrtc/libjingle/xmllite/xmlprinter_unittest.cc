/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/libjingle/xmllite/xmlprinter.h"

#include <sstream>
#include <string>

#include "webrtc/libjingle/xmllite/qname.h"
#include "webrtc/libjingle/xmllite/xmlelement.h"
#include "webrtc/libjingle/xmllite/xmlnsstack.h"
#include "webrtc/base/common.h"
#include "webrtc/base/gunit.h"

using buzz::QName;
using buzz::XmlElement;
using buzz::XmlnsStack;
using buzz::XmlPrinter;

TEST(XmlPrinterTest, TestBasicPrinting) {
  XmlElement elt(QName("google:test", "first"));
  std::stringstream ss;
  XmlPrinter::PrintXml(&ss, &elt);
  EXPECT_EQ("<test:first xmlns:test=\"google:test\"/>", ss.str());
}

TEST(XmlPrinterTest, TestNamespacedPrinting) {
  XmlElement elt(QName("google:test", "first"));
  elt.AddElement(new XmlElement(QName("nested:test", "second")));
  std::stringstream ss;

  XmlnsStack ns_stack;
  ns_stack.AddXmlns("gg", "google:test");
  ns_stack.AddXmlns("", "nested:test");

  XmlPrinter::PrintXml(&ss, &elt, &ns_stack);
  EXPECT_EQ("<gg:first><second/></gg:first>", ss.str());
}
