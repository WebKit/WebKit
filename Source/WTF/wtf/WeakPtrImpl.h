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

#include <wtf/GetPtr.h>
#include <wtf/HashTraits.h>
#include <wtf/SingleThreadIntegralWrapper.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Threading.h>
#include <wtf/TypeCasts.h>

namespace WTF {

DECLARE_COMPACT_ALLOCATOR_WITH_HEAP_IDENTIFIER(WeakPtrImplBase);

template<typename Derived>
class WeakPtrImplBase : public ThreadSafeRefCounted<Derived> {
    WTF_MAKE_NONCOPYABLE(WeakPtrImplBase);
    WTF_MAKE_FAST_COMPACT_ALLOCATED_WITH_HEAP_IDENTIFIER(WeakPtrImplBase);
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
    explicit DefaultWeakPtrImpl(T* ptr)
        : WeakPtrImplBase<DefaultWeakPtrImpl>(ptr)
    {
    }
};

DECLARE_COMPACT_ALLOCATOR_WITH_HEAP_IDENTIFIER(WeakPtrImplBaseSingleThread);

template<typename Derived>
class WeakPtrImplBaseSingleThread {
    WTF_MAKE_NONCOPYABLE(WeakPtrImplBaseSingleThread);
    WTF_MAKE_FAST_COMPACT_ALLOCATED_WITH_HEAP_IDENTIFIER(WeakPtrImplBaseSingleThread);
public:
    ~WeakPtrImplBaseSingleThread() = default;

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
    explicit WeakPtrImplBaseSingleThread(T* ptr)
        : m_ptr(static_cast<typename T::WeakValueType*>(ptr))
#if ASSERT_ENABLED
        , m_wasConstructedOnMainThread(isMainThread())
#endif
    {
    }

    uint32_t refCount() const { return m_refCount; }
    void ref() const { ++m_refCount; }
    void deref() const
    {
        uint32_t tempRefCount = m_refCount - 1;
        if (!tempRefCount) {
            delete const_cast<Derived*>(static_cast<const Derived*>(this));
            return;
        }
        m_refCount = tempRefCount;
    }

private:
    mutable SingleThreadIntegralWrapper<uint32_t> m_refCount { 1 };
    void* m_ptr;
#if ASSERT_ENABLED
    bool m_wasConstructedOnMainThread;
#endif
};

class SingleThreadWeakPtrImpl final : public WeakPtrImplBaseSingleThread<SingleThreadWeakPtrImpl> {
public:
    template<typename T>
    explicit SingleThreadWeakPtrImpl(T* ptr)
        : WeakPtrImplBaseSingleThread<SingleThreadWeakPtrImpl>(ptr)
    {
    }
};

} // namespace WTF

using WTF::SingleThreadWeakPtrImpl;
