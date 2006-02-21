/**
 * This file is part of the XSL implementation.
 *
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

#ifdef KHTML_XSLT

#include "PlatformString.h"
#include "dom/dom_exception.h"

#include "html/html_documentimpl.h"
#include "loader.h"
#include "CachedXSLStyleSheet.h"
#include "DocLoader.h"
#include "xsl_stylesheetimpl.h"
#include "xml_tokenizer.h"

#include <libxslt/xsltutils.h>
#include <libxml/uri.h>

#define IS_BLANK_NODE(n)                                                \
    (((n)->type == XML_TEXT_NODE) && (xsltIsBlank((n)->content)))

namespace WebCore {
    
XSLStyleSheetImpl::XSLStyleSheetImpl(XSLImportRuleImpl* parentRule, const String& href)
    : StyleSheetImpl(parentRule, href)
    , m_ownerDocument(0)
    , m_stylesheetDoc(0)
    , m_embedded(false)
    , m_processed(false) // Child sheets get marked as processed when the libxslt engine has finally seen them.
    , m_stylesheetDocTaken(false)
{
}

XSLStyleSheetImpl::XSLStyleSheetImpl(NodeImpl* parentNode, const String& href,  bool embedded)
    : StyleSheetImpl(parentNode, href)
    , m_ownerDocument(parentNode->getDocument())
    , m_stylesheetDoc(0)
    , m_embedded(embedded)
    , m_processed(true) // The root sheet starts off processed.
    , m_stylesheetDocTaken(false)
{
}

XSLStyleSheetImpl::~XSLStyleSheetImpl()
{
    if (!m_stylesheetDocTaken)
        xmlFreeDoc(m_stylesheetDoc);
}

bool XSLStyleSheetImpl::isLoading()
{
    unsigned len = length();
    for (unsigned i = 0; i < len; ++i) {
        StyleBaseImpl* rule = item(i);
        if (rule->isImportRule()) {
            XSLImportRuleImpl* import = static_cast<XSLImportRuleImpl*>(rule);
            if (import->isLoading())
                return true;
        }
    }
    return false;
}

void XSLStyleSheetImpl::checkLoaded()
{
    if (isLoading()) 
        return;
    if (parent())
        parent()->checkLoaded();
    if (m_parentNode)
        m_parentNode->sheetLoaded();
}

xmlDocPtr XSLStyleSheetImpl::document()
{
    if (m_embedded && ownerDocument())
        return (xmlDocPtr)ownerDocument()->transformSource();
    return m_stylesheetDoc;
}

void XSLStyleSheetImpl::clearDocuments()
{
    m_stylesheetDoc = 0;
    unsigned len = length();
    for (unsigned i = 0; i < len; ++i) {
        StyleBaseImpl* rule = item(i);
        if (rule->isImportRule()) {
            XSLImportRuleImpl* import = static_cast<XSLImportRuleImpl*>(rule);
            if (import->styleSheet())
                import->styleSheet()->clearDocuments();
        }
    }
}

khtml::DocLoader* XSLStyleSheetImpl::docLoader()
{
    if (!m_ownerDocument)
        return 0;
    return m_ownerDocument->docLoader();
}

bool XSLStyleSheetImpl::parseString(const DOMString &string, bool strict)
{
    // Parse in a single chunk into an xmlDocPtr
    const QChar BOM(0xFEFF);
    const unsigned char BOMHighByte = *reinterpret_cast<const unsigned char*>(&BOM);
    setLoaderForLibXMLCallbacks(docLoader());
    if (!m_stylesheetDocTaken)
        xmlFreeDoc(m_stylesheetDoc);
    m_stylesheetDocTaken = false;
    m_stylesheetDoc = xmlReadMemory(reinterpret_cast<const char *>(string.unicode()),
                                    string.length() * sizeof(QChar),
                                    m_ownerDocument->URL().ascii(),
                                    BOMHighByte == 0xFF ? "UTF-16LE" : "UTF-16BE", 
                                    XML_PARSE_NOCDATA|XML_PARSE_DTDATTR|XML_PARSE_NOENT);
    loadChildSheets();
    setLoaderForLibXMLCallbacks(0);
    return m_stylesheetDoc;
}

void XSLStyleSheetImpl::loadChildSheets()
{
    if (!document())
        return;
    
    xmlNodePtr stylesheetRoot = document()->children;
    if (m_embedded) {
        // We have to locate (by ID) the appropriate embedded stylesheet element, so that we can walk the 
        // import/include list.
        xmlAttrPtr idNode = xmlGetID(document(), (const xmlChar*)(href().qstring().utf8().data()));
        if (idNode == NULL)
            return;
        stylesheetRoot = idNode->parent;
    } else {
        // FIXME: Need to handle an external URI with a # in it.  This is a pretty minor edge case, so we'll deal
        // with it later.
    }
    
    if (stylesheetRoot) {
        // Walk the children of the root element and look for import/include elements.
        // Imports must occur first.
        xmlNodePtr curr = stylesheetRoot->children;
        while (curr) {
            if (IS_BLANK_NODE(curr)) {
                curr = curr->next;
                continue;
            }
            if (curr->type == XML_ELEMENT_NODE && IS_XSLT_ELEM(curr) && IS_XSLT_NAME(curr, "import")) {
                xmlChar* uriRef = xsltGetNsProp(curr, (const xmlChar*)"href", XSLT_NAMESPACE);                
                loadChildSheet(QString::fromUtf8((const char*)uriRef));
                xmlFree(uriRef);
            } else
                break;
            curr = curr->next;
        }

        // Now handle includes.
        while (curr) {
            if (curr->type == XML_ELEMENT_NODE && IS_XSLT_ELEM(curr) && IS_XSLT_NAME(curr, "include")) {
                xmlChar* uriRef = xsltGetNsProp(curr, (const xmlChar*)"href", XSLT_NAMESPACE);
                loadChildSheet(QString::fromUtf8((const char*)uriRef));
                xmlFree(uriRef);
            }
            curr = curr->next;
        }
    }
}

void XSLStyleSheetImpl::loadChildSheet(const QString& href)
{
    RefPtr<XSLImportRuleImpl> childRule = new XSLImportRuleImpl(this, href);
    append(childRule);
    childRule->loadSheet();
}

XSLStyleSheetImpl* XSLImportRuleImpl::parentStyleSheet() const
{
    return (parent() && parent()->isXSLStyleSheet()) ? static_cast<XSLStyleSheetImpl*>(parent()) : 0;
}

xsltStylesheetPtr XSLStyleSheetImpl::compileStyleSheet()
{
    // FIXME: Hook up error reporting for the stylesheet compilation process.
    if (m_embedded)
        return xsltLoadStylesheetPI(document());
    
    // xsltParseStylesheetDoc makes the document part of the stylesheet
    // so we have to release our pointer to it.
    ASSERT(!m_stylesheetDocTaken);
    xsltStylesheetPtr result = xsltParseStylesheetDoc(m_stylesheetDoc);
    if (result)
        m_stylesheetDocTaken = true;
    return result;
}

xmlDocPtr XSLStyleSheetImpl::locateStylesheetSubResource(xmlDocPtr parentDoc, const xmlChar* uri)
{
    bool matchedParent = (parentDoc == document());
    unsigned len = length();
    for (unsigned i = 0; i < len; ++i) {
        StyleBaseImpl* rule = item(i);
        if (rule->isImportRule()) {
            XSLImportRuleImpl* import = static_cast<XSLImportRuleImpl*>(rule);
            XSLStyleSheetImpl* child = import->styleSheet();
            if (!child)
                continue;
            if (matchedParent) {
                if (child->processed())
                    continue; // libxslt has been given this sheet already.
                
                // Check the URI of the child stylesheet against the doc URI.
                // In order to ensure that libxml canonicalized both URLs, we get the original href
                // string from the import rule and canonicalize it using libxml before comparing it
                // with the URI argument.
                QCString importHref = import->href().qstring().utf8();
                xmlChar* base = xmlNodeGetBase(parentDoc, (xmlNodePtr)parentDoc);
                xmlChar* childURI = xmlBuildURI((const xmlChar*)(const char*)importHref, base);
                bool equalURIs = xmlStrEqual(uri, childURI);
                xmlFree(base);
                xmlFree(childURI);
                if (equalURIs) {
                    child->markAsProcessed();
                    return child->document();
                }
            } else {
                xmlDocPtr result = import->styleSheet()->locateStylesheetSubResource(parentDoc, uri);
                if (result)
                    return result;
            }
        }
    }
    
    return 0;
}

void XSLStyleSheetImpl::markAsProcessed()
{
    ASSERT(!m_processed);
    ASSERT(!m_stylesheetDocTaken);
    m_processed = true;
    m_stylesheetDocTaken = true;
}

// ----------------------------------------------------------------------------------------------

XSLImportRuleImpl::XSLImportRuleImpl(StyleBaseImpl* parent, const DOMString &href)
: StyleBaseImpl(parent)
{
    m_strHref = href;
    m_cachedSheet = 0;
    m_loading = false;
}

XSLImportRuleImpl::~XSLImportRuleImpl()
{
    if (m_styleSheet)
        m_styleSheet->setParent(0);
    
    if (m_cachedSheet)
        m_cachedSheet->deref(this);
}

void XSLImportRuleImpl::setStyleSheet(const DOMString& url, const DOMString& sheet)
{
    if (m_styleSheet)
        m_styleSheet->setParent(0);
    
    m_styleSheet = new XSLStyleSheetImpl(this, url);
    
    XSLStyleSheetImpl* parent = parentStyleSheet();
    if (parent)
        m_styleSheet->setOwnerDocument(parent->ownerDocument());

    m_styleSheet->parseString(sheet);
    m_loading = false;
    
    checkLoaded();
}

bool XSLImportRuleImpl::isLoading()
{
    return (m_loading || (m_styleSheet && m_styleSheet->isLoading()));
}

void XSLImportRuleImpl::loadSheet()
{
    DocLoader* docLoader = 0;
    StyleBaseImpl* root = this;
    StyleBaseImpl* parent;
    while ((parent = root->parent()))
        root = parent;
    if (root->isXSLStyleSheet())
        docLoader = static_cast<XSLStyleSheetImpl*>(root)->docLoader();
    
    DOMString absHref = m_strHref;
    XSLStyleSheetImpl* parentSheet = parentStyleSheet();
    if (!parentSheet->href().isNull())
        // use parent styleheet's URL as the base URL
        absHref = KURL(parentSheet->href().qstring(),m_strHref.qstring()).url();
    
    // Check for a cycle in our import chain.  If we encounter a stylesheet
    // in our parent chain with the same URL, then just bail.
    for (parent = static_cast<StyleBaseImpl*>(this)->parent(); parent; parent = parent->parent())
        if (absHref == parent->baseURL())
            return;
    
    m_cachedSheet = docLoader->requestXSLStyleSheet(absHref);
    
    if (m_cachedSheet) {
        m_cachedSheet->ref(this);
        
        // If the imported sheet is in the cache, then setStyleSheet gets called,
        // and the sheet even gets parsed (via parseString).  In this case we have
        // loaded (even if our subresources haven't), so if we have a stylesheet after
        // checking the cache, then we've clearly loaded.
        if (!m_styleSheet)
            m_loading = true;
    }
}

}
#endif
