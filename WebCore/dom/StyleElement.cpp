/*
 * Copyright (C) 2006, 2007 Rob Buis
 * Copyright (C) 2008 Apple, Inc. All rights reserved.
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
#include "StyleElement.h"

#include "Attribute.h"
#include "Document.h"
#include "Element.h"
#include "MediaList.h"
#include "MediaQueryEvaluator.h"
#include "ScriptableDocumentParser.h"

namespace WebCore {

StyleElement::StyleElement(Document* document, bool createdByParser)
    : m_createdByParser(createdByParser)
    , m_loading(false)
    , m_startLineNumber(0)
{
    if (createdByParser && document && document->scriptableDocumentParser())
        m_startLineNumber = document->scriptableDocumentParser()->lineNumber();
}

StyleSheet* StyleElement::sheet(Element* e)
{
    if (!m_sheet)
        createSheet(e, 0);
    return m_sheet.get();
}

void StyleElement::insertedIntoDocument(Document* document, Element* element)
{
    ASSERT(document);
    ASSERT(element);
    document->addStyleSheetCandidateNode(element, m_createdByParser);
    if (m_createdByParser)
        return;

    process(element);
}

void StyleElement::removedFromDocument(Document* document, Element* element)
{
    ASSERT(document);
    ASSERT(element);
    document->removeStyleSheetCandidateNode(element);

    // If we're in document teardown, then we don't need to do any notification of our sheet's removal.
    if (!document->renderer())
        return;

    if (m_sheet)
        document->styleSelectorChanged(DeferRecalcStyle);
}

void StyleElement::childrenChanged(Element* element)
{
    ASSERT(element);
    if (m_createdByParser)
        return;

    process(element);
}

void StyleElement::finishParsingChildren(Element* element)
{
    ASSERT(element);
    process(element);
    sheet(element);
    m_createdByParser = false;
}

void StyleElement::process(Element* e)
{
    if (!e || !e->inDocument())
        return;

    unsigned resultLength = 0;
    for (Node* c = e->firstChild(); c; c = c->nextSibling()) {
        Node::NodeType nodeType = c->nodeType();
        if (nodeType == Node::TEXT_NODE || nodeType == Node::CDATA_SECTION_NODE || nodeType == Node::COMMENT_NODE)
            resultLength += c->nodeValue().length();
    }
    UChar* text;
    String sheetText = String::createUninitialized(resultLength, text);

    UChar* p = text;
    for (Node* c = e->firstChild(); c; c = c->nextSibling()) {
        Node::NodeType nodeType = c->nodeType();
        if (nodeType == Node::TEXT_NODE || nodeType == Node::CDATA_SECTION_NODE || nodeType == Node::COMMENT_NODE) {
            String nodeValue = c->nodeValue();
            unsigned nodeLength = nodeValue.length();
            memcpy(p, nodeValue.characters(), nodeLength * sizeof(UChar));
            p += nodeLength;
        }
    }
    ASSERT(p == text + resultLength);

    createSheet(e, m_startLineNumber, sheetText);
}

void StyleElement::createSheet(Element* e, int startLineNumber, const String& text)
{
    ASSERT(e);
    Document* document = e->document();
    if (m_sheet) {
        if (m_sheet->isLoading())
            document->removePendingSheet();
        m_sheet = 0;
    }

    // If type is empty or CSS, this is a CSS style sheet.
    const AtomicString& type = this->type();
    if (type.isEmpty() || (e->isHTMLElement() ? equalIgnoringCase(type, "text/css") : (type == "text/css"))) {
        RefPtr<MediaList> mediaList = MediaList::create(media(), e->isHTMLElement());
        MediaQueryEvaluator screenEval("screen", true);
        MediaQueryEvaluator printEval("print", true);
        if (screenEval.eval(mediaList.get()) || printEval.eval(mediaList.get())) {
            document->addPendingSheet();
            m_loading = true;
            m_sheet = CSSStyleSheet::create(e, String(), KURL(), document->inputEncoding());
            m_sheet->parseStringAtLine(text, !document->inCompatMode(), startLineNumber);
            m_sheet->setMedia(mediaList.get());
            m_sheet->setTitle(e->title());
            m_loading = false;
        }
    }

    if (m_sheet)
        m_sheet->checkLoaded();
}

bool StyleElement::isLoading() const
{
    if (m_loading)
        return true;
    return m_sheet ? m_sheet->isLoading() : false;
}

bool StyleElement::sheetLoaded(Document* document)
{
    ASSERT(document);
    if (isLoading())
        return false;

    document->removePendingSheet();
    return true;
}

}
