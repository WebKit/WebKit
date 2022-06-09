/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2018 Apple Inc. All rights reserved.
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
#include "HTMLDivElement.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "HTMLNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLDivElement);

using namespace HTMLNames;

HTMLDivElement::HTMLDivElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
    ASSERT(hasTagName(divTag));
}

Ref<HTMLDivElement> HTMLDivElement::create(Document& document)
{
    return adoptRef(*new HTMLDivElement(divTag, document));
}

Ref<HTMLDivElement> HTMLDivElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLDivElement(tagName, document));
}

void HTMLDivElement::collectPresentationalHintsForAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    if (name == alignAttr) {
        if (equalLettersIgnoringASCIICase(value, "middle") || equalLettersIgnoringASCIICase(value, "center"))
            addPropertyToPresentationalHintStyle(style, CSSPropertyTextAlign, CSSValueWebkitCenter);
        else if (equalLettersIgnoringASCIICase(value, "left"))
            addPropertyToPresentationalHintStyle(style, CSSPropertyTextAlign, CSSValueWebkitLeft);
        else if (equalLettersIgnoringASCIICase(value, "right"))
            addPropertyToPresentationalHintStyle(style, CSSPropertyTextAlign, CSSValueWebkitRight);
        else
            addPropertyToPresentationalHintStyle(style, CSSPropertyTextAlign, value);
    } else
        HTMLElement::collectPresentationalHintsForAttribute(name, value, style);
}

}
