/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

#include "LegacyRenderSVGResource.h"
#include "SVGFilterElement.h"
#include "SVGFilterPrimitiveStandardAttributes.h"
#include "SVGNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGFEMergeNodeElement);

inline SVGFEMergeNodeElement::SVGFEMergeNodeElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
{
    ASSERT(hasTagName(SVGNames::feMergeNodeTag));
    
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::inAttr, &SVGFEMergeNodeElement::m_in1>();
    });
}

Ref<SVGFEMergeNodeElement> SVGFEMergeNodeElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFEMergeNodeElement(tagName, document));
}

void SVGFEMergeNodeElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    if (name == SVGNames::inAttr)
        m_in1->setBaseValInternal(newValue);

    SVGElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

void SVGFEMergeNodeElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
        ASSERT(attrName == SVGNames::inAttr);
        InstanceInvalidationGuard guard(*this);
        SVGFilterPrimitiveStandardAttributes::invalidateFilterPrimitiveParent(this);
        return;
    }

    SVGElement::svgAttributeChanged(attrName);
}

}
