/**
 * This file is part of the DOM implementation for KDE.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */
#include "config.h"
#include "HTMLOListElement.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "HTMLNames.h"

namespace WebCore {

using namespace HTMLNames;

HTMLOListElement::HTMLOListElement(Document* doc)
    : HTMLElement(HTMLNames::olTag, doc)
    , m_start(1)
{
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
            addCSSProperty(attr, CSS_PROP_LIST_STYLE_TYPE, CSS_VAL_LOWER_ALPHA);
        else if (attr->value() == "A")
            addCSSProperty(attr, CSS_PROP_LIST_STYLE_TYPE, CSS_VAL_UPPER_ALPHA);
        else if (attr->value() == "i")
            addCSSProperty(attr, CSS_PROP_LIST_STYLE_TYPE, CSS_VAL_LOWER_ROMAN);
        else if (attr->value() == "I")
            addCSSProperty(attr, CSS_PROP_LIST_STYLE_TYPE, CSS_VAL_UPPER_ROMAN);
        else if (attr->value() == "1")
            addCSSProperty(attr, CSS_PROP_LIST_STYLE_TYPE, CSS_VAL_DECIMAL);
    } else if (attr->name() == startAttr)
        m_start = !attr->isNull() ? attr->value().toInt() : 1;
    else
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
