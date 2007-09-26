/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2006, 2007 Rob Buis
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

#include "Document.h"
#include "Element.h"
#include "MappedAttribute.h"
#include "MediaList.h"
#include "MediaQueryEvaluator.h"

namespace WebCore {

StyleElement::StyleElement()
{
}

StyleSheet* StyleElement::sheet(Element* e)
{
    if (!m_sheet)
        createSheet(e);
    return m_sheet.get();
}

void StyleElement::insertedIntoDocument(Document* document, Element* element)
{
    process(element);
}

void StyleElement::removedFromDocument(Document* document)
{
    if (m_sheet)
        document->updateStyleSelector();
}

void StyleElement::process(Element* e)
{
    if (!e || !e->inDocument())
        return;

    String text = "";

    for (Node* c = e->firstChild(); c; c = c->nextSibling())
        if (c->nodeType() == Node::TEXT_NODE || c->nodeType() == Node::CDATA_SECTION_NODE || c->nodeType() == Node::COMMENT_NODE)
            text += c->nodeValue();

    createSheet(e, text);
}

void StyleElement::createSheet(Element* e, const String& text)
{
    Document* document = e->document();
    if (m_sheet) {
        if (static_cast<CSSStyleSheet*>(m_sheet.get())->isLoading())
            document->removePendingSheet();
        m_sheet = 0;
    }

    String typeValue = e->isHTMLElement() ? type().deprecatedString().lower() : type();
    if (typeValue.isEmpty() || typeValue == "text/css") { // Type must be empty or CSS
        RefPtr<MediaList> mediaList = new MediaList((CSSStyleSheet*)0, media(), e->isHTMLElement());
        MediaQueryEvaluator screenEval("screen", true);
        MediaQueryEvaluator printEval("print", true);
        if (screenEval.eval(mediaList.get()) || printEval.eval(mediaList.get())) {
            document->addPendingSheet();
            setLoading(true);
            m_sheet = new CSSStyleSheet(e, String(), document->inputEncoding());
            m_sheet->parseString(text, !document->inCompatMode());
            m_sheet->setMedia(mediaList.get());
            m_sheet->setTitle(e->title());
            setLoading(false);
        }
    }

    if (m_sheet)
        m_sheet->checkLoaded();
}

}
