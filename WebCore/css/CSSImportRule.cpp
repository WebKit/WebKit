/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2005, 2006 Apple Computer, Inc.
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
#include "CSSImportRule.h"

#include "CachedCSSStyleSheet.h"
#include "CSSStyleSheet.h"
#include "DocLoader.h"
#include "Document.h"
#include "KURL.h"
#include "MediaList.h"

namespace WebCore {

CSSImportRule::CSSImportRule(StyleBase* parent, const String& href, MediaList* media)
    : CSSRule(parent)
    , m_strHref(href)
    , m_lstMedia(media)
    , m_cachedSheet(0)
    , m_loading(false)
{
    if (m_lstMedia)
        m_lstMedia->setParent(this);
    else
        m_lstMedia = new MediaList(this, String());
}

CSSImportRule::~CSSImportRule()
{
    if (m_lstMedia)
        m_lstMedia->setParent(0);
    if (m_styleSheet)
        m_styleSheet->setParent(0);
    if (m_cachedSheet)
        m_cachedSheet->deref(this);
}

void CSSImportRule::setCSSStyleSheet(const String& url, const String& charset, const String& sheet)
{
    if (m_styleSheet)
        m_styleSheet->setParent(0);
    m_styleSheet = new CSSStyleSheet(this, url, charset);

    CSSStyleSheet* parent = parentStyleSheet();
    m_styleSheet->parseString(sheet, !parent || parent->useStrictParsing());
    m_loading = false;

    checkLoaded();
}

bool CSSImportRule::isLoading() const
{
    return m_loading || (m_styleSheet && m_styleSheet->isLoading());
}

void CSSImportRule::insertedIntoParent()
{
    StyleBase* root = this;
    StyleBase* parent;
    while ((parent = root->parent()))
        root = parent;
    if (!root->isCSSStyleSheet())
        return;
    DocLoader* docLoader = static_cast<CSSStyleSheet*>(root)->docLoader();
    if (!docLoader)
        return;

    String absHref = m_strHref;
    CSSStyleSheet* parentSheet = parentStyleSheet();
    if (!parentSheet->href().isNull())
        // use parent styleheet's URL as the base URL
        absHref = KURL(parentSheet->href().deprecatedString(), m_strHref.deprecatedString()).string();

    // Check for a cycle in our import chain.  If we encounter a stylesheet
    // in our parent chain with the same URL, then just bail.
    for (parent = static_cast<StyleBase*>(this)->parent(); parent; parent = parent->parent()) {
        if (absHref == parent->baseURL())
            return;
    }

    m_cachedSheet = docLoader->requestCSSStyleSheet(absHref, parentSheet->charset());
    if (m_cachedSheet) {
        // if the import rule is issued dynamically, the sheet may be
        // removed from the pending sheet count, so let the doc know
        // the sheet being imported is pending.
        if (parentSheet && parentSheet->loadCompleted() && parentSheet->doc())
            parentSheet->doc()->addPendingSheet();
        m_loading = true;
        m_cachedSheet->ref(this);
    }
}

String CSSImportRule::cssText() const
{
    String result = "@import url(\"";
    result += m_strHref;
    result += "\")";

    if (m_lstMedia) {
        result += " ";
        result += m_lstMedia->mediaText();
    }
    result += ";";

    return result;
}

} // namespace WebCore
