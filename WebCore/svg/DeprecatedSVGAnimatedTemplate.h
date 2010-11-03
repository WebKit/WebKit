/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
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

#ifndef DeprecatedSVGAnimatedTemplate_h
#define DeprecatedSVGAnimatedTemplate_h

#if ENABLE(SVG)
#include "DeprecatedSVGAnimatedPropertyTraits.h"
#include "QualifiedName.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>

namespace WebCore {
   
    class SVGElement;
    class SVGTransformList;

    struct DeprecatedSVGAnimatedTypeWrapperKey {            
        // Empty value
        DeprecatedSVGAnimatedTypeWrapperKey()
            : element(0)
            , attributeName(0)
        { }

        // Deleted value
        DeprecatedSVGAnimatedTypeWrapperKey(WTF::HashTableDeletedValueType)
            : element(reinterpret_cast<SVGElement*>(-1))
        {
        }

        bool isHashTableDeletedValue() const
        {
            return element == reinterpret_cast<SVGElement*>(-1);
        }

        DeprecatedSVGAnimatedTypeWrapperKey(const SVGElement* _element, const AtomicString& _attributeName)
            : element(_element)
            , attributeName(_attributeName.impl())
        {
            ASSERT(element);
            ASSERT(attributeName);
        }

        bool operator==(const DeprecatedSVGAnimatedTypeWrapperKey& other) const
        {
            return element == other.element && attributeName == other.attributeName;
        }

        const SVGElement* element;
        AtomicStringImpl* attributeName;
    };
    
    struct DeprecatedSVGAnimatedTypeWrapperKeyHash {
        static unsigned hash(const DeprecatedSVGAnimatedTypeWrapperKey& key)
        {
            return WTF::StringHasher::createBlobHash<sizeof(DeprecatedSVGAnimatedTypeWrapperKey)>(&key);
        }

        static bool equal(const DeprecatedSVGAnimatedTypeWrapperKey& a, const DeprecatedSVGAnimatedTypeWrapperKey& b)
        {
            return a == b;
        }

        static const bool safeToCompareToEmptyOrDeleted = true;
    };

    struct DeprecatedSVGAnimatedTypeWrapperKeyHashTraits : WTF::GenericHashTraits<DeprecatedSVGAnimatedTypeWrapperKey> {
        static const bool emptyValueIsZero = true;

        static void constructDeletedValue(DeprecatedSVGAnimatedTypeWrapperKey& slot)
        {
            new (&slot) DeprecatedSVGAnimatedTypeWrapperKey(WTF::HashTableDeletedValue);
        }

        static bool isDeletedValue(const DeprecatedSVGAnimatedTypeWrapperKey& value)
        {
            return value.isHashTableDeletedValue();
        }
    };
 
    template<typename AnimatedType>
    class DeprecatedSVGAnimatedTemplate : public RefCounted<DeprecatedSVGAnimatedTemplate<AnimatedType> > {
    public:
        typedef typename DeprecatedSVGAnimatedPropertyTraits<AnimatedType>::PassType PassType;
        typedef typename DeprecatedSVGAnimatedPropertyTraits<AnimatedType>::ReturnType ReturnType;

        virtual ~DeprecatedSVGAnimatedTemplate() { forgetWrapper(this); }

        virtual ReturnType baseVal() const = 0;
        virtual void setBaseVal(PassType) = 0;

        virtual ReturnType animVal() const = 0;
        virtual void setAnimVal(PassType) = 0;

        virtual const QualifiedName& associatedAttributeName() const = 0;

        typedef HashMap<DeprecatedSVGAnimatedTypeWrapperKey, DeprecatedSVGAnimatedTemplate<AnimatedType>*, DeprecatedSVGAnimatedTypeWrapperKeyHash, DeprecatedSVGAnimatedTypeWrapperKeyHashTraits > ElementToWrapperMap;
        typedef typename ElementToWrapperMap::const_iterator ElementToWrapperMapIterator;

        static ElementToWrapperMap* wrapperCache()
        {
            static ElementToWrapperMap* s_wrapperCache = new ElementToWrapperMap;                
            return s_wrapperCache;
        }

        static void forgetWrapper(DeprecatedSVGAnimatedTemplate<AnimatedType>* wrapper)
        {
            ElementToWrapperMap* cache = wrapperCache();
            ElementToWrapperMapIterator itr = cache->begin();
            ElementToWrapperMapIterator end = cache->end();
            for (; itr != end; ++itr) {
                if (itr->second == wrapper) {
                    cache->remove(itr->first);
                    break;
                }
            }
        }
    };

    template<typename AnimatedType>
    class DeprecatedSVGAnimatedProperty;

    template<typename AnimatedType, typename AnimatedTearOff>
    PassRefPtr<AnimatedTearOff> lookupOrCreateWrapper(SVGElement* element, DeprecatedSVGAnimatedProperty<AnimatedType>& creator, const QualifiedName& attrName)
    {
        DeprecatedSVGAnimatedTypeWrapperKey key(element, attrName.localName());
        RefPtr<AnimatedTearOff> wrapper = static_cast<AnimatedTearOff*>(AnimatedTearOff::wrapperCache()->get(key));

        if (!wrapper) {
            wrapper = AnimatedTearOff::create(creator, element);
            AnimatedTearOff::wrapperCache()->set(key, wrapper.get());
        }

        return wrapper.release();
    }

    // Common type definitions, to ease IDL generation.
    typedef DeprecatedSVGAnimatedTemplate<SVGTransformList*> SVGAnimatedTransformList;

}

#endif
#endif
