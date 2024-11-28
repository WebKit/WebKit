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

/*
 * This is the Math.sumPrecise() polyfill by Kevin Gibbons, ported to C++. Original LICENSE is as follows:
 *
 * BSD 3-Clause License
 * 
 * Copyright (c) 2024 Kevin Gibbons
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions
 * and the following disclaimer in the documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS” AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/PreciseSum.h>

#include <cmath>

namespace WTF {

static constexpr double MAX_DOUBLE = 1.79769313486231570815e+308;
static constexpr double PENULTIMATE_DOUBLE = 1.79769313486231550856e+308;
static constexpr double MAX_ULP = MAX_DOUBLE - PENULTIMATE_DOUBLE;
static constexpr double TWO_POW_1023 = 8.98846567431158e+307;

ALWAYS_INLINE std::pair<double, double> twosum(double x, double y)
{
    double hi = x + y;
    double lo = y - (hi - x);
    return std::make_pair(hi, lo);
}

void PreciseSum::add(double x)
{
    if (!(!x && std::signbit(x)))
        m_everyValueIsNegativeZero = false;

    unsigned actuallyUsedPartials = 0;

    for (double y : m_partials) {
        if (std::fabs(x) < std::fabs(y))
            std::swap(x, y);

        auto pair = twosum(x, y);
        if (std::isinf(std::fabs(pair.first))) {
            double sign = pair.first < 0 ? -1 : 1;
            m_overflow += sign;

            x = (x - sign * TWO_POW_1023) - sign * TWO_POW_1023;
            if (std::fabs(x) < std::fabs(y))
                std::swap(x, y);

            pair = twosum(x, y);
        }

        if (auto lo = pair.second) {
            m_partials[actuallyUsedPartials] = lo;
            ++actuallyUsedPartials;
        }

        x = pair.first;
    }

    m_partials.shrink(actuallyUsedPartials);

    if (x)
        m_partials.append(x);
}

double PreciseSum::compute()
{
    if (m_everyValueIsNegativeZero)
        return -0.0;

    int32_t n = m_partials.size() - 1;
    double hi = 0;
    double lo = 0;

    if (m_overflow) {
        double next = n >= 0 ? m_partials[n] : 0;
        --n;
        if (std::fabs(m_overflow) > 1 || (m_overflow > 0 && next > 0) || (m_overflow < 0 && next < 0))
            return m_overflow > 0 ? std::numeric_limits<double>::infinity() : -std::numeric_limits<double>::infinity();

        auto pair = twosum(m_overflow * TWO_POW_1023, next / 2);
        hi = pair.first;
        lo = pair.second * 2;

        if (std::isinf(std::fabs(hi * 2))) {
            // silly edge case: rounding to the maximum value
            // MAX_DOUBLE has a 1 in the last place of its significand, so if we subtract exactly half a ULP from 2**1024, the result rounds away from it (i.e. to infinity) under ties-to-even
            // but if the next partial has the opposite sign of the current value, we need to round towards MAX_DOUBLE instead
            // this is the same as the "handle rounding" case below, but there's only one potentially-finite case we need to worry about, so we just hardcode that one
            if (hi > 0) {
                if (hi == TWO_POW_1023 && lo == -(MAX_ULP / 2) && n >= 0 && m_partials[n] < 0)
                    return MAX_DOUBLE;
                return std::numeric_limits<double>::infinity();
            }

            if (hi == -TWO_POW_1023 && lo == (MAX_ULP / 2) && n >= 0 && m_partials[n] > 0)
                return -MAX_DOUBLE;
            return -std::numeric_limits<double>::infinity();
        }

        if (lo) {
            m_partials[n + 1] = lo;
            ++n;
            lo = 0;
        }

        hi *= 2;
    }

    while (n >= 0) {
        double x = hi;
        double y = m_partials[n];
        --n;

        auto pair = twosum(x, y);
        hi = pair.first;
        lo = pair.second;

        if (lo)
            break;
    }

    // handle rounding
    // when the roundoff error is exactly half of the ULP for the result, we need to check one more partial to know which way to round
    if (n >= 0 && ((lo < 0 && m_partials[n] < 0) || (lo > 0 && m_partials[n] > 0))) {
        double y = lo * 2;
        double x = hi + y;
        double yr = x - hi;
        if (y == yr)
            hi = x;
    }

    return hi;
}

} // namespace WTF
