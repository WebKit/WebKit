/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004-2009, 2011-2012, 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2008, 2009, 2011, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
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
#include "ExtensionStyleSheets.h"

#include "CSSStyleSheet.h"
#include "Element.h"
#include "HTMLIFrameElement.h"
#include "HTMLLinkElement.h"
#include "HTMLStyleElement.h"
#include "Page.h"
#include "PageGroup.h"
#include "ProcessingInstruction.h"
#include "SVGNames.h"
#include "SVGStyleElement.h"
#include "Settings.h"
#include "StyleInvalidationAnalysis.h"
#include "StyleResolver.h"
#include "StyleSheetContents.h"
#include "StyleSheetList.h"
#include "UserContentController.h"
#include "UserContentURLPattern.h"
#include "UserStyleSheet.h"

namespace WebCore {

using namespace ContentExtensions;
using namespace HTMLNames;

ExtensionStyleSheets::ExtensionStyleSheets(Document& document)
    : m_document(document)
    , m_styleResolverChangedTimer(*this, &ExtensionStyleSheets::styleResolverChangedTimerFired)
{
}

CSSStyleSheet* ExtensionStyleSheets::pageUserSheet()
{
    if (m_pageUserSheet)
        return m_pageUserSheet.get();
    
    Page* owningPage = m_document.page();
    if (!owningPage)
        return 0;
    
    String userSheetText = owningPage->userStyleSheet();
    if (userSheetText.isEmpty())
        return 0;
    
    // Parse the sheet and cache it.
    m_pageUserSheet = CSSStyleSheet::createInline(m_document, m_document.settings()->userStyleSheetLocation());
    m_pageUserSheet->contents().setIsUserStyleSheet(true);
    m_pageUserSheet->contents().parseString(userSheetText);
    return m_pageUserSheet.get();
}

void ExtensionStyleSheets::clearPageUserSheet()
{
    if (m_pageUserSheet) {
        m_pageUserSheet = nullptr;
        m_document.styleResolverChanged(DeferRecalcStyle);
    }
}

void ExtensionStyleSheets::updatePageUserSheet()
{
    clearPageUserSheet();
    if (pageUserSheet())
        m_document.styleResolverChanged(RecalcStyleImmediately);
}

const Vector<RefPtr<CSSStyleSheet>>& ExtensionStyleSheets::injectedUserStyleSheets() const
{
    updateInjectedStyleSheetCache();
    return m_injectedUserStyleSheets;
}

const Vector<RefPtr<CSSStyleSheet>>& ExtensionStyleSheets::injectedAuthorStyleSheets() const
{
    updateInjectedStyleSheetCache();
    return m_injectedAuthorStyleSheets;
}

void ExtensionStyleSheets::updateInjectedStyleSheetCache() const
{
    if (m_injectedStyleSheetCacheValid)
        return;
    m_injectedStyleSheetCacheValid = true;
    m_injectedUserStyleSheets.clear();
    m_injectedAuthorStyleSheets.clear();

    Page* owningPage = m_document.page();
    if (!owningPage)
        return;

    if (!owningPage->captionUserPreferencesStyleSheet().isEmpty()) {
        // Identify our override style sheet with a unique URL - a new scheme and a UUID.
        static NeverDestroyed<URL> captionsStyleSheetURL(ParsedURLString, "user-captions-override:01F6AF12-C3B0-4F70-AF5E-A3E00234DC23");

        RefPtr<CSSStyleSheet> sheet = CSSStyleSheet::createInline(const_cast<Document&>(m_document), captionsStyleSheetURL.get());
        m_injectedAuthorStyleSheets.append(sheet);

        sheet->contents().setIsUserStyleSheet(false);
        sheet->contents().parseString(owningPage->captionUserPreferencesStyleSheet());
    }

    const auto* userContentController = owningPage->userContentController();
    if (!userContentController)
        return;

    const UserStyleSheetMap* userStyleSheets = userContentController->userStyleSheets();
    if (!userStyleSheets)
        return;

    for (auto& styleSheets : userStyleSheets->values()) {
        for (const auto& sheet : *styleSheets) {
            if (sheet->injectedFrames() == InjectInTopFrameOnly && m_document.ownerElement())
                continue;

            if (!UserContentURLPattern::matchesPatterns(m_document.url(), sheet->whitelist(), sheet->blacklist()))
                continue;

            RefPtr<CSSStyleSheet> groupSheet = CSSStyleSheet::createInline(const_cast<Document&>(m_document), sheet->url());
            bool isUserStyleSheet = sheet->level() == UserStyleUserLevel;
            if (isUserStyleSheet)
                m_injectedUserStyleSheets.append(groupSheet);
            else
                m_injectedAuthorStyleSheets.append(groupSheet);

            groupSheet->contents().setIsUserStyleSheet(isUserStyleSheet);
            groupSheet->contents().parseString(sheet->source());
        }
    }
}

void ExtensionStyleSheets::invalidateInjectedStyleSheetCache()
{
    if (!m_injectedStyleSheetCacheValid)
        return;
    m_injectedStyleSheetCacheValid = false;
    if (m_injectedUserStyleSheets.isEmpty() && m_injectedAuthorStyleSheets.isEmpty())
        return;
    m_document.styleResolverChanged(DeferRecalcStyle);
}

void ExtensionStyleSheets::addUserStyleSheet(Ref<StyleSheetContents>&& userSheet)
{
    ASSERT(userSheet.get().isUserStyleSheet());
    m_userStyleSheets.append(CSSStyleSheet::create(WTFMove(userSheet), &m_document));
    m_document.styleResolverChanged(RecalcStyleImmediately);
}

void ExtensionStyleSheets::addAuthorStyleSheetForTesting(Ref<StyleSheetContents>&& authorSheet)
{
    ASSERT(!authorSheet.get().isUserStyleSheet());
    m_authorStyleSheetsForTesting.append(CSSStyleSheet::create(WTFMove(authorSheet), &m_document));
    m_document.styleResolverChanged(RecalcStyleImmediately);
}

#if ENABLE(CONTENT_EXTENSIONS)
void ExtensionStyleSheets::addDisplayNoneSelector(const String& identifier, const String& selector, uint32_t selectorID)
{
    auto result = m_contentExtensionSelectorSheets.add(identifier, nullptr);
    if (result.isNewEntry) {
        result.iterator->value = ContentExtensionStyleSheet::create(m_document);
        m_userStyleSheets.append(&result.iterator->value->styleSheet());
    }

    if (result.iterator->value->addDisplayNoneSelector(selector, selectorID))
        m_styleResolverChangedTimer.startOneShot(0);
}

void ExtensionStyleSheets::maybeAddContentExtensionSheet(const String& identifier, StyleSheetContents& sheet)
{
    ASSERT(sheet.isUserStyleSheet());

    if (m_contentExtensionSheets.contains(identifier))
        return;

    Ref<CSSStyleSheet> cssSheet = CSSStyleSheet::create(sheet, &m_document);
    m_contentExtensionSheets.set(identifier, &cssSheet.get());
    m_userStyleSheets.append(adoptRef(cssSheet.leakRef()));
    m_styleResolverChangedTimer.startOneShot(0);
}
#endif // ENABLE(CONTENT_EXTENSIONS)

void ExtensionStyleSheets::styleResolverChangedTimerFired()
{
    m_document.styleResolverChanged(RecalcStyleImmediately);
}

void ExtensionStyleSheets::detachFromDocument()
{
    if (m_pageUserSheet)
        m_pageUserSheet->detachFromDocument();
    for (auto& sheet : m_injectedUserStyleSheets)
        sheet->detachFromDocument();
    for (auto& sheet :  m_injectedAuthorStyleSheets)
        sheet->detachFromDocument();
    for (auto& sheet : m_userStyleSheets)
        sheet->detachFromDocument();
    for (auto& sheet : m_authorStyleSheetsForTesting)
        sheet->detachFromDocument();
}

}
