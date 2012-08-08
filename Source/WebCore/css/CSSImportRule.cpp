/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2005, 2006, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#include "CSSStyleSheet.h"
#include "CachedCSSStyleSheet.h"
#include "CachedResourceLoader.h"
#include "Document.h"
#include "MediaList.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

PassRefPtr<StyleRuleImport> StyleRuleImport::create(const String& href, PassRefPtr<MediaQuerySet> media)
{
    return adoptRef(new StyleRuleImport(href, media));
}

StyleRuleImport::StyleRuleImport(const String& href, PassRefPtr<MediaQuerySet> media)
    : StyleRuleBase(Import, 0)
    , m_parentStyleSheet(0)
    , m_styleSheetClient(this)
    , m_strHref(href)
    , m_mediaQueries(media)
    , m_cachedSheet(0)
    , m_loading(false)
{
    if (!m_mediaQueries)
        m_mediaQueries = MediaQuerySet::create(String());
}

StyleRuleImport::~StyleRuleImport()
{
    if (m_styleSheet)
        m_styleSheet->clearOwnerRule();
    if (m_cachedSheet)
        m_cachedSheet->removeClient(&m_styleSheetClient);
}

void StyleRuleImport::setCSSStyleSheet(const String& href, const KURL& baseURL, const String& charset, const CachedCSSStyleSheet* cachedStyleSheet)
{
    if (m_styleSheet)
        m_styleSheet->clearOwnerRule();

    CSSParserContext context = m_parentStyleSheet ? m_parentStyleSheet->parserContext() : CSSStrictMode;
    context.charset = charset;
    if (!baseURL.isNull())
        context.baseURL = baseURL;

    m_styleSheet = StyleSheetInternal::create(this, href, baseURL, context);

    Document* document = m_parentStyleSheet ? m_parentStyleSheet->singleOwnerDocument() : 0;
    m_styleSheet->parseAuthorStyleSheet(cachedStyleSheet, document ? document->securityOrigin() : 0);

    m_loading = false;

    if (m_parentStyleSheet) {
        m_parentStyleSheet->notifyLoadedSheet(cachedStyleSheet);
        m_parentStyleSheet->checkLoaded();
    }
}

bool StyleRuleImport::isLoading() const
{
    return m_loading || (m_styleSheet && m_styleSheet->isLoading());
}

void StyleRuleImport::requestStyleSheet()
{
    if (!m_parentStyleSheet)
        return;
    Document* document = m_parentStyleSheet->singleOwnerDocument();
    if (!document)
        return;

    CachedResourceLoader* cachedResourceLoader = document->cachedResourceLoader();
    if (!cachedResourceLoader)
        return;

    String absHref = m_strHref;
    if (!m_parentStyleSheet->finalURL().isNull())
        // use parent styleheet's URL as the base URL
        absHref = KURL(m_parentStyleSheet->finalURL(), m_strHref).string();

    // Check for a cycle in our import chain.  If we encounter a stylesheet
    // in our parent chain with the same URL, then just bail.
    StyleSheetInternal* rootSheet = m_parentStyleSheet;
    for (StyleSheetInternal* sheet = m_parentStyleSheet; sheet; sheet = sheet->parentStyleSheet()) {
        if (absHref == sheet->finalURL().string() || absHref == sheet->originalURL())
            return;
        rootSheet = sheet;
    }

    ResourceRequest request(document->completeURL(absHref));
    if (m_parentStyleSheet->isUserStyleSheet())
        m_cachedSheet = cachedResourceLoader->requestUserCSSStyleSheet(request, m_parentStyleSheet->charset());
    else
        m_cachedSheet = cachedResourceLoader->requestCSSStyleSheet(request, m_parentStyleSheet->charset());
    if (m_cachedSheet) {
        // if the import rule is issued dynamically, the sheet may be
        // removed from the pending sheet count, so let the doc know
        // the sheet being imported is pending.
        if (m_parentStyleSheet && m_parentStyleSheet->loadCompleted() && rootSheet == m_parentStyleSheet)
            m_parentStyleSheet->startLoadingDynamicSheet();
        m_loading = true;
        m_cachedSheet->addClient(&m_styleSheetClient);
    }
}

CSSImportRule::CSSImportRule(StyleRuleImport* importRule, CSSStyleSheet* parent)
    : CSSRule(parent, CSSRule::IMPORT_RULE)
    , m_importRule(importRule)
{
}

CSSImportRule::~CSSImportRule()
{
    if (m_styleSheetCSSOMWrapper)
        m_styleSheetCSSOMWrapper->clearOwnerRule();
    if (m_mediaCSSOMWrapper)
        m_mediaCSSOMWrapper->clearParentRule();
}

MediaList* CSSImportRule::media() const
{
    if (!m_mediaCSSOMWrapper)
        m_mediaCSSOMWrapper = MediaList::create(m_importRule->mediaQueries(), const_cast<CSSImportRule*>(this));
    return m_mediaCSSOMWrapper.get();
}

String CSSImportRule::cssText() const
{
    StringBuilder result;
    result.append("@import url(\"");
    result.append(m_importRule->href());
    result.append("\")");

    if (m_importRule->mediaQueries()) {
        result.append(' ');
        result.append(m_importRule->mediaQueries()->mediaText());
    }
    result.append(';');
    
    return result.toString();
}

CSSStyleSheet* CSSImportRule::styleSheet() const
{ 
    if (!m_importRule->styleSheet())
        return 0;

    if (!m_styleSheetCSSOMWrapper)
        m_styleSheetCSSOMWrapper = CSSStyleSheet::create(m_importRule->styleSheet(), const_cast<CSSImportRule*>(this));
    return m_styleSheetCSSOMWrapper.get(); 
}


} // namespace WebCore
