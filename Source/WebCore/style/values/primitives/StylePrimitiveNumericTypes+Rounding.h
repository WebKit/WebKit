/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#include <cmath>
#include <concepts>
#include <limits>

namespace WebCore {
namespace Style {

// Dimension calculations are imprecise, often resulting in values of e.g.
// 44.99998. We need to round if we're really close to the next integer value.
template<typename T> inline T roundForImpreciseConversion(double value)
{
    if constexpr (std::floating_point<T>) {
        double ceiledValue = std::ceil(value);
        double proximityToNextInt = ceiledValue - value;
        if (proximityToNextInt <= 0.01 && value > 0)
            return static_cast<T>(ceiledValue);
        if (proximityToNextInt >= 0.99 && value < 0)
            return static_cast<T>(floor(value));
        return static_cast<T>(value);
    } else if constexpr (std::integral<T>) {
        value += (value < 0) ? -0.01 : +0.01;
        return ((value > std::numeric_limits<T>::max()) || (value < std::numeric_limits<T>::min())) ? 0 : static_cast<T>(value);
    }
}

} // namespace Style
} // namespace WebCore
