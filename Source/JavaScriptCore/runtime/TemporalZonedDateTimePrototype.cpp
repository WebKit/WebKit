/*
 * Copyright (C) 2026 Igalia, S.L. All rights reserved.
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
#include "TemporalZonedDateTime.h"

namespace JSC {

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
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterEpochMilliseconds);
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterEpochNanoseconds);
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterDayOfWeek);
static JSC_DECLARE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterDayOfYear);

}

#include "TemporalZonedDateTimePrototype.lut.h"

namespace JSC {

const ClassInfo TemporalZonedDateTimePrototype::s_info = { "Temporal.ZonedDateTime"_s, &Base::s_info, &zonedDateTimePrototypeTable, nullptr, CREATE_METHOD_TABLE(TemporalZonedDateTimePrototype) };

/* Source for TemporalZonedDateTimePrototype.lut.h
@begin zonedDateTimePrototypeTable
  calendarId            temporalZonedDateTimePrototypeGetterCalendarId          DontEnum|ReadOnly|CustomAccessor
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

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.calendarid
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterCalendarId, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.calendarId called on value that's not a ZonedDateTime"_s);

    // FIXME: when calendars are supported, get the string ID of the calendar
    return JSValue::encode(jsString(vm, StringView("iso8601"_s)));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.year
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.year called on value that's not a ZonedDateTime"_s);

    auto isoDateTime = TemporalTimeZone::getISODateTimeFor(globalObject, zonedDateTime->timeZone(), zonedDateTime->exactTime());
    return JSValue::encode(jsNumber(isoDateTime.date().year()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.month
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterMonth, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.month called on value that's not a ZonedDateTime"_s);

    auto isoDateTime = TemporalTimeZone::getISODateTimeFor(globalObject, zonedDateTime->timeZone(), zonedDateTime->exactTime());
    return JSValue::encode(jsNumber(isoDateTime.date().month()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.monthcode
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterMonthCode, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.monthCode called on value that's not a ZonedDateTime"_s);

    auto isoDateTime = TemporalTimeZone::getISODateTimeFor(globalObject, zonedDateTime->timeZone(), zonedDateTime->exactTime());
    return JSValue::encode(jsNontrivialString(vm, ISO8601::monthCode(isoDateTime.date().month())));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.day
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterDay, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.day called on value that's not a ZonedDateTime"_s);

    auto isoDateTime = TemporalTimeZone::getISODateTimeFor(globalObject, zonedDateTime->timeZone(), zonedDateTime->exactTime());
    return JSValue::encode(jsNumber(isoDateTime.date().day()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.hour
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterHour, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.hour called on value that's not a ZonedDateTime"_s);

    auto isoDateTime = TemporalTimeZone::getISODateTimeFor(globalObject, zonedDateTime->timeZone(), zonedDateTime->exactTime());
    return JSValue::encode(jsNumber(isoDateTime.time().hour()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.minute
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterMinute, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.minute called on value that's not a ZonedDateTime"_s);

    auto isoDateTime = TemporalTimeZone::getISODateTimeFor(globalObject, zonedDateTime->timeZone(), zonedDateTime->exactTime());
    return JSValue::encode(jsNumber(isoDateTime.time().minute()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.second
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterSecond, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.second called on value that's not a ZonedDateTime"_s);

    auto isoDateTime = TemporalTimeZone::getISODateTimeFor(globalObject, zonedDateTime->timeZone(), zonedDateTime->exactTime());
    return JSValue::encode(jsNumber(isoDateTime.time().second()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.millisecond
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterMillisecond, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.millisecond called on value that's not a ZonedDateTime"_s);

    auto isoDateTime = TemporalTimeZone::getISODateTimeFor(globalObject, zonedDateTime->timeZone(), zonedDateTime->exactTime());
    return JSValue::encode(jsNumber(isoDateTime.time().millisecond()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.microsecond
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterMicrosecond, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.microsecond called on value that's not a ZonedDateTime"_s);

    auto isoDateTime = TemporalTimeZone::getISODateTimeFor(globalObject, zonedDateTime->timeZone(), zonedDateTime->exactTime());
    return JSValue::encode(jsNumber(isoDateTime.time().microsecond()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.nanosecond
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterNanosecond, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.nanosecond called on value that's not a ZonedDateTime"_s);

    auto isoDateTime = TemporalTimeZone::getISODateTimeFor(globalObject, zonedDateTime->timeZone(), zonedDateTime->exactTime());
    return JSValue::encode(jsNumber(isoDateTime.time().nanosecond()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.epochmilliseconds
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterEpochMilliseconds, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.epochMilliseconds called on value that's not a ZonedDateTime"_s);

    auto ns = zonedDateTime->exactTime().epochNanoseconds();
    // 4. Let ms be floor(ℝ(ns) / 10**6).
    Int128 ms = ISO8601::floorDiv(ns, 1000000);
    return JSValue::encode(jsNumber(static_cast<double>(ms)));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.epochnanoseconds
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterEpochNanoseconds, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.epochNanoseconds called on value that's not a ZonedDateTime"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(JSBigInt::createFrom(globalObject, zonedDateTime->exactTime().epochNanoseconds())));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.dayofweek
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterDayOfWeek, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.dayOfWeek called on value that's not a ZonedDateTime"_s);

    auto isoDateTime = TemporalTimeZone::getISODateTimeFor(globalObject, zonedDateTime->timeZone(), zonedDateTime->exactTime());
    return JSValue::encode(jsNumber(ISO8601::dayOfWeek(isoDateTime.date())));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.dayofyear
JSC_DEFINE_CUSTOM_GETTER(temporalZonedDateTimePrototypeGetterDayOfYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(JSValue::decode(thisValue));
    if (!zonedDateTime) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime.prototype.dayOfYear called on value that's not a ZonedDateTime"_s);

    auto isoDateTime = TemporalTimeZone::getISODateTimeFor(globalObject, zonedDateTime->timeZone(), zonedDateTime->exactTime());
    return JSValue::encode(jsNumber(ISO8601::dayOfYear(isoDateTime.date())));
}

} // namespace JSC
