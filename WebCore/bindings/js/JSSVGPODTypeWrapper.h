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

#include "Frame.h"
#include "SVGElement.h"

namespace WebCore {

template<typename PODType>
class JSSVGPODTypeWrapper : public RefCounted<JSSVGPODTypeWrapper<PODType> > {
public:
    virtual ~JSSVGPODTypeWrapper() { }

    // Getter wrapper
    virtual operator PODType() = 0;

    // Setter wrapper
    virtual void commitChange(PODType, SVGElement*) = 0;
};

template<typename PODType, typename PODTypeCreator>
class JSSVGPODTypeWrapperCreatorReadWrite : public JSSVGPODTypeWrapper<PODType> {
public:
    typedef PODType (PODTypeCreator::*GetterMethod)() const; 
    typedef void (PODTypeCreator::*SetterMethod)(PODType);

    static PassRefPtr<JSSVGPODTypeWrapperCreatorReadWrite> create(PassRefPtr<PODTypeCreator> creator, GetterMethod getter, SetterMethod setter)
    {
        return adoptRef(new JSSVGPODTypeWrapperCreatorReadWrite(creator, getter, setter));
    }

    // Getter wrapper
    virtual operator PODType() { return (m_creator.get()->*m_getter)(); }

    // Setter wrapper
    virtual void commitChange(PODType type, SVGElement* context)
    {
        if (!m_setter)
            return;

        (m_creator.get()->*m_setter)(type);

        if (context)
            context->svgAttributeChanged(m_creator->associatedAttributeName());
    }

private:
    JSSVGPODTypeWrapperCreatorReadWrite(PassRefPtr<PODTypeCreator> creator, GetterMethod getter, SetterMethod setter)
        : m_creator(creator)
        , m_getter(getter)
        , m_setter(setter)
    {
        ASSERT(m_creator);
        ASSERT(m_getter);
        ASSERT(m_setter);
    }

    // Update callbacks
    RefPtr<PODTypeCreator> m_creator;
    GetterMethod m_getter;
    SetterMethod m_setter;
};

template<typename PODType>
class JSSVGPODTypeWrapperCreatorReadOnly : public JSSVGPODTypeWrapper<PODType> {
public:
    static PassRefPtr<JSSVGPODTypeWrapperCreatorReadOnly> create(PODType type)
    {
        return adoptRef(new JSSVGPODTypeWrapperCreatorReadOnly(type));
    }

    // Getter wrapper
    virtual operator PODType() { return m_podType; }

    // Setter wrapper
    virtual void commitChange(PODType type, SVGElement*)
    {
        m_podType = type;
    }

private:
    JSSVGPODTypeWrapperCreatorReadOnly(PODType type)
        : m_podType(type)
    {
    }

    PODType m_podType;
};

template<typename PODType>
class SVGPODListItem;

template<typename PODType>
class JSSVGPODTypeWrapperCreatorForList : public JSSVGPODTypeWrapper<PODType> {
public:
    typedef PODType (SVGPODListItem<PODType>::*GetterMethod)() const; 
    typedef void (SVGPODListItem<PODType>::*SetterMethod)(PODType);

    static PassRefPtr<JSSVGPODTypeWrapperCreatorForList> create(PassRefPtr<SVGPODListItem<PODType> > creator, const QualifiedName& attributeName)
    {
        return adoptRef(new JSSVGPODTypeWrapperCreatorForList(creator, attributeName));
    }

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
    JSSVGPODTypeWrapperCreatorForList(PassRefPtr<SVGPODListItem<PODType> > creator, const QualifiedName& attributeName)
        : m_creator(creator)
        , m_getter(&SVGPODListItem<PODType>::value)
        , m_setter(&SVGPODListItem<PODType>::setValue)
        , m_associatedAttributeName(attributeName)
    {
        ASSERT(m_creator);
        ASSERT(m_getter);
        ASSERT(m_setter);
    }

    // Update callbacks
    RefPtr<SVGPODListItem<PODType> > m_creator;
    GetterMethod m_getter;
    SetterMethod m_setter;
    const QualifiedName& m_associatedAttributeName;
};

// Caching facilities
template<typename PODType, typename PODTypeCreator>
struct PODTypeReadWriteHashInfo {
    typedef PODType (PODTypeCreator::*GetterMethod)() const; 
    typedef void (PODTypeCreator::*SetterMethod)(PODType);

    // Empty value
    PODTypeReadWriteHashInfo()
        : creator(0)
        , getter(0)
        , setter(0)
    {
    }

    // Deleted value
    PODTypeReadWriteHashInfo(WTF::HashTableDeletedValueType)
        : creator(reinterpret_cast<PODTypeCreator*>(-1))
    {
    }
    bool isHashTableDeletedValue() const
    {
        return creator == reinterpret_cast<PODTypeCreator*>(-1);
    }

    PODTypeReadWriteHashInfo(PODTypeCreator* _creator, GetterMethod _getter, SetterMethod _setter)
        : creator(_creator)
        , getter(_getter)
        , setter(_setter)
    {
        ASSERT(creator);
        ASSERT(getter);
    }

    bool operator==(const PODTypeReadWriteHashInfo& other) const
    {
        return creator == other.creator && getter == other.getter && setter == other.setter;
    }

    PODTypeCreator* creator;
    GetterMethod getter;
    SetterMethod setter;
};

template<typename PODType, typename PODTypeCreator>
struct PODTypeReadWriteHashInfoHash {
    static unsigned hash(const PODTypeReadWriteHashInfo<PODType, PODTypeCreator>& info)
    {
        return StringImpl::computeHash(reinterpret_cast<const UChar*>(&info), sizeof(PODTypeReadWriteHashInfo<PODType, PODTypeCreator>) / sizeof(UChar));
    }

    static bool equal(const PODTypeReadWriteHashInfo<PODType, PODTypeCreator>& a, const PODTypeReadWriteHashInfo<PODType, PODTypeCreator>& b)
    {
        return a == b;
    }

    static const bool safeToCompareToEmptyOrDeleted = true;
};

template<typename PODType, typename PODTypeCreator>
struct PODTypeReadWriteHashInfoTraits : WTF::GenericHashTraits<PODTypeReadWriteHashInfo<PODType, PODTypeCreator> > {
    static const bool emptyValueIsZero = true;
    static const bool needsDestruction = false;

    static const PODTypeReadWriteHashInfo<PODType, PODTypeCreator>& emptyValue()
    {
        static PODTypeReadWriteHashInfo<PODType, PODTypeCreator> key;
        return key;
    }

    static void constructDeletedValue(PODTypeReadWriteHashInfo<PODType, PODTypeCreator>* slot)
    {
        new (slot) PODTypeReadWriteHashInfo<PODType, PODTypeCreator>(WTF::HashTableDeletedValue);
    }
    static bool isDeletedValue(const PODTypeReadWriteHashInfo<PODType, PODTypeCreator>& value)
    {
        return value.isHashTableDeletedValue();
    }
};

template<typename PODType, typename PODTypeCreator>
class JSSVGPODTypeWrapperCache {
public:
    typedef PODType (PODTypeCreator::*GetterMethod)() const; 
    typedef void (PODTypeCreator::*SetterMethod)(PODType);

    typedef HashMap<PODTypeReadWriteHashInfo<PODType, PODTypeCreator>, JSSVGPODTypeWrapperCreatorReadWrite<PODType, PODTypeCreator>*, PODTypeReadWriteHashInfoHash<PODType, PODTypeCreator>, PODTypeReadWriteHashInfoTraits<PODType, PODTypeCreator> > ReadWriteHashMap;
    typedef typename ReadWriteHashMap::const_iterator ReadWriteHashMapIterator;

    static ReadWriteHashMap& readWriteHashMap()
    {
        static ReadWriteHashMap _readWriteHashMap;
        return _readWriteHashMap;
    }

    // Used for readwrite attributes only
    static PassRefPtr<JSSVGPODTypeWrapper<PODType> > lookupOrCreateWrapper(PODTypeCreator* creator, GetterMethod getter, SetterMethod setter)
    {
        ReadWriteHashMap& map(readWriteHashMap());
        PODTypeReadWriteHashInfo<PODType, PODTypeCreator> info(creator, getter, setter);

        if (map.contains(info))
            return map.get(info);

        RefPtr<JSSVGPODTypeWrapperCreatorReadWrite<PODType, PODTypeCreator> > wrapper = JSSVGPODTypeWrapperCreatorReadWrite<PODType, PODTypeCreator>::create(creator, getter, setter);
        map.set(info, wrapper.get());
        return wrapper.release();
    }

    static void forgetWrapper(JSSVGPODTypeWrapper<PODType>* wrapper)
    {
        ReadWriteHashMap& map(readWriteHashMap());

        ReadWriteHashMapIterator it = map.begin();
        ReadWriteHashMapIterator end = map.end();

        for (; it != end; ++it) {
            if (it->second != wrapper)
                continue;

            // It's guaranteed that there's just one object we need to take care of.
            map.remove(it->first);
            break;
        }
    }
};

};

#endif // ENABLE(SVG)
#endif // JSSVGPODTypeWrapper_h
