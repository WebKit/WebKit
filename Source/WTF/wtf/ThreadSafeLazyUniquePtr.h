/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <memory>
#include <wtf/Atomics.h>
#include <wtf/Noncopyable.h>
#include <wtf/Nonmovable.h>
#include <wtf/UniqueRef.h>

namespace WTF {

// ThreadSafeLazyUniquePtr<T> is a unique-ownership pointer that is lazily installed
// once, atomically, via a compare-and-swap — no lock required. If multiple
// threads race to install, only the first install wins and the losers delete
// their candidate and observe the winner.
template<typename T>
class ThreadSafeLazyUniquePtr final {
    WTF_MAKE_NONCOPYABLE(ThreadSafeLazyUniquePtr);
    WTF_MAKE_NONMOVABLE(ThreadSafeLazyUniquePtr);
public:
    ThreadSafeLazyUniquePtr() = default;

    ~ThreadSafeLazyUniquePtr()
    {
        delete m_pointer.load(std::memory_order_relaxed);
    }

    // In general there is no need to fence in func the compareExchangeStrong will ensure any stores
    // to initialize the value will happen before the new pointer becomes visible.
    template<typename Func>
    T& ensure(NOESCAPE const Func& func) const
    {
        T* oldValue = m_pointer.load(std::memory_order_relaxed);
        if (oldValue) [[likely]] {
            // On all sensible CPUs, we get an implicit dependency-based load-load barrier when
            // loading this.
            return *oldValue;
        }

        T* newValue = toRawOwned(func());
        if (T* actual = m_pointer.compareExchangeStrong(nullptr, newValue, std::memory_order_release, std::memory_order_relaxed)) {
            delete newValue;
            return *actual;
        }
        return *newValue;
    }

    T* get() const { return m_pointer.load(std::memory_order_relaxed); }
    explicit operator bool() const { return !!get(); }

private:
    static T* toRawOwned(std::unique_ptr<T>&& ptr) { return ptr.release(); }
    static T* toRawOwned(UniqueRef<T>&& ref) { return ref.moveToUniquePtr().release(); }

    mutable Atomic<T*> m_pointer { nullptr };
};

static_assert(sizeof(ThreadSafeLazyUniquePtr<int>) == sizeof(int*));

} // namespace WTF

using WTF::ThreadSafeLazyUniquePtr;
