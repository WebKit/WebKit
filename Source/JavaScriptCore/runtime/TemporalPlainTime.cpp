/*
 * Copyright (C) 2021 Apple Inc.
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
#include "TemporalPlainTime.h"

#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "LazyPropertyInlines.h"
#include "TemporalDuration.h"
#include "TemporalPlainDateTime.h"
#include "VMTrapsInlines.h"

namespace JSC {
namespace TemporalPlainTimeInternal {
static constexpr bool verbose = false;
}

const ClassInfo TemporalPlainTime::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(TemporalPlainTime) };

TemporalPlainTime* TemporalPlainTime::create(VM& vm, Structure* structure, ISO8601::PlainTime&& plainTime)
{
    auto* object = new (NotNull, allocateCell<TemporalPlainTime>(vm)) TemporalPlainTime(vm, structure, WTFMove(plainTime));
    object->finishCreation(vm);
    return object;
}

Structure* TemporalPlainTime::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalPlainTime::TemporalPlainTime(VM& vm, Structure* structure, ISO8601::PlainTime&& plainTime)
    : Base(vm, structure)
    , m_plainTime(WTFMove(plainTime))
{
}

void TemporalPlainTime::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    m_calendar.initLater(
        [] (const auto& init) {
            VM& vm = init.vm;
            auto* plainTime = jsCast<TemporalPlainTime*>(init.owner);
            auto* globalObject = plainTime->globalObject();
            auto* calendar = TemporalCalendar::create(vm, globalObject->calendarStructure(), iso8601CalendarID());
            init.set(calendar);
        });
}

template<typename Visitor>
void TemporalPlainTime::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    Base::visitChildren(cell, visitor);

    auto* thisObject = jsCast<TemporalPlainTime*>(cell);
    thisObject->m_calendar.visit(visitor);
}

DEFINE_VISIT_CHILDREN(TemporalPlainTime);

// https://tc39.es/proposal-temporal/#sec-temporal-isvalidtime
ISO8601::PlainTime TemporalPlainTime::toPlainTime(JSGlobalObject* globalObject, const ISO8601::Duration& duration)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    double hour = duration.hours();
    double minute = duration.minutes();
    double second = duration.seconds();
    double millisecond = duration.milliseconds();
    double microsecond = duration.microseconds();
    double nanosecond = duration.nanoseconds();
    if (!(hour >= 0 && hour <= 23)) {
        throwRangeError(globalObject, scope, "hour is out of range"_s);
        return { };
    }
    if (!(minute >= 0 && minute <= 59)) {
        throwRangeError(globalObject, scope, "minute is out of range"_s);
        return { };
    }
    if (!(second >= 0 && second <= 59)) {
        throwRangeError(globalObject, scope, "second is out of range"_s);
        return { };
    }
    if (!(millisecond >= 0 && millisecond <= 999)) {
        throwRangeError(globalObject, scope, "millisecond is out of range"_s);
        return { };
    }
    if (!(microsecond >= 0 && microsecond <= 999)) {
        throwRangeError(globalObject, scope, "microsecond is out of range"_s);
        return { };
    }
    if (!(nanosecond >= 0 && nanosecond <= 999)) {
        throwRangeError(globalObject, scope, "nanosecond is out of range"_s);
        return { };
    }
    return ISO8601::PlainTime {
        static_cast<unsigned>(hour),
        static_cast<unsigned>(minute),
        static_cast<unsigned>(second),
        static_cast<unsigned>(millisecond),
        static_cast<unsigned>(microsecond),
        static_cast<unsigned>(nanosecond)
    };
}

// CreateTemporalPlainTime ( years, months, weeks, days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds [ , newTarget ] )
// https://tc39.es/proposal-temporal/#sec-temporal-createtemporalplainTime
TemporalPlainTime* TemporalPlainTime::tryCreateIfValid(JSGlobalObject* globalObject, Structure* structure, ISO8601::Duration&& duration)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto plainTime = toPlainTime(globalObject, duration);
    RETURN_IF_EXCEPTION(scope, { });

    return TemporalPlainTime::create(vm, structure, WTFMove(plainTime));
}

// https://tc39.es/proposal-temporal/#sec-temporal-balancetime
static ISO8601::Duration balanceTime(double hour, double minute, double second, double millisecond, double microsecond, double nanosecond)
{
    // https://github.com/tc39/proposal-temporal/issues/1804
    // Use non-negative modulo operation.
    microsecond += std::floor(nanosecond / 1000);
    nanosecond = nonNegativeModulo(nanosecond, 1000);
    millisecond += std::floor(microsecond / 1000);
    microsecond = nonNegativeModulo(microsecond, 1000);
    second += std::floor(millisecond / 1000);
    millisecond = nonNegativeModulo(millisecond, 1000);
    minute += std::floor(second / 60);
    second = nonNegativeModulo(second, 60);
    hour += std::floor(minute / 60);
    minute = nonNegativeModulo(minute, 60);
    double days = std::floor(hour / 24);
    hour = nonNegativeModulo(hour, 24);
    return ISO8601::Duration(0, 0, 0, days, hour, minute, second, millisecond, microsecond, nanosecond);
}

// https://tc39.es/proposal-temporal/#sec-temporal-roundtime
ISO8601::Duration TemporalPlainTime::roundTime(ISO8601::PlainTime plainTime, double increment, TemporalUnit unit, RoundingMode roundingMode, std::optional<double> dayLengthNs)
{
    auto fractionalSecond = [](ISO8601::PlainTime plainTime) -> double {
        return plainTime.second() + plainTime.millisecond() * 1e-3 + plainTime.microsecond() * 1e-6 + plainTime.nanosecond() * 1e-9;
    };

    double quantity = 0;
    switch (unit) {
    case TemporalUnit::Day: {
        double length = dayLengthNs.value_or(8.64 * 1e13);
        quantity = (((((plainTime.hour() * 60.0 + plainTime.minute()) * 60.0 + plainTime.second()) * 1000.0 + plainTime.millisecond()) * 1000.0 + plainTime.microsecond()) * 1000.0 + plainTime.nanosecond()) / length;
        auto result = roundNumberToIncrement(quantity, increment, roundingMode);
        return ISO8601::Duration(0, 0, 0, result, 0, 0, 0, 0, 0, 0);
    }
    case TemporalUnit::Hour: {
        quantity = (fractionalSecond(plainTime) / 60.0 + plainTime.minute()) / 60.0 + plainTime.hour();
        auto result = roundNumberToIncrement(quantity, increment, roundingMode);
        return balanceTime(result, 0, 0, 0, 0, 0);
    }
    case TemporalUnit::Minute: {
        quantity = fractionalSecond(plainTime) / 60.0 + plainTime.minute();
        auto result = roundNumberToIncrement(quantity, increment, roundingMode);
        return balanceTime(plainTime.hour(), result, 0, 0, 0, 0);
    }
    case TemporalUnit::Second: {
        quantity = fractionalSecond(plainTime);
        auto result = roundNumberToIncrement(quantity, increment, roundingMode);
        return balanceTime(plainTime.hour(), plainTime.minute(), result, 0, 0, 0);
    }
    case TemporalUnit::Millisecond: {
        quantity = plainTime.millisecond() + plainTime.microsecond() * 1e-3 + plainTime.nanosecond() * 1e-6;
        auto result = roundNumberToIncrement(quantity, increment, roundingMode);
        return balanceTime(plainTime.hour(), plainTime.minute(), plainTime.second(), result, 0, 0);
    }
    case TemporalUnit::Microsecond: {
        quantity = plainTime.microsecond() + plainTime.nanosecond() * 1e-3;
        auto result = roundNumberToIncrement(quantity, increment, roundingMode);
        return balanceTime(plainTime.hour(), plainTime.minute(), plainTime.second(), plainTime.millisecond(), result, 0);
    }
    case TemporalUnit::Nanosecond: {
        quantity = plainTime.nanosecond();
        auto result = roundNumberToIncrement(quantity, increment, roundingMode);
        return balanceTime(plainTime.hour(), plainTime.minute(), plainTime.second(), plainTime.millisecond(), plainTime.microsecond(), result);
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    return ISO8601::Duration();
}

ISO8601::PlainTime TemporalPlainTime::round(JSGlobalObject* globalObject, JSValue optionsValue) const
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

        if (smallest.value() <= TemporalUnit::Day) {
            throwRangeError(globalObject, scope, "smallestUnit is a disallowed unit"_s);
            return { };
        }
    } else {
        options = intlGetOptionsObject(globalObject, optionsValue);
        RETURN_IF_EXCEPTION(scope, { });

        smallest = temporalSmallestUnit(globalObject, options, { TemporalUnit::Year, TemporalUnit::Month, TemporalUnit::Week, TemporalUnit::Day });
        RETURN_IF_EXCEPTION(scope, { });
        if (!smallest) {
            throwRangeError(globalObject, scope, "Cannot round without a smallestUnit option"_s);
            return { };
        }
    }
    TemporalUnit smallestUnit = smallest.value();

    auto roundingMode = temporalRoundingMode(globalObject, options, RoundingMode::HalfExpand);
    RETURN_IF_EXCEPTION(scope, { });

    auto increment = temporalRoundingIncrement(globalObject, options, maximumRoundingIncrement(smallestUnit), false);
    RETURN_IF_EXCEPTION(scope, { });

    auto duration = roundTime(m_plainTime, increment, smallestUnit, roundingMode, std::nullopt);
    RELEASE_AND_RETURN(scope, toPlainTime(globalObject, duration));
}

String TemporalPlainTime::toString(JSGlobalObject* globalObject, JSValue optionsValue) const
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

    auto duration = roundTime(m_plainTime, data.increment, data.unit, roundingMode, std::nullopt);
    auto plainTime = toPlainTime(globalObject, duration);
    RETURN_IF_EXCEPTION(scope, { });

    return ISO8601::temporalTimeToString(plainTime, data.precision);
}

ISO8601::Duration TemporalPlainTime::toTemporalTimeRecord(JSGlobalObject* globalObject, JSObject* temporalTimeLike, bool skipRelevantPropertyCheck)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ISO8601::Duration duration { };
    auto hasRelevantProperty = false;
    for (TemporalUnit unit : temporalUnitsInTableOrder) {
        if (unit < TemporalUnit::Hour)
            continue;
        auto name = temporalUnitSingularPropertyName(vm, unit);
        JSValue value = temporalTimeLike->get(globalObject, name);
        RETURN_IF_EXCEPTION(scope, { });

        if (value.isUndefined())
            continue;

        hasRelevantProperty = true;
        double integer = value.toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        if (!std::isfinite(integer)) {
            throwRangeError(globalObject, scope, "Temporal time properties must be finite"_s);
            return { };
        }
        duration[unit] = integer;
    }

    if (!hasRelevantProperty && !skipRelevantPropertyCheck) {
        throwTypeError(globalObject, scope, "Object must contain at least one Temporal time property"_s);
        return { };
    }

    return duration;
}

std::array<std::optional<double>, numberOfTemporalPlainTimeUnits> TemporalPlainTime::toPartialTime(JSGlobalObject* globalObject, JSObject* temporalTimeLike, bool skipRelevantPropertyCheck)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    bool hasAnyFields = false;
    std::array<std::optional<double>, numberOfTemporalPlainTimeUnits> partialTime { };
    for (TemporalUnit unit : temporalUnitsInTableOrder) {
        if (unit < TemporalUnit::Hour)
            continue;
        auto name = temporalUnitSingularPropertyName(vm, unit);
        JSValue value = temporalTimeLike->get(globalObject, name);
        RETURN_IF_EXCEPTION(scope, { });

        if (!value.isUndefined()) {
            hasAnyFields = true;
            double doubleValue = value.toIntegerOrInfinity(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            if (!std::isfinite(doubleValue)) {
                throwRangeError(globalObject, scope, "Temporal time properties must be finite"_s);
                return { };
            }
            partialTime[static_cast<unsigned>(unit) - static_cast<unsigned>(TemporalUnit::Hour)] = doubleValue;
        }
    }
    if (!hasAnyFields && !skipRelevantPropertyCheck) {
        throwTypeError(globalObject, scope, "Object must contain at least one Temporal time property"_s);
        return { };
    }
    return partialTime;
}

static ISO8601::PlainTime constrainTime(ISO8601::Duration&& duration)
{
    auto constrainToRange = [](double value, unsigned minimum, unsigned maximum) -> unsigned {
        if (std::isnan(value))
            return 0;
        return static_cast<unsigned>(std::min<double>(std::max<double>(value, minimum), maximum));
    };
    return ISO8601::PlainTime(
        constrainToRange(duration.hours(), 0, 23),
        constrainToRange(duration.minutes(), 0, 59),
        constrainToRange(duration.seconds(), 0, 59),
        constrainToRange(duration.milliseconds(), 0, 999),
        constrainToRange(duration.microseconds(), 0, 999),
        constrainToRange(duration.nanoseconds(), 0, 999));
}

ISO8601::PlainTime TemporalPlainTime::regulateTime(JSGlobalObject* globalObject, ISO8601::Duration&& duration, TemporalOverflow overflow)
{
    switch (overflow) {
    case TemporalOverflow::Constrain:
        return constrainTime(WTFMove(duration));
    case TemporalOverflow::Reject:
        return TemporalPlainTime::toPlainTime(globalObject, duration);
    }
    return { };
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporaltime
TemporalPlainTime* TemporalPlainTime::from(JSGlobalObject* globalObject, JSValue itemValue, std::optional<TemporalOverflow> overflowValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto overflow = overflowValue.value_or(TemporalOverflow::Constrain);

    if (itemValue.isObject()) {
        if (itemValue.inherits<TemporalPlainTime>())
            return jsCast<TemporalPlainTime*>(itemValue);

        if (itemValue.inherits<TemporalPlainDateTime>())
            return TemporalPlainTime::create(vm, globalObject->plainTimeStructure(), jsCast<TemporalPlainDateTime*>(itemValue)->plainTime());

        JSObject* calendar = TemporalCalendar::getTemporalCalendarWithISODefault(globalObject, itemValue);
        RETURN_IF_EXCEPTION(scope, { });
        JSString* calendarString = calendar->toString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        String calendarWTFString = calendarString->value(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        if (calendarWTFString != "iso8601"_s) {
            throwRangeError(globalObject, scope, "calendar is not iso8601"_s);
            return { };
        }
        auto duration = toTemporalTimeRecord(globalObject, jsCast<JSObject*>(itemValue));
        RETURN_IF_EXCEPTION(scope, { });
        auto plainTime = regulateTime(globalObject, WTFMove(duration), overflow);
        RETURN_IF_EXCEPTION(scope, { });
        return TemporalPlainTime::create(vm, globalObject->plainTimeStructure(), WTFMove(plainTime));
    }

    if (!itemValue.isString()) {
        throwTypeError(globalObject, scope, "can only convert to PlainTime from object or string values"_s);
        return { };
    }

    // https://tc39.es/proposal-temporal/#sec-temporal-parsetemporaltimestring
    // TemporalTimeString :
    //    CalendarTime
    //    CalendarDateTimeTimeRequired

    auto string = itemValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    auto time = ISO8601::parseCalendarTime(string);
    if (time) {
        auto [plainTime, timeZoneOptional, calendarOptional] = WTFMove(time.value());
        if (!(timeZoneOptional && timeZoneOptional->m_z))
            return TemporalPlainTime::create(vm, globalObject->plainTimeStructure(), WTFMove(plainTime));
    }

    auto dateTime = ISO8601::parseCalendarDateTime(string);
    if (dateTime) {
        auto [plainDate, plainTimeOptional, timeZoneOptional, calendarOptional] = WTFMove(dateTime.value());
        if (plainTimeOptional) {
            if (!(timeZoneOptional && timeZoneOptional->m_z))
                return TemporalPlainTime::create(vm, globalObject->plainTimeStructure(), WTFMove(plainTimeOptional.value()));
        }
    }

    throwRangeError(globalObject, scope, "invalid time string"_s);
    return { };
}

// https://tc39.es/proposal-temporal/#sec-temporal-comparetemporaltime
int32_t TemporalPlainTime::compare(const ISO8601::PlainTime& t1, const ISO8601::PlainTime& t2)
{
    if (t1.hour() > t2.hour())
        return 1;
    if (t1.hour() < t2.hour())
        return -1;
    if (t1.minute() > t2.minute())
        return 1;
    if (t1.minute() < t2.minute())
        return -1;
    if (t1.second() > t2.second())
        return 1;
    if (t1.second() < t2.second())
        return -1;
    if (t1.millisecond() > t2.millisecond())
        return 1;
    if (t1.millisecond() < t2.millisecond())
        return -1;
    if (t1.microsecond() > t2.microsecond())
        return 1;
    if (t1.microsecond() < t2.microsecond())
        return -1;
    if (t1.nanosecond() > t2.nanosecond())
        return 1;
    if (t1.nanosecond() < t2.nanosecond())
        return -1;
    return 0;
}

// https://tc39.es/proposal-temporal/#sec-temporal-addtime
ISO8601::Duration TemporalPlainTime::addTime(const ISO8601::PlainTime& plainTime, const ISO8601::Duration& duration)
{
    return balanceTime(
        plainTime.hour() + duration.hours(),
        plainTime.minute() + duration.minutes(),
        plainTime.second() + duration.seconds(),
        plainTime.millisecond() + duration.milliseconds(),
        plainTime.microsecond() + duration.microseconds(),
        plainTime.nanosecond() + duration.nanoseconds());
}

ISO8601::PlainTime TemporalPlainTime::with(JSGlobalObject* globalObject, JSObject* temporalTimeLike, JSValue optionsValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    rejectObjectWithCalendarOrTimeZone(globalObject, temporalTimeLike);
    RETURN_IF_EXCEPTION(scope, { });

    auto [hourOptional, minuteOptional, secondOptional, millisecondOptional, microsecondOptional, nanosecondOptional] = toPartialTime(globalObject, temporalTimeLike);
    RETURN_IF_EXCEPTION(scope, { });

    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });

    TemporalOverflow overflow = toTemporalOverflow(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    ISO8601::Duration duration { };
    duration.setHours(hourOptional.value_or(hour()));
    duration.setMinutes(minuteOptional.value_or(minute()));
    duration.setSeconds(secondOptional.value_or(second()));
    duration.setMilliseconds(millisecondOptional.value_or(millisecond()));
    duration.setMicroseconds(microsecondOptional.value_or(microsecond()));
    duration.setNanoseconds(nanosecondOptional.value_or(nanosecond()));

    RELEASE_AND_RETURN(scope, regulateTime(globalObject, WTFMove(duration), overflow));
}

static ISO8601::Duration differenceTime(ISO8601::PlainTime time1, ISO8601::PlainTime time2)
{
    double hours = static_cast<double>(time2.hour()) - static_cast<double>(time1.hour());
    double minutes = static_cast<double>(time2.minute()) - static_cast<double>(time1.minute());
    double seconds = static_cast<double>(time2.second()) - static_cast<double>(time1.second());
    double milliseconds = static_cast<double>(time2.millisecond()) - static_cast<double>(time1.millisecond());
    double microseconds = static_cast<double>(time2.microsecond()) - static_cast<double>(time1.microsecond());
    double nanoseconds = static_cast<double>(time2.nanosecond()) - static_cast<double>(time1.nanosecond());
    dataLogLnIf(TemporalPlainTimeInternal::verbose, "Diff ", hours, " ", minutes, " ", seconds, " ", milliseconds, " ", microseconds, " ", nanoseconds);
    int32_t sign = TemporalDuration::sign(ISO8601::Duration(0, 0, 0, 0, hours, minutes, seconds, milliseconds, microseconds, nanoseconds));
    auto duration = balanceTime(hours * sign, minutes * sign, seconds * sign, milliseconds * sign, microseconds * sign, nanoseconds * sign);
    dataLogLnIf(TemporalPlainTimeInternal::verbose, "Balanced ", duration.days(), " ", duration.hours(), " ", duration.minutes(), " ", duration.seconds(), " ", duration.milliseconds(), " ", duration.microseconds(), " ", duration.nanoseconds());
    if (sign == -1)
        return -duration;
    return duration;
}

ISO8601::Duration TemporalPlainTime::until(JSGlobalObject* globalObject, TemporalPlainTime* other, JSValue optionsValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto [smallestUnit, largestUnit, roundingMode, increment] = extractDifferenceOptions(globalObject, optionsValue, UnitGroup::Time, TemporalUnit::Nanosecond, TemporalUnit::Hour);
    RETURN_IF_EXCEPTION(scope, { });

    auto result = differenceTime(plainTime(), other->plainTime());
    result.setYears(0);
    result.setMonths(0);
    result.setWeeks(0);
    result.setDays(0);
    TemporalDuration::round(result, increment, smallestUnit, roundingMode);
    TemporalDuration::balance(result, largestUnit);
    return result;
}

ISO8601::Duration TemporalPlainTime::since(JSGlobalObject* globalObject, TemporalPlainTime* other, JSValue optionsValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto [smallestUnit, largestUnit, roundingMode, increment] = extractDifferenceOptions(globalObject, optionsValue, UnitGroup::Time, TemporalUnit::Nanosecond, TemporalUnit::Hour);
    RETURN_IF_EXCEPTION(scope, { });
    roundingMode = negateTemporalRoundingMode(roundingMode);

    auto result = differenceTime(other->plainTime(), plainTime());
    result = -result;
    result.setYears(0);
    result.setMonths(0);
    result.setWeeks(0);
    result.setDays(0);
    TemporalDuration::round(result, increment, smallestUnit, roundingMode);
    result = -result;
    result.setDays(0);
    TemporalDuration::balance(result, largestUnit);
    return result;
}

} // namespace JSC
