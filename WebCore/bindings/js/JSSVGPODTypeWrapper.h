/*
 * Copyright (C) 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef JSSVGPODTypeWrapper_h
#define JSSVGPODTypeWrapper_h

#if ENABLE(SVG)
#include "JSSVGContextCache.h"
#include "SVGElement.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

class DOMObject;

template<typename PODType>
class JSSVGPODTypeWrapper : public RefCounted<JSSVGPODTypeWrapper<PODType> > {
public:
    virtual ~JSSVGPODTypeWrapper() { }

    virtual operator PODType() = 0;
    virtual void commitChange(PODType, DOMObject*) = 0;
};

// This file contains JS wrapper objects for SVG datatypes, that are passed around by value
// in WebCore/svg (aka. 'POD types'). For instance SVGMatrix is mapped to AffineTransform, and
// passed around as const reference. SVG DOM demands these objects to be "live", changes to any
// of the writable attributes of SVGMatrix need to be reflected in the object which exposed the
// SVGMatrix object (i.e. 'someElement.transform.matrix.a = 50.0', in that case 'SVGTransform').
// The SVGTransform class stores its "AffineTransform m_matrix" object on the stack. If it would
// be stored as pointer we could just build an auto-generated JSSVG* wrapper object around it
// and all changes to that object would automatically affect the AffineTransform* object stored
// in the SVGTransform object. For the sake of efficiency and memory we don't pass around any
// primitive values as pointers, so a custom JS wrapper object is needed for all SVG types, that
// are internally represented by POD types (SVGRect <-> FloatRect, SVGPoint <-> FloatPoint, ...).
// Those custom wrappers are called JSSVGPODTypeWrapper and are capable of updating the POD types
// by taking function pointers to the getter & setter functions of the "creator object", the object
// which exposed a SVG POD type. For example, the JSSVGPODTypeWrapper object wrapping a SVGMatrix
// object takes (SVGTransform*, &SVGTransform::matrix, &SVGTransform::setMatrix). A JS call like
// "someElement.transform.matrix.a = 50.0' causes the JSSVGMatrix object to call SVGTransform::setMatrix,
// method, which in turn notifies 'someElement' that the 'SVGNames::transformAttr' has changed.
// That's a short sketch of our SVG DOM implementation.

// Represents a JS wrapper object for SVGAnimated* classes, exposing SVG POD types that contain writable properties
// (Two cases: SVGAnimatedLength exposing SVGLength, SVGAnimatedRect exposing SVGRect)

#if COMPILER(MSVC)
// GetterMethod and SetterMethod are each 12 bytes. We have to pack to a size
// greater than or equal to that to avoid an alignment warning (C4121). 16 is
// the next-largest size allowed for packing, so we use that.
#pragma pack(push, 16)
#endif
template<typename PODType, typename PODTypeCreator>
class JSSVGDynamicPODTypeWrapper : public JSSVGPODTypeWrapper<PODType> {
public:
    typedef PODType (PODTypeCreator::*GetterMethod)() const; 
    typedef void (PODTypeCreator::*SetterMethod)(const PODType&);

    static PassRefPtr<JSSVGDynamicPODTypeWrapper> create(PassRefPtr<PODTypeCreator> creator, GetterMethod getter, SetterMethod setter)
    {
        return adoptRef(new JSSVGDynamicPODTypeWrapper(creator, getter, setter));
    }

    virtual operator PODType()
    {
        return (m_creator.get()->*m_getter)();
    }

    virtual void commitChange(PODType type, DOMObject* wrapper)
    {
        (m_creator.get()->*m_setter)(type);
        JSSVGContextCache::propagateSVGDOMChange(wrapper, m_creator->associatedAttributeName());
    }

private:
    JSSVGDynamicPODTypeWrapper(PassRefPtr<PODTypeCreator> creator, GetterMethod getter, SetterMethod setter)
        : m_creator(creator)
        , m_getter(getter)
        , m_setter(setter)
    {
        ASSERT(m_creator);
        ASSERT(m_getter);
        ASSERT(m_setter);
    }

    virtual ~JSSVGDynamicPODTypeWrapper();

    // Update callbacks
    RefPtr<PODTypeCreator> m_creator;
    GetterMethod m_getter;
    SetterMethod m_setter;
};
#if COMPILER(MSVC)
#pragma pack(pop)
#endif

// Represents a JS wrapper object for SVG POD types (not for SVGAnimated* classes). Any modification to the SVG POD
// types don't cause any updates unlike JSSVGDynamicPODTypeWrapper. This class is used for return values (ie. getBBox())
// and for properties where SVG specification explicitly states, that the contents of the POD type are immutable.

template<typename PODType>
class JSSVGStaticPODTypeWrapper : public JSSVGPODTypeWrapper<PODType> {
public:
    static PassRefPtr<JSSVGStaticPODTypeWrapper> create(PODType type)
    {
        return adoptRef(new JSSVGStaticPODTypeWrapper(type));
    }

    virtual operator PODType()
    {
        return m_podType;
    }

    virtual void commitChange(PODType type, DOMObject*)
    {
        m_podType = type;
    }

protected:
    JSSVGStaticPODTypeWrapper(PODType type)
        : m_podType(type)
    {
    }

    PODType m_podType;
};

template<typename PODType, typename ParentTypeArg>
class JSSVGStaticPODTypeWrapperWithPODTypeParent : public JSSVGStaticPODTypeWrapper<PODType> {
public:
    typedef JSSVGPODTypeWrapper<ParentTypeArg> ParentType;

    static PassRefPtr<JSSVGStaticPODTypeWrapperWithPODTypeParent> create(PODType type, PassRefPtr<ParentType> parent)
    {
        return adoptRef(new JSSVGStaticPODTypeWrapperWithPODTypeParent(type, parent));
    }

    virtual void commitChange(PODType type, DOMObject* wrapper)
    {
        JSSVGStaticPODTypeWrapper<PODType>::commitChange(type, wrapper);
        m_parentType->commitChange(ParentTypeArg(type), wrapper);
    }

private:
    JSSVGStaticPODTypeWrapperWithPODTypeParent(PODType type, PassRefPtr<ParentType> parent)
        : JSSVGStaticPODTypeWrapper<PODType>(type)
        , m_parentType(parent)
    {
    }

    RefPtr<ParentType> m_parentType;
};

#if COMPILER(MSVC)
// GetterMethod and SetterMethod are each 12 bytes. We have to pack to a size
// greater than or equal to that to avoid an alignment warning (C4121). 16 is
// the next-largest size allowed for packing, so we use that.
#pragma pack(push, 16)
#endif
template<typename PODType, typename ParentType>
class JSSVGStaticPODTypeWrapperWithParent : public JSSVGPODTypeWrapper<PODType> {
public:
    typedef PODType (ParentType::*GetterMethod)() const;
    typedef void (ParentType::*SetterMethod)(const PODType&);

    static PassRefPtr<JSSVGStaticPODTypeWrapperWithParent> create(PassRefPtr<ParentType> parent, GetterMethod getter, SetterMethod setter)
    {
        return adoptRef(new JSSVGStaticPODTypeWrapperWithParent(parent, getter, setter));
    }

    virtual operator PODType()
    {
        return (m_parent.get()->*m_getter)();
    }

    virtual void commitChange(PODType type, DOMObject*)
    {
        (m_parent.get()->*m_setter)(type);
    }

private:
    JSSVGStaticPODTypeWrapperWithParent(PassRefPtr<ParentType> parent, GetterMethod getter, SetterMethod setter)
        : m_parent(parent)
        , m_getter(getter)
        , m_setter(setter)
    {
        ASSERT(m_parent);
        ASSERT(m_getter);
        ASSERT(m_setter);
    }

    // Update callbacks
    RefPtr<ParentType> m_parent;
    GetterMethod m_getter;
    SetterMethod m_setter;
};

template<typename PODType>
class SVGPODListItem;

// Just like JSSVGDynamicPODTypeWrapper, but only used for SVGList* objects wrapping around POD values.

template<typename PODType>
class JSSVGPODTypeWrapperCreatorForList : public JSSVGPODTypeWrapper<PODType> {
public:
    typedef SVGPODListItem<PODType> PODListItemPtrType;

    typedef PODType (SVGPODListItem<PODType>::*GetterMethod)() const; 
    typedef void (SVGPODListItem<PODType>::*SetterMethod)(const PODType&);

    static PassRefPtr<JSSVGPODTypeWrapperCreatorForList> create(PassRefPtr<PODListItemPtrType> creator, const QualifiedName& attributeName)
    {
        return adoptRef(new JSSVGPODTypeWrapperCreatorForList(creator, attributeName));
    }

    virtual operator PODType()
    {
        return (m_creator.get()->*m_getter)();
    }

    virtual void commitChange(PODType type, DOMObject* wrapper)
    {
        if (!m_setter)
            return;

        (m_creator.get()->*m_setter)(type);
        JSSVGContextCache::propagateSVGDOMChange(wrapper, m_associatedAttributeName);
    }

private:
    JSSVGPODTypeWrapperCreatorForList(PassRefPtr<PODListItemPtrType> creator, const QualifiedName& attributeName)
        : m_creator(creator)
        , m_getter(&PODListItemPtrType::value)
        , m_setter(&PODListItemPtrType::setValue)
        , m_associatedAttributeName(attributeName)
    {
        ASSERT(m_creator);
        ASSERT(m_getter);
        ASSERT(m_setter);
    }

    // Update callbacks
    RefPtr<PODListItemPtrType> m_creator;
    GetterMethod m_getter;
    SetterMethod m_setter;
    const QualifiedName& m_associatedAttributeName;
};

// Caching facilities
template<typename PODType, typename PODTypeCreator>
struct PODTypeWrapperCacheInfo {
    typedef PODType (PODTypeCreator::*GetterMethod)() const; 
    typedef void (PODTypeCreator::*SetterMethod)(const PODType&);

    // Empty value
    PODTypeWrapperCacheInfo()
        : creator(0)
        , getter(0)
        , setter(0)
    {
    }

    // Deleted value
    PODTypeWrapperCacheInfo(WTF::HashTableDeletedValueType)
        : creator(reinterpret_cast<PODTypeCreator*>(-1))
        , getter(0)
        , setter(0)
    {
    }
    bool isHashTableDeletedValue() const
    {
        return creator == reinterpret_cast<PODTypeCreator*>(-1);
    }

    PODTypeWrapperCacheInfo(PODTypeCreator* _creator, GetterMethod _getter, SetterMethod _setter)
        : creator(_creator)
        , getter(_getter)
        , setter(_setter)
    {
        ASSERT(creator);
        ASSERT(getter);
    }

    bool operator==(const PODTypeWrapperCacheInfo& other) const
    {
        return creator == other.creator && getter == other.getter && setter == other.setter;
    }

    PODTypeCreator* creator;
    GetterMethod getter;
    SetterMethod setter;
};
#if COMPILER(MSVC)
#pragma pack(pop)
#endif

template<typename PODType, typename PODTypeCreator>
struct PODTypeWrapperCacheInfoHash {
    typedef PODTypeWrapperCacheInfo<PODType, PODTypeCreator> CacheInfo;

    static unsigned hash(const CacheInfo& info)
    {
        return StringImpl::computeHash(reinterpret_cast<const UChar*>(&info), sizeof(CacheInfo) / sizeof(UChar));
    }

    static bool equal(const CacheInfo& a, const CacheInfo& b)
    {
        return a == b;
    }

    static const bool safeToCompareToEmptyOrDeleted = true;
};

template<typename PODType, typename PODTypeCreator>
struct PODTypeWrapperCacheInfoTraits : WTF::GenericHashTraits<PODTypeWrapperCacheInfo<PODType, PODTypeCreator> > {
    typedef PODTypeWrapperCacheInfo<PODType, PODTypeCreator> CacheInfo;

    static const bool emptyValueIsZero = true;
    static const bool needsDestruction = false;

    static const CacheInfo& emptyValue()
    {
        DEFINE_STATIC_LOCAL(CacheInfo, key, ());
        return key;
    }

    static void constructDeletedValue(CacheInfo& slot)
    {
        new (&slot) CacheInfo(WTF::HashTableDeletedValue);
    }

    static bool isDeletedValue(const CacheInfo& value)
    {
        return value.isHashTableDeletedValue();
    }
};

// Used for dynamic read-write attributes
template<typename PODType, typename PODTypeCreator>
class JSSVGDynamicPODTypeWrapperCache {
public:
    typedef PODType (PODTypeCreator::*GetterMethod)() const; 
    typedef void (PODTypeCreator::*SetterMethod)(const PODType&);

    typedef PODTypeWrapperCacheInfo<PODType, PODTypeCreator> CacheInfo;
    typedef PODTypeWrapperCacheInfoHash<PODType, PODTypeCreator> CacheInfoHash;
    typedef PODTypeWrapperCacheInfoTraits<PODType, PODTypeCreator> CacheInfoTraits;

    typedef JSSVGPODTypeWrapper<PODType> WrapperBase;
    typedef JSSVGDynamicPODTypeWrapper<PODType, PODTypeCreator> Wrapper;
    typedef HashMap<CacheInfo, Wrapper*, CacheInfoHash, CacheInfoTraits> WrapperMap;

    static WrapperMap& wrapperMap()
    {
        DEFINE_STATIC_LOCAL(WrapperMap, s_wrapperMap, ());
        return s_wrapperMap;
    }

    static PassRefPtr<WrapperBase> lookupOrCreateWrapper(PODTypeCreator* creator, GetterMethod getter, SetterMethod setter)
    {
        CacheInfo info(creator, getter, setter);
        pair<typename WrapperMap::iterator, bool> result = wrapperMap().add(info, 0);
        if (!result.second) // pre-existing entry
            return result.first->second;

        RefPtr<Wrapper> wrapper = Wrapper::create(creator, getter, setter);
        result.first->second = wrapper.get();
        return wrapper.release();
    }

    static void forgetWrapper(PODTypeCreator* creator, GetterMethod getter, SetterMethod setter)
    {
        CacheInfo info(creator, getter, setter);
        wrapperMap().remove(info);
    }
};

template<typename PODType, typename PODTypeCreator>
JSSVGDynamicPODTypeWrapper<PODType, PODTypeCreator>::~JSSVGDynamicPODTypeWrapper()
{
    JSSVGDynamicPODTypeWrapperCache<PODType, PODTypeCreator>::forgetWrapper(m_creator.get(), m_getter, m_setter);
}

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // JSSVGPODTypeWrapper_h
