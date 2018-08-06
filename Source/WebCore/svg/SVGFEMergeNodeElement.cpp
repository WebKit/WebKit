/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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
#include "SVGFEMergeNodeElement.h"

#include "RenderSVGResource.h"
#include "SVGFilterElement.h"
#include "SVGFilterPrimitiveStandardAttributes.h"
#include "SVGNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGFEMergeNodeElement);

inline SVGFEMergeNodeElement::SVGFEMergeNodeElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document)
{
    ASSERT(hasTagName(SVGNames::feMergeNodeTag));
    registerAttributes();
}

Ref<SVGFEMergeNodeElement> SVGFEMergeNodeElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFEMergeNodeElement(tagName, document));
}

void SVGFEMergeNodeElement::registerAttributes()
{
    auto& registry = attributeRegistry();
    if (!registry.isEmpty())
        return;
    registry.registerAttribute<SVGNames::inAttr, &SVGFEMergeNodeElement::m_in1>();
}

void SVGFEMergeNodeElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == SVGNames::inAttr) {
        m_in1.setValue(value);
        return;
    }

    SVGElement::parseAttribute(name, value);
}

void SVGFEMergeNodeElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (attrName == SVGNames::inAttr) {
        InstanceInvalidationGuard guard(*this);
        invalidateFilterPrimitiveParent(this);
        return;
    }

    SVGElement::svgAttributeChanged(attrName);
}

}
