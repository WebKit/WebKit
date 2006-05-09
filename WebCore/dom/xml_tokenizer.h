/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2005, 2006 Apple Computer, Inc.
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

#ifndef XML_Tokenizer_h_
#define XML_Tokenizer_h_

#include "StringHash.h"
#include <wtf/HashMap.h>

namespace WebCore {

class DocLoader;
class DocumentFragment;
class Document;
class Element;
class FrameView;
class SegmentedString;

class Tokenizer
{
public:
    Tokenizer() : m_parserStopped(false) { }
    virtual ~Tokenizer() { }

    // Script output must be prepended, while new data
    // received during executing a script must be appended, hence the
    // extra bool to be able to distinguish between both cases.
    // document.write() always uses false, while the loader uses true.
    virtual bool write(const SegmentedString&, bool appendData) = 0;
    virtual void finish() = 0;
    virtual bool isWaitingForScripts() const = 0;
    virtual void stopParsing() { m_parserStopped = true; }
    virtual bool processingData() const { return false; }
    virtual int executingScript() const { return 0; }

protected:
    // The tokenizer has buffers, so parsing may continue even after
    // it stops receiving data. We use m_parserStopped to stop the tokenizer
    // even when it has buffered data.
    bool m_parserStopped;
};

Tokenizer* newXMLTokenizer(Document*, FrameView* = 0);
#if KHTML_XSLT
void* xmlDocPtrForString(const DeprecatedString& source, const DeprecatedString& URL = DeprecatedString());
void setLoaderForLibXMLCallbacks(DocLoader*);
#endif
HashMap<String, String> parseAttributes(const String&, bool& attrsOK);
bool parseXMLDocumentFragment(const String&, DocumentFragment*, Element* parent = 0);

}

#endif
