/*
 * Copyright (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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

#ifndef DeprecatedSVGAnimatedProperty_h
#define DeprecatedSVGAnimatedProperty_h

#if ENABLE(SVG)
#include "SVGAnimatedPropertySynchronizer.h"
#include "DeprecatedSVGAnimatedPropertyTraits.h"
#include "DeprecatedSVGAnimatedTemplate.h"

namespace WebCore {

template<typename AnimatedType>
class DeprecatedSVGAnimatedProperty;

template<typename AnimatedType>
class DeprecatedSVGAnimatedPropertyTearOff : public DeprecatedSVGAnimatedTemplate<AnimatedType> {
public:
    typedef typename DeprecatedSVGAnimatedPropertyTraits<AnimatedType>::PassType PassType;
    typedef typename DeprecatedSVGAnimatedPropertyTraits<AnimatedType>::ReturnType ReturnType;

    typedef DeprecatedSVGAnimatedPropertyTearOff<AnimatedType> Self;
    typedef DeprecatedSVGAnimatedProperty<AnimatedType> Creator;

    static PassRefPtr<Self> create(Creator& creator, SVGElement* contextElement)
    {
        return adoptRef(new Self(creator, contextElement));
    }

    virtual void setBaseVal(PassType type)
    {
        m_creator.setBaseValue(type);
        m_contextElement->invalidateSVGAttributes();
    }

    virtual void setAnimVal(PassType type)
    {
        m_creator.setValue(type);
        m_contextElement->invalidateSVGAttributes();
    }

    virtual ReturnType baseVal() const { return m_creator.baseValue(); }
    virtual ReturnType animVal() const { return m_creator.value(); }
    virtual const QualifiedName& associatedAttributeName() const { return m_creator.associatedAttributeName(); }

private:
    DeprecatedSVGAnimatedPropertyTearOff(Creator& creator, SVGElement* contextElement)
        : m_creator(creator)
        , m_contextElement(contextElement)
    {
        m_creator.setShouldSynchronize(true);
    }

    virtual ~DeprecatedSVGAnimatedPropertyTearOff()
    {
        m_creator.setShouldSynchronize(false);
    }

    Creator& m_creator;
    RefPtr<SVGElement> m_contextElement;
};

template<typename AnimatedType>
class DeprecatedSVGAnimatedProperty {
public:
    virtual ~DeprecatedSVGAnimatedProperty() { }

    typedef typename DeprecatedSVGAnimatedPropertyTraits<AnimatedType>::PassType PassType;
    typedef typename DeprecatedSVGAnimatedPropertyTraits<AnimatedType>::ReturnType ReturnType;
    typedef typename DeprecatedSVGAnimatedPropertyTraits<AnimatedType>::StoredType StoredType;

    DeprecatedSVGAnimatedProperty()
        : m_value(DeprecatedSVGAnimatedPropertyTraits<AnimatedType>::null())
        , m_shouldSynchronize(false)
    {
    }

    template<typename ConstructorParameterOne>
    DeprecatedSVGAnimatedProperty(const ConstructorParameterOne& value1)
        : m_value(value1)
        , m_shouldSynchronize(false)
    {
    }

    template<typename ConstructorParameterOne, typename ConstructorParameterTwo>
    DeprecatedSVGAnimatedProperty(const ConstructorParameterOne& value1, const ConstructorParameterTwo& value2)
        : m_value(value1, value2)
        , m_shouldSynchronize(false)
    {
    }

    ReturnType value() const { return DeprecatedSVGAnimatedPropertyTraits<AnimatedType>::toReturnType(m_value); }
    ReturnType baseValue() const { return DeprecatedSVGAnimatedPropertyTraits<AnimatedType>::toReturnType(m_value); }

    void setValue(PassType type) { m_value = type; }
    void setBaseValue(PassType type) { m_value = type; }

    bool shouldSynchronize() const { return m_shouldSynchronize; }
    void setShouldSynchronize(bool value) { m_shouldSynchronize = value; }

    virtual const QualifiedName& associatedAttributeName() const = 0;

protected:
    StoredType m_value;
    bool m_shouldSynchronize;
};

};

// Helper macro used within DECLARE_ANIMATED_PROPERTY below
#define DEFINE_ANIMATED_PROPERTY(OwnerType, DOMAttribute, AnimatedType, UpperProperty) \
class DeprecatedSVGAnimatedProperty##UpperProperty : public DeprecatedSVGAnimatedProperty<AnimatedType> { \
public: \
    DeprecatedSVGAnimatedProperty##UpperProperty() \
        : DeprecatedSVGAnimatedProperty<AnimatedType>() \
    { \
    } \
    \
    template<typename ConstructorParameterOne> \
    DeprecatedSVGAnimatedProperty##UpperProperty(const ConstructorParameterOne& value1) \
        : DeprecatedSVGAnimatedProperty<AnimatedType>(value1) \
    { \
    } \
    \
    template<typename ConstructorParameterOne, typename ConstructorParameterTwo> \
    DeprecatedSVGAnimatedProperty##UpperProperty(const ConstructorParameterOne& value1, const ConstructorParameterTwo& value2) \
        : DeprecatedSVGAnimatedProperty<AnimatedType>(value1, value2) \
    { \
    } \
    \
    void synchronize(SVGElement* contextElement) \
    { \
        ASSERT(m_shouldSynchronize); \
        AtomicString value(DeprecatedSVGAnimatedPropertyTraits<AnimatedType>::toString(baseValue())); \
        SVGAnimatedPropertySynchronizer<IsDerivedFromSVGElement<OwnerType>::value>::synchronize(contextElement, DOMAttribute, value); \
    } \
    \
    virtual const QualifiedName& associatedAttributeName() const \
    { \
        return DOMAttribute; \
    } \
}

// Helper macro shared by DECLARE_ANIMATED_PROPERTY / DECLARE_ANIMATED_PROPERTY_MULTIPLE_WRAPPERS
#define DECLARE_ANIMATED_PROPERTY_SHARED(OwnerType, DOMAttribute, SVGDOMAttributeIdentifier, AnimatedType, UpperProperty, LowerProperty) \
private: \
    typedef DeprecatedSVGAnimatedPropertyTearOff<AnimatedType> DeprecatedSVGAnimatedPropertyTearOff##UpperProperty; \
    DEFINE_ANIMATED_PROPERTY(OwnerType, DOMAttribute, AnimatedType, UpperProperty); \
    DeprecatedSVGAnimatedProperty##UpperProperty m_##LowerProperty; \
    \
public: \
    DeprecatedSVGAnimatedPropertyTraits<AnimatedType>::ReturnType LowerProperty() const \
    { \
        return m_##LowerProperty.value(); \
    } \
    \
    DeprecatedSVGAnimatedPropertyTraits<AnimatedType>::ReturnType LowerProperty##BaseValue() const \
    { \
        return m_##LowerProperty.baseValue(); \
    } \
    \
    void set##UpperProperty(DeprecatedSVGAnimatedPropertyTraits<AnimatedType>::PassType type) \
    { \
        m_##LowerProperty.setValue(type); \
        SVGElement* contextElement = GetOwnerElementForType<OwnerType, IsDerivedFromSVGElement<OwnerType>::value>::ownerElement(this); \
        contextElement->invalidateSVGAttributes(); \
    } \
    \
    void set##UpperProperty##BaseValue(DeprecatedSVGAnimatedPropertyTraits<AnimatedType>::PassType type) \
    { \
        m_##LowerProperty.setBaseValue(type); \
        SVGElement* contextElement = GetOwnerElementForType<OwnerType, IsDerivedFromSVGElement<OwnerType>::value>::ownerElement(this); \
        contextElement->invalidateSVGAttributes(); \
    } \
    \
    void synchronize##UpperProperty() \
    { \
        if (!m_##LowerProperty.shouldSynchronize()) \
            return; \
        SVGElement* contextElement = GetOwnerElementForType<OwnerType, IsDerivedFromSVGElement<OwnerType>::value>::ownerElement(this); \
        m_##LowerProperty.synchronize(contextElement); \
    } \
    \
    PassRefPtr<DeprecatedSVGAnimatedPropertyTearOff##UpperProperty> LowerProperty##Animated() \
    { \
        SVGElement* contextElement = GetOwnerElementForType<OwnerType, IsDerivedFromSVGElement<OwnerType>::value>::ownerElement(this); \
        return lookupOrCreateWrapper<AnimatedType, DeprecatedSVGAnimatedPropertyTearOff##UpperProperty>(contextElement, m_##LowerProperty, DOMAttribute); \
    }

// Used for SVG DOM properties that map exactly to one XML DOM attribute
#define DECLARE_ANIMATED_PROPERTY(OwnerType, DOMAttribute, AnimatedType, UpperProperty, LowerProperty) \
DECLARE_ANIMATED_PROPERTY_SHARED(OwnerType, DOMAttribute, DOMAttribute.localName(), AnimatedType, UpperProperty, LowerProperty)

// Used for the rare case multiple SVG DOM properties that map to the same XML dom attribute
#define DECLARE_ANIMATED_PROPERTY_MULTIPLE_WRAPPERS(OwnerType, DOMAttribute, SVGDOMAttributeIdentifier, AnimatedType, UpperProperty, LowerProperty) \
DECLARE_ANIMATED_PROPERTY_SHARED(OwnerType, DOMAttribute, SVGDOMAttributeIdentifier, AnimatedType, UpperProperty, LowerProperty)

#endif
#endif
