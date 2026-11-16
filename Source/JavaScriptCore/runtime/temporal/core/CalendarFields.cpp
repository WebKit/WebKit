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
#include "ISOArithmetic.h"
#include "IntlObject.h"
#include "JSCInlines.h"
#include "TemporalCalendar.h"
#include "TemporalObject.h"
#include <cstdint>
#include <wtf/DateMath.h>
#include <wtf/MathExtras.h>
#include <wtf/OptionSet.h>

namespace JSC {
namespace TemporalCore {

// temporal_rs: fields.rs ROUGH_YEAR_RANGE = -300000..300000
static constexpr int32_t safeYearMin = -300000;
static constexpr int32_t safeYearMax = 300000;
// ISO reference year for PlainMonthDay — spec: CalendarMonthDayToISOReferenceDate. A leap year (Feb 29 representable).
static constexpr int32_t isoMonthDayReferenceLeapYear = 1972;

// temporal_rs: fields.rs check_year_in_safe_arithmetical_range
static TemporalResult<void> checkYearRange(const CalendarFieldsIn& fields)
{
    if (fields.year && (*fields.year < safeYearMin || *fields.year > safeYearMax))
        return makeUnexpected(rangeError("Date is not within representable range"_s));
    if (fields.eraYear && (*fields.eraYear < safeYearMin || *fields.eraYear > safeYearMax))
        return makeUnexpected(rangeError("eraYear is not within representable range"_s));
    return { };
}

static TemporalResult<void> checkLunisolarMonthConsistency(CalendarID calendarId, const CalendarFieldsIn& fields, const ISO8601::PlainDate& resolvedDate)
{
    if (!calendarIsLunisolar(calendarId) || !fields.month || !fields.monthCode)
        return { };

    auto resolvedFields = isoToCalendarFields(calendarId, resolvedDate);
    if (!resolvedFields)
        return makeUnexpected(resolvedFields.error());
    if (*fields.month != resolvedFields->month)
        return makeUnexpected(rangeError("month does not match monthCode"_s));
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

// https://tc39.es/proposal-temporal/#sec-temporal-calendarresolvefields
static TemporalResult<void> calendarResolveFields(CalendarID calendarId, CalendarFieldsIn& fields, ResolveType type)
{
    if (calendarIsISO(calendarId)) {
        // Steps 1.a-1.d: needsYear/needsDay.
        bool needsYear = type == ResolveType::Date || type == ResolveType::YearMonth;
        bool needsDay = type == ResolveType::Date || type == ResolveType::MonthDay;
        // Step 1.e: needsYear + year unset.
        if (needsYear && !fields.year)
            return makeUnexpected(typeError("year property must be present"_s));
        // Step 1.f: needsDay + day unset.
        if (needsDay && !fields.day)
            return makeUnexpected(typeError("day property must be present"_s));
        // Step 1.g: month and monthCode both unset.
        if (!fields.month && !fields.monthCode)
            return makeUnexpected(typeError("month or monthCode property must be present"_s));
        // Step 1.h: monthCode validation + consistency with month.
        if (fields.monthCode) {
            auto& mc = *fields.monthCode;
            if (mc.isLeapMonth)
                return makeUnexpected(rangeError("iso8601 calendar does not have leap months"_s));
            if (mc.monthNumber < 1 || mc.monthNumber > 12)
                return makeUnexpected(rangeError("month must be 1-12 for iso8601 calendar"_s));
            uint8_t codeMonth = static_cast<uint8_t>(mc.monthNumber);
            if (fields.month && *fields.month != codeMonth)
                return makeUnexpected(rangeError("month does not match monthCode"_s));
            fields.month = codeMonth;
        }
        return { };
    }
    // Step 2: NonISOResolveFields.
    return nonISOResolveFields(calendarId, fields, type);
}

// https://tc39.es/proposal-intl-era-monthcode/#sup-temporal-nonisoresolvefields
TemporalResult<void> nonISOResolveFields(CalendarID calendarId, CalendarFieldsIn& fields, ResolveType type)
{
    ASSERT(!calendarIsISO(calendarId));

    // Steps 1-4: needsYear.
    bool needsYear = type == ResolveType::Date
        || type == ResolveType::YearMonth
        || !fields.monthCode
        || fields.month.has_value();
    bool needsOrdinalMonth = fields.year.has_value() || fields.eraYear.has_value();
    // Steps 8-9: needsDay.
    bool needsDay = type == ResolveType::Date || type == ResolveType::MonthDay;

    bool hasEras = calendarHasEras(calendarId);
    bool hasEraYearPair = fields.era.has_value() && fields.eraYear.has_value();

    // Step 10: needsYear + year unset → require era+eraYear on an era-supporting calendar.
    if (needsYear && !fields.year) {
        if (!hasEras || !fields.era || !fields.eraYear) [[unlikely]]
            return makeUnexpected(typeError("year property must be present"_s));
    }
    if (hasEras && fields.era.has_value() != fields.eraYear.has_value()) [[unlikely]]
        return makeUnexpected(typeError("era and eraYear must both be present or both absent"_s));
    // Step 12: needsDay + day unset.
    if (needsDay && !fields.day) [[unlikely]]
        return makeUnexpected(typeError("day property must be present"_s));
    // Step 13: month and monthCode both unset.
    if (!fields.month && !fields.monthCode) [[unlikely]]
        return makeUnexpected(typeError("month or monthCode property must be present"_s));

    // MonthDay-only checks not in the spec emu-alg: NonISOMonthDayToISOReferenceDate needs a
    // year to resolve leap-month layout, so demand monthCode + (year OR era+eraYear).
    if (type == ResolveType::MonthDay && !fields.monthCode && !fields.year && !hasEraYearPair) [[unlikely]]
        return makeUnexpected(typeError("monthCode is required for non-ISO calendar MonthDay"_s));
    if (type == ResolveType::MonthDay && fields.month && fields.monthCode && !fields.year && !hasEraYearPair) [[unlikely]]
        return makeUnexpected(typeError("year is required when both month and monthCode are given"_s));

    auto rangeCheck = checkYearRange(fields);
    if (!rangeCheck) [[unlikely]]
        return makeUnexpected(rangeCheck.error());

    // Step 14: CalendarDateArithmeticYearForEraYear = canonicalize + non-positive remap + math;
    //          then reconcile with fields.year and store the arithmetic year.
    if (hasEras && fields.era && fields.eraYear) {
        if (auto canonical = canonicalizeEraInCalendar(calendarId, StringView(*fields.era)))
            fields.era = String(*canonical);
        if (auto remapped = remapNonPositiveEraYear(calendarId, StringView(*fields.era), *fields.eraYear)) {
            fields.era = String(remapped->first);
            fields.eraYear = remapped->second;
        }
        auto arithmeticYear = calendarDateArithmeticYearForEraYear(calendarId, StringView(*fields.era), *fields.eraYear);
        if (!arithmeticYear) [[unlikely]]
            return makeUnexpected(rangeError("era is not valid for this calendar"_s));
        if (fields.year && *fields.year != *arithmeticYear) [[unlikely]]
            return makeUnexpected(rangeError("year is inconsistent with era and eraYear"_s));
        fields.year = *arithmeticYear;
    }
    // Steps 15-17: erase era + eraYear.
    fields.era = std::nullopt;
    fields.eraYear = std::nullopt;

    // Step 18: monthCode validation + resolve fields.[[Month]] from monthCode ordinal.
    if (fields.monthCode) {
        auto& mc = *fields.monthCode;
        // Step 18.a: IsValidMonthCodeForCalendar.
        if (!isValidMonthCodeForCalendar(calendarId, mc)) [[unlikely]]
            return makeUnexpected(rangeError("monthCode is not valid for this calendar"_s));

        // Step 18.b — if year is set, derive MonthCodeToOrdinal and set fields.[[Month]].
        if (fields.year) {
            std::optional<uint8_t> ordinal;
            if (!calendarIsLunisolar(calendarId))
                ordinal = static_cast<uint8_t>(mc.monthNumber);
            else {
                auto contained = yearContainsMonthCode(calendarId, *fields.year, mc);
                if (!contained) [[unlikely]]
                    return makeUnexpected(contained.error());
                auto constrained = mc;
                if (!*contained) {
                    auto constrainResult = constrainMonthCode(calendarId, *fields.year, mc, TemporalOverflow::Constrain);
                    if (!constrainResult) [[unlikely]]
                        return makeUnexpected(constrainResult.error());
                    constrained = *constrainResult;
                }
                // Step 18.b.ii: MonthCodeToOrdinal (precondition guaranteed by constrain above).
                auto ordinalResult = monthCodeOrdinalInYear(calendarId, constrained, *fields.year);
                if (!ordinalResult) [[unlikely]]
                    return makeUnexpected(ordinalResult.error());
                ordinal = static_cast<uint8_t>(*ordinalResult);
            }
            if (ordinal) {
                // Step 18.b.iii: consistency.
                if (fields.month && clampTo<uint8_t>(*fields.month) != *ordinal) [[unlikely]]
                    return makeUnexpected(rangeError("month does not match monthCode"_s));
                // Step 18.b.iv: Set fields.[[Month]] to month.
                fields.month = *ordinal;
            }
        }
    }

    // Steps 19-22: postconditions.
    ASSERT(!fields.era && !fields.eraYear);
    ASSERT(!needsYear || fields.year.has_value());
    // Assertion 21 (needsOrdinalMonth → month set) may be relaxed when the lunisolar ICU
    // ordinal probe fails; downstream nonISOCalendarDateToISO re-derives via the monthCode.
    ASSERT_UNUSED(needsOrdinalMonth, !needsOrdinalMonth || fields.month.has_value() || fields.monthCode.has_value());
    ASSERT(!needsDay || fields.day.has_value());

    return { };
}

// https://tc39.es/proposal-temporal/#sec-temporal-calendardatetoiso
static TemporalResult<ResolvedISOFields> calendarDateToISO(CalendarID calendarId, const CalendarFieldsIn& fields, TemporalOverflow overflow, ISOResolveType type)
{
    // Step 1: If calendar is "iso8601", then
    if (calendarIsISO(calendarId)) {
        // Step 1.a: Assert: fields.[[Year]], fields.[[Month]], and fields.[[Day]] are not unset.
        ASSERT(fields.year.has_value());
        ASSERT(fields.month.has_value());
        ASSERT(type == ISOResolveType::YearMonth || fields.day.has_value());
        int32_t year = *fields.year;
        uint8_t day = type == ISOResolveType::YearMonth ? 1 : *fields.day;
        uint32_t month = *fields.month;

        // Step 1.b: Return ? RegulateISODate(fields.[[Year]], fields.[[Month]], fields.[[Day]], overflow).
        auto regulated = regulateISODate(year, clampTo<int32_t>(month), day, overflow);
        if (!regulated)
            return makeUnexpected(regulated.error());
        return ResolvedISOFields { regulated->year(), regulated->month(), regulated->day() };
    }

    // Step 2: Return ? NonISOCalendarDateToISO(calendar, fields, overflow).
    // CalendarResolveFields guarantees one of the two; 0 means "resolve by the monthCode".
    ASSERT(fields.month || fields.monthCode);
    uint8_t month = fields.month ? clampTo<uint8_t>(*fields.month) : 0;
    uint8_t day = type == ISOResolveType::YearMonth ? 1 : *fields.day;
    auto result = nonISOCalendarDateToISO(calendarId, fields.year, month, day, fields.monthCode, overflow);
    if (!result)
        return makeUnexpected(result.error());
    return ResolvedISOFields { result->year(), result->month(), result->day() };
}

// CalendarDateFromFields — temporal_rs: Calendar::date_from_fields (src/builtins/core/calendar.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-calendardatefromfields
// Implements steps 1-4; PrepareCalendarFields done by JS-layer caller.
TemporalResult<ResolvedCalendarDate> dateFromFields(CalendarID calendarId, const CalendarFieldsIn& fields, TemporalOverflow overflow)
{
    bool isISO = calendarIsISO(calendarId);

    CalendarFieldsIn resolved = fields;
    if (isISO) {
        auto rangeCheck = checkYearRange(resolved);
        if (!rangeCheck)
            return makeUnexpected(rangeCheck.error());
    }
    // Step 1: CalendarResolveFields.
    if (auto validated = calendarResolveFields(calendarId, resolved, ResolveType::Date); !validated)
        return makeUnexpected(validated.error());

    // Step 2: CalendarDateToISO.
    auto resolvedIso = calendarDateToISO(calendarId, resolved, overflow, ISOResolveType::Date);
    if (!resolvedIso)
        return makeUnexpected(resolvedIso.error());
    auto isoDate = ISO8601::PlainDate(resolvedIso->year, resolvedIso->month, resolvedIso->day);
    if (auto consistencyCheck = checkLunisolarMonthConsistency(calendarId, fields, isoDate); !consistencyCheck)
        return makeUnexpected(consistencyCheck.error());

    // Step 3: If ISODateWithinLimits(result) is false, throw RangeError.
    if (!ISO8601::isDateTimeWithinLimits(resolvedIso->year, resolvedIso->month, resolvedIso->day, 12, 0, 0, 0, 0, 0))
        return makeUnexpected(rangeError("Date is not within representable range"_s));

    // Step 4: Return result.
    return ResolvedCalendarDate { isoDate, isISO ? iso8601CalendarID() : calendarId };
}

// CalendarYearMonthFromFields — temporal_rs: Calendar::year_month_from_fields (src/builtins/core/calendar.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-calendaryearmonthfromfields
TemporalResult<ResolvedCalendarDate> yearMonthFromFields(CalendarID calendarId, const CalendarFieldsIn& fields, TemporalOverflow overflow)
{
    bool isISO = calendarIsISO(calendarId);

    // Step 1: Set fields.[[Day]] to 1.
    CalendarFieldsIn resolved = fields;
    resolved.day = 1;

    if (isISO) {
        auto rangeCheck = checkYearRange(resolved);
        if (!rangeCheck)
            return makeUnexpected(rangeCheck.error());
    }

    // Step 2: CalendarResolveFields. (non-ISO: nonISOResolveFields does its own checkYearRange
    // interleaved with its presence/consistency checks — do not range-check again here.)
    if (auto validated = calendarResolveFields(calendarId, resolved, ResolveType::YearMonth); !validated)
        return makeUnexpected(validated.error());

    // Step 3: CalendarDateToISO.
    auto resolvedIso = calendarDateToISO(calendarId, resolved, overflow, ISOResolveType::YearMonth);
    if (!resolvedIso)
        return makeUnexpected(resolvedIso.error());
    auto isoDate = ISO8601::PlainDate(resolvedIso->year, resolvedIso->month, resolvedIso->day);
    if (auto consistencyCheck = checkLunisolarMonthConsistency(calendarId, fields, isoDate); !consistencyCheck)
        return makeUnexpected(consistencyCheck.error());

    // Step 4: If ISOYearMonthWithinLimits(result) is false, throw RangeError.
    if (!ISO8601::isYearMonthWithinLimits(resolvedIso->year, resolvedIso->month))
        return makeUnexpected(rangeError("YearMonth is not within representable range"_s));

    // Step 5: Return result.
    return ResolvedCalendarDate { isoDate, isISO ? iso8601CalendarID() : calendarId };
}

struct ReferenceYearResolution {
    int32_t year { 0 };
    bool usedRegularMonthFallback { false };
};
static TemporalResult<ReferenceYearResolution> ecmaReferenceYearWithRegularFallback(CalendarID calendarId, ParsedMonthCode monthCode, uint8_t day, TemporalOverflow overflow)
{
    auto refYear = ecmaReferenceYear(calendarId, monthCode.monthNumber, monthCode.isLeapMonth, day);
    if (refYear)
        return ReferenceYearResolution { *refYear, false };
    switch (refYear.error()) {
    case EcmaReferenceYearError::MonthNotInCalendar:
        return makeUnexpected(rangeError("This month code does not exist in this calendar"_s));
    case EcmaReferenceYearError::UseRegularIfConstrain: {
        if (overflow == TemporalOverflow::Reject)
            return makeUnexpected(rangeError("This leap month does not exist in this calendar near the reference year"_s));
        auto fallback = ecmaReferenceYear(calendarId, monthCode.monthNumber, /* isLeapMonth */ false, day);
        RELEASE_ASSERT(fallback);
        return ReferenceYearResolution { *fallback, true };
    }
    }
    RELEASE_ASSERT_NOT_REACHED();
}

// https://tc39.es/proposal-intl-era-monthcode/#sup-temporal-nonisomonthdaytoisoreferencedate
// Steps 4-5 (day clamp) are folded into the nonISOCalendarDateToISO calls below, not written out
// separately. `fields` (the unresolved record) is only used for checkLunisolarMonthConsistency.
static TemporalResult<ISO8601::PlainDate> nonISOMonthDayToISOReferenceDate(CalendarID calendarId, const CalendarFieldsIn& resolved, const CalendarFieldsIn& fields, TemporalOverflow overflow)
{
    // Step 1: Assert: fields.[[Day]] is not ~unset~.
    ASSERT(resolved.day);

    ParsedMonthCode monthCode;
    uint8_t month = 0; // 0 = "resolve by monthCode"; overwritten below when a year is given.
    uint8_t day = clampTo<uint8_t>(*resolved.day);
    // Step 8 needs an overflow the spec doesn't give it; see below for when it's forced to Constrain.
    TemporalOverflow step8Overflow = overflow;

    if (resolved.year) {
        // Step 2.a: month was set upstream by nonISOResolveFields when a year is present.
        ASSERT(resolved.month);
        // Steps 2.d-2.i, all at once: resolve month/monthCode/day in the caller's year.
        auto inCallerYear = nonISOCalendarDateToISO(calendarId, resolved.year, clampTo<uint8_t>(*resolved.month), day, resolved.monthCode, overflow);
        if (!inCallerYear)
            return makeUnexpected(inCallerYear.error());
        if (auto consistencyCheck = checkLunisolarMonthConsistency(calendarId, fields, *inCallerYear); !consistencyCheck)
            return makeUnexpected(consistencyCheck.error());
        // Step 2.b: no input in this year lands within the ISO limits.
        if (!ISO8601::isYearWithinLimits(inCallerYear->year()))
            return makeUnexpected(rangeError("Date is not within representable range"_s));
        // Step 2.g: read the monthCode back off that date.
        auto callerYearFields = isoToCalendarFields(calendarId, *inCallerYear);
        if (!callerYearFields)
            return makeUnexpected(callerYearFields.error());
        if (callerYearFields->monthCode.isEmpty())
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        auto parsed = ISO8601::parseMonthCode(callerYearFields->monthCode);
        if (!parsed)
            return makeUnexpected(rangeError(icuReadCalendarFailed));
        monthCode = *parsed;
        month = callerYearFields->month;
        day = callerYearFields->day;
        // Steps 4-5 already applied above; the step 8 stand-in must not reject again.
        step8Overflow = TemporalOverflow::Constrain;
    } else {
        // Steps 3.a-3.b: no ordinal yet (a monthCode's number isn't its ordinal position — M05L is
        // number 5 but ordinal 6 in a leap year), so leave month at 0 for step 8 to resolve by monthCode.
        ASSERT(resolved.monthCode);
        monthCode = *resolved.monthCode;
    }

    // Steps 6-7: reference year, plus 6.c's reject-or-strip. The chinese/dangi-only guard lives inside
    // ecmaReferenceYear (only it returns UseRegularIfConstrain), so this applies safely to every calendar.
    auto reference = ecmaReferenceYearWithRegularFallback(calendarId, monthCode, day, overflow);
    if (!reference)
        return makeUnexpected(reference.error());
    if (reference->usedRegularMonthFallback)
        monthCode = ParsedMonthCode { monthCode.monthNumber, /* isLeapMonth */ false };

    // Step 8, delegated: nonISOCalendarDateToISO's own resolution IS this AO's steps 2.d-2.i and 4-5
    // (the spec duplicates them). referenceYear was chosen so this call never needs to clamp.
    auto result = nonISOCalendarDateToISO(calendarId, std::optional<int32_t> { reference->year }, month, day, std::optional<ParsedMonthCode> { monthCode }, step8Overflow);
    if (!result)
        return makeUnexpected(result.error());
    return *result;
}

// https://tc39.es/proposal-temporal/#sec-temporal-calendarmonthdaytoisoreferencedate
// calendarDateToISO is not reused: it stores whatever year it is given, this stores the reference year.
static TemporalResult<ISO8601::PlainDate> monthDayToISOReferenceDate(CalendarID calendarId, const CalendarFieldsIn& resolved, const CalendarFieldsIn& fields, TemporalOverflow overflow)
{
    // Step 2 first; step 1 is the iso8601 arm below.
    if (!calendarIsISO(calendarId))
        return nonISOMonthDayToISOReferenceDate(calendarId, resolved, fields, overflow);

    // Step 1.a.
    ASSERT(resolved.month && resolved.day);
    // Steps 1.b-1.c: year only decides whether Feb 29 exists; RegulateISODate won't range-check it.
    int32_t year = resolved.year.value_or(isoMonthDayReferenceLeapYear);
    // Step 1.d.
    auto regulated = regulateISODate(year, clampTo<int32_t>(*resolved.month), *resolved.day, overflow);
    if (!regulated)
        return makeUnexpected(regulated.error());
    // Step 1.e: the stored year is always the reference year, never the caller's.
    return ISO8601::PlainDate(isoMonthDayReferenceLeapYear, regulated->month(), regulated->day());
}

// CalendarMonthDayFromFields — temporal_rs: Calendar::month_day_from_fields (src/builtins/core/calendar.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-calendarmonthdayfromfields
TemporalResult<ResolvedCalendarDate> monthDayFromFields(CalendarID calendarId, const CalendarFieldsIn& fields, TemporalOverflow overflow)
{
    CalendarFieldsIn resolved = fields;
    // Step 1: CalendarResolveFields.
    if (auto validated = calendarResolveFields(calendarId, resolved, ResolveType::MonthDay); !validated)
        return makeUnexpected(validated.error());

    // Step 2: CalendarMonthDayToISOReferenceDate.
    auto isoDate = monthDayToISOReferenceDate(calendarId, resolved, fields, overflow);
    if (!isoDate)
        return makeUnexpected(isoDate.error());

    // Step 3: Assert: ISODateWithinLimits(result).
    ASSERT(ISO8601::isDateTimeWithinLimits(isoDate->year(), isoDate->month(), isoDate->day(), 12, 0, 0, 0, 0, 0));
    // Step 4: Return result.
    return ResolvedCalendarDate { *isoDate, calendarIsISO(calendarId) ? iso8601CalendarID() : calendarId };
}

// https://tc39.es/proposal-temporal/#sec-temporal-isodatetofields
TemporalResult<CalendarFieldsIn> isoDateToFields(CalendarID calendarId, const ISO8601::PlainDate& isoDate, ResolveType type)
{
    // Step 1: Let fields be an empty Calendar Fields Record with all fields set to ~unset~.
    CalendarFieldsIn fields;

    // Step 2: Let calendarDate be CalendarISOToDate(calendar, isoDate).
    if (calendarIsISO(calendarId)) {
        // Identity for "iso8601", so no ICU round-trip; the month code is the ISO month, never leap.
        fields.monthCode = ParsedMonthCode { isoDate.month(), /* isLeapMonth */ false }; // Step 3
        if (type == ResolveType::MonthDay || type == ResolveType::Date) // Step 4
            fields.day = isoDate.day();
        if (type == ResolveType::YearMonth || type == ResolveType::Date) // Step 5
            fields.year = isoDate.year();
        return fields; // Step 6
    }

    auto calendarDate = isoToCalendarFields(calendarId, isoDate);
    if (!calendarDate)
        return makeUnexpected(calendarDate.error());

    // Step 3: Set fields.[[MonthCode]] to calendarDate.[[MonthCode]].
    if (!calendarDate->monthCode.isEmpty())
        fields.monthCode = ISO8601::parseMonthCode(calendarDate->monthCode);
    // Step 4: If type is either ~month-day~ or ~date~, set fields.[[Day]] to calendarDate.[[Day]].
    if (type == ResolveType::MonthDay || type == ResolveType::Date)
        fields.day = calendarDate->day;
    // Step 5: If type is either ~year-month~ or ~date~, set fields.[[Year]] to calendarDate.[[Year]].
    if (type == ResolveType::YearMonth || type == ResolveType::Date)
        fields.year = calendarDate->year;
    // Step 6: Return fields.
    return fields;
}

// The Calendar Fields Record table's Enumeration Key column, restricted to the six calendar-date
// keys CalendarFieldsIn carries; a bitset rather than the spec's List to stay allocation-free. No
// ignore rule names the time keys or ~offset~/~time-zone~, so for those the merge degenerates to
// "the partial wins, else the receiver", which the two .with callers do inline.
enum class CalendarFieldKey : uint8_t {
    Era = 1 << 0,
    EraYear = 1 << 1,
    Year = 1 << 2,
    Month = 1 << 3,
    MonthCode = 1 << 4,
    Day = 1 << 5,
};
using CalendarFieldKeys = OptionSet<CalendarFieldKey>;

// https://tc39.es/proposal-temporal/#sec-temporal-calendarfieldkeyspresent
static CalendarFieldKeys calendarFieldKeysPresent(const CalendarFieldsIn& fields)
{
    CalendarFieldKeys keys;
    if (fields.era)
        keys.add(CalendarFieldKey::Era);
    if (fields.eraYear)
        keys.add(CalendarFieldKey::EraYear);
    if (fields.year)
        keys.add(CalendarFieldKey::Year);
    if (fields.month)
        keys.add(CalendarFieldKey::Month);
    if (fields.monthCode)
        keys.add(CalendarFieldKey::MonthCode);
    if (fields.day)
        keys.add(CalendarFieldKey::Day);
    return keys;
}

// https://tc39.es/proposal-intl-era-monthcode/#sup-temporal-nonisofieldkeystoignore
static CalendarFieldKeys nonISOFieldKeysToIgnore(CalendarID calendarId, CalendarFieldKeys keys)
{
    ASSERT(!calendarIsISO(calendarId));
    // Step 1: Let ignoredKeys be a copy of keys — "a field always invalidates at least itself".
    CalendarFieldKeys ignoredKeys = keys;
    // Steps 2.a-2.b: month and monthCode are two encodings of the same field.
    if (keys.contains(CalendarFieldKey::Month))
        ignoredKeys.add(CalendarFieldKey::MonthCode);
    if (keys.contains(CalendarFieldKey::MonthCode))
        ignoredKeys.add(CalendarFieldKey::Month);
    // Step 2.c: era+eraYear and year are two encodings of the same year, so changing either
    //   encoding invalidates the other.
    if (calendarHasEras(calendarId) && keys.containsAny({ CalendarFieldKey::Era, CalendarFieldKey::EraYear, CalendarFieldKey::Year }))
        ignoredKeys.add({ CalendarFieldKey::Era, CalendarFieldKey::EraYear, CalendarFieldKey::Year });
    // Step 2.d: with mid-year eras a day/month move can cross an era boundary without changing the
    //   arithmetic year, so drop the era pair but keep year.
    if (calendarHasMidYearEras(calendarId) && keys.containsAny({ CalendarFieldKey::Day, CalendarFieldKey::Month, CalendarFieldKey::MonthCode }))
        ignoredKeys.add({ CalendarFieldKey::Era, CalendarFieldKey::EraYear });
    // Step 4: Return ignoredKeys.
    return ignoredKeys;
}

// https://tc39.es/proposal-temporal/#sec-temporal-calendarfieldkeystoignore
static CalendarFieldKeys calendarFieldKeysToIgnore(CalendarID calendarId, CalendarFieldKeys keys)
{
    // Step 1: If calendar is "iso8601" — only the month/monthCode pairing applies.
    if (calendarIsISO(calendarId)) {
        CalendarFieldKeys ignoredKeys = keys; // Steps 1.a-1.b.i
        if (keys.contains(CalendarFieldKey::Month)) // Step 1.b.ii
            ignoredKeys.add(CalendarFieldKey::MonthCode);
        if (keys.contains(CalendarFieldKey::MonthCode)) // Step 1.b.iii
            ignoredKeys.add(CalendarFieldKey::Month);
        return ignoredKeys; // Step 1.d
    }
    // Step 2: Return NonISOFieldKeysToIgnore(calendar, keys).
    return nonISOFieldKeysToIgnore(calendarId, keys);
}

// https://tc39.es/proposal-temporal/#sec-temporal-calendarmergefields
CalendarFieldsIn calendarMergeFields(CalendarID calendarId, const CalendarFieldsIn& fields, const CalendarFieldsIn& additionalFields)
{
    // Step 1: Let additionalKeys be CalendarFieldKeysPresent(additionalFields).
    auto additionalKeys = calendarFieldKeysPresent(additionalFields);
    // Step 2: Let overriddenKeys be CalendarFieldKeysToIgnore(calendar, additionalKeys).
    auto overriddenKeys = calendarFieldKeysToIgnore(calendarId, additionalKeys);
    // Step 3: Let merged be a Calendar Fields Record with all fields set to ~unset~.
    CalendarFieldsIn merged;
    // Step 4: fieldsKeys is CalendarFieldKeysPresent(fields) — each engaged optional below is its
    //   own membership test, so there is nothing to precompute.
    // Step 5: For each row of the Calendar Fields Record table.
    auto mergeKey = [&](CalendarFieldKey key, auto& mergedField, const auto& field, const auto& additionalField) {
        // Step 5.b: fieldsKeys contains key and overriddenKeys does not contain key.
        if (field && !overriddenKeys.contains(key))
            mergedField = field;
        // Step 5.c: additionalKeys contains key.
        if (additionalField)
            mergedField = additionalField;
    };
    mergeKey(CalendarFieldKey::Era, merged.era, fields.era, additionalFields.era);
    mergeKey(CalendarFieldKey::EraYear, merged.eraYear, fields.eraYear, additionalFields.eraYear);
    mergeKey(CalendarFieldKey::Year, merged.year, fields.year, additionalFields.year);
    mergeKey(CalendarFieldKey::Month, merged.month, fields.month, additionalFields.month);
    mergeKey(CalendarFieldKey::MonthCode, merged.monthCode, fields.monthCode, additionalFields.monthCode);
    mergeKey(CalendarFieldKey::Day, merged.day, fields.day, additionalFields.day);
    // Step 6: Return merged.
    return merged;
}

// plainYearMonthWith — temporal_rs: PlainYearMonth::with (src/builtins/core/plain_year_month.rs)
// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.with
// Implements steps 5, 7 and 10; IsPartialTemporalObject and PrepareCalendarFields done by JS-layer caller.
TemporalResult<ResolvedCalendarDate> plainYearMonthWith(CalendarID calendarId, const ISO8601::PlainDate& currentISODate, const CalendarFieldsIn& partialFields, TemporalOverflow overflow)
{
    auto fields = isoDateToFields(calendarId, currentISODate, ResolveType::YearMonth);
    if (!fields)
        return makeUnexpected(fields.error());
    return yearMonthFromFields(calendarId, calendarMergeFields(calendarId, *fields, partialFields), overflow);
}

// plainDateWith — temporal_rs: PlainDate::with (src/builtins/core/plain_date.rs)
//   merge: CalendarFields::with_fallback_date (src/builtins/core/calendar/fields.rs)
//   resolve: Calendar::date_from_fields → dateFromFields
// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.with
// Implements steps 5 (ISODateToFields), 7 (CalendarMergeFields), 10 (CalendarDateFromFields).
// Steps 6, 8-9 (PrepareCalendarFields, overflow) done by PlainDate::with(); step 11 (CreateTemporalDate) by prototype caller.
TemporalResult<ResolvedCalendarDate> plainDateWith(CalendarID calendarId, const ISO8601::PlainDate& currentISODate, const CalendarFieldsIn& partialFields, TemporalOverflow overflow)
{
    auto fields = isoDateToFields(calendarId, currentISODate, ResolveType::Date);
    if (!fields)
        return makeUnexpected(fields.error());
    return dateFromFields(calendarId, calendarMergeFields(calendarId, *fields, partialFields), overflow);
}

// plainMonthDayWith — temporal_rs: PlainMonthDay::with (src/builtins/core/plain_month_day.rs)
// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.prototype.with
// Implements steps 5, 7 and 10 (CalendarMonthDayFromFields).
TemporalResult<ResolvedCalendarDate> plainMonthDayWith(CalendarID calendarId, const ISO8601::PlainDate& currentISODate, const CalendarFieldsIn& partialFields, TemporalOverflow overflow)
{
    auto fields = isoDateToFields(calendarId, currentISODate, ResolveType::MonthDay);
    if (!fields)
        return makeUnexpected(fields.error());
    return monthDayFromFields(calendarId, calendarMergeFields(calendarId, *fields, partialFields), overflow);
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
        return calendarDateUntil(calendarId, thisDate, otherDate, largestUnit);
    }

    // Non-ISO: resolve both to day=1 via dateFromFields (matching temporal_rs).
    // Steps 7-9 for each side: ISODateToFields(calendar, isoDate, ~year-month~); set [[Day]] to 1;
    //   ? CalendarDateFromFields(calendar, fields, ~constrain~).
    auto thisFields = isoDateToFields(calendarId, thisISODate, ResolveType::YearMonth);
    if (!thisFields)
        return makeUnexpected(thisFields.error());
    thisFields->day = 1;
    auto thisResolved = dateFromFields(calendarId, *thisFields, TemporalOverflow::Constrain);
    if (!thisResolved)
        return makeUnexpected(thisResolved.error());

    auto otherFields = isoDateToFields(calendarId, otherISODate, ResolveType::YearMonth);
    if (!otherFields)
        return makeUnexpected(otherFields.error());
    otherFields->day = 1;
    auto otherResolved = dateFromFields(calendarId, *otherFields, TemporalOverflow::Constrain);
    if (!otherResolved)
        return makeUnexpected(otherResolved.error());

    // Steps 10-14: CalendarDateUntil(thisDate, otherDate, largestUnit).
    return TemporalCore::calendarDateUntil(calendarId, thisResolved->isoDate, otherResolved->isoDate, largestUnit);
}

// plainYearMonthAdd — temporal_rs: PlainYearMonth::add_duration (src/builtins/core/plain_year_month.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-adddurationtoyearmonth
// Implements steps 9-14 (ISODateToFields → CalendarDateFromFields → CalendarDateAdd →
// ISODateToFields → CalendarYearMonthFromFields). Steps 1-8 (duration prep + calendar lookup)
// and step 15 (CreateTemporalYearMonth wrap) are done by the JS-layer caller.
TemporalResult<ResolvedCalendarDate> plainYearMonthAdd(CalendarID calendarId, const ISO8601::PlainDate& currentISODate, const ISO8601::Duration& duration, TemporalOverflow overflow)
{
    // Step 9: Let fields be ISODateToFields(calendar, yearMonth.[[ISODate]], ~year-month~).
    auto fields = isoDateToFields(calendarId, currentISODate, ResolveType::YearMonth);
    if (!fields)
        return makeUnexpected(fields.error());
    // Step 10: Set fields.[[Day]] to 1.
    fields->day = 1;

    // Step 11: Let date be ? CalendarDateFromFields(calendar, fields, ~constrain~).
    auto dateResult = dateFromFields(calendarId, *fields, TemporalOverflow::Constrain);
    if (!dateResult)
        return makeUnexpected(dateResult.error());

    // Step 12: Let addedDate be ? CalendarDateAdd(calendar, date, durationToAdd, overflow).
    //   ISO takes the pure isoDateAdd overload, which handles the extreme boundary years
    //   (±271821) that ICU calendar arithmetic clamps.
    auto addedISO = calendarIsISO(calendarId)
        ? calendarDateAdd(dateResult->isoDate, duration, overflow)
        : calendarDateAdd(calendarId, dateResult->isoDate, duration, overflow);
    if (!addedISO)
        return makeUnexpected(addedISO.error());

    // Step 13: Let addedDateFields be ISODateToFields(calendar, addedDate, ~year-month~).
    auto addedFields = isoDateToFields(calendarId, *addedISO, ResolveType::YearMonth);
    if (!addedFields)
        return makeUnexpected(addedFields.error());

    // Step 14: Return ? CalendarYearMonthFromFields(calendar, addedDateFields, overflow).
    return yearMonthFromFields(calendarId, *addedFields, overflow);
}

// plainYearMonthToPlainDate — temporal_rs: PlainYearMonth::to_plain_date (src/builtins/core/plain_year_month.rs)
// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.toplaindate
// Implements steps 5, 7, 8; steps 1-4, 6, 9 done by JS-layer caller.
TemporalResult<ResolvedCalendarDate> plainYearMonthToPlainDate(CalendarID calendarId, const ISO8601::PlainDate& pymISODate, uint8_t day)
{
    // Step 5: Let fields be ISODateToFields(calendar, plainYearMonth.[[ISODate]], ~year-month~).
    auto fields = isoDateToFields(calendarId, pymISODate, ResolveType::YearMonth);
    if (!fields)
        return makeUnexpected(fields.error());

    // Step 6's PrepareCalendarFields(«~day~») is the caller's; it arrives as `day`.
    CalendarFieldsIn inputFields;
    inputFields.day = day;

    // Step 7: Let mergedFields be CalendarMergeFields(calendar, fields, inputFields).
    // Step 8: Return ? CalendarDateFromFields(calendar, mergedFields, ~constrain~).
    return dateFromFields(calendarId, calendarMergeFields(calendarId, *fields, inputFields), TemporalOverflow::Constrain);
}

// plainYearMonthFromISODate — no 1:1 temporal_rs function; inlined in PlainYearMonth::from_parsed
// https://tc39.es/proposal-temporal/#sec-temporal-totemporalyearmonth (string parse path)
// Implements steps 12 and 14; step 11's ISOYearMonthWithinLimits check runs inside
// yearMonthFromFields (see the guard in that function).
TemporalResult<ResolvedCalendarDate> plainYearMonthFromISODate(CalendarID calendarId, const ISO8601::PlainDate& fullISODate)
{
    // Step 12: Set result to ISODateToFields(calendar, isoDate, ~year-month~).
    auto fields = isoDateToFields(calendarId, fullISODate, ResolveType::YearMonth);
    if (!fields)
        return makeUnexpected(fields.error());
    // Step 14: Return ? CalendarYearMonthFromFields(calendar, result, ~constrain~) — ~constrain~
    //   regardless of overflow, per Step 13's NOTE. yearMonthFromFields stores day=1 itself.
    return yearMonthFromFields(calendarId, *fields, TemporalOverflow::Constrain);
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporalmonthday (string parse path)
// Implements steps 13 and 15; step 12's ISODateWithinLimits check runs inside monthDayFromFields.
// Step 10's "iso8601" shortcut is not special-cased: routing it through CalendarMonthDayFromFields
// reaches the same reference year 1972 without a second code path.
TemporalResult<ResolvedCalendarDate> plainMonthDayFromISODate(CalendarID calendarId, const ISO8601::PlainDate& fullISODate, TemporalOverflow overflow)
{
    // Step 13: Set result to ISODateToFields(calendar, isoDate, ~month-day~).
    auto fields = isoDateToFields(calendarId, fullISODate, ResolveType::MonthDay);
    if (!fields)
        return makeUnexpected(fields.error());
    // Step 15: Return ? CalendarMonthDayFromFields(calendar, result, ~constrain~) — every caller
    //   passes ~constrain~ per Step 14's NOTE.
    return monthDayFromFields(calendarId, *fields, overflow);
}

} // namespace TemporalCore
} // namespace JSC
