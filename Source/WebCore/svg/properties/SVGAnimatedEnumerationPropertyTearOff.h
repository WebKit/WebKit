/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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

#pragma once

#include "SVGAnimatedStaticPropertyTearOff.h"
#include "SVGPropertyTraits.h"

namespace WebCore {

template<typename EnumType>
class SVGAnimatedEnumerationPropertyTearOff final : public SVGAnimatedStaticPropertyTearOff<unsigned> {
public:
    using ContentType = EnumType;

    const unsigned& baseVal() final
    {
        const unsigned& baseVal = SVGAnimatedStaticPropertyTearOff::baseVal();

        if (baseVal > SVGIDLEnumLimits<EnumType>::highestExposedEnumValue())
            return m_outOfRangeEnumValue;

        return baseVal;
    }

    const unsigned& animVal() final
    {
        const unsigned& animVal = SVGAnimatedStaticPropertyTearOff::animVal();

        if (animVal > SVGIDLEnumLimits<EnumType>::highestExposedEnumValue())
            return m_outOfRangeEnumValue;

        return animVal;
    }

    ExceptionOr<void> setBaseVal(const unsigned& property) final
    {
        // All SVG enumeration values, that are allowed to be set via SVG DOM start with 1, 0 corresponds to unknown and is not settable through SVG DOM.
        if (!property || property > SVGIDLEnumLimits<EnumType>::highestExposedEnumValue())
            return Exception { TypeError };
        return SVGAnimatedStaticPropertyTearOff<unsigned>::setBaseVal(property);
    }

    static Ref<SVGAnimatedEnumerationPropertyTearOff<EnumType>> create(SVGElement* contextElement, const QualifiedName& attributeName, AnimatedPropertyType animatedPropertyType, EnumType& property)
    {
        ASSERT(contextElement);
        return adoptRef(*new SVGAnimatedEnumerationPropertyTearOff<EnumType>(contextElement, attributeName, animatedPropertyType, reinterpret_cast<unsigned&>(property)));
    }

    EnumType& currentAnimatedValue()
    {
        unsigned& animatedValue = SVGAnimatedStaticPropertyTearOff<unsigned>::currentAnimatedValue();
        ASSERT(animatedValue <= SVGPropertyTraits<EnumType>::highestEnumValue());
        return reinterpret_cast<EnumType&>(animatedValue);
    }

private:
    SVGAnimatedEnumerationPropertyTearOff(SVGElement* contextElement, const QualifiedName& attributeName, AnimatedPropertyType animatedPropertyType, unsigned& property)
        : SVGAnimatedStaticPropertyTearOff<unsigned>(contextElement, attributeName, animatedPropertyType, property)
    {
    }

    static unsigned m_outOfRangeEnumValue;
};

// By convention, all enum values that represent UNKNOWN in SVG are equal to zero.
template<typename EnumType>
unsigned SVGAnimatedEnumerationPropertyTearOff<EnumType>::m_outOfRangeEnumValue = 0;

} // namespace WebCore
