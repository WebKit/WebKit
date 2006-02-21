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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef xsl_stylesheetimpl_h_
#define xsl_stylesheetimpl_h_

#ifdef KHTML_XSLT

#include "CachedObjectClient.h"
#include "css/css_stylesheetimpl.h"
#include <libxml/parser.h>
#include <libxslt/transform.h>

namespace WebCore {

class XSLImportRuleImpl;
class CachedXSLStyleSheet;
    
class XSLStyleSheetImpl : public StyleSheetImpl
{
public:
    XSLStyleSheetImpl(NodeImpl *parentNode, const String& href = String(), bool embedded = false);
    XSLStyleSheetImpl(XSLImportRuleImpl* parentImport, const String& href = String());
    ~XSLStyleSheetImpl();
    
    virtual bool isXSLStyleSheet() const { return true; }

    virtual String type() const { return "text/xml"; }

    virtual bool parseString(const String &string, bool strict = true);
    
    virtual bool isLoading();
    virtual void checkLoaded();

    void loadChildSheets();
    void loadChildSheet(const QString& href);

    xsltStylesheetPtr compileStyleSheet();

    DocLoader* docLoader();

    DocumentImpl* ownerDocument() { return m_ownerDocument; }
    void setOwnerDocument(DocumentImpl* doc) { m_ownerDocument = doc; }

    xmlDocPtr document();

    void clearDocuments();

    xmlDocPtr locateStylesheetSubResource(xmlDocPtr parentDoc, const xmlChar* uri);
    
    void markAsProcessed();
    bool processed() const { return m_processed; }

protected:
    DocumentImpl* m_ownerDocument;
    xmlDocPtr m_stylesheetDoc;
    bool m_embedded;
    bool m_processed;
    bool m_stylesheetDocTaken;
};

class XSLImportRuleImpl : public CachedObjectClient, public StyleBaseImpl
{
public:
    XSLImportRuleImpl(StyleBaseImpl* parent, const String& href);
    virtual ~XSLImportRuleImpl();
    
    String href() const { return m_strHref; }
    XSLStyleSheetImpl* styleSheet() const { return m_styleSheet.get(); }
    
    virtual bool isImportRule() { return true; }
    XSLStyleSheetImpl* parentStyleSheet() const;
    
    // from CachedObjectClient
    virtual void setStyleSheet(const String& url, const String &sheet);
    
    bool isLoading();
    void loadSheet();
    
protected:
    String m_strHref;
    RefPtr<XSLStyleSheetImpl> m_styleSheet;
    CachedXSLStyleSheet* m_cachedSheet;
    bool m_loading;
};

}
#endif
#endif
