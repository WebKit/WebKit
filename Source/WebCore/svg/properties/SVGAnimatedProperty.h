/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "SVGAnimatedPropertyDescription.h"
#include "SVGPropertyInfo.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class SVGElement;
class SVGProperty;

class SVGAnimatedProperty : public RefCounted<SVGAnimatedProperty> {
public:
    SVGElement* contextElement() const { return m_contextElement.get(); }
    const QualifiedName& attributeName() const { return m_attributeName; }
    AnimatedPropertyType animatedPropertyType() const { return m_animatedPropertyType; }
    bool isReadOnly() const { return m_isReadOnly; }
    void setIsReadOnly() { m_isReadOnly = true; }

    void commitChange();

    virtual bool isAnimating() const { return false; }
    virtual bool isAnimatedListTearOff() const { return false; }
    virtual void propertyWillBeDeleted(const SVGProperty&) { }

    // Caching facilities.
    typedef HashMap<SVGAnimatedPropertyDescription, SVGAnimatedProperty*, SVGAnimatedPropertyDescriptionHash, SVGAnimatedPropertyDescriptionHashTraits> Cache;

    virtual ~SVGAnimatedProperty();

    template<typename OwnerType, typename TearOffType, typename PropertyType>
    static Ref<TearOffType> lookupOrCreateWrapper(OwnerType* element, const SVGPropertyInfo* info, PropertyType& property)
    {
        ASSERT(info);
        SVGAnimatedPropertyDescription key(element, info->propertyIdentifier);

        auto result = animatedPropertyCache()->add(key, nullptr);
        if (!result.isNewEntry)
            return static_cast<TearOffType&>(*result.iterator->value);

        Ref<SVGAnimatedProperty> wrapper = TearOffType::create(element, info->attributeName, info->animatedPropertyType, property);
        if (info->animatedPropertyState == PropertyIsReadOnly)
            wrapper->setIsReadOnly();

        // Cache the raw pointer but return a Ref<>. This will break the cyclic reference
        // between SVGAnimatedProperty and SVGElement once the property pointer is not needed.
        result.iterator->value = wrapper.ptr();
        return static_reference_cast<TearOffType>(wrapper);
    }

    template<typename OwnerType, typename TearOffType>
    static RefPtr<TearOffType> lookupWrapper(OwnerType* element, const SVGPropertyInfo* info)
    {
        ASSERT(info);
        SVGAnimatedPropertyDescription key(element, info->propertyIdentifier);
        return static_cast<TearOffType*>(animatedPropertyCache()->get(key));
    }

    template<typename OwnerType, typename TearOffType>
    static RefPtr<TearOffType> lookupWrapper(const OwnerType* element, const SVGPropertyInfo* info)
    {
        return lookupWrapper<OwnerType, TearOffType>(const_cast<OwnerType*>(element), info);
    }

protected:
    SVGAnimatedProperty(SVGElement*, const QualifiedName&, AnimatedPropertyType);

private:
    static Cache* animatedPropertyCache();

    RefPtr<SVGElement> m_contextElement;
    const QualifiedName& m_attributeName;
    AnimatedPropertyType m_animatedPropertyType;

protected:
    bool m_isReadOnly;
};

} // namespace WebCore
