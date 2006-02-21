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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef DOM_DOMImplementationImpl_h
#define DOM_DOMImplementationImpl_h

#include "Shared.h"

namespace KXMLCore {
    template <typename T> class PassRefPtr;
}
using KXMLCore::PassRefPtr;

namespace WebCore {

class CSSStyleSheetImpl;
class DocumentImpl;
class DocumentTypeImpl;
class FrameView;
class HTMLDocumentImpl;
class String;

typedef int ExceptionCode;

class DOMImplementationImpl : public Shared<DOMImplementationImpl> {
public:
    // DOM methods & attributes for DOMImplementation
    bool hasFeature(const String& feature, const String& version) const;
    PassRefPtr<DocumentTypeImpl> createDocumentType(const String& qualifiedName, const String& publicId, const String &systemId, ExceptionCode&);
    PassRefPtr<DocumentImpl> createDocument(const String& namespaceURI, const String& qualifiedName, DocumentTypeImpl*, ExceptionCode&);

    DOMImplementationImpl* getInterface(const String& feature) const;

    // From the DOMImplementationCSS interface
    PassRefPtr<CSSStyleSheetImpl> createCSSStyleSheet(const String& title, const String& media, ExceptionCode&);

    // From the HTMLDOMImplementation interface
    PassRefPtr<HTMLDocumentImpl> createHTMLDocument(const String& title);

    // Other methods (not part of DOM)
    PassRefPtr<DocumentImpl> createDocument(FrameView* = 0);
    PassRefPtr<HTMLDocumentImpl> createHTMLDocument(FrameView* = 0);

    // Returns the static instance of this class - only one instance of this class should
    // ever be present, and is used as a factory method for creating DocumentImpl objects
    static DOMImplementationImpl* instance();

    static bool isXMLMIMEType(const String& mimeType);
};

} //namespace

#endif
