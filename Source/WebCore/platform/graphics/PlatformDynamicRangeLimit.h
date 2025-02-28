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
    constexpr PlatformDynamicRangeLimit() = default;

    static constexpr PlatformDynamicRangeLimit standard() { return PlatformDynamicRangeLimit(standardValue); }
    static constexpr PlatformDynamicRangeLimit constrainedHigh() { return PlatformDynamicRangeLimit(constrainedHighValue); }
    static constexpr PlatformDynamicRangeLimit noLimit() { return PlatformDynamicRangeLimit(noLimitValue); }

    // `dynamic-range-limit` mapped to PlatformDynamicRangeLimit.value():
    // ["standard", "constrainedHigh"] -> [standard().value(), constrainedHigh().value()],
    // ["constrainedHigh", "noLimit"] -> [constrainedHigh().value(), noLimit().value()]
    constexpr float value() const { return m_value; }

    constexpr auto operator<=>(const PlatformDynamicRangeLimit&) const = default;

private:
    friend struct IPC::ArgumentCoder<WebCore::PlatformDynamicRangeLimit, void>;
    friend Style::DynamicRangeLimit;

    constexpr PlatformDynamicRangeLimit(float value) : m_value(std::clamp(value, 0.0f, 1.0f)) { }

    PlatformDynamicRangeLimit(float standardPercent, float constrainedHighPercent, float noLimitPercent)
        : m_value(normalizedAverage(standardPercent, constrainedHighPercent, noLimitPercent)) { }

    static float normalizedAverage(float standardPercent, float constrainedHighPercent, float noLimitPercent);

    static constexpr float standardValue = 0;
    static constexpr float constrainedHighValue = 0.5;
    static constexpr float noLimitValue = 1;

    float m_value { constrainedHighValue };
};

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, PlatformDynamicRangeLimit);

} // namespace WebCore
