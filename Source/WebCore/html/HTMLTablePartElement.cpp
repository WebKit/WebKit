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
#include "ElementAncestorIterator.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTMLTableElement.h"
#include "MutableStyleProperties.h"
#include "StyleProperties.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLTablePartElement);

using namespace HTMLNames;

bool HTMLTablePartElement::hasPresentationalHintsForAttribute(const QualifiedName& name) const
{
    if (name == bgcolorAttr || name == backgroundAttr || name == valignAttr || name == heightAttr)
        return true;
    return HTMLElement::hasPresentationalHintsForAttribute(name);
}

void HTMLTablePartElement::collectPresentationalHintsForAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    if (name == bgcolorAttr)
        addHTMLColorToStyle(style, CSSPropertyBackgroundColor, value);
    else if (name == backgroundAttr) {
        String url = stripLeadingAndTrailingHTMLSpaces(value);
        if (!url.isEmpty())
            style.setProperty(CSSProperty(CSSPropertyBackgroundImage, CSSImageValue::create(document().completeURL(url), LoadedFromOpaqueSource::No)));
    } else if (name == valignAttr) {
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
    } else if (name == alignAttr) {
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
    } else if (name == heightAttr) {
        if (!value.isEmpty())
            addHTMLLengthToStyle(style, CSSPropertyHeight, value);
    } else
        HTMLElement::collectPresentationalHintsForAttribute(name, value, style);
}

RefPtr<const HTMLTableElement> HTMLTablePartElement::findParentTable() const
{
    return ancestorsOfType<HTMLTableElement>(*this).first();
}

}
