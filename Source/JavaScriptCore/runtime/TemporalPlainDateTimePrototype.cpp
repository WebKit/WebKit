/*
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
#include "TemporalPlainDateTimePrototype.h"

#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include "TemporalPlainDateTime.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncGetISOFields);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncToString);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncToJSON);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncToLocaleString);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncValueOf);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterCalendar);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterMonth);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterMonthCode);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterDay);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterHour);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterMinute);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterSecond);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterMillisecond);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterMicrosecond);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterNanosecond);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterDayOfWeek);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterDayOfYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterWeekOfYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterDaysInWeek);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterDaysInMonth);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterDaysInYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterMonthsInYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterInLeapYear);

}

#include "TemporalPlainDateTimePrototype.lut.h"

namespace JSC {

const ClassInfo TemporalPlainDateTimePrototype::s_info = { "Temporal.PlainDateTime"_s, &Base::s_info, &plainDateTimePrototypeTable, nullptr, CREATE_METHOD_TABLE(TemporalPlainDateTimePrototype) };

/* Source for TemporalPlainDateTimePrototype.lut.h
@begin plainDateTimePrototypeTable
  getISOFields     temporalPlainDateTimePrototypeFuncGetISOFields       DontEnum|Function 0
  toString         temporalPlainDateTimePrototypeFuncToString           DontEnum|Function 0
  toJSON           temporalPlainDateTimePrototypeFuncToJSON             DontEnum|Function 0
  toLocaleString   temporalPlainDateTimePrototypeFuncToLocaleString     DontEnum|Function 0
  valueOf          temporalPlainDateTimePrototypeFuncValueOf            DontEnum|Function 0
  calendar         temporalPlainDateTimePrototypeGetterCalendar         DontEnum|ReadOnly|CustomAccessor
  year             temporalPlainDateTimePrototypeGetterYear             DontEnum|ReadOnly|CustomAccessor
  month            temporalPlainDateTimePrototypeGetterMonth            DontEnum|ReadOnly|CustomAccessor
  monthCode        temporalPlainDateTimePrototypeGetterMonthCode        DontEnum|ReadOnly|CustomAccessor
  day              temporalPlainDateTimePrototypeGetterDay              DontEnum|ReadOnly|CustomAccessor
  hour             temporalPlainDateTimePrototypeGetterHour             DontEnum|ReadOnly|CustomAccessor
  minute           temporalPlainDateTimePrototypeGetterMinute           DontEnum|ReadOnly|CustomAccessor
  second           temporalPlainDateTimePrototypeGetterSecond           DontEnum|ReadOnly|CustomAccessor
  millisecond      temporalPlainDateTimePrototypeGetterMillisecond      DontEnum|ReadOnly|CustomAccessor
  microsecond      temporalPlainDateTimePrototypeGetterMicrosecond      DontEnum|ReadOnly|CustomAccessor
  nanosecond       temporalPlainDateTimePrototypeGetterNanosecond       DontEnum|ReadOnly|CustomAccessor
  dayOfWeek        temporalPlainDateTimePrototypeGetterDayOfWeek        DontEnum|ReadOnly|CustomAccessor
  dayOfYear        temporalPlainDateTimePrototypeGetterDayOfYear        DontEnum|ReadOnly|CustomAccessor
  weekOfYear       temporalPlainDateTimePrototypeGetterWeekOfYear       DontEnum|ReadOnly|CustomAccessor
  daysInWeek       temporalPlainDateTimePrototypeGetterDaysInWeek       DontEnum|ReadOnly|CustomAccessor
  daysInMonth      temporalPlainDateTimePrototypeGetterDaysInMonth      DontEnum|ReadOnly|CustomAccessor
  daysInYear       temporalPlainDateTimePrototypeGetterDaysInYear       DontEnum|ReadOnly|CustomAccessor
  monthsInYear     temporalPlainDateTimePrototypeGetterMonthsInYear     DontEnum|ReadOnly|CustomAccessor
  inLeapYear       temporalPlainDateTimePrototypeGetterInLeapYear       DontEnum|ReadOnly|CustomAccessor
@end
*/

TemporalPlainDateTimePrototype* TemporalPlainDateTimePrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    auto* prototype = new (NotNull, allocateCell<TemporalPlainDateTimePrototype>(vm)) TemporalPlainDateTimePrototype(vm, structure);
    prototype->finishCreation(vm, globalObject);
    return prototype;
}

Structure* TemporalPlainDateTimePrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalPlainDateTimePrototype::TemporalPlainDateTimePrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void TemporalPlainDateTimePrototype::finishCreation(VM& vm, JSGlobalObject*)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.getisofields
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncGetISOFields, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = jsDynamicCast<TemporalPlainDateTime*>(callFrame->thisValue());
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.equals called on value that's not a PlainDateTime"_s);

    JSObject* fields = constructEmptyObject(globalObject);
    fields->putDirect(vm, vm.propertyNames->calendar, plainDateTime->calendar());
    fields->putDirect(vm, vm.propertyNames->isoDay, jsNumber(plainDateTime->day()));
    fields->putDirect(vm, vm.propertyNames->isoHour, jsNumber(plainDateTime->hour()));
    fields->putDirect(vm, vm.propertyNames->isoMicrosecond, jsNumber(plainDateTime->microsecond()));
    fields->putDirect(vm, vm.propertyNames->isoMillisecond, jsNumber(plainDateTime->millisecond()));
    fields->putDirect(vm, vm.propertyNames->isoMinute, jsNumber(plainDateTime->minute()));
    fields->putDirect(vm, vm.propertyNames->isoMonth, jsNumber(plainDateTime->month()));
    fields->putDirect(vm, vm.propertyNames->isoNanosecond, jsNumber(plainDateTime->nanosecond()));
    fields->putDirect(vm, vm.propertyNames->isoSecond, jsNumber(plainDateTime->second()));
    fields->putDirect(vm, vm.propertyNames->isoYear, jsNumber(plainDateTime->year()));
    return JSValue::encode(fields);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.tostring
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncToString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = jsDynamicCast<TemporalPlainDateTime*>(callFrame->thisValue());
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.toString called on value that's not a PlainDateTime"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(jsString(vm, plainDateTime->toString(globalObject, callFrame->argument(0)))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.tojson
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncToJSON, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = jsDynamicCast<TemporalPlainDateTime*>(callFrame->thisValue());
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.toJSON called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsString(vm, plainDateTime->toString()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.tolocalestring
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncToLocaleString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = jsDynamicCast<TemporalPlainDateTime*>(callFrame->thisValue());
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.toLocaleString called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsString(vm, plainDateTime->toString()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.valueof
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncValueOf, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.valueOf must not be called. To compare PlainDateTime values, use Temporal.PlainDateTime.compare"_s);
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterCalendar, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = jsDynamicCast<TemporalPlainDateTime*>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.calendar called on value that's not a PlainDateTime"_s);

    return JSValue::encode(plainDateTime->calendar());
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = jsDynamicCast<TemporalPlainDateTime*>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.year called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsNumber(plainDateTime->year()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterMonth, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = jsDynamicCast<TemporalPlainDateTime*>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.month called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsNumber(plainDateTime->month()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterMonthCode, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = jsDynamicCast<TemporalPlainDateTime*>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.monthCode called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsNontrivialString(vm, plainDateTime->monthCode()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterDay, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = jsDynamicCast<TemporalPlainDateTime*>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.day called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsNumber(plainDateTime->day()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterHour, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = jsDynamicCast<TemporalPlainDateTime*>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.hour called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsNumber(plainDateTime->hour()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterMinute, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = jsDynamicCast<TemporalPlainDateTime*>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.minute called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsNumber(plainDateTime->minute()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterSecond, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = jsDynamicCast<TemporalPlainDateTime*>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.second called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsNumber(plainDateTime->second()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterMillisecond, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = jsDynamicCast<TemporalPlainDateTime*>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.millisecond called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsNumber(plainDateTime->millisecond()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterMicrosecond, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = jsDynamicCast<TemporalPlainDateTime*>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.microsecond called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsNumber(plainDateTime->microsecond()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterNanosecond, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = jsDynamicCast<TemporalPlainDateTime*>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.nanosecond called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsNumber(plainDateTime->nanosecond()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterDayOfWeek, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = jsDynamicCast<TemporalPlainDateTime*>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.dayOfWeek called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsNumber(plainDateTime->dayOfWeek()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterDayOfYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = jsDynamicCast<TemporalPlainDateTime*>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.dayOfYear called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsNumber(plainDateTime->dayOfYear()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterWeekOfYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = jsDynamicCast<TemporalPlainDateTime*>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.weekOfYear called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsNumber(plainDateTime->weekOfYear()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterDaysInWeek, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = jsDynamicCast<TemporalPlainDateTime*>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.daysInWeek called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsNumber(7)); // ISO8601 calendar always returns 7.
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterDaysInMonth, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = jsDynamicCast<TemporalPlainDateTime*>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.daysInMonth called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsNumber(ISO8601::daysInMonth(plainDateTime->year(), plainDateTime->month())));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterDaysInYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = jsDynamicCast<TemporalPlainDateTime*>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.daysInYear called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsNumber(isLeapYear(plainDateTime->year()) ? 366 : 365));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterMonthsInYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = jsDynamicCast<TemporalPlainDateTime*>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.monthsInYear called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsNumber(12)); // ISO8601 calendar always returns 12.
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterInLeapYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = jsDynamicCast<TemporalPlainDateTime*>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.inLeapYear called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsBoolean(isLeapYear(plainDateTime->year())));
}

} // namespace JSC
