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
#include "ElementInlines.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "MutableStyleProperties.h"
#include "NodeName.h"
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

bool HTMLHRElement::hasPresentationalHintsForAttribute(const QualifiedName& name) const
{
    switch (name.nodeName()) {
    case AttributeNames::widthAttr:
    case AttributeNames::colorAttr:
    case AttributeNames::noshadeAttr:
    case AttributeNames::sizeAttr:
        return true;
    default:
        break;
    }
    return HTMLElement::hasPresentationalHintsForAttribute(name);
}

void HTMLHRElement::collectPresentationalHintsForAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    switch (name.nodeName()) {
    case AttributeNames::alignAttr:
        if (equalLettersIgnoringASCIICase(value, "left"_s)) {
            addPropertyToPresentationalHintStyle(style, CSSPropertyMarginLeft, 0, CSSUnitType::CSS_PX);
            addPropertyToPresentationalHintStyle(style, CSSPropertyMarginRight, CSSValueAuto);
        } else if (equalLettersIgnoringASCIICase(value, "right"_s)) {
            addPropertyToPresentationalHintStyle(style, CSSPropertyMarginLeft, CSSValueAuto);
            addPropertyToPresentationalHintStyle(style, CSSPropertyMarginRight, 0, CSSUnitType::CSS_PX);
        } else {
            addPropertyToPresentationalHintStyle(style, CSSPropertyMarginLeft, CSSValueAuto);
            addPropertyToPresentationalHintStyle(style, CSSPropertyMarginRight, CSSValueAuto);
        }
        break;
    case AttributeNames::widthAttr:
        if (auto valueInteger = parseHTMLInteger(value); valueInteger && !*valueInteger)
            addPropertyToPresentationalHintStyle(style, CSSPropertyWidth, 0, CSSUnitType::CSS_PX);
        else
            addHTMLLengthToStyle(style, CSSPropertyWidth, value);
        break;
    case AttributeNames::colorAttr:
        addPropertyToPresentationalHintStyle(style, CSSPropertyBorderStyle, CSSValueSolid);
        addHTMLColorToStyle(style, CSSPropertyBorderColor, value);
        addHTMLColorToStyle(style, CSSPropertyBackgroundColor, value);
        break;
    case AttributeNames::noshadeAttr:
        if (!hasAttributeWithoutSynchronization(colorAttr)) {
            addPropertyToPresentationalHintStyle(style, CSSPropertyBorderStyle, CSSValueSolid);
            auto darkGrayValue = CSSValuePool::singleton().createColorValue(Color::darkGray);
            style.setProperty(CSSPropertyBorderColor, darkGrayValue.ptr());
            style.setProperty(CSSPropertyBackgroundColor, WTFMove(darkGrayValue));
        }
        break;
    case AttributeNames::sizeAttr:
        if (int size = parseHTMLInteger(value).value_or(0); size > 1)
            addPropertyToPresentationalHintStyle(style, CSSPropertyHeight, size - 2, CSSUnitType::CSS_PX);
        else
            addPropertyToPresentationalHintStyle(style, CSSPropertyBorderBottomWidth, 0, CSSUnitType::CSS_PX);
        break;
    default:
        HTMLElement::collectPresentationalHintsForAttribute(name, value, style);
        break;
    }
}

bool HTMLHRElement::canContainRangeEndPoint() const
{
    return hasChildNodes() && HTMLElement::canContainRangeEndPoint();
}

}
