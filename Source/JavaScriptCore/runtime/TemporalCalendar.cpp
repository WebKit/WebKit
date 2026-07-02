/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 * Copyright (C) 2022 Sony Interactive Entertainment Inc.
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
#include "TemporalCalendar.h"

#include "CalendarFields.h"
#include "CalendarICUBridge.h"
#include "DurationArithmetic.h"
#include "IntlObjectInlines.h"
#include "JSObjectInlines.h"
#include "StructureCreateInlines.h"
#include "TemporalDuration.h"
#include "TemporalObject.h"
#include "TemporalPlainDate.h"
#include "TemporalPlainDateTime.h"
#include "TemporalPlainMonthDay.h"
#include "TemporalPlainYearMonth.h"
#include "TemporalZonedDateTime.h"
#include "TimeZoneICUBridge.h"
#include <wtf/text/MakeString.h>

namespace JSC {

std::optional<CalendarID> isBuiltinCalendar(StringView string)
{
    // FIXME: bare "islamic" is accepted via ICU's keyword set but V8/temporal_rs reject it;
    // canonicalize to "islamic-civil" (CLDR alias) or filter out.
    const auto& calendars = intlAvailableCalendars();
    for (unsigned index = 0; index < calendars.size(); ++index) {
        if (WTF::equalIgnoringASCIICase(calendars[index], string))
            return index;
    }
    // Legacy alias: "islamicc" → "islamic-civil" (per CLDR/BCP 47).
    if (WTF::equalIgnoringASCIICase(string, "islamicc"_s)) {
        for (unsigned index = 0; index < calendars.size(); ++index) {
            if (calendars[index] == "islamic-civil"_s)
                return index;
        }
    }
    // Legacy alias: "ethiopic-amete-alem" → "ethioaa".
    if (WTF::equalIgnoringASCIICase(string, "ethiopic-amete-alem"_s)) {
        for (unsigned index = 0; index < calendars.size(); ++index) {
            if (calendars[index] == "ethioaa"_s)
                return index;
        }
    }
    return std::nullopt;
}

// https://tc39.es/proposal-temporal/#sec-temporal-calendarresolvefields
static void calendarResolveFields(JSGlobalObject* globalObject, std::optional<int32_t> year, unsigned month, std::optional<ParsedMonthCode> monthCode, TemporalDateFormat format, CalendarID calendarId)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    bool isISO = calendarId == iso8601CalendarID();

    if ((format == TemporalDateFormat::Date || format == TemporalDateFormat::YearMonth)
        && !year) [[unlikely]] {
        throwTypeError(globalObject, scope, "year must be supplied for this Temporal type"_s);
        return;
    }
    if (monthCode) {
        if (isISO) {
            if (monthCode->isLeapMonth) [[unlikely]] {
                throwRangeError(globalObject, scope, "iso8601 calendar does not have leap months"_s);
                return;
            }
            if (monthCode->monthNumber > 12) [[unlikely]] {
                throwRangeError(globalObject, scope, "month must be <= 12 with iso8601 calendar"_s);
                return;
            }
            if (month != monthCode->monthNumber) [[unlikely]] {
                throwRangeError(globalObject, scope, "month does not match month code"_s);
                return;
            }
        }
        // For non-ISO calendars, monthCode validation is handled by the ICU calendar.
    }
}

// temporal_rs: CalendarFields::from_prop_bag
// https://tc39.es/proposal-temporal/#sec-temporal-preparecalendarfields
template<FieldSetType type, CalendarRead calendarRead>
TemporalCore::CalendarFieldsIn readCalendarFieldsFromObject(JSGlobalObject* globalObject, JSObject* bag, CalendarID& outCalendarId)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    TemporalCore::CalendarFieldsIn fields;

    // Alphabetical order per spec: calendar, day, era, eraYear, month, monthCode, year.

    // calendar
    if constexpr (calendarRead == CalendarRead::Read) {
        outCalendarId = iso8601CalendarID();
        JSValue calProp = bag->get(globalObject, vm.propertyNames->calendar);
        RETURN_IF_EXCEPTION(scope, fields);
        if (!calProp.isUndefined()) {
            outCalendarId = toTemporalCalendarIdentifier(globalObject, calProp);
            RETURN_IF_EXCEPTION(scope, fields);
        }
        // For non-ISO calendars, monthCode validation is handled by the ICU calendar.
    }

    // day (not read for YearMonth per spec)
    if constexpr (type != FieldSetType::YearMonth) {
        JSValue dayProp = bag->get(globalObject, vm.propertyNames->day);
        RETURN_IF_EXCEPTION(scope, fields);
        if (!dayProp.isUndefined()) {
            double d = dayProp.toIntegerWithTruncation(globalObject);
            RETURN_IF_EXCEPTION(scope, fields);
            if (!(d > 0 && std::isfinite(d))) [[unlikely]] {
                throwRangeError(globalObject, scope, "day must be positive and finite"_s);
                return fields;
            }
            fields.day = clampTo<uint8_t>(d);
        }
    }

    // era, eraYear (only for calendars with eras)
    if (TemporalCore::calendarHasEras(outCalendarId)) {
        JSValue eraProp = bag->get(globalObject, Identifier::fromString(vm, "era"_s));
        RETURN_IF_EXCEPTION(scope, fields);
        if (!eraProp.isUndefined()) {
            fields.era = eraProp.toWTFString(globalObject);
            RETURN_IF_EXCEPTION(scope, fields);
        }
        JSValue eraYearProp = bag->get(globalObject, Identifier::fromString(vm, "eraYear"_s));
        RETURN_IF_EXCEPTION(scope, fields);
        if (!eraYearProp.isUndefined()) {
            double ey = eraYearProp.toIntegerWithTruncation(globalObject);
            RETURN_IF_EXCEPTION(scope, fields);
            if (!std::isfinite(ey)) [[unlikely]] {
                throwRangeError(globalObject, scope, "eraYear must be finite"_s);
                return fields;
            }
            fields.eraYear = clampTo<int32_t>(ey);
        }
        // era and eraYear must be provided together or not at all.
        if (fields.era.has_value() != fields.eraYear.has_value()) [[unlikely]] {
            throwTypeError(globalObject, scope, "era and eraYear must both be present or both absent"_s);
            return fields;
        }
    }

    // month
    {
        JSValue monthProp = bag->get(globalObject, vm.propertyNames->month);
        RETURN_IF_EXCEPTION(scope, fields);
        if (!monthProp.isUndefined()) {
            double m = monthProp.toIntegerWithTruncation(globalObject);
            RETURN_IF_EXCEPTION(scope, fields);
            if (!std::isfinite(m) || m < 1) [[unlikely]] {
                throwRangeError(globalObject, scope, "month must be positive and finite"_s);
                return fields;
            }
            fields.month = clampTo<uint32_t>(m);
        }
    }

    // monthCode
    {
        JSValue mcProp = bag->get(globalObject, vm.propertyNames->monthCode);
        RETURN_IF_EXCEPTION(scope, fields);
        if (!mcProp.isUndefined()) {
            auto mcPrimitive = mcProp.toPrimitive(globalObject, PreferString);
            RETURN_IF_EXCEPTION(scope, fields);
            if (mcPrimitive.isString()) {
                auto mcStr = asString(mcPrimitive)->value(globalObject);
                RETURN_IF_EXCEPTION(scope, fields);
                fields.monthCode = ISO8601::parseMonthCode(mcStr);
                if (!fields.monthCode) [[unlikely]] {
                    throwRangeError(globalObject, scope, "Invalid monthCode"_s);
                    return fields;
                }
            }
        }
    }

    // year
    {
        JSValue yearProp = bag->get(globalObject, vm.propertyNames->year);
        RETURN_IF_EXCEPTION(scope, fields);
        if (!yearProp.isUndefined()) {
            double y = yearProp.toIntegerWithTruncation(globalObject);
            RETURN_IF_EXCEPTION(scope, fields);
            if (!std::isfinite(y)) [[unlikely]] {
                throwRangeError(globalObject, scope, "year must be finite"_s);
                return fields;
            }
            fields.year = clampTo<int32_t>(y);
        }
    }

    return fields;
}

// https://tc39.es/proposal-temporal/#sec-temporal-preparecalendarfields (ZonedDateTime variant)
//
// Implements PrepareCalendarFields for ZonedDateTime, reading all 15 fields in alphabetical order:
//   calendar, day, era, eraYear, hour, microsecond, millisecond, minute, month,
//   monthCode, nanosecond, offset, second, timeZone, year.
//
// calendarFieldNames  = «year, month, month-code, day» (+ CalendarExtraFields: era, eraYear)
// nonCalendarFieldNames = «hour, minute, second, millisecond, microsecond, nanosecond, offset, time-zone»
// requiredFieldNames = «time-zone» (Full mode) or ~partial~ (Partial mode)
//
// mode == Full: from() — timeZone is the only required field; day/year/month checked later
//               in CalendarDateFromFields/CalendarResolveFields.
// mode == Partial: with() — all fields optional; anyFieldSet tracks whether anything was present.
template<ZonedDateTimeFieldMode mode, CalendarRead calendarRead>
ZonedDateTimeFields readZonedDateTimeFieldsFromObject(JSGlobalObject* globalObject, JSObject* bag, CalendarID& outCalendarId)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    ZonedDateTimeFields result; // step 6: all fields ~unset~, step 7: any = false

    // Steps 2-8: calendar read first (per ToTemporalZonedDateTime step 4.b), then fields in
    // lexicographic order (step 8). Steps 1/5 are static assertions; steps 3-4/6-7 are trivial.

    // calendar — outside PrepareCalendarFields proper (ToTemporalZonedDateTime step 4.b).
    if constexpr (calendarRead == CalendarRead::Read) {
        outCalendarId = iso8601CalendarID();
        JSValue calProp = bag->get(globalObject, vm.propertyNames->calendar);
        RETURN_IF_EXCEPTION(scope, result);
        if (!calProp.isUndefined()) {
            outCalendarId = toTemporalCalendarIdentifier(globalObject, calProp);
            RETURN_IF_EXCEPTION(scope, result);
        }
    }

    // Step 9: for each field in lexicographic order — Get, convert (step 9.c), check required (step 9.d).

    // day (~to-positive-integer-with-truncation~)
    // Not in requiredFieldNames («time-zone» only) — a missing day is caught later in CalendarResolveFields.
    {
        JSValue v = bag->get(globalObject, vm.propertyNames->day);
        RETURN_IF_EXCEPTION(scope, result);
        if (!v.isUndefined()) {
            double d = v.toIntegerWithTruncation(globalObject);
            RETURN_IF_EXCEPTION(scope, result);
            if (!(d > 0 && std::isfinite(d))) [[unlikely]] {
                throwRangeError(globalObject, scope, "day property must be positive and finite"_s);
                return result;
            }
            result.dateFields.day = clampTo<uint8_t>(d);
            result.dayPresent = true;
            result.anyFieldSet = true;
        }
    }

    // era (~to-string~), eraYear (~to-integer-with-truncation~)
    // CalendarExtraFields contributes these for era-based calendars (alphabetically between day and hour).
    if (TemporalCore::calendarHasEras(outCalendarId)) {
        JSValue eraVal = bag->get(globalObject, Identifier::fromString(vm, "era"_s));
        RETURN_IF_EXCEPTION(scope, result);
        if (!eraVal.isUndefined()) {
            result.dateFields.era = eraVal.toWTFString(globalObject);
            RETURN_IF_EXCEPTION(scope, result);
            result.anyFieldSet = true;
        }
        JSValue eraYearVal = bag->get(globalObject, Identifier::fromString(vm, "eraYear"_s));
        RETURN_IF_EXCEPTION(scope, result);
        if (!eraYearVal.isUndefined()) {
            double ey = eraYearVal.toIntegerWithTruncation(globalObject);
            RETURN_IF_EXCEPTION(scope, result);
            if (!std::isfinite(ey)) [[unlikely]] {
                throwRangeError(globalObject, scope, "eraYear property must be finite"_s);
                return result;
            }
            result.dateFields.eraYear = clampTo<int32_t>(ey);
            result.anyFieldSet = true;
        }
        // CalendarResolveFields requires era and eraYear to be present together or both absent.
        if (result.dateFields.era.has_value() != result.dateFields.eraYear.has_value()) [[unlikely]] {
            throwTypeError(globalObject, scope, "era and eraYear must both be present or both absent"_s);
            return result;
        }
    }

    // Helper for the six ~to-integer-with-truncation~ time fields.
    auto readTimeField = [&](PropertyName name, std::optional<double>& field) {
        JSValue v = bag->get(globalObject, name);
        RETURN_IF_EXCEPTION(scope, void());
        if (v.isUndefined())
            return;
        double val = v.toIntegerWithTruncation(globalObject);
        RETURN_IF_EXCEPTION(scope, void());
        if (!std::isfinite(val)) [[unlikely]] {
            throwRangeError(globalObject, scope, "Temporal time properties must be finite"_s);
            return;
        }
        field = val;
        result.anyFieldSet = true;
    };

    // hour, microsecond, millisecond, minute (~to-integer-with-truncation~)
    readTimeField(vm.propertyNames->hour, result.hour);
    RETURN_IF_EXCEPTION(scope, result);
    readTimeField(vm.propertyNames->microsecond, result.microsecond);
    RETURN_IF_EXCEPTION(scope, result);
    readTimeField(vm.propertyNames->millisecond, result.millisecond);
    RETURN_IF_EXCEPTION(scope, result);
    readTimeField(vm.propertyNames->minute, result.minute);
    RETURN_IF_EXCEPTION(scope, result);

    // month (~to-positive-integer-with-truncation~)
    {
        JSValue v = bag->get(globalObject, vm.propertyNames->month);
        RETURN_IF_EXCEPTION(scope, result);
        if (!v.isUndefined()) {
            double m = v.toIntegerWithTruncation(globalObject);
            RETURN_IF_EXCEPTION(scope, result);
            if (!std::isfinite(m) || m <= 0) [[unlikely]] {
                throwRangeError(globalObject, scope, "month property must be a positive finite integer"_s);
                return result;
            }
            result.dateFields.month = clampTo<uint32_t>(m);
            result.monthPresent = true;
            result.anyFieldSet = true;
        }
    }

    // monthCode (~to-month-code~: ParseMonthCode)
    {
        JSValue v = bag->get(globalObject, vm.propertyNames->monthCode);
        RETURN_IF_EXCEPTION(scope, result);
        if (!v.isUndefined()) {
            result.dateFields.monthCode = parseMonthCode(globalObject, v);
            RETURN_IF_EXCEPTION(scope, result);
            result.monthCodePresent = true;
            result.anyFieldSet = true;
        }
    }

    // nanosecond (~to-integer-with-truncation~)
    readTimeField(vm.propertyNames->nanosecond, result.nanosecond);
    RETURN_IF_EXCEPTION(scope, result);

    // offset (~to-offset-string~: ToPrimitive + String check + ParseDateTimeUTCOffset)
    // Pre-parsed to nanoseconds rather than storing the raw string.
    {
        JSValue v = bag->get(globalObject, vm.propertyNames->offset);
        RETURN_IF_EXCEPTION(scope, result);
        if (!v.isUndefined()) {
            JSValue prim = v.toPrimitive(globalObject, PreferString);
            RETURN_IF_EXCEPTION(scope, result);
            if (!prim.isString()) [[unlikely]] {
                throwTypeError(globalObject, scope, "offset must be a string"_s);
                return result;
            }
            String offsetStr = asString(prim)->value(globalObject);
            RETURN_IF_EXCEPTION(scope, result);
            auto parsed = ISO8601::parseUTCOffset(offsetStr);
            if (!parsed) [[unlikely]] {
                throwRangeError(globalObject, scope, makeString("'"_s, ellipsizeAt(100, offsetStr), "' is not a valid UTC offset string"_s));
                return result;
            }
            result.offsetNs = *parsed;
            result.anyFieldSet = true;
        }
    }

    // second (~to-integer-with-truncation~)
    readTimeField(vm.propertyNames->second, result.second);
    RETURN_IF_EXCEPTION(scope, result);

    // timeZone (~to-temporal-time-zone-identifier~; Full mode only — step 9.d required field)
    if constexpr (mode != ZonedDateTimeFieldMode::Partial) {
        JSValue v = bag->get(globalObject, vm.propertyNames->timeZone);
        RETURN_IF_EXCEPTION(scope, result);
        if (v.isUndefined()) [[unlikely]] {
            throwTypeError(globalObject, scope, "Temporal.ZonedDateTime.from: timeZone property is required"_s);
            return result;
        }
        // ToTemporalTimeZoneIdentifier: accepts ZonedDateTime or String.
        auto timeZone = toTemporalTimeZoneIdentifier(globalObject, v);
        RETURN_IF_EXCEPTION(scope, result);
        ASSERT(timeZone);
        result.timeZone = *timeZone;
    }

    // year (~to-integer-with-truncation~)
    {
        JSValue v = bag->get(globalObject, vm.propertyNames->year);
        RETURN_IF_EXCEPTION(scope, result);
        if (!v.isUndefined()) {
            // Step 9.c: apply ~to-integer-with-truncation~.
            double y = v.toIntegerWithTruncation(globalObject);
            RETURN_IF_EXCEPTION(scope, result);
            if (!std::isfinite(y)) [[unlikely]] {
                throwRangeError(globalObject, scope, "year property must be finite"_s);
                return result;
            }
            result.dateFields.year = clampTo<int32_t>(y);
            result.yearPresent = true;
            result.anyFieldSet = true;
        }
    }

    // Step 10: ~partial~ and no field set → TypeError.
    if constexpr (mode == ZonedDateTimeFieldMode::Partial) {
        if (!result.anyFieldSet) [[unlikely]] {
            throwTypeError(globalObject, scope, "at least one Temporal field must be provided"_s);
            return result;
        }
    }

    // Step 11: Return result.
    return result;
}

// https://tc39.es/proposal-temporal/#sec-temporal-parsemonthcode
std::optional<ParsedMonthCode> parseMonthCode(JSGlobalObject* globalObject, JSValue argument)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Let monthCode be ? ToPrimitive(argument, ~string~).
    JSValue primitive = argument.toPrimitive(globalObject, PreferString);
    RETURN_IF_EXCEPTION(scope, { });
    // Step 2: If monthCode is not a String, throw a TypeError exception.
    if (!primitive.isString()) [[unlikely]] {
        throwTypeError(globalObject, scope, "monthCode must be a string"_s);
        return { };
    }
    auto string = asString(primitive)->value(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    // Steps 3-8: validate the MonthCode grammar and extract [[IsLeapMonth]] / [[MonthNumber]].
    //            Returns nullopt if the string does not match the grammar (step 3 RangeError below).
    auto parsed = ISO8601::parseMonthCode(string);
    if (!parsed) [[unlikely]] {
        throwRangeError(globalObject, scope, "Invalid monthCode"_s);
        return { };
    }
    return parsed;
}

// https://tc39.es/proposal-temporal/#sec-temporal-isodatefromfields
ISO8601::PlainDate isoDateFromFields(JSGlobalObject* globalObject, TemporalDateFormat format, int32_t year, uint32_t month, uint32_t day, std::optional<ParsedMonthCode> monthCode, TemporalOverflow overflow, CalendarID calendarId)
{

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    calendarResolveFields(globalObject, year, month, monthCode, format, calendarId);
    RETURN_IF_EXCEPTION(scope, { });

    ASSERT(month > 0);
    ASSERT(day > 0);

    if (calendarId != iso8601CalendarID()) {
        auto result = TemporalCore::calendarDateFromFields(
            calendarId, std::optional<int32_t>(year), static_cast<uint8_t>(month), static_cast<uint8_t>(day),
            std::nullopt, std::nullopt, monthCode, overflow);
        if (!result) [[unlikely]] {
            throwRangeError(globalObject, scope, String(result.error().message));
            return { };
        }
        return *result;
    }

    if (overflow == TemporalOverflow::Constrain) {
        month = std::min<uint32_t>(month, 12);
        day = std::min<uint32_t>(day, ISO8601::daysInMonth(year, month));
    }

    auto plainDate = TemporalPlainDate::validateAndCreateISODateRecord(globalObject, ISO8601::Duration(year, month, 0LL, day, 0LL, 0LL, 0LL, 0LL, Int128(0), Int128(0)));
    RETURN_IF_EXCEPTION(scope, { });

    bool valid = true;
    switch (format) {
    case TemporalDateFormat::YearMonth:
        valid = ISO8601::isYearMonthWithinLimits(plainDate.year(), plainDate.month());
        break;
    default:
        valid = ISO8601::isDateTimeWithinLimits(plainDate.year(), plainDate.month(), plainDate.day(), 12, 0, 0, 0, 0, 0);
        break;
    }

    if (!valid) [[unlikely]] {
        throwRangeError(globalObject, scope, "date time is out of range of ECMAScript representation"_s);
        return { };
    }

    return plainDate;
}

ISO8601::PlainDate isoDateAdd(JSGlobalObject* globalObject, const ISO8601::PlainDate& plainDate, const ISO8601::Duration& duration, TemporalOverflow overflow)
{
    auto result = TemporalCore::isoDateAdd(plainDate, duration, overflow);
    if (!result) [[unlikely]] {
        VM& vm = globalObject->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        throwRangeError(globalObject, scope, result.error().message);
        return { };
    }
    return *result;
}

// https://tc39.es/proposal-temporal/#sec-temporal-calendardateadd
ISO8601::PlainDate calendarDateAdd(JSGlobalObject* globalObject, CalendarID calendarId, const ISO8601::PlainDate& plainDate, const ISO8601::Duration& duration, TemporalOverflow overflow)
{
    if (calendarId == iso8601CalendarID())
        return isoDateAdd(globalObject, plainDate, duration, overflow);
    auto result = TemporalCore::calendarDateAdd(calendarId, plainDate, duration, overflow);
    if (!result) [[unlikely]] {
        VM& vm = globalObject->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        throwRangeError(globalObject, scope, result.error().message);
        return { };
    }
    return *result;
}

// https://tc39.es/proposal-temporal/#sec-temporal-calendardateuntil
ISO8601::Duration calendarDateUntil(CalendarID calendarId, const ISO8601::PlainDate& one, const ISO8601::PlainDate& two, TemporalUnit largestUnit)
{
    if (calendarId == iso8601CalendarID())
        return TemporalCore::calendarDateUntil(one, two, largestUnit);
    auto result = TemporalCore::calendarDateUntil(calendarId, one, two, largestUnit);
    if (!result) [[unlikely]]
        return { };
    return *result;
}

// https://tc39.es/proposal-temporal/#sec-temporal-differencetemporalplainyearmonth
template<DifferenceOperation op>
ISO8601::Duration differenceTemporalPlainYearMonth(JSGlobalObject* globalObject, const ISO8601::PlainYearMonth& yearMonth, const ISO8601::PlainYearMonth& other, unsigned increment, TemporalUnit smallestUnit, TemporalUnit largestUnit, RoundingMode roundingMode, CalendarID calendarId)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (largestUnit == TemporalUnit::Week || largestUnit == TemporalUnit::Day) [[unlikely]] {
        throwRangeError(globalObject, scope, "largestUnit must be one of year, years, month, months"_s);
        return { };
    }

    if (smallestUnit == TemporalUnit::Week || smallestUnit == TemporalUnit::Day) [[unlikely]] {
        throwRangeError(globalObject, scope, "smallestUnit must be one of year, years, month, months"_s);
        return { };
    }

    auto sign = TemporalCore::isoDateCompare(yearMonth.isoPlainDate(), other.isoPlainDate());
    if (!sign)
        return { };

    auto thisDate = yearMonth.isoPlainDate();
    auto otherDate = other.isoPlainDate();

    auto thisWithinLimits = ISO8601::isDateTimeWithinLimits(thisDate.year(), thisDate.month(), thisDate.day(), 12, 0, 0, 0, 0, 0);
    auto otherWithinLimits = ISO8601::isDateTimeWithinLimits(otherDate.year(), otherDate.month(), otherDate.day(), 12, 0, 0, 0, 0, 0);
    if (!thisWithinLimits || !otherWithinLimits) [[unlikely]] {
        throwRangeError(globalObject, scope, "date/time value is outside of supported range"_s);
        return { };
    }
    // Use temporal/core differenceYearMonth which resolves both to day=1 via dateFromFields.
    auto dateDiffResult = TemporalCore::differenceYearMonth(calendarId, thisDate, otherDate, largestUnit);
    ISO8601::Duration dateDifference;
    if (!dateDiffResult) {
        // Fallback to direct calendarDateUntil if differenceYearMonth fails.
        if (calendarId != iso8601CalendarID())
            dateDifference = calendarDateUntil(calendarId, thisDate, otherDate, largestUnit);
        else
            dateDifference = TemporalCore::calendarDateUntil(thisDate, otherDate, largestUnit);
    } else
        dateDifference = *dateDiffResult;
    auto duration = ISO8601::InternalDuration::combineDateAndTimeDuration(ISO8601::Duration { dateDifference.years(), dateDifference.months(), 0LL, 0LL, 0LL, 0LL, 0LL, 0LL, Int128(0), Int128(0) }, 0);

    if (smallestUnit != TemporalUnit::Month || increment != 1) {
        auto originEpochNs = TemporalCore::getUTCEpochNanoseconds(thisDate, ISO8601::PlainTime());
        auto destEpochNs = TemporalCore::getUTCEpochNanoseconds(otherDate, ISO8601::PlainTime());
        auto roundResult = TemporalCore::roundRelativeDuration(duration, originEpochNs, destEpochNs, thisDate, ISO8601::PlainTime(), largestUnit, increment, smallestUnit, roundingMode, nullptr, calendarId);
        if (!roundResult) [[unlikely]] {
            throwTemporalError(globalObject, scope, roundResult.error());
            return { };
        }
    }
    auto durResult = TemporalCore::temporalDurationFromInternal(duration, TemporalUnit::Day);
    if (!durResult) [[unlikely]] {
        throwTemporalError(globalObject, scope, durResult.error());
        return { };
    }
    ISO8601::Duration result = *durResult;
    if (op == DifferenceOperation::Since)
        result = -result;
    return result;
}

template ISO8601::Duration differenceTemporalPlainYearMonth<DifferenceOperation::Since>(JSGlobalObject*, const ISO8601::PlainYearMonth&, const ISO8601::PlainYearMonth&, unsigned, TemporalUnit, TemporalUnit, RoundingMode, CalendarID);
template ISO8601::Duration differenceTemporalPlainYearMonth<DifferenceOperation::Until>(JSGlobalObject*, const ISO8601::PlainYearMonth&, const ISO8601::PlainYearMonth&, unsigned, TemporalUnit, TemporalUnit, RoundingMode, CalendarID);

template TemporalCore::CalendarFieldsIn readCalendarFieldsFromObject<FieldSetType::Date, CalendarRead::Read>(JSGlobalObject*, JSObject*, CalendarID&);
template TemporalCore::CalendarFieldsIn readCalendarFieldsFromObject<FieldSetType::Date, CalendarRead::Skip>(JSGlobalObject*, JSObject*, CalendarID&);
template TemporalCore::CalendarFieldsIn readCalendarFieldsFromObject<FieldSetType::YearMonth, CalendarRead::Read>(JSGlobalObject*, JSObject*, CalendarID&);
template TemporalCore::CalendarFieldsIn readCalendarFieldsFromObject<FieldSetType::YearMonth, CalendarRead::Skip>(JSGlobalObject*, JSObject*, CalendarID&);
template TemporalCore::CalendarFieldsIn readCalendarFieldsFromObject<FieldSetType::MonthDay, CalendarRead::Read>(JSGlobalObject*, JSObject*, CalendarID&);
template TemporalCore::CalendarFieldsIn readCalendarFieldsFromObject<FieldSetType::MonthDay, CalendarRead::Skip>(JSGlobalObject*, JSObject*, CalendarID&);

template ZonedDateTimeFields readZonedDateTimeFieldsFromObject<ZonedDateTimeFieldMode::Full, CalendarRead::Read>(JSGlobalObject*, JSObject*, CalendarID&);
template ZonedDateTimeFields readZonedDateTimeFieldsFromObject<ZonedDateTimeFieldMode::Partial, CalendarRead::Skip>(JSGlobalObject*, JSObject*, CalendarID&);

} // namespace JSC
