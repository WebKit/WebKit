/**
 * This file is part of the XSL implementation.
 *
 * Copyright (C) 2004 Apple Computer, Inc.
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

#ifdef KHTML_XSLT

#include "dom/dom_string.h"
#include "dom/dom_exception.h"

#include "xml/dom_nodeimpl.h"
#include "html/html_documentimpl.h"
#include "misc/loader.h"
#include "xsl_stylesheetimpl.h"

#include <kdebug.h>

using namespace khtml;
using namespace DOM;

namespace DOM {
    
XSLStyleSheetImpl::XSLStyleSheetImpl(XSLStyleSheetImpl *parentSheet, DOMString href)
    : StyleSheetImpl(parentSheet, href)
{
    m_lstChildren = new QPtrList<StyleBaseImpl>;
    m_ownerDocument = parentSheet ? parentSheet->ownerDocument() : 0;
    parentSheet->append(this);
}

XSLStyleSheetImpl::XSLStyleSheetImpl(NodeImpl *parentNode, DOMString href)
    : StyleSheetImpl(parentNode, href)
{
    m_lstChildren = new QPtrList<StyleBaseImpl>;
    m_ownerDocument = parentNode->getDocument();
}

XSLStyleSheetImpl::~XSLStyleSheetImpl()
{
    xmlFreeDoc(m_stylesheetDoc);
}

bool XSLStyleSheetImpl::isLoading()
{
    StyleBaseImpl* sheet;
    for (sheet = m_lstChildren->first(); sheet; sheet = m_lstChildren->next()) {
        if (sheet->isXSLStyleSheet()) {
            XSLStyleSheetImpl* import = static_cast<XSLStyleSheetImpl*>(import);
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
    if (m_parent)
        m_parent->checkLoaded();
    if (m_parentNode)
        m_parentNode->sheetLoaded();
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
    const unsigned char BOMHighByte = *reinterpret_cast<const unsigned char *>(&BOM);
    m_stylesheetDoc = xmlReadMemory(reinterpret_cast<const char *>(string.unicode()),
                                    string.length() * sizeof(QChar),
                                    m_ownerDocument->URL().ascii(),
                                    BOMHighByte == 0xFF ? "UTF-16LE" : "UTF-16BE", 
                                    XML_PARSE_NOCDATA|XML_PARSE_DTDATTR|XML_PARSE_NOENT);
    return m_stylesheetDoc;
}

}
#endif
