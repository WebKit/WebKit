/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMLLITE_XMLPRINTER_H_
#define WEBRTC_LIBJINGLE_XMLLITE_XMLPRINTER_H_

#include <iosfwd>
#include <string>

namespace buzz {

class XmlElement;
class XmlnsStack;

class XmlPrinter {
 public:
  static void PrintXml(std::ostream* pout, const XmlElement* pelt);

  static void PrintXml(std::ostream* pout, const XmlElement* pelt,
                       XmlnsStack* ns_stack);
};

}  // namespace buzz

#endif  // WEBRTC_LIBJINGLE_XMLLITE_XMLPRINTER_H_
