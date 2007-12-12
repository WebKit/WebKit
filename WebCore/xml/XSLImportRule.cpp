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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "XSLImportRule.h"

#if ENABLE(XSLT)

#include "CachedXSLStyleSheet.h"
#include "DocLoader.h"
#include "XSLStyleSheet.h"

namespace WebCore {

XSLImportRule::XSLImportRule(StyleBase* parent, const String& href)
    : StyleBase(parent)
    , m_strHref(href)
    , m_cachedSheet(0)
    , m_loading(false)
{
}

XSLImportRule::~XSLImportRule()
{
    if (m_styleSheet)
        m_styleSheet->setParent(0);
    
    if (m_cachedSheet)
        m_cachedSheet->deref(this);
}

XSLStyleSheet* XSLImportRule::parentStyleSheet() const
{
    return (parent() && parent()->isXSLStyleSheet()) ? static_cast<XSLStyleSheet*>(parent()) : 0;
}

void XSLImportRule::setXSLStyleSheet(const String& url, const String& sheet)
{
    if (m_styleSheet)
        m_styleSheet->setParent(0);
    
    m_styleSheet = new XSLStyleSheet(this, url);
    
    XSLStyleSheet* parent = parentStyleSheet();
    if (parent)
        m_styleSheet->setOwnerDocument(parent->ownerDocument());

    m_styleSheet->parseString(sheet);
    m_loading = false;
    
    checkLoaded();
}

bool XSLImportRule::isLoading()
{
    return (m_loading || (m_styleSheet && m_styleSheet->isLoading()));
}

void XSLImportRule::loadSheet()
{
    DocLoader* docLoader = 0;
    StyleBase* root = this;
    StyleBase* parent;
    while ((parent = root->parent()))
        root = parent;
    if (root->isXSLStyleSheet())
        docLoader = static_cast<XSLStyleSheet*>(root)->docLoader();
    
    String absHref = m_strHref;
    XSLStyleSheet* parentSheet = parentStyleSheet();
    if (!parentSheet->href().isNull())
        // use parent styleheet's URL as the base URL
        absHref = KURL(parentSheet->href().deprecatedString(), m_strHref.deprecatedString()).string();
    
    // Check for a cycle in our import chain.  If we encounter a stylesheet
    // in our parent chain with the same URL, then just bail.
    for (parent = static_cast<StyleBase*>(this)->parent(); parent; parent = parent->parent()) {
        if (absHref == parent->baseURL())
            return;
    }
    
    m_cachedSheet = docLoader->requestXSLStyleSheet(absHref);
    
    if (m_cachedSheet) {
        m_cachedSheet->ref(this);
        
        // If the imported sheet is in the cache, then setXSLStyleSheet gets called,
        // and the sheet even gets parsed (via parseString).  In this case we have
        // loaded (even if our subresources haven't), so if we have a stylesheet after
        // checking the cache, then we've clearly loaded.
        if (!m_styleSheet)
            m_loading = true;
    }
}

} // namespace WebCore

#endif // ENABLE(XSLT)
