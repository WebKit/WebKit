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
#include "TemporalDurationPrototype.h"

#include "DurationArithmetic.h"
#include "IntlDurationFormat.h"
#include "JSCInlines.h"
#include "TemporalDuration.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(temporalDurationPrototypeFuncWith);
static JSC_DECLARE_HOST_FUNCTION(temporalDurationPrototypeFuncNegated);
static JSC_DECLARE_HOST_FUNCTION(temporalDurationPrototypeFuncAbs);
static JSC_DECLARE_HOST_FUNCTION(temporalDurationPrototypeFuncAdd);
static JSC_DECLARE_HOST_FUNCTION(temporalDurationPrototypeFuncSubtract);
static JSC_DECLARE_HOST_FUNCTION(temporalDurationPrototypeFuncRound);
static JSC_DECLARE_HOST_FUNCTION(temporalDurationPrototypeFuncTotal);
static JSC_DECLARE_HOST_FUNCTION(temporalDurationPrototypeFuncToString);
static JSC_DECLARE_HOST_FUNCTION(temporalDurationPrototypeFuncToJSON);
static JSC_DECLARE_HOST_FUNCTION(temporalDurationPrototypeFuncToLocaleString);
static JSC_DECLARE_HOST_FUNCTION(temporalDurationPrototypeFuncValueOf);
static JSC_DECLARE_CUSTOM_GETTER(temporalDurationPrototypeGetterYears);
static JSC_DECLARE_CUSTOM_GETTER(temporalDurationPrototypeGetterMonths);
static JSC_DECLARE_CUSTOM_GETTER(temporalDurationPrototypeGetterWeeks);
static JSC_DECLARE_CUSTOM_GETTER(temporalDurationPrototypeGetterDays);
static JSC_DECLARE_CUSTOM_GETTER(temporalDurationPrototypeGetterHours);
static JSC_DECLARE_CUSTOM_GETTER(temporalDurationPrototypeGetterMinutes);
static JSC_DECLARE_CUSTOM_GETTER(temporalDurationPrototypeGetterSeconds);
static JSC_DECLARE_CUSTOM_GETTER(temporalDurationPrototypeGetterMilliseconds);
static JSC_DECLARE_CUSTOM_GETTER(temporalDurationPrototypeGetterMicroseconds);
static JSC_DECLARE_CUSTOM_GETTER(temporalDurationPrototypeGetterNanoseconds);
static JSC_DECLARE_CUSTOM_GETTER(temporalDurationPrototypeGetterSign);
static JSC_DECLARE_CUSTOM_GETTER(temporalDurationPrototypeGetterBlank);

}

#include "TemporalDurationPrototype.lut.h"

namespace JSC {

const ClassInfo TemporalDurationPrototype::s_info = { "Temporal.Duration"_s, &Base::s_info, &durationPrototypeTable, nullptr, CREATE_METHOD_TABLE(TemporalDurationPrototype) };

/* Source for TemporalDurationPrototype.lut.h
@begin durationPrototypeTable
  with             temporalDurationPrototypeFuncWith               DontEnum|Function 1
  negated          temporalDurationPrototypeFuncNegated            DontEnum|Function 0
  abs              temporalDurationPrototypeFuncAbs                DontEnum|Function 0
  add              temporalDurationPrototypeFuncAdd                DontEnum|Function 1
  subtract         temporalDurationPrototypeFuncSubtract           DontEnum|Function 1
  round            temporalDurationPrototypeFuncRound              DontEnum|Function 1
  total            temporalDurationPrototypeFuncTotal              DontEnum|Function 1
  toString         temporalDurationPrototypeFuncToString           DontEnum|Function 0
  toJSON           temporalDurationPrototypeFuncToJSON             DontEnum|Function 0
  toLocaleString   temporalDurationPrototypeFuncToLocaleString     DontEnum|Function 0
  valueOf          temporalDurationPrototypeFuncValueOf            DontEnum|Function 0
  years            temporalDurationPrototypeGetterYears            DontEnum|ReadOnly|CustomAccessor
  months           temporalDurationPrototypeGetterMonths           DontEnum|ReadOnly|CustomAccessor
  weeks            temporalDurationPrototypeGetterWeeks            DontEnum|ReadOnly|CustomAccessor
  days             temporalDurationPrototypeGetterDays             DontEnum|ReadOnly|CustomAccessor
  hours            temporalDurationPrototypeGetterHours            DontEnum|ReadOnly|CustomAccessor
  minutes          temporalDurationPrototypeGetterMinutes          DontEnum|ReadOnly|CustomAccessor
  seconds          temporalDurationPrototypeGetterSeconds          DontEnum|ReadOnly|CustomAccessor
  milliseconds     temporalDurationPrototypeGetterMilliseconds     DontEnum|ReadOnly|CustomAccessor
  microseconds     temporalDurationPrototypeGetterMicroseconds     DontEnum|ReadOnly|CustomAccessor
  nanoseconds      temporalDurationPrototypeGetterNanoseconds      DontEnum|ReadOnly|CustomAccessor
  sign             temporalDurationPrototypeGetterSign             DontEnum|ReadOnly|CustomAccessor
  blank            temporalDurationPrototypeGetterBlank            DontEnum|ReadOnly|CustomAccessor
@end
*/

TemporalDurationPrototype* TemporalDurationPrototype::create(VM& vm, Structure* structure)
{
    auto* prototype = new (NotNull, allocateCell<TemporalDurationPrototype>(vm)) TemporalDurationPrototype(vm, structure);
    prototype->finishCreation(vm);
    return prototype;
}

Structure* TemporalDurationPrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalDurationPrototype::TemporalDurationPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void TemporalDurationPrototype::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.with
JSC_DEFINE_HOST_FUNCTION(temporalDurationPrototypeFuncWith, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: RequireInternalSlot — branding check.
    auto* duration = dynamicDowncast<TemporalDuration>(callFrame->thisValue());
    if (!duration) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Duration.prototype.with called on value that's not a Duration"_s);

    // Step 2: If temporalDurationLike is not an Object, throw TypeError.
    JSValue durationLike = callFrame->argument(0);
    if (!durationLike.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "First argument to Temporal.Duration.prototype.with must be an object"_s);

    // Steps 3-4: ToTemporalPartialDurationRecord + CreateTemporalDuration.
    auto result = duration->with(globalObject, asObject(durationLike));
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalDuration::tryCreateIfValid(globalObject, WTF::move(result))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.negated
JSC_DEFINE_HOST_FUNCTION(temporalDurationPrototypeFuncNegated, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: RequireInternalSlot — branding check.
    auto* duration = dynamicDowncast<TemporalDuration>(callFrame->thisValue());
    if (!duration) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Duration.prototype.negated called on value that's not a Duration"_s);

    // Step 2: Return CreateNegatedTemporalDuration(duration).
    return JSValue::encode(TemporalDuration::create(vm, globalObject->durationStructure(), TemporalCore::negateDuration(duration->duration())));
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.abs
JSC_DEFINE_HOST_FUNCTION(temporalDurationPrototypeFuncAbs, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: RequireInternalSlot — branding check.
    auto* duration = dynamicDowncast<TemporalDuration>(callFrame->thisValue());
    if (!duration) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Duration.prototype.abs called on value that's not a Duration"_s);

    // Step 2: Return CreateAbsTemporalDuration(duration).
    return JSValue::encode(TemporalDuration::create(vm, globalObject->durationStructure(), TemporalCore::absDuration(duration->duration())));
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.add
JSC_DEFINE_HOST_FUNCTION(temporalDurationPrototypeFuncAdd, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: RequireInternalSlot — branding check.
    auto* duration = dynamicDowncast<TemporalDuration>(callFrame->thisValue());
    if (!duration) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Duration.prototype.add called on value that's not a Duration"_s);

    // Steps 2-3: AddDurations(add, duration, other) → CreateTemporalDuration.
    auto result = duration->add(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalDuration::tryCreateIfValid(globalObject, WTF::move(result))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.subtract
JSC_DEFINE_HOST_FUNCTION(temporalDurationPrototypeFuncSubtract, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: RequireInternalSlot — branding check.
    auto* duration = dynamicDowncast<TemporalDuration>(callFrame->thisValue());
    if (!duration) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Duration.prototype.subtract called on value that's not a Duration"_s);

    // Steps 2-3: AddDurations(subtract, duration, other) → CreateTemporalDuration.
    auto result = duration->subtract(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalDuration::tryCreateIfValid(globalObject, WTF::move(result))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.round
JSC_DEFINE_HOST_FUNCTION(temporalDurationPrototypeFuncRound, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: RequireInternalSlot — branding check.
    auto* duration = dynamicDowncast<TemporalDuration>(callFrame->thisValue());
    if (!duration) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Duration.prototype.round called on value that's not a Duration"_s);

    // Step 2: If roundTo is undefined, throw TypeError.
    auto options = callFrame->argument(0);
    if (options.isUndefined()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Duration.prototype.round requires an options argument"_s);

    // Steps 3-13: GetDifferenceSettings + TemporalDurationRound → CreateTemporalDuration.
    auto result = duration->round(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalDuration::tryCreateIfValid(globalObject, WTF::move(result))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.total
JSC_DEFINE_HOST_FUNCTION(temporalDurationPrototypeFuncTotal, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: RequireInternalSlot — branding check.
    auto* duration = dynamicDowncast<TemporalDuration>(callFrame->thisValue());
    if (!duration) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Duration.prototype.total called on value that's not a Duration"_s);

    // Step 2: If totalOf is undefined, throw TypeError.
    auto options = callFrame->argument(0);
    if (options.isUndefined()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Duration.prototype.total requires an options argument"_s);

    // Steps 3-10: GetDifferenceSettings + TemporalDurationTotal.
    RELEASE_AND_RETURN(scope, JSValue::encode(jsNumber(duration->total(globalObject, options))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.tostring
JSC_DEFINE_HOST_FUNCTION(temporalDurationPrototypeFuncToString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: RequireInternalSlot — branding check.
    auto* duration = dynamicDowncast<TemporalDuration>(callFrame->thisValue());
    if (!duration) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Duration.prototype.toString called on value that's not a Duration"_s);

    // Steps 2-5: GetOptionsObject + precision settings + TemporalDurationToString.
    RELEASE_AND_RETURN(scope, JSValue::encode(jsString(vm, duration->toString(globalObject, callFrame->argument(0)))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.tojson
JSC_DEFINE_HOST_FUNCTION(temporalDurationPrototypeFuncToJSON, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: RequireInternalSlot — branding check.
    auto* duration = dynamicDowncast<TemporalDuration>(callFrame->thisValue());
    if (!duration) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Duration.prototype.toJSON called on value that's not a Duration"_s);

    // Step 2: Return TemporalDurationToString(duration, "auto").
    RELEASE_AND_RETURN(scope, JSValue::encode(jsString(vm, duration->toString(globalObject))));
}

// https://tc39.es/proposal-temporal/#sup-temporal.duration.prototype.tolocalestring
JSC_DEFINE_HOST_FUNCTION(temporalDurationPrototypeFuncToLocaleString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: RequireInternalSlot — branding check.
    auto* duration = dynamicDowncast<TemporalDuration>(callFrame->thisValue());
    if (!duration) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Duration.prototype.toLocaleString called on value that's not a Duration"_s);

    auto* formatter = IntlDurationFormat::create(vm, globalObject->durationFormatStructure());
    formatter->initializeDurationFormat(globalObject, callFrame->argument(0), callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    ISO8601::Duration dur(duration->years(), duration->months(), duration->weeks(), duration->days(),
        duration->hours(), duration->minutes(), duration->seconds(),
        duration->milliseconds(), Int128(duration->microseconds()), Int128(duration->nanoseconds()));
    RELEASE_AND_RETURN(scope, JSValue::encode(formatter->format(globalObject, dur)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.valueof
JSC_DEFINE_HOST_FUNCTION(temporalDurationPrototypeFuncValueOf, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Throw TypeError — Duration has no primitive value.
    return throwVMTypeError(globalObject, scope, "Temporal.Duration.prototype.valueOf must not be called. To compare Duration values, use Temporal.Duration.compare"_s);
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.years
JSC_DEFINE_CUSTOM_GETTER(temporalDurationPrototypeGetterYears, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* duration = dynamicDowncast<TemporalDuration>(JSValue::decode(thisValue));
    if (!duration) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Duration.prototype.years called on value that's not a Duration"_s);

    return JSValue::encode(jsNumber(duration->years()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.months
JSC_DEFINE_CUSTOM_GETTER(temporalDurationPrototypeGetterMonths, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* duration = dynamicDowncast<TemporalDuration>(JSValue::decode(thisValue));
    if (!duration) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Duration.prototype.months called on value that's not a Duration"_s);

    return JSValue::encode(jsNumber(duration->months()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.weeks
JSC_DEFINE_CUSTOM_GETTER(temporalDurationPrototypeGetterWeeks, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* duration = dynamicDowncast<TemporalDuration>(JSValue::decode(thisValue));
    if (!duration) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Duration.prototype.weeks called on value that's not a Duration"_s);

    return JSValue::encode(jsNumber(duration->weeks()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.days
JSC_DEFINE_CUSTOM_GETTER(temporalDurationPrototypeGetterDays, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* duration = dynamicDowncast<TemporalDuration>(JSValue::decode(thisValue));
    if (!duration) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Duration.prototype.days called on value that's not a Duration"_s);

    return JSValue::encode(jsNumber(duration->days()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.hours
JSC_DEFINE_CUSTOM_GETTER(temporalDurationPrototypeGetterHours, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* duration = dynamicDowncast<TemporalDuration>(JSValue::decode(thisValue));
    if (!duration) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Duration.prototype.hours called on value that's not a Duration"_s);

    return JSValue::encode(jsNumber(duration->hours()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.minutes
JSC_DEFINE_CUSTOM_GETTER(temporalDurationPrototypeGetterMinutes, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* duration = dynamicDowncast<TemporalDuration>(JSValue::decode(thisValue));
    if (!duration) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Duration.prototype.minutes called on value that's not a Duration"_s);

    return JSValue::encode(jsNumber(duration->minutes()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.seconds
JSC_DEFINE_CUSTOM_GETTER(temporalDurationPrototypeGetterSeconds, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* duration = dynamicDowncast<TemporalDuration>(JSValue::decode(thisValue));
    if (!duration) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Duration.prototype.seconds called on value that's not a Duration"_s);

    return JSValue::encode(jsNumber(duration->seconds()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.milliseconds
JSC_DEFINE_CUSTOM_GETTER(temporalDurationPrototypeGetterMilliseconds, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* duration = dynamicDowncast<TemporalDuration>(JSValue::decode(thisValue));
    if (!duration) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Duration.prototype.milliseconds called on value that's not a Duration"_s);

    return JSValue::encode(jsNumber(duration->milliseconds()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.microseconds
JSC_DEFINE_CUSTOM_GETTER(temporalDurationPrototypeGetterMicroseconds, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* duration = dynamicDowncast<TemporalDuration>(JSValue::decode(thisValue));
    if (!duration) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Duration.prototype.microseconds called on value that's not a Duration"_s);

    return JSValue::encode(jsNumber(duration->microseconds()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.nanoseconds
JSC_DEFINE_CUSTOM_GETTER(temporalDurationPrototypeGetterNanoseconds, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* duration = dynamicDowncast<TemporalDuration>(JSValue::decode(thisValue));
    if (!duration) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Duration.prototype.nanoseconds called on value that's not a Duration"_s);

    return JSValue::encode(jsNumber(duration->nanoseconds()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.sign
JSC_DEFINE_CUSTOM_GETTER(temporalDurationPrototypeGetterSign, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* duration = dynamicDowncast<TemporalDuration>(JSValue::decode(thisValue));
    if (!duration) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Duration.prototype.sign called on value that's not a Duration"_s);

    return JSValue::encode(jsNumber(duration->sign()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.blank
JSC_DEFINE_CUSTOM_GETTER(temporalDurationPrototypeGetterBlank, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* duration = dynamicDowncast<TemporalDuration>(JSValue::decode(thisValue));
    if (!duration) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Duration.prototype.blank called on value that's not a Duration"_s);

    return JSValue::encode(jsBoolean(!duration->sign()));
}

} // namespace JSC
