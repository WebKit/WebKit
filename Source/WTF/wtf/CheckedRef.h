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
#include <wtf/HashTraits.h>
#include <wtf/RawPtrTraits.h>

#if ASSERT_ENABLED
#include <wtf/Threading.h>
#endif

namespace WTF {

template<typename T, typename PtrTraits>
class CheckedRef {
    WTF_MAKE_FAST_ALLOCATED;
public:

    ~CheckedRef()
    {
        unpoison(*this);
        if (auto* ptr = PtrTraits::exchange(m_ptr, nullptr))
            PtrTraits::unwrap(ptr)->decrementPtrCount();
    }

    CheckedRef(T& object)
        : m_ptr(&object)
    {
        PtrTraits::unwrap(m_ptr)->incrementPtrCount();
    }

    ALWAYS_INLINE CheckedRef(const CheckedRef& other)
        : m_ptr { PtrTraits::unwrap(other.m_ptr) }
    {
        PtrTraits::unwrap(m_ptr)->incrementPtrCount();
    }

    template<typename OtherType, typename OtherPtrTraits>
    CheckedRef(const CheckedRef<OtherType, OtherPtrTraits>& other)
        : m_ptr { PtrTraits::unwrap(other.m_ptr) }
    {
        PtrTraits::unwrap(m_ptr)->incrementPtrCount();
    }

    ALWAYS_INLINE CheckedRef(CheckedRef&& other)
        : m_ptr { other.releasePtr() }
    {
        ASSERT(m_ptr);
    }

    template<typename OtherType, typename OtherPtrTraits>
    CheckedRef(CheckedRef<OtherType, OtherPtrTraits>&& other)
        : m_ptr { other.releasePtr() }
    {
        ASSERT(m_ptr);
    }

    CheckedRef(HashTableDeletedValueType) : m_ptr(PtrTraits::hashTableDeletedValue()) { }
    bool isHashTableDeletedValue() const { return PtrTraits::isHashTableDeletedValue(m_ptr); }

    CheckedRef(HashTableEmptyValueType) : m_ptr(hashTableEmptyValue()) { }
    bool isHashTableEmptyValue() const { return m_ptr == hashTableEmptyValue(); }
    static T* hashTableEmptyValue() { return nullptr; }

    const T* ptrAllowingHashTableEmptyValue() const { ASSERT(m_ptr || isHashTableEmptyValue()); return PtrTraits::unwrap(m_ptr); }
    T* ptrAllowingHashTableEmptyValue() { ASSERT(m_ptr || isHashTableEmptyValue()); return PtrTraits::unwrap(m_ptr); }

    ALWAYS_INLINE const T* ptr() const { return PtrTraits::unwrap(m_ptr); }
    ALWAYS_INLINE T* ptr() { return PtrTraits::unwrap(m_ptr); }
    ALWAYS_INLINE const T& get() const { ASSERT(ptr()); return *ptr(); }
    ALWAYS_INLINE T& get() { ASSERT(ptr()); return *ptr(); }
    ALWAYS_INLINE const T* operator->() const { return ptr(); }
    ALWAYS_INLINE T* operator->() { return ptr(); }

    ALWAYS_INLINE operator const T&() const { return get(); }
    ALWAYS_INLINE operator T&() { return get(); }
    ALWAYS_INLINE bool operator!() const { return !get(); }

    CheckedRef& operator=(T& reference)
    {
        unpoison(*this);
        CheckedRef copy { reference };
        PtrTraits::swap(m_ptr, copy.m_ptr);
        return *this;
    }

    CheckedRef& operator=(const CheckedRef& other)
    {
        unpoison(*this);
        CheckedRef copy { other };
        PtrTraits::swap(m_ptr, copy.m_ptr);
        return *this;
    }

    template<typename OtherType, typename OtherPtrTraits> CheckedRef& operator=(const CheckedRef<OtherType, OtherPtrTraits>& other)
    {
        unpoison(*this);
        CheckedRef copy { other };
        PtrTraits::swap(m_ptr, copy.m_ptr);
        return *this;
    }

    CheckedRef& operator=(CheckedRef&& other)
    {
        unpoison(*this);
        CheckedRef moved { WTFMove(other) };
        PtrTraits::swap(m_ptr, moved.m_ptr);
        return *this;
    }

    template<typename OtherType, typename OtherPtrTraits> CheckedRef& operator=(CheckedRef<OtherType, OtherPtrTraits>&& other)
    {
        unpoison(*this);
        CheckedRef moved { WTFMove(other) };
        PtrTraits::swap(m_ptr, moved.m_ptr);
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
    typedef T* PtrType;
    static T* getPtr(const CheckedRef<T, PtrTraits>& p) { return const_cast<T*>(p.ptr()); }
};

template <typename T, typename U>
struct IsSmartPtr<CheckedRef<T, U>> {
    static constexpr bool value = true;
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
    ~CanMakeCheckedPtrBase() { RELEASE_ASSERT(!m_count); }

    PtrCounterType ptrCount() const { return m_count; }
    void incrementPtrCount() const { ++m_count; }
    void decrementPtrCount() const { ASSERT(m_count); --m_count; }

private:
    mutable StorageType m_count { 0 };
};

template <typename IntegralType>
class SingleThreadIntegralWrapper {
public:
    SingleThreadIntegralWrapper(IntegralType);

    operator IntegralType() const;
    bool operator!() const;
    SingleThreadIntegralWrapper& operator++();
    SingleThreadIntegralWrapper& operator--();

private:
#if ASSERT_ENABLED && !USE(WEB_THREAD)
    void assertThread() const { ASSERT(m_thread.ptr() == &Thread::current()); }
#else
    constexpr void assertThread() const { }
#endif

    IntegralType m_value;
#if ASSERT_ENABLED && !USE(WEB_THREAD)
    Ref<Thread> m_thread;
#endif
};

template <typename IntegralType>
inline SingleThreadIntegralWrapper<IntegralType>::SingleThreadIntegralWrapper(IntegralType value)
    : m_value { value }
#if ASSERT_ENABLED && !USE(WEB_THREAD)
    , m_thread { Thread::current() }
#endif
{ }

template <typename IntegralType>
inline SingleThreadIntegralWrapper<IntegralType>::operator IntegralType() const
{
    assertThread();
    return m_value;
}

template <typename IntegralType>
inline bool SingleThreadIntegralWrapper<IntegralType>::operator!() const
{
    assertThread();
    return !m_value;
}

template <typename IntegralType>
inline SingleThreadIntegralWrapper<IntegralType>& SingleThreadIntegralWrapper<IntegralType>::operator++()
{
    assertThread();
    m_value++;
    return *this;
}

template <typename IntegralType>
inline SingleThreadIntegralWrapper<IntegralType>& SingleThreadIntegralWrapper<IntegralType>::operator--()
{
    assertThread();
    m_value--;
    return *this;
}

using CanMakeCheckedPtr = CanMakeCheckedPtrBase<SingleThreadIntegralWrapper<uint16_t>, uint16_t>;
using CanMakeThreadSafeCheckedPtr = CanMakeCheckedPtrBase<std::atomic<uint16_t>, uint16_t>;

} // namespace WTF

using WTF::CanMakeCheckedPtr;
using WTF::CanMakeThreadSafeCheckedPtr;
using WTF::CheckedRef;
