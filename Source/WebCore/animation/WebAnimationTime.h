/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include <WebCore/CSSNumericValue.h>
#include <wtf/Forward.h>
#include <wtf/Seconds.h>

namespace WebCore {

class WebAnimationTime {
public:
    WebAnimationTime() = default;
    WEBCORE_EXPORT WebAnimationTime(std::optional<Seconds>, std::optional<double>);

    WEBCORE_EXPORT WebAnimationTime(const Seconds&);
    WEBCORE_EXPORT WebAnimationTime(const ReducedResolutionSeconds&);
    WebAnimationTime(const CSSNumberish&);

    static WebAnimationTime LEGACY_NODELETE fromMilliseconds(double);
    WEBCORE_EXPORT static WebAnimationTime LEGACY_NODELETE fromPercentage(double);

    WEBCORE_EXPORT std::optional<Seconds> LEGACY_NODELETE time() const;
    WEBCORE_EXPORT std::optional<double> LEGACY_NODELETE percentage() const;

    bool LEGACY_NODELETE isValid() const;
    bool LEGACY_NODELETE isInfinity() const;
    bool LEGACY_NODELETE isZero() const;
    bool LEGACY_NODELETE isNaN() const;

    WebAnimationTime LEGACY_NODELETE matchingZero() const;
    WebAnimationTime LEGACY_NODELETE matchingEpsilon() const;
    WebAnimationTime LEGACY_NODELETE matchingInfinity() const;

    bool LEGACY_NODELETE approximatelyEqualTo(const WebAnimationTime&) const;
    bool LEGACY_NODELETE approximatelyLessThan(const WebAnimationTime&) const;
    bool LEGACY_NODELETE approximatelyGreaterThan(const WebAnimationTime&) const;

    WebAnimationTime LEGACY_NODELETE operator+(const WebAnimationTime&) const;
    WebAnimationTime LEGACY_NODELETE operator-(const WebAnimationTime&) const;
    double LEGACY_NODELETE operator/(const WebAnimationTime&) const;
    WebAnimationTime& LEGACY_NODELETE operator+=(const WebAnimationTime&);
    WebAnimationTime& LEGACY_NODELETE operator-=(const WebAnimationTime&);

    friend bool operator==(const WebAnimationTime&, const WebAnimationTime&) = default;
    friend std::partial_ordering LEGACY_NODELETE operator<=>(const WebAnimationTime&, const WebAnimationTime&);
    friend std::partial_ordering LEGACY_NODELETE operator<=>(const WebAnimationTime&, Seconds);

    WebAnimationTime LEGACY_NODELETE operator+(const Seconds&) const;
    WebAnimationTime LEGACY_NODELETE operator-(const Seconds&) const;
    bool LEGACY_NODELETE operator==(const Seconds&) const;

    WebAnimationTime LEGACY_NODELETE operator*(double) const;
    WebAnimationTime LEGACY_NODELETE operator/(double) const;

    WEBCORE_EXPORT operator Seconds LEGACY_NODELETE () const;
    operator CSSNumberish() const;

    void dump(TextStream&) const;

private:
    enum class Type : uint8_t { Unknown, Time, Percentage };

    WebAnimationTime(Type, double);

    Type m_type { Type::Unknown };
    double m_value { 0 };
};

TextStream& operator<<(TextStream&, const WebAnimationTime&);

} // namespace WebCore
