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

#include <wtf/CanBorrow.h>
#include <wtf/Compiler.h>
#include <wtf/ForbidHeapAllocation.h>
#include <wtf/Noncopyable.h>

#if OS(DARWIN)
#include <pthread.h>
#endif

namespace WTF {

/**
 * @brief Borrow is a scoped token that ensures a view into an object remains
 * valid even if the object performs internal destruction.
 *
 * Some objects may invalidate internal state as a side effect of mutation. For
 * example, a Vector may reallocate its backing store during append(), which
 * would invalidate any outstanding span(). A Borrow<T> prevents such
 * invalidation for as long as it is alive: if the borrowed object attempts a
 * destructive operation while a Borrow is outstanding, the program will crash
 * (via RELEASE_ASSERT) instead of silently corrupting memory.
 *
 * Borrow is noncopyable and intended for stack use.
 * @code
 * Borrow vector = object->vector();
 * auto span = vector->span(); // span is guaranteed valid for the lifetime of vector
 * use(span);
 * @endcode
 *
 * @code
 * use(borrow(object->vector())->span()); // argument is guaranteed valid for the duration of the call
 * @endcode
 *
 * T must implement the CanBorrow protocol — either by inheriting from
 * CanBorrow, or by providing setIsBorrowed() / crashIfBorrowed() by hand.
 *
 * Borrow is a new concept and we are still working through the static analysis
 * that will enforce it.
 */
template<typename T>
class Borrow {
    WTF_MAKE_NONCOPYABLE(Borrow);
    WTF_FORBID_HEAP_ALLOCATION;
public:
    Borrow(T& ref)
        : m_ref(ref)
        , m_previous(m_ref.setIsBorrowed(true))
    {
        assertIsOnStack();
    }

    ~Borrow()
    {
        m_ref.setIsBorrowed(m_previous);
    }

    operator T&() const LIFETIME_BOUND
    {
        return m_ref;
    }

    T& get() const LIFETIME_BOUND
    {
        return m_ref;
    }

    T* operator->() const LIFETIME_BOUND
    {
        return &m_ref;
    }

private:
    void assertIsOnStack()
    {
#if OS(DARWIN) && ASSERT_ENABLED
        auto self = pthread_self();
        void* origin = pthread_get_stackaddr_np(self);
        rlim_t size = pthread_get_stacksize_np(self);

        ASSERT(origin, this < origin);
        ASSERT(size, reinterpret_cast<uintptr_t>(origin) - reinterpret_cast<uintptr_t>(this) < size);
#endif
    }

    T& m_ref;
    bool m_previous { false };
};

template<typename T>
Borrow(T&) -> Borrow<T>;

// -Wdangling reports false positives on temporaries in range-based for loops.
// Suppress -Wdangling entirely when we're using borrow(x) so that we don't
// have to suppress at every usage site.
// FIXME: Remove this once all supported compilers have this fix.
// Fixed in upstream Clang commit c86c815fc57c (July 2025).
#if defined(__clang__) && defined(__clang_major__) && __clang_major__ < 21
IGNORE_WARNINGS_BEGIN("dangling")
#endif

template<typename T>
inline Borrow<T> borrow(T& ref)
{
    return Borrow<T>(ref);
}

} // namespace WTF

using WTF::Borrow;
using WTF::borrow;
