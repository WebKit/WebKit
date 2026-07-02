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
 * \brief Adjustable-precision floating point operations.
 *//*--------------------------------------------------------------------*/

#include "tcuFloatFormat.hpp"

#include "deMath.h"
#include "deUniquePtr.hpp"

#include <sstream>
#include <iomanip>
#include <limits>
#include <cmath>

namespace tcu
{
namespace
{

Interval chooseInterval(YesNoMaybe choice, const Interval &no, const Interval &yes)
{
    switch (choice)
    {
    case NO:
        return no;
    case YES:
        return yes;
    case MAYBE:
        return no | yes;
    default:
        DE_FATAL("Impossible case");
    }

    return Interval();
}

double computeMaxValue(int maxExp, int fractionBits)
{
    return (deLdExp(1.0, maxExp) + deLdExp(double((1ull << fractionBits) - 1), maxExp - fractionBits));
}

} // namespace

FloatFormat::FloatFormat(int minExp, int maxExp, int fractionBits, bool exactPrecision, YesNoMaybe hasSubnormal_,
                         YesNoMaybe hasInf_, YesNoMaybe hasNaN_)
    : m_minExp(minExp)
    , m_maxExp(maxExp)
    , m_fractionBits(fractionBits)
    , m_hasSubnormal(hasSubnormal_)
    , m_hasInf(hasInf_)
    , m_hasNaN(hasNaN_)
    , m_exactPrecision(exactPrecision)
    , m_maxValue(computeMaxValue(maxExp, fractionBits))
{
    DE_ASSERT(minExp <= maxExp);
}

/*-------------------------------------------------------------------------
 * On the definition of ULP
 *
 * The GLSL spec does not define ULP. However, it refers to IEEE 754, which
 * (reportedly) uses Harrison's definition:
 *
 * ULP(x) is the distance between the closest floating point numbers
 * a and be such that a <= x <= b and a != b
 *
 * Note that this means that when x = 2^n, ULP(x) = 2^(n-p-1), i.e. it is the
 * distance to the next lowest float, not next highest.
 *
 * Furthermore, it is assumed that ULP is calculated relative to the exact
 * value, not the approximation. This is because otherwise a less accurate
 * approximation could be closer in ULPs, because its ULPs are bigger.
 *
 * For details, see "On the definition of ulp(x)" by Jean-Michel Muller
 *
 *-----------------------------------------------------------------------*/

double FloatFormat::ulp(double x, double count) const
{
    int exp           = 0;
    const double frac = deFractExp(deAbs(x), &exp);

    if (std::isnan(frac))
        return TCU_NAN;
    else if (std::isinf(frac))
        return deLdExp(1.0, m_maxExp - m_fractionBits);
    else if (frac == 1.0)
    {
        // Harrison's ULP: choose distance to closest (i.e. next lower) at binade
        // boundary.
        --exp;
    }
    else if (frac == 0.0)
        exp = m_minExp;

    // ULP cannot be lower than the smallest quantum.
    exp = de::max(exp, m_minExp);

    {
        const double oneULP = deLdExp(1.0, exp - m_fractionBits);
        ScopedRoundingMode ctx(DE_ROUNDINGMODE_TO_POSITIVE_INF);

        return oneULP * count;
    }
}

//! Return the difference between the given nominal exponent and
//! the exponent of the lowest significand bit of the
//! representation of a number with this format.
//! For normal numbers this is the number of significand bits, but
//! for subnormals it is less and for values of exp where 2^exp is too
//! small to represent it is <0
int FloatFormat::exponentShift(int exp) const
{
    return m_fractionBits - de::max(m_minExp - exp, 0);
}

//! Return the number closest to `d` that is exactly representable with the
//! significand bits and minimum exponent of the floatformat. Round up if
//! `upward` is true, otherwise down.
double FloatFormat::round(double d, bool upward) const
{
    int exp                = 0;
    const double frac      = deFractExp(d, &exp);
    const int shift        = exponentShift(exp);
    const double shiftFrac = deLdExp(frac, shift);
    const double roundFrac = upward ? deCeil(shiftFrac) : deFloor(shiftFrac);

    return deLdExp(roundFrac, exp - shift);
}

//! Return the range of numbers that `d` might be converted to in the
//! floatformat, given its limitations with infinities, subnormals and maximum
//! exponent.
Interval FloatFormat::clampValue(double d) const
{
    const double rSign = deSign(d);
    int rExp           = 0;

    DE_ASSERT(!std::isnan(d));

    deFractExp(d, &rExp);
    if (rExp < m_minExp)
        return chooseInterval(m_hasSubnormal, rSign * 0.0, d);
    else if (std::isinf(d) || rExp > m_maxExp)
        return chooseInterval(m_hasInf, rSign * getMaxValue(), rSign * TCU_INFINITY);

    return Interval(d);
}

//! Return the range of numbers that might be used with this format to
//! represent a number within `x`.
Interval FloatFormat::convert(const Interval &x) const
{
    Interval ret;
    Interval tmp = x;

    if (x.hasNaN())
    {
        // If NaN might be supported, NaN is a legal return value
        if (m_hasNaN != NO)
            ret |= TCU_NAN;

        // If NaN might not be supported, any (non-NaN) value is legal,
        // _subject_ to clamping. Hence we modify tmp, not ret.
        if (m_hasNaN != YES)
            tmp = Interval::unbounded();
    }

    // Round both bounds _inwards_ to closest representable values.
    if (!tmp.empty())
        ret |= clampValue(round(tmp.lo(), false)) | clampValue(round(tmp.hi(), true));

    // If this format's precision is not exact, the (possibly out-of-bounds)
    // original value is also a possible result.
    if (!m_exactPrecision)
        ret |= x;

    return ret;
}

double FloatFormat::roundOut(double d, bool upward, bool roundUnderOverflow) const
{
    int exp = 0;
    deFractExp(d, &exp);

    if (roundUnderOverflow && exp > m_maxExp && (upward == (d < 0.0)))
        return deSign(d) * getMaxValue();
    else
        return round(d, upward);
}

//! Round output of an operation.
//! \param roundUnderOverflow Can +/-inf rounded to min/max representable;
//!                              should be false if any of operands was inf, true otherwise.
Interval FloatFormat::roundOut(const Interval &x, bool roundUnderOverflow) const
{
    Interval ret = x.nan();

    if (!x.empty())
        ret |= Interval(roundOut(x.lo(), false, roundUnderOverflow), roundOut(x.hi(), true, roundUnderOverflow));

    return ret;
}

std::string FloatFormat::floatToHex(double x) const
{
    if (std::isnan(x))
        return "NaN";
    else if (std::isinf(x))
        return (x < 0.0 ? "-" : "+") + std::string("inf");
    else if (x == 0.0) // \todo [2014-03-27 lauri] Negative zero
        return "0.0";

    int exp                 = 0;
    const double frac       = deFractExp(deAbs(x), &exp);
    const int shift         = exponentShift(exp);
    const uint64_t bits     = uint64_t(deLdExp(frac, shift));
    const uint64_t whole    = bits >> m_fractionBits;
    const uint64_t fraction = bits & ((uint64_t(1) << m_fractionBits) - 1);
    const int exponent      = exp + m_fractionBits - shift;
    const int numDigits     = (m_fractionBits + 3) / 4;
    const uint64_t aligned  = fraction << (numDigits * 4 - m_fractionBits);
    std::ostringstream oss;

    oss << (x < 0 ? "-" : "") << "0x" << whole << "." << std::hex << std::setw(numDigits) << std::setfill('0')
        << aligned << "p" << std::dec << std::setw(0) << exponent;

    return oss.str();
}

std::string FloatFormat::intervalToHex(const Interval &interval) const
{
    if (interval.empty())
        return interval.hasNaN() ? "{ NaN }" : "{}";

    else if (interval.lo() == interval.hi())
        return (std::string(interval.hasNaN() ? "{ NaN, " : "{ ") + floatToHex(interval.lo()) + " }");
    else if (interval == Interval::unbounded(true))
        return "<any>";

    return (std::string(interval.hasNaN() ? "{ NaN } | " : "") + "[" + floatToHex(interval.lo()) + ", " +
            floatToHex(interval.hi()) + "]");
}

template <typename T>
static FloatFormat nativeFormat(void)
{
    typedef std::numeric_limits<T> Limits;

    DE_ASSERT(Limits::radix == 2);

    return FloatFormat(Limits::min_exponent - 1, // These have a built-in offset of one
                       Limits::max_exponent - 1,
                       Limits::digits - 1, // don't count the hidden bit
                       Limits::has_denorm != std::denorm_absent, Limits::has_infinity ? YES : NO,
                       Limits::has_quiet_NaN ? YES : NO,
                       ((Limits::has_denorm == std::denorm_present) ? YES :
                        (Limits::has_denorm == std::denorm_absent)  ? NO :
                                                                      MAYBE));
}

FloatFormat FloatFormat::nativeFloat(void)
{
    return nativeFormat<float>();
}

FloatFormat FloatFormat::nativeDouble(void)
{
    return nativeFormat<double>();
}

NormalizedFormat::NormalizedFormat(int fractionBits) : FloatFormat(0, 0, fractionBits, true, tcu::YES)
{
}

double NormalizedFormat::round(double d, bool upward) const
{
    const int fractionBits = getFractionBits();

    if (fractionBits <= 0)
        return d;

    const int maxIntValue  = (1 << fractionBits) - 1;
    const double value     = d * maxIntValue;
    const double normValue = upward ? deCeil(value) : deFloor(value);
    return normValue / maxIntValue;
}

double NormalizedFormat::ulp(double x, double count) const
{
    (void)x;

    const int maxIntValue  = (1 << getFractionBits()) - 1;
    const double precision = 1.0 / maxIntValue;
    return precision * count;
}

double NormalizedFormat::roundOut(double d, bool upward, bool roundUnderOverflow) const
{
    if (roundUnderOverflow && deAbs(d) > 1.0 && (upward == (d < 0.0)))
        return deSign(d);
    else
        return round(d, upward);
}

namespace
{

using de::MovePtr;
using de::UniquePtr;
using std::ostringstream;
using std::string;

class Test
{
protected:
    Test(MovePtr<FloatFormat> fmt) : m_fmt(fmt)
    {
    }
    double p(int e) const
    {
        return deLdExp(1.0, e);
    }
    void check(const string &expr, double result, double reference) const;
    void testULP(double arg, double ref) const;
    void testRound(double arg, double refDown, double refUp) const;

    UniquePtr<FloatFormat> m_fmt;
};

void Test::check(const string &expr, double result, double reference) const
{
    if (result != reference)
    {
        ostringstream oss;
        oss << expr << " returned " << result << ", expected " << reference;
        TCU_FAIL(oss.str().c_str());
    }
}

void Test::testULP(double arg, double ref) const
{
    ostringstream oss;

    oss << "ulp(" << arg << ")";
    check(oss.str(), m_fmt->ulp(arg), ref);
}

void Test::testRound(double arg, double refDown, double refUp) const
{
    {
        ostringstream oss;
        oss << "round(" << arg << ", false)";
        check(oss.str(), m_fmt->round(arg, false), refDown);
    }
    {
        ostringstream oss;
        oss << "round(" << arg << ", true)";
        check(oss.str(), m_fmt->round(arg, true), refUp);
    }
}

class TestBinary32 : public Test
{
public:
    TestBinary32(void) : Test(MovePtr<FloatFormat>(new FloatFormat(-126, 127, 23, true)))
    {
    }

    void runTest(void) const;
};

void TestBinary32::runTest(void) const
{
    testULP(p(0), p(-24));
    testULP(p(0) + p(-23), p(-23));
    testULP(p(-124), p(-148));
    testULP(p(-125), p(-149));
    testULP(p(-125) + p(-140), p(-148));
    testULP(p(-126), p(-149));
    testULP(p(-130), p(-149));

    testRound(p(0) + p(-20) + p(-40), p(0) + p(-20), p(0) + p(-20) + p(-23));
    testRound(p(-126) - p(-150), p(-126) - p(-149), p(-126));

    TCU_CHECK(m_fmt->floatToHex(p(0)) == "0x1.000000p0");
    TCU_CHECK(m_fmt->floatToHex(p(8) + p(-4)) == "0x1.001000p8");
    TCU_CHECK(m_fmt->floatToHex(p(-140)) == "0x0.000400p-126");
    TCU_CHECK(m_fmt->floatToHex(p(-140)) == "0x0.000400p-126");
    TCU_CHECK(m_fmt->floatToHex(p(-126) + p(-125)) == "0x1.800000p-125");
}

} // namespace

void FloatFormat_selfTest(void)
{
    TestBinary32 test32;
    test32.runTest();
}

} // namespace tcu
