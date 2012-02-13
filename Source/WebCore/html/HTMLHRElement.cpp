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

bool HTMLHRElement::isPresentationAttribute(Attribute* attr) const
{
    if (attr->name() == alignAttr || attr->name() == widthAttr || attr->name() == colorAttr || attr->name() == noshadeAttr || attr->name() == sizeAttr)
        return true;
    return HTMLElement::isPresentationAttribute(attr);
}

void HTMLHRElement::collectStyleForAttribute(Attribute* attr, StylePropertySet* style)
{
    if (attr->name() == alignAttr) {
        if (equalIgnoringCase(attr->value(), "left")) {
            style->setProperty(CSSPropertyMarginLeft, "0"); // FIXME: Pass as integer.
            style->setProperty(CSSPropertyMarginRight, CSSValueAuto);
        } else if (equalIgnoringCase(attr->value(), "right")) {
            style->setProperty(CSSPropertyMarginLeft, CSSValueAuto);
            style->setProperty(CSSPropertyMarginRight, "0"); // FIXME: Pass as integer.
        } else {
            style->setProperty(CSSPropertyMarginLeft, CSSValueAuto);
            style->setProperty(CSSPropertyMarginRight, CSSValueAuto);
        }
    } else if (attr->name() == widthAttr) {
        bool ok;
        int v = attr->value().toInt(&ok);
        if (ok && !v)
            addHTMLLengthToStyle(style, CSSPropertyWidth, "1"); // FIXME: Pass as integer.
        else
            addHTMLLengthToStyle(style, CSSPropertyWidth, attr->value());
    } else if (attr->name() == colorAttr) {
        style->setProperty(CSSPropertyBorderTopStyle, CSSValueSolid);
        style->setProperty(CSSPropertyBorderRightStyle, CSSValueSolid);
        style->setProperty(CSSPropertyBorderBottomStyle, CSSValueSolid);
        style->setProperty(CSSPropertyBorderLeftStyle, CSSValueSolid);
        addHTMLColorToStyle(style, CSSPropertyBorderColor, attr->value());
        addHTMLColorToStyle(style, CSSPropertyBackgroundColor, attr->value());
    } else if (attr->name() == noshadeAttr) {
        style->setProperty(CSSPropertyBorderTopStyle, CSSValueSolid);
        style->setProperty(CSSPropertyBorderRightStyle, CSSValueSolid);
        style->setProperty(CSSPropertyBorderBottomStyle, CSSValueSolid);
        style->setProperty(CSSPropertyBorderLeftStyle, CSSValueSolid);
        addHTMLColorToStyle(style, CSSPropertyBorderColor, String("grey")); // FIXME: Pass as rgb() value.
        addHTMLColorToStyle(style, CSSPropertyBackgroundColor, String("grey")); // FIXME: Pass as rgb() value.
    } else if (attr->name() == sizeAttr) {
        StringImpl* si = attr->value().impl();
        int size = si->toInt();
        if (size <= 1)
            style->setProperty(CSSPropertyBorderBottomWidth, String("0")); // FIXME: Pass as integer.
        else
            addHTMLLengthToStyle(style, CSSPropertyHeight, String::number(size - 2)); // FIXME: Pass as integer.
    } else
        HTMLElement::collectStyleForAttribute(attr, style);
}

}
