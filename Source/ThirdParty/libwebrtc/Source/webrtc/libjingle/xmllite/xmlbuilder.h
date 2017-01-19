/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef _xmlbuilder_h_
#define _xmlbuilder_h_

#include <memory>
#include <string>
#include <vector>
#include "webrtc/libjingle/xmllite/xmlparser.h"

#ifdef EXPAT_RELATIVE_PATH
#include "expat.h"
#else
#include "third_party/expat/v2_0_1/Source/lib/expat.h"
#endif  // EXPAT_RELATIVE_PATH

namespace buzz {

class XmlElement;
class XmlParseContext;


class XmlBuilder : public XmlParseHandler {
public:
  XmlBuilder();

  static XmlElement * BuildElement(XmlParseContext * pctx,
                                  const char * name, const char ** atts);
  virtual void StartElement(XmlParseContext * pctx,
                            const char * name, const char ** atts);
  virtual void EndElement(XmlParseContext * pctx, const char * name);
  virtual void CharacterData(XmlParseContext * pctx,
                             const char * text, int len);
  virtual void Error(XmlParseContext * pctx, XML_Error);
  virtual ~XmlBuilder();

  void Reset();

  // Take ownership of the built element; second call returns NULL
  XmlElement * CreateElement();

  // Peek at the built element without taking ownership
  XmlElement * BuiltElement();

private:
  XmlElement * pelCurrent_;
  std::unique_ptr<XmlElement> pelRoot_;
  std::unique_ptr<std::vector<XmlElement*> > pvParents_;
};

}

#endif
