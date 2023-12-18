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

#include <wtf/ClockType.h>
#include <wtf/GenericTimeMixin.h>

namespace WTF {

class WallTime;
class PrintStream;

// The current time according to an approximate monotonic clock. Similar to MonotonicTime, but its resolution is
// coarse, instead ApproximateTime::now() is much faster than MonotonicTime::now().
class ApproximateTime final : public GenericTimeMixin<ApproximateTime> {
public:
    static constexpr ClockType clockType = ClockType::Approximate;

    // This is the epoch. So, x.secondsSinceEpoch() should be the same as x - ApproximateTime().
    constexpr ApproximateTime() = default;

#if OS(DARWIN)
    WTF_EXPORT_PRIVATE static ApproximateTime fromMachApproximateTime(uint64_t);
    WTF_EXPORT_PRIVATE uint64_t toMachApproximateTime() const;
#endif

    WTF_EXPORT_PRIVATE static ApproximateTime now();

    ApproximateTime approximateApproximateTime() const { return *this; }
    WTF_EXPORT_PRIVATE WallTime approximateWallTime() const;
    WTF_EXPORT_PRIVATE MonotonicTime approximateMonotonicTime() const;

    WTF_EXPORT_PRIVATE void dump(PrintStream&) const;

    struct MarkableTraits;

private:
    friend class GenericTimeMixin<ApproximateTime>;
    constexpr ApproximateTime(double rawValue)
        : GenericTimeMixin<ApproximateTime>(rawValue)
    {
    }
};
static_assert(sizeof(ApproximateTime) == sizeof(double));

struct ApproximateTime::MarkableTraits {
    static bool isEmptyValue(ApproximateTime time)
    {
        return time.isNaN();
    }

    static constexpr ApproximateTime emptyValue()
    {
        return ApproximateTime::nan();
    }
};

} // namespace WTF

using WTF::ApproximateTime;
