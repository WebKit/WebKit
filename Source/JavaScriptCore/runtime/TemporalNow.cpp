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
#include "TemporalPlainDate.h"
#include "TemporalPlainDateTime.h"
#include "TemporalPlainTime.h"
#include "TemporalZonedDateTime.h"
#include "TimeZoneICUBridge.h"
#include <unicode/ucal.h>
#include <wtf/DateMath.h>
#include <wtf/unicode/icu/ICUHelpers.h>

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
    return JSValue::encode(TemporalInstant::tryCreateIfValid(globalObject, ISO8601::ExactTime::now()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.now.timezoneid
// https://tc39.es/proposal-temporal/#sec-temporal-systemtimezoneidentifier
JSC_DEFINE_HOST_FUNCTION(temporalNowFuncTimeZoneId, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    return JSValue::encode(jsNontrivialString(vm, vm.dateCache.defaultTimeZone().toString()));
}

// Validate and parse the optional time zone argument for Temporal.Now functions.
// Returns a TimeZone if the argument is a valid non-undefined timezone string.
// Returns nullopt if the argument is undefined (caller should use the system timezone).
// Throws TypeError if the argument is not a string and not undefined.
// Throws RangeError if the argument is a string but not a valid timezone identifier.
static std::optional<TimeZone> parseNowTimeZoneArgument(JSGlobalObject* globalObject, JSValue arg)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (arg.isUndefined())
        return std::nullopt;

    if (!arg.isString()) {
        throwTypeError(globalObject, scope, "Temporal.Now time zone argument must be a string"_s);
        return { };
    }

    auto tzString = asString(arg)->value(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    if (auto tz = ISO8601::parseTemporalTimeZoneIdentifier(tzString))
        return tz;

    throwRangeError(globalObject, scope, "argument is not a valid time zone identifier"_s);
    return { };
}

// Compute the UTC offset in nanoseconds for a TimeZone at the given epoch milliseconds.
// For UTC-offset timezones, returns the fixed offset directly.
// For named timezones, uses ICU to compute the UTC+DST offset at the given instant.
static int64_t getOffsetNanosecondsForTimeZone(const TimeZone& timeZone, double epochMs)
{
    if (timeZone.isUTCOffset())
        return timeZone.utcOffsetNanoseconds();

    // Named timezone: open a temporary ICU calendar and query the offset.
    String timeZoneForICU = timeZone.toICUString();
    StringView view(timeZoneForICU);
    auto upconverted = view.upconvertedCharacters();

    UErrorCode status = U_ZERO_ERROR;
    auto calendar = std::unique_ptr<UCalendar, ICUDeleter<ucal_close>>(
        ucal_open(upconverted, view.length(), "", UCAL_DEFAULT, &status));
    if (U_FAILURE(status))
        return 0;

    ucal_setMillis(calendar.get(), epochMs, &status);
    if (U_FAILURE(status))
        return 0;

    int32_t rawOffset = ucal_get(calendar.get(), UCAL_ZONE_OFFSET, &status);
    int32_t dstOffset = ucal_get(calendar.get(), UCAL_DST_OFFSET, &status);
    if (U_FAILURE(status))
        return 0;

    constexpr int64_t nsPerMs = 1'000'000;
    return static_cast<int64_t>(rawOffset + dstOffset) * nsPerMs;
}

// https://tc39.es/proposal-temporal/#sec-temporal.now.plaindateiso
JSC_DEFINE_HOST_FUNCTION(temporalNowFuncPlainDateISO, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto tzOpt = parseNowTimeZoneArgument(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    auto exactTime = ISO8601::ExactTime::now();
    ISO8601::PlainDate plainDate;

    if (!tzOpt) {
        // Use system timezone via DateCache.
        GregorianDateTime dt;
        vm.dateCache.msToGregorianDateTime(static_cast<double>(exactTime.floorEpochMilliseconds()), TimeType::LocalTime, dt);
        plainDate = ISO8601::PlainDate(dt.year(), static_cast<uint8_t>(dt.month() + 1), static_cast<uint8_t>(dt.monthDay()));
    } else {
        int64_t offsetNs = getOffsetNanosecondsForTimeZone(*tzOpt, static_cast<double>(exactTime.floorEpochMilliseconds()));
        ISO8601::PlainTime unusedTime;
        TemporalCore::exactTimeToLocalDateAndTime(exactTime, offsetNs, plainDate, unusedTime);
    }

    return JSValue::encode(TemporalPlainDate::create(vm, globalObject->plainDateStructure(), WTF::move(plainDate)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.now.plaindatetimeiso
JSC_DEFINE_HOST_FUNCTION(temporalNowFuncPlainDateTimeISO, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto tzOpt = parseNowTimeZoneArgument(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    auto exactTime = ISO8601::ExactTime::now();
    ISO8601::PlainDate plainDate;
    ISO8601::PlainTime plainTime;

    if (!tzOpt) {
        // Use system timezone via DateCache for millisecond resolution.
        GregorianDateTime dt;
        vm.dateCache.msToGregorianDateTime(static_cast<double>(exactTime.floorEpochMilliseconds()), TimeType::LocalTime, dt);
        // Reconstruct offset from the system timezone to reuse exactTimeToLocalDateAndTime for sub-ms.
        int64_t offsetMs = static_cast<int64_t>(dt.utcOffsetInMinute()) * 60 * 1000;
        TemporalCore::exactTimeToLocalDateAndTime(exactTime, offsetMs * 1'000'000, plainDate, plainTime);
    } else {
        int64_t offsetNs = getOffsetNanosecondsForTimeZone(*tzOpt, static_cast<double>(exactTime.floorEpochMilliseconds()));
        TemporalCore::exactTimeToLocalDateAndTime(exactTime, offsetNs, plainDate, plainTime);
    }

    return JSValue::encode(TemporalPlainDateTime::create(vm, globalObject->plainDateTimeStructure(), WTF::move(plainDate), WTF::move(plainTime)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.now.plaintimeiso
JSC_DEFINE_HOST_FUNCTION(temporalNowFuncPlainTimeISO, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto tzOpt = parseNowTimeZoneArgument(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    auto exactTime = ISO8601::ExactTime::now();
    ISO8601::PlainDate unusedDate;
    ISO8601::PlainTime plainTime;

    if (!tzOpt) {
        GregorianDateTime dt;
        vm.dateCache.msToGregorianDateTime(static_cast<double>(exactTime.floorEpochMilliseconds()), TimeType::LocalTime, dt);
        int64_t offsetMs = static_cast<int64_t>(dt.utcOffsetInMinute()) * 60 * 1000;
        TemporalCore::exactTimeToLocalDateAndTime(exactTime, offsetMs * 1'000'000, unusedDate, plainTime);
    } else {
        int64_t offsetNs = getOffsetNanosecondsForTimeZone(*tzOpt, static_cast<double>(exactTime.floorEpochMilliseconds()));
        TemporalCore::exactTimeToLocalDateAndTime(exactTime, offsetNs, unusedDate, plainTime);
    }

    return JSValue::encode(TemporalPlainTime::create(vm, globalObject->plainTimeStructure(), WTF::move(plainTime)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.now.zoneddatetimeiso
JSC_DEFINE_HOST_FUNCTION(temporalNowFuncZonedDateTimeISO, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto tzOpt = parseNowTimeZoneArgument(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    auto exactTime = ISO8601::ExactTime::now();

    TimeZone tz = tzOpt ? *tzOpt : vm.dateCache.defaultTimeZone();
    String tzId = tz.toString();

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalZonedDateTime::create(vm, globalObject->zonedDateTimeStructure(),
        exactTime, tz, WTF::move(tzId), "iso8601"_s)));
}

} // namespace JSC
