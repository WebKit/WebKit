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
#include "HTMLLinkElement.h"
#include "HTMLStyleElement.h"
#include "Page.h"
#include "ProcessingInstruction.h"
#include "SVGStyleElement.h"
#include "Settings.h"
#include "StyleInvalidator.h"
#include "StyleResolver.h"
#include "StyleScope.h"
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
{
}

static Ref<CSSStyleSheet> createExtensionsStyleSheet(Document& document, URL url, const String& text, UserStyleLevel level)
{
    auto contents = StyleSheetContents::create(url, CSSParserContext(document, url));
    auto styleSheet = CSSStyleSheet::create(contents.get(), document, true);

    contents->setIsUserStyleSheet(level == UserStyleUserLevel);
    contents->parseString(text);

    return styleSheet;
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
    
    m_pageUserSheet = createExtensionsStyleSheet(m_document, m_document.settings().userStyleSheetLocation(), userSheetText, UserStyleUserLevel);

    return m_pageUserSheet.get();
}

void ExtensionStyleSheets::clearPageUserSheet()
{
    if (m_pageUserSheet) {
        m_pageUserSheet = nullptr;
        m_document.styleScope().didChangeStyleSheetEnvironment();
    }
}

void ExtensionStyleSheets::updatePageUserSheet()
{
    clearPageUserSheet();
    if (pageUserSheet())
        m_document.styleScope().didChangeStyleSheetEnvironment();
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

    owningPage->userContentProvider().forEachUserStyleSheet([&](const UserStyleSheet& userStyleSheet) {
        if (userStyleSheet.injectedFrames() == InjectInTopFrameOnly && m_document.ownerElement())
            return;

        if (!UserContentURLPattern::matchesPatterns(m_document.url(), userStyleSheet.whitelist(), userStyleSheet.blacklist()))
            return;

        auto sheet = createExtensionsStyleSheet(const_cast<Document&>(m_document), userStyleSheet.url(), userStyleSheet.source(), userStyleSheet.level());

        if (userStyleSheet.level() == UserStyleUserLevel)
            m_injectedUserStyleSheets.append(WTFMove(sheet));
        else
            m_injectedAuthorStyleSheets.append(WTFMove(sheet));
    });
}

void ExtensionStyleSheets::invalidateInjectedStyleSheetCache()
{
    m_injectedStyleSheetCacheValid = false;
    m_document.styleScope().didChangeStyleSheetEnvironment();
}

void ExtensionStyleSheets::addUserStyleSheet(Ref<StyleSheetContents>&& userSheet)
{
    ASSERT(userSheet.get().isUserStyleSheet());
    m_userStyleSheets.append(CSSStyleSheet::create(WTFMove(userSheet), m_document));
    m_document.styleScope().didChangeStyleSheetEnvironment();
}

void ExtensionStyleSheets::addAuthorStyleSheetForTesting(Ref<StyleSheetContents>&& authorSheet)
{
    ASSERT(!authorSheet.get().isUserStyleSheet());
    m_authorStyleSheetsForTesting.append(CSSStyleSheet::create(WTFMove(authorSheet), m_document));
    m_document.styleScope().didChangeStyleSheetEnvironment();
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
        m_document.styleScope().didChangeStyleSheetEnvironment();
}

void ExtensionStyleSheets::maybeAddContentExtensionSheet(const String& identifier, StyleSheetContents& sheet)
{
    ASSERT(sheet.isUserStyleSheet());

    if (m_contentExtensionSheets.contains(identifier))
        return;

    Ref<CSSStyleSheet> cssSheet = CSSStyleSheet::create(sheet, m_document);
    m_contentExtensionSheets.set(identifier, &cssSheet.get());
    m_userStyleSheets.append(adoptRef(cssSheet.leakRef()));
    m_document.styleScope().didChangeStyleSheetEnvironment();

}
#endif // ENABLE(CONTENT_EXTENSIONS)

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
