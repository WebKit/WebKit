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

#include "AuthorStyleSheets.h"
#include "ContentSecurityPolicy.h"
#include "Element.h"
#include "MediaList.h"
#include "MediaQueryEvaluator.h"
#include "ScriptableDocumentParser.h"
#include "ShadowRoot.h"
#include "StyleSheetContents.h"
#include "TextNodeTraversal.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

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

static AuthorStyleSheets& authorStyleSheetsForElement(Element& element)
{
    auto* shadowRoot = element.containingShadowRoot();
    return shadowRoot ? shadowRoot->authorStyleSheets() : element.document().authorStyleSheets();
}

void InlineStyleSheetOwner::insertedIntoDocument(Document&, Element& element)
{
    authorStyleSheetsForElement(element).addStyleSheetCandidateNode(element, m_isParsingChildren);

    if (m_isParsingChildren)
        return;
    createSheetFromTextContents(element);
}

void InlineStyleSheetOwner::removedFromDocument(Document& document, Element& element)
{
    authorStyleSheetsForElement(element).removeStyleSheetCandidateNode(element);

    if (m_sheet)
        clearSheet();

    // If we're in document teardown, then we don't need to do any notification of our sheet's removal.
    if (document.hasLivingRenderTree())
        document.styleResolverChanged(DeferRecalcStyle);
}

void InlineStyleSheetOwner::clearDocumentData(Document&, Element& element)
{
    if (m_sheet)
        m_sheet->clearOwnerNode();

    if (!element.inDocument())
        return;
    authorStyleSheetsForElement(element).removeStyleSheetCandidateNode(element);
}

void InlineStyleSheetOwner::childrenChanged(Element& element)
{
    if (m_isParsingChildren)
        return;
    if (!element.inDocument())
        return;
    createSheetFromTextContents(element);
}

void InlineStyleSheetOwner::finishParsingChildren(Element& element)
{
    if (element.inDocument())
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
    ASSERT(element.inDocument());
    Document& document = element.document();
    if (m_sheet) {
        if (m_sheet->isLoading())
            document.authorStyleSheets().removePendingSheet();
        clearSheet();
    }

    if (!isValidCSSContentType(element, m_contentType))
        return;

    ASSERT(document.contentSecurityPolicy());
    const ContentSecurityPolicy& contentSecurityPolicy = *document.contentSecurityPolicy();
    bool hasKnownNonce = contentSecurityPolicy.allowStyleWithNonce(element.attributeWithoutSynchronization(HTMLNames::nonceAttr), element.isInUserAgentShadowTree());
    if (!contentSecurityPolicy.allowInlineStyle(document.url(), m_startTextPosition.m_line, text, hasKnownNonce))
        return;

    RefPtr<MediaQuerySet> mediaQueries;
    if (element.isHTMLElement())
        mediaQueries = MediaQuerySet::createAllowingDescriptionSyntax(m_media);
    else
        mediaQueries = MediaQuerySet::create(m_media);

    MediaQueryEvaluator screenEval(ASCIILiteral("screen"), true);
    MediaQueryEvaluator printEval(ASCIILiteral("print"), true);
    if (!screenEval.evaluate(*mediaQueries) && !printEval.evaluate(*mediaQueries))
        return;

    authorStyleSheetsForElement(element).addPendingSheet();

    m_loading = true;

    m_sheet = CSSStyleSheet::createInline(element, URL(), m_startTextPosition, document.encoding());
    m_sheet->setMediaQueries(mediaQueries.releaseNonNull());
    m_sheet->setTitle(element.title());
    m_sheet->contents().parseStringAtPosition(text, m_startTextPosition, m_isParsingChildren);

    m_loading = false;

    if (m_sheet)
        m_sheet->contents().checkLoaded();
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

    authorStyleSheetsForElement(element).removePendingSheet();
    return true;
}

void InlineStyleSheetOwner::startLoadingDynamicSheet(Element& element)
{
    authorStyleSheetsForElement(element).addPendingSheet();
}

}
