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

// https://tc39.es/proposal-temporal/#sec-temporal-isodatefromfields
ISO8601::PlainDate TemporalCalendar::isoDateFromFields(JSGlobalObject* globalObject, JSObject* temporalDateLike, TemporalOverflow overflow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue dayProperty = temporalDateLike->get(globalObject, vm.propertyNames->day);
    RETURN_IF_EXCEPTION(scope, { });
    if (dayProperty.isUndefined()) {
        throwTypeError(globalObject, scope, "day property must be present"_s);
        return { };
    }

    double day = dayProperty.toIntegerOrInfinity(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    if (!(day > 0 && std::isfinite(day))) {
        throwRangeError(globalObject, scope, "day property must be positive and finite"_s);
        return { };
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
        auto monthCode = monthCodeProperty.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        auto otherMonth = ISO8601::monthFromCode(monthCode);
        if (!otherMonth) {
            throwRangeError(globalObject, scope, "Invalid monthCode property"_s);
            return { };
        }

        if (monthProperty.isUndefined())
            month = otherMonth;
        else if (otherMonth != month) {
            throwRangeError(globalObject, scope, "month and monthCode properties must match if both are provided"_s);
            return { };
        }
    }

    JSValue yearProperty = temporalDateLike->get(globalObject, vm.propertyNames->year);
    RETURN_IF_EXCEPTION(scope, { });
    if (yearProperty.isUndefined()) {
        throwTypeError(globalObject, scope, "year property must be present"_s);
        return { };
    }

    double year = yearProperty.toIntegerOrInfinity(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    if (!std::isfinite(year)) {
        throwRangeError(globalObject, scope, "year property must be finite"_s);
        return { };
    }

    RELEASE_AND_RETURN(scope, isoDateFromFields(globalObject, year, month, day, overflow));
}

ISO8601::PlainDate TemporalCalendar::isoDateFromFields(JSGlobalObject* globalObject, double year, double month, double day, TemporalOverflow overflow)
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

    if (!ISO8601::isDateTimeWithinLimits(plainDate.year(), plainDate.month(), plainDate.day(), 12, 0, 0, 0, 0, 0)) {
        throwRangeError(globalObject, scope, "date time is out of range of ECMAScript representation"_s);
        return { };
    }

    return plainDate;
}

// https://tc39.es/proposal-temporal/#sec-temporal-balanceisodate
static bool balanceISODate(double& year, double& month, double& day)
{
    ASSERT(isInteger(day));
    ASSERT(month >= 1 && month <= 12);
    if (!ISO8601::isYearWithinLimits(year))
        return false;

    double daysFrom1970 = day + dateToDaysFrom1970(static_cast<int>(year), static_cast<int>(month - 1), 1) - 1;

    double balancedYear = std::floor(daysFrom1970 / 365.2425) + 1970;
    if (!ISO8601::isYearWithinLimits(balancedYear))
        return false;

    double daysUntilYear = daysFrom1970ToYear(static_cast<int>(balancedYear));
    if (daysUntilYear > daysFrom1970) {
        balancedYear--;
        daysUntilYear -= daysInYear(static_cast<int>(balancedYear));
    } else {
        double daysUntilFollowingYear = daysUntilYear + daysInYear(static_cast<int>(balancedYear));
        if (daysUntilFollowingYear <= daysFrom1970) {
            daysUntilYear = daysUntilFollowingYear;
            balancedYear++;
        }
    }

    ASSERT(daysFrom1970 - daysUntilYear >= 0);
    auto dayInYear = static_cast<unsigned>(daysFrom1970 - daysUntilYear + 1);

    unsigned daysUntilMonth = 0;
    unsigned balancedMonth = 1;
    for (; balancedMonth < 12; balancedMonth++) {
        auto monthDays = balancedMonth != 2 ? ISO8601::daysInMonth(balancedMonth) : ISO8601::daysInMonth(static_cast<int>(balancedYear), balancedMonth);
        if (daysUntilMonth + monthDays >= dayInYear)
            break;
        daysUntilMonth += monthDays;
    }

    year = balancedYear;
    month = balancedMonth;
    day = dayInYear - daysUntilMonth;
    return true;
}

// https://tc39.es/proposal-temporal/#sec-temporal-addisodate
ISO8601::PlainDate TemporalCalendar::isoDateAdd(JSGlobalObject* globalObject, const ISO8601::PlainDate& plainDate, const ISO8601::Duration& duration, TemporalOverflow overflow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ISO8601::Duration balancedDuration = duration;
    TemporalDuration::balance(balancedDuration, TemporalUnit::Day);

    double year = plainDate.year() + duration.years();
    double month = plainDate.month() + duration.months();
    if (month < 1 || month > 12) {
        year += std::floor((month - 1) / 12);
        month = nonNegativeModulo((month - 1), 12) + 1;
    }

    double daysInMonth = ISO8601::daysInMonth(year, month);
    double day = plainDate.day();
    if (overflow == TemporalOverflow::Constrain)
        day = std::min<double>(day, daysInMonth);
    else if (day > daysInMonth) {
        throwRangeError(globalObject, scope, "date time is out of range of ECMAScript representation"_s);
        return { };
    }

    day += balancedDuration.days() + 7 * duration.weeks();
    if (day < 1 || day > daysInMonth) {
        if (!balanceISODate(year, month, day)) {
            throwRangeError(globalObject, scope, "date time is out of range of ECMAScript representation"_s);
            return { };
        }
    }

    auto result = TemporalPlainDate::toPlainDate(globalObject, ISO8601::Duration(year, month, 0, day, 0, 0, 0, 0, 0, 0));
    RETURN_IF_EXCEPTION(scope, { });

    if (!ISO8601::isDateTimeWithinLimits(result.year(), result.month(), result.day(), 12, 0, 0, 0, 0, 0)) {
        throwRangeError(globalObject, scope, "date time is out of range of ECMAScript representation"_s);
        return { };
    }

    return result;
}

// https://tc39.es/proposal-temporal/#sec-temporal-differenceisodate
ISO8601::Duration TemporalCalendar::isoDateDifference(JSGlobalObject* globalObject, const ISO8601::PlainDate& date1, const ISO8601::PlainDate& date2, TemporalUnit largestUnit)
{
    ASSERT(largestUnit <= TemporalUnit::Day);

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (largestUnit <= TemporalUnit::Month) {
        auto sign = isoDateCompare(date2, date1);
        if (!sign)
            return { };

        double years = date2.year() - date1.year();
        ISO8601::PlainDate mid = isoDateAdd(globalObject, date1, { years, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, TemporalOverflow::Constrain);
        RETURN_IF_EXCEPTION(scope, { });
        auto midSign = isoDateCompare(date2, mid);
        if (!midSign) {
            if (largestUnit == TemporalUnit::Month)
                return { 0, 12 * years, 0, 0, 0, 0, 0, 0, 0, 0 };

            return { years, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        }

        double months = date2.month() - date1.month();
        if (midSign != sign) {
            years -= sign;
            months += 12 * sign;
        }
        mid = isoDateAdd(globalObject, date1, { years, months, 0, 0, 0, 0, 0, 0, 0, 0 }, TemporalOverflow::Constrain);
        RETURN_IF_EXCEPTION(scope, { });
        midSign = isoDateCompare(date2, mid);
        if (!midSign) {
            if (largestUnit == TemporalUnit::Month)
                return { 0, months + 12 * years, 0, 0, 0, 0, 0, 0, 0, 0 };

            return { years, months, 0, 0, 0, 0, 0, 0, 0, 0 };
        }

        if (midSign != sign) {
            months -= sign;
            if (months == -sign) {
                years -= sign;
                months = 11 * sign;
            }
            mid = isoDateAdd(globalObject, date1, { years, months, 0, 0, 0, 0, 0, 0, 0, 0 }, TemporalOverflow::Constrain);
            RETURN_IF_EXCEPTION(scope, { });
        }

        double days;
        if (mid.month() == date2.month())
            days = date2.day() - mid.day();
        else if (sign < 0)
            days = -mid.day() - (ISO8601::daysInMonth(date2.year(), date2.month()) - date2.day());
        else
            days = date2.day() + (ISO8601::daysInMonth(mid.year(), mid.month()) - mid.day());

        if (largestUnit == TemporalUnit::Month)
            return { 0, months + 12 * years, 0, days, 0, 0, 0, 0, 0, 0 };

        return { years, months, 0, days, 0, 0, 0, 0, 0, 0 };
    }

    double days = dateToDaysFrom1970(static_cast<int>(date2.year()), static_cast<int>(date2.month() - 1), static_cast<int>(date2.day()))
        - dateToDaysFrom1970(static_cast<int>(date1.year()), static_cast<int>(date1.month() - 1), static_cast<int>(date1.day()));

    double weeks = 0;
    if (largestUnit == TemporalUnit::Week) {
        weeks = std::trunc(days / 7);
        days = std::fmod(days, 7) + 0.0;
    }

    return { 0, 0, weeks, days, 0, 0, 0, 0, 0, 0 };
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

} // namespace JSC
