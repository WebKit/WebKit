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

#include "Element.h"
#include "QualifiedName.h"
#include "SVGAnimatedProperty.h"
#include "SVGAnimatedPropertyType.h"
#include "SVGAttribute.h"
#include "SVGLengthValue.h"
#include "SVGNames.h"
#include "SVGPropertyTraits.h"

namespace WebCore {

class SVGAttribute;
class SVGElement;

template<typename OwnerType>
class SVGAttributeAccessor {
    WTF_MAKE_FAST_ALLOCATED;
public:
    SVGAttributeAccessor(const QualifiedName& attributeName)
        : m_attributeName(attributeName)
    {
    }
    virtual ~SVGAttributeAccessor() = default;

    const QualifiedName& attributeName() const { return m_attributeName; }

    virtual bool isMatched(const OwnerType&, const SVGAttribute&) const = 0;
    virtual void synchronizeProperty(OwnerType&, Element&) const = 0;

    virtual bool isAnimatedLengthAttribute() const { return false; }
    virtual AnimatedPropertyType animatedType() const { return AnimatedUnknown; }
    virtual Vector<AnimatedPropertyType> animatedTypes() const { return { animatedType() }; }

    virtual RefPtr<SVGAnimatedProperty> lookupOrCreateAnimatedProperty(OwnerType&, SVGElement&, const SVGAttribute&, AnimatedPropertyState) const { return nullptr; };
    virtual RefPtr<SVGAnimatedProperty> lookupAnimatedProperty(const OwnerType&, const SVGElement&, const SVGAttribute&) const { return nullptr; };
    virtual Vector<RefPtr<SVGAnimatedProperty>> lookupOrCreateAnimatedProperties(OwnerType&, SVGElement&, AnimatedPropertyState) const { return { }; }

protected:
    const QualifiedName m_attributeName;
};

template<typename OwnerType, typename AttributeType>
class SVGPropertyAttributeAccessor : public SVGAttributeAccessor<OwnerType> {
public:
    using Base = SVGAttributeAccessor<OwnerType>;
    using Base::m_attributeName;
    
    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, AttributeType OwnerType::*attribute>
    static SVGAttributeAccessor<OwnerType>& singleton()
    {
        static NeverDestroyed<SVGPropertyAttributeAccessor> attributeAccessor { attributeName, attributeName->localName(), attribute };
        return attributeAccessor;
    }

    SVGPropertyAttributeAccessor(const QualifiedName& attributeName, const AtomicString& identifier, AttributeType OwnerType::*attribute)
        : Base(attributeName)
        , m_identifier(identifier)
        , m_attribute(attribute)
    {
    }

protected:
    auto& attribute(OwnerType& owner) const { return owner.*m_attribute; }
    const auto& attribute(const OwnerType& owner) const { return owner.*m_attribute; }

    bool isMatched(const OwnerType& owner, const SVGAttribute& attribute) const override
    {
        return &this->attribute(owner) == &attribute;
    }

    void synchronizeProperty(OwnerType& owner, Element& element) const override
    {
        attribute(owner).synchronize(element, m_attributeName);
    }

    const AtomicString& m_identifier;
    AttributeType OwnerType::*m_attribute;
};

template<typename OwnerType, typename AnimatedAttributeType, AnimatedPropertyType type>
class SVGAnimatedAttributeAccessor : public SVGPropertyAttributeAccessor<OwnerType, AnimatedAttributeType> {
public:
    using PropertyTearOffType = typename AnimatedAttributeType::PropertyTearOffType;
    using PropertyType = typename AnimatedAttributeType::PropertyType;
    using Base = SVGPropertyAttributeAccessor<OwnerType, AnimatedAttributeType>;
    using Base::attribute;
    using Base::isMatched;
    using Base::m_attributeName;
    using Base::m_identifier;

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, AnimatedAttributeType OwnerType::*attribute>
    static SVGAttributeAccessor<OwnerType>& singleton()
    {
        static NeverDestroyed<SVGAnimatedAttributeAccessor> attributeAccessor { attributeName, attributeName->localName(), attribute };
        return attributeAccessor;
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, const AtomicString& (*identifier)(), AnimatedAttributeType OwnerType::*attribute>
    static SVGAttributeAccessor<OwnerType>& singleton()
    {
        static NeverDestroyed<SVGAnimatedAttributeAccessor> attributeAccessor { attributeName, identifier(), attribute };
        return attributeAccessor;
    }

    SVGAnimatedAttributeAccessor(const QualifiedName& attributeName, const AtomicString& identifier, AnimatedAttributeType OwnerType::*attribute)
        : Base(attributeName, identifier, attribute)
    {
    }

protected:
    template<typename PropertyTearOff = PropertyTearOffType, typename Property = PropertyType, AnimatedPropertyType animatedType = type>
    static RefPtr<SVGAnimatedProperty> lookupOrCreateAnimatedProperty(SVGElement& element, const QualifiedName& attributeName, const AtomicString& identifier, Property& property, AnimatedPropertyState animatedState)
    {
        return SVGAnimatedProperty::lookupOrCreateAnimatedProperty<PropertyTearOff, Property, animatedType>(element, attributeName, identifier, property, animatedState);
    }

    static RefPtr<SVGAnimatedProperty> lookupAnimatedProperty(const SVGElement& element, const AtomicString& identifier)
    {
        return SVGAnimatedProperty::lookupAnimatedProperty(element, identifier);
    }

    bool isAnimatedLengthAttribute() const override { return std::is_same<PropertyType, SVGLengthValue>::value; }
    AnimatedPropertyType animatedType() const override { return type; }

    RefPtr<SVGAnimatedProperty> lookupOrCreateAnimatedProperty(OwnerType& owner, SVGElement& element, const SVGAttribute& attribute, AnimatedPropertyState animatedState) const override
    {
        ASSERT_UNUSED(attribute, isMatched(owner, attribute));
        return lookupOrCreateAnimatedProperty(element, m_attributeName, m_identifier, this->attribute(owner).value(), animatedState);
    }

    RefPtr<SVGAnimatedProperty> lookupAnimatedProperty(const OwnerType&, const SVGElement& element, const SVGAttribute&) const override
    {
        return lookupAnimatedProperty(element, m_identifier);
    }

    Vector<RefPtr<SVGAnimatedProperty>> lookupOrCreateAnimatedProperties(OwnerType& owner, SVGElement& element, AnimatedPropertyState animatedState) const override
    {
        return { lookupOrCreateAnimatedProperty(element, m_attributeName, m_identifier, attribute(owner).value(), animatedState) };
    }
};

template<typename OwnerType, typename AnimatedAttributeType, AnimatedPropertyType type, typename SecondAnimatedAttributeType, AnimatedPropertyType secondType>
class SVGAnimatedPairAttributeAccessor : public SVGAnimatedAttributeAccessor<OwnerType, AnimatedAttributeType, type> {
public:
    using PropertyTearOffType = typename AnimatedAttributeType::PropertyTearOffType;
    using SecondPropertyTearOffType = typename SecondAnimatedAttributeType::PropertyTearOffType;
    using SecondPropertyType = typename SecondAnimatedAttributeType::PropertyType;
    using Base = SVGAnimatedAttributeAccessor<OwnerType, AnimatedAttributeType, type>;
    using Base::attribute;
    using Base::lookupOrCreateAnimatedProperty;
    using Base::lookupAnimatedProperty;
    using Base::m_attributeName;
    using Base::m_identifier;

    template<
        const LazyNeverDestroyed<const QualifiedName>& attributeName,
        const AtomicString& (*identifier)(), AnimatedAttributeType OwnerType::*attribute,
        const AtomicString& (*secondIdentifier)(), SecondAnimatedAttributeType OwnerType::*secondAttribute
    >
    static SVGAttributeAccessor<OwnerType>& singleton()
    {
        static NeverDestroyed<SVGAnimatedPairAttributeAccessor> attributeAccessor { attributeName, identifier(), attribute, secondIdentifier(), secondAttribute };
        return attributeAccessor;
    }

    SVGAnimatedPairAttributeAccessor(const QualifiedName& attributeName, const AtomicString& identifier, AnimatedAttributeType OwnerType::*attribute, const AtomicString& secondIdentifier, SecondAnimatedAttributeType OwnerType::*secondAttribute)
        : Base(attributeName, identifier, attribute)
        , m_secondIdentifier(secondIdentifier)
        , m_secondAttribute(secondAttribute)
    {
    }

private:
    auto& secondAttribute(OwnerType& owner) const { return owner.*m_secondAttribute; }
    const auto& secondAttribute(const OwnerType& owner) const { return owner.*m_secondAttribute; }

    bool isMatched(const OwnerType& owner, const SVGAttribute& attribute) const override
    {
        return Base::isMatched(owner, attribute) || &secondAttribute(owner) == &attribute;
    }

    void synchronizeProperty(OwnerType& owner, Element& element) const override
    {
        attribute(owner).synchronize(element, m_attributeName);
        secondAttribute(owner).synchronize(element, m_attributeName);
    }

    Vector<AnimatedPropertyType> animatedTypes() const override { return { type, secondType }; }

    RefPtr<SVGAnimatedProperty> lookupOrCreateAnimatedProperty(OwnerType& owner, SVGElement& element, const SVGAttribute& attribute, AnimatedPropertyState animatedState) const override
    {
        if (Base::isMatched(owner, attribute))
            return lookupOrCreateAnimatedProperty(element, m_attributeName, m_identifier, this->attribute(owner).value(), animatedState);
        ASSERT(&secondAttribute(owner) == &attribute);
        return Base::template lookupOrCreateAnimatedProperty<SecondPropertyTearOffType, SecondPropertyType, secondType>(element, m_attributeName, m_secondIdentifier, secondAttribute(owner).value(), animatedState);
    }

    RefPtr<SVGAnimatedProperty> lookupAnimatedProperty(const OwnerType& owner, const SVGElement& element, const SVGAttribute& attribute) const override
    {
        if (Base::isMatched(owner, attribute))
            return lookupAnimatedProperty(element, m_identifier);
        ASSERT(&secondAttribute(owner) == &attribute);
        return lookupAnimatedProperty(element, m_secondIdentifier);
    }

    Vector<RefPtr<SVGAnimatedProperty>> lookupOrCreateAnimatedProperties(OwnerType& owner, SVGElement& element, AnimatedPropertyState animatedState) const override
    {
        return {
            lookupOrCreateAnimatedProperty(element, m_attributeName, m_identifier, attribute(owner).value(), animatedState),
            Base::template lookupOrCreateAnimatedProperty<SecondPropertyTearOffType, SecondPropertyType, secondType>(element, m_attributeName, m_secondIdentifier, secondAttribute(owner).value(), animatedState)
        };
    }

    const AtomicString& m_secondIdentifier;
    SecondAnimatedAttributeType OwnerType::*m_secondAttribute;
};

template<typename OwnerType, typename AnimatedAttributeType, AnimatedPropertyType type>
using SVGAnimatedOptionalAttributeAccessor = SVGAnimatedPairAttributeAccessor<OwnerType, AnimatedAttributeType, type, AnimatedAttributeType, type>;

} // namespace WebCore
