/*
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

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

#ifndef SVGAnimatedTemplate_h
#define SVGAnimatedTemplate_h

#if ENABLE(SVG)
#include "SVGAnimatedPropertyTraits.h"
#include <wtf/HashMap.h>

namespace WebCore {
   
    class SVGAngle;
    class SVGElement;
    class SVGLengthList;
    class SVGNumberList;
    class SVGPreserveAspectRatio;
    class SVGTransformList;
    class String;
    class QualifiedName;

    struct SVGAnimatedTypeWrapperKey {            
        // Empty value
        SVGAnimatedTypeWrapperKey()
            : element(0)
            , attributeName(0)
        { }

        // Deleted value
        SVGAnimatedTypeWrapperKey(WTF::HashTableDeletedValueType)
            : element(reinterpret_cast<SVGElement*>(-1))
        {
        }

        bool isHashTableDeletedValue() const
        {
            return element == reinterpret_cast<SVGElement*>(-1);
        }

        SVGAnimatedTypeWrapperKey(const SVGElement* _element, const AtomicString& _attributeName)
            : element(_element)
            , attributeName(_attributeName.impl())
        {
            ASSERT(element);
            ASSERT(attributeName);
        }

        bool operator==(const SVGAnimatedTypeWrapperKey& other) const
        {
            return element == other.element && attributeName == other.attributeName;
        }

        const SVGElement* element;
        AtomicStringImpl* attributeName;
    };
    
    struct SVGAnimatedTypeWrapperKeyHash {
        static unsigned hash(const SVGAnimatedTypeWrapperKey& key)
        {
            return StringImpl::computeHash(reinterpret_cast<const UChar*>(&key), sizeof(SVGAnimatedTypeWrapperKey) / sizeof(UChar));
        }

        static bool equal(const SVGAnimatedTypeWrapperKey& a, const SVGAnimatedTypeWrapperKey& b)
        {
            return a == b;
        }

        static const bool safeToCompareToEmptyOrDeleted = true;
    };

    struct SVGAnimatedTypeWrapperKeyHashTraits : WTF::GenericHashTraits<SVGAnimatedTypeWrapperKey> {
        static const bool emptyValueIsZero = true;

        static void constructDeletedValue(SVGAnimatedTypeWrapperKey& slot)
        {
            new (&slot) SVGAnimatedTypeWrapperKey(WTF::HashTableDeletedValue);
        }

        static bool isDeletedValue(const SVGAnimatedTypeWrapperKey& value)
        {
            return value.isHashTableDeletedValue();
        }
    };
 
    template<typename AnimatedType>
    class SVGAnimatedTemplate : public RefCounted<SVGAnimatedTemplate<AnimatedType> > {
    public:
        typedef typename SVGAnimatedPropertyTraits<AnimatedType>::PassType PassType;
        typedef typename SVGAnimatedPropertyTraits<AnimatedType>::ReturnType ReturnType;

        virtual ~SVGAnimatedTemplate() { forgetWrapper(this); }

        virtual ReturnType baseVal() const = 0;
        virtual void setBaseVal(PassType) = 0;

        virtual ReturnType animVal() const = 0;
        virtual void setAnimVal(PassType) = 0;

        virtual const QualifiedName& associatedAttributeName() const = 0;

        typedef HashMap<SVGAnimatedTypeWrapperKey, SVGAnimatedTemplate<AnimatedType>*, SVGAnimatedTypeWrapperKeyHash, SVGAnimatedTypeWrapperKeyHashTraits > ElementToWrapperMap;
        typedef typename ElementToWrapperMap::const_iterator ElementToWrapperMapIterator;

        static ElementToWrapperMap* wrapperCache()
        {
            static ElementToWrapperMap* s_wrapperCache = new ElementToWrapperMap;                
            return s_wrapperCache;
        }

        static void forgetWrapper(SVGAnimatedTemplate<AnimatedType>* wrapper)
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
    class SVGAnimatedProperty;

    template<typename AnimatedType, typename AnimatedTearOff>
    PassRefPtr<AnimatedTearOff> lookupOrCreateWrapper(SVGElement* element, SVGAnimatedProperty<AnimatedType>& creator, const QualifiedName& attrName)
    {
        SVGAnimatedTypeWrapperKey key(element, attrName.localName());
        RefPtr<AnimatedTearOff> wrapper = static_cast<AnimatedTearOff*>(AnimatedTearOff::wrapperCache()->get(key));

        if (!wrapper) {
            wrapper = AnimatedTearOff::create(creator, element);
            AnimatedTearOff::wrapperCache()->set(key, wrapper.get());
        }

        return wrapper.release();
    }

    // Common type definitions, to ease IDL generation.
    typedef SVGAnimatedTemplate<SVGAngle> SVGAnimatedAngle;
    typedef SVGAnimatedTemplate<bool> SVGAnimatedBoolean;
    typedef SVGAnimatedTemplate<int> SVGAnimatedEnumeration;
    typedef SVGAnimatedTemplate<long> SVGAnimatedInteger;
    typedef SVGAnimatedTemplate<SVGLength> SVGAnimatedLength;
    typedef SVGAnimatedTemplate<SVGLengthList*> SVGAnimatedLengthList;
    typedef SVGAnimatedTemplate<float> SVGAnimatedNumber;
    typedef SVGAnimatedTemplate<SVGNumberList*> SVGAnimatedNumberList; 
    typedef SVGAnimatedTemplate<SVGPreserveAspectRatio> SVGAnimatedPreserveAspectRatio;
    typedef SVGAnimatedTemplate<FloatRect> SVGAnimatedRect;
    typedef SVGAnimatedTemplate<String> SVGAnimatedString;
    typedef SVGAnimatedTemplate<SVGTransformList*> SVGAnimatedTransformList;

}

#endif
#endif
