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

#include <wtf/Compiler.h>
#include <wtf/MainThread.h>
#include <wtf/ThreadSafetyAnalysis.h>
#include <wtf/Threading.h>

namespace WTF {

// A type to use for asserting that private member functions or private member variables
// of a class are accessed from correct threads.
// Supports run-time checking with assertion enabled builds.
// Supports compile-time declaration and checking.
// Example:
// struct MyClass {
//     void doTask() { assertIsCurrent(m_ownerThread); doTaskImpl(); }
//     template<typename> void doTaskCompileFailure() { doTaskImpl(); }
// private:
//     void doTaskImpl() WTF_REQUIRES_LOCK(m_ownerThread);
//     int m_value WTF_GUARDED_BY_LOCK(m_ownerThread) { 0 };
//     NO_UNIQUE_ADDRESS ThreadAssertion m_ownerThread;
// };
class WTF_CAPABILITY_LOCK ThreadAssertion {
public:
    ThreadAssertion() = default;
    enum UninitializedTag { Uninitialized };
    constexpr ThreadAssertion(UninitializedTag)
#if ASSERT_ENABLED
        : m_uid(0) // Thread::uid() does not return this.
#endif
    {
    }
    ~ThreadAssertion() { assertIsCurrent(*this); }
    void reset() { *this = ThreadAssertion { }; }
private:
#if ASSERT_ENABLED
    uint32_t m_uid { Thread::current().uid() };
#endif
    friend void assertIsCurrent(const ThreadAssertion&);
};

inline void assertIsCurrent(const ThreadAssertion& threadAssertion) WTF_ASSERTS_ACQUIRED_LOCK(threadAssertion)
{
    ASSERT_UNUSED(threadAssertion, Thread::current().uid() == threadAssertion.m_uid);
}

// Type for globally named assertions for describing access requirements.
// Can be used, for example, to require that a variable is accessed only in
// a known named thread.
// Example:
// extern NamedAssertion& mainThread;
// inline void assertIsMainThread() WTF_ASSERTS_ACQUIRED_LOCK(mainThread);
// void myTask() WTF_REQUIRES_LOCK(mainThread) { printf("my task is running"); }
// void runner() {
//     assertIsMainThread();
//     myTask();
// }
// template<typename> runnerCompileFailure() {
//     myTask();
// }
class WTF_CAPABILITY_LOCK NamedAssertion { };

// To be used with WTF_REQUIRES_LOCK(mainThread). Symbol is undefined.
extern NamedAssertion& mainThread;
inline void assertIsMainThread() WTF_ASSERTS_ACQUIRED_LOCK(mainThread) { ASSERT(isMainThread()); }

// To be used with WTF_REQUIRES_LOCK(mainRunLoop). Symbol is undefined.
extern NamedAssertion& mainRunLoop;
inline void assertIsMainRunLoop() WTF_ASSERTS_ACQUIRED_LOCK(mainRunLoop) { ASSERT(isMainRunLoop()); }

}

using WTF::ThreadAssertion;
using WTF::assertIsCurrent;
using WTF::NamedAssertion;
using WTF::assertIsMainThread;
using WTF::assertIsMainRunLoop;
using WTF::mainThread;
using WTF::mainRunLoop;
