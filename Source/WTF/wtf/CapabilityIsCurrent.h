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

#include <wtf/ThreadSafetyAnalysis.h>

// Functions implementing "is current" capability, declared by WTF_CAPABILITY("is current").

namespace WTF {

class IsCurrentAssertion;

// Class to inherit when creating a new "threadlike sequence" type, a type that can be current.
class ImplementsIsCurrent {
protected:
    static std::atomic<uint32_t> s_sequenceID;
    static IsCurrentAssertion createIsCurrentAssertion(uint32_t sequenceID);
    friend class IsCurrentAssertion;
};

// A type to use for asserting that private member functions or private member variables
// of a class are accessed from correct threads and work queues.
// Supports run-time checking with assertion enabled builds.
// Supports compile-time declaration and checking.
// Example:
// struct MyClass {
//     void doTask() { assertIsCurrent(m_ownerThread); doTaskImpl(); }
//     template<typename> void doTaskCompileFailure() { doTaskImpl(); }
// private:
//     void doTaskImpl() WTF_REQUIRES_CAPABILITY(m_ownerThread);
//     int m_value WTF_GUARDED_BY_CAPABILITY(m_ownerThread) { 0 };
//     NO_UNIQUE_ADDRESS IsCurrentAssertion m_ownerThread;
// };
class WTF_CAPABILITY("is current") IsCurrentAssertion {
public:
    IsCurrentAssertion() = default;
    enum UninitializedTag { Uninitialized };
    constexpr IsCurrentAssertion(UninitializedTag)
        : IsCurrentAssertion { 0 }
    {
    }
    ~IsCurrentAssertion() { assertIsCurrent(*this); }
    void reset() { *this = IsCurrentAssertion { }; }
private:
    explicit constexpr IsCurrentAssertion(uint32_t sequenceID)
#if ASSERT_ENABLED
        : m_uid(sequenceID)
#endif
    {
#if !ASSERT_ENABLED
        UNUSED_PARAM(sequenceID);
#endif
    }
    // Returns unique id for current work queue or thread.
    // Return value is never 0.
    WTF_EXPORT_PRIVATE static uint32_t currentExecutionSequenceID();

#if ASSERT_ENABLED
    uint32_t m_uid { currentExecutionSequenceID() };
#endif

    friend void assertIsCurrent(const IsCurrentAssertion&);
    friend class ImplementsIsCurrent;
};

inline void assertIsCurrent(const IsCurrentAssertion& assertion) WTF_ASSERTS_ACQUIRED_CAPABILITY(assertion)
{
    ASSERT_UNUSED(assertion, IsCurrentAssertion { }.m_uid == assertion.m_uid);
}

inline IsCurrentAssertion ImplementsIsCurrent::createIsCurrentAssertion(uint32_t sequenceID)
{
    return IsCurrentAssertion { sequenceID };
}

}

using WTF::ImplementsIsCurrent;
using WTF::IsCurrentAssertion;
using WTF::assertIsCurrent;
