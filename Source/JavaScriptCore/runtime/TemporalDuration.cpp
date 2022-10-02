/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "TemporalObject.h"
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringConcatenate.h>

namespace JSC {

static constexpr double nanosecondsPerDay = 24.0 * 60 * 60 * 1000 * 1000 * 1000;

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

void TemporalDuration::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
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
        result[unit] = value.toIntegerWithoutRounding(globalObject);
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

// TotalDurationNanoseconds ( days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds, offsetShift )
// https://tc39.es/proposal-temporal/#sec-temporal-totaldurationnanoseconds
static double totalNanoseconds(ISO8601::Duration& duration)
{
    auto hours = 24 * duration.days() + duration.hours();
    auto minutes = 60 * hours + duration.minutes();
    auto seconds = 60 * minutes + duration.seconds();
    auto milliseconds = 1000 * seconds + duration.milliseconds();
    auto microseconds = 1000 * milliseconds + duration.microseconds();
    return 1000 * microseconds + duration.nanoseconds();
}

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

    auto nsOne = totalNanoseconds(one->m_duration);
    auto nsTwo = totalNanoseconds(two->m_duration);
    return jsNumber(nsOne > nsTwo ? 1 : nsOne < nsTwo ? -1 : 0);
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

// BalanceDuration ( days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds, largestUnit [ , relativeTo ] )
// https://tc39.es/proposal-temporal/#sec-temporal-balanceduration
std::optional<double> TemporalDuration::balance(ISO8601::Duration& duration, TemporalUnit largestUnit)
{
    auto nanoseconds = totalNanoseconds(duration);
    if (!std::isfinite(nanoseconds))
        return nanoseconds;
    duration.clear();

    if (largestUnit <= TemporalUnit::Day) {
        duration.setDays(std::trunc(nanoseconds / nanosecondsPerDay));
        nanoseconds = std::fmod(nanoseconds, nanosecondsPerDay);
    }

    double microseconds = std::trunc(nanoseconds / 1000);
    double milliseconds = std::trunc(microseconds / 1000);
    double seconds = std::trunc(milliseconds / 1000);
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
        duration.setMilliseconds(milliseconds);
    } else if (largestUnit == TemporalUnit::Microsecond) {
        duration.setNanoseconds(std::fmod(nanoseconds, 1000));
        duration.setMicroseconds(microseconds);
    } else
        duration.setNanoseconds(nanoseconds);

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

    ISO8601::Duration result {
        0, 0, 0, days() + other.days(),
        hours() + other.hours(), minutes() + other.minutes(), seconds() + other.seconds(),
        milliseconds() + other.milliseconds(), microseconds() + other.microseconds(), nanoseconds() + other.nanoseconds()
    };

    balance(result, largestUnit);
    return result;
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

    ISO8601::Duration result {
        0, 0, 0, days() - other.days(),
        hours() - other.hours(), minutes() - other.minutes(), seconds() - other.seconds(),
        milliseconds() - other.milliseconds(), microseconds() - other.microseconds(), nanoseconds() - other.nanoseconds()
    };

    balance(result, largestUnit);
    return result;
}

// RoundDuration ( years, months, weeks, days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds, increment, unit, roundingMode [ , relativeTo ] )
// https://tc39.es/proposal-temporal/#sec-temporal-roundduration
double TemporalDuration::round(ISO8601::Duration& duration, double increment, TemporalUnit unit, RoundingMode mode)
{
    ASSERT(unit >= TemporalUnit::Day);
    double remainder = 0;

    if (unit == TemporalUnit::Day) {
        auto originalDays = duration.days();
        duration.setDays(0);
        auto nanoseconds = totalNanoseconds(duration);

        auto fractionalDays = originalDays + nanoseconds / nanosecondsPerDay;
        auto newDays = roundNumberToIncrement(fractionalDays, increment, mode);
        remainder = fractionalDays - newDays;
        duration.setDays(newDays);
    } else if (unit == TemporalUnit::Hour) {
        auto fractionalSeconds = duration.seconds() + duration.milliseconds() * 1e-3 + duration.microseconds() * 1e-6 + duration.nanoseconds() * 1e-9;
        auto fractionalHours = duration.hours() + (duration.minutes() + fractionalSeconds / 60) / 60;
        auto newHours = roundNumberToIncrement(fractionalHours, increment, mode);
        remainder = fractionalHours - newHours;
        duration.setHours(newHours);
    } else if (unit == TemporalUnit::Minute) {
        auto fractionalSeconds = duration.seconds() + duration.milliseconds() * 1e-3 + duration.microseconds() * 1e-6 + duration.nanoseconds() * 1e-9;
        auto fractionalMinutes = duration.minutes() + fractionalSeconds / 60;
        auto newMinutes = roundNumberToIncrement(fractionalMinutes, increment, mode);
        remainder = fractionalMinutes - newMinutes;
        duration.setMinutes(newMinutes);
    } else if (unit == TemporalUnit::Second) {
        auto fractionalSeconds = duration.seconds() + duration.milliseconds() * 1e-3 + duration.microseconds() * 1e-6 + duration.nanoseconds() * 1e-9;
        auto newSeconds = roundNumberToIncrement(fractionalSeconds, increment, mode);
        remainder = fractionalSeconds - newSeconds;
        duration.setSeconds(newSeconds);
    } else if (unit == TemporalUnit::Millisecond) {
        auto fractionalMilliseconds = duration.milliseconds() + duration.microseconds() * 1e-3 + duration.nanoseconds() * 1e-6;
        auto newMilliseconds = roundNumberToIncrement(fractionalMilliseconds, increment, mode);
        remainder = fractionalMilliseconds - newMilliseconds;
        duration.setMilliseconds(newMilliseconds);
    } else if (unit == TemporalUnit::Microsecond) {
        auto fractionalMicroseconds = duration.microseconds() + duration.nanoseconds() * 1e-3;
        auto newMicroseconds = roundNumberToIncrement(fractionalMicroseconds, increment, mode);
        remainder = fractionalMicroseconds - newMicroseconds;
        duration.setMicroseconds(newMicroseconds);
    } else {
        auto newNanoseconds = roundNumberToIncrement(duration.nanoseconds(), increment, mode);
        remainder = duration.nanoseconds() - newNanoseconds;
        duration.setNanoseconds(newNanoseconds);
    }

    for (auto i = static_cast<uint8_t>(unit) + 1u; i < numberOfTemporalUnits; i++)
        duration[i] = 0;

    return remainder;
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
    if (largestUnit <= TemporalUnit::Week) {
        throwVMError(globalObject, scope, "FIXME: years, months, or weeks rounding with relativeTo not implemented yet"_s);
        return { };
    }

    ISO8601::Duration newDuration = m_duration;
    round(newDuration, increment, smallestUnit, roundingMode);
    balance(newDuration, largestUnit);
    return newDuration;
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
    if (unit > TemporalUnit::Year && (years() || months() || weeks() || (days() && unit < TemporalUnit::Day))) {
        throwRangeError(globalObject, scope, "Cannot total a duration of years, months, or weeks without a relativeTo option"_s);
        return { };
    }

    ISO8601::Duration newDuration = m_duration;
    auto infiniteResult = balance(newDuration, unit);
    if (infiniteResult)
        return infiniteResult.value();
    double remainder = round(newDuration, 1, unit, RoundingMode::Trunc);
    return newDuration[static_cast<uint8_t>(unit)] + remainder;
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

    auto roundingMode = temporalRoundingMode(globalObject, options, RoundingMode::Trunc);
    RETURN_IF_EXCEPTION(scope, { });

    // No need to make a new object if we were given explicit defaults.
    if (std::get<0>(data.precision) == Precision::Auto && roundingMode == RoundingMode::Trunc)
        RELEASE_AND_RETURN(scope, toString(globalObject));

    ISO8601::Duration newDuration = m_duration;
    round(newDuration, data.increment, data.unit, roundingMode);
    RELEASE_AND_RETURN(scope, toString(globalObject, newDuration, data.precision));
}

static void appendInteger(JSGlobalObject* globalObject, StringBuilder& builder, double value)
{
    ASSERT(std::isfinite(value));

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    double absValue = std::abs(value);
    if (LIKELY(absValue <= maxSafeInteger())) {
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

    auto balancedMicroseconds = duration.microseconds() + std::trunc(duration.nanoseconds() / 1000);
    auto balancedNanoseconds = std::fmod(duration.nanoseconds(), 1000);
    auto balancedMilliseconds = duration.milliseconds() + std::trunc(balancedMicroseconds / 1000);
    balancedMicroseconds = std::fmod(balancedMicroseconds, 1000);
    auto balancedSeconds = duration.seconds() + std::trunc(balancedMilliseconds / 1000);
    balancedMilliseconds = std::fmod(balancedMilliseconds, 1000);

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

    // The zero value is displayed in seconds.
    auto usesSeconds = balancedSeconds || balancedMilliseconds || balancedMicroseconds || balancedNanoseconds || !sign || std::get<0>(precision) != Precision::Auto;
    if (!duration.hours() && !duration.minutes() && !usesSeconds)
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
    if (usesSeconds) {
        // TEMPORARY! (pending spec discussion about rebalancing @ https://github.com/tc39/proposal-temporal/issues/2195)
        // Although we must be able to display Number values beyond MAX_SAFE_INTEGER, it does not seem reasonable
        // to require that calculations be performed outside of double space purely to support a case like
        // `Temporal.Duration.from({ microseconds: Number.MAX_VALUE, nanoseconds: Number.MAX_VALUE }).toString()`.
        if (UNLIKELY(!std::isfinite(balancedSeconds))) {
            throwRangeError(globalObject, scope, "Cannot display infinite seconds!"_s);
            return { };
        }

        appendInteger(globalObject, builder, balancedSeconds);
        RETURN_IF_EXCEPTION(scope, { });

        auto fraction = std::abs(balancedMilliseconds) * 1e6 + std::abs(balancedMicroseconds) * 1e3 + std::abs(balancedNanoseconds);
        formatSecondsStringFraction(builder, fraction, precision);

        builder.append('S');
    }

    return builder.toString();
}

} // namespace JSC
