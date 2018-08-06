/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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

#include "QualifiedName.h"
#include "SVGAnimatedPropertyDescription.h"
#include "SVGAnimatedPropertyType.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class SVGElement;
class SVGProperty;

class SVGAnimatedProperty : public RefCounted<SVGAnimatedProperty> {
public:
    virtual ~SVGAnimatedProperty();
    virtual bool isAnimating() const { return false; }
    virtual bool isAnimatedListTearOff() const { return false; }

    SVGElement* contextElement() const { return m_contextElement.get(); }
    const QualifiedName& attributeName() const { return m_attributeName; }
    AnimatedPropertyType animatedPropertyType() const { return m_animatedPropertyType; }
    bool isReadOnly() const { return m_isReadOnly; }
    void setIsReadOnly() { m_isReadOnly = true; }

    void commitChange();

    template<typename TearOffType, typename PropertyType, AnimatedPropertyType animatedType>
    static RefPtr<SVGAnimatedProperty> lookupOrCreateAnimatedProperty(SVGElement& element, const QualifiedName& attributeName, const AtomicString& identifier, PropertyType& property, AnimatedPropertyState animatedState)
    {
        SVGAnimatedPropertyDescription key(&element, identifier);

        auto result = animatedPropertyCache().add(key, nullptr);
        if (!result.isNewEntry)
            return result.iterator->value;

        auto wrapper = TearOffType::create(&element, attributeName, animatedType, property);
        if (animatedState == PropertyIsReadOnly)
            wrapper->setIsReadOnly();

        // Cache the raw pointer but return a RefPtr<>. This will break the cyclic reference
        // between SVGAnimatedProperty and SVGElement once the property pointer is not needed.
        result.iterator->value = wrapper.ptr();
        return static_reference_cast<SVGAnimatedProperty>(wrapper);
    }

    static RefPtr<SVGAnimatedProperty> lookupAnimatedProperty(const SVGElement& element, const AtomicString& identifier)
    {
        SVGAnimatedPropertyDescription key(const_cast<SVGElement*>(&element), identifier);
        return animatedPropertyCache().get(key);
    }

protected:
    SVGAnimatedProperty(SVGElement*, const QualifiedName&, AnimatedPropertyType);

private:
    // Caching facilities.
    using Cache = HashMap<SVGAnimatedPropertyDescription, SVGAnimatedProperty*, SVGAnimatedPropertyDescriptionHash, SVGAnimatedPropertyDescriptionHashTraits>;
    static Cache& animatedPropertyCache()
    {
        static NeverDestroyed<Cache> cache;
        return cache;
    }

    RefPtr<SVGElement> m_contextElement;
    const QualifiedName& m_attributeName;
    AnimatedPropertyType m_animatedPropertyType;

protected:
    bool m_isReadOnly { false };
};

} // namespace WebCore
