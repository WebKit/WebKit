/*
 * Copyright (C) 2006, 2007 Rob Buis
 * Copyright (C) 2008, 2013 Apple, Inc. All rights reserved.
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
#include "MediaList.h"
#include "MediaQueryEvaluator.h"
#include "ScriptableDocumentParser.h"
#include "StyleSheetContents.h"
#include "TextNodeTraversal.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

InlineStyleSheetOwner::InlineStyleSheetOwner(Document& document, bool createdByParser)
    : m_isParsingChildren(createdByParser)
    , m_loading(false)
    , m_startLineNumber(WTF::OrdinalNumber::beforeFirst())
{
    if (createdByParser && document.scriptableDocumentParser() && !document.isInDocumentWrite())
        m_startLineNumber = document.scriptableDocumentParser()->textPosition().m_line;
}

InlineStyleSheetOwner::~InlineStyleSheetOwner()
{
}

void InlineStyleSheetOwner::insertedIntoDocument(Document& document, Element& element)
{
    document.styleSheetCollection().addStyleSheetCandidateNode(element, m_isParsingChildren);

    if (m_isParsingChildren)
        return;
    createSheetFromTextContents(element);
}

void InlineStyleSheetOwner::removedFromDocument(Document& document, Element& element)
{
    document.styleSheetCollection().removeStyleSheetCandidateNode(element);

    if (m_sheet)
        clearSheet();

    // If we're in document teardown, then we don't need to do any notification of our sheet's removal.
    if (document.hasLivingRenderTree())
        document.styleResolverChanged(DeferRecalcStyle);
}

void InlineStyleSheetOwner::clearDocumentData(Document& document, Element& element)
{
    if (m_sheet)
        m_sheet->clearOwnerNode();

    if (!element.inDocument())
        return;
    document.styleSheetCollection().removeStyleSheetCandidateNode(element);
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
    createSheet(element, TextNodeTraversal::contentsAsString(&element));
}

void InlineStyleSheetOwner::clearSheet()
{
    ASSERT(m_sheet);
    m_sheet.release()->clearOwnerNode();
}

inline bool isValidCSSContentType(Element& element, const AtomicString& type)
{
    DEPRECATED_DEFINE_STATIC_LOCAL(const AtomicString, cssContentType, ("text/css", AtomicString::ConstructFromLiteral));
    if (type.isEmpty())
        return true;
    return element.isHTMLElement() ? equalIgnoringCase(type, cssContentType) : type == cssContentType;
}

void InlineStyleSheetOwner::createSheet(Element& element, const String& text)
{
    ASSERT(element.inDocument());
    Document& document = element.document();
    if (m_sheet) {
        if (m_sheet->isLoading())
            document.styleSheetCollection().removePendingSheet();
        clearSheet();
    }

    if (!isValidCSSContentType(element, m_contentType))
        return;
    if (!document.contentSecurityPolicy()->allowInlineStyle(document.url(), m_startLineNumber))
        return;

    RefPtr<MediaQuerySet> mediaQueries;
    if (element.isHTMLElement())
        mediaQueries = MediaQuerySet::createAllowingDescriptionSyntax(m_media);
    else
        mediaQueries = MediaQuerySet::create(m_media);

    MediaQueryEvaluator screenEval(ASCIILiteral("screen"), true);
    MediaQueryEvaluator printEval(ASCIILiteral("print"), true);
    if (!screenEval.eval(mediaQueries.get()) && !printEval.eval(mediaQueries.get()))
        return;

    document.styleSheetCollection().addPendingSheet();

    m_loading = true;

    m_sheet = CSSStyleSheet::createInline(element, URL(), document.inputEncoding());
    m_sheet->setMediaQueries(mediaQueries.release());
    m_sheet->setTitle(element.title());
    m_sheet->contents().parseStringAtLine(text, m_startLineNumber.zeroBasedInt(), m_isParsingChildren);

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

bool InlineStyleSheetOwner::sheetLoaded(Document& document)
{
    if (isLoading())
        return false;

    document.styleSheetCollection().removePendingSheet();
    return true;
}

void InlineStyleSheetOwner::startLoadingDynamicSheet(Document& document)
{
    document.styleSheetCollection().addPendingSheet();
}

}
