/*
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

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
#include "Attribute.h"
#include "SVGAngle.h"
#include "SVGLength.h"
#include "SVGLengthList.h"
#include "SVGNumberList.h"
#include "SVGTransformList.h"
#include "SVGPreserveAspectRatio.h"

#include <wtf/RefCounted.h>

namespace WebCore {

    class SVGElement;

    struct SVGAnimatedTypeWrapperKey {            
        // Empty value
        SVGAnimatedTypeWrapperKey()
            : element(0)
            , attributeName(0)
        { }
        
        // Deleted value
        explicit SVGAnimatedTypeWrapperKey(bool)
            : element(reinterpret_cast<SVGElement*>(-1))
            , attributeName(0)
        { }
        
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
        static const bool needsDestruction = false;
        
        static const SVGAnimatedTypeWrapperKey& deletedValue()
        {
            static SVGAnimatedTypeWrapperKey deletedKey(true);
            return deletedKey;
        }
        
        static const SVGAnimatedTypeWrapperKey& emptyValue()
        {
            static SVGAnimatedTypeWrapperKey emptyKey;
            return emptyKey;
        }
    };
    
    template<typename BareType>
    class SVGAnimatedTemplate : public RefCounted<SVGAnimatedTemplate<BareType> > {
    public:
        SVGAnimatedTemplate(const QualifiedName& attributeName)
            : RefCounted<SVGAnimatedTemplate<BareType> >(0)
            , m_associatedAttributeName(attributeName)
        {
        }

        virtual ~SVGAnimatedTemplate() { forgetWrapper(this); }

        virtual BareType baseVal() const = 0;
        virtual void setBaseVal(BareType newBaseVal) = 0;

        virtual BareType animVal() const = 0;
        virtual void setAnimVal(BareType newAnimVal) = 0;

        virtual AtomicString toString() const { ASSERT_NOT_REACHED(); return AtomicString(); }

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

    private:
        const QualifiedName& m_associatedAttributeName;
    };

    template <class Type, class SVGElementSubClass>
    Type* lookupOrCreateWrapper(SVGElementSubClass* element, const QualifiedName& domAttrName,
                                const AtomicString& attrIdentifier, void (SVGElement::*synchronizer)()) {
        SVGAnimatedTypeWrapperKey key(element, attrIdentifier);
        Type* wrapper = static_cast<Type*>(Type::wrapperCache()->get(key));
        if (wrapper)
            return wrapper;

        wrapper = new Type(element, domAttrName);
        element->addSVGPropertySynchronizer(domAttrName, synchronizer);

        Type::wrapperCache()->set(key, wrapper);
        return wrapper;
    }

    // Define SVG 1.1 standard animated properties (these SVGAnimated* classes area slo able to convert themselves to strings)
    // Note: If we'd just specialize the SVGAnimatedTemplate<> we'd have to duplicate a lot of code above. Hence choosing
    // the solution, to inherit from the SVGAnimatedTemplate<> classes, and implementing the virtual toString() function.
    class SVGAnimatedAngle : public SVGAnimatedTemplate<SVGAngle*> {
    public:
        SVGAnimatedAngle(const QualifiedName& attributeName) : SVGAnimatedTemplate<SVGAngle*>(attributeName) { }

        virtual AtomicString toString() const
        {
            if (SVGAngle* angle = baseVal())
                return angle->valueAsString();

            return nullAtom;
        }
    };

    class SVGAnimatedBoolean : public SVGAnimatedTemplate<bool> {
    public:
        SVGAnimatedBoolean(const QualifiedName& attributeName) : SVGAnimatedTemplate<bool>(attributeName) { }

        virtual AtomicString toString() const
        {
            return baseVal() ? "true" : "false";
        }
    };

    class SVGAnimatedEnumeration : public SVGAnimatedTemplate<int> {
    public:
        SVGAnimatedEnumeration(const QualifiedName& attributeName) : SVGAnimatedTemplate<int>(attributeName) { }

        virtual AtomicString toString() const
        {
            return String::number(baseVal());
        }
    };

    class SVGAnimatedInteger : public SVGAnimatedTemplate<long> {
    public:
        SVGAnimatedInteger(const QualifiedName& attributeName) : SVGAnimatedTemplate<long>(attributeName) { }

        virtual AtomicString toString() const
        {
            return String::number(baseVal());
        }
    };

    class SVGAnimatedLength : public SVGAnimatedTemplate<SVGLength> {
    public:
        SVGAnimatedLength(const QualifiedName& attributeName) : SVGAnimatedTemplate<SVGLength>(attributeName) { }

        virtual AtomicString toString() const
        {
            return baseVal().valueAsString();
        }
    };

    class SVGAnimatedLengthList : public SVGAnimatedTemplate<SVGLengthList*> {
    public:
        SVGAnimatedLengthList(const QualifiedName& attributeName) : SVGAnimatedTemplate<SVGLengthList*>(attributeName) { }

        virtual AtomicString toString() const
        {
            if (SVGLengthList* list = baseVal())
                return list->valueAsString();

            return nullAtom;
        }
    };

    class SVGAnimatedNumber : public SVGAnimatedTemplate<float> {
    public:
        SVGAnimatedNumber(const QualifiedName& attributeName) : SVGAnimatedTemplate<float>(attributeName) { }

        virtual AtomicString toString() const
        {
            return String::number(baseVal());
        }
    };

    class SVGAnimatedNumberList : public SVGAnimatedTemplate<SVGNumberList*> {
    public:
        SVGAnimatedNumberList(const QualifiedName& attributeName) : SVGAnimatedTemplate<SVGNumberList*>(attributeName) { }

        virtual AtomicString toString() const
        {
            if (SVGNumberList* list = baseVal())
                return list->valueAsString();

            return nullAtom;
        }
    };

    class SVGAnimatedPreserveAspectRatio : public SVGAnimatedTemplate<SVGPreserveAspectRatio*> {
    public:
        SVGAnimatedPreserveAspectRatio(const QualifiedName& attributeName) : SVGAnimatedTemplate<SVGPreserveAspectRatio*>(attributeName) { }

        virtual AtomicString toString() const
        {
            if (SVGPreserveAspectRatio* ratio = baseVal())
                return ratio->valueAsString();

            return nullAtom;
        }
    };

    class SVGAnimatedRect : public SVGAnimatedTemplate<FloatRect> {
    public:
        SVGAnimatedRect(const QualifiedName& attributeName) : SVGAnimatedTemplate<FloatRect>(attributeName) { }

        virtual AtomicString toString() const
        {
            FloatRect rect = baseVal();
            return String::format("%f %f %f %f", rect.x(), rect.y(), rect.width(), rect.height());
        }
    };

    class SVGAnimatedString : public SVGAnimatedTemplate<String> {
    public:
        SVGAnimatedString(const QualifiedName& attributeName) : SVGAnimatedTemplate<String>(attributeName) { }

        virtual AtomicString toString() const
        {
            return baseVal();
        }
    };

    class SVGAnimatedTransformList : public SVGAnimatedTemplate<SVGTransformList*> {
    public:
        SVGAnimatedTransformList(const QualifiedName& attributeName) : SVGAnimatedTemplate<SVGTransformList*>(attributeName) { }

        virtual AtomicString toString() const
        {
            if (SVGTransformList* list = baseVal())
                return list->valueAsString();

            return nullAtom;
        }
    };

    // Helper for ANIMATED_PROPERTY* macros, whose helper classes SVGAnimatedTemplate##UpperProperty.. can't
    // inherit from SVGAnimatedTemplate<xxx> as they rely on a working toString() implementation. So instead
    // create a new helper type SVGAnimatedType, which gets specialized for the support datatypes and in turn inherits
    // from the SVGAnimatedTEmplate<xxx> macros. Looks a bit complicated, but makes sense if you check the macros.
    template<typename T>
    class SVGAnimatedType : public SVGAnimatedTemplate<T> {
    public:
        SVGAnimatedType(const QualifiedName& attributeName) : SVGAnimatedTemplate<T>(attributeName) { ASSERT_NOT_REACHED(); }
    };

#define ADD_ANIMATED_TYPE_HELPER(DataType, SuperClass) \
    template<> \
    class SVGAnimatedType<DataType> : public SuperClass { \
    public: \
        SVGAnimatedType(const QualifiedName& attributeName) : SuperClass(attributeName) { } \
    };

    ADD_ANIMATED_TYPE_HELPER(SVGAngle*, SVGAnimatedAngle)
    ADD_ANIMATED_TYPE_HELPER(bool, SVGAnimatedBoolean)
    ADD_ANIMATED_TYPE_HELPER(int, SVGAnimatedEnumeration)
    ADD_ANIMATED_TYPE_HELPER(long, SVGAnimatedInteger)
    ADD_ANIMATED_TYPE_HELPER(SVGLength, SVGAnimatedLength)
    ADD_ANIMATED_TYPE_HELPER(SVGLengthList*, SVGAnimatedLengthList)
    ADD_ANIMATED_TYPE_HELPER(float, SVGAnimatedNumber)
    ADD_ANIMATED_TYPE_HELPER(SVGNumberList*, SVGAnimatedNumberList)
    ADD_ANIMATED_TYPE_HELPER(SVGPreserveAspectRatio*, SVGAnimatedPreserveAspectRatio)
    ADD_ANIMATED_TYPE_HELPER(FloatRect, SVGAnimatedRect)
    ADD_ANIMATED_TYPE_HELPER(String, SVGAnimatedString)
    ADD_ANIMATED_TYPE_HELPER(SVGTransformList*, SVGAnimatedTransformList)

}

#endif // ENABLE(SVG)
#endif // SVGAnimatedTemplate_h
