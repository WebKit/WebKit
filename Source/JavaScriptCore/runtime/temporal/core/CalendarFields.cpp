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
#include "CalendarFields.h"

#include "CalendarArithmetic.h"
#include "CalendarICUBridge.h"
#include "ISO8601.h"
#include "IntlObject.h"
#include "JSCInlines.h"
#include "TemporalCalendar.h"
#include "TemporalObject.h"
#include <cstdint>
#include <wtf/DateMath.h>
#include <wtf/MathExtras.h>

namespace JSC {
namespace TemporalCore {

// temporal_rs: fields.rs ROUGH_YEAR_RANGE = -300000..300000
static constexpr int32_t safeYearMin = -300000;
static constexpr int32_t safeYearMax = 300000;
// ISO reference years for PlainMonthDay — spec: CalendarMonthDayToISOReferenceDate
// 1972 is a leap year (divisible by 4, not a century), 1971 is not.
static constexpr int32_t isoMonthDayReferenceLeapYear = 1972;
static constexpr int32_t isoMonthDayReferenceNonLeapYear = 1971;

// temporal_rs: fields.rs check_year_in_safe_arithmetical_range
static TemporalResult<void> checkYearRange(const CalendarFieldsIn& fields)
{
    if (fields.year && (*fields.year < safeYearMin || *fields.year > safeYearMax))
        return makeUnexpected(rangeError("Date is not within representable range"_s));
    if (fields.eraYear && (*fields.eraYear < safeYearMin || *fields.eraYear > safeYearMax))
        return makeUnexpected(rangeError("eraYear is not within representable range"_s));
    return { };
}

// temporal_rs: types.rs ResolvedIsoFields::try_from_fields (ISO-only resolution)
enum class ISOResolveType : uint8_t {
    Date,
    YearMonth,
    MonthDay
};

struct ResolvedISOFields {
    int32_t year;
    uint8_t month;
    uint8_t day;
};

// resolveISOFields — internal helper; implements the ISO path of three spec AOs, parameterized by ISOResolveType:
//   Date -> ISODateFromFields (#sec-temporal-isodatefromfields)
//   YearMonth -> ISOYearMonthFromFields (#sec-temporal-isoyearmonthfromfields)
//   MonthDay -> ISOMonthDayFromFields (#sec-temporal-isomonthdayfromfields)
// temporal_rs: types.rs ResolvedIsoFields::try_from_fields
static TemporalResult<ResolvedISOFields> resolveISOFields(const CalendarFieldsIn& fields, TemporalOverflow overflow, ISOResolveType type)
{
    auto rangeCheck = checkYearRange(fields);
    if (!rangeCheck)
        return makeUnexpected(rangeCheck.error());

    // Resolve year: MonthDay uses reference year; others require explicit year field.
    int32_t year;
    if (type == ISOResolveType::MonthDay)
        year = isoMonthDayReferenceLeapYear;
    else {
        if (!fields.year)
            return makeUnexpected(typeError("year property must be present"_s));
        year = *fields.year;
    }

    // Resolve day: YearMonth uses day=1; others require explicit day field.
    uint8_t day;
    if (type == ISOResolveType::YearMonth)
        day = 1;
    else {
        if (!fields.day)
            return makeUnexpected(typeError("day property must be present"_s));
        day = *fields.day;
    }

    // Resolve month: prefer month over monthCode; validate consistency if both present.
    uint32_t month;
    if (fields.month && !fields.monthCode) {
        month = *fields.month;
    } else if (fields.monthCode) {
        auto& mc = *fields.monthCode;
        if (mc.isLeapMonth)
            return makeUnexpected(rangeError("iso8601 calendar does not have leap months"_s));
        if (mc.monthNumber < 1 || mc.monthNumber > 12)
            return makeUnexpected(rangeError("month must be 1-12 for iso8601 calendar"_s));
        uint8_t codeMonth = static_cast<uint8_t>(mc.monthNumber);
        if (fields.month && *fields.month != codeMonth)
            return makeUnexpected(rangeError("month does not match monthCode"_s));
        month = codeMonth;
    } else if (!fields.month)
        return makeUnexpected(typeError("month or monthCode property must be present"_s));
    else
        month = *fields.month;

    // Constrain or reject month and day per overflow.
    if (month < 1 || month > 12) {
        if (overflow == TemporalOverflow::Constrain)
            month = clampTo<uint8_t>(month, 1, 12);
        else
            return makeUnexpected(rangeError("month is out of range"_s));
    }

    uint8_t maxDay = ISO8601::daysInMonth(year, month);
    if (day < 1 || day > maxDay) {
        if (overflow == TemporalOverflow::Constrain)
            day = clampTo<uint8_t>(day, 1, maxDay);
        else
            return makeUnexpected(rangeError("day is out of range"_s));
    }

    return ResolvedISOFields { year, static_cast<uint8_t>(month), day };
}

// CalendarDateFromFields — temporal_rs: Calendar::date_from_fields (src/builtins/core/calendar.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-calendardatefromfields
// Implements steps 1-4; PrepareCalendarFields done by JS-layer caller.
TemporalResult<ResolvedCalendarDate> dateFromFields(CalendarID calendarId, const CalendarFieldsIn& fields, TemporalOverflow overflow)
{
    bool isISO = calendarIsISO(calendarId);

    // Steps 1-2: CalendarResolveFields + CalendarDateToISO — fused into resolveISOFields (ISO path).
    if (isISO) {
        auto resolved = resolveISOFields(fields, overflow, ISOResolveType::Date);
        if (!resolved)
            return makeUnexpected(resolved.error());

        auto isoDate = ISO8601::PlainDate(resolved->year, resolved->month, resolved->day);
        // Step 3: If ISODateWithinLimits(result) is false, throw RangeError.
        if (!ISO8601::isDateTimeWithinLimits(isoDate.year(), isoDate.month(), isoDate.day(), 12, 0, 0, 0, 0, 0))
            return makeUnexpected(rangeError("Date is not within representable range"_s));

        // Step 4: Return result.
        return ResolvedCalendarDate { isoDate, iso8601CalendarID() };
    }

    // Steps 1-2 (non-ISO): CalendarResolveFields + CalendarDateToISO via ICU bridge.
    auto rangeCheck = checkYearRange(fields);
    if (!rangeCheck)
        return makeUnexpected(rangeCheck.error());

    // Non-lunisolar calendars have no leap months — reject leap month codes.
    if (fields.monthCode && fields.monthCode->isLeapMonth && !calendarIsLunisolar(calendarId))
        return makeUnexpected(rangeError("Leap month codes are not valid for this calendar"_s));

    bool hasEraYear = fields.era.has_value() && fields.eraYear.has_value();
    if (!fields.year && !hasEraYear)
        return makeUnexpected(typeError("year property must be present"_s));
    if (!fields.day)
        return makeUnexpected(typeError("day property must be present"_s));

    // Resolve month from month/monthCode
    uint8_t month = 1;
    if (fields.month)
        month = *fields.month;
    else if (fields.monthCode)
        month = static_cast<uint8_t>(fields.monthCode->monthNumber);

    int32_t year = fields.year.value_or(0);
    std::optional<StringView> era;
    if (fields.era)
        era = StringView(*fields.era);

    auto result = calendarDateFromFields(calendarId, year, month, *fields.day, era, fields.eraYear, fields.monthCode, overflow);
    if (!result)
        return makeUnexpected(result.error());

    // Step 3: If ISODateWithinLimits(result) is false, throw RangeError.
    if (!ISO8601::isDateTimeWithinLimits(result->year(), result->month(), result->day(), 12, 0, 0, 0, 0, 0))
        return makeUnexpected(rangeError("Date is not within representable range"_s));

    // Step 4: Return result.
    return ResolvedCalendarDate { *result, calendarId };
}

// CalendarYearMonthFromFields — temporal_rs: Calendar::year_month_from_fields (src/builtins/core/calendar.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-calendaryearmonthfromfields
// Implements steps 1-5; PrepareCalendarFields done by JS-layer caller.
TemporalResult<ResolvedCalendarDate> yearMonthFromFields(CalendarID calendarId, const CalendarFieldsIn& fields, TemporalOverflow overflow)
{
    bool isISO = calendarIsISO(calendarId);

    // Steps 1-3: set [[Day]]=1 + CalendarResolveFields + CalendarDateToISO — fused into resolveISOFields.
    if (isISO) {
        auto resolved = resolveISOFields(fields, overflow, ISOResolveType::YearMonth);
        if (!resolved)
            return makeUnexpected(resolved.error());

        auto isoDate = ISO8601::PlainDate(resolved->year, resolved->month, resolved->day);
        // Step 4: If ISOYearMonthWithinLimits(result) is false, throw RangeError.
        if (!ISO8601::isYearMonthWithinLimits(isoDate.year(), isoDate.month()))
            return makeUnexpected(rangeError("YearMonth is not within representable range"_s));

        // Step 5: Return result.
        return ResolvedCalendarDate { isoDate, iso8601CalendarID() };
    }

    // Steps 1-3 (non-ISO): set [[Day]]=1 + CalendarResolveFields + CalendarDateToISO via ICU bridge.
    auto rangeCheck = checkYearRange(fields);
    if (!rangeCheck)
        return makeUnexpected(rangeCheck.error());

    // Non-lunisolar calendars have no leap months — reject leap month codes.
    if (fields.monthCode && fields.monthCode->isLeapMonth && !calendarIsLunisolar(calendarId))
        return makeUnexpected(rangeError("Leap month codes are not valid for this calendar"_s));

    bool hasEraYear = fields.era.has_value() && fields.eraYear.has_value();
    if (!fields.year && !hasEraYear)
        return makeUnexpected(typeError("year property must be present"_s));

    uint8_t month = 1;
    if (fields.month)
        month = *fields.month;
    else if (fields.monthCode)
        month = static_cast<uint8_t>(fields.monthCode->monthNumber);

    int32_t year = fields.year.value_or(0);
    std::optional<StringView> era;
    if (fields.era)
        era = StringView(*fields.era);

    auto result = calendarDateFromFields(calendarId, year, month, 1, era, fields.eraYear, fields.monthCode, overflow);
    if (!result)
        return makeUnexpected(result.error());

    // Step 4: If ISOYearMonthWithinLimits(result) is false, throw RangeError.
    if (!ISO8601::isYearMonthWithinLimits(result->year(), result->month()))
        return makeUnexpected(rangeError("YearMonth is not within representable range"_s));

    // Step 5: Return result.
    return ResolvedCalendarDate { *result, calendarId };
}

// CalendarMonthDayFromFields — temporal_rs: Calendar::month_day_from_fields (src/builtins/core/calendar.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-calendarmonthdayfromfields
// Implements steps 1-4; PrepareCalendarFields done by JS-layer caller.
TemporalResult<ResolvedCalendarDate> monthDayFromFields(CalendarID calendarId, const CalendarFieldsIn& fields, TemporalOverflow overflow)
{
    bool isISO = calendarIsISO(calendarId);

    // Steps 1-2: CalendarResolveFields + CalendarMonthDayToISOReferenceDate — fused inline (ISO path).
    if (isISO) {
        // temporal_rs: if year is provided, validate day against that year first,
        // then use reference year 1972 for the stored date.
        // Per spec: the year is ONLY used for overflow (leap year check), NOT for range validation.
        if (fields.year) {
            // Substitute a proxy year with the same leap-year property to avoid range errors.
            // This matches the spec requirement that year is not range-checked for PlainMonthDay.
            CalendarFieldsIn fieldsForOverflow = fields;
            int32_t yr = *fields.year;
            // Determine leap year status: divisible by 4, except centuries unless also by 400.
            bool isLeap = !(yr % 400) || (!(yr % 4) && yr % 100);
            fieldsForOverflow.year = isLeap ? isoMonthDayReferenceLeapYear : isoMonthDayReferenceNonLeapYear;
            auto resolved = resolveISOFields(fieldsForOverflow, overflow, ISOResolveType::Date);
            if (!resolved)
                return makeUnexpected(resolved.error());
            // Step 3: Assert: ISODateWithinLimits(result) — always holds for reference year 1972.
            ASSERT(ISO8601::isDateTimeWithinLimits(isoMonthDayReferenceLeapYear, resolved->month, resolved->day, 12, 0, 0, 0, 0, 0));
            auto isoDate = ISO8601::PlainDate(isoMonthDayReferenceLeapYear, resolved->month, resolved->day);
            // Step 4: Return result.
            return ResolvedCalendarDate { isoDate, iso8601CalendarID() };
        }
        auto resolved = resolveISOFields(fields, overflow, ISOResolveType::MonthDay);
        if (!resolved)
            return makeUnexpected(resolved.error());

        auto isoDate = ISO8601::PlainDate(isoMonthDayReferenceLeapYear, resolved->month, resolved->day);
        // Step 3: Assert: ISODateWithinLimits(result) — always holds for reference year 1972.
        ASSERT(ISO8601::isDateTimeWithinLimits(isoMonthDayReferenceLeapYear, resolved->month, resolved->day, 12, 0, 0, 0, 0, 0));
        // Step 4: Return result.
        return ResolvedCalendarDate { isoDate, iso8601CalendarID() };
    }

    // Steps 1-2 (non-ISO): CalendarResolveFields + CalendarMonthDayToISOReferenceDate via ICU bridge.
    auto rangeCheck = checkYearRange(fields);
    if (!rangeCheck)
        return makeUnexpected(rangeCheck.error());

    // Non-lunisolar calendars have no leap months — reject leap month codes.
    if (fields.monthCode && fields.monthCode->isLeapMonth && !calendarIsLunisolar(calendarId))
        return makeUnexpected(rangeError("Leap month codes are not valid for this calendar"_s));

    if (!fields.day)
        return makeUnexpected(typeError("day property must be present"_s));

    // Non-ISO MonthDay requires monthCode (not just month).
    if (!fields.monthCode && !fields.year)
        return makeUnexpected(typeError("monthCode is required for non-ISO calendar MonthDay"_s));

    uint8_t month = 1;
    if (fields.month)
        month = *fields.month;
    else if (fields.monthCode)
        month = static_cast<uint8_t>(fields.monthCode->monthNumber);

    // Default year: use ecmaReferenceYear (ported from icu4x) for non-ISO MonthDay.
    int32_t year;
    bool usedRegularMonthFallback = false;
    if (fields.year)
        year = *fields.year;
    else if (fields.monthCode) {
        year = ecmaReferenceYear(calendarId, fields.monthCode->monthNumber, fields.monthCode->isLeapMonth, fields.day ? *fields.day : 1);
        // icu4x: UseRegularIfConstrain — leap month has no reference year near 1972.
        // Constrain: fall back to the non-leap version's reference year AND strip the leap flag.
        // Reject: throw RangeError (this leap month configuration doesn't exist).
        if (year == ecmaRefYearNotInCalendar)
            return makeUnexpected(rangeError("This month code does not exist in this calendar"_s));
        if (year == ecmaRefYearUseRegular) {
            if (overflow == TemporalOverflow::Constrain) {
                year = ecmaReferenceYear(calendarId, fields.monthCode->monthNumber, false, fields.day ? *fields.day : 1);
                usedRegularMonthFallback = true;
            } else
                return makeUnexpected(rangeError("This leap month does not exist in this calendar near the reference year"_s));
        }
    } else
        RELEASE_ASSERT_NOT_REACHED();

    // ecmaReferenceYear returns ISO proleptic years. On older Apple ICU, UCAL_EXTENDED_YEAR
    // uses epoch-based counting for lunisolar calendars; probe the offset per calendar.
    auto calStr = calendarIDToString(calendarId);
    bool isChineseOrDangi = (calStr == "chinese"_s || calStr == "dangi"_s);
    if (!fields.year && isChineseOrDangi)
        year += lunarCalendarExtendedYearFor1972(calendarId) - 1972;
    std::optional<StringView> era;
    if (fields.era)
        era = StringView(*fields.era);

    // When using the regular-month fallback, use a non-leap monthCode so ICU doesn't
    // look for a leap month in the reference year.
    std::optional<ParsedMonthCode> regularMonthCode;
    const std::optional<ParsedMonthCode>* effectiveMonthCode = &fields.monthCode;
    if (usedRegularMonthFallback && fields.monthCode) {
        regularMonthCode = ParsedMonthCode { fields.monthCode->monthNumber, false };
        effectiveMonthCode = &regularMonthCode;
    }

    auto result = calendarDateFromFields(calendarId, year, month, *fields.day, era, fields.eraYear, *effectiveMonthCode, overflow);
    if (!result)
        return makeUnexpected(result.error());

    // For MonthDay with year provided: validate ISO range, then re-resolve with
    // reference year to get canonical reference ISO date per spec.
    if (fields.year || (fields.era && fields.eraYear)) {
        if (!ISO8601::isYearWithinLimits(result->year()))
            return makeUnexpected(rangeError("Date is not within representable range"_s));

        // Re-resolve: get monthCode+day from first resolution, then use ecmaReferenceYear.
        auto resolvedFields = isoToCalendarFields(calendarId, *result);
        if (resolvedFields && !resolvedFields->monthCode.isEmpty()) {
            auto resolvedMonthCode = ISO8601::parseMonthCode(resolvedFields->monthCode);
            if (resolvedMonthCode) {
                int32_t refYear = ecmaReferenceYear(calendarId, resolvedMonthCode->monthNumber, resolvedMonthCode->isLeapMonth, resolvedFields->day);
                if (isChineseOrDangi)
                    refYear += lunarCalendarExtendedYearFor1972(calendarId) - 1972;
                auto refResult = calendarDateFromFields(calendarId, refYear, resolvedFields->month, resolvedFields->day, std::nullopt, std::nullopt, resolvedMonthCode, TemporalOverflow::Constrain);
                if (refResult)
                    return ResolvedCalendarDate { *refResult, calendarId };
            }
        }
    }

    // Step 3: Assert: ISODateWithinLimits(result) — reference year is always within limits.
    ASSERT(ISO8601::isDateTimeWithinLimits(result->year(), result->month(), result->day(), 12, 0, 0, 0, 0, 0));
    // Step 4: Return result.
    return ResolvedCalendarDate { *result, calendarId };
}

// plainYearMonthWith — temporal_rs: PlainYearMonth::with (src/builtins/core/plain_year_month.rs)
// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.with
// Implements inner merge steps; IsPartialTemporalObject and PrepareCalendarFields done by JS-layer caller.
TemporalResult<ResolvedCalendarDate> plainYearMonthWith(CalendarID calendarId, const ISO8601::PlainDate& currentISODate, const CalendarFieldsIn& partialFields, TemporalOverflow overflow)
{
    // Defensive: JS-layer caller guarantees at least one field via IsPartialTemporalObject;
    // guard against direct internal calls with an empty partialFields.
    if (!partialFields.year && !partialFields.month && !partialFields.monthCode
        && !partialFields.era && !partialFields.eraYear)
        return makeUnexpected(typeError("at least one field (year, month, monthCode, era, or eraYear) must be provided"_s));

    bool isISO = calendarIsISO(calendarId);

    if (isISO) {
        // ISO: same merge logic as temporal_rs with_fallback_year_month.
        CalendarFieldsIn merged;
        merged.year = partialFields.year.has_value() ? partialFields.year : std::optional<int32_t>(currentISODate.year());
        // temporal_rs: impl_with_fallback_method! — month: user's value only, no fallback
        merged.month = partialFields.month;
        // monthCode: user's overrides; fallback from current only if neither month nor monthCode provided
        merged.monthCode = partialFields.monthCode;
        if (!partialFields.month.has_value() && !partialFields.monthCode)
            merged.monthCode = ISO8601::parseMonthCode(ISO8601::monthCode(currentISODate.month()));
        return yearMonthFromFields(calendarId, merged, overflow);
    }

    // Non-ISO: get calendar fields from current date, merge with user partial fields.
    // temporal_rs: impl_with_fallback_method! macro and impl_field_keys_to_ignore! macro (fields.rs).
    auto calFields = isoToCalendarFields(calendarId, currentISODate);
    if (!calFields)
        return makeUnexpected(calFields.error());

    // temporal_rs: impl_field_keys_to_ignore! — determine which fallback fields to suppress
    bool keysToIgnoreMonth = partialFields.month.has_value() || partialFields.monthCode.has_value();
    bool hasEras = calendarHasEras(calendarId);
    bool keysToIgnoreEra = false;
    bool keysToIgnoreYear = false;
    if (hasEras) {
        if (partialFields.year.has_value() || partialFields.eraYear.has_value() || partialFields.era.has_value()) {
            keysToIgnoreEra = true;
            keysToIgnoreYear = true;
        }
    }

    CalendarFieldsIn merged;

    // temporal_rs: impl_with_fallback_method! — era/eraYear: fallback from current if !keysToIgnoreEra
    merged.era = partialFields.era;
    merged.eraYear = partialFields.eraYear;
    if (!keysToIgnoreEra) {
        if (!merged.era && calFields->era.has_value() && !calFields->era->isEmpty())
            merged.era = *calFields->era;
        if (!merged.eraYear && calFields->eraYear.has_value())
            merged.eraYear = *calFields->eraYear;
    }

    // temporal_rs: impl_with_fallback_method! — year: fallback from current if !keysToIgnoreYear
    merged.year = partialFields.year;
    if (!keysToIgnoreYear && !merged.year)
        merged.year = std::optional<int32_t>(calFields->year);

    // temporal_rs: impl_with_fallback_method! — month: user's value only, NEVER fallback
    merged.month = partialFields.month;

    // temporal_rs: impl_with_fallback_method! — monthCode: fallback ONLY when neither month nor monthCode provided
    merged.monthCode = partialFields.monthCode;
    if (!partialFields.month.has_value() && !partialFields.monthCode.has_value() && !keysToIgnoreMonth) {
        if (!calFields->monthCode.isEmpty())
            merged.monthCode = ISO8601::parseMonthCode(calFields->monthCode);
    }

    return yearMonthFromFields(calendarId, merged, overflow);
}

// differenceYearMonth — temporal_rs: PlainYearMonth::diff (src/builtins/core/plain_year_month.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-differencetemporalplainyearmonth
// Implements steps 7-14; steps 1-6 and rounding (steps 15-16) done by JS-layer caller.
TemporalResult<ISO8601::Duration> differenceYearMonth(CalendarID calendarId, const ISO8601::PlainDate& thisISODate, const ISO8601::PlainDate& otherISODate, TemporalUnit largestUnit)
{
    bool isISO = calendarIsISO(calendarId);

    if (isISO) {
        // Steps 7-8: set [[Day]]=1 on both dates; ISODateWithinLimits check.
        auto thisDate = ISO8601::PlainDate(thisISODate.year(), thisISODate.month(), 1);
        auto otherDate = ISO8601::PlainDate(otherISODate.year(), otherISODate.month(), 1);
        if (std::abs(dateToDaysFrom1970(thisDate.year(), static_cast<int>(thisDate.month()) - 1, 1)) > 1e8
            || std::abs(dateToDaysFrom1970(otherDate.year(), static_cast<int>(otherDate.month()) - 1, 1)) > 1e8)
            return makeUnexpected(rangeError("date is outside the representable range for Temporal"_s));
        // Steps 10-14: CalendarDateUntil(thisDate, otherDate, largestUnit).
        return calendarDateUntil(thisDate, otherDate, largestUnit);
    }

    // Non-ISO: resolve both to day=1 via dateFromFields (matching temporal_rs).
    // This re-resolves the ISO date through the calendar pipeline with day=1.
    // Step 7: ISODateToFields(calendar, thisISODate, year-month); set [[Day]] to 1; CalendarDateFromFields.
    auto thisCalFields = isoToCalendarFields(calendarId, thisISODate);
    if (!thisCalFields)
        return makeUnexpected(thisCalFields.error());
    CalendarFieldsIn thisFields;
    thisFields.year = thisCalFields->year;
    thisFields.month = thisCalFields->month;
    thisFields.day = 1;
    if (!thisCalFields->monthCode.isEmpty())
        thisFields.monthCode = ISO8601::parseMonthCode(thisCalFields->monthCode);
    auto thisResolved = dateFromFields(calendarId, thisFields, TemporalOverflow::Constrain);
    if (!thisResolved)
        return makeUnexpected(thisResolved.error());

    // Steps 8-9: ISODateToFields(calendar, otherISODate, year-month); set [[Day]] to 1; CalendarDateFromFields.
    auto otherCalFields = isoToCalendarFields(calendarId, otherISODate);
    if (!otherCalFields)
        return makeUnexpected(otherCalFields.error());
    CalendarFieldsIn otherFields;
    otherFields.year = otherCalFields->year;
    otherFields.month = otherCalFields->month;
    otherFields.day = 1;
    if (!otherCalFields->monthCode.isEmpty())
        otherFields.monthCode = ISO8601::parseMonthCode(otherCalFields->monthCode);
    auto otherResolved = dateFromFields(calendarId, otherFields, TemporalOverflow::Constrain);
    if (!otherResolved)
        return makeUnexpected(otherResolved.error());

    // Steps 10-14: CalendarDateUntil(thisDate, otherDate, largestUnit).
    return TemporalCore::calendarDateUntil(calendarId, thisResolved->isoDate, otherResolved->isoDate, largestUnit);
}

// plainYearMonthAdd — temporal_rs: PlainYearMonth::add_duration (src/builtins/core/plain_year_month.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-adddurationtoyearmonth
// Implements steps 9-15; steps 1-8 done by JS-layer caller.
TemporalResult<ResolvedCalendarDate> plainYearMonthAdd(CalendarID calendarId, const ISO8601::PlainDate& currentISODate, const ISO8601::Duration& duration, TemporalOverflow overflow)
{
    bool isISO = calendarIsISO(calendarId);

    if (isISO) {
        // Step 1: Get calendar fields from currentISODate (year + monthCode), set day=1.
        CalendarFieldsIn fields;
        fields.year = currentISODate.year();
        fields.month = currentISODate.month();
        fields.monthCode = ISO8601::parseMonthCode(ISO8601::monthCode(currentISODate.month()));
        fields.day = 1;
        // Step 2: CalendarDateFromFields -> PlainDate.
        auto dateResult = dateFromFields(calendarId, fields, TemporalOverflow::Constrain);
        if (!dateResult)
            return makeUnexpected(dateResult.error());
        // Step 3: CalendarDateAdd(duration) -> addedDate.
        // ISO: bypass ICU bridge and use pure isoDateAdd — handles extreme boundary dates
        // correctly (year ±271821) without ICU calendar clamping.
        auto addedISO = calendarDateAdd(dateResult->isoDate, duration, overflow);
        if (!addedISO)
            return makeUnexpected(addedISO.error());
        // Steps 4-5: Get year + monthCode from addedDate; CalendarYearMonthFromFields -> result PYM ISO date.
        CalendarFieldsIn addedFields;
        addedFields.year = addedISO->year();
        addedFields.monthCode = ISO8601::parseMonthCode(ISO8601::monthCode(addedISO->month()));
        return yearMonthFromFields(calendarId, addedFields, overflow);
    }

    // Non-ISO: temporal_rs add_duration steps 9-15.
    // Step 1: ISODateToFields(calendar, yearMonth.[[ISODate]], year-month) -> year + monthCode, day=1.
    auto calFields = isoToCalendarFields(calendarId, currentISODate);
    if (!calFields)
        return makeUnexpected(calFields.error());

    CalendarFieldsIn fieldsDay1;
    fieldsDay1.year = calFields->year;
    fieldsDay1.day = 1;
    if (!calFields->monthCode.isEmpty())
        fieldsDay1.monthCode = ISO8601::parseMonthCode(calFields->monthCode);
    else
        fieldsDay1.month = calFields->month;
    if (calFields->era.has_value() && calFields->eraYear.has_value()) {
        fieldsDay1.era = *calFields->era;
        fieldsDay1.eraYear = *calFields->eraYear;
    }

    // Step 2: CalendarDateFromFields -> PlainDate ISO.
    auto dateResult = dateFromFields(calendarId, fieldsDay1, TemporalOverflow::Constrain);
    if (!dateResult)
        return makeUnexpected(dateResult.error());

    // Step 3: CalendarDateAdd(duration) -> addedDate ISO.
    auto addedISO = calendarDateAdd(calendarId, dateResult->isoDate, duration, overflow);
    if (!addedISO)
        return makeUnexpected(addedISO.error());

    // Step 4: Get year + monthCode from addedDate.
    auto addedCalFields = isoToCalendarFields(calendarId, *addedISO);
    if (!addedCalFields)
        return makeUnexpected(addedCalFields.error());

    CalendarFieldsIn addedFields;
    addedFields.year = addedCalFields->year;
    if (!addedCalFields->monthCode.isEmpty())
        addedFields.monthCode = ISO8601::parseMonthCode(addedCalFields->monthCode);
    else
        addedFields.month = addedCalFields->month;
    if (addedCalFields->era.has_value() && addedCalFields->eraYear.has_value()) {
        addedFields.era = *addedCalFields->era;
        addedFields.eraYear = *addedCalFields->eraYear;
    }

    // Step 5: CalendarYearMonthFromFields -> result PYM ISO date.
    return yearMonthFromFields(calendarId, addedFields, overflow);
}

// plainYearMonthToPlainDate — temporal_rs: PlainYearMonth::to_plain_date (src/builtins/core/plain_year_month.rs)
// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.toplaindate
// Implements steps 5, 7, 8; steps 1-4, 6, 9 done by JS-layer caller.
TemporalResult<ResolvedCalendarDate> plainYearMonthToPlainDate(CalendarID calendarId, const ISO8601::PlainDate& pymISODate, uint8_t day)
{
    bool isISO = calendarIsISO(calendarId);

    if (isISO) {
        // Steps 5+7 (ISO): year/month from pymISODate; merge day.
        CalendarFieldsIn fields;
        fields.year = pymISODate.year();
        fields.month = pymISODate.month();
        fields.day = day;
        // Step 8: CalendarDateFromFields(~constrain~).
        return dateFromFields(calendarId, fields, TemporalOverflow::Constrain);
    }

    // Steps 5+7 (non-ISO): ISODateToFields via ICU bridge; merge day.
    auto yearResult = calendarYear(calendarId, pymISODate);
    if (!yearResult)
        return makeUnexpected(yearResult.error());
    auto monthCodeStr = calendarMonthCode(calendarId, pymISODate);
    if (!monthCodeStr)
        return makeUnexpected(monthCodeStr.error());

    CalendarFieldsIn fields;
    fields.year = *yearResult;
    fields.day = day;
    fields.monthCode = ISO8601::parseMonthCode(*monthCodeStr);
    // Step 8: CalendarDateFromFields(~constrain~).
    return dateFromFields(calendarId, fields, TemporalOverflow::Constrain);
}

// plainYearMonthFromISODate — no 1:1 temporal_rs function; inlined in PlainYearMonth::from_parsed
// https://tc39.es/proposal-temporal/#sec-temporal-totemporalyearmonth (string parse path)
// Implements steps 10, 12; step 9 (ISOYearMonthWithinLimits) done by JS-layer caller.
TemporalResult<ResolvedCalendarDate> plainYearMonthFromISODate(CalendarID calendarId, const ISO8601::PlainDate& fullISODate)
{
    bool isISO = calendarIsISO(calendarId);

    if (isISO) {
        // ISO path: year/month from fullISODate; yearMonthFromFields stores with day=1.
        CalendarFieldsIn fields;
        fields.year = fullISODate.year();
        fields.month = fullISODate.month();
        return yearMonthFromFields(calendarId, fields, TemporalOverflow::Constrain);
    }

    // Step 10 (non-ISO): ISODateToFields via ICU bridge — gets year + monthCode.
    auto yearResult = calendarYear(calendarId, fullISODate);
    if (!yearResult)
        return makeUnexpected(yearResult.error());
    auto monthCodeStr = calendarMonthCode(calendarId, fullISODate);
    if (!monthCodeStr)
        return makeUnexpected(monthCodeStr.error());

    CalendarFieldsIn fields;
    fields.year = *yearResult;
    fields.day = 1;
    fields.monthCode = ISO8601::parseMonthCode(*monthCodeStr);
    // Step 12: CalendarYearMonthFromFields(~constrain~) per spec note.
    return yearMonthFromFields(calendarId, fields, TemporalOverflow::Constrain);
}

// plainMonthDayToPlainDate — temporal_rs: PlainMonthDay::to_plain_date (src/builtins/core/plain_month_day.rs)
// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.prototype.toplaindate
// Implements steps 5, 7, 8; steps 1-4, 6, 9 done by JS-layer caller.
TemporalResult<ResolvedCalendarDate> plainMonthDayToPlainDate(CalendarID calendarId, const ISO8601::PlainDate& pmdISODate, int32_t year)
{
    bool isISO = calendarIsISO(calendarId);

    if (isISO) {
        // Steps 5+7: ISO ISODateToFields gives month directly; merge year.
        CalendarFieldsIn fields;
        fields.year = year;
        fields.month = pmdISODate.month();
        fields.day = pmdISODate.day();
        // Step 8: CalendarDateFromFields(~constrain~).
        return dateFromFields(calendarId, fields, TemporalOverflow::Constrain);
    }

    // Steps 5+7 (non-ISO): ISODateToFields via ICU bridge, then merge year.
    auto monthCodeStr = calendarMonthCode(calendarId, pmdISODate);
    if (!monthCodeStr)
        return makeUnexpected(monthCodeStr.error());
    auto dayResult = calendarDay(calendarId, pmdISODate);
    if (!dayResult)
        return makeUnexpected(dayResult.error());

    CalendarFieldsIn fields;
    fields.year = year;
    fields.monthCode = ISO8601::parseMonthCode(*monthCodeStr);
    fields.day = static_cast<uint8_t>(*dayResult);
    // Step 8: CalendarDateFromFields(~constrain~).
    return dateFromFields(calendarId, fields, TemporalOverflow::Constrain);
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporalmonthday (string parse path)
// Implements steps 10, 12; step 9 (ISODateWithinLimits) done by JS-layer caller.
TemporalResult<ResolvedCalendarDate> plainMonthDayFromISODate(CalendarID calendarId, const ISO8601::PlainDate& fullISODate, TemporalOverflow overflow)
{
    bool isISO = calendarIsISO(calendarId);

    if (isISO) {
        // ISO path (spec step 7): store directly with reference year 1972.
        CalendarFieldsIn fields;
        fields.month = fullISODate.month();
        fields.day = fullISODate.day();
        return monthDayFromFields(calendarId, fields, overflow);
    }

    // Step 10: ISODateToFields(calendar, fullISODate, ~month-day~) — gets monthCode + day.
    auto monthCodeStr = calendarMonthCode(calendarId, fullISODate);
    if (!monthCodeStr)
        return makeUnexpected(monthCodeStr.error());
    auto dayResult = calendarDay(calendarId, fullISODate);
    if (!dayResult)
        return makeUnexpected(dayResult.error());

    CalendarFieldsIn fields;
    fields.monthCode = ISO8601::parseMonthCode(*monthCodeStr);
    fields.day = static_cast<uint8_t>(*dayResult);
    // Step 12: caller must pass TemporalOverflow::Constrain per spec note.
    return monthDayFromFields(calendarId, fields, overflow);
}

} // namespace TemporalCore
} // namespace JSC
