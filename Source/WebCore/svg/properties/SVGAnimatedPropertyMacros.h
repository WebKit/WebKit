/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
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

#include "Element.h"
#include "SVGAnimatedProperty.h"
#include "SVGAttributeToPropertyMap.h"
#include "SVGPropertyTraits.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

// SVGSynchronizableAnimatedProperty implementation
template<typename PropertyType>
struct SVGSynchronizableAnimatedProperty {
    SVGSynchronizableAnimatedProperty()
        : value(SVGPropertyTraits<PropertyType>::initialValue())
        , shouldSynchronize(false)
        , isValid(false)
    {
    }

    template<typename ConstructorParameter1>
    SVGSynchronizableAnimatedProperty(const ConstructorParameter1& value1)
        : value(value1)
        , shouldSynchronize(false)
        , isValid(false)
    {
    }

    template<typename ConstructorParameter1, typename ConstructorParameter2>
    SVGSynchronizableAnimatedProperty(const ConstructorParameter1& value1, const ConstructorParameter2& value2)
        : value(value1, value2)
        , shouldSynchronize(false)
        , isValid(false)
    {
    }

    void synchronize(Element* ownerElement, const QualifiedName& attrName, const AtomicString& value)
    {
        ownerElement->setSynchronizedLazyAttribute(attrName, value);
    }

    PropertyType value;
    bool shouldSynchronize : 1;
    bool isValid : 1;
};

// Property registration helpers
#define BEGIN_REGISTER_ANIMATED_PROPERTIES(OwnerType) \
SVGAttributeToPropertyMap& OwnerType::attributeToPropertyMap() \
{ \
    static NeverDestroyed<SVGAttributeToPropertyMap> map; \
    return map; \
} \
\
static void registerAnimatedPropertiesFor##OwnerType() \
{ \
    auto& map = OwnerType::attributeToPropertyMap(); \
    if (!map.isEmpty()) \
        return; \
    typedef OwnerType UseOwnerType;

#define REGISTER_LOCAL_ANIMATED_PROPERTY(LowerProperty) map.addProperty(*UseOwnerType::LowerProperty##PropertyInfo());
#define REGISTER_PARENT_ANIMATED_PROPERTIES(ClassName) map.addProperties(ClassName::attributeToPropertyMap());
#define END_REGISTER_ANIMATED_PROPERTIES }

// Property definition helpers (used in SVG*.cpp files)
#define DEFINE_ANIMATED_PROPERTY(AnimatedPropertyTypeEnum, OwnerType, DOMAttribute, SVGDOMAttributeIdentifier, UpperProperty, LowerProperty) \
const SVGPropertyInfo* OwnerType::LowerProperty##PropertyInfo() { \
    static NeverDestroyed<const SVGPropertyInfo> s_propertyInfo = SVGPropertyInfo \
                        (AnimatedPropertyTypeEnum, \
                         PropertyIsReadWrite, \
                         DOMAttribute, \
                         SVGDOMAttributeIdentifier, \
                         &OwnerType::synchronize##UpperProperty, \
                         &OwnerType::lookupOrCreate##UpperProperty##Wrapper); \
    return &s_propertyInfo.get(); \
} 

// Property declaration helpers (used in SVG*.h files)
#define BEGIN_DECLARE_ANIMATED_PROPERTIES_BASE(OwnerType) \
public: \
    static SVGAttributeToPropertyMap& attributeToPropertyMap(); \
    virtual SVGAttributeToPropertyMap& localAttributeToPropertyMap() \
    { \
        return attributeToPropertyMap(); \
    } \
    typedef OwnerType UseOwnerType;

#define BEGIN_DECLARE_ANIMATED_PROPERTIES(OwnerType) \
public: \
    static SVGAttributeToPropertyMap& attributeToPropertyMap(); \
    SVGAttributeToPropertyMap& localAttributeToPropertyMap() override \
    { \
        return attributeToPropertyMap(); \
    } \
    typedef OwnerType UseOwnerType;

#define DECLARE_ANIMATED_PROPERTY(TearOffType, PropertyType, UpperProperty, LowerProperty, OverrideSpecifier) \
public: \
    static const SVGPropertyInfo* LowerProperty##PropertyInfo(); \
    PropertyType& LowerProperty() const \
    { \
        if (auto wrapper = SVGAnimatedProperty::lookupWrapper<UseOwnerType, TearOffType>(this, LowerProperty##PropertyInfo())) { \
            if (wrapper->isAnimating()) \
                return wrapper->currentAnimatedValue(); \
        } \
        return m_##LowerProperty.value; \
    } \
\
    PropertyType& LowerProperty##BaseValue() const OverrideSpecifier \
    { \
        return m_##LowerProperty.value; \
    } \
\
    void set##UpperProperty##BaseValue(const PropertyType& type, const bool validValue = true) OverrideSpecifier \
    { \
        m_##LowerProperty.value = type; \
        m_##LowerProperty.isValid = validValue; \
    } \
\
    Ref<TearOffType> LowerProperty##Animated() \
    { \
        m_##LowerProperty.shouldSynchronize = true; \
        return static_reference_cast<TearOffType>(lookupOrCreate##UpperProperty##Wrapper(this)); \
    } \
\
    bool LowerProperty##IsValid() const \
    { \
        return m_##LowerProperty.isValid; \
    } \
\
private: \
    void synchronize##UpperProperty() \
    { \
        if (!m_##LowerProperty.shouldSynchronize) \
            return; \
        AtomicString value(SVGPropertyTraits<PropertyType>::toString(m_##LowerProperty.value)); \
        m_##LowerProperty.synchronize(this, LowerProperty##PropertyInfo()->attributeName, value); \
    } \
\
    static Ref<SVGAnimatedProperty> lookupOrCreate##UpperProperty##Wrapper(SVGElement* maskedOwnerType) \
    { \
        ASSERT(maskedOwnerType); \
        UseOwnerType* ownerType = static_cast<UseOwnerType*>(maskedOwnerType); \
        return SVGAnimatedProperty::lookupOrCreateWrapper<UseOwnerType, TearOffType, PropertyType>(ownerType, LowerProperty##PropertyInfo(), ownerType->m_##LowerProperty.value); \
    } \
\
    static void synchronize##UpperProperty(SVGElement* maskedOwnerType) \
    { \
        ASSERT(maskedOwnerType); \
        UseOwnerType* ownerType = static_cast<UseOwnerType*>(maskedOwnerType); \
        ownerType->synchronize##UpperProperty(); \
    } \
\
    mutable SVGSynchronizableAnimatedProperty<PropertyType> m_##LowerProperty;

#define END_DECLARE_ANIMATED_PROPERTIES 

// List specific definition/declaration helpers
#define DECLARE_ANIMATED_LIST_PROPERTY(TearOffType, PropertyType, UpperProperty, LowerProperty) \
DECLARE_ANIMATED_PROPERTY(TearOffType, PropertyType, UpperProperty, LowerProperty, ) \
void detachAnimated##UpperProperty##ListWrappers(unsigned newListSize) \
{ \
    if (auto wrapper = SVGAnimatedProperty::lookupWrapper<UseOwnerType, TearOffType>(this, LowerProperty##PropertyInfo())) \
        wrapper->detachListWrappers(newListSize); \
}

} // namespace WebCore
