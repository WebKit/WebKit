/*
 * Copyright (C) 2006, 2007 Rob Buis
 * Copyright (C) 2008-2016 Apple, Inc. All rights reserved.
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
#include "InlineStyleSheetOwner.h"

#include "CommonAtomStrings.h"
#include "ContentSecurityPolicy.h"
#include "ElementInlines.h"
#include "Logging.h"
#include "MediaList.h"
#include "MediaQueryParser.h"
#include "MediaQueryParserContext.h"
#include "ScriptableDocumentParser.h"
#include "ShadowRoot.h"
#include "StyleScope.h"
#include "StyleSheetContents.h"
#include "StyleSheetContentsCache.h"
#include "TextNodeTraversal.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static CSSParserContext parserContextForElement(const Element& element)
{
    auto* shadowRoot = element.containingShadowRoot();
    // User agent shadow trees can't contain document-relative URLs. Use blank URL as base allowing cross-document sharing.
    auto& baseURL = shadowRoot && shadowRoot->mode() == ShadowRootMode::UserAgent ? aboutBlankURL() : element.document().baseURL();

    CSSParserContext result = CSSParserContext { element.document(), baseURL, element.document().characterSetWithUTF8Fallback() };
    if (shadowRoot && shadowRoot->mode() == ShadowRootMode::UserAgent)
        result.mode = UASheetMode;
    return result;
}

static std::optional<Style::StyleSheetContentsCache::Key> makeStyleSheetContentsCacheKey(const String& text, const Element& element)
{
    // Only cache for shadow trees. Main document inline stylesheets are generally unique and can't be shared between documents.
    // FIXME: This could be relaxed when a stylesheet does not contain document-relative URLs (or #urls).
    if (!element.isInShadowTree())
        return { };

    return Style::StyleSheetContentsCache::Key { text, parserContextForElement(element) };
}

InlineStyleSheetOwner::InlineStyleSheetOwner(Document& document, bool createdByParser)
    : m_isParsingChildren(createdByParser)
    , m_loading(false)
    , m_startTextPosition()
{
    if (createdByParser && document.scriptableDocumentParser() && !document.isInDocumentWrite())
        m_startTextPosition = document.scriptableDocumentParser()->textPosition();
}

InlineStyleSheetOwner::~InlineStyleSheetOwner()
{
    if (m_sheet)
        clearSheet();
}

void InlineStyleSheetOwner::insertedIntoDocument(Element& element)
{
    m_styleScope = Style::Scope::forNode(element);
    m_styleScope->addStyleSheetCandidateNode(element, m_isParsingChildren);

    if (m_isParsingChildren)
        return;
    createSheetFromTextContents(element);
}

void InlineStyleSheetOwner::removedFromDocument(Element& element)
{
    if (CheckedPtr scope = m_styleScope.get()) {
        if (scope->hasPendingSheet(element))
            scope->removePendingSheet(element);
        scope->removeStyleSheetCandidateNode(element);
    }
    if (m_sheet)
        clearSheet();
}

void InlineStyleSheetOwner::clearDocumentData(Element& element)
{
    if (RefPtr sheet = m_sheet)
        sheet->clearOwnerNode();

    if (CheckedPtr scope = m_styleScope.get())
        scope->removeStyleSheetCandidateNode(element);
}

void InlineStyleSheetOwner::childrenChanged(Element& element)
{
    if (m_isParsingChildren)
        return;
    if (!element.isConnected())
        return;
    createSheetFromTextContents(element);
}

void InlineStyleSheetOwner::finishParsingChildren(Element& element)
{
    if (element.isConnected())
        createSheetFromTextContents(element);
    m_isParsingChildren = false;
}

void InlineStyleSheetOwner::createSheetFromTextContents(Element& element)
{
    createSheet(element, TextNodeTraversal::contentsAsString(element));
}

void InlineStyleSheetOwner::clearSheet()
{
    ASSERT(m_sheet);
    RefPtr sheet = std::exchange(m_sheet, nullptr);
    sheet->clearOwnerNode();
}

inline bool isValidCSSContentType(const AtomString& type)
{
    // https://html.spec.whatwg.org/multipage/semantics.html#update-a-style-block
    if (type.isEmpty())
        return true;
    return equalLettersIgnoringASCIICase(type, "text/css"_s);
}

void InlineStyleSheetOwner::createSheet(Element& element, const String& text)
{
    ASSERT(element.isConnected());
    Ref document = element.document();
    if (RefPtr sheet = m_sheet) {
        if (sheet->isLoading() && m_styleScope)
            CheckedRef { *m_styleScope }->removePendingSheet(element);
        clearSheet();
    }

    if (!isValidCSSContentType(m_contentType))
        return;

    ASSERT(document->contentSecurityPolicy());
    if (!document->checkedContentSecurityPolicy()->allowInlineStyle(document->url().string(), m_startTextPosition.m_line, text, CheckUnsafeHashes::No, element, element.nonce(), element.isInUserAgentShadowTree())) {
        element.notifyLoadedSheetAndAllCriticalSubresources(true);
        return;
    }

    auto mediaQueries = MQ::MediaQueryParser::parse(m_media, MediaQueryParserContext(document));

    if (CheckedPtr scope = m_styleScope.get())
        scope->addPendingSheet(element);

    auto cacheKey = makeStyleSheetContentsCacheKey(text, element);
    if (cacheKey) {
        if (RefPtr cachedSheet = Style::StyleSheetContentsCache::singleton().get(*cacheKey)) {
            ASSERT(cachedSheet->isCacheable());
            Ref sheet = CSSStyleSheet::createInline(*cachedSheet, element, m_startTextPosition);
            m_sheet = sheet.copyRef();
            sheet->setMediaQueries(WTFMove(mediaQueries));
            if (!element.isInShadowTree())
                sheet->setTitle(element.title());

            sheetLoaded(element);
            element.notifyLoadedSheetAndAllCriticalSubresources(false);
            return;
        }
    }

    m_loading = true;

    Ref contents = StyleSheetContents::create(String(), parserContextForElement(element));

    Ref sheet = CSSStyleSheet::createInline(contents.get(), element, m_startTextPosition);
    m_sheet = sheet.copyRef();
    sheet->setMediaQueries(WTFMove(mediaQueries));
    if (!element.isInShadowTree())
        sheet->setTitle(element.title());

    contents->parseString(text);

    m_loading = false;

    contents->checkLoaded();

    if (cacheKey && contents->isCacheable())
        Style::StyleSheetContentsCache::singleton().add(WTFMove(*cacheKey), contents);
}

bool InlineStyleSheetOwner::isLoading() const
{
    if (m_loading)
        return true;
    return m_sheet && m_sheet->isLoading();
}

bool InlineStyleSheetOwner::sheetLoaded(Element& element)
{
    if (isLoading())
        return false;

    if (CheckedPtr scope = m_styleScope.get())
        scope->removePendingSheet(element);

    return true;
}

void InlineStyleSheetOwner::startLoadingDynamicSheet(Element& element)
{
    if (CheckedPtr scope = m_styleScope.get(); scope && !scope->hasPendingSheet(element))
        scope->addPendingSheet(element);
}

}
