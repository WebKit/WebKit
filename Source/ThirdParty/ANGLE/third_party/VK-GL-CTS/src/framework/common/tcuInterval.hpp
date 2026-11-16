#ifndef _TCUINTERVAL_HPP
#define _TCUINTERVAL_HPP
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
 * \brief Interval arithmetic and floating point precisions.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"

#include "deMath.h"

#include <iostream>
#include <limits>
#include <cmath>

#define TCU_INFINITY (::std::numeric_limits<float>::infinity())
#define TCU_NAN (::std::numeric_limits<float>::quiet_NaN())

namespace tcu
{

// RAII context for temporarily changing the rounding mode
class ScopedRoundingMode
{
public:
    ScopedRoundingMode(deRoundingMode mode) : m_oldMode(deGetRoundingMode())
    {
        deSetRoundingMode(mode);
    }

    ScopedRoundingMode(void) : m_oldMode(deGetRoundingMode())
    {
    }

    ~ScopedRoundingMode(void)
    {
        deSetRoundingMode(m_oldMode);
    }

private:
    ScopedRoundingMode(const ScopedRoundingMode &);
    ScopedRoundingMode &operator=(const ScopedRoundingMode &);

    const deRoundingMode m_oldMode;
};

class Interval
{
public:
    // Empty interval.
    Interval(void)
        : m_hasNaN(false)
        , m_lo(TCU_INFINITY)
        , m_hi(-TCU_INFINITY)
        , m_warningLo(-TCU_INFINITY)
        , m_warningHi(TCU_INFINITY)
    {
    }

    // Intentionally not explicit. Conversion from double to Interval is common
    // and reasonable.
    Interval(double val)
        : m_hasNaN(!!std::isnan(val))
        , m_lo(m_hasNaN ? TCU_INFINITY : val)
        , m_hi(m_hasNaN ? -TCU_INFINITY : val)
        , m_warningLo(-TCU_INFINITY)
        , m_warningHi(TCU_INFINITY)
    {
    }

    Interval(bool hasNaN_, double lo_, double hi_)
        : m_hasNaN(hasNaN_)
        , m_lo(lo_)
        , m_hi(hi_)
        , m_warningLo(-TCU_INFINITY)
        , m_warningHi(TCU_INFINITY)
    {
    }

    Interval(bool hasNaN_, double lo_, double hi_, double wlo_, double whi_)
        : m_hasNaN(hasNaN_)
        , m_lo(lo_)
        , m_hi(hi_)
        , m_warningLo(wlo_)
        , m_warningHi(whi_)
    {
    }

    Interval(const Interval &a, const Interval &b)
        : m_hasNaN(a.m_hasNaN || b.m_hasNaN)
        , m_lo(de::min(a.lo(), b.lo()))
        , m_hi(de::max(a.hi(), b.hi()))
        , m_warningLo(de::min(a.warningLo(), b.warningLo()))
        , m_warningHi(de::max(a.warningHi(), b.warningHi()))
    {
    }

    double length(void) const
    {
        return m_hi - m_lo;
    }
    double lo(void) const
    {
        return m_lo;
    }
    double hi(void) const
    {
        return m_hi;
    }
    double warningLo(void) const
    {
        return m_warningLo;
    }
    double warningHi(void) const
    {
        return m_warningHi;
    }
    bool hasNaN(void) const
    {
        return m_hasNaN;
    }
    Interval nan(void) const
    {
        return m_hasNaN ? TCU_NAN : Interval();
    }
    bool empty(void) const
    {
        return m_lo > m_hi;
    }

    // The interval is represented in double, it can extend outside the range of smaller floating-point formats
    // and get rounded to infinity.
    bool isFinite(double maxValue) const
    {
        return m_lo > -maxValue && m_hi < maxValue;
    }
    bool isOrdinary(double maxValue) const
    {
        return !hasNaN() && !empty() && isFinite(maxValue);
    }

    void warning(double lo_, double hi_)
    {
        m_warningLo = lo_;
        m_warningHi = hi_;
    }

    Interval operator|(const Interval &other) const
    {
        return Interval(m_hasNaN || other.m_hasNaN, de::min(m_lo, other.m_lo), de::max(m_hi, other.m_hi),
                        de::min(m_warningLo, other.m_warningLo), de::max(m_warningHi, other.m_warningHi));
    }

    Interval &operator|=(const Interval &other)
    {
        return (*this = *this | other);
    }

    Interval operator&(const Interval &other) const
    {
        return Interval(m_hasNaN && other.m_hasNaN, de::max(m_lo, other.m_lo), de::min(m_hi, other.m_hi),
                        de::max(m_warningLo, other.m_warningLo), de::min(m_warningHi, other.m_warningHi));
    }

    Interval &operator&=(const Interval &other)
    {
        return (*this = *this & other);
    }

    bool contains(const Interval &other) const
    {
        return (other.lo() >= lo() && other.hi() <= hi() && (!other.hasNaN() || hasNaN()));
    }

    bool containsWarning(const Interval &other) const
    {
        return (other.lo() >= warningLo() && other.hi() <= warningHi() && (!other.hasNaN() || hasNaN()));
    }

    bool intersects(const Interval &other) const
    {
        return ((other.hi() >= lo() && other.lo() <= hi()) || (other.hasNaN() && hasNaN()));
    }

    Interval operator-(void) const
    {
        return Interval(hasNaN(), -hi(), -lo(), -warningHi(), -warningLo());
    }

    static Interval unbounded(bool nan = false)
    {
        return Interval(nan, -TCU_INFINITY, TCU_INFINITY);
    }

    double midpoint(void) const
    {
        const double h = hi();
        const double l = lo();

        if (h == -l)
            return 0.0;
        if (l == -TCU_INFINITY)
            return -TCU_INFINITY;
        if (h == TCU_INFINITY)
            return TCU_INFINITY;

        const bool negativeH = ::std::signbit(h);
        const bool negativeL = ::std::signbit(l);
        double ret;

        if (negativeH != negativeL)
        {
            // Different signs. Adding both values should be safe.
            ret = (h + l) * 0.5;
        }
        else
        {
            // Same sign. Substracting low from high should be safe.
            ret = l + (h - l) * 0.5;
        }

        return ret;
    }

    bool operator==(const Interval &other) const
    {
        return ((m_hasNaN == other.m_hasNaN) &&
                ((empty() && other.empty()) || (m_lo == other.m_lo && m_hi == other.m_hi)));
    }

private:
    bool m_hasNaN;
    double m_lo;
    double m_hi;
    double m_warningLo;
    double m_warningHi;
} DE_WARN_UNUSED_TYPE;

inline Interval operator+(const Interval &x)
{
    return x;
}
Interval exp2(const Interval &x);
Interval exp(const Interval &x);
int sign(const Interval &x);
Interval abs(const Interval &x);
Interval inverseSqrt(const Interval &x);

Interval operator+(const Interval &x, const Interval &y);
Interval operator-(const Interval &x, const Interval &y);
Interval operator*(const Interval &x, const Interval &y);
Interval operator/(const Interval &nom, const Interval &den);

inline Interval &operator+=(Interval &x, const Interval &y)
{
    return (x = x + y);
}
inline Interval &operator-=(Interval &x, const Interval &y)
{
    return (x = x - y);
}
inline Interval &operator*=(Interval &x, const Interval &y)
{
    return (x = x * y);
}
inline Interval &operator/=(Interval &x, const Interval &y)
{
    return (x = x / y);
}

std::ostream &operator<<(std::ostream &os, const Interval &interval);

#define TCU_SET_INTERVAL_BOUNDS(DST, VAR, SETLOW, SETHIGH)        \
    do                                                            \
    {                                                             \
        DE_FENV_ACCESS_ON                                         \
        ::tcu::ScopedRoundingMode VAR##_ctx_;                     \
        ::tcu::Interval &VAR##_dst_ = (DST);                      \
        ::tcu::Interval VAR##_lo_;                                \
        ::tcu::Interval VAR##_hi_;                                \
                                                                  \
        {                                                         \
            ::tcu::Interval &VAR = VAR##_lo_;                     \
            ::deSetRoundingMode(DE_ROUNDINGMODE_TO_NEGATIVE_INF); \
            SETLOW;                                               \
        }                                                         \
        {                                                         \
            ::tcu::Interval &VAR = VAR##_hi_;                     \
            ::deSetRoundingMode(DE_ROUNDINGMODE_TO_POSITIVE_INF); \
            SETHIGH;                                              \
        }                                                         \
                                                                  \
        VAR##_dst_ = VAR##_lo_ | VAR##_hi_;                       \
    } while (false)

#define TCU_SET_INTERVAL(DST, VAR, BODY) TCU_SET_INTERVAL_BOUNDS(DST, VAR, BODY, BODY)

//! Set the interval DST to the image of BODY on ARG, assuming that BODY on
//! ARG is a monotone function. In practice, BODY is evaluated on both the
//! upper and lower bound of ARG, and DST is set to the union of these
//! results. While evaluating BODY, PARAM is bound to the bound of ARG, and
//! the output of BODY should be stored in VAR.
#define TCU_INTERVAL_APPLY_MONOTONE1(DST, PARAM, ARG, VAR, BODY) \
    do                                                           \
    {                                                            \
        const ::tcu::Interval &VAR##_arg_ = (ARG);               \
        ::tcu::Interval &VAR##_dst_       = (DST);               \
        ::tcu::Interval VAR##_lo_;                               \
        ::tcu::Interval VAR##_hi_;                               \
        if (VAR##_arg_.empty())                                  \
            VAR##_dst_ = Interval();                             \
        else                                                     \
        {                                                        \
            {                                                    \
                const double PARAM   = VAR##_arg_.lo();          \
                ::tcu::Interval &VAR = VAR##_lo_;                \
                BODY;                                            \
            }                                                    \
            {                                                    \
                const double PARAM   = VAR##_arg_.hi();          \
                ::tcu::Interval &VAR = VAR##_hi_;                \
                BODY;                                            \
            }                                                    \
            VAR##_dst_ = VAR##_lo_ | VAR##_hi_;                  \
        }                                                        \
        if (VAR##_arg_.hasNaN())                                 \
            VAR##_dst_ |= TCU_NAN;                               \
    } while (false)

#define TCU_INTERVAL_APPLY_MONOTONE2(DST, P0, A0, P1, A1, VAR, BODY) \
    TCU_INTERVAL_APPLY_MONOTONE1(DST, P0, A0, tmp2_, TCU_INTERVAL_APPLY_MONOTONE1(tmp2_, P1, A1, VAR, BODY))

#define TCU_INTERVAL_APPLY_MONOTONE3(DST, P0, A0, P1, A1, P2, A2, VAR, BODY) \
    TCU_INTERVAL_APPLY_MONOTONE1(DST, P0, A0, tmp3_, TCU_INTERVAL_APPLY_MONOTONE2(tmp3_, P1, A1, P2, A2, VAR, BODY))

typedef double DoubleFunc1(double);
typedef double DoubleFunc2(double, double);
typedef double DoubleFunc3(double, double, double);
typedef Interval DoubleIntervalFunc1(double);
typedef Interval DoubleIntervalFunc2(double, double);
typedef Interval DoubleIntervalFunc3(double, double, double);

Interval applyMonotone(DoubleFunc1 &func, const Interval &arg0);
Interval applyMonotone(DoubleFunc2 &func, const Interval &arg0, const Interval &arg1);
Interval applyMonotone(DoubleIntervalFunc1 &func, const Interval &arg0);
Interval applyMonotone(DoubleIntervalFunc2 &func, const Interval &arg0, const Interval &arg1);

} // namespace tcu

#endif // _TCUINTERVAL_HPP
