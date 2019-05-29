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

// Testing interface for TestWebKitAPI
#ifndef DID_CREATE_WEAK_REFERENCE
#define DID_CREATE_WEAK_REFERENCE(p)
#endif
#ifndef WILL_DESTROY_WEAK_REFERENCE
#define WILL_DESTROY_WEAK_REFERENCE(p)
#endif

template<typename> class WeakHashSet;
template<typename> class WeakPtr;
template<typename> class WeakPtrFactory;

// Note: WeakReference is an implementation detail, and should not be used directly.
class WeakReference : public ThreadSafeRefCounted<WeakReference> {
    WTF_MAKE_NONCOPYABLE(WeakReference);
    WTF_MAKE_FAST_ALLOCATED;
public:
    template<typename T> static Ref<WeakReference> create(T* ptr) { return adoptRef(*new WeakReference(ptr)); }

    ~WeakReference()
    {
        WILL_DESTROY_WEAK_REFERENCE(m_ptr);
    }

    template<typename T, typename WeakValueType> T* get() const { return static_cast<T*>(static_cast<WeakValueType*>(m_ptr)); }
    explicit operator bool() const { return m_ptr; }

    void clear() { m_ptr = nullptr; }

private:
    template<typename T> explicit WeakReference(T* ptr)
        : m_ptr(ptr)
    {
        DID_CREATE_WEAK_REFERENCE(ptr);
    }

    void* m_ptr;
};

template<typename T>
class WeakPtr {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WeakPtr() { }
    WeakPtr(std::nullptr_t) { }
    template<typename U> WeakPtr(const WeakPtr<U>&);
    template<typename U> WeakPtr(WeakPtr<U>&&);

    T* get() const { return m_ref ? m_ref->template get<T, typename T::WeakValueType>() : nullptr; }
    explicit operator bool() const { return m_ref && *m_ref; }

    WeakPtr& operator=(std::nullptr_t) { m_ref = nullptr; return *this; }
    template<typename U> WeakPtr& operator=(const WeakPtr<U>&);
    template<typename U> WeakPtr& operator=(WeakPtr<U>&&);

    T* operator->() const { return get(); }
    T& operator*() const { return *get(); }

    void clear() { m_ref = nullptr; }

private:
    explicit WeakPtr(Ref<WeakReference>&& ref) : m_ref(std::move(ref)) { }
    template<typename> friend class WeakHashSet;
    template<typename> friend class WeakPtr;
    template<typename> friend class WeakPtrFactory;
    template<typename U> friend WeakPtr<U> makeWeakPtr(U&);

    RefPtr<WeakReference> m_ref;
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
            m_ref = WeakReference::create(&ptr);
        return WeakPtr<T>(makeRef(*m_ref));
    }

    WeakPtr<const T> createWeakPtr(const T& ptr) const
    {
        if (!m_ref)
            m_ref = WeakReference::create(const_cast<T*>(&ptr));
        return WeakPtr<T>(makeRef(*m_ref));
    }

    void revokeAll()
    {
        if (!m_ref)
            return;

        m_ref->clear();
        m_ref = nullptr;
    }

private:
    template<typename> friend class WeakHashSet;

    mutable RefPtr<WeakReference> m_ref;
};

template<typename T> class CanMakeWeakPtr {
public:
    typedef T WeakValueType;

    const WeakPtrFactory<T>& weakPtrFactory() const { return m_weakFactory; }
    WeakPtrFactory<T>& weakPtrFactory() { return m_weakFactory; }

private:
    WeakPtrFactory<T> m_weakFactory;
};

template<typename T, typename U> inline WeakReference* weak_reference_cast(WeakReference* weakReference)
{
    UNUSED_VARIABLE(static_cast<T*>(static_cast<typename U::WeakValueType*>(nullptr))); // Verify that casting is valid.
    return weakReference;
}

template<typename T> template<typename U> inline WeakPtr<T>::WeakPtr(const WeakPtr<U>& o)
    : m_ref(weak_reference_cast<T, U>(o.m_ref.get()))
{
}

template<typename T> template<typename U> inline WeakPtr<T>::WeakPtr(WeakPtr<U>&& o)
    : m_ref(adoptRef(weak_reference_cast<T, U>(o.m_ref.leakRef())))
{
}

template<typename T> template<typename U> inline WeakPtr<T>& WeakPtr<T>::operator=(const WeakPtr<U>& o)
{
    m_ref = weak_reference_cast<T, U>(o.m_ref.get());
    return *this;
}

template<typename T> template<typename U> inline WeakPtr<T>& WeakPtr<T>::operator=(WeakPtr<U>&& o)
{
    m_ref = adoptRef(weak_reference_cast<T, U>(o.m_ref.leakRef()));
    return *this;
}

template<typename T> inline WeakPtr<T> makeWeakPtr(T& ref)
{
    return { ref.weakPtrFactory().createWeakPtr(ref) };
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
