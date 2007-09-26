/*
 * This file is part of the XSL implementation.
 *
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef XSLStyleSheet_h
#define XSLStyleSheet_h

#if ENABLE(XSLT)

#include "StyleSheet.h"
#include <libxml/parser.h>
#include <libxslt/transform.h>

namespace WebCore {

class DocLoader;
class Document;
class XSLImportRule;
    
class XSLStyleSheet : public StyleSheet {
public:
    XSLStyleSheet(Node* parentNode, const String& href = String(), bool embedded = false);
    XSLStyleSheet(XSLImportRule* parentImport, const String& href = String());
    ~XSLStyleSheet();
    
    virtual bool isXSLStyleSheet() const { return true; }

    virtual String type() const { return "text/xml"; }

    virtual bool parseString(const String &string, bool strict = true);
    
    virtual bool isLoading();
    virtual void checkLoaded();

    void loadChildSheets();
    void loadChildSheet(const DeprecatedString& href);

    xsltStylesheetPtr compileStyleSheet();

    DocLoader* docLoader();

    Document* ownerDocument() { return m_ownerDocument; }
    void setOwnerDocument(Document* doc) { m_ownerDocument = doc; }

    xmlDocPtr document();

    void clearDocuments();

    xmlDocPtr locateStylesheetSubResource(xmlDocPtr parentDoc, const xmlChar* uri);
    
    void markAsProcessed();
    bool processed() const { return m_processed; }

protected:
    Document* m_ownerDocument;
    xmlDocPtr m_stylesheetDoc;
    bool m_embedded;
    bool m_processed;
    bool m_stylesheetDocTaken;
};

} // namespace WebCore

#endif // ENABLE(XSLT)

#endif // XSLStyleSheet_h
