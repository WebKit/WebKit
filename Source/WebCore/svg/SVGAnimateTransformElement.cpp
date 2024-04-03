/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#include "SVGAnimateTransformElement.h"

#include "SVGNames.h"
#include "SVGTransformable.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGAnimateTransformElement);

inline SVGAnimateTransformElement::SVGAnimateTransformElement(const QualifiedName& tagName, Document& document)
    : SVGAnimateElementBase(tagName, document)
    , m_type(SVGTransformValue::SVG_TRANSFORM_TRANSLATE)
{
    ASSERT(hasTagName(SVGNames::animateTransformTag));
}

Ref<SVGAnimateTransformElement> SVGAnimateTransformElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGAnimateTransformElement(tagName, document));
}

bool SVGAnimateTransformElement::hasValidAttributeType() const
{
    if (!this->targetElement())
        return false;

    if (attributeType() == AttributeType::CSS)
        return false;

    return SVGAnimateElementBase::hasValidAttributeType();
}

void SVGAnimateTransformElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    if (name == SVGNames::typeAttr) {
        m_type = SVGTransformable::parseTransformType(newValue).value_or(SVGTransformValue::SVG_TRANSFORM_UNKNOWN);
        if (m_type == SVGTransformValue::SVG_TRANSFORM_MATRIX)
            m_type = SVGTransformValue::SVG_TRANSFORM_UNKNOWN;
    }

    SVGAnimateElementBase::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

String SVGAnimateTransformElement::animateRangeString(const String& string) const
{
    return SVGTransformValue::prefixForTransformType(m_type) + string + ')';
}

}
