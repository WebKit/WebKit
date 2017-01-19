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
#include <vector>

#include "webrtc/libjingle/xmllite/xmlconstants.h"
#include "webrtc/libjingle/xmllite/xmlelement.h"
#include "webrtc/libjingle/xmllite/xmlnsstack.h"

namespace buzz {

class XmlPrinterImpl {
public:
  XmlPrinterImpl(std::ostream* pout, XmlnsStack* ns_stack);
  void PrintElement(const XmlElement* element);
  void PrintQuotedValue(const std::string& text);
  void PrintBodyText(const std::string& text);
  void PrintCDATAText(const std::string& text);

private:
  std::ostream *pout_;
  XmlnsStack* ns_stack_;
};

void XmlPrinter::PrintXml(std::ostream* pout, const XmlElement* element) {
  XmlnsStack ns_stack;
  PrintXml(pout, element, &ns_stack);
}

void XmlPrinter::PrintXml(std::ostream* pout, const XmlElement* element,
                          XmlnsStack* ns_stack) {
  XmlPrinterImpl printer(pout, ns_stack);
  printer.PrintElement(element);
}

XmlPrinterImpl::XmlPrinterImpl(std::ostream* pout, XmlnsStack* ns_stack)
    : pout_(pout),
      ns_stack_(ns_stack) {
}

void XmlPrinterImpl::PrintElement(const XmlElement* element) {
  ns_stack_->PushFrame();

  // first go through attrs of pel to add xmlns definitions
  const XmlAttr* attr;
  for (attr = element->FirstAttr(); attr; attr = attr->NextAttr()) {
    if (attr->Name() == QN_XMLNS) {
      ns_stack_->AddXmlns(STR_EMPTY, attr->Value());
    } else if (attr->Name().Namespace() == NS_XMLNS) {
      ns_stack_->AddXmlns(attr->Name().LocalPart(),
                          attr->Value());
    }
  }

  // then go through qnames to make sure needed xmlns definitons are added
  std::vector<std::string> new_ns;
  std::pair<std::string, bool> prefix;
  prefix = ns_stack_->AddNewPrefix(element->Name().Namespace(), false);
  if (prefix.second) {
    new_ns.push_back(prefix.first);
    new_ns.push_back(element->Name().Namespace());
  }

  for (attr = element->FirstAttr(); attr; attr = attr->NextAttr()) {
    prefix = ns_stack_->AddNewPrefix(attr->Name().Namespace(), true);
    if (prefix.second) {
      new_ns.push_back(prefix.first);
      new_ns.push_back(attr->Name().Namespace());
    }
  }

  // print the element name
  *pout_ << '<' << ns_stack_->FormatQName(element->Name(), false);

  // and the attributes
  for (attr = element->FirstAttr(); attr; attr = attr->NextAttr()) {
    *pout_ << ' ' << ns_stack_->FormatQName(attr->Name(), true) << "=\"";
    PrintQuotedValue(attr->Value());
    *pout_ << '"';
  }

  // and the extra xmlns declarations
  std::vector<std::string>::iterator i(new_ns.begin());
  while (i < new_ns.end()) {
    if (*i == STR_EMPTY) {
      *pout_ << " xmlns=\"" << *(i + 1) << '"';
    } else {
      *pout_ << " xmlns:" << *i << "=\"" << *(i + 1) << '"';
    }
    i += 2;
  }

  // now the children
  const XmlChild* child = element->FirstChild();

  if (child == NULL)
    *pout_ << "/>";
  else {
    *pout_ << '>';
    while (child) {
      if (child->IsText()) {
        if (element->IsCDATA()) {
          PrintCDATAText(child->AsText()->Text());
        } else {
          PrintBodyText(child->AsText()->Text());
        }
      } else {
        PrintElement(child->AsElement());
      }
      child = child->NextChild();
    }
    *pout_ << "</" << ns_stack_->FormatQName(element->Name(), false) << '>';
  }

  ns_stack_->PopFrame();
}

void XmlPrinterImpl::PrintQuotedValue(const std::string& text) {
  size_t safe = 0;
  for (;;) {
    size_t unsafe = text.find_first_of("<>&\"", safe);
    if (unsafe == std::string::npos)
      unsafe = text.length();
    *pout_ << text.substr(safe, unsafe - safe);
    if (unsafe == text.length())
      return;
    switch (text[unsafe]) {
      case '<': *pout_ << "&lt;"; break;
      case '>': *pout_ << "&gt;"; break;
      case '&': *pout_ << "&amp;"; break;
      case '"': *pout_ << "&quot;"; break;
    }
    safe = unsafe + 1;
    if (safe == text.length())
      return;
  }
}

void XmlPrinterImpl::PrintBodyText(const std::string& text) {
  size_t safe = 0;
  for (;;) {
    size_t unsafe = text.find_first_of("<>&", safe);
    if (unsafe == std::string::npos)
      unsafe = text.length();
    *pout_ << text.substr(safe, unsafe - safe);
    if (unsafe == text.length())
      return;
    switch (text[unsafe]) {
      case '<': *pout_ << "&lt;"; break;
      case '>': *pout_ << "&gt;"; break;
      case '&': *pout_ << "&amp;"; break;
    }
    safe = unsafe + 1;
    if (safe == text.length())
      return;
  }
}

void XmlPrinterImpl::PrintCDATAText(const std::string& text) {
  *pout_ << "<![CDATA[" << text << "]]>";
}

}  // namespace buzz
