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
#include "TemporalInstantPrototype.h"

#include "ISO8601.h"
#include "InstantCore.h"
#include "IntlDateTimeFormat.h"
#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "TemporalInstant.h"
#include "TemporalObject.h"
#include "TemporalZonedDateTime.h"
#include "TimeZoneICUBridge.h"
#include <wtf/text/MakeString.h>

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(temporalInstantPrototypeFuncAdd);
static JSC_DECLARE_HOST_FUNCTION(temporalInstantPrototypeFuncSubtract);
static JSC_DECLARE_HOST_FUNCTION(temporalInstantPrototypeFuncUntil);
static JSC_DECLARE_HOST_FUNCTION(temporalInstantPrototypeFuncSince);
static JSC_DECLARE_HOST_FUNCTION(temporalInstantPrototypeFuncRound);
static JSC_DECLARE_HOST_FUNCTION(temporalInstantPrototypeFuncEquals);
static JSC_DECLARE_HOST_FUNCTION(temporalInstantPrototypeFuncToZonedDateTimeISO);
static JSC_DECLARE_HOST_FUNCTION(temporalInstantPrototypeFuncToString);
static JSC_DECLARE_HOST_FUNCTION(temporalInstantPrototypeFuncToJSON);
static JSC_DECLARE_HOST_FUNCTION(temporalInstantPrototypeFuncToLocaleString);
static JSC_DECLARE_HOST_FUNCTION(temporalInstantPrototypeFuncValueOf);
static JSC_DECLARE_CUSTOM_GETTER(temporalInstantPrototypeGetterEpochMilliseconds);
static JSC_DECLARE_CUSTOM_GETTER(temporalInstantPrototypeGetterEpochNanoseconds);

}

#include "TemporalInstantPrototype.lut.h"

namespace JSC {

const ClassInfo TemporalInstantPrototype::s_info = { "Temporal.Instant"_s, &Base::s_info, &instantPrototypeTable, nullptr, CREATE_METHOD_TABLE(TemporalInstantPrototype) };

/* Source for TemporalInstantPrototype.lut.h
@begin instantPrototypeTable
  add                  temporalInstantPrototypeFuncAdd                  DontEnum|Function 1
  subtract             temporalInstantPrototypeFuncSubtract             DontEnum|Function 1
  until                temporalInstantPrototypeFuncUntil                DontEnum|Function 1
  since                temporalInstantPrototypeFuncSince                DontEnum|Function 1
  round                temporalInstantPrototypeFuncRound                DontEnum|Function 1
  equals               temporalInstantPrototypeFuncEquals               DontEnum|Function 1
  toZonedDateTimeISO   temporalInstantPrototypeFuncToZonedDateTimeISO   DontEnum|Function 1
  toString             temporalInstantPrototypeFuncToString             DontEnum|Function 0
  toJSON               temporalInstantPrototypeFuncToJSON               DontEnum|Function 0
  toLocaleString       temporalInstantPrototypeFuncToLocaleString       DontEnum|Function 0
  valueOf              temporalInstantPrototypeFuncValueOf              DontEnum|Function 0
  epochMilliseconds    temporalInstantPrototypeGetterEpochMilliseconds  DontEnum|ReadOnly|CustomAccessor
  epochNanoseconds     temporalInstantPrototypeGetterEpochNanoseconds   DontEnum|ReadOnly|CustomAccessor
@end
*/

TemporalInstantPrototype* TemporalInstantPrototype::create(VM& vm, Structure* structure)
{
    auto* prototype = new (NotNull, allocateCell<TemporalInstantPrototype>(vm)) TemporalInstantPrototype(vm, structure);
    prototype->finishCreation(vm);
    return prototype;
}

Structure* TemporalInstantPrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalInstantPrototype::TemporalInstantPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void TemporalInstantPrototype::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

static constexpr std::initializer_list<TemporalUnit> disallowedAdditionUnits = {
    TemporalUnit::Year,
    TemporalUnit::Month,
    TemporalUnit::Week,
    TemporalUnit::Day,
};

// AddDurationToInstant ( operation, instant, temporalDurationLike )
// https://tc39.es/proposal-temporal/#sec-temporal-adddurationtoinstant
enum class AddInstantOperation : uint8_t { Add, Subtract };

template<AddInstantOperation operation>
static TemporalInstant* addDurationToInstant(JSGlobalObject* globalObject, TemporalInstant* instant, JSValue durationLike)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: duration = ? ToTemporalDuration(temporalDurationLike).
    ISO8601::Duration duration = TemporalDuration::toTemporalDurationRecord(globalObject, durationLike);
    RETURN_IF_EXCEPTION(scope, nullptr);

    // Step 2: If operation is subtract, set duration to CreateNegatedTemporalDuration(duration).
    if constexpr (operation == AddInstantOperation::Subtract)
        duration = -duration;

    // Steps 3-4: reject when DefaultTemporalLargestUnit(duration) is in the date category
    //   (Y/Mo/W/D). ZonedDateTime handles those calendrical units.
    for (TemporalUnit unit : disallowedAdditionUnits) {
        if (duration[unit]) [[unlikely]] {
            StringView unitName { temporalUnitPluralPropertyName(vm, unit).publicName() };
            throwRangeError(globalObject, scope, makeString("Adding "_s, unitName, " not supported by Temporal.Instant. Try Temporal.ZonedDateTime instead"_s));
            return nullptr;
        }
    }

    // Steps 5-6: ToInternalDurationRecordWith24HourDays + AddInstant (both done by ExactTime::add).
    std::optional<ISO8601::ExactTime> newExactTime = instant->exactTime().add(duration);
    if (!newExactTime) [[unlikely]] {
        throwRangeError(globalObject, scope, operation == AddInstantOperation::Add
            ? "Addition is outside of supported range for Temporal.Instant"_s
            : "Subtraction is outside of supported range for Temporal.Instant"_s);
        return nullptr;
    }

    // Step 7: Return ! CreateTemporalInstant(ns). Range already validated by ExactTime::add.
    return TemporalInstant::create(vm, globalObject->instantStructure(), *newExactTime);
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.add
JSC_DEFINE_HOST_FUNCTION(temporalInstantPrototypeFuncAdd, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: this value + ? RequireInternalSlot(instant, [[InitializedTemporalInstant]]).
    TemporalInstant* instant = dynamicDowncast<TemporalInstant>(callFrame->thisValue());
    if (!instant) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Instant.prototype.add called on value that's not a Instant"_s);

    // Step 3: Return ? AddDurationToInstant(ADD, instant, temporalDurationLike).
    RELEASE_AND_RETURN(scope, JSValue::encode(addDurationToInstant<AddInstantOperation::Add>(globalObject, instant, callFrame->argument(0))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.subtract
JSC_DEFINE_HOST_FUNCTION(temporalInstantPrototypeFuncSubtract, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: this value + ? RequireInternalSlot(instant, [[InitializedTemporalInstant]]).
    TemporalInstant* instant = dynamicDowncast<TemporalInstant>(callFrame->thisValue());
    if (!instant) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Instant.prototype.subtract called on value that's not a Instant"_s);

    // Step 3: Return ? AddDurationToInstant(SUBTRACT, instant, temporalDurationLike).
    RELEASE_AND_RETURN(scope, JSValue::encode(addDurationToInstant<AddInstantOperation::Subtract>(globalObject, instant, callFrame->argument(0))));
}

// DifferenceTemporalInstant ( operation, instant, other, options )
// https://tc39.es/proposal-temporal/#sec-temporal-differencetemporalinstant
template<DifferenceOperation operation>
static TemporalDuration* differenceTemporalInstant(JSGlobalObject* globalObject, TemporalInstant* instant, JSValue otherValue, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Set other to ? ToTemporalInstant(other).
    TemporalInstant* other = TemporalInstant::toInstant(globalObject, otherValue);
    RETURN_IF_EXCEPTION(scope, nullptr);

    // Steps 2-3: GetOptionsObject + GetDifferenceSettings (NegateRoundingMode for SINCE applied inside).
    auto [smallestUnit, largestUnit, roundingMode, increment] = extractDifferenceOptions(globalObject, optionsValue, UnitGroup::Time, TemporalUnit::Nanosecond, TemporalUnit::Second, operation);
    RETURN_IF_EXCEPTION(scope, nullptr);

    // Step 4: Let internalDuration be DifferenceInstant(instant.[[EpochNanoseconds]], other.[[EpochNanoseconds]], ...).
    ISO8601::InternalDuration internalDuration = instant->exactTime().difference(globalObject, other->exactTime(), increment, smallestUnit, roundingMode);
    RETURN_IF_EXCEPTION(scope, nullptr);

    // Step 5: Let result be ! TemporalDurationFromInternal(internalDuration, settings.[[LargestUnit]]).
    auto durResult = TemporalCore::temporalDurationFromInternal(internalDuration, largestUnit);
    if (!durResult) [[unlikely]] {
        throwTemporalError(globalObject, scope, durResult.error());
        return nullptr;
    }
    ISO8601::Duration result = *durResult;

    // Step 6: If operation is SINCE, set result to CreateNegatedTemporalDuration(result).
    if constexpr (operation == DifferenceOperation::Since)
        result = -result;

    // Step 7: Return ! CreateTemporalDuration(result).
    return TemporalDuration::create(vm, globalObject->durationStructure(), WTF::move(result));
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.until
JSC_DEFINE_HOST_FUNCTION(temporalInstantPrototypeFuncUntil, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: this value + ? RequireInternalSlot(instant, [[InitializedTemporalInstant]]).
    TemporalInstant* instant = dynamicDowncast<TemporalInstant>(callFrame->thisValue());
    if (!instant) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Instant.prototype.until called on value that's not a Instant"_s);

    // Step 3: Return ? DifferenceTemporalInstant(UNTIL, instant, other, options).
    RELEASE_AND_RETURN(scope, JSValue::encode(differenceTemporalInstant<DifferenceOperation::Until>(globalObject, instant, callFrame->argument(0), callFrame->argument(1))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.since
JSC_DEFINE_HOST_FUNCTION(temporalInstantPrototypeFuncSince, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: this value + ? RequireInternalSlot(instant, [[InitializedTemporalInstant]]).
    TemporalInstant* instant = dynamicDowncast<TemporalInstant>(callFrame->thisValue());
    if (!instant) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Instant.prototype.since called on value that's not a Instant"_s);

    // Step 3: Return ? DifferenceTemporalInstant(SINCE, instant, other, options).
    RELEASE_AND_RETURN(scope, JSValue::encode(differenceTemporalInstant<DifferenceOperation::Since>(globalObject, instant, callFrame->argument(0), callFrame->argument(1))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.round
JSC_DEFINE_HOST_FUNCTION(temporalInstantPrototypeFuncRound, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: this value + ? RequireInternalSlot(instant, [[InitializedTemporalInstant]]).
    TemporalInstant* instant = dynamicDowncast<TemporalInstant>(callFrame->thisValue());
    if (!instant) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Instant.prototype.round called on value that's not a Instant"_s);

    // Step 3: If roundTo is undefined, throw a TypeError exception.
    JSValue roundToValue = callFrame->argument(0);
    if (roundToValue.isUndefined()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Instant.prototype.round requires a roundTo option"_s);

    JSObject* roundTo = nullptr;
    std::optional<TemporalUnit> smallest;

    if (roundToValue.isString()) {
        // Step 4: If roundTo is a String, let paramString be roundTo, set roundTo to OrdinaryObjectCreate(null),
        //         and CreateDataPropertyOrThrow(roundTo, "smallestUnit", paramString).
        // NOTE: We skip the object wrapper and parse the unit directly as an optimisation.
        auto string = roundToValue.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        smallest = temporalUnitType(string);
        if (!smallest) [[unlikely]]
            return throwVMRangeError(globalObject, scope, "smallestUnit is an invalid Temporal unit"_s);
    } else {
        // Step 5: Else, set roundTo to ? GetOptionsObject(roundTo).
        roundTo = intlGetOptionsObject(globalObject, roundToValue);
        RETURN_IF_EXCEPTION(scope, { });
    }

    // Step 6: NOTE: The following steps read options in alphabetical order.
    // Step 7: Let roundingIncrement be ? GetRoundingIncrementOption(roundTo).
    double roundingIncrement = temporalRoundingIncrement(globalObject, roundTo);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 8: Let roundingMode be ? GetRoundingModeOption(roundTo, HALF-EXPAND).
    RoundingMode roundingMode = temporalRoundingMode(globalObject, roundTo, RoundingMode::HalfExpand);
    RETURN_IF_EXCEPTION(scope, { });

    if (!smallest) {
        // Step 9: Let smallestUnit be ? GetTemporalUnitValuedOption(roundTo, "smallestUnit", REQUIRED).
        auto smallestUnitMaybeAuto = temporalUnitValued(globalObject, roundTo, vm.propertyNames->smallestUnit, TemporalUnitDefault::Required);
        RETURN_IF_EXCEPTION(scope, { });
        // Step 10: Perform ? ValidateTemporalUnitValue(smallestUnit, time).
        validateTemporalUnitValue(globalObject, smallestUnitMaybeAuto, UnitGroup::Time, AllowedUnit::None, "smallestUnit"_s);
        RETURN_IF_EXCEPTION(scope, { });
        smallest = std::get<std::optional<TemporalUnit>>(smallestUnitMaybeAuto);
    } else {
        // Step 10 (string path): Perform ? ValidateTemporalUnitValue(smallestUnit, time).
        validateTemporalUnitValue(globalObject, smallest.value(), UnitGroup::Time, AllowedUnit::None, "smallestUnit"_s);
        RETURN_IF_EXCEPTION(scope, { });
    }
    TemporalUnit smallestUnit = smallest.value();

    // Steps 11-16: Let maximum be the per-unit maximum rounding increment.
    // Step 17: Perform ? ValidateTemporalRoundingIncrement(roundingIncrement, maximum, true).
    validateTemporalRoundingIncrement(globalObject, roundingIncrement, TemporalCore::maximumInstantIncrement(smallestUnit), Inclusivity::Inclusive);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 18: Let roundedNs be RoundTemporalInstant(instant.[[EpochNanoseconds]], roundingIncrement, smallestUnit, roundingMode).
    ISO8601::ExactTime roundedNs = instant->exactTime().round(globalObject, roundingIncrement, smallestUnit, roundingMode);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 19: Return ! CreateTemporalInstant(roundedNs). Range bounded by spec maximum.
    return JSValue::encode(TemporalInstant::create(vm, globalObject->instantStructure(), roundedNs));
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.equals
JSC_DEFINE_HOST_FUNCTION(temporalInstantPrototypeFuncEquals, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: this value + ? RequireInternalSlot(instant, [[InitializedTemporalInstant]]).
    TemporalInstant* instant = dynamicDowncast<TemporalInstant>(callFrame->thisValue());
    if (!instant) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Instant.prototype.equals called on value that's not a Instant"_s);

    // Step 3: Set other to ? ToTemporalInstant(other).
    TemporalInstant* other = TemporalInstant::toInstant(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    // Steps 4-5: If instant.[[EpochNanoseconds]] ≠ other.[[EpochNanoseconds]], return false; else return true.
    return JSValue::encode(jsBoolean(instant->exactTime() == other->exactTime()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.tostring
JSC_DEFINE_HOST_FUNCTION(temporalInstantPrototypeFuncToString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: this value + ? RequireInternalSlot(instant, [[InitializedTemporalInstant]]).
    TemporalInstant* instant = dynamicDowncast<TemporalInstant>(callFrame->thisValue());
    if (!instant) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Instant.prototype.toString called on value that's not a Instant"_s);

    // Step 3: Let resolvedOptions be ? GetOptionsObject(options).
    JSObject* resolvedOptions = intlGetOptionsObject(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    // Fast path: undefined options → default precision (Auto, no rounding, no timezone).
    if (!resolvedOptions)
        return JSValue::encode(jsString(vm, instant->toString()));

    // Step 4: NOTE: The following steps read options and perform independent validation in alphabetical order.
    // Step 5: Let digits be ? GetTemporalFractionalSecondDigitsOption(resolvedOptions).
    auto digits = temporalFractionalSecondDigits(globalObject, resolvedOptions);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 6: Let roundingMode be ? GetRoundingModeOption(resolvedOptions, TRUNC).
    RoundingMode roundingMode = temporalRoundingMode(globalObject, resolvedOptions, RoundingMode::Trunc);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 7: Let smallestUnit be ? GetTemporalUnitValuedOption(resolvedOptions, "smallestUnit", UNSET).
    auto smallestUnitResult = temporalUnitValued(globalObject, resolvedOptions, vm.propertyNames->smallestUnit);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 8: Let timeZone be ? Get(resolvedOptions, "timeZone").
    JSValue timeZoneValue = resolvedOptions->get(globalObject, vm.propertyNames->timeZone);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 9: Perform ? ValidateTemporalUnitValue(smallestUnit, time).
    validateTemporalUnitValue(globalObject, smallestUnitResult, UnitGroup::Time, AllowedUnit::None, "smallestUnit"_s);
    RETURN_IF_EXCEPTION(scope, { });
    auto smallestUnit = std::get<std::optional<TemporalUnit>>(smallestUnitResult);

    // Step 10: If smallestUnit is HOUR, throw a RangeError exception.
    if (smallestUnit == TemporalUnit::Hour) [[unlikely]]
        return throwVMRangeError(globalObject, scope, "smallestUnit cannot be \"hour\" for Instant.toString"_s);

    // Step 11: If timeZone is not undefined, set timeZone to ? ToTemporalTimeZoneIdentifier(timeZone).
    std::optional<TimeZone> timeZone;
    if (!timeZoneValue.isUndefined()) {
        timeZone = toTemporalTimeZoneIdentifier(globalObject, timeZoneValue);
        RETURN_IF_EXCEPTION(scope, { });
        ASSERT(timeZone);
    }

    // Step 12: Let precision be ToSecondsStringPrecisionRecord(smallestUnit, digits).
    auto data = toSecondsStringPrecisionRecord(smallestUnit, digits);

    // Step 13: Let roundedNs be RoundTemporalInstant(...).
    // Step 14: Let roundedInstant be ! CreateTemporalInstant(roundedNs).
    // Auto precision means increment=1 ns — a no-op for any rounding mode; skip steps 13-14.
    ISO8601::ExactTime roundedExactTime = instant->exactTime();
    if (std::get<0>(data.precision) != Precision::Auto) {
        roundedExactTime = instant->exactTime().round(globalObject, data.increment, data.unit, roundingMode);
        RETURN_IF_EXCEPTION(scope, { });
    }

    // Step 15: Return TemporalInstantToString(roundedInstant, timeZone, precision.[[Precision]]).
    std::optional<int64_t> offsetNs;
    if (timeZone) {
        auto offsetResult = TemporalCore::getOffsetNanosecondsFor(*timeZone, roundedExactTime);
        if (!offsetResult) [[unlikely]] {
            throwRangeError(globalObject, scope, offsetResult.error().message);
            return { };
        }
        offsetNs = *offsetResult;
    }
    return JSValue::encode(jsString(vm, TemporalCore::instantToString(roundedExactTime, offsetNs, data)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.tojson
JSC_DEFINE_HOST_FUNCTION(temporalInstantPrototypeFuncToJSON, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: this value + ? RequireInternalSlot(instant, [[InitializedTemporalInstant]]).
    TemporalInstant* instant = dynamicDowncast<TemporalInstant>(callFrame->thisValue());
    if (!instant) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Instant.prototype.toJSON called on value that's not a Instant"_s);

    // Step 3: Return TemporalInstantToString(instant, undefined, AUTO). Default-args toString uses
    //         timeZone = std::nullopt (UTC, "Z" suffix) and precision = Auto.
    RELEASE_AND_RETURN(scope, JSValue::encode(jsString(vm, instant->toString())));
}

// https://tc39.es/proposal-temporal/#sup-temporal.instant.prototype.tolocalestring
JSC_DEFINE_HOST_FUNCTION(temporalInstantPrototypeFuncToLocaleString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: this value + ? RequireInternalSlot(instant, [[InitializedTemporalInstant]]).
    TemporalInstant* instant = dynamicDowncast<TemporalInstant>(callFrame->thisValue());
    if (!instant) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Instant.prototype.toLocaleString called on value that's not a Instant"_s);

    // Step 3: Let dateFormat be ? CreateDateTimeFormat(%Intl.DateTimeFormat%, locales, options, ANY, ALL).
    JSValue locales = callFrame->argument(0);
    JSValue options = callFrame->argument(1);
    IntlDateTimeFormat* formatter;
    if (locales.isUndefined() && options.isUndefined())
        formatter = globalObject->defaultDateTimeFormat();
    else {
        formatter = IntlDateTimeFormat::create(vm, globalObject->dateTimeFormatStructure());
        formatter->initializeDateTimeFormat(globalObject, locales, options, IntlDateTimeFormat::RequiredComponent::Any, IntlDateTimeFormat::Defaults::All);
    }
    RETURN_IF_EXCEPTION(scope, { });

    // Step 4: Return ? FormatDateTime(dateFormat, instant).
    RELEASE_AND_RETURN(scope, JSValue::encode(formatter->format(globalObject, callFrame->thisValue())));
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.valueof
JSC_DEFINE_HOST_FUNCTION(temporalInstantPrototypeFuncValueOf, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Throw a TypeError exception. Instant has no primitive value.
    return throwVMTypeError(globalObject, scope, "Temporal.Instant.prototype.valueOf must not be called. To compare Instant values, use Temporal.Instant.compare"_s);
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.tozoneddatetimeiso
JSC_DEFINE_HOST_FUNCTION(temporalInstantPrototypeFuncToZonedDateTimeISO, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: this value + ? RequireInternalSlot(instant, [[InitializedTemporalInstant]]).
    TemporalInstant* instant = dynamicDowncast<TemporalInstant>(callFrame->thisValue());
    if (!instant) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Instant.prototype.toZonedDateTimeISO called on value that's not an Instant"_s);

    // Step 3: Let timeZone be ? ToTemporalTimeZoneIdentifier(timeZoneIdentifier).
    auto timeZone = toTemporalTimeZoneIdentifier(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });
    ASSERT(timeZone);

    // Step 4: Return ! CreateTemporalZonedDateTime(instant.[[EpochNanoseconds]], timeZone, "iso8601").
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalZonedDateTime::create(vm, globalObject->zonedDateTimeStructure(), instant->exactTime(), *timeZone, iso8601CalendarID())));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.instant.prototype.epochmilliseconds
JSC_DEFINE_CUSTOM_GETTER(temporalInstantPrototypeGetterEpochMilliseconds, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: this value + ? RequireInternalSlot(instant, [[InitializedTemporalInstant]]).
    TemporalInstant* instant = dynamicDowncast<TemporalInstant>(JSValue::decode(thisValue));
    if (!instant) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Instant.prototype.epochMilliseconds called on value that's not a Instant"_s);

    // Steps 3-4: Let ns be instant.[[EpochNanoseconds]]; return 𝔽(floor(ℝ(ns) / 10^6)).
    return JSValue::encode(jsNumber(instant->exactTime().floorEpochMilliseconds()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.instant.prototype.epochnanoseconds
JSC_DEFINE_CUSTOM_GETTER(temporalInstantPrototypeGetterEpochNanoseconds, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: this value + ? RequireInternalSlot(instant, [[InitializedTemporalInstant]]).
    TemporalInstant* instant = dynamicDowncast<TemporalInstant>(JSValue::decode(thisValue));
    if (!instant) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Instant.prototype.epochNanoseconds called on value that's not a Instant"_s);

    // Step 3: Return ℤ(instant.[[EpochNanoseconds]]).
    RELEASE_AND_RETURN(scope, JSValue::encode(JSBigInt::createFrom(globalObject, instant->exactTime().epochNanoseconds())));
}

} // namespace JSC
