/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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
#include <wtf/text/StringBuilder.h>

namespace JSC {

static constexpr double nanosecondsPerDay = 24.0 * 60 * 60 * 1000 * 1000 * 1000;

static PropertyName propertyName(VM& vm, unsigned index)
{
    ASSERT(index < numberOfTemporalUnits);
    switch (static_cast<TemporalUnit>(index)) {
#define JSC_TEMPORAL_DURATION_PROPERTY_NAME(name, capitalizedName) case TemporalUnit::capitalizedName: return vm.propertyNames->name##s; 
        JSC_TEMPORAL_UNITS(JSC_TEMPORAL_DURATION_PROPERTY_NAME)
#undef JSC_TEMPORAL_DURATION_PROPERTY_NAME
    }
}

const ClassInfo TemporalDuration::s_info = { "Object", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(TemporalDuration) };

TemporalDuration* TemporalDuration::create(VM& vm, Structure* structure, Subdurations&& subdurations)
{
    auto* object = new (NotNull, allocateCell<TemporalDuration>(vm.heap)) TemporalDuration(vm, structure, WTFMove(subdurations));
    object->finishCreation(vm);
    return object;
}

Structure* TemporalDuration::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalDuration::TemporalDuration(VM& vm, Structure* structure, Subdurations&& subdurations)
    : Base(vm, structure)
    , m_subdurations(WTFMove(subdurations))
{
}

void TemporalDuration::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
}

// IsValidDuration ( years, months, weeks, days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds )
// https://tc39.es/proposal-temporal/#sec-temporal-isvalidduration
static bool isValidDuration(const TemporalDuration::Subdurations& subdurations)
{
    int sign = 0;
    for (auto value : subdurations) {
        if (!std::isfinite(value) || (value < 0 && sign > 0) || (value > 0 && sign < 0))
            return false;

        if (!sign && value)
            sign = value > 0 ? 1 : -1;
    }

    return true;
}

// CreateTemporalDuration ( years, months, weeks, days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds [ , newTarget ] )
// https://tc39.es/proposal-temporal/#sec-temporal-createtemporalduration
TemporalDuration* TemporalDuration::tryCreateIfValid(JSGlobalObject* globalObject, Subdurations&& subdurations, Structure* structure)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!isValidDuration(subdurations)) {
        throwRangeError(globalObject, scope, "Temporal.Duration properties must be finite and of consistent sign"_s);
        return { };
    }

    return TemporalDuration::create(vm, structure ? structure : globalObject->durationStructure(), WTFMove(subdurations));
}

// ToTemporalDurationRecord ( temporalDurationLike )
// https://tc39.es/proposal-temporal/#sec-temporal-totemporaldurationrecord
TemporalDuration::Subdurations TemporalDuration::fromObject(JSGlobalObject* globalObject, JSObject* durationLike)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(!durationLike->inherits<TemporalDuration>(vm));

    Subdurations result;
    auto hasRelevantProperty = false;
    for (size_t i = 0; i < numberOfTemporalUnits; i++) {
        JSValue value = durationLike->get(globalObject, propertyName(vm, i));

        if (value.isUndefined()) {
            result[i] = 0;
            continue;
        }

        hasRelevantProperty = true;
        result[i] = value.toNumber(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        if (!isInteger(result[i])) {
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

// ToTemporalDuration ( item )
// https://tc39.es/proposal-temporal/#sec-temporal-totemporalduration
TemporalDuration* TemporalDuration::toDuration(JSGlobalObject* globalObject, JSValue itemValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (itemValue.inherits<TemporalDuration>(vm))
        return jsCast<TemporalDuration*>(itemValue);

    if (itemValue.isObject())
        RELEASE_AND_RETURN(scope, TemporalDuration::tryCreateIfValid(globalObject, fromObject(globalObject, asObject(itemValue))));

    String string = itemValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    auto parsedSubdurations = ISO8601::parseDuration(string);
    if (!parsedSubdurations) {
        throwRangeError(globalObject, scope, "Could not parse Duration string"_s);
        return nullptr;
    }

    RELEASE_AND_RETURN(scope, TemporalDuration::tryCreateIfValid(globalObject, WTFMove(parsedSubdurations.value())));
}

TemporalDuration* TemporalDuration::from(JSGlobalObject* globalObject, JSValue itemValue)
{
    VM& vm = globalObject->vm();

    if (itemValue.inherits<TemporalDuration>(vm)) {
        Subdurations cloned = jsCast<TemporalDuration*>(itemValue)->m_subdurations;
        return TemporalDuration::create(vm, globalObject->durationStructure(), WTFMove(cloned));
    }

    return toDuration(globalObject, itemValue);
}

// TotalDurationNanoseconds ( days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds, offsetShift )
// https://tc39.es/proposal-temporal/#sec-temporal-totaldurationnanoseconds
static double totalNanoseconds(TemporalDuration::Subdurations& subdurations)
{
    auto hours = 24 * subdurations.days() + subdurations.hours();
    auto minutes = 60 * hours + subdurations.minutes();
    auto seconds = 60 * minutes + subdurations.seconds();
    auto milliseconds = 1000 * seconds + subdurations.milliseconds();
    auto microseconds = 1000 * milliseconds + subdurations.microseconds();
    return 1000 * microseconds + subdurations.nanoseconds();
}

JSValue TemporalDuration::compare(JSGlobalObject* globalObject, JSValue valueOne, JSValue valueTwo)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* one = toDuration(globalObject, valueOne);
    RETURN_IF_EXCEPTION(scope, { });

    auto* two = toDuration(globalObject, valueTwo);
    RETURN_IF_EXCEPTION(scope, { });

    // FIXME: Implement relativeTo parameter after PlainDateTime / ZonedDateTime.
    if (one->years() || two->years() || one->months() || two->months() || one->weeks() || two->weeks()) {
        throwRangeError(globalObject, scope, "Cannot compare a duration of years, months, or weeks without a relativeTo option"_s);
        return { };
    }

    auto nsOne = totalNanoseconds(one->m_subdurations);
    auto nsTwo = totalNanoseconds(two->m_subdurations);
    return jsNumber(nsOne > nsTwo ? 1 : nsOne < nsTwo ? -1 : 0);
}

int TemporalDuration::sign(const TemporalDuration::Subdurations& subdurations)
{
    for (auto value : subdurations) {
        if (value < 0)
            return -1;

        if (value > 0)
            return 1;
    }

    return 0;
}

TemporalDuration::Subdurations TemporalDuration::with(JSGlobalObject* globalObject, JSObject* durationLike) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Subdurations result;
    auto hasRelevantProperty = false;
    for (size_t i = 0; i < numberOfTemporalUnits; i++) {
        JSValue value = durationLike->get(globalObject, propertyName(vm, i));

        if (value.isUndefined()) {
            result[i] = m_subdurations[i];
            continue;
        }

        hasRelevantProperty = true;
        result[i] = value.toNumber(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        if (!isInteger(result[i])) {
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

TemporalDuration::Subdurations TemporalDuration::negated() const
{
    Subdurations result;
    for (size_t i = 0; i < numberOfTemporalUnits; i++)
        result[i] = -m_subdurations[i];
    return result;
}

TemporalDuration::Subdurations TemporalDuration::abs() const
{
    Subdurations result;
    for (size_t i = 0; i < numberOfTemporalUnits; i++)
        result[i] = std::abs(m_subdurations[i]);
    return result;
}

// DefaultTemporalLargestUnit ( years, months, weeks, days, hours, minutes, seconds, milliseconds, microseconds )
// https://tc39.es/proposal-temporal/#sec-temporal-defaulttemporallargestunit
TemporalUnit TemporalDuration::largestSubduration() const
{
    uint8_t index = 0;
    while (index < numberOfTemporalUnits - 1 && !m_subdurations[index])
        index++;
    return static_cast<TemporalUnit>(index);
}

// BalanceDuration ( days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds, largestUnit [ , relativeTo ] )
// https://tc39.es/proposal-temporal/#sec-temporal-balanceduration
void TemporalDuration::balance(Subdurations& subdurations, TemporalUnit largestUnit)
{
    auto nanoseconds = totalNanoseconds(subdurations);
    subdurations.clear();

    if (largestUnit <= TemporalUnit::Day) {
        subdurations.setDays(std::trunc(nanoseconds / nanosecondsPerDay));
        nanoseconds = std::fmod(nanoseconds, nanosecondsPerDay);
    }

    double microseconds = std::trunc(nanoseconds / 1000);
    double milliseconds = std::trunc(microseconds / 1000);
    double seconds = std::trunc(milliseconds / 1000);
    double minutes = std::trunc(seconds / 60);
    if (largestUnit <= TemporalUnit::Hour) {
        subdurations.setNanoseconds(std::fmod(nanoseconds, 1000));
        subdurations.setMicroseconds(std::fmod(microseconds, 1000));
        subdurations.setMilliseconds(std::fmod(milliseconds, 1000));
        subdurations.setSeconds(std::fmod(seconds, 60));
        subdurations.setMinutes(std::fmod(minutes, 60));
        subdurations.setHours(std::trunc(minutes / 60));
    } else if (largestUnit == TemporalUnit::Minute) {
        subdurations.setNanoseconds(std::fmod(nanoseconds, 1000));
        subdurations.setMicroseconds(std::fmod(microseconds, 1000));
        subdurations.setMilliseconds(std::fmod(milliseconds, 1000));
        subdurations.setSeconds(std::fmod(seconds, 60));
        subdurations.setMinutes(minutes);
    } else if (largestUnit == TemporalUnit::Second) {
        subdurations.setNanoseconds(std::fmod(nanoseconds, 1000));
        subdurations.setMicroseconds(std::fmod(microseconds, 1000));
        subdurations.setMilliseconds(std::fmod(milliseconds, 1000));
        subdurations.setSeconds(seconds);
    } else if (largestUnit == TemporalUnit::Millisecond) {
        subdurations.setNanoseconds(std::fmod(nanoseconds, 1000));
        subdurations.setMicroseconds(std::fmod(microseconds, 1000));
        subdurations.setMilliseconds(milliseconds);
    } else if (largestUnit == TemporalUnit::Microsecond) {
        subdurations.setNanoseconds(std::fmod(nanoseconds, 1000));
        subdurations.setMicroseconds(microseconds);
    } else
        subdurations.setNanoseconds(nanoseconds);
}

TemporalDuration::Subdurations TemporalDuration::add(JSGlobalObject* globalObject, JSValue otherValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* other = toDuration(globalObject, otherValue);
    RETURN_IF_EXCEPTION(scope, { });

    // FIXME: Implement relativeTo parameter after PlainDateTime / ZonedDateTime.
    auto largestUnit = std::min(largestSubduration(), other->largestSubduration());
    if (largestUnit <= TemporalUnit::Week) {
        throwRangeError(globalObject, scope, "Cannot add a duration of years, months, or weeks without a relativeTo option"_s);
        return { };
    }

    Subdurations result {
        0, 0, 0, days() + other->days(),
        hours() + other->hours(), minutes() + other->minutes(), seconds() + other->seconds(),
        milliseconds() + other->milliseconds(), microseconds() + other->microseconds(), nanoseconds() + other->nanoseconds()
    };

    balance(result, largestUnit);
    return result;
}

TemporalDuration::Subdurations TemporalDuration::subtract(JSGlobalObject* globalObject, JSValue otherValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* other = toDuration(globalObject, otherValue);
    RETURN_IF_EXCEPTION(scope, { });

    // FIXME: Implement relativeTo parameter after PlainDateTime / ZonedDateTime.
    auto largestUnit = std::min(largestSubduration(), other->largestSubduration());
    if (largestUnit <= TemporalUnit::Week) {
        throwRangeError(globalObject, scope, "Cannot subtract a duration of years, months, or weeks without a relativeTo option"_s);
        return { };
    }

    Subdurations result {
        0, 0, 0, days() - other->days(),
        hours() - other->hours(), minutes() - other->minutes(), seconds() - other->seconds(),
        milliseconds() - other->milliseconds(), microseconds() - other->microseconds(), nanoseconds() - other->nanoseconds()
    };

    balance(result, largestUnit);
    return result;
}

// RoundDuration ( years, months, weeks, days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds, increment, unit, roundingMode [ , relativeTo ] )
// https://tc39.es/proposal-temporal/#sec-temporal-roundduration
double TemporalDuration::round(TemporalDuration::Subdurations& subdurations, double increment, TemporalUnit unit, RoundingMode mode)
{
    ASSERT(unit >= TemporalUnit::Day);
    double remainder = 0;

    if (unit == TemporalUnit::Day) {
        auto originalDays = subdurations.days();
        subdurations.setDays(0);
        auto nanoseconds = totalNanoseconds(subdurations);

        auto fractionalDays = originalDays + nanoseconds / nanosecondsPerDay;
        auto newDays = roundNumberToIncrement(fractionalDays, increment, mode);
        remainder = fractionalDays - newDays;
        subdurations.setDays(newDays);
    } else if (unit == TemporalUnit::Hour) {
        auto fractionalSeconds = subdurations.seconds() + subdurations.milliseconds() * 1e-3 + subdurations.microseconds() * 1e-6 + subdurations.nanoseconds() * 1e-9;
        auto fractionalHours = subdurations.hours() + (subdurations.minutes() + fractionalSeconds / 60) / 60;
        auto newHours = roundNumberToIncrement(fractionalHours, increment, mode);
        remainder = fractionalHours - newHours;
        subdurations.setHours(newHours);
    } else if (unit == TemporalUnit::Minute) {
        auto fractionalSeconds = subdurations.seconds() + subdurations.milliseconds() * 1e-3 + subdurations.microseconds() * 1e-6 + subdurations.nanoseconds() * 1e-9;
        auto fractionalMinutes = subdurations.minutes() + fractionalSeconds / 60;
        auto newMinutes = roundNumberToIncrement(fractionalMinutes, increment, mode);
        remainder = fractionalMinutes - newMinutes;
        subdurations.setMinutes(newMinutes);
    } else if (unit == TemporalUnit::Second) {
        auto fractionalSeconds = subdurations.seconds() + subdurations.milliseconds() * 1e-3 + subdurations.microseconds() * 1e-6 + subdurations.nanoseconds() * 1e-9;
        auto newSeconds = roundNumberToIncrement(fractionalSeconds, increment, mode);
        remainder = fractionalSeconds - newSeconds;
        subdurations.setSeconds(newSeconds);
    } else if (unit == TemporalUnit::Millisecond) {
        auto fractionalMilliseconds = subdurations.milliseconds() + subdurations.microseconds() * 1e-3 + subdurations.nanoseconds() * 1e-6;
        auto newMilliseconds = roundNumberToIncrement(fractionalMilliseconds, increment, mode);
        remainder = fractionalMilliseconds - newMilliseconds;
        subdurations.setMilliseconds(newMilliseconds);
    } else if (unit == TemporalUnit::Microsecond) {
        auto fractionalMicroseconds = subdurations.microseconds() + subdurations.nanoseconds() * 1e-3;
        auto newMicroseconds = roundNumberToIncrement(fractionalMicroseconds, increment, mode);
        remainder = fractionalMicroseconds - newMicroseconds;
        subdurations.setMicroseconds(newMicroseconds);
    } else {
        auto newNanoseconds = roundNumberToIncrement(subdurations.nanoseconds(), increment, mode);
        remainder = subdurations.nanoseconds() - newNanoseconds;
        subdurations.setNanoseconds(newNanoseconds);
    }

    for (auto i = static_cast<uint8_t>(unit) + 1u; i < numberOfTemporalUnits; i++)
        subdurations[i] = 0;

    return remainder;
}

TemporalDuration::Subdurations TemporalDuration::round(JSGlobalObject* globalObject, JSValue optionsValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });

    auto smallest = temporalSmallestUnit(globalObject, options, { });
    RETURN_IF_EXCEPTION(scope, { });

    TemporalUnit defaultLargestUnit = largestSubduration();
    auto largest = temporalLargestUnit(globalObject, options, { }, defaultLargestUnit);
    RETURN_IF_EXCEPTION(scope, { });

    if (!smallest && !largest) {
        throwRangeError(globalObject, scope, "Cannot round without a smallestUnit or largestUnit option"_s);
        return { };
    }

    if (smallest && largest && smallest.value() < largest.value()) {
        throwRangeError(globalObject, scope, "smallestUnit must be smaller than largestUnit"_s);
        return { };
    }

    TemporalUnit smallestUnit = smallest.value_or(TemporalUnit::Nanosecond);
    TemporalUnit largestUnit = largest.value_or(std::min(defaultLargestUnit, smallestUnit));

    auto roundingMode = intlOption<RoundingMode>(globalObject, options, vm.propertyNames->roundingMode,
        { { "ceil"_s, RoundingMode::Ceil }, { "floor"_s, RoundingMode::Floor }, { "trunc"_s, RoundingMode::Trunc }, { "halfExpand"_s, RoundingMode::HalfExpand } },
        "roundingMode must be either \"ceil\", \"floor\", \"trunc\", or \"halfExpand\""_s, RoundingMode::HalfExpand);
    RETURN_IF_EXCEPTION(scope, { });

    auto increment = temporalRoundingIncrement(globalObject, options, maximumRoundingIncrement(smallestUnit), false);
    RETURN_IF_EXCEPTION(scope, { });

    // FIXME: Implement relativeTo parameter after PlainDateTime / ZonedDateTime.
    if (largestUnit > TemporalUnit::Year && (years() || months() || weeks() || (days() && largestUnit < TemporalUnit::Day))) {
        throwRangeError(globalObject, scope, "Cannot round a duration of years, months, or weeks without a relativeTo option"_s);
        return { };
    }

    Subdurations newSubdurations = m_subdurations;
    round(newSubdurations, increment, smallestUnit, roundingMode);
    balance(newSubdurations, largestUnit);
    return newSubdurations;
}

double TemporalDuration::total(JSGlobalObject* globalObject, JSValue optionsValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, 0);

    String unitString = intlStringOption(globalObject, options, vm.propertyNames->unit, { }, nullptr, nullptr);
    RETURN_IF_EXCEPTION(scope, 0);

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

    Subdurations newSubdurations = m_subdurations;
    balance(newSubdurations, unit);
    double remainder = round(newSubdurations, 1, unit, RoundingMode::Trunc);
    return newSubdurations[static_cast<uint8_t>(unit)] + remainder;
}

String TemporalDuration::toString(JSGlobalObject* globalObject, JSValue optionsValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });

    if (!options)
        return toString();

    PrecisionData data = secondsStringPrecision(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });
    ASSERT(data.unit >= TemporalUnit::Second);

    auto roundingMode = intlOption<RoundingMode>(globalObject, options, vm.propertyNames->roundingMode,
        { { "ceil"_s, RoundingMode::Ceil }, { "floor"_s, RoundingMode::Floor }, { "trunc"_s, RoundingMode::Trunc }, { "halfExpand"_s, RoundingMode::HalfExpand } },
        "roundingMode must be either \"ceil\", \"floor\", \"trunc\", or \"halfExpand\""_s, RoundingMode::Trunc);
    RETURN_IF_EXCEPTION(scope, { });

    // No need to make a new object if we were given explicit defaults.
    if (!data.precision && roundingMode == RoundingMode::Trunc)
        return toString();

    Subdurations newSubdurations = m_subdurations;
    round(newSubdurations, data.increment, data.unit, roundingMode);
    return toString(newSubdurations, data.precision);
}

// TemporalDurationToString ( years, months, weeks, days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds, precision )
// https://tc39.es/proposal-temporal/#sec-temporal-temporaldurationtostring
String TemporalDuration::toString(const TemporalDuration::Subdurations& subdurations, std::optional<unsigned> precision)
{
    ASSERT(!precision || precision.value() < 10);

    auto balancedMicroseconds = subdurations.microseconds() + std::trunc(subdurations.nanoseconds() / 1000);
    auto balancedNanoseconds = std::fmod(subdurations.nanoseconds(), 1000);
    auto balancedMilliseconds = subdurations.milliseconds() + std::trunc(balancedMicroseconds / 1000);
    balancedMicroseconds = std::fmod(balancedMicroseconds, 1000);
    auto balancedSeconds = subdurations.seconds() + std::trunc(balancedMilliseconds / 1000);
    balancedMilliseconds = std::fmod(balancedMilliseconds, 1000);

    // TEMPORARY! (pending spec discussion about maximum values @ https://github.com/tc39/proposal-temporal/issues/1604)
    // We *must* avoid printing a number in scientific notation, which is currently only possible for numbers < 1e21
    // (a value originating in the Number#toFixed spec and upheld by our NumberToStringBuffer).
    auto formatInteger = [](double value) -> double {
        auto absValue = std::abs(value);
        return LIKELY(absValue < 1e21) ? absValue : 1e21 - 65537;
    };

    StringBuilder builder;

    auto sign = TemporalDuration::sign(subdurations);
    if (sign < 0)
        builder.append('-');

    builder.append('P');
    if (subdurations.years())
        builder.append(formatInteger(subdurations.years()), 'Y');
    if (subdurations.months())
        builder.append(formatInteger(subdurations.months()), 'M');
    if (subdurations.weeks())
        builder.append(formatInteger(subdurations.weeks()), 'W');
    if (subdurations.days())
        builder.append(formatInteger(subdurations.days()), 'D');

    // The zero value is displayed in seconds.
    auto usesSeconds = balancedSeconds || balancedMilliseconds || balancedMicroseconds || balancedNanoseconds || !sign;
    if (!subdurations.hours() && !subdurations.minutes() && !usesSeconds)
        return builder.toString();

    builder.append('T');
    if (subdurations.hours())
        builder.append(formatInteger(subdurations.hours()), 'H');
    if (subdurations.minutes())
        builder.append(formatInteger(subdurations.minutes()), 'M');
    if (usesSeconds) {
        builder.append(formatInteger(balancedSeconds));

        auto fraction = std::abs(balancedMilliseconds) * 1e6 + std::abs(balancedMicroseconds) * 1e3 + std::abs(balancedNanoseconds);
        if ((!precision && fraction) || (precision && precision.value())) {
            auto padded = makeString('.', pad('0', 9, fraction));
            if (precision)
                builder.append(StringView(padded).left(padded.length() - (9 - precision.value())));
            else {
                auto lengthWithoutTrailingZeroes = padded.length();
                while (padded[lengthWithoutTrailingZeroes - 1] == '0')
                    lengthWithoutTrailingZeroes--;
                builder.append(StringView(padded).left(lengthWithoutTrailingZeroes));
            }
        }

        builder.append('S');
    }

    return builder.toString();
}

} // namespace JSC
