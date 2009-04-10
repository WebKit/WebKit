/*
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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
 *
 */

#include "config.h"
#include "OptionElement.h"

#include "Document.h"
#include "Element.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "OptionGroupElement.h"
#include "ScriptElement.h"
#include <wtf/Assertions.h>

#if ENABLE(WML)
#include "WMLOptionElement.h"
#include "WMLNames.h"
#endif

namespace WebCore {

void OptionElement::setSelectedState(OptionElementData& data, bool selected)
{
    if (data.selected() == selected)
        return;

    data.setSelected(selected);
    data.element()->setNeedsStyleRecalc();
}

String OptionElement::collectOptionText(const OptionElementData& data, Document* document)
{
    String text;

    // WinIE does not use the label attribute, so as a quirk, we ignore it.
    if (!document->inCompatMode())
        text = data.label();

    if (text.isEmpty()) {
        Node* n = data.element()->firstChild();
        while (n) {
            if (n->nodeType() == Node::TEXT_NODE || n->nodeType() == Node::CDATA_SECTION_NODE)
                text += n->nodeValue();

            // skip script content
            if (n->isElementNode() && toScriptElement(static_cast<Element*>(n)))
                n = n->traverseNextSibling(data.element());
            else
                n = n->traverseNextNode(data.element());
        }
    }

    text = document->displayStringModifiedByEncoding(text);

    // In WinIE, leading and trailing whitespace is ignored in options and optgroups. We match this behavior.
    text = text.stripWhiteSpace();

    // We want to collapse our whitespace too.  This will match other browsers.
    text = text.simplifyWhiteSpace();
    return text;
}

String OptionElement::collectOptionTextRespectingGroupLabel(const OptionElementData& data, Document* document)
{
    Element* parentElement = static_cast<Element*>(data.element()->parentNode());
    if (parentElement && toOptionGroupElement(parentElement))
        return "    " + collectOptionText(data, document);

    return collectOptionText(data, document);
}

String OptionElement::collectOptionValue(const OptionElementData& data, Document* document)
{
    String value = data.value();
    if (!value.isNull())
        return value;

    // Use the text if the value wasn't set.
    return collectOptionText(data, document).stripWhiteSpace();
}

// OptionElementData
OptionElementData::OptionElementData(Element* element)
    : m_element(element)
    , m_selected(false)
{
    ASSERT(m_element);
}

OptionElementData::~OptionElementData()
{
}

OptionElement* toOptionElement(Element* element)
{
    if (element->isHTMLElement() && element->hasTagName(HTMLNames::optionTag))
        return static_cast<HTMLOptionElement*>(element);

#if ENABLE(WML)
    if (element->isWMLElement() && element->hasTagName(WMLNames::optionTag))
        return static_cast<WMLOptionElement*>(element);
#endif

    return 0;
}

}
