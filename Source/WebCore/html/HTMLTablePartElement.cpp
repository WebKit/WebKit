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

bool HTMLTablePartElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == bgcolorAttr || name == backgroundAttr || name == bordercolorAttr || name == valignAttr || name == alignAttr || name == heightAttr)
        return true;
    return HTMLElement::isPresentationAttribute(name);
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
            addPropertyToAttributeStyle(style, CSSPropertyBorderStyle, CSSValueSolid);
        }
    } else if (attr->name() == valignAttr) {
        if (equalIgnoringCase(attr->value(), "top"))
            addPropertyToAttributeStyle(style, CSSPropertyVerticalAlign, CSSValueTop);
        else if (equalIgnoringCase(attr->value(), "middle"))
            addPropertyToAttributeStyle(style, CSSPropertyVerticalAlign, CSSValueMiddle);
        else if (equalIgnoringCase(attr->value(), "bottom"))
            addPropertyToAttributeStyle(style, CSSPropertyVerticalAlign, CSSValueBottom);
        else if (equalIgnoringCase(attr->value(), "baseline"))
            addPropertyToAttributeStyle(style, CSSPropertyVerticalAlign, CSSValueBaseline);
        else
            addPropertyToAttributeStyle(style, CSSPropertyVerticalAlign, attr->value());
    } else if (attr->name() == alignAttr) {
        if (equalIgnoringCase(attr->value(), "middle") || equalIgnoringCase(attr->value(), "center"))
            addPropertyToAttributeStyle(style, CSSPropertyTextAlign, CSSValueWebkitCenter);
        else if (equalIgnoringCase(attr->value(), "absmiddle"))
            addPropertyToAttributeStyle(style, CSSPropertyTextAlign, CSSValueCenter);
        else if (equalIgnoringCase(attr->value(), "left"))
            addPropertyToAttributeStyle(style, CSSPropertyTextAlign, CSSValueWebkitLeft);
        else if (equalIgnoringCase(attr->value(), "right"))
            addPropertyToAttributeStyle(style, CSSPropertyTextAlign, CSSValueWebkitRight);
        else
            addPropertyToAttributeStyle(style, CSSPropertyTextAlign, attr->value());
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
