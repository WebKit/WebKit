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

#define DEBUG_POINTER_INSTRUMENTATION 0

namespace WTF {

class MemoryClassInfo;
class MemoryObjectInfo;
class MemoryInstrumentation;

typedef const char* MemoryObjectType;

enum MemoryOwningType {
    byPointer,
    byReference
};

template<typename T> void reportMemoryUsage(const T* const&, MemoryObjectInfo*);

class MemoryInstrumentationClient {
public:
    virtual ~MemoryInstrumentationClient() { }
    virtual void countObjectSize(const void*, MemoryObjectType, size_t) = 0;
    virtual bool visited(const void*) = 0;
    virtual bool checkCountedObject(const void*) = 0;
};

class MemoryInstrumentation {
public:
    WTF_EXPORT_PRIVATE explicit MemoryInstrumentation(MemoryInstrumentationClient*);
    WTF_EXPORT_PRIVATE virtual ~MemoryInstrumentation();

    template <typename T> void addRootObject(const T& t)
    {
        addObject(t, m_rootObjectInfo.get());
        processDeferredInstrumentedPointers();
    }

protected:
    class InstrumentedPointerBase {
    public:
        WTF_EXPORT_PRIVATE explicit InstrumentedPointerBase(MemoryObjectInfo*);
        virtual ~InstrumentedPointerBase() { }
        WTF_EXPORT_PRIVATE void process(MemoryInstrumentation*);

    protected:
        virtual const void* callReportMemoryUsage(MemoryObjectInfo*) = 0;

    private:
        const MemoryObjectType m_ownerObjectType;
#if DEBUG_POINTER_INSTRUMENTATION
        static const int s_maxCallStackSize = 32;
        void* m_callStack[s_maxCallStackSize];
        int m_callStackSize;
#endif
    };

private:
    void countObjectSize(const void* object, MemoryObjectType objectType, size_t size) { m_client->countObjectSize(object, objectType, size); }
    bool visited(const void* pointer) { return m_client->visited(pointer); }
    bool checkCountedObject(const void* pointer) { return m_client->checkCountedObject(pointer); }

    virtual void deferInstrumentedPointer(PassOwnPtr<InstrumentedPointerBase>) = 0;
    virtual void processDeferredInstrumentedPointers() = 0;

    WTF_EXPORT_PRIVATE static MemoryObjectType getObjectType(MemoryObjectInfo*);

    friend class MemoryClassInfo;
    template<typename T> friend void reportMemoryUsage(const T* const&, MemoryObjectInfo*);

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
    static void reportObjectMemoryUsage(const T* const& object, MemoryObjectInfo* memoryObjectInfo, ...)
    {
        callReportObjectInfo(memoryObjectInfo, object, 0, sizeof(T));
    }
    WTF_EXPORT_PRIVATE static void callReportObjectInfo(MemoryObjectInfo*, const void* pointer, MemoryObjectType, size_t objectSize);

    template<typename T> class InstrumentedPointer : public InstrumentedPointerBase {
    public:
        InstrumentedPointer(const T* pointer, MemoryObjectInfo* ownerObjectInfo);

    protected:
        virtual const void* callReportMemoryUsage(MemoryObjectInfo*) OVERRIDE;

    private:
        const T* m_pointer;
    };

    template<typename T> void addObject(const T& t, MemoryObjectInfo* ownerObjectInfo) { OwningTraits<T>::addObject(this, t, ownerObjectInfo); }
    void addRawBuffer(const void* const& buffer, MemoryObjectType ownerObjectType, size_t size)
    {
        if (!buffer || visited(buffer))
            return;
        countObjectSize(buffer, ownerObjectType, size);
    }

    template<typename T>
    struct OwningTraits { // Default byReference implementation.
        static void addObject(MemoryInstrumentation* instrumentation, const T& t, MemoryObjectInfo* ownerObjectInfo)
        {
            instrumentation->addObjectImpl(&t, ownerObjectInfo, byReference);
        }
    };

    template<typename T>
    struct OwningTraits<T*> { // Custom byPointer implementation.
        static void addObject(MemoryInstrumentation* instrumentation, const T* const& t, MemoryObjectInfo* ownerObjectInfo)
        {
            instrumentation->addObjectImpl(t, ownerObjectInfo, byPointer);
        }
    };

    template<typename T> void addObjectImpl(const T* const&, MemoryObjectInfo*, MemoryOwningType);
    template<typename T> void addObjectImpl(const OwnPtr<T>* const&, MemoryObjectInfo*, MemoryOwningType);
    template<typename T> void addObjectImpl(const RefPtr<T>* const&, MemoryObjectInfo*, MemoryOwningType);

    MemoryInstrumentationClient* m_client;
    OwnPtr<MemoryObjectInfo> m_rootObjectInfo;
};

class MemoryClassInfo {
public:
    template<typename T>
    MemoryClassInfo(MemoryObjectInfo* memoryObjectInfo, const T* pointer, MemoryObjectType objectType = 0, size_t actualSize = sizeof(T))
        : m_memoryObjectInfo(memoryObjectInfo)
        , m_memoryInstrumentation(0)
        , m_objectType(0)
    {
        init(pointer, objectType, actualSize);
    }

    template<typename M> void addMember(const M& member) { m_memoryInstrumentation->addObject(member, m_memoryObjectInfo); }
    WTF_EXPORT_PRIVATE void addRawBuffer(const void* const& buffer, size_t);
    WTF_EXPORT_PRIVATE void addPrivateBuffer(size_t, MemoryObjectType ownerObjectType = 0);

    void addWeakPointer(void*) { }

private:
    WTF_EXPORT_PRIVATE void init(const void* pointer, MemoryObjectType, size_t actualSize);

    MemoryObjectInfo* m_memoryObjectInfo;
    MemoryInstrumentation* m_memoryInstrumentation;
    MemoryObjectType m_objectType;
};

template<typename T>
void reportMemoryUsage(const T* const& object, MemoryObjectInfo* memoryObjectInfo)
{
    MemoryInstrumentation::selectInstrumentationMethod<T>(object, memoryObjectInfo);
}

template<typename T>
void MemoryInstrumentation::addObjectImpl(const T* const& object, MemoryObjectInfo* ownerObjectInfo, MemoryOwningType owningType)
{
    if (owningType == byReference)
        reportMemoryUsage(object, ownerObjectInfo);
    else {
        if (!object || visited(object))
            return;
        deferInstrumentedPointer(adoptPtr(new InstrumentedPointer<T>(object, ownerObjectInfo)));
    }
}

template<typename T>
void MemoryInstrumentation::addObjectImpl(const OwnPtr<T>* const& object, MemoryObjectInfo* ownerObjectInfo, MemoryOwningType owningType)
{
    if (owningType == byPointer && !visited(object))
        countObjectSize(object, getObjectType(ownerObjectInfo), sizeof(*object));
    addObjectImpl(object->get(), ownerObjectInfo, byPointer);
}

template<typename T>
void MemoryInstrumentation::addObjectImpl(const RefPtr<T>* const& object, MemoryObjectInfo* ownerObjectInfo, MemoryOwningType owningType)
{
    if (owningType == byPointer && !visited(object))
        countObjectSize(object, getObjectType(ownerObjectInfo), sizeof(*object));
    addObjectImpl(object->get(), ownerObjectInfo, byPointer);
}

template<typename T>
MemoryInstrumentation::InstrumentedPointer<T>::InstrumentedPointer(const T* pointer, MemoryObjectInfo* ownerObjectInfo)
    : InstrumentedPointerBase(ownerObjectInfo)
    , m_pointer(pointer)
{
}

template<typename T>
const void* MemoryInstrumentation::InstrumentedPointer<T>::callReportMemoryUsage(MemoryObjectInfo* memoryObjectInfo)
{
    reportMemoryUsage(m_pointer, memoryObjectInfo);
    return m_pointer;
}

// Link time guard for classes with external memory instrumentation.
template<typename T, size_t inlineCapacity> class Vector;
template<typename T, size_t inlineCapacity> void reportMemoryUsage(const Vector<T, inlineCapacity>* const&, MemoryObjectInfo*);

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg> class HashMap;
template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg> void reportMemoryUsage(const HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg>* const&, MemoryObjectInfo*);

template<typename ValueArg, typename HashArg, typename TraitsArg> class HashCountedSet;
template<typename ValueArg, typename HashArg, typename TraitsArg> void reportMemoryUsage(const HashCountedSet<ValueArg, HashArg, TraitsArg>* const&, MemoryObjectInfo*);

template<typename ValueArg, size_t inlineCapacity, typename HashArg> class ListHashSet;
template<typename ValueArg, size_t inlineCapacity, typename HashArg> void reportMemoryUsage(const ListHashSet<ValueArg, inlineCapacity, HashArg>* const&, MemoryObjectInfo*);

class String;
void reportMemoryUsage(const String* const&, MemoryObjectInfo*);

class StringImpl;
void reportMemoryUsage(const StringImpl* const&, MemoryObjectInfo*);

class AtomicString;
void reportMemoryUsage(const AtomicString* const&, MemoryObjectInfo*);

class CString;
void reportMemoryUsage(const CString* const&, MemoryObjectInfo*);

class CStringBuffer;
void reportMemoryUsage(const CStringBuffer* const&, MemoryObjectInfo*);

class ParsedURL;
void reportMemoryUsage(const ParsedURL* const&, MemoryObjectInfo*);

class URLString;
void reportMemoryUsage(const URLString* const&, MemoryObjectInfo*);

} // namespace WTF

#endif // !defined(MemoryInstrumentation_h)
