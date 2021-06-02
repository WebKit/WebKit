/*
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>
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

#include "config.h"
#include <wtf/CPUTime.h>

#include <windows.h>

namespace WTF {

static Seconds fileTimeToSeconds(const FILETIME& fileTime)
{
    // https://msdn.microsoft.com/ja-jp/library/windows/desktop/ms683223(v=vs.85).aspx
    // "All times are expressed using FILETIME data structures. Such a structure contains
    // two 32-bit values that combine to form a 64-bit count of 100-nanosecond time units."

    const constexpr double hundredsOfNanosecondsPerSecond = (1000.0 * 1000.0 * 1000.0) / 100.0;

    // https://msdn.microsoft.com/ja-jp/library/windows/desktop/ms724284(v=vs.85).aspx
    // "It is not recommended that you add and subtract values from the FILETIME structure to obtain relative times.
    // Instead, you should copy the low- and high-order parts of the file time to a ULARGE_INTEGER structure,
    // perform 64-bit arithmetic on the QuadPart member, and copy the LowPart and HighPart members into the FILETIME structure."
    ULARGE_INTEGER durationIn100NS;
    memcpy(&durationIn100NS, &fileTime, sizeof(durationIn100NS));
    return Seconds { durationIn100NS.QuadPart / hundredsOfNanosecondsPerSecond };
}

std::optional<CPUTime> CPUTime::get()
{
    // https://msdn.microsoft.com/ja-jp/library/windows/desktop/ms683223(v=vs.85).aspx
    FILETIME creationTime;
    FILETIME exitTime;
    FILETIME kernelTime;
    FILETIME userTime;
    if (!::GetProcessTimes(::GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime))
        return std::nullopt;

    return CPUTime { MonotonicTime::now(), fileTimeToSeconds(userTime), fileTimeToSeconds(kernelTime) };
}

Seconds CPUTime::forCurrentThread()
{
    FILETIME userTime, kernelTime, creationTime, exitTime;

    BOOL ret = GetThreadTimes(GetCurrentThread(), &creationTime, &exitTime, &kernelTime, &userTime);
    RELEASE_ASSERT(ret);

    return fileTimeToSeconds(userTime) + fileTimeToSeconds(kernelTime);
}

}
