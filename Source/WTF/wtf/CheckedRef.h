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
#include <wtf/SwiftBridging.h>
#include <wtf/TypeTraits.h>
#include <wtf/UniqueRef.h>

namespace WTF {

#define USING_CAN_MAKE_CHECKEDPTR(BASE) \
    using BASE::checkedPtrCount; \
    using BASE::checkedPtrCountWithoutThreadCheck; \
    using BASE::incrementCheckedPtrCount; \
    using BASE::decrementCheckedPtrCount

/**
 * @brief CheckedRef is the non-nullable variant of CheckedPtr.
 *
 * See CheckedPtr for full documentation on checked pointers, including usage requirements and
 * the zombie mechanism for detecting use-after-free.
 */
template<typename T, typename PtrTraits>
class CheckedRef {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(CheckedRef);
public:

    ~CheckedRef()
    {
        unpoison(*this);
        if (auto* ptr = PtrTraits::unwrap(m_ptr))
            ptr->decrementCheckedPtrCount();
    }

    CheckedRef(T& object)
        : m_ptr(&object)
    {
        object.incrementCheckedPtrCount();
    }

    enum AdoptTag { Adopt };
    CheckedRef(T& object, AdoptTag)
        : m_ptr(&object)
    {
    }

    ALWAYS_INLINE CheckedRef(const CheckedRef& other)
        : m_ptr { other.m_ptr }
    {
        PtrTraits::unwrap(m_ptr)->incrementCheckedPtrCount();
    }

    template<typename OtherType, typename OtherPtrTraits>
    CheckedRef(const CheckedRef<OtherType, OtherPtrTraits>& other)
        : m_ptr { OtherPtrTraits::unwrap(other.m_ptr) }
    {
        PtrTraits::unwrap(m_ptr)->incrementCheckedPtrCount();
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

    template<typename X, typename WeakPtrImplType>
    CheckedRef(const WeakRef<X, WeakPtrImplType>& other) requires std::is_convertible_v<X*, T*>
        : CheckedRef(other.get())
    { }

    CheckedRef(HashTableDeletedValueType) : m_ptr(PtrTraits::hashTableDeletedValue()) { }
    bool isHashTableDeletedValue() const { return PtrTraits::isHashTableDeletedValue(m_ptr); }

    CheckedRef(HashTableEmptyValueType) : m_ptr(hashTableEmptyValue()) { }
    bool isHashTableEmptyValue() const { return m_ptr == hashTableEmptyValue(); }
    static T* hashTableEmptyValue() { return nullptr; }

    const T* ptrAllowingHashTableEmptyValue() const { ASSERT(m_ptr || isHashTableEmptyValue()); return PtrTraits::unwrap(m_ptr); }
    T* ptrAllowingHashTableEmptyValue() { ASSERT(m_ptr || isHashTableEmptyValue()); return PtrTraits::unwrap(m_ptr); }

    ALWAYS_INLINE T* ptr() const LIFETIME_BOUND
    {
        // In normal execution, a CheckedPtr always points to an object with a non-zero checkedPtrCount().
        // When it detects a dangling pointer, WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR scribbles an object with zeroes and then leaks it.
        // When we check checkedPtrCountWithoutThreadCheck() here, we're checking for a scribbled object.
        ASSERT(PtrTraits::unwrap(m_ptr)->checkedPtrCountWithoutThreadCheck());
        return PtrTraits::unwrap(m_ptr);
    }

    ALWAYS_INLINE T& get() const LIFETIME_BOUND
    {
        RELEASE_ASSERT(m_ptr);
        return *ptr();
    }

    ALWAYS_INLINE T* operator->() const LIFETIME_BOUND
    {
        RELEASE_ASSERT(m_ptr);
        return ptr();
    }

    ALWAYS_INLINE operator T&() const LIFETIME_BOUND { return get(); }
    ALWAYS_INLINE explicit operator bool() const { return ptr(); }

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
        CheckedRef moved { WTF::move(other) };
        PtrTraits::swap(m_ptr, moved.m_ptr);
        return *this;
    }

    template<typename OtherType, typename OtherPtrTraits> CheckedRef& operator=(CheckedRef<OtherType, OtherPtrTraits>&& other)
    {
        unpoison(*this);
        CheckedRef moved { WTF::move(other) };
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

template<typename X, typename WeakPtrImplType> CheckedRef(WeakRef<X, WeakPtrImplType>&) -> CheckedRef<X>;
template<typename X, typename WeakPtrImplType> CheckedRef(const WeakRef<X, WeakPtrImplType>&) -> CheckedRef<X>;

template <typename T, typename PtrTraits>
struct GetPtrHelper<CheckedRef<T, PtrTraits>> {
    using PtrType = T*;
    using UnderlyingType = T;
    static T* getPtr(const CheckedRef<T, PtrTraits>& p) { return const_cast<T*>(p.ptr()); }
};

template <typename T, typename U>
struct IsSmartPtr<CheckedRef<T, U>> {
    static constexpr bool value = true;
    static constexpr bool isNullable = false;
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

template<typename... ExpectedTypes, typename ArgType, typename ArgPtrTraits>
inline bool isAnyOf(CheckedRef<ArgType, ArgPtrTraits>& source)
{
    return isAnyOf<ExpectedTypes...>(source.get());
}

template<typename... ExpectedTypes, typename ArgType, typename ArgPtrTraits>
inline bool isAnyOf(const CheckedRef<ArgType, ArgPtrTraits>& source)
{
    return isAnyOf<ExpectedTypes...>(source.get());
}

template<typename ExpectedType, typename ArgType, typename ArgPtrTraits>
inline ExpectedType& downcast(CheckedRef<ArgType, ArgPtrTraits>& source LIFETIME_BOUND)
{
    return downcast<ExpectedType>(source.get());
}

template<typename ExpectedType, typename ArgType, typename ArgPtrTraits>
inline ExpectedType& downcast(const CheckedRef<ArgType, ArgPtrTraits>& source LIFETIME_BOUND)
{
    return downcast<ExpectedType>(source.get());
}

template<typename ExpectedType, typename ArgType, typename ArgPtrTraits>
inline const ExpectedType& downcast(CheckedRef<const ArgType, ArgPtrTraits>& source LIFETIME_BOUND)
{
    return downcast<ExpectedType>(source.get());
}

template<typename ExpectedType, typename ArgType, typename ArgPtrTraits>
inline CheckedPtr<match_constness_t<ArgType, ExpectedType>> dynamicDowncast(CheckedRef<ArgType, ArgPtrTraits>& source)
{
    return dynamicDowncast<ExpectedType>(source.get());
}

template<typename ExpectedType, typename ArgType, typename ArgPtrTraits>
inline CheckedPtr<match_constness_t<ArgType, ExpectedType>> dynamicDowncast(const CheckedRef<ArgType, ArgPtrTraits>& source)
{
    return dynamicDowncast<ExpectedType>(source.get());
}

template<typename ExpectedType, typename ArgType, typename ArgPtrTraits>
inline const CheckedPtr<match_constness_t<ArgType, ExpectedType>> dynamicDowncast(CheckedRef<const ArgType, ArgPtrTraits>& source)
{
    return dynamicDowncast<ExpectedType>(source.get());
}

template<typename T, typename PtrTraits = RawPtrTraits<T>>
    requires (HasCheckedPtrMemberFunctions<T>::value && !HasRefPtrMemberFunctions<T>::value)
ALWAYS_INLINE CLANG_POINTER_CONVERSION CheckedRef<T, PtrTraits> protect(T& reference)
{
    return CheckedRef<T, PtrTraits>(reference);
}

template<typename T, typename PtrTraits>
ALWAYS_INLINE CLANG_POINTER_CONVERSION CheckedRef<T, PtrTraits> protect(const CheckedRef<T, PtrTraits>& reference)
{
    return reference;
}

template<typename T, typename PtrTraits>
CheckedRef<T, PtrTraits> protect(CheckedRef<T, PtrTraits>&&)
{
    static_assert(WTF::unreachableForType<T>, "Calling protect() on an rvalue is unnecessary; the caller already owns the value.");
}

template<typename T, typename PtrTraits = RawPtrTraits<T>>
    requires (HasCheckedPtrMemberFunctions<T>::value && !HasRefPtrMemberFunctions<T>::value)
ALWAYS_INLINE CLANG_POINTER_CONVERSION CheckedRef<T, PtrTraits> protect(const UniqueRef<T>& reference)
{
    return CheckedRef<T, PtrTraits>(reference.get());
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
    static TakeType take(CheckedRef<P>&& value) { return isEmptyValue(value) ? nullptr : CheckedPtr<P>(WTF::move(value)); }
};

template<typename P> struct HashTraits<CheckedRef<P>> : CheckedRefHashTraits<P> { };

template<typename P> struct PtrHash<CheckedRef<P>> : PtrHashBase<CheckedRef<P>, IsSmartPtr<CheckedRef<P>>::value> {
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

template<typename P> struct DefaultHash<CheckedRef<P>> : PtrHash<CheckedRef<P>> { };

enum class DefaultedOperatorEqual : bool { No, Yes };

// DO NOT make use of this enum in new code. An object which supports CanMakeCheckedPtr must be heap allocated on its own.
enum class CheckedPtrDeleteCheckException : bool { No, Yes };

template<typename T>
concept AtomicLike = requires(T t) {
    t.load(std::memory_order_relaxed);
    t.fetch_add(1, std::memory_order_relaxed);
    t.fetch_sub(1, std::memory_order_relaxed);
};

template <typename StorageType, typename PtrCounterType, typename DeletionFlagType, CheckedPtrDeleteCheckException deleteException> class CanMakeCheckedPtrBase {
public:
    CanMakeCheckedPtrBase() = default;
    CanMakeCheckedPtrBase(CanMakeCheckedPtrBase&&) { }
    CanMakeCheckedPtrBase& operator=(CanMakeCheckedPtrBase&&) { return *this; }
    CanMakeCheckedPtrBase(const CanMakeCheckedPtrBase&) { }
    CanMakeCheckedPtrBase& operator=(const CanMakeCheckedPtrBase&) { return *this; }

    ~CanMakeCheckedPtrBase()
    {
        ASSERT_WITH_SECURITY_IMPLICATION(m_didBeginDeletion || deleteException == CheckedPtrDeleteCheckException::Yes);
    }

    PtrCounterType checkedPtrCount() const { return m_checkedPtrCount; }
    void incrementCheckedPtrCount() const
    {
        if constexpr (AtomicLike<StorageType>)
            m_checkedPtrCount.fetch_add(1, std::memory_order_relaxed);
        else
            ++m_checkedPtrCount;
    }
    ALWAYS_INLINE void decrementCheckedPtrCount() const
    {
        // In normal execution, a CheckedPtr always points to an object with a non-zero checkedPtrCount().
        // When it detects a dangling pointer, WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR scribbles an object with zeroes and then leaks it.
        // When we check the count here, we're checking for a scribbled object.
        if constexpr (AtomicLike<StorageType>) {
            // Combine the acquire load (zombie check) with the release decrement into a
            // single acq_rel fetch_sub. The returned old value lets us detect a zero count.
            if (!m_checkedPtrCount.fetch_sub(1, std::memory_order_acq_rel)) [[unlikely]]
                crashDueToCheckedPtrToDeadObject();
        } else {
            if (!m_checkedPtrCount.valueWithoutThreadCheck()) [[unlikely]]
                crashDueToCheckedPtrToDeadObject();
            --m_checkedPtrCount;
        }
    }

    ALWAYS_INLINE PtrCounterType checkedPtrCountWithoutThreadCheck() const
    {
        if constexpr (AtomicLike<StorageType>)
            return m_checkedPtrCount.load(std::memory_order_acquire);
        else
            return m_checkedPtrCount.valueWithoutThreadCheck();
    }

    void setDidBeginCheckedPtrDeletion()
    {
#if ASSERT_ENABLED || ENABLE(SECURITY_ASSERTIONS)
        m_didBeginDeletion = true;
#endif
    }

private:
    static NO_RETURN_DUE_TO_CRASH NEVER_INLINE void crashDueToCheckedPtrToDeadObject()
    {
        CRASH();
    }

    mutable StorageType m_checkedPtrCount { 0 };
#if ASSERT_ENABLED || ENABLE(SECURITY_ASSERTIONS)
    DeletionFlagType m_didBeginDeletion { false };
#endif
} SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT;

template<typename T, DefaultedOperatorEqual defaultedOperatorEqual = DefaultedOperatorEqual::No, CheckedPtrDeleteCheckException deleteException = CheckedPtrDeleteCheckException::No>
class CanMakeCheckedPtr : public CanMakeCheckedPtrBase<SingleThreadIntegralWrapper<uint32_t>, uint32_t, bool, deleteException> {
public:
    ~CanMakeCheckedPtr()
    {
        static_assert(std::is_same<typename T::WTFIsFastMallocAllocated, int>::value, "Objects that use CanMakeCheckedPtr must use TZoneMalloc (WTF_MAKE_TZONE_ALLOCATED or one of its variants)");
        static_assert(std::is_same<typename T::WTFDidOverrideDeleteForCheckedPtr, int>::value, "Objects that use CanMakeCheckedPtr must use WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR");
    }

    friend bool operator==(const CanMakeCheckedPtr&, const CanMakeCheckedPtr&)
    {
        static_assert(defaultedOperatorEqual == DefaultedOperatorEqual::Yes, "Derived class should opt-in when defaulting operator==, or invalid/undefined comparison should be reworked/defined");
        return true;
    }
};

template<typename T, DefaultedOperatorEqual defaultedOperatorEqual = DefaultedOperatorEqual::No, CheckedPtrDeleteCheckException deleteException = CheckedPtrDeleteCheckException::No>
class CanMakeThreadSafeCheckedPtr : public CanMakeCheckedPtrBase<std::atomic<uint32_t>, uint32_t, std::atomic<bool>, deleteException> {
public:
    ~CanMakeThreadSafeCheckedPtr()
    {
        static_assert(std::is_same<typename T::WTFIsFastMallocAllocated, int>::value, "Objects that use CanMakeCheckedPtr must use TZoneMalloc (WTF_MAKE_TZONE_ALLOCATED or one of its variants)");
        static_assert(std::is_same<typename T::WTFDidOverrideDeleteForCheckedPtr, int>::value, "Objects that use CanMakeCheckedPtr must use WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR");
    }

    friend bool operator==(const CanMakeThreadSafeCheckedPtr&, const CanMakeThreadSafeCheckedPtr&)
    {
        static_assert(defaultedOperatorEqual == DefaultedOperatorEqual::Yes, "Derived class should opt-in when defaulting operator==, or invalid/undefined comparison should be reworked/defined");
        return true;
    }
};

} // namespace WTF

using WTF::CanMakeCheckedPtr;
using WTF::CanMakeThreadSafeCheckedPtr;
using WTF::CheckedRef;
using WTF::protect;
