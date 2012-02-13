/**
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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
 */

#include "config.h"
#include "HTMLTablePartElement.h"

#include "Attribute.h"
#include "CSSImageValue.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "Document.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTMLTableElement.h"

namespace WebCore {

using namespace HTMLNames;

bool HTMLTablePartElement::isPresentationAttribute(Attribute* attr) const
{
    if (attr->name() == bgcolorAttr || attr->name() == backgroundAttr || attr->name() == bordercolorAttr || attr->name() == valignAttr || attr->name() == alignAttr || attr->name() == heightAttr)
        return true;
    return HTMLElement::isPresentationAttribute(attr);
}

void HTMLTablePartElement::collectStyleForAttribute(Attribute* attr, StylePropertySet* style)
{
    if (attr->name() == bgcolorAttr)
        addHTMLColorToStyle(style, CSSPropertyBackgroundColor, attr->value());
    else if (attr->name() == backgroundAttr) {
        String url = stripLeadingAndTrailingHTMLSpaces(attr->value());
        if (!url.isEmpty())
            style->setProperty(CSSProperty(CSSPropertyBackgroundImage, CSSImageValue::create(document()->completeURL(url).string())));
    } else if (attr->name() == bordercolorAttr) {
        if (!attr->value().isEmpty()) {
            addHTMLColorToStyle(style, CSSPropertyBorderColor, attr->value());
            style->setProperty(CSSPropertyBorderTopStyle, CSSValueSolid);
            style->setProperty(CSSPropertyBorderBottomStyle, CSSValueSolid);
            style->setProperty(CSSPropertyBorderLeftStyle, CSSValueSolid);
            style->setProperty(CSSPropertyBorderRightStyle, CSSValueSolid);
        }
    } else if (attr->name() == valignAttr) {
        if (!attr->value().isEmpty())
            style->setProperty(CSSPropertyVerticalAlign, attr->value());
    } else if (attr->name() == alignAttr) {
        if (equalIgnoringCase(attr->value(), "middle") || equalIgnoringCase(attr->value(), "center"))
            style->setProperty(CSSPropertyTextAlign, CSSValueWebkitCenter);
        else if (equalIgnoringCase(attr->value(), "absmiddle"))
            style->setProperty(CSSPropertyTextAlign, CSSValueCenter);
        else if (equalIgnoringCase(attr->value(), "left"))
            style->setProperty(CSSPropertyTextAlign, CSSValueWebkitLeft);
        else if (equalIgnoringCase(attr->value(), "right"))
            style->setProperty(CSSPropertyTextAlign, CSSValueWebkitRight);
        else
            style->setProperty(CSSPropertyTextAlign, attr->value());
    } else if (attr->name() == heightAttr) {
        if (!attr->value().isEmpty())
            addHTMLLengthToStyle(style, CSSPropertyHeight, attr->value());
    } else
        HTMLElement::collectStyleForAttribute(attr, style);
}

HTMLTableElement* HTMLTablePartElement::findParentTable() const
{
    ContainerNode* parent = parentNode();
    while (parent && !parent->hasTagName(tableTag))
        parent = parent->parentNode();
    return static_cast<HTMLTableElement*>(parent);
}

}
