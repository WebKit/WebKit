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
#include "HTMLPreElement.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "HTMLNames.h"
#include "MutableStyleProperties.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLPreElement);

using namespace HTMLNames;

inline HTMLPreElement::HTMLPreElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
}

Ref<HTMLPreElement> HTMLPreElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLPreElement(tagName, document));
}

bool HTMLPreElement::hasPresentationalHintsForAttribute(const QualifiedName& name) const
{
    if (name == wrapAttr)
        return true;
    return HTMLElement::hasPresentationalHintsForAttribute(name);
}

void HTMLPreElement::collectPresentationalHintsForAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    if (name == wrapAttr) {
        style.setProperty(CSSPropertyWhiteSpaceCollapse, CSSValuePreserve);
        style.setProperty(CSSPropertyTextWrapMode, CSSValueWrap);
    } else
        HTMLElement::collectPresentationalHintsForAttribute(name, value, style);
}

}
