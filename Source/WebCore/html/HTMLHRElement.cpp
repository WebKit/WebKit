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
#include "HTMLHRElement.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "CSSValuePool.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "StyleProperties.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLHRElement);

using namespace HTMLNames;

HTMLHRElement::HTMLHRElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
    ASSERT(hasTagName(hrTag));
}

Ref<HTMLHRElement> HTMLHRElement::create(Document& document)
{
    return adoptRef(*new HTMLHRElement(hrTag, document));
}

Ref<HTMLHRElement> HTMLHRElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLHRElement(tagName, document));
}

bool HTMLHRElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == widthAttr || name == colorAttr || name == noshadeAttr || name == sizeAttr)
        return true;
    return HTMLElement::isPresentationAttribute(name);
}

void HTMLHRElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    if (name == alignAttr) {
        if (equalLettersIgnoringASCIICase(value, "left")) {
            addPropertyToPresentationAttributeStyle(style, CSSPropertyMarginLeft, 0, CSSUnitType::CSS_PX);
            addPropertyToPresentationAttributeStyle(style, CSSPropertyMarginRight, CSSValueAuto);
        } else if (equalLettersIgnoringASCIICase(value, "right")) {
            addPropertyToPresentationAttributeStyle(style, CSSPropertyMarginLeft, CSSValueAuto);
            addPropertyToPresentationAttributeStyle(style, CSSPropertyMarginRight, 0, CSSUnitType::CSS_PX);
        } else {
            addPropertyToPresentationAttributeStyle(style, CSSPropertyMarginLeft, CSSValueAuto);
            addPropertyToPresentationAttributeStyle(style, CSSPropertyMarginRight, CSSValueAuto);
        }
    } else if (name == widthAttr) {
        if (auto valueInteger = parseHTMLInteger(value); valueInteger && !*valueInteger)
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWidth, 1, CSSUnitType::CSS_PX);
        else
            addHTMLLengthToStyle(style, CSSPropertyWidth, value);
    } else if (name == colorAttr) {
        addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderStyle, CSSValueSolid);
        addHTMLColorToStyle(style, CSSPropertyBorderColor, value);
        addHTMLColorToStyle(style, CSSPropertyBackgroundColor, value);
    } else if (name == noshadeAttr) {
        if (!hasAttributeWithoutSynchronization(colorAttr)) {
            addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderStyle, CSSValueSolid);

            RefPtr<CSSPrimitiveValue> darkGrayValue = CSSValuePool::singleton().createColorValue(Color::darkGray);
            style.setProperty(CSSPropertyBorderColor, darkGrayValue);
            style.setProperty(CSSPropertyBackgroundColor, darkGrayValue);
        }
    } else if (name == sizeAttr) {
        int size = parseHTMLInteger(value).value_or(0);
        if (size <= 1)
            addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderBottomWidth, 0, CSSUnitType::CSS_PX);
        else
            addPropertyToPresentationAttributeStyle(style, CSSPropertyHeight, size - 2, CSSUnitType::CSS_PX);
    } else
        HTMLElement::collectStyleForPresentationAttribute(name, value, style);
}

bool HTMLHRElement::canContainRangeEndPoint() const
{
    return hasChildNodes() && HTMLElement::canContainRangeEndPoint();
}

}
