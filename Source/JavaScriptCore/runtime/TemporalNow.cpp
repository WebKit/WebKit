/*
 *  Copyright (C) 2021 Igalia S.L. All rights reserved.
 *  Copyright (C) 2021-2026 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "TemporalNow.h"

#include "ISO8601.h"
#include "JSCJSValueInlines.h"
#include "JSDateMath.h"
#include "JSGlobalObject.h"
#include "JSObjectInlines.h"
#include "ObjectPrototype.h"
#include "TemporalInstant.h"
#include "TemporalObject.h"
#include "TemporalPlainDate.h"
#include "TemporalPlainDateTime.h"
#include "TemporalPlainTime.h"
#include "TemporalZonedDateTime.h"
#include "TimeZoneICUBridge.h"
#include <wtf/DateMath.h>

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(TemporalNow);

static JSC_DECLARE_HOST_FUNCTION(temporalNowFuncInstant);
static JSC_DECLARE_HOST_FUNCTION(temporalNowFuncTimeZoneId);
static JSC_DECLARE_HOST_FUNCTION(temporalNowFuncPlainDateISO);
static JSC_DECLARE_HOST_FUNCTION(temporalNowFuncPlainDateTimeISO);
static JSC_DECLARE_HOST_FUNCTION(temporalNowFuncPlainTimeISO);
static JSC_DECLARE_HOST_FUNCTION(temporalNowFuncZonedDateTimeISO);

} // namespace JSC

#include "TemporalNow.lut.h"

namespace JSC {

/* Source for TemporalNow.lut.h
@begin temporalNowTable
    instant             temporalNowFuncInstant          DontEnum|Function 0
    timeZoneId          temporalNowFuncTimeZoneId        DontEnum|Function 0
    plainDateISO        temporalNowFuncPlainDateISO      DontEnum|Function 0
    plainDateTimeISO    temporalNowFuncPlainDateTimeISO  DontEnum|Function 0
    plainTimeISO        temporalNowFuncPlainTimeISO      DontEnum|Function 0
    zonedDateTimeISO    temporalNowFuncZonedDateTimeISO  DontEnum|Function 0
@end
*/

const ClassInfo TemporalNow::s_info = { "Temporal.Now"_s, &Base::s_info, &temporalNowTable, nullptr, CREATE_METHOD_TABLE(TemporalNow) };

TemporalNow::TemporalNow(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

TemporalNow* TemporalNow::create(VM& vm, Structure* structure)
{
    TemporalNow* object = new (NotNull, allocateCell<TemporalNow>(vm)) TemporalNow(vm, structure);
    object->finishCreation(vm);
    return object;
}

Structure* TemporalNow::createStructure(VM& vm, JSGlobalObject* globalObject)
{
    return Structure::create(vm, globalObject, globalObject->objectPrototype(), TypeInfo(ObjectType, StructureFlags), info());
}

void TemporalNow::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

// https://tc39.es/proposal-temporal/#sec-temporal.now.instant
JSC_DEFINE_HOST_FUNCTION(temporalNowFuncInstant, (JSGlobalObject* globalObject, CallFrame*))
{
    return JSValue::encode(TemporalInstant::create(globalObject->vm(), globalObject->instantStructure(), ISO8601::ExactTime::now()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.now.timezoneid
// https://tc39.es/proposal-temporal/#sec-temporal-systemtimezoneidentifier
JSC_DEFINE_HOST_FUNCTION(temporalNowFuncTimeZoneId, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    return JSValue::encode(jsNontrivialString(vm, vm.dateCache.defaultTimeZone().toString()));
}

// Resolve the timezone argument for Temporal.Now.* functions.
// undefined → nullopt (callers fall back to DateCache.defaultTimeZone()).
// Otherwise → ? ToTemporalTimeZoneIdentifier(temporalTimeZoneLike).
static std::optional<TimeZone> resolveNowTimeZone(JSGlobalObject* globalObject, JSValue arg)
{
    if (arg.isUndefined())
        return std::nullopt;
    return toTemporalTimeZoneIdentifier(globalObject, arg);
}

// https://tc39.es/proposal-temporal/#sec-temporal-systemdatetime
static ISO8601::PlainDateTime systemDateTime(JSGlobalObject* globalObject, JSValue timeZoneLike)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: If tzLike is undefined, tz = SystemTimeZoneIdentifier(); else tz = ? ToTemporalTimeZoneIdentifier(tzLike).
    auto tzOpt = resolveNowTimeZone(globalObject, timeZoneLike);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 3: Let epochNs be SystemUTCEpochNanoseconds().
    auto exactTime = ISO8601::ExactTime::now();

    // Step 4: Return GetISODateTimeFor(tz, epochNs).
    if (!tzOpt) {
        // Fast path for system tz: use DateCache's cached offset directly,
        // then decompose via the canonical exactTimeToLocalDateAndTime.
        GregorianDateTime dt;
        vm.dateCache.msToGregorianDateTime(static_cast<double>(exactTime.floorEpochMilliseconds()), TimeType::LocalTime, dt);
        int64_t offsetNs = static_cast<int64_t>(dt.utcOffsetInMinute()) * WTF::Int64Milliseconds::msPerMinute * static_cast<int64_t>(ISO8601::ExactTime::nsPerMillisecond);
        return TemporalCore::exactTimeToLocalDateAndTime(exactTime, offsetNs);
    }
    auto result = TemporalCore::getISODateTimeFor(*tzOpt, exactTime);
    if (!result) [[unlikely]] {
        throwTemporalError(globalObject, scope, result.error());
        return { };
    }
    return *result;
}

// https://tc39.es/proposal-temporal/#sec-temporal.now.plaindateiso
JSC_DEFINE_HOST_FUNCTION(temporalNowFuncPlainDateISO, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Let isoDateTime be ? SystemDateTime(temporalTimeZoneLike).
    auto isoDT = systemDateTime(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    // Step 2: Return ! CreateTemporalDate(isoDateTime.[[ISODate]], "iso8601").
    return JSValue::encode(TemporalPlainDate::create(vm, globalObject->plainDateStructure(), WTF::move(isoDT.date)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.now.plaindatetimeiso
JSC_DEFINE_HOST_FUNCTION(temporalNowFuncPlainDateTimeISO, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Let isoDateTime be ? SystemDateTime(temporalTimeZoneLike).
    auto isoDT = systemDateTime(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    // Step 2: Return ! CreateTemporalDateTime(isoDateTime, "iso8601").
    return JSValue::encode(TemporalPlainDateTime::create(vm, globalObject->plainDateTimeStructure(), WTF::move(isoDT.date), WTF::move(isoDT.time)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.now.plaintimeiso
JSC_DEFINE_HOST_FUNCTION(temporalNowFuncPlainTimeISO, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Let isoDateTime be ? SystemDateTime(temporalTimeZoneLike).
    auto isoDT = systemDateTime(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    // Step 2: Return ! CreateTemporalTime(isoDateTime.[[Time]]).
    return JSValue::encode(TemporalPlainTime::create(vm, globalObject->plainTimeStructure(), WTF::move(isoDT.time)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.now.zoneddatetimeiso
JSC_DEFINE_HOST_FUNCTION(temporalNowFuncZonedDateTimeISO, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: If temporalTimeZoneLike is undefined, let tz be SystemTimeZoneIdentifier();
    //   else, let tz be ? ToTemporalTimeZoneIdentifier(temporalTimeZoneLike).
    auto tzOpt = resolveNowTimeZone(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });
    TimeZone tz = tzOpt ? *tzOpt : vm.dateCache.defaultTimeZone();

    // Step 2: Let epochNs be SystemUTCEpochNanoseconds().
    // Step 3: Return ! CreateTemporalZonedDateTime(epochNs, tz, "iso8601").
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalZonedDateTime::create(vm, globalObject->zonedDateTimeStructure(), ISO8601::ExactTime::now(), tz, iso8601CalendarID())));
}

} // namespace JSC
