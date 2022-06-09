/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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

#include <wtf/ClockType.h>
#include <wtf/GenericTimeMixin.h>

namespace WTF {

class MonotonicTime;
class PrintStream;

// The current time according to a wall clock (aka real time clock). This uses floating point
// internally so that you can reason about infinity and other things that arise in math. It's
// acceptable to use this to wrap NaN times, negative times, and infinite times, so long as they
// are relative to the same clock. Use this only if wall clock time is needed. For elapsed time
// measurement use MonotonicTime instead.
class WallTime final : public GenericTimeMixin<WallTime> {
public:
    static constexpr ClockType clockType = ClockType::Wall;
    
    // This is the epoch. So, x.secondsSinceEpoch() should be the same as x - WallTime().
    constexpr WallTime() = default;

    WTF_EXPORT_PRIVATE static WallTime now();
    
    WallTime approximateWallTime() const { return *this; }
    WTF_EXPORT_PRIVATE MonotonicTime approximateMonotonicTime() const;
    
    WTF_EXPORT_PRIVATE void dump(PrintStream&) const;
    
    struct MarkableTraits;

private:
    friend class GenericTimeMixin<WallTime>;
    constexpr WallTime(double rawValue)
        : GenericTimeMixin<WallTime>(rawValue)
    {
    }
};
static_assert(sizeof(WallTime) == sizeof(double));

struct WallTime::MarkableTraits {
    static bool isEmptyValue(WallTime time)
    {
        return std::isnan(time.m_value);
    }

    static constexpr WallTime emptyValue()
    {
        return WallTime::nan();
    }
};

WTF_EXPORT_PRIVATE void sleep(WallTime);

} // namespace WTF

namespace std {

inline bool isnan(WTF::WallTime time)
{
    return std::isnan(time.secondsSinceEpoch().value());
}

inline bool isinf(WTF::WallTime time)
{
    return std::isinf(time.secondsSinceEpoch().value());
}

inline bool isfinite(WTF::WallTime time)
{
    return std::isfinite(time.secondsSinceEpoch().value());
}

} // namespace std

using WTF::WallTime;
