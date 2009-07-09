/*
 * Copyright (C) 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Google. All rights reserved.
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

#ifndef V8SVGPODTypeWrapper_h
#define V8SVGPODTypeWrapper_h

#if ENABLE(SVG)

#include "SVGElement.h"
#include "SVGList.h"
#include "V8Proxy.h"

#include <wtf/Assertions.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

template<typename PODType>
class V8SVGPODTypeWrapper : public RefCounted<V8SVGPODTypeWrapper<PODType> > {
public:
    V8SVGPODTypeWrapper() { }
    virtual ~V8SVGPODTypeWrapper() { }
    virtual operator PODType() = 0;
    virtual void commitChange(PODType, SVGElement*) = 0;
};

template<typename PODType>
class V8SVGPODTypeWrapperCreatorForList : public V8SVGPODTypeWrapper<PODType> {
public:
    typedef PODType (SVGPODListItem<PODType>::*GetterMethod)() const;
    typedef void (SVGPODListItem<PODType>::*SetterMethod)(PODType);

    V8SVGPODTypeWrapperCreatorForList(SVGPODListItem<PODType>* creator, const QualifiedName& attributeName)
        : m_creator(creator)
        , m_getter(&SVGPODListItem<PODType>::value)
        , m_setter(&SVGPODListItem<PODType>::setValue)
        , m_associatedAttributeName(attributeName)
    {
        ASSERT(m_creator);
        ASSERT(m_getter);
        ASSERT(m_setter);
    }

    virtual ~V8SVGPODTypeWrapperCreatorForList() { }

    // Getter wrapper
    virtual operator PODType() { return (m_creator.get()->*m_getter)(); }

    // Setter wrapper
    virtual void commitChange(PODType type, SVGElement* context)
    {
        if (!m_setter)
            return;

        (m_creator.get()->*m_setter)(type);

        if (context)
            context->svgAttributeChanged(m_associatedAttributeName);
    }

private:
    // Update callbacks
    RefPtr<SVGPODListItem<PODType> > m_creator;
    GetterMethod m_getter;
    SetterMethod m_setter;
    const QualifiedName& m_associatedAttributeName;
};

template<typename PODType>
class V8SVGStaticPODTypeWrapper : public V8SVGPODTypeWrapper<PODType> {
public:
    V8SVGStaticPODTypeWrapper(PODType type)
        : m_podType(type)
    {
    }

    virtual ~V8SVGStaticPODTypeWrapper() { }

    // Getter wrapper
    virtual operator PODType() { return m_podType; }

    // Setter wrapper
    virtual void commitChange(PODType type, SVGElement*)
    {
        m_podType = type;
    }

private:
    PODType m_podType;
};

template<typename PODType, typename ParentTypeArg>
class V8SVGStaticPODTypeWrapperWithPODTypeParent : public V8SVGStaticPODTypeWrapper<PODType> {
public:
    typedef V8SVGPODTypeWrapper<ParentTypeArg> ParentType;

     V8SVGStaticPODTypeWrapperWithPODTypeParent(PODType type, ParentType* parent)
        : V8SVGStaticPODTypeWrapper<PODType>(type)
        , m_parentType(parent)
    {
    }

    virtual void commitChange(PODType type, SVGElement* context)
    {
        V8SVGStaticPODTypeWrapper<PODType>::commitChange(type, context);
        m_parentType->commitChange(ParentTypeArg(type), context);
    }

private:
    RefPtr<ParentType> m_parentType;
};

template<typename PODType, typename ParentType>
class V8SVGStaticPODTypeWrapperWithParent : public V8SVGPODTypeWrapper<PODType> {
public:
    typedef PODType (ParentType::*GetterMethod)() const;
    typedef void (ParentType::*SetterMethod)(const PODType&);

    V8SVGStaticPODTypeWrapperWithParent(ParentType* parent, GetterMethod getter, SetterMethod setter)
        : m_parent(parent)
        , m_getter(getter)
        , m_setter(setter)
    {
        ASSERT(m_parent);
        ASSERT(m_getter);
        ASSERT(m_setter);
    }

    virtual operator PODType()
    {
        return (m_parent.get()->*m_getter)();
    }

    virtual void commitChange(PODType type, SVGElement* context)
    {
        (m_parent.get()->*m_setter)(type);
    }

private:
    RefPtr<ParentType> m_parent;
    GetterMethod m_getter;
    SetterMethod m_setter;
};

template<typename PODType, typename PODTypeCreator>
class V8SVGDynamicPODTypeWrapper : public V8SVGPODTypeWrapper<PODType> {
public:
    typedef PODType (PODTypeCreator::*GetterMethod)() const;
    typedef void (PODTypeCreator::*SetterMethod)(PODType);
    typedef void (*CacheRemovalCallback)(V8SVGPODTypeWrapper<PODType>*);

    V8SVGDynamicPODTypeWrapper(PODTypeCreator* creator, GetterMethod getter, SetterMethod setter, CacheRemovalCallback cacheRemovalCallback)
        : m_creator(creator)
        , m_getter(getter)
        , m_setter(setter)
        , m_cacheRemovalCallback(cacheRemovalCallback)
    {
        ASSERT(creator);
        ASSERT(getter);
        ASSERT(setter);
        ASSERT(cacheRemovalCallback);
    }

    virtual ~V8SVGDynamicPODTypeWrapper() {
        ASSERT(m_cacheRemovalCallback);

        (*m_cacheRemovalCallback)(this);
    }

    // Getter wrapper
    virtual operator PODType() { return (m_creator.get()->*m_getter)(); }

    // Setter wrapper
    virtual void commitChange(PODType type, SVGElement* context)
    {
        (m_creator.get()->*m_setter)(type);

        if (context)
            context->svgAttributeChanged(m_creator->associatedAttributeName());
    }

private:
    // Update callbacks
    RefPtr<PODTypeCreator> m_creator;
    GetterMethod m_getter;
    SetterMethod m_setter;
    CacheRemovalCallback m_cacheRemovalCallback;
};

// Caching facilities
template<typename PODType, typename PODTypeCreator>
struct PODTypeWrapperCacheInfo {
    typedef PODType (PODTypeCreator::*GetterMethod)() const;
    typedef void (PODTypeCreator::*SetterMethod)(PODType);

    // Empty value
    PODTypeWrapperCacheInfo()
        : creator(0)
        , getter(0)
        , setter(0)
    { }

    // Deleted value
    explicit PODTypeWrapperCacheInfo(WTF::HashTableDeletedValueType)
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

template<typename PODType, typename PODTypeCreator>
struct PODTypeWrapperCacheInfoHash {
    static unsigned hash(const PODTypeWrapperCacheInfo<PODType, PODTypeCreator>& info)
    {
        unsigned creator = reinterpret_cast<unsigned>(info.creator);
        unsigned getter = reinterpret_cast<unsigned>(*(void**)&info.getter);
        unsigned setter = reinterpret_cast<unsigned>(*(void**)&info.setter);
        return (creator * 13) + getter ^ (setter >> 2);
    }

    static bool equal(const PODTypeWrapperCacheInfo<PODType, PODTypeCreator>& a, const PODTypeWrapperCacheInfo<PODType, PODTypeCreator>& b)
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

template<typename PODType, typename PODTypeCreator>
class V8SVGDynamicPODTypeWrapperCache {
public:
    typedef PODType (PODTypeCreator::*GetterMethod)() const;
    typedef void (PODTypeCreator::*SetterMethod)(PODType);

    typedef PODTypeWrapperCacheInfo<PODType, PODTypeCreator> CacheInfo;
    typedef PODTypeWrapperCacheInfoHash<PODType, PODTypeCreator> CacheInfoHash;
    typedef PODTypeWrapperCacheInfoTraits<PODType, PODTypeCreator> CacheInfoTraits;

    typedef V8SVGPODTypeWrapper<PODType> WrapperBase;
    typedef V8SVGDynamicPODTypeWrapper<PODType, PODTypeCreator> DynamicWrapper;

    typedef HashMap<CacheInfo, DynamicWrapper*, CacheInfoHash, CacheInfoTraits> DynamicWrapperHashMap;
    typedef typename DynamicWrapperHashMap::const_iterator DynamicWrapperHashMapIterator;

    static DynamicWrapperHashMap& dynamicWrapperHashMap()
    {
        DEFINE_STATIC_LOCAL(DynamicWrapperHashMap, dynamicWrapperHashMap, ());
        return dynamicWrapperHashMap;
    }

    // Used for readwrite attributes only
    static WrapperBase* lookupOrCreateWrapper(PODTypeCreator* creator, GetterMethod getter, SetterMethod setter)
    {
        DynamicWrapperHashMap& map(dynamicWrapperHashMap());
        CacheInfo info(creator, getter, setter);

        if (map.contains(info))
            return map.get(info);

        DynamicWrapper* wrapper = new V8SVGDynamicPODTypeWrapper<PODType, PODTypeCreator>(creator, getter, setter, forgetWrapper);
        map.set(info, wrapper);
        return wrapper;
    }

    static void forgetWrapper(V8SVGPODTypeWrapper<PODType>* wrapper)
    {
        DynamicWrapperHashMap& map(dynamicWrapperHashMap());

        DynamicWrapperHashMapIterator it = map.begin();
        DynamicWrapperHashMapIterator end = map.end();

        for (; it != end; ++it) {
            if (it->second != wrapper)
                continue;

            // It's guaranteed that there's just one object we need to take care of.
            map.remove(it->first);
            break;
        }
    }
};

class V8SVGPODTypeUtil {
public:
    template <class P>
    static P toSVGPODType(V8ClassIndex::V8WrapperType type, v8::Handle<v8::Value> object, bool& ok);
};

template <class P>
P V8SVGPODTypeUtil::toSVGPODType(V8ClassIndex::V8WrapperType type, v8::Handle<v8::Value> object, bool& ok)
{
    void *wrapper = V8DOMWrapper::convertToSVGPODTypeImpl(type, object);
    if (wrapper == NULL) {
        ok = false;
        return P();
    } else {
        ok = true;
        return *static_cast<V8SVGPODTypeWrapper<P>*>(wrapper);
    }
}

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // V8SVGPODTypeWrapper_h
