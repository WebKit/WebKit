/*
 * Copyright (C) 2018 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "SVGAnimatedBoolean.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedInteger.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedLengthList.h"
#include "SVGAnimatedNumber.h"
#include "SVGAnimatedNumberList.h"
#include "SVGAnimatedPointList.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "SVGAnimatedProperty.h"
#include "SVGAnimatedRect.h"
#include "SVGAnimatedString.h"
#include "SVGAnimatedTransformList.h"
#include "SVGAttributeAccessor.h"
#include "SVGStringListValues.h"
#include "SVGZoomAndPanType.h"
#include <wtf/HashMap.h>

namespace WebCore {

template<typename OwnerType, typename... BaseTypes>
class SVGAttributeRegistry {
public:
    static SVGAttributeRegistry<OwnerType, BaseTypes...>& singleton()
    {
        static NeverDestroyed<SVGAttributeRegistry<OwnerType, BaseTypes...>> map;
        return map;
    }

    // Non animatable attributes
    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, SVGStringListValuesAttribute OwnerType::*attribute>
    void registerAttribute()
    {
        registerAttribute(SVGStringListValuesAttributeAccessor<OwnerType>::template singleton<attributeName, attribute>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, SVGZoomAndPanTypeAttribute OwnerType::*attribute>
    void registerAttribute()
    {
        registerAttribute(SVGZoomAndPanTypeAttributeAccessor<OwnerType>::template singleton<attributeName, attribute>());
    }

    // Animatable attributes
    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, SVGAnimatedBooleanAttribute OwnerType::*attribute>
    void registerAttribute()
    {
        registerAttribute(SVGAnimatedBooleanAttributeAccessor<OwnerType>::template singleton<attributeName, attribute>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, typename EnumType, SVGAnimatedEnumerationAttribute<EnumType> OwnerType::*attribute>
    void registerAttribute()
    {
        registerAttribute(SVGAnimatedEnumerationAttributeAccessor<OwnerType, EnumType>::template singleton<attributeName, attribute>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, SVGAnimatedIntegerAttribute OwnerType::*attribute>
    void registerAttribute()
    {
        registerAttribute(SVGAnimatedIntegerAttributeAccessor<OwnerType>::template singleton<attributeName, attribute>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName,
        const AtomicString& (*identifier)(), SVGAnimatedIntegerAttribute OwnerType::*attribute,
        const AtomicString& (*optionalIdentifier)(), SVGAnimatedIntegerAttribute OwnerType::*optionalAttribute>
    void registerAttribute()
    {
        registerAttribute(SVGAnimatedOptionalIntegerAttributeAccessor<OwnerType>::template singleton<attributeName, identifier, attribute, optionalIdentifier, optionalAttribute>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, SVGAnimatedLengthAttribute OwnerType::*attribute>
    void registerAttribute()
    {
        registerAttribute(SVGAnimatedLengthAttributeAccessor<OwnerType>::template singleton<attributeName, attribute>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, SVGAnimatedLengthListAttribute OwnerType::*attribute>
    void registerAttribute()
    {
        registerAttribute(SVGAnimatedLengthListAttributeAccessor<OwnerType>::template singleton<attributeName, attribute>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, SVGAnimatedNumberAttribute OwnerType::*attribute>
    void registerAttribute()
    {
        registerAttribute(SVGAnimatedNumberAttributeAccessor<OwnerType>::template singleton<attributeName, attribute>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, SVGAnimatedNumberListAttribute OwnerType::*attribute>
    void registerAttribute()
    {
        registerAttribute(SVGAnimatedNumberListAttributeAccessor<OwnerType>::template singleton<attributeName, attribute>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName,
        const AtomicString& (*identifier)(), SVGAnimatedNumberAttribute OwnerType::*attribute,
        const AtomicString& (*optionalIdentifier)(), SVGAnimatedNumberAttribute OwnerType::*optionalAttribute>
    void registerAttribute()
    {
        registerAttribute(SVGAnimatedOptionalNumberAttributeAccessor<OwnerType>::template singleton<attributeName, identifier, attribute, optionalIdentifier, optionalAttribute>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, SVGAnimatedPointListAttribute OwnerType::*attribute>
    void registerAttribute()
    {
        registerAttribute(SVGAnimatedPointListAttributeAccessor<OwnerType>::template singleton<attributeName, attribute>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, SVGAnimatedPreserveAspectRatioAttribute OwnerType::*attribute>
    void registerAttribute()
    {
        registerAttribute(SVGAnimatedPreserveAspectRatioAttributeAccessor<OwnerType>::template singleton<attributeName, attribute>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, SVGAnimatedRectAttribute OwnerType::*attribute>
    void registerAttribute()
    {
        registerAttribute(SVGAnimatedRectAttributeAccessor<OwnerType>::template singleton<attributeName, attribute>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, SVGAnimatedStringAttribute OwnerType::*attribute>
    void registerAttribute()
    {
        registerAttribute(SVGAnimatedStringAttributeAccessor<OwnerType>::template singleton<attributeName, attribute>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, SVGAnimatedTransformListAttribute OwnerType::*attribute>
    void registerAttribute()
    {
        registerAttribute(SVGAnimatedTransformListAttributeAccessor<OwnerType>::template singleton<attributeName, attribute>());
    }

    bool isEmpty() const { return m_map.isEmpty(); }

    bool isKnownAttribute(const QualifiedName& attributeName) const
    {
        // Here we need to loop through the entries in the map and use matches() to compare them with attributeName.
        // m_map.contains() uses QualifiedName::operator==() which compares the impl pointers only while matches()
        // compares the contents if the impl pointers differ.
        auto it = std::find_if(m_map.begin(), m_map.end(), [&attributeName](const auto& entry) -> bool {
            return entry.key.matches(attributeName);
        });
        return it != m_map.end();
    }

    bool isAnimatedLengthAttribute(const QualifiedName& attributeName) const
    {
        if (const auto* attributeAccessor = findAttributeAccessor(attributeName))
            return attributeAccessor->isAnimatedLengthAttribute();
        return false;
    }

    Vector<AnimatedPropertyType> animatedTypes(const QualifiedName& attributeName) const
    {
        // If this registry has an accessor for attributeName, return animatedTypes() of this accessor.
        if (const auto* attributeAccessor = findAttributeAccessor(attributeName))
            return attributeAccessor->animatedTypes();
        // Otherwise loop through BaeTypes and see if any of them knows attributeName.
        return animatedTypesBaseTypes(attributeName);
    }

    void synchronizeAttributes(OwnerType& owner, SVGElement& element) const
    {
        for (auto* attributeAccessor : m_map.values())
            attributeAccessor->synchronizeProperty(owner, element);
        synchronizeAttributesBaseTypes(owner, element);
    }

    bool synchronizeAttribute(OwnerType& owner, SVGElement& element, const QualifiedName& attributeName) const
    {
        if (const auto* attributeAccessor = findAttributeAccessor(attributeName)) {
            attributeAccessor->synchronizeProperty(owner, element);
            return true;
        }
        return synchronizeAttributeBaseTypes(owner, element, attributeName);
    }

    RefPtr<SVGAnimatedProperty> lookupOrCreateAnimatedProperty(OwnerType& owner, SVGElement& element, const SVGAttribute& attribute, AnimatedPropertyState animatedState) const
    {
        if (const auto* attributeAccessor = findAttributeAccessor(owner, attribute))
            return attributeAccessor->lookupOrCreateAnimatedProperty(owner, element, attribute, animatedState);
        return lookupOrCreateAnimatedPropertyBaseTypes(owner, element, attribute, animatedState);
    }

    RefPtr<SVGAnimatedProperty> lookupAnimatedProperty(const OwnerType& owner, const SVGElement& element, const SVGAttribute& attribute) const
    {
        if (const auto* attributeAccessor = findAttributeAccessor(owner, attribute))
            return attributeAccessor->lookupAnimatedProperty(owner, element, attribute);
        return lookupAnimatedPropertyBaseTypes(owner, element, attribute);
    }

    Vector<RefPtr<SVGAnimatedProperty>> lookupOrCreateAnimatedProperties(OwnerType& owner, SVGElement& element, const QualifiedName& attributeName, AnimatedPropertyState animatedState) const
    {
        if (const auto* attributeAccessor = findAttributeAccessor(attributeName))
            return attributeAccessor->lookupOrCreateAnimatedProperties(owner, element, animatedState);
        return lookupOrCreateAnimatedPropertiesBaseTypes(owner, element, attributeName, animatedState);
    }

    void registerAttribute(const SVGAttributeAccessor<OwnerType>& attributeAccessor)
    {
        m_map.add(attributeAccessor.attributeName(), &attributeAccessor);
    }

private:
    // This is a template function with parameter 'I' whose default value = 0. So you can call it without any parameter from animatedTypes().
    // It returns Vector<AnimatedPropertyType> and is enable_if<I == sizeof...(BaseTypes)>. So it is mainly for breaking the recursion. If
    // it is called, this means no attribute was found in this registry for this QualifiedName. So return an empty Vector<AnimatedPropertyType>.
    template<size_t I = 0>
    static typename std::enable_if<I == sizeof...(BaseTypes), Vector<AnimatedPropertyType>>::type animatedTypesBaseTypes(const QualifiedName&) { return { }; }

    // This version of animatedTypesBaseTypes() is enable_if<I < sizeof...(BaseTypes)>.
    template<size_t I = 0>
    static typename std::enable_if<I < sizeof...(BaseTypes), Vector<AnimatedPropertyType>>::type animatedTypesBaseTypes(const QualifiedName& attributeName)
    {
        // Get the base type at index 'I' using std::tuple and std::tuple_element.
        using BaseType = typename std::tuple_element<I, typename std::tuple<BaseTypes...>>::type;

        // Get the SVGAttributeRegistry of BaseType. If it knows attributeName break the recursion.
        auto animatedTypes = BaseType::attributeRegistry().animatedTypes(attributeName);
        if (!animatedTypes.isEmpty())
            return animatedTypes;

        // SVGAttributeRegistry of BaseType does not know attributeName. Recurse to the next BaseType.
        return animatedTypesBaseTypes<I + 1>(attributeName);
    }

    template<size_t I = 0>
    static typename std::enable_if<I == sizeof...(BaseTypes), void>::type synchronizeAttributesBaseTypes(OwnerType&, Element&) { }

    template<size_t I = 0>
    static typename std::enable_if<I < sizeof...(BaseTypes), void>::type synchronizeAttributesBaseTypes(OwnerType& owner, SVGElement& element)
    {
        using BaseType = typename std::tuple_element<I, typename std::tuple<BaseTypes...>>::type;
        BaseType::attributeRegistry().synchronizeAttributes(owner, element);
        synchronizeAttributesBaseTypes<I + 1>(owner, element);
    }

    template<size_t I = 0>
    static typename std::enable_if<I == sizeof...(BaseTypes), bool>::type synchronizeAttributeBaseTypes(OwnerType&, Element&, const QualifiedName&) { return false; }

    template<size_t I = 0>
    static typename std::enable_if<I < sizeof...(BaseTypes), bool>::type synchronizeAttributeBaseTypes(OwnerType& owner, SVGElement& element, const QualifiedName& attributeName)
    {
        using BaseType = typename std::tuple_element<I, typename std::tuple<BaseTypes...>>::type;
        if (BaseType::attributeRegistry().synchronizeAttribute(owner, element, attributeName))
            return true;
        return synchronizeAttributeBaseTypes<I + 1>(owner, element, attributeName);
    }

    template<size_t I = 0>
    static typename std::enable_if<I == sizeof...(BaseTypes), RefPtr<SVGAnimatedProperty>>::type lookupOrCreateAnimatedPropertyBaseTypes(OwnerType&, SVGElement&, const SVGAttribute&, AnimatedPropertyState) { return nullptr; }

    template<size_t I = 0>
    static typename std::enable_if<I < sizeof...(BaseTypes), RefPtr<SVGAnimatedProperty>>::type lookupOrCreateAnimatedPropertyBaseTypes(OwnerType& owner, SVGElement& element, const SVGAttribute& attribute, AnimatedPropertyState animatedState)
    {
        using BaseType = typename std::tuple_element<I, typename std::tuple<BaseTypes...>>::type;
        if (auto animatedProperty = BaseType::attributeRegistry().lookupOrCreateAnimatedProperty(owner, element, attribute, animatedState))
            return animatedProperty;
        return lookupOrCreateAnimatedPropertyBaseTypes<I + 1>(owner, element, attribute, animatedState);
    }

    template<size_t I = 0>
    static typename std::enable_if<I == sizeof...(BaseTypes), RefPtr<SVGAnimatedProperty>>::type lookupAnimatedPropertyBaseTypes(const OwnerType&, const SVGElement&, const SVGAttribute&) { return nullptr; }

    template<size_t I = 0>
    static typename std::enable_if<I < sizeof...(BaseTypes), RefPtr<SVGAnimatedProperty>>::type lookupAnimatedPropertyBaseTypes(const OwnerType& owner, const SVGElement& element, const SVGAttribute& attribute)
    {
        using BaseType = typename std::tuple_element<I, typename std::tuple<BaseTypes...>>::type;
        if (auto animatedProperty = BaseType::attributeRegistry().lookupAnimatedProperty(owner, element, attribute))
            return animatedProperty;
        return lookupAnimatedPropertyBaseTypes<I + 1>(owner, element, attribute);
    }

    template<size_t I = 0>
    static typename std::enable_if<I == sizeof...(BaseTypes), Vector<RefPtr<SVGAnimatedProperty>>>::type lookupOrCreateAnimatedPropertiesBaseTypes(OwnerType&, SVGElement&, const QualifiedName&, AnimatedPropertyState) { return { }; }

    template<size_t I = 0>
    static typename std::enable_if<I < sizeof...(BaseTypes), Vector<RefPtr<SVGAnimatedProperty>>>::type lookupOrCreateAnimatedPropertiesBaseTypes(OwnerType& owner, SVGElement& element, const QualifiedName& attributeName, AnimatedPropertyState animatedState)
    {
        using BaseType = typename std::tuple_element<I, typename std::tuple<BaseTypes...>>::type;
        auto animatedProperties = BaseType::attributeRegistry().lookupOrCreateAnimatedProperties(owner, element, attributeName, animatedState);
        if (!animatedProperties.isEmpty())
            return animatedProperties;
        return lookupOrCreateAnimatedPropertiesBaseTypes<I + 1>(owner, element, attributeName, animatedState);
    }

    const SVGAttributeAccessor<OwnerType>* findAttributeAccessor(const OwnerType& owner, const SVGAttribute& attribute) const
    {
        for (auto* attributeAccessor : m_map.values()) {
            if (attributeAccessor->isMatched(owner, attribute))
                return attributeAccessor;
        }
        return nullptr;
    }

    const SVGAttributeAccessor<OwnerType>* findAttributeAccessor(const QualifiedName& attributeName) const
    {
        return m_map.get(attributeName);
    }

    HashMap<QualifiedName, const SVGAttributeAccessor<OwnerType>*> m_map;
};

}
