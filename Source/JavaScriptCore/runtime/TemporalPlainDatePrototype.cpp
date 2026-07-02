/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "CalendarFields.h"
#include "CalendarICUBridge.h"
#include "IntlDateTimeFormat.h"
#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include "TemporalCalendar.h"
#include "TemporalDuration.h"
#include "TemporalObject.h"
#include "TemporalPlainDate.h"
#include "TemporalPlainDateTime.h"
#include "TemporalPlainMonthDay.h"
#include "TemporalPlainTime.h"
#include "TemporalPlainYearMonth.h"
#include "TemporalZonedDateTime.h"
#include "TimeZoneICUBridge.h"
#include "ZonedDateTimeCore.h"

#include <wtf/text/MakeString.h>
namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(temporalPlainDatePrototypeFuncWithCalendar);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDatePrototypeFuncToPlainMonthDay);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDatePrototypeFuncToPlainYearMonth);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDatePrototypeFuncAdd);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDatePrototypeFuncSubtract);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDatePrototypeFuncWith);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDatePrototypeFuncUntil);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDatePrototypeFuncSince);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDatePrototypeFuncEquals);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDatePrototypeFuncToPlainDateTime);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDatePrototypeFuncToZonedDateTime);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDatePrototypeFuncToString);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDatePrototypeFuncToJSON);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDatePrototypeFuncToLocaleString);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDatePrototypeFuncValueOf);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterCalendarId);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterMonth);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterMonthCode);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDay);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDayOfWeek);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDayOfYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterWeekOfYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterYearOfWeek);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDaysInWeek);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDaysInMonth);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDaysInYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterMonthsInYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterInLeapYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterEra);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterEraYear);

}

#include "TemporalPlainDatePrototype.lut.h"
namespace JSC {

const ClassInfo TemporalPlainDatePrototype::s_info = { "Temporal.PlainDate"_s, &Base::s_info, &plainDatePrototypeTable, nullptr, CREATE_METHOD_TABLE(TemporalPlainDatePrototype) };

/* Source for TemporalPlainDatePrototype.lut.h
@begin plainDatePrototypeTable
  toPlainMonthDay  temporalPlainDatePrototypeFuncToPlainMonthDay    DontEnum|Function 0
  toPlainYearMonth temporalPlainDatePrototypeFuncToPlainYearMonth   DontEnum|Function 0
  withCalendar     temporalPlainDatePrototypeFuncWithCalendar       DontEnum|Function 1
  add              temporalPlainDatePrototypeFuncAdd                DontEnum|Function 1
  subtract         temporalPlainDatePrototypeFuncSubtract           DontEnum|Function 1
  with             temporalPlainDatePrototypeFuncWith               DontEnum|Function 1
  until            temporalPlainDatePrototypeFuncUntil              DontEnum|Function 1
  since            temporalPlainDatePrototypeFuncSince              DontEnum|Function 1
  equals           temporalPlainDatePrototypeFuncEquals             DontEnum|Function 1
  toPlainDateTime  temporalPlainDatePrototypeFuncToPlainDateTime    DontEnum|Function 0
  toZonedDateTime  temporalPlainDatePrototypeFuncToZonedDateTime    DontEnum|Function 1
  toString         temporalPlainDatePrototypeFuncToString           DontEnum|Function 0
  toJSON           temporalPlainDatePrototypeFuncToJSON             DontEnum|Function 0
  toLocaleString   temporalPlainDatePrototypeFuncToLocaleString     DontEnum|Function 0
  valueOf          temporalPlainDatePrototypeFuncValueOf            DontEnum|Function 0
  calendarId       temporalPlainDatePrototypeGetterCalendarId       DontEnum|ReadOnly|CustomAccessor
  year             temporalPlainDatePrototypeGetterYear             DontEnum|ReadOnly|CustomAccessor
  month            temporalPlainDatePrototypeGetterMonth            DontEnum|ReadOnly|CustomAccessor
  monthCode        temporalPlainDatePrototypeGetterMonthCode        DontEnum|ReadOnly|CustomAccessor
  day              temporalPlainDatePrototypeGetterDay              DontEnum|ReadOnly|CustomAccessor
  dayOfWeek        temporalPlainDatePrototypeGetterDayOfWeek        DontEnum|ReadOnly|CustomAccessor
  dayOfYear        temporalPlainDatePrototypeGetterDayOfYear        DontEnum|ReadOnly|CustomAccessor
  weekOfYear       temporalPlainDatePrototypeGetterWeekOfYear       DontEnum|ReadOnly|CustomAccessor
  yearOfWeek       temporalPlainDatePrototypeGetterYearOfWeek       DontEnum|ReadOnly|CustomAccessor
  daysInWeek       temporalPlainDatePrototypeGetterDaysInWeek       DontEnum|ReadOnly|CustomAccessor
  daysInMonth      temporalPlainDatePrototypeGetterDaysInMonth      DontEnum|ReadOnly|CustomAccessor
  daysInYear       temporalPlainDatePrototypeGetterDaysInYear       DontEnum|ReadOnly|CustomAccessor
  monthsInYear     temporalPlainDatePrototypeGetterMonthsInYear     DontEnum|ReadOnly|CustomAccessor
  inLeapYear       temporalPlainDatePrototypeGetterInLeapYear       DontEnum|ReadOnly|CustomAccessor
  era              temporalPlainDatePrototypeGetterEra              DontEnum|ReadOnly|CustomAccessor
  eraYear          temporalPlainDatePrototypeGetterEraYear          DontEnum|ReadOnly|CustomAccessor
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

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.toplainmonthday
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncToPlainMonthDay, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: branding.
    auto* temporalDate = dynamicDowncast<TemporalPlainDate>(callFrame->thisValue());
    if (!temporalDate) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.toPlainMonthDay called on value that's not a PlainDate"_s);

    // Steps 3-6: ISODateToFields + ? CalendarMonthDayFromFields + CreateTemporalMonthDay.
    if (!TemporalCore::calendarIsISO(temporalDate->calendarID())) {
        auto resolved = TemporalCore::plainMonthDayFromISODate(temporalDate->calendarID(), temporalDate->plainDate(), TemporalOverflow::Constrain);
        if (!resolved) [[unlikely]] {
            throwRangeError(globalObject, scope, String(resolved.error().message));
            return { };
        }
        auto* result = TemporalPlainMonthDay::tryCreateIfValid(globalObject, globalObject->plainMonthDayStructure(), WTF::move(resolved->isoDate));
        RETURN_IF_EXCEPTION(scope, { });
        result->setCalendarID(resolved->calendarId);
        return JSValue::encode(result);
    }

    // ISO: encode month-day with reference year 1972 (leap year, so Feb 29 fits).
    ISO8601::PlainDate dateToUse(1972, temporalDate->plainDate().month(), temporalDate->plainDate().day());
    auto* mdResult = TemporalPlainMonthDay::tryCreateIfValid(globalObject, globalObject->plainMonthDayStructure(), WTF::move(dateToUse));
    RETURN_IF_EXCEPTION(scope, { });
    return JSValue::encode(mdResult);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.toplainyearmonth
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncToPlainYearMonth, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: branding.
    auto* temporalDate = dynamicDowncast<TemporalPlainDate>(callFrame->thisValue());
    if (!temporalDate) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.toPlainYearMonth called on value that's not a PlainDate"_s);

    // Steps 3-6: ISODateToFields + ? CalendarYearMonthFromFields + CreateTemporalYearMonth.
    if (!TemporalCore::calendarIsISO(temporalDate->calendarID())) {
        auto resolved = TemporalCore::plainYearMonthFromISODate(temporalDate->calendarID(), temporalDate->plainDate());
        if (!resolved) [[unlikely]] {
            throwRangeError(globalObject, scope, String(resolved.error().message));
            return { };
        }
        auto* result = TemporalPlainYearMonth::create(vm, globalObject->plainYearMonthStructure(), ISO8601::PlainYearMonth(WTF::move(resolved->isoDate)));
        result->setCalendarID(resolved->calendarId);
        return JSValue::encode(result);
    }

    // ISO: encode year-month with day=1.
    ISO8601::PlainDate dateToUse(temporalDate->plainDate().year(), temporalDate->plainDate().month(), 1);
    auto* ymResult = TemporalPlainYearMonth::tryCreateIfValid(globalObject, globalObject->plainYearMonthStructure(), WTF::move(dateToUse));
    RETURN_IF_EXCEPTION(scope, { });
    return JSValue::encode(ymResult);
}

// AddDurationToDate ( operation, temporalDate, temporalDurationLike, options )
// https://tc39.es/proposal-temporal/#sec-temporal-adddurationtodate
// Caller handles Steps 1-3 (calendar binding, ToTemporalDuration, negate-if-subtract via `-duration`).
static EncodedJSValue addDurationToPlainDate(JSGlobalObject* globalObject, ThrowScope& scope, TemporalPlainDate* plainDate, const ISO8601::Duration& duration, JSValue optionsArg)
{
    // Step 4: dateDuration = ToDateDurationRecordWithoutTime — rolls 24h → 1 day.
    auto dateDuration = TemporalDuration::toDateDurationRecordWithoutTime(globalObject, duration);
    RETURN_IF_EXCEPTION(scope, { });

    // Steps 5-6: GetOptionsObject + GetTemporalOverflowOption.
    TemporalOverflow overflow = toTemporalOverflow(globalObject, optionsArg);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 7: result = ? CalendarDateAdd(calendar, isoDate, dateDuration, overflow).
    auto result = calendarDateAdd(globalObject, plainDate->calendarID(), plainDate->plainDate(), dateDuration, overflow);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 8: Return ! CreateTemporalDate(result, calendar).
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainDate::tryCreateIfValid(globalObject, globalObject->plainDateStructure(), WTF::move(result), plainDate->calendarID())));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.add
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncAdd, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: branding.
    auto* plainDate = dynamicDowncast<TemporalPlainDate>(callFrame->thisValue());
    if (!plainDate) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.add called on value that's not a PlainDate"_s);

    // Step 3: Return ? AddDurationToDate(~add~, this, temporalDurationLike, options).
    //   Inner Step 2: ? ToTemporalDuration.
    auto duration = TemporalDuration::toTemporalDurationRecord(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });
    return addDurationToPlainDate(globalObject, scope, plainDate, duration, callFrame->argument(1));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.subtract
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncSubtract, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: branding.
    auto* plainDate = dynamicDowncast<TemporalPlainDate>(callFrame->thisValue());
    if (!plainDate) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.subtract called on value that's not a PlainDate"_s);

    // Step 3: Return ? AddDurationToDate(~subtract~, …). `-duration` ≡ CreateNegatedTemporalDuration.
    auto duration = TemporalDuration::toTemporalDurationRecord(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });
    return addDurationToPlainDate(globalObject, scope, plainDate, -duration, callFrame->argument(1));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.with
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncWith, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: branding.
    auto* plainDate = dynamicDowncast<TemporalPlainDate>(callFrame->thisValue());
    if (!plainDate) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.with called on value that's not a PlainDate"_s);

    // Step 3: If ? IsPartialTemporalObject(temporalDateLike) is false, throw TypeError.
    //   isObject() handles the non-Object case; Temporal.* / calendar / timeZone rejection
    //   is enforced inside plainDate->with() via rejectObjectWithCalendarOrTimeZone.
    JSValue temporalDateLike = callFrame->argument(0);
    if (!temporalDateLike.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "First argument to Temporal.PlainDate.prototype.with must be an object"_s);

    // Steps 4-10: PrepareCalendarFields + CalendarMergeFields + CalendarDateFromFields inside plainDate->with().
    auto result = plainDate->with(globalObject, asObject(temporalDateLike), callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    // Step 11: Return ! CreateTemporalDate(isoDate, calendar).
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainDate::tryCreateIfValid(globalObject, globalObject->plainDateStructure(), WTF::move(result), plainDate->calendarID())));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.until
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncUntil, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: branding.
    auto* plainDate = dynamicDowncast<TemporalPlainDate>(callFrame->thisValue());
    if (!plainDate) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.until called on value that's not a PlainDate"_s);

    // Step 3: Return ? DifferenceTemporalPlainDate(~until~, this, other, options).
    //   Inner Step 1 (ToTemporalDate) handled by from().
    auto* other = TemporalPlainDate::from(globalObject, callFrame->argument(0), jsUndefined());
    RETURN_IF_EXCEPTION(scope, { });

    auto result = plainDate->until(globalObject, other, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalDuration::tryCreateIfValid(globalObject, WTF::move(result), globalObject->durationStructure())));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.since
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncSince, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: branding.
    auto* plainDate = dynamicDowncast<TemporalPlainDate>(callFrame->thisValue());
    if (!plainDate) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.since called on value that's not a PlainDate"_s);

    // Step 3: Return ? DifferenceTemporalPlainDate(~since~, this, other, options).
    auto* other = TemporalPlainDate::from(globalObject, callFrame->argument(0), jsUndefined());
    RETURN_IF_EXCEPTION(scope, { });

    auto result = plainDate->since(globalObject, other, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalDuration::tryCreateIfValid(globalObject, WTF::move(result), globalObject->durationStructure())));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.equals
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncEquals, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: branding.
    auto* plainDate = dynamicDowncast<TemporalPlainDate>(callFrame->thisValue());
    if (!plainDate) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.equals called on value that's not a PlainDate"_s);

    // Step 3: Set other to ? ToTemporalDate(other).
    auto* other = TemporalPlainDate::from(globalObject, callFrame->argument(0), jsUndefined());
    RETURN_IF_EXCEPTION(scope, { });

    // Step 4: If CompareISODate ≠ 0, return false. operator!= ≡ CompareISODate ≠ 0.
    if (plainDate->plainDate() != other->plainDate())
        return JSValue::encode(jsBoolean(false));

    // Steps 5-6: If CalendarEquals is false return false; else true.
    return JSValue::encode(jsBoolean(plainDate->calendarID() == other->calendarID()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.tozoneddatetime
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncToZonedDateTime, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: Let plainDate be the this value; RequireInternalSlot.
    auto* plainDate = dynamicDowncast<TemporalPlainDate>(callFrame->thisValue());
    if (!plainDate) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.toZonedDateTime called on value that's not a PlainDate"_s);

    JSValue item = callFrame->argument(0);
    std::optional<TimeZone> tzOpt;
    JSValue temporalTime = jsUndefined(); // spec: ~undefined~ initially

    // Step 3: If item is an Object:
    if (item.isObject()) {
        JSObject* itemObj = asObject(item);

        // Step 3.a: Let timeZoneLike be ? Get(item, "timeZone").
        JSValue timeZoneLike = itemObj->get(globalObject, vm.propertyNames->timeZone);
        RETURN_IF_EXCEPTION(scope, { });

        // Step 3.b: If timeZoneLike is undefined:
        if (timeZoneLike.isUndefined()) {
            // Step 3.b.i: timeZone = ? ToTemporalTimeZoneIdentifier(item).
            tzOpt = toTemporalTimeZoneIdentifier(globalObject, item);
            RETURN_IF_EXCEPTION(scope, { });
            // Step 3.b.ii: temporalTime = undefined. (already undefined)
        } else {
            // Step 3.c.i: timeZone = ? ToTemporalTimeZoneIdentifier(timeZoneLike).
            tzOpt = toTemporalTimeZoneIdentifier(globalObject, timeZoneLike);
            RETURN_IF_EXCEPTION(scope, { });
            // Step 3.c.ii: temporalTime = ? Get(item, "plainTime").
            temporalTime = itemObj->get(globalObject, Identifier::fromString(vm, "plainTime"_s));
            RETURN_IF_EXCEPTION(scope, { });
        }
    } else {
        // Step 4.a: timeZone = ? ToTemporalTimeZoneIdentifier(item).
        tzOpt = toTemporalTimeZoneIdentifier(globalObject, item);
        RETURN_IF_EXCEPTION(scope, { });
        // Step 4.b: temporalTime = undefined. (already undefined)
    }
    ASSERT(tzOpt);
    const TimeZone& tz = *tzOpt;

    ISO8601::ExactTime epochNs;

    // Step 5: If temporalTime is undefined:
    if (temporalTime.isUndefined()) {
        // Step 5.a: epochNs = ? GetStartOfDay(timeZone, plainDate.[[ISODate]]).
        auto sodResult = TemporalCore::getStartOfDay(tz, plainDate->plainDate());
        if (!sodResult) [[unlikely]] {
            throwRangeError(globalObject, scope, sodResult.error().message);
            return { };
        }
        epochNs = *sodResult;
    } else {
        // Step 6.a: Set temporalTime to ? ToTemporalTime(temporalTime).
        // (temporalTime was read via Get at step 3.c.ii; ToTemporalTime converts it now at step 6.)
        auto* pt = TemporalPlainTime::from(globalObject, temporalTime, jsUndefined());
        RETURN_IF_EXCEPTION(scope, { });
        ISO8601::PlainTime plainTime = pt->plainTime();

        // Step 6.b: isoDateTime = CombineISODateAndTimeRecord(plainDate.[[ISODate]], temporalTime.[[Time]]).
        // Step 6.c: If ISODateTimeWithinLimits(isoDateTime) is false, throw RangeError.
        bool withinLimits = ISO8601::isDateTimeWithinLimits(plainDate->year(), plainDate->month(), plainDate->day(),
            plainTime.hour(), plainTime.minute(), plainTime.second(),
            plainTime.millisecond(), plainTime.microsecond(), plainTime.nanosecond());
        if (!withinLimits) [[unlikely]] {
            throwRangeError(globalObject, scope, "date-time combination is outside the valid range"_s);
            return { };
        }
        // Step 6.d: epochNs = ? GetEpochNanosecondsFor(timeZone, isoDateTime, ~compatible~).
        auto exactTimeOpt = TemporalZonedDateTime::getEpochNanosecondsFor(globalObject, tz, plainDate->plainDate(), plainTime, TemporalDisambiguation::Compatible);
        RETURN_IF_EXCEPTION(scope, { });
        epochNs = *exactTimeOpt;
    }

    // Step 7: Return ! CreateTemporalZonedDateTime(epochNs, timeZone, plainDate.[[Calendar]]).
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalZonedDateTime::create(vm, globalObject->zonedDateTimeStructure(), epochNs, tz, plainDate->calendarID())));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.toplaindatetime
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncToPlainDateTime, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: branding.
    auto* plainDate = dynamicDowncast<TemporalPlainDate>(callFrame->thisValue());
    if (!plainDate) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.toPlainDateTime called on value that's not a PlainDate"_s);

    // Steps 3-4: time = (temporalTime undef) ? MidnightTimeRecord : ? ToTemporalTime(temporalTime).[[Time]].
    JSValue itemValue = callFrame->argument(0);
    ISO8601::PlainTime plainTime;
    if (!itemValue.isUndefined()) {
        auto* pt = TemporalPlainTime::from(globalObject, itemValue, jsUndefined());
        RETURN_IF_EXCEPTION(scope, { });
        plainTime = pt->plainTime();
    }

    // Steps 5-6: CombineISODateAndTimeRecord + CreateTemporalDateTime.
    auto* result = TemporalPlainDateTime::tryCreateIfValid(globalObject, globalObject->plainDateTimeStructure(), plainDate->plainDate(), WTF::move(plainTime));
    RETURN_IF_EXCEPTION(scope, { });
    if (result && plainDate->calendarID() != iso8601CalendarID())
        result->setCalendarID(plainDate->calendarID());
    return JSValue::encode(result);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.tostring
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncToString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: branding.
    auto* plainDate = dynamicDowncast<TemporalPlainDate>(callFrame->thisValue());
    if (!plainDate) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.toString called on value that's not a PlainDate"_s);

    // Step 3: resolvedOptions = ? GetOptionsObject(options).
    JSObject* options = intlGetOptionsObject(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    if (!options)
        RELEASE_AND_RETURN(scope, JSValue::encode(jsString(vm, plainDate->toString())));

    // Step 4: showCalendar = ? GetTemporalShowCalendarNameOption(resolvedOptions).
    String calOpt = temporalShowCalendarName(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 5: Return TemporalDateToString(plainDate, showCalendar). Inlined:
    //   never    → bare ISO; always → [u-ca=<id>]; critical → [!u-ca=<id>]; auto → annotate iff non-ISO.
    auto base = ISO8601::temporalDateToString(plainDate->plainDate());
    auto calId = plainDate->calendarIDAsString();
    String result;
    if (calOpt == "never"_s)
        result = base;
    else if (calOpt == "always"_s)
        result = makeString(base, "[u-ca="_s, calId, ']');
    else if (calOpt == "critical"_s)
        result = makeString(base, "[!u-ca="_s, calId, ']');
    else if (plainDate->calendarID() != iso8601CalendarID())
        result = makeString(base, "[u-ca="_s, calId, ']');
    else
        result = base;
    RELEASE_AND_RETURN(scope, JSValue::encode(jsString(vm, result)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.tojson
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncToJSON, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: branding.
    auto* plainDate = dynamicDowncast<TemporalPlainDate>(callFrame->thisValue());
    if (!plainDate) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.toJSON called on value that's not a PlainDate"_s);

    // Step 3: Return TemporalDateToString(this, ~auto~).
    return JSValue::encode(jsString(vm, plainDate->toString()));
}

// https://tc39.es/proposal-temporal/#sup-temporal.plaindate.prototype.tolocalestring
// (The Temporal proposal's i18n supplement; staged for inclusion in ECMA-402.)
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncToLocaleString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: branding.
    auto* plainDate = dynamicDowncast<TemporalPlainDate>(callFrame->thisValue());
    if (!plainDate) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.toLocaleString called on value that's not a PlainDate"_s);

    // Step 3: dateFormat = ? CreateDateTimeFormat(%Intl.DateTimeFormat%, locales, options, ~date~, ~date~).
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

    // Step 4: Return ? FormatDateTime(dateFormat, plainDate).
    RELEASE_AND_RETURN(scope, JSValue::encode(formatter->format(globalObject, callFrame->thisValue())));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.valueof
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncValueOf, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Throw a TypeError exception.
    return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.valueOf must not be called. To compare PlainDate values, use Temporal.PlainDate.compare"_s);
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindate.prototype.calendarid
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterCalendarId, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(JSValue::decode(thisValue));
    if (!plainDate) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.calendarId called on value that's not a PlainDate"_s);

    return JSValue::encode(jsString(vm, plainDate->calendarIDAsString()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindate.prototype.year
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(JSValue::decode(thisValue));
    if (!plainDate) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.year called on value that's not a PlainDate"_s);

    // https://tc39.es/proposal-temporal/#sec-get-temporal.plaindate.prototype.year
    // Step 3: Return ? CalendarYear(dateTime.[[Calendar]], dateTime).
    if (!TemporalCore::calendarIsISO(plainDate->calendarID())) {
        auto result = TemporalCore::calendarYear(plainDate->calendarID(), plainDate->plainDate());
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(plainDate->year()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindate.prototype.month
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterMonth, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(JSValue::decode(thisValue));
    if (!plainDate) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.month called on value that's not a PlainDate"_s);

    if (!TemporalCore::calendarIsISO(plainDate->calendarID())) {
        auto result = TemporalCore::calendarMonth(plainDate->calendarID(), plainDate->plainDate());
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(plainDate->month()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindate.prototype.monthcode
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterMonthCode, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(JSValue::decode(thisValue));
    if (!plainDate) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.monthCode called on value that's not a PlainDate"_s);

    if (!TemporalCore::calendarIsISO(plainDate->calendarID())) {
        auto result = TemporalCore::calendarMonthCode(plainDate->calendarID(), plainDate->plainDate());
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNontrivialString(vm, *result));
    }
    return JSValue::encode(jsNontrivialString(vm, plainDate->monthCode()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindate.prototype.day
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDay, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(JSValue::decode(thisValue));
    if (!plainDate) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.day called on value that's not a PlainDate"_s);

    if (!TemporalCore::calendarIsISO(plainDate->calendarID())) {
        auto result = TemporalCore::calendarDay(plainDate->calendarID(), plainDate->plainDate());
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(plainDate->day()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindate.prototype.dayofweek
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDayOfWeek, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(JSValue::decode(thisValue));
    if (!plainDate) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.dayOfWeek called on value that's not a PlainDate"_s);

    return JSValue::encode(jsNumber(plainDate->dayOfWeek()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindate.prototype.dayofyear
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDayOfYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(JSValue::decode(thisValue));
    if (!plainDate) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.dayOfYear called on value that's not a PlainDate"_s);

    return JSValue::encode(jsNumber(plainDate->dayOfYear()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindate.prototype.weekofyear
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterWeekOfYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(JSValue::decode(thisValue));
    if (!plainDate) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.weekOfYear called on value that's not a PlainDate"_s);

    if (plainDate->calendarID() != iso8601CalendarID())
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsNumber(plainDate->weekOfYear()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindate.prototype.daysinweek
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDaysInWeek, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(JSValue::decode(thisValue));
    if (!plainDate) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.daysInWeek called on value that's not a PlainDate"_s);

    return JSValue::encode(jsNumber(7)); // ISO8601 calendar always returns 7.
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindate.prototype.daysinmonth
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDaysInMonth, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(JSValue::decode(thisValue));
    if (!plainDate) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.daysInMonth called on value that's not a PlainDate"_s);

    if (!TemporalCore::calendarIsISO(plainDate->calendarID())) {
        auto result = TemporalCore::calendarDaysInMonth(plainDate->calendarID(), plainDate->plainDate());
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(ISO8601::daysInMonth(plainDate->year(), plainDate->month())));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindate.prototype.daysinyear
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDaysInYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(JSValue::decode(thisValue));
    if (!plainDate) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.daysInYear called on value that's not a PlainDate"_s);

    if (!TemporalCore::calendarIsISO(plainDate->calendarID())) {
        auto result = TemporalCore::calendarDaysInYear(plainDate->calendarID(), plainDate->plainDate());
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(isLeapYear(plainDate->year()) ? 366 : 365));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindate.prototype.monthsinyear
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterMonthsInYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(JSValue::decode(thisValue));
    if (!plainDate) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.monthsInYear called on value that's not a PlainDate"_s);

    if (!TemporalCore::calendarIsISO(plainDate->calendarID())) {
        auto result = TemporalCore::calendarMonthsInYear(plainDate->calendarID(), plainDate->plainDate());
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(12));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindate.prototype.inleapyear
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterInLeapYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(JSValue::decode(thisValue));
    if (!plainDate) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.inLeapYear called on value that's not a PlainDate"_s);

    if (!TemporalCore::calendarIsISO(plainDate->calendarID())) {
        auto result = TemporalCore::calendarInLeapYear(plainDate->calendarID(), plainDate->plainDate());
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsBoolean(*result));
    }
    return JSValue::encode(jsBoolean(isLeapYear(plainDate->year())));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindate.prototype.era
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterEra, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(JSValue::decode(thisValue));
    if (!plainDate) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.era called on value that's not a PlainDate"_s);

    // Step 3: Return CalendarISOToDate(calendar, isoDate).[[Era]].
    auto result = TemporalCore::calendarEra(plainDate->calendarID(), plainDate->plainDate());
    if (!result || !*result)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsString(vm, **result));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindate.prototype.erayear
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterEraYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(JSValue::decode(thisValue));
    if (!plainDate) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.eraYear called on value that's not a PlainDate"_s);

    // Steps 3-5: Return CalendarISOToDate(calendar, isoDate).[[EraYear]], or undefined.
    auto result = TemporalCore::calendarEraYear(plainDate->calendarID(), plainDate->plainDate());
    if (!result || !*result)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsNumber(**result));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindate.prototype.yearofweek
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterYearOfWeek, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(JSValue::decode(thisValue));
    if (!plainDate) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.yearOfWeek called on value that's not a PlainDate"_s);

    if (plainDate->calendarID() != iso8601CalendarID())
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsNumber(plainDate->yearOfWeek()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.withcalendar
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncWithCalendar, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: branding.
    auto* plainDate = dynamicDowncast<TemporalPlainDate>(callFrame->thisValue().toThis(globalObject, ECMAMode::strict()));
    if (!plainDate) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.withCalendar called on value that's not a PlainDate"_s);

    // Step 3: calendar = ? ToTemporalCalendarIdentifier(calendarLike).
    auto newCalendarID = toTemporalCalendarIdentifier(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });
    // Step 4: Return ! CreateTemporalDate(this.[[ISODate]], calendar).
    auto* result = TemporalPlainDate::create(vm, globalObject->plainDateStructure(), plainDate->plainDate());
    result->setCalendarID(newCalendarID);
    return JSValue::encode(result);
}

} // namespace JSC
