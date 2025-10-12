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

static JSC_DECLARE_HOST_FUNCTION(temporalPlainMonthDayPrototypeFuncToString);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainMonthDayPrototypeFuncToJSON);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainMonthDayPrototypeFuncToLocaleString);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainMonthDayPrototypeGetterCalendarId);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainMonthDayPrototypeGetterDay);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainMonthDayPrototypeGetterMonthCode);

}

#include "TemporalPlainMonthDayPrototype.lut.h"

namespace JSC {

const ClassInfo TemporalPlainMonthDayPrototype::s_info = { "Temporal.PlainMonthDay"_s, &Base::s_info, &plainMonthDayPrototypeTable, nullptr, CREATE_METHOD_TABLE(TemporalPlainMonthDayPrototype) };

/* Source for TemporalPlainMonthDayPrototype.lut.h
@begin plainMonthDayPrototypeTable
  toString         temporalPlainMonthDayPrototypeFuncToString           DontEnum|Function 0
  toJSON           temporalPlainMonthDayPrototypeFuncToJSON             DontEnum|Function 0
  toLocaleString   temporalPlainMonthDayPrototypeFuncToLocaleString     DontEnum|Function 0
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

// https://tc39.es/proposal-temporal/#sec-get-temporal.plainmonthday.prototype.calendarid
JSC_DEFINE_CUSTOM_GETTER(temporalPlainMonthDayPrototypeGetterCalendarId, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* monthDay = jsDynamicCast<TemporalPlainMonthDay*>(JSValue::decode(thisValue));
    if (!monthDay)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainMonthDay.prototype.calendar called on value that's not a PlainMonthDay"_s);

    // TODO: when calendars are supported, get the string ID of the calendar
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
