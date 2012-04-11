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

#include "CachedCSSStyleSheet.h"
#include "CachedResourceLoader.h"
#include "Document.h"
#include "MediaList.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

PassRefPtr<StyleRuleImport> StyleRuleImport::create(StyleSheetInternal* parent, const String& href, PassRefPtr<MediaQuerySet> media)
{
    return adoptRef(new StyleRuleImport(parent, href, media));
}

StyleRuleImport::StyleRuleImport(StyleSheetInternal* parent, const String& href, PassRefPtr<MediaQuerySet> media)
    : StyleRuleBase(Import, 0)
    , m_parentStyleSheet(parent)
    , m_styleSheetClient(this)
    , m_strHref(href)
    , m_mediaQueries(media)
    , m_cachedSheet(0)
    , m_loading(false)
{
    ASSERT(parent);
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

void StyleRuleImport::setCSSStyleSheet(const String& href, const KURL& baseURL, const String& charset, const CachedCSSStyleSheet* sheet)
{
    if (m_styleSheet)
        m_styleSheet->clearOwnerRule();
    m_styleSheet = StyleSheetInternal::create(this, href, baseURL, charset);

    bool crossOriginCSS = false;
    bool validMIMEType = false;
    CSSParserMode cssParserMode = m_parentStyleSheet ? m_parentStyleSheet->cssParserMode() : CSSStrictMode;
    bool enforceMIMEType = isStrictParserMode(cssParserMode);
    Document* document = m_parentStyleSheet ? m_parentStyleSheet->findDocument() : 0;
    bool needsSiteSpecificQuirks = document && document->settings() && document->settings()->needsSiteSpecificQuirks();

#ifdef BUILDING_ON_LEOPARD
    if (enforceMIMEType && needsSiteSpecificQuirks) {
        // Covers both http and https, with or without "www."
        if (baseURL.string().contains("mcafee.com/japan/", false))
            enforceMIMEType = false;
    }
#endif

    String sheetText = sheet->sheetText(enforceMIMEType, &validMIMEType);
    m_styleSheet->parseString(sheetText, cssParserMode);

    if (!document || !document->securityOrigin()->canRequest(baseURL))
        crossOriginCSS = true;

    if (crossOriginCSS && !validMIMEType && !m_styleSheet->hasSyntacticallyValidCSSHeader())
        m_styleSheet = StyleSheetInternal::create(this, href, baseURL, charset);

    if (isStrictParserMode(cssParserMode) && needsSiteSpecificQuirks) {
        // Work around <https://bugs.webkit.org/show_bug.cgi?id=28350>.
        DEFINE_STATIC_LOCAL(const String, slashKHTMLFixesDotCss, ("/KHTMLFixes.css"));
        DEFINE_STATIC_LOCAL(const String, mediaWikiKHTMLFixesStyleSheet, ("/* KHTML fix stylesheet */\n/* work around the horizontal scrollbars */\n#column-content { margin-left: 0; }\n\n"));
        // There are two variants of KHTMLFixes.css. One is equal to mediaWikiKHTMLFixesStyleSheet,
        // while the other lacks the second trailing newline.
        if (baseURL.string().endsWith(slashKHTMLFixesDotCss) && !sheetText.isNull() && mediaWikiKHTMLFixesStyleSheet.startsWith(sheetText)
                && sheetText.length() >= mediaWikiKHTMLFixesStyleSheet.length() - 1) {
            ASSERT(m_styleSheet->childRules().size() == 1);
            m_styleSheet->clearRules();
        }
    }

    m_loading = false;

    if (m_parentStyleSheet) {
        m_parentStyleSheet->notifyLoadedSheet(sheet);
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
    Document* document = m_parentStyleSheet->findDocument();
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
        // FIXME: This is wrong if the finalURL was updated via document::updateBaseURL.
        if (absHref == sheet->finalURL().string())
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
}

MediaList* CSSImportRule::media()
{
    return m_importRule->mediaQueries()->ensureMediaList(parentStyleSheet());
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
