/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include "XMLDocument.h"

namespace WebCore {

class DOMImplementation : public ScriptWrappable {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit DOMImplementation(Document&);

    void ref() { m_document.ref(); }
    void deref() { m_document.deref(); }
    Document& document() { return m_document; }

    // DOM methods & attributes for DOMImplementation
    WEBCORE_EXPORT static bool hasFeature(const String& feature, const String& version);
    WEBCORE_EXPORT RefPtr<DocumentType> createDocumentType(const String& qualifiedName, const String& publicId, const String& systemId, ExceptionCode&);
    WEBCORE_EXPORT RefPtr<XMLDocument> createDocument(const String& namespaceURI, const String& qualifiedName, DocumentType*, ExceptionCode&);

    DOMImplementation* getInterface(const String& feature);

    // From the DOMImplementationCSS interface
    WEBCORE_EXPORT static Ref<CSSStyleSheet> createCSSStyleSheet(const String& title, const String& media, ExceptionCode&);

    // From the HTMLDOMImplementation interface
    WEBCORE_EXPORT Ref<HTMLDocument> createHTMLDocument(const String& title);

    // Other methods (not part of DOM)
    static Ref<Document> createDocument(const String& MIMEType, Frame*, const URL&);

    WEBCORE_EXPORT static bool isXMLMIMEType(const String& MIMEType);
    WEBCORE_EXPORT static bool isTextMIMEType(const String& MIMEType);

private:
    Document& m_document;
};

} // namespace WebCore
