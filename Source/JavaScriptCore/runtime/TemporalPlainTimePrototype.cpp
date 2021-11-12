/*
 * Copyright (C) 2021 Apple Inc.
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
#include "TemporalPlainTimePrototype.h"

#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include "TemporalDuration.h"
#include "TemporalPlainTime.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(temporalPlainTimePrototypeFuncAdd);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainTimePrototypeFuncSubtract);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainTimePrototypeFuncWith);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainTimePrototypeFuncUntil);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainTimePrototypeFuncSince);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainTimePrototypeFuncRound);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainTimePrototypeFuncEquals);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainTimePrototypeFuncGetISOFields);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainTimePrototypeFuncToString);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainTimePrototypeFuncToJSON);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainTimePrototypeFuncToLocaleString);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainTimePrototypeFuncValueOf);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainTimePrototypeGetterHour);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainTimePrototypeGetterMinute);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainTimePrototypeGetterSecond);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainTimePrototypeGetterMillisecond);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainTimePrototypeGetterMicrosecond);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainTimePrototypeGetterNanosecond);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainTimePrototypeGetterCalendar);

}

#include "TemporalPlainTimePrototype.lut.h"

namespace JSC {

const ClassInfo TemporalPlainTimePrototype::s_info = { "Temporal.PlainTime", &Base::s_info, &plainTimePrototypeTable, nullptr, CREATE_METHOD_TABLE(TemporalPlainTimePrototype) };

/* Source for TemporalPlainTimePrototype.lut.h
@begin plainTimePrototypeTable
  add              temporalPlainTimePrototypeFuncAdd                DontEnum|Function 1
  subtract         temporalPlainTimePrototypeFuncSubtract           DontEnum|Function 1
  with             temporalPlainTimePrototypeFuncWith               DontEnum|Function 1
  until            temporalPlainTimePrototypeFuncUntil              DontEnum|Function 1
  since            temporalPlainTimePrototypeFuncSince              DontEnum|Function 1
  round            temporalPlainTimePrototypeFuncRound              DontEnum|Function 1
  equals           temporalPlainTimePrototypeFuncEquals             DontEnum|Function 1
  getISOFields     temporalPlainTimePrototypeFuncGetISOFields       DontEnum|Function 0
  toString         temporalPlainTimePrototypeFuncToString           DontEnum|Function 0
  toJSON           temporalPlainTimePrototypeFuncToJSON             DontEnum|Function 0
  toLocaleString   temporalPlainTimePrototypeFuncToLocaleString     DontEnum|Function 0
  valueOf          temporalPlainTimePrototypeFuncValueOf            DontEnum|Function 0
  hour             temporalPlainTimePrototypeGetterHour             DontEnum|ReadOnly|CustomAccessor
  minute           temporalPlainTimePrototypeGetterMinute           DontEnum|ReadOnly|CustomAccessor
  second           temporalPlainTimePrototypeGetterSecond           DontEnum|ReadOnly|CustomAccessor
  millisecond      temporalPlainTimePrototypeGetterMillisecond      DontEnum|ReadOnly|CustomAccessor
  microsecond      temporalPlainTimePrototypeGetterMicrosecond      DontEnum|ReadOnly|CustomAccessor
  nanosecond       temporalPlainTimePrototypeGetterNanosecond       DontEnum|ReadOnly|CustomAccessor
  calendar         temporalPlainTimePrototypeGetterCalendar         DontEnum|ReadOnly|CustomAccessor
@end
*/

TemporalPlainTimePrototype* TemporalPlainTimePrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    auto* prototype = new (NotNull, allocateCell<TemporalPlainTimePrototype>(vm)) TemporalPlainTimePrototype(vm, structure);
    prototype->finishCreation(vm, globalObject);
    return prototype;
}

Structure* TemporalPlainTimePrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalPlainTimePrototype::TemporalPlainTimePrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void TemporalPlainTimePrototype::finishCreation(VM& vm, JSGlobalObject*)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.add
JSC_DEFINE_HOST_FUNCTION(temporalPlainTimePrototypeFuncAdd, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainTime = jsDynamicCast<TemporalPlainTime*>(vm, callFrame->thisValue());
    if (!plainTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainTime.prototype.add called on value that's not a plainTime"_s);

    auto result = plainTime->add(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    return JSValue::encode(TemporalPlainTime::create(vm, globalObject->plainTimeStructure(), WTFMove(result)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.subtract
JSC_DEFINE_HOST_FUNCTION(temporalPlainTimePrototypeFuncSubtract, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainTime = jsDynamicCast<TemporalPlainTime*>(vm, callFrame->thisValue());
    if (!plainTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainTime.prototype.subtract called on value that's not a plainTime"_s);

    auto result = plainTime->subtract(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    return JSValue::encode(TemporalPlainTime::create(vm, globalObject->plainTimeStructure(), WTFMove(result)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.with
JSC_DEFINE_HOST_FUNCTION(temporalPlainTimePrototypeFuncWith, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainTime = jsDynamicCast<TemporalPlainTime*>(vm, callFrame->thisValue());
    if (!plainTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainTime.prototype.with called on value that's not a plainTime"_s);

    JSValue temporalTimeLike  = callFrame->argument(0);
    if (!temporalTimeLike.isObject())
        return throwVMTypeError(globalObject, scope, "First argument to Temporal.PlainTime.prototype.with must be an object"_s);

    auto result = plainTime->with(globalObject, asObject(temporalTimeLike), callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    return JSValue::encode(TemporalPlainTime::create(vm, globalObject->plainTimeStructure(), WTFMove(result)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.until
JSC_DEFINE_HOST_FUNCTION(temporalPlainTimePrototypeFuncUntil, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainTime = jsDynamicCast<TemporalPlainTime*>(vm, callFrame->thisValue());
    if (!plainTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainTime.prototype.until called on value that's not a plainTime"_s);

    auto* other = TemporalPlainTime::from(globalObject, callFrame->argument(0), std::nullopt);
    RETURN_IF_EXCEPTION(scope, { });

    auto result = plainTime->until(globalObject, other, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalDuration::tryCreateIfValid(globalObject, WTFMove(result), globalObject->durationStructure())));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.since
JSC_DEFINE_HOST_FUNCTION(temporalPlainTimePrototypeFuncSince, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainTime = jsDynamicCast<TemporalPlainTime*>(vm, callFrame->thisValue());
    if (!plainTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainTime.prototype.since called on value that's not a plainTime"_s);

    auto* other = TemporalPlainTime::from(globalObject, callFrame->argument(0), std::nullopt);
    RETURN_IF_EXCEPTION(scope, { });

    auto result = plainTime->since(globalObject, other, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalDuration::tryCreateIfValid(globalObject, WTFMove(result), globalObject->durationStructure())));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.round
JSC_DEFINE_HOST_FUNCTION(temporalPlainTimePrototypeFuncRound, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainTime = jsDynamicCast<TemporalPlainTime*>(vm, callFrame->thisValue());
    if (!plainTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainTime.prototype.round called on value that's not a plainTime"_s);

    auto options = callFrame->argument(0);
    if (options.isUndefined())
        return throwVMTypeError(globalObject, scope, "Temporal.PlainTime.prototype.round requires an options argument"_s);

    auto rounded = plainTime->round(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    return JSValue::encode(TemporalPlainTime::create(vm, globalObject->plainTimeStructure(), WTFMove(rounded)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.equals
JSC_DEFINE_HOST_FUNCTION(temporalPlainTimePrototypeFuncEquals, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainTime = jsDynamicCast<TemporalPlainTime*>(vm, callFrame->thisValue());
    if (!plainTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainTime.prototype.equals called on value that's not a plainTime"_s);

    auto* other = TemporalPlainTime::from(globalObject, callFrame->argument(0), std::nullopt);
    RETURN_IF_EXCEPTION(scope, { });

    return JSValue::encode(jsBoolean(plainTime->plainTime() == other->plainTime()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.getisofields
JSC_DEFINE_HOST_FUNCTION(temporalPlainTimePrototypeFuncGetISOFields, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainTime = jsDynamicCast<TemporalPlainTime*>(vm, callFrame->thisValue());
    if (!plainTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainTime.prototype.equals called on value that's not a plainTime"_s);

    JSObject* fields = constructEmptyObject(globalObject);
    fields->putDirect(vm, vm.propertyNames->calendar, plainTime->calendar());
    fields->putDirect(vm, vm.propertyNames->isoHour, jsNumber(plainTime->hour()));
    fields->putDirect(vm, vm.propertyNames->isoMicrosecond, jsNumber(plainTime->microsecond()));
    fields->putDirect(vm, vm.propertyNames->isoMillisecond, jsNumber(plainTime->millisecond()));
    fields->putDirect(vm, vm.propertyNames->isoMinute, jsNumber(plainTime->minute()));
    fields->putDirect(vm, vm.propertyNames->isoNanosecond, jsNumber(plainTime->nanosecond()));
    fields->putDirect(vm, vm.propertyNames->isoSecond, jsNumber(plainTime->second()));
    return JSValue::encode(fields);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.tostring
JSC_DEFINE_HOST_FUNCTION(temporalPlainTimePrototypeFuncToString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainTime = jsDynamicCast<TemporalPlainTime*>(vm, callFrame->thisValue());
    if (!plainTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainTime.prototype.toString called on value that's not a plainTime"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(jsString(vm, plainTime->toString(globalObject, callFrame->argument(0)))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.tojson
JSC_DEFINE_HOST_FUNCTION(temporalPlainTimePrototypeFuncToJSON, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainTime = jsDynamicCast<TemporalPlainTime*>(vm, callFrame->thisValue());
    if (!plainTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainTime.prototype.toJSON called on value that's not a plainTime"_s);

    return JSValue::encode(jsString(vm, plainTime->toString()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.tolocalestring
JSC_DEFINE_HOST_FUNCTION(temporalPlainTimePrototypeFuncToLocaleString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainTime = jsDynamicCast<TemporalPlainTime*>(vm, callFrame->thisValue());
    if (!plainTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainTime.prototype.toLocaleString called on value that's not a plainTime"_s);

    return JSValue::encode(jsString(vm, plainTime->toString()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.valueof
JSC_DEFINE_HOST_FUNCTION(temporalPlainTimePrototypeFuncValueOf, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    return throwVMTypeError(globalObject, scope, "Temporal.PlainTime.prototype.valueOf must not be called. To compare PlainTime values, use Temporal.PlainTime.compare"_s);
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainTimePrototypeGetterHour, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainTime = jsDynamicCast<TemporalPlainTime*>(vm, JSValue::decode(thisValue));
    if (!plainTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainTime.prototype.hour called on value that's not a plainTime"_s);

    return JSValue::encode(jsNumber(plainTime->hour()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainTimePrototypeGetterMinute, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainTime = jsDynamicCast<TemporalPlainTime*>(vm, JSValue::decode(thisValue));
    if (!plainTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainTime.prototype.minute called on value that's not a plainTime"_s);

    return JSValue::encode(jsNumber(plainTime->minute()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainTimePrototypeGetterSecond, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainTime = jsDynamicCast<TemporalPlainTime*>(vm, JSValue::decode(thisValue));
    if (!plainTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainTime.prototype.second called on value that's not a plainTime"_s);

    return JSValue::encode(jsNumber(plainTime->second()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainTimePrototypeGetterMillisecond, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainTime = jsDynamicCast<TemporalPlainTime*>(vm, JSValue::decode(thisValue));
    if (!plainTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainTime.prototype.millisecond called on value that's not a plainTime"_s);

    return JSValue::encode(jsNumber(plainTime->millisecond()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainTimePrototypeGetterMicrosecond, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainTime = jsDynamicCast<TemporalPlainTime*>(vm, JSValue::decode(thisValue));
    if (!plainTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainTime.prototype.microsecond called on value that's not a plainTime"_s);

    return JSValue::encode(jsNumber(plainTime->microsecond()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainTimePrototypeGetterNanosecond, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainTime = jsDynamicCast<TemporalPlainTime*>(vm, JSValue::decode(thisValue));
    if (!plainTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainTime.prototype.nanosecond called on value that's not a plainTime"_s);

    return JSValue::encode(jsNumber(plainTime->nanosecond()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainTimePrototypeGetterCalendar, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainTime = jsDynamicCast<TemporalPlainTime*>(vm, JSValue::decode(thisValue));
    if (!plainTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainTime.prototype.calendar called on value that's not a plainTime"_s);

    return JSValue::encode(plainTime->calendar());
}

} // namespace JSC
