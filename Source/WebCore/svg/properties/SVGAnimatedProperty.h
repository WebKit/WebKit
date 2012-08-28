/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#ifndef SVGAnimatedProperty_h
#define SVGAnimatedProperty_h

#if ENABLE(SVG)
#include "SVGAnimatedPropertyDescription.h"
#include "SVGElement.h"
#include "SVGPropertyInfo.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class SVGElement;

class SVGAnimatedProperty : public RefCounted<SVGAnimatedProperty> {
public:
    SVGElement* contextElement() const { return m_contextElement.get(); }
    const QualifiedName& attributeName() const { return m_attributeName; }
    AnimatedPropertyType animatedPropertyType() const { return m_animatedPropertyType; }
    bool isAnimating() const { return m_isAnimating; }
    bool isReadOnly() const { return m_isReadOnly; }
    void setIsReadOnly() { m_isReadOnly = true; }

    void commitChange()
    {
        ASSERT(m_contextElement);
        ASSERT(!m_contextElement->m_deletionHasBegun);
        m_contextElement->invalidateSVGAttributes();
        m_contextElement->svgAttributeChanged(m_attributeName);
    }

    virtual bool isAnimatedListTearOff() const { return false; }

    // Caching facilities.
    typedef HashMap<SVGAnimatedPropertyDescription, SVGAnimatedProperty*, SVGAnimatedPropertyDescriptionHash, SVGAnimatedPropertyDescriptionHashTraits> Cache;

    virtual ~SVGAnimatedProperty()
    {
        // Remove wrapper from cache.
        Cache* cache = animatedPropertyCache();
        const Cache::const_iterator end = cache->end();
        for (Cache::const_iterator it = cache->begin(); it != end; ++it) {
            if (it->value == this) {
                cache->remove(it->key);
                break;
            }
        }

        // Assure that animationEnded() was called, if animationStarted() was called before.
        ASSERT(!m_isAnimating);
    }

    template<typename OwnerType, typename TearOffType, typename PropertyType>
    static PassRefPtr<TearOffType> lookupOrCreateWrapper(OwnerType* element, const SVGPropertyInfo* info, PropertyType& property)
    {
        ASSERT(info);
        SVGAnimatedPropertyDescription key(element, info->propertyIdentifier);
        RefPtr<SVGAnimatedProperty> wrapper = animatedPropertyCache()->get(key);
        if (!wrapper) {
            wrapper = TearOffType::create(element, info->attributeName, info->animatedPropertyType, property);
            if (info->animatedPropertyState == PropertyIsReadOnly)
                wrapper->setIsReadOnly();
            animatedPropertyCache()->set(key, wrapper.get());
        }
        return static_pointer_cast<TearOffType>(wrapper);
    }

    template<typename OwnerType, typename TearOffType>
    static TearOffType* lookupWrapper(OwnerType* element, const SVGPropertyInfo* info)
    {
        ASSERT(info);
        SVGAnimatedPropertyDescription key(element, info->propertyIdentifier);
        return static_cast<TearOffType*>(animatedPropertyCache()->get(key));
    }

    template<typename OwnerType, typename TearOffType>
    static TearOffType* lookupWrapper(const OwnerType* element, const SVGPropertyInfo* info)
    {
        return lookupWrapper<OwnerType, TearOffType>(const_cast<OwnerType*>(element), info);
    }

protected:
    SVGAnimatedProperty(SVGElement* contextElement, const QualifiedName& attributeName, AnimatedPropertyType animatedPropertyType)
        : m_contextElement(contextElement)
        , m_attributeName(attributeName)
        , m_animatedPropertyType(animatedPropertyType)
        , m_isAnimating(false)
        , m_isReadOnly(false)
    {
    }

private:
    static Cache* animatedPropertyCache()
    {
        static Cache* s_cache = new Cache;                
        return s_cache;
    }

    RefPtr<SVGElement> m_contextElement;
    const QualifiedName& m_attributeName;
    AnimatedPropertyType m_animatedPropertyType;

protected:
    bool m_isAnimating;
    bool m_isReadOnly;
};

}

#endif // ENABLE(SVG)
#endif // SVGAnimatedProperty_h
