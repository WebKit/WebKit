/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef HTMLDocument_h
#define HTMLDocument_h

#include "CachedResourceClient.h"
#include "Document.h"

namespace WebCore {

class FrameView;
class HTMLElement;

class HTMLDocument : public Document, public CachedResourceClient {
public:
    HTMLDocument(DOMImplementation*, FrameView* = 0);
    virtual ~HTMLDocument();

    virtual bool isHTMLDocument() const { return true; }

    String lastModified() const;
    String cookie() const;
    void setCookie(const String&);

    void setBody(HTMLElement*, ExceptionCode&);

    virtual Tokenizer* createTokenizer();

    virtual bool childAllowed(Node*);

    virtual PassRefPtr<Element> createElement(const String& tagName, ExceptionCode&);

    virtual void determineParseMode(const String&);

    void addNamedItem(const String& name);
    void removeNamedItem(const String& name);
    bool hasNamedItem(const String& name);

    void addDocExtraNamedItem(const String& name);
    void removeDocExtraNamedItem(const String& name);
    bool hasDocExtraNamedItem(const String& name);

    typedef HashMap<StringImpl*, int> NameCountMap;

private:
    NameCountMap namedItemCounts;
    NameCountMap docExtraNamedItemCounts;
};

} // namespace

#endif
