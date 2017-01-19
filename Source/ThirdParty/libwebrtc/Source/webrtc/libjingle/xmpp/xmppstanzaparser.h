/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_XMPPSTANZAPARSER_H_
#define WEBRTC_LIBJINGLE_XMPP_XMPPSTANZAPARSER_H_

#include "webrtc/libjingle/xmllite/xmlbuilder.h"
#include "webrtc/libjingle/xmllite/xmlparser.h"


namespace buzz {

class XmlElement;

class XmppStanzaParseHandler {
public:
  virtual ~XmppStanzaParseHandler() {}
  virtual void StartStream(const XmlElement * pelStream) = 0;
  virtual void Stanza(const XmlElement * pelStanza) = 0;
  virtual void EndStream() = 0;
  virtual void XmlError() = 0;
};

class XmppStanzaParser {
public:
  XmppStanzaParser(XmppStanzaParseHandler *psph);
  bool Parse(const char * data, size_t len, bool isFinal)
    { return parser_.Parse(data, len, isFinal); }
  void Reset();

private:
  class ParseHandler : public XmlParseHandler {
  public:
    ParseHandler(XmppStanzaParser * outer) : outer_(outer) {}
    virtual void StartElement(XmlParseContext * pctx,
               const char * name, const char ** atts)
      { outer_->IncomingStartElement(pctx, name, atts); }
    virtual void EndElement(XmlParseContext * pctx,
               const char * name)
      { outer_->IncomingEndElement(pctx, name); }
    virtual void CharacterData(XmlParseContext * pctx,
               const char * text, int len)
      { outer_->IncomingCharacterData(pctx, text, len); }
    virtual void Error(XmlParseContext * pctx,
               XML_Error errCode)
      { outer_->IncomingError(pctx, errCode); }
  private:
    XmppStanzaParser * const outer_;
  };

  friend class ParseHandler;

  void IncomingStartElement(XmlParseContext * pctx,
               const char * name, const char ** atts);
  void IncomingEndElement(XmlParseContext * pctx,
               const char * name);
  void IncomingCharacterData(XmlParseContext * pctx,
               const char * text, int len);
  void IncomingError(XmlParseContext * pctx,
               XML_Error errCode);

  XmppStanzaParseHandler * psph_;
  ParseHandler innerHandler_;
  XmlParser parser_;
  int depth_;
  XmlBuilder builder_;

 };


}

#endif  // WEBRTC_LIBJINGLE_XMPP_XMPPSTANZAPARSER_H_
