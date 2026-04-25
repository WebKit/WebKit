/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <algorithm>
#include <wtf/ArgumentCoder.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

namespace Style {
struct DynamicRangeLimit;
}

class PlatformDynamicRangeLimit {
public:
    using ValueType = float;

    static constexpr PlatformDynamicRangeLimit standard() { return PlatformDynamicRangeLimit(standardValue); }
    static constexpr PlatformDynamicRangeLimit constrained() { return PlatformDynamicRangeLimit(constrainedValue); }
    static constexpr PlatformDynamicRangeLimit noLimit() { return PlatformDynamicRangeLimit(noLimitValue); }

    static constexpr PlatformDynamicRangeLimit initialValue() { return noLimit(); }
    static constexpr PlatformDynamicRangeLimit initialValueForVideos() { return noLimit(); }

    static constexpr PlatformDynamicRangeLimit defaultWhenSuppressingHDR() { return standard(); }
    static constexpr PlatformDynamicRangeLimit defaultWhenSuppressingHDRInVideos() { return constrained(); }

    // `dynamic-range-limit` mapped to PlatformDynamicRangeLimit.value():
    // ["standard", "constrained"] -> [standard().value(), constrained().value()],
    // ["constrained", "noLimit"] -> [constrained().value(), noLimit().value()]
    constexpr ValueType value() const { return m_value; }

    constexpr auto operator<=>(const PlatformDynamicRangeLimit&) const = default;

private:
    friend struct IPC::ArgumentCoder<WebCore::PlatformDynamicRangeLimit>;
    friend Style::DynamicRangeLimit;

    constexpr PlatformDynamicRangeLimit(ValueType value) : m_value(std::clamp(value, 0.0f, 1.0f)) { }

    PlatformDynamicRangeLimit(float standardPercent, float constrainedPercent, float noLimitPercent)
        : m_value(normalizedAverage(standardPercent, constrainedPercent, noLimitPercent)) { }

    static ValueType normalizedAverage(float standardPercent, float constrainedPercent, float noLimitPercent);

    static constexpr ValueType standardValue = 0;
    static constexpr ValueType constrainedValue = 0.5;
    static constexpr ValueType noLimitValue = 1;

    ValueType m_value { noLimitValue };
};

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, PlatformDynamicRangeLimit);

} // namespace WebCore
