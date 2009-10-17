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
#include "AtomicString.h"
#include "FloatRect.h"
#include "SVGLength.h"
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
 
    template<typename BareType>
    class SVGAnimatedTemplate : public RefCounted<SVGAnimatedTemplate<BareType> > {
    public:
        virtual ~SVGAnimatedTemplate() { forgetWrapper(this); }

        virtual BareType baseVal() const = 0;
        virtual void setBaseVal(BareType) = 0;

        virtual BareType animVal() const = 0;
        virtual void setAnimVal(BareType) = 0;

        typedef HashMap<SVGAnimatedTypeWrapperKey, SVGAnimatedTemplate<BareType>*, SVGAnimatedTypeWrapperKeyHash, SVGAnimatedTypeWrapperKeyHashTraits > ElementToWrapperMap;
        typedef typename ElementToWrapperMap::const_iterator ElementToWrapperMapIterator;

        static ElementToWrapperMap* wrapperCache()
        {
            static ElementToWrapperMap* s_wrapperCache = new ElementToWrapperMap;                
            return s_wrapperCache;
        }

        static void forgetWrapper(SVGAnimatedTemplate<BareType>* wrapper)
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

        const QualifiedName& associatedAttributeName() const { return m_associatedAttributeName; }

    protected:
        SVGAnimatedTemplate(const QualifiedName& attributeName)
            : m_associatedAttributeName(attributeName)
        {
        }

    private:
        const QualifiedName& m_associatedAttributeName;
    };

    template<typename OwnerTypeArg, typename AnimatedTypeArg, const char* TagName, const char* PropertyName>
    class SVGAnimatedProperty;

    template<typename OwnerType, typename AnimatedType, const char* TagName, const char* PropertyName, typename Type, typename OwnerElement> 
    PassRefPtr<Type> lookupOrCreateWrapper(const SVGAnimatedProperty<OwnerType, AnimatedType, TagName, PropertyName>& creator,
                                           const OwnerElement* element, const QualifiedName& attrName, const AtomicString& attrIdentifier)
    {
        SVGAnimatedTypeWrapperKey key(element, attrIdentifier);
        RefPtr<Type> wrapper = static_cast<Type*>(Type::wrapperCache()->get(key));

        if (!wrapper) {
            wrapper = Type::create(creator, element, attrName);
            element->propertyController().setPropertyNeedsSynchronization(attrName);
            Type::wrapperCache()->set(key, wrapper.get());
        }

        return wrapper.release();
    }

    // Default implementation for pointer types
    template<typename Type>
    struct SVGAnimatedTypeValue : Noncopyable {
        typedef RefPtr<Type> StorableType;
        typedef Type* DecoratedType;

        static Type null() { return 0; }
        static AtomicString toString(Type type) { return type ? AtomicString(type->valueAsString()) : nullAtom; }
    };

    template<>
    struct SVGAnimatedTypeValue<bool> : Noncopyable {
        typedef bool StorableType;
        typedef bool DecoratedType;

        static bool null() { return false; }
        static AtomicString toString(bool type) { return type ? "true" : "false"; }
    };

    template<>
    struct SVGAnimatedTypeValue<int> : Noncopyable {
        typedef int StorableType;
        typedef int DecoratedType;

        static int null() { return 0; }
        static AtomicString toString(int type) { return String::number(type); }
    };

    template<>
    struct SVGAnimatedTypeValue<long> : Noncopyable {
        typedef long StorableType;
        typedef long DecoratedType;

        static long null() { return 0l; }
        static AtomicString toString(long type) { return String::number(type); }
    };

    template<>
    struct SVGAnimatedTypeValue<SVGLength> : Noncopyable {
        typedef SVGLength StorableType;
        typedef SVGLength DecoratedType;

        static SVGLength null() { return SVGLength(); }
        static AtomicString toString(const SVGLength& type) { return type.valueAsString(); }
    };

    template<>
    struct SVGAnimatedTypeValue<float> : Noncopyable {
        typedef float StorableType;
        typedef float DecoratedType;

        static float null() { return 0.0f; }
        static AtomicString toString(float type) { return String::number(type); }
    };

    template<>
    struct SVGAnimatedTypeValue<FloatRect> : Noncopyable {
        typedef FloatRect StorableType;
        typedef FloatRect DecoratedType;

        static FloatRect null() { return FloatRect(); }
        static AtomicString toString(const FloatRect& type) { return String::format("%f %f %f %f", type.x(), type.y(), type.width(), type.height()); }
    };

    template<>
    struct SVGAnimatedTypeValue<String> : Noncopyable {
        typedef String StorableType;
        typedef String DecoratedType;

        static String null() { return String(); }
        static AtomicString toString(const String& type) { return type; }
    };

    // Common type definitions, to ease IDL generation.
    typedef SVGAnimatedTemplate<SVGAngle*> SVGAnimatedAngle;
    typedef SVGAnimatedTemplate<bool> SVGAnimatedBoolean;
    typedef SVGAnimatedTemplate<int> SVGAnimatedEnumeration;
    typedef SVGAnimatedTemplate<long> SVGAnimatedInteger;
    typedef SVGAnimatedTemplate<SVGLength> SVGAnimatedLength;
    typedef SVGAnimatedTemplate<SVGLengthList*> SVGAnimatedLengthList;
    typedef SVGAnimatedTemplate<float> SVGAnimatedNumber;
    typedef SVGAnimatedTemplate<SVGNumberList*> SVGAnimatedNumberList; 
    typedef SVGAnimatedTemplate<SVGPreserveAspectRatio*> SVGAnimatedPreserveAspectRatio;
    typedef SVGAnimatedTemplate<FloatRect> SVGAnimatedRect;
    typedef SVGAnimatedTemplate<String> SVGAnimatedString;
    typedef SVGAnimatedTemplate<SVGTransformList*> SVGAnimatedTransformList;

}

#endif // ENABLE(SVG)
#endif // SVGAnimatedTemplate_h
