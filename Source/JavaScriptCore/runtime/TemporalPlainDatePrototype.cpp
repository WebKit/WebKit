/*
 * Copyright (C) 2022 Apple Inc.
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
#include "TemporalPlainDatePrototype.h"

#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include "TemporalDuration.h"
#include "TemporalPlainDate.h"
#include "TemporalPlainDateTime.h"
#include "TemporalPlainTime.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(temporalPlainDatePrototypeFuncGetISOFields);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDatePrototypeFuncAdd);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDatePrototypeFuncSubtract);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDatePrototypeFuncWith);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDatePrototypeFuncUntil);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDatePrototypeFuncSince);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDatePrototypeFuncEquals);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDatePrototypeFuncToPlainDateTime);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDatePrototypeFuncToString);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDatePrototypeFuncToJSON);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDatePrototypeFuncToLocaleString);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDatePrototypeFuncValueOf);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterCalendar);
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
  getISOFields     temporalPlainDatePrototypeFuncGetISOFields       DontEnum|Function 0
  add              temporalPlainDatePrototypeFuncAdd                DontEnum|Function 1
  subtract         temporalPlainDatePrototypeFuncSubtract           DontEnum|Function 1
  with             temporalPlainDatePrototypeFuncWith               DontEnum|Function 1
  until            temporalPlainDatePrototypeFuncUntil              DontEnum|Function 1
  since            temporalPlainDatePrototypeFuncSince              DontEnum|Function 1
  equals           temporalPlainDatePrototypeFuncEquals             DontEnum|Function 1
  toPlainDateTime  temporalPlainDatePrototypeFuncToPlainDateTime    DontEnum|Function 0
  toString         temporalPlainDatePrototypeFuncToString           DontEnum|Function 0
  toJSON           temporalPlainDatePrototypeFuncToJSON             DontEnum|Function 0
  toLocaleString   temporalPlainDatePrototypeFuncToLocaleString     DontEnum|Function 0
  valueOf          temporalPlainDatePrototypeFuncValueOf            DontEnum|Function 0
  calendar         temporalPlainDatePrototypeGetterCalendar         DontEnum|ReadOnly|CustomAccessor
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

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.getisofields
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncGetISOFields, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(callFrame->thisValue());
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.getISOFields called on value that's not a PlainDate"_s);

    JSObject* fields = constructEmptyObject(globalObject);
    fields->putDirect(vm, vm.propertyNames->calendar, plainDate->calendar());
    fields->putDirect(vm, vm.propertyNames->isoDay, jsNumber(plainDate->day()));
    fields->putDirect(vm, vm.propertyNames->isoMonth, jsNumber(plainDate->month()));
    fields->putDirect(vm, vm.propertyNames->isoYear, jsNumber(plainDate->year()));
    return JSValue::encode(fields);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.add
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncAdd, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(callFrame->thisValue());
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.add called on value that's not a PlainDate"_s);

    auto duration = TemporalDuration::toISO8601Duration(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    JSObject* options = intlGetOptionsObject(globalObject, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    TemporalOverflow overflow = toTemporalOverflow(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    ISO8601::PlainDate result = TemporalCalendar::isoDateAdd(globalObject, plainDate->plainDate(), duration, overflow);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainDate::create(vm, globalObject->plainDateStructure(), WTFMove(result))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.subtract
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncSubtract, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(callFrame->thisValue());
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.subtract called on value that's not a PlainDate"_s);

    auto duration = TemporalDuration::toISO8601Duration(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    JSObject* options = intlGetOptionsObject(globalObject, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    TemporalOverflow overflow = toTemporalOverflow(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    ISO8601::PlainDate result = TemporalCalendar::isoDateAdd(globalObject, plainDate->plainDate(), -duration, overflow);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainDate::create(vm, globalObject->plainDateStructure(), WTFMove(result))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.with
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncWith, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(callFrame->thisValue());
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.with called on value that's not a PlainDate"_s);

    JSValue temporalDateLike  = callFrame->argument(0);
    if (!temporalDateLike.isObject())
        return throwVMTypeError(globalObject, scope, "First argument to Temporal.PlainDate.prototype.with must be an object"_s);

    auto result = plainDate->with(globalObject, asObject(temporalDateLike), callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    return JSValue::encode(TemporalPlainDate::create(vm, globalObject->plainDateStructure(), WTFMove(result)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.until
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncUntil, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(callFrame->thisValue());
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.until called on value that's not a PlainDate"_s);

    auto* other = TemporalPlainDate::from(globalObject, callFrame->argument(0), std::nullopt);
    RETURN_IF_EXCEPTION(scope, { });

    auto result = plainDate->until(globalObject, other, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalDuration::tryCreateIfValid(globalObject, WTFMove(result), globalObject->durationStructure())));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.since
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncSince, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(callFrame->thisValue());
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.since called on value that's not a PlainDate"_s);

    auto* other = TemporalPlainDate::from(globalObject, callFrame->argument(0), std::nullopt);
    RETURN_IF_EXCEPTION(scope, { });

    auto result = plainDate->since(globalObject, other, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalDuration::tryCreateIfValid(globalObject, WTFMove(result), globalObject->durationStructure())));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.equals
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncEquals, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(callFrame->thisValue());
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.equals called on value that's not a PlainDate"_s);

    auto* other = TemporalPlainDate::from(globalObject, callFrame->argument(0), std::nullopt);
    RETURN_IF_EXCEPTION(scope, { });

    if (plainDate->plainDate() != other->plainDate())
        return JSValue::encode(jsBoolean(false));

    RELEASE_AND_RETURN(scope, JSValue::encode(jsBoolean(plainDate->calendar()->equals(globalObject, other->calendar()))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.toplaindatetime
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncToPlainDateTime, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(callFrame->thisValue());
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.toPlainDateTime called on value that's not a PlainDate"_s);

    JSValue itemValue = callFrame->argument(0); 
    if (itemValue.isUndefined())
        RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainDateTime::tryCreateIfValid(globalObject, globalObject->plainDateTimeStructure(), plainDate->plainDate(), { })));

    auto* plainTime = TemporalPlainTime::from(globalObject, itemValue, std::nullopt);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainDateTime::tryCreateIfValid(globalObject, globalObject->plainDateTimeStructure(), plainDate->plainDate(), plainTime->plainTime())));
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

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.tojson
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncToJSON, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(callFrame->thisValue());
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.toJSON called on value that's not a PlainDate"_s);

    return JSValue::encode(jsString(vm, plainDate->toString()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.tolocalestring
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncToLocaleString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(callFrame->thisValue());
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.toLocaleString called on value that's not a PlainDate"_s);

    return JSValue::encode(jsString(vm, plainDate->toString()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.valueof
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncValueOf, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.valueOf must not be called. To compare PlainDate values, use Temporal.PlainDate.compare"_s);
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterCalendar, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.calendar called on value that's not a PlainDate"_s);

    return JSValue::encode(plainDate->calendar());
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.year called on value that's not a PlainDate"_s);

    return JSValue::encode(jsNumber(plainDate->year()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterMonth, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.month called on value that's not a PlainDate"_s);

    return JSValue::encode(jsNumber(plainDate->month()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterMonthCode, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.monthCode called on value that's not a PlainDate"_s);

    return JSValue::encode(jsNontrivialString(vm, plainDate->monthCode()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDay, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.day called on value that's not a PlainDate"_s);

    return JSValue::encode(jsNumber(plainDate->day()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDayOfWeek, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.dayOfWeek called on value that's not a PlainDate"_s);

    return JSValue::encode(jsNumber(plainDate->dayOfWeek()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDayOfYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.dayOfYear called on value that's not a PlainDate"_s);

    return JSValue::encode(jsNumber(plainDate->dayOfYear()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterWeekOfYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.weekOfYear called on value that's not a PlainDate"_s);

    return JSValue::encode(jsNumber(plainDate->weekOfYear()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDaysInWeek, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.daysInWeek called on value that's not a PlainDate"_s);

    return JSValue::encode(jsNumber(7)); // ISO8601 calendar always returns 7.
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDaysInMonth, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.daysInMonth called on value that's not a PlainDate"_s);

    return JSValue::encode(jsNumber(ISO8601::daysInMonth(plainDate->year(), plainDate->month())));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDaysInYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.daysInYear called on value that's not a PlainDate"_s);

    return JSValue::encode(jsNumber(isLeapYear(plainDate->year()) ? 366 : 365));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterMonthsInYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.monthsInYear called on value that's not a PlainDate"_s);

    return JSValue::encode(jsNumber(12)); // ISO8601 calendar always returns 12.
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterInLeapYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.inLeapYear called on value that's not a PlainDate"_s);

    return JSValue::encode(jsBoolean(isLeapYear(plainDate->year())));
}

} // namespace JSC
