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
#include "HTMLNames.h"
#include "RenderListItem.h"

namespace WebCore {

using namespace HTMLNames;

HTMLLIElement::HTMLLIElement(const QualifiedName& tagName, Document* document)
    : HTMLElement(tagName, document)
{
    ASSERT(hasTagName(liTag));
}

PassRefPtr<HTMLLIElement> HTMLLIElement::create(Document* document)
{
    return adoptRef(new HTMLLIElement(liTag, document));
}

PassRefPtr<HTMLLIElement> HTMLLIElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new HTMLLIElement(tagName, document));
}

bool HTMLLIElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == typeAttr)
        return true;
    return HTMLElement::isPresentationAttribute(name);
}

void HTMLLIElement::collectStyleForAttribute(Attribute* attr, StylePropertySet* style)
{
    if (attr->name() == typeAttr) {
        if (attr->value() == "a")
            addPropertyToAttributeStyle(style, CSSPropertyListStyleType, CSSValueLowerAlpha);
        else if (attr->value() == "A")
            addPropertyToAttributeStyle(style, CSSPropertyListStyleType, CSSValueUpperAlpha);
        else if (attr->value() == "i")
            addPropertyToAttributeStyle(style, CSSPropertyListStyleType, CSSValueLowerRoman);
        else if (attr->value() == "I")
            addPropertyToAttributeStyle(style, CSSPropertyListStyleType, CSSValueUpperRoman);
        else if (attr->value() == "1")
            addPropertyToAttributeStyle(style, CSSPropertyListStyleType, CSSValueDecimal);
        else
            addPropertyToAttributeStyle(style, CSSPropertyListStyleType, attr->value());
    } else
        HTMLElement::collectStyleForAttribute(attr, style);
}

void HTMLLIElement::parseAttribute(Attribute* attr)
{
    if (attr->name() == valueAttr) {
        if (renderer() && renderer()->isListItem())
            parseValue(attr->value());
    } else
        HTMLElement::parseAttribute(attr);
}

void HTMLLIElement::attach()
{
    ASSERT(!attached());

    HTMLElement::attach();

    if (renderer() && renderer()->isListItem()) {
        RenderListItem* render = toRenderListItem(renderer());

        // Find the enclosing list node.
        Node* listNode = 0;
        Node* n = this;
        while (!listNode && (n = n->parentNode())) {
            if (n->hasTagName(ulTag) || n->hasTagName(olTag))
                listNode = n;
        }

        // If we are not in a list, tell the renderer so it can position us inside.
        // We don't want to change our style to say "inside" since that would affect nested nodes.
        if (!listNode)
            render->setNotInList(true);

        parseValue(fastGetAttribute(valueAttr));
    }
}

inline void HTMLLIElement::parseValue(const AtomicString& value)
{
    ASSERT(renderer() && renderer()->isListItem());

    bool valueOK;
    int requestedValue = value.toInt(&valueOK);
    if (valueOK)
        toRenderListItem(renderer())->setExplicitValue(requestedValue);
    else
        toRenderListItem(renderer())->clearExplicitValue();
}

}
