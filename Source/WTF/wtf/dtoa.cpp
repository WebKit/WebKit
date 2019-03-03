/*
 *  Copyright (C) 2003-2019 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include <wtf/dtoa.h>

namespace WTF {

const char* numberToString(float number, NumberToStringBuffer& buffer)
{
    double_conversion::StringBuilder builder(&buffer[0], sizeof(buffer));
    double_conversion::DoubleToStringConverter::EcmaScriptConverter().ToShortestSingle(number, &builder);
    return builder.Finalize();
}

const char* numberToString(double d, NumberToStringBuffer& buffer)
{
    double_conversion::StringBuilder builder(&buffer[0], sizeof(buffer));
    auto& converter = double_conversion::DoubleToStringConverter::EcmaScriptConverter();
    converter.ToShortest(d, &builder);
    return builder.Finalize();
}

static inline const char* formatStringTruncatingTrailingZerosIfNeeded(NumberToStringBuffer& buffer, double_conversion::StringBuilder& builder)
{
    size_t length = builder.position();
    size_t decimalPointPosition = 0;
    for (; decimalPointPosition < length; ++decimalPointPosition) {
        if (buffer[decimalPointPosition] == '.')
            break;
    }

    // No decimal separator found, early exit.
    if (decimalPointPosition == length)
        return builder.Finalize();

    size_t pastMantissa = decimalPointPosition + 1;
    for (; pastMantissa < length; ++pastMantissa) {
        if (buffer[pastMantissa] == 'e')
            break;
    }

    size_t truncatedLength = pastMantissa;
    for (; truncatedLength > decimalPointPosition + 1; --truncatedLength) {
        if (buffer[truncatedLength - 1] != '0')
            break;
    }

    // No trailing zeros found to strip.
    if (truncatedLength == pastMantissa)
        return builder.Finalize();

    // If we removed all trailing zeros, remove the decimal point as well.
    if (truncatedLength == decimalPointPosition + 1)
        truncatedLength = decimalPointPosition;

    // Truncate the mantissa, and return the final result.
    builder.RemoveCharacters(truncatedLength, pastMantissa);
    return builder.Finalize();
}

const char* numberToFixedPrecisionString(double d, unsigned significantFigures, NumberToStringBuffer& buffer, bool truncateTrailingZeros)
{
    // Mimic sprintf("%.[precision]g", ...), but use dtoas rounding facilities.
    // "g": Signed value printed in f or e format, whichever is more compact for the given value and precision.
    // The e format is used only when the exponent of the value is less than –4 or greater than or equal to the
    // precision argument. Trailing zeros are truncated, and the decimal point appears only if one or more digits follow it.
    // "precision": The precision specifies the maximum number of significant digits printed.
    double_conversion::StringBuilder builder(&buffer[0], sizeof(buffer));
    auto& converter = double_conversion::DoubleToStringConverter::EcmaScriptConverter();
    converter.ToPrecision(d, significantFigures, &builder);
    if (!truncateTrailingZeros)
        return builder.Finalize();
    return formatStringTruncatingTrailingZerosIfNeeded(buffer, builder);
}

const char* numberToFixedWidthString(double d, unsigned decimalPlaces, NumberToStringBuffer& buffer)
{
    // Mimic sprintf("%.[precision]f", ...), but use dtoas rounding facilities.
    // "f": Signed value having the form [ – ]dddd.dddd, where dddd is one or more decimal digits.
    // The number of digits before the decimal point depends on the magnitude of the number, and
    // the number of digits after the decimal point depends on the requested precision.
    // "precision": The precision value specifies the number of digits after the decimal point.
    // If a decimal point appears, at least one digit appears before it.
    // The value is rounded to the appropriate number of digits.    
    double_conversion::StringBuilder builder(&buffer[0], sizeof(buffer));
    auto& converter = double_conversion::DoubleToStringConverter::EcmaScriptConverter();
    converter.ToFixed(d, decimalPlaces, &builder);
    return builder.Finalize();
}

namespace Internal {

double parseDoubleFromLongString(const UChar* string, size_t length, size_t& parsedLength)
{
    Vector<LChar> conversionBuffer(length);
    for (size_t i = 0; i < length; ++i)
        conversionBuffer[i] = isASCII(string[i]) ? string[i] : 0;
    return parseDouble(conversionBuffer.data(), length, parsedLength);
}

} // namespace Internal

} // namespace WTF
