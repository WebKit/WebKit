/*
    Copyright (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
    Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef SVGAnimatedProperty_h
#define SVGAnimatedProperty_h

#if ENABLE(SVG)
#include "SVGAnimatedPropertySynchronizer.h"
#include "SVGAnimatedPropertyTraits.h"
#include "SVGAnimatedTemplate.h"

namespace WebCore {

template<typename AnimatedType>
class SVGAnimatedProperty;

template<typename AnimatedType>
class SVGAnimatedPropertyTearOff : public SVGAnimatedTemplate<AnimatedType> {
public:
    typedef typename SVGAnimatedPropertyTraits<AnimatedType>::PassType PassType;
    typedef typename SVGAnimatedPropertyTraits<AnimatedType>::ReturnType ReturnType;

    typedef SVGAnimatedPropertyTearOff<AnimatedType> Self;
    typedef SVGAnimatedProperty<AnimatedType> Creator;

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
    SVGAnimatedPropertyTearOff(Creator& creator, SVGElement* contextElement)
        : m_creator(creator)
        , m_contextElement(contextElement)
    {
        m_creator.setShouldSynchronize(true);
    }

    virtual ~SVGAnimatedPropertyTearOff()
    {
        m_creator.setShouldSynchronize(false);
    }

    Creator& m_creator;
    RefPtr<SVGElement> m_contextElement;
};

template<typename AnimatedType>
class SVGAnimatedProperty {
public:
    virtual ~SVGAnimatedProperty() { }

    typedef typename SVGAnimatedPropertyTraits<AnimatedType>::PassType PassType;
    typedef typename SVGAnimatedPropertyTraits<AnimatedType>::ReturnType ReturnType;
    typedef typename SVGAnimatedPropertyTraits<AnimatedType>::StoredType StoredType;

    SVGAnimatedProperty()
        : m_value(SVGAnimatedPropertyTraits<AnimatedType>::null())
        , m_shouldSynchronize(false)
    {
    }

    template<typename ConstructorParameterOne>
    SVGAnimatedProperty(const ConstructorParameterOne& value1)
        : m_value(value1)
        , m_shouldSynchronize(false)
    {
    }

    template<typename ConstructorParameterOne, typename ConstructorParameterTwo>
    SVGAnimatedProperty(const ConstructorParameterOne& value1, const ConstructorParameterTwo& value2)
        : m_value(value1, value2)
        , m_shouldSynchronize(false)
    {
    }

    ReturnType value() const { return SVGAnimatedPropertyTraits<AnimatedType>::toReturnType(m_value); }
    ReturnType baseValue() const { return SVGAnimatedPropertyTraits<AnimatedType>::toReturnType(m_value); }

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
class SVGAnimatedProperty##UpperProperty : public SVGAnimatedProperty<AnimatedType> { \
public: \
    SVGAnimatedProperty##UpperProperty() \
        : SVGAnimatedProperty<AnimatedType>() \
    { \
    } \
    \
    template<typename ConstructorParameterOne> \
    SVGAnimatedProperty##UpperProperty(const ConstructorParameterOne& value1) \
        : SVGAnimatedProperty<AnimatedType>(value1) \
    { \
    } \
    \
    template<typename ConstructorParameterOne, typename ConstructorParameterTwo> \
    SVGAnimatedProperty##UpperProperty(const ConstructorParameterOne& value1, const ConstructorParameterTwo& value2) \
        : SVGAnimatedProperty<AnimatedType>(value1, value2) \
    { \
    } \
    \
    void synchronize(SVGElement* contextElement) \
    { \
        ASSERT(m_shouldSynchronize); \
        AtomicString value(SVGAnimatedPropertyTraits<AnimatedType>::toString(baseValue())); \
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
    typedef SVGAnimatedPropertyTearOff<AnimatedType> SVGAnimatedPropertyTearOff##UpperProperty; \
    DEFINE_ANIMATED_PROPERTY(OwnerType, DOMAttribute, AnimatedType, UpperProperty); \
    SVGAnimatedProperty##UpperProperty m_##LowerProperty; \
    \
public: \
    SVGAnimatedPropertyTraits<AnimatedType>::ReturnType LowerProperty() const \
    { \
        return m_##LowerProperty.value(); \
    } \
    \
    SVGAnimatedPropertyTraits<AnimatedType>::ReturnType LowerProperty##BaseValue() const \
    { \
        return m_##LowerProperty.baseValue(); \
    } \
    \
    void set##UpperProperty(SVGAnimatedPropertyTraits<AnimatedType>::PassType type) \
    { \
        m_##LowerProperty.setValue(type); \
        SVGElement* contextElement = GetOwnerElementForType<OwnerType, IsDerivedFromSVGElement<OwnerType>::value>::ownerElement(this); \
        contextElement->invalidateSVGAttributes(); \
    } \
    \
    void set##UpperProperty##BaseValue(SVGAnimatedPropertyTraits<AnimatedType>::PassType type) \
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
    PassRefPtr<SVGAnimatedPropertyTearOff##UpperProperty> LowerProperty##Animated() \
    { \
        SVGElement* contextElement = GetOwnerElementForType<OwnerType, IsDerivedFromSVGElement<OwnerType>::value>::ownerElement(this); \
        return lookupOrCreateWrapper<AnimatedType, SVGAnimatedPropertyTearOff##UpperProperty>(contextElement, m_##LowerProperty, DOMAttribute); \
    }

// Used for SVG DOM properties that map exactly to one XML DOM attribute
#define DECLARE_ANIMATED_PROPERTY(OwnerType, DOMAttribute, AnimatedType, UpperProperty, LowerProperty) \
DECLARE_ANIMATED_PROPERTY_SHARED(OwnerType, DOMAttribute, DOMAttribute.localName(), AnimatedType, UpperProperty, LowerProperty)

// Used for the rare case multiple SVG DOM properties that map to the same XML dom attribute
#define DECLARE_ANIMATED_PROPERTY_MULTIPLE_WRAPPERS(OwnerType, DOMAttribute, SVGDOMAttributeIdentifier, AnimatedType, UpperProperty, LowerProperty) \
DECLARE_ANIMATED_PROPERTY_SHARED(OwnerType, DOMAttribute, SVGDOMAttributeIdentifier, AnimatedType, UpperProperty, LowerProperty)

#endif
#endif
