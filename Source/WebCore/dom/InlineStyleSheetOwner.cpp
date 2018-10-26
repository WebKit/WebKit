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

#include "ContentSecurityPolicy.h"
#include "Element.h"
#include "Logging.h"
#include "MediaList.h"
#include "MediaQueryEvaluator.h"
#include "MediaQueryParser.h"
#include "ScriptableDocumentParser.h"
#include "ShadowRoot.h"
#include "StyleScope.h"
#include "StyleSheetContents.h"
#include "TextNodeTraversal.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

using InlineStyleSheetCacheKey = std::pair<String, CSSParserContext>;
using InlineStyleSheetCache = HashMap<InlineStyleSheetCacheKey, RefPtr<StyleSheetContents>>;

static InlineStyleSheetCache& inlineStyleSheetCache()
{
    static NeverDestroyed<InlineStyleSheetCache> cache;
    return cache;
}

static CSSParserContext parserContextForElement(const Element& element)
{
    auto* shadowRoot = element.containingShadowRoot();
    // User agent shadow trees can't contain document-relative URLs. Use blank URL as base allowing cross-document sharing.
    auto& baseURL = shadowRoot && shadowRoot->mode() == ShadowRootMode::UserAgent ? blankURL() : element.document().baseURL();

    CSSParserContext result = CSSParserContext { element.document(), baseURL, element.document().characterSetWithUTF8Fallback() };
    if (shadowRoot && shadowRoot->mode() == ShadowRootMode::UserAgent)
        result.mode = UASheetMode;
    return result;
}

static std::optional<InlineStyleSheetCacheKey> makeInlineStyleSheetCacheKey(const String& text, const Element& element)
{
    // Only cache for shadow trees. Main document inline stylesheets are generally unique and can't be shared between documents.
    // FIXME: This could be relaxed when a stylesheet does not contain document-relative URLs (or #urls).
    if (!element.isInShadowTree())
        return { };

    return std::make_pair(text, parserContextForElement(element));
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
    m_styleScope = &Style::Scope::forNode(element);
    m_styleScope->addStyleSheetCandidateNode(element, m_isParsingChildren);

    if (m_isParsingChildren)
        return;
    createSheetFromTextContents(element);
}

void InlineStyleSheetOwner::removedFromDocument(Element& element)
{
    if (m_styleScope) {
        m_styleScope->removeStyleSheetCandidateNode(element);
        m_styleScope = nullptr;
    }
    if (m_sheet)
        clearSheet();
}

void InlineStyleSheetOwner::clearDocumentData(Element& element)
{
    if (m_sheet)
        m_sheet->clearOwnerNode();

    if (m_styleScope) {
        m_styleScope->removeStyleSheetCandidateNode(element);
        m_styleScope = nullptr;
    }
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
    auto sheet = WTFMove(m_sheet);
    sheet->clearOwnerNode();
}

inline bool isValidCSSContentType(Element& element, const AtomicString& type)
{
    if (type.isEmpty())
        return true;
    // FIXME: Should MIME types really be case sensitive in XML documents? Doesn't seem like they should,
    // even though other things are case sensitive in that context. MIME types should never be case sensitive.
    // We should verify this and then remove the isHTMLElement check here.
    static NeverDestroyed<const AtomicString> cssContentType("text/css", AtomicString::ConstructFromLiteral);
    return element.isHTMLElement() ? equalLettersIgnoringASCIICase(type, "text/css") : type == cssContentType;
}

void InlineStyleSheetOwner::createSheet(Element& element, const String& text)
{
    ASSERT(element.isConnected());
    Document& document = element.document();
    if (m_sheet) {
        if (m_sheet->isLoading() && m_styleScope)
            m_styleScope->removePendingSheet(element);
        clearSheet();
    }

    if (!isValidCSSContentType(element, m_contentType))
        return;

    ASSERT(document.contentSecurityPolicy());
    const ContentSecurityPolicy& contentSecurityPolicy = *document.contentSecurityPolicy();
    bool hasKnownNonce = contentSecurityPolicy.allowStyleWithNonce(element.attributeWithoutSynchronization(HTMLNames::nonceAttr), element.isInUserAgentShadowTree());
    if (!contentSecurityPolicy.allowInlineStyle(document.url(), m_startTextPosition.m_line, text, hasKnownNonce))
        return;

    RefPtr<MediaQuerySet> mediaQueries = MediaQuerySet::create(m_media, MediaQueryParserContext(document));

    MediaQueryEvaluator screenEval("screen"_s, true);
    MediaQueryEvaluator printEval("print"_s, true);
    LOG(MediaQueries, "InlineStyleSheetOwner::createSheet evaluating queries");
    if (!screenEval.evaluate(*mediaQueries) && !printEval.evaluate(*mediaQueries))
        return;

    if (m_styleScope)
        m_styleScope->addPendingSheet(element);

    auto cacheKey = makeInlineStyleSheetCacheKey(text, element);
    if (cacheKey) {
        if (auto* cachedSheet = inlineStyleSheetCache().get(*cacheKey)) {
            ASSERT(cachedSheet->isCacheable());
            m_sheet = CSSStyleSheet::createInline(*cachedSheet, element, m_startTextPosition);
            m_sheet->setMediaQueries(mediaQueries.releaseNonNull());
            m_sheet->setTitle(element.title());

            sheetLoaded(element);
            element.notifyLoadedSheetAndAllCriticalSubresources(false);
            return;
        }
    }

    m_loading = true;

    auto contents = StyleSheetContents::create(String(), parserContextForElement(element));

    m_sheet = CSSStyleSheet::createInline(contents.get(), element, m_startTextPosition);
    m_sheet->setMediaQueries(mediaQueries.releaseNonNull());
    m_sheet->setTitle(element.title());

    contents->parseString(text);

    m_loading = false;

    contents->checkLoaded();

    if (cacheKey && contents->isCacheable()) {
        m_sheet->contents().addedToMemoryCache();
        inlineStyleSheetCache().add(*cacheKey, &m_sheet->contents());

        // Prevent pathological growth.
        const size_t maximumInlineStyleSheetCacheSize = 50;
        if (inlineStyleSheetCache().size() > maximumInlineStyleSheetCacheSize) {
            auto toRemove = inlineStyleSheetCache().random();
            toRemove->value->removedFromMemoryCache();
            inlineStyleSheetCache().remove(toRemove);
        }
    }
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

    if (m_styleScope)
        m_styleScope->removePendingSheet(element);

    return true;
}

void InlineStyleSheetOwner::startLoadingDynamicSheet(Element& element)
{
    if (m_styleScope)
        m_styleScope->addPendingSheet(element);
}

void InlineStyleSheetOwner::clearCache()
{
    inlineStyleSheetCache().clear();
}

}
