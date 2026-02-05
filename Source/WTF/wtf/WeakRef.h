/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

// WeakRef is a non-nullable weak pointer that does not prevent the referenced
// object from being destroyed. Unlike WeakPtr, WeakRef is expected to always
// point to a valid object and will safely crash (via RELEASE_ASSERT) if you
// dereference it or call get() after the referenced object has been destroyed.
// This makes it useful for hardening code where a raw reference (e.g., Foo& m_foo)
// was previously used, and where the reference is expected to remain valid for
// the lifetime of the WeakRef.
//
// WeakRef can only be used with classes that inherit from CanMakeWeakPtr or
// CanMakeWeakPtrWithBitField (which provide the weak pointer implementation).
//
// WeakRef is essentially a convenience wrapper around WeakPtr for cases where
// you expect the pointer to never become null during its usage. If there is a
// possibility that the referenced object may be destroyed while the pointer is
// held, use WeakPtr instead.
//
// Performance note: WeakRef is often less efficient than Ref or CheckedRef
// because it involves an extra level of indirection when dereferencing (it is
// a pointer to a pointer). This can hurt compiler optimizations. Prefer Ref or
// CheckedRef in performance sensitive code.

#include <wtf/GetPtr.h>
#include <wtf/HashTraits.h>
#include <wtf/SingleThreadIntegralWrapper.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Threading.h>
#include <wtf/TypeCasts.h>
#include <wtf/TypeTraits.h>
#include <wtf/WeakPtrImpl.h>

namespace WTF {
// Classes that offer weak pointers should also offer RefPtr or CheckedPtr. Please do not add new exceptions.
template<typename T> struct IsDeprecatedWeakRefSmartPointerException : std::false_type { };

enum class EnableWeakPtrThreadingAssertions : bool { No, Yes };

// Similar to a WeakPtr but it is an error for it to become null. It is useful for hardening when replacing
// things like `Foo& m_foo`. It is similar to CheckedRef but it generates crashes that are more actionable.
template<typename T, typename WeakPtrImpl>
class WeakRef {
public:
    WeakRef(const T& object, EnableWeakPtrThreadingAssertions shouldEnableAssertions = EnableWeakPtrThreadingAssertions::Yes) requires (!IsSmartPtr<T>::value && !std::is_pointer_v<T>)
        : m_impl(object.weakImpl())
#if ASSERT_ENABLED
        , m_shouldEnableAssertions(shouldEnableAssertions == EnableWeakPtrThreadingAssertions::Yes)
#endif
    {
        UNUSED_PARAM(shouldEnableAssertions);
    }

    explicit WeakRef(Ref<WeakPtrImpl>&& impl, EnableWeakPtrThreadingAssertions shouldEnableAssertions = EnableWeakPtrThreadingAssertions::Yes)
        : m_impl(WTF::move(impl))
#if ASSERT_ENABLED
        , m_shouldEnableAssertions(shouldEnableAssertions == EnableWeakPtrThreadingAssertions::Yes)
#endif
    {
        UNUSED_PARAM(shouldEnableAssertions);
    }

    WeakRef(HashTableDeletedValueType) : m_impl(HashTableDeletedValue) { }
    WeakRef(HashTableEmptyValueType) : m_impl(HashTableEmptyValue) { }

    bool isHashTableDeletedValue() const { return m_impl.isHashTableDeletedValue(); }
    bool isHashTableEmptyValue() const { return m_impl.isHashTableEmptyValue(); }

    WeakPtrImpl& impl() const { return m_impl; }
    Ref<WeakPtrImpl> releaseImpl() { return WTF::move(m_impl); }

    T* ptrAllowingHashTableEmptyValue() const
    {
        static_assert(
            HasRefPtrMemberFunctions<T>::value || HasCheckedPtrMemberFunctions<T>::value || IsDeprecatedWeakRefSmartPointerException<std::remove_cv_t<T>>::value,
            "Classes that offer weak pointers should also offer RefPtr or CheckedPtr. Please do not add new exceptions.");

        return !m_impl.isHashTableEmptyValue() ? static_cast<T*>(m_impl->template get<T>()) : nullptr;
    }

    T* ptr() const
    {
        static_assert(
            HasRefPtrMemberFunctions<T>::value || HasCheckedPtrMemberFunctions<T>::value || IsDeprecatedWeakRefSmartPointerException<std::remove_cv_t<T>>::value,
            "Classes that offer weak pointers should also offer RefPtr or CheckedPtr. Please do not add new exceptions.");

        auto* ptr = static_cast<T*>(m_impl->template get<T>());
        RELEASE_ASSERT(ptr);
        return ptr;
    }

    T& get() const
    {
        static_assert(
            HasRefPtrMemberFunctions<T>::value || HasCheckedPtrMemberFunctions<T>::value || IsDeprecatedWeakRefSmartPointerException<std::remove_cv_t<T>>::value,
            "Classes that offer weak pointers should also offer RefPtr or CheckedPtr. Please do not add new exceptions.");

        auto* ptr = static_cast<T*>(m_impl->template get<T>());
        RELEASE_ASSERT(ptr);
        return *ptr;
    }

    operator T&() const { return get(); }

    T* operator->() const
    {
        ASSERT(canSafelyBeUsed());
        return ptr();
    }

    EnableWeakPtrThreadingAssertions enableWeakPtrThreadingAssertions() const
    {
#if ASSERT_ENABLED
        return m_shouldEnableAssertions ? EnableWeakPtrThreadingAssertions::Yes : EnableWeakPtrThreadingAssertions::No;
#else
        return EnableWeakPtrThreadingAssertions::No;
#endif
    }

private:
#if ASSERT_ENABLED
    inline bool canSafelyBeUsed() const
    {
        // FIXME: Our GC threads currently need to get opaque pointers from WeakPtrs and have to be special-cased.
        return !m_impl
            || !m_shouldEnableAssertions
            || (m_impl->wasConstructedOnMainThread() && Thread::mayBeGCThread())
            || m_impl->wasConstructedOnMainThread() == isMainThread();
    }
#endif

    Ref<WeakPtrImpl> m_impl;
#if ASSERT_ENABLED
    bool m_shouldEnableAssertions { true };
#endif
};

template<class T>
    requires (!IsSmartPtr<T>::value && !std::is_pointer_v<T>)
WeakRef(const T& value, EnableWeakPtrThreadingAssertions = EnableWeakPtrThreadingAssertions::Yes) -> WeakRef<T, typename T::WeakPtrImplType>;

template <typename T, typename WeakPtrImpl>
struct GetPtrHelper<WeakRef<T, WeakPtrImpl>> {
    using PtrType = T*;
    using UnderlyingType = T;
    static T* getPtr(const WeakRef<T, WeakPtrImpl>& p) { return const_cast<T*>(p.ptr()); }
};

template <typename T, typename WeakPtrImpl>
struct IsSmartPtr<WeakRef<T, WeakPtrImpl>> {
    static constexpr bool value = true;
    static constexpr bool isNullable = false;
};

template<typename P, typename WeakPtrImpl> struct WeakRefHashTraits : SimpleClassHashTraits<WeakRef<P, WeakPtrImpl>> {
    static constexpr bool emptyValueIsZero = true;
    static WeakRef<P, WeakPtrImpl> emptyValue() { return HashTableEmptyValue; }

    template <typename>
    static void constructEmptyValue(WeakRef<P, WeakPtrImpl>& slot)
    {
        new (NotNull, std::addressof(slot)) WeakRef<P, WeakPtrImpl>(HashTableEmptyValue);
    }

    static constexpr bool hasIsEmptyValueFunction = true;
    static bool isEmptyValue(const WeakRef<P, WeakPtrImpl>& value) { return value.isHashTableEmptyValue(); }

    using PeekType = P*;
    static PeekType peek(const WeakRef<P, WeakPtrImpl>& value) { return const_cast<PeekType>(value.ptrAllowingHashTableEmptyValue()); }
    static PeekType peek(P* value) { return value; }

    using TakeType = WeakPtr<P, WeakPtrImpl>;
    static TakeType take(WeakRef<P, WeakPtrImpl>&& value) { return isEmptyValue(value) ? nullptr : WeakPtr<P, WeakPtrImpl>(WTF::move(value)); }
};

template<typename P, typename WeakPtrImpl> struct HashTraits<WeakRef<P, WeakPtrImpl>> : WeakRefHashTraits<P, WeakPtrImpl> { };

template<typename P, typename WeakPtrImpl> struct PtrHash<WeakRef<P, WeakPtrImpl>> : PtrHashBase<WeakRef<P, WeakPtrImpl>, IsSmartPtr<WeakRef<P, WeakPtrImpl>>::value> {
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

template<typename P, typename WeakPtrImpl> struct DefaultHash<WeakRef<P, WeakPtrImpl>> : PtrHash<WeakRef<P, WeakPtrImpl>> { };

template<typename T> using SingleThreadWeakRef = WeakRef<T, SingleThreadWeakPtrImpl>;

template<typename ExpectedType, typename ArgType, typename WeakPtrImpl>
inline bool is(WeakRef<ArgType, WeakPtrImpl>& source)
{
    return is<ExpectedType>(source.get());
}

template<typename ExpectedType, typename ArgType, typename WeakPtrImpl>
inline bool is(const WeakRef<ArgType, WeakPtrImpl>& source)
{
    return is<ExpectedType>(source.get());
}

template<typename Target, typename Source, typename WeakPtrImpl>
inline WeakRef<match_constness_t<Source, Target>, WeakPtrImpl> downcast(WeakRef<Source, WeakPtrImpl> source)
{
    static_assert(!std::same_as<Source, Target>, "Unnecessary cast to same type");
    static_assert(std::derived_from<Target, Source>, "Should be a downcast");
    RELEASE_ASSERT(is<Target>(source));
    return WeakRef<match_constness_t<Source, Target>, WeakPtrImpl> { unsafeRefDowncast<match_constness_t<Source, Target>>(source.releaseImpl()), source.enableWeakPtrThreadingAssertions() };
}

template<typename Target, typename Source, typename WeakPtrImpl>
inline WeakPtr<match_constness_t<Source, Target>, WeakPtrImpl> dynamicDowncast(WeakRef<Source, WeakPtrImpl> source)
{
    static_assert(!std::same_as<Source, Target>, "Unnecessary cast to same type");
    static_assert(std::derived_from<Target, Source>, "Should be a downcast");
    if (!is<Target>(source))
        return nullptr;
    return WeakPtr<match_constness_t<Source, Target>, WeakPtrImpl> { unsafeRefDowncast<match_constness_t<Source, Target>>(source.releaseImpl()), source.enableWeakPtrThreadingAssertions() };
}

template<typename T, typename WeakPtrImpl, typename PtrTraits = RawPtrTraits<T>>
    requires HasRefPtrMemberFunctions<T>::value
ALWAYS_INLINE CLANG_POINTER_CONVERSION Ref<T, PtrTraits> protect(const WeakRef<T, WeakPtrImpl>& weakRef)
{
    return Ref<T, PtrTraits>(weakRef.get());
}

template<typename T, typename WeakPtrImpl, typename CheckedPtrTraits = RawPtrTraits<T>>
    requires (HasCheckedPtrMemberFunctions<T>::value && !HasRefPtrMemberFunctions<T>::value)
ALWAYS_INLINE CLANG_POINTER_CONVERSION CheckedRef<T, CheckedPtrTraits> protect(const WeakRef<T, WeakPtrImpl>& weakRef)
{
    return CheckedRef<T, CheckedPtrTraits>(weakRef.get());
}

} // namespace WTF

using WTF::EnableWeakPtrThreadingAssertions;
using WTF::SingleThreadWeakRef;
using WTF::WeakRef;
