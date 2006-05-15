/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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
#include "HTMLHRElement.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "HTMLNames.h"

namespace WebCore {

using namespace HTMLNames;

HTMLHRElement::HTMLHRElement(Document *doc)
    : HTMLElement(hrTag, doc)
{
}

HTMLHRElement::~HTMLHRElement()
{
}

bool HTMLHRElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == alignAttr ||
        attrName == widthAttr ||
        attrName == colorAttr ||
        attrName == sizeAttr ||
        attrName == noshadeAttr) {
        result = eHR;
        return false;
    }
    return HTMLElement::mapToEntry(attrName, result);
}

void HTMLHRElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == alignAttr) {
        if (equalIgnoringCase(attr->value(), "left")) {
            addCSSProperty(attr, CSS_PROP_MARGIN_LEFT, "0");
            addCSSProperty(attr, CSS_PROP_MARGIN_RIGHT, CSS_VAL_AUTO);
        } else if (equalIgnoringCase(attr->value(), "right")) {
            addCSSProperty(attr, CSS_PROP_MARGIN_LEFT, CSS_VAL_AUTO);
            addCSSProperty(attr, CSS_PROP_MARGIN_RIGHT, "0");
        } else {
            addCSSProperty(attr, CSS_PROP_MARGIN_LEFT, CSS_VAL_AUTO);
            addCSSProperty(attr, CSS_PROP_MARGIN_RIGHT, CSS_VAL_AUTO);
        }
    } else if (attr->name() == widthAttr) {
        bool ok;
        int v = attr->value().impl()->toInt(&ok);
        if(ok && !v)
            addCSSLength(attr, CSS_PROP_WIDTH, "1");
        else
            addCSSLength(attr, CSS_PROP_WIDTH, attr->value());
    } else if (attr->name() == colorAttr) {
        addCSSProperty(attr, CSS_PROP_BORDER_TOP_STYLE, CSS_VAL_SOLID);
        addCSSProperty(attr, CSS_PROP_BORDER_RIGHT_STYLE, CSS_VAL_SOLID);
        addCSSProperty(attr, CSS_PROP_BORDER_BOTTOM_STYLE, CSS_VAL_SOLID);
        addCSSProperty(attr, CSS_PROP_BORDER_LEFT_STYLE, CSS_VAL_SOLID);
        addCSSColor(attr, CSS_PROP_BORDER_COLOR, attr->value());
        addCSSColor(attr, CSS_PROP_BACKGROUND_COLOR, attr->value());
    } else if (attr->name() == noshadeAttr) {
        addCSSProperty(attr, CSS_PROP_BORDER_TOP_STYLE, CSS_VAL_SOLID);
        addCSSProperty(attr, CSS_PROP_BORDER_RIGHT_STYLE, CSS_VAL_SOLID);
        addCSSProperty(attr, CSS_PROP_BORDER_BOTTOM_STYLE, CSS_VAL_SOLID);
        addCSSProperty(attr, CSS_PROP_BORDER_LEFT_STYLE, CSS_VAL_SOLID);
        addCSSColor(attr, CSS_PROP_BORDER_COLOR, String("grey"));
        addCSSColor(attr, CSS_PROP_BACKGROUND_COLOR, String("grey"));
    } else if (attr->name() == sizeAttr) {
        StringImpl* si = attr->value().impl();
        int size = si->toInt();
        if (size <= 1)
            addCSSProperty(attr, CSS_PROP_BORDER_BOTTOM_WIDTH, String("0"));
        else
            addCSSLength(attr, CSS_PROP_HEIGHT, String::number(size-2));
    } else
        HTMLElement::parseMappedAttribute(attr);
}

String HTMLHRElement::align() const
{
    return getAttribute(alignAttr);
}

void HTMLHRElement::setAlign(const String &value)
{
    setAttribute(alignAttr, value);
}

bool HTMLHRElement::noShade() const
{
    return !getAttribute(noshadeAttr).isNull();
}

void HTMLHRElement::setNoShade(bool noShade)
{
    setAttribute(noshadeAttr, noShade ? "" : 0);
}

String HTMLHRElement::size() const
{
    return getAttribute(sizeAttr);
}

void HTMLHRElement::setSize(const String &value)
{
    setAttribute(sizeAttr, value);
}

String HTMLHRElement::width() const
{
    return getAttribute(widthAttr);
}

void HTMLHRElement::setWidth(const String &value)
{
    setAttribute(widthAttr, value);
}

}
