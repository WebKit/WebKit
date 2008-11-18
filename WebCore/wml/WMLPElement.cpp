/**
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved.
 *               http://www.torchmobile.com/
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#if ENABLE(WML)
#include "WMLPElement.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "HTMLNames.h"
#include "WMLNames.h"

namespace WebCore {

using namespace WMLNames;

WMLPElement::WMLPElement(const QualifiedName& tagName, Document* doc)
    : WMLElement(tagName, doc)
{
}

bool WMLPElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == HTMLNames::alignAttr) {
        result = eBlock; // We can share with DIV here.
        return false;
    }

    return WMLElement::mapToEntry(attrName, result);
}

void WMLPElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == HTMLNames::alignAttr) {
        String v = attr->value();
        if (equalIgnoringCase(attr->value(), "middle") || equalIgnoringCase(attr->value(), "center"))
            addCSSProperty(attr, CSSPropertyTextAlign, CSSValueWebkitCenter);
        else if (equalIgnoringCase(attr->value(), "left"))
            addCSSProperty(attr, CSSPropertyTextAlign, CSSValueWebkitLeft);
        else if (equalIgnoringCase(attr->value(), "right"))
            addCSSProperty(attr, CSSPropertyTextAlign, CSSValueWebkitRight);
        else
            addCSSProperty(attr, CSSPropertyTextAlign, v);
    } else
        WMLElement::parseMappedAttribute(attr);
}

}

#endif
