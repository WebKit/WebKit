/**
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
 */

#include "config.h"
#include "DOMImplementationImpl.h"
#include "DocumentTypeImpl.h"
#include "html_documentimpl.h"

#include "PlatformString.h"
#include "dom_exception.h"
#include "css_stylesheetimpl.h"

namespace DOM {

// FIXME: An implementation of this is still waiting for me to understand the distinction between
// a "malformed" qualified name and one with bad characters in it. For example, is a second colon
// an illegal character or a malformed qualified name? This will determine both what parameters
// this function needs to take and exactly what it will do. Should also be exported so that
// ElementImpl can use it too.
static bool qualifiedNameIsMalformed(const DOMString &)
{
    return false;
}

DOMImplementationImpl::DOMImplementationImpl()
{
}

DOMImplementationImpl::~DOMImplementationImpl()
{
}

bool DOMImplementationImpl::hasFeature (const DOMString &feature, const DOMString &version) const
{
    DOMString lower = feature.lower();
    if (lower == "core" || lower == "html" || lower == "xml" || lower == "xhtml")
        return version.isEmpty() || version == "1.0" || version == "2.0";
    if (lower == "css"
            || lower == "css2"
            || lower == "events"
            || lower == "htmlevents"
            || lower == "mouseevents"
            || lower == "mutationevents"
            || lower == "range"
            || lower == "stylesheets"
            || lower == "traversal"
            || lower == "uievents"
            || lower == "views")
        return version.isEmpty() || version == "2.0";
    return false;
}

DocumentTypeImpl *DOMImplementationImpl::createDocumentType( const DOMString &qualifiedName, const DOMString &publicId,
                                                             const DOMString &systemId, int &exceptioncode )
{
    // Not mentioned in spec: throw NAMESPACE_ERR if no qualifiedName supplied
    if (qualifiedName.isNull()) {
        exceptioncode = DOMException::NAMESPACE_ERR;
        return 0;
    }

    // INVALID_CHARACTER_ERR: Raised if the specified qualified name contains an illegal character.
    DOMString prefix, localName;
    if (!DocumentImpl::parseQualifiedName(qualifiedName, prefix, localName)) {
        exceptioncode = DOMException::INVALID_CHARACTER_ERR;
        return 0;
    }

    // NAMESPACE_ERR: Raised if the qualifiedName is malformed.
    if (qualifiedNameIsMalformed(qualifiedName)) {
        exceptioncode = DOMException::NAMESPACE_ERR;
        return 0;
    }

    return new DocumentTypeImpl(this, 0, qualifiedName, publicId, systemId);
}

DOMImplementationImpl* DOMImplementationImpl::getInterface(const DOMString& /*feature*/) const
{
    // ###
    return 0;
}

DocumentImpl *DOMImplementationImpl::createDocument( const DOMString &namespaceURI, const DOMString &qualifiedName,
                                                     DocumentTypeImpl *doctype, int &exceptioncode )
{
    exceptioncode = 0;

    if (!qualifiedName.isEmpty()) {
        // INVALID_CHARACTER_ERR: Raised if the specified qualified name contains an illegal character.
        DOMString prefix, localName;
        if (!DocumentImpl::parseQualifiedName(qualifiedName, prefix, localName)) {
            exceptioncode = DOMException::INVALID_CHARACTER_ERR;
            return 0;
        }

        // NAMESPACE_ERR:
        // - Raised if the qualifiedName is malformed,
        // - if the qualifiedName has a prefix and the namespaceURI is null, or
        // - if the qualifiedName has a prefix that is "xml" and the namespaceURI is different
        //   from "http://www.w3.org/XML/1998/namespace" [Namespaces].
        int colonpos = -1;
        uint i;
        DOMStringImpl *qname = qualifiedName.impl();
        for (i = 0; i < qname->l && colonpos < 0; i++) {
            if ((*qname)[i] == ':')
                colonpos = i;
        }
    
        if (qualifiedNameIsMalformed(qualifiedName) ||
            (colonpos >= 0 && namespaceURI.isNull()) ||
            (colonpos == 3 && qualifiedName[0] == 'x' && qualifiedName[1] == 'm' && qualifiedName[2] == 'l' &&
             namespaceURI != "http://www.w3.org/XML/1998/namespace")) {

            exceptioncode = DOMException::NAMESPACE_ERR;
            return 0;
        }
    }
    
    // WRONG_DOCUMENT_ERR: Raised if doctype has already been used with a different document or was
    // created from a different implementation.
    if (doctype && (doctype->getDocument() || doctype->implementation() != this)) {
        exceptioncode = DOMException::WRONG_DOCUMENT_ERR;
        return 0;
    }

    // ### this is completely broken.. without a view it will not work (Dirk)
    DocumentImpl *doc = new DocumentImpl(this, 0);

    // now get the interesting parts of the doctype
    if (doctype)
        doc->setDocType(new DocumentTypeImpl(doc, *doctype));

    if (!qualifiedName.isEmpty())
        doc->addChild(doc->createElementNS(namespaceURI, qualifiedName, exceptioncode));
    
    return doc;
}

CSSStyleSheetImpl *DOMImplementationImpl::createCSSStyleSheet(const DOMString &/*title*/, const DOMString &media, int &/*exception*/)
{
    // ### TODO : title should be set, and media could have wrong syntax, in which case we should generate an exception.
    CSSStyleSheetImpl * const nullSheet = 0;
    CSSStyleSheetImpl *sheet = new CSSStyleSheetImpl(nullSheet);
    sheet->setMedia(new MediaListImpl(sheet, media));
    return sheet;
}

DocumentImpl *DOMImplementationImpl::createDocument( FrameView *v )
{
    return new DocumentImpl(this, v);
}

HTMLDocumentImpl *DOMImplementationImpl::createHTMLDocument( FrameView *v )
{
    return new HTMLDocumentImpl(this, v);
}

DOMImplementationImpl* DOMImplementationImpl::instance()
{
    static RefPtr<DOMImplementationImpl> i = new DOMImplementationImpl;
    return i.get();
}

bool DOMImplementationImpl::isXMLMIMEType(const DOMString& mimeType)
{
    if (mimeType == "text/xml" || mimeType == "application/xml" || mimeType == "application/xhtml+xml" ||
        mimeType == "text/xsl" || mimeType == "application/rss+xml" || mimeType == "application/atom+xml"
#if SVG_SUPPORT
        || mimeType == "image/svg+xml"
#endif
        )
        return true;
    return false;
}

HTMLDocumentImpl *DOMImplementationImpl::createHTMLDocument(const DOMString &title)
{
    HTMLDocumentImpl *d = createHTMLDocument( 0 /* ### create a view otherwise it doesn't work */);
    d->open();
    // FIXME: Need to escape special characters in the title?
    d->write("<html><head><title>" + title.qstring() + "</title></head>");
    return d;
}

};