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

#include <wtf/Borrow.h>
#include <wtf/ForbidHeapAllocation.h>
#include <wtf/Noncopyable.h>
#include <wtf/Ref.h>
#include <wtf/SwiftBridging.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Threading.h>

namespace WTF {

// Lends a C++ object to Swift, for the duration of a call, as a view that no mistake in Swift can use to
// reach the object once the call is over.
//
// The object must implement the CanBorrow protocol. A NonEscapableBorrowLender, which must be a stack
// local of the C++ code that lends the object, holds a Borrow of it for as long as it lends it, so
// CanBorrow catches the object being destroyed while it is lent. The view, a subclass of
// NonEscapableBorrow, reaches the object only through a ref-counted NonEscapableBorrowLease, whose
// pointer to the object the lender sets after its Borrow begins and clears before it ends. Using the
// view, or any copy of it, after that, or on another thread, crashes.
//
// The view is SWIFT_NONESCAPABLE, so Swift rejects most attempts to keep it beyond the call, and the lease
// catches the rest at run time. Swift treats it as safe without SWIFT_SAFE, because all it holds is a Ref.
// It is copyable because Swift 6.3 leaves Swift functions that take a non-copyable C++ type out of the
// generated C++ header.
//
// To give Swift operations on a type, subclass NonEscapableBorrow, implement them with borrowed(), and
// make the constructor private, so that only a lender can make the view. The constructor's parameter
// needs LIFETIME_BOUND, without which some Swift compilers cannot import a non-escapable type:
//
//     class CounterView : public NonEscapableBorrow<Counter> {
//     public:
//         void increment() { borrowed().increment(); }
//
//     private:
//         friend class NonEscapableBorrowLender<CounterView>;
//
//         explicit CounterView(NonEscapableBorrowLease<Counter>& lease LIFETIME_BOUND)
//             : NonEscapableBorrow(lease)
//         {
//         }
//     } SWIFT_NONESCAPABLE;
//
// and lend an object to a Swift function that takes `inout CounterView`:
//
//     NonEscapableBorrowLender<CounterView> lender { counter };
//     swiftObject.useCounter(lender.view());

template<typename> class NonEscapableBorrowLease;

template<typename View>
class NonEscapableBorrowLender {
    WTF_MAKE_NONCOPYABLE(NonEscapableBorrowLender);
    WTF_FORBID_HEAP_ALLOCATION;
public:
    using BorrowedType = typename View::BorrowedType;

    explicit NonEscapableBorrowLender(BorrowedType& object LIFETIME_BOUND)
        : m_borrow(object)
        , m_lease(adoptRef(*new NonEscapableBorrowLease<BorrowedType>(object)))
        , m_view(m_lease.get())
    {
    }

    ~NonEscapableBorrowLender()
    {
        m_lease->end();
    }

    View& view() LIFETIME_BOUND { return m_view; }

private:
    Borrow<BorrowedType> m_borrow;
    const Ref<NonEscapableBorrowLease<BorrowedType>> m_lease;
    View m_view;
};

template<typename T>
class NonEscapableBorrowLease final : public ThreadSafeRefCounted<NonEscapableBorrowLease<T>> {
public:
    T& get() const
    {
        RELEASE_ASSERT(Thread::currentSingleton().uid() == m_threadUID);
        RELEASE_ASSERT(m_object);
        return *m_object;
    }

private:
    template<typename> friend class NonEscapableBorrowLender;

    explicit NonEscapableBorrowLease(T& object)
        : m_object(&object)
        , m_threadUID(Thread::currentSingleton().uid())
    {
    }

    void end() { m_object = nullptr; }

    T* m_object;
    const uint32_t m_threadUID;
};

template<typename T>
class NonEscapableBorrow {
public:
    using BorrowedType = T;

protected:
    explicit NonEscapableBorrow(NonEscapableBorrowLease<T>& lease LIFETIME_BOUND)
        : m_lease(lease)
    {
    }

    T& borrowed() const { return m_lease->get(); }

private:
    const Ref<NonEscapableBorrowLease<T>> m_lease;
} SWIFT_NONESCAPABLE;

} // namespace WTF

using WTF::NonEscapableBorrow;
using WTF::NonEscapableBorrowLease;
using WTF::NonEscapableBorrowLender;
