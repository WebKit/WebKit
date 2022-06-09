/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2003, 2006, 2009, 2010 Apple Inc. All rights reserved.
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
#include "HTMLBRElement.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "HTMLNames.h"
#include "RenderLineBreak.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLBRElement);

using namespace HTMLNames;

HTMLBRElement::HTMLBRElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
    ASSERT(hasTagName(brTag));
}

Ref<HTMLBRElement> HTMLBRElement::create(Document& document)
{
    return adoptRef(*new HTMLBRElement(brTag, document));
}

Ref<HTMLBRElement> HTMLBRElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLBRElement(tagName, document));
}

bool HTMLBRElement::hasPresentationalHintsForAttribute(const QualifiedName& name) const
{
    if (name == clearAttr)
        return true;
    return HTMLElement::hasPresentationalHintsForAttribute(name);
}

void HTMLBRElement::collectPresentationalHintsForAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    if (name == clearAttr) {
        // If the string is empty, then don't add the clear property.
        // <br clear> and <br clear=""> are just treated like <br> by Gecko, Mac IE, etc. -dwh
        if (!value.isEmpty()) {
            if (equalLettersIgnoringASCIICase(value, "all"))
                addPropertyToPresentationalHintStyle(style, CSSPropertyClear, CSSValueBoth);
            else
                addPropertyToPresentationalHintStyle(style, CSSPropertyClear, value);
        }
    } else
        HTMLElement::collectPresentationalHintsForAttribute(name, value, style);
}

RenderPtr<RenderElement> HTMLBRElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    if (style.hasContent() && RenderElement::isContentDataSupported(*style.contentData()))
        return RenderElement::createFor(*this, WTFMove(style));

    return createRenderer<RenderLineBreak>(*this, WTFMove(style));
}

}
