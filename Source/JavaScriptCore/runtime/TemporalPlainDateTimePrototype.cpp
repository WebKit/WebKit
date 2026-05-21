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

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.add
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncAdd, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(callFrame->thisValue());
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.add called on value that's not a PlainDateTime"_s);

    auto duration = TemporalDuration::toISO8601Duration(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    JSObject* options = intlGetOptionsObject(globalObject, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    auto balancedTimeDuration = TemporalPlainTime::addTime(plainDateTime->plainTime(), duration);
    auto plainTime = TemporalPlainTime::toPlainTime(globalObject, balancedTimeDuration);
    RETURN_IF_EXCEPTION(scope, { });

    TemporalOverflow overflow = toTemporalOverflow(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    ISO8601::Duration dateDuration { duration.years(), duration.months(), duration.weeks(), duration.days() + balancedTimeDuration.days(), 0LL, 0LL, 0LL, 0LL, Int128(0), Int128(0) };
    ISO8601::PlainDate plainDate;
    if (plainDateTime->calendarId() != "iso8601"_s && !plainDateTime->calendarId().isEmpty())
        plainDate = calendarDateAdd(globalObject, plainDateTime->calendarId(), plainDateTime->plainDate(), dateDuration, overflow);
    else
        plainDate = isoDateAdd(globalObject, plainDateTime->plainDate(), dateDuration, overflow);
    RETURN_IF_EXCEPTION(scope, { });

    auto* addResult = TemporalPlainDateTime::tryCreateIfValid(globalObject, globalObject->plainDateTimeStructure(), WTF::move(plainDate), WTF::move(plainTime));
    RETURN_IF_EXCEPTION(scope, { });
    if (addResult && plainDateTime->calendarId() != "iso8601"_s && !plainDateTime->calendarId().isEmpty())
        addResult->setCalendarId(String(plainDateTime->calendarId()));
    return JSValue::encode(addResult);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.subtract
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncSubtract, (JSGlobalObject* globalObject, CallFrame* callFrame))
{

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(callFrame->thisValue());
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.subtract called on value that's not a PlainDateTime"_s);

    auto duration = TemporalDuration::toISO8601Duration(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });
    duration = -duration;

    JSObject* options = intlGetOptionsObject(globalObject, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    auto balancedTimeDuration = TemporalPlainTime::addTime(plainDateTime->plainTime(), duration);
    auto plainTime = TemporalPlainTime::toPlainTime(globalObject, balancedTimeDuration);
    RETURN_IF_EXCEPTION(scope, { });

    TemporalOverflow overflow = toTemporalOverflow(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    ISO8601::Duration dateDuration { duration.years(), duration.months(), duration.weeks(), duration.days() + balancedTimeDuration.days(), 0LL, 0LL, 0LL, 0LL, Int128(0), Int128(0) };
    ISO8601::PlainDate plainDate;
    if (plainDateTime->calendarId() != "iso8601"_s && !plainDateTime->calendarId().isEmpty())
        plainDate = calendarDateAdd(globalObject, plainDateTime->calendarId(), plainDateTime->plainDate(), dateDuration, overflow);
    else
        plainDate = isoDateAdd(globalObject, plainDateTime->plainDate(), dateDuration, overflow);
    RETURN_IF_EXCEPTION(scope, { });

    auto* subResult = TemporalPlainDateTime::tryCreateIfValid(globalObject, globalObject->plainDateTimeStructure(), WTF::move(plainDate), WTF::move(plainTime));
    RETURN_IF_EXCEPTION(scope, { });
    if (subResult && plainDateTime->calendarId() != "iso8601"_s && !plainDateTime->calendarId().isEmpty())
        subResult->setCalendarId(String(plainDateTime->calendarId()));
    return JSValue::encode(subResult);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.with
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncWith, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(callFrame->thisValue());
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.with called on value that's not a PlainDateTime"_s);

    JSValue fieldsArg = callFrame->argument(0);
    if (!fieldsArg.isObject())
        return throwVMTypeError(globalObject, scope, "First argument to Temporal.PlainDateTime.prototype.with must be an object"_s);
    JSObject* fields = asObject(fieldsArg);

    rejectObjectWithCalendarOrTimeZone(globalObject, fields);
    RETURN_IF_EXCEPTION(scope, { });

    // Read ALL fields in alphabetical order before options — spec PrepareCalendarFields ordering.
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
            throwRangeError(globalObject, scope, "field value must be positive"_s);
            return std::nullopt;
        }
        anyFieldSet = true;
        return static_cast<int32_t>(dv);
    };

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
                return throwVMRangeError(globalObject, scope, "Invalid monthCode"_s);
            anyFieldSet = true;
        }
    }

    auto partialNanosecond = readIntField(vm.propertyNames->nanosecond);
    RETURN_IF_EXCEPTION(scope, { });
    auto partialSecond = readIntField(vm.propertyNames->second);
    RETURN_IF_EXCEPTION(scope, { });
    auto partialYear = readIntField(vm.propertyNames->year);
    RETURN_IF_EXCEPTION(scope, { });

    if (!anyFieldSet)
        return throwVMTypeError(globalObject, scope, "at least one field must be provided"_s);

    // NOW read options (after all fields).
    JSObject* options = intlGetOptionsObject(globalObject, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });
    TemporalOverflow overflow = toTemporalOverflow(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    // Merge and build result. Use calendar coordinates for fallback values.
    int32_t fallbackYear = plainDateTime->year();
    unsigned fallbackMonth = plainDateTime->month();
    unsigned fallbackDay = plainDateTime->day();
    std::optional<ParsedMonthCode> fallbackMonthCode;
    bool isNonISO = TemporalCore::calendarIDFromString(plainDateTime->calendarId()) != iso8601CalendarID();
    if (isNonISO) {
        auto calFields = TemporalCore::isoToCalendarFields(TemporalCore::calendarIDFromString(plainDateTime->calendarId()), plainDateTime->plainDate());
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

    std::optional<ParsedMonthCode> resolvedMonthCode = partialMonthCode;
    if (!partialMonth && !partialMonthCode && fallbackMonthCode)
        resolvedMonthCode = fallbackMonthCode;

    auto newDate = isoDateFromFields(globalObject, TemporalDateFormat::Date,
        mergedYear, mergedMonth, mergedDay, resolvedMonthCode, overflow, plainDateTime->calendarId());
    RETURN_IF_EXCEPTION(scope, { });

    ISO8601::Duration timeDur { };
    timeDur.setHours(static_cast<int64_t>(partialHour.value_or(static_cast<int32_t>(plainDateTime->hour()))));
    timeDur.setMinutes(static_cast<int64_t>(partialMinute.value_or(static_cast<int32_t>(plainDateTime->minute()))));
    timeDur.setSeconds(static_cast<int64_t>(partialSecond.value_or(static_cast<int32_t>(plainDateTime->second()))));
    timeDur.setMilliseconds(static_cast<int64_t>(partialMillisecond.value_or(static_cast<int32_t>(plainDateTime->millisecond()))));
    timeDur.setMicroseconds(partialMicrosecond.value_or(static_cast<int32_t>(plainDateTime->microsecond())));
    timeDur.setNanoseconds(partialNanosecond.value_or(static_cast<int32_t>(plainDateTime->nanosecond())));
    auto newTime = TemporalPlainTime::regulateTime(globalObject, WTF::move(timeDur), overflow);
    RETURN_IF_EXCEPTION(scope, { });

    auto* withResult = TemporalPlainDateTime::tryCreateIfValid(globalObject, globalObject->plainDateTimeStructure(), WTF::move(newDate), WTF::move(newTime));
    RETURN_IF_EXCEPTION(scope, { });
    if (withResult && plainDateTime->calendarId() != "iso8601"_s && !plainDateTime->calendarId().isEmpty())
        withResult->setCalendarId(String(plainDateTime->calendarId()));
    return JSValue::encode(withResult);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.withplaintime
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncWithPlainTime, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(callFrame->thisValue());
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.withPlainTime called on value that's not a PlainDateTime"_s);

    TemporalPlainTime* plainTime = nullptr;
    JSValue plainTimeLike = callFrame->argument(0);
    if (!plainTimeLike.isUndefined()) {
        plainTime = TemporalPlainTime::from(globalObject, plainTimeLike, nullptr);
        RETURN_IF_EXCEPTION(scope, { });
    }

    auto* wptResult = TemporalPlainDateTime::tryCreateIfValid(globalObject, globalObject->plainDateTimeStructure(), plainDateTime->plainDate(), plainTime ? plainTime->plainTime() : ISO8601::PlainTime());
    RETURN_IF_EXCEPTION(scope, { });
    if (wptResult && plainDateTime->calendarId() != "iso8601"_s && !plainDateTime->calendarId().isEmpty())
        wptResult->setCalendarId(String(plainDateTime->calendarId()));
    return JSValue::encode(wptResult);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.round
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncRound, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(callFrame->thisValue());
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.round called on value that's not a PlainDateTime"_s);

    auto options = callFrame->argument(0);
    if (options.isUndefined())
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.round requires an options argument"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(plainDateTime->round(globalObject, options)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.equals
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncEquals, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(callFrame->thisValue());
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.equals called on value that's not a PlainDateTime"_s);

    auto* other = TemporalPlainDateTime::from(globalObject, callFrame->argument(0), nullptr);
    RETURN_IF_EXCEPTION(scope, { });

    if (plainDateTime->plainDate() != other->plainDate() || plainDateTime->plainTime() != other->plainTime())
        return JSValue::encode(jsBoolean(false));

    auto calA = plainDateTime->calendarId();
    auto calB = other->calendarId();
    if (calA.isEmpty())
        calA = "iso8601"_s;
    if (calB.isEmpty())
        calB = "iso8601"_s;
    return JSValue::encode(jsBoolean(calA == calB));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.tozoneddatetime
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncToZonedDateTime, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(callFrame->thisValue());
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.toZonedDateTime called on value that's not a PlainDateTime"_s);

    JSValue arg = callFrame->argument(0);
    if (!arg.isString()) {
        throwTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.toZonedDateTime requires a string time zone identifier"_s);
        return { };
    }

    String tzString = asString(arg)->value(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    auto tz = ISO8601::parseTemporalTimeZoneIdentifier(tzString);
    if (!tz) {
        throwRangeError(globalObject, scope, makeString("'"_s, ellipsizeAt(100, tzString), "' is not a valid time zone identifier"_s));
        return { };
    }

    JSObject* options = intlGetOptionsObject(globalObject, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    TemporalDisambiguation disambiguation = toTemporalDisambiguation(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    auto exactTimeOpt = TemporalZonedDateTime::getEpochNanosecondsFor(globalObject, *tz, plainDateTime->plainDate(), plainDateTime->plainTime(), disambiguation);
    RETURN_IF_EXCEPTION(scope, { });
    if (!exactTimeOpt)
        return { };

    String tzId;
    // If the input string is a UTC offset (e.g. "+00:00"), preserve it exactly using
    // formatTimeZoneOffsetString — do NOT map through intlAvailableNamedTimeZone which
    // would convert "+00:00" to "UTC". Both "UTC" and "+00:00" parse to isUTCOffset=true
    // internally; the tzString prefix distinguishes them.
    bool isOffsetString = !tzString.isEmpty() && (tzString[0] == '+' || tzString[0] == '-');
    if (isOffsetString)
        tzId = ISO8601::formatTimeZoneOffsetString(tz->utcOffsetNanoseconds());
    else if (auto namedTz = intlAvailableNamedTimeZone(tzString))
        tzId = namedTz->identifier;
    else
        tzId = tz->toString();
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalZonedDateTime::create(vm, globalObject->zonedDateTimeStructure(),
        *exactTimeOpt, *tz, WTF::move(tzId), String(plainDateTime->calendarId()))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.toplaindate
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncToPlainDate, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(callFrame->thisValue());
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.toPlainDate called on value that's not a PlainDateTime"_s);

    if (plainDateTime->calendarId() != "iso8601"_s && !plainDateTime->calendarId().isEmpty())
        return JSValue::encode(TemporalPlainDate::create(vm, globalObject->plainDateStructure(), plainDateTime->plainDate(), String(plainDateTime->calendarId())));
    return JSValue::encode(TemporalPlainDate::create(vm, globalObject->plainDateStructure(), plainDateTime->plainDate()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.toplaintime
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncToPlainTime, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(callFrame->thisValue());
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.toPlainTime called on value that's not a PlainDateTime"_s);

    return JSValue::encode(TemporalPlainTime::create(vm, globalObject->plainTimeStructure(), plainDateTime->plainTime()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.tostring
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncToString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(callFrame->thisValue());
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.toString called on value that's not a PlainDateTime"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(jsString(vm, plainDateTime->toString(globalObject, callFrame->argument(0)))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.tojson
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncToJSON, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(callFrame->thisValue());
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.toJSON called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsString(vm, plainDateTime->toString()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.tolocalestring
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncToLocaleString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(callFrame->thisValue());
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.toLocaleString called on value that's not a PlainDateTime"_s);

    auto* formatter = IntlDateTimeFormat::create(vm, globalObject->dateTimeFormatStructure());
    formatter->initializeDateTimeFormat(globalObject, callFrame->argument(0), callFrame->argument(1), IntlDateTimeFormat::RequiredComponent::Any, IntlDateTimeFormat::Defaults::All);
    RETURN_IF_EXCEPTION(scope, { });

    if (plainDateTime->calendarId() != "iso8601"_s
        && !IntlDateTimeFormat::calendarMatchesICU(plainDateTime->calendarId(), formatter->ensureCalendar()))
        return throwVMRangeError(globalObject, scope, "Temporal calendar does not match locale calendar"_s);

    auto d = plainDateTime->plainDate();
    auto t = plainDateTime->plainTime();
    auto et = ISO8601::ExactTime::fromISOPartsAndOffset(
        d.year(), d.month(), d.day(), t.hour(), t.minute(), t.second(),
        t.millisecond(), t.microsecond(), t.nanosecond(), 0);
    RELEASE_AND_RETURN(scope, JSValue::encode(formatter->format(globalObject, et.epochMilliseconds(), IntlDateTimeFormat::TemporalFieldKind::PlainDateTime)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.valueof
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimePrototypeFuncValueOf, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.valueOf must not be called. To compare PlainDateTime values, use Temporal.PlainDateTime.compare"_s);
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterCalendarId, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.calendarId called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsString(vm, plainDate->calendarId()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.year called on value that's not a PlainDateTime"_s);

    if (plainDateTime->calendarId() != "iso8601"_s) {
        auto result = TemporalCore::calendarYear(TemporalCore::calendarIDFromString(plainDateTime->calendarId()), plainDateTime->plainDate());
        if (!result)
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(plainDateTime->year()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterMonth, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.month called on value that's not a PlainDateTime"_s);

    if (plainDateTime->calendarId() != "iso8601"_s) {
        auto result = TemporalCore::calendarMonth(TemporalCore::calendarIDFromString(plainDateTime->calendarId()), plainDateTime->plainDate());
        if (!result)
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(plainDateTime->month()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterMonthCode, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.monthCode called on value that's not a PlainDateTime"_s);

    if (plainDateTime->calendarId() != "iso8601"_s) {
        auto result = TemporalCore::calendarMonthCode(TemporalCore::calendarIDFromString(plainDateTime->calendarId()), plainDateTime->plainDate());
        if (!result)
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNontrivialString(vm, *result));
    }
    return JSValue::encode(jsNontrivialString(vm, plainDateTime->monthCode()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterDay, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.day called on value that's not a PlainDateTime"_s);

    if (plainDateTime->calendarId() != "iso8601"_s) {
        auto result = TemporalCore::calendarDay(TemporalCore::calendarIDFromString(plainDateTime->calendarId()), plainDateTime->plainDate());
        if (!result)
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(plainDateTime->day()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterHour, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.hour called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsNumber(plainDateTime->hour()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterMinute, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.minute called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsNumber(plainDateTime->minute()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterSecond, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.second called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsNumber(plainDateTime->second()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterMillisecond, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.millisecond called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsNumber(plainDateTime->millisecond()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterMicrosecond, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.microsecond called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsNumber(plainDateTime->microsecond()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterNanosecond, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.nanosecond called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsNumber(plainDateTime->nanosecond()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterDayOfWeek, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.dayOfWeek called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsNumber(plainDateTime->dayOfWeek()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterDayOfYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.dayOfYear called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsNumber(plainDateTime->dayOfYear()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterWeekOfYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.weekOfYear called on value that's not a PlainDateTime"_s);

    if (plainDateTime->calendarId() != "iso8601"_s && !plainDateTime->calendarId().isEmpty())
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsNumber(plainDateTime->weekOfYear()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterDaysInWeek, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.daysInWeek called on value that's not a PlainDateTime"_s);

    return JSValue::encode(jsNumber(7)); // ISO8601 calendar always returns 7.
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterDaysInMonth, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.daysInMonth called on value that's not a PlainDateTime"_s);

    if (plainDateTime->calendarId() != "iso8601"_s) {
        auto result = TemporalCore::calendarDaysInMonth(TemporalCore::calendarIDFromString(plainDateTime->calendarId()), plainDateTime->plainDate());
        if (!result)
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(ISO8601::daysInMonth(plainDateTime->year(), plainDateTime->month())));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterDaysInYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.daysInYear called on value that's not a PlainDateTime"_s);

    if (plainDateTime->calendarId() != "iso8601"_s) {
        auto result = TemporalCore::calendarDaysInYear(TemporalCore::calendarIDFromString(plainDateTime->calendarId()), plainDateTime->plainDate());
        if (!result)
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(isLeapYear(plainDateTime->year()) ? 366 : 365));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterMonthsInYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.monthsInYear called on value that's not a PlainDateTime"_s);

    if (plainDateTime->calendarId() != "iso8601"_s) {
        auto result = TemporalCore::calendarMonthsInYear(TemporalCore::calendarIDFromString(plainDateTime->calendarId()), plainDateTime->plainDate());
        if (!result)
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(12)); // ISO8601 calendar always returns 12.
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterInLeapYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.inLeapYear called on value that's not a PlainDateTime"_s);

    if (plainDateTime->calendarId() != "iso8601"_s) {
        auto result = TemporalCore::calendarInLeapYear(TemporalCore::calendarIDFromString(plainDateTime->calendarId()), plainDateTime->plainDate());
        if (!result)
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
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.until called on value that's not a PlainDateTime"_s);

    auto* other = TemporalPlainDateTime::from(globalObject, callFrame->argument(0), nullptr);
    RETURN_IF_EXCEPTION(scope, { });

    auto [smallestUnit, largestUnit, roundingMode, increment] = extractDifferenceOptions(globalObject, callFrame->argument(1), UnitGroup::DateTime, TemporalUnit::Nanosecond, TemporalUnit::Day);
    RETURN_IF_EXCEPTION(scope, { });

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
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.since called on value that's not a PlainDateTime"_s);

    auto* other = TemporalPlainDateTime::from(globalObject, callFrame->argument(0), nullptr);
    RETURN_IF_EXCEPTION(scope, { });

    auto [smallestUnit, largestUnit, roundingMode, increment] = extractDifferenceOptions(globalObject, callFrame->argument(1), UnitGroup::DateTime, TemporalUnit::Nanosecond, TemporalUnit::Day);
    RETURN_IF_EXCEPTION(scope, { });
    roundingMode = TemporalCore::negateTemporalRoundingMode(roundingMode);

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
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.withCalendar called on value that's not a PlainDateTime"_s);

    String newCalendarId = toTemporalCalendarIdentifier(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    return JSValue::encode(TemporalPlainDateTime::create(vm, globalObject->plainDateTimeStructure(),
        plainDateTime->plainDate(), plainDateTime->plainTime(), WTF::move(newCalendarId)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.yearofweek
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterYearOfWeek, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.yearOfWeek called on value that's not a PlainDateTime"_s);

    if (plainDateTime->calendarId() != "iso8601"_s && !plainDateTime->calendarId().isEmpty())
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsNumber(plainDateTime->yearOfWeek()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.era
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterEra, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.era called on value that's not a PlainDateTime"_s);

    auto result = TemporalCore::calendarEra(TemporalCore::calendarIDFromString(plainDateTime->calendarId()), plainDateTime->plainDate());
    if (!result)
        return throwVMRangeError(globalObject, scope, result.error().message);
    if (!*result)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsString(vm, **result));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.erayear
JSC_DEFINE_CUSTOM_GETTER(temporalPlainDateTimePrototypeGetterEraYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(JSValue::decode(thisValue));
    if (!plainDateTime)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDateTime.prototype.eraYear called on value that's not a PlainDateTime"_s);

    auto result = TemporalCore::calendarEraYear(TemporalCore::calendarIDFromString(plainDateTime->calendarId()), plainDateTime->plainDate());
    if (!result)
        return throwVMRangeError(globalObject, scope, result.error().message);
    if (!*result)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsNumber(**result));
}

} // namespace JSC
