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

void StyleElement::insertedIntoDocument(Document*, Element* element)
{
    process(element);
}

void StyleElement::removedFromDocument(Document* document)
{
    // If we're in document teardown, then we don't need to do any notification of our sheet's removal.
    if (!document->renderer())
        return;

    // FIXME: It's terrible to do a synchronous update of the style selector just because a <style> or <link> element got removed.
    if (m_sheet)
        document->updateStyleSelector();
}

void StyleElement::process(Element* e)
{
    if (!e || !e->inDocument())
        return;

    Vector<UChar> text;

    for (Node* c = e->firstChild(); c; c = c->nextSibling())
        if (c->nodeType() == Node::TEXT_NODE || c->nodeType() == Node::CDATA_SECTION_NODE || c->nodeType() == Node::COMMENT_NODE)
            append(text, c->nodeValue());

    createSheet(e, String::adopt(text));
}

void StyleElement::createSheet(Element* e, const String& text)
{
    Document* document = e->document();
    if (m_sheet) {
        if (static_cast<CSSStyleSheet*>(m_sheet.get())->isLoading())
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
            setLoading(true);
            m_sheet = CSSStyleSheet::create(e, String(), document->inputEncoding());
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
