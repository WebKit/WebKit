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

// https://tc39.es/proposal-temporal/#sec-temporal-totemporaldatetime
TemporalPlainDateTime* TemporalPlainDateTime::from(JSGlobalObject* globalObject, JSValue itemValue, std::optional<TemporalOverflow> overflowValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto overflow = overflowValue.value_or(TemporalOverflow::Constrain);

    if (itemValue.isObject()) {
        if (itemValue.inherits<TemporalPlainDateTime>())
            return jsCast<TemporalPlainDateTime*>(itemValue);

        if (itemValue.inherits<TemporalPlainDate>())
            return TemporalPlainDateTime::create(vm, globalObject->plainDateTimeStructure(), jsCast<TemporalPlainDate*>(itemValue)->plainDate(), { });

        JSObject* calendar = TemporalCalendar::getTemporalCalendarWithISODefault(globalObject, itemValue);
        RETURN_IF_EXCEPTION(scope, { });

        // FIXME: Implement after fleshing out Temporal.Calendar.
        if (!calendar->inherits<TemporalCalendar>() || !jsCast<TemporalCalendar*>(calendar)->isISO8601()) {
            throwRangeError(globalObject, scope, "unimplemented: from non-ISO8601 calendar"_s);
            return { };
        }

        constexpr bool skipRelevantPropertyCheck = true;
        auto timeDuration = TemporalPlainTime::toTemporalTimeRecord(globalObject, asObject(itemValue), skipRelevantPropertyCheck);
        RETURN_IF_EXCEPTION(scope, { });

        auto plainDate = TemporalCalendar::isoDateFromFields(globalObject, asObject(itemValue), overflow);
        RETURN_IF_EXCEPTION(scope, { });

        auto plainTime = TemporalPlainTime::regulateTime(globalObject, WTFMove(timeDuration), overflow);
        RETURN_IF_EXCEPTION(scope, { });

        RELEASE_AND_RETURN(scope, TemporalPlainDateTime::tryCreateIfValid(globalObject, globalObject->plainDateTimeStructure(), WTFMove(plainDate), WTFMove(plainTime)));
    }

    if (!itemValue.isString()) {
        throwTypeError(globalObject, scope, "can only convert to PlainDateTime from object or string values"_s);
        return { };
    }

    auto string = itemValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    // https://tc39.es/proposal-temporal/#sec-temporal-parsetemporaldatetimestring
    // TemporalDateString :
    //     CalendarDateTime
    auto dateTime = ISO8601::parseCalendarDateTime(string);
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
    auto plainDate = TemporalCalendar::isoDateFromFields(globalObject, y, m, d, overflow);
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
    auto increment = temporalRoundingIncrement(globalObject, options, maximum, false);
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

} // namespace JSC
