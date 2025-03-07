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

#include "DateConstructor.h"
#include "JSObjectInlines.h"
#include "LazyPropertyInlines.h"
#include "ParseInt.h"
#include "StructureInlines.h"
#include "TemporalDuration.h"
#include "TemporalPlainDate.h"
#include "TemporalPlainDateTime.h"
#include "TemporalPlainMonthDay.h"
#include "TemporalPlainTime.h"
#include "TemporalPlainYearMonth.h"
#include "TemporalTimeZone.h"
#include "TemporalZonedDateTime.h"

namespace JSC {

const ClassInfo TemporalCalendar::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(TemporalCalendar) };

TemporalCalendar* TemporalCalendar::create(VM& vm, Structure* structure, CalendarID identifier)
{
    TemporalCalendar* format = new (NotNull, allocateCell<TemporalCalendar>(vm)) TemporalCalendar(vm, structure, identifier);
    format->finishCreation(vm);
    return format;
}

Structure* TemporalCalendar::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalCalendar::TemporalCalendar(VM& vm, Structure* structure, CalendarID identifier)
    : Base(vm, structure)
    , m_identifier(identifier)
{
}

// https://tc39.es/proposal-temporal/#sec-temporal-parsetemporalcalendarstring
static std::optional<CalendarID> parseTemporalCalendarString(JSGlobalObject* globalObject, StringView string)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto parseResult = ISO8601::parseCalendarDateTime(string, TemporalDateFormat::Date);
    std::optional<ISO8601::CalendarRecord> calendarParseResult;
    if (parseResult)
        calendarParseResult = std::get<3>(parseResult.value());
    else {
        parseResult = ISO8601::parseCalendarDateTime(string, TemporalDateFormat::YearMonth);
        if (parseResult)
            calendarParseResult = std::get<3>(parseResult.value());
        else {
            parseResult = ISO8601::parseCalendarDateTime(string, TemporalDateFormat::MonthDay);
            if (parseResult)
                calendarParseResult = std::get<3>(parseResult.value());
            else {
                calendarParseResult = ISO8601::parseCalendarName(string);
                if (!calendarParseResult) {
                    throwRangeError(globalObject, scope, "invalid calendar ID"_s);
                    return std::nullopt;
                }
            }
        }
    }
    if (!calendarParseResult)
        return iso8601CalendarID();
    if (WTF::String(calendarParseResult->m_name).convertToASCIILowercase() == "iso8601"_s)
        return iso8601CalendarID();

    throwRangeError(globalObject, scope, "calendar ID not supported yet"_s);
    return std::nullopt;
}

JSObject* TemporalCalendar::toTemporalCalendarWithISODefault(JSGlobalObject* globalObject, JSValue temporalCalendarLike)
{
    if (temporalCalendarLike.isUndefined())
        return TemporalCalendar::create(globalObject->vm(), globalObject->calendarStructure(), iso8601CalendarID());
    return TemporalCalendar::from(globalObject, temporalCalendarLike);
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporalcalendaridentifier
CalendarID TemporalCalendar::toTemporalCalendarIdentifier(JSGlobalObject* globalObject, JSValue temporalCalendarLike)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (temporalCalendarLike.inherits<TemporalPlainDate>())
        return jsCast<TemporalPlainDate*>(temporalCalendarLike)->calendar()->identifier();

    if (temporalCalendarLike.inherits<TemporalPlainDateTime>())
        return jsCast<TemporalPlainDateTime*>(temporalCalendarLike)->calendar()->identifier();

    if (temporalCalendarLike.inherits<TemporalPlainTime>())
        return jsCast<TemporalPlainTime*>(temporalCalendarLike)->calendar()->identifier();

    if (temporalCalendarLike.inherits<TemporalPlainMonthDay>())
        return jsCast<TemporalPlainMonthDay*>(temporalCalendarLike)->calendar()->identifier();

    if (temporalCalendarLike.inherits<TemporalPlainYearMonth>())
        return jsCast<TemporalPlainYearMonth*>(temporalCalendarLike)->calendar()->identifier();

    if (temporalCalendarLike.inherits<TemporalZonedDateTime>())
        return jsCast<TemporalZonedDateTime*>(temporalCalendarLike)->calendar()->identifier();

    if (!temporalCalendarLike.isString()) {
        throwTypeError(globalObject, scope, "calendar must be a string"_s);
        return { };
    }

    auto identifier = temporalCalendarLike.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    std::optional<CalendarID> calendarId = isBuiltinCalendar(identifier);
    if (!calendarId) {
        calendarId = parseTemporalCalendarString(globalObject, identifier);
        RETURN_IF_EXCEPTION(scope, { });
    }

    ASSERT(calendarId);

    // TODO: CanonicalizeCalendar
    return calendarId.value();
}

// https://tc39.es/proposal-temporal/#sec-temporal-gettemporalcalendarslotvaluewithisodefault
JSObject* TemporalCalendar::getTemporalCalendarWithISODefault(JSGlobalObject* globalObject, JSValue itemValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (itemValue.inherits<TemporalPlainDate>())
        return jsCast<TemporalPlainDate*>(itemValue)->calendar();

    if (itemValue.inherits<TemporalPlainDateTime>())
        return jsCast<TemporalPlainDateTime*>(itemValue)->calendar();

    if (itemValue.inherits<TemporalPlainTime>())
        return jsCast<TemporalPlainTime*>(itemValue)->calendar();

    JSValue calendar = itemValue.get(globalObject, vm.propertyNames->calendar);
    RETURN_IF_EXCEPTION(scope, { });
    RELEASE_AND_RETURN(scope, toTemporalCalendarWithISODefault(globalObject, calendar));
}

// https://tc39.es/proposal-temporal/#sec-temporal-gettemporalcalendarslotvaluewithisodefault
CalendarID TemporalCalendar::getTemporalCalendarIdentifierWithISODefault(JSGlobalObject* globalObject, JSValue itemValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (itemValue.inherits<TemporalPlainDate>())
        return jsCast<TemporalPlainDate*>(itemValue)->calendar()->identifier();

    if (itemValue.inherits<TemporalPlainDateTime>())
        return jsCast<TemporalPlainDateTime*>(itemValue)->calendar()->identifier();

    if (itemValue.inherits<TemporalPlainTime>())
        return jsCast<TemporalPlainTime*>(itemValue)->calendar()->identifier();

    if (itemValue.inherits<TemporalPlainMonthDay>())
        return jsCast<TemporalPlainMonthDay*>(itemValue)->calendar()->identifier();

    if (itemValue.inherits<TemporalPlainYearMonth>())
        return jsCast<TemporalPlainYearMonth*>(itemValue)->calendar()->identifier();

    if (itemValue.inherits<TemporalZonedDateTime>())
        return jsCast<TemporalZonedDateTime*>(itemValue)->calendar()->identifier();

    JSValue calendar = itemValue.get(globalObject, vm.propertyNames->calendar);
    RETURN_IF_EXCEPTION(scope, { });
    if (calendar.isUndefined())
        return iso8601CalendarID();
    RELEASE_AND_RETURN(scope, toTemporalCalendarIdentifier(globalObject, calendar));
}

std::optional<CalendarID> TemporalCalendar::isBuiltinCalendar(StringView string)
{
    const auto& calendars = intlAvailableCalendars();
    for (unsigned index = 0; index < calendars.size(); ++index) {
        if (calendars[index] == string)
            return index;
    }
    return std::nullopt;
}

// https://tc39.es/ecma262/#sec-hourfromtime
static double hourFromTime(double t)
{
    double result = std::trunc(std::fmod(std::floor(t / msPerHour), WTF::hoursPerDay));
    if (result < 0)
        result += WTF::hoursPerDay;
    return result;
}

// https://tc39.es/ecma262/#sec-minfromtime
static double minFromTime(double t)
{
    double result = std::trunc(std::fmod(std::floor(t / msPerMinute), minutesPerHour));
    if (result < 0)
        result += minutesPerHour;
    return result;
}

// https://tc39.es/ecma262/#sec-secfromtime
static double secFromTime(double t)
{
    double result = std::trunc(std::fmod(std::floor(t / msPerSecond), secondsPerMinute));
    if (result < 0)
        result += secondsPerMinute;
    return result;
}

// https://tc39.es/ecma262/#sec-msfromtime
static double msFromTime(double t)
{
    double result = std::fmod(t, msPerSecond);
    if (result < 0)
        result += msPerSecond;
    return std::trunc(result);
}

// https://tc39.es/proposal-temporal/#sec-temporal-getisopartsfromepoch
ISO8601::PlainDateTime TemporalCalendar::getISOPartsFromEpoch(ISO8601::ExactTime epochNanoseconds)
{
    ASSERT(epochNanoseconds.isValid());
    Int128 remainderNs = epochNanoseconds.epochNanoseconds() % 1000000;
    if (remainderNs < 0)
        remainderNs += 1000000;
    auto epochMilliseconds = (epochNanoseconds.epochNanoseconds() - remainderNs) / 1000000;
    auto year = TemporalCalendar::epochTimeToEpochYear(epochMilliseconds);
    auto month = TemporalCalendar::epochTimeToMonthInYear(epochMilliseconds) + 1;
    auto day = TemporalCalendar::epochTimeToDate(epochMilliseconds);
    auto hour = hourFromTime(epochMilliseconds);
    auto minute = minFromTime(epochMilliseconds);
    auto second = secFromTime(epochMilliseconds);
    auto millisecond = msFromTime(epochMilliseconds);
    auto microsecond = remainderNs / 1000;
    ASSERT(microsecond < 1000);
    auto nanosecond = remainderNs % 1000;
    auto isoDate = ISO8601::PlainDate(year, month, day);
    auto time = ISO8601::PlainTime(hour, minute, second, millisecond, microsecond, nanosecond);
    return ISO8601::PlainDateTime(WTFMove(isoDate), WTFMove(time));
}

// https://tc39.es/proposal-temporal/#sec-temporal-formatcalendarannotation
String TemporalCalendar::formatCalendarAnnotation(TemporalShowCalendar showCalendar)
{
    // TODO: non-iso8601 calendars
    switch (showCalendar) {
    case TemporalShowCalendar::Never:
        return ""_s;
    case TemporalShowCalendar::Auto:
        return ""_s;
    case TemporalShowCalendar::Critical:
        return "[!u-ca=iso8601]"_s;
    case TemporalShowCalendar::Always:
        return "[u-ca=iso8601]"_s;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporalcalendar
JSObject* TemporalCalendar::from(JSGlobalObject* globalObject, JSValue calendarLike)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (calendarLike.isObject()) {
        if (calendarLike.inherits<TemporalPlainDate>())
            return jsCast<TemporalPlainDate*>(calendarLike)->calendar();

        if (calendarLike.inherits<TemporalPlainDateTime>())
            return jsCast<TemporalPlainDateTime*>(calendarLike)->calendar();

        if (calendarLike.inherits<TemporalPlainTime>())
            return jsCast<TemporalPlainTime*>(calendarLike)->calendar();

        if (calendarLike.inherits<TemporalPlainMonthDay>())
            return jsCast<TemporalPlainMonthDay*>(calendarLike)->calendar();

        if (calendarLike.inherits<TemporalPlainYearMonth>())
            return jsCast<TemporalPlainYearMonth*>(calendarLike)->calendar();

        if (calendarLike.inherits<TemporalZonedDateTime>())
            return jsCast<TemporalZonedDateTime*>(calendarLike)->calendar();

        JSObject* calendarLikeObject = jsCast<JSObject*>(calendarLike);
        bool hasProperty = calendarLikeObject->hasProperty(globalObject, vm.propertyNames->calendar);
        RETURN_IF_EXCEPTION(scope, { });
        if (!hasProperty)
            return jsCast<JSObject*>(calendarLike);

        calendarLike = calendarLikeObject->get(globalObject, vm.propertyNames->calendar);
        if (calendarLike.isObject()) {
            bool hasProperty = jsCast<JSObject*>(calendarLike)->hasProperty(globalObject, vm.propertyNames->calendar);
            RETURN_IF_EXCEPTION(scope, { });
            if (!hasProperty)
                return jsCast<JSObject*>(calendarLike);
        }
    }

    if (!calendarLike.isString()) {
        throwTypeError(globalObject, scope, "calendar must be a string"_s);
        return { };
    }
    auto identifier = calendarLike.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    std::optional<CalendarID> calendarId = isBuiltinCalendar(identifier);
    if (!calendarId) {
        calendarId = parseTemporalCalendarString(globalObject, identifier);
        RETURN_IF_EXCEPTION(scope, { });
    }

    ASSERT(calendarId);
    return TemporalCalendar::create(vm, globalObject->calendarStructure(), calendarId.value());
}

// https://tc39.es/proposal-temporal/#sec-temporal-isodatefromfields
ISO8601::PlainDate TemporalCalendar::isoDateFromFields(JSGlobalObject* globalObject, JSObject* temporalDateLike, TemporalDateFormat format, std::variant<JSObject*, TemporalOverflow> optionsOrOverflow, TemporalOverflow& overflow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    double day = 1;
    if (format != TemporalDateFormat::YearMonth) {
        JSValue dayProperty = temporalDateLike->get(globalObject, vm.propertyNames->day);
        RETURN_IF_EXCEPTION(scope, { });

        if (dayProperty.isUndefined()) {
            throwTypeError(globalObject, scope, "day property must be present"_s);
            return { };
        }

        if (!dayProperty.isUndefined()) {
            day = dayProperty.toIntegerOrInfinity(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            if (!(day > 0 && std::isfinite(day))) {
                throwRangeError(globalObject, scope, "day property must be positive and finite"_s);
                return { };
            }
        }
    }

    JSValue monthProperty = temporalDateLike->get(globalObject, vm.propertyNames->month);
    RETURN_IF_EXCEPTION(scope, { });
    double month = 0;
    if (!monthProperty.isUndefined()) {
        month = monthProperty.toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
    }

    JSValue monthCodeProperty = temporalDateLike->get(globalObject, vm.propertyNames->monthCode);
    RETURN_IF_EXCEPTION(scope, { });
    std::optional<WTF::String> monthCode;
    if (monthCodeProperty.isUndefined()) {
        if (monthProperty.isUndefined()) {
            throwTypeError(globalObject, scope, "Either month or monthCode property must be provided"_s);
            return { };
        }

        if (!(month > 0 && std::isfinite(month))) {
            throwRangeError(globalObject, scope, "month property must be positive and finite"_s);
            return { };
        }
    } else {
        monthCode = monthCodeProperty.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        if (!ISO8601::validMonthCode(monthCode.value())) {
            throwRangeError(globalObject, scope, "Invalid monthCode property"_s);
            return { };
        }
    }

    double year = 1972; // Default reference year for MonthDay
    JSValue yearProperty = temporalDateLike->get(globalObject, vm.propertyNames->year);
    RETURN_IF_EXCEPTION(scope, { });

    if (format != TemporalDateFormat::MonthDay) {
        if (yearProperty.isUndefined()) {
            throwTypeError(globalObject, scope, "year property must be present"_s);
            return { };
        }
    }

    if (!yearProperty.isUndefined()) {
        year = yearProperty.toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        if (!std::isfinite(year)) {
            throwRangeError(globalObject, scope, "year property must be finite"_s);
            return { };
        }
    }

    // Parse monthCode if applicable
    double otherMonth = 0;
    if (monthCode) {
        otherMonth = ISO8601::monthFromCode(monthCode.value());

        // TODO: ISO8601 calendar assumed
        if (otherMonth < 1 || otherMonth > 12 || monthCode->length() == 4) {
            throwRangeError(globalObject, scope, "month code is not valid for ISO 8601 calendar"_s);
            return { };
        }

        if (monthProperty.isUndefined())
            month = otherMonth;
        else if (otherMonth != month) {
            throwRangeError(globalObject, scope, "month and monthCode properties must match if both are provided"_s);
            return { };
        }
    }

    if (std::holds_alternative<TemporalOverflow>(optionsOrOverflow))
        overflow = std::get<TemporalOverflow>(optionsOrOverflow);
    else {
        overflow = toTemporalOverflow(globalObject, std::get<JSObject*>(optionsOrOverflow));
        RETURN_IF_EXCEPTION(scope, { });
    }
    RELEASE_AND_RETURN(scope, isoDateFromFields(globalObject, format, year, month, day, overflow));
}

ISO8601::PlainDate TemporalCalendar::isoDateFromFields(JSGlobalObject* globalObject, TemporalDateFormat format, double year, double month, double day, TemporalOverflow overflow)
{
    ASSERT(isInteger(year));
    ASSERT(isInteger(month) && month > 0);
    ASSERT(isInteger(day) && day > 0);

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (overflow == TemporalOverflow::Constrain) {
        month = std::min<unsigned>(month, 12);
        day = std::min<unsigned>(day, ISO8601::daysInMonth(year, month));
    }

    auto plainDate = TemporalPlainDate::toPlainDate(globalObject, ISO8601::Duration(year, month, 0, day, 0, 0, 0, 0, 0, 0));
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

    if (!valid) {
        throwRangeError(globalObject, scope, "date time is out of range of ECMAScript representation"_s);
        return { };
    }

    return plainDate;
}

// https://tc39.es/proposal-temporal/#sec-temporal-calendaryearmonthfromfields
ISO8601::PlainDate TemporalCalendar::yearMonthFromFields(JSGlobalObject* globalObject, double year, double month, TemporalOverflow overflow)
{
    // 2. Let firstDayIndex be the 1-based index of the first day of the month described by fields
    // (i.e., 1 unless the month's first day is skipped by this calendar.)
    return isoDateFromFields(globalObject, TemporalDateFormat::YearMonth, year, month, 1, overflow);
}

// https://tc39.es/proposal-temporal/#sec-temporal-calendarmonthdayfromfields
// https://tc39.es/proposal-temporal/#sec-temporal-calendarmonthdaytoisoreferencedate
ISO8601::PlainDate TemporalCalendar::monthDayFromFields(JSGlobalObject* globalObject, std::optional<double> referenceYear, double month, double day, TemporalOverflow overflow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    double year = referenceYear.value_or(1972);
    auto plainDateOptional = TemporalPlainDate::regulateISODate(year, month, day, overflow);
    if (!plainDateOptional) {
        throwRangeError(globalObject, scope, "monthDayFromFields: date is out of range of ECMAScript representation"_s);
        return { };
    }
    auto plainDate = plainDateOptional.value();
    return isoDateFromFields(globalObject, TemporalDateFormat::MonthDay, plainDate.year(), plainDate.month(), plainDate.day(), overflow);
}

static PropertyName propertyName(VM& vm, FieldName property)
{
    switch (property) {
    case FieldName::Year:
        return vm.propertyNames->year;
    case FieldName::Month:
        return vm.propertyNames->month;
    case FieldName::MonthCode:
        return vm.propertyNames->monthCode;
    case FieldName::Day:
        return vm.propertyNames->day;
    case FieldName::Hour:
        return vm.propertyNames->hour;
    case FieldName::Minute:
        return vm.propertyNames->minute;
    case FieldName::Second:
        return vm.propertyNames->second;
    case FieldName::Millisecond:
        return vm.propertyNames->millisecond;
    case FieldName::Microsecond:
        return vm.propertyNames->microsecond;
    case FieldName::Nanosecond:
        return vm.propertyNames->nanosecond;
    case FieldName::Calendar:
        return vm.propertyNames->calendar;
    case FieldName::Offset:
        return vm.propertyNames->offset;
    case FieldName::TimeZone:
        return vm.propertyNames->timeZone;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

static int32_t toIntegerWithTruncation(JSGlobalObject* globalObject, JSValue argument)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto number = argument.toIntegerOrInfinity(globalObject);
    if (!std::isfinite(number)) {
        throwRangeError(globalObject, scope, "Temporal properties must be finite"_s);
        return { };
    }
    return number;
}

static unsigned toPositiveIntegerWithTruncation(JSGlobalObject* globalObject, JSValue argument)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    int32_t integer = toIntegerWithTruncation(globalObject, argument);
    RETURN_IF_EXCEPTION(scope, { });
    if (integer <= 0) {
        throwRangeError(globalObject, scope, "Temporal property must be a positive integer"_s);
        return { };
    }
    return integer;
}

static double monthCodeToMonth(String monthCode)
{
    ASSERT(monthCode.length() >= 3 && monthCode.length() <= 4);
    ASSERT(monthCode[0] == 'M');
    ASSERT(isASCIIDigit(monthCode[1]) && isASCIIDigit(monthCode[2]));
    auto monthCodeInteger = parseInt(monthCode.substring(1, 3).span8(), 10);
    return monthCodeInteger;
}

// https://tc39.es/proposal-temporal/#sec-temporal-tomonthcode
static String toMonthCode(JSGlobalObject* globalObject, JSValue argument)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto monthCodeVal = argument.toPrimitive(globalObject, PreferString);
    RETURN_IF_EXCEPTION(scope, { });
    if (!monthCodeVal.isString()) {
        throwTypeError(globalObject, scope, "month code must be a string"_s);
        return { };
    }
    String monthCode = monthCodeVal.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    if (monthCode.length() < 3 || monthCode.length() > 4) {
        throwRangeError(globalObject, scope, "length of month code must be 3 or 4"_s);
        return { };
    }
    if (monthCode[0] != 'M') {
        throwRangeError(globalObject, scope, "month code must begin with 'M'"_s);
        return { };
    }
    if (monthCode[1] < '0' || monthCode[1] > '9' || monthCode[2] < '0' || monthCode[2] > '9') {
        throwRangeError(globalObject, scope, "digits expected in month code"_s);
        return { };
    }
    if (monthCode.length() == 4 && monthCode[3] != 'L') {
        throwRangeError(globalObject, scope, "monthCode must end in 'L' if length is 4"_s);
        return { };
    }
    auto monthCodeInteger = parseInt(monthCode.substring(1, 3).span8(), 10);
    if (!monthCodeInteger && monthCode.length() != 4) {
        throwRangeError(globalObject, scope, "monthCode cannot be 0 if last character is not 'L'"_s);
        return { };
    }
    return monthCode;
}

// https://tc39.es/proposal-temporal/#sec-temporal-tooffsetstring
static String toOffsetString(JSGlobalObject* globalObject, JSValue argument)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto offsetVal = argument.toPrimitive(globalObject, PreferString);
    RETURN_IF_EXCEPTION(scope, { });
    if (!offsetVal.isString()) {
        throwTypeError(globalObject, scope, "offset must be a string"_s);
        return { };
    }
    String offset = offsetVal.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    Vector<LChar> ignore;
    if (!ISO8601::parseUTCOffset(offset, ignore, true)) {
        throwRangeError(globalObject, scope, makeString("error parsing offset string "_s, offset));
        return { };
    }
    return offset;
}

std::tuple<std::optional<double>, std::optional<double>, std::optional<String>, std::optional<double>,
std::optional<double>, std::optional<double>, std::optional<double>, std::optional<double>,
std::optional<double>, std::optional<double>, std::optional<String>, std::optional<ISO8601::TimeZone>>
TemporalCalendar::prepareCalendarFields(JSGlobalObject* globalObject, CalendarID calendar, JSObject* fields,
    Vector<FieldName> fieldNames, std::optional<Vector<FieldName>> requiredFieldNames)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // TODO: non-iso8601 calendars
    (void) calendar;
    // auto extraFieldNames = calendarExtraFields(calendar, calendarFieldNames);
    // fieldNames.append(extraFieldNames);

    std::optional<double> yearOptional;
    std::optional<unsigned> monthOptional;
    std::optional<String> monthCodeOptional;
    std::optional<unsigned> dayOptional;
    std::optional<double> hourOptional;
    std::optional<double> minuteOptional;
    std::optional<double> secondOptional;
    std::optional<double> millisecondOptional;
    std::optional<double> microsecondOptional;
    std::optional<double> nanosecondOptional;
    std::optional<String> offsetOptional;
    std::optional<ISO8601::TimeZone> timeZoneOptional;
    auto any = false;

    for (auto property : fieldNames) {
        auto value = fields->get(globalObject, propertyName(vm, property));
        RETURN_IF_EXCEPTION(scope, { });
        if (!value.isUndefined()) {
            any = true;
            switch (property) {
            case FieldName::Year: {
                double val = toIntegerWithTruncation(globalObject, value);
                RETURN_IF_EXCEPTION(scope, { });
                yearOptional = val;
                break;
            }
            case FieldName::Month: {
                unsigned val = toPositiveIntegerWithTruncation(globalObject, value);
                RETURN_IF_EXCEPTION(scope, { });
                monthOptional = val;
                break;
            }
            case FieldName::MonthCode: {
                String val = toMonthCode(globalObject, value);
                RETURN_IF_EXCEPTION(scope, { });
                monthCodeOptional = val;
                if (!monthOptional)
                    monthOptional = monthCodeToMonth(val);
                break;
            }
            case FieldName::Day: {
                unsigned val = toPositiveIntegerWithTruncation(globalObject, value);
                RETURN_IF_EXCEPTION(scope, { });
                dayOptional = val;
                break;
            }
            case FieldName::Hour: {
                double val = toIntegerWithTruncation(globalObject, value);
                RETURN_IF_EXCEPTION(scope, { });
                hourOptional = val;
                break;
            }
            case FieldName::Minute: {
                double val = toIntegerWithTruncation(globalObject, value);
                RETURN_IF_EXCEPTION(scope, { });
                minuteOptional = val;
                break;
            }
            case FieldName::Second: {
                double val = toIntegerWithTruncation(globalObject, value);
                RETURN_IF_EXCEPTION(scope, { });
                secondOptional = val;
                break;
            }
            case FieldName::Millisecond: {
                double val = toIntegerWithTruncation(globalObject, value);
                RETURN_IF_EXCEPTION(scope, { });
                millisecondOptional = val;
                break;
            }
            case FieldName::Microsecond: {
                double val = toIntegerWithTruncation(globalObject, value);
                RETURN_IF_EXCEPTION(scope, { });
                microsecondOptional = val;
                break;
            }
            case FieldName::Nanosecond: {
                double val = toIntegerWithTruncation(globalObject, value);
                RETURN_IF_EXCEPTION(scope, { });
                nanosecondOptional = val;
                break;
            }
            case FieldName::Offset: {
                String val = toOffsetString(globalObject, value);
                RETURN_IF_EXCEPTION(scope, { });
                offsetOptional = val;
                break;
            }
            case FieldName::TimeZone: {
                ISO8601::TimeZone val = TemporalTimeZone::toTemporalTimeZoneIdentifier(globalObject, value);
                RETURN_IF_EXCEPTION(scope, { });
                timeZoneOptional = val;
                break;
            }
            case FieldName::Calendar: {
                String val = value.toWTFString(globalObject);
                // TODO: implement non-ISO8601 calendars (currently string is ignored)
                RETURN_IF_EXCEPTION(scope, { });
                break;
            }
            }
        } else {
            if (requiredFieldNames && requiredFieldNames->contains(property)) {
                throwTypeError(globalObject, scope, "prepareCalendarFields: missing required property name"_s);
                return { };
            }
        }
    }
    if (!requiredFieldNames && !any) {
        throwTypeError(globalObject, scope, "prepareCalendarFields: at least one Temporal property must be given"_s);
        return { };
    }
    return { yearOptional, monthOptional, monthCodeOptional, dayOptional,
        hourOptional, minuteOptional, secondOptional, millisecondOptional, microsecondOptional,
        nanosecondOptional, offsetOptional, timeZoneOptional };
}

// https://tc39.es/proposal-temporal/#sec-temporal-calendarresolvefields
static void calendarResolveFields(JSGlobalObject* globalObject, CalendarID calendar, std::optional<double> optionalYear,
    std::optional<double> optionalMonth, std::optional<String> optionalMonthCode, std::optional<double> optionalDay,
    double& month, TemporalDateFormat format)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (calendar == iso8601CalendarID()) {
        if ((format == TemporalDateFormat::Date || format == TemporalDateFormat::YearMonth) && !optionalYear) {
            throwTypeError(globalObject, scope, "year property missing in Temporal.ZonedDateTime.from"_s);
            return;
        }
        if ((format == TemporalDateFormat::Date || format == TemporalDateFormat::MonthDay) && !optionalDay) {
            throwTypeError(globalObject, scope, "day property missing in Temporal.ZonedDateTime.from"_s);
            return;
        }
        if (!optionalMonthCode && !optionalMonth) {
            throwTypeError(globalObject, scope, "month and monthCode properties missing in Temporal.ZonedDateTime.from"_s);
            return;
        }
        if (!optionalMonthCode) {
            month = optionalMonth.value();
            return;
        }
        auto monthCode = optionalMonthCode.value();
        if (monthCode.length() != 3) {
            throwRangeError(globalObject, scope, "invalid month code in Temporal.ZonedDateTime.from"_s);
            return;
        }
        if (monthCode[0] != 'M') {
            throwRangeError(globalObject, scope, "invalid month code in Temporal.ZonedDateTime.from (must begin with 'M')"_s);
            return;
        }
        auto monthCodeInteger = parseInt(monthCode.substring(1, 3).span8(), 10);
        if (optionalMonth && optionalMonth.value() != monthCodeInteger) {
            throwRangeError(globalObject, scope, "month and monthCode properties disagree in Temporal.ZonedDateTime.from"_s);
            return;
        }
        if (monthCodeInteger < 1 || monthCodeInteger > 12) {
            throwRangeError(globalObject, scope, makeString("invalid month code: "_s, monthCode));
            return;
        }
        month = monthCodeInteger;
        return;
    }
    throwRangeError(globalObject, scope, "non-ISO8601 calendars not supported yet"_s);
    return;
}

// https://tc39.es/proposal-temporal/#sec-temporal-calendardatetoiso
ISO8601::PlainDate calendarDateToISO(JSGlobalObject* globalObject, CalendarID calendar,
    std::optional<double> optionalYear,
    std::optional<double> optionalMonth, std::optional<double> optionalDay,
    TemporalOverflow overflow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (calendar == iso8601CalendarID()) {
        ASSERT(optionalYear && optionalMonth && optionalDay);
        auto result = TemporalPlainDate::regulateISODate(optionalYear.value(),
            optionalMonth.value(), optionalDay.value(), overflow);
        if (!result) {
            throwRangeError(globalObject, scope, "invalid date in calendarDateToISO"_s);
            return { };
        }
        return result.value();
    }
    throwRangeError(globalObject, scope, "non-ISO8601 calendars not supported yet"_s);
    return { };
}

// https://tc39.es/proposal-temporal/#sec-temporal-calendardatefromfields
ISO8601::PlainDate calendarDateFromFields(JSGlobalObject* globalObject,
    CalendarID calendar, std::optional<double> optionalYear,
    std::optional<double> optionalMonth, std::optional<String> optionalMonthCode, std::optional<double> optionalDay,
    TemporalOverflow overflow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    double month = 0;
    calendarResolveFields(globalObject, calendar, optionalYear, optionalMonth, optionalMonthCode, optionalDay, month, TemporalDateFormat::Date);
    RETURN_IF_EXCEPTION(scope, { });
    auto result = calendarDateToISO(globalObject, calendar, optionalYear, month, optionalDay, overflow);
    RETURN_IF_EXCEPTION(scope, { });
    if (!isoDateWithinLimits(result)) {
        throwRangeError(globalObject, scope, "in calendarDateFromFields, date is out of range"_s);
        return { };
    }
    return result;
}

// https://tc39.es/proposal-temporal/#sec-temporal-interprettemporaldatetimefields
ISO8601::PlainDateTime
TemporalCalendar::interpretTemporalDateTimeFields(JSGlobalObject* globalObject, CalendarID calendar,
    std::optional<double> optionalYear, std::optional<double> optionalMonth, std::optional<String> optionalMonthCode,
    std::optional<double> optionalDay, double hour, double minute, double second, double millisecond,
    double microsecond, double nanosecond, TemporalOverflow overflow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto isoDate = calendarDateFromFields(globalObject, calendar, optionalYear, optionalMonth, optionalMonthCode, optionalDay, overflow);
    RETURN_IF_EXCEPTION(scope, { });
    auto time = TemporalPlainTime::regulateTime(globalObject, ISO8601::Duration { 0, 0, 0, 0, hour, minute, second, millisecond, microsecond, nanosecond }, overflow);
    RETURN_IF_EXCEPTION(scope, { });
    return TemporalPlainDateTime::combineISODateAndTimeRecord(isoDate, time);
}

// https://tc39.es/proposal-temporal/#sec-temporal-balanceisodate
ISO8601::PlainDate TemporalCalendar::balanceISODate(double year, double month, double day)
{
    auto epochDays = makeDay(year, month - 1, day);
    double ms = makeDate(epochDays, 0);
    int32_t y = TemporalCalendar::epochTimeToEpochYear(ms);
    int32_t m = TemporalCalendar::epochTimeToMonthInYear(ms) + 1;
    int32_t d = std::trunc(TemporalCalendar::epochTimeToDate(ms));
    return ISO8601::PlainDate { y, (unsigned) m, (unsigned) d };
}

// https://tc39.es/proposal-temporal/#sec-temporal-adddurationtodate
// AddDurationToDate ( operation, temporalDate, temporalDurationLike, options )
ISO8601::PlainDate TemporalCalendar::addDurationToDate(JSGlobalObject* globalObject, const ISO8601::PlainDate& plainDate, const ISO8601::Duration& duration, TemporalOverflow overflow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto dateDuration = TemporalDuration::toDateDurationRecordWithoutTime(globalObject, duration);
    RETURN_IF_EXCEPTION(scope, { });
    RELEASE_AND_RETURN(scope, isoDateAdd(globalObject, plainDate, dateDuration, overflow));
}

// https://tc39.es/proposal-temporal/#sec-temporal-calendardateadd
ISO8601::PlainDate TemporalCalendar::isoDateAdd(JSGlobalObject* globalObject, const ISO8601::PlainDate& plainDate, const ISO8601::Duration& duration, TemporalOverflow overflow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    double years = plainDate.year() + duration.years();
    double months = plainDate.month() + duration.months();
    double days = plainDate.day();
    ISO8601::PlainYearMonth intermediate = balanceISOYearMonth(years, months);
    std::optional<ISO8601::PlainDate> intermediate1 = TemporalPlainDate::regulateISODate(intermediate.year(),
        intermediate.month(), days, overflow);
    if (!intermediate1) {
        throwRangeError(globalObject, scope, "date time is out of range of ECMAScript representation"_s);
        return { };
    }
    auto d = intermediate1.value().day() + duration.days() + (7 * duration.weeks());
    auto result = balanceISODate(intermediate1.value().year(), intermediate1.value().month(), d);
    if (!ISO8601::isDateTimeWithinLimits(result.year(), result.month(), result.day(), 12, 0, 0, 0, 0, 0)) {
        throwRangeError(globalObject, scope, "date time is out of range of ECMAScript representation"_s);
        return { };
    }
    return result;
}

static ISO8601::Duration dateDuration(double y, double m, double w, double d)
{
    return ISO8601::Duration { y, m, w, d, 0, 0, 0, 0, 0, 0 };
}

static bool isoDateSurpasses(int32_t sign, double y1, double m1, double d1, const ISO8601::PlainDate& isoDate2)
{
    if (y1 != isoDate2.year()) {
        if (sign * (y1 - isoDate2.year()) > 0)
            return true;
    } else if (m1 != isoDate2.month()) {
        if (sign * (m1 - isoDate2.month()) > 0)
            return true;
    } else if (d1 != isoDate2.day()) {
        if (sign * (d1 - isoDate2.day()) > 0)
            return true;
    }
    return false;
}

// https://tc39.es/proposal-temporal/#sec-temporal-balanceisoyearmonth
ISO8601::PlainYearMonth TemporalCalendar::balanceISOYearMonth(double year, double month)
{
    year += std::floor((month - 1) / 12);
    // ECMA modulo operator always results in same sign as y in x mod y
    month = std::fmod(month - 1, 12) + 1;
    if (month < 1)
        month += 12;
    return ISO8601::PlainYearMonth(year, month);
}

// https://tc39.es/proposal-temporal/#sec-temporal-calendardateuntil
// CalendarDateUntil ( calendar, one, two, largestUnit )
ISO8601::Duration TemporalCalendar::calendarDateUntil(const ISO8601::PlainDate& one, const ISO8601::PlainDate& two, TemporalUnit largestUnit)
{
    auto sign = -1 * isoDateCompare(one, two);
    if (!sign)
        return { };

// Follows polyfill rather than spec, for practicality reasons (avoiding the loop
// in step 1(n)).
    auto years = 0;
    auto months = 0;

    if (largestUnit == TemporalUnit::Year || largestUnit == TemporalUnit::Month) {
        auto candidateYears = two.year() - one.year();
        if (candidateYears)
            candidateYears -= sign;
        while (!isoDateSurpasses(sign, one.year() + candidateYears, one.month(), one.day(), two)) {
            years = candidateYears;
            candidateYears += sign;
        }

        auto candidateMonths = sign;
        auto intermediate = balanceISOYearMonth(one.year() + years, one.month() + candidateMonths);
        while (!isoDateSurpasses(sign, intermediate.year(), intermediate.month(), one.day(), two)) {
            months = candidateMonths;
            candidateMonths += sign;
            intermediate = balanceISOYearMonth(intermediate.year(), intermediate.month() + sign);
        }

        if (largestUnit == TemporalUnit::Month) {
            months += years * 12;
            years = 0;
        }
    }

    auto intermediate = balanceISOYearMonth(one.year() + years, one.month() + months);
    auto constrained = TemporalPlainDate::regulateISODate(intermediate.year(), intermediate.month(),
        one.day(), TemporalOverflow::Constrain);
    ASSERT(constrained); // regulateISODate() should succeed, because the overflow mode is Constrain

    double weeks = 0;
    double days = makeDay(two.year(), two.month() - 1, two.day()) -
        makeDay(constrained->year(), constrained->month() - 1, constrained->day());

    if (largestUnit == TemporalUnit::Week) {
        weeks = std::trunc(std::abs(days) / 7.0);
        days = std::trunc((double) (((Int128) std::trunc(days)) % 7));
        if (weeks)
            weeks *= sign; // Avoid -0
    }

    return dateDuration(years, months, weeks, days);
}

// https://tc39.es/proposal-temporal/#sec-temporal-differencetemporalplainyearmonth
ISO8601::Duration TemporalCalendar::differenceTemporalPlainYearMonth(JSGlobalObject* globalObject,
    bool isSince, const ISO8601::PlainYearMonth& yearMonth, const ISO8601::PlainYearMonth& other,
    unsigned increment, TemporalUnit smallestUnit, TemporalUnit largestUnit,
    RoundingMode roundingMode)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (largestUnit == TemporalUnit::Week || largestUnit == TemporalUnit::Day) {
        throwRangeError(globalObject, scope, "largestUnit must be one of year, years, month, months"_s);
        return { };
    }

    if (smallestUnit == TemporalUnit::Week || smallestUnit == TemporalUnit::Day) {
        throwRangeError(globalObject, scope, "smallestUnit must be one of year, years, month, months"_s);
        return { };
    }

    auto sign = isoDateCompare(yearMonth.isoPlainDate(), other.isoPlainDate());
    if (!sign)
        return { };

    auto thisDate = yearMonth.isoPlainDate();
    auto otherDate = other.isoPlainDate();

    auto thisWithinLimits = ISO8601::isDateTimeWithinLimits(
        thisDate.year(), thisDate.month(), thisDate.day(), 12, 0, 0, 0, 0, 0);
    auto otherWithinLimits = ISO8601::isDateTimeWithinLimits(
        otherDate.year(), otherDate.month(), otherDate.day(), 12, 0, 0, 0, 0, 0);
    if (!thisWithinLimits || !otherWithinLimits) {
        throwRangeError(globalObject, scope, "date/time value is outside of supported range"_s);
        return { };
    }
    auto dateDifference = calendarDateUntil(thisDate, otherDate, largestUnit);
    auto duration = ISO8601::InternalDuration::combineDateAndTimeDuration(globalObject,
        ISO8601::Duration { dateDifference.years(), dateDifference.months(), 0, 0, 0, 0, 0, 0, 0, 0 },
        0);
    RETURN_IF_EXCEPTION(scope, { });

    if (smallestUnit != TemporalUnit::Month || increment != 1) {
        auto isoDateTimeOther = TemporalPlainDateTime::combineISODateAndTimeRecord(otherDate, ISO8601::PlainTime());
        auto destEpochNs = ISO8601::getUTCEpochNanoseconds(isoDateTimeOther);
        TemporalDuration::roundRelativeDuration(globalObject, duration, destEpochNs,
            TemporalPlainDateTime::combineISODateAndTimeRecord(thisDate, ISO8601::PlainTime()),
            std::nullopt, largestUnit, increment, smallestUnit, roundingMode);
        RETURN_IF_EXCEPTION(scope, { });
    }
    auto result = TemporalDuration::temporalDurationFromInternal(duration, TemporalUnit::Day);
    if (isSince)
        result = -result;
    return result;
}

// https://tc39.es/proposal-temporal/#sec-temporal-compareisodate
int32_t TemporalCalendar::isoDateCompare(const ISO8601::PlainDate& d1, const ISO8601::PlainDate& d2)
{
    if (d1.year() > d2.year())
        return 1;
    if (d1.year() < d2.year())
        return -1;
    if (d1.month() > d2.month())
        return 1;
    if (d1.month() < d2.month())
        return -1;
    if (d1.day() > d2.day())
        return 1;
    if (d1.day() < d2.day())
        return -1;
    return 0;
}

bool TemporalCalendar::equals(JSGlobalObject* globalObject, TemporalCalendar* other)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (other == this)
        return true;

    JSString* thisString = toString(globalObject);
    RETURN_IF_EXCEPTION(scope, false);
    JSString* thatString = other->toString(globalObject);
    RETURN_IF_EXCEPTION(scope, false);

    RELEASE_AND_RETURN(scope, thisString->equal(globalObject, thatString));
}

CalendarDateRecord isoToDate(const ISO8601::PlainDate& isoDate) {
    // Compute day of week
    auto month = isoDate.month();
    auto year = isoDate.year();
    auto day = isoDate.day();
    auto shiftedMonth = month + (month < 3 ? 10 : -2);
    auto shiftedYear = year - (month < 3 ? 1 : 0);
    auto century = std::floor(shiftedYear / 100);
    auto yearInCentury = shiftedYear - century * 100;
    auto monthTerm = std::floor(2.6 * shiftedMonth - 0.2);
    auto yearTerm = yearInCentury + std::floor(yearInCentury / 4);
    auto centuryTerm = std::floor(century / 4) - 2 * century;
    auto dow = std::fmod(day + monthTerm + yearTerm + centuryTerm, 7);

    int32_t dayOfWeek = dow + (dow <= 0 ? 7 : 0);

    // Compute day of year
    int32_t dayOfYear = day;
    for (int32_t m = month - 1; m > 0; m--) {
        dayOfYear += ISO8601::daysInMonth(year, m);
    }

    // Compute days in year
    int32_t daysInYear = isLeapYear(year) ? 366 : 365;

    return { dayOfWeek, dayOfYear, daysInYear };
}

static int32_t weekNumber(int32_t firstDayOfWeek, int32_t minimalDaysInFirstWeek,
    int32_t desiredDay, int32_t dayOfWeek)
{
    auto periodStartDayOfWeek = (dayOfWeek - firstDayOfWeek - desiredDay + 1) % 7;
    if (periodStartDayOfWeek < 0)
        periodStartDayOfWeek += 7;
    auto weekNo = std::floor((desiredDay + periodStartDayOfWeek - 1) / 7);
    if (7 - periodStartDayOfWeek >= minimalDaysInFirstWeek)
        weekNo++;
    return weekNo;

}

// Follows the polyfill due to the definition of EpochTimeToEpochYear
// in the spec
YearWeekRecord TemporalCalendar::calendarDateWeekOfYear(JSGlobalObject* globalObject,
    const ISO8601::PlainDate& isoDate)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    double yearOfWeek = isoDate.year();
    auto [ dayOfWeek, dayOfYear, daysInYear ] = isoToDate(isoDate);
    auto firstDayOfWeek = 1;
    auto minimalDaysInWeek = 4;

    auto relativeDayOfWeek = (dayOfWeek + 7 - firstDayOfWeek) % 7;
    auto relativeDayOfWeekJan1 = (dayOfWeek - dayOfYear + 7001 - firstDayOfWeek) % 7;
    auto weekOfYear = std::floor((dayOfYear - 1 + relativeDayOfWeekJan1) / 7);
    if (7 - relativeDayOfWeekJan1 >= minimalDaysInWeek)
        weekOfYear++;

    if (weekOfYear == 0) {
        auto prevYearCalendar = isoToDate(isoDateAdd(globalObject, isoDate,
            { -1, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, TemporalOverflow::Constrain));
        auto previousDayOfYear = dayOfYear = prevYearCalendar.daysInYear;
        RETURN_IF_EXCEPTION(scope, { });
        weekOfYear = weekNumber(firstDayOfWeek, minimalDaysInWeek, previousDayOfYear, dayOfWeek);
        yearOfWeek--;
    } else {
        auto lastDayOfYear = daysInYear;
        if (dayOfYear >= lastDayOfYear - 5) {
            auto lastRelativeDayOfWeek = (relativeDayOfWeek + lastDayOfYear - dayOfYear) % 7;
            if (lastRelativeDayOfWeek < 0)
                lastRelativeDayOfWeek += 7;
            if (6 - lastRelativeDayOfWeek >= minimalDaysInWeek && dayOfYear + 7 - relativeDayOfWeek > lastDayOfYear) {
                weekOfYear = 1;
                yearOfWeek++;
            }
        }
    }

    return { weekOfYear, yearOfWeek };
}

} // namespace JSC
