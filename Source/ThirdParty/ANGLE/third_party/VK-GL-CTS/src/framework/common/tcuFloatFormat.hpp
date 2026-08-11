#ifndef _TCUFLOATFORMAT_HPP
#define _TCUFLOATFORMAT_HPP
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

#include "tcuDefs.hpp"
#include "tcuInterval.hpp"

#include <string>

namespace tcu
{

enum YesNoMaybe
{
    NO,
    MAYBE,
    YES
};

class FloatFormat
{
public:
    FloatFormat(int minExp, int maxExp, int fractionBits, bool exactPrecision, YesNoMaybe hasSubnormal = MAYBE,
                YesNoMaybe hasInf = MAYBE, YesNoMaybe hasNaN = MAYBE);
    virtual ~FloatFormat()
    {
    }

    int getMinExp(void) const
    {
        return m_minExp;
    }
    int getMaxExp(void) const
    {
        return m_maxExp;
    }
    double getMaxValue(void) const
    {
        return m_maxValue;
    }
    int getFractionBits(void) const
    {
        return m_fractionBits;
    }
    YesNoMaybe hasInf(void) const
    {
        return m_hasInf;
    }
    YesNoMaybe hasSubnormal(void) const
    {
        return m_hasSubnormal;
    }

    virtual double ulp(double x, double count = 1.0) const;
    Interval roundOut(const Interval &x, bool roundUnderOverflow) const;
    virtual double round(double d, bool upward) const;
    virtual double roundOut(double d, bool upward, bool roundUnderOverflow) const;
    Interval convert(const Interval &x) const;

    std::string floatToHex(double x) const;
    std::string intervalToHex(const Interval &interval) const;

    static FloatFormat nativeFloat(void);
    static FloatFormat nativeDouble(void);

private:
    int exponentShift(int exp) const;
    Interval clampValue(double d) const;

    int m_minExp;              // Minimum exponent, inclusive
    int m_maxExp;              // Maximum exponent, inclusive
    int m_fractionBits;        // Number of fractional bits in significand
    YesNoMaybe m_hasSubnormal; // Does the format support denormalized numbers?
    YesNoMaybe m_hasInf;       // Does the format support infinities?
    YesNoMaybe m_hasNaN;       // Does the format support NaNs?
    bool m_exactPrecision;     // Are larger precisions disallowed?
    double m_maxValue;         // Largest representable finite value.
} DE_WARN_UNUSED_TYPE;

class NormalizedFormat : public FloatFormat
{
public:
    NormalizedFormat(int fractionBits);
    ~NormalizedFormat()
    {
    }

    double ulp(double x, double count = 1.0) const override;
    double round(double d, bool upward) const override;
    double roundOut(double d, bool upward, bool roundUnderOverflow) const override;

} DE_WARN_UNUSED_TYPE;

void FloatFormat_selfTest(void);

} // namespace tcu

#endif // _TCUFLOATFORMAT_HPP
