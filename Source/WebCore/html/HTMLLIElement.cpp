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
#include "ElementAncestorIterator.h"
#include "HTMLNames.h"
#include "HTMLOListElement.h"
#include "HTMLUListElement.h"
#include "RenderListItem.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLLIElement);

using namespace HTMLNames;

HTMLLIElement::HTMLLIElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
    ASSERT(hasTagName(liTag));
    setHasCustomStyleResolveCallbacks();
}

Ref<HTMLLIElement> HTMLLIElement::create(Document& document)
{
    return adoptRef(*new HTMLLIElement(liTag, document));
}

Ref<HTMLLIElement> HTMLLIElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLLIElement(tagName, document));
}

bool HTMLLIElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == typeAttr)
        return true;
    return HTMLElement::isPresentationAttribute(name);
}

void HTMLLIElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStyleProperties& style)
{
    if (name == typeAttr) {
        if (value == "a")
            addPropertyToPresentationAttributeStyle(style, CSSPropertyListStyleType, CSSValueLowerAlpha);
        else if (value == "A")
            addPropertyToPresentationAttributeStyle(style, CSSPropertyListStyleType, CSSValueUpperAlpha);
        else if (value == "i")
            addPropertyToPresentationAttributeStyle(style, CSSPropertyListStyleType, CSSValueLowerRoman);
        else if (value == "I")
            addPropertyToPresentationAttributeStyle(style, CSSPropertyListStyleType, CSSValueUpperRoman);
        else if (value == "1")
            addPropertyToPresentationAttributeStyle(style, CSSPropertyListStyleType, CSSValueDecimal);
        else
            addPropertyToPresentationAttributeStyle(style, CSSPropertyListStyleType, value);
    } else
        HTMLElement::collectStyleForPresentationAttribute(name, value, style);
}

void HTMLLIElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == valueAttr) {
        if (renderer() && renderer()->isListItem())
            parseValue(value);
    } else
        HTMLElement::parseAttribute(name, value);
}

void HTMLLIElement::didAttachRenderers()
{
    if (!is<RenderListItem>(renderer()))
        return;
    auto& listItemRenderer = downcast<RenderListItem>(*renderer());

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
        listItemRenderer.setNotInList(true);

    parseValue(attributeWithoutSynchronization(valueAttr));
}

inline void HTMLLIElement::parseValue(const AtomicString& value)
{
    ASSERT(renderer());

    bool valueOK;
    int requestedValue = value.toInt(&valueOK);
    if (valueOK)
        downcast<RenderListItem>(*renderer()).setExplicitValue(requestedValue);
    else
        downcast<RenderListItem>(*renderer()).setExplicitValue(WTF::nullopt);
}

}
