/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include <wtf/Forward.h>
#include <wtf/GenericTimeMixin.h>

namespace WTF {

class ContinuousTime;
class WallTime;
class PrintStream;

// A fast (where available) implementation of MonotonicTime using hardware counters
// and without barriers. Because UnbarrieredMonotonicTime does not use barriers,
// its read of the hardware counter may be executed out of order (OoO) relative to
// adjacent instructions on modern CPUs that support OoO execution. This is the
// tradeoff that UnbarrieredMonotonicTime makes in order to be fast.
//
// Because of this potential OoO effect, the values time values read using
// UnbarrieredMonotonicTime may be slightly off / inaccurate. For some applications,
// this slight loss of accuracy is not consequential.
//
// For other applications where accuracy is important (e.g. profiling of the execution
// time of a region of code), it is important that the HW counter is read in the right
// order. In such a scenario, the client should apply their own barriers before/after
// UnbarrieredMonotonicTime, or alternately, just use MonotonicTime instead.
//
// At present, availability would only be on OS(DARWIN) and CPU(ARM64). For all
// other ports, UnbarrieredMonotonicTime will just forward to MonotonicTime.
// UnbarrieredMonotonicTime is expected to have the same timer resolution as
// MonotonicTime (unlike the coarse ApproximateTime).

#if OS(DARWIN) && CPU(ARM64)
#define USE_HARDWARE_UNBARRIERED_MONOTONIC_TIME 1
#else
#define USE_HARDWARE_UNBARRIERED_MONOTONIC_TIME 0
#endif

class UnbarrieredMonotonicTime final : public GenericTimeMixin<UnbarrieredMonotonicTime> {
public:
    static constexpr ClockType clockType = ClockType::UnbarrieredMonotonic;

    // This is the epoch. So, x.secondsSinceEpoch() should be the same as x - UnbarrieredMonotonicTime().
    constexpr UnbarrieredMonotonicTime() = default;

#if USE(HARDWARE_UNBARRIERED_MONOTONIC_TIME)
    static inline UnbarrieredMonotonicTime now();
#else
    WTF_EXPORT_PRIVATE static UnbarrieredMonotonicTime now();
#endif

    WTF_EXPORT_PRIVATE void dump(PrintStream&) const;

    friend struct MarkableTraits<UnbarrieredMonotonicTime>;

private:
    friend class GenericTimeMixin<UnbarrieredMonotonicTime>;
    constexpr UnbarrieredMonotonicTime(double rawValue)
        : GenericTimeMixin<UnbarrieredMonotonicTime>(rawValue)
    {
    }

#if USE(HARDWARE_UNBARRIERED_MONOTONIC_TIME)
    struct Calibration {
        uint64_t startCounter;
        double startNanoseconds;
        double nanosecondsPerTick;
    };

    WTF_EXPORT_PRIVATE static void calibrate();
    ALWAYS_INLINE static uint64_t readCounter();

    WTF_EXPORT_PRIVATE static Calibration s_calibration;
#endif
};
static_assert(sizeof(UnbarrieredMonotonicTime) == sizeof(double));

#if USE(HARDWARE_UNBARRIERED_MONOTONIC_TIME)
ALWAYS_INLINE uint64_t UnbarrieredMonotonicTime::readCounter()
{
    uint64_t val;
    __asm__ volatile("mrs %0, CNTVCT_EL0" : "=r"(val) : : "memory");
    return val;
}

inline UnbarrieredMonotonicTime UnbarrieredMonotonicTime::now()
{
    if (!s_calibration.nanosecondsPerTick) [[unlikely]]
        calibrate();

    constexpr double nanosecondsPerSecond = 1000'000'000;
    double nanoseconds = (readCounter() - s_calibration.startCounter) * s_calibration.nanosecondsPerTick + s_calibration.startNanoseconds;
    return UnbarrieredMonotonicTime::fromRawSeconds(nanoseconds / nanosecondsPerSecond);
}
#endif // USE(HARDWARE_UNBARRIERED_MONOTONIC_TIME)

template<>
struct MarkableTraits<UnbarrieredMonotonicTime> {
    static bool isEmptyValue(UnbarrieredMonotonicTime time)
    {
        return std::isnan(time.m_value);
    }

    static constexpr UnbarrieredMonotonicTime emptyValue()
    {
        return UnbarrieredMonotonicTime::nan();
    }
};

} // namespace WTF

using WTF::UnbarrieredMonotonicTime;
