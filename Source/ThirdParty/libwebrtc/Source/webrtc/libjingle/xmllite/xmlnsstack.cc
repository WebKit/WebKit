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

#include <sstream>
#include <string>
#include <vector>

#include "webrtc/libjingle/xmllite/xmlconstants.h"
#include "webrtc/libjingle/xmllite/xmlelement.h"

namespace buzz {

XmlnsStack::XmlnsStack() :
  pxmlnsStack_(new std::vector<std::string>),
  pxmlnsDepthStack_(new std::vector<size_t>) {
}

XmlnsStack::~XmlnsStack() {}

void XmlnsStack::PushFrame() {
  pxmlnsDepthStack_->push_back(pxmlnsStack_->size());
}

void XmlnsStack::PopFrame() {
  size_t prev_size = pxmlnsDepthStack_->back();
  pxmlnsDepthStack_->pop_back();
  if (prev_size < pxmlnsStack_->size()) {
    pxmlnsStack_->erase(pxmlnsStack_->begin() + prev_size,
                        pxmlnsStack_->end());
  }
}

std::pair<std::string, bool> XmlnsStack::NsForPrefix(
    const std::string& prefix) {
  if (prefix.length() >= 3 &&
      (prefix[0] == 'x' || prefix[0] == 'X') &&
      (prefix[1] == 'm' || prefix[1] == 'M') &&
      (prefix[2] == 'l' || prefix[2] == 'L')) {
    if (prefix == "xml")
      return std::make_pair(NS_XML, true);
    if (prefix == "xmlns")
      return std::make_pair(NS_XMLNS, true);
    // Other names with xml prefix are illegal.
    return std::make_pair(STR_EMPTY, false);
  }

  std::vector<std::string>::iterator pos;
  for (pos = pxmlnsStack_->end(); pos > pxmlnsStack_->begin(); ) {
    pos -= 2;
    if (*pos == prefix)
      return std::make_pair(*(pos + 1), true);
  }

  if (prefix == STR_EMPTY)
    return std::make_pair(STR_EMPTY, true);  // default namespace

  return std::make_pair(STR_EMPTY, false);  // none found
}

bool XmlnsStack::PrefixMatchesNs(const std::string& prefix,
                                 const std::string& ns) {
  const std::pair<std::string, bool> match = NsForPrefix(prefix);
  return match.second && (match.first == ns);
}

std::pair<std::string, bool> XmlnsStack::PrefixForNs(const std::string& ns,
                                                     bool isattr) {
  if (ns == NS_XML)
    return std::make_pair(std::string("xml"), true);
  if (ns == NS_XMLNS)
    return std::make_pair(std::string("xmlns"), true);
  if (isattr ? ns == STR_EMPTY : PrefixMatchesNs(STR_EMPTY, ns))
    return std::make_pair(STR_EMPTY, true);

  std::vector<std::string>::iterator pos;
  for (pos = pxmlnsStack_->end(); pos > pxmlnsStack_->begin(); ) {
    pos -= 2;
    if (*(pos + 1) == ns &&
        (!isattr || !pos->empty()) && PrefixMatchesNs(*pos, ns))
      return std::make_pair(*pos, true);
  }

  return std::make_pair(STR_EMPTY, false); // none found
}

std::string XmlnsStack::FormatQName(const QName& name, bool isAttr) {
  std::string prefix(PrefixForNs(name.Namespace(), isAttr).first);
  if (prefix == STR_EMPTY)
    return name.LocalPart();
  else
    return prefix + ':' + name.LocalPart();
}

void XmlnsStack::AddXmlns(const std::string & prefix, const std::string & ns) {
  pxmlnsStack_->push_back(prefix);
  pxmlnsStack_->push_back(ns);
}

void XmlnsStack::RemoveXmlns() {
  pxmlnsStack_->pop_back();
  pxmlnsStack_->pop_back();
}

static bool IsAsciiLetter(char ch) {
  return ((ch >= 'a' && ch <= 'z') ||
          (ch >= 'A' && ch <= 'Z'));
}

static std::string AsciiLower(const std::string & s) {
  std::string result(s);
  size_t i;
  for (i = 0; i < result.length(); i++) {
    if (result[i] >= 'A' && result[i] <= 'Z')
      result[i] += 'a' - 'A';
  }
  return result;
}

static std::string SuggestPrefix(const std::string & ns) {
  size_t len = ns.length();
  size_t i = ns.find_last_of('.');
  if (i != std::string::npos && len - i <= 4 + 1)
    len = i; // chop off ".html" or ".xsd" or ".?{0,4}"
  size_t last = len;
  while (last > 0) {
    last -= 1;
    if (IsAsciiLetter(ns[last])) {
      size_t first = last;
      last += 1;
      while (first > 0) {
        if (!IsAsciiLetter(ns[first - 1]))
          break;
        first -= 1;
      }
      if (last - first > 4)
        last = first + 3;
      std::string candidate(AsciiLower(ns.substr(first, last - first)));
      if (candidate.find("xml") != 0)
        return candidate;
      break;
    }
  }
  return "ns";
}

std::pair<std::string, bool> XmlnsStack::AddNewPrefix(const std::string& ns,
                                                      bool isAttr) {
  if (PrefixForNs(ns, isAttr).second)
    return std::make_pair(STR_EMPTY, false);

  std::string base(SuggestPrefix(ns));
  std::string result(base);
  int i = 2;
  while (NsForPrefix(result).second) {
    std::stringstream ss;
    ss << base;
    ss << (i++);
    ss >> result;
  }
  AddXmlns(result, ns);
  return std::make_pair(result, true);
}

void XmlnsStack::Reset() {
  pxmlnsStack_->clear();
  pxmlnsDepthStack_->clear();
}

}
