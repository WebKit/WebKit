/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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

#include <wtf/Assertions.h>
#include <wtf/MainThread.h>
#include <wtf/ProcessID.h>
#include <wtf/text/StringConcatenate.h>

#define SLEEP_THREAD_FOR_DEBUGGER() \
do { \
    WTFReportError(__FILE__, __LINE__, WTF_PRETTY_FUNCTION, "Sleeping thread for debugger; attach to process (PID: %d) to unsleep the thread.", getCurrentProcessID()); \
    do { \
        sleep(1); \
        if (WTFIsDebuggerAttached()) \
            break; \
    } while (1); \
    WTFBreakpointTrap(); \
} while (0)

namespace WTF {

template<typename StringType>
const char* debugString(StringType string)
{
    return debugString(string, "");
}

template<typename... StringTypes>
const char* debugString(StringTypes... strings)
{
    String result = tryMakeString(strings...);
    if (!result)
        CRASH();

    auto cString = result.utf8();
    const char* cStringData = cString.data();

    callOnMainThread([cString = WTFMove(cString)] {
    });

    return cStringData;
}

} // namespace WTF

using WTF::debugString;
