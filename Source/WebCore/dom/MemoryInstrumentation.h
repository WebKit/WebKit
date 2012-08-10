
/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MemoryInstrumentation_h
#define MemoryInstrumentation_h

#include <wtf/Assertions.h>
#include <wtf/Forward.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

template <typename T> class DataRef;
class KURL;
class MemoryObjectInfo;

class MemoryInstrumentation {
public:
    virtual ~MemoryInstrumentation() { }

    enum ObjectType {
        Other,
        DOM,
        CSS,
        Binding,
        Loader,
        MemoryCacheStructures,
        CachedResource,
        CachedResourceCSS,
        CachedResourceFont,
        CachedResourceImage,
        CachedResourceScript,
        CachedResourceSVG,
        CachedResourceShader,
        CachedResourceXSLT,
        LastTypeEntry
    };

    template <typename T> void addRootObject(const T& t)
    {
        addInstrumentedObject(t, Other);
        processDeferredInstrumentedPointers();
    }

    template <typename Container> static size_t calculateContainerSize(const Container&, bool contentOnly = false);

protected:
    class InstrumentedPointerBase {
    public:
        virtual ~InstrumentedPointerBase() { }
        virtual void process(MemoryInstrumentation*) = 0;
    };

private:
    virtual void countObjectSize(ObjectType, size_t) = 0;
    virtual void deferInstrumentedPointer(PassOwnPtr<InstrumentedPointerBase>) = 0;
    virtual bool visited(const void*) = 0;
    virtual void processDeferredInstrumentedPointers() = 0;

    friend class MemoryClassInfo;
    template <typename T> class InstrumentedPointer : public InstrumentedPointerBase {
    public:
        explicit InstrumentedPointer(const T* pointer, ObjectType ownerObjectType) : m_pointer(pointer), m_ownerObjectType(ownerObjectType) { }
        virtual void process(MemoryInstrumentation*) OVERRIDE;

    private:
        const T* m_pointer;
        const ObjectType m_ownerObjectType;
    };

    template <typename T> void addObject(const T& t, ObjectType ownerObjectType)
    {
        OwningTraits<T>::addObject(this, t, ownerObjectType);
    }
    void addObject(const String&, ObjectType);
    void addObject(const StringImpl*, ObjectType);
    void addObject(const KURL&, ObjectType);
    template <typename T> void addInstrumentedObject(const T& t, ObjectType ownerObjectType) { OwningTraits<T>::addInstrumentedObject(this, t, ownerObjectType); }
    template <typename HashMapType> void addHashMap(const HashMapType&, ObjectType, bool contentOnly = false);
    template <typename HashSetType> void addHashSet(const HashSetType&, ObjectType, bool contentOnly = false);
    template <typename CollectionType> void addInstrumentedCollection(const CollectionType&, ObjectType, bool contentOnly = false);
    template <typename MapType> void addInstrumentedMapEntries(const MapType&, ObjectType);
    template <typename MapType> void addInstrumentedMapValues(const MapType&, ObjectType);
    template <typename ListHashSetType> void addListHashSet(const ListHashSetType&, ObjectType, bool contentOnly = false);
    template <typename VectorType> void addVector(const VectorType&, ObjectType, bool contentOnly = false);
    void addRawBuffer(const void* const& buffer, ObjectType ownerObjectType, size_t size)
    {
        if (!buffer || visited(buffer))
            return;
        countObjectSize(ownerObjectType, size);
    }

    enum OwningType {
        byPointer,
        byReference
    };

    template <typename T>
    struct OwningTraits { // Default byReference implementation.
        static void addInstrumentedObject(MemoryInstrumentation* instrumentation, const T& t, ObjectType ownerObjectType) { instrumentation->addInstrumentedObjectImpl(&t, ownerObjectType, byReference); }
        static void addObject(MemoryInstrumentation* instrumentation, const T& t, ObjectType ownerObjectType) { instrumentation->addObjectImpl(&t, ownerObjectType, byReference); }
    };

    template <typename T>
    struct OwningTraits<T*> { // Custom byPointer implementation.
        static void addInstrumentedObject(MemoryInstrumentation* instrumentation, const T* const& t, ObjectType ownerObjectType) { instrumentation->addInstrumentedObjectImpl(t, ownerObjectType, byPointer); }
        static void addObject(MemoryInstrumentation* instrumentation, const T* const& t, ObjectType ownerObjectType) { instrumentation->addObjectImpl(t, ownerObjectType, byPointer); }
    };

    template <typename T> void addInstrumentedObjectImpl(const T* const&, ObjectType, OwningType);
    template <typename T> void addInstrumentedObjectImpl(const DataRef<T>* const&, ObjectType, OwningType);
    template <typename T> void addInstrumentedObjectImpl(const OwnPtr<T>* const&, ObjectType, OwningType);
    template <typename T> void addInstrumentedObjectImpl(const RefPtr<T>* const&, ObjectType, OwningType);

    template <typename T> void addObjectImpl(const T* const&, ObjectType, OwningType);
    template <typename T> void addObjectImpl(const DataRef<T>* const&, ObjectType, OwningType);
    template <typename T> void addObjectImpl(const OwnPtr<T>* const&, ObjectType, OwningType);
    template <typename T> void addObjectImpl(const RefPtr<T>* const&, ObjectType, OwningType);
};

class MemoryObjectInfo {
public:
    MemoryObjectInfo(MemoryInstrumentation* memoryInstrumentation, MemoryInstrumentation::ObjectType ownerObjectType)
        : m_memoryInstrumentation(memoryInstrumentation)
        , m_objectType(ownerObjectType)
        , m_objectSize(0)
    { }

    MemoryInstrumentation::ObjectType objectType() const { return m_objectType; }
    size_t objectSize() const { return m_objectSize; }

    MemoryInstrumentation* memoryInstrumentation() { return m_memoryInstrumentation; }

private:
    friend class MemoryClassInfo;

    template <typename T> void reportObjectInfo(MemoryInstrumentation::ObjectType objectType)
    {
        if (!m_objectSize) {
            m_objectSize = sizeof(T);
            if (objectType != MemoryInstrumentation::Other)
                m_objectType = objectType;
        }
    }

    MemoryInstrumentation* m_memoryInstrumentation;
    MemoryInstrumentation::ObjectType m_objectType;
    size_t m_objectSize;
};

class MemoryClassInfo {
public:
    template <typename T>
    MemoryClassInfo(MemoryObjectInfo* memoryObjectInfo, const T*, MemoryInstrumentation::ObjectType objectType)
        : m_memoryObjectInfo(memoryObjectInfo)
        , m_memoryInstrumentation(memoryObjectInfo->memoryInstrumentation())
    {
        m_memoryObjectInfo->reportObjectInfo<T>(objectType);
        m_objectType = memoryObjectInfo->objectType();
    }

    template <typename M> void addInstrumentedMember(const M& member) { m_memoryInstrumentation->addInstrumentedObject(member, m_objectType); }
    template <typename M> void addMember(const M& member) { m_memoryInstrumentation->addObject(member, m_objectType); }

    template <typename HashMapType> void addHashMap(const HashMapType& map) { m_memoryInstrumentation->addHashMap(map, m_objectType, true); }
    template <typename HashSetType> void addHashSet(const HashSetType& set) { m_memoryInstrumentation->addHashSet(set, m_objectType, true); }
    template <typename HashSetType> void addHashCountedSet(const HashSetType& set) { m_memoryInstrumentation->addHashSet(set, m_objectType, true); }
    template <typename HashSetType> void addInstrumentedHashSet(const HashSetType& set) { m_memoryInstrumentation->addInstrumentedCollection(set, m_objectType, true); }
    template <typename VectorType> void addInstrumentedVector(const VectorType& vector) { m_memoryInstrumentation->addInstrumentedCollection(vector, m_objectType, true); }
    template <typename VectorType> void addInstrumentedVectorPtr(const OwnPtr<VectorType>& vector) { m_memoryInstrumentation->addInstrumentedCollection(*vector, m_objectType, false); }
    template <typename VectorType> void addInstrumentedVectorPtr(const VectorType* const& vector) { m_memoryInstrumentation->addInstrumentedCollection(*vector, m_objectType, false); }
    template <typename MapType> void addInstrumentedMapEntries(const MapType& map) { m_memoryInstrumentation->addInstrumentedMapEntries(map, m_objectType); }
    template <typename MapType> void addInstrumentedMapValues(const MapType& map) { m_memoryInstrumentation->addInstrumentedMapValues(map, m_objectType); }
    template <typename ListHashSetType> void addListHashSet(const ListHashSetType& set) { m_memoryInstrumentation->addListHashSet(set, m_objectType, true); }
    template <typename VectorType> void addVector(const VectorType& vector) { m_memoryInstrumentation->addVector(vector, m_objectType, true); }
    template <typename VectorType> void addVectorPtr(const VectorType* const vector) { m_memoryInstrumentation->addVector(*vector, m_objectType, false); }
    void addRawBuffer(const void* const& buffer, size_t size) { m_memoryInstrumentation->addRawBuffer(buffer, m_objectType, size); }

    void addMember(const String& string) { m_memoryInstrumentation->addObject(string, m_objectType); }
    void addMember(const AtomicString& string) { m_memoryInstrumentation->addObject((const String&)string, m_objectType); }
    void addMember(const StringImpl* string) { m_memoryInstrumentation->addObject(string, m_objectType); }
    void addMember(const KURL& url) { m_memoryInstrumentation->addObject(url, m_objectType); }

private:
    MemoryObjectInfo* m_memoryObjectInfo;
    MemoryInstrumentation* m_memoryInstrumentation;
    MemoryInstrumentation::ObjectType m_objectType;
};

template <typename T>
void MemoryInstrumentation::addInstrumentedObjectImpl(const T* const& object, ObjectType ownerObjectType, OwningType owningType)
{
    if (owningType == byReference) {
        MemoryObjectInfo memoryObjectInfo(this, ownerObjectType);
        object->reportMemoryUsage(&memoryObjectInfo);
    } else {
        if (!object || visited(object))
            return;
        deferInstrumentedPointer(adoptPtr(new InstrumentedPointer<T>(object, ownerObjectType)));
    }
}

template <typename T>
void MemoryInstrumentation::addInstrumentedObjectImpl(const DataRef<T>* const& object, ObjectType ownerObjectType, OwningType owningType)
{
    if (owningType == byPointer)
        countObjectSize(ownerObjectType, sizeof(DataRef<T>));
    addInstrumentedObjectImpl(object->get(), ownerObjectType, byPointer);
}

template <typename T>
void MemoryInstrumentation::addInstrumentedObjectImpl(const OwnPtr<T>* const& object, ObjectType ownerObjectType, OwningType owningType)
{
    if (owningType == byPointer)
        countObjectSize(ownerObjectType, sizeof(OwnPtr<T>));
    addInstrumentedObjectImpl(object->get(), ownerObjectType, byPointer);
}

template <typename T>
void MemoryInstrumentation::addInstrumentedObjectImpl(const RefPtr<T>* const& object, ObjectType ownerObjectType, OwningType owningType)
{
    if (owningType == byPointer)
        countObjectSize(ownerObjectType, sizeof(RefPtr<T>));
    addInstrumentedObjectImpl(object->get(), ownerObjectType, byPointer);
}

template <typename T>
void MemoryInstrumentation::addObjectImpl(const DataRef<T>* const& object, ObjectType ownerObjectType, OwningType owningType)
{
    if (owningType == byPointer)
        countObjectSize(ownerObjectType, sizeof(DataRef<T>));
    addObjectImpl(object->get(), ownerObjectType, byPointer);
}

template <typename T>
void MemoryInstrumentation::addObjectImpl(const OwnPtr<T>* const& object, ObjectType ownerObjectType, OwningType owningType)
{
    if (owningType == byPointer)
        countObjectSize(ownerObjectType, sizeof(RefPtr<T>));
    addObjectImpl(object->get(), ownerObjectType, byPointer);
}

template <typename T>
void MemoryInstrumentation::addObjectImpl(const RefPtr<T>* const& object, ObjectType ownerObjectType, OwningType owningType)
{
    if (owningType == byPointer)
        countObjectSize(ownerObjectType, sizeof(RefPtr<T>));
    addObjectImpl(object->get(), ownerObjectType, byPointer);
}

template <typename T>
void MemoryInstrumentation::addObjectImpl(const T* const& object, ObjectType ownerObjectType, OwningType owningType)
{
    if (!object || visited(object))
        return;
    if (owningType == byReference)
        return;
    countObjectSize(ownerObjectType, sizeof(T));
}

template<typename HashMapType>
void MemoryInstrumentation::addHashMap(const HashMapType& hashMap, ObjectType ownerObjectType, bool contentOnly)
{
    if (visited(&hashMap))
        return;
    countObjectSize(ownerObjectType, calculateContainerSize(hashMap, contentOnly));
}

template<typename HashSetType>
void MemoryInstrumentation::addHashSet(const HashSetType& hashSet, ObjectType ownerObjectType, bool contentOnly)
{
    if (visited(&hashSet))
        return;
    countObjectSize(ownerObjectType, calculateContainerSize(hashSet, contentOnly));
}

template <typename CollectionType>
void MemoryInstrumentation::addInstrumentedCollection(const CollectionType& collection, ObjectType ownerObjectType, bool contentOnly)
{
    if (visited(&collection))
        return;
    countObjectSize(ownerObjectType, calculateContainerSize(collection, contentOnly));
    typename CollectionType::const_iterator end = collection.end();
    for (typename CollectionType::const_iterator i = collection.begin(); i != end; ++i)
        addInstrumentedObject(*i, ownerObjectType);
}

template <typename MapType>
void MemoryInstrumentation::addInstrumentedMapEntries(const MapType& map, ObjectType ownerObjectType)
{
    typename MapType::const_iterator end = map.end();
    for (typename MapType::const_iterator i = map.begin(); i != end; ++i) {
        addInstrumentedObject(i->first, ownerObjectType);
        addInstrumentedObject(i->second, ownerObjectType);
    }
}

template <typename MapType>
void MemoryInstrumentation::addInstrumentedMapValues(const MapType& map, ObjectType ownerObjectType)
{
    typename MapType::const_iterator end = map.end();
    for (typename MapType::const_iterator i = map.begin(); i != end; ++i)
        addInstrumentedObject(i->second, ownerObjectType);
}

template<typename ListHashSetType>
void MemoryInstrumentation::addListHashSet(const ListHashSetType& hashSet, ObjectType ownerObjectType, bool contentOnly)
{
    if (visited(&hashSet))
        return;
    size_t size = (contentOnly ? 0 : sizeof(ListHashSetType)) + hashSet.capacity() * sizeof(void*) + hashSet.size() * (sizeof(typename ListHashSetType::ValueType) + 2 * sizeof(void*));
    countObjectSize(ownerObjectType, size);
}

template <typename VectorType>
void MemoryInstrumentation::addVector(const VectorType& vector, ObjectType ownerObjectType, bool contentOnly)
{
    if (!vector.data() || visited(vector.data()))
        return;
    countObjectSize(ownerObjectType, calculateContainerSize(vector, contentOnly));
}

template <typename Container>
size_t MemoryInstrumentation::calculateContainerSize(const Container& container, bool contentOnly)
{
    return (contentOnly ? 0 : sizeof(container)) + container.capacity() * sizeof(typename Container::ValueType);
}

template<typename T>
void MemoryInstrumentation::InstrumentedPointer<T>::process(MemoryInstrumentation* memoryInstrumentation)
{
    MemoryObjectInfo memoryObjectInfo(memoryInstrumentation, m_ownerObjectType);
    m_pointer->reportMemoryUsage(&memoryObjectInfo);
    memoryInstrumentation->countObjectSize(memoryObjectInfo.objectType(), memoryObjectInfo.objectSize());
}


} // namespace WebCore

#endif // !defined(MemoryInstrumentation_h)
