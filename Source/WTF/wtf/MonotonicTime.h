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
#include <wtf/Seconds.h>

namespace WTF {

class WallTime;
class PrintStream;

// The current time according to a monotonic clock. Monotonic clocks don't go backwards and
// possibly don't count downtime. This uses floating point internally so that you can reason about
// infinity and other things that arise in math. It's acceptable to use this to wrap NaN times,
// negative times, and infinite times, so long as they are all relative to the same clock.
class MonotonicTime final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static constexpr ClockType clockType = ClockType::Monotonic;
    
    // This is the epoch. So, x.secondsSinceEpoch() should be the same as x - MonotonicTime().
    constexpr MonotonicTime() { }
    
    // Call this if you know for sure that the double represents monotonic time according to the
    // same time source as MonotonicTime. It must be in seconds.
    static constexpr MonotonicTime fromRawSeconds(double value)
    {
        return MonotonicTime(value);
    }
    
    WTF_EXPORT_PRIVATE static MonotonicTime now();
    
    static constexpr MonotonicTime infinity() { return fromRawSeconds(std::numeric_limits<double>::infinity()); }
    static constexpr MonotonicTime nan() { return fromRawSeconds(std::numeric_limits<double>::quiet_NaN()); }

    constexpr Seconds secondsSinceEpoch() const { return Seconds(m_value); }
    
    MonotonicTime approximateMonotonicTime() const { return *this; }
    WTF_EXPORT_PRIVATE WallTime approximateWallTime() const;
    
    explicit constexpr operator bool() const { return !!m_value; }
    
    constexpr MonotonicTime operator+(Seconds other) const
    {
        return fromRawSeconds(m_value + other.value());
    }
    
    constexpr MonotonicTime operator-(Seconds other) const
    {
        return fromRawSeconds(m_value - other.value());
    }

    Seconds operator%(Seconds other) const
    {
        return Seconds { fmod(m_value, other.value()) };
    }
    
    // Time is a scalar and scalars can be negated as this could arise from algebraic
    // transformations. So, we allow it.
    constexpr MonotonicTime operator-() const
    {
        return fromRawSeconds(-m_value);
    }
    
    MonotonicTime operator+=(Seconds other)
    {
        return *this = *this + other;
    }
    
    MonotonicTime operator-=(Seconds other)
    {
        return *this = *this - other;
    }
    
    constexpr Seconds operator-(MonotonicTime other) const
    {
        return Seconds(m_value - other.m_value);
    }
    
    constexpr bool operator==(MonotonicTime other) const
    {
        return m_value == other.m_value;
    }
    
    constexpr bool operator!=(MonotonicTime other) const
    {
        return m_value != other.m_value;
    }
    
    constexpr bool operator<(MonotonicTime other) const
    {
        return m_value < other.m_value;
    }
    
    constexpr bool operator>(MonotonicTime other) const
    {
        return m_value > other.m_value;
    }
    
    constexpr bool operator<=(MonotonicTime other) const
    {
        return m_value <= other.m_value;
    }
    
    constexpr bool operator>=(MonotonicTime other) const
    {
        return m_value >= other.m_value;
    }
    
    WTF_EXPORT_PRIVATE void dump(PrintStream&) const;

    MonotonicTime isolatedCopy() const
    {
        return *this;
    }

    template<class Encoder>
    void encode(Encoder& encoder) const
    {
        encoder << m_value;
    }

    template<class Decoder>
    static Optional<MonotonicTime> decode(Decoder& decoder)
    {
        Optional<double> time;
        decoder >> time;
        if (!time)
            return WTF::nullopt;
        return MonotonicTime::fromRawSeconds(*time);
    }

    template<class Decoder>
    static bool decode(Decoder& decoder, MonotonicTime& time)
    {
        double value;
        if (!decoder.decode(value))
            return false;

        time = MonotonicTime::fromRawSeconds(value);
        return true;
    }

    struct MarkableTraits;

private:
    constexpr MonotonicTime(double rawValue)
        : m_value(rawValue)
    {
    }

    double m_value { 0 };
};

struct MonotonicTime::MarkableTraits {
    static bool isEmptyValue(MonotonicTime time)
    {
        return std::isnan(time.m_value);
    }

    static constexpr MonotonicTime emptyValue()
    {
        return MonotonicTime::nan();
    }
};

} // namespace WTF

namespace std {

inline bool isnan(WTF::MonotonicTime time)
{
    return std::isnan(time.secondsSinceEpoch().value());
}

inline bool isinf(WTF::MonotonicTime time)
{
    return std::isinf(time.secondsSinceEpoch().value());
}

inline bool isfinite(WTF::MonotonicTime time)
{
    return std::isfinite(time.secondsSinceEpoch().value());
}

} // namespace std

using WTF::MonotonicTime;
