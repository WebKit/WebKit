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

#include <memory>
#include <wtf/CheckedRef.h>
#include <wtf/RawPtrTraits.h>
#include <wtf/TypeTraits.h>

namespace WTF {

/**
 * @brief A nullable smart pointer that prevents use-after-free by crashing instead.
 *
 * When an object is destroyed while CheckedPtr pointers still reference it, the
 * object's memory is zeroed out (turning it into a "zombie") and then leaked.
 * When the next CheckedPtr to the object goes out of scope, the CheckedPtr
 * crashes safely (via RELEASE_ASSERT), showing you a backtrace to the code that
 * held a pointer too long.
 *
 * CheckedPtr can only be used with heap-allocated classes that inherit from
 * CanMakeCheckedPtr, CanMakeThreadSafeCheckedPtr, or AbstractCanMakeCheckedPtr
 * (which provide the checked pointer implementation). These classes must also
 * override their delete operator using the WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR()
 * macro to enable the zombie mechanism.
 *
 * If you expect the pointer to never be null during its usage, consider using
 * CheckedRef instead, which provides clearer non-nullable semantics.
 *
 * @note CheckedPtr may introduce RELEASE_ASSERT crashes even in cases where
 * there is no actual use-after-free. The crash indicates that a pointer became
 * stale (the referenced object was destroyed), not that there was an attempt
 * to use the stale pointer.
 *
 * @note CheckedPtr is more efficient than WeakPtr because it does not involve
 * an extra level of indirection when dereferencing (WeakPtr is a pointer to a
 * pointer). This makes CheckedPtr a better choice for performance sensitive
 * code where the weak reference semantics of WeakPtr are not needed. If you
 * are looking for the semantics of a weak pointer but without the extra level
 * of indirection, you may consider using InlineWeakPtr.
 */
template<typename T, typename PtrTraits>
class CheckedPtr {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(CheckedPtr);
public:

    constexpr CheckedPtr()
        : m_ptr(nullptr)
    { }

    constexpr CheckedPtr(std::nullptr_t)
        : m_ptr(nullptr)
    { }

    ALWAYS_INLINE CheckedPtr(T* ptr)
        : m_ptr { ptr }
    {
        refIfNotNull();
    }

    ALWAYS_INLINE CheckedPtr(const CheckedPtr& other)
        : m_ptr { other.m_ptr }
    {
        refIfNotNull();
    }

    ALWAYS_INLINE CheckedPtr(CheckedPtr&& other)
        : m_ptr { PtrTraits::exchange(other.m_ptr, nullptr) }
    {
    }

    ALWAYS_INLINE ~CheckedPtr()
    {
        derefIfNotNull();
    }

    template<typename OtherType, typename OtherPtrTraits> CheckedPtr(const CheckedPtr<OtherType, OtherPtrTraits>& other)
        : CheckedPtr(OtherPtrTraits::unwrap(other.m_ptr))
    { }

    template<typename OtherType, typename OtherPtrTraits> CheckedPtr(CheckedPtr<OtherType, OtherPtrTraits>&& other)
        : m_ptr { OtherPtrTraits::exchange(other.m_ptr, nullptr) }
    {
    }

    CheckedPtr(CheckedRef<T, PtrTraits>& other)
        : CheckedPtr(PtrTraits::unwrap(other.m_ptr))
    { }

    template<typename OtherType, typename OtherPtrTraits> CheckedPtr(const CheckedRef<OtherType, OtherPtrTraits>& other)
        : CheckedPtr(OtherPtrTraits::unwrap(other.m_ptr))
    { }

    CheckedPtr(CheckedRef<T, PtrTraits>&& other)
        : m_ptr { other.releasePtr() }
    {
        ASSERT(get());
    }

    template<typename OtherType, typename OtherPtrTraits> CheckedPtr(CheckedRef<OtherType, OtherPtrTraits>&& other)
        : m_ptr { other.releasePtr() }
    {
        ASSERT(get());
    }

    CheckedPtr(HashTableDeletedValueType)
        : m_ptr(PtrTraits::hashTableDeletedValue())
    { }

    bool isHashTableDeletedValue() const { return PtrTraits::isHashTableDeletedValue(m_ptr); }

    ALWAYS_INLINE explicit operator bool() const { return PtrTraits::unwrap(m_ptr); }

    ALWAYS_INLINE T* get() const LIFETIME_BOUND { return PtrTraits::unwrap(m_ptr); }
    ALWAYS_INLINE T* unsafeGet() const { return PtrTraits::unwrap(m_ptr); }
    ALWAYS_INLINE operator T*() const LIFETIME_BOUND { return PtrTraits::unwrap(m_ptr); }
    ALWAYS_INLINE T& operator*() const LIFETIME_BOUND { RELEASE_ASSERT(m_ptr); return *get(); }
    ALWAYS_INLINE T* operator->() const LIFETIME_BOUND { RELEASE_ASSERT(m_ptr); return get(); }

    CheckedRef<T> releaseNonNull()
    {
        RELEASE_ASSERT(m_ptr);
        auto& ptr = *PtrTraits::unwrap(std::exchange(m_ptr, nullptr));
        return CheckedRef { ptr, CheckedRef<T>::Adopt };
    }

    bool operator==(const T* other) const { return m_ptr == other; }
    template<typename U> bool operator==(U* other) const { return m_ptr == other; }

    bool operator==(const CheckedPtr& other) const { return m_ptr == other.m_ptr; }

    template<typename OtherType, typename OtherPtrTraits>
    bool operator==(const CheckedPtr<OtherType, OtherPtrTraits>& other) const { return m_ptr == other.m_ptr; }

    CheckedPtr& operator=(std::nullptr_t)
    {
        derefIfNotNull();
        m_ptr = nullptr;
        return *this;
    }

    CheckedPtr& operator=(T* ptr)
    {
        CheckedPtr copy { ptr };
        PtrTraits::swap(m_ptr, copy.m_ptr);
        return *this;
    }

    CheckedPtr& operator=(const CheckedPtr& other)
    {
        CheckedPtr copy { other };
        PtrTraits::swap(m_ptr, copy.m_ptr);
        return *this;
    }

    template<typename OtherType, typename OtherPtrTraits> CheckedPtr& operator=(const CheckedPtr<OtherType, OtherPtrTraits>& other)
    {
        CheckedPtr copy { other };
        PtrTraits::swap(m_ptr, copy.m_ptr);
        return *this;
    }

    CheckedPtr& operator=(CheckedPtr&& other)
    {
        CheckedPtr moved { WTF::move(other) };
        PtrTraits::swap(m_ptr, moved.m_ptr);
        return *this;
    }

    template<typename OtherType, typename OtherPtrTraits> CheckedPtr& operator=(CheckedPtr<OtherType, OtherPtrTraits>&& other)
    {
        CheckedPtr moved { WTF::move(other) };
        PtrTraits::swap(m_ptr, moved.m_ptr);
        return *this;
    }

private:
    template<typename OtherType, typename OtherPtrTraits> friend class CheckedPtr;

    ALWAYS_INLINE void refIfNotNull()
    {
        if (T* ptr = PtrTraits::unwrap(m_ptr); ptr) [[likely]]
            ptr->incrementCheckedPtrCount();
    }

    ALWAYS_INLINE void derefIfNotNull()
    {
        if (T* ptr = PtrTraits::unwrap(m_ptr); ptr) [[likely]]
            ptr->decrementCheckedPtrCount();
    }

    typename PtrTraits::StorageType m_ptr;
};

template <typename T, typename PtrTraits>
struct GetPtrHelper<CheckedPtr<T, PtrTraits>> {
    using PtrType = T*;
    using UnderlyingType = T;
    static T* getPtr(const CheckedPtr<T, PtrTraits>& p) { return const_cast<T*>(p.get()); }
};

template <typename T, typename U>
struct IsSmartPtr<CheckedPtr<T, U>> {
    static constexpr bool value = true;
    static constexpr bool isNullable = false;
};

template<typename T, typename PtrTraits = RawPtrTraits<T>>
    requires (HasCheckedPtrMemberFunctions<T>::value && !HasRefPtrMemberFunctions<T>::value)
ALWAYS_INLINE CLANG_POINTER_CONVERSION CheckedPtr<T, PtrTraits> protect(T* ptr)
{
    return CheckedPtr<T, PtrTraits>(ptr);
}

template<typename T, typename PtrTraits>
ALWAYS_INLINE CLANG_POINTER_CONVERSION CheckedPtr<T, PtrTraits> protect(const CheckedPtr<T, PtrTraits>& ptr)
{
    return ptr;
}

template<typename T, typename PtrTraits>
CheckedPtr<T, PtrTraits> protect(CheckedPtr<T, PtrTraits>&&)
{
    static_assert(WTF::unreachableForType<T>, "Calling protect() on an rvalue is unnecessary; the caller already owns the value.");
}

template<typename T, typename Deleter, typename PtrTraits = RawPtrTraits<T>>
    requires (HasCheckedPtrMemberFunctions<T>::value && !HasRefPtrMemberFunctions<T>::value)
ALWAYS_INLINE CLANG_POINTER_CONVERSION CheckedPtr<T, PtrTraits> protect(const std::unique_ptr<T, Deleter>& ptr)
{
    return CheckedPtr<T, PtrTraits>(ptr.get());
}

template<typename ExpectedType, typename ArgType, typename ArgPtrTraits>
inline bool is(CheckedPtr<ArgType, ArgPtrTraits>& source)
{
    return is<ExpectedType>(source.get());
}

template<typename ExpectedType, typename ArgType, typename ArgPtrTraits>
inline bool is(const CheckedPtr<ArgType, ArgPtrTraits>& source)
{
    return is<ExpectedType>(source.get());
}

template<typename... ExpectedTypes, typename ArgType, typename ArgPtrTraits>
inline bool isAnyOf(CheckedPtr<ArgType, ArgPtrTraits>& source)
{
    return is<ExpectedTypes...>(source.get());
}

template<typename... ExpectedTypes, typename ArgType, typename ArgPtrTraits>
inline bool isAnyOf(const CheckedPtr<ArgType, ArgPtrTraits>& source)
{
    return isAnyOf<ExpectedTypes...>(source.get());
}

template<typename ExpectedType, typename ArgType, typename ArgPtrTraits>
inline ExpectedType& downcast(CheckedPtr<ArgType, ArgPtrTraits>& source)
{
    return downcast<ExpectedType>(source.get());
}

template<typename ExpectedType, typename ArgType, typename ArgPtrTraits>
inline ExpectedType& downcast(const CheckedPtr<ArgType, ArgPtrTraits>& source)
{
    return downcast<ExpectedType>(source.get());
}

template<typename ExpectedType, typename ArgType, typename ArgPtrTraits>
inline const ExpectedType& downcast(CheckedPtr<const ArgType, ArgPtrTraits>& source)
{
    return downcast<ExpectedType>(source.get());
}

template<typename ExpectedType, typename ArgType, typename ArgPtrTraits>
inline CheckedPtr<match_constness_t<ArgType, ExpectedType>> dynamicDowncast(CheckedPtr<ArgType, ArgPtrTraits>& source)
{
    return dynamicDowncast<ExpectedType>(source.get());
}

template<typename ExpectedType, typename ArgType, typename ArgPtrTraits>
inline CheckedPtr<match_constness_t<ArgType, ExpectedType>> dynamicDowncast(const CheckedPtr<ArgType, ArgPtrTraits>& source)
{
    return dynamicDowncast<ExpectedType>(source.get());
}

template<typename ExpectedType, typename ArgType, typename ArgPtrTraits>
inline const CheckedPtr<match_constness_t<ArgType, ExpectedType>> dynamicDowncast(CheckedPtr<const ArgType, ArgPtrTraits>& source)
{
    return dynamicDowncast<ExpectedType>(source.get());
}

template<typename P> struct HashTraits<CheckedPtr<P>> : SimpleClassHashTraits<CheckedPtr<P>> {
    static P* emptyValue() { return nullptr; }
    static bool isEmptyValue(const CheckedPtr<P>& value) { return !value; }

    typedef P* PeekType;
    static PeekType peek(const CheckedPtr<P>& value) { return value.get(); }
    static PeekType peek(P* value) { return value; }

    static void customDeleteBucket(CheckedPtr<P>& value)
    {
        // See unique_ptr's customDeleteBucket() for an explanation.
        ASSERT(!SimpleClassHashTraits<CheckedPtr<P>>::isDeletedValue(value));
        auto valueToBeDestroyed = WTF::move(value);
        SimpleClassHashTraits<CheckedPtr<P>>::constructDeletedValue(value);
    }
};

template<typename P> struct DefaultHash<CheckedPtr<P>> : PtrHash<CheckedPtr<P>> { };

template<typename> struct PackedPtrTraits;
template<typename T> using PackedCheckedPtr = CheckedPtr<T, PackedPtrTraits<T>>;

} // namespace WTF

using WTF::CheckedPtr;
using WTF::PackedCheckedPtr;
using WTF::protect;

