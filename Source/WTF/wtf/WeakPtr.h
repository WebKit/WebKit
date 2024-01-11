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

#include <wtf/CompactRefPtrTuple.h>
#include <wtf/Packed.h>
#include <wtf/WeakRef.h>

namespace WTF {

template<typename, typename> class WeakPtrFactory;

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
        : m_impl(object ? implForObject(*object) : nullptr)
#if ASSERT_ENABLED
        , m_shouldEnableAssertions(shouldEnableAssertions == EnableWeakPtrThreadingAssertions::Yes)
#endif
    {
        UNUSED_PARAM(shouldEnableAssertions);
        ASSERT(!object || object == m_impl->template get<T>());
    }

    template<typename = std::enable_if_t<!IsSmartPtr<T>::value && !std::is_pointer_v<T>>> WeakPtr(const T& object, EnableWeakPtrThreadingAssertions shouldEnableAssertions = EnableWeakPtrThreadingAssertions::Yes)
        : m_impl(implForObject(object))
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
        ASSERT(canSafelyBeUsed());
        return get();
    }

    T& operator*() const
    {
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

    template<typename U> static WeakPtrImpl* implForObject(const U& object)
    {
        object.weakPtrFactory().initializeIfNeeded(object);
        return object.weakPtrFactory().impl();
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

// Note: you probably want to inherit from CanMakeWeakPtr rather than use this directly.
template<typename T, typename WeakPtrImpl = DefaultWeakPtrImpl>
class WeakPtrFactory {
    WTF_MAKE_NONCOPYABLE(WeakPtrFactory);
    WTF_MAKE_FAST_ALLOCATED;
public:
    using ObjectType = T;
    using WeakPtrImplType = WeakPtrImpl;

    WeakPtrFactory()
#if ASSERT_ENABLED
        : m_wasConstructedOnMainThread(isMainThread())
#endif
    {
    }

    ~WeakPtrFactory()
    {
        if (m_impl)
            m_impl->clear();
    }

    WeakPtrImpl* impl() const
    {
        return m_impl.get();
    }

    void initializeIfNeeded(const T& object) const
    {
        if (m_impl)
            return;

        ASSERT(m_wasConstructedOnMainThread == isMainThread());

        static_assert(std::is_final_v<WeakPtrImpl>);
        m_impl = adoptRef(*new WeakPtrImpl(const_cast<T*>(&object)));
    }

    template<typename U> WeakPtr<U, WeakPtrImpl> createWeakPtr(U& object, EnableWeakPtrThreadingAssertions enableWeakPtrThreadingAssertions = EnableWeakPtrThreadingAssertions::Yes) const
    {
        initializeIfNeeded(object);

        ASSERT(&object == m_impl->template get<T>());
        return WeakPtr<U, WeakPtrImpl>(*m_impl, enableWeakPtrThreadingAssertions);
    }

    void revokeAll()
    {
        if (RefPtr impl = std::exchange(m_impl, nullptr))
            impl->clear();
    }

    unsigned weakPtrCount() const
    {
        return m_impl ? m_impl->refCount() - 1 : 0u;
    }

#if ASSERT_ENABLED
    bool isInitialized() const { return m_impl; }
#endif

private:
    template<typename, typename, EnableWeakPtrThreadingAssertions> friend class WeakHashSet;
    template<typename, typename, EnableWeakPtrThreadingAssertions> friend class WeakListHashSet;
    template<typename, typename, typename> friend class WeakHashMap;
    template<typename, typename, typename> friend class WeakPtr;
    template<typename, typename> friend class WeakRef;

    mutable RefPtr<WeakPtrImpl> m_impl;
#if ASSERT_ENABLED
    bool m_wasConstructedOnMainThread;
#endif
};

// Note: you probably want to inherit from CanMakeWeakPtrWithBitField rather than use this directly.
template<typename T, typename WeakPtrImpl = DefaultWeakPtrImpl>
class WeakPtrFactoryWithBitField {
    WTF_MAKE_NONCOPYABLE(WeakPtrFactoryWithBitField);
    WTF_MAKE_FAST_ALLOCATED;
public:
    using ObjectType = T;
    using WeakPtrImplType = WeakPtrImpl;

    WeakPtrFactoryWithBitField()
#if ASSERT_ENABLED
        : m_wasConstructedOnMainThread(isMainThread())
#endif
    {
    }

    ~WeakPtrFactoryWithBitField()
    {
        if (auto* pointer = m_impl.pointer())
            pointer->clear();
    }

    WeakPtrImpl* impl() const
    {
        return m_impl.pointer();
    }

    void initializeIfNeeded(const T& object) const
    {
        if (m_impl.pointer())
            return;

        ASSERT(m_wasConstructedOnMainThread == isMainThread());

        static_assert(std::is_final_v<WeakPtrImpl>);
        m_impl.setPointer(adoptRef(*new WeakPtrImpl(const_cast<T*>(&object))));
    }

    template<typename U> WeakPtr<U, WeakPtrImpl> createWeakPtr(U& object, EnableWeakPtrThreadingAssertions enableWeakPtrThreadingAssertions = EnableWeakPtrThreadingAssertions::Yes) const
    {
        initializeIfNeeded(object);

        ASSERT(&object == m_impl.pointer()->template get<T>());
        return WeakPtr<U, WeakPtrImpl>(*m_impl.pointer(), enableWeakPtrThreadingAssertions);
    }

    void revokeAll()
    {
        if (auto* pointer = m_impl.pointer()) {
            pointer->clear();
            m_impl.setPointer(nullptr);
        }
    }

    unsigned weakPtrCount() const
    {
        if (auto* pointer = m_impl.pointer())
            return pointer->refCount() - 1;
        return 0;
    }

#if ASSERT_ENABLED
    bool isInitialized() const { return m_impl.pointer(); }
#endif

    uint16_t bitfield() const { return m_impl.type(); }
    void setBitfield(uint16_t value) const { return m_impl.setType(value); }

private:
    template<typename, typename, EnableWeakPtrThreadingAssertions> friend class WeakHashSet;
    template<typename, typename, EnableWeakPtrThreadingAssertions> friend class WeakListHashSet;
    template<typename, typename, typename> friend class WeakHashMap;
    template<typename, typename, typename> friend class WeakPtr;
    template<typename, typename> friend class WeakRef;

    mutable CompactRefPtrTuple<WeakPtrImpl, uint16_t> m_impl;
#if ASSERT_ENABLED
    bool m_wasConstructedOnMainThread;
#endif
};

// We use lazy initialization of the WeakPtrFactory by default to avoid unnecessary initialization. Eager
// initialization is however useful if you plan to call construct WeakPtrs from other threads.
enum class WeakPtrFactoryInitialization { Lazy, Eager };

template<typename WeakPtrFactoryType, WeakPtrFactoryInitialization initializationMode = WeakPtrFactoryInitialization::Lazy>
class CanMakeWeakPtrBase {
public:
    using WeakValueType = typename WeakPtrFactoryType::ObjectType;
    using WeakPtrImplType = typename WeakPtrFactoryType::WeakPtrImplType;

    const WeakPtrFactoryType& weakPtrFactory() const { return m_weakPtrFactory; }
    WeakPtrFactoryType& weakPtrFactory() { return m_weakPtrFactory; }

protected:
    CanMakeWeakPtrBase()
    {
        if (initializationMode == WeakPtrFactoryInitialization::Eager)
            initializeWeakPtrFactory();
    }

    CanMakeWeakPtrBase(const CanMakeWeakPtrBase&)
    {
        if (initializationMode == WeakPtrFactoryInitialization::Eager)
            initializeWeakPtrFactory();
    }

    CanMakeWeakPtrBase& operator=(const CanMakeWeakPtrBase&) { return *this; }

    void initializeWeakPtrFactory()
    {
        m_weakPtrFactory.initializeIfNeeded(static_cast<WeakValueType&>(*this));
    }

private:
    WeakPtrFactoryType m_weakPtrFactory;
};

template<typename T, WeakPtrFactoryInitialization initializationMode = WeakPtrFactoryInitialization::Lazy, typename WeakPtrImpl = DefaultWeakPtrImpl>
using CanMakeWeakPtr = CanMakeWeakPtrBase<WeakPtrFactory<T, WeakPtrImpl>, initializationMode>;

template<typename T, WeakPtrFactoryInitialization initializationMode = WeakPtrFactoryInitialization::Lazy, typename WeakPtrImpl = DefaultWeakPtrImpl>
using CanMakeWeakPtrWithBitField = CanMakeWeakPtrBase<WeakPtrFactoryWithBitField<T, WeakPtrImpl>, initializationMode>;

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
    // FIXME: This is too expensive to enable on x86 for now but we should try and
    // enable the RELEASE_ASSERT() on all architectures.
#if CPU(ARM64)
    RELEASE_ASSERT(!source || is<Target>(*source));
#else
    ASSERT_WITH_SECURITY_IMPLICATION(!source || is<Target>(*source));
#endif
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

template<typename T, WeakPtrFactoryInitialization initializationMode = WeakPtrFactoryInitialization::Lazy>
using CanMakeSingleThreadWeakPtr = CanMakeWeakPtr<T, initializationMode, SingleThreadWeakPtrImpl>;

template<typename T, EnableWeakPtrThreadingAssertions enableWeakPtrThreadingAssertions = EnableWeakPtrThreadingAssertions::Yes>
using SingleThreadWeakHashSet = WeakHashSet<T, SingleThreadWeakPtrImpl, enableWeakPtrThreadingAssertions>;

template<typename KeyType, typename ValueType> using SingleThreadWeakHashMap = WeakHashMap<KeyType, ValueType, SingleThreadWeakPtrImpl>;

template<typename T, EnableWeakPtrThreadingAssertions enableWeakPtrThreadingAssertions = EnableWeakPtrThreadingAssertions::Yes>
using SingleThreadWeakListHashSet = WeakListHashSet<T, SingleThreadWeakPtrImpl, enableWeakPtrThreadingAssertions>;

} // namespace WTF

using WTF::CanMakeWeakPtr;
using WTF::CanMakeWeakPtrWithBitField;
using WTF::CanMakeSingleThreadWeakPtr;
using WTF::SingleThreadPackedWeakPtr;
using WTF::SingleThreadWeakPtr;
using WTF::SingleThreadWeakHashSet;
using WTF::SingleThreadWeakListHashSet;
using WTF::WeakHashMap;
using WTF::SingleThreadWeakHashMap;
using WTF::WeakHashSet;
using WTF::WeakListHashSet;
using WTF::WeakPtr;
using WTF::WeakPtrFactory;
using WTF::WeakPtrFactoryInitialization;
