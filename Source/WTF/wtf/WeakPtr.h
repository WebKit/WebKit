/*
 * Copyright (C) 2013 Google, Inc. All Rights Reserved.
 * Copyright (C) 2015, 2017 Apple Inc. All Rights Reserved.
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

#include <wtf/MainThread.h>
#include <wtf/Noncopyable.h>
#include <wtf/Ref.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Threading.h>

namespace WTF {

// Testing interface for TestWebKitAPI
#ifndef DID_CREATE_WEAK_PTR_IMPL
#define DID_CREATE_WEAK_PTR_IMPL(p)
#endif
#ifndef WILL_DESTROY_WEAK_PTR_IMPL
#define WILL_DESTROY_WEAK_PTR_IMPL(p)
#endif

template<typename> class WeakHashSet;
template<typename> class WeakPtr;
template<typename> class WeakPtrFactory;

class WeakPtrImpl : public ThreadSafeRefCounted<WeakPtrImpl> {
    WTF_MAKE_NONCOPYABLE(WeakPtrImpl);
    WTF_MAKE_FAST_ALLOCATED;
public:
    template<typename T> static Ref<WeakPtrImpl> create(T* ptr)
    {
        return adoptRef(*new WeakPtrImpl(ptr));
    }

    ~WeakPtrImpl()
    {
        WILL_DESTROY_WEAK_PTR_IMPL(m_ptr);
    }

    template<typename T> typename T::WeakValueType* get()
    {
        return static_cast<typename T::WeakValueType*>(m_ptr);
    }

    explicit operator bool() const { return m_ptr; }
    void clear() { m_ptr = nullptr; }

#if !ASSERT_DISABLED
    bool wasConstructedOnMainThread() const { return m_wasConstructedOnMainThread; }
#endif

private:
    template<typename T> explicit WeakPtrImpl(T* ptr)
        : m_ptr(static_cast<typename T::WeakValueType*>(ptr))
#if !ASSERT_DISABLED
        , m_wasConstructedOnMainThread(isMainThread())
#endif
    {
        DID_CREATE_WEAK_PTR_IMPL(ptr);
    }

    void* m_ptr;
#if !ASSERT_DISABLED
    bool m_wasConstructedOnMainThread;
#endif
};

template<typename T>
class WeakPtr {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WeakPtr() { }
    WeakPtr(std::nullptr_t) { }
    template<typename U> WeakPtr(const WeakPtr<U>&);
    template<typename U> WeakPtr(WeakPtr<U>&&);

    T* get() const
    {
        // FIXME: Our GC threads currently need to get opaque pointers from WeakPtrs and have to be special-cased.
        ASSERT(!m_impl || Thread::mayBeGCThread() || m_impl->wasConstructedOnMainThread() == isMainThread());
        return m_impl ? static_cast<T*>(m_impl->get<T>()) : nullptr;
    }

    explicit operator bool() const { return m_impl && *m_impl; }

    WeakPtr& operator=(std::nullptr_t) { m_impl = nullptr; return *this; }
    template<typename U> WeakPtr& operator=(const WeakPtr<U>&);
    template<typename U> WeakPtr& operator=(WeakPtr<U>&&);

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
    explicit WeakPtr(Ref<WeakPtrImpl>&& ref) : m_impl(WTFMove(ref)) { }
    template<typename> friend class WeakHashSet;
    template<typename> friend class WeakPtr;
    template<typename> friend class WeakPtrFactory;
    template<typename U> friend WeakPtr<U> makeWeakPtr(U&);

    RefPtr<WeakPtrImpl> m_impl;
};

// Note: you probably want to inherit from CanMakeWeakPtr rather than use this directly.
template<typename T>
class WeakPtrFactory {
    WTF_MAKE_NONCOPYABLE(WeakPtrFactory<T>);
    WTF_MAKE_FAST_ALLOCATED;
public:
    WeakPtrFactory()
#if !ASSERT_DISABLED
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
        m_impl = WeakPtrImpl::create(const_cast<T*>(&object));
    }

    WeakPtr<T> createWeakPtr(T& object) const
    {
        initializeIfNeeded(object);

        ASSERT(&object == m_impl->get<T>());
        return WeakPtr<T>(makeRef(*m_impl));
    }

    WeakPtr<const T> createWeakPtr(const T& object) const
    {
        initializeIfNeeded(object);

        ASSERT(&object == m_impl->get<T>());
        return WeakPtr<T>(makeRef(*m_impl));
    }

    void revokeAll()
    {
        if (!m_impl)
            return;

        m_impl->clear();
        m_impl = nullptr;
    }

private:
    template<typename> friend class WeakHashSet;

    mutable RefPtr<WeakPtrImpl> m_impl;
#if !ASSERT_DISABLED
    bool m_wasConstructedOnMainThread;
#endif
};

// We use lazy initialization of the WeakPtrFactory by default to avoid unnecessary initialization. Eager
// initialization is however useful if you plan to call makeWeakPtr() from other threads.
enum class WeakPtrFactoryInitialization { Lazy, Eager };

template<typename T, WeakPtrFactoryInitialization initializationMode = WeakPtrFactoryInitialization::Lazy> class CanMakeWeakPtr {
public:
    using WeakValueType = T;

    const WeakPtrFactory<T>& weakPtrFactory() const { return m_weakPtrFactory; }
    WeakPtrFactory<T>& weakPtrFactory() { return m_weakPtrFactory; }

protected:
    CanMakeWeakPtr()
    {
        if (initializationMode == WeakPtrFactoryInitialization::Eager)
            m_weakPtrFactory.initializeIfNeeded(static_cast<T&>(*this));
    }

private:
    WeakPtrFactory<T> m_weakPtrFactory;
};

template<typename T, typename U> inline WeakPtrImpl* weak_ptr_impl_cast(WeakPtrImpl* impl)
{
    static_assert(std::is_same<typename T::WeakValueType, typename U::WeakValueType>::value, "Invalid weak pointer cast");
    return impl;
}

template<typename T> template<typename U> inline WeakPtr<T>::WeakPtr(const WeakPtr<U>& o)
    : m_impl(weak_ptr_impl_cast<T, U>(o.m_impl.get()))
{
}

template<typename T> template<typename U> inline WeakPtr<T>::WeakPtr(WeakPtr<U>&& o)
    : m_impl(adoptRef(weak_ptr_impl_cast<T, U>(o.m_impl.leakRef())))
{
}

template<typename T> template<typename U> inline WeakPtr<T>& WeakPtr<T>::operator=(const WeakPtr<U>& o)
{
    m_impl = weak_ptr_impl_cast<T, U>(o.m_impl.get());
    return *this;
}

template<typename T> template<typename U> inline WeakPtr<T>& WeakPtr<T>::operator=(WeakPtr<U>&& o)
{
    m_impl = adoptRef(weak_ptr_impl_cast<T, U>(o.m_impl.leakRef()));
    return *this;
}

template<typename T> inline WeakPtr<T> makeWeakPtr(T& object)
{
    return { object.weakPtrFactory().createWeakPtr(object) };
}

template<typename T> inline WeakPtr<T> makeWeakPtr(T* ptr)
{
    if (!ptr)
        return { };
    return makeWeakPtr(*ptr);
}

template<typename T, typename U> inline bool operator==(const WeakPtr<T>& a, const WeakPtr<U>& b)
{
    return a.get() == b.get();
}

template<typename T, typename U> inline bool operator==(const WeakPtr<T>& a, U* b)
{
    return a.get() == b;
}

template<typename T, typename U> inline bool operator==(T* a, const WeakPtr<U>& b)
{
    return a == b.get();
}

template<typename T, typename U> inline bool operator!=(const WeakPtr<T>& a, const WeakPtr<U>& b)
{
    return a.get() != b.get();
}

template<typename T, typename U> inline bool operator!=(const WeakPtr<T>& a, U* b)
{
    return a.get() != b;
}

template<typename T, typename U> inline bool operator!=(T* a, const WeakPtr<U>& b)
{
    return a != b.get();
}

} // namespace WTF

using WTF::CanMakeWeakPtr;
using WTF::WeakPtr;
using WTF::WeakPtrFactory;
using WTF::WeakPtrFactoryInitialization;
using WTF::makeWeakPtr;
