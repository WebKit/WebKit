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
    NODELETE WEBCORE_EXPORT WebAnimationTime(std::optional<Seconds>, std::optional<double>);

    NODELETE WEBCORE_EXPORT WebAnimationTime(const Seconds&);
    WEBCORE_EXPORT WebAnimationTime(const ReducedResolutionSeconds&);
    WebAnimationTime(const CSSNumberish&);

    NODELETE static WebAnimationTime fromMilliseconds(double);
    NODELETE WEBCORE_EXPORT static WebAnimationTime fromPercentage(double);

    NODELETE WEBCORE_EXPORT std::optional<Seconds> time() const;
    NODELETE WEBCORE_EXPORT std::optional<double> percentage() const;

    NODELETE bool isValid() const;
    NODELETE bool isInfinity() const;
    NODELETE bool isZero() const;
    NODELETE bool isNaN() const;

    NODELETE WebAnimationTime matchingZero() const;
    NODELETE WebAnimationTime matchingEpsilon() const;
    NODELETE WebAnimationTime matchingInfinity() const;

    NODELETE bool approximatelyEqualTo(const WebAnimationTime&) const;
    NODELETE bool approximatelyLessThan(const WebAnimationTime&) const;
    NODELETE bool approximatelyGreaterThan(const WebAnimationTime&) const;

    NODELETE WebAnimationTime operator+(const WebAnimationTime&) const;
    NODELETE WebAnimationTime operator-(const WebAnimationTime&) const;
    NODELETE double operator/(const WebAnimationTime&) const;
    NODELETE WebAnimationTime& operator+=(const WebAnimationTime&);
    NODELETE WebAnimationTime& operator-=(const WebAnimationTime&);

    friend bool operator==(const WebAnimationTime&, const WebAnimationTime&) = default;
    friend std::partial_ordering operator<=>(const WebAnimationTime&, const WebAnimationTime&);
    friend std::partial_ordering operator<=>(const WebAnimationTime&, Seconds);

    NODELETE WebAnimationTime operator+(const Seconds&) const;
    NODELETE WebAnimationTime operator-(const Seconds&) const;
    NODELETE bool operator==(const Seconds&) const;

    NODELETE WebAnimationTime operator*(double) const;
    NODELETE WebAnimationTime operator/(double) const;

    NODELETE WEBCORE_EXPORT operator Seconds() const;
    operator CSSNumberish() const;

    void dump(TextStream&) const;

private:
    enum class Type : uint8_t { Unknown, Time, Percentage };

    NODELETE WebAnimationTime(Type, double);

    Type m_type { Type::Unknown };
    double m_value { 0 };
};

NODELETE std::partial_ordering operator<=>(const WebAnimationTime&, const WebAnimationTime&);
NODELETE std::partial_ordering operator<=>(const WebAnimationTime&, Seconds);

TextStream& operator<<(TextStream&, const WebAnimationTime&);

} // namespace WebCore
