/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2006, 2007, 2010 Apple Inc. All rights reserved.
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
#include "HTMLLIElement.h"

#include "Attribute.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "ElementAncestorIteratorInlines.h"
#include "HTMLNames.h"
#include "HTMLOListElement.h"
#include "HTMLParserIdioms.h"
#include "HTMLUListElement.h"
#include "RenderListItem.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLLIElement);

using namespace HTMLNames;

HTMLLIElement::HTMLLIElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document, TypeFlag::HasCustomStyleResolveCallbacks)
{
    ASSERT(hasTagName(liTag));
}

Ref<HTMLLIElement> HTMLLIElement::create(Document& document)
{
    return adoptRef(*new HTMLLIElement(liTag, document));
}

Ref<HTMLLIElement> HTMLLIElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLLIElement(tagName, document));
}

bool HTMLLIElement::hasPresentationalHintsForAttribute(const QualifiedName& name) const
{
    if (name == typeAttr || name == valueAttr)
        return true;
    return HTMLElement::hasPresentationalHintsForAttribute(name);
}

void HTMLLIElement::collectPresentationalHintsForAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    if (name == typeAttr) {
        if (value == "a"_s)
            addPropertyToPresentationalHintStyle(style, CSSPropertyListStyleType, CSSValueLowerAlpha);
        else if (value == "A"_s)
            addPropertyToPresentationalHintStyle(style, CSSPropertyListStyleType, CSSValueUpperAlpha);
        else if (value == "i"_s)
            addPropertyToPresentationalHintStyle(style, CSSPropertyListStyleType, CSSValueLowerRoman);
        else if (value == "I"_s)
            addPropertyToPresentationalHintStyle(style, CSSPropertyListStyleType, CSSValueUpperRoman);
        else if (value == "1"_s)
            addPropertyToPresentationalHintStyle(style, CSSPropertyListStyleType, CSSValueDecimal);
        else {
            auto valueLowerCase = value.convertToASCIILowercase();
            if (valueLowerCase == "disc"_s)
                addPropertyToPresentationalHintStyle(style, CSSPropertyListStyleType, CSSValueDisc);
            else if (valueLowerCase == "circle"_s)
                addPropertyToPresentationalHintStyle(style, CSSPropertyListStyleType, CSSValueCircle);
            else if (valueLowerCase == "round"_s)
                addPropertyToPresentationalHintStyle(style, CSSPropertyListStyleType, CSSValueRound);
            else if (valueLowerCase == "square"_s)
                addPropertyToPresentationalHintStyle(style, CSSPropertyListStyleType, CSSValueSquare);
            else if (valueLowerCase == "none"_s)
                addPropertyToPresentationalHintStyle(style, CSSPropertyListStyleType, CSSValueNone);
        }
    } else if (name == valueAttr) {
        if (auto parsedValue = parseHTMLInteger(value))
            addPropertyToPresentationalHintStyle(style, CSSPropertyCounterSet, makeString("list-item "_s, *parsedValue));
    } else
        HTMLElement::collectPresentationalHintsForAttribute(name, value, style);
}

void HTMLLIElement::didAttachRenderers()
{
    CheckedPtr listItemRenderer = dynamicDowncast<RenderListItem>(*renderer());
    if (!listItemRenderer)
        return;

    // Check if there is an enclosing list.
    bool isInList = false;
    for (auto& ancestor : ancestorsOfType<HTMLElement>(*this)) {
        if (is<HTMLUListElement>(ancestor) || is<HTMLOListElement>(ancestor)) {
            isInList = true;
            break;
        }
    }

    // If we are not in a list, tell the renderer so it can position us inside.
    // We don't want to change our style to say "inside" since that would affect nested nodes.
    if (!isInList)
        listItemRenderer->setNotInList(true);
}

}
