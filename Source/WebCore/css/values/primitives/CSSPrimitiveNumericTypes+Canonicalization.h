/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSPrimitiveNumericTypes.h"

namespace WebCore {

struct NoConversionDataRequiredToken;

namespace CSS {

// MARK: Integer

// There are no integer units, so canonicalization is trivially just return the underlying value.

template<auto R, typename V> double canonicalize(IntegerRaw<R, V> raw)
{
    return raw.value;
}

// MARK: Number

// There are no number units, so canonicalization is trivially just return the underlying value.

template<auto R, typename V> double canonicalize(NumberRaw<R, V> raw)
{
    return raw.value;
}

// MARK: Percentage

// There are no percentage units, so canonicalization is trivially just return the underlying value.

template<auto R, typename V> double canonicalize(PercentageRaw<R, V> raw)
{
    return raw.value;
}

// MARK: Angle

double canonicalizeAngle(double value, AngleUnit);

template<auto R, typename V> double canonicalize(AngleRaw<R, V> raw)
{
    return canonicalizeAngle(raw.value, raw.unit);
}

// MARK: Time

double canonicalizeTime(double, TimeUnit);

template<auto R, typename V> double canonicalize(TimeRaw<R, V> raw)
{
    return canonicalizeTime(raw.value, raw.unit);
}

// MARK: Frequency

double canonicalizeFrequency(double, FrequencyUnit);

template<auto R, typename V> double canonicalize(FrequencyRaw<R, V> raw)
{
    return canonicalizeFrequency(raw.value, raw.unit);
}

// MARK: Resolution

double canonicalizeResolution(double, ResolutionUnit);

template<auto R, typename V> double canonicalize(ResolutionRaw<R, V> raw)
{
    return canonicalizeResolution(raw.value, raw.unit);
}

// MARK: Flex

// There is only one flex unit, `fr`, so canonicalization is trivially just return the underlying value.

template<auto R, typename V> double canonicalize(FlexRaw<R, V> raw)
{
    return raw.value;
}

} // namespace CSS
} // namespace WebCore
