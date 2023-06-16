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
#include <wtf/GetPtr.h>
#include <wtf/HashTraits.h>
#include <wtf/Threading.h>
#include <wtf/TypeCasts.h>

namespace WTF {

enum class EnableWeakPtrThreadingAssertions : bool { No, Yes };
template<typename, typename> class WeakPtrFactory;

template<typename, typename, typename = DefaultWeakPtrImpl> class WeakHashMap;
template<typename, typename = DefaultWeakPtrImpl, EnableWeakPtrThreadingAssertions = EnableWeakPtrThreadingAssertions::Yes> class WeakHashSet;
template <typename, typename = DefaultWeakPtrImpl, EnableWeakPtrThreadingAssertions = EnableWeakPtrThreadingAssertions::Yes> class WeakListHashSet;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(WeakPtrImplBase);

template<typename Derived>
class WeakPtrImplBase : public ThreadSafeRefCounted<Derived> {
    WTF_MAKE_NONCOPYABLE(WeakPtrImplBase);
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(WeakPtrImplBase);
public:
    ~WeakPtrImplBase() = default;

    template<typename T> typename T::WeakValueType* get()
    {
        return static_cast<typename T::WeakValueType*>(m_ptr);
    }

    explicit operator bool() const { return m_ptr; }
    void clear() { m_ptr = nullptr; }

#if ASSERT_ENABLED
    bool wasConstructedOnMainThread() const { return m_wasConstructedOnMainThread; }
#endif

    template<typename T>
    explicit WeakPtrImplBase(T* ptr)
        : m_ptr(static_cast<typename T::WeakValueType*>(ptr))
#if ASSERT_ENABLED
        , m_wasConstructedOnMainThread(isMainThread())
#endif
    {
    }

private:
    void* m_ptr;
#if ASSERT_ENABLED
    bool m_wasConstructedOnMainThread;
#endif
};

class DefaultWeakPtrImpl final : public WeakPtrImplBase<DefaultWeakPtrImpl> {
public:
    template<typename T>
    explicit DefaultWeakPtrImpl(T* ptr) : WeakPtrImplBase<DefaultWeakPtrImpl>(ptr) { }
};

template<typename T, typename WeakPtrImpl> class WeakPtr {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WeakPtr() { }
    WeakPtr(std::nullptr_t) { }
    template<typename U> WeakPtr(const WeakPtr<U, WeakPtrImpl>&);
    template<typename U> WeakPtr(WeakPtr<U, WeakPtrImpl>&&);

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

    T* get() const
    {
        ASSERT(canSafelyBeUsed());
        return m_impl ? static_cast<T*>(m_impl->template get<T>()) : nullptr;
    }

    bool operator!() const { return !m_impl || !*m_impl; }
    explicit operator bool() const { return m_impl && *m_impl; }

    WeakPtr& operator=(std::nullptr_t) { m_impl = nullptr; return *this; }
    template<typename U> WeakPtr& operator=(const WeakPtr<U, WeakPtrImpl>&);
    template<typename U> WeakPtr& operator=(WeakPtr<U, WeakPtrImpl>&&);

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

private:
    template<typename, typename, typename> friend class WeakHashMap;
    template<typename, typename, EnableWeakPtrThreadingAssertions> friend class WeakHashSet;
    template<typename, typename, EnableWeakPtrThreadingAssertions> friend class WeakListHashSet;
    template<typename, typename> friend class WeakPtr;
    template<typename, typename> friend class WeakPtrFactory;

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
        return object.weakPtrFactory().m_impl.pointer();
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

    RefPtr<WeakPtrImpl> m_impl;
#if ASSERT_ENABLED
    bool m_shouldEnableAssertions { true };
#endif
};

// Note: you probably want to inherit from CanMakeWeakPtr rather than use this directly.
template<typename T, typename WeakPtrImpl = DefaultWeakPtrImpl> class WeakPtrFactory {
    WTF_MAKE_NONCOPYABLE(WeakPtrFactory);
    WTF_MAKE_FAST_ALLOCATED;
public:
    using WeakPtrImplType = WeakPtrImpl;

    WeakPtrFactory()
#if ASSERT_ENABLED
        : m_wasConstructedOnMainThread(isMainThread())
#endif
    {
    }

    ~WeakPtrFactory()
    {
        if (auto* pointer = m_impl.pointer())
            pointer->clear();
    }

    WeakPtrImpl* impl()
    {
        return m_impl.pointer();
    }

    const WeakPtrImpl* impl() const
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
    template<typename, typename> friend class WeakPtr;

    mutable CompactRefPtrTuple<WeakPtrImpl, uint16_t> m_impl;
#if ASSERT_ENABLED
    bool m_wasConstructedOnMainThread;
#endif
};

// We use lazy initialization of the WeakPtrFactory by default to avoid unnecessary initialization. Eager
// initialization is however useful if you plan to call construct WeakPtrs from other threads.
enum class WeakPtrFactoryInitialization { Lazy, Eager };

template<typename T, WeakPtrFactoryInitialization initializationMode = WeakPtrFactoryInitialization::Lazy, typename WeakPtrImpl = DefaultWeakPtrImpl> class CanMakeWeakPtr {
public:
    using WeakValueType = T;
    using WeakPtrImplType = WeakPtrImpl;

    const WeakPtrFactory<T, WeakPtrImpl>& weakPtrFactory() const { return m_weakPtrFactory; }
    WeakPtrFactory<T, WeakPtrImpl>& weakPtrFactory() { return m_weakPtrFactory; }

protected:
    CanMakeWeakPtr()
    {
        if (initializationMode == WeakPtrFactoryInitialization::Eager)
            initializeWeakPtrFactory();
    }

    void initializeWeakPtrFactory()
    {
        m_weakPtrFactory.initializeIfNeeded(static_cast<T&>(*this));
    }

private:
    WeakPtrFactory<T, WeakPtrImpl> m_weakPtrFactory;
};

template<typename T, typename U, typename WeakPtrImpl> inline WeakPtrImpl* weak_ptr_impl_cast(WeakPtrImpl* impl)
{
    static_assert(std::is_same_v<typename T::WeakValueType, typename U::WeakValueType>, "Invalid weak pointer cast");
    return impl;
}

template<typename T, typename WeakPtrImpl> template<typename U> inline WeakPtr<T, WeakPtrImpl>::WeakPtr(const WeakPtr<U, WeakPtrImpl>& o)
    : m_impl(weak_ptr_impl_cast<T, U>(o.m_impl.get()))
{
}

template<typename T, typename WeakPtrImpl> template<typename U> inline WeakPtr<T, WeakPtrImpl>::WeakPtr(WeakPtr<U, WeakPtrImpl>&& o)
    : m_impl(adoptRef(weak_ptr_impl_cast<T, U>(o.m_impl.leakRef())))
{
}

template<typename T, typename WeakPtrImpl> template<typename U> inline WeakPtr<T, WeakPtrImpl>& WeakPtr<T, WeakPtrImpl>::operator=(const WeakPtr<U, WeakPtrImpl>& o)
{
    m_impl = weak_ptr_impl_cast<T, U>(o.m_impl.get());
    return *this;
}

template<typename T, typename WeakPtrImpl> template<typename U> inline WeakPtr<T, WeakPtrImpl>& WeakPtr<T, WeakPtrImpl>::operator=(WeakPtr<U, WeakPtrImpl>&& o)
{
    m_impl = adoptRef(weak_ptr_impl_cast<T, U>(o.m_impl.leakRef()));
    return *this;
}

template <typename T, typename WeakPtrImpl>
struct GetPtrHelper<WeakPtr<T, WeakPtrImpl>> {
    using PtrType = T*;
    static T* getPtr(const WeakPtr<T, WeakPtrImpl>& p) { return const_cast<T*>(p.get()); }
};

template <typename T, typename WeakPtrImpl>
struct IsSmartPtr<WeakPtr<T, WeakPtrImpl>> {
    static constexpr bool value = true;
};

template<typename ExpectedType, typename ArgType, typename WeakPtrImpl>
inline bool is(WeakPtr<ArgType, WeakPtrImpl>& source)
{
    return is<ExpectedType>(source.get());
}

template<typename ExpectedType, typename ArgType, typename WeakPtrImpl>
inline bool is(const WeakPtr<ArgType, WeakPtrImpl>& source)
{
    return is<ExpectedType>(source.get());
}

template<typename Target, typename Source, typename WeakPtrImpl>
inline Target* downcast(WeakPtr<Source, WeakPtrImpl>& source)
{
    return downcast<Target>(source.get());
}

template<typename Target, typename Source, typename WeakPtrImpl>
inline Target* downcast(const WeakPtr<Source, WeakPtrImpl>& source)
{
    return downcast<Target>(source.get());
}

template<typename T, typename U, typename WeakPtrImpl> inline bool operator==(const WeakPtr<T, WeakPtrImpl>& a, const WeakPtr<U, WeakPtrImpl>& b)
{
    return a.get() == b.get();
}

template<typename T, typename U, typename WeakPtrImpl> inline bool operator==(const WeakPtr<T, WeakPtrImpl>& a, U* b)
{
    return a.get() == b;
}

template<typename T, typename U, typename WeakPtrImpl> inline bool operator==(T* a, const WeakPtr<U, WeakPtrImpl>& b)
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

} // namespace WTF

using WTF::CanMakeWeakPtr;
using WTF::EnableWeakPtrThreadingAssertions;
using WTF::WeakHashMap;
using WTF::WeakHashSet;
using WTF::WeakListHashSet;
using WTF::WeakPtr;
using WTF::WeakPtrFactory;
using WTF::WeakPtrFactoryInitialization;
