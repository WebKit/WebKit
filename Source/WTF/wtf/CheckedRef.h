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

    enum AdoptTag { Adopt };
    CheckedRef(T& object, AdoptTag)
        : m_ptr(&object)
    {
    }

    ALWAYS_INLINE CheckedRef(const CheckedRef& other)
        : m_ptr { PtrTraits::unwrap(other.m_ptr) }
    {
        auto* ptr = PtrTraits::unwrap(m_ptr);
        ptr->incrementPtrCount();
    }

    template<typename OtherType, typename OtherPtrTraits>
    CheckedRef(const CheckedRef<OtherType, OtherPtrTraits>& other)
        : m_ptr { PtrTraits::unwrap(other.m_ptr) }
    {
        auto* ptr = PtrTraits::unwrap(m_ptr);
        ptr->incrementPtrCount();
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

    ALWAYS_INLINE T* ptr() const
    {
        // In normal execution, a CheckedPtr always points to an object with a non-zero ptrCount().
        // When it detects a dangling pointer, WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR scribbles an object with zeroes and then leaks it.
        // When we check ptrCountWithoutThreadCheck() here, we're checking for a scribbled object.
        ASSERT(PtrTraits::unwrap(m_ptr)->ptrCountWithoutThreadCheck());
        return PtrTraits::unwrap(m_ptr);
    }

    ALWAYS_INLINE T& get() const
    {
        ASSERT(ptr());
        return *ptr();
    }

    ALWAYS_INLINE T* operator->() const { return ptr(); }

    ALWAYS_INLINE operator T&() const { return get(); }
    ALWAYS_INLINE explicit operator bool() const { return get(); }

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

enum class DefaultedOperatorEqual : bool { No, Yes };

template <typename StorageType, typename PtrCounterType, DefaultedOperatorEqual defaultedOperatorEqual> class CanMakeCheckedPtrBase {
public:
    CanMakeCheckedPtrBase() = default;
    CanMakeCheckedPtrBase(CanMakeCheckedPtrBase&&) { }
    CanMakeCheckedPtrBase& operator=(CanMakeCheckedPtrBase&&) { return *this; }
    CanMakeCheckedPtrBase(const CanMakeCheckedPtrBase&) { }
    CanMakeCheckedPtrBase& operator=(const CanMakeCheckedPtrBase&) { return *this; }

    ~CanMakeCheckedPtrBase() = default;

    PtrCounterType ptrCount() const { return m_count; }
    void incrementPtrCount() const { ++m_count; }
    ALWAYS_INLINE void decrementPtrCount() const
    {
        // In normal execution, a CheckedPtr always points to an object with a non-zero ptrCount().
        // When it detects a dangling pointer, WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR scribbles an object with zeroes and then leaks it.
        // When we check ptrCountWithoutThreadCheck() here, we're checking for a scribbled object.
        RELEASE_ASSERT(ptrCountWithoutThreadCheck());
        --m_count;
    }

    ALWAYS_INLINE PtrCounterType ptrCountWithoutThreadCheck() const
    {
        if constexpr (std::is_same_v<StorageType, std::atomic<uint32_t>>)
            return m_count;
        else
            return m_count.valueWithoutThreadCheck();
    }

    friend bool operator==(const CanMakeCheckedPtrBase&, const CanMakeCheckedPtrBase&)
    {
        static_assert(defaultedOperatorEqual == DefaultedOperatorEqual::Yes, "Derived class should opt-in when defaulting operator==, or invalid/undefined comparison should be reworked/defined");
        return true;
    }

private:
    mutable StorageType m_count { 0 };
};

template<typename T, DefaultedOperatorEqual defaultedOperatorEqual = DefaultedOperatorEqual::No>
class CanMakeCheckedPtr : public CanMakeCheckedPtrBase<SingleThreadIntegralWrapper<uint32_t>, uint32_t, defaultedOperatorEqual> {
public:
    ~CanMakeCheckedPtr()
    {
        static_assert(std::is_same<typename T::WTFIsFastAllocated, int>::value, "Objects that use CanMakeCheckedPtr must use FastMalloc (WTF_MAKE_FAST_ALLOCATED)");
        static_assert(std::is_same<typename T::WTFDidOverrideDeleteForCheckedPtr, int>::value, "Objects that use CanMakeCheckedPtr must use WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR");
    }
};

template<typename T, DefaultedOperatorEqual defaultedOperatorEqual = DefaultedOperatorEqual::No> class CanMakeThreadSafeCheckedPtr : public CanMakeCheckedPtrBase<std::atomic<uint32_t>, uint32_t, defaultedOperatorEqual> {
public:
    ~CanMakeThreadSafeCheckedPtr()
    {
        static_assert(std::is_same<typename T::WTFIsFastAllocated, int>::value, "Objects that use CanMakeCheckedPtr must use FastMalloc (WTF_MAKE_FAST_ALLOCATED)");
        static_assert(std::is_same<typename T::WTFDidOverrideDeleteForCheckedPtr, int>::value, "Objects that use CanMakeCheckedPtr must use WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR");
    }
};

} // namespace WTF

using WTF::CanMakeCheckedPtr;
using WTF::CanMakeThreadSafeCheckedPtr;
using WTF::CheckedRef;
