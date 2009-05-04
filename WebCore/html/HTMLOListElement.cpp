/**
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
#include "HTMLOListElement.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "HTMLNames.h"
#include "MappedAttribute.h"
#include "RenderListItem.h"

namespace WebCore {

using namespace HTMLNames;

HTMLOListElement::HTMLOListElement(const QualifiedName& tagName, Document* doc)
    : HTMLElement(tagName, doc)
    , m_start(1)
{
    ASSERT(hasTagName(olTag));
}

bool HTMLOListElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == typeAttr) {
        result = eListItem; // Share with <li>
        return false;
    }
    
    return HTMLElement::mapToEntry(attrName, result);
}

void HTMLOListElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == typeAttr) {
        if (attr->value() == "a")
            addCSSProperty(attr, CSSPropertyListStyleType, CSSValueLowerAlpha);
        else if (attr->value() == "A")
            addCSSProperty(attr, CSSPropertyListStyleType, CSSValueUpperAlpha);
        else if (attr->value() == "i")
            addCSSProperty(attr, CSSPropertyListStyleType, CSSValueLowerRoman);
        else if (attr->value() == "I")
            addCSSProperty(attr, CSSPropertyListStyleType, CSSValueUpperRoman);
        else if (attr->value() == "1")
            addCSSProperty(attr, CSSPropertyListStyleType, CSSValueDecimal);
    } else if (attr->name() == startAttr) {
        int s = !attr->isNull() ? attr->value().toInt() : 1;
        if (s != m_start) {
            m_start = s;
            for (RenderObject* r = renderer(); r; r = r->nextInPreOrder(renderer()))
                if (r->isListItem())
                    static_cast<RenderListItem*>(r)->updateValue();
        }
    } else
        HTMLElement::parseMappedAttribute(attr);
}

bool HTMLOListElement::compact() const
{
    return !getAttribute(compactAttr).isNull();
}

void HTMLOListElement::setCompact(bool b)
{
    setAttribute(compactAttr, b ? "" : 0);
}

void HTMLOListElement::setStart(int start)
{
    setAttribute(startAttr, String::number(start));
}

String HTMLOListElement::type() const
{
    return getAttribute(typeAttr);
}

void HTMLOListElement::setType(const String& value)
{
    setAttribute(typeAttr, value);
}
}
