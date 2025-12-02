/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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
#include "SVGFEBlendElement.h"

#include "FEBlend.h"
#include "NodeName.h"
#include "SVGNames.h"
#include "SVGPropertyOwnerRegistry.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(SVGFEBlendElement);

inline SVGFEBlendElement::SVGFEBlendElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
{
    ASSERT(hasTagName(SVGNames::feBlendTag));
    
    static bool didRegistration = false;
    if (!didRegistration) [[unlikely]] {
        didRegistration = true;
        PropertyRegistry::registerProperty<SVGNames::modeAttr, BlendMode, &SVGFEBlendElement::m_mode>();
        PropertyRegistry::registerProperty<SVGNames::inAttr, &SVGFEBlendElement::m_in1>();
        PropertyRegistry::registerProperty<SVGNames::in2Attr, &SVGFEBlendElement::m_in2>();
    }
}

Ref<SVGFEBlendElement> SVGFEBlendElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFEBlendElement(tagName, document));
}

void SVGFEBlendElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    switch (name.nodeName()) {
    case AttributeNames::modeAttr: {
        BlendMode mode = BlendMode::Normal;
        if (parseBlendMode(newValue, mode))
        Ref { m_mode }->setBaseValInternal<BlendMode>(mode);
        break;
    }
    case AttributeNames::inAttr:
        Ref { m_in1 }->setBaseValInternal(newValue);
        break;
    case AttributeNames::in2Attr:
        Ref { m_in2 }->setBaseValInternal(newValue);
        break;
    default:
        break;
    }

    SVGFilterPrimitiveStandardAttributes::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

bool SVGFEBlendElement::setFilterEffectAttribute(FilterEffect& effect, const QualifiedName& attrName)
{
    auto& feBlend = downcast<FEBlend>(effect);
    if (attrName == SVGNames::modeAttr)
        return feBlend.setBlendMode(mode());

    ASSERT_NOT_REACHED();
    return false;
}

void SVGFEBlendElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        if (attrName == SVGNames::modeAttr)
            primitiveAttributeChanged(attrName);
        else {
            ASSERT(attrName == SVGNames::inAttr || attrName == SVGNames::in2Attr);
            updateSVGRendererForElementChange();
        }
        return;
    }

    SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
}

RefPtr<FilterEffect> SVGFEBlendElement::createFilterEffect(const FilterEffectVector&, const GraphicsContext&) const
{
    return FEBlend::create(mode());
}

} // namespace WebCore
