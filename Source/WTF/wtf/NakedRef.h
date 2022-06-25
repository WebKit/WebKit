/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include <wtf/FastMalloc.h>
#include <wtf/RawPtrTraits.h>

namespace WTF {

// The purpose of this class is to ensure that the wrapped pointer will never be
// used uninitialized.

template <typename T> class NakedRef {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ALWAYS_INLINE NakedRef(T& ref) : m_ptr(&ref) { }
    ALWAYS_INLINE NakedRef(const NakedRef&) = delete;
    template<typename U> NakedRef(const NakedRef<U>&) = delete;

    ALWAYS_INLINE NakedRef(NakedRef&& other)
        : m_ptr(&other.leakRef())
    {
        ASSERT(m_ptr);
    }

    template<typename U>
    NakedRef(NakedRef<U>&& other)
        : m_ptr(&other.leakRef())
    {
        ASSERT(m_ptr);
    }

    ALWAYS_INLINE T* operator->() const { ASSERT(m_ptr); return m_ptr; }
    T* ptr() const RETURNS_NONNULL { ASSERT(m_ptr); return m_ptr; }
    T& get() const { return *m_ptr; }
    operator T&() const { ASSERT(m_ptr); return *m_ptr; }
    bool operator!() const { ASSERT(m_ptr); return !*m_ptr; }

    NakedRef copyRef() && = delete;
    NakedRef copyRef() const & WARN_UNUSED_RETURN { return NakedRef(*m_ptr); }

    NakedRef& operator=(T&);
    NakedRef& operator=(NakedRef&&);
    template<typename U> NakedRef& operator=(NakedRef<U>&&);

    // Use copyRef() and the move assignment operators instead.
    NakedRef& operator=(const NakedRef&) = delete;
    template<typename X> NakedRef& operator=(const NakedRef<X>&) = delete;

    template<typename U> void swap(NakedRef<U>&);

    T& leakRef() WARN_UNUSED_RETURN
    {
        ASSERT(m_ptr);
        T& result = *RawPtrTraits<T>::exchange(m_ptr, nullptr);
        return result;
    }

private:
    T* m_ptr;
};

template<typename T> inline NakedRef<T>& NakedRef<T>::operator=(NakedRef&& reference)
{
    NakedRef movedReference = WTFMove(reference);
    swap(movedReference);
    return *this;
}

template<typename T> inline NakedRef<T>& NakedRef<T>::operator=(T& ref)
{
    NakedRef copiedReference = ref;
    swap(copiedReference);
    return *this;
}

template<typename T> template<typename U> inline NakedRef<T>& NakedRef<T>::operator=(NakedRef<U>&& other)
{
    NakedRef ref = WTFMove(other);
    swap(ref);
    return *this;
}

template<class T>
template<class U>
inline void NakedRef<T>::swap(NakedRef<U>& other)
{
    std::swap(m_ptr, other.m_ptr);
}

template<class T, class U> inline void swap(NakedRef<T>& a, NakedRef<U>& b)
{
    a.swap(b);
}

} // namespace WTF

using WTF::NakedRef;
