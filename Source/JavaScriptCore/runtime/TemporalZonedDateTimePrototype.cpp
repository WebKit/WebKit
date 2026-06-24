/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "TemporalZonedDateTimePrototype.h"

#include "CalendarICUBridge.h"
#include "DurationArithmetic.h"
#include "ISO8601.h"
#include "ISOArithmetic.h"
#include "IntlDateTimeFormat.h"
#include "IntlObjectInlines.h"
#include "JSBigInt.h"
#include "JSCInlines.h"
#include "JSGlobalObject.h"
#include "ObjectConstructor.h"
#include "Rounding.h"
#include "TemporalCalendar.h"
#include "TemporalDuration.h"
#include "TemporalEnums.h"
#include "TemporalInstant.h"
#include "TemporalObject.h"
#include "TemporalPlainDate.h"
#include "TemporalPlainDateTime.h"
#include "TemporalPlainTime.h"
#include "TemporalZonedDateTime.h"
#include "TimeZoneICUBridge.h"
#include "ZonedDateTimeCore.h"
#include <wtf/DateMath.h>
#include <wtf/text/MakeString.h>

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncWith);
static JSC_DECLARE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncWithPlainTime);
static JSC_DECLARE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncWithTimeZone);
static JSC_DECLARE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncWithCalendar);
static JSC_DECLARE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncAdd);
static JSC_DECLARE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncSubtract);
static JSC_DECLARE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncUntil);
static JSC_DECLARE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncSince);
static JSC_DECLARE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncRound);
static JSC_DECLARE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncStartOfDay);
static JSC_DECLARE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncGetTimeZoneTransition);
static JSC_DECLARE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncEquals);
static JSC_DECLARE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncToInstant);
static JSC_DECLARE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncToPlainDateTime);
static JSC_DECLARE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncToPlainDate);
static JSC_DECLARE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncToPlainTime);
static JSC_DECLARE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncToString);
static JSC_DECLARE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncToJSON);
static JSC_DECLARE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncToLocaleString);
static JSC_DECLARE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncValueOf);
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterEpochNanoseconds);
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterTimeZoneId);
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterCalendarId);
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterMonth);
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterMonthCode);
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterDay);
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterHour);
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterMinute);
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterSecond);
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterMillisecond);
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterMicrosecond);
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterNanosecond);
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterOffset);
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterOffsetNanoseconds);
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterDayOfWeek);
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterDayOfYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterWeekOfYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterYearOfWeek);
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterHoursInDay);
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterDaysInWeek);
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterDaysInMonth);
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterDaysInYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterMonthsInYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterInLeapYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterEra);
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterEraYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterEpochMilliseconds);

}

#include "TemporalZonedDateTimePrototype.lut.h"

namespace JSC {

const ClassInfo TemporalZonedDateTimePrototype::s_info = { "Temporal.ZonedDateTime"_s, &Base::s_info, &zonedDateTimePrototypeTable, nullptr, CREATE_METHOD_TABLE(TemporalZonedDateTimePrototype) };

/* Source for TemporalZonedDateTimePrototype.lut.h
@begin zonedDateTimePrototypeTable
  with               temporalZonedDateTimePrototypeFuncWith               DontEnum|Function 1
  withPlainTime      temporalZonedDateTimePrototypeFuncWithPlainTime      DontEnum|Function 0
  withTimeZone       temporalZonedDateTimePrototypeFuncWithTimeZone       DontEnum|Function 1
  withCalendar       temporalZonedDateTimePrototypeFuncWithCalendar       DontEnum|Function 1
  add                temporalZonedDateTimePrototypeFuncAdd                DontEnum|Function 1
  subtract           temporalZonedDateTimePrototypeFuncSubtract           DontEnum|Function 1
  until              temporalZonedDateTimePrototypeFuncUntil              DontEnum|Function 1
  since              temporalZonedDateTimePrototypeFuncSince              DontEnum|Function 1
  round              temporalZonedDateTimePrototypeFuncRound              DontEnum|Function 1
  startOfDay         temporalZonedDateTimePrototypeFuncStartOfDay         DontEnum|Function 0
  getTimeZoneTransition temporalZonedDateTimePrototypeFuncGetTimeZoneTransition DontEnum|Function 1
  equals             temporalZonedDateTimePrototypeFuncEquals             DontEnum|Function 1
  toInstant          temporalZonedDateTimePrototypeFuncToInstant          DontEnum|Function 0
  toPlainDateTime    temporalZonedDateTimePrototypeFuncToPlainDateTime    DontEnum|Function 0
  toPlainDate        temporalZonedDateTimePrototypeFuncToPlainDate        DontEnum|Function 0
  toPlainTime        temporalZonedDateTimePrototypeFuncToPlainTime        DontEnum|Function 0
  toString           temporalZonedDateTimePrototypeFuncToString           DontEnum|Function 0
  toJSON             temporalZonedDateTimePrototypeFuncToJSON             DontEnum|Function 0
  toLocaleString     temporalZonedDateTimePrototypeFuncToLocaleString     DontEnum|Function 0
  valueOf            temporalZonedDateTimePrototypeFuncValueOf            DontEnum|Function 0
  epochNanoseconds   temporalZonedDateTimePrototypeGetterEpochNanoseconds DontEnum|ReadOnly|CustomAccessor
  timeZoneId         temporalZonedDateTimePrototypeGetterTimeZoneId       DontEnum|ReadOnly|CustomAccessor
  calendarId         temporalZonedDateTimePrototypeGetterCalendarId       DontEnum|ReadOnly|CustomAccessor
  year               temporalZonedDateTimePrototypeGetterYear             DontEnum|ReadOnly|CustomAccessor
  month              temporalZonedDateTimePrototypeGetterMonth            DontEnum|ReadOnly|CustomAccessor
  monthCode          temporalZonedDateTimePrototypeGetterMonthCode        DontEnum|ReadOnly|CustomAccessor
  day                temporalZonedDateTimePrototypeGetterDay              DontEnum|ReadOnly|CustomAccessor
  hour               temporalZonedDateTimePrototypeGetterHour             DontEnum|ReadOnly|CustomAccessor
  minute             temporalZonedDateTimePrototypeGetterMinute           DontEnum|ReadOnly|CustomAccessor
  second             temporalZonedDateTimePrototypeGetterSecond           DontEnum|ReadOnly|CustomAccessor
  millisecond        temporalZonedDateTimePrototypeGetterMillisecond      DontEnum|ReadOnly|CustomAccessor
  microsecond        temporalZonedDateTimePrototypeGetterMicrosecond      DontEnum|ReadOnly|CustomAccessor
  nanosecond         temporalZonedDateTimePrototypeGetterNanosecond       DontEnum|ReadOnly|CustomAccessor
  offset             temporalZonedDateTimePrototypeGetterOffset           DontEnum|ReadOnly|CustomAccessor
  offsetNanoseconds  temporalZonedDateTimePrototypeGetterOffsetNanoseconds DontEnum|ReadOnly|CustomAccessor
  dayOfWeek          temporalZonedDateTimePrototypeGetterDayOfWeek        DontEnum|ReadOnly|CustomAccessor
  dayOfYear          temporalZonedDateTimePrototypeGetterDayOfYear        DontEnum|ReadOnly|CustomAccessor
  weekOfYear         temporalZonedDateTimePrototypeGetterWeekOfYear       DontEnum|ReadOnly|CustomAccessor
  yearOfWeek         temporalZonedDateTimePrototypeGetterYearOfWeek       DontEnum|ReadOnly|CustomAccessor
  hoursInDay         temporalZonedDateTimePrototypeGetterHoursInDay       DontEnum|ReadOnly|CustomAccessor
  daysInWeek         temporalZonedDateTimePrototypeGetterDaysInWeek       DontEnum|ReadOnly|CustomAccessor
  daysInMonth        temporalZonedDateTimePrototypeGetterDaysInMonth      DontEnum|ReadOnly|CustomAccessor
  daysInYear         temporalZonedDateTimePrototypeGetterDaysInYear       DontEnum|ReadOnly|CustomAccessor
  monthsInYear       temporalZonedDateTimePrototypeGetterMonthsInYear     DontEnum|ReadOnly|CustomAccessor
  inLeapYear         temporalZonedDateTimePrototypeGetterInLeapYear       DontEnum|ReadOnly|CustomAccessor
  era                temporalZonedDateTimePrototypeGetterEra              DontEnum|ReadOnly|CustomAccessor
  eraYear            temporalZonedDateTimePrototypeGetterEraYear          DontEnum|ReadOnly|CustomAccessor
  epochMilliseconds  temporalZonedDateTimePrototypeGetterEpochMilliseconds DontEnum|ReadOnly|CustomAccessor
@end
*/

TemporalZonedDateTimePrototype* TemporalZonedDateTimePrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    auto* object = new (NotNull, allocateCell<TemporalZonedDateTimePrototype>(vm)) TemporalZonedDateTimePrototype(vm, structure);
    object->finishCreation(vm, globalObject);
    return object;
}

Structure* TemporalZonedDateTimePrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalZonedDateTimePrototype::TemporalZonedDateTimePrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void TemporalZonedDateTimePrototype::finishCreation(VM& vm, JSGlobalObject*)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

// temporal_rs: ZonedDateTime::start_of_day_with_provider
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.startofday
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncStartOfDay, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Let zonedDateTime be the this value.
    // Step 2: Perform ? RequireInternalSlot(zonedDateTime, [[InitializedTemporalZonedDateTime]]).
    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.startOfDay called on value that's not a ZonedDateTime"_s);

    // Steps 3-5: Get timeZone, calendar, and local isoDateTime via GetISODateTimeFor.
    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    zdt->getLocalDateAndTime(globalObject, date, time);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 6: Let epochNanoseconds be ? GetStartOfDay(timeZone, isoDateTime.[[ISODate]]).
    auto sodResult = TemporalCore::getStartOfDay(zdt->timeZone(), date);
    RETURN_IF_EXCEPTION(scope, { });
    if (!sodResult) [[unlikely]] {
        throwRangeError(globalObject, scope, sodResult.error().message);
        return { };
    }

    // Step 7: Return ! CreateTemporalZonedDateTime(epochNanoseconds, timeZone, calendar).
    RELEASE_AND_RETURN(scope, JSValue::encode(zdt->withExactTime(globalObject, *sodResult)));
}

// temporal_rs: ZonedDateTime::with_plain_time_and_provider
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.withplaintime
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncWithPlainTime, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Let zonedDateTime be the this value.
    // Step 2: Perform ? RequireInternalSlot(zonedDateTime, [[InitializedTemporalZonedDateTime]]).
    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.withPlainTime called on value that's not a ZonedDateTime"_s);

    // Steps 3-5: timeZone, calendar, isoDateTime = GetISODateTimeFor(timeZone, epochNanoseconds).
    ISO8601::PlainTime newTime;
    JSValue timeArg = callFrame->argument(0);
    if (!timeArg.isUndefined()) {
        // Step 7.a: plainTime = ? ToTemporalTime(plainTimeLike).
        auto* pt = TemporalPlainTime::from(globalObject, timeArg, jsUndefined());
        RETURN_IF_EXCEPTION(scope, { });
        newTime = pt->plainTime();
    }

    ISO8601::PlainDate date;
    ISO8601::PlainTime unused;
    zdt->getLocalDateAndTime(globalObject, date, unused);
    RETURN_IF_EXCEPTION(scope, { });

    ISO8601::ExactTime resultEpochNs;
    if (timeArg.isUndefined()) {
        // Step 6.a: epochNs = ? GetStartOfDay(timeZone, isoDateTime.[[ISODate]]).
        auto sodResult2 = TemporalCore::getStartOfDay(zdt->timeZone(), date);
        RETURN_IF_EXCEPTION(scope, { });
        if (!sodResult2) [[unlikely]] {
            throwRangeError(globalObject, scope, sodResult2.error().message);
            return { };
        }
        resultEpochNs = *sodResult2;
    } else {
        // Steps 7.b-7.c: CombineISODateAndTimeRecord + GetEpochNanosecondsFor(timeZone, result, ~compatible~).
        auto epochNs = TemporalCore::getEpochNanosecondsFor(zdt->timeZone(), date, newTime, TemporalDisambiguation::Compatible);
        if (!epochNs) [[unlikely]] {
            throwRangeError(globalObject, scope, epochNs.error().message);
            return { };
        }
        resultEpochNs = *epochNs;
    }

    // Step 8: Return ! CreateTemporalZonedDateTime(epochNs, timeZone, calendar).
    RELEASE_AND_RETURN(scope, JSValue::encode(zdt->withExactTime(globalObject, resultEpochNs)));
}

// https://tc39.es/proposal-temporal/#sec-temporal-adddurationtozoneddatetime
// Caller handles steps 1 (ToTemporalDuration) and 2 (negate if subtract).
static EncodedJSValue addDurationToZonedDateTime(JSGlobalObject* globalObject, ThrowScope& scope, TemporalZonedDateTime* zdt, ISO8601::Duration duration, JSValue optionsArg)
{
    // Steps 3-4: GetOptionsObject + GetTemporalOverflowOption.
    TemporalOverflow overflow = toTemporalOverflow(globalObject, optionsArg);
    RETURN_IF_EXCEPTION(scope, { });
    // Steps 5-8: AddZonedDateTime(epochNs, timeZone, calendar, duration, overflow).
    auto addResult = TemporalCore::addZonedDateTime(zdt->exactTime(), zdt->timeZone(), duration, overflow, zdt->calendarID());
    if (!addResult) [[unlikely]] {
        throwRangeError(globalObject, scope, addResult.error().message);
        return { };
    }
    // Step 9: Return ! CreateTemporalZonedDateTime(epochNanoseconds, timeZone, calendar).
    RELEASE_AND_RETURN(scope, JSValue::encode(zdt->withExactTime(globalObject, *addResult)));
}

// temporal_rs: ZonedDateTime::add_with_provider
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.add
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncAdd, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Let zonedDateTime be the this value.
    // Step 2: Perform ? RequireInternalSlot(zonedDateTime, [[InitializedTemporalZonedDateTime]]).
    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.add called on value that's not a ZonedDateTime"_s);

    // Step 3: Return ? AddDurationToZonedDateTime(~add~, zonedDateTime, temporalDurationLike, options).
    auto duration = TemporalDuration::toISO8601Duration(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });
    RELEASE_AND_RETURN(scope, addDurationToZonedDateTime(globalObject, scope, zdt, WTF::move(duration), callFrame->argument(1)));
}

// temporal_rs: ZonedDateTime::subtract_with_provider
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.subtract
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncSubtract, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Let zonedDateTime be the this value.
    // Step 2: Perform ? RequireInternalSlot(zonedDateTime, [[InitializedTemporalZonedDateTime]]).
    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.subtract called on value that's not a ZonedDateTime"_s);

    // Step 3: Return ? AddDurationToZonedDateTime(~subtract~, ...). Step 2 of that AO negates.
    auto duration = TemporalDuration::toISO8601Duration(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });
    RELEASE_AND_RETURN(scope, addDurationToZonedDateTime(globalObject, scope, zdt, -WTF::move(duration), callFrame->argument(1)));
}

// https://tc39.es/proposal-temporal/#sec-temporal-differencetemporalzoneddatetime
// Caller handles steps 1-2 (RequireInternalSlot). op selects ~until~ vs ~since~.
static EncodedJSValue differenceTemporalZonedDateTime(JSGlobalObject* globalObject, ThrowScope& scope, TemporalZonedDateTime* zdt, JSValue otherArg, JSValue optionsArg, DifferenceOperation op)
{
    // Step 1: Set _other_ to ? ToTemporalZonedDateTime(_other_).
    auto* other = TemporalZonedDateTime::from(globalObject, otherArg);
    RETURN_IF_EXCEPTION(scope, { });
    ASSERT(other);

    // Step 2: If CalendarEquals(zonedDateTime.[[Calendar]], other.[[Calendar]]) is false, throw RangeError.
    // CalendarID is already canonical (from isBuiltinCalendar); direct comparison implements CalendarEquals.
    if (zdt->calendarID() != other->calendarID()) [[unlikely]] {
        throwRangeError(globalObject, scope, "cannot compute difference between ZonedDateTimes with different calendars"_s);
        return { };
    }

    // Steps 3-4: GetOptionsObject + GetDifferenceSettings(op, options, ~datetime~, «», ~nanosecond~, ~hour~).
    auto [smallestUnit, largestUnit, roundingMode, increment] = extractDifferenceOptions(
        globalObject, optionsArg, UnitGroup::DateTime, TemporalUnit::Nanosecond, TemporalUnit::Hour, op);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 5: If TemporalUnitCategory(largestUnit) is ~time~, use DifferenceInstant fast path.
    // We skip this optimization — differenceZonedDateTimeWithRounding handles both branches correctly.

    // Step 7: If TimeZoneEquals(zonedDateTime.[[TimeZone]], other.[[TimeZone]]) is false, throw RangeError.
    // Only reached when largestUnit is not a time unit (spec step 5 otherwise returns early).
    if (largestUnit <= TemporalUnit::Day) {
        if (!TemporalCore::timeZoneEquals(zdt->timeZone(), other->timeZone())) {
            throwRangeError(globalObject, scope, "cannot compute day-or-larger difference between ZonedDateTimes with different time zones"_s);
            return { };
        }
    }

    // Step 9: Let internalDuration be ? DifferenceZonedDateTimeWithRounding(...).
    // (Spec step 8 "if epochNs equal return zero duration" is subsumed by the core function.)
    auto coreResult = TemporalCore::differenceZonedDateTimeWithRounding(zdt->exactTime(), other->exactTime(),
        zdt->timeZone(), largestUnit, smallestUnit, roundingMode, increment, zdt->calendarID());
    if (!coreResult) [[unlikely]] {
        throwTemporalError(globalObject, scope, coreResult.error());
        return { };
    }
    // Step 10: Let result be ! TemporalDurationFromInternal(internalDuration, ~hour~).
    // The spec always uses ~hour~ here; for time-category largestUnit (skipped fast path),
    // durationLargestUnit = largestUnit which matches the fast path's TemporalDurationFromInternal(dur, largestUnit).
    TemporalUnit durationLargestUnit = (largestUnit <= TemporalUnit::Day) ? TemporalUnit::Hour : largestUnit;
    auto durResult = TemporalCore::temporalDurationFromInternal(*coreResult, durationLargestUnit);
    if (!durResult) [[unlikely]] {
        throwTemporalError(globalObject, scope, durResult.error());
        return { };
    }
    ISO8601::Duration result = *durResult;
    // Step 11: For ~since~, negate the result.
    if (op == DifferenceOperation::Since)
        result = -result;

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalDuration::tryCreateIfValid(globalObject, WTF::move(result), globalObject->durationStructure())));
}

// temporal_rs: ZonedDateTime::until_with_provider
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.until
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncUntil, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: RequireInternalSlot.
    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.until called on value that's not a ZonedDateTime"_s);

    // Step 3: Return ? DifferenceTemporalZonedDateTime(~until~, ...).
    RELEASE_AND_RETURN(scope, differenceTemporalZonedDateTime(globalObject, scope, zdt, callFrame->argument(0), callFrame->argument(1), DifferenceOperation::Until));
}

// temporal_rs: ZonedDateTime::since_with_provider
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.since
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncSince, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: RequireInternalSlot.
    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.since called on value that's not a ZonedDateTime"_s);

    // Step 3: Return ? DifferenceTemporalZonedDateTime(~since~, ...).
    RELEASE_AND_RETURN(scope, differenceTemporalZonedDateTime(globalObject, scope, zdt, callFrame->argument(0), callFrame->argument(1), DifferenceOperation::Since));
}

// temporal_rs: ZonedDateTime::round_with_provider
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.round
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncRound, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: RequireInternalSlot.
    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.round called on value that's not a ZonedDateTime"_s);

    // Step 3: If roundTo is undefined, throw TypeError.
    // Steps 4-5: String shorthand → synthetic object; else GetOptionsObject.
    JSValue optionsArg = callFrame->argument(0);
    JSObject* options = nullptr;
    if (optionsArg.isString()) {
        options = constructEmptyObject(globalObject->vm(), globalObject->nullPrototypeObjectStructure());
        RETURN_IF_EXCEPTION(scope, { });
        options->putDirect(vm, vm.propertyNames->smallestUnit, optionsArg);
    } else if (optionsArg.isObject())
        options = asObject(optionsArg);
    else
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.round requires a smallestUnit option string or options object"_s);

    // Step 6: NOTE — read options in alphabetical order.
    // Step 7: Let roundingIncrement be ? GetRoundingIncrementOption(roundTo).
    auto roundingIncrement = temporalRoundingIncrement(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });
    // Step 8: Let roundingMode be ? GetRoundingModeOption(roundTo, ~half-expand~).
    RoundingMode roundingMode = temporalRoundingMode(globalObject, options, RoundingMode::HalfExpand);
    RETURN_IF_EXCEPTION(scope, { });
    // Step 9: Let smallestUnit be ? GetTemporalUnitValuedOption(roundTo, "smallestUnit", ~required~).
    auto smallestUnitMaybeAuto = temporalUnitValued(globalObject, options, vm.propertyNames->smallestUnit, TemporalUnitDefault::Required);
    RETURN_IF_EXCEPTION(scope, { });
    // Step 10: Perform ? ValidateTemporalUnitValue(smallestUnit, ~time~, « ~day~ »).
    validateTemporalUnitValue(globalObject, smallestUnitMaybeAuto, UnitGroup::Time, AllowedUnit::Day, "smallestUnit"_s);
    RETURN_IF_EXCEPTION(scope, { });
    auto smallestOpt = std::get<std::optional<TemporalUnit>>(smallestUnitMaybeAuto);
    TemporalUnit smallestUnit = smallestOpt.value();

    // Steps 11-13: Compute maximum increment and ValidateTemporalRoundingIncrement.
    if (smallestUnit == TemporalUnit::Day)
        validateTemporalRoundingIncrement(globalObject, roundingIncrement, 1, Inclusivity::Inclusive);
    else {
        auto maxOpt = TemporalCore::maximumRoundingIncrement(smallestUnit);
        validateTemporalRoundingIncrement(globalObject, roundingIncrement, maxOpt ? std::optional<double>(*maxOpt) : std::nullopt, Inclusivity::Exclusive);
    }
    RETURN_IF_EXCEPTION(scope, { });

    // Step 14: If smallestUnit is ~nanosecond~ and roundingIncrement = 1, return new ZDT.
    // Spec requires a new object even when no rounding occurs (result !== input).
    if (smallestUnit == TemporalUnit::Nanosecond && roundingIncrement == 1)
        RELEASE_AND_RETURN(scope, JSValue::encode(zdt->withExactTime(globalObject, zdt->exactTime())));

    // Steps 15-17: thisNs, timeZone, calendar.
    // Step 18: isoDateTime = GetISODateTimeFor(timeZone, thisNs).
    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    zdt->getLocalDateAndTime(globalObject, date, time);
    RETURN_IF_EXCEPTION(scope, { });

    Int128 epochNs = zdt->exactTime().epochNanoseconds();
    Int128 resultNs;

    if (smallestUnit == TemporalUnit::Day) {
        // Step 19.a: dateStart = isoDateTime.[[ISODate]] — `date` from step 18.
        // Step 19.b: dateEnd = AddDaysToISODate(dateStart, 1).
        ISO8601::PlainDate nextDate;
        {
            int32_t days = WTF::daysFromYearMonth(date.year(), date.month() - 1) + (date.day() - 1) + 1;
            auto [y, m, d] = WTF::yearMonthDayFromDays(days);
            nextDate = ISO8601::PlainDate(y, static_cast<uint8_t>(m + 1), static_cast<uint8_t>(d));
        }
        // Step 19.c: startNs = ? GetStartOfDay(timeZone, dateStart).
        auto startOfDayResult = TemporalCore::getStartOfDay(zdt->timeZone(), date);
        RETURN_IF_EXCEPTION(scope, { });
        if (!startOfDayResult) [[unlikely]] {
            throwRangeError(globalObject, scope, startOfDayResult.error().message);
            return { };
        }
        // Step 19.d: Assert: thisNs ≥ startNs.
        // Step 19.e: endNs = ? GetStartOfDay(timeZone, dateEnd).
        auto nextDayStartResult = TemporalCore::getStartOfDay(zdt->timeZone(), nextDate);
        RETURN_IF_EXCEPTION(scope, { });
        if (!nextDayStartResult) [[unlikely]] {
            throwRangeError(globalObject, scope, nextDayStartResult.error().message);
            return { };
        }
        Int128 startNs = startOfDayResult->epochNanoseconds();
        Int128 nextNs = nextDayStartResult->epochNanoseconds();
        // Step 19.g: dayLengthNs = endNs - startNs.
        Int128 dayLength = nextNs - startNs;
        if (!dayLength || epochNs < startNs) [[unlikely]] {
            throwRangeError(globalObject, scope, "Rounding result is outside the supported range of Temporal.ZonedDateTime"_s);
            return { };
        }
        // Step 19.f: Assert: thisNs < endNs.
        // Polyfill fix (issue #3312): backward DST transitions crossing midnight cause epochNs ≥ nextNs
        // for a valid local date (e.g. Antarctica/Casey 2010-03-05 +11→+08 skipped midnight).
        // Cap to nextNs - 1 so rounding stays within the day's epoch range.
        if (epochNs >= nextNs)
            epochNs = nextNs - 1;
        // Step 19.h: dayProgressNs = TimeDurationFromEpochNanosecondsDifference(thisNs, startNs).
        Int128 offset = epochNs - startNs;
        // Step 19.i: roundedDayNs = ! RoundTimeDurationToIncrement(dayProgressNs, dayLengthNs, roundingMode).
        Int128 roundedOffset = TemporalCore::roundNumberToIncrementInt128(offset, dayLength, roundingMode);
        // Step 19.j: epochNanoseconds = AddTimeDurationToEpochNanoseconds(roundedDayNs, startNs).
        resultNs = startNs + roundedOffset;
    } else {
        // Step 20.a: roundResult = RoundISODateTime(isoDateTime, roundingIncrement, smallestUnit, roundingMode).
        Int128 unitLen = static_cast<Int128>(lengthInNanoseconds(smallestUnit));
        Int128 incrementNs = unitLen * static_cast<Int128>(static_cast<int64_t>(roundingIncrement));
        auto curOffsetResult = TemporalCore::getOffsetNanosecondsFor(zdt->timeZone(), zdt->exactTime());
        if (!curOffsetResult) [[unlikely]] {
            throwRangeError(globalObject, scope, curOffsetResult.error().message);
            return { };
        }
        auto [roundedDate, roundedTime] = TemporalCore::roundISODateTime(date, time, incrementNs, smallestUnit, roundingMode);
        // Step 20.b: offsetNanoseconds = GetOffsetNanosecondsFor(timeZone, thisNs).
        // Step 20.c: epochNanoseconds = ? InterpretISODateTimeOffset(...).
        auto epochNsResult = TemporalCore::interpretISODateTimeOffset(
            roundedDate, roundedTime, /* useStartOfDay */ false,
            OffsetBehaviour::Option, TemporalOffsetDisambiguation::Prefer,
            *curOffsetResult, /* offsetHasSubMinutePrecision */ false,
            zdt->timeZone(), TemporalDisambiguation::Compatible);
        if (!epochNsResult) [[unlikely]] {
            throwTemporalError(globalObject, scope, epochNsResult.error());
            return { };
        }
        resultNs = epochNsResult->epochNanoseconds();
    }

    // Step 21: Return ! CreateTemporalZonedDateTime(epochNanoseconds, timeZone, calendar).
    RELEASE_AND_RETURN(scope, JSValue::encode(zdt->withExactTime(globalObject, ISO8601::ExactTime(resultNs))));
}

// temporal_rs: ZonedDateTime::get_time_zone_transition_with_provider
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.gettimezonetransition
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncGetTimeZoneTransition, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: RequireInternalSlot.
    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.getTimeZoneTransition called on value that's not a ZonedDateTime"_s);

    // Step 3: Let timeZone be zonedDateTime.[[TimeZone]].
    // Step 4: If directionParam is undefined, throw TypeError.
    // Steps 5-6: String shorthand → synthetic object with "direction"; else GetOptionsObject.
    JSValue dirArg = callFrame->argument(0);
    String dirStr;
    if (dirArg.isString()) {
        dirStr = asString(dirArg)->value(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
    } else {
        JSObject* options = dirArg.getObject();
        if (!options) [[unlikely]] {
            if (dirArg.isUndefined())
                return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.getTimeZoneTransition requires a 'direction' option"_s);
            return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.getTimeZoneTransition requires an options object or string"_s);
        }
        JSValue dirVal = options->get(globalObject, Identifier::fromString(vm, "direction"_s));
        RETURN_IF_EXCEPTION(scope, { });
        if (dirVal.isUndefined())
            return throwVMRangeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.getTimeZoneTransition requires a 'direction' option"_s);
        dirStr = dirVal.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
    }
    // Step 7: Let direction be ? GetDirectionOption(directionParam).
    TransitionDirection direction;
    if (dirStr == "next"_s)
        direction = TransitionDirection::Next;
    else if (dirStr == "previous"_s)
        direction = TransitionDirection::Previous;
    else
        return throwVMRangeError(globalObject, scope, "direction must be \"next\" or \"previous\""_s);

    // Step 8: If IsOffsetTimeZoneIdentifier(timeZone) is true, return null.
    // Steps 9-10: GetNamedTimeZoneNextTransition / GetNamedTimeZonePreviousTransition.
    auto transResult = TemporalCore::getTimeZoneTransition(zdt->timeZone(), zdt->exactTime(), direction);
    if (!transResult) [[unlikely]] {
        throwRangeError(globalObject, scope, transResult.error().message);
        return { };
    }

    // Step 11: If transition is null, return null.
    if (!transResult->has_value())
        return JSValue::encode(jsNull());

    // Step 12: Return ! CreateTemporalZonedDateTime(transition, timeZone, zonedDateTime.[[Calendar]]).
    RELEASE_AND_RETURN(scope, JSValue::encode(zdt->withExactTime(globalObject, (*transResult).value())));
}

// temporal_rs: ZonedDateTime::with_with_provider
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.with
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncWith, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: RequireInternalSlot.
    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.with called on value that's not a ZonedDateTime"_s);

    // Step 3: If ? IsPartialTemporalObject(temporalZonedDateTimeLike) is false, throw TypeError.
    JSValue fieldsArg = callFrame->argument(0);
    if (!fieldsArg.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.with requires a plain object of field overrides"_s);
    JSObject* fields = asObject(fieldsArg);
    rejectObjectWithCalendarOrTimeZone(globalObject, fields);
    RETURN_IF_EXCEPTION(scope, { });

    // Steps 4-6: epochNs, timeZone, calendar from this ZDT.
    // Steps 7-8: offsetNanoseconds = GetOffsetNanosecondsFor(timeZone, epochNs);
    //            isoDateTime = GetISODateTimeFor(timeZone, epochNs).
    ISO8601::PlainDate curDate;
    ISO8601::PlainTime curTime;
    zdt->getLocalDateAndTime(globalObject, curDate, curTime);
    RETURN_IF_EXCEPTION(scope, { });
    auto curOffsetNsOpt = zdt->getOffsetNanoseconds(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    ASSERT(curOffsetNsOpt);
    int64_t curOffsetNs = *curOffsetNsOpt;

    // Steps 9-16: ISODateToFields(calendar, isoDate, ~date~) + set time/offset fields.
    //   (Fused with CalendarMergeFields below; current field values serve as the base.)
    // Step 17: partialZonedDateTime = ? PrepareCalendarFields(calendar, temporalZonedDateTimeLike,
    //          «year,month,month-code,day», «hour,...,nanosecond,offset», ~partial~).
    //   CalendarRead::Skip — calendar already known from zdt; timeZone not read in with().
    //   outCalendarId output is discarded (not used after this call).
    CalendarID unusedCalId = zdt->calendarID();
    auto partialFields = readZonedDateTimeFieldsFromObject<ZonedDateTimeFieldMode::Partial, CalendarRead::Skip>(globalObject, fields, unusedCalId);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 18: fields = CalendarMergeFields(calendar, fields, partialZonedDateTime).
    // Implemented by merging partialFields into the current ZDT's field values.
    // For non-ISO calendars, fall back to calendar-coordinate values from ISODateToFields.

    // Step 19: resolvedOptions = ? GetOptionsObject(options).
    JSObject* options = intlGetOptionsObject(globalObject, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    // Steps 20-22: disambiguation, offset, overflow (alphabetical order per spec NOTE).
    auto disambiguation = toTemporalDisambiguation(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });
    auto offsetOpt = toTemporalOffset(globalObject, options, TemporalOffsetDisambiguation::Prefer);
    RETURN_IF_EXCEPTION(scope, { });
    TemporalOverflow overflow = toTemporalOverflow(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 23: dateTimeResult = ? InterpretTemporalDateTimeFields(calendar, fields, overflow).
    // For non-ISO calendars, fall back to calendar-coordinate current values from ISODateToFields.
    int32_t fallbackYear = curDate.year();
    uint32_t fallbackMonth = curDate.month();
    uint32_t fallbackDay = curDate.day();
    std::optional<ParsedMonthCode> fallbackMonthCode;
    if (!TemporalCore::calendarIsISO(zdt->calendarID())) {
        auto calFields = TemporalCore::isoToCalendarFields(zdt->calendarID(), curDate);
        if (calFields) {
            fallbackYear = calFields->year;
            fallbackMonth = calFields->month;
            fallbackDay = calFields->day;
            if (!calFields->monthCode.isEmpty())
                fallbackMonthCode = ISO8601::parseMonthCode(calFields->monthCode);
        }
    }

    auto& pf = partialFields;
    auto& pDate = pf.dateFields;
    int32_t mergedYear = pDate.year.value_or(fallbackYear);
    uint32_t mergedMonth;
    if (pDate.month)
        mergedMonth = static_cast<uint32_t>(*pDate.month);
    else if (pDate.monthCode)
        mergedMonth = pDate.monthCode->monthNumber;
    else
        mergedMonth = fallbackMonth;
    uint32_t mergedDay = pDate.day ? static_cast<uint32_t>(*pDate.day) : fallbackDay;

    // For non-ISO with(), if neither month nor monthCode provided by user, use fallback monthCode.
    std::optional<ParsedMonthCode> resolvedMonthCode = pDate.monthCode;
    if (!pDate.month && !pDate.monthCode && fallbackMonthCode)
        resolvedMonthCode = fallbackMonthCode;

    ISO8601::PlainDate newDate;
    if (pDate.era || pDate.eraYear) {
        // User provided era+eraYear: route through calendarDateFromFields so ICU resolves the
        // ISO year from era+eraYear (UCAL_ERA + UCAL_YEAR = eraYear).
        // Pass pDate.year as optional: nullopt when absent (no consistency check),
        // has_value when user provided it (conflict with era+eraYear throws RangeError).
        std::optional<StringView> era;
        if (pDate.era)
            era = StringView(*pDate.era);
        auto result = TemporalCore::calendarDateFromFields(
            zdt->calendarID(), pDate.year, clampTo<uint8_t>(mergedMonth),
            static_cast<uint8_t>(mergedDay), era, pDate.eraYear, resolvedMonthCode, overflow);
        if (!result) [[unlikely]] {
            throwRangeError(globalObject, scope, String(result.error().message));
            return { };
        }
        newDate = *result;
    } else {
        newDate = isoDateFromFields(globalObject, TemporalDateFormat::Date, mergedYear, mergedMonth, mergedDay, resolvedMonthCode, overflow, zdt->calendarID());
        RETURN_IF_EXCEPTION(scope, { });
    }

    ISO8601::Duration timeDur { };
    timeDur.setField(TemporalUnit::Hour, pf.hour.value_or(static_cast<double>(curTime.hour())));
    timeDur.setField(TemporalUnit::Minute, pf.minute.value_or(static_cast<double>(curTime.minute())));
    timeDur.setField(TemporalUnit::Second, pf.second.value_or(static_cast<double>(curTime.second())));
    timeDur.setField(TemporalUnit::Millisecond, pf.millisecond.value_or(static_cast<double>(curTime.millisecond())));
    timeDur.setField(TemporalUnit::Microsecond, pf.microsecond.value_or(static_cast<double>(curTime.microsecond())));
    timeDur.setField(TemporalUnit::Nanosecond, pf.nanosecond.value_or(static_cast<double>(curTime.nanosecond())));
    auto newTime = TemporalPlainTime::regulateTime(globalObject, WTF::move(timeDur), overflow);
    RETURN_IF_EXCEPTION(scope, { });

    // Steps 24-25: InterpretISODateTimeOffset — resolve epoch nanoseconds.
    // Step 24: newOffsetNanoseconds — use provided offset OR current offset.
    // temporal_rs: ZonedDateTime::with_with_provider — offset always Some (either explicit or current).
    int64_t givenOffsetNs = pf.offsetNs.value_or(curOffsetNs);

    // Steps 24-25: InterpretISODateTimeOffset.
    // temporal_rs: ZonedDateTime::with_with_provider — is_exact=true when offset option is 'use'.
    // offset_nanos = Some(givenOffsetNs) always.
    if (offsetOpt == TemporalOffsetDisambiguation::Use) {
        Int128 naiveNs = TemporalCore::getUTCEpochNanoseconds(newDate, newTime);
        auto resultNs = naiveNs - Int128(givenOffsetNs);
        RELEASE_AND_RETURN(scope, JSValue::encode(zdt->withExactTime(globalObject, ISO8601::ExactTime(resultNs))));
    }

    if (offsetOpt == TemporalOffsetDisambiguation::Prefer || offsetOpt == TemporalOffsetDisambiguation::Reject) {
        // InterpretISODateTimeOffset step 7: CheckISODaysRange(isoDate).
        // The local date from with() may be outside ±10^8 days even if the UTC epoch is valid.
        if (std::abs(dateToDaysFrom1970(newDate.year(), static_cast<int>(newDate.month()) - 1, static_cast<int>(newDate.day()))) > 1e8) {
            throwRangeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.with result is outside the supported range"_s);
            return { };
        }
        // Try to find a possible instant matching the given offset (exact match).
        auto possible = TemporalCore::getPossibleEpochNanosecondsFor(zdt->timeZone(), newDate, newTime);
        if (!possible) [[unlikely]] {
            throwRangeError(globalObject, scope, possible.error().message);
            return { };
        }
        for (auto& candidate : TemporalCore::epochCandidates(*possible)) {
            auto offsetResult = TemporalCore::getOffsetNanosecondsFor(zdt->timeZone(), candidate);
            if (offsetResult && *offsetResult == givenOffsetNs)
                RELEASE_AND_RETURN(scope, JSValue::encode(zdt->withExactTime(globalObject, candidate)));
        }
        if (offsetOpt == TemporalOffsetDisambiguation::Reject) {
            throwRangeError(globalObject, scope, "ZonedDateTime.with: given offset does not match any possible instant"_s);
            return { };
        }
        // Prefer: no match found, fall through to disambiguation.
    }

    auto epochNs = TemporalZonedDateTime::getEpochNanosecondsFor(globalObject, zdt->timeZone(), newDate, newTime, disambiguation);
    RETURN_IF_EXCEPTION(scope, { });
    // Step 26: Return ! CreateTemporalZonedDateTime(epochNanoseconds, timeZone, calendar).
    RELEASE_AND_RETURN(scope, JSValue::encode(zdt->withExactTime(globalObject, *epochNs)));
}

// temporal_rs: ZonedDateTime::with_calendar
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.withcalendar
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncWithCalendar, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Let zonedDateTime be the this value.
    // Step 2: Perform ? RequireInternalSlot(zonedDateTime, [[InitializedTemporalZonedDateTime]]).
    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.withCalendar called on value that's not a ZonedDateTime"_s);

    // Step 3: Let calendar be ? ToTemporalCalendarIdentifier(calendarLike).
    JSValue calArg = callFrame->argument(0);
    CalendarID newCalendarID = toTemporalCalendarIdentifier(globalObject, calArg);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 4: Return ! CreateTemporalZonedDateTime(zonedDateTime.[[EpochNanoseconds]], zonedDateTime.[[TimeZone]], calendar).
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalZonedDateTime::create(vm, globalObject->zonedDateTimeStructure(), zdt->exactTime(), zdt->timeZone(), newCalendarID)));
}

// temporal_rs: ZonedDateTime::with_time_zone_with_provider
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.withtimezone
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncWithTimeZone, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Let zonedDateTime be the this value.
    // Step 2: Perform ? RequireInternalSlot(zonedDateTime, [[InitializedTemporalZonedDateTime]]).
    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.withTimeZone called on value that's not a ZonedDateTime"_s);

    // Step 3: Let timeZone be ? ToTemporalTimeZoneIdentifier(timeZoneLike).
    auto timeZone = toTemporalTimeZoneIdentifier(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });
    ASSERT(timeZone);

    // Step 4: Return ! CreateTemporalZonedDateTime(zonedDateTime.[[EpochNanoseconds]], timeZone, zonedDateTime.[[Calendar]]).
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalZonedDateTime::create(vm, globalObject->zonedDateTimeStructure(), zdt->exactTime(), *timeZone, zdt->calendarID())));
}

// temporal_rs: ZonedDateTime::equals_with_provider
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.equals
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncEquals, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Let zonedDateTime be the this value.
    // Step 2: Perform ? RequireInternalSlot(zonedDateTime, [[InitializedTemporalZonedDateTime]]).
    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.equals called on value that's not a ZonedDateTime"_s);

    // Step 3: Set other to ? ToTemporalZonedDateTime(other).
    auto* other = TemporalZonedDateTime::from(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });
    ASSERT(other);

    // Step 4: If zonedDateTime.[[EpochNanoseconds]] ≠ other.[[EpochNanoseconds]], return false.
    // Step 5: If TimeZoneEquals(...) is false, return false.
    // Step 6: Return CalendarEquals(zonedDateTime.[[Calendar]], other.[[Calendar]]).
    return JSValue::encode(jsBoolean(
        zdt->exactTime() == other->exactTime()
        && TemporalCore::timeZoneEquals(zdt->timeZone(), other->timeZone())
        && zdt->calendarID() == other->calendarID()));
}

// temporal_rs: ZonedDateTime::to_instant
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.toinstant
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncToInstant, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: RequireInternalSlot.
    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.toInstant called on value that's not a ZonedDateTime"_s);

    // Step 3: Return ! CreateTemporalInstant(zonedDateTime.[[EpochNanoseconds]]).
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalInstant::create(vm, globalObject->instantStructure(), zdt->exactTime())));
}

// temporal_rs: ZonedDateTime::to_plain_date
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.toplaindate
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncToPlainDate, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: RequireInternalSlot.
    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.toPlainDate called on value that's not a ZonedDateTime"_s);

    // Step 3: isoDateTime = GetISODateTimeFor(timeZone, epochNanoseconds).
    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    zdt->getLocalDateAndTime(globalObject, date, time);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 4: Return ! CreateTemporalDate(isoDateTime.[[ISODate]], zonedDateTime.[[Calendar]]).
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainDate::create(vm, globalObject->plainDateStructure(), WTF::move(date), zdt->calendarID())));
}

// temporal_rs: ZonedDateTime::to_plain_time
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.toplaintime
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncToPlainTime, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: RequireInternalSlot.
    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.toPlainTime called on value that's not a ZonedDateTime"_s);

    // Step 3: isoDateTime = GetISODateTimeFor(timeZone, epochNanoseconds).
    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    zdt->getLocalDateAndTime(globalObject, date, time);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 4: Return ! CreateTemporalTime(isoDateTime.[[Time]]).
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainTime::create(vm, globalObject->plainTimeStructure(), WTF::move(time))));
}

// temporal_rs: ZonedDateTime::to_plain_date_time
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.toplaindatetime
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncToPlainDateTime, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: RequireInternalSlot.
    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.toPlainDateTime called on value that's not a ZonedDateTime"_s);

    // Step 3: isoDateTime = GetISODateTimeFor(timeZone, epochNanoseconds).
    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    zdt->getLocalDateAndTime(globalObject, date, time);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 4: Return ! CreateTemporalDateTime(isoDateTime, zonedDateTime.[[Calendar]]).
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainDateTime::create(vm, globalObject->plainDateTimeStructure(), WTF::move(date), WTF::move(time), zdt->calendarID())));
}

// https://tc39.es/proposal-temporal/#sec-temporal-temporalzoneddatetimetostring
static String temporalZonedDateTimeToString(JSGlobalObject* globalObject, const TemporalZonedDateTime* zdt,
    const PrecisionData& precision, RoundingMode roundingMode,
    StringView showOffset, // ~auto~ | ~never~
    StringView showTimeZone, // ~auto~ | ~never~ | ~critical~
    StringView showCalendar, // ~auto~ | ~always~ | ~never~ | ~critical~
    CalendarID calendarID)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Round epoch nanoseconds (RoundTemporalInstant).
    Int128 epochNs = zdt->exactTime().epochNanoseconds();
    Int128 incrementNs = static_cast<Int128>(lengthInNanoseconds(precision.unit)) * static_cast<Int128>(static_cast<int64_t>(precision.increment));
    if (incrementNs > 0)
        epochNs = TemporalCore::roundNumberToIncrementAsIfPositive(epochNs, incrementNs, roundingMode);
    ISO8601::ExactTime roundedExact(epochNs);

    // Step 2: GetOffsetNanosecondsFor(outputTimeZone, roundedEpochNs).
    auto offsetOpt = TemporalCore::getOffsetNanosecondsFor(zdt->timeZone(), roundedExact);
    if (!offsetOpt) [[unlikely]] {
        throwRangeError(globalObject, scope, offsetOpt.error().message);
        return { };
    }

    // Step 3: GetISODateTimeFor(outputTimeZone, roundedEpochNs) — derive local date/time.
    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    TemporalCore::exactTimeToLocalDateAndTime(roundedExact, *offsetOpt, date, time);

    // Step 4: ISODateTimeToString(isoDateTime, calendar, precision, ~never~).
    StringBuilder sb;
    sb.append(ISO8601::temporalDateTimeToString(date, time, precision.precision));

    // Step 5: If showOffset is not ~never~, FormatDateTimeUTCOffsetRounded(offsetNs).
    if (showOffset != "never"_s) {
        int64_t offsetNs = *offsetOpt;
        int64_t offsetMinutes = offsetNs / 60'000'000'000;
        int64_t remainder = offsetNs % 60'000'000'000;
        if (remainder > 30'000'000'000 || (remainder == 30'000'000'000 && offsetNs > 0))
            offsetMinutes++;
        else if (remainder < -30'000'000'000 || (remainder == -30'000'000'000 && offsetNs < 0))
            offsetMinutes--;
        sb.append(ISO8601::formatTimeZoneOffsetString(offsetMinutes * 60'000'000'000));
    }

    // Step 6: If showTimeZone is not ~never~, append [!?timeZoneId].
    if (showTimeZone != "never"_s) {
        sb.append('[');
        if (showTimeZone == "critical"_s)
            sb.append('!');
        sb.append(zdt->timeZoneId());
        sb.append(']');
    }

    // Step 7: If showCalendar warrants it, append [!?u-ca=calendarId].
    bool appendCalendar = showCalendar == "always"_s || showCalendar == "critical"_s
        || (showCalendar == "auto"_s && !TemporalCore::calendarIsISO(calendarID));
    if (appendCalendar) {
        sb.append('[');
        if (showCalendar == "critical"_s)
            sb.append('!');
        sb.append("u-ca="_s);
        sb.append(zdt->calendarId());
        sb.append(']');
    }

    return sb.toString();
}

// temporal_rs: ZonedDateTime::to_ixdtf_string_with_provider (default precision)
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.tojson
// toJSON always uses default format (auto precision, full string), ignoring any argument.
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncToJSON, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: RequireInternalSlot.
    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.toJSON called on value that's not a ZonedDateTime"_s);

    // Step 3: Return TemporalZonedDateTimeToString(zonedDateTime, ~auto~, ~auto~, ~auto~, ~auto~).
    PrecisionData precision { { Precision::Auto, 0 }, TemporalUnit::Nanosecond, 1 };
    String result = temporalZonedDateTimeToString(globalObject, zdt, precision, RoundingMode::Trunc, /* showOffset */ "auto"_s, /* showTimeZone */ "auto"_s, /* showCalendar */ "auto"_s, zdt->calendarID());
    RETURN_IF_EXCEPTION(scope, { });
    return JSValue::encode(jsString(vm, WTF::move(result)));
}

// temporal_rs: ZonedDateTime::to_ixdtf_string_with_provider
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.tostring
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncToString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: RequireInternalSlot.
    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.toString called on value that's not a ZonedDateTime"_s);

    // Step 3: resolvedOptions = ? GetOptionsObject(options).
    JSObject* options = intlGetOptionsObject(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    // Step 4: NOTE: The following steps read options in alphabetical order.
    // Step 5: Let showCalendar be ? GetTemporalShowCalendarNameOption(resolvedOptions).
    String calendarOpt = options ? temporalShowCalendarName(globalObject, options) : "auto"_s;
    RETURN_IF_EXCEPTION(scope, { });

    // Step 6: fractionalSecondDigits (read before smallestUnit per spec alphabetical order).
    auto digits = temporalFractionalSecondDigits(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 7: Let showOffset be ? GetTemporalShowOffsetOption(resolvedOptions).
    String offsetOpt = "auto"_s;
    if (options) {
        offsetOpt = intlStringOption(globalObject, options, vm.propertyNames->offset,
            { "auto"_s, "never"_s }, "offset must be \"auto\" or \"never\""_s, "auto"_s);
        RETURN_IF_EXCEPTION(scope, { });
    }

    // Step 8: roundingMode
    auto roundingMode = temporalRoundingMode(globalObject, options, RoundingMode::Trunc);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 9: smallestUnit (read only, validate later)
    auto smallestUnitResult = temporalUnitValued(globalObject, options, vm.propertyNames->smallestUnit);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 10: Let showTimeZone be ? GetTemporalShowTimeZoneNameOption(resolvedOptions).
    String tzNameOpt = "auto"_s;
    if (options) {
        tzNameOpt = intlStringOption(globalObject, options, Identifier::fromString(vm, "timeZoneName"_s),
            { "auto"_s, "never"_s, "critical"_s }, "timeZoneName must be \"auto\", \"never\", or \"critical\""_s, "auto"_s);
        RETURN_IF_EXCEPTION(scope, { });
    }

    // Step 11: Perform ? ValidateTemporalUnitValue(smallestUnit, ~time~).
    validateTemporalUnitValue(globalObject, smallestUnitResult, UnitGroup::Time, AllowedUnit::None, "smallestUnit"_s);
    RETURN_IF_EXCEPTION(scope, { });
    std::optional<TemporalUnit> smallestUnit = std::get<std::optional<TemporalUnit>>(smallestUnitResult);
    // Step 12: If smallestUnit is ~hour~, throw a RangeError exception.
    if (smallestUnit == TemporalUnit::Hour) [[unlikely]] {
        throwRangeError(globalObject, scope, "smallestUnit cannot be \"hour\" for ZonedDateTime.toString"_s);
        return { };
    }

    // Step 13: Let precision be ToSecondsStringPrecisionRecord(smallestUnit, digits).
    auto precision = toSecondsStringPrecisionRecord(smallestUnit, digits);

    // Step 14: Return TemporalZonedDateTimeToString(zonedDateTime, precision, showCalendar, showTimeZone, showOffset, ...).
    String result = temporalZonedDateTimeToString(globalObject, zdt, precision, roundingMode,
        offsetOpt, tzNameOpt, calendarOpt, zdt->calendarID());
    RETURN_IF_EXCEPTION(scope, { });
    return JSValue::encode(jsString(vm, WTF::move(result)));
}

// https://tc39.es/proposal-temporal/#sup-temporal.zoneddatetime.prototype.tolocalestring
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncToLocaleString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: RequireInternalSlot.
    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.toLocaleString called on value that's not a ZonedDateTime"_s);

    // Step 3: dateTimeFormat = ? CreateDateTimeFormat(%Intl.DateTimeFormat%, locales, options, ~any~, ~all~, zonedDateTime.[[TimeZone]]).
    // initializeDateTimeFormat handles the timeZone conflict check and injection via toLocaleStringTimeZone.
    JSValue userOptions = callFrame->argument(1);
    auto* formatter = IntlDateTimeFormat::create(vm, globalObject->dateTimeFormatStructure());
    formatter->initializeDateTimeFormat(globalObject, callFrame->argument(0), userOptions,
        IntlDateTimeFormat::RequiredComponent::Any, IntlDateTimeFormat::Defaults::ZonedDateTime,
        zdt->timeZoneId());
    RETURN_IF_EXCEPTION(scope, { });

    // Step 4: If calendar ≠ "iso8601" and CalendarEquals(calendar, dateTimeFormat.[[Calendar]]) is false, throw RangeError.
    if (!TemporalCore::calendarIsISO(zdt->calendarID())
        && !IntlDateTimeFormat::calendarMatchesICU(zdt->calendarId(), formatter->ensureCalendar()))
        return throwVMRangeError(globalObject, scope, "ZonedDateTime calendar does not match locale calendar"_s);

    // Steps 5-6: instant = ! CreateTemporalInstant(epochNanoseconds); Return ? FormatDateTime(dateTimeFormat, instant).
    // ICU formats the "+00:00" offset timezone as "UTC" but the spec requires "GMT"; fix it up.
    const TimeZone& tz = zdt->timeZone();
    bool isZeroOffset = tz.isUTCOffset() && !tz.utcOffsetNanoseconds();
    JSValue formatted = formatter->format(globalObject, zdt->exactTime().epochMilliseconds());
    RETURN_IF_EXCEPTION(scope, { });
    if (isZeroOffset && formatted.isString()) {
        String str = asString(formatted)->value(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        size_t pos = str.find("UTC"_s);
        if (pos != WTF::notFound)
            return JSValue::encode(jsString(vm, makeString(str.substring(0, pos), "GMT"_s, str.substring(pos + 3))));
    }
    return JSValue::encode(formatted);
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.valueof
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncValueOf, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.valueOf must not be called. To compare ZonedDateTime values, use Temporal.ZonedDateTime.compare"_s);
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.epochnanoseconds
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterEpochNanoseconds, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.epochNanoseconds called on value that's not a ZonedDateTime"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(JSBigInt::createFrom(globalObject, zdt->exactTime().epochNanoseconds())));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.timezoneid
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterTimeZoneId, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.timeZoneId called on value that's not a ZonedDateTime"_s);

    return JSValue::encode(jsString(vm, zdt->timeZoneId()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.calendarid
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterCalendarId, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.calendarId called on value that's not a ZonedDateTime"_s);

    return JSValue::encode(jsString(vm, zdt->calendarId()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.offsetnanoseconds
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterOffsetNanoseconds, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.offsetNanoseconds called on value that's not a ZonedDateTime"_s);

    auto offsetOpt = zdt->getOffsetNanoseconds(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    ASSERT(offsetOpt);
    return JSValue::encode(jsNumber(static_cast<double>(*offsetOpt)));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.offset
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterOffset, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.offset called on value that's not a ZonedDateTime"_s);

    auto offsetOpt = zdt->getOffsetNanoseconds(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    ASSERT(offsetOpt);
    return JSValue::encode(jsString(vm, ISO8601::formatTimeZoneOffsetString(*offsetOpt)));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.year
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.year called on value that's not a ZonedDateTime"_s);

    // Step 3: isoDateTime = GetISODateTimeFor(timeZone, epochNanoseconds).
    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    zdt->getLocalDateAndTime(globalObject, date, time);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 4: Return ? CalendarYear(calendar, isoDateTime.[[ISODate]]).
    if (!TemporalCore::calendarIsISO(zdt->calendarID())) {
        auto result = TemporalCore::calendarYear(zdt->calendarID(), date);
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(date.year()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.month
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterMonth, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.month called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    // Step 3: isoDateTime = GetISODateTimeFor(timeZone, epochNanoseconds).
    zdt->getLocalDateAndTime(globalObject, date, time);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 4: Return ? CalendarMonth(calendar, isoDateTime.[[ISODate]]).
    if (!TemporalCore::calendarIsISO(zdt->calendarID())) {
        auto result = TemporalCore::calendarMonth(zdt->calendarID(), date);
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(date.month()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.monthcode
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterMonthCode, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.monthCode called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    // Step 3: isoDateTime = GetISODateTimeFor(timeZone, epochNanoseconds).
    zdt->getLocalDateAndTime(globalObject, date, time);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 4: Return ? CalendarMonthCode(calendar, isoDateTime.[[ISODate]]).
    if (!TemporalCore::calendarIsISO(zdt->calendarID())) {
        auto result = TemporalCore::calendarMonthCode(zdt->calendarID(), date);
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNontrivialString(vm, *result));
    }
    return JSValue::encode(jsNontrivialString(vm, ISO8601::monthCode(date.month())));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.day
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterDay, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.day called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    // Step 3: isoDateTime = GetISODateTimeFor(timeZone, epochNanoseconds).
    zdt->getLocalDateAndTime(globalObject, date, time);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 4: Return ? CalendarDay(calendar, isoDateTime.[[ISODate]]).
    if (!TemporalCore::calendarIsISO(zdt->calendarID())) {
        auto result = TemporalCore::calendarDay(zdt->calendarID(), date);
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(date.day()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.hour
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterHour, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.hour called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    // Step 3: isoDateTime = GetISODateTimeFor(timeZone, epochNanoseconds).
    zdt->getLocalDateAndTime(globalObject, date, time);
    RETURN_IF_EXCEPTION(scope, { });
    // Step 4: Return isoDateTime.[[Time]].[[Hour]].
    return JSValue::encode(jsNumber(time.hour()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.minute
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterMinute, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.minute called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    // Step 3: isoDateTime = GetISODateTimeFor(timeZone, epochNanoseconds).
    zdt->getLocalDateAndTime(globalObject, date, time);
    RETURN_IF_EXCEPTION(scope, { });
    // Step 4: Return isoDateTime.[[Time]].[[Minute]].
    return JSValue::encode(jsNumber(time.minute()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.second
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterSecond, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.second called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    // Step 3: isoDateTime = GetISODateTimeFor(timeZone, epochNanoseconds).
    zdt->getLocalDateAndTime(globalObject, date, time);
    RETURN_IF_EXCEPTION(scope, { });
    // Step 4: Return isoDateTime.[[Time]].[[Second]].
    return JSValue::encode(jsNumber(time.second()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.millisecond
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterMillisecond, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.millisecond called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    // Step 3: isoDateTime = GetISODateTimeFor(timeZone, epochNanoseconds).
    zdt->getLocalDateAndTime(globalObject, date, time);
    RETURN_IF_EXCEPTION(scope, { });
    // Step 4: Return isoDateTime.[[Time]].[[Millisecond]].
    return JSValue::encode(jsNumber(time.millisecond()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.microsecond
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterMicrosecond, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.microsecond called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    // Step 3: isoDateTime = GetISODateTimeFor(timeZone, epochNanoseconds).
    zdt->getLocalDateAndTime(globalObject, date, time);
    RETURN_IF_EXCEPTION(scope, { });
    // Step 4: Return isoDateTime.[[Time]].[[Microsecond]].
    return JSValue::encode(jsNumber(time.microsecond()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.nanosecond
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterNanosecond, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.nanosecond called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    // Step 3: isoDateTime = GetISODateTimeFor(timeZone, epochNanoseconds).
    zdt->getLocalDateAndTime(globalObject, date, time);
    RETURN_IF_EXCEPTION(scope, { });
    // Step 4: Return isoDateTime.[[Time]].[[Nanosecond]].
    return JSValue::encode(jsNumber(time.nanosecond()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.dayofweek
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterDayOfWeek, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.dayOfWeek called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    // Step 3: isoDateTime = GetISODateTimeFor(timeZone, epochNanoseconds).
    zdt->getLocalDateAndTime(globalObject, date, time);
    RETURN_IF_EXCEPTION(scope, { });
    // Step 4: Return 𝔽(CalendarISOToDate(calendar, isoDateTime.[[ISODate]]).[[DayOfWeek]]).
    // DayOfWeek is universal across all calendar systems (Mon=1 … Sun=7).
    return JSValue::encode(jsNumber(ISO8601::dayOfWeek(date)));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.dayofyear
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterDayOfYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.dayOfYear called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    // Step 3: isoDateTime = GetISODateTimeFor(timeZone, epochNanoseconds).
    zdt->getLocalDateAndTime(globalObject, date, time);
    RETURN_IF_EXCEPTION(scope, { });
    // Step 4: Return 𝔽(CalendarISOToDate(calendar, isoDateTime.[[ISODate]]).[[DayOfYear]]).
    // NOTE: for non-ISO calendars this should return the day within the calendar year, not the ISO year.
    // JSC currently returns the ISO day-of-year for all calendars (pre-existing behavior, matches PlainDate).
    return JSValue::encode(jsNumber(ISO8601::dayOfYear(date)));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.weekofyear
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterWeekOfYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.weekOfYear called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    // Step 3: isoDateTime = GetISODateTimeFor(timeZone, epochNanoseconds).
    zdt->getLocalDateAndTime(globalObject, date, time);
    RETURN_IF_EXCEPTION(scope, { });
    // Step 4: Return ? CalendarWeekOfYear(calendar, isoDateTime.[[ISODate]]). Undefined for non-ISO.
    if (!TemporalCore::calendarIsISO(zdt->calendarID()))
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsNumber(ISO8601::weekOfYear(date)));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.yearofweek
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterYearOfWeek, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.yearOfWeek called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    // Step 3: isoDateTime = GetISODateTimeFor(timeZone, epochNanoseconds).
    zdt->getLocalDateAndTime(globalObject, date, time);
    RETURN_IF_EXCEPTION(scope, { });
    // Step 4: Return ? CalendarYearOfWeek(calendar, isoDateTime.[[ISODate]]). Undefined for non-ISO.
    if (!TemporalCore::calendarIsISO(zdt->calendarID()))
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsNumber(ISO8601::yearOfWeek(date)));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.hoursinday
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterHoursInDay, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: RequireInternalSlot.
    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.hoursInDay called on value that's not a ZonedDateTime"_s);

    // Steps 3-5: timeZone, isoDateTime = GetISODateTimeFor, today = isoDateTime.[[ISODate]].
    ISO8601::PlainDate date;
    ISO8601::PlainTime plainTime;
    zdt->getLocalDateAndTime(globalObject, date, plainTime);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 6: tomorrow = AddDaysToISODate(today, 1) — simple epoch-day +1, no calendar needed.
    int32_t daysEpoch = WTF::daysFromYearMonth(date.year(), date.month() - 1) + (date.day() - 1) + 1;
    auto [y, m, d] = WTF::yearMonthDayFromDays(daysEpoch);
    ISO8601::PlainDate tomorrow(y, static_cast<uint8_t>(m + 1), static_cast<uint8_t>(d));

    // Step 7: todayNs = ? GetStartOfDay(timeZone, today).
    auto startOfDayResult = TemporalCore::getStartOfDay(zdt->timeZone(), date);
    RETURN_IF_EXCEPTION(scope, { });
    if (!startOfDayResult) [[unlikely]] {
        throwRangeError(globalObject, scope, startOfDayResult.error().message);
        return { };
    }

    // Step 8: tomorrowNs = ? GetStartOfDay(timeZone, tomorrow).
    auto startOfNextDayResult = TemporalCore::getStartOfDay(zdt->timeZone(), tomorrow);
    RETURN_IF_EXCEPTION(scope, { });
    if (!startOfNextDayResult) [[unlikely]] {
        throwRangeError(globalObject, scope, startOfNextDayResult.error().message);
        return { };
    }

    // Steps 9-10: diff = TimeDurationFromEpochNanosecondsDifference; Return TotalTimeDuration(diff, ~hour~).
    double hoursInDay = (double)(startOfNextDayResult->epochNanoseconds() - startOfDayResult->epochNanoseconds()) / (double)ISO8601::ExactTime::nsPerHour;
    return JSValue::encode(jsNumber(hoursInDay));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.daysinweek
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterDaysInWeek, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.daysInWeek called on value that's not a ZonedDateTime"_s);

    // Step 3: isoDateTime = GetISODateTimeFor(timeZone, epochNanoseconds).
    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    zdt->getLocalDateAndTime(globalObject, date, time);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 4: Return 𝔽(CalendarISOToDate(calendar, isoDate).[[DaysInWeek]]).
    // All calendar systems have a 7-day week.
    return JSValue::encode(jsNumber(7));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.daysinmonth
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterDaysInMonth, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.daysInMonth called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    // Step 3: isoDateTime = GetISODateTimeFor(timeZone, epochNanoseconds).
    zdt->getLocalDateAndTime(globalObject, date, time);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 4: Return ? CalendarDaysInMonth(calendar, isoDateTime.[[ISODate]]).
    if (!TemporalCore::calendarIsISO(zdt->calendarID())) {
        auto result = TemporalCore::calendarDaysInMonth(zdt->calendarID(), date);
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(ISO8601::daysInMonth(date.year(), date.month())));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.daysinyear
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterDaysInYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.daysInYear called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    // Step 3: isoDateTime = GetISODateTimeFor(timeZone, epochNanoseconds).
    zdt->getLocalDateAndTime(globalObject, date, time);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 4: Return ? CalendarDaysInYear(calendar, isoDateTime.[[ISODate]]).
    if (!TemporalCore::calendarIsISO(zdt->calendarID())) {
        auto result = TemporalCore::calendarDaysInYear(zdt->calendarID(), date);
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(isLeapYear(date.year()) ? 366 : 365));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.monthsinyear
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterMonthsInYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.monthsInYear called on value that's not a ZonedDateTime"_s);

    if (!TemporalCore::calendarIsISO(zdt->calendarID())) {
        // Step 3: isoDateTime = GetISODateTimeFor(timeZone, epochNanoseconds).
        ISO8601::PlainDate date;
        ISO8601::PlainTime time2;
        zdt->getLocalDateAndTime(globalObject, date, time2);
        RETURN_IF_EXCEPTION(scope, { });
        // Step 4: Return ? CalendarMonthsInYear(calendar, isoDateTime.[[ISODate]]).
        auto result = TemporalCore::calendarMonthsInYear(zdt->calendarID(), date);
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(12));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.inleapyear
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterInLeapYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.inLeapYear called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    // Step 3: isoDateTime = GetISODateTimeFor(timeZone, epochNanoseconds).
    zdt->getLocalDateAndTime(globalObject, date, time);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 4: Return ? CalendarInLeapYear(calendar, isoDateTime.[[ISODate]]).
    if (!TemporalCore::calendarIsISO(zdt->calendarID())) {
        auto result = TemporalCore::calendarInLeapYear(zdt->calendarID(), date);
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsBoolean(*result));
    }
    return JSValue::encode(jsBoolean(isLeapYear(date.year())));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.era
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterEra, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.era called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    // Step 3: isoDateTime = GetISODateTimeFor(timeZone, epochNanoseconds).
    zdt->getLocalDateAndTime(globalObject, date, time);
    RETURN_IF_EXCEPTION(scope, { });
    // Step 4: Return ? CalendarEra(calendar, isoDateTime.[[ISODate]]).
    auto result = TemporalCore::calendarEra(zdt->calendarID(), date);
    if (!result) [[unlikely]]
        return throwVMRangeError(globalObject, scope, result.error().message);
    if (!*result)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsString(vm, **result));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.erayear
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterEraYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.eraYear called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time2;
    zdt->getLocalDateAndTime(globalObject, date, time2);
    RETURN_IF_EXCEPTION(scope, { });
    auto result = TemporalCore::calendarEraYear(zdt->calendarID(), date);
    if (!result) [[unlikely]]
        return throwVMRangeError(globalObject, scope, result.error().message);
    if (!*result)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsNumber(**result));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.epochmilliseconds
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterEpochMilliseconds, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.epochMilliseconds called on value that's not a ZonedDateTime"_s);

    return JSValue::encode(jsNumber(static_cast<double>(zdt->exactTime().floorEpochMilliseconds())));
}

} // namespace JSC
