/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2010 Apple Inc. All rights reserved.
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

#include "Attribute.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "HTMLNames.h"

namespace WebCore {

using namespace HTMLNames;

HTMLHRElement::HTMLHRElement(const QualifiedName& tagName, Document* document)
    : HTMLElement(tagName, document)
{
    ASSERT(hasTagName(hrTag));
}

PassRefPtr<HTMLHRElement> HTMLHRElement::create(Document* document)
{
    return adoptRef(new HTMLHRElement(hrTag, document));
}

PassRefPtr<HTMLHRElement> HTMLHRElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new HTMLHRElement(tagName, document));
}

void HTMLHRElement::parseMappedAttribute(Attribute* attr)
{
    if (attr->name() == alignAttr) {
        if (attr->value().isNull()) {
            removeCSSProperties(CSSPropertyMarginLeft, CSSPropertyMarginRight);
        } else if (equalIgnoringCase(attr->value(), "left")) {
            addCSSProperty(CSSPropertyMarginLeft, "0");
            addCSSProperty(CSSPropertyMarginRight, CSSValueAuto);
        } else if (equalIgnoringCase(attr->value(), "right")) {
            addCSSProperty(CSSPropertyMarginLeft, CSSValueAuto);
            addCSSProperty(CSSPropertyMarginRight, "0");
        } else {
            addCSSProperty(CSSPropertyMarginLeft, CSSValueAuto);
            addCSSProperty(CSSPropertyMarginRight, CSSValueAuto);
        }
    } else if (attr->name() == widthAttr) {
        if (attr->value().isNull())
            removeCSSProperty(CSSPropertyWidth);
        else {
            bool ok;
            int v = attr->value().toInt(&ok);
            if (ok && !v)
                addCSSLength(CSSPropertyWidth, "1");
            else
                addCSSLength(CSSPropertyWidth, attr->value());
        }
    } else if (attr->name() == colorAttr) {
        if (attr->value().isNull())
            removeCSSProperties(CSSPropertyBorderTopStyle, CSSPropertyBorderRightStyle, CSSPropertyBorderBottomStyle, CSSPropertyBorderLeftStyle, CSSPropertyBorderColor, CSSPropertyBackgroundColor);
        else {
            addCSSProperty(CSSPropertyBorderTopStyle, CSSValueSolid);
            addCSSProperty(CSSPropertyBorderRightStyle, CSSValueSolid);
            addCSSProperty(CSSPropertyBorderBottomStyle, CSSValueSolid);
            addCSSProperty(CSSPropertyBorderLeftStyle, CSSValueSolid);
            addCSSColor(CSSPropertyBorderColor, attr->value());
            addCSSColor(CSSPropertyBackgroundColor, attr->value());
        }
    } else if (attr->name() == noshadeAttr) {
        if (attr->value().isNull())
            removeCSSProperties(CSSPropertyBorderTopStyle, CSSPropertyBorderRightStyle, CSSPropertyBorderBottomStyle, CSSPropertyBorderLeftStyle, CSSPropertyBorderColor, CSSPropertyBackgroundColor);
        else {
            addCSSProperty(CSSPropertyBorderTopStyle, CSSValueSolid);
            addCSSProperty(CSSPropertyBorderRightStyle, CSSValueSolid);
            addCSSProperty(CSSPropertyBorderBottomStyle, CSSValueSolid);
            addCSSProperty(CSSPropertyBorderLeftStyle, CSSValueSolid);
            addCSSColor(CSSPropertyBorderColor, String("grey"));
            addCSSColor(CSSPropertyBackgroundColor, String("grey"));
        }
    } else if (attr->name() == sizeAttr) {
        if (attr->value().isNull())
            removeCSSProperties(CSSPropertyBorderBottomWidth, CSSPropertyHeight);
        else {
            StringImpl* si = attr->value().impl();
            int size = si->toInt();
            if (size <= 1) {
                addCSSProperty(CSSPropertyBorderBottomWidth, String("0"));
                removeCSSProperty(CSSPropertyHeight);
            } else {
                addCSSLength(CSSPropertyHeight, String::number(size-2));
                removeCSSProperty(CSSPropertyBorderBottomWidth);
            }
        }
    } else
        HTMLElement::parseMappedAttribute(attr);
}

}
