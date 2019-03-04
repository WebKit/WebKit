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

#include <wtf/Noncopyable.h>
#include <wtf/Ref.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Threading.h>

namespace WTF {

template<typename T> class WeakPtr;
template<typename T> class WeakPtrFactory;

// Note: WeakReference is an implementation detail, and should not be used directly.
template<typename T>
class WeakReference : public ThreadSafeRefCounted<WeakReference<T>> {
    WTF_MAKE_NONCOPYABLE(WeakReference<T>);
    WTF_MAKE_FAST_ALLOCATED;
public:
    ~WeakReference() { } // So that we can use a template specialization for testing purposes to detect leaks.

    T* get() const { return m_ptr; }

    void clear() { m_ptr = nullptr; }

private:
    friend class WeakPtr<T>;
    friend class WeakPtrFactory<T>;

    static Ref<WeakReference<T>> create(T* ptr) { return adoptRef(*new WeakReference(ptr)); }

    explicit WeakReference(T* ptr)
        : m_ptr(ptr)
    {
    }

    T* m_ptr;
};

template<typename T>
class WeakPtr {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WeakPtr() { }
    WeakPtr(std::nullptr_t) { }
    WeakPtr(Ref<WeakReference<T>>&& ref) : m_ref(std::forward<Ref<WeakReference<T>>>(ref)) { }
    template<typename U> WeakPtr(const WeakPtr<U>&);
    template<typename U> WeakPtr(WeakPtr<U>&&);

    T* get() const { return m_ref ? m_ref->get() : nullptr; }
    explicit operator bool() const { return m_ref && m_ref->get(); }

    WeakPtr& operator=(std::nullptr_t) { m_ref = nullptr; return *this; }
    template<typename U> WeakPtr& operator=(const WeakPtr<U>&);
    template<typename U> WeakPtr& operator=(WeakPtr<U>&&);

    T* operator->() const { return m_ref->get(); }
    T& operator*() const { return *m_ref->get(); }

    void clear() { m_ref = nullptr; }

private:
    template<typename U> friend class WeakHashSet;
    template<typename U> friend class WeakPtr;
    template<typename U> friend WeakPtr<U> makeWeakPtr(U&);

    RefPtr<WeakReference<T>> m_ref;
};

// Note: you probably want to inherit from CanMakeWeakPtr rather than use this directly.
template<typename T>
class WeakPtrFactory {
    WTF_MAKE_NONCOPYABLE(WeakPtrFactory<T>);
    WTF_MAKE_FAST_ALLOCATED;
public:
    WeakPtrFactory() = default;
    ~WeakPtrFactory()
    {
        if (!m_ref)
            return;
        m_ref->clear();
    }

    WeakPtr<T> createWeakPtr(T& ptr) const
    {
        if (!m_ref)
            m_ref = WeakReference<T>::create(&ptr);
        return { makeRef(*m_ref) };
    }

    WeakPtr<const T> createWeakPtr(const T& ptr) const
    {
        if (!m_ref)
            m_ref = WeakReference<T>::create(const_cast<T*>(&ptr));
        return { makeRef(reinterpret_cast<WeakReference<const T>&>(*m_ref)) };
    }

    void revokeAll()
    {
        if (!m_ref)
            return;

        m_ref->clear();
        m_ref = nullptr;
    }

private:
    template<typename U> friend class WeakHashSet;

    mutable RefPtr<WeakReference<T>> m_ref;
};

template<typename T> class CanMakeWeakPtr {
public:
    const WeakPtrFactory<T>& weakPtrFactory() const { return m_weakFactory; }
    WeakPtrFactory<T>& weakPtrFactory() { return m_weakFactory; }

private:
    WeakPtrFactory<T> m_weakFactory;
};

template<typename T, typename U> inline WeakReference<T>* weak_reference_upcast(WeakReference<U>* weakReference)
{
    static_assert(std::is_convertible<U*, T*>::value, "U* must be convertible to T*");
    return reinterpret_cast<WeakReference<T>*>(weakReference);
}

template<typename T, typename U> inline WeakReference<T>* weak_reference_downcast(WeakReference<U>* weakReference)
{
    static_assert(std::is_convertible<T*, U*>::value, "T* must be convertible to U*");
    return reinterpret_cast<WeakReference<T>*>(weakReference);
}

template<typename T> template<typename U> inline WeakPtr<T>::WeakPtr(const WeakPtr<U>& o)
    : m_ref(weak_reference_upcast<T>(o.m_ref.get()))
{
}

template<typename T> template<typename U> inline WeakPtr<T>::WeakPtr(WeakPtr<U>&& o)
    : m_ref(adoptRef(weak_reference_upcast<T>(o.m_ref.leakRef())))
{
}

template<typename T> template<typename U> inline WeakPtr<T>& WeakPtr<T>::operator=(const WeakPtr<U>& o)
{
    m_ref = weak_reference_upcast<T>(o.m_ref.get());
    return *this;
}

template<typename T> template<typename U> inline WeakPtr<T>& WeakPtr<T>::operator=(WeakPtr<U>&& o)
{
    m_ref = adoptRef(weak_reference_upcast<T>(o.m_ref.leakRef()));
    return *this;
}

template<typename T> inline WeakPtr<T> makeWeakPtr(T& ref)
{
    return { adoptRef(*weak_reference_downcast<T>(ref.weakPtrFactory().createWeakPtr(ref).m_ref.leakRef())) };
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
using WTF::WeakReference;
using WTF::makeWeakPtr;
