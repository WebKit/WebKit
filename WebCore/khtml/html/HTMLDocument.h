/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2006 Apple Computer, Inc.
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

#ifndef HTML_DOCUMENTIMPL_H
#define HTML_DOCUMENTIMPL_H

#include "CachedObjectClient.h"
#include "Document.h"
#include "HTMLCollection.h"
#include <kxmlcore/HashMap.h>

class DeprecatedString;

namespace WebCore {

class FrameView;
class HTMLElement;

class HTMLDocument : public WebCore::Document, public WebCore::CachedObjectClient
{
public:
    HTMLDocument(DOMImplementation *_implementation, FrameView *v = 0);
    ~HTMLDocument();

    virtual bool isHTMLDocument() const { return true; }
    virtual Element* documentElement() const;

    String lastModified() const;
    String cookie() const;
    void setCookie(const String&);

    void setBody(HTMLElement*, ExceptionCode& ec);

    virtual Tokenizer* createTokenizer();

    virtual bool childAllowed(Node*);

    virtual PassRefPtr<Element> createElement(const String& tagName, ExceptionCode&);

    virtual void determineParseMode(const DeprecatedString&);

    void addNamedItem(const String& name);
    void removeNamedItem(const String& name);
    bool hasNamedItem(const String& name);

    void addDocExtraNamedItem(const String& name);
    void removeDocExtraNamedItem(const String& name);
    bool hasDocExtraNamedItem(const String& name);

    HTMLCollection::CollectionInfo* collectionInfo(int type)
    { 
        if (type < HTMLCollection::NUM_CACHEABLE_TYPES) 
            return m_collection_info + type; 
        return 0;
    }

    virtual DocumentType* doctype() const;

    typedef HashMap<StringImpl*, int> NameCountMap;

protected:
    HTMLElement* bodyElement;
    HTMLElement* htmlElement;

private:
    HTMLCollection::CollectionInfo m_collection_info[HTMLCollection::NUM_CACHEABLE_TYPES];

    NameCountMap namedItemCounts;
    NameCountMap docExtraNamedItemCounts;
};

} //namespace

#endif
