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
#include <stdio.h>

#include <wtf/Assertions.h>
#include <wtf/Forward.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

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
        LastTypeEntry
    };

    template <typename T> void addInstrumentedMember(const T& t)
    {
        OwningTraits<T>::addInstrumentedMember(this, t);
    }

    template <typename T> void addMember(const T& t, ObjectType objectType)
    {
        OwningTraits<T>::addMember(this, t, objectType);
    }

    template <typename HashMapType> void addHashMap(const HashMapType&, ObjectType, bool contentOnly = false);
    template <typename HashSetType> void addHashSet(const HashSetType&, ObjectType, bool contentOnly = false);
    template <typename HashSetType> void addInstrumentedHashSet(const HashSetType&, ObjectType, bool contentOnly = false);
    template <typename ListHashSetType> void addListHashSet(const ListHashSetType&, ObjectType, bool contentOnly = false);
    template <typename VectorType> void addVector(const VectorType&, ObjectType, bool contentOnly = false);
    void addRawBuffer(const void* const& buffer, ObjectType objectType, size_t size)
    {
        if (!buffer || visited(buffer))
            return;
        countObjectSize(objectType, size);
    }

protected:
    enum OwningType {
        byPointer,
        byReference
    };

    template <typename T>
    struct OwningTraits { // Default byReference implementation.
        static void addInstrumentedMember(MemoryInstrumentation* instrumentation, const T& t) { instrumentation->addInstrumentedMemberImpl(&t, byReference); }
        static void addMember(MemoryInstrumentation* instrumentation, const T& t, MemoryInstrumentation::ObjectType objectType) { instrumentation->addMemberImpl(&t, objectType, byReference); }
    };

    template <typename T>
    struct OwningTraits<T*> { // Custom byPointer implementation.
        static void addInstrumentedMember(MemoryInstrumentation* instrumentation, const T* const& t) { instrumentation->addInstrumentedMemberImpl(t, byPointer); }
        static void addMember(MemoryInstrumentation* instrumentation, const T* const& t, MemoryInstrumentation::ObjectType objectType) { instrumentation->addMemberImpl(t, objectType, byPointer); }
    };

    template <typename T> void addInstrumentedMemberImpl(const T* const&, OwningType);
    template <typename T> void addInstrumentedMemberImpl(const OwnPtr<T>* const& object, MemoryInstrumentation::OwningType owningType) { addInstrumentedMemberImpl(object->get(), owningType); }
    template <typename T> void addInstrumentedMemberImpl(const RefPtr<T>* const& object, MemoryInstrumentation::OwningType owningType) { addInstrumentedMemberImpl(object->get(), owningType); }

    template <typename T>
    void addMemberImpl(const T* const& object, ObjectType objectType, OwningType owningType)
    {
        if (!object || visited(object))
            return;
        if (owningType == byReference)
            return;
        countObjectSize(objectType, sizeof(T));
    }

    class InstrumentedPointerBase {
    public:
        virtual ~InstrumentedPointerBase() { }

        virtual void process(MemoryInstrumentation*) = 0;
    };

    template <typename Container>
    size_t calculateContainerSize(const Container& container, bool contentOnly = false)
    {
        return (contentOnly ? 0 : sizeof(container)) + container.capacity() * sizeof(typename Container::ValueType);
    }

private:
    template <typename T> friend class MemoryClassInfo;
    template <typename T>
    class InstrumentedPointer : public InstrumentedPointerBase {
    public:
        explicit InstrumentedPointer(const T* pointer) : m_pointer(pointer) { }

        virtual void process(MemoryInstrumentation*) OVERRIDE;

    private:
        const T* m_pointer;
    };

    virtual void addString(const String&, ObjectType) = 0;
    virtual void countObjectSize(ObjectType, size_t) = 0;
    virtual void deferInstrumentedPointer(PassOwnPtr<InstrumentedPointerBase>) = 0;
    virtual bool visited(const void*) = 0;
};

class MemoryObjectInfo {
public:
    MemoryObjectInfo(MemoryInstrumentation* memoryInstrumentation)
        : m_memoryInstrumentation(memoryInstrumentation)
        , m_objectType(MemoryInstrumentation::Other)
        , m_objectSize(0)
     { }

    MemoryInstrumentation::ObjectType objectType() const { return m_objectType; }
    size_t objectSize() const { return m_objectSize; }

    MemoryInstrumentation* memoryInstrumentation() { return m_memoryInstrumentation; }

private:
    template <typename T> friend class MemoryClassInfo;

    template <typename T> void reportObjectInfo(const T*, MemoryInstrumentation::ObjectType objectType)
    {
        if (m_objectType != MemoryInstrumentation::Other)
            return;
        m_objectType = objectType;
        m_objectSize = sizeof(T);
    }

    MemoryInstrumentation* m_memoryInstrumentation;
    MemoryInstrumentation::ObjectType m_objectType;
    size_t m_objectSize;
};

// Link time guard for string members. They produce link error is a string is reported via addMember.
template <> void MemoryInstrumentation::addMemberImpl<AtomicString>(const AtomicString* const&, MemoryInstrumentation::ObjectType, MemoryInstrumentation::OwningType);
template <> void MemoryInstrumentation::addMemberImpl<String>(const String* const&, MemoryInstrumentation::ObjectType, MemoryInstrumentation::OwningType);


template <typename T>
void MemoryInstrumentation::addInstrumentedMemberImpl(const T* const& object, MemoryInstrumentation::OwningType owningType)
{
    if (!object || visited(object))
        return;
    if (owningType == byReference) {
        MemoryObjectInfo memoryObjectInfo(this);
        object->reportMemoryUsage(&memoryObjectInfo);
    } else
        deferInstrumentedPointer(adoptPtr(new InstrumentedPointer<T>(object)));
}


template <typename T>
class MemoryClassInfo {
public:
    MemoryClassInfo(MemoryObjectInfo* memoryObjectInfo, const T* ptr, MemoryInstrumentation::ObjectType objectType)
        : m_memoryObjectInfo(memoryObjectInfo)
        , m_memoryInstrumentation(memoryObjectInfo->memoryInstrumentation())
    {
        m_memoryObjectInfo->reportObjectInfo(ptr, objectType);
        m_objectType = memoryObjectInfo->objectType();
    }

    template <typename P> void visitBaseClass(const P* ptr) { ptr->P::reportMemoryUsage(m_memoryObjectInfo); }

    template <typename M> void addInstrumentedMember(const M& member) { m_memoryInstrumentation->addInstrumentedMember(member); }
    template <typename M> void addMember(const M& member) { m_memoryInstrumentation->addMember(member, m_objectType); }

    template <typename HashMapType> void addHashMap(const HashMapType& map) { m_memoryInstrumentation->addHashMap(map, m_objectType, true); }
    template <typename HashSetType> void addHashSet(const HashSetType& set) { m_memoryInstrumentation->addHashSet(set, m_objectType, true); }
    template <typename HashSetType> void addInstrumentedHashSet(const HashSetType& set) { m_memoryInstrumentation->addInstrumentedHashSet(set, m_objectType, true); }
    template <typename ListHashSetType> void addListHashSet(const ListHashSetType& set) { m_memoryInstrumentation->addListHashSet(set, m_objectType, true); }
    template <typename VectorType> void addVector(const VectorType& vector) { m_memoryInstrumentation->addVector(vector, m_objectType, true); }
    void addRawBuffer(const void* const& buffer, size_t size) { m_memoryInstrumentation->addRawBuffer(buffer, m_objectType, size); }

    void addString(const String& string) { m_memoryInstrumentation->addString(string, m_objectType); }
    void addString(const AtomicString& string) { m_memoryInstrumentation->addString((const String&)string, m_objectType); }

private:
    MemoryObjectInfo* m_memoryObjectInfo;
    MemoryInstrumentation* m_memoryInstrumentation;
    MemoryInstrumentation::ObjectType m_objectType;
};

template<typename HashMapType>
void MemoryInstrumentation::addHashMap(const HashMapType& hashMap, ObjectType objectType, bool contentOnly)
{
    countObjectSize(objectType, calculateContainerSize(hashMap, contentOnly));
}

template<typename HashSetType>
void MemoryInstrumentation::addHashSet(const HashSetType& hashSet, ObjectType objectType, bool contentOnly)
{
    if (visited(&hashSet))
        return;
    countObjectSize(objectType, calculateContainerSize(hashSet, contentOnly));
}

template<typename HashSetType>
void MemoryInstrumentation::addInstrumentedHashSet(const HashSetType& hashSet, ObjectType objectType, bool contentOnly)
{
    if (visited(&hashSet))
        return;
    countObjectSize(objectType, calculateContainerSize(hashSet, contentOnly));
    for (typename HashSetType::const_iterator i = hashSet.begin(); i != hashSet.end(); ++i)
        addInstrumentedMember(*i);
}

template<typename ListHashSetType>
void MemoryInstrumentation::addListHashSet(const ListHashSetType& hashSet, ObjectType objectType, bool contentOnly)
{
    if (visited(&hashSet))
        return;
    size_t size = (contentOnly ? 0 : sizeof(ListHashSetType)) + hashSet.capacity() * sizeof(void*) + hashSet.size() * (sizeof(typename ListHashSetType::ValueType) + 2 * sizeof(void*));
    countObjectSize(objectType, size);
}

template <typename VectorType>
void MemoryInstrumentation::addVector(const VectorType& vector, ObjectType objectType, bool contentOnly)
{
    if (!vector.data() || visited(vector.data()))
        return;
    countObjectSize(objectType, calculateContainerSize(vector, contentOnly));
}

template<typename T>
void MemoryInstrumentation::InstrumentedPointer<T>::process(MemoryInstrumentation* memoryInstrumentation)
{
    MemoryObjectInfo memoryObjectInfo(memoryInstrumentation);
    m_pointer->reportMemoryUsage(&memoryObjectInfo);
    memoryInstrumentation->countObjectSize(memoryObjectInfo.objectType(), memoryObjectInfo.objectSize());
}


} // namespace WebCore

#endif // !defined(MemoryInstrumentation_h)
