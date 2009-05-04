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
#include "HTMLUListElement.h"

#include "CSSPropertyNames.h"
#include "HTMLNames.h"
#include "MappedAttribute.h"

namespace WebCore {

using namespace HTMLNames;

HTMLUListElement::HTMLUListElement(const QualifiedName& tagName, Document* doc)
    : HTMLElement(tagName, doc)
{
    ASSERT(hasTagName(ulTag));
}

bool HTMLUListElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == typeAttr) {
        result = eUnorderedList;
        return false;
    }
    
    return HTMLElement::mapToEntry(attrName, result);
}

void HTMLUListElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == typeAttr)
        addCSSProperty(attr, CSSPropertyListStyleType, attr->value());
    else
        HTMLElement::parseMappedAttribute(attr);
}

bool HTMLUListElement::compact() const
{
    return !getAttribute(compactAttr).isNull();
}

void HTMLUListElement::setCompact(bool b)
{
    setAttribute(compactAttr, b ? "" : 0);
}

String HTMLUListElement::type() const
{
    return getAttribute(typeAttr);
}

void HTMLUListElement::setType(const String &value)
{
    setAttribute(typeAttr, value);
}

}
