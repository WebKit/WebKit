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
            if (property->animatedPropertyType() == m_type)
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

    template<typename AnimatedType, typename AnimValType>
    void startAnimation(const Vector<SVGAnimatedProperty*>& properties, unsigned whichProperty, AnimatedType* type)
    {
        ASSERT(whichProperty < properties.size());
        AnimValType* property = castAnimatedPropertyToActualType<AnimValType>(properties[whichProperty]);

        Vector<AnimValType*> result;
        result.append(property);
        collectAnimatedPropertiesFromInstances(result, whichProperty, property->contextElement(), property->attributeName());

        size_t resultSize = result.size();
        for (size_t i = 0; i < resultSize; ++i)
            result[i]->animationStarted(type);
    }

    template<typename AnimValType>
    void stopAnimValAnimationForType(const Vector<SVGAnimatedProperty*>& properties)
    {
        Vector<AnimValType*> result;
        size_t propertiesSize = properties.size();
        for (size_t whichProperty = 0; whichProperty < propertiesSize; ++whichProperty) {
            AnimValType* property = castAnimatedPropertyToActualType<AnimValType>(properties[whichProperty]);
            SVGElementInstance::InstanceUpdateBlocker blocker(property->contextElement());

            result.append(property);
            collectAnimatedPropertiesFromInstances(result, whichProperty, property->contextElement(), property->attributeName());

            size_t resultSize = result.size();
            for (size_t i = 0; i < resultSize; ++i)
                result[i]->animationEnded();
            result.clear();
        }
    }

    template<typename AnimValType>
    void animValDidChangeForType(const Vector<SVGAnimatedProperty*>& properties)
    {
        Vector<AnimValType*> result;
        size_t propertiesSize = properties.size();
        for (size_t whichProperty = 0; whichProperty < propertiesSize; ++whichProperty) {
            AnimValType* property = castAnimatedPropertyToActualType<AnimValType>(properties[whichProperty]);
            SVGElementInstance::InstanceUpdateBlocker blocker(property->contextElement());

            result.append(property);
            collectAnimatedPropertiesFromInstances(result, whichProperty, property->contextElement(), property->attributeName());

            size_t resultSize = result.size();
            for (size_t i = 0; i < resultSize; ++i)
                result[i]->animValDidChange();
            result.clear();
        }
    }

    template<typename AnimValType>
    void animValWillChangeForType(const Vector<SVGAnimatedProperty*>& properties)
    {
        Vector<AnimValType*> result;
        size_t propertiesSize = properties.size();
        for (size_t whichProperty = 0; whichProperty < propertiesSize; ++whichProperty) {
            AnimValType* property = castAnimatedPropertyToActualType<AnimValType>(properties[whichProperty]);

            result.append(property);
            collectAnimatedPropertiesFromInstances(result, whichProperty, property->contextElement(), property->attributeName());

            size_t resultSize = result.size();
            for (size_t i = 0; i < resultSize; ++i)
                result[i]->animValWillChange();
            result.clear();
        }
    }

    template<typename AnimatedType, typename AnimValType>
    AnimatedType* constructFromOneBaseValue(const Vector<SVGAnimatedProperty*>& properties)
    {
        ASSERT(properties.size() == 1);
        const AnimatedType& animatedType = castAnimatedPropertyToActualType<AnimValType>(properties[0])->currentBaseValue();

        AnimatedType* copy = new AnimatedType(animatedType);
        startAnimation<AnimatedType, AnimValType>(properties, 0, copy);
        return copy;
    }

    template<typename AnimatedType, typename AnimValType>
    void resetFromOneBaseValue(const Vector<SVGAnimatedProperty*>& properties, SVGAnimatedType* type, AnimatedType& (SVGAnimatedType::*getter)())
    {
        ASSERT(type);
        ASSERT(type->type() == m_type);
        ASSERT(properties.size() == 1);
        (type->*getter)() = castAnimatedPropertyToActualType<AnimValType>(properties[0])->currentBaseValue();
    }

    template<typename AnimatedType, typename ContentType, typename ContentAnimValType>
    AnimatedType* constructFromTwoBaseValues(const Vector<SVGAnimatedProperty*>& properties)
    {
        ASSERT(properties.size() == 2);
        const ContentType& firstType = castAnimatedPropertyToActualType<ContentAnimValType>(properties[0])->currentBaseValue();
        const ContentType& secondType = castAnimatedPropertyToActualType<ContentAnimValType>(properties[1])->currentBaseValue();

        AnimatedType* copy = new AnimatedType(firstType, secondType);
        startAnimation<ContentType, ContentAnimValType>(properties, 0, &copy->first);
        startAnimation<ContentType, ContentAnimValType>(properties, 1, &copy->second);
        return copy;
    }

    template<typename AnimatedType, typename ContentType, typename ContentAnimValType>
    void resetFromTwoBaseValues(const Vector<SVGAnimatedProperty*>& properties, SVGAnimatedType* type, AnimatedType& (SVGAnimatedType::*getter)())
    {
        ASSERT(type);
        ASSERT(type->type() == m_type);
        ASSERT(properties.size() == 2);

        AnimatedType& animatedTypeValue = (type->*getter)();
        animatedTypeValue.first = castAnimatedPropertyToActualType<ContentAnimValType>(properties[0])->currentBaseValue();
        animatedTypeValue.second = castAnimatedPropertyToActualType<ContentAnimValType>(properties[1])->currentBaseValue();
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
        ASSERT(property->animatedPropertyType() == m_type);
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
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGAnimatedTypeAnimator_h
