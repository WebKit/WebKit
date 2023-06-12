/*
 * Copyright (C) 2011 Leo Yang <leoyang@webkit.org>
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "SVGGlyphRefElement.h"

#include "NodeName.h"
#include "SVGElementTypeHelpers.h"
#include "SVGGlyphElement.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include "XLinkNames.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGGlyphRefElement);

inline SVGGlyphRefElement::SVGGlyphRefElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
    , SVGURIReference(this)
{
    ASSERT(hasTagName(SVGNames::glyphRefTag));
}

Ref<SVGGlyphRefElement> SVGGlyphRefElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGGlyphRefElement(tagName, document));
}

bool SVGGlyphRefElement::hasValidGlyphElement(String& glyphName) const
{
    // FIXME: We only support xlink:href so far.
    // https://bugs.webkit.org/show_bug.cgi?id=64787
    // No need to support glyphRef referencing another node inside a shadow tree.
    auto target = targetElementFromIRIString(getAttribute(SVGNames::hrefAttr, XLinkNames::hrefAttr), document());
    glyphName = target.identifier;
    return is<SVGGlyphElement>(target.element);
}

static float parseFloat(const AtomString& value)
{
    return parseNumber(value).value_or(0);
}

void SVGGlyphRefElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    // FIXME: Is the error handling in parseFloat correct for these attributes?
    switch (name.nodeName()) {
    case AttributeNames::xAttr:
        m_x = parseFloat(newValue);
        break;
    case AttributeNames::yAttr:
        m_y = parseFloat(newValue);
        break;
    case AttributeNames::dxAttr:
        m_dx = parseFloat(newValue);
        break;
    case AttributeNames::dyAttr:
        m_dy = parseFloat(newValue);
        break;
    default:
        break;
    }

    SVGURIReference::parseAttribute(name, newValue);
    SVGElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

void SVGGlyphRefElement::setX(float x)
{
    setAttribute(SVGNames::xAttr, AtomString::number(x));
}

void SVGGlyphRefElement::setY(float y)
{
    setAttribute(SVGNames::yAttr, AtomString::number(y));
}

void SVGGlyphRefElement::setDx(float dx)
{
    setAttribute(SVGNames::dxAttr, AtomString::number(dx));
}

void SVGGlyphRefElement::setDy(float dy)
{
    setAttribute(SVGNames::dyAttr, AtomString::number(dy));
}

}
