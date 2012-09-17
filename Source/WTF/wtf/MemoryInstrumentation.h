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

#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RefPtr.h>

namespace WTF {

class MemoryClassInfo;
class MemoryObjectInfo;
class MemoryInstrumentation;

typedef const char* MemoryObjectType;

enum MemoryOwningType {
    byPointer,
    byReference
};

class MemoryObjectInfo {
public:
    MemoryObjectInfo(MemoryInstrumentation* memoryInstrumentation, MemoryObjectType ownerObjectType)
        : m_memoryInstrumentation(memoryInstrumentation)
        , m_objectType(ownerObjectType)
        , m_objectSize(0)
    { }

    typedef MemoryClassInfo ClassInfo;

    MemoryObjectType objectType() const { return m_objectType; }
    size_t objectSize() const { return m_objectSize; }

    MemoryInstrumentation* memoryInstrumentation() { return m_memoryInstrumentation; }

private:
    friend class MemoryClassInfo;
    friend class MemoryInstrumentation;

    template<typename T> void reportObjectInfo(MemoryObjectType objectType, size_t actualSize)
    {
        if (!m_objectSize) {
            m_objectSize = actualSize ? actualSize : sizeof(T);
            if (objectType)
                m_objectType = objectType;
        }
    }

    MemoryInstrumentation* m_memoryInstrumentation;
    MemoryObjectType m_objectType;
    size_t m_objectSize;
};

class MemoryInstrumentation {
public:
    virtual ~MemoryInstrumentation() { }

    template <typename T> void addRootObject(const T& t)
    {
        addInstrumentedObject(t, 0);
        processDeferredInstrumentedPointers();
    }

    template<typename Container> static size_t calculateContainerSize(const Container&, bool contentOnly = false);

protected:
    class InstrumentedPointerBase {
    public:
        virtual ~InstrumentedPointerBase() { }
        virtual void process(MemoryInstrumentation*) = 0;
    };

private:
    virtual void countObjectSize(MemoryObjectType, size_t) = 0;
    virtual void deferInstrumentedPointer(PassOwnPtr<InstrumentedPointerBase>) = 0;
    virtual bool visited(const void*) = 0;
    virtual void processDeferredInstrumentedPointers() = 0;

    friend class MemoryClassInfo;

    template<typename T> static void selectInstrumentationMethod(const T* const& object, MemoryObjectInfo* memoryObjectInfo)
    {
        // If there is reportMemoryUsage method on the object, call it.
        // Otherwise count only object's self size.
        reportObjectMemoryUsage<T, void (T::*)(MemoryObjectInfo*) const>(object, memoryObjectInfo, 0);
    }

    template<typename Type, Type Ptr> struct MemberHelperStruct;
    template<typename T, typename Type>
    static void reportObjectMemoryUsage(const T* const& object, MemoryObjectInfo* memoryObjectInfo,  MemberHelperStruct<Type, &T::reportMemoryUsage>*)
    {
        object->reportMemoryUsage(memoryObjectInfo);
    }

    template<typename T, typename Type>
    static void reportObjectMemoryUsage(const T* const&, MemoryObjectInfo* memoryObjectInfo, ...)
    {
        memoryObjectInfo->reportObjectInfo<T>(0, sizeof(T));
    }

    template<typename T>
    static void countNotInstrumentedObject(const T* const&, MemoryObjectInfo*);

    template<typename T> class InstrumentedPointer : public InstrumentedPointerBase {
    public:
        explicit InstrumentedPointer(const T* pointer, MemoryObjectType ownerObjectType) : m_pointer(pointer), m_ownerObjectType(ownerObjectType) { }
        virtual void process(MemoryInstrumentation*) OVERRIDE;

    private:
        const T* m_pointer;
        const MemoryObjectType m_ownerObjectType;
    };

    template<typename T> void addObject(const T& t, MemoryObjectType ownerObjectType)
    {
        OwningTraits<T>::addObject(this, t, ownerObjectType);
    }

    template<typename T> void addInstrumentedObject(const T& t, MemoryObjectType ownerObjectType) { OwningTraits<T>::addInstrumentedObject(this, t, ownerObjectType); }
    template<typename HashMapType> void addHashMap(const HashMapType&, MemoryObjectType, bool contentOnly = false);
    template<typename HashSetType> void addHashSet(const HashSetType&, MemoryObjectType, bool contentOnly = false);
    template<typename CollectionType> void addInstrumentedCollection(const CollectionType&, MemoryObjectType, bool contentOnly = false);
    template<typename MapType> void addInstrumentedMapEntries(const MapType&, MemoryObjectType);
    template<typename MapType> void addInstrumentedMapValues(const MapType&, MemoryObjectType);
    template<typename ListHashSetType> void addListHashSet(const ListHashSetType&, MemoryObjectType, bool contentOnly = false);
    template<typename VectorType> void addVector(const VectorType&, MemoryObjectType, bool contentOnly = false);
    void addRawBuffer(const void* const& buffer, MemoryObjectType ownerObjectType, size_t size)
    {
        if (!buffer || visited(buffer))
            return;
        countObjectSize(ownerObjectType, size);
    }

    template<typename T>
    struct OwningTraits { // Default byReference implementation.
        static void addInstrumentedObject(MemoryInstrumentation* instrumentation, const T& t, MemoryObjectType ownerObjectType)
        {
            instrumentation->addInstrumentedObjectImpl(&t, ownerObjectType, byReference);
        }
        static void addObject(MemoryInstrumentation* instrumentation, const T& t, MemoryObjectType ownerObjectType)
        {
            instrumentation->addObjectImpl(&t, ownerObjectType, byReference);
        }
    };

    template<typename T>
    struct OwningTraits<T*> { // Custom byPointer implementation.
        static void addInstrumentedObject(MemoryInstrumentation* instrumentation, const T* const& t, MemoryObjectType ownerObjectType)
        {
            instrumentation->addInstrumentedObjectImpl(t, ownerObjectType, byPointer);
        }
        static void addObject(MemoryInstrumentation* instrumentation, const T* const& t, MemoryObjectType ownerObjectType)
        {
            instrumentation->addObjectImpl(t, ownerObjectType, byPointer);
        }
    };

    template<typename T> void addInstrumentedObjectImpl(const T* const&, MemoryObjectType, MemoryOwningType);
    template<typename T> void addInstrumentedObjectImpl(const OwnPtr<T>* const&, MemoryObjectType, MemoryOwningType);
    template<typename T> void addInstrumentedObjectImpl(const RefPtr<T>* const&, MemoryObjectType, MemoryOwningType);

    template<typename T> void addObjectImpl(const T* const&, MemoryObjectType, MemoryOwningType);
    template<typename T> void addObjectImpl(const OwnPtr<T>* const&, MemoryObjectType, MemoryOwningType);
    template<typename T> void addObjectImpl(const RefPtr<T>* const&, MemoryObjectType, MemoryOwningType);
};

class MemoryClassInfo {
public:
    template<typename T>
    MemoryClassInfo(MemoryObjectInfo* memoryObjectInfo, const T*, MemoryObjectType objectType = 0, size_t actualSize = 0)
        : m_memoryObjectInfo(memoryObjectInfo)
        , m_memoryInstrumentation(memoryObjectInfo->memoryInstrumentation())
    {
        m_memoryObjectInfo->reportObjectInfo<T>(objectType, actualSize);
        m_objectType = memoryObjectInfo->objectType();
    }

    template<typename M> void addInstrumentedMember(const M& member) { m_memoryInstrumentation->addInstrumentedObject(member, m_objectType); }
    template<typename M> void addMember(const M& member) { m_memoryInstrumentation->addObject(member, m_objectType); }

    template<typename HashMapType> void addHashMap(const HashMapType& map) { m_memoryInstrumentation->addHashMap(map, m_objectType, true); }
    template<typename HashSetType> void addHashSet(const HashSetType& set) { m_memoryInstrumentation->addHashSet(set, m_objectType, true); }
    template<typename HashSetType> void addHashCountedSet(const HashSetType& set) { m_memoryInstrumentation->addHashSet(set, m_objectType, true); }
    template<typename HashSetType> void addInstrumentedHashSet(const HashSetType& set) { m_memoryInstrumentation->addInstrumentedCollection(set, m_objectType, true); }
    template<typename VectorType> void addInstrumentedVector(const VectorType& vector) { m_memoryInstrumentation->addInstrumentedCollection(vector, m_objectType, true); }
    template<typename VectorType> void addInstrumentedVectorPtr(const OwnPtr<VectorType>& vector) { m_memoryInstrumentation->addInstrumentedCollection(*vector, m_objectType, false); }
    template<typename VectorType> void addInstrumentedVectorPtr(const VectorType* const& vector) { m_memoryInstrumentation->addInstrumentedCollection(*vector, m_objectType, false); }
    template<typename MapType> void addInstrumentedMapEntries(const MapType& map) { m_memoryInstrumentation->addInstrumentedMapEntries(map, m_objectType); }
    template<typename MapType> void addInstrumentedMapValues(const MapType& map) { m_memoryInstrumentation->addInstrumentedMapValues(map, m_objectType); }
    template<typename ListHashSetType> void addListHashSet(const ListHashSetType& set) { m_memoryInstrumentation->addListHashSet(set, m_objectType, true); }
    template<typename VectorType> void addVector(const VectorType& vector) { m_memoryInstrumentation->addVector(vector, m_objectType, true); }
    template<typename VectorType> void addVectorPtr(const VectorType* const vector) { m_memoryInstrumentation->addVector(*vector, m_objectType, false); }
    void addRawBuffer(const void* const& buffer, size_t size) { m_memoryInstrumentation->addRawBuffer(buffer, m_objectType, size); }

    void addWeakPointer(void*) { }

private:
    MemoryObjectInfo* m_memoryObjectInfo;
    MemoryInstrumentation* m_memoryInstrumentation;
    MemoryObjectType m_objectType;
};

template<typename T>
void MemoryInstrumentation::addInstrumentedObjectImpl(const T* const& object, MemoryObjectType ownerObjectType, MemoryOwningType owningType)
{
    if (owningType == byReference) {
        MemoryObjectInfo memoryObjectInfo(this, ownerObjectType);
        selectInstrumentationMethod<T>(object, &memoryObjectInfo);
    } else {
        if (!object || visited(object))
            return;
        deferInstrumentedPointer(adoptPtr(new InstrumentedPointer<T>(object, ownerObjectType)));
    }
}

template<typename T>
void MemoryInstrumentation::addInstrumentedObjectImpl(const OwnPtr<T>* const& object, MemoryObjectType ownerObjectType, MemoryOwningType owningType)
{
    if (owningType == byPointer)
        countObjectSize(ownerObjectType, sizeof(*object));
    addInstrumentedObjectImpl(object->get(), ownerObjectType, byPointer);
}

template<typename T>
void MemoryInstrumentation::addInstrumentedObjectImpl(const RefPtr<T>* const& object, MemoryObjectType ownerObjectType, MemoryOwningType owningType)
{
    if (owningType == byPointer)
        countObjectSize(ownerObjectType, sizeof(*object));
    addInstrumentedObjectImpl(object->get(), ownerObjectType, byPointer);
}

template<typename T>
void MemoryInstrumentation::addObjectImpl(const OwnPtr<T>* const& object, MemoryObjectType ownerObjectType, MemoryOwningType owningType)
{
    if (owningType == byPointer && !visited(object))
        countObjectSize(ownerObjectType, sizeof(*object));
    addObjectImpl(object->get(), ownerObjectType, byPointer);
}

template<typename T>
void MemoryInstrumentation::addObjectImpl(const RefPtr<T>* const& object, MemoryObjectType ownerObjectType, MemoryOwningType owningType)
{
    if (owningType == byPointer && !visited(object))
        countObjectSize(ownerObjectType, sizeof(*object));
    addObjectImpl(object->get(), ownerObjectType, byPointer);
}

template<typename T>
void MemoryInstrumentation::addObjectImpl(const T* const& object, MemoryObjectType ownerObjectType, MemoryOwningType owningType)
{
    addInstrumentedObjectImpl(object, ownerObjectType, owningType);
}

template<typename HashMapType>
void MemoryInstrumentation::addHashMap(const HashMapType& hashMap, MemoryObjectType ownerObjectType, bool contentOnly)
{
    if (visited(&hashMap))
        return;
    countObjectSize(ownerObjectType, calculateContainerSize(hashMap, contentOnly));
}

template<typename HashSetType>
void MemoryInstrumentation::addHashSet(const HashSetType& hashSet, MemoryObjectType ownerObjectType, bool contentOnly)
{
    if (visited(&hashSet))
        return;
    countObjectSize(ownerObjectType, calculateContainerSize(hashSet, contentOnly));
}

template<typename CollectionType>
void MemoryInstrumentation::addInstrumentedCollection(const CollectionType& collection, MemoryObjectType ownerObjectType, bool contentOnly)
{
    if (visited(&collection))
        return;
    countObjectSize(ownerObjectType, calculateContainerSize(collection, contentOnly));
    typename CollectionType::const_iterator end = collection.end();
    for (typename CollectionType::const_iterator i = collection.begin(); i != end; ++i)
        addInstrumentedObject(*i, ownerObjectType);
}

template<typename MapType>
void MemoryInstrumentation::addInstrumentedMapEntries(const MapType& map, MemoryObjectType ownerObjectType)
{
    typename MapType::const_iterator end = map.end();
    for (typename MapType::const_iterator i = map.begin(); i != end; ++i) {
        addInstrumentedObject(i->first, ownerObjectType);
        addInstrumentedObject(i->second, ownerObjectType);
    }
}

template<typename MapType>
void MemoryInstrumentation::addInstrumentedMapValues(const MapType& map, MemoryObjectType ownerObjectType)
{
    typename MapType::const_iterator end = map.end();
    for (typename MapType::const_iterator i = map.begin(); i != end; ++i)
        addInstrumentedObject(i->second, ownerObjectType);
}

template<typename ListHashSetType>
void MemoryInstrumentation::addListHashSet(const ListHashSetType& hashSet, MemoryObjectType ownerObjectType, bool contentOnly)
{
    if (visited(&hashSet))
        return;
    size_t size = (contentOnly ? 0 : sizeof(ListHashSetType)) + hashSet.capacity() * sizeof(void*) + hashSet.size() * (sizeof(typename ListHashSetType::ValueType) + 2 * sizeof(void*));
    countObjectSize(ownerObjectType, size);
}

template<typename VectorType>
void MemoryInstrumentation::addVector(const VectorType& vector, MemoryObjectType ownerObjectType, bool contentOnly)
{
    if (!vector.data() || visited(vector.data()))
        return;
    countObjectSize(ownerObjectType, calculateContainerSize(vector, contentOnly));
}

template<typename Container>
size_t MemoryInstrumentation::calculateContainerSize(const Container& container, bool contentOnly)
{
    return (contentOnly ? 0 : sizeof(container)) + container.capacity() * sizeof(typename Container::ValueType);
}

template<typename T>
void MemoryInstrumentation::InstrumentedPointer<T>::process(MemoryInstrumentation* memoryInstrumentation)
{
    MemoryObjectInfo memoryObjectInfo(memoryInstrumentation, m_ownerObjectType);
    selectInstrumentationMethod<T>(m_pointer, &memoryObjectInfo);
    memoryInstrumentation->countObjectSize(memoryObjectInfo.objectType(), memoryObjectInfo.objectSize());
}

} // namespace WTF

#endif // !defined(MemoryInstrumentation_h)
