/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Interval arithmetic.
 *//*--------------------------------------------------------------------*/

#include "tcuInterval.hpp"

#include "deMath.h"

#include <cmath>

namespace tcu
{

using std::ldexp;

Interval applyMonotone(DoubleFunc1 &func, const Interval &arg0)
{
    Interval ret;
    TCU_INTERVAL_APPLY_MONOTONE1(ret, x, arg0, val, TCU_SET_INTERVAL(val, point, point = func(x)));
    return ret;
}

Interval applyMonotone(DoubleIntervalFunc1 &func, const Interval &arg0)
{
    return Interval(func(arg0.lo()), func(arg0.hi()));
}

Interval applyMonotone(DoubleFunc2 &func, const Interval &arg0, const Interval &arg1)
{
    Interval ret;

    TCU_INTERVAL_APPLY_MONOTONE2(ret, x, arg0, y, arg1, val, TCU_SET_INTERVAL(val, point, point = func(x, y)));

    return ret;
}

Interval applyMonotone(DoubleIntervalFunc2 &func, const Interval &arg0, const Interval &arg1)
{
    double lo0 = arg0.lo(), hi0 = arg0.hi(), lo1 = arg1.lo(), hi1 = arg1.hi();
    return Interval(Interval(func(lo0, lo1), func(lo0, hi1)), Interval(func(hi0, lo1), func(hi0, hi1)));
}

Interval operator+(const Interval &x, const Interval &y)
{
    Interval ret;

    if (!x.empty() && !y.empty())
        TCU_SET_INTERVAL_BOUNDS(ret, p, p = x.lo() + y.lo(), p = x.hi() + y.hi());
    if (x.hasNaN() || y.hasNaN())
        ret |= TCU_NAN;

    return ret;
}

Interval operator-(const Interval &x, const Interval &y)
{
    Interval ret;

    TCU_INTERVAL_APPLY_MONOTONE2(ret, xp, x, yp, y, val, TCU_SET_INTERVAL(val, point, point = xp - yp));
    return ret;
}

Interval operator*(const Interval &x, const Interval &y)
{
    Interval ret;

    TCU_INTERVAL_APPLY_MONOTONE2(ret, xp, x, yp, y, val, TCU_SET_INTERVAL(val, point, point = xp * yp));
    return ret;
}

Interval operator/(const Interval &nom, const Interval &den)
{
    if (den.contains(0.0))
    {
        // \todo [2014-03-21 lauri] Non-inf endpoint when one den endpoint is
        // zero and nom doesn't cross zero?
        return Interval::unbounded();
    }
    else
    {
        Interval ret;

        TCU_INTERVAL_APPLY_MONOTONE2(ret, nomp, nom, denp, den, val, TCU_SET_INTERVAL(val, point, point = nomp / denp));
        return ret;
    }
}

static double negate(double x)
{
    return -x;
}

Interval operator-(const Interval &x)
{
    return applyMonotone(negate, x);
}

Interval exp2(const Interval &x)
{
    return applyMonotone(std::pow, 2.0, x);
}

Interval exp(const Interval &x)
{
    return applyMonotone(std::exp, x);
}

Interval sqrt(const Interval &x)
{
    return applyMonotone(std::sqrt, x);
}

Interval inverseSqrt(const Interval &x)
{
    return 1.0 / sqrt(x);
}

Interval abs(const Interval &x)
{
    const Interval mono = applyMonotone(std::abs, x);

    if (x.contains(0.0))
        return Interval(0.0, mono);

    return mono;
}

std::ostream &operator<<(std::ostream &os, const Interval &interval)
{
    if (interval.empty())
        if (interval.hasNaN())
            os << "[NaN]";
        else
            os << "()";
    else
        os << (interval.hasNaN() ? "~" : "") << "[" << interval.lo() << ", " << interval.hi() << "]";
    return os;
}

} // namespace tcu
