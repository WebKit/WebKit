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
        LastTypeEntry
    };

    template <typename T> void reportInstrumentedObject(const T&);
    template <typename T> void reportObject(const T&) { }
    template <typename T> void reportInstrumentedPointer(const T*);
    template <typename T> void reportPointer(const T* object, ObjectType objectType)
    {
        if (!object || visited(object))
            return;
        countObjectSize(objectType, sizeof(T));
    }
    template <typename HashMapType> void reportHashMap(const HashMapType&, ObjectType, bool contentOnly = false);
    template <typename HashSetType> void reportHashSet(const HashSetType&, ObjectType, bool contentOnly = false);
    template <typename ListHashSetType> void reportListHashSet(const ListHashSetType&, ObjectType, bool contentOnly = false);
    template <typename VectorType> void reportVector(const VectorType&, ObjectType, bool contentOnly = false);

protected:
    class InstrumentedPointerBase {
    public:
        virtual ~InstrumentedPointerBase() { }

        virtual void process(MemoryInstrumentation*) = 0;
    };

private:
    friend class MemoryObjectInfo;

    template <typename T>
    class InstrumentedPointer : public InstrumentedPointerBase {
    public:
        explicit InstrumentedPointer(const T* pointer) : m_pointer(pointer) { }

        virtual void process(MemoryInstrumentation*) OVERRIDE;

    private:
        const T* m_pointer;
    };

    virtual void reportString(ObjectType, const String&) = 0;
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

    template <typename P>
    void reportInstrumentedPointer(const P* memberPointer)
    {
        m_memoryInstrumentation->reportInstrumentedPointer(memberPointer);
    }

    template <typename P>
    void reportPointer(const P* pointer, MemoryInstrumentation::ObjectType objectType)
    {
        m_memoryInstrumentation->reportPointer(pointer, objectType);
    }

    template <typename T>
    void reportInstrumentedObject(const T& memberObject)
    {
        m_memoryInstrumentation->reportInstrumentedObject(memberObject);
    }

    template <typename T>
    void reportObject(const T& object) { m_memoryInstrumentation->reportObject(object); }

    template <typename T>
    void reportObjectInfo(const T*, MemoryInstrumentation::ObjectType objectType)
    {
        if (m_objectType != MemoryInstrumentation::Other)
            return;
        m_objectType = objectType;
        m_objectSize = sizeof(T);
    }

    template <typename HashMapType>
    void reportHashMap(const HashMapType& map)
    {
        m_memoryInstrumentation->reportHashMap(map, objectType(), true);
    }

    template <typename HashSetType>
    void reportHashSet(const HashSetType& set)
    {
        m_memoryInstrumentation->reportHashSet(set, objectType(), true);
    }

    template <typename ListHashSetType>
    void reportListHashSet(const ListHashSetType& set)
    {
        m_memoryInstrumentation->reportListHashSet(set, objectType(), true);
    }

    template <typename VectorType>
    void reportVector(const VectorType& vector)
    {
        m_memoryInstrumentation->reportVector(vector, objectType(), true);
    }

    void reportString(const String& string)
    {
        m_memoryInstrumentation->reportString(objectType(), string);
    }

    MemoryInstrumentation::ObjectType objectType() const { return m_objectType; }
    size_t objectSize() const { return m_objectSize; }

    MemoryInstrumentation* memoryInstrumentation() { return m_memoryInstrumentation; }

 private:
    MemoryInstrumentation* m_memoryInstrumentation;
    MemoryInstrumentation::ObjectType m_objectType;
    size_t m_objectSize;
};

template <typename T>
void MemoryInstrumentation::reportInstrumentedPointer(const T* const object)
{
    if (!object || visited(object))
        return;
    deferInstrumentedPointer(adoptPtr(new InstrumentedPointer<T>(object)));
}

template<typename T>
void MemoryInstrumentation::reportInstrumentedObject(const T& object)
{
    if (visited(&object))
        return;
    MemoryObjectInfo memoryObjectInfo(this);
    object.reportMemoryUsage(&memoryObjectInfo);
}

template<typename HashMapType>
void MemoryInstrumentation::reportHashMap(const HashMapType& hashMap, ObjectType objectType, bool contentOnly)
{
    size_t size = (contentOnly ? 0 : sizeof(HashMapType)) + hashMap.capacity() * sizeof(typename HashMapType::ValueType);
    countObjectSize(objectType, size);
}

template<typename HashSetType>
void MemoryInstrumentation::reportHashSet(const HashSetType& hashSet, ObjectType objectType, bool contentOnly)
{
    if (visited(&hashSet))
        return;
    size_t size = (contentOnly ? 0 : sizeof(HashSetType)) + hashSet.capacity() * sizeof(typename HashSetType::ValueType);
    countObjectSize(objectType, size);
}

template<typename ListHashSetType>
void MemoryInstrumentation::reportListHashSet(const ListHashSetType& hashSet, ObjectType objectType, bool contentOnly)
{
    if (visited(&hashSet))
        return;
    size_t size = (contentOnly ? 0 : sizeof(ListHashSetType)) + hashSet.capacity() * sizeof(void*) + hashSet.size() * (sizeof(typename ListHashSetType::ValueType) + 2 * sizeof(void*));
    countObjectSize(objectType, size);
}

template <typename VectorType>
void MemoryInstrumentation::reportVector(const VectorType& vector, ObjectType objectType, bool contentOnly)
{
    if (visited(vector.data()))
        return;
    size_t size = (contentOnly ? 0 : sizeof(VectorType)) + vector.capacity() * sizeof(typename VectorType::ValueType);
    countObjectSize(objectType, size);
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
