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
#include "StructureInlines.h"
#include "TemporalDuration.h"
#include "TemporalPlainDate.h"
#include "TemporalPlainDateTime.h"
#include "TemporalPlainTime.h"

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

JSObject* TemporalCalendar::toTemporalCalendarWithISODefault(JSGlobalObject* globalObject, JSValue temporalCalendarLike)
{
    if (temporalCalendarLike.isUndefined())
        return TemporalCalendar::create(globalObject->vm(), globalObject->calendarStructure(), iso8601CalendarID());
    return TemporalCalendar::from(globalObject, temporalCalendarLike);
}

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

std::optional<CalendarID> TemporalCalendar::isBuiltinCalendar(StringView string)
{
    const auto& calendars = intlAvailableCalendars();
    for (unsigned index = 0; index < calendars.size(); ++index) {
        if (calendars[index] == string)
            return index;
    }
    return std::nullopt;
}

// https://tc39.es/proposal-temporal/#sec-temporal-parsetemporalcalendarstring
static std::optional<CalendarID> parseTemporalCalendarString(JSGlobalObject* globalObject, StringView)
{
    // FIXME: Implement parsing temporal calendar string, which requires full ISO 8601 parser.
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    throwRangeError(globalObject, scope, "invalid calendar ID"_s);
    return std::nullopt;
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporalcalendar
JSObject* TemporalCalendar::from(JSGlobalObject* globalObject, JSValue calendarLike)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (calendarLike.isObject()) {
        // FIXME: Also support PlainMonthDay, PlainYearMonth, ZonedDateTime.
        if (calendarLike.inherits<TemporalPlainDate>())
            return jsCast<TemporalPlainDate*>(calendarLike)->calendar();

        if (calendarLike.inherits<TemporalPlainDateTime>())
            return jsCast<TemporalPlainDateTime*>(calendarLike)->calendar();

        if (calendarLike.inherits<TemporalPlainTime>())
            return jsCast<TemporalPlainTime*>(calendarLike)->calendar();

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

// https://tc39.es/proposal-temporal/#sec-temporal-calendarresolvefields
/* static */ void TemporalCalendar::calendarResolveFields(JSGlobalObject* globalObject, std::optional<int32_t> year, unsigned month, std::optional<ParsedMonthCode> monthCode, TemporalDateFormat format)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // FIXME: non-ISO8601 calendars
    if ((format == TemporalDateFormat::Date || format == TemporalDateFormat::YearMonth)
        && !year) [[unlikely]] {
        throwTypeError(globalObject, scope, "year must be supplied for this Temporal type"_s);
        return;
    }
    if (monthCode) {
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
}

// https://tc39.es/proposal-temporal/#sec-temporal-isodatefromfields
ISO8601::PlainDate TemporalCalendar::isoDateFromFields(JSGlobalObject* globalObject, JSObject* temporalDateLike, TemporalDateFormat format, Variant<JSObject*, TemporalOverflow> optionsOrOverflow, TemporalOverflow& overflow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

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
        auto monthCodeString = monthCodeProperty.toWTFString(globalObject);
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
        if (yearProperty.isUndefined()) [[unlikely]] {
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
        overflow = toTemporalOverflow(globalObject, std::get<JSObject*>(optionsOrOverflow));
        RETURN_IF_EXCEPTION(scope, { });
    }

    // Check month code if applicable
    if (monthCodePresent) {
        ASSERT(otherMonth);

        // FIXME: ISO8601 calendar assumed
        if (otherMonth->monthNumber < 1 || otherMonth->monthNumber > 12 || otherMonth->isLeapMonth) [[unlikely]] {
            throwRangeError(globalObject, scope, "month code is not valid for ISO 8601 calendar"_s);
            return { };
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

    if (overflow == TemporalOverflow::Constrain)
        month = std::min<unsigned>(month, 12);

    uint8_t daysInMonth = ISO8601::daysInMonth(year, month);
    if (overflow == TemporalOverflow::Constrain)
        day = std::min<unsigned>(day, daysInMonth);

    if (!(month >= 1 && month <= 12)) [[unlikely]] {
        throwRangeError(globalObject, scope, "month is out of range"_s);
        return { };
    }

    if (!(day >= 1 && day <= daysInMonth)) [[unlikely]] {
        throwRangeError(globalObject, scope, "day is out of range"_s);
        return { };
    }

    RELEASE_AND_RETURN(scope, isoDateFromFields(globalObject, format,
        static_cast<int32_t>(year), static_cast<unsigned>(month), static_cast<unsigned>(day),
        otherMonth, overflow));
}

ISO8601::PlainDate TemporalCalendar::isoDateFromFields(JSGlobalObject* globalObject, TemporalDateFormat format, int32_t year, unsigned month, unsigned day, std::optional<ParsedMonthCode> monthCode, TemporalOverflow overflow)
{

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    calendarResolveFields(globalObject, year, month, monthCode, format);
    RETURN_IF_EXCEPTION(scope, { });

    ASSERT(month > 0);
    ASSERT(day > 0);

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

    if (!valid) [[unlikely]] {
        throwRangeError(globalObject, scope, "date time is out of range of ECMAScript representation"_s);
        return { };
    }

    return plainDate;
}

// https://tc39.es/proposal-temporal/#sec-temporal-calendarmonthdayfromfields
ISO8601::PlainDate TemporalCalendar::monthDayFromFields(JSGlobalObject* globalObject, std::optional<int32_t> referenceYear, unsigned month, unsigned day, std::optional<ParsedMonthCode> monthCode, TemporalOverflow overflow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    calendarResolveFields(globalObject, referenceYear, month, monthCode, TemporalDateFormat::MonthDay);
    RETURN_IF_EXCEPTION(scope, { });
    double year = referenceYear.value_or(1972);
    auto result = TemporalDuration::regulateISODate(year, month, day, overflow);
    if (!result || !ISO8601::isValidISODate(result->year(), result->month(), result->day())) [[unlikely]] {
        throwRangeError(globalObject, scope, "monthDayFromFields: date is out of range of ECMAScript representation"_s);
        return { };
    }
    return ISO8601::PlainDate(1972, result->month(), result->day());
}

// https://tc39.es/proposal-temporal/#sec-temporal-balanceisodate
ISO8601::PlainDate TemporalCalendar::balanceISODate(JSGlobalObject* globalObject, double year, double month, double day)
{
    // Avoid turning an out-of-range date into an in-range date
    ASSERT(std::isfinite(year));
    if (static_cast<int32_t>(year) == ISO8601::outOfRangeYear) [[unlikely]]
        return ISO8601::PlainDate { ISO8601::outOfRangeYear, 1, 1 };
    auto epochDays = makeDay(year, month - 1, day);
    double ms = makeDate(epochDays, 0);
    double daysToUse = msToDays(ms);
    // Need the check here because yearMonthFromDays() takes an int32_t
    if (!isInBounds<int32_t>(daysToUse)) [[unlikely]] {
        // It doesn't matter what month and day we return, as this
        // date will be flagged as an error later on anyway.
        return ISO8601::PlainDate { ISO8601::outOfRangeYear, 1, 1 };
    }
    auto [ y, m, d ] = globalObject->vm().dateCache.yearMonthDayFromDaysWithCache(static_cast<int32_t>(daysToUse));
    if (!ISO8601::isYearWithinLimits(y)) [[unlikely]]
        return ISO8601::PlainDate { ISO8601::outOfRangeYear, static_cast<unsigned>(m + 1), static_cast<unsigned>(d) };
    return ISO8601::PlainDate { y, static_cast<unsigned>(m + 1), static_cast<unsigned>(d) };
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

// https://tc39.es/proposal-temporal/#sec-temporal-addisodate
ISO8601::PlainDate TemporalCalendar::isoDateAdd(JSGlobalObject* globalObject, const ISO8601::PlainDate& plainDate, const ISO8601::Duration& duration, TemporalOverflow overflow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    double years = plainDate.year() + duration.years();
    double months = plainDate.month() + duration.months();
    double days = plainDate.day();
    ISO8601::PlainYearMonth intermediate = balanceISOYearMonth(years, months);
    std::optional<ISO8601::PlainDate> intermediate1 = TemporalDuration::regulateISODate(intermediate.year(), intermediate.month(), days, overflow);
    if (!intermediate1) {
        throwRangeError(globalObject, scope, "date time is out of range of ECMAScript representation"_s);
        return { };
    }
    auto d = intermediate1.value().day() + duration.days() + (7 * duration.weeks());
    auto result = balanceISODate(globalObject, intermediate1.value().year(), intermediate1.value().month(), d);
    if (!ISO8601::isDateTimeWithinLimits(result.year(), result.month(), result.day(), 12, 0, 0, 0, 0, 0)) [[unlikely]] {
        throwRangeError(globalObject, scope, "date time is out of range of ECMAScript representation"_s);
        return { };
    }
    return result;
}

// https://tc39.es/proposal-temporal/#sec-temporal-balanceisoyearmonth
ISO8601::PlainYearMonth TemporalCalendar::balanceISOYearMonth(double year, double month)
{
    year += std::floor((month - 1) / 12);
    // ECMA modulo operator always results in same sign as y in x mod y
    month = std::fmod(month - 1, 12) + 1;
    if (month < 1)
        month += 12;
    if (!ISO8601::isYearWithinLimits(year)) [[unlikely]]
        year = ISO8601::outOfRangeYear;
    return ISO8601::PlainYearMonth(year, static_cast<int32_t>(month));
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
    auto constrained = TemporalDuration::regulateISODate(intermediate.year(), intermediate.month(), one.day(), TemporalOverflow::Constrain);
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

} // namespace JSC
