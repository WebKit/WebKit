/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <atomic>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashTraits.h>
#include <wtf/RawPtrTraits.h>
#include <wtf/SingleThreadIntegralWrapper.h>
#include <wtf/StackTrace.h>
#include <wtf/text/WTFString.h>

#if ASSERT_ENABLED
#include <wtf/Threading.h>
#endif

namespace WTF {

#define CHECKED_POINTER_DEBUG 0

template<typename T, typename PtrTraits>
class CheckedRef {
    WTF_MAKE_FAST_ALLOCATED;
public:

    ~CheckedRef()
    {
        unpoison(*this);
        if (auto* ptr = PtrTraits::exchange(m_ptr, nullptr)) {
#if CHECKED_POINTER_DEBUG
            PtrTraits::unwrap(ptr)->unregisterCheckedPtr(this);
#endif
            PtrTraits::unwrap(ptr)->decrementPtrCount();
        }
    }

    CheckedRef(T& object)
        : m_ptr(&object)
    {
        PtrTraits::unwrap(m_ptr)->incrementPtrCount();
#if CHECKED_POINTER_DEBUG
        PtrTraits::unwrap(m_ptr)->registerCheckedPtr(this);
#endif
    }

    enum AdoptTag { Adopt };
    CheckedRef(T& object, AdoptTag)
        : m_ptr(&object)
    {
#if CHECKED_POINTER_DEBUG
        PtrTraits::unwrap(m_ptr)->registerCheckedPtr(this);
#endif
    }

    ALWAYS_INLINE CheckedRef(const CheckedRef& other)
        : m_ptr { PtrTraits::unwrap(other.m_ptr) }
    {
        auto* ptr = PtrTraits::unwrap(m_ptr);
        ptr->incrementPtrCount();
#if CHECKED_POINTER_DEBUG
        ptr->copyCheckedPtr(&other, this);
#endif
    }

    template<typename OtherType, typename OtherPtrTraits>
    CheckedRef(const CheckedRef<OtherType, OtherPtrTraits>& other)
        : m_ptr { PtrTraits::unwrap(other.m_ptr) }
    {
        auto* ptr = PtrTraits::unwrap(m_ptr);
        ptr->incrementPtrCount();
#if CHECKED_POINTER_DEBUG
        ptr->copyCheckedPtr(&other, this);
#endif
    }

    ALWAYS_INLINE CheckedRef(CheckedRef&& other)
        : m_ptr { other.releasePtr() }
    {
        ASSERT(m_ptr);
#if CHECKED_POINTER_DEBUG
        PtrTraits::unwrap(m_ptr)->moveCheckedPtr(&other, this);
#endif
    }

    template<typename OtherType, typename OtherPtrTraits>
    CheckedRef(CheckedRef<OtherType, OtherPtrTraits>&& other)
        : m_ptr { other.releasePtr() }
    {
        ASSERT(m_ptr);
#if CHECKED_POINTER_DEBUG
        PtrTraits::unwrap(m_ptr)->moveCheckedPtr(&other, this);
#endif
    }

    CheckedRef(HashTableDeletedValueType) : m_ptr(PtrTraits::hashTableDeletedValue()) { }
    bool isHashTableDeletedValue() const { return PtrTraits::isHashTableDeletedValue(m_ptr); }

    CheckedRef(HashTableEmptyValueType) : m_ptr(hashTableEmptyValue()) { }
    bool isHashTableEmptyValue() const { return m_ptr == hashTableEmptyValue(); }
    static T* hashTableEmptyValue() { return nullptr; }

    const T* ptrAllowingHashTableEmptyValue() const { ASSERT(m_ptr || isHashTableEmptyValue()); return PtrTraits::unwrap(m_ptr); }
    T* ptrAllowingHashTableEmptyValue() { ASSERT(m_ptr || isHashTableEmptyValue()); return PtrTraits::unwrap(m_ptr); }

    ALWAYS_INLINE T* ptr() const { return PtrTraits::unwrap(m_ptr); }
    ALWAYS_INLINE T& get() const { ASSERT(ptr()); return *ptr(); }
    ALWAYS_INLINE T* operator->() const { return ptr(); }

    ALWAYS_INLINE operator T&() const { return get(); }
    ALWAYS_INLINE explicit operator bool() const { return get(); }

    CheckedRef& operator=(T& reference)
    {
        unpoison(*this);
        unregisterCheckedPtrIfNecessary();
        CheckedRef copy { reference };
        PtrTraits::swap(m_ptr, copy.m_ptr);
#if CHECKED_POINTER_DEBUG
        PtrTraits::unwrap(m_ptr)->copyCheckedPtr(&copy, this);
#endif
        return *this;
    }

    CheckedRef& operator=(const CheckedRef& other)
    {
        unpoison(*this);
        unregisterCheckedPtrIfNecessary();
        CheckedRef copy { other };
        PtrTraits::swap(m_ptr, copy.m_ptr);
#if CHECKED_POINTER_DEBUG
        PtrTraits::unwrap(m_ptr)->copyCheckedPtr(&copy, this);
#endif
        return *this;
    }

    template<typename OtherType, typename OtherPtrTraits> CheckedRef& operator=(const CheckedRef<OtherType, OtherPtrTraits>& other)
    {
        unpoison(*this);
        unregisterCheckedPtrIfNecessary();
        CheckedRef copy { other };
        PtrTraits::swap(m_ptr, copy.m_ptr);
#if CHECKED_POINTER_DEBUG
        PtrTraits::unwrap(m_ptr)->copyCheckedPtr(&copy, this);
#endif
        return *this;
    }

    CheckedRef& operator=(CheckedRef&& other)
    {
        unpoison(*this);
        unregisterCheckedPtrIfNecessary();
        CheckedRef moved { WTFMove(other) };
        PtrTraits::swap(m_ptr, moved.m_ptr);
#if CHECKED_POINTER_DEBUG
        PtrTraits::unwrap(m_ptr)->copyCheckedPtr(&moved, this);
#endif
        return *this;
    }

    template<typename OtherType, typename OtherPtrTraits> CheckedRef& operator=(CheckedRef<OtherType, OtherPtrTraits>&& other)
    {
        unpoison(*this);
        unregisterCheckedPtrIfNecessary();
        CheckedRef moved { WTFMove(other) };
        PtrTraits::swap(m_ptr, moved.m_ptr);
#if CHECKED_POINTER_DEBUG
        PtrTraits::unwrap(m_ptr)->copyCheckedPtr(&moved, this);
#endif
        return *this;
    }

private:
    template<typename OtherType, typename OtherPtrTraits> friend class CheckedRef;
    template<typename OtherType, typename OtherPtrTraits> friend class CheckedPtr;

    T* releasePtr()
    {
        T* ptr = PtrTraits::exchange(m_ptr, nullptr);
        poison(*this);
        return ptr;
    }

    ALWAYS_INLINE void unregisterCheckedPtrIfNecessary()
    {
#if CHECKED_POINTER_DEBUG
        if (isHashTableDeletedValue() || isHashTableEmptyValue())
            return;

        PtrTraits::unwrap(m_ptr)->unregisterCheckedPtr(this);
#endif
    }

#if ASAN_ENABLED
    template <typename ObjectType>
    void poison(ObjectType& object)
    {
        __asan_poison_memory_region(&object, sizeof(ObjectType));
    }

    template <typename ObjectType>
    void unpoison(ObjectType& object)
    {
        if (__asan_address_is_poisoned(&object))
            __asan_unpoison_memory_region(&object, sizeof(ObjectType));
    }
#else
    template <typename ObjectType> void poison(ObjectType&) { }
    template <typename ObjectType> void unpoison(ObjectType&) { }
#endif

    typename PtrTraits::StorageType m_ptr;
};

template <typename T, typename PtrTraits>
struct GetPtrHelper<CheckedRef<T, PtrTraits>> {
    using PtrType = T*;
    using UnderlyingType = T;
    static T* getPtr(const CheckedRef<T, PtrTraits>& p) { return const_cast<T*>(p.ptr()); }
};

template <typename T, typename U>
struct IsSmartPtr<CheckedRef<T, U>> {
    static constexpr bool value = true;
    static constexpr bool isNullable = true;
};

template<typename ExpectedType, typename ArgType, typename ArgPtrTraits>
inline bool is(CheckedRef<ArgType, ArgPtrTraits>& source)
{
    return is<ExpectedType>(source.get());
}

template<typename ExpectedType, typename ArgType, typename ArgPtrTraits>
inline bool is(const CheckedRef<ArgType, ArgPtrTraits>& source)
{
    return is<ExpectedType>(source.get());
}

template<typename ExpectedType, typename ArgType, typename ArgPtrTraits>
inline ExpectedType& downcast(CheckedRef<ArgType, ArgPtrTraits>& source)
{
    return downcast<ExpectedType>(source.get());
}

template<typename ExpectedType, typename ArgType, typename ArgPtrTraits>
inline const ExpectedType& downcast(const CheckedRef<ArgType, ArgPtrTraits>& source)
{
    return downcast<ExpectedType>(source.get());
}

template<typename ExpectedType, typename ArgType, typename ArgPtrTraits>
inline const ExpectedType& downcast(CheckedRef<const ArgType, ArgPtrTraits>& source)
{
    return downcast<ExpectedType>(source.get());
}

template<typename P> struct CheckedRefHashTraits : SimpleClassHashTraits<CheckedRef<P>> {
    static constexpr bool emptyValueIsZero = true;
    static CheckedRef<P> emptyValue() { return HashTableEmptyValue; }

    template <typename>
    static void constructEmptyValue(CheckedRef<P>& slot)
    {
        new (NotNull, std::addressof(slot)) CheckedRef<P>(HashTableEmptyValue);
    }

    static constexpr bool hasIsEmptyValueFunction = true;
    static bool isEmptyValue(const CheckedRef<P>& value) { return value.isHashTableEmptyValue(); }

    using PeekType = P*;
    static PeekType peek(const CheckedRef<P>& value) { return const_cast<PeekType>(value.ptrAllowingHashTableEmptyValue()); }
    static PeekType peek(P* value) { return value; }

    using TakeType = CheckedPtr<P>;
    static TakeType take(CheckedRef<P>&& value) { return isEmptyValue(value) ? nullptr : CheckedPtr<P>(WTFMove(value)); }
};

template<typename P> struct HashTraits<CheckedRef<P>> : CheckedRefHashTraits<P> { };

template<typename P> struct PtrHash<CheckedRef<P>> : PtrHashBase<CheckedRef<P>, IsSmartPtr<CheckedRef<P>>::value> {
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

template<typename P> struct DefaultHash<CheckedRef<P>> : PtrHash<CheckedRef<P>> { };

template <typename StorageType, typename PtrCounterType> class CanMakeCheckedPtrBase {
public:
    CanMakeCheckedPtrBase() = default;
    CanMakeCheckedPtrBase(CanMakeCheckedPtrBase&&) { }
    CanMakeCheckedPtrBase& operator=(CanMakeCheckedPtrBase&&) { return *this; }
    CanMakeCheckedPtrBase(const CanMakeCheckedPtrBase&) { }
    CanMakeCheckedPtrBase& operator=(const CanMakeCheckedPtrBase&) { return *this; }

    ~CanMakeCheckedPtrBase()
    {
#if CHECKED_POINTER_DEBUG
        if (m_count) {
            for (auto& stackTrace : m_checkedPtrs.values())
                WTFLogAlways("%s", stackTrace->toString().utf8().data());
        }
#endif
        // If you hit this assertion, you can turn on CHECKED_POINTER_DEBUG to help identify
        // which CheckedPtr / CheckedRef outlived the object.
        RELEASE_ASSERT(!m_count);
    }

    PtrCounterType ptrCount() const { return m_count; }
    void incrementPtrCount() const { ++m_count; }
    void decrementPtrCount() const { ASSERT(m_count); --m_count; }

    friend bool operator==(const CanMakeCheckedPtrBase&, const CanMakeCheckedPtrBase&) { return true; }

#if CHECKED_POINTER_DEBUG
    void registerCheckedPtr(const void* pointer) const;

    void copyCheckedPtr(const void* source, const void* destination) const
    {
        auto doCopyCheckedPtr = [&] {
            auto stackTrace = m_checkedPtrs.get(source);
            ASSERT(stackTrace);
            m_checkedPtrs.add(destination, WTFMove(stackTrace));
        };
        if constexpr (std::is_same_v<StorageType, std::atomic<uint32_t>>) {
            Locker locker { m_checkedPtrsLock };
            doCopyCheckedPtr();
        } else
            doCopyCheckedPtr();
    }

    void moveCheckedPtr(const void* source, const void* destination) const
    {
        auto doMoveCheckedPtr = [&] {
            auto stackTrace = m_checkedPtrs.take(source);
            ASSERT(stackTrace);
            m_checkedPtrs.add(destination, WTFMove(stackTrace));
        };
        if constexpr (std::is_same_v<StorageType, std::atomic<uint32_t>>) {
            Locker locker { m_checkedPtrsLock };
            doMoveCheckedPtr();
        } else
            doMoveCheckedPtr();
    }

    void unregisterCheckedPtr(const void* pointer) const
    {
        if constexpr (std::is_same_v<StorageType, std::atomic<uint32_t>>) {
            Locker locker { m_checkedPtrsLock };
            m_checkedPtrs.remove(pointer);
        } else
            m_checkedPtrs.remove(pointer);
    }
#endif // CHECKED_POINTER_DEBUG

private:
#if CHECKED_POINTER_DEBUG
    class SharedStackTrace : public RefCounted<SharedStackTrace> {
    public:
        static Ref<SharedStackTrace> create()
        {
            return adoptRef(*new SharedStackTrace);
        }

        String toString() const { return m_trace->toString(); }

    private:
        SharedStackTrace()
        {
            static constexpr size_t maxFrameToCapture = 8;
            static constexpr size_t framesToSkip = 4;
            m_trace = StackTrace::captureStackTrace(maxFrameToCapture, framesToSkip);
        }

        std::unique_ptr<StackTrace> m_trace;
    };
#endif

    mutable StorageType m_count { 0 };
#if CHECKED_POINTER_DEBUG
    mutable Lock m_checkedPtrsLock;
    mutable HashMap<const void* /* CheckedPtr or CheckedRef */, RefPtr<SharedStackTrace>> m_checkedPtrs;
#endif
};

#if CHECKED_POINTER_DEBUG
template <typename StorageType, typename PtrCounterType>
void CanMakeCheckedPtrBase<StorageType, PtrCounterType>::registerCheckedPtr(const void* pointer) const
{
    if constexpr (std::is_same_v<StorageType, std::atomic<uint32_t>>) {
        Locker locker { m_checkedPtrsLock };
        m_checkedPtrs.add(pointer, SharedStackTrace::create());
    } else
        m_checkedPtrs.add(pointer, SharedStackTrace::create());
}
#endif

using CanMakeCheckedPtr = CanMakeCheckedPtrBase<SingleThreadIntegralWrapper<uint32_t>, uint32_t>;
using CanMakeThreadSafeCheckedPtr = CanMakeCheckedPtrBase<std::atomic<uint32_t>, uint32_t>;

} // namespace WTF

using WTF::CanMakeCheckedPtr;
using WTF::CanMakeThreadSafeCheckedPtr;
using WTF::CheckedRef;
