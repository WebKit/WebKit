/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004-2025 Apple Inc. All rights reserved.
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
#include "DocumentPage.h"
#include "Element.h"
#include "FrameDestructionObserverInlines.h"
#include "HTMLLinkElement.h"
#include "HTMLStyleElement.h"
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
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ExtensionStyleSheets);

#if ENABLE(CONTENT_EXTENSIONS)
using namespace ContentExtensions;
#endif
using namespace HTMLNames;

ExtensionStyleSheets::ExtensionStyleSheets(Document& document)
    : m_document(document)
{
}

ExtensionStyleSheets::~ExtensionStyleSheets() = default;

static Ref<CSSStyleSheet> createExtensionsStyleSheet(Document& document, URL url, const String& text, UserStyleLevel level)
{
    auto contents = StyleSheetContents::create(url.string(), CSSParserContext(document, url));
    auto styleSheet = CSSStyleSheet::create(contents.get(), document, true);

    contents->setIsUserStyleSheet(level == UserStyleLevel::User);
    contents->parseString(text);

    return styleSheet;
}

CSSStyleSheet* ExtensionStyleSheets::pageUserSheet()
{
    if (m_pageUserSheet)
        return m_pageUserSheet.get();
    
    RefPtr owningPage = m_document->page();
    if (!owningPage)
        return nullptr;
    
    String userSheetText = owningPage->userStyleSheet();
    if (userSheetText.isEmpty())
        return nullptr;
    
    m_pageUserSheet = createExtensionsStyleSheet(protect(m_document), m_document->settings().userStyleSheetLocation(), userSheetText, UserStyleLevel::User);

    return m_pageUserSheet.get();
}

void ExtensionStyleSheets::clearPageUserSheet()
{
    if (m_pageUserSheet) {
        m_pageUserSheet = nullptr;
        protect(m_document)->styleScope().didChangeExtensionStyleSheets();
    }
}

void ExtensionStyleSheets::updatePageUserSheet()
{
    clearPageUserSheet();
    if (pageUserSheet())
        protect(m_document)->styleScope().didChangeExtensionStyleSheets();
}

const Vector<Ref<CSSStyleSheet>>& ExtensionStyleSheets::injectedUserStyleSheets() const
{
    updateInjectedStyleSheetCache();
    return m_injectedUserStyleSheets;
}

const Vector<Ref<CSSStyleSheet>>& ExtensionStyleSheets::injectedAuthorStyleSheets() const
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
    m_injectedStyleSheetToSource.clear();

    Ref document = m_document;
    RefPtr owningPage = document->page();
    if (!owningPage)
        return;

    auto addStyleSheet = [&](const UserStyleSheet& userStyleSheet) {
        Ref sheet = createExtensionsStyleSheet(document, userStyleSheet.url(), userStyleSheet.source(), userStyleSheet.level());

        m_injectedStyleSheetToSource.set(sheet.copyRef(), userStyleSheet.source());

        if (sheet->contents().isUserStyleSheet())
            m_injectedUserStyleSheets.append(WTF::move(sheet));
        else
            m_injectedAuthorStyleSheets.append(WTF::move(sheet));
    };

    for (const auto& userStyleSheet : m_pageSpecificStyleSheets)
        addStyleSheet(userStyleSheet);

    RefPtr frame = document->frame();
    RefPtr userContentProvider = frame ? frame->userContentProvider() : nullptr;

    userContentProvider->forEachUserStyleSheet([&](const UserStyleSheet& userStyleSheet) {
        if (userStyleSheet.pageID())
            return;

        if (userStyleSheet.injectedFrames() == UserContentInjectedFrames::InjectInTopFrameOnly && document->ownerElement())
            return;

        auto url = document->url();

        if (RefPtr parentDocument = document->parentDocument()) {
            switch (userStyleSheet.matchParentFrame()) {
            case UserContentMatchParentFrame::ForOpaqueOrigins:
                if (url.protocolIsAbout() || url.protocolIsBlob() || url.protocolIsData())
                    url = parentDocument->url();
                break;

            case UserContentMatchParentFrame::ForAboutBlank:
                if (url.isAboutBlank())
                    url = parentDocument->url();
                break;

            case UserContentMatchParentFrame::Never:
                break;
            }
        }

        if (!UserContentURLPattern::matchesPatterns(url, userStyleSheet.allowlist(), userStyleSheet.blocklist()))
            return;

        addStyleSheet(userStyleSheet);
    });
}

void ExtensionStyleSheets::injectPageSpecificUserStyleSheet(const UserStyleSheet& userStyleSheet)
{
    m_pageSpecificStyleSheets.append(userStyleSheet);
    invalidateInjectedStyleSheetCache();
}

void ExtensionStyleSheets::removePageSpecificUserStyleSheet(const UserStyleSheet& userStyleSheet)
{
    bool removedStyleSheet = m_pageSpecificStyleSheets.removeFirstMatching([&](const auto& styleSheet) {
        return styleSheet.url() == userStyleSheet.url();
    });

    if (removedStyleSheet)
        invalidateInjectedStyleSheetCache();
}

bool ExtensionStyleSheets::hasCachedInjectedStyleSheets() const
{
    return !m_injectedUserStyleSheets.isEmpty()
        || !m_injectedAuthorStyleSheets.isEmpty()
        || !m_injectedStyleSheetToSource.isEmpty();
}

void ExtensionStyleSheets::invalidateInjectedStyleSheetCache()
{
    m_injectedStyleSheetCacheValid = false;
    protect(m_document)->styleScope().didChangeExtensionStyleSheets();
}

void ExtensionStyleSheets::addUserStyleSheet(Ref<StyleSheetContents>&& userSheet)
{
    ASSERT(userSheet.get().isUserStyleSheet());
    Ref document = m_document;
    m_userStyleSheets.append(CSSStyleSheet::create(WTF::move(userSheet), document));
    document->styleScope().didChangeExtensionStyleSheets();
}

void ExtensionStyleSheets::addAuthorStyleSheetForTesting(Ref<StyleSheetContents>&& authorSheet)
{
    ASSERT(!authorSheet.get().isUserStyleSheet());
    Ref document = m_document;
    m_authorStyleSheetsForTesting.append(CSSStyleSheet::create(WTF::move(authorSheet), document));
    document->styleScope().didChangeExtensionStyleSheets();
}

#if ENABLE(CONTENT_EXTENSIONS)
void ExtensionStyleSheets::addDisplayNoneSelector(const String& identifier, const String& selector, uint32_t selectorID)
{
    Ref document = m_document;
    auto result = m_contentExtensionSelectorSheets.ensure(identifier, [&] {
        return ContentExtensionStyleSheet::create(protect(document));
    });
    if (result.isNewEntry)
        m_userStyleSheets.append(result.iterator->value->styleSheet());

    if (result.iterator->value->addDisplayNoneSelector(selector, selectorID))
        document->styleScope().didChangeExtensionStyleSheets();
}

void ExtensionStyleSheets::maybeAddContentExtensionSheet(const String& identifier, StyleSheetContents& sheet)
{
    ASSERT(sheet.isUserStyleSheet());

    if (m_contentExtensionSheets.contains(identifier))
        return;

    Ref document = m_document;
    Ref cssSheet = CSSStyleSheet::create(sheet, document);
    m_contentExtensionSheets.set(identifier, cssSheet.copyRef());
    m_userStyleSheets.append(WTF::move(cssSheet));
    document->styleScope().didChangeExtensionStyleSheets();

}
#endif // ENABLE(CONTENT_EXTENSIONS)

String ExtensionStyleSheets::contentForInjectedStyleSheet(CSSStyleSheet& styleSheet) const
{
    return m_injectedStyleSheetToSource.get(styleSheet);
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
