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

#ifndef SVGAnimatedTypeAnimator_h
#define SVGAnimatedTypeAnimator_h

#if ENABLE(SVG)
#include "SVGAnimatedProperty.h"
#include "SVGAnimatedType.h"
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class SVGAnimationElement;
class SVGGenericAnimatedType;

class SVGAnimatedTypeAnimator {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~SVGAnimatedTypeAnimator() { }
    virtual PassOwnPtr<SVGAnimatedType> constructFromString(const String&) = 0;
    // FIXME: Make these two pure once all types implement this.
    virtual PassOwnPtr<SVGAnimatedType> constructFromBaseValue(const Vector<SVGAnimatedProperty*>&, Vector<SVGGenericAnimatedType*>&) { return PassOwnPtr<SVGAnimatedType>(); }
    virtual void resetAnimatedTypeToBaseValue(const Vector<SVGAnimatedProperty*>&, SVGAnimatedType*) { }

    virtual void calculateFromAndToValues(OwnPtr<SVGAnimatedType>& fromValue, OwnPtr<SVGAnimatedType>& toValue, const String& fromString, const String& toString) = 0;
    virtual void calculateFromAndByValues(OwnPtr<SVGAnimatedType>& fromValue, OwnPtr<SVGAnimatedType>& toValue, const String& fromString, const String& toString) = 0;
    virtual void calculateAnimatedValue(float percentage, unsigned repeatCount,
                                        OwnPtr<SVGAnimatedType>& fromValue, OwnPtr<SVGAnimatedType>& toValue, OwnPtr<SVGAnimatedType>& animatedValue) = 0;
    virtual float calculateDistance(const String& fromString, const String& toString) = 0;

    void setContextElement(SVGElement* contextElement) { m_contextElement = contextElement; }

    AnimatedPropertyType type() const { return m_type; }

protected:
    SVGAnimatedTypeAnimator(AnimatedPropertyType type, SVGAnimationElement* animationElement, SVGElement* contextElement)
        : m_type(type)
        , m_animationElement(animationElement)
        , m_contextElement(contextElement)
    {
    }

    SVGGenericAnimatedType* currentBaseValueVariant(SVGAnimatedProperty* property)
    {
        ASSERT(property);
        ASSERT(property->animatedPropertyType() == m_type);
        return property->currentBaseValueVariant();
    }

    template<typename AnimatedType>
    AnimatedType* constructFromOneBaseValue(const Vector<SVGAnimatedProperty*>& properties, Vector<SVGGenericAnimatedType*>& types)
    {
        ASSERT(properties.size() == 1);
        SVGGenericAnimatedType* animatedType = currentBaseValueVariant(properties[0]);

        // FIXME: Make this type safe, a follow-up patch will adress this.
        AnimatedType* copy = new AnimatedType(*reinterpret_cast<AnimatedType*>(animatedType));
        types.append(reinterpret_cast<SVGGenericAnimatedType*>(copy));
        return copy;
    }

    template<typename AnimatedType>
    void resetAnimatedTypeFromOneBaseValue(const Vector<SVGAnimatedProperty*>& properties, SVGAnimatedType* type, AnimatedType& (SVGAnimatedType::*getter)())
    {
        ASSERT(type);
        ASSERT(type->type() == m_type);
        ASSERT(properties.size() == 1);
        // FIXME: Make this type safe, a follow-up patch will adress this.
        (type->*getter)() = *reinterpret_cast<AnimatedType*>(currentBaseValueVariant(properties[0]));
    }

    template<typename AnimatedType, typename ContentType>
    AnimatedType* constructFromTwoBaseValues(const Vector<SVGAnimatedProperty*>& properties, Vector<SVGGenericAnimatedType*>& types)
    {
        ASSERT(properties.size() == 2);
        SVGGenericAnimatedType* firstType = currentBaseValueVariant(properties[0]);
        SVGGenericAnimatedType* secondType = currentBaseValueVariant(properties[1]);

        // FIXME: Make this type safe, a follow-up patch will adress this.
        AnimatedType* copy = new AnimatedType(*reinterpret_cast<ContentType*>(firstType), *reinterpret_cast<ContentType*>(secondType));
        types.reserveCapacity(2);
        types.append(reinterpret_cast<SVGGenericAnimatedType*>(&copy->first));
        types.append(reinterpret_cast<SVGGenericAnimatedType*>(&copy->second));
        return copy;
    }

    template<typename AnimatedType, typename ContentType>
    void resetAnimatedTypeFromTwoBaseValues(const Vector<SVGAnimatedProperty*>& properties, SVGAnimatedType* type, AnimatedType& (SVGAnimatedType::*getter)())
    {
        ASSERT(type);
        ASSERT(type->type() == m_type);
        ASSERT(properties.size() == 2);

        // FIXME: Make this type safe, a follow-up patch will adress this.
        AnimatedType& animatedTypeValue = (type->*getter)();
        animatedTypeValue.first = *reinterpret_cast<ContentType*>(currentBaseValueVariant(properties[0]));
        animatedTypeValue.second = *reinterpret_cast<ContentType*>(currentBaseValueVariant(properties[1]));
    }

    AnimatedPropertyType m_type;
    SVGAnimationElement* m_animationElement;
    SVGElement* m_contextElement;
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGAnimatedTypeAnimator_h
