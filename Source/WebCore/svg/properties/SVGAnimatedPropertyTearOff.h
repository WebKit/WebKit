/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "SVGAnimatedProperty.h"
#include "SVGPropertyTearOff.h"

namespace WebCore {

template<typename T>
class SVGAnimatedPropertyTearOff final : public SVGAnimatedProperty {
public:
    using PropertyTearOff = T;
    using PropertyType = typename PropertyTearOff::PropertyType;
    using ContentType = PropertyType;

    static Ref<SVGAnimatedPropertyTearOff<PropertyTearOff>> create(SVGElement* contextElement, const QualifiedName& attributeName, AnimatedPropertyType animatedPropertyType, PropertyType& property)
    {
        ASSERT(contextElement);
        return adoptRef(*new SVGAnimatedPropertyTearOff<PropertyTearOff>(contextElement, attributeName, animatedPropertyType, property));
    }

    Ref<PropertyTearOff> baseVal()
    {
        if (m_baseVal)
            return *m_baseVal;

        auto property = PropertyTearOff::create(*this, BaseValRole, m_property);
        m_baseVal = property.ptr();
        return property;
    }

    Ref<PropertyTearOff> animVal()
    {
        if (m_animVal)
            return *m_animVal;

        auto property = PropertyTearOff::create(*this, AnimValRole, m_property);
        m_animVal = property.ptr();
        return property;
    }

    bool isAnimating() const final { return m_animatedProperty; }

    void propertyWillBeDeleted(const SVGProperty& property) final
    {
        if (&property == m_baseVal)
            m_baseVal = nullptr;
        else if (&property == m_animVal)
            m_animVal = nullptr;
    }

    PropertyType& currentAnimatedValue()
    {
        ASSERT(isAnimating());
        return m_animatedProperty->propertyReference();
    }

    const PropertyType& currentBaseValue() const
    {
        return m_property;
    }

    void animationStarted(PropertyType* newAnimVal)
    {
        ASSERT(!isAnimating());
        ASSERT(newAnimVal);
        m_animatedProperty = animVal();
        m_animatedProperty->setValue(*newAnimVal);
    }

    void animationEnded()
    {
        ASSERT(isAnimating());
        m_animatedProperty->setValue(m_property);
        m_animatedProperty = nullptr;
    }

    void animValWillChange()
    {
        // no-op for non list types.
        ASSERT(isAnimating());
    }

    void animValDidChange()
    {
        // no-op for non list types.
        ASSERT(isAnimating());
    }

    void synchronizeWrappersIfNeeded()
    {
        // no-op
    }

private:
    SVGAnimatedPropertyTearOff(SVGElement* contextElement, const QualifiedName& attributeName, AnimatedPropertyType animatedPropertyType, PropertyType& property)
        : SVGAnimatedProperty(contextElement, attributeName, animatedPropertyType)
        , m_property(property)
    {
    }

    PropertyType& m_property;
    PropertyTearOff* m_baseVal { nullptr };
    PropertyTearOff* m_animVal { nullptr };

    RefPtr<PropertyTearOff> m_animatedProperty;
};

}
