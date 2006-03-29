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
#include "DOMImplementation.h"

#include "DocumentType.h"
#include "Element.h"
#include "ExceptionCode.h"
#include "PlatformString.h"
#include "css_stylesheetimpl.h"
#include "HTMLDocument.h"

namespace WebCore {

// FIXME: An implementation of this is still waiting for me to understand the distinction between
// a "malformed" qualified name and one with bad characters in it. For example, is a second colon
// an illegal character or a malformed qualified name? This will determine both what parameters
// this function needs to take and exactly what it will do. Should also be exported so that
// Element can use it too.
static bool qualifiedNameIsMalformed(const String&)
{
    return false;
}

bool DOMImplementation::hasFeature (const String& feature, const String& version) const
{
    String lower = feature.lower();
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

PassRefPtr<DocumentType> DOMImplementation::createDocumentType(const String& qualifiedName,
    const String& publicId, const String& systemId, ExceptionCode& ec)
{
    // Not mentioned in spec: throw NAMESPACE_ERR if no qualifiedName supplied
    if (qualifiedName.isNull()) {
        ec = NAMESPACE_ERR;
        return 0;
    }

    // INVALID_CHARACTER_ERR: Raised if the specified qualified name contains an illegal character.
    String prefix, localName;
    if (!Document::parseQualifiedName(qualifiedName, prefix, localName)) {
        ec = INVALID_CHARACTER_ERR;
        return 0;
    }

    // NAMESPACE_ERR: Raised if the qualifiedName is malformed.
    if (qualifiedNameIsMalformed(qualifiedName)) {
        ec = NAMESPACE_ERR;
        return 0;
    }

    ec = 0;
    return new DocumentType(this, 0, qualifiedName, publicId, systemId);
}

DOMImplementation* DOMImplementation::getInterface(const String& /*feature*/) const
{
    // ###
    return 0;
}

PassRefPtr<Document> DOMImplementation::createDocument(const String& namespaceURI,
    const String& qualifiedName, DocumentType* doctype, ExceptionCode& ec)
{
    if (!qualifiedName.isEmpty()) {
        // INVALID_CHARACTER_ERR: Raised if the specified qualified name contains an illegal character.
        String prefix, localName;
        if (!Document::parseQualifiedName(qualifiedName, prefix, localName)) {
            ec = INVALID_CHARACTER_ERR;
            return 0;
        }

        // NAMESPACE_ERR:
        // - Raised if the qualifiedName is malformed,
        // - if the qualifiedName has a prefix and the namespaceURI is null, or
        // - if the qualifiedName has a prefix that is "xml" and the namespaceURI is different
        //   from "http://www.w3.org/XML/1998/namespace" [Namespaces].
        int colonpos = -1;
        unsigned i;
        StringImpl *qname = qualifiedName.impl();
        for (i = 0; i < qname->length() && colonpos < 0; i++) {
            if ((*qname)[i] == ':')
                colonpos = i;
        }
    
        if (qualifiedNameIsMalformed(qualifiedName) ||
            (colonpos >= 0 && namespaceURI.isNull()) ||
            (colonpos == 3 && qualifiedName[0] == 'x' && qualifiedName[1] == 'm' && qualifiedName[2] == 'l' &&
             namespaceURI != "http://www.w3.org/XML/1998/namespace")) {

            ec = NAMESPACE_ERR;
            return 0;
        }
    }
    
    // WRONG_DOCUMENT_ERR: Raised if doctype has already been used with a different document or was
    // created from a different implementation.
    if (doctype && (doctype->document() || doctype->implementation() != this)) {
        ec = WRONG_DOCUMENT_ERR;
        return 0;
    }

    // ### this is completely broken.. without a view it will not work (Dirk)
    RefPtr<Document> doc = new Document(this, 0);

    // now get the interesting parts of the doctype
    if (doctype)
        doc->setDocType(new DocumentType(doc.get(), *doctype));

    if (!qualifiedName.isEmpty())
        doc->addChild(doc->createElementNS(namespaceURI, qualifiedName, ec));
    
    ec = 0;
    return doc.release();
}

PassRefPtr<CSSStyleSheet> DOMImplementation::createCSSStyleSheet(const String&, const String& media, ExceptionCode& ec)
{
    // ### TODO : title should be set, and media could have wrong syntax, in which case we should generate an exception.
    ec = 0;
    CSSStyleSheet* const nullSheet = 0;
    RefPtr<CSSStyleSheet> sheet = new CSSStyleSheet(nullSheet);
    sheet->setMedia(new MediaList(sheet.get(), media));
    return sheet.release();
}

PassRefPtr<Document> DOMImplementation::createDocument(FrameView* v)
{
    return new Document(this, v);
}

PassRefPtr<HTMLDocument> DOMImplementation::createHTMLDocument(FrameView* v)
{
    return new HTMLDocument(this, v);
}

DOMImplementation* DOMImplementation::instance()
{
    static RefPtr<DOMImplementation> i = new DOMImplementation;
    return i.get();
}

bool DOMImplementation::isXMLMIMEType(const String& mimeType)
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

PassRefPtr<HTMLDocument> DOMImplementation::createHTMLDocument(const String& title)
{
    RefPtr<HTMLDocument> d = createHTMLDocument(0 /* ### create a view otherwise it doesn't work */);
    d->open();
    // FIXME: Need to escape special characters in the title?
    d->write("<html><head><title>" + title.deprecatedString() + "</title></head>");
    return d.release();
}

}
