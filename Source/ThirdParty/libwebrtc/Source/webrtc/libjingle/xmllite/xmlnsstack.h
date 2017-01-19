/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMLLITE_XMLNSSTACK_H_
#define WEBRTC_LIBJINGLE_XMLLITE_XMLNSSTACK_H_

#include <memory>
#include <string>
#include <vector>
#include "webrtc/libjingle/xmllite/qname.h"

namespace buzz {

class XmlnsStack {
public:
  XmlnsStack();
  ~XmlnsStack();

  void AddXmlns(const std::string& prefix, const std::string& ns);
  void RemoveXmlns();
  void PushFrame();
  void PopFrame();
  void Reset();

  std::pair<std::string, bool> NsForPrefix(const std::string& prefix);
  bool PrefixMatchesNs(const std::string & prefix, const std::string & ns);
  std::pair<std::string, bool> PrefixForNs(const std::string& ns, bool isAttr);
  std::pair<std::string, bool> AddNewPrefix(const std::string& ns, bool isAttr);
  std::string FormatQName(const QName & name, bool isAttr);

private:

  std::unique_ptr<std::vector<std::string> > pxmlnsStack_;
  std::unique_ptr<std::vector<size_t> > pxmlnsDepthStack_;
};
}

#endif  // WEBRTC_LIBJINGLE_XMLLITE_XMLNSSTACK_H_
