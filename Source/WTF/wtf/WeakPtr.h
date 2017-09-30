/*
 * Copyright (C) 2013 Google, Inc. All Rights Reserved.
 * Copyright (C) 2015 Apple Inc. All Rights Reserved.
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

#ifndef WTF_WeakPtr_h
#define WTF_WeakPtr_h

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
    WeakPtr() : m_ref(WeakReference<T>::create(nullptr)) { }
    WeakPtr(std::nullptr_t) : m_ref(WeakReference<T>::create(nullptr)) { }
    WeakPtr(const WeakPtr& o) : m_ref(o.m_ref.copyRef()) { }
    WeakPtr(Ref<WeakReference<T>>&& ref) : m_ref(std::forward<Ref<WeakReference<T>>>(ref)) { }

    T* get() const { return m_ref->get(); }
    operator bool() const { return m_ref->get(); }

    WeakPtr& operator=(const WeakPtr& o) { m_ref = o.m_ref.copyRef(); return *this; }
    WeakPtr& operator=(std::nullptr_t) { m_ref = WeakReference<T>::create(nullptr); return *this; }

    T* operator->() const { return get(); }
    T& operator*() const { return *get(); }

    void clear() { m_ref = WeakReference<T>::create(nullptr); }

private:
    Ref<WeakReference<T>> m_ref;
};

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

    template<typename U = T>
    WeakPtr<U> createWeakPtr(T& ptr) const
    {
        if (!m_ref)
            m_ref = WeakReference<T>::create(&ptr);
        ASSERT(&ptr == m_ref->get());
        static_assert(std::is_convertible<U*, T*>::value, "T* must be convertible to U*");
        return WeakPtr<U>(Ref<WeakReference<U>>(reinterpret_cast<WeakReference<U>&>(*m_ref)));
    }

    void revokeAll()
    {
        if (!m_ref)
            return;

        m_ref->clear();
        m_ref = nullptr;
    }

private:
    mutable RefPtr<WeakReference<T>> m_ref;
};

template <typename T> inline WeakPtr<T> makeWeakPtr(T& ref)
{
    return ref.weakPtrFactory().template createWeakPtr<T>(ref);
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

using WTF::WeakPtr;
using WTF::WeakPtrFactory;
using WTF::WeakReference;
using WTF::makeWeakPtr;

#endif
