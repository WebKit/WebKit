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
#include "TemporalPlainTime.h"

#include "DurationArithmetic.h"
#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "Rounding.h"
#include "TemporalDuration.h"
#include "TemporalPlainDateTime.h"
#include "TemporalZonedDateTime.h"
#include "VMTrapsInlines.h"

namespace JSC {
namespace TemporalPlainTimeInternal {
static constexpr bool verbose = false;
}

const ClassInfo TemporalPlainTime::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(TemporalPlainTime) };

TemporalPlainTime* TemporalPlainTime::create(VM& vm, Structure* structure, ISO8601::PlainTime&& plainTime)
{
    auto* object = new (NotNull, allocateCell<TemporalPlainTime>(vm)) TemporalPlainTime(vm, structure, WTF::move(plainTime));
    object->finishCreation(vm);
    return object;
}

Structure* TemporalPlainTime::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalPlainTime::TemporalPlainTime(VM& vm, Structure* structure, ISO8601::PlainTime&& plainTime)
    : Base(vm, structure)
    , m_plainTime(WTF::move(plainTime))
{
}


// https://tc39.es/proposal-temporal/#sec-temporal-isvalidtime
ISO8601::PlainTime TemporalPlainTime::toPlainTime(JSGlobalObject* globalObject, const ISO8601::Duration& duration)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    double hour = duration.hours();
    double minute = duration.minutes();
    double second = duration.seconds();
    double millisecond = duration.milliseconds();
    double microsecond = static_cast<double>(duration.microseconds());
    double nanosecond = static_cast<double>(duration.nanoseconds());
    if (!(hour >= 0 && hour <= 23)) [[unlikely]] {
        throwRangeError(globalObject, scope, "hour is out of range"_s);
        return { };
    }
    if (!(minute >= 0 && minute <= 59)) [[unlikely]] {
        throwRangeError(globalObject, scope, "minute is out of range"_s);
        return { };
    }
    if (!(second >= 0 && second <= 59)) [[unlikely]] {
        throwRangeError(globalObject, scope, "second is out of range"_s);
        return { };
    }
    if (!(millisecond >= 0 && millisecond <= 999)) [[unlikely]] {
        throwRangeError(globalObject, scope, "millisecond is out of range"_s);
        return { };
    }
    if (!(microsecond >= 0 && microsecond <= 999)) [[unlikely]] {
        throwRangeError(globalObject, scope, "microsecond is out of range"_s);
        return { };
    }
    if (!(nanosecond >= 0 && nanosecond <= 999)) [[unlikely]] {
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

    return TemporalPlainTime::create(vm, structure, WTF::move(plainTime));
}

// https://tc39.es/proposal-temporal/#sec-temporal-balancetime
static ISO8601::Duration NODELETE balanceTime(Int128 hour, Int128 minute, Int128 second, Int128 millisecond, Int128 microsecond, Int128 nanosecond)
{
    // https://github.com/tc39/proposal-temporal/issues/1804
    // Use non-negative modulo operation.
    microsecond += nanosecond / 1000;
    nanosecond = nanosecond % 1000;
    if (nanosecond < 0) {
        microsecond -= 1;
        nanosecond += 1000;
    }

    millisecond += microsecond / 1000;
    microsecond = microsecond % 1000;
    if (microsecond < 0) {
        millisecond -= 1;
        microsecond += 1000;
    }

    second += millisecond / 1000;
    millisecond = millisecond % 1000;
    if (millisecond < 0) {
        second -= 1;
        millisecond += 1000;
    }

    minute += second / 60;
    second = second % 60;
    if (second < 0) {
        minute -= 1;
        second += 60;
    }

    hour += minute / 60;
    minute = minute % 60;
    if (minute < 0) {
        hour -= 1;
        minute += 60;
    }

    Int128 days = hour / 24;
    hour = hour % 24;
    if (hour < 0) {
        days -= 1;
        hour += 24;
    }

    return ISO8601::Duration(0, 0, 0, static_cast<int64_t>(days), static_cast<int64_t>(hour), static_cast<int64_t>(minute), static_cast<int64_t>(second), static_cast<int64_t>(millisecond), Int128(microsecond), Int128(nanosecond));
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
        auto result = TemporalCore::roundNumberToIncrementDouble(quantity, increment, roundingMode);
        return ISO8601::Duration(0, 0, 0, static_cast<int64_t>(result), 0, 0, 0, 0, Int128(0), Int128(0));
    }
    case TemporalUnit::Hour: {
        quantity = (fractionalSecond(plainTime) / 60.0 + plainTime.minute()) / 60.0 + plainTime.hour();
        auto result = TemporalCore::roundNumberToIncrementDouble(quantity, increment, roundingMode);
        ASSERT(std::isfinite(result));
        return balanceTime(static_cast<Int128>(result), 0, 0, 0, 0, 0);
    }
    case TemporalUnit::Minute: {
        quantity = fractionalSecond(plainTime) / 60.0 + plainTime.minute();
        auto result = TemporalCore::roundNumberToIncrementDouble(quantity, increment, roundingMode);
        ASSERT(std::isfinite(result));
        return balanceTime(static_cast<Int128>(plainTime.hour()), static_cast<Int128>(result), 0, 0, 0, 0);
    }
    case TemporalUnit::Second: {
        quantity = fractionalSecond(plainTime);
        auto result = TemporalCore::roundNumberToIncrementDouble(quantity, increment, roundingMode);
        ASSERT(std::isfinite(result));
        return balanceTime(static_cast<Int128>(plainTime.hour()), static_cast<Int128>(plainTime.minute()), static_cast<Int128>(result), 0, 0, 0);
    }
    case TemporalUnit::Millisecond: {
        quantity = plainTime.millisecond() + plainTime.microsecond() * 1e-3 + plainTime.nanosecond() * 1e-6;
        auto result = TemporalCore::roundNumberToIncrementDouble(quantity, increment, roundingMode);
        ASSERT(std::isfinite(result));
        return balanceTime(static_cast<Int128>(plainTime.hour()), static_cast<Int128>(plainTime.minute()), static_cast<Int128>(plainTime.second()), static_cast<Int128>(result), 0, 0);
    }
    case TemporalUnit::Microsecond: {
        quantity = plainTime.microsecond() + plainTime.nanosecond() * 1e-3;
        auto result = TemporalCore::roundNumberToIncrementDouble(quantity, increment, roundingMode);
        ASSERT(std::isfinite(result));
        return balanceTime(static_cast<Int128>(plainTime.hour()), static_cast<Int128>(plainTime.minute()), static_cast<Int128>(plainTime.second()), static_cast<Int128>(plainTime.millisecond()), static_cast<Int128>(result), 0);
    }
    case TemporalUnit::Nanosecond: {
        quantity = plainTime.nanosecond();
        auto result = TemporalCore::roundNumberToIncrementDouble(quantity, increment, roundingMode);
        ASSERT(std::isfinite(result));
        return balanceTime(static_cast<Int128>(plainTime.hour()), static_cast<Int128>(plainTime.minute()), static_cast<Int128>(plainTime.second()), static_cast<Int128>(plainTime.millisecond()), static_cast<Int128>(plainTime.microsecond()), static_cast<Int128>(result));
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    return ISO8601::Duration();
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.round
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
        if (!smallest) [[unlikely]] {
            throwRangeError(globalObject, scope, "smallestUnit is an invalid Temporal unit"_s);
            return { };
        }

        if (smallest.value() <= TemporalUnit::Day) [[unlikely]] {
            throwRangeError(globalObject, scope, "smallestUnit is a disallowed unit"_s);
            return { };
        }
    } else {
        options = intlGetOptionsObject(globalObject, optionsValue);
        RETURN_IF_EXCEPTION(scope, { });
    }

    auto roundingIncrement = temporalRoundingIncrement(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });
    auto roundingMode = temporalRoundingMode(globalObject, options, RoundingMode::HalfExpand);
    RETURN_IF_EXCEPTION(scope, { });
    if (!smallest) {
        auto smallestMaybeAuto = temporalUnitValued(globalObject, options, vm.propertyNames->smallestUnit, TemporalUnitDefault::Required);
        RETURN_IF_EXCEPTION(scope, { });
        validateTemporalUnitValue(globalObject, smallestMaybeAuto, UnitGroup::Time, AllowedUnit::None, "smallestUnit"_s);
        RETURN_IF_EXCEPTION(scope, { });
        smallest = std::get<std::optional<TemporalUnit>>(smallestMaybeAuto);
    }
    auto smallestUnit = smallest.value();
    validateTemporalUnitValue(globalObject, smallestUnit, UnitGroup::Time, AllowedUnit::None, "smallestUnit"_s);
    RETURN_IF_EXCEPTION(scope, { });
    auto maximum = TemporalCore::maximumRoundingIncrement(smallestUnit);
    validateTemporalRoundingIncrement(globalObject, roundingIncrement, maximum, Inclusivity::Exclusive);
    RETURN_IF_EXCEPTION(scope, { });

    auto duration = roundTime(m_plainTime, roundingIncrement, smallestUnit, roundingMode, std::nullopt);
    RELEASE_AND_RETURN(scope, toPlainTime(globalObject, duration));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.tostring
String TemporalPlainTime::toString(JSGlobalObject* globalObject, JSValue optionsValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: branding check done by the caller.

    // Step 3: Let resolvedOptions be ?GetOptionsObject(options).
    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });

    if (!options)
        return toString();

    // Step 4: NOTE: The following steps read options in alphabetical order.
    // Step 5: Let digits be ?GetTemporalFractionalSecondDigitsOption(resolvedOptions).
    auto digits = temporalFractionalSecondDigits(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 6: Let roundingMode be ?GetRoundingModeOption(resolvedOptions, ~trunc~).
    auto roundingMode = temporalRoundingMode(globalObject, options, RoundingMode::Trunc);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 7: Let smallestUnit be ?GetTemporalUnitValuedOption(resolvedOptions, "smallestUnit", ~unset~).
    auto smallestUnitResult = temporalUnitValued(globalObject, options, vm.propertyNames->smallestUnit);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 8: Perform ? ValidateTemporalUnitValue(smallestUnit, ~time~).
    validateTemporalUnitValue(globalObject, smallestUnitResult, UnitGroup::Time, AllowedUnit::None, "smallestUnit"_s);
    RETURN_IF_EXCEPTION(scope, { });
    std::optional<TemporalUnit> smallestUnit = std::get<std::optional<TemporalUnit>>(smallestUnitResult);
    // Step 9: If smallestUnit is ~hour~, throw a RangeError exception.
    if (smallestUnit == TemporalUnit::Hour) [[unlikely]] {
        throwRangeError(globalObject, scope, "smallestUnit cannot be \"hour\" for PlainTime.toString"_s);
        return { };
    }

    // Step 10: Let precision be ToSecondsStringPrecisionRecord(smallestUnit, digits).
    auto data = toSecondsStringPrecisionRecord(smallestUnit, digits);

    // No need to round if given explicit defaults (steps 11-12 are no-ops when precision=auto, roundingMode=trunc).
    if (std::get<0>(data.precision) == Precision::Auto && roundingMode == RoundingMode::Trunc)
        return toString();

    // Step 11: Let roundResult be RoundTime(plainTime.[[Time]], precision.[[Increment]], precision.[[Unit]], roundingMode).
    // Step 12: Return TimeRecordToString(roundResult, precision.[[Precision]]).
    // roundTime() is RoundTime — returns ISO8601::Duration (our Time Record, which has [[Days]] per spec).
    // toPlainTime() extracts the clock fields, equivalent to TimeRecordToString ignoring [[Days]].
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
        double integer = value.toIntegerWithTruncation(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        if (!std::isfinite(integer)) [[unlikely]] {
            throwRangeError(globalObject, scope, "Temporal time properties must be finite"_s);
            return { };
        }
        duration.setField(unit, integer);
    }

    if (!hasRelevantProperty && !skipRelevantPropertyCheck) [[unlikely]] {
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
            double doubleValue = value.toIntegerWithTruncation(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            if (!std::isfinite(doubleValue)) [[unlikely]] {
                throwRangeError(globalObject, scope, "Temporal time properties must be finite"_s);
                return { };
            }
            partialTime[static_cast<unsigned>(unit) - static_cast<unsigned>(TemporalUnit::Hour)] = doubleValue;
        }
    }
    if (!hasAnyFields && !skipRelevantPropertyCheck) [[unlikely]] {
        throwTypeError(globalObject, scope, "Object must contain at least one Temporal time property"_s);
        return { };
    }
    return partialTime;
}

static ISO8601::PlainTime constrainTime(ISO8601::Duration&& duration)
{
    return ISO8601::PlainTime(
        clampTo<unsigned>(duration.hours(), 0, 23),
        clampTo<unsigned>(duration.minutes(), 0, 59),
        clampTo<unsigned>(duration.seconds(), 0, 59),
        clampTo<unsigned>(duration.milliseconds(), 0, 999),
        clampTo<unsigned>(static_cast<double>(duration.microseconds()), 0u, 999u),
        clampTo<unsigned>(static_cast<double>(duration.nanoseconds()), 0u, 999u));
}

ISO8601::PlainTime TemporalPlainTime::regulateTime(JSGlobalObject* globalObject, ISO8601::Duration&& duration, TemporalOverflow overflow)
{
    switch (overflow) {
    case TemporalOverflow::Constrain:
        return constrainTime(WTF::move(duration));
    case TemporalOverflow::Reject:
        return TemporalPlainTime::toPlainTime(globalObject, duration);
    }
    return { };
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporaltime
TemporalPlainTime* TemporalPlainTime::from(JSGlobalObject* globalObject, JSValue itemValue, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: If options is not present, set options to undefined. (Caller passes jsUndefined().)

    // Step 2: If item is an Object:
    if (itemValue.isObject()) {
        // Step 2.a: item has [[InitializedTemporalTime]].
        // Spec sub-steps: GetOptionsObject, GetTemporalOverflowOption (result unused).
        // Return ! CreateTemporalTime(item.[[Time]]).
        if (itemValue.inherits<TemporalPlainTime>()) {
            toTemporalOverflow(globalObject, optionsValue);
            RETURN_IF_EXCEPTION(scope, { });
            return TemporalPlainTime::create(vm, globalObject->plainTimeStructure(),
                uncheckedDowncast<TemporalPlainTime>(itemValue)->plainTime());
        }

        // Step 2.b: item has [[InitializedTemporalDateTime]].
        // Spec sub-steps: GetOptionsObject, GetTemporalOverflowOption (result unused).
        // Return ! CreateTemporalTime(item.[[ISODateTime]].[[Time]]).
        if (itemValue.inherits<TemporalPlainDateTime>()) {
            toTemporalOverflow(globalObject, optionsValue);
            RETURN_IF_EXCEPTION(scope, { });
            return TemporalPlainTime::create(vm, globalObject->plainTimeStructure(),
                uncheckedDowncast<TemporalPlainDateTime>(itemValue)->plainTime());
        }

        // Step 2.c: item has [[InitializedTemporalZonedDateTime]].
        // GetISODateTimeFor must precede GetOptionsObject (spec step order).
        // Spec sub-steps: GetOptionsObject, GetTemporalOverflowOption (result unused).
        // Return ! CreateTemporalTime(isoDateTime.[[Time]]).
        if (itemValue.inherits<TemporalZonedDateTime>()) {
            auto* zdt = uncheckedDowncast<TemporalZonedDateTime>(itemValue);
            ISO8601::PlainDate date;
            ISO8601::PlainTime time;
            zdt->getLocalDateAndTime(globalObject, date, time);
            RETURN_IF_EXCEPTION(scope, { });
            toTemporalOverflow(globalObject, optionsValue);
            RETURN_IF_EXCEPTION(scope, { });
            return TemporalPlainTime::create(vm, globalObject->plainTimeStructure(), WTF::move(time));
        }

        // Step 2.d: Let _result_ be ? ToTemporalTimeRecord(_item_).
        //           Must run before GetOptionsObject — field reads are observable first.
        auto duration = toTemporalTimeRecord(globalObject, uncheckedDowncast<JSObject>(itemValue));
        RETURN_IF_EXCEPTION(scope, { });

        // Steps 2.e-2.f: resolvedOptions = ? GetOptionsObject(options);
        //                overflow = ? GetTemporalOverflowOption(resolvedOptions).
        // Three branches: fast-path undefined, preserve exact TypeError message for non-objects,
        // and call toTemporalOverflow(JSObject*) directly for objects to avoid a redundant isObject check.
        TemporalOverflow overflow;
        if (optionsValue.isUndefined())
            overflow = TemporalOverflow::Constrain; // GetOptionsObject(undefined) → { }; default = ~constrain~.
        else if (!optionsValue.isObject()) [[unlikely]] {
            throwTypeError(globalObject, scope, "options must be an object"_s);
            return { };
        } else {
            overflow = toTemporalOverflow(globalObject, asObject(optionsValue));
            RETURN_IF_EXCEPTION(scope, { });
        }

        // Step 2.g: Set _result_ to ? RegulateTime(_result_, _overflow_).
        auto plainTime = regulateTime(globalObject, WTF::move(duration), overflow);
        RETURN_IF_EXCEPTION(scope, { });

        // Step 4: Return ! CreateTemporalTime(_result_).
        return TemporalPlainTime::create(vm, globalObject->plainTimeStructure(), WTF::move(plainTime));
    }

    // Step 3: Else (item is not an Object).

    // Step 3.a: If item is not a String, throw TypeError.
    if (!itemValue.isString()) [[unlikely]] {
        throwTypeError(globalObject, scope, "can only convert to PlainTime from object or string values"_s);
        return { };
    }

    // Step 3.b: parseResult = ? ParseISODateTime(item, « TemporalTimeString »).
    //   TemporalTimeString accepts both time-only ("14:30:00") and datetime ("2021-01-01T14:30:00")
    //   forms; parseISODateTime tries the time-only production first then falls back to datetime.
    auto string = itemValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    auto parsed = ISO8601::parseISODateTime(string, ISO8601::TemporalProduction::Time);
    if (parsed) [[likely]] {
        auto [plainDateOpt, plainTimeOpt, timeZoneOpt, calendarOpt, matched, isShortForm] = WTF::move(*parsed);
        ASSERT(plainTimeOpt);
        auto plainTime = WTF::move(*plainTimeOpt);
        // Step 3.c: Assert _parseResult_.[[Time]] is not ~start-of-day~. (guaranteed by grammar)
        // Step 3.d: Set _result_ to _parseResult_.[[Time]].
        // Step 3.f: resolvedOptions = ? GetOptionsObject(options).
        // Step 3.g: Perform ? GetTemporalOverflowOption(resolvedOptions). (result unused for strings)
        // Step 4: Return ! CreateTemporalTime(_result_).
        toTemporalOverflow(globalObject, optionsValue);
        RETURN_IF_EXCEPTION(scope, { });
        return TemporalPlainTime::create(vm, globalObject->plainTimeStructure(), WTF::move(plainTime));
    }

    // Step 3.b: ParseISODateTime failed → throw RangeError.
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
        // The seconds, milliseconds, microseconds, or nanoseconds fields can be up to
        // MAX_SAFE_INTEGER. So to do the addition, we have to convert to Int128.
        // The hour and minute fields are more constrained, but for consistency, these
        // arguments are Int128 as well.
        static_cast<Int128>(plainTime.hour()) + static_cast<Int128>(duration.hours()),
        static_cast<Int128>(plainTime.minute()) + static_cast<Int128>(duration.minutes()),
        static_cast<Int128>(plainTime.second()) + static_cast<Int128>(duration.seconds()),
        static_cast<Int128>(plainTime.millisecond()) + static_cast<Int128>(duration.milliseconds()),
        static_cast<Int128>(plainTime.microsecond()) + static_cast<Int128>(duration.microseconds()),
        static_cast<Int128>(plainTime.nanosecond()) + static_cast<Int128>(duration.nanoseconds()));
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
    duration.setField(TemporalUnit::Hour, hourOptional.value_or(hour()));
    duration.setField(TemporalUnit::Minute, minuteOptional.value_or(minute()));
    duration.setField(TemporalUnit::Second, secondOptional.value_or(second()));
    duration.setField(TemporalUnit::Millisecond, millisecondOptional.value_or(millisecond()));
    duration.setField(TemporalUnit::Microsecond, microsecondOptional.value_or(microsecond()));
    duration.setField(TemporalUnit::Nanosecond, nanosecondOptional.value_or(nanosecond()));

    RELEASE_AND_RETURN(scope, regulateTime(globalObject, WTF::move(duration), overflow));
}

// https://tc39.es/proposal-temporal/#sec-temporal-differencetime
static Int128 differenceTime(ISO8601::PlainTime time1, ISO8601::PlainTime time2)
{
    double hours = static_cast<double>(time2.hour()) - static_cast<double>(time1.hour());
    double minutes = static_cast<double>(time2.minute()) - static_cast<double>(time1.minute());
    double seconds = static_cast<double>(time2.second()) - static_cast<double>(time1.second());
    double milliseconds = static_cast<double>(time2.millisecond()) - static_cast<double>(time1.millisecond());
    double microseconds = static_cast<double>(time2.microsecond()) - static_cast<double>(time1.microsecond());
    double nanoseconds = static_cast<double>(time2.nanosecond()) - static_cast<double>(time1.nanosecond());
    dataLogLnIf(TemporalPlainTimeInternal::verbose, "Diff ", hours, " ", minutes, " ", seconds, " ", milliseconds, " ", microseconds, " ", nanoseconds);

    return TemporalCore::timeDurationFromComponents(hours, minutes, seconds, milliseconds, microseconds, nanoseconds);
}

// https://tc39.es/proposal-temporal/#sec-temporal-differencetemporalplaintime
// DifferenceTemporalPlainTime ( operation, temporalTime, other, options )
ISO8601::Duration TemporalPlainTime::differenceTemporalPlainTime(DifferenceOperation operation, JSGlobalObject* globalObject, TemporalPlainTime* other, JSValue optionsValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto [smallestUnit, largestUnit, roundingMode, increment] = extractDifferenceOptions(globalObject, optionsValue, UnitGroup::Time, TemporalUnit::Nanosecond, TemporalUnit::Hour);
    RETURN_IF_EXCEPTION(scope, { });

    Int128 timeDuration;

    // The sign of the argument to roundTimeDuration() determines the choice
    // of rounding mode, so the sign should be preserved here instead of
    // negating the duration at the end
    if (operation == DifferenceOperation::Since)
        timeDuration = differenceTime(other->plainTime(), plainTime());
    else
        timeDuration = differenceTime(plainTime(), other->plainTime());

    auto d = ISO8601::roundTimeDuration(globalObject, timeDuration, increment, smallestUnit, roundingMode);
    RETURN_IF_EXCEPTION(scope, { });
    auto duration = ISO8601::InternalDuration::combineDateAndTimeDuration(ISO8601::Duration(), d);
    auto durResult = TemporalCore::temporalDurationFromInternal(duration, largestUnit);
    if (!durResult) [[unlikely]] {
        throwTemporalError(globalObject, scope, durResult.error());
        return { };
    }
    return *durResult;
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.until
ISO8601::Duration TemporalPlainTime::until(JSGlobalObject* globalObject, TemporalPlainTime* other, JSValue optionsValue) const
{
    return differenceTemporalPlainTime(DifferenceOperation::Until, globalObject, other, optionsValue);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.since
ISO8601::Duration TemporalPlainTime::since(JSGlobalObject* globalObject, TemporalPlainTime* other, JSValue optionsValue) const
{
    return differenceTemporalPlainTime(DifferenceOperation::Since, globalObject, other, optionsValue);
}

} // namespace JSC
