/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef DecimalNumber_h
#define DecimalNumber_h

#include <math.h>
#include <wtf/dtoa.h>
#include <wtf/text/WTFString.h>

namespace WTF {

enum RoundingSignificantFiguresType { RoundingSignificantFigures };
enum RoundingDecimalPlacesType { RoundingDecimalPlaces };

class DecimalNumber {
public:
    DecimalNumber(double d)
    {
        bool sign = d < 0; // This (correctly) ignores the sign on -0.0.
        init(sign, d);
    }

    // If our version of dtoa could round to a given number of significant figures then
    // we could remove the pre-rounding code from here.  We could also do so just by
    // calling dtoa and post-rounding, however currently this is slower, since it forces
    // dtoa to generate a higher presision result.
    DecimalNumber(double d, RoundingSignificantFiguresType, unsigned significantFigures)
    {
        ASSERT(!isnan(d) && !isinf(d));
        ASSERT(significantFigures && significantFigures <= 21);

        bool sign = d < 0; // This (correctly) ignores the sign on -0.0.
        d = fabs(d); // make d positive before going any further.

        int adjust = 0;
        if (d) {
            // To round a number we align it such that the correct number of digits are
            // to the left of the decimal point, then floor/ceil.  For example, to round
            // 13579 to 3 s.f. we first adjust it to 135.79, use floor/ceil to obtain the
            // values 135/136, and then select 136 (since this is closest to 135.79).
            // There are currently (exp + 1) digits to the left of the decimal point,
            // and we want thsi to be significantFigures, so we're going to adjust the
            // exponent by ((exp + 1) - significantFigures).  Adjust is effectively
            // a count of how many decimal digits to right-shift the number by.
            int exp = static_cast<int>(floor(log10(d)));
            adjust = (exp + 1) - significantFigures;

            // If the adjust value might be positive or negative - or zero.  If zero, then
            // nothing to do! - the number is already appropriately aligned.  If adjust
            // is positive then divide d by 10^adjust.  If adjust is negative multiply d
            // by 10^-adjust. This is mathematically the same, but avoids two fp divides
            // (one inside intPow10, where the power is negative).
            if (adjust > 0)
                d /= intPow10(adjust);
            else if (adjust < 0)
                d *= intPow10(-adjust);

            // Try rounding up & rounding down, select whichever is closest (rounding up if equal distance).
            double floorOfD = floor(d);
            double ceilOfD = floorOfD + 1;
            d = (fabs(ceilOfD - d) <= fabs(floorOfD - d)) ? ceilOfD : floorOfD;

            // The number's exponent has been altered - but don't change it back!  We can
            // just run dtoa on the modified value, and adjust the exponent afterward to
            // account for this.
        }

        init(sign, d);

        // We alterered the value when rounding it - modify the exponent to adjust for this,
        // but don't mess with the exponent of zero.
        if (!isZero())
            m_exponent += adjust;

        // Make sure the significand does not contain more digits than requested.
        roundToPrecision(significantFigures);
    }

    // If our version of dtoa could round to a given number of decimal places then we
    // could remove the pre-rounding code from here.  We could also do so just by calling
    // dtoa and post-rounding, however currently this is slower, since it forces dtoa to
    // generate a higher presision result.
    DecimalNumber(double d, RoundingDecimalPlacesType, unsigned decimalPlaces)
    {
        ASSERT(!isnan(d) && !isinf(d));
        ASSERT(decimalPlaces <= 20);

        bool sign = d < 0; // This (correctly) ignores the sign on -0.0.
        d = fabs(d); // Make d positive before going any further.

        ASSERT(d < 1e+21); // We don't currently support rounding to decimal places for values >= 1e+21.

        // Adjust the number by increasing the exponent by decimalPlaces, such
        // that we can round to this number of decimal places jsing floor.
        if (decimalPlaces)
            d *= intPow10(decimalPlaces);
        // Try rounding up & rounding down, select whichever is closest (rounding up if equal distance).
        double floorOfD = floor(d);
        double ceilOfD = floorOfD + 1;
        d = (fabs(ceilOfD - d) <= fabs(floorOfD - d)) ? ceilOfD : floorOfD;
        // The number's exponent has been altered - but don't change it back!  We can
        // just run dtoa on the modified value, and adjust the exponent afterward to
        // account for this.

        init(sign, d);

        // We rouned the value before calling dtoa, so the result should not be fractional.
        ASSERT(m_exponent >= 0);

        // We alterered the value when rounding it - modify the exponent to adjust for this,
        // but don't mess with the exponent of zero.
        if (!isZero())
            m_exponent -= decimalPlaces;

        // The value was < 1e+21 before we started, should still be.
        ASSERT(m_exponent < 21);

        unsigned significantFigures = 1 + m_exponent + decimalPlaces;
        ASSERT(significantFigures && significantFigures <= 41);
        roundToPrecision(significantFigures);
    }

    unsigned toStringDecimal(NumberToStringBuffer& buffer)
    {
        // Should always be at least one digit to add to the string!
        ASSERT(m_precision);
        UChar* next = buffer;

        // if the exponent is negative the number decimal representation is of the form:
        // [<sign>]0.[<zeros>]<significand>
        if (m_exponent < 0) {
            unsigned zeros = -m_exponent - 1;

            if (m_sign)
                *next++ = '-';
            *next++ = '0';
            *next++ = '.';
            for (unsigned i = 0; i < zeros; ++i)
                *next++ = '0';
            for (unsigned i = 0; i < m_precision; ++i)
                *next++ = m_significand[i];

            return next - buffer;
        }

        unsigned digitsBeforeDecimalPoint = m_exponent + 1;

        // If the precision is <= than the number of digits to get up to the decimal
        // point, then there is no fractional part, number is of the form:
        // [<sign>]<significand>[<zeros>]
        if (m_precision <= digitsBeforeDecimalPoint) {
            if (m_sign)
                *next++ = '-';
            for (unsigned i = 0; i < m_precision; ++i)
                *next++ = m_significand[i];
            for (unsigned i = 0; i < (digitsBeforeDecimalPoint - m_precision); ++i)
                *next++ = '0';

            return next - buffer;
        }

        // If we get here, number starts before the decimal point, and ends after it,
        // as such is of the form:
        // [<sign>]<significand-begin>.<significand-end>

        if (m_sign)
            *next++ = '-';
        for (unsigned i = 0; i < digitsBeforeDecimalPoint; ++i)
            *next++ = m_significand[i];
        *next++ = '.';
        for (unsigned i = digitsBeforeDecimalPoint; i < m_precision; ++i)
            *next++ = m_significand[i];

        return next - buffer;
    }

    unsigned toStringExponential(NumberToStringBuffer &buffer)
    {
        // Should always be at least one digit to add to the string!
        ASSERT(m_precision);

        UChar* next = buffer;

        // Add the sign
        if (m_sign)
            *next++ = '-';

        // Add the significand
        *next++ = m_significand[0];
        if (m_precision > 1) {
            *next++ = '.';
            for (unsigned i = 1; i < m_precision; ++i)
                *next++ = m_significand[i];
        }

        // Add "e+" or "e-"
        *next++ = 'e';
        int exponent;
        if (m_exponent >= 0) {
            *next++ = '+';
            exponent = m_exponent;
        } else {
            *next++ = '-';
            exponent = -m_exponent;
        }

        // Add the exponent
        if (exponent >= 100)
            *next++ = '0' + exponent / 100;
        if (exponent >= 10)
            *next++ = '0' + (exponent % 100) / 10;
        *next++ = '0' + exponent % 10;

        return next - buffer;
    }

    bool sign() { return m_sign; }
    int exponent() { return m_exponent; }
    const char* significand() { return m_significand; } // significand contains precision characters, is not null-terminated.
    unsigned precision() { return m_precision; }

private:
    void init(bool sign, double d)
    {
        ASSERT(!isnan(d) && !isinf(d));

        int decimalPoint;
        int signUnused;
        char* resultEnd = 0;
        WTF::dtoa(m_significand, d, 0, &decimalPoint, &signUnused, &resultEnd);

        m_sign = sign;
        m_precision = resultEnd - m_significand;
        m_exponent = decimalPoint - 1;

        // No values other than zero should have a leading zero.
        ASSERT(m_significand[0] != '0' || m_precision == 1);
        // Zero should always have exponent 0.
        ASSERT(m_significand[0] != '0' || !m_exponent);
    }

    bool isZero()
    {
        return m_significand[0] == '0';
    }

    // We pre-round the values to dtoa - which leads to it generating faster results.
    // But dtoa won't have zero padded the significand to the precision we require,
    // and also might have produced too many digits if rounding went wrong somehow.
    // Adjust for this.
    void roundToPrecision(unsigned significantFigures)
    {
        ASSERT(significantFigures && significantFigures <= sizeof(DtoaBuffer));

        // If there are too few of too many digits in the significand then add more, or remove some!
        for (unsigned i = m_precision; i < significantFigures; ++i)
            m_significand[i] = '0';
        m_precision = significantFigures;
    }

    bool m_sign;
    int m_exponent;
    DtoaBuffer m_significand;
    unsigned m_precision;
};

} // namespace WTF

using WTF::DecimalNumber;
using WTF::RoundingSignificantFigures;
using WTF::RoundingDecimalPlaces;

#endif // DecimalNumber_h
