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

#include "CalendarFields.h"
#include "CalendarICUBridge.h"
#include "ISOArithmetic.h"
#include "IntlDateTimeFormat.h"
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
static JSC_DECLARE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncUntil);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncSince);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncToPlainDate);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncToString);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncToJSON);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncToLocaleString);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncWith);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncEquals);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncValueOf);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterCalendarId);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterMonth);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterMonthCode);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterDaysInMonth);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterDaysInYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterMonthsInYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterInLeapYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterEra);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterEraYear);

}

#include "TemporalPlainYearMonthPrototype.lut.h"
namespace JSC {

const ClassInfo TemporalPlainYearMonthPrototype::s_info = { "Temporal.PlainYearMonth"_s, &Base::s_info, &plainYearMonthPrototypeTable, nullptr, CREATE_METHOD_TABLE(TemporalPlainYearMonthPrototype) };

/* Source for TemporalPlainYearMonthPrototype.lut.h
@begin plainYearMonthPrototypeTable
  add              temporalPlainYearMonthPrototypeFuncAdd                DontEnum|Function 1
  subtract         temporalPlainYearMonthPrototypeFuncSubtract           DontEnum|Function 1
  until            temporalPlainYearMonthPrototypeFuncUntil              DontEnum|Function 1
  since            temporalPlainYearMonthPrototypeFuncSince              DontEnum|Function 1
  toPlainDate      temporalPlainYearMonthPrototypeFuncToPlainDate        DontEnum|Function 1
  toString         temporalPlainYearMonthPrototypeFuncToString           DontEnum|Function 0
  toJSON           temporalPlainYearMonthPrototypeFuncToJSON             DontEnum|Function 0
  toLocaleString   temporalPlainYearMonthPrototypeFuncToLocaleString     DontEnum|Function 0
  with             temporalPlainYearMonthPrototypeFuncWith               DontEnum|Function 1
  equals           temporalPlainYearMonthPrototypeFuncEquals             DontEnum|Function 1
  valueOf          temporalPlainYearMonthPrototypeFuncValueOf            DontEnum|Function 0
  calendarId       temporalPlainYearMonthPrototypeGetterCalendarId       DontEnum|ReadOnly|CustomAccessor
  year             temporalPlainYearMonthPrototypeGetterYear             DontEnum|ReadOnly|CustomAccessor
  month            temporalPlainYearMonthPrototypeGetterMonth            DontEnum|ReadOnly|CustomAccessor
  monthCode        temporalPlainYearMonthPrototypeGetterMonthCode        DontEnum|ReadOnly|CustomAccessor
  daysInMonth      temporalPlainYearMonthPrototypeGetterDaysInMonth      DontEnum|ReadOnly|CustomAccessor
  daysInYear       temporalPlainYearMonthPrototypeGetterDaysInYear       DontEnum|ReadOnly|CustomAccessor
  monthsInYear     temporalPlainYearMonthPrototypeGetterMonthsInYear     DontEnum|ReadOnly|CustomAccessor
  inLeapYear       temporalPlainYearMonthPrototypeGetterInLeapYear       DontEnum|ReadOnly|CustomAccessor
  era              temporalPlainYearMonthPrototypeGetterEra              DontEnum|ReadOnly|CustomAccessor
  eraYear          temporalPlainYearMonthPrototypeGetterEraYear          DontEnum|ReadOnly|CustomAccessor
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

template<AddOrSubtract op>
static JSC::EncodedJSValue temporalPlainYearMonthPrototypeAddOrSubtract(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(callFrame->thisValue());
    if (!yearMonth) [[unlikely]] {
        if constexpr (op == AddOrSubtract::Add)
            return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.add called on value that's not a PlainYearMonth"_s);
        else
            return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.subtract called on value that's not a PlainYearMonth"_s);
    }

    auto duration = TemporalDuration::toTemporalDurationRecord(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    TemporalOverflow overflow = toTemporalOverflow(globalObject, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    if (duration.weeks() || duration.days() || duration.hours() || duration.minutes() || duration.seconds() || duration.milliseconds() || duration.microseconds() || duration.nanoseconds()) [[unlikely]] {
        throwRangeError(globalObject, scope, "Duration must not have units below months for PlainYearMonth arithmetic"_s);
        return { };
    }

    // Step 2: negate for subtract.
    if constexpr (op == AddOrSubtract::Subtract)
        duration = -duration;

    // Steps 8-15: CalendarDateFromFields, CalendarDateAdd, CalendarYearMonthFromFields.
    auto result = TemporalCore::plainYearMonthAdd(yearMonth->calendarID(), yearMonth->plainYearMonth().isoPlainDate(), duration, overflow);
    if (!result) [[unlikely]] {
        if (result.error().kind == TemporalErrorKind::TypeError)
            throwTypeError(globalObject, scope, String(result.error().message));
        else
            throwRangeError(globalObject, scope, String(result.error().message));
        return { };
    }

    auto* ymResult = TemporalPlainYearMonth::create(vm, globalObject->plainYearMonthStructure(), ISO8601::PlainYearMonth(WTF::move(result->isoDate)));
    if (ymResult && yearMonth->calendarID() != iso8601CalendarID())
        ymResult->setCalendarID(yearMonth->calendarID());
    return JSValue::encode(ymResult);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.add
JSC_DEFINE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncAdd, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return temporalPlainYearMonthPrototypeAddOrSubtract<AddOrSubtract::Add>(globalObject, callFrame);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.subtract
JSC_DEFINE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncSubtract, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return temporalPlainYearMonthPrototypeAddOrSubtract<AddOrSubtract::Subtract>(globalObject, callFrame);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.with
JSC_DEFINE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncWith, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(callFrame->thisValue());
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.with called on value that's not a PlainYearMonth"_s);

    JSValue temporalYearMonthLike  = callFrame->argument(0);
    if (!temporalYearMonthLike.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "First argument to Temporal.PlainYearMonth.prototype.with must be an object"_s);

    auto result = yearMonth->with(globalObject, asObject(temporalYearMonthLike), callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    auto* withResult = TemporalPlainYearMonth::tryCreateIfValid(globalObject, globalObject->plainYearMonthStructure(), WTF::move(result));
    RETURN_IF_EXCEPTION(scope, { });
    if (withResult && yearMonth->calendarID() != iso8601CalendarID())
        withResult->setCalendarID(yearMonth->calendarID());
    return JSValue::encode(withResult);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.until
JSC_DEFINE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncUntil, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(callFrame->thisValue());
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.until called on value that's not a PlainYearMonth"_s);

    auto* other = TemporalPlainYearMonth::from(globalObject, callFrame->argument(0), jsUndefined());
    RETURN_IF_EXCEPTION(scope, { });

    auto result = yearMonth->until(globalObject, other, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalDuration::tryCreateIfValid(globalObject, WTF::move(result), globalObject->durationStructure())));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.since
JSC_DEFINE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncSince, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(callFrame->thisValue());
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.since called on value that's not a PlainYearMonth"_s);

    auto* other = TemporalPlainYearMonth::from(globalObject, callFrame->argument(0), jsUndefined());
    RETURN_IF_EXCEPTION(scope, { });

    auto result = yearMonth->since(globalObject, other, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalDuration::tryCreateIfValid(globalObject, WTF::move(result), globalObject->durationStructure())));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.equals
JSC_DEFINE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncEquals, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(callFrame->thisValue());
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.equals called on value that's not a PlainYearMonth"_s);

    auto* other = TemporalPlainYearMonth::from(globalObject, callFrame->argument(0), jsUndefined());
    RETURN_IF_EXCEPTION(scope, { });

    if (yearMonth->plainYearMonth() != other->plainYearMonth())
        return JSValue::encode(jsBoolean(false));

    return JSValue::encode(jsBoolean(yearMonth->calendarID() == other->calendarID()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.toplaindate
JSC_DEFINE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncToPlainDate, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(callFrame->thisValue());
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.toPlainDate called on value that's not a PlainYearMonth"_s);

    JSValue itemValue = callFrame->argument(0);
    if (!itemValue.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.toPlainDate: item is not an object"_s);

    auto itemDay = TemporalPlainDate::toDay(globalObject, asObject(itemValue));
    RETURN_IF_EXCEPTION(scope, { });

    if (!itemDay) [[unlikely]] {
        throwTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.toPlainDate: item does not have a day field"_s);
        return { };
    }

    if (yearMonth->calendarID() != iso8601CalendarID()) {
        auto resolved = TemporalCore::plainYearMonthToPlainDate(yearMonth->calendarID(), yearMonth->plainYearMonth().isoPlainDate(), static_cast<uint8_t>(itemDay.value()));
        if (!resolved) [[unlikely]] {
            if (resolved.error().kind == TemporalErrorKind::TypeError)
                throwTypeError(globalObject, scope, String(resolved.error().message));
            else
                throwRangeError(globalObject, scope, String(resolved.error().message));
            return { };
        }
        auto calIdCopy = resolved->calendarId;
        RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainDate::tryCreateIfValid(globalObject, globalObject->plainDateStructure(), WTF::move(resolved->isoDate), WTF::move(calIdCopy))));
    }

    auto thisYear = yearMonth->year();
    auto thisMonth = yearMonth->month();
    auto plainDateResult = TemporalCore::regulateISODate(thisYear, thisMonth, itemDay.value(), TemporalOverflow::Constrain);
    if (!plainDateResult) [[unlikely]] {
        throwRangeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.toPlainDate: date is invalid"_s);
        return { };
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainDate::tryCreateIfValid(globalObject, globalObject->plainDateStructure(), WTF::move(*plainDateResult), yearMonth->calendarID())));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.tostring
JSC_DEFINE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncToString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(callFrame->thisValue());
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.toString called on value that's not a PlainYearMonth"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(jsString(vm, yearMonth->toString(globalObject, callFrame->argument(0)))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.tojson
JSC_DEFINE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncToJSON, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(callFrame->thisValue());
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.toJSON called on value that's not a PlainYearMonth"_s);

    return JSValue::encode(jsString(vm, yearMonth->toString()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.tolocalestring
JSC_DEFINE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncToLocaleString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(callFrame->thisValue());
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.toLocaleString called on value that's not a PlainYearMonth"_s);

    JSValue locales = callFrame->argument(0);
    JSValue options = callFrame->argument(1);
    IntlDateTimeFormat* formatter;
    if (locales.isUndefined() && options.isUndefined())
        formatter = globalObject->defaultDateFormat();
    else {
        formatter = IntlDateTimeFormat::create(vm, globalObject->dateTimeFormatStructure());
        formatter->initializeDateTimeFormat(globalObject, locales, options, IntlDateTimeFormat::RequiredComponent::Date, IntlDateTimeFormat::Defaults::Date);
    }
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(formatter->format(globalObject, callFrame->thisValue())));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.valueof
JSC_DEFINE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncValueOf, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.valueOf must not be called. To compare PlainYearMonth values, use Temporal.PlainYearMonth.compare"_s);
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plainyearmonth.prototype.calendarid
JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterCalendarId, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(JSValue::decode(thisValue));
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.calendar called on value that's not a PlainYearMonth"_s);

    return JSValue::encode(jsString(vm, yearMonth->calendarIDAsString()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plainyearmonth.prototype.year
JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(JSValue::decode(thisValue));
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.year called on value that's not a PlainYearMonth"_s);

    if (!TemporalCore::calendarIsISO(yearMonth->calendarID())) {
        auto result = TemporalCore::calendarYear(yearMonth->calendarID(), yearMonth->plainYearMonth().isoPlainDate());
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(yearMonth->year()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plainyearmonth.prototype.month
JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterMonth, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(JSValue::decode(thisValue));
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.month called on value that's not a PlainYearMonth"_s);

    if (!TemporalCore::calendarIsISO(yearMonth->calendarID())) {
        auto result = TemporalCore::calendarMonth(yearMonth->calendarID(), yearMonth->plainYearMonth().isoPlainDate());
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(yearMonth->month()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plainyearmonth.prototype.monthcode
JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterMonthCode, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(JSValue::decode(thisValue));
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.monthCode called on value that's not a PlainYearMonth"_s);

    if (!TemporalCore::calendarIsISO(yearMonth->calendarID())) {
        auto result = TemporalCore::calendarMonthCode(yearMonth->calendarID(), yearMonth->plainYearMonth().isoPlainDate());
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNontrivialString(vm, *result));
    }
    return JSValue::encode(jsNontrivialString(vm, yearMonth->monthCode()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plainyearmonth.prototype.daysinmonth
JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterDaysInMonth, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(JSValue::decode(thisValue));
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.daysInMonth called on value that's not a PlainYearMonth"_s);

    if (!TemporalCore::calendarIsISO(yearMonth->calendarID())) {
        auto result = TemporalCore::calendarDaysInMonth(yearMonth->calendarID(), yearMonth->plainYearMonth().isoPlainDate());
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(ISO8601::daysInMonth(yearMonth->year(), yearMonth->month())));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plainyearmonth.prototype.daysinyear
JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterDaysInYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(JSValue::decode(thisValue));
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.daysInYear called on value that's not a PlainYearMonth"_s);

    if (!TemporalCore::calendarIsISO(yearMonth->calendarID())) {
        auto result = TemporalCore::calendarDaysInYear(yearMonth->calendarID(), yearMonth->plainYearMonth().isoPlainDate());
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(isLeapYear(yearMonth->year()) ? 366 : 365));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plainyearmonth.prototype.monthsinyear
JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterMonthsInYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(JSValue::decode(thisValue));
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.monthsInYear called on value that's not a PlainYearMonth"_s);

    if (!TemporalCore::calendarIsISO(yearMonth->calendarID())) {
        auto result = TemporalCore::calendarMonthsInYear(yearMonth->calendarID(), yearMonth->plainYearMonth().isoPlainDate());
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(12)); // ISO8601 calendar always returns 12.
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plainyearmonth.prototype.inleapyear
JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterInLeapYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(JSValue::decode(thisValue));
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.inLeapYear called on value that's not a PlainYearMonth"_s);

    if (!TemporalCore::calendarIsISO(yearMonth->calendarID())) {
        auto result = TemporalCore::calendarInLeapYear(yearMonth->calendarID(), yearMonth->plainYearMonth().isoPlainDate());
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsBoolean(*result));
    }
    return JSValue::encode(jsBoolean(isLeapYear(yearMonth->year())));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plainyearmonth.prototype.era
JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterEra, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(JSValue::decode(thisValue));
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.era called on value that's not a PlainYearMonth"_s);

    // Step 3: Return CalendarISOToDate(calendar, isoDate).[[Era]].
    auto result = TemporalCore::calendarEra(yearMonth->calendarID(), yearMonth->plainYearMonth().isoPlainDate());
    if (!result || !*result)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsString(vm, **result));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plainyearmonth.prototype.erayear
JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterEraYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(JSValue::decode(thisValue));
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.eraYear called on value that's not a PlainYearMonth"_s);

    // Steps 3-5: Return CalendarISOToDate(calendar, isoDate).[[EraYear]], or undefined.
    auto result = TemporalCore::calendarEraYear(yearMonth->calendarID(), yearMonth->plainYearMonth().isoPlainDate());
    if (!result || !*result)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsNumber(**result));
}

} // namespace JSC
