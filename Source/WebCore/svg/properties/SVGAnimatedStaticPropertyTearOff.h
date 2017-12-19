/*
 * Copyright (C) Research In Motion Limited 2010-2012. All rights reserved.
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

#include "ExceptionOr.h"
#include "SVGAnimatedProperty.h"

namespace WebCore {

template<typename PropertyType>
class SVGAnimatedStaticPropertyTearOff : public SVGAnimatedProperty {
public:
    typedef PropertyType ContentType;

    virtual const PropertyType& baseVal()
    {
        return m_property;
    }

    virtual const PropertyType& animVal()
    {
        if (m_animatedProperty)
            return *m_animatedProperty;
        return m_property;
    }

    virtual ExceptionOr<void> setBaseVal(const PropertyType& property)
    {
        m_property = property;
        commitChange();
        return { };
    }

    bool isAnimating() const override { return m_animatedProperty; }

    static Ref<SVGAnimatedStaticPropertyTearOff<PropertyType>> create(SVGElement* contextElement, const QualifiedName& attributeName, AnimatedPropertyType animatedPropertyType, PropertyType& property)
    {
        ASSERT(contextElement);
        return adoptRef(*new SVGAnimatedStaticPropertyTearOff<PropertyType>(contextElement, attributeName, animatedPropertyType, property));
    }

    PropertyType& currentAnimatedValue()
    {
        ASSERT(isAnimating());
        return *m_animatedProperty;
    }

    const PropertyType& currentBaseValue() const
    {
        return m_property;
    }

    void animationStarted(PropertyType* newAnimVal)
    {
        ASSERT(!isAnimating());
        ASSERT(newAnimVal);
        m_animatedProperty = newAnimVal;
    }

    void animationEnded()
    {
        ASSERT(isAnimating());
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

protected:
    SVGAnimatedStaticPropertyTearOff(SVGElement* contextElement, const QualifiedName& attributeName, AnimatedPropertyType animatedPropertyType, PropertyType& property)
        : SVGAnimatedProperty(contextElement, attributeName, animatedPropertyType)
        , m_property(property)
        , m_animatedProperty(nullptr)
    {
    }

private:
    PropertyType& m_property;
    PropertyType* m_animatedProperty;
};

}
