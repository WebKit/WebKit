/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "TemporalPlainDate.h"

#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "LazyPropertyInlines.h"
#include "TemporalDuration.h"
#include "TemporalPlainDateTime.h"
#include "VMTrapsInlines.h"

namespace JSC {

const ClassInfo TemporalPlainDate::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(TemporalPlainDate) };

TemporalPlainDate* TemporalPlainDate::create(VM& vm, Structure* structure, ISO8601::PlainDate&& plainDate)
{
    auto* object = new (NotNull, allocateCell<TemporalPlainDate>(vm)) TemporalPlainDate(vm, structure, WTFMove(plainDate));
    object->finishCreation(vm);
    return object;
}

Structure* TemporalPlainDate::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalPlainDate::TemporalPlainDate(VM& vm, Structure* structure, ISO8601::PlainDate&& plainDate)
    : Base(vm, structure)
    , m_plainDate(WTFMove(plainDate))
{
}

void TemporalPlainDate::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    m_calendar.initLater(
        [] (const auto& init) {
            VM& vm = init.vm;
            auto* plainDate = jsCast<TemporalPlainDate*>(init.owner);
            auto* globalObject = plainDate->globalObject();
            auto* calendar = TemporalCalendar::create(vm, globalObject->calendarStructure(), iso8601CalendarID());
            init.set(calendar);
        });
}

template<typename Visitor>
void TemporalPlainDate::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    Base::visitChildren(cell, visitor);

    auto* thisObject = jsCast<TemporalPlainDate*>(cell);
    thisObject->m_calendar.visit(visitor);
}

DEFINE_VISIT_CHILDREN(TemporalPlainDate);

ISO8601::PlainDate TemporalPlainDate::toPlainDate(JSGlobalObject* globalObject, const ISO8601::Duration& duration)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    double yearDouble = duration.years();
    double monthDouble = duration.months();
    double dayDouble = duration.days();

    if (!ISO8601::isYearWithinLimits(yearDouble)) {
        throwRangeError(globalObject, scope, "year is out of range"_s);
        return { };
    }
    int32_t year = static_cast<int32_t>(yearDouble);

    if (!(monthDouble >= 1 && monthDouble <= 12)) {
        throwRangeError(globalObject, scope, "month is out of range"_s);
        return { };
    }
    unsigned month = static_cast<unsigned>(monthDouble);

    double daysInMonth = ISO8601::daysInMonth(year, month);
    if (!(dayDouble >= 1 && dayDouble <= daysInMonth)) {
        throwRangeError(globalObject, scope, "day is out of range"_s);
        return { };
    }
    unsigned day = static_cast<unsigned>(dayDouble);

    return ISO8601::PlainDate {
        year,
        month,
        day
    };
}

// CreateTemporalDate ( years, months, days )
// https://tc39.es/proposal-temporal/#sec-temporal-createtemporaldate
TemporalPlainDate* TemporalPlainDate::tryCreateIfValid(JSGlobalObject* globalObject, Structure* structure, ISO8601::PlainDate&& plainDate)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!ISO8601::isDateTimeWithinLimits(plainDate.year(), plainDate.month(), plainDate.day(), 12, 0, 0, 0, 0, 0)) {
        throwRangeError(globalObject, scope, "date time is out of range of ECMAScript representation"_s);
        return { };
    }

    return TemporalPlainDate::create(vm, structure, WTFMove(plainDate));
}

TemporalPlainDate* TemporalPlainDate::tryCreateIfValid(JSGlobalObject* globalObject, Structure* structure, ISO8601::Duration&& duration)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto plainDate = toPlainDate(globalObject, duration);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, TemporalPlainDate::tryCreateIfValid(globalObject, structure,  WTFMove(plainDate)));
}

String TemporalPlainDate::toString(JSGlobalObject* globalObject, JSValue optionsValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });

    if (!options)
        return toString();

    return ISO8601::temporalDateToString(m_plainDate);
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporaldate
TemporalPlainDate* TemporalPlainDate::from(JSGlobalObject* globalObject, JSValue itemValue, std::optional<TemporalOverflow> overflowValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto overflow = overflowValue.value_or(TemporalOverflow::Constrain);

    if (itemValue.isObject()) {
        if (itemValue.inherits<TemporalPlainDate>())
            return jsCast<TemporalPlainDate*>(itemValue);

        if (itemValue.inherits<TemporalPlainDateTime>())
            return TemporalPlainDate::create(vm, globalObject->plainDateStructure(), jsCast<TemporalPlainDateTime*>(itemValue)->plainDate());

        JSObject* calendar = TemporalCalendar::getTemporalCalendarWithISODefault(globalObject, itemValue);
        RETURN_IF_EXCEPTION(scope, { });

        // FIXME: Implement after fleshing out Temporal.Calendar.
        if (!calendar->inherits<TemporalCalendar>() || !jsCast<TemporalCalendar*>(calendar)->isISO8601()) {
            throwRangeError(globalObject, scope, "unimplemented: from non-ISO8601 calendar"_s);
            return { };
        }

        auto plainDate = TemporalCalendar::isoDateFromFields(globalObject, asObject(itemValue), overflow);
        RETURN_IF_EXCEPTION(scope, { });
        return TemporalPlainDate::create(vm, globalObject->plainDateStructure(), WTFMove(plainDate));
    }

    auto string = itemValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    // https://tc39.es/proposal-temporal/#sec-temporal-parsetemporaldatestring
    // TemporalDateString :
    //     CalendarDateTime
    auto dateTime = ISO8601::parseCalendarDateTime(string);
    if (dateTime) {
        auto [plainDate, plainTimeOptional, timeZoneOptional, calendarOptional] = WTFMove(dateTime.value());
        if (!(timeZoneOptional && timeZoneOptional->m_z))
            RELEASE_AND_RETURN(scope, TemporalPlainDate::tryCreateIfValid(globalObject, globalObject->plainDateStructure(), WTFMove(plainDate)));
    }

    throwRangeError(globalObject, scope, "invalid date string"_s);
    return { };
}

std::array<std::optional<double>, numberOfTemporalPlainDateUnits> TemporalPlainDate::toPartialDate(JSGlobalObject* globalObject, JSObject* temporalDateLike)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    std::optional<double> day;
    JSValue dayProperty = temporalDateLike->get(globalObject, vm.propertyNames->day);
    RETURN_IF_EXCEPTION(scope, { });
    if (!dayProperty.isUndefined()) {
        day = dayProperty.toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        if (day.value() <= 0 || !std::isfinite(day.value())) {
            throwRangeError(globalObject, scope, "day property must be positive and finite"_s);
            return { };
        }
    }

    std::optional<double> month;
    JSValue monthProperty = temporalDateLike->get(globalObject, vm.propertyNames->month);
    RETURN_IF_EXCEPTION(scope, { });
    if (!monthProperty.isUndefined()) {
        month = monthProperty.toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        if (month.value() <= 0 || !std::isfinite(month.value())) {
            throwRangeError(globalObject, scope, "month property must be positive and finite"_s);
            return { };
        }
    }

    JSValue monthCodeProperty = temporalDateLike->get(globalObject, vm.propertyNames->monthCode);
    RETURN_IF_EXCEPTION(scope, { });
    if (!monthCodeProperty.isUndefined()) {
        auto monthCode = monthCodeProperty.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        auto otherMonth = ISO8601::monthFromCode(monthCode);
        if (!otherMonth) {
            throwRangeError(globalObject, scope, "Invalid monthCode property"_s);
            return { };
        }

        if (!month)
            month = otherMonth;
        else if (month.value() != otherMonth) {
            throwRangeError(globalObject, scope, "month and monthCode properties must match if both are provided"_s);
            return { };
        }
    }

    std::optional<double> year;
    JSValue yearProperty = temporalDateLike->get(globalObject, vm.propertyNames->year);
    RETURN_IF_EXCEPTION(scope, { });
    if (!yearProperty.isUndefined()) {
        year = yearProperty.toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        if (!std::isfinite(year.value())) {
            throwRangeError(globalObject, scope, "year property must be finite"_s);
            return { };
        }
    }

    return { year, month, day };
}

ISO8601::PlainDate TemporalPlainDate::with(JSGlobalObject* globalObject, JSObject* temporalDateLike, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    rejectObjectWithCalendarOrTimeZone(globalObject, temporalDateLike);
    RETURN_IF_EXCEPTION(scope, { });

    if (!calendar()->isISO8601()) {
        throwRangeError(globalObject, scope, "unimplemented: with non-ISO8601 calendar"_s);
        return { };
    }

    auto [optionalYear, optionalMonth, optionalDay] = toPartialDate(globalObject, temporalDateLike);
    RETURN_IF_EXCEPTION(scope, { });
    if (!optionalYear && !optionalMonth && !optionalDay) {
        throwTypeError(globalObject, scope, "Object must contain at least one Temporal date property"_s);
        return { };
    }

    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });

    TemporalOverflow overflow = toTemporalOverflow(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    double y = optionalYear.value_or(year());
    double m = optionalMonth.value_or(month());
    double d = optionalDay.value_or(day());
    RELEASE_AND_RETURN(scope, TemporalCalendar::isoDateFromFields(globalObject, y, m, d, overflow));
}

ISO8601::Duration TemporalPlainDate::until(JSGlobalObject* globalObject, TemporalPlainDate* other, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    bool calendarsMatch = calendar()->equals(globalObject, other->calendar());
    RETURN_IF_EXCEPTION(scope, { });
    if (!calendarsMatch) {
        throwRangeError(globalObject, scope, "calendars must match"_s);
        return { };
    }

    if (!calendar()->isISO8601()) {
        throwRangeError(globalObject, scope, "unimplemented: with non-ISO8601 calendar"_s);
        return { };
    }

    auto [smallestUnit, largestUnit, roundingMode, increment] = extractDifferenceOptions(globalObject, optionsValue, UnitGroup::Date, TemporalUnit::Day, TemporalUnit::Day);
    RETURN_IF_EXCEPTION(scope, { });

    auto result = TemporalCalendar::isoDateDifference(globalObject, plainDate(), other->plainDate(), largestUnit);
    RETURN_IF_EXCEPTION(scope, { });

    if (smallestUnit != TemporalUnit::Day || increment != 1) {
        if (smallestUnit != TemporalUnit::Day) {
            throwRangeError(globalObject, scope, "unimplemented: depends on Duration relativeTo"_s);
            return { };
        }
        result.setHours(0);
        result.setMinutes(0);
        result.setSeconds(0);
        result.setMilliseconds(0);
        result.setMicroseconds(0);
        result.setNanoseconds(0);
        TemporalDuration::round(result, increment, smallestUnit, roundingMode);
    }

    return result;
}

ISO8601::Duration TemporalPlainDate::since(JSGlobalObject* globalObject, TemporalPlainDate* other, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    bool calendarsMatch = calendar()->equals(globalObject, other->calendar());
    RETURN_IF_EXCEPTION(scope, { });
    if (!calendarsMatch) {
        throwRangeError(globalObject, scope, "calendars must match"_s);
        return { };
    }

    if (!calendar()->isISO8601()) {
        throwRangeError(globalObject, scope, "unimplemented: with non-ISO8601 calendar"_s);
        return { };
    }

    auto [smallestUnit, largestUnit, roundingMode, increment] = extractDifferenceOptions(globalObject, optionsValue, UnitGroup::Date, TemporalUnit::Day, TemporalUnit::Day);
    RETURN_IF_EXCEPTION(scope, { });
    roundingMode = negateTemporalRoundingMode(roundingMode);

    auto result = TemporalCalendar::isoDateDifference(globalObject, plainDate(), other->plainDate(), largestUnit);
    RETURN_IF_EXCEPTION(scope, { });

    if (smallestUnit != TemporalUnit::Day || increment != 1) {
        if (smallestUnit != TemporalUnit::Day) {
            throwRangeError(globalObject, scope, "unimplemented: depends on Duration relativeTo"_s);
            return { };
        }
        result.setHours(0);
        result.setMinutes(0);
        result.setSeconds(0);
        result.setMilliseconds(0);
        result.setMicroseconds(0);
        result.setNanoseconds(0);
        TemporalDuration::round(result, increment, smallestUnit, roundingMode);
    }

    return -result;
}

String TemporalPlainDate::monthCode() const
{
    return ISO8601::monthCode(m_plainDate.month());
}

uint8_t TemporalPlainDate::dayOfWeek() const
{
    return ISO8601::dayOfWeek(m_plainDate);
}

uint16_t TemporalPlainDate::dayOfYear() const
{
    return ISO8601::dayOfYear(m_plainDate);
}

uint8_t TemporalPlainDate::weekOfYear() const
{
    return ISO8601::weekOfYear(m_plainDate);
}

} // namespace JSC
