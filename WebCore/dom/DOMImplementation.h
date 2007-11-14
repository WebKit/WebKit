/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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

#ifndef DOMImplementation_h
#define DOMImplementation_h

#include <wtf/RefCounted.h>
#include <wtf/Forward.h>

namespace WebCore {

class CSSStyleSheet;
class Document;
class DocumentType;
class Frame;
class HTMLDocument;
class String;

typedef int ExceptionCode;

class DOMImplementation : public RefCounted<DOMImplementation> {
public:
    virtual ~DOMImplementation(); 

    // DOM methods & attributes for DOMImplementation
    bool hasFeature(const String& feature, const String& version) const;
    PassRefPtr<DocumentType> createDocumentType(const String& qualifiedName, const String& publicId, const String &systemId, ExceptionCode&);
    PassRefPtr<Document> createDocument(const String& namespaceURI, const String& qualifiedName, DocumentType*, ExceptionCode&);

    DOMImplementation* getInterface(const String& feature) const;

    // From the DOMImplementationCSS interface
    PassRefPtr<CSSStyleSheet> createCSSStyleSheet(const String& title, const String& media, ExceptionCode&);

    // From the HTMLDOMImplementation interface
    PassRefPtr<HTMLDocument> createHTMLDocument(const String& title);

    // Other methods (not part of DOM)
    PassRefPtr<Document> createDocument(const String& MIMEType, Frame*, bool inViewSourceMode);
    PassRefPtr<Document> createDocument(Frame*);
    PassRefPtr<HTMLDocument> createHTMLDocument(Frame*);

    // Returns the static instance of this class - only one instance of this class should
    // ever be present, and is used as a factory method for creating Document objects
    static DOMImplementation* instance();

    static bool isXMLMIMEType(const String& MIMEType);
    static bool isTextMIMEType(const String& MIMEType);
};

} //namespace

#endif
