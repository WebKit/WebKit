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

#include "CSSNumericValue.h"
#include <wtf/Seconds.h>

namespace WebCore {

class CSSNumberishTime {
public:
    CSSNumberishTime() = default;
    WEBCORE_EXPORT CSSNumberishTime(std::optional<Seconds>, std::optional<double>);

    CSSNumberishTime(const Seconds&);
    CSSNumberishTime(const CSSNumberish&);

    static CSSNumberishTime fromMilliseconds(double);
    static CSSNumberishTime fromPercentage(double);

    WEBCORE_EXPORT std::optional<Seconds> time() const;
    WEBCORE_EXPORT std::optional<double> percentage() const;

    bool isValid() const;
    bool isInfinity() const;
    bool isZero() const;

    CSSNumberishTime matchingZero() const;

    bool approximatelyEqualTo(const CSSNumberishTime&) const;
    bool approximatelyLessThan(const CSSNumberishTime&) const;
    bool approximatelyGreaterThan(const CSSNumberishTime&) const;

    CSSNumberishTime operator+(const CSSNumberishTime&) const;
    CSSNumberishTime operator-(const CSSNumberishTime&) const;
    double operator/(const CSSNumberishTime&) const;
    CSSNumberishTime& operator+=(const CSSNumberishTime&);
    CSSNumberishTime& operator-=(const CSSNumberishTime&);
    bool operator<(const CSSNumberishTime&) const;
    bool operator<=(const CSSNumberishTime&) const;
    bool operator>(const CSSNumberishTime&) const;
    bool operator>=(const CSSNumberishTime&) const;
    bool operator==(const CSSNumberishTime&) const;

    CSSNumberishTime operator+(const Seconds&) const;
    CSSNumberishTime operator-(const Seconds&) const;
    bool operator<(const Seconds&) const;
    bool operator<=(const Seconds&) const;
    bool operator>(const Seconds&) const;
    bool operator>=(const Seconds&) const;
    bool operator==(const Seconds&) const;

    CSSNumberishTime operator*(double) const;
    CSSNumberishTime operator/(double) const;

    operator Seconds() const;
    operator CSSNumberish() const;

    void dump(TextStream&) const;

private:
    enum class Type : uint8_t { Unknown, Time, Percentage };

    CSSNumberishTime(Type, double);

    Type m_type { Type::Unknown };
    double m_value { 0 };
};

TextStream& operator<<(TextStream&, const CSSNumberishTime&);

} // namespace WebCore
