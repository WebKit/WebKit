/**
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "HTMLHRElement.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "HTMLNames.h"
#include "MappedAttribute.h"

namespace WebCore {

using namespace HTMLNames;

HTMLHRElement::HTMLHRElement(const QualifiedName& tagName, Document* doc)
    : HTMLElement(tagName, doc)
{
    ASSERT(hasTagName(hrTag));
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
            addCSSProperty(attr, CSSPropertyMarginLeft, "0");
            addCSSProperty(attr, CSSPropertyMarginRight, CSSValueAuto);
        } else if (equalIgnoringCase(attr->value(), "right")) {
            addCSSProperty(attr, CSSPropertyMarginLeft, CSSValueAuto);
            addCSSProperty(attr, CSSPropertyMarginRight, "0");
        } else {
            addCSSProperty(attr, CSSPropertyMarginLeft, CSSValueAuto);
            addCSSProperty(attr, CSSPropertyMarginRight, CSSValueAuto);
        }
    } else if (attr->name() == widthAttr) {
        bool ok;
        int v = attr->value().toInt(&ok);
        if(ok && !v)
            addCSSLength(attr, CSSPropertyWidth, "1");
        else
            addCSSLength(attr, CSSPropertyWidth, attr->value());
    } else if (attr->name() == colorAttr) {
        addCSSProperty(attr, CSSPropertyBorderTopStyle, CSSValueSolid);
        addCSSProperty(attr, CSSPropertyBorderRightStyle, CSSValueSolid);
        addCSSProperty(attr, CSSPropertyBorderBottomStyle, CSSValueSolid);
        addCSSProperty(attr, CSSPropertyBorderLeftStyle, CSSValueSolid);
        addCSSColor(attr, CSSPropertyBorderColor, attr->value());
        addCSSColor(attr, CSSPropertyBackgroundColor, attr->value());
    } else if (attr->name() == noshadeAttr) {
        addCSSProperty(attr, CSSPropertyBorderTopStyle, CSSValueSolid);
        addCSSProperty(attr, CSSPropertyBorderRightStyle, CSSValueSolid);
        addCSSProperty(attr, CSSPropertyBorderBottomStyle, CSSValueSolid);
        addCSSProperty(attr, CSSPropertyBorderLeftStyle, CSSValueSolid);
        addCSSColor(attr, CSSPropertyBorderColor, String("grey"));
        addCSSColor(attr, CSSPropertyBackgroundColor, String("grey"));
    } else if (attr->name() == sizeAttr) {
        StringImpl* si = attr->value().impl();
        int size = si->toInt();
        if (size <= 1)
            addCSSProperty(attr, CSSPropertyBorderBottomWidth, String("0"));
        else
            addCSSLength(attr, CSSPropertyHeight, String::number(size-2));
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
