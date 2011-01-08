/**
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
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

#if ENABLE(WML)
#include "WMLPElement.h"

#include "Attribute.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "Document.h"
#include "HTMLNames.h"
#include "NodeList.h"
#include "WMLNames.h"

namespace WebCore {

using namespace WMLNames;

WMLPElement::WMLPElement(const QualifiedName& tagName, Document* doc)
    : WMLElement(tagName, doc)
{
}

PassRefPtr<WMLPElement> WMLPElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new WMLPElement(tagName, document));
}

bool WMLPElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == HTMLNames::alignAttr) {
        result = eBlock; // We can share with DIV here.
        return false;
    }

    return WMLElement::mapToEntry(attrName, result);
}

void WMLPElement::parseMappedAttribute(Attribute* attr)
{
    if (attr->name() == HTMLNames::alignAttr) {
        const AtomicString& value = attr->value();
        if (equalIgnoringCase(value, "middle") || equalIgnoringCase(value, "center"))
            addCSSProperty(attr, CSSPropertyTextAlign, CSSValueWebkitCenter);
        else if (equalIgnoringCase(value, "left"))
            addCSSProperty(attr, CSSPropertyTextAlign, CSSValueWebkitLeft);
        else if (equalIgnoringCase(value, "right"))
            addCSSProperty(attr, CSSPropertyTextAlign, CSSValueWebkitRight);
        else
            addCSSProperty(attr, CSSPropertyTextAlign, value);
    } else if (attr->name() == modeAttr) {
        m_mode = attr->value();
        if (m_mode == "wrap")
            addCSSProperty(attr, CSSPropertyWordWrap, CSSValueBreakWord);
        else if (m_mode == "nowrap")
            addCSSProperty(attr, CSSPropertyWhiteSpace, CSSValueNowrap);
    } else
        WMLElement::parseMappedAttribute(attr);
}

void WMLPElement::insertedIntoDocument()
{
    WMLElement::insertedIntoDocument();

    // If not explicitly specified, the linewrap mode is identical to 
    // the line-wrap mode of the previous paragraph in the text flow of
    // a card. The default mode for the first paragraph in a card is wrap.
    if (!m_mode.isEmpty())
        return;

    RefPtr<NodeList> nodeList = document()->getElementsByTagName("p");
    if (!nodeList)
        return;

    unsigned length = nodeList->length();
    if (length < 2)
        return;

    // Assure we're the last inserted paragraph element
    // This only works while parsing, otherwhise this statement is never true.
    if (nodeList->item(length - 1) != this)
        return;

    WMLPElement* lastParagraph = static_cast<WMLPElement*>(nodeList->item(length - 2));
    ASSERT(lastParagraph);

    String lastMode = lastParagraph->getAttribute(modeAttr);
    if (lastMode.isEmpty() || lastMode == "wrap") // Default value, do nothing.
        return;

    setAttribute(modeAttr, lastMode);
}

}

#endif
