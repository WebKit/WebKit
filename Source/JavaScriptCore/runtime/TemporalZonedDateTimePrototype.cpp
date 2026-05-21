/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

// -----------------------------------------------------------------------
// Static helpers shared by the remaining ZDT methods
// -----------------------------------------------------------------------

// Convenience alias: delegate to the shared class-level implementation.
static std::optional<ISO8601::ExactTime> getEpochNanosecondsFor(
    JSGlobalObject* globalObject,
    const TimeZone& timeZone,
    const ISO8601::PlainDate& date,
    const ISO8601::PlainTime& time,
    TemporalDisambiguation disambiguation)
{
    return TemporalZonedDateTime::getEpochNanosecondsFor(globalObject, timeZone, date, time, disambiguation);
}

static std::optional<ISO8601::ExactTime> addZonedDateTime(
    JSGlobalObject* globalObject,
    ISO8601::ExactTime startEpochNs,
    const TimeZone& timeZone,
    const ISO8601::Duration& duration,
    TemporalOverflow overflow,
    CalendarID calendarId = iso8601CalendarID())
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto result = TemporalCore::addZonedDateTime(startEpochNs, timeZone, duration, overflow, calendarId);
    if (!result) {
        throwRangeError(globalObject, scope, result.error().message);
        return std::nullopt;
    }
    return *result;
}

// -----------------------------------------------------------------------
// Implemented methods: remaining T16/T17
// -----------------------------------------------------------------------

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.startofday
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncStartOfDay, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.startOfDay called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    if (!zdt->getLocalDateAndTime(globalObject, date, time))
        return { };

    auto sodResult = TemporalCore::getStartOfDay(zdt->timeZone(), date);
    RETURN_IF_EXCEPTION(scope, { });
    if (!sodResult) {
        throwRangeError(globalObject, scope, sodResult.error().message);
        return { };
    }

    auto* result = TemporalZonedDateTime::tryCreate(globalObject, globalObject->zonedDateTimeStructure(),
        *sodResult, zdt->timeZone(), String(zdt->timeZoneId()), String(zdt->calendarId()));
    RETURN_IF_EXCEPTION(scope, { });
    return JSValue::encode(result);
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.withplaintime
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncWithPlainTime, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.withPlainTime called on value that's not a ZonedDateTime"_s);

    // Parse the plain time argument: undefined → midnight; PlainTime/string/object → parse.
    ISO8601::PlainTime newTime;
    JSValue timeArg = callFrame->argument(0);
    if (!timeArg.isUndefined()) {
        auto* pt = TemporalPlainTime::from(globalObject, timeArg, nullptr);
        RETURN_IF_EXCEPTION(scope, { });
        newTime = pt->plainTime();
    }

    // Get local date from this ZDT, keep timezone/calendar.
    ISO8601::PlainDate date;
    ISO8601::PlainTime unused;
    if (!zdt->getLocalDateAndTime(globalObject, date, unused))
        return { };

    // Per temporal_rs with_plain_time_and_provider: if time is undefined, use GetStartOfDay.
    ISO8601::ExactTime resultEpochNs;
    if (timeArg.isUndefined()) {
        // GetStartOfDay: first valid instant of the day.
        auto sodResult2 = TemporalCore::getStartOfDay(zdt->timeZone(), date);
        RETURN_IF_EXCEPTION(scope, { });
        if (!sodResult2) {
            throwRangeError(globalObject, scope, sodResult2.error().message);
            return { };
        }
        resultEpochNs = *sodResult2;
    } else {
        auto epochNs = TemporalCore::getEpochNanosecondsFor(zdt->timeZone(), date, newTime, TemporalDisambiguation::Compatible);
        if (!epochNs) {
            throwRangeError(globalObject, scope, epochNs.error().message);
            return { };
        }
        resultEpochNs = *epochNs;
    }

    auto* result = TemporalZonedDateTime::tryCreate(globalObject, globalObject->zonedDateTimeStructure(),
        resultEpochNs, zdt->timeZone(), String(zdt->timeZoneId()), String(zdt->calendarId()));
    RETURN_IF_EXCEPTION(scope, { });
    return JSValue::encode(result);
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.add
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncAdd, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.add called on value that's not a ZonedDateTime"_s);

    auto duration = TemporalDuration::toISO8601Duration(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    TemporalOverflow overflow = toTemporalOverflow(globalObject, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    auto newEpochNs = addZonedDateTime(globalObject, zdt->exactTime(), zdt->timeZone(), duration, overflow, TemporalCore::calendarIDFromString(zdt->calendarId()));
    RETURN_IF_EXCEPTION(scope, { });
    if (!newEpochNs)
        return { };

    auto* result = TemporalZonedDateTime::tryCreate(globalObject, globalObject->zonedDateTimeStructure(),
        *newEpochNs, zdt->timeZone(), String(zdt->timeZoneId()), String(zdt->calendarId()));
    RETURN_IF_EXCEPTION(scope, { });
    return JSValue::encode(result);
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.subtract
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncSubtract, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.subtract called on value that's not a ZonedDateTime"_s);

    auto duration = TemporalDuration::toISO8601Duration(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    TemporalOverflow overflow = toTemporalOverflow(globalObject, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    auto newEpochNs = addZonedDateTime(globalObject, zdt->exactTime(), zdt->timeZone(), -duration, overflow, TemporalCore::calendarIDFromString(zdt->calendarId()));
    RETURN_IF_EXCEPTION(scope, { });
    if (!newEpochNs)
        return { };

    auto* result = TemporalZonedDateTime::tryCreate(globalObject, globalObject->zonedDateTimeStructure(),
        *newEpochNs, zdt->timeZone(), String(zdt->timeZoneId()), String(zdt->calendarId()));
    RETURN_IF_EXCEPTION(scope, { });
    return JSValue::encode(result);
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.until
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncUntil, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.until called on value that's not a ZonedDateTime"_s);

    auto* other = TemporalZonedDateTime::from(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    if (zdt->calendarId() != other->calendarId()
        && !(zdt->calendarId().isEmpty() && other->calendarId() == "iso8601"_s)
        && !(zdt->calendarId() == "iso8601"_s && other->calendarId().isEmpty())) {
        throwRangeError(globalObject, scope, "cannot compute difference between ZonedDateTimes with different calendars"_s);
        return { };
    }

    // Default largestUnit for ZDT is "hour".
    auto [smallestUnit, largestUnit, roundingMode, increment] = extractDifferenceOptions(
        globalObject, callFrame->argument(1), UnitGroup::DateTime, TemporalUnit::Nanosecond, TemporalUnit::Hour);
    RETURN_IF_EXCEPTION(scope, { });

    // Spec: DifferenceTemporalZonedDateTime step 8 — TimeZoneEquals check for day-or-larger.
    if (largestUnit <= TemporalUnit::Day) {
        if (!TemporalCore::timeZoneEquals(zdt->timeZoneId(), other->timeZoneId())) {
            throwRangeError(globalObject, scope, "cannot compute day-or-larger difference between ZonedDateTimes with different time zones"_s);
            return { };
        }
    }

    auto coreResult1 = TemporalCore::differenceZonedDateTimeWithRounding(zdt->exactTime(), other->exactTime(),
        zdt->timeZone(), largestUnit, smallestUnit, roundingMode, increment, TemporalCore::calendarIDFromString(zdt->calendarId()));
    if (!coreResult1) {
        throwTemporalError(globalObject, scope, coreResult1.error());
        return { };
    }
    // Spec DifferenceTemporalZonedDateTime step 9: TemporalDurationFromInternal(d, ~hour~).
    // For date-category largestUnit (year/month/week/day), the time component is expressed in hours.
    // For time-category largestUnit (hour and below), use largestUnit directly.
    TemporalUnit durationLargestUnit = (largestUnit <= TemporalUnit::Day) ? TemporalUnit::Hour : largestUnit;
    auto result = TemporalCore::temporalDurationFromInternal(*coreResult1, durationLargestUnit);

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalDuration::tryCreateIfValid(globalObject, WTF::move(result), globalObject->durationStructure())));
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.since
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncSince, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.since called on value that's not a ZonedDateTime"_s);

    auto* other = TemporalZonedDateTime::from(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    if (zdt->calendarId() != other->calendarId()
        && !(zdt->calendarId().isEmpty() && other->calendarId() == "iso8601"_s)
        && !(zdt->calendarId() == "iso8601"_s && other->calendarId().isEmpty())) {
        throwRangeError(globalObject, scope, "cannot compute difference between ZonedDateTimes with different calendars"_s);
        return { };
    }

    auto [smallestUnit, largestUnit, roundingMode, increment] = extractDifferenceOptions(
        globalObject, callFrame->argument(1), UnitGroup::DateTime, TemporalUnit::Nanosecond, TemporalUnit::Hour);
    RETURN_IF_EXCEPTION(scope, { });

    // since(other) = -until(other), with the receiver as rounding origin.
    // The rounding mode must be negated so that direction-sensitive modes (floor↔ceil,
    // halfFloor↔halfCeil) produce the correct result after the final negation.
    // Symmetric modes (halfExpand, trunc, halfTrunc, halfEven) are unchanged.
    RoundingMode negatedMode = TemporalCore::negateTemporalRoundingMode(roundingMode);

    // Spec: DifferenceTemporalZonedDateTime step 8 — TimeZoneEquals check for day-or-larger.
    if (largestUnit <= TemporalUnit::Day) {
        if (!TemporalCore::timeZoneEquals(zdt->timeZoneId(), other->timeZoneId())) {
            throwRangeError(globalObject, scope, "cannot compute day-or-larger difference between ZonedDateTimes with different time zones"_s);
            return { };
        }
    }

    auto coreResult2 = TemporalCore::differenceZonedDateTimeWithRounding(zdt->exactTime(), other->exactTime(),
        zdt->timeZone(), largestUnit, smallestUnit, negatedMode, increment, TemporalCore::calendarIDFromString(zdt->calendarId()));
    if (!coreResult2) {
        throwTemporalError(globalObject, scope, coreResult2.error());
        return { };
    }
    // Spec DifferenceTemporalZonedDateTime step 9: TemporalDurationFromInternal(d, ~hour~).
    TemporalUnit durationLargestUnit2 = (largestUnit <= TemporalUnit::Day) ? TemporalUnit::Hour : largestUnit;
    auto result = -TemporalCore::temporalDurationFromInternal(*coreResult2, durationLargestUnit2);

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalDuration::tryCreateIfValid(globalObject, WTF::move(result), globalObject->durationStructure())));
}

// temporal_rs: ZonedDateTime::round_with_provider
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.round
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncRound, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.round called on value that's not a ZonedDateTime"_s);

    JSValue optionsArg = callFrame->argument(0);
    // Spec: if roundTo is undefined → TypeError; string shorthand allowed.
    JSObject* options = nullptr;
    if (optionsArg.isString()) {
        // String shorthand: round("hour") → round({ smallestUnit: "hour" }).
        options = constructEmptyObject(globalObject->vm(), globalObject->nullPrototypeObjectStructure());
        RETURN_IF_EXCEPTION(scope, { });
        options->putDirect(vm, vm.propertyNames->smallestUnit, optionsArg);
    } else if (optionsArg.isObject())
        options = asObject(optionsArg);
    else {
        // undefined, null, bool, number, bigint, symbol → TypeError.
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.round requires a smallestUnit option string or options object"_s);
    }

    // Options must be read in spec order: roundingIncrement, roundingMode, smallestUnit.
    auto roundingIncrement = temporalRoundingIncrement(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    RoundingMode roundingMode = temporalRoundingMode(globalObject, options, RoundingMode::HalfExpand);
    RETURN_IF_EXCEPTION(scope, { });

    // smallestUnit is required.
    auto smallestUnitMaybeAuto = getTemporalUnitValuedOption(globalObject, options, vm.propertyNames->smallestUnit);
    RETURN_IF_EXCEPTION(scope, { });
    ASSERT(std::holds_alternative<std::optional<TemporalUnit>>(smallestUnitMaybeAuto));
    auto smallestOpt = std::get<std::optional<TemporalUnit>>(smallestUnitMaybeAuto);
    if (!smallestOpt) {
        throwRangeError(globalObject, scope, "ZonedDateTime.round requires a smallestUnit option"_s);
        return { };
    }
    TemporalUnit smallestUnit = smallestOpt.value();
    validateTemporalUnitValue(globalObject, smallestUnit, UnitGroup::DateTime, AllowedUnit::Day, "smallestUnit"_s);
    RETURN_IF_EXCEPTION(scope, { });
    if (smallestUnit == TemporalUnit::Year || smallestUnit == TemporalUnit::Month || smallestUnit == TemporalUnit::Week)
        return throwVMRangeError(globalObject, scope, "ZonedDateTime.round smallestUnit cannot be 'year', 'month', or 'week'"_s);

    // Validate increment for the unit.
    if (smallestUnit == TemporalUnit::Day)
        validateTemporalRoundingIncrement(globalObject, roundingIncrement, 1, Inclusivity::Inclusive);
    else {
        auto maxOpt = TemporalCore::maximumRoundingIncrement(smallestUnit);
        validateTemporalRoundingIncrement(globalObject, roundingIncrement, maxOpt ? std::optional<double>(*maxOpt) : std::nullopt, Inclusivity::Exclusive);
    }
    RETURN_IF_EXCEPTION(scope, { });

    // Spec step 13: If smallestUnit is "nanosecond" and roundingIncrement = 1,
    // return ! CreateTemporalZonedDateTime(epochNanoseconds, timeZone, calendar).
    // temporal_rs: ZonedDateTime::round_with_provider — returns self.clone() (new object with same values).
    // Must create a NEW object (test262 rounding-is-noop.js asserts result !== input).
    if (smallestUnit == TemporalUnit::Nanosecond && roundingIncrement == 1)
        RELEASE_AND_RETURN(scope, JSValue::encode(TemporalZonedDateTime::create(vm, globalObject->zonedDateTimeStructure(), zdt->exactTime(), zdt->timeZone(), String(zdt->timeZoneId()), String(zdt->calendarId()))));

    // Compute local date/time from the ZDT's epoch.
    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    if (!zdt->getLocalDateAndTime(globalObject, date, time))
        return { };

    // Compute nextDate (date + 1) — used for Day rounding and sub-day overflow detection.
    ISO8601::PlainDate nextDate;
    {
        int32_t days = WTF::daysFromYearMonth(date.year(), date.month() - 1) + (date.day() - 1) + 1;
        auto [y, m, d] = WTF::yearMonthDayFromDays(days);
        nextDate = ISO8601::PlainDate(y, static_cast<uint8_t>(m + 1), static_cast<uint8_t>(d));
    }

    Int128 epochNs = zdt->exactTime().epochNanoseconds();
    Int128 resultNs;

    if (smallestUnit == TemporalUnit::Day) {
        // temporal_rs: ZonedDateTime::round_with_provider — Day branch:
        //   start = get_start_of_day(&dateStart)?  ← propagates error if out of range
        //   end   = get_start_of_day(&dateEnd)?    ← propagates error if out of range
        //   assert thisNs ∈ [start.ns, end.ns) else ZDTOutOfDayBounds
        // getStartOfDay called only for Day case (not sub-day) — matches temporal_rs structure.
        auto startOfDayResult = TemporalCore::getStartOfDay(zdt->timeZone(), date);
        RETURN_IF_EXCEPTION(scope, { });
        if (!startOfDayResult) {
            throwRangeError(globalObject, scope, startOfDayResult.error().message);
            return { };
        }
        auto nextDayStartResult = TemporalCore::getStartOfDay(zdt->timeZone(), nextDate);
        RETURN_IF_EXCEPTION(scope, { });
        if (!nextDayStartResult) {
            throwRangeError(globalObject, scope, nextDayStartResult.error().message);
            return { };
        }
        Int128 startNs = startOfDayResult->epochNanoseconds();
        Int128 nextNs = nextDayStartResult->epochNanoseconds();
        Int128 dayLength = nextNs - startNs;
        if (!dayLength || !(epochNs >= startNs && epochNs < nextNs)) {
            throwRangeError(globalObject, scope, "Rounding result is outside the supported range of Temporal.ZonedDateTime"_s);
            return { };
        }
        Int128 offset = epochNs - startNs;
        Int128 roundedOffset = TemporalCore::roundNumberToIncrementInt128(offset, dayLength, roundingMode);
        resultNs = startNs + roundedOffset;
    } else {
        // Sub-day rounding: spec steps RoundISODateTime + InterpretISODateTimeOffset(prefer).
        Int128 unitLen = static_cast<Int128>(lengthInNanoseconds(smallestUnit));
        Int128 incrementNs = unitLen * static_cast<Int128>(static_cast<int64_t>(roundingIncrement));
        auto curOffsetResult = TemporalCore::getOffsetNanosecondsFor(zdt->timeZone(), zdt->exactTime());
        if (!curOffsetResult) {
            throwRangeError(globalObject, scope, curOffsetResult.error().message);
            return { };
        }
        // Step 1: RoundISODateTime — pure date+time rounding, no timezone.
        auto [roundedDate, roundedTime] = TemporalCore::roundISODateTime(date, time, incrementNs, smallestUnit, roundingMode);
        // Step 2: InterpretISODateTimeOffset with OffsetBehaviour::Option + Prefer — re-resolves
        // the rounded wall-clock time to an epoch instant, preferring the current UTC offset to
        // handle DST fold round-up correctly, falling back to Compatible disambiguation.
        auto epochNsResult = TemporalCore::interpretISODateTimeOffset(
            roundedDate, roundedTime, /* useStartOfDay */ false,
            OffsetBehaviour::Option, TemporalOffsetDisambiguation::Prefer,
            *curOffsetResult, /* offsetHasSubMinutePrecision */ false,
            zdt->timeZone(), TemporalDisambiguation::Compatible);
        if (!epochNsResult) {
            throwTemporalError(globalObject, scope, epochNsResult.error());
            return { };
        }
        resultNs = epochNsResult->epochNanoseconds();
    }

    auto* result = TemporalZonedDateTime::tryCreate(globalObject, globalObject->zonedDateTimeStructure(),
        ISO8601::ExactTime(resultNs), zdt->timeZone(), String(zdt->timeZoneId()), String(zdt->calendarId()));
    if (!result) {
        throwRangeError(globalObject, scope, "Rounding result is outside the supported range of Temporal.ZonedDateTime"_s);
        return { };
    }
    RETURN_IF_EXCEPTION(scope, { });
    return JSValue::encode(result);
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.gettimezonetransition
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncGetTimeZoneTransition, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.getTimeZoneTransition called on value that's not a ZonedDateTime"_s);

    // Parse direction option: accepts string shorthand or object with "direction" property.
    JSValue dirArg = callFrame->argument(0);
    String dirStr;
    if (dirArg.isString()) {
        // String shorthand: getTimeZoneTransition("next") or ("previous").
        dirStr = asString(dirArg)->value(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
    } else {
        JSObject* options = dirArg.getObject();
        if (!options) {
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
    TransitionDirection direction;
    if (dirStr == "next"_s)
        direction = TransitionDirection::Next;
    else if (dirStr == "previous"_s)
        direction = TransitionDirection::Previous;
    else
        return throwVMRangeError(globalObject, scope, "direction must be \"next\" or \"previous\""_s);

    auto transResult = TemporalCore::getTimeZoneTransition(zdt->timeZone(), zdt->exactTime(), direction);
    if (!transResult) {
        throwRangeError(globalObject, scope, transResult.error().message);
        return { };
    }

    if (!transResult->has_value())
        return JSValue::encode(jsNull());

    auto* result = TemporalZonedDateTime::tryCreate(globalObject, globalObject->zonedDateTimeStructure(),
        (*transResult).value(), zdt->timeZone(), String(zdt->timeZoneId()), String(zdt->calendarId()));
    RETURN_IF_EXCEPTION(scope, { });
    return JSValue::encode(result);
}

// temporal_rs: ZonedDateTime::with_with_provider
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.with
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncWith, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.with called on value that's not a ZonedDateTime"_s);

    // Step 3: IsPartialTemporalObject — must be a plain object, not a Temporal type.
    JSValue fieldsArg = callFrame->argument(0);
    if (!fieldsArg.isObject())
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.with requires a plain object of field overrides"_s);
    JSObject* fields = asObject(fieldsArg);
    rejectObjectWithCalendarOrTimeZone(globalObject, fields);
    RETURN_IF_EXCEPTION(scope, { });

    // Steps 7-8, 14-16: Get current ZDT local state.
    ISO8601::PlainDate curDate;
    ISO8601::PlainTime curTime;
    if (!zdt->getLocalDateAndTime(globalObject, curDate, curTime))
        return { };
    auto curOffsetNsOpt = zdt->getOffsetNanoseconds(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    ASSERT(curOffsetNsOpt);
    int64_t curOffsetNs = *curOffsetNsOpt;

    // Step 17: PrepareCalendarFields — read ALL partial fields in alphabetical order.
    // Numeric fields: ToIntegerWithTruncation (via toIntegerOrInfinity + finite check).
    // day/month: additionally must be positive (ToPositiveIntegerWithTruncation).
    // monthCode: ToString. offset: ToPrimitive(string) + type check + format validation.
    bool anyFieldSet = false;

    auto readIntField = [&](PropertyName name) -> std::optional<int32_t> {
        JSValue v = fields->get(globalObject, name);
        if (scope.exception())
            return std::nullopt;
        if (v.isUndefined())
            return std::nullopt;
        double dv = v.toIntegerOrInfinity(globalObject);
        if (scope.exception())
            return std::nullopt;
        if (!std::isfinite(dv)) {
            throwRangeError(globalObject, scope, "field value must be finite"_s);
            return std::nullopt;
        }
        anyFieldSet = true;
        return static_cast<int32_t>(dv);
    };

    auto readPositiveIntField = [&](PropertyName name) -> std::optional<int32_t> {
        JSValue v = fields->get(globalObject, name);
        if (scope.exception())
            return std::nullopt;
        if (v.isUndefined())
            return std::nullopt;
        double dv = v.toIntegerOrInfinity(globalObject);
        if (scope.exception())
            return std::nullopt;
        if (!std::isfinite(dv) || dv <= 0) {
            throwRangeError(globalObject, scope, "field value must be a positive finite integer"_s);
            return std::nullopt;
        }
        anyFieldSet = true;
        return static_cast<int32_t>(dv);
    };

    // Alphabetical: day, hour, microsecond, millisecond, minute, month, monthCode,
    //               nanosecond, offset, second, year.
    auto partialDay = readPositiveIntField(vm.propertyNames->day);
    RETURN_IF_EXCEPTION(scope, { });
    auto partialHour = readIntField(vm.propertyNames->hour);
    RETURN_IF_EXCEPTION(scope, { });
    auto partialMicrosecond = readIntField(vm.propertyNames->microsecond);
    RETURN_IF_EXCEPTION(scope, { });
    auto partialMillisecond = readIntField(vm.propertyNames->millisecond);
    RETURN_IF_EXCEPTION(scope, { });
    auto partialMinute = readIntField(vm.propertyNames->minute);
    RETURN_IF_EXCEPTION(scope, { });
    auto partialMonth = readPositiveIntField(vm.propertyNames->month);
    RETURN_IF_EXCEPTION(scope, { });

    std::optional<ParsedMonthCode> partialMonthCode;
    {
        JSValue v = fields->get(globalObject, Identifier::fromString(vm, "monthCode"_s));
        RETURN_IF_EXCEPTION(scope, { });
        if (!v.isUndefined()) {
            String mcStr = v.toWTFString(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            partialMonthCode = ISO8601::parseMonthCode(mcStr);
            if (!partialMonthCode)
                return throwVMRangeError(globalObject, scope, "Invalid monthCode property"_s);
            anyFieldSet = true;
        }
    }

    auto partialNanosecond = readIntField(vm.propertyNames->nanosecond);
    RETURN_IF_EXCEPTION(scope, { });

    std::optional<int64_t> partialOffsetNs;
    {
        JSValue v = fields->get(globalObject, vm.propertyNames->offset);
        RETURN_IF_EXCEPTION(scope, { });
        if (!v.isUndefined()) {
            JSValue prim = v.toPrimitive(globalObject, PreferString);
            RETURN_IF_EXCEPTION(scope, { });
            if (!prim.isString())
                return throwVMTypeError(globalObject, scope, "offset must be a string"_s);
            String offsetStr = asString(prim)->value(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            auto parsed = ISO8601::parseUTCOffset(offsetStr);
            if (!parsed)
                return throwVMRangeError(globalObject, scope, "invalid offset string"_s);
            partialOffsetNs = *parsed;
            anyFieldSet = true;
        }
    }

    auto partialSecond = readIntField(vm.propertyNames->second);
    RETURN_IF_EXCEPTION(scope, { });

    std::optional<int32_t> partialYear;
    {
        JSValue v = fields->get(globalObject, vm.propertyNames->year);
        RETURN_IF_EXCEPTION(scope, { });
        if (!v.isUndefined()) {
            double dv = v.toIntegerOrInfinity(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            if (!std::isfinite(dv))
                return throwVMRangeError(globalObject, scope, "year must be a finite integer"_s);
            partialYear = static_cast<int32_t>(dv);
            anyFieldSet = true;
        }
    }

    if (!anyFieldSet)
        return throwVMTypeError(globalObject, scope, "at least one Temporal field must be provided"_s);

    // Step 19: GetOptionsObject(options).
    JSObject* options = intlGetOptionsObject(globalObject, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    // Steps 20-22: Read options in order: disambiguation, offset, overflow.
    auto disambiguation = toTemporalDisambiguation(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });
    auto offsetOpt = toTemporalOffset(globalObject, options, TemporalOffsetDisambiguation::Prefer);
    RETURN_IF_EXCEPTION(scope, { });
    TemporalOverflow overflow = toTemporalOverflow(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 23: InterpretTemporalDateTimeFields — build date+time with overflow.
    // CalendarMergeFields: when monthCode is provided without month, month derives from monthCode.
    // For non-ISO calendars, use calendar coordinates as fallback values (not ISO coordinates).
    int32_t fallbackYear = curDate.year();
    unsigned fallbackMonth = curDate.month();
    unsigned fallbackDay = curDate.day();
    std::optional<ParsedMonthCode> fallbackMonthCode;
    if (!TemporalCore::calendarIsISO(TemporalCore::calendarIDFromString(zdt->calendarId()))) {
        auto calFields = TemporalCore::isoToCalendarFields(TemporalCore::calendarIDFromString(zdt->calendarId()), curDate);
        if (calFields) {
            fallbackYear = calFields->year;
            fallbackMonth = calFields->month;
            fallbackDay = calFields->day;
            if (!calFields->monthCode.isEmpty())
                fallbackMonthCode = ISO8601::parseMonthCode(calFields->monthCode);
        }
    }

    int32_t mergedYear = partialYear.value_or(fallbackYear);
    unsigned mergedMonth;
    if (partialMonth)
        mergedMonth = static_cast<unsigned>(*partialMonth);
    else if (partialMonthCode)
        mergedMonth = partialMonthCode->monthNumber;
    else
        mergedMonth = fallbackMonth;
    unsigned mergedDay = static_cast<unsigned>(partialDay.value_or(fallbackDay));

    // For non-ISO with(), if neither month nor monthCode provided by user, use fallback monthCode.
    std::optional<ParsedMonthCode> resolvedMonthCode = partialMonthCode;
    if (!partialMonth && !partialMonthCode && fallbackMonthCode)
        resolvedMonthCode = fallbackMonthCode;

    auto newDate = isoDateFromFields(globalObject, TemporalDateFormat::Date,
        mergedYear, mergedMonth, mergedDay, resolvedMonthCode, overflow, zdt->calendarId());
    RETURN_IF_EXCEPTION(scope, { });

    ISO8601::Duration timeDur { };
    timeDur.setHours(static_cast<int64_t>(partialHour.value_or(static_cast<int32_t>(curTime.hour()))));
    timeDur.setMinutes(static_cast<int64_t>(partialMinute.value_or(static_cast<int32_t>(curTime.minute()))));
    timeDur.setSeconds(static_cast<int64_t>(partialSecond.value_or(static_cast<int32_t>(curTime.second()))));
    timeDur.setMilliseconds(static_cast<int64_t>(partialMillisecond.value_or(static_cast<int32_t>(curTime.millisecond()))));
    timeDur.setMicroseconds(partialMicrosecond.value_or(static_cast<int32_t>(curTime.microsecond())));
    timeDur.setNanoseconds(partialNanosecond.value_or(static_cast<int32_t>(curTime.nanosecond())));
    auto newTime = TemporalPlainTime::regulateTime(globalObject, WTF::move(timeDur), overflow);
    RETURN_IF_EXCEPTION(scope, { });

    // Steps 24-25: InterpretISODateTimeOffset — resolve epoch nanoseconds.
    // Step 24: newOffsetNanoseconds — use provided offset OR current offset.
    // temporal_rs: ZonedDateTime::with_with_provider — offset always Some (either explicit or current).
    int64_t givenOffsetNs = partialOffsetNs.value_or(curOffsetNs);

    // Steps 24-25: InterpretISODateTimeOffset.
    // temporal_rs: ZonedDateTime::with_with_provider — is_exact=true when offset option is 'use'.
    // offset_nanos = Some(givenOffsetNs) always.
    if (offsetOpt == TemporalOffsetDisambiguation::Use) {
        Int128 naiveNs = getUTCEpochNanoseconds({ newDate, newTime });
        auto resultNs = naiveNs - Int128(givenOffsetNs);
        auto* result = TemporalZonedDateTime::tryCreate(globalObject, globalObject->zonedDateTimeStructure(),
            ISO8601::ExactTime(resultNs), zdt->timeZone(), String(zdt->timeZoneId()), String(zdt->calendarId()));
        RETURN_IF_EXCEPTION(scope, { });
        return JSValue::encode(result);
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
        if (!possible) {
            throwRangeError(globalObject, scope, possible.error().message);
            return { };
        }
        for (auto& candidate : TemporalCore::epochCandidates(*possible)) {
            auto offsetResult = TemporalCore::getOffsetNanosecondsFor(zdt->timeZone(), candidate);
            if (offsetResult && *offsetResult == givenOffsetNs) {
                auto* result = TemporalZonedDateTime::tryCreate(globalObject, globalObject->zonedDateTimeStructure(),
                    candidate, zdt->timeZone(), String(zdt->timeZoneId()), String(zdt->calendarId()));
                RETURN_IF_EXCEPTION(scope, { });
                return JSValue::encode(result);
            }
        }
        if (offsetOpt == TemporalOffsetDisambiguation::Reject) {
            throwRangeError(globalObject, scope, "ZonedDateTime.with: given offset does not match any possible instant"_s);
            return { };
        }
        // Prefer: no match found, fall through to disambiguation.
    }

    auto epochNs = getEpochNanosecondsFor(globalObject, zdt->timeZone(), newDate, newTime, disambiguation);
    RETURN_IF_EXCEPTION(scope, { });
    if (!epochNs)
        return { };
    auto* result = TemporalZonedDateTime::tryCreate(globalObject, globalObject->zonedDateTimeStructure(),
        *epochNs, zdt->timeZone(), String(zdt->timeZoneId()), String(zdt->calendarId()));
    RETURN_IF_EXCEPTION(scope, { });
    return JSValue::encode(result);
}

// -----------------------------------------------------------------------
// Implemented methods (T16/T17)
// -----------------------------------------------------------------------

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.withcalendar
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncWithCalendar, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.withCalendar called on value that's not a ZonedDateTime"_s);

    JSValue calArg = callFrame->argument(0);
    auto newCalendarId = toTemporalCalendarIdentifier(globalObject, calArg);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalZonedDateTime::create(vm, globalObject->zonedDateTimeStructure(),
        zdt->exactTime(), zdt->timeZone(), String(zdt->timeZoneId()), WTF::move(newCalendarId))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.withtimezone
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncWithTimeZone, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.withTimeZone called on value that's not a ZonedDateTime"_s);

    JSValue tzArg = callFrame->argument(0);
    if (!tzArg.isString())
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.withTimeZone requires a string"_s);
    String tzString = asString(tzArg)->value(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    auto newTZ = ISO8601::parseTemporalTimeZoneIdentifier(tzString);
    if (!newTZ)
        return throwVMRangeError(globalObject, scope, makeString("'"_s, ellipsizeAt(100, tzString), "' is not a valid time zone identifier"_s));

    String newTZId;
    if (auto namedTz = intlAvailableNamedTimeZone(tzString))
        newTZId = namedTz->identifier;
    else
        newTZId = newTZ->toString();
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalZonedDateTime::create(vm, globalObject->zonedDateTimeStructure(),
        zdt->exactTime(), *newTZ, WTF::move(newTZId), String(zdt->calendarId()))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.equals
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncEquals, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.equals called on value that's not a ZonedDateTime"_s);

    auto* other = TemporalZonedDateTime::from(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });
    if (!other)
        return JSValue::encode(jsBoolean(false));

    return JSValue::encode(jsBoolean(
        zdt->exactTime() == other->exactTime()
        && TemporalCore::timeZoneEquals(zdt->timeZoneId(), other->timeZoneId())
        && zdt->calendarId() == other->calendarId()));
}

// temporal_rs: ZonedDateTime::to_instant
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.toinstant
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncToInstant, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.toInstant called on value that's not a ZonedDateTime"_s);

    // Steps 1-2: Validate this ZonedDateTime (inherits check done by dynamicDowncast).
    // Step 3: Return ! CreateTemporalInstant(zonedDateTime.[[EpochNanoseconds]]).
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalInstant::create(vm, globalObject->instantStructure(), zdt->exactTime())));
}

// temporal_rs: ZonedDateTime::to_plain_date
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.toplaindate
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncToPlainDate, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.toPlainDate called on value that's not a ZonedDateTime"_s);

    // Step 3: Let isoDateTime be GetISODateTimeFor(timeZone, epochNanoseconds).
    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    if (!zdt->getLocalDateAndTime(globalObject, date, time))
        return { };

    // Step 4: Return ! CreateTemporalDate(isoDateTime.[[ISODate]], calendar).
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainDate::create(vm, globalObject->plainDateStructure(), WTF::move(date), String(zdt->calendarId()))));
}

// temporal_rs: ZonedDateTime::to_plain_time
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.toplaintime
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncToPlainTime, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.toPlainTime called on value that's not a ZonedDateTime"_s);

    // Step 3: Let isoDateTime be GetISODateTimeFor(timeZone, epochNanoseconds).
    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    if (!zdt->getLocalDateAndTime(globalObject, date, time))
        return { };

    // Step 4: Return ! CreateTemporalTime(isoDateTime.[[Time]]).
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainTime::create(vm, globalObject->plainTimeStructure(), WTF::move(time))));
}

// temporal_rs: ZonedDateTime::to_plain_date_time
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.toplaindatetime
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncToPlainDateTime, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.toPlainDateTime called on value that's not a ZonedDateTime"_s);

    // Step 3: Let isoDateTime be GetISODateTimeFor(timeZone, epochNanoseconds).
    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    if (!zdt->getLocalDateAndTime(globalObject, date, time))
        return { };

    // Step 4: Return ! CreateTemporalDateTime(isoDateTime, calendar).
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainDateTime::create(vm, globalObject->plainDateTimeStructure(), WTF::move(date), WTF::move(time), String(zdt->calendarId()))));
}

// Format a ZonedDateTime as an RFC 9557 string.
// Format: "YYYY-MM-DDTHH:MM:SS.sssssssss±HH:MM[timezone]"
// With options: fractionalSecondDigits, smallestUnit, roundingMode, calendarName, timeZoneName, offset.
static String zonedDateTimeToString(JSGlobalObject* globalObject, const TemporalZonedDateTime* zdt,
    const PrecisionData& precision, bool showOffset, bool showBracket, bool showCalendar)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    if (!zdt->getLocalDateAndTime(globalObject, date, time))
        return { };

    auto offsetOpt = zdt->getOffsetNanoseconds(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    ASSERT(offsetOpt);

    // Apply rounding if needed.
    if (std::get<0>(precision.precision) != Precision::Auto || precision.unit != TemporalUnit::Nanosecond) {
        auto roundedDuration = TemporalPlainTime::roundTime(time, precision.increment, precision.unit, RoundingMode::Trunc, std::nullopt);
        // Handle day overflow from rounding.
        double extraDays = roundedDuration.days();
        time = TemporalPlainTime::toPlainTime(globalObject, roundedDuration);
        RETURN_IF_EXCEPTION(scope, { });
        if (extraDays) {
            // Use WTF epoch-day arithmetic to correctly handle month-end overflow.
            int32_t daysEpoch = WTF::daysFromYearMonth(date.year(), date.month() - 1) + (date.day() - 1) + static_cast<int>(extraDays);
            auto [newYear, newMonth, newDay] = WTF::yearMonthDayFromDays(daysEpoch);
            // yearMonthDayFromDays returns month 0-indexed.
            date = ISO8601::PlainDate(newYear, static_cast<uint8_t>(newMonth + 1), static_cast<uint8_t>(newDay));
        }
    }

    StringBuilder sb;
    sb.append(ISO8601::temporalDateTimeToString(date, time, precision.precision));
    if (showOffset)
        sb.append(ISO8601::formatTimeZoneOffsetString(*offsetOpt));
    if (showBracket) {
        sb.append('[');
        sb.append(zdt->timeZoneId());
        sb.append(']');
    }
    if (showCalendar && !WTF::equalIgnoringASCIICase(StringView { zdt->calendarId() }, "iso8601"_s)) {
        sb.append("[u-ca="_s);
        sb.append(zdt->calendarId());
        sb.append(']');
    }
    return sb.toString();
}

// temporal_rs: ZonedDateTime::to_ixdtf_string_with_provider
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.tojson
// toJSON always uses default format (auto precision, full string), ignoring any argument.
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncToJSON, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.toJSON called on value that's not a ZonedDateTime"_s);

    // Steps 1-2: RequireInternalSlot check done by dynamicDowncast above.
    // Step 3: Return TemporalZonedDateTimeToString(zonedDateTime, ~auto~, ~auto~, ~auto~, ~auto~).
    // Auto precision, show offset, show bracket annotation, show non-ISO calendar.
    PrecisionData precision { { Precision::Auto, 0 }, TemporalUnit::Nanosecond, 1 };
    String result = zonedDateTimeToString(globalObject, zdt, precision, /* showOffset */ true, /* showBracket */ true, /* showCalendar */ true);
    RETURN_IF_EXCEPTION(scope, { });
    return JSValue::encode(jsString(vm, WTF::move(result)));
}

// temporal_rs: ZonedDateTime::to_ixdtf_string_with_provider
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.tostring
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncToString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.toString called on value that's not a ZonedDateTime"_s);

    JSObject* options = intlGetOptionsObject(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    // Read options in spec order: calendarName, fractionalSecondDigits, offset, roundingMode, smallestUnit, timeZoneName.

    // calendarName: "auto" (default) | "always" | "never" | "critical"
    // Default "auto" → show only if not iso8601.
    bool showCalendar = !WTF::equalIgnoringASCIICase(StringView { zdt->calendarId() }, "iso8601"_s);
    bool calendarCritical = false;
    if (options) {
        String calOpt = intlStringOption(globalObject, options, Identifier::fromString(vm, "calendarName"_s),
            { "auto"_s, "always"_s, "never"_s, "critical"_s }, "calendarName must be \"auto\", \"always\", \"never\", or \"critical\""_s, "auto"_s);
        RETURN_IF_EXCEPTION(scope, { });
        if (calOpt == "always"_s)
            showCalendar = true;
        else if (calOpt == "critical"_s) {
            showCalendar = true;
            calendarCritical = true;
        } else if (calOpt == "never"_s)
            showCalendar = false;
        // "auto": showCalendar already set from the iso8601 check above.
    }

    // Step 6: fractionalSecondDigits (read before smallestUnit per spec alphabetical order).
    auto digits = temporalFractionalSecondDigits(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 7: offset — "auto" (default) | "never"
    bool showOffset = true;
    if (options) {
        String offsetOpt = intlStringOption(globalObject, options, vm.propertyNames->offset,
            { "auto"_s, "never"_s }, "offset must be \"auto\" or \"never\""_s, "auto"_s);
        RETURN_IF_EXCEPTION(scope, { });
        if (offsetOpt == "never"_s)
            showOffset = false;
    }

    // Step 8: roundingMode
    auto roundingMode = temporalRoundingMode(globalObject, options, RoundingMode::Trunc);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 9: smallestUnit (read only, validate later)
    auto smallestUnitResult = getTemporalUnitValuedOption(globalObject, options, vm.propertyNames->smallestUnit);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 10: timeZoneName — "auto" (default) | "never" | "critical"
    bool showBracket = true;
    bool tzNameCritical = false;
    if (options) {
        String tzNameOpt = intlStringOption(globalObject, options, Identifier::fromString(vm, "timeZoneName"_s),
            { "auto"_s, "never"_s, "critical"_s }, "timeZoneName must be \"auto\", \"never\", or \"critical\""_s, "auto"_s);
        RETURN_IF_EXCEPTION(scope, { });
        if (tzNameOpt == "never"_s)
            showBracket = false;
        else if (tzNameOpt == "critical"_s)
            tzNameCritical = true;
    }

    // Steps 11-12: ValidateTemporalUnitValue(smallestUnit, time) + reject hour.
    std::optional<TemporalUnit> smallestUnit;
    if (std::holds_alternative<TemporalAuto>(smallestUnitResult)) {
        throwRangeError(globalObject, scope, "smallestUnit \"auto\" is not valid for toString"_s);
        return { };
    }
    smallestUnit = std::get<std::optional<TemporalUnit>>(smallestUnitResult);
    if (smallestUnit) {
        auto disallowed = { TemporalUnit::Year, TemporalUnit::Month, TemporalUnit::Week, TemporalUnit::Day, TemporalUnit::Hour };
        if (std::ranges::find(disallowed, *smallestUnit) != disallowed.end()) {
            throwRangeError(globalObject, scope, "smallestUnit is a disallowed unit"_s);
            return { };
        }
    }

    // Step 13: ToSecondsStringPrecisionRecord(smallestUnit, digits).
    PrecisionData precision;
    if (smallestUnit) {
        switch (*smallestUnit) {
        case TemporalUnit::Minute: precision = { { Precision::Minute, 0 }, TemporalUnit::Minute, 1 }; break;
        case TemporalUnit::Second: precision = { { Precision::Fixed, 0 }, TemporalUnit::Second, 1 }; break;
        case TemporalUnit::Millisecond: precision = { { Precision::Fixed, 3 }, TemporalUnit::Millisecond, 1 }; break;
        case TemporalUnit::Microsecond: precision = { { Precision::Fixed, 6 }, TemporalUnit::Microsecond, 1 }; break;
        case TemporalUnit::Nanosecond: precision = { { Precision::Fixed, 9 }, TemporalUnit::Nanosecond, 1 }; break;
        default: RELEASE_ASSERT_NOT_REACHED();
        }
    } else if (!digits)
        precision = { { Precision::Auto, 0 }, TemporalUnit::Nanosecond, 1 };
    else {
        auto pow10 = [](unsigned n) -> unsigned {
            unsigned r = 1;
            for (unsigned i = 0; i < n; i++)
                r *= 10;
            return r;
        };
        unsigned d = digits.value();
        if (!d)
            precision = { { Precision::Fixed, 0 }, TemporalUnit::Second, 1 };
        else if (d <= 3)
            precision = { { Precision::Fixed, d }, TemporalUnit::Millisecond, pow10(3 - d) };
        else if (d <= 6)
            precision = { { Precision::Fixed, d }, TemporalUnit::Microsecond, pow10(6 - d) };
        else
            precision = { { Precision::Fixed, d }, TemporalUnit::Nanosecond, pow10(9 - d) };
    }

    // Per temporal_rs to_ixdtf_string_with_provider:
    // 1. round_instant (round epoch nanoseconds)
    // 2. get_offset_nanos_for(rounded_instant) — recompute offset
    // 3. get_iso_datetime_for(rounded_instant) — recompute local datetime
    // This correctly handles DST gaps (e.g. rounded 02:00 → 03:00-07:00 in spring-forward).
    Int128 epochNs = zdt->exactTime().epochNanoseconds();
    // Round epoch nanoseconds using temporal_rs round_as_if_positive semantics.
    {
        Int128 incrementNs = static_cast<Int128>(lengthInNanoseconds(precision.unit)) * static_cast<Int128>(static_cast<int64_t>(precision.increment));
        if (incrementNs > 0) {
            // div_euclid: floor division (rounds towards -infinity for negative numbers).
            auto divEuclid = [](Int128 a, Int128 b) -> Int128 {
                Int128 q = a / b;
                Int128 r = a % b;
                return r < 0 ? q - 1 : q;
            };
            auto absI128 = [](Int128 a) -> Int128 {
                return a < 0 ? -a : a;
            };
            // round_as_if_positive: getUnsignedRoundingMode(mode, positive=true).
            auto unsignedMode = getUnsignedRoundingMode(roundingMode, /* isNegative = */ false);
            Int128 r1 = divEuclid(epochNs, incrementNs); // floor quotient
            Int128 r2 = r1 + 1;
            Int128 remainder = epochNs - r1 * incrementNs; // always [0, incrementNs)
            Int128 rounded;
            if (!remainder)
                rounded = r1;
            else if (unsignedMode == UnsignedRoundingMode::Zero)
                rounded = r1;
            else if (unsignedMode == UnsignedRoundingMode::Infinity)
                rounded = r2;
            else {
                Int128 twice = absI128(remainder * 2);
                if (twice < incrementNs)
                    rounded = r1;
                else if (twice > incrementNs)
                    rounded = r2;
                else if (unsignedMode == UnsignedRoundingMode::HalfZero)
                    rounded = r1;
                else if (unsignedMode == UnsignedRoundingMode::HalfInfinity)
                    rounded = r2;
                else // HalfEven
                    rounded = (!(r1 % 2)) ? r1 : r2;
            }
            epochNs = rounded * incrementNs;
        }
    }
    ISO8601::ExactTime roundedExact(epochNs);

    // Recompute local date/time and offset from the rounded epoch.
    auto roundedOffsetOpt = TemporalCore::getOffsetNanosecondsFor(zdt->timeZone(), roundedExact);
    if (!roundedOffsetOpt) {
        throwRangeError(globalObject, scope, roundedOffsetOpt.error().message);
        return { };
    }
    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    TemporalCore::exactTimeToLocalDateAndTime(roundedExact, *roundedOffsetOpt, date, time);
    int64_t offsetNsForFormat = *roundedOffsetOpt;

    StringBuilder sb;
    sb.append(ISO8601::temporalDateTimeToString(date, time, precision.precision));
    if (showOffset) {
        // Spec: FormatDateTimeUTCOffsetRounded — round offset to ±HH:MM (no sub-minute).
        int64_t offsetNs = offsetNsForFormat;
        // Round to nearest minute (toward zero for negative, away from zero for positive).
        int64_t offsetMinutes = offsetNs / 60'000'000'000;
        int64_t remainder = offsetNs % 60'000'000'000;
        if (remainder > 30'000'000'000 || (remainder == 30'000'000'000 && offsetNs > 0))
            offsetMinutes++;
        else if (remainder < -30'000'000'000 || (remainder == -30'000'000'000 && offsetNs < 0))
            offsetMinutes--;
        sb.append(ISO8601::formatTimeZoneOffsetString(offsetMinutes * 60'000'000'000));
    }
    if (showBracket) {
        sb.append('[');
        if (tzNameCritical)
            sb.append('!');
        sb.append(zdt->timeZoneId());
        sb.append(']');
    }
    if (showCalendar) {
        sb.append('[');
        if (calendarCritical)
            sb.append('!');
        sb.append("u-ca="_s);
        sb.append(zdt->calendarId());
        sb.append(']');
    }

    return JSValue::encode(jsString(vm, sb.toString()));
}

// temporal_rs: ZonedDateTime::to_locale_string (ECMA-402 path)
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.tolocalestring
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncToLocaleString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(callFrame->thisValue());
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.toLocaleString called on value that's not a ZonedDateTime"_s);

    // Steps 1-2: RequireInternalSlot check done by dynamicDowncast above.
    // Step 3 (non-ECMA-402 fallback): Return TemporalZonedDateTimeToString(zonedDateTime, ~auto~, ~auto~, ~auto~, ~auto~).
    // Full ECMA-402 path: format via Intl.DateTimeFormat with the ZDT's time zone injected.
    JSValue userOptions = callFrame->argument(1);

    // If user provided timeZone option, throw TypeError per spec.
    if (userOptions.isObject()) {
        JSValue tzOpt = asObject(userOptions)->get(globalObject, vm.propertyNames->timeZone);
        RETURN_IF_EXCEPTION(scope, { });
        if (!tzOpt.isUndefined())
            return throwVMTypeError(globalObject, scope, "ZonedDateTime.toLocaleString does not accept a timeZone option; the ZonedDateTime's time zone is used"_s);
    }

    // Create options with ZDT's timezone merged in.
    JSObject* mergedOptions = constructEmptyObject(globalObject);
    if (userOptions.isObject()) {
        JSObject* userObj = asObject(userOptions);
        PropertyNameArrayBuilder propertyNames(vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude);
        userObj->methodTable()->getOwnPropertyNames(userObj, globalObject, propertyNames, DontEnumPropertiesMode::Include);
        RETURN_IF_EXCEPTION(scope, { });
        for (size_t i = 0; i < propertyNames.size(); i++) {
            const auto& name = propertyNames[i];
            JSValue val = userObj->get(globalObject, name);
            RETURN_IF_EXCEPTION(scope, { });
            mergedOptions->putDirect(vm, name, val);
        }
    }
    // Pass the timezone to Intl. For offset timezones (timeZoneId starts with +/-),
    // pass the original offset string directly (Intl accepts +HH:MM format).
    mergedOptions->putDirect(vm, Identifier::fromString(vm, "timeZone"_s), jsString(vm, zdt->timeZoneId()));

    auto* formatter = IntlDateTimeFormat::create(vm, globalObject->dateTimeFormatStructure());
    formatter->initializeDateTimeFormat(globalObject, callFrame->argument(0), mergedOptions, IntlDateTimeFormat::RequiredComponent::Any, IntlDateTimeFormat::Defaults::All);
    RETURN_IF_EXCEPTION(scope, { });

    // Calendar validation: non-ISO calendar must match formatter's calendar.
    if (zdt->calendarId() != "iso8601"_s
        && !IntlDateTimeFormat::calendarMatchesICU(zdt->calendarId(), formatter->ensureCalendar()))
        return throwVMRangeError(globalObject, scope, "ZonedDateTime calendar does not match locale calendar"_s);

    // For +00:00 offset timezone: ICU formats it as "UTC" (same as named UTC),
    // but temporal_rs shows "GMT" for offset zero. Now that constructor preserves
    // "+00:00" vs "UTC" distinction, we can safely replace "UTC"→"GMT" only for offsets.
    bool isZeroOffset = (zdt->timeZoneId() == "+00:00"_s || zdt->timeZoneId() == "-00:00"_s);
    JSValue formatted = formatter->format(globalObject, zdt->exactTime().epochMilliseconds(), IntlDateTimeFormat::TemporalFieldKind::ZonedDateTime);
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

// temporal_rs: (no Rust equivalent — always throws TypeError)
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.valueof
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncValueOf, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    // Step 1: Throw a TypeError exception.
    return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.valueOf must not be called. To compare ZonedDateTime values, use Temporal.ZonedDateTime.compare"_s);
}

// -----------------------------------------------------------------------
// Property getters
// -----------------------------------------------------------------------

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.epochnanoseconds
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterEpochNanoseconds, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.epochNanoseconds called on value that's not a ZonedDateTime"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(JSBigInt::createFrom(globalObject, zdt->exactTime().epochNanoseconds())));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.timezoneid
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterTimeZoneId, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.timeZoneId called on value that's not a ZonedDateTime"_s);

    return JSValue::encode(jsString(vm, zdt->timeZoneId()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.calendarid
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterCalendarId, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.calendarId called on value that's not a ZonedDateTime"_s);

    return JSValue::encode(jsString(vm, zdt->calendarId()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.offsetnanoseconds
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterOffsetNanoseconds, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt)
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
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.offset called on value that's not a ZonedDateTime"_s);

    auto offsetOpt = zdt->getOffsetNanoseconds(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    ASSERT(offsetOpt);
    return JSValue::encode(jsString(vm, ISO8601::formatTimeZoneOffsetString(*offsetOpt)));
}

// Local date/time field getters — all compute (PlainDate, PlainTime) from ExactTime+TimeZone.

JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.year called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    if (!zdt->getLocalDateAndTime(globalObject, date, time))
        return { };

    if (zdt->calendarId() != "iso8601"_s) {
        auto result = TemporalCore::calendarYear(TemporalCore::calendarIDFromString(zdt->calendarId()), date);
        if (!result)
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(date.year()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterMonth, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.month called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    if (!zdt->getLocalDateAndTime(globalObject, date, time))
        return { };

    if (zdt->calendarId() != "iso8601"_s) {
        auto result = TemporalCore::calendarMonth(TemporalCore::calendarIDFromString(zdt->calendarId()), date);
        if (!result)
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(date.month()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterMonthCode, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.monthCode called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    if (!zdt->getLocalDateAndTime(globalObject, date, time))
        return { };

    if (zdt->calendarId() != "iso8601"_s) {
        auto result = TemporalCore::calendarMonthCode(TemporalCore::calendarIDFromString(zdt->calendarId()), date);
        if (!result)
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNontrivialString(vm, *result));
    }
    return JSValue::encode(jsNontrivialString(vm, ISO8601::monthCode(date.month())));
}

JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterDay, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.day called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    if (!zdt->getLocalDateAndTime(globalObject, date, time))
        return { };

    if (zdt->calendarId() != "iso8601"_s) {
        auto result = TemporalCore::calendarDay(TemporalCore::calendarIDFromString(zdt->calendarId()), date);
        if (!result)
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(date.day()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterHour, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.hour called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    if (!zdt->getLocalDateAndTime(globalObject, date, time))
        return { };

    return JSValue::encode(jsNumber(time.hour()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterMinute, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.minute called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    if (!zdt->getLocalDateAndTime(globalObject, date, time))
        return { };

    return JSValue::encode(jsNumber(time.minute()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterSecond, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.second called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    if (!zdt->getLocalDateAndTime(globalObject, date, time))
        return { };

    return JSValue::encode(jsNumber(time.second()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterMillisecond, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.millisecond called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    if (!zdt->getLocalDateAndTime(globalObject, date, time))
        return { };

    return JSValue::encode(jsNumber(time.millisecond()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterMicrosecond, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.microsecond called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    if (!zdt->getLocalDateAndTime(globalObject, date, time))
        return { };

    return JSValue::encode(jsNumber(time.microsecond()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterNanosecond, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.nanosecond called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    if (!zdt->getLocalDateAndTime(globalObject, date, time))
        return { };

    return JSValue::encode(jsNumber(time.nanosecond()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.dayofweek
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterDayOfWeek, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.dayOfWeek called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    if (!zdt->getLocalDateAndTime(globalObject, date, time))
        return { };

    return JSValue::encode(jsNumber(ISO8601::dayOfWeek(date)));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.dayofyear
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterDayOfYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.dayOfYear called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    if (!zdt->getLocalDateAndTime(globalObject, date, time))
        return { };

    return JSValue::encode(jsNumber(ISO8601::dayOfYear(date)));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.weekofyear
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterWeekOfYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.weekOfYear called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    if (!zdt->getLocalDateAndTime(globalObject, date, time))
        return { };

    if (zdt->calendarId() != "iso8601"_s && !zdt->calendarId().isEmpty())
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsNumber(ISO8601::weekOfYear(date)));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.yearofweek
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterYearOfWeek, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.yearOfWeek called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    if (!zdt->getLocalDateAndTime(globalObject, date, time))
        return { };

    if (zdt->calendarId() != "iso8601"_s && !zdt->calendarId().isEmpty())
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsNumber(ISO8601::yearOfWeek(date)));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.hoursinday
// Returns the number of hours in the local calendar day of this ZDT (23, 24, or 25 due to DST).
// Algorithm: compute the UTC epoch for local midnight today and local midnight tomorrow, then
// return (tomorrowMidnightEpoch - todayMidnightEpoch) / nsPerHour.
// For UTC-offset timezones there are no DST transitions, so the result is always 24.
// For named timezones, we use the offset delta over a 24h UTC window starting from the
// approximate local midnight, which correctly accounts for DST transitions.
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterHoursInDay, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.hoursInDay called on value that's not a ZonedDateTime"_s);

    // Get the current local date to find start-of-day instants.
    ISO8601::PlainDate date;
    ISO8601::PlainTime plainTime;
    if (!zdt->getLocalDateAndTime(globalObject, date, plainTime))
        return { };

    // https://tc39.es/proposal-temporal/#sec-temporal-gethoursinday
    // Uses GetStartOfDay: for gaps, find transition epoch (not compatible).
    auto startOfDayResult = TemporalCore::getStartOfDay(zdt->timeZone(), date);
    RETURN_IF_EXCEPTION(scope, { });
    if (!startOfDayResult) {
        throwRangeError(globalObject, scope, startOfDayResult.error().message);
        return { };
    }
    auto startOfDay = *startOfDayResult;

    auto nextDateResult = TemporalCore::isoDateAdd(date, ISO8601::Duration { 0LL, 0LL, 0LL, 1LL, 0LL, 0LL, 0LL, 0LL, Int128(0), Int128(0) }, TemporalOverflow::Constrain);
    if (!nextDateResult) {
        throwRangeError(globalObject, scope, nextDateResult.error().message);
        return { };
    }
    auto startOfNextDayResult = TemporalCore::getStartOfDay(zdt->timeZone(), *nextDateResult);
    RETURN_IF_EXCEPTION(scope, { });
    if (!startOfNextDayResult) {
        throwRangeError(globalObject, scope, startOfNextDayResult.error().message);
        return { };
    }
    auto startOfNextDay = *startOfNextDayResult;

    // hoursInDay = (startOfNextDay.epochNs - startOfDay.epochNs) / nsPerHour
    constexpr double nsPerHour = 3'600'000'000'000.0;
    double hoursInDay = (double)(startOfNextDay.epochNanoseconds() - startOfDay.epochNanoseconds()) / nsPerHour;
    return JSValue::encode(jsNumber(hoursInDay));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.daysinweek
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterDaysInWeek, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.daysInWeek called on value that's not a ZonedDateTime"_s);

    return JSValue::encode(jsNumber(7)); // ISO8601 calendar always returns 7.
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.daysinmonth
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterDaysInMonth, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = dynamicDowncast<TemporalZonedDateTime>(JSValue::decode(thisValue));
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.daysInMonth called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    if (!zdt->getLocalDateAndTime(globalObject, date, time))
        return { };

    if (zdt->calendarId() != "iso8601"_s) {
        auto result = TemporalCore::calendarDaysInMonth(TemporalCore::calendarIDFromString(zdt->calendarId()), date);
        if (!result)
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
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.daysInYear called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    if (!zdt->getLocalDateAndTime(globalObject, date, time))
        return { };

    if (zdt->calendarId() != "iso8601"_s) {
        auto result = TemporalCore::calendarDaysInYear(TemporalCore::calendarIDFromString(zdt->calendarId()), date);
        if (!result)
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
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.monthsInYear called on value that's not a ZonedDateTime"_s);

    if (zdt->calendarId() != "iso8601"_s) {
        ISO8601::PlainDate date;
        ISO8601::PlainTime time2;
        if (!zdt->getLocalDateAndTime(globalObject, date, time2))
            return { };
        auto result = TemporalCore::calendarMonthsInYear(TemporalCore::calendarIDFromString(zdt->calendarId()), date);
        if (!result)
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
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.inLeapYear called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    if (!zdt->getLocalDateAndTime(globalObject, date, time))
        return { };

    if (zdt->calendarId() != "iso8601"_s) {
        auto result = TemporalCore::calendarInLeapYear(TemporalCore::calendarIDFromString(zdt->calendarId()), date);
        if (!result)
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
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.era called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    if (!zdt->getLocalDateAndTime(globalObject, date, time))
        return { };
    auto result = TemporalCore::calendarEra(TemporalCore::calendarIDFromString(zdt->calendarId()), date);
    if (!result)
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
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.eraYear called on value that's not a ZonedDateTime"_s);

    ISO8601::PlainDate date;
    ISO8601::PlainTime time2;
    if (!zdt->getLocalDateAndTime(globalObject, date, time2))
        return { };
    auto result = TemporalCore::calendarEraYear(TemporalCore::calendarIDFromString(zdt->calendarId()), date);
    if (!result)
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
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.epochMilliseconds called on value that's not a ZonedDateTime"_s);

    return JSValue::encode(jsNumber(static_cast<double>(zdt->exactTime().floorEpochMilliseconds())));
}

} // namespace JSC
