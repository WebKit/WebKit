/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include <wtf/Seconds.h>

#include <wtf/Condition.h>
#include <wtf/Lock.h>
#include <wtf/MonotonicTime.h>
#include <wtf/PrintStream.h>
#include <wtf/TimeWithDynamicClockType.h>
#include <wtf/WallTime.h>
#include <wtf/text/TextStream.h>

namespace WTF {

WallTime Seconds::operator+(WallTime other) const
{
    return other + *this;
}

MonotonicTime Seconds::operator+(MonotonicTime other) const
{
    return other + *this;
}

TimeWithDynamicClockType Seconds::operator+(const TimeWithDynamicClockType& other) const
{
    return other + *this;
}

WallTime Seconds::operator-(WallTime other) const
{
    return WallTime::fromRawSeconds(value() - other.secondsSinceEpoch().value());
}

MonotonicTime Seconds::operator-(MonotonicTime other) const
{
    return MonotonicTime::fromRawSeconds(value() - other.secondsSinceEpoch().value());
}

TimeWithDynamicClockType Seconds::operator-(const TimeWithDynamicClockType& other) const
{
    return other.withSameClockAndRawSeconds(value() - other.secondsSinceEpoch().value());
}

void Seconds::dump(PrintStream& out) const
{
    out.print(m_value, " sec");
}

TextStream& operator<<(TextStream& ts, Seconds seconds)
{
    ts << seconds.value() << "s";
    return ts;
}

void sleep(Seconds value)
{
    // It's very challenging to find portable ways of sleeping for less than a second. On UNIX, you want to
    // use usleep() but it's hard to #include it in a portable way (you'd think it's in unistd.h, but then
    // you'd be wrong on some OSX SDKs). Also, usleep() won't save you on Windows. Hence, bottoming out in
    // lock code, which already solves the sleeping problem, is probably for the best.

    Lock fakeLock;
    Condition fakeCondition;
    LockHolder fakeLocker(fakeLock);
    fakeCondition.waitFor(fakeLock, value);
}

} // namespace WTF

