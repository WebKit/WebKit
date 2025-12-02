/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#include "TemporalDuration.h"

#include "DateConstructor.h"
#include "FractionToDouble.h"
#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "TemporalCalendar.h"
#include "TemporalObject.h"
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace JSC {

const ClassInfo TemporalDuration::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(TemporalDuration) };

TemporalDuration* TemporalDuration::create(VM& vm, Structure* structure, ISO8601::Duration&& duration)
{
    auto* object = new (NotNull, allocateCell<TemporalDuration>(vm)) TemporalDuration(vm, structure, WTFMove(duration));
    object->finishCreation(vm);
    return object;
}

Structure* TemporalDuration::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalDuration::TemporalDuration(VM& vm, Structure* structure, ISO8601::Duration&& duration)
    : Base(vm, structure)
    , m_duration(WTFMove(duration))
{
}

// CreateTemporalDuration ( years, months, weeks, days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds [ , newTarget ] )
// https://tc39.es/proposal-temporal/#sec-temporal-createtemporalduration
TemporalDuration* TemporalDuration::tryCreateIfValid(JSGlobalObject* globalObject, ISO8601::Duration&& duration, Structure* structure)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!ISO8601::isValidDuration(duration)) {
        throwRangeError(globalObject, scope, "Temporal.Duration properties must be finite and of consistent sign"_s);
        return { };
    }

    return TemporalDuration::create(vm, structure ? structure : globalObject->durationStructure(), WTFMove(duration));
}

// ToTemporalDurationRecord ( temporalDurationLike )
// https://tc39.es/proposal-temporal/#sec-temporal-totemporaldurationrecord
ISO8601::Duration TemporalDuration::fromDurationLike(JSGlobalObject* globalObject, JSObject* durationLike)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (durationLike->inherits<TemporalDuration>())
        return jsCast<TemporalDuration*>(durationLike)->m_duration;

    ISO8601::Duration result;
    auto hasRelevantProperty = false;
    for (TemporalUnit unit : temporalUnitsInTableOrder) {
        JSValue value = durationLike->get(globalObject, temporalUnitPluralPropertyName(vm, unit));
        RETURN_IF_EXCEPTION(scope, { });

        if (value.isUndefined())
            continue;

        hasRelevantProperty = true;
        result[unit] = value.toNumber(globalObject) + 0.0;
        RETURN_IF_EXCEPTION(scope, { });

        if (!isInteger(result[unit])) {
            throwRangeError(globalObject, scope, "Temporal.Duration properties must be integers"_s);
            return { };
        }
    }

    if (!hasRelevantProperty) {
        throwTypeError(globalObject, scope, "Object must contain at least one Temporal.Duration property"_s);
        return { };
    }

    return result;
}

// ToLimitedTemporalDuration ( temporalDurationLike, disallowedFields )
// https://tc39.es/proposal-temporal/#sec-temporal-tolimitedtemporalduration
ISO8601::Duration TemporalDuration::toISO8601Duration(JSGlobalObject* globalObject, JSValue itemValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ISO8601::Duration duration;
    if (itemValue.isObject()) {
        duration = fromDurationLike(globalObject, asObject(itemValue));
        RETURN_IF_EXCEPTION(scope, { });
    } else {
        if (!itemValue.isString()) {
            throwTypeError(globalObject, scope, "can only convert to Duration from object or string values"_s);
            return { };
        }

        String string = itemValue.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        auto parsedDuration = ISO8601::parseDuration(string);
        if (!parsedDuration) {
            // 3090: 308 digits * 10 fields + 10 designators
            throwRangeError(globalObject, scope, makeString("'"_s, ellipsizeAt(3090, string), "' is not a valid Duration string"_s));
            return { };
        }

        duration = parsedDuration.value();
    }

    if (!ISO8601::isValidDuration(duration)) {
        throwRangeError(globalObject, scope, "Temporal.Duration properties must be finite and of consistent sign"_s);
        return { };
    }

    return duration;
}

// ToTemporalDuration ( item )
// https://tc39.es/proposal-temporal/#sec-temporal-totemporalduration
TemporalDuration* TemporalDuration::toTemporalDuration(JSGlobalObject* globalObject, JSValue itemValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (itemValue.inherits<TemporalDuration>())
        return jsCast<TemporalDuration*>(itemValue);

    auto result = toISO8601Duration(globalObject, itemValue);
    RETURN_IF_EXCEPTION(scope, nullptr);

    return TemporalDuration::create(vm, globalObject->durationStructure(), WTFMove(result));
}

// ToLimitedTemporalDuration ( temporalDurationLike, disallowedFields )
// https://tc39.es/proposal-temporal/#sec-temporal-tolimitedtemporalduration
ISO8601::Duration TemporalDuration::toLimitedDuration(JSGlobalObject* globalObject, JSValue itemValue, std::initializer_list<TemporalUnit> disallowedUnits)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ISO8601::Duration duration = toISO8601Duration(globalObject, itemValue);
    RETURN_IF_EXCEPTION(scope, { });

    if (!isValidDuration(duration)) {
        throwRangeError(globalObject, scope, "Temporal.Duration properties must be finite and of consistent sign"_s);
        return { };
    }

    for (TemporalUnit unit : disallowedUnits) {
        if (duration[unit]) {
            throwRangeError(globalObject, scope, makeString("Adding "_s, temporalUnitPluralPropertyName(vm, unit).publicName(), " not supported by Temporal.Instant. Try Temporal.ZonedDateTime instead"_s));
            return { };
        }
    }

    return duration;
}

TemporalDuration* TemporalDuration::from(JSGlobalObject* globalObject, JSValue itemValue)
{
    VM& vm = globalObject->vm();

    if (itemValue.inherits<TemporalDuration>()) {
        ISO8601::Duration cloned = jsCast<TemporalDuration*>(itemValue)->m_duration;
        return TemporalDuration::create(vm, globalObject->durationStructure(), WTFMove(cloned));
    }

    return toTemporalDuration(globalObject, itemValue);
}

static double totalSeconds(ISO8601::Duration& duration)
{
    auto hours = 24 * duration.days() + duration.hours();
    auto minutes = 60 * hours + duration.minutes();
    return 60 * minutes + duration.seconds();
}

static double totalSubseconds(ISO8601::Duration& duration)
{
    auto milliseconds = duration.milliseconds();
    auto microseconds = 1000 * milliseconds + duration.microseconds();
    return 1000 * microseconds + duration.nanoseconds();
}

// https://tc39.es/proposal-temporal/#sec-temporal-add24hourdaystonormalizedtimeduration
static Int128 add24HourDaysToTimeDuration(JSGlobalObject* globalObject, Int128 d, double days)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    CheckedInt128 daysInNanoseconds = checkedCastDoubleToInt128(days) * ISO8601::ExactTime::nsPerDay;
    CheckedInt128 result = d + daysInNanoseconds;
    ASSERT(!result.hasOverflowed());
    if (absInt128(result) > ISO8601::InternalDuration::maxTimeDuration) [[unlikely]] {
        throwRangeError(globalObject, scope, "Total time in duration is out of range"_s);
        return { };
    }
    return result;
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.compare
JSValue TemporalDuration::compare(JSGlobalObject* globalObject, JSValue valueOne, JSValue valueTwo)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* one = toTemporalDuration(globalObject, valueOne);
    RETURN_IF_EXCEPTION(scope, { });

    auto* two = toTemporalDuration(globalObject, valueTwo);
    RETURN_IF_EXCEPTION(scope, { });

    // FIXME: Implement relativeTo parameter after PlainDateTime / ZonedDateTime.
    if (one->years() || two->years() || one->months() || two->months() || one->weeks() || two->weeks()) {
        throwRangeError(globalObject, scope, "Cannot compare a duration of years, months, or weeks without a relativeTo option"_s);
        return { };
    }

    auto duration1 = toInternalDuration(one->m_duration);
    auto duration2 = toInternalDuration(two->m_duration);
    auto days1 = one->days();
    auto days2 = two->days();
    auto timeDuration1 = add24HourDaysToTimeDuration(globalObject, duration1.time(), days1);
    RETURN_IF_EXCEPTION(scope, { });
    auto timeDuration2 = add24HourDaysToTimeDuration(globalObject, duration2.time(), days2);
    RETURN_IF_EXCEPTION(scope, { });

    return jsNumber(timeDuration1 > timeDuration2 ? 1 : timeDuration1 < timeDuration2 ? -1 : 0);
}

int TemporalDuration::sign(const ISO8601::Duration& duration)
{
    for (auto value : duration) {
        if (value < 0)
            return -1;

        if (value > 0)
            return 1;
    }

    return 0;
}

ISO8601::Duration TemporalDuration::with(JSGlobalObject* globalObject, JSObject* durationLike) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ISO8601::Duration result;
    auto hasRelevantProperty = false;
    for (TemporalUnit unit : temporalUnitsInTableOrder) {
        JSValue value = durationLike->get(globalObject, temporalUnitPluralPropertyName(vm, unit));
        RETURN_IF_EXCEPTION(scope, { });

        if (value.isUndefined()) {
            result[unit] = m_duration[unit];
            continue;
        }

        hasRelevantProperty = true;
        result[unit] = value.toNumber(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        if (!isInteger(result[unit])) {
            throwRangeError(globalObject, scope, "Temporal.Duration properties must be integers"_s);
            return { };
        }
    }

    if (!hasRelevantProperty) {
        throwTypeError(globalObject, scope, "Object must contain at least one Temporal.Duration property"_s);
        return { };
    }

    return result;
}

ISO8601::Duration TemporalDuration::negated() const
{
    return -m_duration;
}

ISO8601::Duration TemporalDuration::abs() const
{
    ISO8601::Duration result;
    for (size_t i = 0; i < numberOfTemporalUnits; i++)
        result[i] = std::abs(m_duration[i]);
    return result;
}

// DefaultTemporalLargestUnit ( years, months, weeks, days, hours, minutes, seconds, milliseconds, microseconds )
// https://tc39.es/proposal-temporal/#sec-temporal-defaulttemporallargestunit
static TemporalUnit largestSubduration(const ISO8601::Duration& duration)
{
    uint8_t index = 0;
    while (index < numberOfTemporalUnits - 1 && !duration[index])
        index++;
    return static_cast<TemporalUnit>(index);
}

// https://tc39.es/proposal-temporal/#sec-temporal-timedurationfromcomponents
Int128 TemporalDuration::timeDurationFromComponents(double hours, double minutes, double seconds, double milliseconds, double microseconds, double nanoseconds)
{
    CheckedInt128 min = checkedCastDoubleToInt128(minutes) + checkedCastDoubleToInt128(hours) * Int128 { 60 };
    CheckedInt128 sec = checkedCastDoubleToInt128(seconds) + min * Int128 { 60 };
    CheckedInt128 millis = checkedCastDoubleToInt128(milliseconds) + sec * Int128 { 1000 };
    CheckedInt128 micros = checkedCastDoubleToInt128(microseconds) + millis * Int128 { 1000 };
    CheckedInt128 nanos = checkedCastDoubleToInt128(nanoseconds) + micros * Int128 { 1000 };
    // The components come from a TemporalDuration, so as per
    // TemporalDuration::tryCreateIfValid(), these components can at most add up
    // to ISO8601::InternalDuration::maxTimeDuration
    ASSERT(!nanos.hasOverflowed());
    ASSERT(absInt128(nanos) <= ISO8601::InternalDuration::maxTimeDuration);
    return nanos;
}

// https://tc39.es/proposal-temporal/#sec-temporal-tointernaldurationrecordwith24hourdays
ISO8601::InternalDuration TemporalDuration::toInternalDurationRecordWith24HourDays(JSGlobalObject* globalObject,
    ISO8601::Duration d)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Int128 timeDuration = timeDurationFromComponents(d.hours(), d.minutes(), d.seconds(),
        d.milliseconds(), d.microseconds(), d.nanoseconds());
    timeDuration = add24HourDaysToTimeDuration(globalObject, timeDuration, d.days());
    RETURN_IF_EXCEPTION(scope, { });
    ISO8601::Duration dateDuration = ISO8601::Duration { d.years(), d.months(), d.weeks(),
        0, 0, 0, 0, 0, 0, 0 };
    return ISO8601::InternalDuration::combineDateAndTimeDuration(dateDuration,
        timeDuration);
}

// https://tc39.es/proposal-temporal/#sec-temporal-regulateisodate
std::optional<ISO8601::PlainDate> TemporalDuration::regulateISODate(double year, double month, double day, TemporalOverflow overflow)
{
    if (overflow == TemporalOverflow::Constrain) {
        if (month < 1)
            month = 1;
        if (month > 12)
            month = 12;
        auto daysInMonth = ISO8601::daysInMonth(year, month);
        if (day < 1)
            day = 1;
        if (day > daysInMonth)
            day = daysInMonth;
    } else if (!ISO8601::isValidISODate(year, month, day))
        return std::nullopt;
    return ISO8601::createISODateRecord(year, month, day);
}

// https://tc39.es/proposal-temporal/#sec-temporal-todatedurationrecordwithouttime
// ToDateDurationRecordWithoutTime ( duration )
ISO8601::Duration TemporalDuration::toDateDurationRecordWithoutTime(JSGlobalObject* globalObject, const ISO8601::Duration& duration)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto internalDuration = toInternalDurationRecordWith24HourDays(globalObject, duration);
    RETURN_IF_EXCEPTION(scope, { });
    auto days = internalDuration.time() / ISO8601::ExactTime::nsPerDay;
    return ISO8601::Duration { internalDuration.dateDuration().years(), internalDuration.dateDuration().months(), internalDuration.dateDuration().weeks(), static_cast<double>(days), 0, 0, 0, 0, 0, 0 };
}

// BalanceDuration ( days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds, largestUnit [ , relativeTo ] )
// https://tc39.es/proposal-temporal/#sec-temporal-balanceduration
std::optional<double> TemporalDuration::balance(ISO8601::Duration& duration, TemporalUnit largestUnit)
{

    auto nanoseconds = totalSubseconds(duration);
    auto seconds = totalSeconds(duration);

    if (!std::isfinite(nanoseconds))
        return nanoseconds;
    duration.clear();

    if (largestUnit <= TemporalUnit::Day) {
        duration.setDays(std::trunc(seconds / secondsPerDay));
        seconds = std::fmod(seconds, secondsPerDay);
    }

    double microseconds = std::trunc(nanoseconds / 1000);
    double milliseconds = std::trunc(microseconds / 1000);
    double minutes = std::trunc(seconds / 60);
    if (largestUnit <= TemporalUnit::Hour) {
        duration.setNanoseconds(std::fmod(nanoseconds, 1000));
        duration.setMicroseconds(std::fmod(microseconds, 1000));
        duration.setMilliseconds(std::fmod(milliseconds, 1000));
        duration.setSeconds(std::fmod(seconds, 60));
        duration.setMinutes(std::fmod(minutes, 60));
        duration.setHours(std::trunc(minutes / 60));
    } else if (largestUnit == TemporalUnit::Minute) {
        duration.setNanoseconds(std::fmod(nanoseconds, 1000));
        duration.setMicroseconds(std::fmod(microseconds, 1000));
        duration.setMilliseconds(std::fmod(milliseconds, 1000));
        duration.setSeconds(std::fmod(seconds, 60));
        duration.setMinutes(minutes);
    } else if (largestUnit == TemporalUnit::Second) {
        duration.setNanoseconds(std::fmod(nanoseconds, 1000));
        duration.setMicroseconds(std::fmod(microseconds, 1000));
        duration.setMilliseconds(std::fmod(milliseconds, 1000));
        duration.setSeconds(seconds);
    } else if (largestUnit == TemporalUnit::Millisecond) {
        duration.setNanoseconds(std::fmod(nanoseconds, 1000));
        duration.setMicroseconds(std::fmod(microseconds, 1000));
        milliseconds += seconds * 1000;
        duration.setMilliseconds(milliseconds);
    } else if (largestUnit == TemporalUnit::Microsecond) {
        duration.setNanoseconds(std::fmod(nanoseconds, 1000));
        microseconds += seconds * 1000 * 1000;
        duration.setMicroseconds(microseconds);
    } else {
        nanoseconds += seconds * 1000 * 1000 * 1000;
        duration.setNanoseconds(nanoseconds);
    }

    return std::nullopt;
}

ISO8601::Duration TemporalDuration::add(JSGlobalObject* globalObject, JSValue otherValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto other = toISO8601Duration(globalObject, otherValue);
    RETURN_IF_EXCEPTION(scope, { });

    // FIXME: Implement relativeTo parameter after PlainDateTime / ZonedDateTime.
    auto largestUnit = std::min(largestSubduration(m_duration), largestSubduration(other));
    if (largestUnit <= TemporalUnit::Week) {
        throwRangeError(globalObject, scope, "Cannot add a duration of years, months, or weeks without a relativeTo option"_s);
        return { };
    }

    RELEASE_AND_RETURN(scope, addDurations(globalObject, AddOrSubtract::Add, other, largestUnit));
}

// https://tc39.es/proposal-temporal/#sec-temporal-adddurations
/* static */ ISO8601::Duration TemporalDuration::addDurations(JSGlobalObject* globalObject,
    AddOrSubtract op, ISO8601::Duration other, TemporalUnit largestUnit) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (op == AddOrSubtract::Subtract)
        other = -other;

    auto d1 = toInternalDurationRecordWith24HourDays(globalObject, m_duration);
    RETURN_IF_EXCEPTION(scope, { });
    auto d2 = toInternalDurationRecordWith24HourDays(globalObject, other);
    RETURN_IF_EXCEPTION(scope, { });
    auto timeResult = d1.time() + d2.time();
    if (absInt128(timeResult) > ISO8601::InternalDuration::maxTimeDuration) {
        throwRangeError(globalObject, scope, "Sum of durations exceeds maximum time duration"_s);
        return { };
    }

    auto result = ISO8601::InternalDuration::combineDateAndTimeDuration(ISO8601::Duration(),
        timeResult);
    return temporalDurationFromInternal(result, largestUnit);
}

ISO8601::InternalDuration TemporalDuration::toInternalDuration(ISO8601::Duration d)
{
    auto timeDuration = timeDurationFromComponents(d.hours(), d.minutes(), d.seconds(), d.milliseconds(), d.microseconds(), d.nanoseconds());
    return ISO8601::InternalDuration::combineDateAndTimeDuration(d, timeDuration);
}

// https://tc39.es/proposal-temporal/#sec-temporal-temporaldurationfrominternal
ISO8601::Duration TemporalDuration::temporalDurationFromInternal(ISO8601::InternalDuration internalDuration,
    TemporalUnit largestUnit)
{
    double days = 0;
    double hours = 0;
    double minutes = 0;
    double seconds = 0;
    Int128 milliseconds = 0;
    Int128 microseconds = 0;

    int32_t sign = internalDuration.timeDurationSign();
    Int128 nanoseconds = absInt128(internalDuration.time());

    if (largestUnit <= TemporalUnit::Day) {
        microseconds = nanoseconds / 1000;
        nanoseconds = nanoseconds % 1000;
        milliseconds = microseconds / 1000;
        microseconds = microseconds % 1000;
        seconds = (double) (milliseconds / 1000);
        milliseconds = milliseconds % 1000;
        minutes = std::trunc(seconds / 60);
        seconds = std::fmod(seconds, 60);
        hours = std::trunc(minutes / 60);
        minutes = std::fmod(minutes, 60);
        days = std::trunc(hours / 24);
        hours = std::fmod(hours, 24);
    } else if (largestUnit == TemporalUnit::Hour) {
        microseconds = nanoseconds / 1000;
        nanoseconds = nanoseconds % 1000;
        milliseconds = microseconds / 1000;
        microseconds = microseconds % 1000;
        seconds = (double) (milliseconds / 1000);
        milliseconds = milliseconds % 1000;
        minutes = std::trunc(seconds / 60);
        seconds = std::fmod(seconds, 60);
        hours = std::trunc(minutes / 60);
        minutes = std::fmod(minutes, 60);
    } else if (largestUnit == TemporalUnit::Minute) {
        microseconds = nanoseconds / 1000;
        nanoseconds = nanoseconds % 1000;
        milliseconds = microseconds / 1000;
        microseconds = microseconds % 1000;
        seconds = (double) (milliseconds / 1000);
        milliseconds = milliseconds % 1000;
        minutes = std::trunc(seconds / 60);
        seconds = std::fmod(seconds, 60);
    } else if (largestUnit == TemporalUnit::Second) {
        microseconds = nanoseconds / 1000;
        nanoseconds = nanoseconds % 1000;
        milliseconds = microseconds / 1000;
        microseconds = microseconds % 1000;
        seconds = (double) (milliseconds / 1000);
        milliseconds = milliseconds % 1000;
    } else if (largestUnit == TemporalUnit::Millisecond) {
        microseconds = nanoseconds / 1000;
        nanoseconds = nanoseconds % 1000;
        milliseconds = microseconds / 1000;
        microseconds = microseconds % 1000;
    } else if (largestUnit == TemporalUnit::Microsecond) {
        microseconds = nanoseconds / 1000;
        nanoseconds = nanoseconds % 1000;
    }
    // Otherwise, unit is nanoseconds -- nothing to do

    // Avoid negative 0
    if (hours)
        hours *= sign;
    if (minutes)
        minutes *= sign;
    if (seconds)
        seconds *= sign;
    if (milliseconds)
        milliseconds *= sign;
    if (microseconds)
        microseconds *= sign;
    if (nanoseconds)
        nanoseconds *= sign;
    return ISO8601::Duration { internalDuration.dateDuration().years(),
        internalDuration.dateDuration().months(), internalDuration.dateDuration().weeks(),
        internalDuration.dateDuration().days() + days * sign, hours, minutes,
        static_cast<double>(seconds), static_cast<double>(milliseconds),
        static_cast<double>(microseconds), static_cast<double>(nanoseconds) };
}

ISO8601::Duration TemporalDuration::subtract(JSGlobalObject* globalObject, JSValue otherValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto other = toISO8601Duration(globalObject, otherValue);
    RETURN_IF_EXCEPTION(scope, { });

    // FIXME: Implement relativeTo parameter after PlainDateTime / ZonedDateTime.
    auto largestUnit = std::min(largestSubduration(m_duration), largestSubduration(other));
    if (largestUnit <= TemporalUnit::Week) {
        throwRangeError(globalObject, scope, "Cannot subtract a duration of years, months, or weeks without a relativeTo option"_s);
        return { };
    }

    RELEASE_AND_RETURN(scope, addDurations(globalObject, AddOrSubtract::Subtract, other, largestUnit));
}

// https://tc39.es/proposal-temporal/#sec-temporal-totaltimeduration
static double totalTimeDuration(Int128 timeDuration, TemporalUnit unit)
{
    double divisor = static_cast<double>(lengthInNanoseconds(unit));
    // guaranteed, maximum lengthInNanoseconds is 86400e9
    ASSERT(isSafeInteger(divisor));
    return fractionToDouble(timeDuration, divisor);
}

static ISO8601::Duration adjustDateDurationRecord(JSGlobalObject* globalObject, const ISO8601::Duration& dateDuration, double days, std::optional<double> weeks, std::optional<double> months)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto result = ISO8601::Duration { dateDuration.years(), months ? months.value() : dateDuration.months(), weeks ? weeks.value() : dateDuration.weeks(), days, 0, 0, 0, 0, 0, 0 };
    if (!ISO8601::isValidDuration(result)) {
        throwRangeError(globalObject, scope, "Temporal.Duration properties must be valid and of consistent sign"_s);
        return { };
    }
    return result;
}

static std::tuple<ISO8601::PlainDate, ISO8601::PlainTime> combineISODateAndTimeRecord(ISO8601::PlainDate isoDate, ISO8601::PlainTime isoTime)
{
    return std::tuple<ISO8601::PlainDate, ISO8601::PlainTime>(isoDate, isoTime);
}

Int128 getUTCEpochNanoseconds(std::tuple<ISO8601::PlainDate, ISO8601::PlainTime> isoDateTime)
{
    auto isoDate = std::get<0>(isoDateTime);
    auto isoTime = std::get<1>(isoDateTime);
    auto date = makeDay(isoDate.year(), isoDate.month() - 1, isoDate.day());
    auto time = makeTime(isoTime.hour(), isoTime.minute(), isoTime.second(), isoTime.millisecond());
    auto ms = makeDate(date, time);
    ASSERT(isInteger(ms));
    return (((Int128) ms) * 1000000
        + ((Int128) isoTime.microsecond()) * 1000
        + ((Int128) isoTime.nanosecond()));
}

static Int128 getEpochNanosecondsFor(std::tuple<ISO8601::PlainDate, ISO8601::PlainTime> isoDateTime, TemporalDisambiguation disambiguation)
{
    // FIXME time zones
    (void) disambiguation;
    return getUTCEpochNanoseconds(isoDateTime);
}

// https://tc39.es/proposal-temporal/#sec-temporal-nudgetocalendarunit
// NudgeToCalendarUnit ( sign, duration, destEpochNs, isoDateTime, timeZone, calendar, increment, unit, roundingMode )
static Nudged nudgeToCalendarUnit(JSGlobalObject* globalObject, int32_t sign, const ISO8601::InternalDuration& duration, Int128 destEpochNs, ISO8601::PlainDate isoDate, double increment, TemporalUnit unit, RoundingMode roundingMode)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    double r1 = 0;
    double r2 = 0;
    ISO8601::Duration startDuration;
    ISO8601::Duration endDuration;
    switch (unit) {
    case TemporalUnit::Year: {
        Int128 years = roundNumberToIncrementInt128((Int128) duration.dateDuration().years(),
            (Int128) increment, RoundingMode::Trunc);
        r1 = (double) years;
        r2 = (double) years + increment * sign;
        startDuration = ISO8601::Duration { r1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        endDuration = ISO8601::Duration { r2, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        break;
    }
    case TemporalUnit::Month: {
        Int128 months = roundNumberToIncrementInt128((Int128) duration.dateDuration().months(),
            (Int128) increment, RoundingMode::Trunc);
        r1 = (double) months;
        r2 = (double) months + increment * sign;
        startDuration = adjustDateDurationRecord(globalObject, duration.dateDuration(), 0, 0, r1);
        RETURN_IF_EXCEPTION(scope, { });
        endDuration = adjustDateDurationRecord(globalObject, duration.dateDuration(), 0, 0, r2);
        RETURN_IF_EXCEPTION(scope, { });
        break;
    }
    case TemporalUnit::Week: {
        auto yearsMonths = adjustDateDurationRecord(globalObject, duration.dateDuration(), 0, 0, std::nullopt);
        RETURN_IF_EXCEPTION(scope, { });
        auto weeksStart = TemporalCalendar::isoDateAdd(globalObject, isoDate, yearsMonths, TemporalOverflow::Constrain);
        RETURN_IF_EXCEPTION(scope, { });
        auto weeksEnd = TemporalCalendar::balanceISODate(globalObject, weeksStart.year(), weeksStart.month(), weeksStart.day() + duration.dateDuration().days());
        auto untilResult = TemporalCalendar::calendarDateUntil(weeksStart, weeksEnd, TemporalUnit::Week);
        Int128 weeks = roundNumberToIncrementInt128((Int128) (duration.dateDuration().weeks() + untilResult.weeks()),
            (Int128) increment, RoundingMode::Trunc);
        r1 = (double) weeks;
        r2 = (double) weeks + increment * sign;
        startDuration = adjustDateDurationRecord(globalObject, duration.dateDuration(), 0, r1, std::nullopt);
        RETURN_IF_EXCEPTION(scope, { });
        endDuration = adjustDateDurationRecord(globalObject, duration.dateDuration(), 0, r2, std::nullopt);
        RETURN_IF_EXCEPTION(scope, { });
        break;
    }
    default: {
        ASSERT(unit == TemporalUnit::Day);
        Int128 days = roundNumberToIncrementInt128((Int128) duration.dateDuration().days(),
            (Int128) increment, RoundingMode::Trunc);
        r1 = (double) days;
        r2 = (double) days + increment * sign;
        startDuration = adjustDateDurationRecord(globalObject, duration.dateDuration(), r1, std::nullopt, std::nullopt);
        RETURN_IF_EXCEPTION(scope, { });
        endDuration = adjustDateDurationRecord(globalObject, duration.dateDuration(), r2, std::nullopt, std::nullopt);
        RETURN_IF_EXCEPTION(scope, { });
        break;
    }
    }
    ASSERT(sign != 1 || (r1 >= 0 && r1 < r2));
    ASSERT(sign != -1 || (r1 <= 0 && r1 > r2));
    auto start = TemporalCalendar::isoDateAdd(globalObject, isoDate, startDuration, TemporalOverflow::Constrain);
    RETURN_IF_EXCEPTION(scope, { });
    auto end = TemporalCalendar::isoDateAdd(globalObject, isoDate, endDuration, TemporalOverflow::Constrain);
    RETURN_IF_EXCEPTION(scope, { });
    auto startDateTime = combineISODateAndTimeRecord(start, ISO8601::PlainTime());
    auto endDateTime = combineISODateAndTimeRecord(end, ISO8601::PlainTime());
    Int128 startEpochNs = getEpochNanosecondsFor(startDateTime, TemporalDisambiguation::Compatible);
    Int128 endEpochNs = getEpochNanosecondsFor(endDateTime, TemporalDisambiguation::Compatible);
    ASSERT(sign != 1 || ((startEpochNs <= destEpochNs) && (destEpochNs <= endEpochNs)));
    ASSERT(sign == 1 || ((endEpochNs <= destEpochNs) && (destEpochNs <= startEpochNs)));
    ASSERT(startEpochNs != endEpochNs);
    // See 18. NOTE
    Int128 progressNumerator = destEpochNs - startEpochNs;
    Int128 progressDenominator = endEpochNs - startEpochNs;
    double total = r1 + (((double) progressNumerator) / ((double) progressDenominator)) * increment * sign;
    Int128 progress = progressNumerator / progressDenominator;
    ASSERT(0 <= progress && progress <= 1);
    auto isNegative = sign < 0;
    UnsignedRoundingMode unsignedRoundingMode = getUnsignedRoundingMode(roundingMode, isNegative);
    double roundedUnit = std::abs(r2);
    if (progress != 1) {
        ASSERT(std::abs(r1) <= std::abs(total) && std::abs(total) < std::abs(r2));
        roundedUnit = applyUnsignedRoundingMode(
            std::abs(total), std::abs(r1), std::abs(r2), unsignedRoundingMode);
    }
    bool didExpandCalendarUnit = true;
    ISO8601::Duration resultDuration = endDuration;
    Int128 nudgedEpochNs = endEpochNs;
    if (roundedUnit != std::abs(r2)) {
        didExpandCalendarUnit = false;
        resultDuration = startDuration;
        nudgedEpochNs = startEpochNs;
    }
    ISO8601::InternalDuration resultDurationInternal = ISO8601::InternalDuration::combineDateAndTimeDuration(resultDuration, 0);
    RETURN_IF_EXCEPTION(scope, { });
    auto nudgeResult = NudgeResult(resultDurationInternal, nudgedEpochNs, didExpandCalendarUnit);
    return Nudged(nudgeResult, total);
}

// https://tc39.es/proposal-temporal/#sec-temporal-nudgetodayortime
//  NudgeToDayOrTime ( duration, destEpochNs, largestUnit, increment, smallestUnit, roundingMode )
static NudgeResult nudgeToDayOrTime(JSGlobalObject* globalObject, ISO8601::InternalDuration duration, Int128 destEpochNs, TemporalUnit largestUnit, double increment, TemporalUnit smallestUnit, RoundingMode roundingMode)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Int128 timeDuration = add24HourDaysToTimeDuration(globalObject, duration.time(), duration.dateDuration().days());
    RETURN_IF_EXCEPTION(scope, { });
    Int128 unitLength = lengthInNanoseconds(smallestUnit);
    Int128 roundedTime = roundNumberToIncrementInt128(timeDuration,
        unitLength * (Int128) std::trunc(increment), roundingMode);
    Int128 diffTime = roundedTime - timeDuration;
    double wholeDays = totalTimeDuration(timeDuration, TemporalUnit::Day);
    double roundedWholeDays = totalTimeDuration(roundedTime, TemporalUnit::Day);
    auto dayDelta = roundedWholeDays - wholeDays;
    auto dayDeltaSign = dayDelta < 0 ? -1 : dayDelta > 0 ? 1 : 0;
    bool didExpandDays = dayDeltaSign == (timeDuration < 0 ? -1 : timeDuration > 0 ? 1 : 0);
    auto nudgedEpochNs = diffTime + destEpochNs;
    auto days = 0;
    auto remainder = roundedTime;
    if (largestUnit <= TemporalUnit::Day) {
        days = roundedWholeDays;
        remainder = roundedTime + TemporalDuration::timeDurationFromComponents(-roundedWholeDays * WTF::hoursPerDay, 0, 0, 0, 0, 0);
    }
    auto dateDuration = adjustDateDurationRecord(globalObject, duration.dateDuration(), days, std::nullopt, std::nullopt);
    RETURN_IF_EXCEPTION(scope, { });
    auto resultDuration = ISO8601::InternalDuration::combineDateAndTimeDuration(dateDuration, remainder);
    RETURN_IF_EXCEPTION(scope, { });
    return NudgeResult(resultDuration, nudgedEpochNs, didExpandDays);
}

static constexpr int32_t unitIndexInTable(TemporalUnit unit)
{
    switch (unit) {
    case TemporalUnit::Year:
        return 0;
    case TemporalUnit::Month:
        return 1;
    case TemporalUnit::Week:
        return 2;
    case TemporalUnit::Day:
        return 3;
    case TemporalUnit::Hour:
        return 4;
    case TemporalUnit::Minute:
        return 5;
    case TemporalUnit::Second:
        return 6;
    case TemporalUnit::Millisecond:
        return 7;
    case TemporalUnit::Microsecond:
        return 8;
    case TemporalUnit::Nanosecond:
        return 9;
    default: {
        RELEASE_ASSERT_NOT_REACHED();
    }
    }
}

static constexpr TemporalUnit unitInTable(int32_t i)
{
    switch (i) {
    case 0:
        return TemporalUnit::Year;
    case 1:
        return TemporalUnit::Month;
    case 2:
        return TemporalUnit::Week;
    case 3:
        return TemporalUnit::Day;
    case 4:
        return TemporalUnit::Hour;
    case 5:
        return TemporalUnit::Minute;
    case 6:
        return TemporalUnit::Second;
    case 7:
        return TemporalUnit::Millisecond;
    case 8:
        return TemporalUnit::Microsecond;
    case 9:
        return TemporalUnit::Nanosecond;
    default: {
        RELEASE_ASSERT_NOT_REACHED();
    }
    }
}

// https://tc39.es/proposal-temporal/#sec-temporal-bubblerelativeduration
// BubbleRelativeDuration ( sign, duration, nudgedEpochNs, isoDateTime, timeZone, calendar, largestUnit, smallestUnit )
static ISO8601::InternalDuration bubbleRelativeDuration(JSGlobalObject* globalObject, int32_t sign, ISO8601::InternalDuration duration, Int128 nudgedEpochNs, ISO8601::PlainDate isoDate, TemporalUnit largestUnit, TemporalUnit smallestUnit)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (smallestUnit == largestUnit)
        return duration;
    auto largestUnitIndex = unitIndexInTable(largestUnit);
    auto smallestUnitIndex = unitIndexInTable(smallestUnit);
    auto unitIndex = smallestUnitIndex - 1;
    bool done = false;
    ISO8601::Duration endDuration;
    while (unitIndex >= largestUnitIndex && !done) {
        auto unit = unitInTable(unitIndex);
        if (unit != TemporalUnit::Week || largestUnit == TemporalUnit::Week) {
            if (unit == TemporalUnit::Year) {
                auto years = duration.dateDuration().years() + sign;
                endDuration = ISO8601::Duration { years, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
            } else if (unit == TemporalUnit::Month) {
                auto months = duration.dateDuration().months() + sign;
                endDuration = adjustDateDurationRecord(globalObject, duration.dateDuration(), 0, 0, months);
                RETURN_IF_EXCEPTION(scope, { });
            } else {
                ASSERT(unit == TemporalUnit::Week);
                auto weeks = duration.dateDuration().weeks() + sign;
                endDuration = adjustDateDurationRecord(globalObject, duration.dateDuration(), 0, weeks, std::nullopt);
                RETURN_IF_EXCEPTION(scope, { });
            }
            auto end = TemporalCalendar::isoDateAdd(globalObject, isoDate, endDuration, TemporalOverflow::Constrain);
            RETURN_IF_EXCEPTION(scope, { });
            auto endDateTime = combineISODateAndTimeRecord(end, ISO8601::PlainTime());
            auto endEpochNs = getUTCEpochNanoseconds(endDateTime);
            auto beyondEnd = nudgedEpochNs - endEpochNs;
            auto beyondEndSign = beyondEnd < 0 ? -1 : beyondEnd > 0 ? 1 : 0;
            if (beyondEndSign != -sign) {
                duration = ISO8601::InternalDuration::combineDateAndTimeDuration(endDuration, 0);
                RETURN_IF_EXCEPTION(scope, { });
            } else
                done = true;
        }
        unitIndex--;
    }
    return duration;
}

// RoundRelativeDuration ( duration, destEpochNs, isoDateTime, timeZone, calendar, largestUnit, increment, smallestUnit, roundingMode )
// https://tc39.es/proposal-temporal/#sec-temporal-roundrelativeduration
// FIXME: calendar and time zone
void TemporalDuration::roundRelativeDuration(JSGlobalObject* globalObject, ISO8601::InternalDuration& duration, Int128 destEpochNs, ISO8601::PlainDate isoDate, TemporalUnit largestUnit, double increment, TemporalUnit smallestUnit, RoundingMode roundingMode)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    bool irregularLengthUnit = smallestUnit <= TemporalUnit::Week;
    int32_t sign = duration.sign() < 0 ? -1 : 1;
    NudgeResult nudgeResult;
    if (irregularLengthUnit) {
        Nudged record = nudgeToCalendarUnit(globalObject, sign, duration, destEpochNs, isoDate, increment, smallestUnit, roundingMode);
        RETURN_IF_EXCEPTION(scope, void());
        nudgeResult = record.m_nudgeResult;
    } else {
        nudgeResult = nudgeToDayOrTime(globalObject, duration, destEpochNs, largestUnit, increment, smallestUnit, roundingMode);
        RETURN_IF_EXCEPTION(scope, void());
    }
    duration = nudgeResult.m_duration;
    if (nudgeResult.m_didExpandCalendarUnit && smallestUnit != TemporalUnit::Week) {
        auto startUnit = smallestUnit <= TemporalUnit::Day ? smallestUnit : TemporalUnit::Day;
        duration = bubbleRelativeDuration(globalObject, sign, duration, nudgeResult.m_nudgedEpochNs, isoDate, largestUnit, startUnit);
        RETURN_IF_EXCEPTION(scope, void());
    }
}

// RoundDuration ( years, months, weeks, days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds, increment, unit, roundingMode [ , relativeTo ] )
// https://tc39.es/proposal-temporal/#sec-temporal-roundduration
ISO8601::InternalDuration TemporalDuration::round(JSGlobalObject* globalObject, ISO8601::InternalDuration internalDuration, double increment, TemporalUnit unit, RoundingMode mode)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(unit >= TemporalUnit::Day);

    if (unit == TemporalUnit::Day) {
        double fractionalDays = totalTimeDuration(internalDuration.time(), TemporalUnit::Day);
        double days = roundNumberToIncrementDouble(fractionalDays, increment, mode);
        return ISO8601::InternalDuration::combineDateAndTimeDuration(
            ISO8601::Duration { 0, 0, 0, (double) days, 0, 0, 0, 0, 0, 0 },
            0);
    } else  {
        std::optional<Int128> timeDuration =
            ISO8601::roundTimeDuration(globalObject, internalDuration.time(), increment, unit, mode);
        RETURN_IF_EXCEPTION(scope, { });
        if (!timeDuration) {
            throwRangeError(globalObject, scope, "Rounded duration exceeds maximum time duration"_s);
            return { };
        }
        return ISO8601::InternalDuration::combineDateAndTimeDuration(ISO8601::Duration(), timeDuration.value());
    }
}

ISO8601::Duration TemporalDuration::round(JSGlobalObject* globalObject, JSValue optionsValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* options = nullptr;
    std::optional<TemporalUnit> smallest;
    std::optional<TemporalUnit> largest;
    TemporalUnit defaultLargestUnit = largestSubduration(m_duration);
    if (optionsValue.isString()) {
        auto string = optionsValue.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        smallest = temporalUnitType(string);
        if (!smallest) {
            throwRangeError(globalObject, scope, "smallestUnit is an invalid Temporal unit"_s);
            return { };
        }
    } else {
        options = intlGetOptionsObject(globalObject, optionsValue);
        RETURN_IF_EXCEPTION(scope, { });

        smallest = temporalSmallestUnit(globalObject, options, { });
        RETURN_IF_EXCEPTION(scope, { });

        largest = temporalLargestUnit(globalObject, options, { }, defaultLargestUnit);
        RETURN_IF_EXCEPTION(scope, { });

        if (!smallest && !largest) {
            throwRangeError(globalObject, scope, "Cannot round without a smallestUnit or largestUnit option"_s);
            return { };
        }

        if (smallest && largest && smallest.value() < largest.value()) {
            throwRangeError(globalObject, scope, "smallestUnit must be smaller than largestUnit"_s);
            return { };
        }
    }
    TemporalUnit smallestUnit = smallest.value_or(TemporalUnit::Nanosecond);
    TemporalUnit largestUnit = largest.value_or(std::min(defaultLargestUnit, smallestUnit));

    auto roundingMode = temporalRoundingMode(globalObject, options, RoundingMode::HalfExpand);
    RETURN_IF_EXCEPTION(scope, { });

    auto increment = temporalRoundingIncrement(globalObject, options, maximumRoundingIncrement(smallestUnit), false);
    RETURN_IF_EXCEPTION(scope, { });

    // FIXME: Implement relativeTo parameter after PlainDateTime / ZonedDateTime.
    if (largestUnit > TemporalUnit::Year && (years() || months() || weeks() || (days() && largestUnit < TemporalUnit::Day))) {
        throwRangeError(globalObject, scope, "Cannot round a duration of years, months, or weeks without a relativeTo option"_s);
        return { };
    }
    if (largestUnit <= TemporalUnit::Week || smallestUnit <= TemporalUnit::Week) {
        throwVMError(globalObject, scope, "FIXME: years, months, or weeks rounding with relativeTo not implemented yet"_s);
        return { };
    }

    ISO8601::InternalDuration internalDuration = toInternalDurationRecordWith24HourDays(globalObject, m_duration);
    RETURN_IF_EXCEPTION(scope, { });
    auto result = round(globalObject, internalDuration, increment, smallestUnit, roundingMode);
    RETURN_IF_EXCEPTION(scope, { });
    return temporalDurationFromInternal(result, largestUnit);
}

double TemporalDuration::total(JSGlobalObject* globalObject, JSValue optionsValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String unitString;
    if (optionsValue.isString()) {
        unitString = optionsValue.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, 0);
    } else {
        JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
        RETURN_IF_EXCEPTION(scope, 0);

        unitString = intlStringOption(globalObject, options, vm.propertyNames->unit, { }, { }, { });
        RETURN_IF_EXCEPTION(scope, 0);
    }

    auto unitType = temporalUnitType(unitString);
    if (!unitType) {
        throwRangeError(globalObject, scope, "unit is an invalid Temporal unit"_s);
        return 0;
    }
    TemporalUnit unit = unitType.value();

    // FIXME: Implement relativeTo parameter after PlainDateTime / ZonedDateTime.
    if (unit == TemporalUnit::Week
        || unit == TemporalUnit::Month
        || unit == TemporalUnit::Year
        || (years() || months() || weeks() || (days() && unit < TemporalUnit::Day))) {
        throwRangeError(globalObject, scope, "Cannot total a duration of years, months, or weeks without a relativeTo option"_s);
        return { };
    }
    if (unit <= TemporalUnit::Week) {
        throwVMError(globalObject, scope, "FIXME: years, months, or weeks totalling with relativeTo not implemented yet"_s);
        return { };
    }

    auto internalDuration = toInternalDurationRecordWith24HourDays(globalObject, m_duration);
    RETURN_IF_EXCEPTION(scope, { });
    return totalTimeDuration(internalDuration.time(), unit);
}

String TemporalDuration::toString(JSGlobalObject* globalObject, JSValue optionsValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });

    if (!options)
        RELEASE_AND_RETURN(scope, toString(globalObject));

    PrecisionData data = secondsStringPrecision(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });
    if (data.unit < TemporalUnit::Second) {
        throwRangeError(globalObject, scope, "smallestUnit must not be \"minute\""_s);
        return { };
    }
    auto smallestUnit = data.unit;

    auto roundingMode = temporalRoundingMode(globalObject, options, RoundingMode::Trunc);
    RETURN_IF_EXCEPTION(scope, { });

    // No need to make a new object if we were given explicit defaults.
    if (std::get<0>(data.precision) == Precision::Auto && roundingMode == RoundingMode::Trunc)
        RELEASE_AND_RETURN(scope, toString(globalObject));

    auto internalDuration = toInternalDuration(m_duration);
    auto timeDuration = ISO8601::roundTimeDuration(globalObject, internalDuration.time(),
        data.increment, smallestUnit, roundingMode);
    RETURN_IF_EXCEPTION(scope, { });
    internalDuration = ISO8601::InternalDuration::combineDateAndTimeDuration(internalDuration.dateDuration(),
        timeDuration);
    auto roundedLargestUnit = std::min(largestSubduration(m_duration), TemporalUnit::Second);
    auto roundedDuration = temporalDurationFromInternal(internalDuration, roundedLargestUnit);
    RELEASE_AND_RETURN(scope, toString(globalObject, roundedDuration, data.precision));
}

static TemporalUnit defaultTemporalLargestUnit(const ISO8601::Duration& duration)
{
    if (duration.years())
        return TemporalUnit::Year;
    if (duration.months())
        return TemporalUnit::Month;
    if (duration.weeks())
        return TemporalUnit::Week;
    if (duration.days())
        return TemporalUnit::Day;
    if (duration.hours())
        return TemporalUnit::Hour;
    if (duration.minutes())
        return TemporalUnit::Minute;
    if (duration.seconds())
        return TemporalUnit::Second;
    if (duration.milliseconds())
        return TemporalUnit::Millisecond;
    if (duration.microseconds())
        return TemporalUnit::Microsecond;
    return TemporalUnit::Nanosecond;
}

static void appendInteger(JSGlobalObject* globalObject, StringBuilder& builder, double value)
{
    ASSERT(std::isfinite(value));

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    double absValue = std::abs(value);
    if (absValue <= maxSafeInteger()) [[likely]] {
        builder.append(absValue);
        return;
    }

    auto* bigint = JSBigInt::createFrom(globalObject, absValue);
    RETURN_IF_EXCEPTION(scope, void());

    String string = bigint->toString(globalObject, 10);
    RETURN_IF_EXCEPTION(scope, void());

    builder.append(string);
}

// TemporalDurationToString ( years, months, weeks, days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds, precision )
// https://tc39.es/proposal-temporal/#sec-temporal-temporaldurationtostring
String TemporalDuration::toString(JSGlobalObject* globalObject, const ISO8601::Duration& duration, std::tuple<Precision, unsigned> precision)
{
    ASSERT(std::get<0>(precision) == Precision::Auto || std::get<1>(precision) < 10);

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    StringBuilder builder;
    auto sign = TemporalDuration::sign(duration);
    if (sign < 0)
        builder.append('-');

    builder.append('P');
    if (duration.years()) {
        appendInteger(globalObject, builder, duration.years());
        RETURN_IF_EXCEPTION(scope, { });
        builder.append('Y');
    }
    if (duration.months()) {
        appendInteger(globalObject, builder, duration.months());
        RETURN_IF_EXCEPTION(scope, { });
        builder.append('M');
    }
    if (duration.weeks()) {
        appendInteger(globalObject, builder, duration.weeks());
        RETURN_IF_EXCEPTION(scope, { });
        builder.append('W');
    }
    if (duration.days()) {
        appendInteger(globalObject, builder, duration.days());
        RETURN_IF_EXCEPTION(scope, { });
        builder.append('D');
    }

    auto secondsDuration = timeDurationFromComponents(0, 0, duration.seconds(), duration.milliseconds(), duration.microseconds(), duration.nanoseconds());

    if (!duration.hours() && !duration.minutes() && !secondsDuration && sign && std::get<0>(precision) == Precision::Auto)
        return builder.toString();

    builder.append('T');
    if (duration.hours()) {
        appendInteger(globalObject, builder, duration.hours());
        RETURN_IF_EXCEPTION(scope, { });
        builder.append('H');
    }
    if (duration.minutes()) {
        appendInteger(globalObject, builder, duration.minutes());
        RETURN_IF_EXCEPTION(scope, { });
        builder.append('M');
    }

    bool zeroMinutesAndHigher = defaultTemporalLargestUnit(duration) >= TemporalUnit::Second;

    if (secondsDuration || (zeroMinutesAndHigher || std::get<0>(precision) != Precision::Auto)) {
        double secondsPart = std::abs(std::trunc((double) (secondsDuration / 1000000000)));
        double subSecondsPart = std::abs((double) (secondsDuration % 1000000000));
        appendInteger(globalObject, builder, secondsPart);
        RETURN_IF_EXCEPTION(scope, { });
        formatSecondsStringFraction(builder, subSecondsPart, precision);
        builder.append('S');
    }

    return builder.toString();
}

} // namespace JSC
