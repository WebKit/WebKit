/*
 * Copyright (C) 2021 Igalia S.L.
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
#include "TemporalInstant.h"

#include "AuxiliaryBarrierInlines.h"
#include "Error.h"
#include "InstantCore.h"
#include "IntlObjectInlines.h"
#include "ISO8601.h"
#include "JSBigInt.h"
#include "JSGlobalObject.h"
#include "JSObjectInlines.h"
#include "MathCommon.h"
#include "StructureCreateInlines.h"
#include "TemporalDuration.h"
#include "TemporalObject.h"
#include "TemporalTimeZone.h"
// FIXME: #include "TemporalZonedDateTime.h"
#include "TimeZoneICUBridge.h"

#include <wtf/text/MakeString.h>
namespace JSC {

const ClassInfo TemporalInstant::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(TemporalInstant) };

Structure* TemporalInstant::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalInstant::TemporalInstant(VM& vm, Structure* structure, ISO8601::ExactTime exactTime)
    : Base(vm, structure)
    , m_exactTime(exactTime)
{
}

TemporalInstant* TemporalInstant::create(VM& vm, Structure* structure, ISO8601::ExactTime exactTime)
{
    ASSERT(exactTime.isValid());
    auto* object = new (NotNull, allocateCell<TemporalInstant>(vm)) TemporalInstant(vm, structure, exactTime);
    object->finishCreation(vm);
    return object;
}

// CreateTemporalInstant ( epochNanoseconds [ , newTarget ] )
// https://tc39.es/proposal-temporal/#sec-temporal-createtemporalinstant
TemporalInstant* TemporalInstant::tryCreateIfValid(JSGlobalObject* globalObject, ISO8601::ExactTime exactTime, Structure* structure)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!exactTime.isValid()) [[unlikely]] {
        throwRangeError(globalObject, scope, makeString(exactTime.asString(), " epoch nanoseconds is outside of supported range for Temporal.Instant"_s));
        return nullptr;
    }

    return create(vm, structure ? structure : globalObject->instantStructure(), exactTime);
}

TemporalInstant* TemporalInstant::tryCreateIfValid(JSGlobalObject* globalObject, JSValue value, Structure* structure)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue epochNanoseconds = value.toBigInt(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

#if USE(BIGINT32)
    if (epochNanoseconds.isBigInt32()) {
        int32_t total = epochNanoseconds.bigInt32AsInt32();
        ISO8601::ExactTime exactTime { total };
        ASSERT(exactTime.isValid());
        return create(vm, structure ? structure : globalObject->instantStructure(), exactTime);
    }
#endif

    JSBigInt* bigint = asHeapBigInt(epochNanoseconds);
    bool bigIntTooLong = false;
    Int128 total;
    if constexpr (sizeof(JSBigInt::Digit) == 4) {
        Int128 d0 { bigint->length() > 0 ? bigint->digit(0) : 0 };
        Int128 d1 { bigint->length() > 1 ? bigint->digit(1) : 0 };
        Int128 d2 { bigint->length() > 2 ? bigint->digit(2) : 0 };
        total = d2 << 64 | d1 << 32 | d0;
        bigIntTooLong = bigint->length() > 3;
    } else {
        ASSERT(sizeof(JSBigInt::Digit) == 8);
        // Handle maxint128 < abs(bigint) <= maxuint128 explicitly, otherwise
        // the int128 arithmetic below is undefined
        if (bigint->length() > 1 && (bigint->digit(1) & 0x8000'0000'0000'0000)) {
            total = 0;
            bigIntTooLong = true;
        } else {
            Int128 low { bigint->length() > 0 ? bigint->digit(0) : 0 };
            Int128 high { bigint->length() > 1 ? bigint->digit(1) : 0 };
            total = high << 64 | low;
            bigIntTooLong = bigint->length() > 2;
        }
    }
    ISO8601::ExactTime exactTime { total * (bigint->sign() ? -1 : 1) };

    if (bigIntTooLong || !exactTime.isValid()) {
        String argAsString = bigint->toString(globalObject, 10);
        if (scope.exception()) {
            TRY_CLEAR_EXCEPTION(scope, nullptr);
            argAsString = "The given number of"_s;
        }

        throwRangeError(globalObject, scope, makeString(ellipsizeAt(100, argAsString), " epoch nanoseconds is outside of the supported range for Temporal.Instant"_s));
        return nullptr;
    }

    return create(vm, structure ? structure : globalObject->instantStructure(), exactTime);
}

// ToTemporalInstant ( item )
// https://tc39.es/proposal-temporal/#sec-temporal-totemporalinstant
TemporalInstant* TemporalInstant::toInstant(JSGlobalObject* globalObject, JSValue itemValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: If item is not an Object and not a String, throw TypeError.
    if (!itemValue.isObject() && !itemValue.isString()) [[unlikely]] {
        throwTypeError(globalObject, scope, "can only convert to Instant from object or string values"_s);
        return nullptr;
    }

    // Step 2: If item has [[InitializedTemporalInstant]], return item.
    if (itemValue.inherits<TemporalInstant>())
        return uncheckedDowncast<TemporalInstant>(itemValue);

    // Step 3: If item has [[InitializedTemporalZonedDateTime]], return CreateTemporalInstant(item.[[EpochNanoseconds]]).
    // FIXME:
    // if (itemValue.inherits<TemporalZonedDateTime>())
    //     return TemporalInstant::create(vm, globalObject->instantStructure(), uncheckedDowncast<TemporalZonedDateTime>(itemValue)->exactTime());

    // Step 4: Let string be ? ToString(item).
    String string = itemValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    // Step 5: Let epochNanoseconds be ? ParseTemporalInstantString(string).
    auto parsedExactTime = ISO8601::parseInstant(string);
    if (!parsedExactTime) [[unlikely]] {
        throwRangeError(globalObject, scope, makeString("'"_s, ellipsizeAt(100, string), "' is not a valid Temporal.Instant string"_s));
        return nullptr;
    }

    // Step 6: Return ? CreateTemporalInstant(epochNanoseconds).
    RELEASE_AND_RETURN(scope, tryCreateIfValid(globalObject, parsedExactTime.value()));
}

// Temporal.Instant.from ( item )
// https://tc39.es/proposal-temporal/#sec-temporal.instant.from
TemporalInstant* TemporalInstant::from(JSGlobalObject* globalObject, JSValue itemValue)
{
    VM& vm = globalObject->vm();

    // Step 1: If item has [[InitializedTemporalInstant]], return CreateTemporalInstant(item.[[EpochNanoseconds]]).
    if (itemValue.inherits<TemporalInstant>()) {
        ISO8601::ExactTime exactTime = uncheckedDowncast<TemporalInstant>(itemValue)->exactTime();
        return TemporalInstant::create(vm, globalObject->instantStructure(), exactTime);
    }

    // Step 2: Return ? ToTemporalInstant(item).
    return toInstant(globalObject, itemValue);
}

// Temporal.Instant.fromEpochMilliseconds ( epochMilliseconds )
// https://tc39.es/proposal-temporal/#sec-temporal.instant.fromepochmilliseconds
TemporalInstant* TemporalInstant::fromEpochMilliseconds(JSGlobalObject* globalObject, JSValue epochMillisecondsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Let epochMilliseconds be ? ToNumber(epochMilliseconds).
    double epochMilliseconds = epochMillisecondsValue.toNumber(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    // Step 2: If IsIntegralNumber(epochMilliseconds) is false, throw RangeError.
    if (!isInteger(epochMilliseconds)) [[unlikely]] {
        throwRangeError(globalObject, scope, makeString(epochMilliseconds, " is not a valid integer number of epoch milliseconds"_s));
        return nullptr;
    }

    // Steps 3-4: Let epochNanoseconds = epochMilliseconds × 10^6; return ? CreateTemporalInstant(epochNanoseconds).
    ISO8601::ExactTime exactTime = ISO8601::ExactTime::fromEpochMilliseconds(epochMilliseconds);
    RELEASE_AND_RETURN(scope, tryCreateIfValid(globalObject, exactTime));
}

// Temporal.Instant.fromEpochNanoseconds ( epochNanoseconds )
// https://tc39.es/proposal-temporal/#sec-temporal.instant.fromepochnanoseconds
TemporalInstant* TemporalInstant::fromEpochNanoseconds(JSGlobalObject* globalObject, JSValue epochNanosecondsValue)
{
    // Step 1: Return ? CreateTemporalInstant(? ToBigInt(epochNanoseconds)).
    return tryCreateIfValid(globalObject, epochNanosecondsValue);
}

// Temporal.Instant.compare ( one, two )
// https://tc39.es/proposal-temporal/#sec-temporal.instant.compare
JSValue TemporalInstant::compare(JSGlobalObject* globalObject, JSValue oneValue, JSValue twoValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Set one = ? ToTemporalInstant(one).
    TemporalInstant* one = toInstant(globalObject, oneValue);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 2: Set two = ? ToTemporalInstant(two).
    TemporalInstant* two = toInstant(globalObject, twoValue);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 3: Return CompareEpochNanoseconds(one.[[EpochNanoseconds]], two.[[EpochNanoseconds]]).
    if (one->exactTime() > two->exactTime())
        return jsNumber(1);
    if (one->exactTime() < two->exactTime())
        return jsNumber(-1);
    return jsNumber(0);
}

// https://tc39.es/proposal-temporal/#sec-temporal-differencetemporalinstant
ISO8601::Duration TemporalInstant::difference(JSGlobalObject* globalObject, TemporalInstant* other, JSValue optionsValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: RequireInternalSlot + ToTemporalInstant done by caller (until/since prototype functions).
    // Step 3: GetDifferenceSettings.
    auto [smallestUnit, largestUnit, roundingMode, increment] = extractDifferenceOptions(globalObject, optionsValue, UnitGroup::Time, TemporalUnit::Nanosecond, TemporalUnit::Second);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 4: DifferenceInstant + RoundRelativeDuration.
    ISO8601::InternalDuration internalDuration = exactTime().difference(globalObject, other->exactTime(), increment, smallestUnit, roundingMode);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 5: Return CreateTemporalDuration from the balanced result.
    return TemporalDuration::temporalDurationFromInternal(internalDuration, largestUnit);
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.round
ISO8601::ExactTime TemporalInstant::round(JSGlobalObject* globalObject, JSValue optionsValue) const
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

    double roundingIncrement = temporalRoundingIncrement(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });
    RoundingMode roundingMode = temporalRoundingMode(globalObject, options, RoundingMode::HalfExpand);
    RETURN_IF_EXCEPTION(scope, { });

    if (!smallest) {
        auto smallestUnitMaybeAuto = getTemporalUnitValuedOption(globalObject, options, vm.propertyNames->smallestUnit);
        ASSERT(std::holds_alternative<std::optional<TemporalUnit>>(smallestUnitMaybeAuto));
        smallest = std::get<std::optional<TemporalUnit>>(smallestUnitMaybeAuto);
        RETURN_IF_EXCEPTION(scope, { });
    }

    if (!smallest) [[unlikely]] {
        throwRangeError(globalObject, scope, "Cannot round without a smallestUnit option"_s);
        return { };
    }

    TemporalUnit smallestUnit = smallest.value();
    validateTemporalUnitValue(globalObject, smallestUnit, UnitGroup::Time, AllowedUnit::None, "smallestUnit"_s);
    RETURN_IF_EXCEPTION(scope, { });

    validateTemporalRoundingIncrement(globalObject, roundingIncrement, TemporalCore::maximumInstantIncrement(smallestUnit), Inclusivity::Inclusive);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, exactTime().round(globalObject, roundingIncrement, smallestUnit, roundingMode));
}

// Temporal.Instant.prototype.toString( [ options ] )
// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.tostring
String TemporalInstant::toString(JSGlobalObject* globalObject, JSValue optionsValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });

    if (!options)
        return toString();

    // Read options in spec order: fractionalSecondDigits, roundingMode, smallestUnit, timeZone.
    auto digits = temporalFractionalSecondDigits(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    RoundingMode roundingMode = temporalRoundingMode(globalObject, options, RoundingMode::Trunc);
    RETURN_IF_EXCEPTION(scope, { });

    auto smallestUnitResult = getTemporalUnitValuedOption(globalObject, options, vm.propertyNames->smallestUnit);
    RETURN_IF_EXCEPTION(scope, { });

    JSObject* timeZone = nullptr;
    JSValue timeZoneValue = options->get(globalObject, vm.propertyNames->timeZone);
    RETURN_IF_EXCEPTION(scope, { });
    if (!timeZoneValue.isUndefined()) {
        if (!timeZoneValue.isString()) [[unlikely]] {
            throwTypeError(globalObject, scope, "timeZone option must be a string"_s);
            return { };
        }
        timeZone = TemporalTimeZone::from(globalObject, timeZoneValue);
        RETURN_IF_EXCEPTION(scope, { });
    }

    std::optional<TemporalUnit> smallestUnit;
    if (std::holds_alternative<TemporalAuto>(smallestUnitResult)) [[unlikely]] {
        throwRangeError(globalObject, scope, "smallestUnit \"auto\" is not valid for toString"_s);
        return { };
    }
    smallestUnit = std::get<std::optional<TemporalUnit>>(smallestUnitResult);
    if (smallestUnit) {
        auto disallowed = { TemporalUnit::Year, TemporalUnit::Month, TemporalUnit::Week, TemporalUnit::Day, TemporalUnit::Hour };
        if (std::ranges::find(disallowed, *smallestUnit) != disallowed.end()) [[unlikely]] {
            throwRangeError(globalObject, scope, "smallestUnit is a disallowed unit"_s);
            return { };
        }
    }

    PrecisionData data;
    if (smallestUnit) {
        switch (*smallestUnit) {
        case TemporalUnit::Minute:
            data = { { Precision::Minute, 0 }, TemporalUnit::Minute, 1 };
            break;
        case TemporalUnit::Second:
            data = { { Precision::Fixed, 0 }, TemporalUnit::Second, 1 };
            break;
        case TemporalUnit::Millisecond:
            data = { { Precision::Fixed, 3 }, TemporalUnit::Millisecond, 1 };
            break;
        case TemporalUnit::Microsecond:
            data = { { Precision::Fixed, 6 }, TemporalUnit::Microsecond, 1 };
            break;
        case TemporalUnit::Nanosecond:
            data = { { Precision::Fixed, 9 }, TemporalUnit::Nanosecond, 1 };
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    } else if (!digits)
        data = { { Precision::Auto, 0 }, TemporalUnit::Nanosecond, 1 };
    else {
        auto pow10 = [](unsigned n) -> unsigned {
            unsigned r = 1;
            for (unsigned i = 0; i < n; i++)
                r *= 10;
            return r;
        };
        unsigned d = digits.value();
        if (!d)
            data = { { Precision::Fixed, 0 }, TemporalUnit::Second, 1 };
        else if (d <= 3)
            data = { { Precision::Fixed, d }, TemporalUnit::Millisecond, pow10(3 - d) };
        else if (d <= 6)
            data = { { Precision::Fixed, d }, TemporalUnit::Microsecond, pow10(6 - d) };
        else
            data = { { Precision::Fixed, d }, TemporalUnit::Nanosecond, pow10(9 - d) };
    }

    // No need to make a new object if we were given explicit defaults.
    if (std::get<0>(data.precision) == Precision::Auto && roundingMode == RoundingMode::Trunc) {
        std::optional<int64_t> offsetNs;
        if (timeZone) {
            auto* tz = dynamicDowncast<TemporalTimeZone>(timeZone);
            if (tz) {
                auto offsetResult = TemporalCore::getOffsetNanosecondsFor(tz->timeZone(), exactTime());
                if (!offsetResult) [[unlikely]] {
                    throwRangeError(globalObject, scope, offsetResult.error().message);
                    return { };
                }
                offsetNs = *offsetResult;
            }
        }
        return TemporalCore::instantToString(exactTime(), offsetNs, data);
    }

    auto newExactTime = exactTime().round(globalObject, data.increment, data.unit, roundingMode);
    RETURN_IF_EXCEPTION(scope, { });

    // FIXME: Missing, relies on TimeZone:
    // 1. Let roundedInstant be ! CreateTemporalInstant(roundedNs).
    // ...
    // 1. If _outputTimeZone_ is *undefined*, then
    //   1. Set _outputTimeZone_ to ! CreateTemporalTimeZone(*"UTC"*).
    // 1. Let _isoCalendar_ be ! GetISO8601Calendar().
    // 1. Let _dateTime_ be ? BuiltinTimeZoneGetPlainDateTimeFor(_outputTimeZone_, _instant_, _isoCalendar_).
    JSObject* outputTimeZone = timeZone;
    std::optional<int64_t> outputOffsetNs;
    if (outputTimeZone) {
        auto* tz = dynamicDowncast<TemporalTimeZone>(outputTimeZone);
        if (tz) {
            auto offsetResult = TemporalCore::getOffsetNanosecondsFor(tz->timeZone(), newExactTime);
            if (!offsetResult) [[unlikely]] {
                throwRangeError(globalObject, scope, offsetResult.error().message);
                return { };
            }
            outputOffsetNs = *offsetResult;
        }
    }

    return TemporalCore::instantToString(newExactTime, outputOffsetNs, data);
}

} // namespace JSC
