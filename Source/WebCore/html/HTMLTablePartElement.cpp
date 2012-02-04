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
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "Document.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"

namespace WebCore {

using namespace HTMLNames;

void HTMLTablePartElement::parseMappedAttribute(Attribute* attr)
{
    if (attr->name() == bgcolorAttr)
        if (attr->value().isNull())
            removeCSSProperty(CSSPropertyBackgroundColor);
        else
            addCSSColor(CSSPropertyBackgroundColor, attr->value());
    else if (attr->name() == backgroundAttr) {
        String url = stripLeadingAndTrailingHTMLSpaces(attr->value());
        if (!url.isEmpty())
            addCSSImageProperty(CSSPropertyBackgroundImage, document()->completeURL(url).string());
        else
            removeCSSProperty(CSSPropertyBackgroundImage);
    } else if (attr->name() == bordercolorAttr) {
        if (!attr->value().isEmpty()) {
            addCSSColor(CSSPropertyBorderColor, attr->value());
            addCSSProperty(CSSPropertyBorderTopStyle, CSSValueSolid);
            addCSSProperty(CSSPropertyBorderBottomStyle, CSSValueSolid);
            addCSSProperty(CSSPropertyBorderLeftStyle, CSSValueSolid);
            addCSSProperty(CSSPropertyBorderRightStyle, CSSValueSolid);
        } else
            removeCSSProperties(CSSPropertyBorderColor, CSSPropertyBorderTopStyle, CSSPropertyBorderBottomStyle, CSSPropertyBorderLeftStyle, CSSPropertyBorderRightStyle);
    } else if (attr->name() == valignAttr) {
        if (!attr->value().isEmpty())
            addCSSProperty(CSSPropertyVerticalAlign, attr->value());
        else
            removeCSSProperty(CSSPropertyVerticalAlign);
    } else if (attr->name() == alignAttr) {
        const AtomicString& v = attr->value();
        if (v.isNull())
            removeCSSProperty(CSSPropertyTextAlign);
        else if (equalIgnoringCase(v, "middle") || equalIgnoringCase(v, "center"))
            addCSSProperty(CSSPropertyTextAlign, CSSValueWebkitCenter);
        else if (equalIgnoringCase(v, "absmiddle"))
            addCSSProperty(CSSPropertyTextAlign, CSSValueCenter);
        else if (equalIgnoringCase(v, "left"))
            addCSSProperty(CSSPropertyTextAlign, CSSValueWebkitLeft);
        else if (equalIgnoringCase(v, "right"))
            addCSSProperty(CSSPropertyTextAlign, CSSValueWebkitRight);
        else
            addCSSProperty(CSSPropertyTextAlign, v);
    } else if (attr->name() == heightAttr) {
        if (!attr->value().isEmpty())
            addCSSLength(CSSPropertyHeight, attr->value());
        else
            removeCSSProperty(CSSPropertyHeight);
    } else
        HTMLElement::parseMappedAttribute(attr);
}

}
