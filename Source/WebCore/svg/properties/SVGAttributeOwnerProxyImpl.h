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

#include "SVGAttributeOwnerProxy.h"
#include "SVGAttributeRegistry.h"

namespace WebCore {

template<typename OwnerType, typename... BaseTypes>
class SVGAttributeOwnerProxyImpl : public SVGAttributeOwnerProxy {
public:
    using AttributeRegistry = SVGAttributeRegistry<OwnerType, BaseTypes...>;

    SVGAttributeOwnerProxyImpl(OwnerType& owner, SVGElement& element, AnimatedPropertyState animatedState = PropertyIsReadWrite)
        : SVGAttributeOwnerProxy(element)
        , m_owner(owner)
        , m_animatedState(animatedState)
    {
        // This is the OwnerProxy constructor for the non SVGElement based owners, e.g. SVGTests.
    }

    SVGAttributeOwnerProxyImpl(OwnerType& owner)
        : SVGAttributeOwnerProxy(owner)
        , m_owner(owner)
    {
        static_assert(std::is_base_of<SVGElement, OwnerType>::value, "The owner of SVGAttributeOwnerProxy should be derived from SVGElement.");
    }

    static AttributeRegistry& attributeRegistry()
    {
        return AttributeRegistry::singleton();
    }

    static bool isKnownAttribute(const QualifiedName& attributeName)
    {
        return attributeRegistry().isKnownAttribute(attributeName);
    }

    static bool isAnimatedLengthAttribute(const QualifiedName& attributeName)
    {
        return attributeRegistry().isAnimatedLengthAttribute(attributeName);
    }

private:
    void synchronizeAttributes() const override
    {
        attributeRegistry().synchronizeAttributes(m_owner, *m_element);
    }

    void synchronizeAttribute(const QualifiedName& attributeName) const override
    {
        attributeRegistry().synchronizeAttribute(m_owner, *m_element, attributeName);
    }

    Vector<AnimatedPropertyType> animatedTypes(const QualifiedName& attributeName) const override
    {
        return attributeRegistry().animatedTypes(attributeName);
    }

    RefPtr<SVGAnimatedProperty> lookupOrCreateAnimatedProperty(const SVGAttribute& attribute) const override
    {
        return attributeRegistry().lookupOrCreateAnimatedProperty(m_owner, *m_element, attribute, m_animatedState);
    }

    RefPtr<SVGAnimatedProperty> lookupAnimatedProperty(const SVGAttribute& attribute) const override
    {
        return attributeRegistry().lookupAnimatedProperty(m_owner, *m_element, attribute);
    }

    Vector<RefPtr<SVGAnimatedProperty>> lookupOrCreateAnimatedProperties(const QualifiedName& attributeName) const override
    {
        return attributeRegistry().lookupOrCreateAnimatedProperties(m_owner, *m_element, attributeName, m_animatedState);
    }

    OwnerType& m_owner;
    AnimatedPropertyState m_animatedState { PropertyIsReadWrite };
};

}
