/*
 * Copyright (C) Research In Motion Limited 2011-2012. All rights reserved.
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
#include "SVGAttributeToPropertyMap.h"
#include "SVGElementInstance.h"
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class SVGAnimationElement;

class SVGAnimatedTypeAnimator {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~SVGAnimatedTypeAnimator() { }
    virtual PassOwnPtr<SVGAnimatedType> constructFromString(const String&) = 0;

    // FIXME: Make these pure once all types implement this.
    virtual PassOwnPtr<SVGAnimatedType> startAnimValAnimation(const Vector<SVGAnimatedProperty*>&) { return PassOwnPtr<SVGAnimatedType>(); }
    virtual void stopAnimValAnimation(const Vector<SVGAnimatedProperty*>&) { }
    virtual void resetAnimValToBaseVal(const Vector<SVGAnimatedProperty*>&, SVGAnimatedType*) { }
    virtual void animValWillChange(const Vector<SVGAnimatedProperty*>&) { }
    virtual void animValDidChange(const Vector<SVGAnimatedProperty*>&) { }

    virtual void calculateFromAndToValues(OwnPtr<SVGAnimatedType>& fromValue, OwnPtr<SVGAnimatedType>& toValue, const String& fromString, const String& toString) = 0;
    virtual void calculateFromAndByValues(OwnPtr<SVGAnimatedType>& fromValue, OwnPtr<SVGAnimatedType>& toValue, const String& fromString, const String& toString) = 0;
    virtual void calculateAnimatedValue(float percentage, unsigned repeatCount,
                                        OwnPtr<SVGAnimatedType>& fromValue, OwnPtr<SVGAnimatedType>& toValue, OwnPtr<SVGAnimatedType>& animatedValue) = 0;
    virtual float calculateDistance(const String& fromString, const String& toString) = 0;

    void setContextElement(SVGElement* contextElement) { m_contextElement = contextElement; }

    AnimatedPropertyType type() const { return m_type; }

    Vector<SVGAnimatedProperty*> findAnimatedPropertiesForAttributeName(SVGElement* targetElement, const QualifiedName& attributeName)
    {
        ASSERT(targetElement);

        Vector<RefPtr<SVGAnimatedProperty> > properties;
        targetElement->localAttributeToPropertyMap().animatedPropertiesForAttribute(targetElement, attributeName, properties);

        // FIXME: This check can go away once all types support animVal.
        if (!SVGAnimatedType::supportsAnimVal(m_type))
            return Vector<SVGAnimatedProperty*>();

        Vector<SVGAnimatedProperty*> result;
        size_t propertiesSize = properties.size();
        for (size_t i = 0; i < propertiesSize; ++i) {
            SVGAnimatedProperty* property = properties[i].get();
            if (property->animatedPropertyType() != m_type) {
                ASSERT(m_type == AnimatedAngle);
                ASSERT(property->animatedPropertyType() == AnimatedEnumeration);
            }

            result.append(property);
        }

        return result;
    }

protected:
    SVGAnimatedTypeAnimator(AnimatedPropertyType type, SVGAnimationElement* animationElement, SVGElement* contextElement)
        : m_type(type)
        , m_animationElement(animationElement)
        , m_contextElement(contextElement)
    {
    }

    // Helpers for animators that operate on single types, eg. just one SVGAnimatedInteger.
    template<typename AnimValType>
    typename AnimValType::ContentType* constructFromBaseValue(const Vector<SVGAnimatedProperty*>& properties)
    {
        ASSERT(properties.size() == 1);
        const typename AnimValType::ContentType& animatedType = castAnimatedPropertyToActualType<AnimValType>(properties[0])->currentBaseValue();

        typename AnimValType::ContentType* copy = new typename AnimValType::ContentType(animatedType);
        executeAction<AnimValType>(StartAnimationAction, properties, 0, copy);
        return copy;
    }

    template<typename AnimValType>
    void resetFromBaseValue(const Vector<SVGAnimatedProperty*>& properties, SVGAnimatedType* type, typename AnimValType::ContentType& (SVGAnimatedType::*getter)())
    {
        ASSERT(properties.size() == 1);
        ASSERT(type);
        ASSERT(type->type() == m_type);
        (type->*getter)() = castAnimatedPropertyToActualType<AnimValType>(properties[0])->currentBaseValue();
    }

    template<typename AnimValType>
    void stopAnimValAnimationForType(const Vector<SVGAnimatedProperty*>& properties)
    {
        ASSERT(properties.size() == 1);
        executeAction<AnimValType>(StopAnimationAction, properties, 0);
    }

    template<typename AnimValType>
    void animValDidChangeForType(const Vector<SVGAnimatedProperty*>& properties)
    {
        ASSERT(properties.size() == 1);
        executeAction<AnimValType>(AnimValDidChangeAction, properties, 0);
    }

    template<typename AnimValType>
    void animValWillChangeForType(const Vector<SVGAnimatedProperty*>& properties)
    {
        ASSERT(properties.size() == 1);
        executeAction<AnimValType>(AnimValWillChangeAction, properties, 0);
    }

    // Helpers for animators that operate on pair types, eg. a pair of SVGAnimatedIntegers.
    template<typename AnimValType1, typename AnimValType2>
    pair<typename AnimValType1::ContentType, typename AnimValType2::ContentType>* constructFromBaseValues(const Vector<SVGAnimatedProperty*>& properties)
    {
        ASSERT(properties.size() == 2);
        const typename AnimValType1::ContentType& firstType = castAnimatedPropertyToActualType<AnimValType1>(properties[0])->currentBaseValue();
        const typename AnimValType2::ContentType& secondType = castAnimatedPropertyToActualType<AnimValType2>(properties[1])->currentBaseValue();

        pair<typename AnimValType1::ContentType, typename AnimValType2::ContentType>* copy = new pair<typename AnimValType1::ContentType, typename AnimValType2::ContentType>(firstType, secondType);
        executeAction<AnimValType1>(StartAnimationAction, properties, 0, &copy->first);
        executeAction<AnimValType2>(StartAnimationAction, properties, 1, &copy->second);
        return copy;
    }

    template<typename AnimValType1, typename AnimValType2>
    void resetFromBaseValues(const Vector<SVGAnimatedProperty*>& properties, SVGAnimatedType* type, pair<typename AnimValType1::ContentType, typename AnimValType2::ContentType>& (SVGAnimatedType::*getter)())
    {
        ASSERT(properties.size() == 2);
        ASSERT(type);
        ASSERT(type->type() == m_type);

        pair<typename AnimValType1::ContentType, typename AnimValType2::ContentType>& animatedTypeValue = (type->*getter)();
        animatedTypeValue.first = castAnimatedPropertyToActualType<AnimValType1>(properties[0])->currentBaseValue();
        animatedTypeValue.second = castAnimatedPropertyToActualType<AnimValType2>(properties[1])->currentBaseValue();
    }

    template<typename AnimValType1, typename AnimValType2>
    void stopAnimValAnimationForTypes(const Vector<SVGAnimatedProperty*>& properties)
    {
        ASSERT(properties.size() == 2);
        executeAction<AnimValType1>(StopAnimationAction, properties, 0);
        executeAction<AnimValType2>(StopAnimationAction, properties, 1);
    }

    template<typename AnimValType1, typename AnimValType2>
    void animValDidChangeForTypes(const Vector<SVGAnimatedProperty*>& properties)
    {
        ASSERT(properties.size() == 2);
        executeAction<AnimValType1>(AnimValDidChangeAction, properties, 0);
        executeAction<AnimValType2>(AnimValDidChangeAction, properties, 1);
    }

    template<typename AnimValType1, typename AnimValType2>
    void animValWillChangeForTypes(const Vector<SVGAnimatedProperty*>& properties)
    {
        ASSERT(properties.size() == 2);
        executeAction<AnimValType1>(AnimValWillChangeAction, properties, 0);
        executeAction<AnimValType2>(AnimValWillChangeAction, properties, 1);
    }

    AnimatedPropertyType m_type;
    SVGAnimationElement* m_animationElement;
    SVGElement* m_contextElement;

private:
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

    template<typename AnimValType>
    void collectAnimatedPropertiesFromInstances(Vector<AnimValType*>& result, unsigned whichProperty, SVGElement* targetElement, const QualifiedName& attributeName)
    {
        ASSERT(targetElement);

        // If the target element has instances, update them as well, w/o requiring the <use> tree to be rebuilt.
        const HashSet<SVGElementInstance*>& instances = targetElement->instancesForElement();
        const HashSet<SVGElementInstance*>::const_iterator end = instances.end();
        for (HashSet<SVGElementInstance*>::const_iterator it = instances.begin(); it != end; ++it) {
            SVGElement* shadowTreeElement = (*it)->shadowTreeElement();
            if (!shadowTreeElement)
                continue;

            Vector<SVGAnimatedProperty*> propertiesInstance = findAnimatedPropertiesForAttributeName(shadowTreeElement, attributeName);
            ASSERT(whichProperty < propertiesInstance.size());
            result.append(castAnimatedPropertyToActualType<AnimValType>(propertiesInstance[whichProperty]));
        }
    }

    enum AnimationAction {
        StartAnimationAction,
        StopAnimationAction,
        AnimValWillChangeAction,
        AnimValDidChangeAction
    };

    template<typename AnimValType>
    void executeAction(AnimationAction action, const Vector<SVGAnimatedProperty*>& properties, unsigned whichProperty, typename AnimValType::ContentType* type = 0)
    {
        ASSERT(whichProperty < properties.size());
        AnimValType* property = castAnimatedPropertyToActualType<AnimValType>(properties[whichProperty]);
        SVGElementInstance::InstanceUpdateBlocker blocker(property->contextElement());

        Vector<AnimValType*> result;
        result.append(property);
        collectAnimatedPropertiesFromInstances(result, whichProperty, property->contextElement(), property->attributeName());

        size_t resultSize = result.size();
        for (size_t i = 0; i < resultSize; ++i) {
            switch (action) {
            case StartAnimationAction:
                ASSERT(type);
                result[i]->animationStarted(type);
                break;
            case StopAnimationAction:
                ASSERT(!type);
                result[i]->animationEnded();
                break;
            case AnimValWillChangeAction:
                ASSERT(!type);
                result[i]->animValWillChange();
                break;
            case AnimValDidChangeAction:
                ASSERT(!type);
                result[i]->animValDidChange();
                break;
            }
        }
    }
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGAnimatedTypeAnimator_h
