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
#include "CSSValuePool.h"
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

bool HTMLHRElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == alignAttr || name == widthAttr || name == colorAttr || name == noshadeAttr || name == sizeAttr)
        return true;
    return HTMLElement::isPresentationAttribute(name);
}

void HTMLHRElement::collectStyleForAttribute(Attribute* attr, StylePropertySet* style)
{
    if (attr->name() == alignAttr) {
        if (equalIgnoringCase(attr->value(), "left")) {
            addPropertyToAttributeStyle(style, CSSPropertyMarginLeft, 0, CSSPrimitiveValue::CSS_PX);
            addPropertyToAttributeStyle(style, CSSPropertyMarginRight, CSSValueAuto);
        } else if (equalIgnoringCase(attr->value(), "right")) {
            addPropertyToAttributeStyle(style, CSSPropertyMarginLeft, CSSValueAuto);
            addPropertyToAttributeStyle(style, CSSPropertyMarginRight, 0, CSSPrimitiveValue::CSS_PX);
        } else {
            addPropertyToAttributeStyle(style, CSSPropertyMarginLeft, CSSValueAuto);
            addPropertyToAttributeStyle(style, CSSPropertyMarginRight, CSSValueAuto);
        }
    } else if (attr->name() == widthAttr) {
        bool ok;
        int v = attr->value().toInt(&ok);
        if (ok && !v)
            addPropertyToAttributeStyle(style, CSSPropertyWidth, 1, CSSPrimitiveValue::CSS_PX);
        else
            addHTMLLengthToStyle(style, CSSPropertyWidth, attr->value());
    } else if (attr->name() == colorAttr) {
        addPropertyToAttributeStyle(style, CSSPropertyBorderStyle, CSSValueSolid);
        addHTMLColorToStyle(style, CSSPropertyBorderColor, attr->value());
        addHTMLColorToStyle(style, CSSPropertyBackgroundColor, attr->value());
    } else if (attr->name() == noshadeAttr) {
        addPropertyToAttributeStyle(style, CSSPropertyBorderStyle, CSSValueSolid);

        RefPtr<CSSPrimitiveValue> darkGrayValue = cssValuePool().createColorValue(Color::darkGray);
        style->setProperty(CSSPropertyBorderColor, darkGrayValue);
        style->setProperty(CSSPropertyBackgroundColor, darkGrayValue);
    } else if (attr->name() == sizeAttr) {
        StringImpl* si = attr->value().impl();
        int size = si->toInt();
        if (size <= 1)
            addPropertyToAttributeStyle(style, CSSPropertyBorderBottomWidth, 0, CSSPrimitiveValue::CSS_PX);
        else
            addPropertyToAttributeStyle(style, CSSPropertyHeight, size - 2, CSSPrimitiveValue::CSS_PX);
    } else
        HTMLElement::collectStyleForAttribute(attr, style);
}

}
