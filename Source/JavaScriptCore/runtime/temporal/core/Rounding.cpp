/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Rounding.h"

#include "FractionToDouble.h"
#include "TemporalObject.h"

namespace JSC {
namespace TemporalCore {

// NegateRoundingMode — temporal_rs: RoundingMode::negate (src/options.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-negateroundingmode
RoundingMode negateTemporalRoundingMode(RoundingMode roundingMode)
{
    switch (roundingMode) {
    case RoundingMode::Ceil:
        return RoundingMode::Floor;
    case RoundingMode::Floor:
        return RoundingMode::Ceil;
    case RoundingMode::HalfCeil:
        return RoundingMode::HalfFloor;
    case RoundingMode::HalfFloor:
        return RoundingMode::HalfCeil;
    default:
        return roundingMode;
    }
}

// ApplyUnsignedRoundingMode — temporal_rs: apply_unsigned_rounding_mode
// https://tc39.es/proposal-temporal/#sec-applyunsignedroundingmode
double applyUnsignedRoundingMode(double x, double r1, double r2, UnsignedRoundingMode unsignedRoundingMode)
{
    // 1. If x = r1, return r1.
    if (x == r1)
        return r1;
    // 2. Assert: r1 < x < r2.
    ASSERT(r1 < x && x < r2);
    // 3. Assert: unsignedRoundingMode is not undefined.
    // 4. If unsignedRoundingMode is ~zero~, return r1.
    if (unsignedRoundingMode == UnsignedRoundingMode::Zero)
        return r1;
    // 5. If unsignedRoundingMode is ~infinity~, return r2.
    if (unsignedRoundingMode == UnsignedRoundingMode::Infinity)
        return r2;
    // 6. Let d1 be x - r1.
    double d1 = x - r1;
    // 7. Let d2 be r2 - x.
    double d2 = r2 - x;
    // 8. If d1 < d2, return r1.
    if (d1 < d2)
        return r1;
    // 9. If d2 < d1, return r2.
    if (d2 < d1)
        return r2;
    // 10. Assert: d1 is equal to d2.
    ASSERT(d1 == d2);
    // 11. If unsignedRoundingMode is ~half-zero~, return r1.
    if (unsignedRoundingMode == UnsignedRoundingMode::HalfZero)
        return r1;
    // 12. If unsignedRoundingMode is ~half-infinity~, return r2.
    if (unsignedRoundingMode == UnsignedRoundingMode::HalfInfinity)
        return r2;
    // 13. Assert: unsignedRoundingMode is ~half-even~.
    ASSERT(unsignedRoundingMode == UnsignedRoundingMode::HalfEven);
    // 14. Let cardinality be (r1 / (r2 - r1)) modulo 2.
    auto cardinality = std::fmod(r1 / (r2 - r1), 2);
    // 15. If cardinality = 0, return r1. 16. Return r2.
    return !cardinality ? r1 : r2;
}

// ApplyUnsignedRoundingMode (Int128 path) — integer analogue of the double version above.
// https://tc39.es/proposal-temporal/#sec-applyunsignedroundingmode
// cmp: -1 if x is closer to r1, +1 if closer to r2, 0 if exactly at midpoint.
// Callers handle the exact case (x = r1) before calling this.
static Int128 applyUnsignedRoundingModeInt128(Int128 r1, Int128 r2, int cmp, UnsignedRoundingMode unsignedRoundingMode)
{
    // 4. If unsignedRoundingMode is ~zero~, return r1.
    if (unsignedRoundingMode == UnsignedRoundingMode::Zero)
        return r1;
    // 5. If unsignedRoundingMode is ~infinity~, return r2.
    if (unsignedRoundingMode == UnsignedRoundingMode::Infinity)
        return r2;
    // 8. If d1 < d2, return r1. 9. If d2 < d1, return r2.
    if (cmp < 0)
        return r1;
    if (cmp > 0)
        return r2;
    // 11. If unsignedRoundingMode is ~half-zero~, return r1.
    if (unsignedRoundingMode == UnsignedRoundingMode::HalfZero)
        return r1;
    // 12. If unsignedRoundingMode is ~half-infinity~, return r2.
    if (unsignedRoundingMode == UnsignedRoundingMode::HalfInfinity)
        return r2;
    // 13-16. ~half-even~: return r1 if r1 is even, else r2.
    return !(r1 % 2) ? r1 : r2;
}
// MaximumTemporalDurationRoundingIncrement — temporal_rs: Unit::to_maximum_rounding_increment (src/options.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-maximumtemporaldurationroundingincrement
std::optional<unsigned> maximumRoundingIncrement(TemporalUnit unit)
{
    // 1. Return the value from the "Maximum duration rounding increment" column of #table-temporal-units.
    // Year/Month/Week/Day have no maximum (return ~unset~); time units return fixed values.
    if (unit <= TemporalUnit::Day)
        return std::nullopt;
    if (unit == TemporalUnit::Hour)
        return 24;
    if (unit <= TemporalUnit::Second)
        return 60;
    return 1000;
}

// RoundNumberToIncrement (double path) — temporal_rs: IncrementRounder<f64>::round (src/rounding.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-roundnumbertoincrement
// NOTE: Stays signed throughout rather than abs+negate like temporal_rs/spec.
// truncatedQuotient = -r1 (negative) or r1 (positive); expandedQuotient = -r2 or r2.
// Produces identical results to IncrementRounder<f64>::round.
double roundNumberToIncrementDouble(double x, double increment, RoundingMode mode)
{
    // quotient = x / increment (spec step 1).
    auto quotient = x / increment;
    auto truncatedQuotient = std::trunc(quotient);
    if (truncatedQuotient == quotient)
        return truncatedQuotient * increment;

    auto isNegative = quotient < 0;
    // expandedQuotient = r2 (positive) or r1 (negative) in spec terms.
    auto expandedQuotient = isNegative ? truncatedQuotient - 1 : truncatedQuotient + 1;

    if (mode >= RoundingMode::HalfCeil) {
        auto unsignedFractionalPart = std::abs(quotient - truncatedQuotient);
        if (unsignedFractionalPart < 0.5)
            return truncatedQuotient * increment;
        if (unsignedFractionalPart > 0.5)
            return expandedQuotient * increment;
    }

    switch (mode) {
    case RoundingMode::Ceil:
    case RoundingMode::HalfCeil:
        return (isNegative ? truncatedQuotient : expandedQuotient) * increment;
    case RoundingMode::Floor:
    case RoundingMode::HalfFloor:
        return (isNegative ? expandedQuotient : truncatedQuotient) * increment;
    case RoundingMode::Expand:
    case RoundingMode::HalfExpand:
        return expandedQuotient * increment;
    case RoundingMode::Trunc:
    case RoundingMode::HalfTrunc:
        return truncatedQuotient * increment;
    case RoundingMode::HalfEven:
        return (!std::fmod(truncatedQuotient, 2) ? truncatedQuotient : expandedQuotient) * increment;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

// RoundNumberToIncrementAsIfPositive — temporal_rs: IncrementRounder::round_as_if_positive (src/rounding.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-roundnumbertoincrementasifpositive
Int128 roundNumberToIncrementAsIfPositive(Int128 x, Int128 increment, RoundingMode roundingMode)
{
    // 1. Let quotient be x / increment.
    Int128 quotient = x / increment;
    Int128 remainder = x % increment;
    // Exact case: x is divisible by increment — return x directly before computing r1/r2.
    // (spec step 1 of ApplyUnsignedRoundingMode: "if x = r1, return r1")
    if (!remainder)
        return x;
    // 2. Let unsignedRoundingMode be GetUnsignedRoundingMode(roundingMode, ~positive~).
    auto unsignedRoundingMode = getUnsignedRoundingMode(roundingMode, false);
    // 3. Let r1 be the largest integer such that r1 ≤ quotient.
    // 4. Let r2 be the smallest integer such that r2 > quotient.
    auto r1 = quotient;
    auto r2 = quotient + 1;
    if (x < 0) {
        r1 = quotient - 1;
        r2 = quotient;
    }
    auto doubleRemainder = absInt128(remainder * 2);
    int cmp = (doubleRemainder < increment ? -1 : doubleRemainder == increment ? 0 : 1) * (x < 0 ? -1 : 1);
    // 5. Let rounded be ApplyUnsignedRoundingMode(quotient, r1, r2, unsignedRoundingMode).
    // 6. Return rounded × increment.
    return applyUnsignedRoundingModeInt128(r1, r2, cmp, unsignedRoundingMode) * increment;
}

// RoundNumberToIncrement (Int128 path) — temporal_rs: IncrementRounder<i128>::round (src/rounding.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-roundnumbertoincrement
Int128 roundNumberToIncrementInt128(Int128 x, Int128 increment, RoundingMode mode)
{
    // 1. Let quotient be x / increment.
    Int128 quotient = x / increment;
    Int128 remainder = x % increment;
    if (!remainder)
        return x;
    // 2-3. Determine isNegative from x; work with abs(quotient) as unsigned quotient.
    bool isNegative = x < 0;
    // 4. Let unsignedRoundingMode be GetUnsignedRoundingMode(roundingMode, isNegative).
    auto unsignedRoundingMode = getUnsignedRoundingMode(mode, isNegative);
    // 5. Let r1 be the largest integer such that r1 ≤ abs(quotient).
    Int128 r1 = absInt128(quotient);
    // 6. Let r2 be the smallest integer such that r2 > abs(quotient).
    Int128 r2 = r1 + 1;
    int cmp = absInt128(remainder * 2) < increment ? -1 : absInt128(remainder * 2) > increment ? 1 : 0;
    // 7. Let rounded be ApplyUnsignedRoundingMode(abs(quotient), r1, r2, unsignedRoundingMode).
    Int128 rounded = applyUnsignedRoundingModeInt128(r1, r2, cmp, unsignedRoundingMode);
    // 8. If isNegative is ~negative~, set rounded to -rounded.
    if (isNegative)
        rounded = -rounded;
    // 9. Return rounded × increment.
    return rounded * increment;
}

// ValidateTemporalRoundingIncrement — temporal_rs: RoundingIncrement::validate (src/options/increment.rs)
// https://tc39.es/proposal-temporal/#sec-validatetemporalroundingincrement
// NOTE: dividend is optional here; callers pass nullopt when largestUnit ≤ Day (no maximum applies).
// In that case we use nsPerSecond as a safe upper bound (largest valid nanosecond increment).
TemporalResult<void> validateTemporalRoundingIncrement(double increment, std::optional<double> dividend, Inclusivity isInclusive)
{
    // 1. If inclusive is true, then a. Let maximum be dividend.
    // 2. Else, b. Let maximum be dividend - 1.
    double maximum;
    if (!dividend)
        maximum = static_cast<double>(lengthInNanoseconds(TemporalUnit::Second));
    else if (isInclusive == Inclusivity::Inclusive)
        maximum = dividend.value();
    else if (dividend.value() > 1)
        maximum = dividend.value() - 1;
    else
        maximum = 1;

    increment = std::trunc(increment);
    // 3. If increment > maximum, throw a RangeError exception.
    if (increment < 1 || increment > maximum)
        return makeUnexpected(TemporalError { TemporalErrorKind::RangeError, "rounding increment is out of range"_s });
    // 4. If dividend modulo increment ≠ 0, throw a RangeError exception.
    if (dividend && std::fmod(dividend.value(), increment))
        return makeUnexpected(TemporalError { TemporalErrorKind::RangeError, "roundingIncrement does not divide evenly"_s });
    // 5. Return ~unused~.
    return { };
}

} // namespace TemporalCore
} // namespace JSC
