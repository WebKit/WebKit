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
#include "TemporalZonedDateTimePrototype.h"

#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include "TemporalDuration.h"
#include "TemporalInstant.h"
#include "TemporalPlainDate.h"
#include "TemporalPlainDateTime.h"
#include "TemporalPlainTime.h"
#include "TemporalTimeZone.h"

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
static JSC_DECLARE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncEquals);
static JSC_DECLARE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncToString);
static JSC_DECLARE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncToLocaleString);
static JSC_DECLARE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncToJSON);
static JSC_DECLARE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncValueOf);
static JSC_DECLARE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncStartOfDay);
static JSC_DECLARE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncGetTimeZoneTransition);
static JSC_DECLARE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncToInstant);
static JSC_DECLARE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncToPlainDate);
static JSC_DECLARE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncToPlainTime);
static JSC_DECLARE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncToPlainDateTime);
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterCalendarId);
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterTimeZoneId);
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
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterEpochMilliseconds);
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterEpochNanoseconds);
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
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterOffset);
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterOffsetNanoseconds);

}

#include "TemporalZonedDateTimePrototype.lut.h"

namespace JSC {

const ClassInfo TemporalZonedDateTimePrototype::s_info = { "Temporal.ZonedDateTime"_s, &Base::s_info, &plainDateTimePrototypeTable, nullptr, CREATE_METHOD_TABLE(TemporalZonedDateTimePrototype) };

/* Source for TemporalZonedDateTimePrototype.lut.h
@begin plainDateTimePrototypeTable
  with                  temporalZonedDateTimePrototypeFuncWith                  DontEnum|Function 1
  withPlainTime         temporalZonedDateTimePrototypeFuncWithPlainTime         DontEnum|Function 0
  withTimeZone          temporalZonedDateTimePrototypeFuncWithTimeZone          DontEnum|Function 1
  withCalendar          temporalZonedDateTimePrototypeFuncWithCalendar          DontEnum|Function 1
  add                   temporalZonedDateTimePrototypeFuncAdd                   DontEnum|Function 1
  subtract              temporalZonedDateTimePrototypeFuncSubtract              DontEnum|Function 1
  until                 temporalZonedDateTimePrototypeFuncUntil                 DontEnum|Function 1
  since                 temporalZonedDateTimePrototypeFuncSince                 DontEnum|Function 1
  round                 temporalZonedDateTimePrototypeFuncRound                 DontEnum|Function 1
  equals                temporalZonedDateTimePrototypeFuncEquals                DontEnum|Function 1
  toString              temporalZonedDateTimePrototypeFuncToString              DontEnum|Function 0
  toLocaleString        temporalZonedDateTimePrototypeFuncToLocaleString        DontEnum|Function 0
  toJSON                temporalZonedDateTimePrototypeFuncToJSON                DontEnum|Function 0
  valueOf               temporalZonedDateTimePrototypeFuncValueOf               DontEnum|Function 0
  startOfDay            temporalZonedDateTimePrototypeFuncStartOfDay            DontEnum|Function 0
  getTimeZoneTransition temporalZonedDateTimePrototypeFuncGetTimeZoneTransition DontEnum|Function 1
  toInstant             temporalZonedDateTimePrototypeFuncToInstant             DontEnum|Function 0
  toPlainDate           temporalZonedDateTimePrototypeFuncToPlainDate           DontEnum|Function 0
  toPlainTime           temporalZonedDateTimePrototypeFuncToPlainTime           DontEnum|Function 0
  toPlainDateTime       temporalZonedDateTimePrototypeFuncToPlainDateTime       DontEnum|Function 0
  calendarId            temporalZonedDateTimePrototypeGetterCalendarId          DontEnum|ReadOnly|CustomAccessor
  timeZoneId            temporalZonedDateTimePrototypeGetterTimeZoneId          DontEnum|ReadOnly|CustomAccessor
  year                  temporalZonedDateTimePrototypeGetterYear                DontEnum|ReadOnly|CustomAccessor
  month                 temporalZonedDateTimePrototypeGetterMonth               DontEnum|ReadOnly|CustomAccessor
  monthCode             temporalZonedDateTimePrototypeGetterMonthCode           DontEnum|ReadOnly|CustomAccessor
  day                   temporalZonedDateTimePrototypeGetterDay                 DontEnum|ReadOnly|CustomAccessor
  hour                  temporalZonedDateTimePrototypeGetterHour                DontEnum|ReadOnly|CustomAccessor
  minute                temporalZonedDateTimePrototypeGetterMinute              DontEnum|ReadOnly|CustomAccessor
  second                temporalZonedDateTimePrototypeGetterSecond              DontEnum|ReadOnly|CustomAccessor
  millisecond           temporalZonedDateTimePrototypeGetterMillisecond         DontEnum|ReadOnly|CustomAccessor
  microsecond           temporalZonedDateTimePrototypeGetterMicrosecond         DontEnum|ReadOnly|CustomAccessor
  nanosecond            temporalZonedDateTimePrototypeGetterNanosecond          DontEnum|ReadOnly|CustomAccessor
  epochMilliseconds     temporalZonedDateTimePrototypeGetterEpochMilliseconds   DontEnum|ReadOnly|CustomAccessor
  epochNanoseconds      temporalZonedDateTimePrototypeGetterEpochNanoseconds    DontEnum|ReadOnly|CustomAccessor
  dayOfWeek             temporalZonedDateTimePrototypeGetterDayOfWeek           DontEnum|ReadOnly|CustomAccessor
  dayOfYear             temporalZonedDateTimePrototypeGetterDayOfYear           DontEnum|ReadOnly|CustomAccessor
  weekOfYear            temporalZonedDateTimePrototypeGetterWeekOfYear          DontEnum|ReadOnly|CustomAccessor
  yearOfWeek            temporalZonedDateTimePrototypeGetterYearOfWeek          DontEnum|ReadOnly|CustomAccessor
  hoursInDay            temporalZonedDateTimePrototypeGetterHoursInDay          DontEnum|ReadOnly|CustomAccessor
  daysInWeek            temporalZonedDateTimePrototypeGetterDaysInWeek          DontEnum|ReadOnly|CustomAccessor
  daysInMonth           temporalZonedDateTimePrototypeGetterDaysInMonth         DontEnum|ReadOnly|CustomAccessor
  daysInYear            temporalZonedDateTimePrototypeGetterDaysInYear          DontEnum|ReadOnly|CustomAccessor
  monthsInYear          temporalZonedDateTimePrototypeGetterMonthsInYear        DontEnum|ReadOnly|CustomAccessor
  inLeapYear            temporalZonedDateTimePrototypeGetterInLeapYear          DontEnum|ReadOnly|CustomAccessor
  offset                temporalZonedDateTimePrototypeGetterOffset              DontEnum|ReadOnly|CustomAccessor
  offsetNanoseconds     temporalZonedDateTimePrototypeGetterOffsetNanoseconds   DontEnum|ReadOnly|CustomAccessor
@end
*/

TemporalZonedDateTimePrototype* TemporalZonedDateTimePrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    auto* prototype = new (NotNull, allocateCell<TemporalZonedDateTimePrototype>(vm)) TemporalZonedDateTimePrototype(vm, structure);
    prototype->finishCreation(vm, globalObject);
    return prototype;
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

static EncodedJSValue addOrSubtract(JSGlobalObject* globalObject, bool isAdd, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(callFrame->thisValue());
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.add called on value that's not a ZonedDateTime"_s);

    auto duration = TemporalDuration::toISO8601Duration(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    JSObject* options = intlGetOptionsObject(globalObject, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    TemporalZonedDateTime* result = zonedDateTime->addDurationToZonedDateTime(globalObject, isAdd, duration, options);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(result));
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.add
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncAdd, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return addOrSubtract(globalObject, true, callFrame);
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.subtract
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncSubtract, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return addOrSubtract(globalObject, false, callFrame);
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.with
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncWith, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(callFrame->thisValue());
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.with called on value that's not a ZonedDateTime"_s);

    JSValue temporalDateTimeLike  = callFrame->argument(0);
    if (!temporalDateTimeLike.isObject())
        return throwVMTypeError(globalObject, scope, "First argument to Temporal.ZonedDateTime.prototype.with must be an object"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(zonedDateTime->with(globalObject, asObject(temporalDateTimeLike), callFrame->argument(1))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.withplaintime
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncWithPlainTime, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(callFrame->thisValue());
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.withPlainTime called on value that's not a ZonedDateTime"_s);

    auto timeZone = zonedDateTime->timeZone();
    auto isoDateTime = ISO8601::getISODateTimeFor(timeZone, zonedDateTime->exactTime());

    JSValue plainTimeLike = callFrame->argument(0);
    ISO8601::ExactTime epochNs;
    if (plainTimeLike.isUndefined()) {
        epochNs = TemporalTimeZone::getStartOfDay(globalObject, timeZone, isoDateTime.date());
        RETURN_IF_EXCEPTION(scope, { });
    } else {
        auto plainTime = TemporalPlainTime::from(globalObject, plainTimeLike, std::nullopt);
        RETURN_IF_EXCEPTION(scope, { });
        auto resultISODateTime = TemporalPlainDateTime::combineISODateAndTimeRecord(isoDateTime.date(), plainTime->plainTime());
        epochNs = TemporalTimeZone::getEpochNanosecondsFor(globalObject, timeZone, resultISODateTime,
            TemporalDisambiguation::Compatible);
        RETURN_IF_EXCEPTION(scope, { });
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalZonedDateTime::tryCreateIfValid(globalObject, globalObject->zonedDateTimeStructure(), WTFMove(epochNs), WTFMove(timeZone))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.withtimezone
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncWithTimeZone, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(callFrame->thisValue());
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.withTimeZone called on value that's not a ZonedDateTime"_s);

    auto timeZone = TemporalTimeZone::toTemporalTimeZoneIdentifier(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalZonedDateTime::tryCreateIfValid(globalObject, globalObject->zonedDateTimeStructure(), zonedDateTime->exactTime(), WTFMove(timeZone))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.withcalendar
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncWithCalendar, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(callFrame->thisValue());
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.withCalendar called on value that's not a ZonedDateTime"_s);

    TemporalCalendar::toTemporalCalendarIdentifier(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    // TODO: calendars
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalZonedDateTime::tryCreateIfValid(globalObject, globalObject->zonedDateTimeStructure(), zonedDateTime->exactTime(), zonedDateTime->timeZone())));
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.until
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncUntil, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

   auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(callFrame->thisValue());
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.since called on value that's not a ZonedDateTime"_s);

    TemporalZonedDateTime* other = TemporalZonedDateTime::from(globalObject, callFrame->argument(0), std::nullopt);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalDuration::tryCreateIfValid(globalObject,
        zonedDateTime->until(globalObject, callFrame->argument(1), other))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.since
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncSince, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(callFrame->thisValue());
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.since called on value that's not a ZonedDateTime"_s);

    TemporalZonedDateTime* other = TemporalZonedDateTime::from(globalObject, callFrame->argument(0), std::nullopt);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalDuration::tryCreateIfValid(globalObject,
        zonedDateTime->since(globalObject, callFrame->argument(1), other))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.round
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncRound, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(callFrame->thisValue());
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.round called on value that's not a ZonedDateTime"_s);

    auto options = callFrame->argument(0);
    if (options.isUndefined())
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.round requires an options argument"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(zonedDateTime->round(globalObject, options)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.equals
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncEquals, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(callFrame->thisValue());
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.equals called on value that's not a ZonedDateTime"_s);

    auto* other = TemporalZonedDateTime::from(globalObject, callFrame->argument(0), std::nullopt);
    RETURN_IF_EXCEPTION(scope, { });

    if (zonedDateTime->exactTime() != other->exactTime() || zonedDateTime->timeZone() != other->timeZone())
        return JSValue::encode(jsBoolean(false));

    RELEASE_AND_RETURN(scope, JSValue::encode(jsBoolean(zonedDateTime->calendar()->equals(globalObject, other->calendar()))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.toplaindate
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncToPlainDate, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(callFrame->thisValue());
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.toPlainDate called on value that's not a ZonedDateTime"_s);

    auto isoDateTime = ISO8601::getISODateTimeFor(zonedDateTime->timeZone(), zonedDateTime->exactTime());

    return JSValue::encode(TemporalPlainDate::create(vm, globalObject->plainDateStructure(), isoDateTime.date()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.toplaintime
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncToPlainTime, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(callFrame->thisValue());
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.toPlainTime called on value that's not a ZonedDateTime"_s);

    auto isoDateTime = ISO8601::getISODateTimeFor(zonedDateTime->timeZone(), zonedDateTime->exactTime());

    return JSValue::encode(TemporalPlainTime::create(vm, globalObject->plainTimeStructure(), isoDateTime.time()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.toplaindatetime
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncToPlainDateTime, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(callFrame->thisValue());
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.toPlainDateTime called on value that's not a ZonedDateTime"_s);

    auto isoDateTime = ISO8601::getISODateTimeFor(zonedDateTime->timeZone(), zonedDateTime->exactTime());

    return JSValue::encode(TemporalPlainDateTime::create(vm, globalObject->plainDateTimeStructure(), isoDateTime.date(), isoDateTime.time()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.tostring
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncToString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(callFrame->thisValue());
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.toString called on value that's not a ZonedDateTime"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(jsString(vm, zonedDateTime->toString(globalObject, callFrame->argument(0)))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.tojson
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncToJSON, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(callFrame->thisValue());
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.toJSON called on value that's not a ZonedDateTime"_s);

    return JSValue::encode(jsString(vm, zonedDateTime->toString()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.tolocalestring
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncToLocaleString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(callFrame->thisValue());
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.toLocaleString called on value that's not a ZonedDateTime"_s);

    return JSValue::encode(jsString(vm, zonedDateTime->toString()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.valueof
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncValueOf, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.valueOf must not be called. To compare ZonedDateTime values, use Temporal.ZonedDateTime.compare"_s);
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.startofday
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncStartOfDay, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(callFrame->thisValue());
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.startOfDay called on value that's not a ZonedDateTime"_s);

    auto timeZone = zonedDateTime->timeZone();
    auto isoDateTime = ISO8601::getISODateTimeFor(timeZone, zonedDateTime->exactTime());
    auto epochNanoseconds = TemporalTimeZone::getStartOfDay(globalObject, timeZone, isoDateTime.date());
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalZonedDateTime::tryCreateIfValid(globalObject, globalObject->zonedDateTimeStructure(), WTFMove(epochNanoseconds), WTFMove(timeZone))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.gettimezonetransition
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncGetTimeZoneTransition, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(callFrame->thisValue());
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.getTimeZoneTransition called on value that's not a ZonedDateTime"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(zonedDateTime->getTimeZoneTransition(globalObject, callFrame->argument(0))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.toinstant
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimePrototypeFuncToInstant, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(callFrame->thisValue());
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.toInstant called on value that's not a ZonedDateTime"_s);

    RELEASE_AND_RETURN (scope, JSValue::encode(TemporalInstant::create(vm, globalObject->instantStructure(), zonedDateTime->exactTime())));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.calendarid
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterCalendarId, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.calendarId called on value that's not a ZonedDateTime"_s);

    // TODO: when calendars are supported, get the string ID of the calendar
    return JSValue::encode(jsString(vm, String::fromLatin1("iso8601")));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.timezoneid
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterTimeZoneId, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zdt = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zdt)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.timeZoneId called on value that's not a ZonedDateTime"_s);

    return JSValue::encode(jsString(vm, ISO8601::formatTimeZone(zdt->timeZone())));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.year
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.year called on value that's not a ZonedDateTime"_s);

    auto isoDateTime = ISO8601::getISODateTimeFor(zonedDateTime->timeZone(), zonedDateTime->exactTime());
    return JSValue::encode(jsNumber(isoDateTime.date().year()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.month
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterMonth, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.month called on value that's not a ZonedDateTime"_s);

    auto isoDateTime = ISO8601::getISODateTimeFor(zonedDateTime->timeZone(), zonedDateTime->exactTime());
    return JSValue::encode(jsNumber(isoDateTime.date().month()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.monthcode
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterMonthCode, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.monthCode called on value that's not a ZonedDateTime"_s);

    auto isoDateTime = ISO8601::getISODateTimeFor(zonedDateTime->timeZone(), zonedDateTime->exactTime());
    return JSValue::encode(jsNontrivialString(vm, ISO8601::monthCode(isoDateTime.date().month())));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.day
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterDay, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.day called on value that's not a ZonedDateTime"_s);

    auto isoDateTime = ISO8601::getISODateTimeFor(zonedDateTime->timeZone(), zonedDateTime->exactTime());
    return JSValue::encode(jsNumber(isoDateTime.date().day()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.hour
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterHour, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.hour called on value that's not a ZonedDateTime"_s);

    auto isoDateTime = ISO8601::getISODateTimeFor(zonedDateTime->timeZone(), zonedDateTime->exactTime());
    return JSValue::encode(jsNumber(isoDateTime.time().hour()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.minute
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterMinute, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.minute called on value that's not a ZonedDateTime"_s);

    auto isoDateTime = ISO8601::getISODateTimeFor(zonedDateTime->timeZone(), zonedDateTime->exactTime());
    return JSValue::encode(jsNumber(isoDateTime.time().minute()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.second
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterSecond, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.second called on value that's not a ZonedDateTime"_s);

    auto isoDateTime = ISO8601::getISODateTimeFor(zonedDateTime->timeZone(), zonedDateTime->exactTime());
    return JSValue::encode(jsNumber(isoDateTime.time().second()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.millisecond
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterMillisecond, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.millisecond called on value that's not a ZonedDateTime"_s);

    auto isoDateTime = ISO8601::getISODateTimeFor(zonedDateTime->timeZone(), zonedDateTime->exactTime());
    return JSValue::encode(jsNumber(isoDateTime.time().millisecond()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.microsecond
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterMicrosecond, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.microsecond called on value that's not a ZonedDateTime"_s);

    auto isoDateTime = ISO8601::getISODateTimeFor(zonedDateTime->timeZone(), zonedDateTime->exactTime());
    return JSValue::encode(jsNumber(isoDateTime.time().microsecond()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.nanosecond
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterNanosecond, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.nanosecond called on value that's not a ZonedDateTime"_s);

    auto isoDateTime = ISO8601::getISODateTimeFor(zonedDateTime->timeZone(), zonedDateTime->exactTime());
    return JSValue::encode(jsNumber(isoDateTime.time().nanosecond()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.epochmilliseconds
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterEpochMilliseconds, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.epochMilliseconds called on value that's not a ZonedDateTime"_s);

    auto ns = zonedDateTime->exactTime().epochNanoseconds();
    Int128 ms = ns / 1000000;
    // 4. Let ms be floor(ℝ(ns) / 10**6).
    if (ns % 1000000 && ms < 0)
        ms--;
    return JSValue::encode(jsNumber(static_cast<double>(ms)));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.epochnanoseconds
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterEpochNanoseconds, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.epochNanoseconds called on value that's not a ZonedDateTime"_s);

    return JSValue::encode(JSBigInt::createFrom(globalObject, zonedDateTime->exactTime().epochNanoseconds()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.dayofweek
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterDayOfWeek, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.dayOfWeek called on value that's not a ZonedDateTime"_s);

    auto isoDateTime = ISO8601::getISODateTimeFor(zonedDateTime->timeZone(), zonedDateTime->exactTime());
    return JSValue::encode(jsNumber(ISO8601::dayOfWeek(isoDateTime.date())));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.dayofyear
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterDayOfYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.dayOfYear called on value that's not a ZonedDateTime"_s);

    auto isoDateTime = ISO8601::getISODateTimeFor(zonedDateTime->timeZone(), zonedDateTime->exactTime());
    return JSValue::encode(jsNumber(ISO8601::dayOfYear(isoDateTime.date())));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.weekofyear
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterWeekOfYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.weekOfYear called on value that's not a ZonedDateTime"_s);

    auto isoDateTime = ISO8601::getISODateTimeFor(zonedDateTime->timeZone(), zonedDateTime->exactTime());
    return JSValue::encode(jsNumber(ISO8601::weekOfYear(isoDateTime.date())));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.yearofweek
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterYearOfWeek, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.weekOfYear called on value that's not a ZonedDateTime"_s);

    auto isoDateTime = ISO8601::getISODateTimeFor(zonedDateTime->timeZone(), zonedDateTime->exactTime());
    return JSValue::encode(jsNumber(ISO8601::weekOfYear(isoDateTime.date())));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.hoursinday
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterHoursInDay, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.hoursInDay called on value that's not a ZonedDateTime"_s);

    auto timeZone = zonedDateTime->timeZone();
    auto isoDateTime = ISO8601::getISODateTimeFor(zonedDateTime->timeZone(), zonedDateTime->exactTime());
    auto today = isoDateTime.date();
    auto tomorrow = TemporalCalendar::balanceISODate(today.year(), today.month(), today.day() + 1);
    auto todayNs = TemporalTimeZone::getStartOfDay(globalObject, timeZone, today);
    RETURN_IF_EXCEPTION(scope, { });
    auto tomorrowNs = TemporalTimeZone::getStartOfDay(globalObject, timeZone, tomorrow);
    RETURN_IF_EXCEPTION(scope, { });
    auto diff = TemporalDuration::timeDurationFromEpochNanosecondsDifference(tomorrowNs, todayNs);
    RELEASE_AND_RETURN(scope, JSValue::encode(jsNumber(TemporalDuration::totalTimeDuration(globalObject, diff, TemporalUnit::Hour))));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.daysinweek
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterDaysInWeek, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.daysInWeek called on value that's not a ZonedDateTime"_s);

    return JSValue::encode(jsNumber(7)); // ISO8601 calendar always returns 7.
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.daysinmonth
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterDaysInMonth, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.daysInMonth called on value that's not a ZonedDateTime"_s);

    auto isoDateTime = ISO8601::getISODateTimeFor(zonedDateTime->timeZone(), zonedDateTime->exactTime());
    auto isoDate = isoDateTime.date();

    return JSValue::encode(jsNumber(ISO8601::daysInMonth(isoDate.year(), isoDate.month())));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.daysinyear
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterDaysInYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.daysInYear called on value that's not a ZonedDateTime"_s);

    auto isoDateTime = ISO8601::getISODateTimeFor(zonedDateTime->timeZone(), zonedDateTime->exactTime());
    return JSValue::encode(jsNumber(isLeapYear(isoDateTime.date().year()) ? 366 : 365));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.monthsinyear
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterMonthsInYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.monthsInYear called on value that's not a ZonedDateTime"_s);

    return JSValue::encode(jsNumber(12)); // ISO8601 calendar always returns 12.
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.inleapyear
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterInLeapYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.inLeapYear called on value that's not a ZonedDateTime"_s);

    auto isoDateTime = ISO8601::getISODateTimeFor(zonedDateTime->timeZone(), zonedDateTime->exactTime());

    return JSValue::encode(jsBoolean(isLeapYear(isoDateTime.date().year())));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.offset
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterOffset, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.offset called on value that's not a ZonedDateTime"_s);

    auto offsetNanoseconds = ISO8601::getOffsetNanosecondsFor(zonedDateTime->timeZone(), zonedDateTime->exactTime().epochNanoseconds());
    return JSValue::encode(jsString(vm, ISO8601::formatUTCOffsetNanoseconds((int64_t) offsetNanoseconds)));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.offsetnanoseconds
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterOffsetNanoseconds, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.offsetNanoseconds called on value that's not a ZonedDateTime"_s);

    auto result = ISO8601::getOffsetNanosecondsFor(zonedDateTime->timeZone(), zonedDateTime->exactTime().epochNanoseconds());
    return JSValue::encode(jsNumber((double) result));
}
} // namespace JSC
