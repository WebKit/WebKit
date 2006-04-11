/*
 * Copyright (C) 2006 Apple Computer, Inc.
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

#ifndef DOMImplementationFront_h
#define DOMImplementationFront_h

// FIXME: This source file exists to work around a problem that occurs trying
// to mix DOMImplementation and WebCore::DOMImplementation in DOM.mm.
// It seems to affect only older versions of gcc. Once the buildbot is upgraded,
// we should consider increasing the minimum required version of gcc to build
// WebCore, and rolling the change that introduced this file back.

#include <kxmlcore/Forward.h>

namespace WebCore {

class CSSStyleSheet;
class Document;
class DocumentType;
class HTMLDocument;
class String;

typedef int ExceptionCode;

class DOMImplementationFront {
public:
    void ref();
    void deref();
    bool hasFeature(const String& feature, const String& version) const;
    PassRefPtr<DocumentType> createDocumentType(const String& qualifiedName, const String& publicId, const String& systemId, ExceptionCode&);
    PassRefPtr<Document> createDocument(const String& namespaceURI, const String& qualifiedName, DocumentType*, ExceptionCode&);
    DOMImplementationFront* getInterface(const String& feature) const;
    PassRefPtr<CSSStyleSheet> createCSSStyleSheet(const String& title, const String& media, ExceptionCode&);
    PassRefPtr<HTMLDocument> createHTMLDocument(const String& title);
};

DOMImplementationFront* implementationFront(Document*);

} //namespace

#endif
