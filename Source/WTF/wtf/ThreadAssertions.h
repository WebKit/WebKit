/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include <atomic>
#include <cstdint>
#include <utility>
#include <wtf/Assertions.h>
#include <wtf/CurrentThread.h>
#include <wtf/ExportMacros.h>
#include <wtf/StdLibExtras.h>
#include <wtf/ThreadSafetyAnalysis.h>

namespace WTF {

class ThreadLikeAssertion;
WTF_EXPORT_PRIVATE bool NODELETE isMainThread();

struct MainThreadLike {
    constexpr operator uint32_t() const { return -3; }
};
inline constexpr MainThreadLike mainThreadLike;

struct CurrentThreadLike {
    constexpr operator uint32_t() const { return static_cast<uint32_t>(-2); }
};
inline constexpr CurrentThreadLike currentThreadLike;

struct AnyThreadLike {
    constexpr operator uint32_t() const { return static_cast<uint32_t>(-1); }
};
inline constexpr AnyThreadLike anyThreadLike;

struct NoneThreadLike {
    constexpr operator uint32_t() const { return 0; }
};
inline constexpr NoneThreadLike noneThreadLike;

class ThreadLike {
public:
    // Never returns 0.
    WTF_EXPORT_PRIVATE static uint32_t currentSequence();

protected:
    static constexpr uint32_t mainThreadID { 1 };
    WTF_EXPORT_PRIVATE static std::atomic<uint32_t> s_uid;
    static ThreadLikeAssertion createThreadLikeAssertion(uint32_t);
};

// A type to use for asserting that private member functions or private member variables
// of a class are accessed from correct threads.
// Supports run-time checking with assertion enabled builds.
// Supports compile-time declaration and checking.
// Example:
// struct MyClass {
//     void doTask() { assertIsCurrent(m_ownerThread); doTaskImpl(); }
//     template<typename> void doTaskCompileFailure() { doTaskImpl(); }
// private:
//     void doTaskImpl() WTF_REQUIRES_CAPABILITY(m_ownerThread);
//     int m_value WTF_GUARDED_BY_CAPABILITY(m_ownerThread) { 0 };
//     NO_UNIQUE_ADDRESS ThreadLikeAssertion m_ownerThread;
// };
class WTF_CAPABILITY("is current") ThreadLikeAssertion {
public:
    constexpr ThreadLikeAssertion(NoneThreadLike a)
        : ThreadLikeAssertion(static_cast<uint32_t>(a))
    {
    }
    constexpr ThreadLikeAssertion(AnyThreadLike a)
        : ThreadLikeAssertion(static_cast<uint32_t>(a))
    {
    }
    constexpr ThreadLikeAssertion(MainThreadLike a)
        : ThreadLikeAssertion(static_cast<uint32_t>(a))
    {
    }
    ThreadLikeAssertion(CurrentThreadLike = currentThreadLike);
    ThreadLikeAssertion(ThreadLikeAssertion&&);
    ~ThreadLikeAssertion() { assertIsCurrent(*this); }
    ThreadLikeAssertion(const ThreadLikeAssertion&) = default;
    ThreadLikeAssertion& operator=(const ThreadLikeAssertion&) = default;
    ThreadLikeAssertion& operator=(ThreadLikeAssertion&&);

    void reset() { *this = currentThreadLike; }
    bool isCurrent() const; // Public as used in API tests.
private:
    constexpr ThreadLikeAssertion(uint32_t uid);
#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
    uint32_t m_uid;
#endif
    friend void assertIsCurrent(const ThreadLikeAssertion&);
    friend class ThreadLike;
};

inline ThreadLikeAssertion::ThreadLikeAssertion(CurrentThreadLike)
{
#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
    m_uid = isMainThread() ? mainThreadLike : ThreadLike::currentSequence();
#endif
}

inline ThreadLikeAssertion::ThreadLikeAssertion(ThreadLikeAssertion&& other)
{
    *this = WTF::move(other);
}

inline ThreadLikeAssertion& ThreadLikeAssertion::operator=(ThreadLikeAssertion&& other)
{
#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
    m_uid = std::exchange(other.m_uid, anyThreadLike);
#else
    UNUSED_PARAM(other);
#endif
    return *this;
}

inline constexpr ThreadLikeAssertion::ThreadLikeAssertion(uint32_t uid)
#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
    : m_uid(uid)
#endif
{
    UNUSED_PARAM(uid);
}

inline bool ThreadLikeAssertion::isCurrent() const
{
#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
    if (m_uid == anyThreadLike)
        return true;
    if (m_uid == mainThreadLike)
        return isMainThread();
    return ThreadLike::currentSequence() == m_uid;
#else
    return true;
#endif
}

inline void assertIsCurrent(const ThreadLikeAssertion& threadLikeAssertion) WTF_ASSERTS_ACQUIRED_CAPABILITY(threadLikeAssertion)
{
    UNUSED_PARAM(threadLikeAssertion);
    ASSERT_WITH_SECURITY_IMPLICATION(threadLikeAssertion.isCurrent());
}

inline ThreadLikeAssertion ThreadLike::createThreadLikeAssertion(uint32_t uid)
{
    return ThreadLikeAssertion { uid };
}

// Variant of ThreadLikeAssertion whose mutating check uses RELEASE_ASSERT, so that violations are
// caught in release builds too. Consequently the owner is stored unconditionally, costing four bytes
// even when assertions are disabled, and there is deliberately no destructor check: the garbage
// collector is a legitimate accessor, so destruction from a GC thread must not be fatal.
//
// Only releaseAssertIsCurrentAndLatch(), used on the mutating paths, asserts in release builds. The
// read side goes through assertIsOwnerThread(), which is only a security assertion, because readers
// are typically far hotter than writers and must not pay for the check in shipping builds.
//
// The owner is latched on first use rather than at construction, for state whose owning thread is
// not yet known when the object is created. Like ThreadLikeAssertion, a main thread owner is recorded
// as mainThreadLike and rechecked through isMainThread(), so that the legacy iOS web thread is
// handled without callers needing their own escape hatch.
class WTF_CAPABILITY("is current") ThreadLikeReleaseAssertion {
public:
    ThreadLikeReleaseAssertion() = default;

    bool isUnset() const { return !m_uid; }
    bool isCurrent() const
    {
        if (m_uid == static_cast<uint32_t>(mainThreadLike))
            return isMainThread();
        return m_uid && m_uid == ThreadLike::currentSequence();
    }

private:
    void latchToCurrentThread() { m_uid = isMainThread() ? static_cast<uint32_t>(mainThreadLike) : ThreadLike::currentSequence(); }

    uint32_t m_uid { 0 };

    friend void releaseAssertIsCurrentAndLatch(ThreadLikeReleaseAssertion&);
};

// Latches the owner thread on first call, and thereafter requires every access to come either from
// that thread or from the garbage collector. Call this from the mutating paths.
inline void releaseAssertIsCurrentAndLatch(ThreadLikeReleaseAssertion& ownerThread) WTF_ASSERTS_ACQUIRED_CAPABILITY(ownerThread)
{
    if (ownerThread.isUnset()) {
        ASSERT_WITH_SECURITY_IMPLICATION(!currentThreadMayBeGCThread());
        ownerThread.latchToCurrentThread();
        return;
    }
    if (ownerThread.isCurrent()) [[likely]]
        return;
    RELEASE_ASSERT(currentThreadMayBeGCThread());
}

// Some state is mutated only on one thread, always while holding a lock, yet is read on that same
// thread without taking the lock because doing so on every read would be too expensive. Threads
// other than the owner must hold the lock even to read. Annotate such state with
// WTF_GUARDED_BY_LOCK() as usual, and call assertIsOwnerThread() on the unlocked read path:
//
// struct MyClass {
//     Element* element() const { assertIsOwnerThread(m_lock, mainThreadLike); return m_element.get(); }
//     void setElement(Element* element) { Locker locker { m_lock }; m_element = element; }
//     // Runs on another thread, so it must lock even though it only reads.
//     void visit(Visitor& visitor) const { Locker locker { m_lock }; visitor.append(m_element); }
// private:
//     mutable Lock m_lock;
//     RefPtr<Element> m_element WTF_GUARDED_BY_LOCK(m_lock);
// };
//
// The owner may be given as mainThreadLike, as a ThreadLikeAssertion member for state owned by some
// other thread or work queue, or as a ThreadLikeReleaseAssertion member when the owner should also be
// checked in release builds.
//
// assertIsOwnerThread() grants only shared, read-only access, so thread safety analysis still
// reports any write that does not hold the lock exclusively. That is what makes this preferable to
// leaving the state unannotated: the "every write holds the lock" half of the invariant stays
// machine-checked, reads from any other thread must still lock, and the unlocked reads become both
// self-documenting and checked at run time.
//
// BlockDirectory has long used this idiom by hand, with assertIsMutatorOrMutatorIsStopped() granting
// shared access to m_bitvectorLock; these helpers generalize it.
//
// A function that calls assertIsOwnerThread() and then takes the lock must release the assertion
// first, by calling releaseOwnerThreadAssertion() below. Otherwise the assertion leaves the lock
// marked as held for the remainder of the scope and the subsequent Locker is reported as acquiring
// a lock that is already held.
template<typename LockType>
inline void assertIsOwnerThread(const LockType& lock, const ThreadLikeAssertion& ownerThread) WTF_ASSERTS_ACQUIRED_SHARED_LOCK(lock)
{
    UNUSED_PARAM(lock);
    assertIsCurrent(ownerThread);
}

// Overloads for an owner latched by releaseAssertIsCurrentAndLatch(). Note that the capability is
// granted unconditionally by calling these; the run-time check is only what catches a caller that
// had no business taking the unlocked path. The garbage collector is therefore deliberately not
// tolerated here, unlike on the mutating path: a GC thread reaching an unlocked read is exactly the
// race these annotations exist to catch, and it must take the lock instead. An owner that has not
// been latched yet is tolerated, because no mutation has happened and there is nothing to race with.
//
// This one is only a security assertion, not a release assertion: these reads sit on hot paths such
// as event dispatch, and must not pay for an isMainThread() call in release builds. The release
// assertion on the mutating path is what gives the shipping guarantee. Readers that are not hot
// should prefer releaseAssertIsOwnerThread() below.
template<typename LockType>
inline void assertIsOwnerThread(const LockType& lock, const ThreadLikeReleaseAssertion& ownerThread) WTF_ASSERTS_ACQUIRED_SHARED_LOCK(lock)
{
    UNUSED_PARAM(lock);
    UNUSED_PARAM(ownerThread);
    ASSERT_WITH_SECURITY_IMPLICATION(ownerThread.isUnset() || ownerThread.isCurrent());
}

// As above, but checked in release builds too. Use this on read paths that are not hot enough for
// the isMainThread() call to matter, so that they keep the same shipping guarantee as the mutating
// paths. There is deliberately no equivalent taking a ThreadLikeAssertion: that type does not store
// its owner when assertions are disabled, so it cannot be checked in release builds.
template<typename LockType>
inline void releaseAssertIsOwnerThread(const LockType& lock, const ThreadLikeReleaseAssertion& ownerThread) WTF_ASSERTS_ACQUIRED_SHARED_LOCK(lock)
{
    UNUSED_PARAM(lock);
    RELEASE_ASSERT(ownerThread.isUnset() || ownerThread.isCurrent());
}

// Hands back the shared access granted by assertIsOwnerThread(), so that a function which starts
// with an unlocked owner-thread read can go on to take the lock. Without this the analysis still
// considers the lock held and reports the Locker as acquiring a lock that is already held. This
// generates no code; it only moves the analysis state.
template<typename LockType>
ALWAYS_INLINE void releaseOwnerThreadAssertion(const LockType& lock) WTF_RELEASES_SHARED_LOCK(lock) WTF_IGNORES_THREAD_SAFETY_ANALYSIS
{
    UNUSED_PARAM(lock);
}

// Type for globally named assertions for describing access requirements.
// Can be used, for example, to require that a variable is accessed only in
// a known named thread.
// Example:
// extern NamedAssertion& mainThread;
// inline void assertIsMainThread() WTF_ASSERTS_ACQUIRED_CAPABILITY(mainThread);
// void myTask() WTF_REQUIRES_CAPABILITY(mainThread) { printf("my task is running"); }
// void runner() {
//     assertIsMainThread();
//     myTask();
// }
// template<typename> runnerCompileFailure() {
//     myTask();
// }
class WTF_CAPABILITY("is current") NamedAssertion { };

}

using WTF::anyThreadLike;
using WTF::AnyThreadLike;
using WTF::assertIsCurrent;
using WTF::assertIsOwnerThread;
using WTF::currentThreadLike;
using WTF::CurrentThreadLike;
using WTF::mainThreadLike;
using WTF::MainThreadLike;
using WTF::NamedAssertion;
using WTF::noneThreadLike;
using WTF::NoneThreadLike;
using WTF::releaseAssertIsCurrentAndLatch;
using WTF::releaseAssertIsOwnerThread;
using WTF::releaseOwnerThreadAssertion;
using WTF::ThreadLike;
using WTF::ThreadLikeAssertion;
using WTF::ThreadLikeReleaseAssertion;
