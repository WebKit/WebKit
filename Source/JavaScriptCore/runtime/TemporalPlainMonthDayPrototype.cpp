/*
 * Copyright (C) 2025 Igalia, S.L.
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
#include "TemporalPlainMonthDayPrototype.h"

#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include "TemporalDuration.h"
#include "TemporalPlainDate.h"
#include "TemporalPlainDateTime.h"
#include "TemporalPlainMonthDay.h"
#include "TemporalPlainTime.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(temporalPlainMonthDayPrototypeFuncToPlainDate);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainMonthDayPrototypeFuncToString);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainMonthDayPrototypeFuncToJSON);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainMonthDayPrototypeFuncToLocaleString);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainMonthDayPrototypeFuncWith);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainMonthDayPrototypeFuncEquals);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainMonthDayPrototypeFuncValueOf);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainMonthDayPrototypeGetterCalendarId);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainMonthDayPrototypeGetterDay);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainMonthDayPrototypeGetterMonthCode);

}

#include "TemporalPlainMonthDayPrototype.lut.h"

namespace JSC {

const ClassInfo TemporalPlainMonthDayPrototype::s_info = { "Temporal.PlainMonthDay"_s, &Base::s_info, &plainMonthDayPrototypeTable, nullptr, CREATE_METHOD_TABLE(TemporalPlainMonthDayPrototype) };

/* Source for TemporalPlainMonthDayPrototype.lut.h
@begin plainMonthDayPrototypeTable
  toPlainDate      temporalPlainMonthDayPrototypeFuncToPlainDate        DontEnum|Function 1
  toString         temporalPlainMonthDayPrototypeFuncToString           DontEnum|Function 0
  toJSON           temporalPlainMonthDayPrototypeFuncToJSON             DontEnum|Function 0
  toLocaleString   temporalPlainMonthDayPrototypeFuncToLocaleString     DontEnum|Function 0
  with             temporalPlainMonthDayPrototypeFuncWith               DontEnum|Function 1
  equals           temporalPlainMonthDayPrototypeFuncEquals             DontEnum|Function 1
  valueOf          temporalPlainMonthDayPrototypeFuncValueOf            DontEnum|Function 0
  calendarId       temporalPlainMonthDayPrototypeGetterCalendarId       DontEnum|ReadOnly|CustomAccessor
  day              temporalPlainMonthDayPrototypeGetterDay              DontEnum|ReadOnly|CustomAccessor
  monthCode        temporalPlainMonthDayPrototypeGetterMonthCode        DontEnum|ReadOnly|CustomAccessor
@end
*/

TemporalPlainMonthDayPrototype* TemporalPlainMonthDayPrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    auto* prototype = new (NotNull, allocateCell<TemporalPlainMonthDayPrototype>(vm)) TemporalPlainMonthDayPrototype(vm, structure);
    prototype->finishCreation(vm, globalObject);
    return prototype;
}

Structure* TemporalPlainMonthDayPrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalPlainMonthDayPrototype::TemporalPlainMonthDayPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void TemporalPlainMonthDayPrototype::finishCreation(VM& vm, JSGlobalObject*)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.prototype.tostring
JSC_DEFINE_HOST_FUNCTION(temporalPlainMonthDayPrototypeFuncToString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* monthDay = jsDynamicCast<TemporalPlainMonthDay*>(callFrame->thisValue());
    if (!monthDay)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainMonthDay.prototype.toString called on value that's not a PlainMonthDay"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(jsString(vm, monthDay->toString(globalObject, callFrame->argument(0)))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.prototype.tojson
JSC_DEFINE_HOST_FUNCTION(temporalPlainMonthDayPrototypeFuncToJSON, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* monthDay = jsDynamicCast<TemporalPlainMonthDay*>(callFrame->thisValue());
    if (!monthDay)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainMonthDay.prototype.toJSON called on value that's not a PlainMonthDay"_s);

    return JSValue::encode(jsString(vm, monthDay->toString()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.prototype.tolocalestring
JSC_DEFINE_HOST_FUNCTION(temporalPlainMonthDayPrototypeFuncToLocaleString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* monthDay = jsDynamicCast<TemporalPlainMonthDay*>(callFrame->thisValue());
    if (!monthDay)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainMonthDay.prototype.toLocaleString called on value that's not a PlainMonthDay"_s);

    return JSValue::encode(jsString(vm, monthDay->toString()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.prototype.with
JSC_DEFINE_HOST_FUNCTION(temporalPlainMonthDayPrototypeFuncWith, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* monthDay = jsDynamicCast<TemporalPlainMonthDay*>(callFrame->thisValue());
    if (!monthDay) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainMonthDay.prototype.with called on value that's not a PlainMonthDay"_s);

    JSValue temporalMonthDayLike  = callFrame->argument(0);
    if (!temporalMonthDayLike.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "First argument to Temporal.PlainMonthDay.prototype.with must be an object"_s);

    auto result = monthDay->with(globalObject, asObject(temporalMonthDayLike), callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(
        TemporalPlainMonthDay::tryCreateIfValid(
            globalObject, globalObject->plainMonthDayStructure(), WTFMove(result))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.prototype.equals
JSC_DEFINE_HOST_FUNCTION(temporalPlainMonthDayPrototypeFuncEquals, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* monthDay = jsDynamicCast<TemporalPlainMonthDay*>(callFrame->thisValue());
    if (!monthDay) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainMonthDay.prototype.equals called on value that's not a PlainMonthDay"_s);

    auto* other = TemporalPlainMonthDay::from(globalObject, callFrame->argument(0), std::nullopt);
    RETURN_IF_EXCEPTION(scope, { });

    if (monthDay->plainMonthDay() != other->plainMonthDay())
        return JSValue::encode(jsBoolean(false));

    return JSValue::encode(jsBoolean(true));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.prototype.toplaindate
JSC_DEFINE_HOST_FUNCTION(temporalPlainMonthDayPrototypeFuncToPlainDate, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* monthDay = jsDynamicCast<TemporalPlainMonthDay*>(callFrame->thisValue());
    if (!monthDay) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainMonthDay.prototype.toPlainDate called on value that's not a PlainMonthDay"_s);

    JSValue itemValue = callFrame->argument(0);
    if (!itemValue.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainMonthDay.prototype.toPlainDate: item is not an object"_s);

    auto thisMonth = monthDay->month();
    auto thisDay = monthDay->day();
    auto itemYear = TemporalPlainDate::toYear(globalObject, asObject(itemValue));
    RETURN_IF_EXCEPTION(scope, { });

    if (!itemYear) [[unlikely]] {
        throwTypeError(globalObject, scope, "Temporal.PlainMonthDay.prototype.toPlainDate: item does not have a year field"_s);
        return { };
    }

    auto plainDateOptional =
        TemporalDuration::regulateISODate(itemYear.value(), thisMonth, thisDay, TemporalOverflow::Constrain);
    if (!plainDateOptional) [[unlikely]] {
        throwRangeError(globalObject, scope, "Temporal.PlainMonthDay.prototype.toPlainDate: date is invalid"_s);
        return { };
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainDate::tryCreateIfValid(globalObject, globalObject->plainDateStructure(), WTFMove(plainDateOptional.value()))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.prototype.valueof
JSC_DEFINE_HOST_FUNCTION(temporalPlainMonthDayPrototypeFuncValueOf, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    return throwVMTypeError(globalObject, scope, "Temporal.PlainMonthDay.prototype.valueOf must not be called. To compare PlainMonthDay values, use Temporal.PlainDate.compare on the corresponding PlainDate objects."_s);
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plainmonthday.prototype.calendarid
JSC_DEFINE_CUSTOM_GETTER(temporalPlainMonthDayPrototypeGetterCalendarId, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* monthDay = jsDynamicCast<TemporalPlainMonthDay*>(JSValue::decode(thisValue));
    if (!monthDay)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainMonthDay.prototype.calendar called on value that's not a PlainMonthDay"_s);

    // FIXME: when calendars are supported, get the string ID of the calendar
    return JSValue::encode(jsString(vm, String::fromLatin1("iso8601")));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plainmonthday.prototype.day
JSC_DEFINE_CUSTOM_GETTER(temporalPlainMonthDayPrototypeGetterDay, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* monthDay = jsDynamicCast<TemporalPlainMonthDay*>(JSValue::decode(thisValue));
    if (!monthDay)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainMonthDay.prototype.day called on value that's not a PlainMonthDay"_s);

    return JSValue::encode(jsNumber(monthDay->day()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plainmonthday.prototype.monthcode
JSC_DEFINE_CUSTOM_GETTER(temporalPlainMonthDayPrototypeGetterMonthCode, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* monthDay = jsDynamicCast<TemporalPlainMonthDay*>(JSValue::decode(thisValue));
    if (!monthDay)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainMonthDay.prototype.monthCode called on value that's not a PlainMonthDay"_s);

    return JSValue::encode(jsNontrivialString(vm, monthDay->monthCode()));
}

} // namespace JSC
