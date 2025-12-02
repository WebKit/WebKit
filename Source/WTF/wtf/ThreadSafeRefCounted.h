/*
 * Copyright (C) 2007, 2008, 2010, 2013, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Justin Haygood (jhaygood@reaktix.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <atomic>
#include <wtf/FastMalloc.h>
#include <wtf/MainThread.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefCounted.h>
#include <wtf/SwiftBridging.h>

namespace WTF {

class WTF_EMPTY_BASE_CLASS ThreadSafeRefCountedBase : public RefCountDebugger {
    WTF_MAKE_NONCOPYABLE(ThreadSafeRefCountedBase);
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(ThreadSafeRefCountedBase);
public:
    void ref() const
    {
        willRef(m_refCount, RefCountIsThreadSafe::Yes);
        ++m_refCount;
    }

    bool hasOneRef() const { return m_refCount == 1; }
    unsigned refCount() const { return m_refCount; }

protected:
    ThreadSafeRefCountedBase()
    {
        // FIXME: Lots of subclasses violate our adoption requirements. Migrate
        // this call into only those subclasses that need it.
        relaxAdoptionRequirement();
    }

    ~ThreadSafeRefCountedBase()
    {
        willDestroy(m_refCount);
        // FIXME: Test performance, then change this to RELEASE_ASSERT.
        ASSERT(m_refCount == 1);
    }

    // Returns true if the pointer should be freed.
    bool derefBase() const
    {
        willDeref(m_refCount, RefCountIsThreadSafe::Yes);

        if (!--m_refCount) [[unlikely]] {
            willDelete();

            m_refCount = 1;
            return true;
        }

        return false;
    }

private:
    mutable std::atomic<unsigned> m_refCount { 1 };
};

template<class T, DestructionThread destructionThread = DestructionThread::Any> class ThreadSafeRefCounted : public ThreadSafeRefCountedBase {
public:
    void deref() const
    {
        if (!derefBase())
            return;

        if constexpr (destructionThread == DestructionThread::Any) {
            delete static_cast<const T*>(this);
        } else if constexpr (destructionThread == DestructionThread::Main) {
            ensureOnMainThread([this] {
                delete static_cast<const T*>(this);
            });
        } else if constexpr (destructionThread == DestructionThread::MainRunLoop) {
            ensureOnMainRunLoop([this] {
                delete static_cast<const T*>(this);
            });
        } else
            STATIC_ASSERT_NOT_REACHED_FOR_VALUE(destructionThread, "Unexpected destructionThread enumerator value");
    }

protected:
    ThreadSafeRefCounted() = default;
    ~ThreadSafeRefCounted() = default;
} SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT;

} // namespace WTF

using WTF::ThreadSafeRefCounted;
