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
#include "TemporalPlainDate.h"
#include "TemporalPlainDateTime.h"
#include "TemporalPlainMonthDay.h"
#include "TemporalPlainTime.h"
#include "TemporalPlainYearMonth.h"
#include "TemporalZonedDateTime.h"
#include "TimeZoneICUBridge.h"

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

    auto* temporalDate = dynamicDowncast<TemporalPlainDate>(callFrame->thisValue());
    if (!temporalDate)
        [[unlikely]] return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.toPlainMonthDay called on value that's not a PlainDate"_s);

    if (!TemporalCore::calendarIsISO(temporalDate->calendarID())) {
        auto resolved = TemporalCore::plainMonthDayFromISODate(temporalDate->calendarID(), temporalDate->plainDate(), TemporalOverflow::Constrain);
        if (!resolved) {
            throwRangeError(globalObject, scope, String(resolved.error().message));
            return { };
        }
        auto* result = TemporalPlainMonthDay::tryCreateIfValid(globalObject, globalObject->plainMonthDayStructure(), WTF::move(resolved->isoDate));
        RETURN_IF_EXCEPTION(scope, { });
        if (result && resolved->calendarId != "iso8601"_s)
            result->setCalendarId(resolved->calendarId);
        return JSValue::encode(result);
    }

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

    auto* temporalDate = dynamicDowncast<TemporalPlainDate>(callFrame->thisValue());
    if (!temporalDate)
        [[unlikely]] return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.toPlainYearMonth called on value that's not a PlainDate"_s);

    if (!TemporalCore::calendarIsISO(temporalDate->calendarID())) {
        auto resolved = TemporalCore::plainYearMonthFromISODate(temporalDate->calendarID(), temporalDate->plainDate());
        if (!resolved) {
            throwRangeError(globalObject, scope, String(resolved.error().message));
            return { };
        }
        auto* result = TemporalPlainYearMonth::create(vm, globalObject->plainYearMonthStructure(), ISO8601::PlainYearMonth(WTF::move(resolved->isoDate)));
        if (resolved->calendarId != "iso8601"_s)
            result->setCalendarId(resolved->calendarId);
        return JSValue::encode(result);
    }

    ISO8601::PlainDate dateToUse(temporalDate->plainDate().year(), temporalDate->plainDate().month(), 1);
    auto* ymResult = TemporalPlainYearMonth::tryCreateIfValid(globalObject, globalObject->plainYearMonthStructure(), WTF::move(dateToUse));
    RETURN_IF_EXCEPTION(scope, { });
    return JSValue::encode(ymResult);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.add
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncAdd, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(callFrame->thisValue());
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.add called on value that's not a PlainDate"_s);

    auto duration = TemporalDuration::toISO8601Duration(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    TemporalOverflow overflow = toTemporalOverflow(globalObject, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    ISO8601::PlainDate result;
    if (plainDate->calendarId() != "iso8601"_s && !plainDate->calendarId().isEmpty())
        result = calendarDateAdd(globalObject, plainDate->calendarId(), plainDate->plainDate(), duration, overflow);
    else
        result = addDurationToDate(globalObject, plainDate->plainDate(), duration, overflow);
    RETURN_IF_EXCEPTION(scope, { });

    String calId = plainDate->calendarId();
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainDate::tryCreateIfValid(globalObject, globalObject->plainDateStructure(), WTF::move(result), WTF::move(calId))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.subtract
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncSubtract, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(callFrame->thisValue());
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.subtract called on value that's not a PlainDate"_s);

    auto duration = TemporalDuration::toISO8601Duration(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    TemporalOverflow overflow = toTemporalOverflow(globalObject, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    ISO8601::PlainDate result;
    if (plainDate->calendarId() != "iso8601"_s && !plainDate->calendarId().isEmpty())
        result = calendarDateAdd(globalObject, plainDate->calendarId(), plainDate->plainDate(), -duration, overflow);
    else
        result = addDurationToDate(globalObject, plainDate->plainDate(), -duration, overflow);
    RETURN_IF_EXCEPTION(scope, { });

    String calId = plainDate->calendarId();
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainDate::tryCreateIfValid(globalObject, globalObject->plainDateStructure(), WTF::move(result), WTF::move(calId))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.with
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncWith, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(callFrame->thisValue());
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.with called on value that's not a PlainDate"_s);

    JSValue temporalDateLike  = callFrame->argument(0);
    if (!temporalDateLike.isObject())
        return throwVMTypeError(globalObject, scope, "First argument to Temporal.PlainDate.prototype.with must be an object"_s);

    auto result = plainDate->with(globalObject, asObject(temporalDateLike), callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    String calId = plainDate->calendarId();
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainDate::tryCreateIfValid(globalObject, globalObject->plainDateStructure(), WTF::move(result), WTF::move(calId))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.until
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncUntil, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(callFrame->thisValue());
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.until called on value that's not a PlainDate"_s);

    auto* other = TemporalPlainDate::from(globalObject, callFrame->argument(0), TemporalOverflow::Constrain);
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

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(callFrame->thisValue());
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.since called on value that's not a PlainDate"_s);

    auto* other = TemporalPlainDate::from(globalObject, callFrame->argument(0), TemporalOverflow::Constrain);
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

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(callFrame->thisValue());
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.equals called on value that's not a PlainDate"_s);

    auto* other = TemporalPlainDate::from(globalObject, callFrame->argument(0), TemporalOverflow::Constrain);
    RETURN_IF_EXCEPTION(scope, { });

    if (plainDate->plainDate() != other->plainDate())
        return JSValue::encode(jsBoolean(false));

    // CalendarEquals: compare calendar IDs.
    auto calA = plainDate->calendarId();
    auto calB = other->calendarId();
    if (calA.isEmpty())
        calA = "iso8601"_s;
    if (calB.isEmpty())
        calB = "iso8601"_s;
    return JSValue::encode(jsBoolean(calA == calB));
}

// temporal_rs: PlainDate::to_zoned_date_time_with_provider
// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.tozoneddatetime
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncToZonedDateTime, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(callFrame->thisValue());
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.toZonedDateTime called on value that's not a PlainDate"_s);

    // Steps 1-2: RequireInternalSlot check done by dynamicDowncast above.
    // Step 3: If item is an Object, get timeZone and optional plainTime from it.
    // Step 4 (else): treat item itself as a time zone identifier.
    JSValue item = callFrame->argument(0);

    std::optional<TimeZone> tz;
    String tzString;
    ISO8601::PlainTime plainTime;
    bool hasExplicitPlainTime = false;

    if (item.isString()) {
        // String form: no explicit plainTime → use GetStartOfDay.
        tzString = asString(item)->value(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        tz = ISO8601::parseTemporalTimeZoneIdentifier(tzString);
        if (!tz) {
            throwRangeError(globalObject, scope, makeString("'"_s, ellipsizeAt(100, tzString), "' is not a valid time zone identifier"_s));
            return { };
        }
    } else if (item.isObject()) {
        JSObject* itemObj = asObject(item);

        // Step 3a: Let timeZoneLike be ? Get(item, "timeZone").
        JSValue timeZoneLike = itemObj->get(globalObject, vm.propertyNames->timeZone);
        RETURN_IF_EXCEPTION(scope, { });

        if (!timeZoneLike.isString()) {
            throwTypeError(globalObject, scope, "Temporal.PlainDate.prototype.toZonedDateTime requires timeZone to be a string"_s);
            return { };
        }
        tzString = asString(timeZoneLike)->value(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        tz = ISO8601::parseTemporalTimeZoneIdentifier(tzString);
        if (!tz) {
            throwRangeError(globalObject, scope, makeString("'"_s, ellipsizeAt(100, tzString), "' is not a valid time zone identifier"_s));
            return { };
        }

        // Step 3b: Let temporalTime be ? Get(item, "plainTime").
        // Read plainTime once — track if explicit.
        JSValue plainTimeLike = itemObj->get(globalObject, Identifier::fromString(vm, "plainTime"_s));
        RETURN_IF_EXCEPTION(scope, { });

        if (!plainTimeLike.isUndefined()) {
            hasExplicitPlainTime = true;
            auto* pt = TemporalPlainTime::from(globalObject, plainTimeLike, nullptr);
            RETURN_IF_EXCEPTION(scope, { });
            plainTime = pt->plainTime();
        }
    } else {
        throwTypeError(globalObject, scope, "Temporal.PlainDate.prototype.toZonedDateTime requires a string or object argument"_s);
        return { };
    }

    ISO8601::ExactTime epochNs;

    if (hasExplicitPlainTime) {
        // Steps 6a-d: CombineISODateAndTimeRecord, check limits, GetEpochNanosecondsFor(compatible).
        auto exactTimeOpt = TemporalZonedDateTime::getEpochNanosecondsFor(globalObject, *tz, plainDate->plainDate(), plainTime, TemporalDisambiguation::Compatible);
        RETURN_IF_EXCEPTION(scope, { });
        if (!exactTimeOpt)
            return { };
        epochNs = *exactTimeOpt;
    } else {
        // Step 5: Let epochNs be ? GetStartOfDay(timeZone, plainDate.[[ISODate]]).
        // Per spec and temporal_rs line 1125: use GetStartOfDay when plainTime is absent.
        // This handles DST gaps correctly (returns transition epoch, not compatible midnight).
        auto possible = TemporalCore::getPossibleEpochNanosecondsFor(*tz, plainDate->plainDate(), plainTime);
        if (!possible) {
            throwRangeError(globalObject, scope, possible.error().message);
            return { };
        }
        if (!TemporalCore::isGap(*possible))
            epochNs = TemporalCore::epochCandidates(*possible)[0];
        else {
            // Gap: use transition epoch (first valid instant of the day).
            auto beforeGap = TemporalCore::getEpochNanosecondsFor(*tz, plainDate->plainDate(), plainTime, TemporalDisambiguation::Earlier);
            if (!beforeGap) {
                throwRangeError(globalObject, scope, beforeGap.error().message);
                return { };
            }
            auto transition = TemporalCore::getTimeZoneTransition(*tz, *beforeGap, TransitionDirection::Next);
            if (!transition) {
                throwRangeError(globalObject, scope, transition.error().message);
                return { };
            }
            if (transition->has_value())
                epochNs = transition->value();
            else
                epochNs = *beforeGap;
        }
    }

    // Step 7: Return ! CreateTemporalZonedDateTime(epochNs, timeZone, plainDate.[[Calendar]]).
    String tzId;
    bool isOffsetString = !tzString.isEmpty() && (tzString[0] == '+' || tzString[0] == '-');
    if (isOffsetString)
        tzId = ISO8601::formatTimeZoneOffsetString(tz->utcOffsetNanoseconds());
    else if (auto namedTz = intlAvailableNamedTimeZone(tzString))
        tzId = namedTz->identifier;
    else
        tzId = tz->toString();
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalZonedDateTime::create(vm, globalObject->zonedDateTimeStructure(),
        epochNs, *tz, WTF::move(tzId), String(plainDate->calendarId()))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.toplaindatetime
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncToPlainDateTime, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(callFrame->thisValue());
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.toPlainDateTime called on value that's not a PlainDate"_s);

    JSValue itemValue = callFrame->argument(0);
    if (itemValue.isUndefined()) {
        auto* result = TemporalPlainDateTime::tryCreateIfValid(globalObject, globalObject->plainDateTimeStructure(), plainDate->plainDate(), { });
        RETURN_IF_EXCEPTION(scope, { });
        if (result && plainDate->calendarId() != "iso8601"_s && !plainDate->calendarId().isEmpty())
            result->setCalendarId(String(plainDate->calendarId()));
        return JSValue::encode(result);
    }

    auto* plainTime = TemporalPlainTime::from(globalObject, itemValue, nullptr);
    RETURN_IF_EXCEPTION(scope, { });

    auto* result = TemporalPlainDateTime::tryCreateIfValid(globalObject, globalObject->plainDateTimeStructure(), plainDate->plainDate(), plainTime->plainTime());
    RETURN_IF_EXCEPTION(scope, { });
    if (result && plainDate->calendarId() != "iso8601"_s && !plainDate->calendarId().isEmpty())
        result->setCalendarId(String(plainDate->calendarId()));
    return JSValue::encode(result);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.tostring
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncToString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(callFrame->thisValue());
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.toString called on value that's not a PlainDate"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(jsString(vm, plainDate->toString(globalObject, callFrame->argument(0)))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.tojson
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncToJSON, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(callFrame->thisValue());
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.toJSON called on value that's not a PlainDate"_s);

    return JSValue::encode(jsString(vm, plainDate->toString()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.tolocalestring
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncToLocaleString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(callFrame->thisValue());
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.toLocaleString called on value that's not a PlainDate"_s);

    auto* formatter = IntlDateTimeFormat::create(vm, globalObject->dateTimeFormatStructure());
    formatter->initializeDateTimeFormat(globalObject, callFrame->argument(0), callFrame->argument(1), IntlDateTimeFormat::RequiredComponent::Date, IntlDateTimeFormat::Defaults::Date);
    RETURN_IF_EXCEPTION(scope, { });

    if (plainDate->calendarId() != "iso8601"_s
        && !IntlDateTimeFormat::calendarMatchesICU(plainDate->calendarId(), formatter->ensureCalendar()))
        return throwVMRangeError(globalObject, scope, "Temporal calendar does not match locale calendar"_s);

    auto et = ISO8601::ExactTime::fromISOPartsAndOffset(
        plainDate->year(), plainDate->month(), plainDate->day(), 12, 0, 0, 0, 0, 0, 0);
    RELEASE_AND_RETURN(scope, JSValue::encode(formatter->format(globalObject, et.epochMilliseconds(), IntlDateTimeFormat::TemporalFieldKind::PlainDate)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.valueof
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncValueOf, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.valueOf must not be called. To compare PlainDate values, use Temporal.PlainDate.compare"_s);
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterCalendarId, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.calendarId called on value that's not a PlainDate"_s);

    return JSValue::encode(jsString(vm, plainDate->calendarId()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.year called on value that's not a PlainDate"_s);

    if (TemporalCore::calendarIDFromString(plainDate->calendarId()) != iso8601CalendarID()) {
        auto result = TemporalCore::calendarYear(TemporalCore::calendarIDFromString(plainDate->calendarId()), plainDate->plainDate());
        if (!result)
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(plainDate->year()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterMonth, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.month called on value that's not a PlainDate"_s);

    if (TemporalCore::calendarIDFromString(plainDate->calendarId()) != iso8601CalendarID()) {
        auto result = TemporalCore::calendarMonth(TemporalCore::calendarIDFromString(plainDate->calendarId()), plainDate->plainDate());
        if (!result)
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(plainDate->month()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterMonthCode, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.monthCode called on value that's not a PlainDate"_s);

    if (TemporalCore::calendarIDFromString(plainDate->calendarId()) != iso8601CalendarID()) {
        auto result = TemporalCore::calendarMonthCode(TemporalCore::calendarIDFromString(plainDate->calendarId()), plainDate->plainDate());
        if (!result)
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNontrivialString(vm, *result));
    }
    return JSValue::encode(jsNontrivialString(vm, plainDate->monthCode()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDay, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.day called on value that's not a PlainDate"_s);

    if (TemporalCore::calendarIDFromString(plainDate->calendarId()) != iso8601CalendarID()) {
        auto result = TemporalCore::calendarDay(TemporalCore::calendarIDFromString(plainDate->calendarId()), plainDate->plainDate());
        if (!result)
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(plainDate->day()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDayOfWeek, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.dayOfWeek called on value that's not a PlainDate"_s);

    return JSValue::encode(jsNumber(plainDate->dayOfWeek()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDayOfYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.dayOfYear called on value that's not a PlainDate"_s);

    return JSValue::encode(jsNumber(plainDate->dayOfYear()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterWeekOfYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.weekOfYear called on value that's not a PlainDate"_s);

    if (plainDate->calendarId() != "iso8601"_s && !plainDate->calendarId().isEmpty())
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsNumber(plainDate->weekOfYear()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDaysInWeek, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.daysInWeek called on value that's not a PlainDate"_s);

    return JSValue::encode(jsNumber(7)); // ISO8601 calendar always returns 7.
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDaysInMonth, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.daysInMonth called on value that's not a PlainDate"_s);

    if (TemporalCore::calendarIDFromString(plainDate->calendarId()) != iso8601CalendarID()) {
        auto result = TemporalCore::calendarDaysInMonth(TemporalCore::calendarIDFromString(plainDate->calendarId()), plainDate->plainDate());
        if (!result)
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(ISO8601::daysInMonth(plainDate->year(), plainDate->month())));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDaysInYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.daysInYear called on value that's not a PlainDate"_s);

    if (TemporalCore::calendarIDFromString(plainDate->calendarId()) != iso8601CalendarID()) {
        auto result = TemporalCore::calendarDaysInYear(TemporalCore::calendarIDFromString(plainDate->calendarId()), plainDate->plainDate());
        if (!result)
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(isLeapYear(plainDate->year()) ? 366 : 365));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterMonthsInYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.monthsInYear called on value that's not a PlainDate"_s);

    if (TemporalCore::calendarIDFromString(plainDate->calendarId()) != iso8601CalendarID()) {
        auto result = TemporalCore::calendarMonthsInYear(TemporalCore::calendarIDFromString(plainDate->calendarId()), plainDate->plainDate());
        if (!result)
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(12));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterInLeapYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.inLeapYear called on value that's not a PlainDate"_s);

    if (TemporalCore::calendarIDFromString(plainDate->calendarId()) != iso8601CalendarID()) {
        auto result = TemporalCore::calendarInLeapYear(TemporalCore::calendarIDFromString(plainDate->calendarId()), plainDate->plainDate());
        if (!result)
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsBoolean(*result));
    }

    return JSValue::encode(jsBoolean(isLeapYear(plainDate->year())));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterEra, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.era called on value that's not a PlainDate"_s);

    auto result = TemporalCore::calendarEra(TemporalCore::calendarIDFromString(plainDate->calendarId()), plainDate->plainDate());
    if (!result)
        return throwVMRangeError(globalObject, scope, result.error().message);
    if (!*result)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsString(vm, **result));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterEraYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.eraYear called on value that's not a PlainDate"_s);

    auto result = TemporalCore::calendarEraYear(TemporalCore::calendarIDFromString(plainDate->calendarId()), plainDate->plainDate());
    if (!result)
        return throwVMRangeError(globalObject, scope, result.error().message);
    if (!*result)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsNumber(**result));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterYearOfWeek, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.yearOfWeek called on value that's not a PlainDate"_s);

    if (plainDate->calendarId() != "iso8601"_s && !plainDate->calendarId().isEmpty())
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsNumber(plainDate->yearOfWeek()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.withcalendar
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncWithCalendar, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDate>(callFrame->thisValue().toThis(globalObject, ECMAMode::strict()));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.withCalendar called on value that's not a PlainDate"_s);

    String newCalendarId = toTemporalCalendarIdentifier(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    return JSValue::encode(TemporalPlainDate::create(vm, globalObject->plainDateStructure(), plainDate->plainDate(), WTF::move(newCalendarId)));
}

} // namespace JSC
