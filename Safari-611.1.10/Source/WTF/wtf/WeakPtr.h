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

#include <wtf/Threading.h>

namespace WTF {

template<typename, typename> class WeakHashSet;
template<typename, typename> class WeakPtrFactory;

struct EmptyCounter {
    static void increment() { }
    static void decrement() { }
};

template<typename Counter = EmptyCounter> class WeakPtrImpl : public ThreadSafeRefCounted<WeakPtrImpl<Counter>> {
    WTF_MAKE_NONCOPYABLE(WeakPtrImpl);
    WTF_MAKE_FAST_ALLOCATED;
public:
    template<typename T> static Ref<WeakPtrImpl> create(T* ptr)
    {
        return adoptRef(*new WeakPtrImpl(ptr));
    }

    ~WeakPtrImpl()
    {
        Counter::decrement();
    }

    template<typename T> typename T::WeakValueType* get()
    {
        return static_cast<typename T::WeakValueType*>(m_ptr);
    }

    explicit operator bool() const { return m_ptr; }
    void clear() { m_ptr = nullptr; }

#if ASSERT_ENABLED
    bool wasConstructedOnMainThread() const { return m_wasConstructedOnMainThread; }
#endif

private:
    template<typename T> explicit WeakPtrImpl(T* ptr)
        : m_ptr(static_cast<typename T::WeakValueType*>(ptr))
#if ASSERT_ENABLED
        , m_wasConstructedOnMainThread(isMainThread())
#endif
    {
        Counter::increment();
    }

    void* m_ptr;
#if ASSERT_ENABLED
    bool m_wasConstructedOnMainThread;
#endif
};

template<typename T, typename Counter> class WeakPtr {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WeakPtr() { }
    WeakPtr(std::nullptr_t) { }
    template<typename U> WeakPtr(const WeakPtr<U, Counter>&);
    template<typename U> WeakPtr(WeakPtr<U, Counter>&&);

    T* get() const
    {
        // FIXME: Our GC threads currently need to get opaque pointers from WeakPtrs and have to be special-cased.
        ASSERT(!m_impl || Thread::mayBeGCThread() || m_impl->wasConstructedOnMainThread() == isMainThread());
        return m_impl ? static_cast<T*>(m_impl->template get<T>()) : nullptr;
    }

    bool operator!() const { return !m_impl || !*m_impl; }
    explicit operator bool() const { return m_impl && *m_impl; }

    WeakPtr& operator=(std::nullptr_t) { m_impl = nullptr; return *this; }
    template<typename U> WeakPtr& operator=(const WeakPtr<U, Counter>&);
    template<typename U> WeakPtr& operator=(WeakPtr<U, Counter>&&);

    T* operator->() const
    {
        ASSERT(!m_impl || m_impl->wasConstructedOnMainThread() == isMainThread());
        return get();
    }

    T& operator*() const
    {
        ASSERT(!m_impl || m_impl->wasConstructedOnMainThread() == isMainThread());
        return *get();
    }

    void clear() { m_impl = nullptr; }

private:
    template<typename, typename> friend class WeakHashSet;
    template<typename, typename> friend class WeakPtr;
    template<typename, typename> friend class WeakPtrFactory;

    explicit WeakPtr(Ref<WeakPtrImpl<Counter>>&& ref)
        : m_impl(WTFMove(ref))
    {
    }

    RefPtr<WeakPtrImpl<Counter>> m_impl;
};

// Note: you probably want to inherit from CanMakeWeakPtr rather than use this directly.
template<typename T, typename Counter = EmptyCounter> class WeakPtrFactory {
    WTF_MAKE_NONCOPYABLE(WeakPtrFactory);
    WTF_MAKE_FAST_ALLOCATED;
public:
    using CounterType = Counter;

    WeakPtrFactory()
#if ASSERT_ENABLED
        : m_wasConstructedOnMainThread(isMainThread())
#endif
    {
    }

    ~WeakPtrFactory()
    {
        if (!m_impl)
            return;
        m_impl->clear();
    }

    void initializeIfNeeded(const T& object) const
    {
        if (m_impl)
            return;

        ASSERT(m_wasConstructedOnMainThread == isMainThread());
        m_impl = WeakPtrImpl<Counter>::create(const_cast<T*>(&object));
    }

    template<typename U> WeakPtr<U, Counter> createWeakPtr(U& object) const
    {
        initializeIfNeeded(object);

        ASSERT(&object == m_impl->template get<T>());
        return WeakPtr<U, Counter>(makeRef(*m_impl));
    }

    void revokeAll()
    {
        if (!m_impl)
            return;

        m_impl->clear();
        m_impl = nullptr;
    }

private:
    template<typename, typename> friend class WeakHashSet;

    mutable RefPtr<WeakPtrImpl<Counter>> m_impl;
#if ASSERT_ENABLED
    bool m_wasConstructedOnMainThread;
#endif
};

// We use lazy initialization of the WeakPtrFactory by default to avoid unnecessary initialization. Eager
// initialization is however useful if you plan to call makeWeakPtr() from other threads.
enum class WeakPtrFactoryInitialization { Lazy, Eager };

template<typename T, WeakPtrFactoryInitialization initializationMode = WeakPtrFactoryInitialization::Lazy, typename Counter = EmptyCounter> class CanMakeWeakPtr {
public:
    using WeakValueType = T;

    const WeakPtrFactory<T, Counter>& weakPtrFactory() const { return m_weakPtrFactory; }
    WeakPtrFactory<T, Counter>& weakPtrFactory() { return m_weakPtrFactory; }

protected:
    CanMakeWeakPtr()
    {
        if (initializationMode == WeakPtrFactoryInitialization::Eager)
            m_weakPtrFactory.initializeIfNeeded(static_cast<T&>(*this));
    }

private:
    WeakPtrFactory<T, Counter> m_weakPtrFactory;
};

template<typename T, typename U, typename Counter> inline WeakPtrImpl<Counter>* weak_ptr_impl_cast(WeakPtrImpl<Counter>* impl)
{
    static_assert(std::is_same_v<typename T::WeakValueType, typename U::WeakValueType>, "Invalid weak pointer cast");
    return impl;
}

template<typename T, typename Counter> template<typename U> inline WeakPtr<T, Counter>::WeakPtr(const WeakPtr<U, Counter>& o)
    : m_impl(weak_ptr_impl_cast<T, U>(o.m_impl.get()))
{
}

template<typename T, typename Counter> template<typename U> inline WeakPtr<T, Counter>::WeakPtr(WeakPtr<U, Counter>&& o)
    : m_impl(adoptRef(weak_ptr_impl_cast<T, U>(o.m_impl.leakRef())))
{
}

template<typename T, typename Counter> template<typename U> inline WeakPtr<T, Counter>& WeakPtr<T, Counter>::operator=(const WeakPtr<U, Counter>& o)
{
    m_impl = weak_ptr_impl_cast<T, U>(o.m_impl.get());
    return *this;
}

template<typename T, typename Counter> template<typename U> inline WeakPtr<T, Counter>& WeakPtr<T, Counter>::operator=(WeakPtr<U, Counter>&& o)
{
    m_impl = adoptRef(weak_ptr_impl_cast<T, U>(o.m_impl.leakRef()));
    return *this;
}

template<typename T> inline auto makeWeakPtr(T& object)
{
    return object.weakPtrFactory().template createWeakPtr<T>(object);
}

template<typename T> inline auto makeWeakPtr(T* ptr) -> decltype(makeWeakPtr(*ptr))
{
    if (!ptr)
        return { };
    return makeWeakPtr(*ptr);
}

template<typename T, typename U, typename Counter> inline bool operator==(const WeakPtr<T, Counter>& a, const WeakPtr<U, Counter>& b)
{
    return a.get() == b.get();
}

template<typename T, typename U, typename Counter> inline bool operator==(const WeakPtr<T, Counter>& a, U* b)
{
    return a.get() == b;
}

template<typename T, typename U, typename Counter> inline bool operator==(T* a, const WeakPtr<U, Counter>& b)
{
    return a == b.get();
}

template<typename T, typename U, typename Counter> inline bool operator!=(const WeakPtr<T, Counter>& a, const WeakPtr<U, Counter>& b)
{
    return a.get() != b.get();
}

template<typename T, typename U, typename Counter> inline bool operator!=(const WeakPtr<T, Counter>& a, U* b)
{
    return a.get() != b;
}

template<typename T, typename U, typename Counter> inline bool operator!=(T* a, const WeakPtr<U, Counter>& b)
{
    return a != b.get();
}

} // namespace WTF

using WTF::CanMakeWeakPtr;
using WTF::WeakPtr;
using WTF::WeakPtrFactory;
using WTF::WeakPtrFactoryInitialization;
using WTF::makeWeakPtr;
