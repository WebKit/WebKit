/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/libjingle/xmllite/xmlbuilder.h"

#include <set>
#include <vector>
#include "webrtc/libjingle/xmllite/xmlconstants.h"
#include "webrtc/libjingle/xmllite/xmlelement.h"
#include "webrtc/base/common.h"

namespace buzz {

XmlBuilder::XmlBuilder() :
  pelCurrent_(NULL),
  pelRoot_(),
  pvParents_(new std::vector<XmlElement *>()) {
}

void
XmlBuilder::Reset() {
  pelRoot_.reset();
  pelCurrent_ = NULL;
  pvParents_->clear();
}

XmlElement *
XmlBuilder::BuildElement(XmlParseContext * pctx,
                              const char * name, const char ** atts) {
  QName tagName(pctx->ResolveQName(name, false));
  if (tagName.IsEmpty())
    return NULL;

  XmlElement * pelNew = new XmlElement(tagName);

  if (!*atts)
    return pelNew;

  std::set<QName> seenNonlocalAtts;

  while (*atts) {
    QName attName(pctx->ResolveQName(*atts, true));
    if (attName.IsEmpty()) {
      delete pelNew;
      return NULL;
    }

    // verify that namespaced names are unique
    if (!attName.Namespace().empty()) {
      if (seenNonlocalAtts.count(attName)) {
        delete pelNew;
        return NULL;
      }
      seenNonlocalAtts.insert(attName);
    }

    pelNew->AddAttr(attName, std::string(*(atts + 1)));
    atts += 2;
  }

  return pelNew;
}

void
XmlBuilder::StartElement(XmlParseContext * pctx,
                              const char * name, const char ** atts) {
  XmlElement * pelNew = BuildElement(pctx, name, atts);
  if (pelNew == NULL) {
    pctx->RaiseError(XML_ERROR_SYNTAX);
    return;
  }

  if (!pelCurrent_) {
    pelCurrent_ = pelNew;
    pelRoot_.reset(pelNew);
    pvParents_->push_back(NULL);
  } else {
    pelCurrent_->AddElement(pelNew);
    pvParents_->push_back(pelCurrent_);
    pelCurrent_ = pelNew;
  }
}

void
XmlBuilder::EndElement(XmlParseContext * pctx, const char * name) {
  RTC_UNUSED(pctx);
  RTC_UNUSED(name);
  pelCurrent_ = pvParents_->back();
  pvParents_->pop_back();
}

void
XmlBuilder::CharacterData(XmlParseContext * pctx,
                               const char * text, int len) {
  RTC_UNUSED(pctx);
  if (pelCurrent_) {
    pelCurrent_->AddParsedText(text, len);
  }
}

void
XmlBuilder::Error(XmlParseContext * pctx, XML_Error err) {
  RTC_UNUSED(pctx);
  RTC_UNUSED(err);
  pelRoot_.reset(NULL);
  pelCurrent_ = NULL;
  pvParents_->clear();
}

XmlElement *
XmlBuilder::CreateElement() {
  return pelRoot_.release();
}

XmlElement *
XmlBuilder::BuiltElement() {
  return pelRoot_.get();
}

XmlBuilder::~XmlBuilder() {
}

}  // namespace buzz
