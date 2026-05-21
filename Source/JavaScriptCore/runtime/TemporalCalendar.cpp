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
#include "TemporalPlainDate.h"
#include "TemporalPlainDateTime.h"
#include "TemporalPlainMonthDay.h"
#include "TemporalPlainYearMonth.h"
#include "TemporalZonedDateTime.h"

namespace JSC {

std::optional<CalendarID> isBuiltinCalendar(StringView string)
{
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
void calendarResolveFields(JSGlobalObject* globalObject, std::optional<int32_t> year, unsigned month, std::optional<ParsedMonthCode> monthCode, TemporalDateFormat format, StringView calendarId)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    bool isISO = calendarId.isEmpty() || calendarId == "iso8601"_s;

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

// PrepareCalendarFields equivalent — reads all fields from temporalDateLike
// into local variables. Performs type conversion (ToPositiveIntegerWithTruncation
// for day/month, ToIntegerOrInfinity for year, ToPrimitive+parseMonthCode for
// monthCode). Does NOT read overflow or process fields with overflow.
struct PreparedDateFields {
    double day { 1 };
    double month { 0 };
    double year { 1972 };
    std::optional<ParsedMonthCode> monthCode;
    String calendarId;
    std::optional<String> era;
    std::optional<int32_t> eraYear;
    bool monthCodePresent { false };
    bool monthUndefined { true };
};

static std::optional<PreparedDateFields> prepareDateFields(
    JSGlobalObject* globalObject, JSObject* temporalDateLike, TemporalDateFormat format)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    PreparedDateFields result;

    // Calendar validation (spec: GetTemporalCalendarIdentifierWithISODefault).
    // Read and validate calendar property before other fields.
    JSValue calendarProperty = temporalDateLike->get(globalObject, vm.propertyNames->calendar);
    RETURN_IF_EXCEPTION(scope, std::nullopt);
    if (!calendarProperty.isUndefined()) {
        auto calId = toTemporalCalendarIdentifier(globalObject, calendarProperty);
        RETURN_IF_EXCEPTION(scope, std::nullopt);
        if (!isBuiltinCalendar(calId)) {
            throwRangeError(globalObject, scope, makeString("'"_s, calId, "' is not a valid calendar identifier"_s));
            return std::nullopt;
        }
        result.calendarId = WTF::move(calId);
    }

    // Day
    if (format != TemporalDateFormat::YearMonth) {
        JSValue dayProperty = temporalDateLike->get(globalObject, vm.propertyNames->day);
        RETURN_IF_EXCEPTION(scope, std::nullopt);
        if (dayProperty.isUndefined()) {
            throwTypeError(globalObject, scope, "day property must be present"_s);
            return std::nullopt;
        }
        result.day = dayProperty.toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, std::nullopt);
        if (!(result.day > 0 && std::isfinite(result.day))) {
            throwRangeError(globalObject, scope, "day property must be positive and finite"_s);
            return std::nullopt;
        }
    }

    // Era + EraYear (only for calendars that support eras — not ISO, Chinese, Dangi)
    bool calendarUsesEras = !result.calendarId.isNull()
        && TemporalCore::calendarHasEras(TemporalCore::calendarIDFromString(result.calendarId));
    if (calendarUsesEras) {
        JSValue eraProperty = temporalDateLike->get(globalObject, Identifier::fromString(vm, "era"_s));
        RETURN_IF_EXCEPTION(scope, std::nullopt);
        if (!eraProperty.isUndefined()) {
            auto eraStr = eraProperty.toWTFString(globalObject);
            RETURN_IF_EXCEPTION(scope, std::nullopt);
            result.era = WTF::move(eraStr);
        }

        JSValue eraYearProperty = temporalDateLike->get(globalObject, Identifier::fromString(vm, "eraYear"_s));
        RETURN_IF_EXCEPTION(scope, std::nullopt);
        if (!eraYearProperty.isUndefined()) {
            double ey = eraYearProperty.toIntegerOrInfinity(globalObject);
            RETURN_IF_EXCEPTION(scope, std::nullopt);
            if (!std::isfinite(ey)) {
                throwRangeError(globalObject, scope, "eraYear property must be finite"_s);
                return std::nullopt;
            }
            result.eraYear = static_cast<int32_t>(ey);
        }
    }

    // Month
    JSValue monthProperty = temporalDateLike->get(globalObject, vm.propertyNames->month);
    RETURN_IF_EXCEPTION(scope, std::nullopt);
    if (!monthProperty.isUndefined()) {
        result.month = monthProperty.toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, std::nullopt);
        result.monthUndefined = false;
    }

    // MonthCode
    JSValue monthCodeProperty = temporalDateLike->get(globalObject, vm.propertyNames->monthCode);
    RETURN_IF_EXCEPTION(scope, std::nullopt);
    if (monthCodeProperty.isUndefined()) {
        if (result.monthUndefined) {
            throwTypeError(globalObject, scope, "Either month or monthCode property must be provided"_s);
            return std::nullopt;
        }
        if (!(result.month > 0 && std::isfinite(result.month))) {
            throwRangeError(globalObject, scope, "month property must be positive and finite"_s);
            return std::nullopt;
        }
    } else {
        result.monthCodePresent = true;
        auto monthCodePrimitive = monthCodeProperty.toPrimitive(globalObject, PreferString);
        RETURN_IF_EXCEPTION(scope, std::nullopt);
        if (!monthCodePrimitive.isString()) {
            throwTypeError(globalObject, scope, "monthCode must be a string"_s);
            return std::nullopt;
        }
        auto monthCodeString = asString(monthCodePrimitive)->value(globalObject);
        RETURN_IF_EXCEPTION(scope, std::nullopt);
        result.monthCode = ISO8601::parseMonthCode(monthCodeString);
        if (!result.monthCode) {
            throwRangeError(globalObject, scope, "Invalid monthCode property"_s);
            return std::nullopt;
        }
    }

    // Year
    JSValue yearProperty = temporalDateLike->get(globalObject, vm.propertyNames->year);
    RETURN_IF_EXCEPTION(scope, std::nullopt);
    if (format != TemporalDateFormat::MonthDay) {
        // Year is not required when era+eraYear are provided (they derive the year).
        bool hasEraYear = result.era.has_value() && result.eraYear.has_value();
        if (yearProperty.isUndefined() && !hasEraYear) {
            throwTypeError(globalObject, scope, "year property must be present"_s);
            return std::nullopt;
        }
    }
    if (!yearProperty.isUndefined()) {
        result.year = yearProperty.toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, std::nullopt);
        if (!std::isfinite(result.year)) {
            throwRangeError(globalObject, scope, "year property must be finite"_s);
            return std::nullopt;
        }
    }

    return result;
}

// temporal_rs: CalendarFields::from_prop_bag
// https://tc39.es/proposal-temporal/#sec-temporal-preparecalendarfields
TemporalCore::CalendarFieldsIn readCalendarFieldsFromObject(JSGlobalObject* globalObject, JSObject* bag, String& outCalendarId, FieldSetType type, bool skipCalendarRead)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    TemporalCore::CalendarFieldsIn fields;

    // Alphabetical order per spec: calendar, day, era, eraYear, month, monthCode, year.

    // calendar
    outCalendarId = "iso8601"_s;
    if (!skipCalendarRead) {
        JSValue calProp = bag->get(globalObject, vm.propertyNames->calendar);
        RETURN_IF_EXCEPTION(scope, fields);
        if (!calProp.isUndefined()) {
            outCalendarId = toTemporalCalendarIdentifier(globalObject, calProp);
            RETURN_IF_EXCEPTION(scope, fields);
        }
    }

    // day (not read for YearMonth per spec)
    if (type != FieldSetType::YearMonth) {
        JSValue dayProp = bag->get(globalObject, vm.propertyNames->day);
        RETURN_IF_EXCEPTION(scope, fields);
        if (!dayProp.isUndefined()) {
            double d = dayProp.toIntegerOrInfinity(globalObject);
            RETURN_IF_EXCEPTION(scope, fields);
            if (!(d > 0 && std::isfinite(d))) {
                throwRangeError(globalObject, scope, "day must be positive and finite"_s);
                return fields;
            }
            fields.day = static_cast<uint8_t>(std::min(d, static_cast<double>(std::numeric_limits<uint8_t>::max())));
        }
    }

    // era, eraYear (only for calendars with eras)
    if (TemporalCore::calendarHasEras(TemporalCore::calendarIDFromString(outCalendarId))) {
        JSValue eraProp = bag->get(globalObject, Identifier::fromString(vm, "era"_s));
        RETURN_IF_EXCEPTION(scope, fields);
        if (!eraProp.isUndefined()) {
            fields.era = eraProp.toWTFString(globalObject);
            RETURN_IF_EXCEPTION(scope, fields);
        }
        JSValue eraYearProp = bag->get(globalObject, Identifier::fromString(vm, "eraYear"_s));
        RETURN_IF_EXCEPTION(scope, fields);
        if (!eraYearProp.isUndefined()) {
            double ey = eraYearProp.toIntegerOrInfinity(globalObject);
            RETURN_IF_EXCEPTION(scope, fields);
            if (!std::isfinite(ey)) {
                throwRangeError(globalObject, scope, "eraYear must be finite"_s);
                return fields;
            }
            fields.eraYear = static_cast<int32_t>(ey);
        }
        // era and eraYear must be provided together or not at all.
        if (fields.era.has_value() != fields.eraYear.has_value()) {
            throwTypeError(globalObject, scope, "era and eraYear must both be present or both absent"_s);
            return fields;
        }
    }

    // month
    {
        JSValue monthProp = bag->get(globalObject, vm.propertyNames->month);
        RETURN_IF_EXCEPTION(scope, fields);
        if (!monthProp.isUndefined()) {
            double m = monthProp.toIntegerOrInfinity(globalObject);
            RETURN_IF_EXCEPTION(scope, fields);
            if (!std::isfinite(m) || m < 1) {
                throwRangeError(globalObject, scope, "month must be positive and finite"_s);
                return fields;
            }
            fields.month = static_cast<uint32_t>(std::min(m, static_cast<double>(std::numeric_limits<uint32_t>::max())));
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
                if (!fields.monthCode) {
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
            double y = yearProp.toIntegerOrInfinity(globalObject);
            RETURN_IF_EXCEPTION(scope, fields);
            if (!std::isfinite(y)) {
                throwRangeError(globalObject, scope, "year must be finite"_s);
                return fields;
            }
            fields.year = static_cast<int32_t>(y);
        }
    }

    return fields;
}


ISO8601::PlainDate isoDateFromFields(JSGlobalObject* globalObject, JSObject* temporalDateLike, TemporalDateFormat format, Variant<JSObject*, TemporalOverflow> optionsOrOverflow, TemporalOverflow& overflow, String* outCalendarId)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Validate optional 'calendar' property and extract calendar ID (single read).
    bool isNonISOCalendar = false;
    {
        JSValue calendarProperty = temporalDateLike->get(globalObject, vm.propertyNames->calendar);
        RETURN_IF_EXCEPTION(scope, { });
        if (!calendarProperty.isUndefined()) {
            String calId;
            if (calendarProperty.isObject()) {
                JSObject* calObj = asObject(calendarProperty);
                if (calObj->inherits<TemporalPlainDate>())
                    calId = uncheckedDowncast<TemporalPlainDate>(calObj)->calendarId();
                else if (calObj->inherits<TemporalPlainDateTime>())
                    calId = uncheckedDowncast<TemporalPlainDateTime>(calObj)->calendarId();
                else if (calObj->inherits<TemporalPlainYearMonth>())
                    calId = uncheckedDowncast<TemporalPlainYearMonth>(calObj)->calendarId();
                else if (calObj->inherits<TemporalPlainMonthDay>())
                    calId = uncheckedDowncast<TemporalPlainMonthDay>(calObj)->calendarId();
                else if (calObj->inherits<TemporalZonedDateTime>())
                    calId = uncheckedDowncast<TemporalZonedDateTime>(calObj)->calendarId();
                else {
                    throwTypeError(globalObject, scope, "calendar property must be a string or Temporal date-like object"_s);
                    return { };
                }
            } else if (calendarProperty.isString()) {
                calId = toTemporalCalendarIdentifier(globalObject, calendarProperty);
                RETURN_IF_EXCEPTION(scope, { });
            } else {
                throwTypeError(globalObject, scope, "calendar property must be a string"_s);
                return { };
            }
            if (!calId.isNull() && !WTF::equalIgnoringASCIICase(StringView { calId }, "iso8601"_s)) {
                isNonISOCalendar = true;
                if (outCalendarId)
                    *outCalendarId = calId;
            }
        }
    }

    // Access and convert day property
    double day = 1;
    if (format != TemporalDateFormat::YearMonth) {
        JSValue dayProperty = temporalDateLike->get(globalObject, vm.propertyNames->day);
        RETURN_IF_EXCEPTION(scope, { });

        if (dayProperty.isUndefined()) [[unlikely]] {
            throwTypeError(globalObject, scope, "day property must be present"_s);
            return { };
        }

        day = dayProperty.toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        if (!(day > 0 && std::isfinite(day))) [[unlikely]] {
            throwRangeError(globalObject, scope, "day property must be positive and finite"_s);
            return { };
        }
    }

    // Era + EraYear (only for calendars that support eras)
    std::optional<String> extractedEra;
    std::optional<int32_t> extractedEraYear;
    {
        String calIdStr = (outCalendarId && !outCalendarId->isNull()) ? *outCalendarId : String();
        bool calUsesEras = isNonISOCalendar
            && calIdStr != "chinese"_s && calIdStr != "dangi"_s;
        if (calUsesEras) {
            JSValue eraProperty = temporalDateLike->get(globalObject, Identifier::fromString(vm, "era"_s));
            RETURN_IF_EXCEPTION(scope, { });
            if (!eraProperty.isUndefined()) {
                auto eraStr = eraProperty.toWTFString(globalObject);
                RETURN_IF_EXCEPTION(scope, { });
                extractedEra = WTF::move(eraStr);
            }

            JSValue eraYearProperty = temporalDateLike->get(globalObject, Identifier::fromString(vm, "eraYear"_s));
            RETURN_IF_EXCEPTION(scope, { });
            if (!eraYearProperty.isUndefined()) {
                double ey = eraYearProperty.toIntegerOrInfinity(globalObject);
                RETURN_IF_EXCEPTION(scope, { });
                if (!std::isfinite(ey)) {
                    throwRangeError(globalObject, scope, "eraYear property must be finite"_s);
                    return { };
                }
                extractedEraYear = static_cast<int32_t>(ey);
            }
        }
    }

    // Access and convert month property
    JSValue monthProperty = temporalDateLike->get(globalObject, vm.propertyNames->month);
    RETURN_IF_EXCEPTION(scope, { });
    double month = 0;
    if (!monthProperty.isUndefined()) {
        month = monthProperty.toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
    }

    // Access and convert monthCode property
    JSValue monthCodeProperty = temporalDateLike->get(globalObject, vm.propertyNames->monthCode);
    RETURN_IF_EXCEPTION(scope, { });
    std::optional<ParsedMonthCode> otherMonth;
    bool monthCodePresent = false;
    if (monthCodeProperty.isUndefined()) {
        if (monthProperty.isUndefined()) [[unlikely]] {
            throwTypeError(globalObject, scope, "Either month or monthCode property must be provided"_s);
            return { };
        }

        if (!(month > 0 && std::isfinite(month))) [[unlikely]] {
            throwRangeError(globalObject, scope, "month property must be positive and finite"_s);
            return { };
        }
    } else {
        monthCodePresent = true;
        auto monthCodePrimitive = monthCodeProperty.toPrimitive(globalObject, PreferString);
        RETURN_IF_EXCEPTION(scope, { });
        if (!monthCodePrimitive.isString()) {
            throwTypeError(globalObject, scope, "monthCode must be a string"_s);
            return { };
        }
        auto monthCodeString = asString(monthCodePrimitive)->value(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        otherMonth = ISO8601::parseMonthCode(monthCodeString);

        if (!otherMonth) [[unlikely]] {
            throwRangeError(globalObject, scope, "Invalid monthCode property"_s);
            return { };
        }
    }

    // Access and convert year property
    double year = 1972; // Default reference year for MonthDay
    JSValue yearProperty = temporalDateLike->get(globalObject, vm.propertyNames->year);
    RETURN_IF_EXCEPTION(scope, { });

    if (format != TemporalDateFormat::MonthDay) {
        bool hasEraYear = extractedEra.has_value() && extractedEraYear.has_value();
        if (yearProperty.isUndefined() && !hasEraYear) [[unlikely]] {
            throwTypeError(globalObject, scope, "year property must be present"_s);
            return { };
        }
    }

    if (!yearProperty.isUndefined()) {
        year = yearProperty.toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        if (!std::isfinite(year)) [[unlikely]] {
            throwRangeError(globalObject, scope, "year property must be finite"_s);
            return { };
        }
    }

    if (std::holds_alternative<TemporalOverflow>(optionsOrOverflow))
        overflow = std::get<TemporalOverflow>(optionsOrOverflow);
    else {
        JSObject* options = std::get<JSObject*>(optionsOrOverflow);
        if (options) {
            overflow = toTemporalOverflow(globalObject, options);
            RETURN_IF_EXCEPTION(scope, { });
        }
    }

    // Check month code if applicable
    if (monthCodePresent) {
        ASSERT(otherMonth);

        if (!isNonISOCalendar) {
            if (otherMonth->monthNumber < 1 || otherMonth->monthNumber > 12 || otherMonth->isLeapMonth) [[unlikely]] {
                throwRangeError(globalObject, scope, "month code is not valid for ISO 8601 calendar"_s);
                return { };
            }
        }

        if (monthProperty.isUndefined())
            month = otherMonth->monthNumber;
        else if (otherMonth->monthNumber != month) [[unlikely]] {
            throwRangeError(globalObject, scope, "month and monthCode properties must match if both are provided"_s);
            return { };
        }
    }

    // Duplicate code from TemporalPlainDate::toPlainDate so we can convert from
    // double to int32_t / unsigned here
    if (!ISO8601::isYearWithinLimits(year)) [[unlikely]] {
        throwRangeError(globalObject, scope, "year is out of range"_s);
        return { };
    }

    if (!isNonISOCalendar) {
        if (overflow == TemporalOverflow::Constrain)
            month = std::clamp<double>(month, 1, 12); // clamp both lower and upper bounds

        if (!(month >= 1 && month <= 12)) [[unlikely]] {
            throwRangeError(globalObject, scope, "month is out of range"_s);
            return { };
        }
    }

    uint8_t daysInMonth = ISO8601::daysInMonth(year, month);
    if (overflow == TemporalOverflow::Constrain)
        day = std::clamp<double>(day, 1, daysInMonth); // clamp both bounds

    if (!(day >= 1 && day <= daysInMonth)) [[unlikely]] {
        throwRangeError(globalObject, scope, "day is out of range"_s);
        return { };
    }

    // For non-ISO calendars with era/eraYear, use calendarDateFromFields directly.
    if (isNonISOCalendar && outCalendarId && (extractedEra || extractedEraYear)) {
        std::optional<StringView> era;
        if (extractedEra)
            era = StringView(*extractedEra);
        auto result = TemporalCore::calendarDateFromFields(
            TemporalCore::calendarIDFromString(*outCalendarId), static_cast<int32_t>(year),
            static_cast<uint8_t>(month), static_cast<uint8_t>(day),
            era, extractedEraYear, otherMonth, overflow);
        if (!result) {
            throwRangeError(globalObject, scope, String(result.error().message));
            return { };
        }
        return *result;
    }

    RELEASE_AND_RETURN(scope, isoDateFromFields(globalObject, format,
        static_cast<int32_t>(year), static_cast<unsigned>(month), static_cast<unsigned>(day),
        otherMonth, overflow, isNonISOCalendar && outCalendarId ? StringView(*outCalendarId) : StringView()));
}

// Overload taking raw JSValue options — reads fields BEFORE options per spec.
// Uses prepareDateFields (V8's PrepareCalendarFields equivalent) to read all
// fields first, then intlGetOptionsObject + overflow, then processes.
ISO8601::PlainDate isoDateFromFields(JSGlobalObject* globalObject, JSObject* temporalDateLike, TemporalDateFormat format, JSValue optionsValue, TemporalOverflow& overflow, String* outCalendarId)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Phase 1: Read all fields (PrepareCalendarFields).
    auto fields = prepareDateFields(globalObject, temporalDateLike, format);
    RETURN_IF_EXCEPTION(scope, { });
    if (!fields)
        return { };

    // Phase 2: GetOptionsObject + GetTemporalOverflowOption.
    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });
    overflow = TemporalOverflow::Constrain;
    if (options) {
        overflow = toTemporalOverflow(globalObject, options);
        RETURN_IF_EXCEPTION(scope, { });
    }

    // Phase 3: Resolve monthCode and process with overflow.
    bool isNonISO = !fields->calendarId.isNull() && fields->calendarId != "iso8601"_s;
    if (outCalendarId && isNonISO)
        *outCalendarId = fields->calendarId;

    double month = fields->month;
    if (fields->monthCodePresent) {
        ASSERT(fields->monthCode);
        if (!isNonISO) {
            if (fields->monthCode->monthNumber < 1 || fields->monthCode->monthNumber > 12 || fields->monthCode->isLeapMonth) {
                throwRangeError(globalObject, scope, "month code is not valid for ISO 8601 calendar"_s);
                return { };
            }
        }
        if (fields->monthUndefined)
            month = fields->monthCode->monthNumber;
        else if (fields->monthCode->monthNumber != month) {
            throwRangeError(globalObject, scope, "month and monthCode properties must match"_s);
            return { };
        }
    }

    StringView calId = isNonISO ? StringView(fields->calendarId) : StringView();
    if (outCalendarId && isNonISO)
        *outCalendarId = fields->calendarId;

    // For non-ISO calendars with era/eraYear, use calendarDateFromFields directly
    // to get proper era→year conversion via ICU.
    if (isNonISO && (fields->era || fields->eraYear)) {
        std::optional<StringView> era;
        if (fields->era)
            era = StringView(*fields->era);
        auto result = TemporalCore::calendarDateFromFields(
            TemporalCore::calendarIDFromString(calId), static_cast<int32_t>(fields->year), static_cast<uint8_t>(month),
            static_cast<uint8_t>(fields->day), era, fields->eraYear,
            fields->monthCode, overflow);
        if (!result) {
            throwRangeError(globalObject, scope, String(result.error().message));
            return { };
        }
        return *result;
    }

    // Validate month/day at double level before unsafe double→unsigned cast.
    // static_cast<unsigned> of values >= 2^32 is UB and wraps on x86 (gives 0).
    if (!isNonISO) {
        if (overflow == TemporalOverflow::Constrain) {
            month = std::clamp(month, 1.0, 12.0);
            double daysInMon = ISO8601::daysInMonth(static_cast<int32_t>(fields->year), static_cast<unsigned>(month));
            fields->day = std::clamp(fields->day, 1.0, daysInMon);
        } else {
            if (!(month >= 1 && month <= 12)) {
                throwRangeError(globalObject, scope, "month is out of range"_s);
                return { };
            }
        }
    }

    RELEASE_AND_RETURN(scope, isoDateFromFields(globalObject, format,
        static_cast<int32_t>(fields->year), static_cast<unsigned>(month),
        static_cast<unsigned>(fields->day), fields->monthCode, overflow, calId));
}

ISO8601::PlainDate isoDateFromFields(JSGlobalObject* globalObject, TemporalDateFormat format, int32_t year, unsigned month, unsigned day, std::optional<ParsedMonthCode> monthCode, TemporalOverflow overflow, StringView calendarId)
{

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    calendarResolveFields(globalObject, year, month, monthCode, format, calendarId);
    RETURN_IF_EXCEPTION(scope, { });

    ASSERT(month > 0);
    ASSERT(day > 0);

    bool isNonISO = !calendarId.isEmpty() && calendarId != "iso8601"_s;

    // For non-ISO calendars, use ICU to convert calendar fields → ISO date.
    if (isNonISO) {
        auto result = TemporalCore::calendarDateFromFields(
            TemporalCore::calendarIDFromString(calendarId), year, static_cast<uint8_t>(month), static_cast<uint8_t>(day),
            std::nullopt, std::nullopt, monthCode, overflow);
        if (!result) {
            throwRangeError(globalObject, scope, String(result.error().message));
            return { };
        }
        return *result;
    }

    if (overflow == TemporalOverflow::Constrain) {
        month = std::min<unsigned>(month, 12);
        day = std::min<unsigned>(day, ISO8601::daysInMonth(year, month));
    }

    auto plainDate = TemporalPlainDate::toPlainDate(globalObject, ISO8601::Duration(year, month, 0LL, day, 0LL, 0LL, 0LL, 0LL, Int128(0), Int128(0)));
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


// https://tc39.es/proposal-temporal/#sec-temporal-calendaryearmonthfromfields
ISO8601::PlainDate yearMonthFromFields(JSGlobalObject* globalObject, int32_t year, int32_t month, std::optional<ParsedMonthCode> monthCode, TemporalOverflow overflow, StringView calendarId)
{
    return isoDateFromFields(globalObject, TemporalDateFormat::YearMonth, year, month, 1, monthCode, overflow, calendarId);
}

// https://tc39.es/proposal-temporal/#sec-temporal-calendarmonthdayfromfields
ISO8601::PlainDate monthDayFromFields(JSGlobalObject* globalObject, std::optional<int32_t> referenceYear, unsigned month, unsigned day, std::optional<ParsedMonthCode> monthCode, TemporalOverflow overflow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    calendarResolveFields(globalObject, referenceYear, month, monthCode, TemporalDateFormat::MonthDay);
    RETURN_IF_EXCEPTION(scope, { });
    int32_t year = referenceYear.value_or(1972);
    auto result = TemporalDuration::regulateISODate(year, month, day, overflow);
    if (!result || !ISO8601::isDateTimeWithinLimits(result->year(), result->month(), result->day(), 12, 0, 0, 0, 0, 0)) [[unlikely]] {
        throwRangeError(globalObject, scope, "monthDayFromFields: date is out of range of ECMAScript representation"_s);
        return { };
    }
    return ISO8601::PlainDate(1972, result->month(), result->day());
}

// https://tc39.es/proposal-temporal/#sec-temporal-adddurationtodate
// AddDurationToDate ( operation, temporalDate, temporalDurationLike, options )
ISO8601::PlainDate addDurationToDate(JSGlobalObject* globalObject, const ISO8601::PlainDate& plainDate, const ISO8601::Duration& duration, TemporalOverflow overflow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto dateDuration = TemporalDuration::toDateDurationRecordWithoutTime(globalObject, duration);
    RETURN_IF_EXCEPTION(scope, { });
    RELEASE_AND_RETURN(scope, isoDateAdd(globalObject, plainDate, dateDuration, overflow));
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

ISO8601::PlainDate calendarDateAdd(JSGlobalObject* globalObject, StringView calendarId, const ISO8601::PlainDate& plainDate, const ISO8601::Duration& duration, TemporalOverflow overflow)
{
    if (calendarId == "iso8601"_s)
        return isoDateAdd(globalObject, plainDate, duration, overflow);
    auto result = TemporalCore::calendarDateAdd(TemporalCore::calendarIDFromString(calendarId), plainDate, duration, overflow);
    if (!result) [[unlikely]] {
        VM& vm = globalObject->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        throwRangeError(globalObject, scope, result.error().message);
        return { };
    }
    return *result;
}

ISO8601::Duration calendarDateUntil(StringView calendarId, const ISO8601::PlainDate& one, const ISO8601::PlainDate& two, TemporalUnit largestUnit)
{
    if (calendarId == "iso8601"_s)
        return TemporalCore::calendarDateUntil(one, two, largestUnit);
    auto result = TemporalCore::calendarDateUntil(TemporalCore::calendarIDFromString(calendarId), one, two, largestUnit);
    if (!result)
        return { };
    return *result;
}

// https://tc39.es/proposal-temporal/#sec-temporal-differencetemporalplainyearmonth
template<DifferenceOperation op>
ISO8601::Duration differenceTemporalPlainYearMonth(JSGlobalObject* globalObject, const ISO8601::PlainYearMonth& yearMonth, const ISO8601::PlainYearMonth& other, unsigned increment, TemporalUnit smallestUnit, TemporalUnit largestUnit, RoundingMode roundingMode, StringView calendarId)
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
    auto dateDiffResult = TemporalCore::differenceYearMonth(TemporalCore::calendarIDFromString(calendarId), thisDate, otherDate, largestUnit);
    ISO8601::Duration dateDifference;
    if (!dateDiffResult) {
        // Fallback to direct calendarDateUntil if differenceYearMonth fails.
        if (!calendarId.isEmpty() && calendarId != "iso8601"_s)
            dateDifference = calendarDateUntil(calendarId, thisDate, otherDate, largestUnit);
        else
            dateDifference = TemporalCore::calendarDateUntil(thisDate, otherDate, largestUnit);
    } else
        dateDifference = *dateDiffResult;
    auto duration = ISO8601::InternalDuration::combineDateAndTimeDuration(ISO8601::Duration { dateDifference.years(), dateDifference.months(), 0LL, 0LL, 0LL, 0LL, 0LL, 0LL, Int128(0), Int128(0) }, 0);

    if (smallestUnit != TemporalUnit::Month || increment != 1) {
        auto originEpochNs = getUTCEpochNanoseconds(TemporalDuration::combineISODateAndTimeRecord(thisDate, ISO8601::PlainTime()));
        auto isoDateTimeOther = TemporalDuration::combineISODateAndTimeRecord(otherDate, ISO8601::PlainTime());
        auto destEpochNs = getUTCEpochNanoseconds(isoDateTimeOther);
        auto roundResult = TemporalCore::roundRelativeDuration(duration, originEpochNs, destEpochNs, thisDate, ISO8601::PlainTime(), largestUnit, increment, smallestUnit, roundingMode, nullptr, TemporalCore::calendarIDFromString(calendarId));
        if (!roundResult) {
            throwTemporalError(globalObject, scope, roundResult.error());
            return { };
        }
    }
    auto result = TemporalDuration::temporalDurationFromInternal(duration, TemporalUnit::Day);
    if (op == DifferenceOperation::Since)
        result = -result;
    return result;
}

template ISO8601::Duration differenceTemporalPlainYearMonth<DifferenceOperation::Since>(JSGlobalObject*, const ISO8601::PlainYearMonth&, const ISO8601::PlainYearMonth&, unsigned, TemporalUnit, TemporalUnit, RoundingMode, StringView);
template ISO8601::Duration differenceTemporalPlainYearMonth<DifferenceOperation::Until>(JSGlobalObject*, const ISO8601::PlainYearMonth&, const ISO8601::PlainYearMonth&, unsigned, TemporalUnit, TemporalUnit, RoundingMode, StringView);

} // namespace JSC
