/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2010 Apple Inc. All rights reserved.
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
#include "HTMLTableColElement.h"

#include "CSSPropertyNames.h"
#include "ElementInlines.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTMLTableElement.h"
#include "RenderTableCol.h"
#include "Text.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLTableColElement);

const unsigned defaultSpan { 1 };
const unsigned minSpan { 1 };
const unsigned maxSpan { 1000 };

using namespace HTMLNames;

inline HTMLTableColElement::HTMLTableColElement(const QualifiedName& tagName, Document& document)
    : HTMLTablePartElement(tagName, document)
    , m_span(1)
{
}

Ref<HTMLTableColElement> HTMLTableColElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLTableColElement(tagName, document));
}

bool HTMLTableColElement::hasPresentationalHintsForAttribute(const QualifiedName& name) const
{
    if (name == widthAttr)
        return true;
    return HTMLTablePartElement::hasPresentationalHintsForAttribute(name);
}

void HTMLTableColElement::collectPresentationalHintsForAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    if (name == widthAttr)
        addHTMLMultiLengthToStyle(style, CSSPropertyWidth, value);
    else if (name == heightAttr)
        addHTMLMultiLengthToStyle(style, CSSPropertyHeight, value);
    else
        HTMLTablePartElement::collectPresentationalHintsForAttribute(name, value, style);
}

void HTMLTableColElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    HTMLTablePartElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);

    if (name == spanAttr) {
        m_span = clampHTMLNonNegativeIntegerToRange(newValue, minSpan, maxSpan, defaultSpan);
        if (CheckedPtr col = dynamicDowncast<RenderTableCol>(renderer()))
            col->updateFromElement();
    } else if (name == widthAttr) {
        if (!newValue.isEmpty()) {
            if (CheckedPtr col = dynamicDowncast<RenderTableCol>(renderer())) {
                int newWidth = parseHTMLInteger(newValue).value_or(0);
                if (newWidth != col->width())
                    col->setNeedsLayoutAndPrefWidthsRecalc();
            }
        }
    }
}

const MutableStyleProperties* HTMLTableColElement::additionalPresentationalHintStyle() const
{
    if (!hasTagName(colgroupTag))
        return nullptr;
    if (auto table = findParentTable())
        return table->additionalGroupStyle(false);
    return nullptr;
}

void HTMLTableColElement::setSpan(unsigned span)
{
    setUnsignedIntegralAttribute(spanAttr, limitToOnlyHTMLNonNegative(span, defaultSpan));
}

String HTMLTableColElement::width() const
{
    return attributeWithoutSynchronization(widthAttr);
}

}
