/*
 * Copyright (C) 2013 Google, Inc. All rights reserved.
 * Copyright (C) 2015-2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/CanMakeWeakPtr.h>
#include <wtf/CompactRefPtrTuple.h>
#include <wtf/Packed.h>
#include <wtf/WeakPtrFactory.h>
#include <wtf/WeakRef.h>

namespace WTF {

template<typename, typename, typename = DefaultWeakPtrImpl> class WeakHashMap;
template<typename, typename = DefaultWeakPtrImpl, EnableWeakPtrThreadingAssertions = EnableWeakPtrThreadingAssertions::Yes> class WeakHashSet;
template <typename, typename = DefaultWeakPtrImpl, EnableWeakPtrThreadingAssertions = EnableWeakPtrThreadingAssertions::Yes> class WeakListHashSet;

template<typename T, typename WeakPtrImpl, typename PtrTraits> class WeakPtr {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WeakPtr() { }
    WeakPtr(std::nullptr_t) { }
    template<typename U> WeakPtr(const WeakPtr<U, WeakPtrImpl, PtrTraits>&);
    template<typename U> WeakPtr(WeakPtr<U, WeakPtrImpl, PtrTraits>&&);

    template<typename U> WeakPtr(const WeakRef<U, WeakPtrImpl>&);
    template<typename U> WeakPtr(WeakRef<U, WeakPtrImpl>&&);

    template<typename = std::enable_if_t<!IsSmartPtr<T>::value>> WeakPtr(const T* object, EnableWeakPtrThreadingAssertions shouldEnableAssertions = EnableWeakPtrThreadingAssertions::Yes)
        : m_impl(object ? &object->weakImpl() : nullptr)
#if ASSERT_ENABLED
        , m_shouldEnableAssertions(shouldEnableAssertions == EnableWeakPtrThreadingAssertions::Yes)
#endif
    {
        UNUSED_PARAM(shouldEnableAssertions);
        ASSERT(!object || object == m_impl->template get<T>());
    }

    template<typename = std::enable_if_t<!IsSmartPtr<T>::value && !std::is_pointer_v<T>>> WeakPtr(const T& object, EnableWeakPtrThreadingAssertions shouldEnableAssertions = EnableWeakPtrThreadingAssertions::Yes)
        : m_impl(&object.weakImpl())
#if ASSERT_ENABLED
        , m_shouldEnableAssertions(shouldEnableAssertions == EnableWeakPtrThreadingAssertions::Yes)
#endif
    {
        UNUSED_PARAM(shouldEnableAssertions);
        ASSERT(&object == m_impl->template get<T>());
    }

    template<typename = std::enable_if_t<!IsSmartPtr<T>::value>> WeakPtr(const Ref<T>& object, EnableWeakPtrThreadingAssertions shouldEnableAssertions = EnableWeakPtrThreadingAssertions::Yes)
        : WeakPtr(object.get(), shouldEnableAssertions)
    { }

    template<typename = std::enable_if_t<!IsSmartPtr<T>::value>> WeakPtr(const RefPtr<T>& object, EnableWeakPtrThreadingAssertions shouldEnableAssertions = EnableWeakPtrThreadingAssertions::Yes)
        : WeakPtr(object.get(), shouldEnableAssertions)
    { }

    template<typename OtherPtrTraits>
    explicit WeakPtr(RefPtr<WeakPtrImpl, OtherPtrTraits> impl)
        : m_impl(WTFMove(impl))
    {
    }

    RefPtr<WeakPtrImpl, PtrTraits> releaseImpl() { return WTFMove(m_impl); }

    T* get() const
    {
        static_assert(
            HasRefPtrMethods<T>::value || HasCheckedPtrMethods<T>::value || IsDeprecatedWeakRefSmartPointerException<std::remove_cv_t<T>>::value,
            "Classes that offer weak pointers should also offer RefPtr or CheckedPtr. Please do not add new exceptions.");

        ASSERT(canSafelyBeUsed());
        return m_impl ? static_cast<T*>(m_impl->template get<T>()) : nullptr;
    }

    WeakRef<T> releaseNonNull()
    {
        return WeakRef<T> { m_impl.releaseNonNull(), enableWeakPtrThreadingAssertions() };
    }

    bool operator!() const { return !m_impl || !*m_impl; }
    explicit operator bool() const { return m_impl && *m_impl; }

    WeakPtr& operator=(std::nullptr_t) { m_impl = nullptr; return *this; }
    template<typename U> WeakPtr& operator=(const WeakPtr<U, WeakPtrImpl, PtrTraits>&);
    template<typename U> WeakPtr& operator=(WeakPtr<U, WeakPtrImpl, PtrTraits>&&);
    template<typename U> WeakPtr& operator=(const WeakRef<U, WeakPtrImpl>&);
    template<typename U> WeakPtr& operator=(WeakRef<U, WeakPtrImpl>&&);

    T* operator->() const
    {
        static_assert(
            HasRefPtrMethods<T>::value || HasCheckedPtrMethods<T>::value || IsDeprecatedWeakRefSmartPointerException<std::remove_cv_t<T>>::value,
            "Classes that offer weak pointers should also offer RefPtr or CheckedPtr. Please do not add new exceptions.");

        ASSERT(canSafelyBeUsed());
        return get();
    }

    T& operator*() const
    {
        static_assert(
            HasRefPtrMethods<T>::value || HasCheckedPtrMethods<T>::value || IsDeprecatedWeakRefSmartPointerException<std::remove_cv_t<T>>::value,
            "Classes that offer weak pointers should also offer RefPtr or CheckedPtr. Please do not add new exceptions.");

        ASSERT(canSafelyBeUsed());
        return *get();
    }

    void clear() { m_impl = nullptr; }

    EnableWeakPtrThreadingAssertions enableWeakPtrThreadingAssertions() const
    {
#if ASSERT_ENABLED
        return m_shouldEnableAssertions ? EnableWeakPtrThreadingAssertions::Yes : EnableWeakPtrThreadingAssertions::No;
#else
        return EnableWeakPtrThreadingAssertions::No;
#endif
    }

private:
    template<typename, typename, typename> friend class WeakHashMap;
    template<typename, typename, EnableWeakPtrThreadingAssertions> friend class WeakHashSet;
    template<typename, typename, EnableWeakPtrThreadingAssertions> friend class WeakListHashSet;
    template<typename, typename, typename> friend class WeakPtr;
    template<typename, typename> friend class WeakPtrFactory;
    template<typename, typename> friend class WeakPtrFactoryWithBitField;

    explicit WeakPtr(Ref<WeakPtrImpl>&& ref, EnableWeakPtrThreadingAssertions shouldEnableAssertions)
        : m_impl(WTFMove(ref))
#if ASSERT_ENABLED
        , m_shouldEnableAssertions(shouldEnableAssertions == EnableWeakPtrThreadingAssertions::Yes)
#endif
    {
        UNUSED_PARAM(shouldEnableAssertions);
    }

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

    RefPtr<WeakPtrImpl, PtrTraits> m_impl;
#if ASSERT_ENABLED
    bool m_shouldEnableAssertions { true };
#endif
};

template<typename T, typename U, typename WeakPtrImpl> inline WeakPtrImpl* weak_ptr_impl_cast(WeakPtrImpl* impl)
{
    static_assert(std::is_same_v<typename T::WeakValueType, typename U::WeakValueType>, "Invalid weak pointer cast");
    return impl;
}

template<typename T, typename U, typename WeakPtrImpl> inline WeakPtrImpl& weak_ptr_impl_cast(WeakPtrImpl& impl)
{
    static_assert(std::is_same_v<typename T::WeakValueType, typename U::WeakValueType>, "Invalid weak pointer cast");
    return impl;
}

template<typename T, typename WeakPtrImpl, typename PtrTraits> template<typename U> inline WeakPtr<T, WeakPtrImpl, PtrTraits>::WeakPtr(const WeakPtr<U, WeakPtrImpl, PtrTraits>& o)
    : m_impl(weak_ptr_impl_cast<T, U>(o.m_impl.get()))
#if ASSERT_ENABLED
    , m_shouldEnableAssertions(o.m_shouldEnableAssertions)
#endif
{
}

template<typename T, typename WeakPtrImpl, typename PtrTraits> template<typename U> inline WeakPtr<T, WeakPtrImpl, PtrTraits>::WeakPtr(WeakPtr<U, WeakPtrImpl, PtrTraits>&& o)
    : m_impl(adoptRef(weak_ptr_impl_cast<T, U>(o.m_impl.leakRef())))
#if ASSERT_ENABLED
    , m_shouldEnableAssertions(o.m_shouldEnableAssertions)
#endif
{
}

template<typename T, typename WeakPtrImpl, typename PtrTraits> template<typename U> inline WeakPtr<T, WeakPtrImpl, PtrTraits>::WeakPtr(const WeakRef<U, WeakPtrImpl>& o)
    : m_impl(&weak_ptr_impl_cast<T, U>(o.impl()))
#if ASSERT_ENABLED
    , m_shouldEnableAssertions(o.enableWeakPtrThreadingAssertions() == EnableWeakPtrThreadingAssertions::Yes)
#endif
{
}

template<typename T, typename WeakPtrImpl, typename PtrTraits> template<typename U> inline WeakPtr<T, WeakPtrImpl, PtrTraits>::WeakPtr(WeakRef<U, WeakPtrImpl>&& o)
    : m_impl(adoptRef(weak_ptr_impl_cast<T, U>(o.releaseImpl().leakRef())))
#if ASSERT_ENABLED
    , m_shouldEnableAssertions(o.enableWeakPtrThreadingAssertions() == EnableWeakPtrThreadingAssertions::Yes)
#endif
{
}

template<typename T, typename WeakPtrImpl, typename PtrTraits> template<typename U> inline WeakPtr<T, WeakPtrImpl, PtrTraits>& WeakPtr<T, WeakPtrImpl, PtrTraits>::operator=(const WeakPtr<U, WeakPtrImpl, PtrTraits>& o)
{
    m_impl = weak_ptr_impl_cast<T, U>(o.m_impl.get());
#if ASSERT_ENABLED
    m_shouldEnableAssertions = o.m_shouldEnableAssertions;
#endif
    return *this;
}

template<typename T, typename WeakPtrImpl, typename PtrTraits> template<typename U> inline WeakPtr<T, WeakPtrImpl, PtrTraits>& WeakPtr<T, WeakPtrImpl, PtrTraits>::operator=(WeakPtr<U, WeakPtrImpl, PtrTraits>&& o)
{
    m_impl = adoptRef(weak_ptr_impl_cast<T, U>(o.m_impl.leakRef()));
#if ASSERT_ENABLED
    m_shouldEnableAssertions = o.m_shouldEnableAssertions;
#endif
    return *this;
}

template<typename T, typename WeakPtrImpl, typename PtrTraits> template<typename U> inline WeakPtr<T, WeakPtrImpl, PtrTraits>& WeakPtr<T, WeakPtrImpl, PtrTraits>::operator=(const WeakRef<U, WeakPtrImpl>& o)
{
    m_impl = &weak_ptr_impl_cast<T, U>(o.m_impl.get());
#if ASSERT_ENABLED
    m_shouldEnableAssertions = o.enableWeakPtrThreadingAssertions() == EnableWeakPtrThreadingAssertions::Yes;
#endif
    return *this;
}

template<typename T, typename WeakPtrImpl, typename PtrTraits> template<typename U> inline WeakPtr<T, WeakPtrImpl, PtrTraits>& WeakPtr<T, WeakPtrImpl, PtrTraits>::operator=(WeakRef<U, WeakPtrImpl>&& o)
{
    m_impl = adoptRef(weak_ptr_impl_cast<T, U>(o.m_impl.leakRef()));
#if ASSERT_ENABLED
    m_shouldEnableAssertions = o.enableWeakPtrThreadingAssertions() == EnableWeakPtrThreadingAssertions::Yes;
#endif
    return *this;
}

template <typename T, typename WeakPtrImpl, typename PtrTraits>
struct GetPtrHelper<WeakPtr<T, WeakPtrImpl, PtrTraits>> {
    using PtrType = T*;
    using UnderlyingType = T;
    static T* getPtr(const WeakPtr<T, WeakPtrImpl, PtrTraits>& p) { return const_cast<T*>(p.get()); }
};

template <typename T, typename WeakPtrImpl, typename PtrTraits>
struct IsSmartPtr<WeakPtr<T, WeakPtrImpl, PtrTraits>> {
    static constexpr bool value = true;
    static constexpr bool isNullable = true;
};

template<typename ExpectedType, typename ArgType, typename WeakPtrImpl, typename PtrTraits>
inline bool is(WeakPtr<ArgType, WeakPtrImpl, PtrTraits>& source)
{
    return is<ExpectedType>(source.get());
}

template<typename ExpectedType, typename ArgType, typename WeakPtrImpl, typename PtrTraits>
inline bool is(const WeakPtr<ArgType, WeakPtrImpl, PtrTraits>& source)
{
    return is<ExpectedType>(source.get());
}

template<typename Target, typename Source, typename WeakPtrImpl, typename PtrTraits>
inline WeakPtr<match_constness_t<Source, Target>, WeakPtrImpl, PtrTraits> downcast(WeakPtr<Source, WeakPtrImpl, PtrTraits> source)
{
    static_assert(!std::is_same_v<Source, Target>, "Unnecessary cast to same type");
    static_assert(std::is_base_of_v<Source, Target>, "Should be a downcast");
    RELEASE_ASSERT(!source || is<Target>(*source));
    return WeakPtr<match_constness_t<Source, Target>, WeakPtrImpl, PtrTraits> { static_pointer_cast<match_constness_t<Source, Target>>(source.releaseImpl()), source.enableWeakPtrThreadingAssertions() };
}

template<typename Target, typename Source, typename WeakPtrImpl, typename PtrTraits>
inline WeakPtr<match_constness_t<Source, Target>, WeakPtrImpl, PtrTraits> dynamicDowncast(WeakPtr<Source, WeakPtrImpl, PtrTraits> source)
{
    static_assert(!std::is_same_v<Source, Target>, "Unnecessary cast to same type");
    static_assert(std::is_base_of_v<Source, Target>, "Should be a downcast");
    if (!is<Target>(source))
        return nullptr;
    return WeakPtr<match_constness_t<Source, Target>, WeakPtrImpl, PtrTraits> { static_pointer_cast<match_constness_t<Source, Target>, WeakPtrImpl>(source.releaseImpl()), source.enableWeakPtrThreadingAssertions() };
}

template<typename T, typename U, typename WeakPtrImpl, typename PtrTraits> inline bool operator==(const WeakPtr<T, WeakPtrImpl, PtrTraits>& a, const WeakPtr<U, WeakPtrImpl, PtrTraits>& b)
{
    return a.get() == b.get();
}

template<typename T, typename U, typename WeakPtrImpl, typename PtrTraits> inline bool operator==(const WeakPtr<T, WeakPtrImpl, PtrTraits>& a, U* b)
{
    return a.get() == b;
}

template<typename T, typename U, typename WeakPtrImpl, typename PtrTraits> inline bool operator==(T* a, const WeakPtr<U, WeakPtrImpl, PtrTraits>& b)
{
    return a == b.get();
}

template<class T, typename = std::enable_if_t<!IsSmartPtr<T>::value>>
WeakPtr(const T* value, EnableWeakPtrThreadingAssertions = EnableWeakPtrThreadingAssertions::Yes) -> WeakPtr<T, typename T::WeakPtrImplType>;

template<class T, typename = std::enable_if_t<!IsSmartPtr<T>::value && !std::is_pointer_v<T>>>
WeakPtr(const T& value, EnableWeakPtrThreadingAssertions = EnableWeakPtrThreadingAssertions::Yes) -> WeakPtr<T, typename T::WeakPtrImplType>;

template<class T, typename = std::enable_if_t<!IsSmartPtr<T>::value>>
WeakPtr(const Ref<T>& value, EnableWeakPtrThreadingAssertions = EnableWeakPtrThreadingAssertions::Yes) -> WeakPtr<T, typename T::WeakPtrImplType>;

template<class T, typename = std::enable_if_t<!IsSmartPtr<T>::value>>
WeakPtr(const RefPtr<T>& value, EnableWeakPtrThreadingAssertions = EnableWeakPtrThreadingAssertions::Yes) -> WeakPtr<T, typename T::WeakPtrImplType>;

template<typename T, typename PtrTraits = RawPtrTraits<SingleThreadWeakPtrImpl>> using SingleThreadWeakPtr = WeakPtr<T, SingleThreadWeakPtrImpl, PtrTraits>;
template<typename T> using SingleThreadPackedWeakPtr = WeakPtr<T, SingleThreadWeakPtrImpl, PackedPtrTraits<SingleThreadWeakPtrImpl>>;

template<typename T, EnableWeakPtrThreadingAssertions enableWeakPtrThreadingAssertions = EnableWeakPtrThreadingAssertions::Yes>
using SingleThreadWeakHashSet = WeakHashSet<T, SingleThreadWeakPtrImpl, enableWeakPtrThreadingAssertions>;

template<typename KeyType, typename ValueType> using SingleThreadWeakHashMap = WeakHashMap<KeyType, ValueType, SingleThreadWeakPtrImpl>;

template<typename T, EnableWeakPtrThreadingAssertions enableWeakPtrThreadingAssertions = EnableWeakPtrThreadingAssertions::Yes>
using SingleThreadWeakListHashSet = WeakListHashSet<T, SingleThreadWeakPtrImpl, enableWeakPtrThreadingAssertions>;

} // namespace WTF

using WTF::SingleThreadPackedWeakPtr;
using WTF::SingleThreadWeakPtr;
using WTF::SingleThreadWeakHashSet;
using WTF::SingleThreadWeakListHashSet;
using WTF::WeakHashMap;
using WTF::SingleThreadWeakHashMap;
using WTF::WeakHashSet;
using WTF::WeakListHashSet;
using WTF::WeakPtr;
