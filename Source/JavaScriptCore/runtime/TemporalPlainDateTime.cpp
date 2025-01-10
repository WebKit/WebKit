/*
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
#include "TemporalPlainDateTime.h"

#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "LazyPropertyInlines.h"
#include "TemporalPlainDate.h"
#include "TemporalPlainTime.h"
#include "VMTrapsInlines.h"

namespace JSC {

const ClassInfo TemporalPlainDateTime::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(TemporalPlainDateTime) };

TemporalPlainDateTime* TemporalPlainDateTime::create(VM& vm, Structure* structure, ISO8601::PlainDate&& plainDate, ISO8601::PlainTime&& plainTime)
{
    auto* object = new (NotNull, allocateCell<TemporalPlainDateTime>(vm)) TemporalPlainDateTime(vm, structure, WTFMove(plainDate), WTFMove(plainTime));
    object->finishCreation(vm);
    return object;
}

Structure* TemporalPlainDateTime::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalPlainDateTime::TemporalPlainDateTime(VM& vm, Structure* structure, ISO8601::PlainDate&& plainDate, ISO8601::PlainTime&& plainTime)
    : Base(vm, structure)
    , m_plainDate(WTFMove(plainDate))
    , m_plainTime(WTFMove(plainTime))
{
}

void TemporalPlainDateTime::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    m_calendar.initLater(
        [] (const auto& init) {
            VM& vm = init.vm;
            auto* globalObject = jsCast<TemporalPlainDateTime*>(init.owner)->globalObject();
            auto* calendar = TemporalCalendar::create(vm, globalObject->calendarStructure(), iso8601CalendarID());
            init.set(calendar);
        });
}

template<typename Visitor>
void TemporalPlainDateTime::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    Base::visitChildren(cell, visitor);

    auto* thisObject = jsCast<TemporalPlainDateTime*>(cell);
    thisObject->m_calendar.visit(visitor);
}

DEFINE_VISIT_CHILDREN(TemporalPlainDateTime);

// https://tc39.es/proposal-temporal/#sec-temporal-createtemporaldatetime
TemporalPlainDateTime* TemporalPlainDateTime::tryCreateIfValid(JSGlobalObject* globalObject, Structure* structure, ISO8601::PlainDate&& plainDate, ISO8601::PlainTime&& plainTime)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!ISO8601::isDateTimeWithinLimits(plainDate.year(), plainDate.month(), plainDate.day(), plainTime.hour(), plainTime.minute(), plainTime.second(), plainTime.millisecond(), plainTime.microsecond(), plainTime.nanosecond())) {
        throwRangeError(globalObject, scope, "date time is out of range of ECMAScript representation"_s);
        return { };
    }

    return TemporalPlainDateTime::create(vm, structure, WTFMove(plainDate), WTFMove(plainTime));
}

TemporalPlainDateTime* TemporalPlainDateTime::tryCreateIfValid(JSGlobalObject* globalObject, Structure* structure, ISO8601::Duration&& duration)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto plainDate = TemporalPlainDate::toPlainDate(globalObject, duration);
    RETURN_IF_EXCEPTION(scope, { });

    auto plainTime = TemporalPlainTime::toPlainTime(globalObject, duration);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, TemporalPlainDateTime::tryCreateIfValid(globalObject, structure, WTFMove(plainDate), WTFMove(plainTime)));
}

// https://tc39.es/proposal-temporal/#sec-temporal-combineisodateandtimerecord
ISO8601::PlainDateTime TemporalPlainDateTime::combineISODateAndTimeRecord(ISO8601::PlainDate isoDate, ISO8601::PlainTime isoTime)
{
    return ISO8601::PlainDateTime(isoDate, isoTime);
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporaldatetime
TemporalPlainDateTime* TemporalPlainDateTime::from(JSGlobalObject* globalObject, JSValue itemValue, std::optional<JSObject*> optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (itemValue.isObject()) {
        if (itemValue.inherits<TemporalPlainDateTime>())
            return jsCast<TemporalPlainDateTime*>(itemValue);

        if (itemValue.inherits<TemporalPlainDate>()) {
            if (optionsValue) {
                toTemporalOverflow(globalObject, optionsValue.value());
                RETURN_IF_EXCEPTION(scope, { });
            }
            return TemporalPlainDateTime::create(vm, globalObject->plainDateTimeStructure(), jsCast<TemporalPlainDate*>(itemValue)->plainDate(), { });
        }

        JSObject* calendarObject = TemporalCalendar::getTemporalCalendarWithISODefault(globalObject, itemValue);
        RETURN_IF_EXCEPTION(scope, { });

        // FIXME: Implement after fleshing out Temporal.Calendar.
        TemporalCalendar* calendar;
        if (!calendarObject->inherits<TemporalCalendar>()) {
            throwRangeError(globalObject, scope, "bad calendar object in Temporal.PlainDateTime.from"_s);
            return { };
        }
        calendar = jsCast<TemporalCalendar*>(calendarObject);
        if (!calendar->isISO8601()) {
            throwRangeError(globalObject, scope, "unimplemented: from non-ISO8601 calendar"_s);
            return { };
        }

        auto fields =  Vector { FieldName::Day, FieldName::Hour, FieldName::Microsecond, FieldName::Millisecond,
            FieldName::Minute, FieldName::Month, FieldName::MonthCode, FieldName::Nanosecond, FieldName::Second,
            FieldName::Year };
        auto [optionalYear, optionalMonth, optionalMonthCode, optionalDay, optionalHour, optionalMinute,
            optionalSecond, optionalMillisecond, optionalMicrosecond, optionalNanosecond, optionalOffset,
            timeZoneOptional] = TemporalCalendar::prepareCalendarFields(globalObject, calendar->identifier(),
                asObject(itemValue), fields, std::nullopt);
        RETURN_IF_EXCEPTION(scope, { });

        auto hour = optionalHour.value_or(0);
        auto minute = optionalMinute.value_or(0);
        auto second = optionalSecond.value_or(0);
        auto millisecond = optionalMillisecond.value_or(0);
        auto microsecond = optionalMicrosecond.value_or(0);
        auto nanosecond = optionalNanosecond.value_or(0);

        auto overflow = TemporalOverflow::Constrain;
        if (optionsValue) {
            overflow = toTemporalOverflow(globalObject, optionsValue.value());
            RETURN_IF_EXCEPTION(scope, { });
        }

        auto result = TemporalCalendar::interpretTemporalDateTimeFields(globalObject, calendar->identifier(),
            optionalYear, optionalMonth, optionalMonthCode, optionalDay, hour, minute, second,
            millisecond, microsecond, nanosecond, overflow);
        RETURN_IF_EXCEPTION(scope, { });

        RELEASE_AND_RETURN(scope, TemporalPlainDateTime::tryCreateIfValid(globalObject, globalObject->plainDateTimeStructure(), result.date(), result.time()));
    }

    if (!itemValue.isString()) {
        throwTypeError(globalObject, scope, "can only convert to PlainDateTime from object or string values"_s);
        return { };
    }

    auto string = itemValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    if (optionsValue) {
        toTemporalOverflow(globalObject, optionsValue.value()); // Validate overflow
        RETURN_IF_EXCEPTION(scope, { });
    }

    // https://tc39.es/proposal-temporal/#sec-temporal-parsetemporaldatetimestring
    // TemporalDateString :
    //     CalendarDateTime
    auto dateTime = ISO8601::parseCalendarDateTime(string, TemporalDateFormat::Date);
    if (dateTime) {
        auto [plainDate, plainTimeOptional, timeZoneOptional, calendarOptional] = WTFMove(dateTime.value());
        if (!(timeZoneOptional && timeZoneOptional->m_z))
            RELEASE_AND_RETURN(scope, TemporalPlainDateTime::tryCreateIfValid(globalObject, globalObject->plainDateTimeStructure(), WTFMove(plainDate), plainTimeOptional.value_or(ISO8601::PlainTime())));
    }

    throwRangeError(globalObject, scope, "invalid date string"_s);
    return { };
}

// https://tc39.es/proposal-temporal/#sec-temporal-compareisodatetime
int32_t TemporalPlainDateTime::compare(TemporalPlainDateTime* plainDateTime1, TemporalPlainDateTime* plainDateTime2)
{
    if (auto dateResult = TemporalCalendar::isoDateCompare(plainDateTime1->plainDate(), plainDateTime2->plainDate()))
        return dateResult;

    return TemporalPlainTime::compare(plainDateTime1->plainTime(), plainDateTime2->plainTime());
}

static void incrementDay(ISO8601::Duration& duration)
{
    double year = duration.years();
    double month = duration.months();
    double day = duration.days();

    double daysInMonth = ISO8601::daysInMonth(year, month);
    if (day < daysInMonth) {
        duration.setDays(day + 1);
        return;
    }

    duration.setDays(1);
    if (month < 12) {
        duration.setMonths(month + 1);
        return;
    }

    duration.setMonths(1);
    duration.setYears(year + 1);
}

String TemporalPlainDateTime::toString(JSGlobalObject* globalObject, JSValue optionsValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });

    if (!options)
        return toString();

    PrecisionData data = secondsStringPrecision(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    auto roundingMode = temporalRoundingMode(globalObject, options, RoundingMode::Trunc);
    RETURN_IF_EXCEPTION(scope, { });

    // No need to make a new object if we were given explicit defaults.
    if (std::get<0>(data.precision) == Precision::Auto && roundingMode == RoundingMode::Trunc)
        return toString();

    auto duration = TemporalPlainTime::roundTime(m_plainTime, data.increment, data.unit, roundingMode, std::nullopt);
    auto plainTime = TemporalPlainTime::toPlainTime(globalObject, duration);
    RETURN_IF_EXCEPTION(scope, { });

    double extraDays = duration.days();
    duration.setYears(year());
    duration.setMonths(month());
    duration.setDays(day());
    if (extraDays) {
        ASSERT(extraDays == 1);
        incrementDay(duration);
    }

    auto plainDate = TemporalPlainDate::toPlainDate(globalObject, duration);
    RETURN_IF_EXCEPTION(scope, { });

    return ISO8601::temporalDateTimeToString(plainDate, plainTime, data.precision);
}

String TemporalPlainDateTime::monthCode() const
{
    return ISO8601::monthCode(m_plainDate.month());
}

uint8_t TemporalPlainDateTime::dayOfWeek() const
{
    return ISO8601::dayOfWeek(m_plainDate);
}

uint16_t TemporalPlainDateTime::dayOfYear() const
{
    return ISO8601::dayOfYear(m_plainDate);
}

uint8_t TemporalPlainDateTime::weekOfYear() const
{
    return ISO8601::weekOfYear(m_plainDate);
}

TemporalPlainDateTime* TemporalPlainDateTime::addDurationToDateTime(JSGlobalObject* globalObject,
    bool isAdd, ISO8601::Duration duration, JSObject* options) {
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!isAdd)
        duration = -duration;
    TemporalOverflow overflow = toTemporalOverflow(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });
    auto internalDuration = TemporalDuration::toInternalDurationRecordWith24HourDays(globalObject, duration);
    RETURN_IF_EXCEPTION(scope, { });
    auto timeResult = TemporalPlainTime::addTime(m_plainTime, internalDuration.time());
    auto dateDuration = TemporalDuration::adjustDateDurationRecord(globalObject, internalDuration.dateDuration(),
        timeResult.days(), std::nullopt, std::nullopt);
    RETURN_IF_EXCEPTION(scope, { });
    auto addedDate = TemporalCalendar::isoDateAdd(globalObject, m_plainDate, dateDuration, overflow);
    RETURN_IF_EXCEPTION(scope, { });
    auto result = combineISODateAndTimeRecord(addedDate,
        ISO8601::PlainTime(timeResult.hours(), timeResult.minutes(), timeResult.seconds(),
            timeResult.milliseconds(), timeResult.microseconds(), timeResult.nanoseconds()));
    RELEASE_AND_RETURN(scope, TemporalPlainDateTime::tryCreateIfValid(globalObject,
        globalObject->plainDateTimeStructure(), result.date(), result.time()));
}

TemporalPlainDateTime* TemporalPlainDateTime::with(JSGlobalObject* globalObject, JSObject* temporalDateTimeLike, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    rejectObjectWithCalendarOrTimeZone(globalObject, temporalDateTimeLike);
    RETURN_IF_EXCEPTION(scope, { });

    if (!calendar()->isISO8601()) {
        throwRangeError(globalObject, scope, "unimplemented: from non-ISO8601 calendar"_s);
        return { };
    }

    auto [optionalYear, optionalMonth, optionalDay] = TemporalPlainDate::toPartialDate(globalObject, temporalDateTimeLike);
    RETURN_IF_EXCEPTION(scope, { });

    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });

    TemporalOverflow overflow = toTemporalOverflow(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    bool requiresTimeProperty = !optionalYear && !optionalMonth && !optionalDay;
    auto [optionalHour, optionalMinute, optionalSecond, optionalMillisecond, optionalMicrosecond, optionalNanosecond] = TemporalPlainTime::toPartialTime(globalObject, temporalDateTimeLike, !requiresTimeProperty);
    RETURN_IF_EXCEPTION(scope, { });

    double y = optionalYear.value_or(year());
    double m = optionalMonth.value_or(month());
    double d = optionalDay.value_or(day());
    auto plainDate = TemporalCalendar::isoDateFromFields(globalObject, TemporalDateFormat::Date, y, m, d, overflow);
    RETURN_IF_EXCEPTION(scope, { });

    ISO8601::Duration duration { };
    duration.setHours(optionalHour.value_or(hour()));
    duration.setMinutes(optionalMinute.value_or(minute()));
    duration.setSeconds(optionalSecond.value_or(second()));
    duration.setMilliseconds(optionalMillisecond.value_or(millisecond()));
    duration.setMicroseconds(optionalMicrosecond.value_or(microsecond()));
    duration.setNanoseconds(optionalNanosecond.value_or(nanosecond()));
    auto plainTime = TemporalPlainTime::regulateTime(globalObject, WTFMove(duration), overflow);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, TemporalPlainDateTime::tryCreateIfValid(globalObject, globalObject->plainDateTimeStructure(), WTFMove(plainDate), WTFMove(plainTime)));
}

TemporalPlainDateTime* TemporalPlainDateTime::round(JSGlobalObject* globalObject, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* options = nullptr;
    std::optional<TemporalUnit> smallest;
    if (optionsValue.isString()) {
        auto string = optionsValue.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        smallest = temporalUnitType(string);
        if (!smallest) {
            throwRangeError(globalObject, scope, "smallestUnit is an invalid Temporal unit"_s);
            return { };
        }

        if (smallest.value() <= TemporalUnit::Week) {
            throwRangeError(globalObject, scope, "smallestUnit is a disallowed unit"_s);
            return { };
        }
    } else {
        options = intlGetOptionsObject(globalObject, optionsValue);
        RETURN_IF_EXCEPTION(scope, { });

        smallest = temporalSmallestUnit(globalObject, options, { TemporalUnit::Year, TemporalUnit::Month, TemporalUnit::Week });
        RETURN_IF_EXCEPTION(scope, { });
        if (!smallest) {
            throwRangeError(globalObject, scope, "Cannot round without a smallestUnit option"_s);
            return { };
        }
    }
    TemporalUnit smallestUnit = smallest.value();

    auto roundingMode = temporalRoundingMode(globalObject, options, RoundingMode::HalfExpand);
    RETURN_IF_EXCEPTION(scope, { });

    std::optional<double> maximum = smallestUnit == TemporalUnit::Day ? 1 : maximumRoundingIncrement(smallestUnit);
    double increment = doubleNumberOption(globalObject, options, vm.propertyNames->roundingIncrement, 1);
    RETURN_IF_EXCEPTION(scope, { });
    increment = temporalRoundingIncrement(globalObject, increment, maximum, false);
    RETURN_IF_EXCEPTION(scope, { });

    auto duration = TemporalPlainTime::roundTime(m_plainTime, increment, smallestUnit, roundingMode, std::nullopt);
    auto plainTime = TemporalPlainTime::toPlainTime(globalObject, duration);
    RETURN_IF_EXCEPTION(scope, { });

    double extraDays = duration.days();
    duration.setYears(year());
    duration.setMonths(month());
    duration.setDays(day());
    if (extraDays) {
        ASSERT(extraDays == 1);
        incrementDay(duration);
    }

    auto plainDate = TemporalPlainDate::toPlainDate(globalObject, duration);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, TemporalPlainDateTime::tryCreateIfValid(globalObject, globalObject->plainDateTimeStructure(), WTFMove(plainDate), WTFMove(plainTime)));
}

// https://tc39.es/proposal-temporal/#sec-temporal-roundisodatetime
ISO8601::PlainDateTime TemporalPlainDateTime::roundISODateTime(ISO8601::PlainDateTime isoDateTime,
    unsigned increment, TemporalUnit unit, RoundingMode roundingMode)
{
    auto isoDate = isoDateTime.date();
    auto isoTime = isoDateTime.time();

    ASSERT(ISO8601::isDateTimeWithinLimits(isoDate.year(), isoDate.month(), isoDate.day(),
        isoTime.hour(), isoTime.minute(), isoTime.second(), isoTime.millisecond(),
        isoTime.microsecond(), isoTime.nanosecond()));
    auto roundedTime = TemporalPlainTime::roundTime(isoTime, increment, unit, roundingMode, std::nullopt);

    auto balanceResult = TemporalCalendar::balanceISODate(
        isoDate.year(), isoDate.month(), isoDate.day() + roundedTime.days());
    return combineISODateAndTimeRecord(balanceResult,
        ISO8601::PlainTime(roundedTime.hours(), roundedTime.minutes(), roundedTime.seconds(),
            roundedTime.milliseconds(), roundedTime.microseconds(), roundedTime.nanoseconds()));
}

// https://tc39.es/proposal-temporal/#sec-temporal-balanceisodatetime
ISO8601::PlainDateTime TemporalPlainDateTime::balanceISODateTime(double year, double month, double day,
    double hour, double minute, double second, double millisecond, double microsecond, double nanosecond)
{
    auto balancedTime = TemporalPlainTime::balanceTime(
        hour, minute, second, millisecond, microsecond, nanosecond);
    auto balancedDate = TemporalCalendar::balanceISODate(year, month, day + balancedTime.days());
    return ISO8601::PlainDateTime(WTFMove(balancedDate),
        ISO8601::PlainTime(balancedTime.hours(), balancedTime.minutes(),
            balancedTime.seconds(), balancedTime.milliseconds(),
            balancedTime.microseconds(), balancedTime.nanoseconds()));
}

} // namespace JSC
