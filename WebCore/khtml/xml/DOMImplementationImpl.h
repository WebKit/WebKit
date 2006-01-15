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

#include "shared.h"

class KHTMLView;

namespace DOM {

class DOMString;
class CSSStyleSheetImpl;
class DocumentImpl;
class DocumentTypeImpl;
class HTMLDocumentImpl;

class DOMImplementationImpl : public khtml::Shared<DOMImplementationImpl>
{
public:
    DOMImplementationImpl();
    ~DOMImplementationImpl();

    // DOM methods & attributes for DOMImplementation
    bool hasFeature(const DOMString& feature, const DOMString& version) const;
    DocumentTypeImpl *createDocumentType( const DOMString &qualifiedName, const DOMString &publicId,
                                          const DOMString &systemId, int &exceptioncode );
    DocumentImpl *createDocument( const DOMString &namespaceURI, const DOMString &qualifiedName,
                                  DocumentTypeImpl *doctype, int &exceptioncode );

    DOMImplementationImpl* getInterface(const DOMString& feature) const;

    // From the DOMImplementationCSS interface
    CSSStyleSheetImpl *createCSSStyleSheet(const DOMString &title, const DOMString &media, int &exceptioncode);

    // From the HTMLDOMImplementation interface
    HTMLDocumentImpl* createHTMLDocument( const DOMString& title);

    // Other methods (not part of DOM)
    DocumentImpl *createDocument( KHTMLView *v = 0 );
    HTMLDocumentImpl *createHTMLDocument( KHTMLView *v = 0 );

    // Returns the static instance of this class - only one instance of this class should
    // ever be present, and is used as a factory method for creating DocumentImpl objects
    static DOMImplementationImpl *instance();
    
    static bool isXMLMIMEType(const DOMString& mimeType);

protected:
    static DOMImplementationImpl *m_instance;
};

} //namespace

#endif
