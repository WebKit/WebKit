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

struct SVGElementAnimatedProperties {
    SVGElementAnimatedProperties()
        : element(0)
    { }

    SVGElementAnimatedProperties(SVGElement* element, Vector<RefPtr<SVGAnimatedProperty> >& properties)
        : element(element)
        , properties(properties)
    { }

    SVGElement* element;
    Vector<RefPtr<SVGAnimatedProperty> > properties;
};
typedef Vector<SVGElementAnimatedProperties> SVGElementAnimatedPropertyList;

class SVGAnimationElement;

class SVGAnimatedTypeAnimator {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~SVGAnimatedTypeAnimator() { }
    virtual PassOwnPtr<SVGAnimatedType> constructFromString(const String&) = 0;

    virtual PassOwnPtr<SVGAnimatedType> startAnimValAnimation(const SVGElementAnimatedPropertyList&) = 0;
    virtual void stopAnimValAnimation(const SVGElementAnimatedPropertyList&) = 0;
    virtual void resetAnimValToBaseVal(const SVGElementAnimatedPropertyList&, SVGAnimatedType*) = 0;
    virtual void animValWillChange(const SVGElementAnimatedPropertyList&) = 0;
    virtual void animValDidChange(const SVGElementAnimatedPropertyList&) = 0;
    virtual void addAnimatedTypes(SVGAnimatedType*, SVGAnimatedType*) = 0;

    virtual void calculateAnimatedValue(float percentage, unsigned repeatCount, SVGAnimatedType*, SVGAnimatedType*, SVGAnimatedType*, SVGAnimatedType*) = 0;
    virtual float calculateDistance(const String& fromString, const String& toString) = 0;

    void calculateFromAndToValues(OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, const String& fromString, const String& toString)
    {
        from = constructFromString(fromString);
        to = constructFromString(toString);
    }

    void calculateFromAndByValues(OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, const String& fromString, const String& byString)
    {
        from = constructFromString(fromString);
        to = constructFromString(byString);
        addAnimatedTypes(from.get(), to.get());
    }

    void setContextElement(SVGElement* contextElement) { m_contextElement = contextElement; }
    AnimatedPropertyType type() const { return m_type; }

    SVGElementAnimatedPropertyList findAnimatedPropertiesForAttributeName(SVGElement* targetElement, const QualifiedName& attributeName)
    {
        ASSERT(targetElement);

        SVGElementAnimatedPropertyList propertiesByInstance;

        Vector<RefPtr<SVGAnimatedProperty> > targetProperties;
        targetElement->localAttributeToPropertyMap().animatedPropertiesForAttribute(targetElement, attributeName, targetProperties);

        if (!SVGAnimatedType::supportsAnimVal(m_type))
            return SVGElementAnimatedPropertyList();

        SVGElementAnimatedProperties propertiesPair(targetElement, targetProperties);
        propertiesByInstance.append(propertiesPair);

        const HashSet<SVGElementInstance*>& instances = targetElement->instancesForElement();
        const HashSet<SVGElementInstance*>::const_iterator end = instances.end();
        for (HashSet<SVGElementInstance*>::const_iterator it = instances.begin(); it != end; ++it) {
            SVGElement* shadowTreeElement = (*it)->shadowTreeElement();
            if (!shadowTreeElement)
                continue;

            Vector<RefPtr<SVGAnimatedProperty> > instanceProperties;
            targetElement->localAttributeToPropertyMap().animatedPropertiesForAttribute(shadowTreeElement, attributeName, instanceProperties);

            SVGElementAnimatedProperties instancePropertiesPair(shadowTreeElement, instanceProperties);
            propertiesByInstance.append(instancePropertiesPair);
        }

#if !ASSERT_DISABLED
        SVGElementAnimatedPropertyList::const_iterator propertiesEnd = propertiesByInstance.end();
        for (SVGElementAnimatedPropertyList::const_iterator it = propertiesByInstance.begin(); it != propertiesEnd; ++it) {
            size_t propertiesSize = it->properties.size();
            for (size_t i = 0; i < propertiesSize; ++i) {
                RefPtr<SVGAnimatedProperty> property = it->properties[i];
                if (property->animatedPropertyType() != m_type) {
                    ASSERT(m_type == AnimatedAngle);
                    ASSERT(property->animatedPropertyType() == AnimatedEnumeration);
                }
            }
        }
#endif

        return propertiesByInstance;
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
    typename AnimValType::ContentType* constructFromBaseValue(const SVGElementAnimatedPropertyList& animatedTypes)
    {
        ASSERT(animatedTypes[0].properties.size() == 1);
        const typename AnimValType::ContentType& animatedType = castAnimatedPropertyToActualType<AnimValType>(animatedTypes[0].properties[0].get())->currentBaseValue();

        typename AnimValType::ContentType* copy = new typename AnimValType::ContentType(animatedType);
        executeAction<AnimValType>(StartAnimationAction, animatedTypes, 0, copy);
        return copy;
    }

    template<typename AnimValType>
    void resetFromBaseValue(const SVGElementAnimatedPropertyList& animatedTypes, SVGAnimatedType* type, typename AnimValType::ContentType& (SVGAnimatedType::*getter)())
    {
        ASSERT(animatedTypes[0].properties.size() == 1);
        ASSERT(type);
        ASSERT(type->type() == m_type);
        (type->*getter)() = castAnimatedPropertyToActualType<AnimValType>(animatedTypes[0].properties[0].get())->currentBaseValue();
    }

    template<typename AnimValType>
    void stopAnimValAnimationForType(const SVGElementAnimatedPropertyList& animatedTypes)
    {
        ASSERT(animatedTypes[0].properties.size() == 1);
        executeAction<AnimValType>(StopAnimationAction, animatedTypes, 0);
    }

    template<typename AnimValType>
    void animValDidChangeForType(const SVGElementAnimatedPropertyList& animatedTypes)
    {
        ASSERT(animatedTypes[0].properties.size() == 1);
        executeAction<AnimValType>(AnimValDidChangeAction, animatedTypes, 0);
    }

    template<typename AnimValType>
    void animValWillChangeForType(const SVGElementAnimatedPropertyList& animatedTypes)
    {
        ASSERT(animatedTypes[0].properties.size() == 1);
        executeAction<AnimValType>(AnimValWillChangeAction, animatedTypes, 0);
    }

    // Helpers for animators that operate on pair types, eg. a pair of SVGAnimatedIntegers.
    template<typename AnimValType1, typename AnimValType2>
    pair<typename AnimValType1::ContentType, typename AnimValType2::ContentType>* constructFromBaseValues(const SVGElementAnimatedPropertyList& animatedTypes)
    {
        ASSERT(animatedTypes[0].properties.size() == 2);
        const typename AnimValType1::ContentType& firstType = castAnimatedPropertyToActualType<AnimValType1>(animatedTypes[0].properties[0].get())->currentBaseValue();
        const typename AnimValType2::ContentType& secondType = castAnimatedPropertyToActualType<AnimValType2>(animatedTypes[0].properties[1].get())->currentBaseValue();

        pair<typename AnimValType1::ContentType, typename AnimValType2::ContentType>* copy = new pair<typename AnimValType1::ContentType, typename AnimValType2::ContentType>(firstType, secondType);
        executeAction<AnimValType1>(StartAnimationAction, animatedTypes, 0, &copy->first);
        executeAction<AnimValType2>(StartAnimationAction, animatedTypes, 1, &copy->second);
        return copy;
    }

    template<typename AnimValType1, typename AnimValType2>
    void resetFromBaseValues(const SVGElementAnimatedPropertyList& animatedTypes, SVGAnimatedType* type, pair<typename AnimValType1::ContentType, typename AnimValType2::ContentType>& (SVGAnimatedType::*getter)())
    {
        ASSERT(animatedTypes[0].properties.size() == 2);
        ASSERT(type);
        ASSERT(type->type() == m_type);

        pair<typename AnimValType1::ContentType, typename AnimValType2::ContentType>& animatedTypeValue = (type->*getter)();
        animatedTypeValue.first = castAnimatedPropertyToActualType<AnimValType1>(animatedTypes[0].properties[0].get())->currentBaseValue();
        animatedTypeValue.second = castAnimatedPropertyToActualType<AnimValType2>(animatedTypes[0].properties[1].get())->currentBaseValue();
    }

    template<typename AnimValType1, typename AnimValType2>
    void stopAnimValAnimationForTypes(const SVGElementAnimatedPropertyList& animatedTypes)
    {
        ASSERT(animatedTypes[0].properties.size() == 2);
        executeAction<AnimValType1>(StopAnimationAction, animatedTypes, 0);
        executeAction<AnimValType2>(StopAnimationAction, animatedTypes, 1);
    }

    template<typename AnimValType1, typename AnimValType2>
    void animValDidChangeForTypes(const SVGElementAnimatedPropertyList& animatedTypes)
    {
        ASSERT(animatedTypes[0].properties.size() == 2);
        executeAction<AnimValType1>(AnimValDidChangeAction, animatedTypes, 0);
        executeAction<AnimValType2>(AnimValDidChangeAction, animatedTypes, 1);
    }

    template<typename AnimValType1, typename AnimValType2>
    void animValWillChangeForTypes(const SVGElementAnimatedPropertyList& animatedTypes)
    {
        ASSERT(animatedTypes[0].properties.size() == 2);
        executeAction<AnimValType1>(AnimValWillChangeAction, animatedTypes, 0);
        executeAction<AnimValType2>(AnimValWillChangeAction, animatedTypes, 1);
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
    void executeAction(AnimationAction action, const SVGElementAnimatedPropertyList& animatedTypes, unsigned whichProperty, typename AnimValType::ContentType* type = 0)
    {
        SVGElementInstance::InstanceUpdateBlocker blocker(animatedTypes[0].element);

        SVGElementAnimatedPropertyList::const_iterator end = animatedTypes.end();
        for (SVGElementAnimatedPropertyList::const_iterator it = animatedTypes.begin(); it != end; ++it) {
            ASSERT(whichProperty < it->properties.size());
            AnimValType* property = castAnimatedPropertyToActualType<AnimValType>(it->properties[whichProperty].get());

            switch (action) {
            case StartAnimationAction:
                ASSERT(type);
                property->animationStarted(type);
                break;
            case StopAnimationAction:
                ASSERT(!type);
                property->animationEnded();
                break;
            case AnimValWillChangeAction:
                ASSERT(!type);
                property->animValWillChange();
                break;
            case AnimValDidChangeAction:
                ASSERT(!type);
                property->animValDidChange();
                break;
            }
        }
    }
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGAnimatedTypeAnimator_h
