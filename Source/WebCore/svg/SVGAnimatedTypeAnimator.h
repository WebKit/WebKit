/*
 * Copyright (C) Research In Motion Limited 2011-2012. All rights reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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

#pragma once

#include "SVGAnimatedProperty.h"
#include "SVGAnimatedType.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

struct SVGElementAnimatedProperties {
    SVGElement* element;
    Vector<RefPtr<SVGAnimatedProperty>> properties;
};
typedef Vector<SVGElementAnimatedProperties> SVGElementAnimatedPropertyList;

class SVGAnimationElement;

class SVGAnimatedTypeAnimator {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~SVGAnimatedTypeAnimator();
    virtual std::unique_ptr<SVGAnimatedType> constructFromString(const String&) = 0;

    virtual std::unique_ptr<SVGAnimatedType> startAnimValAnimation(const SVGElementAnimatedPropertyList&) = 0;
    virtual void stopAnimValAnimation(const SVGElementAnimatedPropertyList&) = 0;
    virtual void resetAnimValToBaseVal(const SVGElementAnimatedPropertyList&, SVGAnimatedType&) = 0;
    virtual void animValWillChange(const SVGElementAnimatedPropertyList&) = 0;
    virtual void animValDidChange(const SVGElementAnimatedPropertyList&) = 0;
    virtual void addAnimatedTypes(SVGAnimatedType*, SVGAnimatedType*) = 0;

    virtual void calculateAnimatedValue(float percentage, unsigned repeatCount, SVGAnimatedType*, SVGAnimatedType*, SVGAnimatedType*, SVGAnimatedType*) = 0;
    virtual float calculateDistance(const String& fromString, const String& toString) = 0;

    void calculateFromAndToValues(std::unique_ptr<SVGAnimatedType>&, std::unique_ptr<SVGAnimatedType>&, const String& fromString, const String& toString);
    void calculateFromAndByValues(std::unique_ptr<SVGAnimatedType>&, std::unique_ptr<SVGAnimatedType>&, const String& fromString, const String& byString);

    void setContextElement(SVGElement* contextElement) { m_contextElement = contextElement; }
    AnimatedPropertyType type() const { return m_type; }

    SVGElementAnimatedPropertyList findAnimatedPropertiesForAttributeName(SVGElement&, const QualifiedName&);

protected:
    SVGAnimatedTypeAnimator(AnimatedPropertyType, SVGAnimationElement*, SVGElement*);

    // Helpers for animators that operate on single types, eg. just one SVGAnimatedInteger.
    template<typename AnimValType>
    std::unique_ptr<SVGAnimatedType> constructFromBaseValue(const SVGElementAnimatedPropertyList& animatedProperties)
    {
        ASSERT(animatedProperties[0].properties.size() == 1);
        const auto* animatedProperty = castAnimatedPropertyToActualType<AnimValType>(animatedProperties[0].properties[0].get());

        auto animatedType = SVGAnimatedType::create(animatedProperty->currentBaseValue());
        auto& animatedValue = animatedType->template as<typename AnimValType::ContentType>();
        executeAction<AnimValType>(StartAnimationAction, animatedProperties, 0, &animatedValue);
        return animatedType;
    }

    template<typename AnimValType>
    void resetFromBaseValue(const SVGElementAnimatedPropertyList& animatedProperties, SVGAnimatedType& animatedType)
    {
        ASSERT(animatedProperties[0].properties.size() == 1);
        ASSERT(animatedType.type() == m_type);
        auto* animatedProperty = castAnimatedPropertyToActualType<AnimValType>(animatedProperties[0].properties[0].get());
        animatedProperty->synchronizeWrappersIfNeeded();

        auto& animatedValue = animatedType.as<typename AnimValType::ContentType>();
        animatedValue = animatedProperty->currentBaseValue();
        executeAction<AnimValType>(StartAnimationAction, animatedProperties, 0, &animatedValue);
    }

    template<typename AnimValType>
    void stopAnimValAnimationForType(const SVGElementAnimatedPropertyList& animatedProperties)
    {
        ASSERT(animatedProperties[0].properties.size() == 1);
        executeAction<AnimValType>(StopAnimationAction, animatedProperties, 0);
    }

    template<typename AnimValType>
    void animValDidChangeForType(const SVGElementAnimatedPropertyList& animatedProperties)
    {
        ASSERT(animatedProperties[0].properties.size() == 1);
        executeAction<AnimValType>(AnimValDidChangeAction, animatedProperties, 0);
    }

    template<typename AnimValType>
    void animValWillChangeForType(const SVGElementAnimatedPropertyList& animatedProperties)
    {
        ASSERT(animatedProperties[0].properties.size() == 1);
        executeAction<AnimValType>(AnimValWillChangeAction, animatedProperties, 0);
    }

    // Helpers for animators that operate on pair types, eg. a pair of SVGAnimatedIntegers.
    template<typename AnimValType1, typename AnimValType2>
    std::unique_ptr<SVGAnimatedType> constructFromBaseValues(const SVGElementAnimatedPropertyList& animatedProperties)
    {
        ASSERT(animatedProperties[0].properties.size() == 2);
        const auto* firstAnimatedProperty = castAnimatedPropertyToActualType<AnimValType1>(animatedProperties[0].properties[0].get());
        const auto* secondAnimatedProperty = castAnimatedPropertyToActualType<AnimValType2>(animatedProperties[0].properties[1].get());

        auto animatedType = SVGAnimatedType::create(std::make_pair(firstAnimatedProperty->currentBaseValue(), secondAnimatedProperty->currentBaseValue()));
        auto& AnimatedValue = animatedType->template as<std::pair<typename AnimValType1::ContentType, typename AnimValType2::ContentType>>();
        executeAction<AnimValType1>(StartAnimationAction, animatedProperties, 0, &AnimatedValue.first);
        executeAction<AnimValType2>(StartAnimationAction, animatedProperties, 1, &AnimatedValue.second);
        return animatedType;
    }

    template<typename AnimValType1, typename AnimValType2>
    void resetFromBaseValues(const SVGElementAnimatedPropertyList& animatedProperties, SVGAnimatedType& animatedType)
    {
        ASSERT(animatedProperties[0].properties.size() == 2);
        ASSERT(animatedType.type() == m_type);
        auto* firstAnimatedProperty = castAnimatedPropertyToActualType<AnimValType1>(animatedProperties[0].properties[0].get());
        auto* secondAnimatedProperty =  castAnimatedPropertyToActualType<AnimValType2>(animatedProperties[0].properties[1].get());
        firstAnimatedProperty->synchronizeWrappersIfNeeded();
        secondAnimatedProperty->synchronizeWrappersIfNeeded();

        auto& animatedValue = animatedType.as<std::pair<typename AnimValType1::ContentType, typename AnimValType2::ContentType>>();
        animatedValue.first = firstAnimatedProperty->currentBaseValue();
        animatedValue.second = secondAnimatedProperty->currentBaseValue();
        executeAction<AnimValType1>(StartAnimationAction, animatedProperties, 0, &animatedValue.first);
        executeAction<AnimValType2>(StartAnimationAction, animatedProperties, 1, &animatedValue.second);
    }

    template<typename AnimValType1, typename AnimValType2>
    void stopAnimValAnimationForTypes(const SVGElementAnimatedPropertyList& animatedProperties)
    {
        ASSERT(animatedProperties[0].properties.size() == 2);
        executeAction<AnimValType1>(StopAnimationAction, animatedProperties, 0);
        executeAction<AnimValType2>(StopAnimationAction, animatedProperties, 1);
    }

    template<typename AnimValType1, typename AnimValType2>
    void animValDidChangeForTypes(const SVGElementAnimatedPropertyList& animatedProperties)
    {
        ASSERT(animatedProperties[0].properties.size() == 2);
        executeAction<AnimValType1>(AnimValDidChangeAction, animatedProperties, 0);
        executeAction<AnimValType2>(AnimValDidChangeAction, animatedProperties, 1);
    }

    template<typename AnimValType1, typename AnimValType2>
    void animValWillChangeForTypes(const SVGElementAnimatedPropertyList& animatedProperties)
    {
        ASSERT(animatedProperties[0].properties.size() == 2);
        executeAction<AnimValType1>(AnimValWillChangeAction, animatedProperties, 0);
        executeAction<AnimValType2>(AnimValWillChangeAction, animatedProperties, 1);
    }

    template<typename AnimValType>
    AnimValType* castAnimatedPropertyToActualType(SVGAnimatedProperty* property)
    {
        ASSERT(property);
        ASSERT(property->contextElement());
        // We can't assert property->animatedPropertyType() == m_type, as there's an exception for SVGMarkerElements orient attribute.
        if (property->animatedPropertyType() != m_type) {
            ASSERT(m_type == AnimatedAngle);
            ASSERT(property->animatedPropertyType() == AnimatedEnumeration);
        }
        return static_cast<AnimValType*>(property);
    }

    AnimatedPropertyType m_type;
    SVGAnimationElement* m_animationElement;
    SVGElement* m_contextElement;

private:
    enum AnimationAction {
        StartAnimationAction,
        StopAnimationAction,
        AnimValWillChangeAction,
        AnimValDidChangeAction
    };

    template<typename AnimValType>
    void executeAction(AnimationAction action, const SVGElementAnimatedPropertyList& animatedProperties, unsigned whichProperty, typename AnimValType::ContentType* animatedValue = nullptr)
    {
        // FIXME: Can't use SVGElement::InstanceUpdateBlocker because of circular header dependency. Would be nice to untangle this.
        setInstanceUpdatesBlocked(*animatedProperties[0].element, true);

        for (auto& animatedProperty : animatedProperties) {
            RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(whichProperty < animatedProperty.properties.size());
            AnimValType* property = castAnimatedPropertyToActualType<AnimValType>(animatedProperty.properties[whichProperty].get());

            switch (action) {
            case StartAnimationAction:
                ASSERT(animatedValue);
                if (!property->isAnimating())
                    property->animationStarted(animatedValue);
                break;
            case StopAnimationAction:
                ASSERT(!animatedValue);
                if (property->isAnimating())
                    property->animationEnded();
                break;
            case AnimValWillChangeAction:
                ASSERT(!animatedValue);
                property->animValWillChange();
                break;
            case AnimValDidChangeAction:
                ASSERT(!animatedValue);
                property->animValDidChange();
                break;
            }
        }

        setInstanceUpdatesBlocked(*animatedProperties[0].element, false);
    }

    static void setInstanceUpdatesBlocked(SVGElement&, bool);
};

} // namespace WebCore
