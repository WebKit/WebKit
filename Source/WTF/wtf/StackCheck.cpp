/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "config.h"
#include <wtf/StackCheck.h>

#if VERIFY_STACK_CHECK_RESERVED_ZONE_SIZE
#include <wtf/DataLog.h>
#endif

namespace WTF {

#if VERIFY_STACK_CHECK_RESERVED_ZONE_SIZE

NO_RETURN_DUE_TO_CRASH void StackCheck::Scope::reportVerificationFailureAndCrash()
{
    uint8_t* currentStackCheckpoint = m_checker.m_lastStackCheckpoint;
    uint8_t* previousStackCheckpoint = m_savedLastStackCheckpoint;
    ptrdiff_t stackBetweenCheckpoints = previousStackCheckpoint - currentStackCheckpoint;

    dataLogLn("Stack check failure:");
    dataLogLn("    Previous checkpoint stack position: ", RawPointer(previousStackCheckpoint));
    dataLogLn("    Current checkpoint stack position: ", RawPointer(currentStackCheckpoint));
    dataLogLn("    Stack between checkpoints: ", stackBetweenCheckpoints);
    dataLogLn("    ReservedZone space: ", m_checker.m_reservedZone);
    dataLogLn();
    if constexpr (verboseStackCheckVerification) {
        dataLogLn("    Stack at previous checkpoint:");
        dataLogLn(StackTracePrinter { m_savedLastCheckpointStackTrace->stack(), "      " });
        dataLogLn("    Stack at current checkpoint:");
        dataLogLn(StackTracePrinter { m_checker.m_lastCheckpointStackTrace->stack(), "      " });
    } else {
        dataLogLn("    To see the stack traces at the 2 checkpoints, set verboseStackCheckVerification to true in StackCheck.h, rebuild, and re-run your test.");
        dataLogLn();
    }

    RELEASE_ASSERT(stackBetweenCheckpoints > 0);
    RELEASE_ASSERT(previousStackCheckpoint - currentStackCheckpoint < static_cast<ptrdiff_t>(m_checker.m_reservedZone));
    RELEASE_ASSERT_NOT_REACHED();
}

#endif // VERIFY_STACK_CHECK_RESERVED_ZONE_SIZE

} // namespace WTF

