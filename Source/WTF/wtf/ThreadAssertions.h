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
#include <utility>
#include <wtf/ThreadSafetyAnalysis.h>

namespace WTF {

class ThreadLikeAssertion;
WTF_EXPORT_PRIVATE bool isMainThread();

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
    static std::atomic<uint32_t> s_uid;
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
private:
    constexpr ThreadLikeAssertion(uint32_t uid);
    bool isCurrent() const;
#if ASSERT_ENABLED
    uint32_t m_uid;
#endif
    friend void assertIsCurrent(const ThreadLikeAssertion&);
    friend class ThreadLike;
};

inline ThreadLikeAssertion::ThreadLikeAssertion(CurrentThreadLike)
{
#if ASSERT_ENABLED
    m_uid = isMainThread() ? mainThreadLike : ThreadLike::currentSequence();
#endif
}

inline ThreadLikeAssertion::ThreadLikeAssertion(ThreadLikeAssertion&& other)
{
    *this = WTFMove(other);
}

inline ThreadLikeAssertion& ThreadLikeAssertion::operator=(ThreadLikeAssertion&& other)
{
#if ASSERT_ENABLED
    m_uid = std::exchange(other.m_uid, anyThreadLike);
#else
    UNUSED_PARAM(other);
#endif
    return *this;
}

inline constexpr ThreadLikeAssertion::ThreadLikeAssertion(uint32_t uid)
#if ASSERT_ENABLED
    : m_uid(uid)
#endif
{
#if !ASSERT_ENABLED
    UNUSED_PARAM(uid);
#endif
}

inline bool ThreadLikeAssertion::isCurrent() const
{
#if ASSERT_ENABLED
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
    ASSERT_UNUSED(threadLikeAssertion, threadLikeAssertion.isCurrent());
}

inline ThreadLikeAssertion ThreadLike::createThreadLikeAssertion(uint32_t uid)
{
    return ThreadLikeAssertion { uid };
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
using WTF::currentThreadLike;
using WTF::CurrentThreadLike;
using WTF::mainThreadLike;
using WTF::MainThreadLike;
using WTF::NamedAssertion;
using WTF::noneThreadLike;
using WTF::NoneThreadLike;
using WTF::ThreadLike;
using WTF::ThreadLikeAssertion;
