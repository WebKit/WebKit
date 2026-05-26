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
#include "IntlDateTimeFormat.h"
#include "JSCInlines.h"
#include "TemporalInstant.h"
#include "TemporalObject.h"
// FIXME: #include "TemporalZonedDateTime.h"
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

// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.add
JSC_DEFINE_HOST_FUNCTION(temporalInstantPrototypeFuncAdd, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: RequireInternalSlot — branding check.
    TemporalInstant* instant = dynamicDowncast<TemporalInstant>(callFrame->thisValue());
    if (!instant) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Instant.prototype.add called on value that's not a Instant"_s);

    // Steps 2-3: ToTemporalDuration + validate no calendar units.
    ISO8601::Duration duration = TemporalDuration::toLimitedDuration(globalObject, callFrame->argument(0), disallowedAdditionUnits);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 4: AddInstant(instant.[[EpochNanoseconds]], duration).
    std::optional<ISO8601::ExactTime> newExactTime = instant->exactTime().add(duration);
    if (!newExactTime) [[unlikely]] {
        throwRangeError(globalObject, scope, "Addition is outside of supported range for Temporal.Instant"_s);
        return { };
    }

    // Step 5: CreateTemporalInstant(result).
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalInstant::tryCreateIfValid(globalObject, *newExactTime)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.subtract
JSC_DEFINE_HOST_FUNCTION(temporalInstantPrototypeFuncSubtract, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: RequireInternalSlot — branding check.
    TemporalInstant* instant = dynamicDowncast<TemporalInstant>(callFrame->thisValue());
    if (!instant) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Instant.prototype.subtract called on value that's not a Instant"_s);

    // Steps 2-3: ToTemporalDuration + validate no calendar units.
    ISO8601::Duration duration = TemporalDuration::toLimitedDuration(globalObject, callFrame->argument(0), disallowedAdditionUnits);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 4: AddInstant(instant.[[EpochNanoseconds]], -duration).
    std::optional<ISO8601::ExactTime> newExactTime = instant->exactTime().add(-duration);
    if (!newExactTime) [[unlikely]] {
        throwRangeError(globalObject, scope, "Subtraction is outside of supported range for Temporal.Instant"_s);
        return { };
    }

    // Step 5: CreateTemporalInstant(result).
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalInstant::tryCreateIfValid(globalObject, *newExactTime)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.until
JSC_DEFINE_HOST_FUNCTION(temporalInstantPrototypeFuncUntil, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: RequireInternalSlot — branding check.
    TemporalInstant* instant = dynamicDowncast<TemporalInstant>(callFrame->thisValue());
    if (!instant) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Instant.prototype.until called on value that's not a Instant"_s);

    // Step 2: ToTemporalInstant(other).
    TemporalInstant* other = TemporalInstant::toInstant(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    // Steps 3-7: GetDifferenceSettings + DifferenceTemporalInstant (delegated to difference()).
    JSValue options = callFrame->argument(1);
    ISO8601::Duration result = instant->difference(globalObject, other, options);
    RETURN_IF_EXCEPTION(scope, { });

    return JSValue::encode(TemporalDuration::create(vm, globalObject->durationStructure(), WTF::move(result)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.since
JSC_DEFINE_HOST_FUNCTION(temporalInstantPrototypeFuncSince, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: RequireInternalSlot — branding check.
    TemporalInstant* instant = dynamicDowncast<TemporalInstant>(callFrame->thisValue());
    if (!instant) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Instant.prototype.since called on value that's not a Instant"_s);

    // Step 2: ToTemporalInstant(other).
    TemporalInstant* other = TemporalInstant::toInstant(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    // Steps 3-7: DifferenceTemporalInstant(since, instant, other, ...) = other.until(instant).
    JSValue options = callFrame->argument(1);
    ISO8601::Duration result = other->difference(globalObject, instant, options);
    RETURN_IF_EXCEPTION(scope, { });

    return JSValue::encode(TemporalDuration::create(vm, globalObject->durationStructure(), WTF::move(result)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.round
JSC_DEFINE_HOST_FUNCTION(temporalInstantPrototypeFuncRound, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: RequireInternalSlot — branding check.
    TemporalInstant* instant = dynamicDowncast<TemporalInstant>(callFrame->thisValue());
    if (!instant) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Instant.prototype.round called on value that's not a Instant"_s);

    // Step 2: If roundTo is undefined, throw TypeError.
    JSValue options = callFrame->argument(0);
    if (options.isUndefined()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Instant.prototype.round requires an options argument"_s);

    // Steps 3-8: GetOptionsObject + settings + RoundTemporalInstant (delegated to round()).
    ISO8601::ExactTime newExactTime = instant->round(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 9: CreateTemporalInstant(result).
    return JSValue::encode(TemporalInstant::create(vm, globalObject->instantStructure(), newExactTime));
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.equals
JSC_DEFINE_HOST_FUNCTION(temporalInstantPrototypeFuncEquals, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: RequireInternalSlot — branding check.
    TemporalInstant* instant = dynamicDowncast<TemporalInstant>(callFrame->thisValue());
    if (!instant) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Instant.prototype.equals called on value that's not a Instant"_s);

    // Step 2: ToTemporalInstant(other).
    TemporalInstant* other = TemporalInstant::toInstant(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    // Step 3: Return instant.[[EpochNanoseconds]] = other.[[EpochNanoseconds]].
    return JSValue::encode(jsBoolean(instant->exactTime() == other->exactTime()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.tostring
JSC_DEFINE_HOST_FUNCTION(temporalInstantPrototypeFuncToString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    TemporalInstant* instant = dynamicDowncast<TemporalInstant>(callFrame->thisValue());
    if (!instant) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Instant.prototype.toString called on value that's not a Instant"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(jsString(vm, instant->toString(globalObject, callFrame->argument(0)))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.tojson
JSC_DEFINE_HOST_FUNCTION(temporalInstantPrototypeFuncToJSON, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: RequireInternalSlot — branding check.
    TemporalInstant* instant = dynamicDowncast<TemporalInstant>(callFrame->thisValue());
    if (!instant) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Instant.prototype.toJSON called on value that's not a Instant"_s);

    // Step 2: Return TemporalInstantToString(instant, undefined, "auto").
    RELEASE_AND_RETURN(scope, JSValue::encode(jsString(vm, instant->toString())));
}

// https://tc39.es/proposal-temporal/#sup-temporal.instant.prototype.tolocalestring
// FIXME: Is this better as a JSBuiltin?
JSC_DEFINE_HOST_FUNCTION(temporalInstantPrototypeFuncToLocaleString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    TemporalInstant* instant = dynamicDowncast<TemporalInstant>(callFrame->thisValue());
    if (!instant) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Instant.prototype.toLocaleString called on value that's not a Instant"_s);

    IntlDateTimeFormat* formatter = IntlDateTimeFormat::create(vm, globalObject->dateTimeFormatStructure());
    RETURN_IF_EXCEPTION(scope, { });

    formatter->initializeDateTimeFormat(globalObject, callFrame->argument(0), callFrame->argument(1), IntlDateTimeFormat::RequiredComponent::Any, IntlDateTimeFormat::Defaults::All);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(formatter->format(globalObject, instant->exactTime().epochMilliseconds(), IntlDateTimeFormat::TemporalFieldKind::Instant)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.valueof
JSC_DEFINE_HOST_FUNCTION(temporalInstantPrototypeFuncValueOf, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Throw TypeError — Instant has no primitive value.
    return throwVMTypeError(globalObject, scope, "Temporal.Instant.prototype.valueOf must not be called. To compare Instant values, use Temporal.Instant.compare"_s);
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.tozoneddatetimeiso
// FIXME: ZonedDateTime
JSC_DEFINE_HOST_FUNCTION(temporalInstantPrototypeFuncToZonedDateTimeISO, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return throwVMTypeError(globalObject, scope, "Temporal.Instant.prototype.toZonedDateTimeISO is not yet implemented"_s);
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.epochmilliseconds
JSC_DEFINE_CUSTOM_GETTER(temporalInstantPrototypeGetterEpochMilliseconds, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: RequireInternalSlot — branding check.
    TemporalInstant* instant = dynamicDowncast<TemporalInstant>(JSValue::decode(thisValue));
    if (!instant) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Instant.prototype.epochMilliseconds called on value that's not a Instant"_s);

    // Step 2: Return 𝔽(⌊instant.[[EpochNanoseconds]] / 10^6⌋).
    return JSValue::encode(jsNumber(instant->exactTime().floorEpochMilliseconds()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.epochnanoseconds
JSC_DEFINE_CUSTOM_GETTER(temporalInstantPrototypeGetterEpochNanoseconds, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: RequireInternalSlot — branding check.
    TemporalInstant* instant = dynamicDowncast<TemporalInstant>(JSValue::decode(thisValue));
    if (!instant) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.Instant.prototype.epochNanoseconds called on value that's not a Instant"_s);

    // Step 2: Return ! CreateBigInt(instant.[[EpochNanoseconds]]).
    RELEASE_AND_RETURN(scope, JSValue::encode(JSBigInt::createFrom(globalObject, instant->exactTime().epochNanoseconds())));
}

} // namespace JSC
