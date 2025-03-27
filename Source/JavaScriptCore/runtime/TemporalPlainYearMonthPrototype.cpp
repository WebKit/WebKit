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
#include "TemporalPlainYearMonthPrototype.h"

#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include "TemporalDuration.h"
#include "TemporalPlainDate.h"
#include "TemporalPlainDateTime.h"
#include "TemporalPlainTime.h"
#include "TemporalPlainYearMonth.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncAdd);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncSubtract);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncWith);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncUntil);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncSince);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncEquals);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncToPlainDate);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncToString);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncToJSON);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncToLocaleString);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncValueOf);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterCalendarId);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterMonth);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterMonthCode);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterDaysInMonth);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterDaysInYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterMonthsInYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterInLeapYear);

}

#include "TemporalPlainYearMonthPrototype.lut.h"

namespace JSC {

const ClassInfo TemporalPlainYearMonthPrototype::s_info = { "Temporal.PlainYearMonth"_s, &Base::s_info, &plainYearMonthPrototypeTable, nullptr, CREATE_METHOD_TABLE(TemporalPlainYearMonthPrototype) };

/* Source for TemporalPlainYearMonthPrototype.lut.h
@begin plainYearMonthPrototypeTable
  add              temporalPlainYearMonthPrototypeFuncAdd                DontEnum|Function 1
  subtract         temporalPlainYearMonthPrototypeFuncSubtract           DontEnum|Function 1
  with             temporalPlainYearMonthPrototypeFuncWith               DontEnum|Function 1
  until            temporalPlainYearMonthPrototypeFuncUntil              DontEnum|Function 1
  since            temporalPlainYearMonthPrototypeFuncSince              DontEnum|Function 1
  equals           temporalPlainYearMonthPrototypeFuncEquals             DontEnum|Function 1
  toPlainDate      temporalPlainYearMonthPrototypeFuncToPlainDate        DontEnum|Function 1
  toString         temporalPlainYearMonthPrototypeFuncToString           DontEnum|Function 0
  toJSON           temporalPlainYearMonthPrototypeFuncToJSON             DontEnum|Function 0
  toLocaleString   temporalPlainYearMonthPrototypeFuncToLocaleString     DontEnum|Function 0
  valueOf          temporalPlainYearMonthPrototypeFuncValueOf            DontEnum|Function 0
  calendarId       temporalPlainYearMonthPrototypeGetterCalendarId       DontEnum|ReadOnly|CustomAccessor
  year             temporalPlainYearMonthPrototypeGetterYear             DontEnum|ReadOnly|CustomAccessor
  month            temporalPlainYearMonthPrototypeGetterMonth            DontEnum|ReadOnly|CustomAccessor
  monthCode        temporalPlainYearMonthPrototypeGetterMonthCode        DontEnum|ReadOnly|CustomAccessor
  daysInMonth      temporalPlainYearMonthPrototypeGetterDaysInMonth      DontEnum|ReadOnly|CustomAccessor
  daysInYear       temporalPlainYearMonthPrototypeGetterDaysInYear       DontEnum|ReadOnly|CustomAccessor
  monthsInYear     temporalPlainYearMonthPrototypeGetterMonthsInYear     DontEnum|ReadOnly|CustomAccessor
  inLeapYear       temporalPlainYearMonthPrototypeGetterInLeapYear       DontEnum|ReadOnly|CustomAccessor
@end
*/

TemporalPlainYearMonthPrototype* TemporalPlainYearMonthPrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    auto* prototype = new (NotNull, allocateCell<TemporalPlainYearMonthPrototype>(vm)) TemporalPlainYearMonthPrototype(vm, structure);
    prototype->finishCreation(vm, globalObject);
    return prototype;
}

Structure* TemporalPlainYearMonthPrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalPlainYearMonthPrototype::TemporalPlainYearMonthPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void TemporalPlainYearMonthPrototype::finishCreation(VM& vm, JSGlobalObject*)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

static JSC::EncodedJSValue
temporalPlainYearMonthPrototypeAddOrSubtract(JSGlobalObject* globalObject, CallFrame* callFrame, bool isAdd)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = jsDynamicCast<TemporalPlainYearMonth*>(callFrame->thisValue());
    if (!yearMonth)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.add called on value that's not a PlainYearMonth"_s);

    auto duration = TemporalDuration::toISO8601Duration(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    TemporalOverflow overflow = toTemporalOverflow(globalObject, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    ISO8601::PlainYearMonth result =
        TemporalPlainYearMonth::addDurationToYearMonth(
            globalObject, isAdd, yearMonth->plainYearMonth(), duration, overflow);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope,
        JSValue::encode(
            TemporalPlainYearMonth::create(
                vm, globalObject->plainYearMonthStructure(), WTFMove(result))));

}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.add
JSC_DEFINE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncAdd, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return temporalPlainYearMonthPrototypeAddOrSubtract(globalObject, callFrame, true);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.subtract
JSC_DEFINE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncSubtract, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return temporalPlainYearMonthPrototypeAddOrSubtract(globalObject, callFrame, false);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.with
JSC_DEFINE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncWith, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = jsDynamicCast<TemporalPlainYearMonth*>(callFrame->thisValue());
    if (!yearMonth)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.with called on value that's not a PlainYearMonth"_s);

    JSValue temporalYearMonthLike  = callFrame->argument(0);
    if (!temporalYearMonthLike.isObject())
        return throwVMTypeError(globalObject, scope, "First argument to Temporal.PlainYearMonth.prototype.with must be an object"_s);

    auto result = yearMonth->with(globalObject, asObject(temporalYearMonthLike), callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    return JSValue::encode(
        TemporalPlainYearMonth::tryCreateIfValid(
            globalObject, globalObject->plainYearMonthStructure(), WTFMove(result)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.until
JSC_DEFINE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncUntil, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = jsDynamicCast<TemporalPlainYearMonth*>(callFrame->thisValue());
    if (!yearMonth)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.until called on value that's not a PlainYearMonth"_s);

    auto* other = TemporalPlainYearMonth::from(globalObject, callFrame->argument(0), std::nullopt);
    RETURN_IF_EXCEPTION(scope, { });

    auto result = yearMonth->until(globalObject, other, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalDuration::tryCreateIfValid(globalObject, WTFMove(result), globalObject->durationStructure())));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.since
JSC_DEFINE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncSince, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = jsDynamicCast<TemporalPlainYearMonth*>(callFrame->thisValue());
    if (!yearMonth)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.since called on value that's not a PlainYearMonth"_s);

    auto* other = TemporalPlainYearMonth::from(globalObject, callFrame->argument(0), std::nullopt);
    RETURN_IF_EXCEPTION(scope, { });

    auto result = yearMonth->since(globalObject, other, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalDuration::tryCreateIfValid(globalObject, WTFMove(result), globalObject->durationStructure())));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.equals
JSC_DEFINE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncEquals, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = jsDynamicCast<TemporalPlainYearMonth*>(callFrame->thisValue());
    if (!yearMonth)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.equals called on value that's not a PlainYearMonth"_s);

    auto* other = TemporalPlainYearMonth::from(globalObject, callFrame->argument(0), std::nullopt);
    RETURN_IF_EXCEPTION(scope, { });

    if (yearMonth->plainYearMonth() != other->plainYearMonth())
        return JSValue::encode(jsBoolean(false));

    RELEASE_AND_RETURN(scope, JSValue::encode(jsBoolean(true)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.toplaindatetime
JSC_DEFINE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncToPlainDate, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = jsDynamicCast<TemporalPlainYearMonth*>(callFrame->thisValue());
    if (!yearMonth)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.toPlainDate called on value that's not a PlainYearMonth"_s);

    JSValue itemValue = callFrame->argument(0);
    if (!itemValue.isObject())
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.toPlainDate: item is not an object"_s);

    auto thisYear = yearMonth->year();
    auto thisMonth = yearMonth->month();
    auto [itemYear, itemMonth, itemDay] = TemporalPlainDate::toPartialDate(globalObject, asObject(itemValue));
    RETURN_IF_EXCEPTION(scope, { });

    if (!itemDay) {
        throwTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.toPlainDate: item does not have a day field"_s);
        return { };
    }

    auto plainDateOptional =
        TemporalPlainDate::regulateISODate(thisYear, thisMonth, itemDay.value(), TemporalOverflow::Constrain);
    if (!plainDateOptional) {
        throwRangeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.toPlainDate: date is invalid"_s);
        return { };
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainDate::tryCreateIfValid(globalObject, globalObject->plainDateStructure(), WTFMove(plainDateOptional.value()))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.tostring
JSC_DEFINE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncToString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = jsDynamicCast<TemporalPlainYearMonth*>(callFrame->thisValue());
    if (!yearMonth)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.toString called on value that's not a PlainYearMonth"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(jsString(vm, yearMonth->toString(globalObject, callFrame->argument(0)))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.tojson
JSC_DEFINE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncToJSON, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = jsDynamicCast<TemporalPlainYearMonth*>(callFrame->thisValue());
    if (!yearMonth)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.toJSON called on value that's not a PlainYearMonth"_s);

    return JSValue::encode(jsString(vm, yearMonth->toString()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.tolocalestring
JSC_DEFINE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncToLocaleString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = jsDynamicCast<TemporalPlainYearMonth*>(callFrame->thisValue());
    if (!yearMonth)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.toLocaleString called on value that's not a PlainYearMonth"_s);

    return JSValue::encode(jsString(vm, yearMonth->toString()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.valueof
JSC_DEFINE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncValueOf, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.valueOf must not be called. To compare PlainYearMonth values, use Temporal.PlainYearMonth.compare"_s);
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterCalendarId, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = jsDynamicCast<TemporalPlainYearMonth*>(JSValue::decode(thisValue));
    if (!yearMonth)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.calendar called on value that's not a PlainYearMonth"_s);

    // TODO: when calendars are supported, get the string ID of the calendar
    return JSValue::encode(jsString(vm, String::fromLatin1("iso8601")));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = jsDynamicCast<TemporalPlainYearMonth*>(JSValue::decode(thisValue));
    if (!yearMonth)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.year called on value that's not a PlainYearMonth"_s);

    return JSValue::encode(jsNumber(yearMonth->year()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterMonth, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = jsDynamicCast<TemporalPlainYearMonth*>(JSValue::decode(thisValue));
    if (!yearMonth)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.month called on value that's not a PlainYearMonth"_s);

    return JSValue::encode(jsNumber(yearMonth->month()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterMonthCode, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = jsDynamicCast<TemporalPlainYearMonth*>(JSValue::decode(thisValue));
    if (!yearMonth)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.monthCode called on value that's not a PlainYearMonth"_s);

    return JSValue::encode(jsNontrivialString(vm, yearMonth->monthCode()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterDaysInMonth, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = jsDynamicCast<TemporalPlainYearMonth*>(JSValue::decode(thisValue));
    if (!yearMonth)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.daysInMonth called on value that's not a PlainYearMonth"_s);

    return JSValue::encode(jsNumber(ISO8601::daysInMonth(yearMonth->year(), yearMonth->month())));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterDaysInYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = jsDynamicCast<TemporalPlainYearMonth*>(JSValue::decode(thisValue));
    if (!yearMonth)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.daysInYear called on value that's not a PlainYearMonth"_s);

    return JSValue::encode(jsNumber(isLeapYear(yearMonth->year()) ? 366 : 365));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterMonthsInYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = jsDynamicCast<TemporalPlainYearMonth*>(JSValue::decode(thisValue));
    if (!yearMonth)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.monthsInYear called on value that's not a PlainYearMonth"_s);

    return JSValue::encode(jsNumber(12)); // ISO8601 calendar always returns 12.
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterInLeapYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = jsDynamicCast<TemporalPlainYearMonth*>(JSValue::decode(thisValue));
    if (!yearMonth)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.inLeapYear called on value that's not a PlainYearMonth"_s);

    return JSValue::encode(jsBoolean(isLeapYear(yearMonth->year())));
}

} // namespace JSC
