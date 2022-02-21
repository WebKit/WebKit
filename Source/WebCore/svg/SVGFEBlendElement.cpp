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
#include "SVGNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGFEBlendElement);

inline SVGFEBlendElement::SVGFEBlendElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document)
{
    ASSERT(hasTagName(SVGNames::feBlendTag));
    
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::modeAttr, BlendMode, &SVGFEBlendElement::m_mode>();
        PropertyRegistry::registerProperty<SVGNames::inAttr, &SVGFEBlendElement::m_in1>();
        PropertyRegistry::registerProperty<SVGNames::in2Attr, &SVGFEBlendElement::m_in2>();
    });
}

Ref<SVGFEBlendElement> SVGFEBlendElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFEBlendElement(tagName, document));
}
    
void SVGFEBlendElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == SVGNames::modeAttr) {
        BlendMode mode = BlendMode::Normal;
        if (parseBlendMode(value, mode))
            m_mode->setBaseValInternal<BlendMode>(mode);
        return;
    }

    if (name == SVGNames::inAttr) {
        m_in1->setBaseValInternal(value);
        return;
    }

    if (name == SVGNames::in2Attr) {
        m_in2->setBaseValInternal(value);
        return;
    }

    SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
}

bool SVGFEBlendElement::setFilterEffectAttribute(FilterEffect* effect, const QualifiedName& attrName)
{
    FEBlend* blend = static_cast<FEBlend*>(effect);
    if (attrName == SVGNames::modeAttr)
        return blend->setBlendMode(mode());

    ASSERT_NOT_REACHED();
    return false;
}

void SVGFEBlendElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (attrName == SVGNames::modeAttr) {
        InstanceInvalidationGuard guard(*this);
        primitiveAttributeChanged(attrName);
        return;
    }

    if (attrName == SVGNames::inAttr || attrName == SVGNames::in2Attr) {
        InstanceInvalidationGuard guard(*this);
        invalidate();
        return;
    }

    SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
}

RefPtr<FilterEffect> SVGFEBlendElement::filterEffect(const SVGFilterBuilder&, const FilterEffectVector&) const
{
    return FEBlend::create(mode());
}

} // namespace WebCore
