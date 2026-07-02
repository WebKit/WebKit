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
#include <wtf/PrintStream.h>

namespace WTF {

// A timestamp that has gone through privacy-driven coarsening via ReducedResolutionSeconds::reduce.
// Unit-conversion methods round at microsecond precision so that callers observe consistent
// DOMHighResTimeStamp values regardless of the conversion path they take, absorbing the
// sub-microsecond IEEE-754 noise introduced by seconds-double ↔ milliseconds-double conversion.
class ReducedResolutionSeconds final : public GenericTimeMixin<ReducedResolutionSeconds> {
public:
    static constexpr ClockType clockType = ClockType::Monotonic;

    constexpr ReducedResolutionSeconds() = default;

    static constexpr ReducedResolutionSeconds fromSeconds(Seconds value)
    {
        return fromRawSeconds(value.value());
    }

    static constexpr ReducedResolutionSeconds reduce(Seconds value, Seconds resolution)
    {
        return fromRawSeconds(std::floor(value.value() / resolution.value()) * resolution.value());
    }

    double seconds() const { return std::round(m_value * 1000000.0) / 1000000.0; }
    double milliseconds() const { return std::round(m_value * 1000000.0) / 1000.0; }
    double microseconds() const { return std::round(m_value * 1000000.0); }

    void dump(PrintStream& out) const { out.print(milliseconds(), " ms"); }

    friend struct MarkableTraits<ReducedResolutionSeconds>;

private:
    using GenericTimeMixin<ReducedResolutionSeconds>::secondsSinceEpoch;
    friend class GenericTimeMixin<ReducedResolutionSeconds>;
    constexpr ReducedResolutionSeconds(double rawValue)
        : GenericTimeMixin<ReducedResolutionSeconds>(rawValue)
    {
    }
};

static_assert(sizeof(ReducedResolutionSeconds) == sizeof(double));

template<>
struct MarkableTraits<ReducedResolutionSeconds> {
    static bool isEmptyValue(ReducedResolutionSeconds time)
    {
        return std::isnan(time.m_value);
    }

    static constexpr ReducedResolutionSeconds emptyValue()
    {
        return ReducedResolutionSeconds::nan();
    }
};

} // namespace WTF

using WTF::ReducedResolutionSeconds;
