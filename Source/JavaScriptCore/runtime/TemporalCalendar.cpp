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
#include "TemporalPlainTime.h"
#include "TemporalPlainYearMonth.h"
#include "TemporalZonedDateTime.h"
#include "TimeZoneICUBridge.h"
#include <wtf/text/MakeString.h>

namespace JSC {

std::optional<CalendarID> isBuiltinCalendar(StringView string)
{
    const auto& index = intlAvailableCalendarIndex();
    auto entry = index.find<ASCIICaseInsensitiveStringViewHashTranslator>(string);
    if (entry == index.end())
        return std::nullopt;
    return entry->value;
}

// https://tc39.es/proposal-temporal/#sec-temporal-gettemporalcalendarslotvaluewithisodefault
CalendarID getTemporalCalendarIdentifierWithISODefault(JSGlobalObject* globalObject, JSObject* item)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: If item has an [[InitializedTemporalDate/DateTime/MonthDay/YearMonth/ZonedDateTime]]
    // internal slot, return item.[[Calendar]].
    if (auto* pd = dynamicDowncast<TemporalPlainDate>(item))
        return pd->calendarID();
    if (auto* pdt = dynamicDowncast<TemporalPlainDateTime>(item))
        return pdt->calendarID();
    if (auto* pmd = dynamicDowncast<TemporalPlainMonthDay>(item))
        return pmd->calendarID();
    if (auto* pym = dynamicDowncast<TemporalPlainYearMonth>(item))
        return pym->calendarID();
    if (auto* zdt = dynamicDowncast<TemporalZonedDateTime>(item))
        return zdt->calendarID();

    // Step 2: Let calendarLike be ? Get(item, "calendar").
    JSValue calendarLike = item->get(globalObject, vm.propertyNames->calendar);
    RETURN_IF_EXCEPTION(scope, iso8601CalendarID());
    // Step 3: If calendarLike is undefined, return "iso8601".
    if (calendarLike.isUndefined())
        return iso8601CalendarID();
    // Step 4: Return ? ToTemporalCalendarIdentifier(calendarLike).
    RELEASE_AND_RETURN(scope, toTemporalCalendarIdentifier(globalObject, calendarLike));
}

// temporal_rs: CalendarFields::from_prop_bag
// https://tc39.es/proposal-temporal/#sec-temporal-preparecalendarfields
template<FieldSetType type>
TemporalCore::CalendarFieldsIn readCalendarFieldsFromObject(JSGlobalObject* globalObject, JSObject* bag, CalendarID calendarId, TemporalCore::TimeFieldsIn* timeFieldsOut)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    TemporalCore::CalendarFieldsIn fields;

    auto readTimeField = [&](JSValue val, std::optional<double>& target) {
        if (val.isUndefined())
            return;
        double d = val.toIntegerWithTruncation(globalObject);
        RETURN_IF_EXCEPTION(scope, void());
        if (!std::isfinite(d)) [[unlikely]] {
            throwRangeError(globalObject, scope, "Temporal time properties must be finite"_s);
            return;
        }
        target = d;
    };

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
    if (TemporalCore::calendarHasEras(calendarId)) {
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
    }

    // hour, microsecond, millisecond, minute (DateTime only; alphabetically before month)
    if constexpr (type == FieldSetType::DateTime) {
        ASSERT(timeFieldsOut);
        JSValue hourProp = bag->get(globalObject, vm.propertyNames->hour);
        RETURN_IF_EXCEPTION(scope, fields);
        readTimeField(hourProp, timeFieldsOut->hour);
        RETURN_IF_EXCEPTION(scope, fields);

        JSValue microsecondProp = bag->get(globalObject, vm.propertyNames->microsecond);
        RETURN_IF_EXCEPTION(scope, fields);
        readTimeField(microsecondProp, timeFieldsOut->microsecond);
        RETURN_IF_EXCEPTION(scope, fields);

        JSValue millisecondProp = bag->get(globalObject, vm.propertyNames->millisecond);
        RETURN_IF_EXCEPTION(scope, fields);
        readTimeField(millisecondProp, timeFieldsOut->millisecond);
        RETURN_IF_EXCEPTION(scope, fields);

        JSValue minuteProp = bag->get(globalObject, vm.propertyNames->minute);
        RETURN_IF_EXCEPTION(scope, fields);
        readTimeField(minuteProp, timeFieldsOut->minute);
        RETURN_IF_EXCEPTION(scope, fields);
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

    // monthCode (~to-month-code~: ParseMonthCode)
    {
        JSValue mcProp = bag->get(globalObject, vm.propertyNames->monthCode);
        RETURN_IF_EXCEPTION(scope, fields);
        if (!mcProp.isUndefined()) {
            fields.monthCode = parseMonthCode(globalObject, mcProp);
            RETURN_IF_EXCEPTION(scope, fields);
        }
    }

    // nanosecond, second (DateTime only; alphabetically before year)
    if constexpr (type == FieldSetType::DateTime) {
        JSValue nanosecondProp = bag->get(globalObject, vm.propertyNames->nanosecond);
        RETURN_IF_EXCEPTION(scope, fields);
        readTimeField(nanosecondProp, timeFieldsOut->nanosecond);
        RETURN_IF_EXCEPTION(scope, fields);

        JSValue secondProp = bag->get(globalObject, vm.propertyNames->second);
        RETURN_IF_EXCEPTION(scope, fields);
        readTimeField(secondProp, timeFieldsOut->second);
        RETURN_IF_EXCEPTION(scope, fields);
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
template<ZonedDateTimeFieldMode mode>
ZonedDateTimeFields readZonedDateTimeFieldsFromObject(JSGlobalObject* globalObject, JSObject* bag, CalendarID calendarId)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    ZonedDateTimeFields result; // step 6: all fields ~unset~, step 7: any = false

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
    if (TemporalCore::calendarHasEras(calendarId)) {
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

    // timeZone (~to-temporal-time-zone-identifier~): required only for Full (step 9.d); read but
    // optional for RelativeToDuration; not read at all for Partial (with() has no timeZone field).
    if constexpr (mode != ZonedDateTimeFieldMode::Partial) {
        JSValue v = bag->get(globalObject, vm.propertyNames->timeZone);
        RETURN_IF_EXCEPTION(scope, result);
        if constexpr (mode == ZonedDateTimeFieldMode::Full) {
            if (v.isUndefined()) [[unlikely]] {
                throwTypeError(globalObject, scope, "Temporal.ZonedDateTime.from: timeZone property is required"_s);
                return result;
            }
        }
        if (!v.isUndefined()) {
            // ToTemporalTimeZoneIdentifier: accepts ZonedDateTime or String.
            auto timeZone = toTemporalTimeZoneIdentifier(globalObject, v);
            RETURN_IF_EXCEPTION(scope, result);
            ASSERT(timeZone);
            result.timeZone = *timeZone;
            result.timeZonePresent = true;
        }
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

// https://tc39.es/proposal-temporal/#sec-temporal-interprettemporaldatetimefields
ISO8601::PlainDateTime interpretTemporalDateTimeFields(JSGlobalObject* globalObject, CalendarID calendarId,
    const TemporalCore::CalendarFieldsIn& dateFields, const TemporalCore::TimeFieldsIn& timeFields,
    TemporalOverflow overflow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: isoDate = ? CalendarDateFromFields(calendar, fields, overflow).
    auto dateResult = TemporalCore::dateFromFields(calendarId, dateFields, overflow);
    if (!dateResult) [[unlikely]] {
        if (dateResult.error().kind == TemporalErrorKind::TypeError)
            throwTypeError(globalObject, scope, String(dateResult.error().message));
        else
            throwRangeError(globalObject, scope, String(dateResult.error().message));
        return { };
    }

    // Step 2: time = ? RegulateTime(...).
    ISO8601::Duration timeDur;
    timeDur.setField(TemporalUnit::Hour, timeFields.hour.value_or(0));
    timeDur.setField(TemporalUnit::Minute, timeFields.minute.value_or(0));
    timeDur.setField(TemporalUnit::Second, timeFields.second.value_or(0));
    timeDur.setField(TemporalUnit::Millisecond, timeFields.millisecond.value_or(0));
    timeDur.setField(TemporalUnit::Microsecond, timeFields.microsecond.value_or(0));
    timeDur.setField(TemporalUnit::Nanosecond, timeFields.nanosecond.value_or(0));
    auto plainTime = TemporalPlainTime::regulateTime(globalObject, WTF::move(timeDur), overflow);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 3: Return CombineISODateAndTimeRecord(isoDate, time).
    return ISO8601::PlainDateTime(dateResult->isoDate, plainTime);
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

ISO8601::Duration calendarDateUntil(JSGlobalObject* globalObject, CalendarID calendarId, const ISO8601::PlainDate& one, const ISO8601::PlainDate& two, TemporalUnit largestUnit)
{
    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());
    auto result = TemporalCore::calendarDateUntil(calendarId, one, two, largestUnit);
    if (!result) [[unlikely]] {
        throwTemporalError(globalObject, scope, result.error());
        return { };
    }
    return *result;
}

template TemporalCore::CalendarFieldsIn readCalendarFieldsFromObject<FieldSetType::Date>(JSGlobalObject*, JSObject*, CalendarID, TemporalCore::TimeFieldsIn*);
template TemporalCore::CalendarFieldsIn readCalendarFieldsFromObject<FieldSetType::YearMonth>(JSGlobalObject*, JSObject*, CalendarID, TemporalCore::TimeFieldsIn*);
template TemporalCore::CalendarFieldsIn readCalendarFieldsFromObject<FieldSetType::MonthDay>(JSGlobalObject*, JSObject*, CalendarID, TemporalCore::TimeFieldsIn*);
template TemporalCore::CalendarFieldsIn readCalendarFieldsFromObject<FieldSetType::DateTime>(JSGlobalObject*, JSObject*, CalendarID, TemporalCore::TimeFieldsIn*);

template ZonedDateTimeFields readZonedDateTimeFieldsFromObject<ZonedDateTimeFieldMode::Full>(JSGlobalObject*, JSObject*, CalendarID);
template ZonedDateTimeFields readZonedDateTimeFieldsFromObject<ZonedDateTimeFieldMode::Partial>(JSGlobalObject*, JSObject*, CalendarID);
template ZonedDateTimeFields readZonedDateTimeFieldsFromObject<ZonedDateTimeFieldMode::RelativeToDuration>(JSGlobalObject*, JSObject*, CalendarID);

} // namespace JSC
