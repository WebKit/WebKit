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
#include "TemporalPlainDateTimePrototype.h"

#include "CalendarICUBridge.h"
#include "IntlDateTimeFormat.h"
#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include "Rounding.h"
#include "TemporalCalendar.h"
#include "TemporalDuration.h"
#include "TemporalObject.h"
#include "TemporalPlainDate.h"
#include "TemporalPlainDateTime.h"
#include "TemporalPlainTime.h"
#include "TemporalZonedDateTime.h"
#include <wtf/text/MakeString.h>

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncAdd);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncSubtract);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncUntil);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncSince);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncWith);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncWithCalendar);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncWithPlainTime);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncRound);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncEquals);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncToPlainDate);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncToPlainTime);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncToZonedDateTime);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncToString);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncToJSON);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncToLocaleString);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncValueOf);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterCalendarId);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterMonth);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterMonthCode);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterDay);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterHour);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterMinute);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterSecond);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterMillisecond);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterMicrosecond);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterNanosecond);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterDayOfWeek);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterDayOfYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterWeekOfYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterYearOfWeek);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterDaysInWeek);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterDaysInMonth);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterDaysInYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterMonthsInYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterInLeapYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterEra);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterEraYear);

}

#include "TemporalPlainDateTimePrototype.lut.h"

namespace JSC {

const ClassInfo TemporalPlainDateTimePrototype::s_info = { "Temporal.PlainDateTime"_s, &Base::s_info, &plainDateTimePrototypeTable, nullptr, CREATE_METHOD_TABLE(TemporalPlainDateTimePrototype) };

/* Source for TemporalPlainDateTimePrototype.lut.h
@begin plainDateTimePrototypeTable
  add              temporalPlainDateTimePrototypeFuncAdd                DontEnum|Function 1
  subtract         temporalPlainDateTimePrototypeFuncSubtract           DontEnum|Function 1
  until            temporalPlainDateTimePrototypeFuncUntil              DontEnum|Function 1
  since            temporalPlainDateTimePrototypeFuncSince              DontEnum|Function 1
  with             temporalPlainDateTimePrototypeFuncWith               DontEnum|Function 1
  withCalendar     temporalPlainDateTimePrototypeFuncWithCalendar       DontEnum|Function 1
  withPlainTime    temporalPlainDateTimePrototypeFuncWithPlainTime      DontEnum|Function 0
  round            temporalPlainDateTimePrototypeFuncRound              DontEnum|Function 1
  equals           temporalPlainDateTimePrototypeFuncEquals             DontEnum|Function 1
  toPlainDate      temporalPlainDateTimePrototypeFuncToPlainDate        DontEnum|Function 0
  toPlainTime      temporalPlainDateTimePrototypeFuncToPlainTime        DontEnum|Function 0
  toZonedDateTime  temporalPlainDateTimePrototypeFuncToZonedDateTime    DontEnum|Function 1
  toString         temporalPlainDateTimePrototypeFuncToString           DontEnum|Function 0
  toJSON           temporalPlainDateTimePrototypeFuncToJSON             DontEnum|Function 0
  toLocaleString   temporalPlainDateTimePrototypeFuncToLocaleString     DontEnum|Function 0
  valueOf          temporalPlainDateTimePrototypeFuncValueOf            DontEnum|Function 0
  calendarId       temporalPlainDateTimePrototypeGetterCalendarId       DontEnum|ReadOnly|CustomAccessor
  year             temporalPlainDateTimePrototypeGetterYear             DontEnum|ReadOnly|CustomAccessor
  month            temporalPlainDateTimePrototypeGetterMonth            DontEnum|ReadOnly|CustomAccessor
  monthCode        temporalPlainDateTimePrototypeGetterMonthCode        DontEnum|ReadOnly|CustomAccessor
  day              temporalPlainDateTimePrototypeGetterDay              DontEnum|ReadOnly|CustomAccessor
  hour             temporalPlainDateTimePrototypeGetterHour             DontEnum|ReadOnly|CustomAccessor
  minute           temporalPlainDateTimePrototypeGetterMinute           DontEnum|ReadOnly|CustomAccessor
  second           temporalPlainDateTimePrototypeGetterSecond           DontEnum|ReadOnly|CustomAccessor
  millisecond      temporalPlainDateTimePrototypeGetterMillisecond      DontEnum|ReadOnly|CustomAccessor
  microsecond      temporalPlainDateTimePrototypeGetterMicrosecond      DontEnum|ReadOnly|CustomAccessor
  nanosecond       temporalPlainDateTimePrototypeGetterNanosecond       DontEnum|ReadOnly|CustomAccessor
  dayOfWeek        temporalPlainDateTimePrototypeGetterDayOfWeek        DontEnum|ReadOnly|CustomAccessor
  dayOfYear        temporalPlainDateTimePrototypeGetterDayOfYear        DontEnum|ReadOnly|CustomAccessor
  weekOfYear       temporalPlainDateTimePrototypeGetterWeekOfYear       DontEnum|ReadOnly|CustomAccessor
  yearOfWeek       temporalPlainDateTimePrototypeGetterYearOfWeek       DontEnum|ReadOnly|CustomAccessor
  daysInWeek       temporalPlainDateTimePrototypeGetterDaysInWeek       DontEnum|ReadOnly|CustomAccessor
  daysInMonth      temporalPlainDateTimePrototypeGetterDaysInMonth      DontEnum|ReadOnly|CustomAccessor
  daysInYear       temporalPlainDateTimePrototypeGetterDaysInYear       DontEnum|ReadOnly|CustomAccessor
  monthsInYear     temporalPlainDateTimePrototypeGetterMonthsInYear     DontEnum|ReadOnly|CustomAccessor
  inLeapYear       temporalPlainDateTimePrototypeGetterInLeapYear       DontEnum|ReadOnly|CustomAccessor
  era              temporalPlainDateTimePrototypeGetterEra              DontEnum|ReadOnly|CustomAccessor
  eraYear          temporalPlainDateTimePrototypeGetterEraYear          DontEnum|ReadOnly|CustomAccessor
@end
*/

TemporalPlainDateTimePrototype* TemporalPlainDateTimePrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    auto* prototype = new (NotNull, allocateCell<TemporalPlainDateTimePrototype>(vm)) TemporalPlainDateTimePrototype(vm, structure);
    prototype->finishCreation(vm, globalObject);
    return prototype;
}

Structure* TemporalPlainDateTimePrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalPlainDateTimePrototype::TemporalPlainDateTimePrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void TemporalPlainDateTimePrototype::finishCreation(VM& vm, JSGlobalObject*)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

// https://tc39.es/proposal-temporal/#sec-temporal-adddurationtodatetime
// Caller handles steps 1 (ToTemporalDuration) and 2 (negate if subtract).
static EncodedJSValue addDurationToPlainDateTime(JSGlobalObject* globalObject, ThrowScope& scope, TemporalPlainDateTime* plainDateTime, ISO8601::Duration duration, JSValue optionsArg)
{
    // Steps 3-4: GetOptionsObject + GetTemporalOverflowOption.
    JSObject* options = intlGetOptionsObject(globalObject, optionsArg);
    RETURN_IF_EXCEPTION(scope, { });

    // Steps 5-6: ToInternalDurationRecordWith24HourDays + AddTime(dateTime.[[Time]], duration).
    auto balancedTimeDuration = TemporalPlainTime::addTime(plainDateTime->plainTime(), duration);
    auto plainTime = TemporalPlainTime::toPlainTime(globalObject, balancedTimeDuration);
    RETURN_IF_EXCEPTION(scope, { });

    TemporalOverflow overflow = toTemporalOverflow(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 7: AdjustDateDurationRecord — add carried days from AddTime into date duration.
    ISO8601::Duration dateDuration { duration.years(), duration.months(), duration.weeks(), duration.days() + balancedTimeDuration.days(), 0, 0, 0, 0, Int128(0), Int128(0) };

    // Step 8: CalendarDateAdd(calendar, dateTime.[[ISODate]], dateDuration, overflow).
    ISO8601::PlainDate plainDate;
    if (plainDateTime->calendarID() != iso8601CalendarID())
        plainDate = calendarDateAdd(globalObject, plainDateTime->calendarID(), plainDateTime->plainDate(), dateDuration, overflow);
    else
        plainDate = isoDateAdd(globalObject, plainDateTime->plainDate(), dateDuration, overflow);
    RETURN_IF_EXCEPTION(scope, { });

    // Steps 9-10: CombineISODateAndTimeRecord + CreateTemporalDateTime.
    auto* result = TemporalPlainDateTime::tryCreateIfValid(globalObject, globalObject->plainDateTimeStructure(), WTF::move(plainDate), WTF::move(plainTime));
    RETURN_IF_EXCEPTION(scope, { });
    if (result && plainDateTime->calendarID() != iso8601CalendarID())
        result->setCalendarID(plainDateTime->calendarID());
    return JSValue::encode(result);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.add
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncAdd, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(callFrame->thisValue());
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.add called on value that's not a PlainDateTime"_s);

    // Step 1: Return ? AddDurationToDateTime(add, plainDateTime, duration, options).
    auto duration = TemporalDuration::toISO8601Duration(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });
    return addDurationToPlainDateTime(globalObject, scope, plainDateTime, WTF::move(duration), callFrame->argument(1));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.subtract
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncSubtract, (JSGlobalObject * globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(callFrame->thisValue());
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.subtract called on value that's not a PlainDateTime"_s);

    // Step 1: Return ? AddDurationToDateTime(subtract, ...) — step 2 negates duration.
    auto duration = TemporalDuration::toISO8601Duration(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });
    return addDurationToPlainDateTime(globalObject, scope, plainDateTime, -WTF::move(duration), callFrame->argument(1));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.with
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncWith, (JSGlobalObject * globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: RequireInternalSlot + type check done by caller.
    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(callFrame->thisValue());
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.with called on value that's not a PlainDateTime"_s);

    // Step 3: IsPartialTemporalObject.
    JSValue fieldsArg = callFrame->argument(0);
    if (!fieldsArg.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "First argument to Temporal.PlainDateTime.prototype.with must be an object"_s);
    JSObject* fields = asObject(fieldsArg);
    rejectObjectWithCalendarOrTimeZone(globalObject, fields);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 4: calendar = plainDateTime.[[Calendar]].
    CalendarID calendarId = plainDateTime->calendarID();
    bool calHasEras = TemporalCore::calendarHasEras(calendarId);

    // Step 12: PrepareCalendarFields — all fields in one alphabetical pass:
    //   calendar/timeZone (above), day, [era, eraYear,] hour, microsecond, millisecond,
    //   minute, month, monthCode, nanosecond, second, year.
    TemporalCore::CalendarFieldsIn partialDate;
    bool anyFieldSet = false;

    auto readTimeField = [&](PropertyName name) -> double {
        JSValue v = fields->get(globalObject, name);
        RETURN_IF_EXCEPTION(scope, 0.0);
        if (v.isUndefined())
            return std::numeric_limits<double>::quiet_NaN();
        double dv = v.toIntegerWithTruncation(globalObject);
        RETURN_IF_EXCEPTION(scope, 0.0);
        if (!std::isfinite(dv)) [[unlikely]] {
            throwRangeError(globalObject, scope, "field value must be finite"_s);
            return 0.0;
        }
        anyFieldSet = true;
        return dv;
    };

    // day
    {
        JSValue v = fields->get(globalObject, vm.propertyNames->day);
        RETURN_IF_EXCEPTION(scope, { });
        if (!v.isUndefined()) {
            double d = v.toIntegerWithTruncation(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            if (!(d > 0 && std::isfinite(d))) [[unlikely]]
                return throwVMRangeError(globalObject, scope, "day must be a positive finite integer"_s);
            partialDate.day = clampTo<uint8_t>(d);
            anyFieldSet = true;
        }
    }

    // era, eraYear (only for era-based calendars)
    if (calHasEras) {
        JSValue eraVal = fields->get(globalObject, Identifier::fromString(vm, "era"_s));
        RETURN_IF_EXCEPTION(scope, { });
        if (!eraVal.isUndefined()) {
            partialDate.era = eraVal.toWTFString(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            anyFieldSet = true;
        }
        JSValue eraYearVal = fields->get(globalObject, Identifier::fromString(vm, "eraYear"_s));
        RETURN_IF_EXCEPTION(scope, { });
        if (!eraYearVal.isUndefined()) {
            double ey = eraYearVal.toIntegerWithTruncation(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            if (!std::isfinite(ey)) [[unlikely]]
                return throwVMRangeError(globalObject, scope, "eraYear must be finite"_s);
            partialDate.eraYear = clampTo<int32_t>(ey);
            anyFieldSet = true;
        }
        if (partialDate.era.has_value() != partialDate.eraYear.has_value()) [[unlikely]]
            return throwVMTypeError(globalObject, scope, "era and eraYear must both be present or both absent"_s);
    }

    // hour, microsecond, millisecond, minute
    double partialHour = readTimeField(vm.propertyNames->hour);
    RETURN_IF_EXCEPTION(scope, { });
    double partialMicrosecond = readTimeField(Identifier::fromString(vm, "microsecond"_s));
    RETURN_IF_EXCEPTION(scope, { });
    double partialMillisecond = readTimeField(Identifier::fromString(vm, "millisecond"_s));
    RETURN_IF_EXCEPTION(scope, { });
    double partialMinute = readTimeField(Identifier::fromString(vm, "minute"_s));
    RETURN_IF_EXCEPTION(scope, { });

    // month
    {
        JSValue v = fields->get(globalObject, vm.propertyNames->month);
        RETURN_IF_EXCEPTION(scope, { });
        if (!v.isUndefined()) {
            double m = v.toIntegerWithTruncation(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            if (!(m > 0 && std::isfinite(m))) [[unlikely]]
                return throwVMRangeError(globalObject, scope, "month must be a positive finite integer"_s);
            partialDate.month = clampTo<uint32_t>(m);
            anyFieldSet = true;
        }
    }

    // monthCode
    {
        JSValue v = fields->get(globalObject, Identifier::fromString(vm, "monthCode"_s));
        RETURN_IF_EXCEPTION(scope, { });
        if (!v.isUndefined()) {
            String mcStr = v.toWTFString(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            partialDate.monthCode = ISO8601::parseMonthCode(mcStr);
            if (!partialDate.monthCode) [[unlikely]]
                return throwVMRangeError(globalObject, scope, "Invalid monthCode"_s);
            anyFieldSet = true;
        }
    }

    // nanosecond, second
    double partialNanosecond = readTimeField(Identifier::fromString(vm, "nanosecond"_s));
    RETURN_IF_EXCEPTION(scope, { });
    double partialSecond = readTimeField(Identifier::fromString(vm, "second"_s));
    RETURN_IF_EXCEPTION(scope, { });

    // year
    {
        JSValue v = fields->get(globalObject, vm.propertyNames->year);
        RETURN_IF_EXCEPTION(scope, { });
        if (!v.isUndefined()) {
            double y = v.toIntegerWithTruncation(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            if (!std::isfinite(y)) [[unlikely]]
                return throwVMRangeError(globalObject, scope, "year must be finite"_s);
            partialDate.year = clampTo<int32_t>(y);
            anyFieldSet = true;
        }
    }

    if (!anyFieldSet) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "at least one field must be provided"_s);

    // Steps 14-15: GetOptionsObject + GetTemporalOverflowOption.
    JSObject* options = intlGetOptionsObject(globalObject, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });
    TemporalOverflow overflow = toTemporalOverflow(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    // Steps 5, 13, 16: ISODateToFields + CalendarMergeFields + CalendarDateFromFields
    // — fused into plainDateWith.
    auto dateResult = TemporalCore::plainDateWith(calendarId, plainDateTime->plainDate(), partialDate, overflow);
    if (!dateResult) [[unlikely]] {
        if (dateResult.error().kind == TemporalErrorKind::TypeError)
            throwTypeError(globalObject, scope, String(dateResult.error().message));
        else
            throwRangeError(globalObject, scope, String(dateResult.error().message));
        return { };
    }

    // Steps 6-11, 16: time fields merged with this's current values, then RegulateTime.
    auto curTime = plainDateTime->plainTime();
    auto useTime = [](double partial, unsigned cur) {
        return std::isnan(partial) ? static_cast<double>(cur) : partial;
    };
    ISO8601::Duration timeDur { };
    timeDur.setField(TemporalUnit::Hour, useTime(partialHour, curTime.hour()));
    timeDur.setField(TemporalUnit::Minute, useTime(partialMinute, curTime.minute()));
    timeDur.setField(TemporalUnit::Second, useTime(partialSecond, curTime.second()));
    timeDur.setField(TemporalUnit::Millisecond, useTime(partialMillisecond, curTime.millisecond()));
    timeDur.setField(TemporalUnit::Microsecond, useTime(partialMicrosecond, curTime.microsecond()));
    timeDur.setField(TemporalUnit::Nanosecond, useTime(partialNanosecond, curTime.nanosecond()));
    auto newTime = TemporalPlainTime::regulateTime(globalObject, WTF::move(timeDur), overflow);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 17: CreateTemporalDateTime.
    auto* withResult = TemporalPlainDateTime::tryCreateIfValid(globalObject, globalObject->plainDateTimeStructure(), ISO8601::PlainDate(dateResult->isoDate), WTF::move(newTime));
    RETURN_IF_EXCEPTION(scope, { });
    if (withResult && calendarId != iso8601CalendarID())
        withResult->setCalendarID(calendarId);
    return JSValue::encode(withResult);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.withplaintime
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncWithPlainTime, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(callFrame->thisValue());
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.withPlainTime called on value that's not a PlainDateTime"_s);

    // Step 3: ToTimeRecordOrMidnight(plainTimeLike) — undefined → midnight.
    TemporalPlainTime* plainTime = nullptr;
    JSValue plainTimeLike = callFrame->argument(0);
    if (!plainTimeLike.isUndefined()) {
        plainTime = TemporalPlainTime::from(globalObject, plainTimeLike, jsUndefined());
        RETURN_IF_EXCEPTION(scope, { });
    }

    // Steps 4-5: CombineISODateAndTimeRecord + CreateTemporalDateTime.
    auto* wptResult = TemporalPlainDateTime::tryCreateIfValid(globalObject, globalObject->plainDateTimeStructure(), plainDateTime->plainDate(), plainTime ? plainTime->plainTime() : ISO8601::PlainTime());
    RETURN_IF_EXCEPTION(scope, { });
    if (wptResult && plainDateTime->calendarID() != iso8601CalendarID())
        wptResult->setCalendarID(plainDateTime->calendarID());
    return JSValue::encode(wptResult);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.round
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncRound, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(callFrame->thisValue());
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.round called on value that's not a PlainDateTime"_s);

    // Step 3: if roundTo is undefined throw TypeError.
    auto options = callFrame->argument(0);
    if (options.isUndefined()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.round requires an options argument"_s);

    // Steps 4-15: RoundISODateTime + CreateTemporalDateTime — delegated to plainDateTime->round().
    RELEASE_AND_RETURN(scope, JSValue::encode(plainDateTime->round(globalObject, options)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.equals
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncEquals, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: RequireInternalSlot + type check.
    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(callFrame->thisValue());
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.equals called on value that's not a PlainDateTime"_s);

    // Step 3: other = ToTemporalDateTime(other).
    auto* other = TemporalPlainDateTime::from(globalObject, callFrame->argument(0), jsUndefined());
    RETURN_IF_EXCEPTION(scope, { });

    // Step 4: CompareISODateTime(this, other) ≠ 0 → false.
    if (plainDateTime->plainDate() != other->plainDate() || plainDateTime->plainTime() != other->plainTime())
        return JSValue::encode(jsBoolean(false));

    // Step 5: CalendarEquals(this.[[Calendar]], other.[[Calendar]]).
    return JSValue::encode(jsBoolean(plainDateTime->calendarID() == other->calendarID()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.tozoneddatetime
// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.tozoneddatetime
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncToZonedDateTime, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: RequireInternalSlot.
    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(callFrame->thisValue());
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.toZonedDateTime called on value that's not a PlainDateTime"_s);

    // Step 3: Let timeZone be ? ToTemporalTimeZoneIdentifier(temporalTimeZoneLike).
    auto timeZone = toTemporalTimeZoneIdentifier(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });
    ASSERT(timeZone);

    // Step 4: Let resolvedOptions be ? GetOptionsObject(options).
    JSObject* options = intlGetOptionsObject(globalObject, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    // Step 5: Let disambiguation be ? GetTemporalDisambiguationOption(resolvedOptions).
    TemporalDisambiguation disambiguation = toTemporalDisambiguation(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 6: Let epochNs be ? GetEpochNanosecondsFor(timeZone, plainDateTime.[[ISODateTime]], disambiguation).
    auto exactTimeOpt = TemporalZonedDateTime::getEpochNanosecondsFor(globalObject, *timeZone, plainDateTime->plainDate(), plainDateTime->plainTime(), disambiguation);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 7: Return ! CreateTemporalZonedDateTime(epochNs, timeZone, plainDateTime.[[Calendar]]).
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalZonedDateTime::create(vm, globalObject->zonedDateTimeStructure(), *exactTimeOpt, *timeZone, plainDateTime->calendarID())));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.toplaindate
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncToPlainDate, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(callFrame->thisValue());
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.toPlainDate called on value that's not a PlainDateTime"_s);

    if (plainDateTime->calendarID() != iso8601CalendarID())
        return JSValue::encode(TemporalPlainDate::create(vm, globalObject->plainDateStructure(), plainDateTime->plainDate(), plainDateTime->calendarID()));
    return JSValue::encode(TemporalPlainDate::create(vm, globalObject->plainDateStructure(), plainDateTime->plainDate()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.toplaintime
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncToPlainTime, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(callFrame->thisValue());
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.toPlainTime called on value that's not a PlainDateTime"_s);

    return JSValue::encode(TemporalPlainTime::create(vm, globalObject->plainTimeStructure(), plainDateTime->plainTime()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.tostring
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncToString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(callFrame->thisValue());
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.toString called on value that's not a PlainDateTime"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(jsString(vm, plainDateTime->toString(globalObject, callFrame->argument(0)))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.tojson
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncToJSON, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(callFrame->thisValue());
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.toJSON called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsString(vm, plainDateTime->toString()));
}

// https://tc39.es/proposal-temporal/#sup-temporal.plaindatetime.prototype.tolocalestring
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncToLocaleString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(callFrame->thisValue());
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.toLocaleString called on value that's not a PlainDateTime"_s);

    JSValue locales = callFrame->argument(0);
    JSValue options = callFrame->argument(1);
    IntlDateTimeFormat* formatter;
    if (locales.isUndefined() && options.isUndefined())
        formatter = globalObject->defaultDateTimeFormat();
    else {
        formatter = IntlDateTimeFormat::create(vm, globalObject->dateTimeFormatStructure());
        formatter->initializeDateTimeFormat(globalObject, locales, options, IntlDateTimeFormat::RequiredComponent::Any, IntlDateTimeFormat::Defaults::All);
    }
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(formatter->format(globalObject, callFrame->thisValue())));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.valueof
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncValueOf, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.valueOf must not be called. To compare PlainDateTime values, use Temporal.PlainDateTime.compare"_s);
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.calendarid
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterCalendarId, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDate) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.calendarId called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsString(vm, plainDate->calendarIDAsString()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.year
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.year called on value that's not a PlainDateTime"_s);

    if (!TemporalCore::calendarIsISO(plainDateTime->calendarID())) {
        auto result = TemporalCore::calendarYear(plainDateTime->calendarID(), plainDateTime->plainDate());
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(plainDateTime->year()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.month
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterMonth, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.month called on value that's not a PlainDateTime"_s);

    if (!TemporalCore::calendarIsISO(plainDateTime->calendarID())) {
        auto result = TemporalCore::calendarMonth(plainDateTime->calendarID(), plainDateTime->plainDate());
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(plainDateTime->month()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.monthcode
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterMonthCode, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.monthCode called on value that's not a PlainDateTime"_s);

    if (!TemporalCore::calendarIsISO(plainDateTime->calendarID())) {
        auto result = TemporalCore::calendarMonthCode(plainDateTime->calendarID(), plainDateTime->plainDate());
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNontrivialString(vm, *result));
    }
    return JSValue::encode(jsNontrivialString(vm, plainDateTime->monthCode()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.day
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterDay, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.day called on value that's not a PlainDateTime"_s);

    if (!TemporalCore::calendarIsISO(plainDateTime->calendarID())) {
        auto result = TemporalCore::calendarDay(plainDateTime->calendarID(), plainDateTime->plainDate());
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(plainDateTime->day()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.hour
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterHour, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.hour called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsNumber(plainDateTime->hour()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.minute
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterMinute, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.minute called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsNumber(plainDateTime->minute()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.second
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterSecond, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.second called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsNumber(plainDateTime->second()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.millisecond
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterMillisecond, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.millisecond called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsNumber(plainDateTime->millisecond()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.microsecond
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterMicrosecond, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.microsecond called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsNumber(plainDateTime->microsecond()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.nanosecond
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterNanosecond, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.nanosecond called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsNumber(plainDateTime->nanosecond()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.dayofweek
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterDayOfWeek, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.dayOfWeek called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsNumber(plainDateTime->dayOfWeek()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.dayofyear
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterDayOfYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.dayOfYear called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsNumber(plainDateTime->dayOfYear()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.weekofyear
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterWeekOfYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.weekOfYear called on value that's not a PlainDateTime"_s);

    if (plainDateTime->calendarID() != iso8601CalendarID())
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsNumber(plainDateTime->weekOfYear()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.daysinweek
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterDaysInWeek, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.daysInWeek called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsNumber(7)); // ISO8601 calendar always returns 7.
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.daysinmonth
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterDaysInMonth, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.daysInMonth called on value that's not a PlainDateTime"_s);

    if (!TemporalCore::calendarIsISO(plainDateTime->calendarID())) {
        auto result = TemporalCore::calendarDaysInMonth(plainDateTime->calendarID(), plainDateTime->plainDate());
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(ISO8601::daysInMonth(plainDateTime->year(), plainDateTime->month())));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.daysinyear
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterDaysInYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.daysInYear called on value that's not a PlainDateTime"_s);

    if (!TemporalCore::calendarIsISO(plainDateTime->calendarID())) {
        auto result = TemporalCore::calendarDaysInYear(plainDateTime->calendarID(), plainDateTime->plainDate());
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(isLeapYear(plainDateTime->year()) ? 366 : 365));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.monthsinyear
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterMonthsInYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.monthsInYear called on value that's not a PlainDateTime"_s);

    if (!TemporalCore::calendarIsISO(plainDateTime->calendarID())) {
        auto result = TemporalCore::calendarMonthsInYear(plainDateTime->calendarID(), plainDateTime->plainDate());
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(12)); // ISO8601 calendar always returns 12.
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.inleapyear
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterInLeapYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.inLeapYear called on value that's not a PlainDateTime"_s);

    if (!TemporalCore::calendarIsISO(plainDateTime->calendarID())) {
        auto result = TemporalCore::calendarInLeapYear(plainDateTime->calendarID(), plainDateTime->plainDate());
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsBoolean(*result));
    }
    return JSValue::encode(jsBoolean(isLeapYear(plainDateTime->year())));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.until
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncUntil, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(callFrame->thisValue().toThis(globalObject, ECMAMode::strict()));
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.until called on value that's not a PlainDateTime"_s);

    // Step 3: other = ToTemporalDateTime(other).
    auto* other = TemporalPlainDateTime::from(globalObject, callFrame->argument(0), jsUndefined());
    RETURN_IF_EXCEPTION(scope, { });

    // Steps 4-5: GetDifferenceSettings.
    auto [smallestUnit, largestUnit, roundingMode, increment] = extractDifferenceOptions(globalObject, callFrame->argument(1), UnitGroup::DateTime, TemporalUnit::Nanosecond, TemporalUnit::Day);
    RETURN_IF_EXCEPTION(scope, { });

    // Steps 6-9: DifferenceISODateTime + CreateTemporalDuration.
    auto result = plainDateTime->differenceTemporalPlainDateTime(globalObject, DifferenceOperation::Until, other, smallestUnit, largestUnit, roundingMode, increment);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalDuration::tryCreateIfValid(globalObject, WTF::move(result), globalObject->durationStructure())));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.since
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncSince, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(callFrame->thisValue().toThis(globalObject, ECMAMode::strict()));
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.since called on value that's not a PlainDateTime"_s);

    // Step 3: other = ToTemporalDateTime(other).
    auto* other = TemporalPlainDateTime::from(globalObject, callFrame->argument(0), jsUndefined());
    RETURN_IF_EXCEPTION(scope, { });

    // Steps 4-5: GetDifferenceSettings(~since~, ...).
    auto [smallestUnit, largestUnit, roundingMode, increment] = extractDifferenceOptions(globalObject, callFrame->argument(1), UnitGroup::DateTime, TemporalUnit::Nanosecond, TemporalUnit::Day, DifferenceOperation::Since);
    RETURN_IF_EXCEPTION(scope, { });

    // Steps 6-9: DifferenceISODateTime + CreateTemporalDuration.
    auto result = plainDateTime->differenceTemporalPlainDateTime(globalObject, DifferenceOperation::Since, other, smallestUnit, largestUnit, roundingMode, increment);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalDuration::tryCreateIfValid(globalObject, WTF::move(result), globalObject->durationStructure())));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.withcalendar
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncWithCalendar, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(callFrame->thisValue().toThis(globalObject, ECMAMode::strict()));
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.withCalendar called on value that's not a PlainDateTime"_s);

    // Step 3: ToTemporalCalendarIdentifier(calendarLike).
    auto newCalendarID = toTemporalCalendarIdentifier(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });
    // Step 4: CreateTemporalDateTime(isoDateTime, calendar).
    auto* result = TemporalPlainDateTime::create(vm, globalObject->plainDateTimeStructure(),
        plainDateTime->plainDate(), plainDateTime->plainTime());
    result->setCalendarID(newCalendarID);
    return JSValue::encode(result);
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.yearofweek
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterYearOfWeek, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.yearOfWeek called on value that's not a PlainDateTime"_s);

    if (plainDateTime->calendarID() != iso8601CalendarID())
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsNumber(plainDateTime->yearOfWeek()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.era
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterEra, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.era called on value that's not a PlainDateTime"_s);

    // Step 3: Return CalendarISOToDate(calendar, isoDate).[[Era]].
    auto result = TemporalCore::calendarEra(plainDateTime->calendarID(), plainDateTime->plainDate());
    if (!result || !*result)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsString(vm, **result));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.erayear
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterEraYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.eraYear called on value that's not a PlainDateTime"_s);

    // Steps 3-5: Return CalendarISOToDate(calendar, isoDate).[[EraYear]], or undefined.
    auto result = TemporalCore::calendarEraYear(plainDateTime->calendarID(), plainDateTime->plainDate());
    if (!result || !*result)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsNumber(**result));
}

} // namespace JSC
