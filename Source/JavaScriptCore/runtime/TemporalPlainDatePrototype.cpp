/*
 * Copyright (C) 2022 Apple Inc.
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
#include "TemporalPlainDatePrototype.h"

#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include "TemporalDuration.h"
#include "TemporalPlainDate.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(temporalPlainDatePrototypeFuncToString);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterMonth);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterMonthCode);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDay);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDayOfWeek);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDayOfYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterWeekOfYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDaysInWeek);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDaysInMonth);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDaysInYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterMonthsInYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterInLeapYear);

}

#include "TemporalPlainDatePrototype.lut.h"

namespace JSC {

const ClassInfo TemporalPlainDatePrototype::s_info = { "Temporal.PlainDate"_s, &Base::s_info, &plainDatePrototypeTable, nullptr, CREATE_METHOD_TABLE(TemporalPlainDatePrototype) };

/* Source for TemporalPlainDatePrototype.lut.h
@begin plainDatePrototypeTable
  toString         temporalPlainDatePrototypeFuncToString           DontEnum|Function 0
  year             temporalPlainDatePrototypeGetterYear             DontEnum|ReadOnly|CustomAccessor
  month            temporalPlainDatePrototypeGetterMonth            DontEnum|ReadOnly|CustomAccessor
  monthCode        temporalPlainDatePrototypeGetterMonthCode        DontEnum|ReadOnly|CustomAccessor
  day              temporalPlainDatePrototypeGetterDay              DontEnum|ReadOnly|CustomAccessor
  dayOfWeek        temporalPlainDatePrototypeGetterDayOfWeek        DontEnum|ReadOnly|CustomAccessor
  dayOfYear        temporalPlainDatePrototypeGetterDayOfYear        DontEnum|ReadOnly|CustomAccessor
  weekOfYear       temporalPlainDatePrototypeGetterWeekOfYear       DontEnum|ReadOnly|CustomAccessor
  daysInWeek       temporalPlainDatePrototypeGetterDaysInWeek       DontEnum|ReadOnly|CustomAccessor
  daysInMonth      temporalPlainDatePrototypeGetterDaysInMonth      DontEnum|ReadOnly|CustomAccessor
  daysInYear       temporalPlainDatePrototypeGetterDaysInYear       DontEnum|ReadOnly|CustomAccessor
  monthsInYear     temporalPlainDatePrototypeGetterMonthsInYear     DontEnum|ReadOnly|CustomAccessor
  inLeapYear       temporalPlainDatePrototypeGetterInLeapYear       DontEnum|ReadOnly|CustomAccessor
@end
*/

TemporalPlainDatePrototype* TemporalPlainDatePrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    auto* prototype = new (NotNull, allocateCell<TemporalPlainDatePrototype>(vm)) TemporalPlainDatePrototype(vm, structure);
    prototype->finishCreation(vm, globalObject);
    return prototype;
}

Structure* TemporalPlainDatePrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalPlainDatePrototype::TemporalPlainDatePrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void TemporalPlainDatePrototype::finishCreation(VM& vm, JSGlobalObject*)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.tostring
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncToString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(callFrame->thisValue());
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.toString called on value that's not a PlainDate"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(jsString(vm, plainDate->toString(globalObject, callFrame->argument(0)))));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.year called on value that's not a plainDate"_s);

    return JSValue::encode(jsNumber(plainDate->year()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterMonth, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.month called on value that's not a plainDate"_s);

    return JSValue::encode(jsNumber(plainDate->month()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterMonthCode, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.monthCode called on value that's not a plainDate"_s);

    return JSValue::encode(jsNontrivialString(vm, plainDate->monthCode()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDay, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.day called on value that's not a plainDate"_s);

    return JSValue::encode(jsNumber(plainDate->day()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDayOfWeek, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.dayOfWeek called on value that's not a plainDate"_s);

    return JSValue::encode(jsNumber(plainDate->dayOfWeek()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDayOfYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.dayOfYear called on value that's not a plainDate"_s);

    return JSValue::encode(jsNumber(plainDate->dayOfYear()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterWeekOfYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.weekOfYear called on value that's not a plainDate"_s);

    return JSValue::encode(jsNumber(plainDate->weekOfYear()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDaysInWeek, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.daysInWeek called on value that's not a plainDate"_s);

    return JSValue::encode(jsNumber(7)); // ISO8601 calendar always returns 7.
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDaysInMonth, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.daysInMonth called on value that's not a plainDate"_s);

    return JSValue::encode(jsNumber(ISO8601::daysInMonth(plainDate->year(), plainDate->month())));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDaysInYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.daysInYear called on value that's not a plainDate"_s);

    return JSValue::encode(jsNumber(isLeapYear(plainDate->year()) ? 366 : 365));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterMonthsInYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.monthsInYear called on value that's not a plainDate"_s);

    return JSValue::encode(jsNumber(12)); // ISO8601 calendar always returns 12.
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterInLeapYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.inLeapYear called on value that's not a plainDate"_s);

    return JSValue::encode(jsBoolean(isLeapYear(plainDate->year())));
}

} // namespace JSC
