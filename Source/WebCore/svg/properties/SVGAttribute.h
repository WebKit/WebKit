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
#include "SVGAttributeOwnerProxy.h"
#include "SVGPropertyTraits.h"

namespace WebCore {

class SVGAttribute { };

template<typename PropertyType>
class SVGPropertyAttribute : public SVGAttribute {
public:
    SVGPropertyAttribute()
        : m_property(SVGPropertyTraits<PropertyType>::initialValue())
    {
    }

    template<typename... Arguments>
    SVGPropertyAttribute(Arguments&&... arguments)
        : m_property(std::forward<Arguments>(arguments)...)
    {
    }

    PropertyType& value() { return m_property; }
    const PropertyType& value() const { return m_property; }

    void setValue(const PropertyType& property) { m_property = property; }
    void setValue(PropertyType&& property) { m_property = WTFMove(property); }
    void resetValue() { m_property = SVGPropertyTraits<PropertyType>::initialValue(); }

    String toString() const { return SVGPropertyTraits<PropertyType>::toString(m_property); }

    void setShouldSynchronize(bool shouldSynchronize) { m_shouldSynchronize = shouldSynchronize; }
    bool shouldSynchronize() const { return m_shouldSynchronize; }
    void synchronize(Element& element, const QualifiedName& attributeName)
    {
        if (!m_shouldSynchronize)
            return;
        element.setSynchronizedLazyAttribute(attributeName, toString());
    }

protected:
    PropertyType m_property;
    bool m_shouldSynchronize { false };
};

template<typename TearOffType>
class SVGAnimatedAttribute : public SVGPropertyAttribute<typename TearOffType::ContentType> {
public:
    using PropertyTearOffType = TearOffType;
    using PropertyType = typename PropertyTearOffType::ContentType;
    using Base = SVGPropertyAttribute<PropertyType>;
    using Base::m_property;
    using Base::m_shouldSynchronize;

    SVGAnimatedAttribute() = default;

    template<typename... Arguments>
    SVGAnimatedAttribute(Arguments&&... arguments)
        : Base(std::forward<Arguments>(arguments)...)
    {
    }

    const PropertyType& currentValue(const SVGAttributeOwnerProxy& attributeOwnerProxy) const
    {
        if (auto wrapper = attributeOwnerProxy.lookupAnimatedProperty(*this)) {
            if (wrapper->isAnimating())
                return static_pointer_cast<PropertyTearOffType>(wrapper)->currentAnimatedValue();
        }
        return m_property;
    }

    RefPtr<PropertyTearOffType> animatedProperty(const SVGAttributeOwnerProxy& attributeOwnerProxy)
    {
        m_shouldSynchronize = true;
        if (auto wrapper = attributeOwnerProxy.lookupOrCreateAnimatedProperty(*this))
            return static_pointer_cast<PropertyTearOffType>(wrapper);
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }
};

template<typename TearOffType>
class SVGAnimatedAttributeList : public SVGAnimatedAttribute<TearOffType> {
public:
    using PropertyTearOffType = TearOffType;
    using PropertyType = typename PropertyTearOffType::ContentType;
    using Base = SVGAnimatedAttribute<PropertyTearOffType>;

    SVGAnimatedAttributeList() = default;

    template<typename... Arguments>
    SVGAnimatedAttributeList(Arguments&&... arguments)
        : Base(std::forward<Arguments>(arguments)...)
    {
    }

    void detachAnimatedListWrappers(const SVGAttributeOwnerProxy& attributeOwnerProxy, unsigned newListSize)
    {
        if (auto wrapper = attributeOwnerProxy.lookupAnimatedProperty(*this))
            static_pointer_cast<PropertyTearOffType>(wrapper)->detachListWrappers(newListSize);
    }
};

}
