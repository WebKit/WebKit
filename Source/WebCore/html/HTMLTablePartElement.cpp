/**
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2020 Apple Inc. All rights reserved.
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

#include "CSSImageValue.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "Document.h"
#include "ElementAncestorIteratorInlines.h"
#include "HTMLNames.h"
#include "HTMLTableElement.h"
#include "MutableStyleProperties.h"
#include "NodeName.h"
#include "StyleProperties.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLTablePartElement);

using namespace HTMLNames;

bool HTMLTablePartElement::hasPresentationalHintsForAttribute(const QualifiedName& name) const
{
    switch (name.nodeName()) {
    case AttributeNames::bgcolorAttr:
    case AttributeNames::backgroundAttr:
    case AttributeNames::valignAttr:
    case AttributeNames::heightAttr:
        return true;
    default:
        break;
    }
    return HTMLElement::hasPresentationalHintsForAttribute(name);
}

void HTMLTablePartElement::collectPresentationalHintsForAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    switch (name.nodeName()) {
    case AttributeNames::bgcolorAttr:
        addHTMLColorToStyle(style, CSSPropertyBackgroundColor, value);
        break;
    case AttributeNames::backgroundAttr:
        if (!StringView(value).containsOnly<isASCIIWhitespace<UChar>>())
            style.setProperty(CSSProperty(CSSPropertyBackgroundImage, CSSImageValue::create(document().completeURL(value), LoadedFromOpaqueSource::No)));
        break;
    case AttributeNames::valignAttr:
        if (equalLettersIgnoringASCIICase(value, "top"_s))
            addPropertyToPresentationalHintStyle(style, CSSPropertyVerticalAlign, CSSValueTop);
        else if (equalLettersIgnoringASCIICase(value, "middle"_s))
            addPropertyToPresentationalHintStyle(style, CSSPropertyVerticalAlign, CSSValueMiddle);
        else if (equalLettersIgnoringASCIICase(value, "bottom"_s))
            addPropertyToPresentationalHintStyle(style, CSSPropertyVerticalAlign, CSSValueBottom);
        else if (equalLettersIgnoringASCIICase(value, "baseline"_s))
            addPropertyToPresentationalHintStyle(style, CSSPropertyVerticalAlign, CSSValueBaseline);
        else
            addPropertyToPresentationalHintStyle(style, CSSPropertyVerticalAlign, value);
        break;
    case AttributeNames::alignAttr:
        if (equalLettersIgnoringASCIICase(value, "middle"_s) || equalLettersIgnoringASCIICase(value, "center"_s))
            addPropertyToPresentationalHintStyle(style, CSSPropertyTextAlign, CSSValueWebkitCenter);
        else if (equalLettersIgnoringASCIICase(value, "absmiddle"_s))
            addPropertyToPresentationalHintStyle(style, CSSPropertyTextAlign, CSSValueCenter);
        else if (equalLettersIgnoringASCIICase(value, "left"_s))
            addPropertyToPresentationalHintStyle(style, CSSPropertyTextAlign, CSSValueWebkitLeft);
        else if (equalLettersIgnoringASCIICase(value, "right"_s))
            addPropertyToPresentationalHintStyle(style, CSSPropertyTextAlign, CSSValueWebkitRight);
        else
            addPropertyToPresentationalHintStyle(style, CSSPropertyTextAlign, value);
        break;
    case AttributeNames::heightAttr:
        if (!value.isEmpty())
            addHTMLLengthToStyle(style, CSSPropertyHeight, value);
        break;
    default:
        HTMLElement::collectPresentationalHintsForAttribute(name, value, style);
        break;
    }
}

RefPtr<const HTMLTableElement> HTMLTablePartElement::findParentTable() const
{
    return ancestorsOfType<HTMLTableElement>(*this).first();
}

}
