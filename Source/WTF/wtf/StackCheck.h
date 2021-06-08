/*
 * Copyright (C) 2019-2020 Apple Inc. All rights reserved.
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

#include <wtf/StackBounds.h>
#include <wtf/Threading.h>

namespace WTF {

// We only enable the reserved zone size check by default on ASSERT_ENABLED
// builds (which usually mean Debug builds). However, it is more valuable to
// run this test on Release builds. That said, we don't want to do pay this
// cost always because we may need to do stack checks on hot paths too.
// Note also that we're deliberately using RELEASE_ASSERTs if the verification
// is turned on. This makes it easier for us to turn this on for Release builds
// for a reserved zone size calibration test runs.

#define VERIFY_STACK_CHECK_RESERVED_ZONE_SIZE ASSERT_ENABLED

class StackCheck {
    WTF_MAKE_FAST_ALLOCATED;
public:
    class Scope {
    public:
        Scope(StackCheck& checker)
            : m_checker(checker)
        {
            m_checker.isSafeToRecurse();
#if VERIFY_STACK_CHECK_RESERVED_ZONE_SIZE
            RELEASE_ASSERT(checker.m_ownerThread == &Thread::current());

            // Make sure that the stack interval between checks never exceed the
            // reservedZone size. If the interval exceeds the reservedZone size,
            // then we either need to:
            // 1. break the interval into smaller pieces (i.e. insert more checks), or
            // 2. use a larger reservedZone size.
            uint8_t* currentStackCheckpoint = static_cast<uint8_t*>(currentStackPointer());
            uint8_t* previousStackCheckpoint = m_checker.m_lastStackCheckpoint;
            RELEASE_ASSERT(previousStackCheckpoint - currentStackCheckpoint > 0);
            RELEASE_ASSERT(previousStackCheckpoint - currentStackCheckpoint < static_cast<ptrdiff_t>(m_checker.m_reservedZone));

            m_savedLastStackCheckpoint = m_checker.m_lastStackCheckpoint;
            m_checker.m_lastStackCheckpoint = currentStackCheckpoint;
#endif
        }

#if VERIFY_STACK_CHECK_RESERVED_ZONE_SIZE
        ~Scope()
        {
            m_checker.m_lastStackCheckpoint = m_savedLastStackCheckpoint;
        }
#endif

        bool isSafeToRecurse() { return m_checker.isSafeToRecurse(); }

    private:
        StackCheck& m_checker;
#if VERIFY_STACK_CHECK_RESERVED_ZONE_SIZE
        uint8_t* m_savedLastStackCheckpoint;
#endif
    };

    StackCheck(const StackBounds& bounds = Thread::current().stack(), size_t minReservedZone = defaultReservedZoneSize)
        : m_stackLimit(bounds.recursionLimit(minReservedZone))
#if VERIFY_STACK_CHECK_RESERVED_ZONE_SIZE
        , m_ownerThread(&Thread::current())
        , m_lastStackCheckpoint(static_cast<uint8_t*>(currentStackPointer()))
        , m_reservedZone(minReservedZone)
#endif
    { }

    inline bool isSafeToRecurse() { return currentStackPointer() >= m_stackLimit; }

private:
#if ASAN_ENABLED
    static constexpr size_t defaultReservedZoneSize = StackBounds::DefaultReservedZone * 2;
#else
    static constexpr size_t defaultReservedZoneSize = StackBounds::DefaultReservedZone;
#endif

    void* m_stackLimit;
#if VERIFY_STACK_CHECK_RESERVED_ZONE_SIZE
    Thread* m_ownerThread;
    uint8_t* m_lastStackCheckpoint;
    size_t m_reservedZone;
#endif

    friend class Scope;
};

} // namespace WTF

using WTF::StackCheck;
