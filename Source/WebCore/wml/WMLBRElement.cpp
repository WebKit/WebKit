/**
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2003, 2006 Apple Computer, Inc.
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
#include "WMLBRElement.h"

#include "Attribute.h"
#include "CSSPropertyNames.h"
#include "HTMLNames.h"
#include "RenderBR.h"

namespace WebCore {

WMLBRElement::WMLBRElement(const QualifiedName& tagName, Document* doc)
    : WMLElement(tagName, doc)
{
}

PassRefPtr<WMLBRElement> WMLBRElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new WMLBRElement(tagName, document));
}

bool WMLBRElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == HTMLNames::clearAttr) {
        result = eUniversal;
        return false;
    }

    return WMLElement::mapToEntry(attrName, result);
}

void WMLBRElement::parseMappedAttribute(Attribute* attr)
{
    if (attr->name() == HTMLNames::clearAttr) {
        // If the string is empty, then don't add the clear property. 
        // <br clear> and <br clear=""> are just treated like <br> by Gecko, Mac IE, etc. -dwh
        const AtomicString& value = attr->value();
        if (value.isEmpty())
            return;

        if (equalIgnoringCase(value, "all"))
            addCSSProperty(attr, CSSPropertyClear, "both");
        else
            addCSSProperty(attr, CSSPropertyClear, value);
    } else
        WMLElement::parseMappedAttribute(attr);
}

RenderObject* WMLBRElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderBR(this);
}

}

#endif
