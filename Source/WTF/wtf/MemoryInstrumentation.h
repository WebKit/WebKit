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

enum MemberType {
    PointerMember,
    ReferenceMember,
    RetainingPointer,
    LastMemberTypeEntry
};

template<typename T> void reportMemoryUsage(const T*, MemoryObjectInfo*);

class MemoryInstrumentationClient {
public:
    virtual ~MemoryInstrumentationClient() { }
    virtual void countObjectSize(const void*, MemoryObjectType, size_t) = 0;
    virtual bool visited(const void*) = 0;
    virtual bool checkCountedObject(const void*) = 0;

    virtual void reportNode(const MemoryObjectInfo&) = 0;
    virtual void reportEdge(const void* target, const char* edgeName, MemberType) = 0;
    virtual void reportLeaf(const MemoryObjectInfo&, const char* edgeName) = 0;
    virtual void reportBaseAddress(const void* base, const void* real) = 0;
    virtual int registerString(const char*) = 0;
};

class MemoryInstrumentation {
public:
    WTF_EXPORT_PRIVATE explicit MemoryInstrumentation(MemoryInstrumentationClient*);
    WTF_EXPORT_PRIVATE virtual ~MemoryInstrumentation();

    template <typename T> void addRootObject(const T& t, MemoryObjectType objectType = 0)
    {
        MemberTypeTraits<T>::addRootObject(this, t, objectType);
        processDeferredObjects();
    }

    template <typename T> void addRootObject(const OwnPtr<T>&, MemoryObjectType = 0); // Link time guard.
    template <typename T> void addRootObject(const RefPtr<T>&, MemoryObjectType = 0); // Link time guard.

protected:
    class WrapperBase {
    public:
        WTF_EXPORT_PRIVATE WrapperBase(MemoryObjectType, const void* pointer);
        virtual ~WrapperBase() { }
        WTF_EXPORT_PRIVATE void process(MemoryInstrumentation*);
        WTF_EXPORT_PRIVATE void processPointer(MemoryInstrumentation*, bool isRoot);
        WTF_EXPORT_PRIVATE void processRootObjectRef(MemoryInstrumentation*);

    protected:
        virtual void callReportMemoryUsage(MemoryObjectInfo*) = 0;
        const void* m_pointer;
        const MemoryObjectType m_ownerObjectType;

    private:
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

    WTF_EXPORT_PRIVATE void reportEdge(const void* target, const char* edgeName, MemberType);

    virtual void deferObject(PassOwnPtr<WrapperBase>) = 0;
    virtual void processDeferredObjects() = 0;

    WTF_EXPORT_PRIVATE static MemoryObjectType getObjectType(MemoryObjectInfo*);

    friend class MemoryObjectInfo;
    friend class MemoryClassInfo;
    template<typename T> friend void reportMemoryUsage(const T*, MemoryObjectInfo*);

    template <typename Type>
    class IsInstrumented {
        class yes {
            char m;
        };

        class no {
            yes m[2];
        };

        struct BaseMixin {
            void reportMemoryUsage(MemoryObjectInfo*) const { }
        };

#if COMPILER(MSVC)
#pragma warning(push)
#pragma warning(disable: 4624) // Disable warning: destructor could not be generated because a base class destructor is inaccessible.
#endif
        struct Base : public Type, public BaseMixin { };
#if COMPILER(MSVC)
#pragma warning(pop)
#endif

        template <typename T, T t> class Helper { };

        template <typename U> static no deduce(U*, Helper<void (BaseMixin::*)(MemoryObjectInfo*) const, &U::reportMemoryUsage>* = 0);
        static yes deduce(...);

    public:
        static const bool result = sizeof(yes) == sizeof(deduce((Base*)(0)));

    };

    template <int>
    struct InstrumentationSelector {
        template <typename T> static void reportObjectMemoryUsage(const T*, MemoryObjectInfo*);
    };

    template<typename T> class Wrapper : public WrapperBase {
    public:
        Wrapper(const T* pointer, MemoryObjectType);

    protected:
        virtual void callReportMemoryUsage(MemoryObjectInfo*) OVERRIDE;
    };

    template<typename T> void addObject(const T& t, MemoryObjectInfo* ownerObjectInfo, const char* edgeName, MemberType memberType)
    {
        MemberTypeTraits<T>::addObject(this, t, ownerObjectInfo, edgeName, memberType);
    }
    void addRawBuffer(const void* buffer, MemoryObjectType ownerObjectType, size_t size, const char* className = 0, const char* edgeName = 0)
    {
        if (!buffer || visited(buffer))
            return;
        countObjectSize(buffer, ownerObjectType, size);
        reportLinkToBuffer(buffer, ownerObjectType, size, className, edgeName);
    }
    WTF_EXPORT_PRIVATE void reportLinkToBuffer(const void* buffer, MemoryObjectType ownerObjectType, size_t, const char* nodeName, const char* edgeName);

    template<typename T>
    struct MemberTypeTraits { // Default ReferenceMember implementation.
        static void addObject(MemoryInstrumentation* instrumentation, const T& t, MemoryObjectInfo* ownerObjectInfo, const char* edgeName, MemberType)
        {
            instrumentation->addObjectImpl(&t, ownerObjectInfo, ReferenceMember, edgeName);
        }

        static void addRootObject(MemoryInstrumentation* instrumentation, const T& t, MemoryObjectType objectType)
        {
            Wrapper<T>(&t, objectType).processRootObjectRef(instrumentation);
        }
    };

    template<typename T>
    struct MemberTypeTraits<T*> { // Custom PointerMember implementation.
        static void addObject(MemoryInstrumentation* instrumentation, const T* const& t, MemoryObjectInfo* ownerObjectInfo, const char* edgeName, MemberType memberType)
        {
            instrumentation->addObjectImpl(t, ownerObjectInfo, memberType != LastMemberTypeEntry ? memberType : PointerMember, edgeName);
        }

        static void addRootObject(MemoryInstrumentation* instrumentation, const T* const& t, MemoryObjectType objectType)
        {
            if (t && !instrumentation->visited(t))
                Wrapper<T>(t, objectType).processPointer(instrumentation, true);
        }
    };

    template<typename T> void addObjectImpl(const T*, MemoryObjectInfo*, MemberType, const char* edgeName);
    template<typename T> void addObjectImpl(const OwnPtr<T>*, MemoryObjectInfo*, MemberType, const char* edgeName);
    template<typename T> void addObjectImpl(const RefPtr<T>*, MemoryObjectInfo*, MemberType, const char* edgeName);

    MemoryInstrumentationClient* m_client;
};

// We are trying to keep the signature of the function as small as possible
// because it significantly affects the binary size.
// We caluclates class name for 624 classes at the moment.
// So one extra byte of the function signature increases the binary size to 624 extra bytes.
#if COMPILER(MSVC)
template <typename T> struct FN {
    static char* fn() { return const_cast<char*>(__FUNCTION__); }
};

template <typename T> char* fn() { return FN<T>::fn(); }
#else
template <typename T> char* fn() { return const_cast<char*>(__PRETTY_FUNCTION__); }
#endif

class MemoryClassInfo {
public:
    template<typename T>
    MemoryClassInfo(MemoryObjectInfo* memoryObjectInfo, const T* pointer, MemoryObjectType objectType = 0, size_t actualSize = sizeof(T))
        : m_memoryObjectInfo(memoryObjectInfo)
        , m_memoryInstrumentation(0)
        , m_objectType(0)
        , m_skipMembers(false)
    {
        init(pointer, fn<T>(), objectType, actualSize);
    }

    template<typename M> void addMember(const M& member, const char* edgeName = 0, MemberType memberType = LastMemberTypeEntry)
    {
        if (!m_skipMembers)
            m_memoryInstrumentation->addObject(member, m_memoryObjectInfo, edgeName, memberType);
    }

    WTF_EXPORT_PRIVATE void addRawBuffer(const void* buffer, size_t, const char* className = 0, const char* edgeName = 0);
    WTF_EXPORT_PRIVATE void addPrivateBuffer(size_t, MemoryObjectType ownerObjectType = 0, const char* className = 0, const char* edgeName = 0);
    WTF_EXPORT_PRIVATE void setCustomAllocation(bool);

    void addWeakPointer(void*) { }
    template<typename M> void ignoreMember(const M&) { }

    WTF_EXPORT_PRIVATE static void callReportObjectInfo(MemoryObjectInfo*, const void* pointer, const char* stringWithClassName, MemoryObjectType, size_t actualSize);

private:
    WTF_EXPORT_PRIVATE void init(const void* pointer, const char* stringWithClassName, MemoryObjectType, size_t actualSize);

    MemoryObjectInfo* m_memoryObjectInfo;
    MemoryInstrumentation* m_memoryInstrumentation;
    MemoryObjectType m_objectType;
    bool m_skipMembers;
};

template <>
template <typename T>
void MemoryInstrumentation::InstrumentationSelector<true>::reportObjectMemoryUsage(const T* object, MemoryObjectInfo* memoryObjectInfo)
{
    object->reportMemoryUsage(memoryObjectInfo);
}

template <>
template <typename T>
void MemoryInstrumentation::InstrumentationSelector<false>::reportObjectMemoryUsage(const T* object, MemoryObjectInfo* memoryObjectInfo)
{
    MemoryClassInfo::callReportObjectInfo(memoryObjectInfo, object, fn<T>(), 0, sizeof(T));
}

template<typename T>
void reportMemoryUsage(const T* object, MemoryObjectInfo* memoryObjectInfo)
{
    MemoryInstrumentation::InstrumentationSelector<MemoryInstrumentation::IsInstrumented<T>::result>::reportObjectMemoryUsage(object, memoryObjectInfo);
}

template<typename T>
void MemoryInstrumentation::addObjectImpl(const T* object, MemoryObjectInfo* ownerObjectInfo, MemberType memberType, const char* edgeName)
{
    if (memberType == PointerMember)
        return;
    if (memberType == ReferenceMember)
        reportMemoryUsage(object, ownerObjectInfo);
    else {
        if (!object)
            return;
        reportEdge(object, edgeName, memberType);
        if (visited(object))
            return;
        deferObject(adoptPtr(new Wrapper<T>(object, getObjectType(ownerObjectInfo))));
    }
}

template<typename T>
void MemoryInstrumentation::addObjectImpl(const OwnPtr<T>* object, MemoryObjectInfo* ownerObjectInfo, MemberType memberType, const char* edgeName)
{
    if (memberType == PointerMember && !visited(object))
        countObjectSize(object, getObjectType(ownerObjectInfo), sizeof(*object));
    addObjectImpl(object->get(), ownerObjectInfo, RetainingPointer, edgeName);
}

template<typename T>
void MemoryInstrumentation::addObjectImpl(const RefPtr<T>* object, MemoryObjectInfo* ownerObjectInfo, MemberType memberType, const char* edgeName)
{
    if (memberType == PointerMember && !visited(object))
        countObjectSize(object, getObjectType(ownerObjectInfo), sizeof(*object));
    addObjectImpl(object->get(), ownerObjectInfo, RetainingPointer, edgeName);
}

template<typename T>
MemoryInstrumentation::Wrapper<T>::Wrapper(const T* pointer, MemoryObjectType ownerObjectType)
    : WrapperBase(ownerObjectType, pointer)
{
}

template<typename T>
void MemoryInstrumentation::Wrapper<T>::callReportMemoryUsage(MemoryObjectInfo* memoryObjectInfo)
{
    reportMemoryUsage(static_cast<const T*>(m_pointer), memoryObjectInfo);
}

// Link time guard for classes with external memory instrumentation.
template<typename T, size_t inlineCapacity> class Vector;
template<typename T, size_t inlineCapacity> void reportMemoryUsage(const Vector<T, inlineCapacity>*, MemoryObjectInfo*);

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg> class HashMap;
template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg> void reportMemoryUsage(const HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg>*, MemoryObjectInfo*);

template<typename ValueArg, typename HashArg, typename TraitsArg> class HashCountedSet;
template<typename ValueArg, typename HashArg, typename TraitsArg> void reportMemoryUsage(const HashCountedSet<ValueArg, HashArg, TraitsArg>*, MemoryObjectInfo*);

template<typename ValueArg, size_t inlineCapacity, typename HashArg> class ListHashSet;
template<typename ValueArg, size_t inlineCapacity, typename HashArg> void reportMemoryUsage(const ListHashSet<ValueArg, inlineCapacity, HashArg>*, MemoryObjectInfo*);

class String;
void reportMemoryUsage(const String*, MemoryObjectInfo*);

class StringImpl;
void reportMemoryUsage(const StringImpl*, MemoryObjectInfo*);

class AtomicString;
void reportMemoryUsage(const AtomicString*, MemoryObjectInfo*);

class CString;
void reportMemoryUsage(const CString*, MemoryObjectInfo*);

class CStringBuffer;
void reportMemoryUsage(const CStringBuffer*, MemoryObjectInfo*);

class ParsedURL;
void reportMemoryUsage(const ParsedURL*, MemoryObjectInfo*);

class URLString;
void reportMemoryUsage(const URLString*, MemoryObjectInfo*);

} // namespace WTF

#endif // !defined(MemoryInstrumentation_h)
