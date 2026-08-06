/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#include "TemporalDuration.h"

#include "DateConstructor.h"
#include "DurationArithmetic.h"
#include "FractionToDouble.h"
#include "ISOArithmetic.h"
#include "InternalFunction.h"
#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "PlainDateTimeCore.h"
#include "Rounding.h"
#include "TemporalCalendar.h"
#include "TemporalObject.h"
#include "TemporalPlainDate.h"
#include "TemporalPlainDateTime.h"
#include "TemporalPlainTime.h"
#include "TemporalZonedDateTime.h"
#include "TimeZoneICUBridge.h"
#include "ZonedDateTimeCore.h"
#include <wtf/DateMath.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace JSC {

const ClassInfo TemporalDuration::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(TemporalDuration) };

TemporalDuration* TemporalDuration::create(VM& vm, Structure* structure, ISO8601::Duration&& duration)
{
    auto* object = new (NotNull, allocateCell<TemporalDuration>(vm)) TemporalDuration(vm, structure, WTF::move(duration));
    object->finishCreation(vm);
    return object;
}

Structure* TemporalDuration::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalDuration::TemporalDuration(VM& vm, Structure* structure, ISO8601::Duration&& duration)
    : Base(vm, structure)
    , m_duration(WTF::move(duration))
{
}

// https://tc39.es/proposal-temporal/#sec-temporal-createtemporalduration
template<TemporalConstructTarget target>
static TemporalDuration* createTemporalDurationImpl(JSGlobalObject* globalObject, ISO8601::Duration&& duration, TemporalNewTarget newTarget = { })
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: If IsValidDuration(...) is false, throw a RangeError exception.
    if (!ISO8601::isValidDuration(duration)) [[unlikely]] {
        throwRangeError(globalObject, scope, "Temporal.Duration properties must be finite and of consistent sign"_s);
        return { };
    }

    // Step 2: If newTarget is not present, set newTarget to %Temporal.Duration%.
    // Step 3: Let object be ? OrdinaryCreateFromConstructor(newTarget, "%Temporal.Duration.prototype%", « ... »).
    Structure* structure;
    if constexpr (target == TemporalConstructTarget::Intrinsic)
        structure = globalObject->durationStructure();
    else {
        ASSERT(newTarget.newTarget && newTarget.constructor);
        structure = JSC_GET_DERIVED_STRUCTURE(vm, durationStructure, newTarget.newTarget, newTarget.constructor);
        RETURN_IF_EXCEPTION(scope, { });
    }

    // Steps 4-13: set [[Years]]…[[Nanoseconds]]. Step 14: Return object.
    return TemporalDuration::create(vm, structure, WTF::move(duration));
}

TemporalDuration* createTemporalDuration(JSGlobalObject* globalObject, ISO8601::Duration&& duration)
{
    return createTemporalDurationImpl<TemporalConstructTarget::Intrinsic>(globalObject, WTF::move(duration));
}

TemporalDuration* createTemporalDuration(JSGlobalObject* globalObject, ISO8601::Duration&& duration, TemporalNewTarget newTarget)
{
    return createTemporalDurationImpl<TemporalConstructTarget::NewTarget>(globalObject, WTF::move(duration), newTarget);
}

// ToTemporalPartialDurationRecord ( temporalDurationLike )
// https://tc39.es/proposal-temporal/#sec-temporal-totemporalpartialdurationrecord
static std::optional<std::array<std::optional<double>, numberOfTemporalUnits>>
toTemporalPartialDurationRecord(JSGlobalObject* globalObject, JSObject* durationLike)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 2: Let result be a new partial Duration Record with each field set to undefined.
    //   We model "undefined" with std::optional default-init (nullopt).
    std::array<std::optional<double>, numberOfTemporalUnits> result;

    // Step 3 NOTE: properties read in alphabetical order (days, hours, microseconds, ...).
    //   `temporalUnitsInTableOrder` is, despite its name, the spec's alphabetical order.
    // Steps 4-22 (per unit): Get(durationLike, "<unit>"); if !undefined, ? ToIntegerIfIntegral.
    bool any = false;
    for (TemporalUnit unit : temporalUnitsInTableOrder) {
        JSValue value = durationLike->get(globalObject, temporalUnitPluralPropertyName(vm, unit));
        RETURN_IF_EXCEPTION(scope, std::nullopt);
        if (value.isUndefined())
            continue;

        any = true;
        // ToIntegerIfIntegral: `+ 0.0` does step 4 (-0 → +0); !isInteger covers steps 2-3
        // (NaN/±∞ are non-finite, isInteger returns false).
        double v = value.toNumber(globalObject) + 0.0;
        RETURN_IF_EXCEPTION(scope, std::nullopt);
        if (!isInteger(v)) [[unlikely]] {
            throwRangeError(globalObject, scope, "Temporal.Duration properties must be integers"_s);
            return std::nullopt;
        }
        result[static_cast<size_t>(unit)] = v;
    }

    // Step 23: If all properties were undefined, throw TypeError.
    if (!any) [[unlikely]] {
        throwTypeError(globalObject, scope, "Object must contain at least one Temporal.Duration property"_s);
        return std::nullopt;
    }

    // Step 24: Return result.
    return result;
}

// ToTemporalDuration ( item ) — returns the raw Duration Record (skips JSCell wrap).
// https://tc39.es/proposal-temporal/#sec-temporal-totemporalduration
ISO8601::Duration TemporalDuration::toTemporalDurationRecord(JSGlobalObject* globalObject, JSValue itemValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: If item is Object and item has [[InitializedTemporalDuration]] internal slot,
    //   return ! CreateTemporalDuration(item.[[Years]], ..., item.[[Nanoseconds]]).
    if (itemValue.isObject()) {
        if (auto* duration = dynamicDowncast<TemporalDuration>(asObject(itemValue)))
            return duration->m_duration;

        // Steps 3-15 (Object branch): partial record extraction with default-0.
        // Step 4: ? ToTemporalPartialDurationRecord(item).
        auto partial = toTemporalPartialDurationRecord(globalObject, asObject(itemValue));
        RETURN_IF_EXCEPTION(scope, { });
        ASSERT(partial);

        // Step 3: result = new Partial Duration Record with each field set to 0.
        // Steps 5-14 (per unit): if partial.X is not undefined, result.X = partial.X.
        ISO8601::Duration result;
        for (size_t i = 0; i < numberOfTemporalUnits; ++i) {
            if ((*partial)[i].has_value())
                result.setField(i, *(*partial)[i]);
        }

        // Step 15: Return ! CreateTemporalDuration(...). IsValidDuration check below.
        if (!ISO8601::isValidDuration(result)) [[unlikely]] {
            throwRangeError(globalObject, scope, "Temporal.Duration properties must be finite and of consistent sign"_s);
            return { };
        }
        return result;
    }

    // Step 2.a: If item is not Object and not String, throw TypeError.
    if (!itemValue.isString()) [[unlikely]] {
        throwTypeError(globalObject, scope, "can only convert to Duration from object or string values"_s);
        return { };
    }

    // Step 2.b: Return ? ParseTemporalDurationString(item).
    String string = itemValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    auto parsed = ISO8601::parseDuration(string);
    if (!parsed) [[unlikely]] {
        // 3090: 308 digits * 10 fields + 10 designators
        throwRangeError(globalObject, scope, makeString("'"_s, ellipsizeAt(3090, string), "' is not a valid Duration string"_s));
        return { };
    }

    if (!ISO8601::isValidDuration(*parsed)) [[unlikely]] {
        throwRangeError(globalObject, scope, "Temporal.Duration properties must be finite and of consistent sign"_s);
        return { };
    }
    return *parsed;
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.from
TemporalDuration* TemporalDuration::from(JSGlobalObject* globalObject, JSValue itemValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Return ? ToTemporalDuration(item). Always returns a fresh cell — spec's
    //   CreateTemporalDuration constructs a new instance even when item already is one.
    auto record = toTemporalDurationRecord(globalObject, itemValue);
    RETURN_IF_EXCEPTION(scope, nullptr);
    return TemporalDuration::create(vm, globalObject->durationStructure(), WTF::move(record));
}

// https://tc39.es/proposal-temporal/#sec-temporal-add24hourdaystonormalizedtimeduration
static Int128 add24HourDaysToTimeDuration(JSGlobalObject* globalObject, Int128 d, double days)
{
    auto result = TemporalCore::add24HourDaysToTimeDuration(d, days);
    if (!result) {
        VM& vm = globalObject->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        throwRangeError(globalObject, scope, result.error().message);
        return { };
    }
    return *result;
}

// Helper: calendar-aware date addition. Uses calendarDateAdd for non-ISO, isoDateAdd for ISO.
// https://tc39.es/proposal-temporal/#sec-temporal-calendardateadd
static ISO8601::PlainDate calendarAwareDateAdd(JSGlobalObject* globalObject, CalendarID calendarId, const ISO8601::PlainDate& date, const ISO8601::Duration& duration, TemporalOverflow overflow)
{
    if (calendarId == iso8601CalendarID())
        return isoDateAdd(globalObject, date, duration, overflow);
    return calendarDateAdd(globalObject, calendarId, date, duration, overflow);
}

struct PlainRelativeTo {
    ISO8601::PlainDate date;
    CalendarID calendarId { iso8601CalendarID() };
};

struct RelativeToRecord {
    TemporalZonedDateTime* zonedRelativeTo { nullptr }; // [[ZonedRelativeTo]]; nullptr is undefined.
    std::optional<PlainRelativeTo> plainRelativeTo; // [[PlainRelativeTo]].
};

struct RelativeToParts {
    ISO8601::PlainDate isoDate;
    std::optional<ISO8601::PlainTime> time; // std::nullopt is ~start-of-day~.
    CalendarID calendarId { iso8601CalendarID() };
    std::optional<TimeZone> timeZone; // std::nullopt is ~unset~.
    int64_t offsetStringNs { 0 };
    OffsetBehaviour offsetBehaviour { OffsetBehaviour::Option };
    TemporalCore::MatchBehaviour matchBehaviour { TemporalCore::MatchBehaviour::MatchExactly };
};

// https://tc39.es/proposal-temporal/#sec-temporal-gettemporalrelativetooption
static RelativeToRecord getTemporalRelativeToOption(JSGlobalObject* globalObject, JSObject* options)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Let value be ? Get(options, "relativeTo").
    JSValue relativeToValue = options->get(globalObject, Identifier::fromString(vm, "relativeTo"_s));
    RETURN_IF_EXCEPTION(scope, { });

    // Step 2: If value is undefined, return the Record with both fields undefined.
    if (relativeToValue.isUndefined())
        return { };

    // Steps 3-4: offsetBehaviour = ~option~; matchBehaviour = ~match-exactly~.
    RelativeToParts parts;
    String string; // Set by step 6 only; used for its error messages.

    // Step 5: If value is an Object, then
    if (relativeToValue.isObject()) {
        JSObject* obj = asObject(relativeToValue);
        // Step 5.a: [[InitializedTemporalZonedDateTime]] → return { [[ZonedRelativeTo]]: value }.
        if (obj->inherits<TemporalZonedDateTime>())
            return RelativeToRecord { uncheckedDowncast<TemporalZonedDateTime>(obj), std::nullopt };

        // Step 5.b: [[InitializedTemporalDate]] → return { [[PlainRelativeTo]]: value }.
        if (obj->inherits<TemporalPlainDate>()) {
            auto* pd = uncheckedDowncast<TemporalPlainDate>(obj);
            return RelativeToRecord { nullptr, PlainRelativeTo { pd->plainDate(), pd->calendarID() } };
        }

        // Step 5.c: [[InitializedTemporalDateTime]] → return { [[PlainRelativeTo]]: CreateTemporalDate(...) }.
        if (obj->inherits<TemporalPlainDateTime>()) {
            auto* pdt = uncheckedDowncast<TemporalPlainDateTime>(obj);
            return RelativeToRecord { nullptr, PlainRelativeTo { pdt->plainDate(), pdt->calendarID() } };
        }

        // Step 5.d: calendar = ? GetTemporalCalendarIdentifierWithISODefault(value).
        parts.calendarId = getTemporalCalendarIdentifierWithISODefault(globalObject, obj);
        RETURN_IF_EXCEPTION(scope, { });

        // Step 5.e: fields = ? PrepareCalendarFields(calendar, value, «year,month,month-code,day», «hour,...,nanosecond,offset,time-zone», «»).
        auto fields = readZonedDateTimeFieldsFromObject<ZonedDateTimeFieldMode::RelativeToDuration>(globalObject, obj, parts.calendarId);
        RETURN_IF_EXCEPTION(scope, { });

        // Step 5.f: result = ? InterpretTemporalDateTimeFields(calendar, fields, ~constrain~).
        TemporalCore::TimeFieldsIn timeFields { fields.hour, fields.minute, fields.second, fields.millisecond, fields.microsecond, fields.nanosecond };
        auto pdt = interpretTemporalDateTimeFields(globalObject, parts.calendarId, fields.dateFields, timeFields, TemporalOverflow::Constrain);
        RETURN_IF_EXCEPTION(scope, { });

        // Step 5.g: Let timeZone be fields.[[TimeZone]].
        if (fields.timeZonePresent)
            parts.timeZone = fields.timeZone;

        // Steps 5.h-i: offsetString = fields.[[OffsetString]]; if ~unset~, offsetBehaviour = ~wall~.
        if (fields.offsetNs)
            parts.offsetStringNs = *fields.offsetNs;
        else
            parts.offsetBehaviour = OffsetBehaviour::Wall;

        // Steps 5.j-k: isoDate = result.[[ISODate]]; time = result.[[Time]], never ~start-of-day~.
        parts.isoDate = pdt.date;
        parts.time = pdt.time;
    } else {
        // Step 6.a: If value is not a String, throw a TypeError exception.
        if (!relativeToValue.isString()) [[unlikely]] {
            throwTypeError(globalObject, scope, "relativeTo must be a string or Temporal object"_s);
            return { };
        }

        // Step 6.b: result = ? ParseISODateTime(value, « TemporalDateTimeString[+Zoned], TemporalDateTimeString[~Zoned] »).
        string = relativeToValue.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        auto parsed = ISO8601::parseISODateTime(string, { ISO8601::TemporalProduction::DateTimeZoned, ISO8601::TemporalProduction::DateTimeUnzoned });
        if (!parsed) [[unlikely]] {
            throwRangeError(globalObject, scope, makeString("'"_s, ellipsizeAt(200, string), "' is not a valid date or ZonedDateTime string"_s));
            return { };
        }
        auto& [parsedDateOpt, parsedTimeOpt, parsedTzOpt, parsedCalOpt, matched, isShortForm] = *parsed;
        ASSERT(parsedDateOpt);

        // Steps 6.c-6.f: the annotation is ~empty~ — so timeZone stays ~unset~ — exactly when the ~Zoned production matched, since only [+Zoned] accepts a bracket.
        if (matched == ISO8601::TemporalProduction::DateTimeZoned) {
            ASSERT(parsedTzOpt);
            auto& tzRecord = *parsedTzOpt;
            // Step 6.f.i: timeZone = ? ToTemporalTimeZoneIdentifier(annotation).
            auto timeZoneOpt = timeZoneFromRecord(tzRecord);
            if (!timeZoneOpt) [[unlikely]] {
                throwRangeError(globalObject, scope, makeString("'"_s, ellipsizeAt(200, string), "' contains an invalid time zone identifier"_s));
                return { };
            }
            parts.timeZone = *timeZoneOpt;

            // Steps 6.f.ii-iii: Z → ~exact~; no offsetString → ~wall~.
            if (tzRecord.m_z)
                parts.offsetBehaviour = OffsetBehaviour::Exact;
            else if (!tzRecord.m_offset)
                parts.offsetBehaviour = OffsetBehaviour::Wall;
            parts.offsetStringNs = tzRecord.m_offset.value_or(0);

            // Steps 6.f.iv-v: ~match-minutes~, unless the offset has sub-minute precision.
            parts.matchBehaviour = tzRecord.m_offsetHasSubMinutePrecision ? TemporalCore::MatchBehaviour::MatchExactly : TemporalCore::MatchBehaviour::MatchMinutes;
        }

        // Steps 6.g-i: calendar = result.[[Calendar]]; if ~empty~ → "iso8601"; CanonicalizeCalendar.
        if (parsedCalOpt) {
            auto rawCal = StringView(*parsedCalOpt).convertToASCIILowercase();
            auto canonicalized = isBuiltinCalendar(rawCal);
            if (!canonicalized) [[unlikely]] {
                throwRangeError(globalObject, scope, makeString("'"_s, rawCal, "' is not a valid calendar identifier"_s));
                return { };
            }
            parts.calendarId = *canonicalized;
        }

        // Steps 6.j-k: isoDate = CreateISODateRecord(...); time = result.[[Time]], which is ~start-of-day~ when the string had no time part.
        parts.isoDate = *parsedDateOpt;
        parts.time = parsedTimeOpt;
    }

    // Step 7: If timeZone is ~unset~, plainDate = ? CreateTemporalDate(isoDate, calendar); return.
    if (!parts.timeZone) {
        // CreateTemporalDate Step 1. Step 5.f already made the identical check via CalendarDateFromFields Step 3, so only a step 6 string can reach the throw.
        if (!ISO8601::isDateTimeWithinLimits(parts.isoDate.year(), parts.isoDate.month(), parts.isoDate.day(), 12, 0, 0, 0, 0, 0)) [[unlikely]] {
            ASSERT(!string.isNull());
            throwRangeError(globalObject, scope, makeString("'"_s, ellipsizeAt(200, string), "' is outside the representable range for a relativeTo parameter"_s));
            return { };
        }
        return RelativeToRecord { nullptr, PlainRelativeTo { parts.isoDate, parts.calendarId } };
    }

    // Steps 8-9: offsetNs = offsetBehaviour is ~option~ ? ParseDateTimeUTCOffset(offsetString) : 0.
    int64_t offsetNs = parts.offsetBehaviour == OffsetBehaviour::Option ? parts.offsetStringNs : 0;

    // Step 10: epochNanoseconds = ? InterpretISODateTimeOffset(isoDate, time, offsetBehaviour, offsetNs, timeZone, ~compatible~, ~reject~, matchBehaviour).
    auto useStartOfDay = parts.time ? TemporalCore::UseStartOfDay::No : TemporalCore::UseStartOfDay::Yes;
    auto epochNsResult = TemporalCore::interpretISODateTimeOffset(parts.isoDate, parts.time.value_or(ISO8601::PlainTime()),
        useStartOfDay, parts.offsetBehaviour, TemporalOffsetDisambiguation::Reject, offsetNs,
        parts.matchBehaviour, *parts.timeZone, TemporalDisambiguation::Compatible);
    if (!epochNsResult) [[unlikely]] {
        throwRangeError(globalObject, scope, epochNsResult.error().message);
        return { };
    }

    // Steps 11-12: zonedRelativeTo = ! CreateTemporalZonedDateTime(...); return.
    auto* zdt = TemporalZonedDateTime::create(vm, globalObject->zonedDateTimeStructure(), *epochNsResult, WTF::move(*parts.timeZone), parts.calendarId);
    return RelativeToRecord { zdt, std::nullopt };
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.compare
JSValue TemporalDuration::compare(JSGlobalObject* globalObject, JSValue valueOne, JSValue valueTwo, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: one = ? ToTemporalDuration(one); two = ? ToTemporalDuration(two).
    //   We use the raw record form — compare only reads fields, no JSCell needed.
    auto one = toTemporalDurationRecord(globalObject, valueOne);
    RETURN_IF_EXCEPTION(scope, { });
    auto two = toTemporalDurationRecord(globalObject, valueTwo);
    RETURN_IF_EXCEPTION(scope, { });

    // Always validate options type (even if we don't need relativeTo).
    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });

    // Parse relativeTo — must happen before identity check per spec ordering.
    RelativeToRecord relativeTo;
    if (options) {
        relativeTo = getTemporalRelativeToOption(globalObject, options);
        RETURN_IF_EXCEPTION(scope, { });
    }

    // After parsing relativeTo, identical durations always compare equal.
    if (one.years() == two.years()
        && one.months() == two.months()
        && one.weeks() == two.weeks() && one.days() == two.days()
        && one.hours() == two.hours() && one.minutes() == two.minutes()
        && one.seconds() == two.seconds() && one.milliseconds() == two.milliseconds()
        && one.microseconds() == two.microseconds() && one.nanoseconds() == two.nanoseconds()) {
        return jsNumber(0);
    }

    // ZDT path: use addZonedDateTime when either duration has a non-zero date-category unit
    // (years, months, weeks, or days). Pure time durations don't need TZ-aware comparison.
    bool hasDateUnit1 = one.years() || one.months() || one.weeks() || one.days();
    bool hasDateUnit2 = two.years() || two.months() || two.weeks() || two.days();
    if (relativeTo.zonedRelativeTo && (hasDateUnit1 || hasDateUnit2)) {
        auto* zdt = relativeTo.zonedRelativeTo;
        auto startExact = zdt->exactTime();
        auto endTime1 = TemporalCore::addZonedDateTime(startExact, zdt->timeZone(), one, TemporalOverflow::Constrain, zdt->calendarID());
        if (!endTime1) [[unlikely]] {
            throwRangeError(globalObject, scope, endTime1.error().message);
            return { };
        }
        auto endTime2 = TemporalCore::addZonedDateTime(startExact, zdt->timeZone(), two, TemporalOverflow::Constrain, zdt->calendarID());
        if (!endTime2) [[unlikely]] {
            throwRangeError(globalObject, scope, endTime2.error().message);
            return { };
        }
        Int128 ns1 = endTime1->epochNanoseconds();
        Int128 ns2 = endTime2->epochNanoseconds();
        return jsNumber(ns1 > ns2 ? 1 : ns1 < ns2 ? -1 : 0);
    }

    // Fast path: no ZDT addZonedDateTime needed — pure 24h-day time comparison.
    bool hasCalendarUnits = one.years() || two.years() || one.months() || two.months() || one.weeks() || two.weeks();
    if (!hasCalendarUnits) {
        auto timeDuration1 = add24HourDaysToTimeDuration(globalObject, TemporalCore::toInternalDuration(one).time(), one.days());
        RETURN_IF_EXCEPTION(scope, { });
        auto timeDuration2 = add24HourDaysToTimeDuration(globalObject, TemporalCore::toInternalDuration(two).time(), two.days());
        RETURN_IF_EXCEPTION(scope, { });
        return jsNumber(timeDuration1 > timeDuration2 ? 1 : timeDuration1 < timeDuration2 ? -1 : 0);
    }

    if (!relativeTo.plainRelativeTo) [[unlikely]] {
        throwRangeError(globalObject, scope, "Cannot compare a duration of years, months, or weeks without a relativeTo option"_s);
        return { };
    }

    // PlainDate relativeTo: DateDurationDays(dateDuration, plainDate).
    auto& plainDate = relativeTo.plainRelativeTo->date;
    auto calendarId = relativeTo.plainRelativeTo->calendarId;

    ISO8601::Duration dateDuration1(one.years(), one.months(), one.weeks(), one.days(), 0LL, 0LL, 0LL, 0LL, Int128(0), Int128(0));
    auto endDate1 = calendarAwareDateAdd(globalObject, calendarId, plainDate, dateDuration1, TemporalOverflow::Constrain);
    RETURN_IF_EXCEPTION(scope, { });
    auto daysDiff1 = TemporalCore::diffISODate(plainDate, endDate1, TemporalUnit::Day);
    auto timeDuration1 = add24HourDaysToTimeDuration(globalObject, TemporalCore::toInternalDuration(one).time(), daysDiff1.days());
    RETURN_IF_EXCEPTION(scope, { });

    ISO8601::Duration dateDuration2(two.years(), two.months(), two.weeks(), two.days(), 0LL, 0LL, 0LL, 0LL, Int128(0), Int128(0));
    auto endDate2 = calendarAwareDateAdd(globalObject, calendarId, plainDate, dateDuration2, TemporalOverflow::Constrain);
    RETURN_IF_EXCEPTION(scope, { });
    auto daysDiff2 = TemporalCore::diffISODate(plainDate, endDate2, TemporalUnit::Day);
    auto timeDuration2 = add24HourDaysToTimeDuration(globalObject, TemporalCore::toInternalDuration(two).time(), daysDiff2.days());
    RETURN_IF_EXCEPTION(scope, { });

    return jsNumber(timeDuration1 > timeDuration2 ? 1 : timeDuration1 < timeDuration2 ? -1 : 0);
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.with
ISO8601::Duration TemporalDuration::with(JSGlobalObject* globalObject, JSObject* durationLike) const
{
    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());

    // Step 3: Let temporalDurationLike be ? ToTemporalPartialDurationRecord(temporalDurationLike).
    auto partial = toTemporalPartialDurationRecord(globalObject, durationLike);
    RETURN_IF_EXCEPTION(scope, { });
    ASSERT(partial);

    // Steps 4-23 (per unit): if partial.X is not undefined, x = partial.X; else x = duration.X.
    ISO8601::Duration result;
    for (size_t i = 0; i < numberOfTemporalUnits; ++i)
        result.setField(i, (*partial)[i].value_or(m_duration[i]));

    // Step 24: Return ! CreateTemporalDuration(...).
    return result;
}

// https://tc39.es/proposal-temporal/#sec-temporal-tointernaldurationrecordwith24hourdays
ISO8601::InternalDuration TemporalDuration::toInternalDurationRecordWith24HourDays(JSGlobalObject* globalObject,
    ISO8601::Duration d)
{
    auto result = TemporalCore::toInternalDurationRecordWith24HourDays(d);
    if (!result) {
        VM& vm = globalObject->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        throwRangeError(globalObject, scope, result.error().message);
        return { };
    }
    return *result;
}

// https://tc39.es/proposal-temporal/#sec-temporal-todatedurationrecordwithouttime
ISO8601::Duration TemporalDuration::toDateDurationRecordWithoutTime(JSGlobalObject* globalObject, const ISO8601::Duration& duration)
{
    auto result = TemporalCore::toDateDurationRecordWithoutTime(duration);
    if (!result) {
        VM& vm = globalObject->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        throwRangeError(globalObject, scope, result.error().message);
        return { };
    }
    return *result;
}

// AddDurations ( operation, duration, other )
// https://tc39.es/proposal-temporal/#sec-temporal-adddurations
template<AddOrSubtract op>
ISO8601::Duration TemporalDuration::addDurations(JSGlobalObject* globalObject, JSValue otherValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Set other to ? ToTemporalDuration(other).
    auto other = toTemporalDurationRecord(globalObject, otherValue);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 2: If operation is subtract, set other to CreateNegatedTemporalDuration(other).
    if constexpr (op == AddOrSubtract::Subtract)
        other = -other;

    // Steps 3-5: largestUnit = LargerOfTwoTemporalUnits(
    //   DefaultTemporalLargestUnit(duration), DefaultTemporalLargestUnit(other)).
    //   In our enum, smaller index = larger unit (Year=0..Nanosecond=9), so std::min.
    auto largestUnit = std::min(TemporalCore::largestSubduration(m_duration),
        TemporalCore::largestSubduration(other));

    // Step 6: If IsCalendarUnit(largestUnit) is true, throw a RangeError exception.
    if (isCalendarUnit(largestUnit)) [[unlikely]] {
        throwRangeError(globalObject, scope, "Cannot add or subtract durations with calendar units (years, months, or weeks)"_s);
        return { };
    }

    // Step 7: Let d1 be ToInternalDurationRecordWith24HourDays(duration).
    auto d1 = toInternalDurationRecordWith24HourDays(globalObject, m_duration);
    RETURN_IF_EXCEPTION(scope, { });
    // Step 8: Let d2 be ToInternalDurationRecordWith24HourDays(other).
    auto d2 = toInternalDurationRecordWith24HourDays(globalObject, other);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 9: Let timeResult be ? AddTimeDuration(d1.[[Time]], d2.[[Time]]).
    //   AddTimeDuration is `a + b` followed by an overflow check against maxTimeDuration.
    auto timeResult = d1.time() + d2.time();
    if (absInt128(timeResult) > ISO8601::InternalDuration::maxTimeDuration) [[unlikely]] {
        throwRangeError(globalObject, scope, "Sum of durations exceeds maximum time duration"_s);
        return { };
    }

    // Step 10: Let result be CombineDateAndTimeDuration(ZeroDateDuration(), timeResult).
    auto result = ISO8601::InternalDuration::combineDateAndTimeDuration(ISO8601::Duration(), timeResult);

    // Step 11: Return ? TemporalDurationFromInternal(result, largestUnit).
    auto durResult = TemporalCore::temporalDurationFromInternal(result, largestUnit);
    if (!durResult) [[unlikely]] {
        throwTemporalError(globalObject, scope, durResult.error());
        return { };
    }
    return *durResult;
}

template ISO8601::Duration TemporalDuration::addDurations<AddOrSubtract::Add>(JSGlobalObject*, JSValue) const;
template ISO8601::Duration TemporalDuration::addDurations<AddOrSubtract::Subtract>(JSGlobalObject*, JSValue) const;

// Day/time rounding dispatcher for the no-relativeTo path of round/total, and the time-only ZDT fast path.
// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.round
ISO8601::InternalDuration TemporalDuration::round(JSGlobalObject* globalObject, ISO8601::InternalDuration internalDuration, double increment, TemporalUnit unit, RoundingMode mode)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(unit >= TemporalUnit::Day);

    // Step 32: smallestUnit is ~day~.
    if (unit == TemporalUnit::Day) {
        double fractionalDays = TemporalCore::totalTimeDuration(internalDuration.time(), TemporalUnit::Day);
        double days = TemporalCore::roundNumberToIncrementDouble(fractionalDays, increment, mode);
        return ISO8601::InternalDuration::combineDateAndTimeDuration(
            ISO8601::Duration { 0LL, 0LL, 0LL, static_cast<int64_t>(days), 0LL, 0LL, 0LL, 0LL, Int128(0), Int128(0) },
            0);
    }

    // Step 33: time unit — RoundTimeDuration + CombineDateAndTimeDuration.
    std::optional<Int128> timeDuration = ISO8601::roundTimeDuration(globalObject, internalDuration.time(), increment, unit, mode);
    RETURN_IF_EXCEPTION(scope, { });
    if (!timeDuration) [[unlikely]] {
        throwRangeError(globalObject, scope, "Rounded duration exceeds maximum time duration"_s);
        return { };
    }
    return ISO8601::InternalDuration::combineDateAndTimeDuration(ISO8601::Duration(), timeDuration.value());
}

struct PlainRelativeTarget {
    ISO8601::PlainDate targetDate;
    ISO8601::PlainTime targetTime;
};

static std::optional<PlainRelativeTarget> computePlainRelativeTarget(JSGlobalObject* globalObject, CalendarID calendarId, const ISO8601::PlainDate& plainDate, const ISO8601::InternalDuration& internalDuration)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto intermediateDate = calendarAwareDateAdd(globalObject, calendarId, plainDate, internalDuration.dateDuration(), TemporalOverflow::Constrain);
    RETURN_IF_EXCEPTION(scope, std::nullopt);

    auto [overflowDays, subdayNs] = TemporalCore::splitTimeDuration(internalDuration.time());
    ISO8601::Duration dayDuration(0LL, 0LL, 0LL, static_cast<int64_t>(overflowDays), 0LL, 0LL, 0LL, 0LL, Int128(0), Int128(0));
    auto targetDate = calendarAwareDateAdd(globalObject, calendarId, intermediateDate, dayDuration, TemporalOverflow::Constrain);
    RETURN_IF_EXCEPTION(scope, std::nullopt);

    return PlainRelativeTarget { WTF::move(targetDate), TemporalCore::plainTimeFromSubdayNs(subdayNs) };
}

struct ZonedRelativeEndpoints {
    ISO8601::ExactTime startExact;
    ISO8601::ExactTime endExact;
    Int128 nsA;
    Int128 nsB;
};

static std::optional<ZonedRelativeEndpoints> computeZonedRelativeEndpoints(JSGlobalObject* globalObject, TemporalZonedDateTime* zdt, const ISO8601::Duration& duration)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto startExact = zdt->exactTime();
    auto& tz = zdt->timeZone();
    auto endExactResult = TemporalCore::addZonedDateTime(startExact, tz, duration, TemporalOverflow::Constrain, zdt->calendarID());
    if (!endExactResult) [[unlikely]] {
        throwRangeError(globalObject, scope, endExactResult.error().message);
        return std::nullopt;
    }
    auto endExact = *endExactResult;
    return ZonedRelativeEndpoints { startExact, endExact, startExact.epochNanoseconds(), endExact.epochNanoseconds() };
}

// https://tc39.es/proposal-temporal/#sec-temporal-differencezoneddatetimewithtotal
static std::optional<double> differenceZonedDateTimeWithTotal(JSGlobalObject* globalObject, TemporalZonedDateTime* zdt, const ZonedRelativeEndpoints& endpoints, TemporalUnit unit)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: If TemporalUnitCategory(unit) is ~time~, return TotalTimeDuration.
    if (unit > TemporalUnit::Day)
        return TemporalCore::totalTimeDuration(endpoints.nsB - endpoints.nsA, unit);

    auto& tz = zdt->timeZone();
    // Step 2: difference = ? DifferenceZonedDateTime(nsA, nsB, tz, cal, unit).
    auto zdtDiffResult = TemporalCore::differenceZonedDateTimeWithRounding(endpoints.startExact, endpoints.endExact, tz, unit, unit, RoundingMode::Trunc, 1.0, zdt->calendarID());
    if (!zdtDiffResult) [[unlikely]] {
        throwTemporalError(globalObject, scope, zdtDiffResult.error());
        return std::nullopt;
    }

    // Step 3: dateTime = GetISODateTimeFor(tz, nsA).
    auto isoDTResult = TemporalCore::getISODateTimeFor(tz, endpoints.startExact);
    if (!isoDTResult) [[unlikely]] {
        throwRangeError(globalObject, scope, isoDTResult.error().message);
        return std::nullopt;
    }
    auto [startDate, startTime] = *isoDTResult;

    // Step 4: Return ? TotalRelativeDuration — via nudgeToCalendarUnit with Trunc/1.0.
    int32_t sign = (endpoints.nsB > endpoints.nsA) ? 1 : (endpoints.nsB < endpoints.nsA) ? -1 : 1;
    auto nudgedResult = TemporalCore::nudgeToCalendarUnit(sign, *zdtDiffResult, endpoints.nsA, endpoints.nsB, startDate, startTime, 1.0, unit, RoundingMode::Trunc, &tz, zdt->calendarID());
    if (!nudgedResult) [[unlikely]] {
        throwTemporalError(globalObject, scope, nudgedResult.error());
        return std::nullopt;
    }
    return nudgedResult->total;
}

// https://tc39.es/proposal-temporal/#sec-temporal-differenceplaindatetimewithtotal
static std::optional<double> differencePlainDateTimeWithTotal(JSGlobalObject* globalObject, CalendarID calendarId, const ISO8601::PlainDate& plainDate, const PlainRelativeTarget& target, TemporalUnit unit)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 5-6: epoch-ns for dt1 and dt2 (hoisted; reused by the CompareISODateTime check below).
    ISO8601::PlainTime midnight;
    Int128 originEpochNs = TemporalCore::getUTCEpochNanoseconds(plainDate, midnight);
    Int128 destEpochNs = TemporalCore::getUTCEpochNanoseconds(target.targetDate, target.targetTime);

    // Step 1: If CompareISODateTime(dt1, dt2) = 0, return 0.
    if (originEpochNs == destEpochNs)
        return 0.0;

    // Step 2: If !ISODateTimeWithinLimits(dt1) || !ISODateTimeWithinLimits(dt2), throw RangeError.
    bool dtOutOfRange = !ISO8601::isDateTimeWithinLimits(plainDate.year(), plainDate.month(), plainDate.day(), 0, 0, 0, 0, 0, 0)
        || !ISO8601::isDateTimeWithinLimits(target.targetDate.year(), target.targetDate.month(), target.targetDate.day(),
            target.targetTime.hour(), target.targetTime.minute(), target.targetTime.second(),
            target.targetTime.millisecond(), target.targetTime.microsecond(), target.targetTime.nanosecond());
    if (dtOutOfRange) [[unlikely]] {
        throwRangeError(globalObject, scope, "date time is out of range of ECMAScript representation"_s);
        return std::nullopt;
    }

    // Step 7: Return ? TotalRelativeDuration(diff, origin, dest, dt1, ~unset~, calendar, unit).
    //   PlainDateTime relativeTo has no timeZone, so Day falls into the non-calendar (epoch-ns) path.
    if (unit >= TemporalUnit::Day)
        return TemporalCore::totalTimeDuration(destEpochNs - originEpochNs, unit);

    auto diff = TemporalCore::diffISODateTime(plainDate, midnight, target.targetDate, target.targetTime, unit);
    int32_t sign = (destEpochNs > originEpochNs) ? 1 : (destEpochNs < originEpochNs) ? -1 : 1;
    auto nudgedResult = TemporalCore::nudgeToCalendarUnit(sign, diff, originEpochNs, destEpochNs,
        plainDate, midnight, 1.0, unit, RoundingMode::Trunc, nullptr, calendarId);
    if (!nudgedResult) [[unlikely]] {
        throwTemporalError(globalObject, scope, nudgedResult.error());
        return std::nullopt;
    }
    return nudgedResult->total;
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.round
ISO8601::Duration TemporalDuration::round(JSGlobalObject* globalObject, JSValue optionsValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: branding check done by the caller.

    // Step 3: If roundTo is undefined, throw a TypeError exception.
    if (optionsValue.isUndefined()) [[unlikely]] {
        throwTypeError(globalObject, scope, "Temporal.Duration.prototype.round requires a roundTo option"_s);
        return { };
    }

    JSObject* options = nullptr;
    std::optional<TemporalUnit> smallest;

    if (optionsValue.isString()) {
        // Step 4: If roundTo is a String, parse smallestUnit directly (optimisation — skip wrapper object).
        auto string = optionsValue.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        smallest = temporalUnitType(string);
        if (!smallest) [[unlikely]] {
            throwRangeError(globalObject, scope, "smallestUnit is an invalid Temporal unit"_s);
            return { };
        }
    } else {
        // Step 5: Set roundTo to ? GetOptionsObject(roundTo).
        options = intlGetOptionsObject(globalObject, optionsValue);
        RETURN_IF_EXCEPTION(scope, { });
    }

    // Step 6: Let smallestUnitPresent be true.
    // Step 7: Let largestUnitPresent be true.
    bool smallestUnitPresent = true;
    bool largestUnitPresent = true;

    // Step 8: NOTE: The following steps read options in alphabetical order.
    // Step 9: Let largestUnit be ? GetTemporalUnitValuedOption(roundTo, "largestUnit", ~unset~).
    auto largestUnitMaybeAuto = temporalUnitValued(globalObject, options, vm.propertyNames->largestUnit);
    RETURN_IF_EXCEPTION(scope, { });
    // Steps 10-12: Let relativeToRecord = ? GetTemporalRelativeToOption(roundTo).
    RelativeToRecord relativeTo;
    if (options) {
        relativeTo = getTemporalRelativeToOption(globalObject, options);
        RETURN_IF_EXCEPTION(scope, { });
    }
    // Step 13: Let roundingIncrement be ? GetRoundingIncrementOption(roundTo).
    auto roundingIncrement = temporalRoundingIncrement(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });
    // Step 14: Let roundingMode be ? GetRoundingModeOption(roundTo, ~half-expand~).
    auto roundingMode = temporalRoundingMode(globalObject, options, RoundingMode::HalfExpand);
    RETURN_IF_EXCEPTION(scope, { });

    if (!smallest) {
        // Step 15: Let smallestUnit be ? GetTemporalUnitValuedOption(roundTo, "smallestUnit", ~unset~).
        auto smallestUnitMaybeAuto = temporalUnitValued(globalObject, options, vm.propertyNames->smallestUnit);
        RETURN_IF_EXCEPTION(scope, { });
        // Step 16: Perform ? ValidateTemporalUnitValue(smallestUnit, ~datetime~).
        validateTemporalUnitValue(globalObject, smallestUnitMaybeAuto, UnitGroup::DateTime, AllowedUnit::None, "smallestUnit"_s);
        RETURN_IF_EXCEPTION(scope, { });
        auto smallestUnitOptional = std::get<std::optional<TemporalUnit>>(smallestUnitMaybeAuto);
        if (smallestUnitOptional)
            smallest = smallestUnitOptional.value();
    } else {
        // Step 16 (string path): Perform ? ValidateTemporalUnitValue(smallestUnit, ~datetime~).
        validateTemporalUnitValue(globalObject, smallest.value(), UnitGroup::DateTime, AllowedUnit::None, "smallestUnit"_s);
        RETURN_IF_EXCEPTION(scope, { });
    }

    // Step 17: If smallestUnit is ~unset~, set smallestUnitPresent = false, smallestUnit = ~nanosecond~.
    auto smallestUnit = TemporalUnit::Nanosecond;
    if (!smallest)
        smallestUnitPresent = false;
    else
        smallestUnit = smallest.value();

    // Step 18: Let existingLargestUnit be DefaultTemporalLargestUnit(duration).
    auto existingLargestUnit = TemporalCore::largestSubduration(m_duration);
    // Step 19: Let defaultLargestUnit be LargerOfTwoTemporalUnits(existingLargestUnit, smallestUnit).
    auto defaultLargestUnit = std::min(existingLargestUnit, smallestUnit);

    // Steps 20-21: Set largestUnit from ~unset~/~auto~/explicit.
    auto largestUnit = defaultLargestUnit;
    if (isAbsentUnit(largestUnitMaybeAuto))
        largestUnitPresent = false; // Step 20: ~unset~ → largestUnitPresent = false, largestUnit = defaultLargestUnit.
    else if (std::holds_alternative<std::optional<TemporalUnit>>(largestUnitMaybeAuto))
        largestUnit = std::get<std::optional<TemporalUnit>>(largestUnitMaybeAuto).value();
    // Step 21: ~auto~ → largestUnit = defaultLargestUnit (already set above).

    // Step 22: If smallestUnitPresent is false and largestUnitPresent is false, throw a RangeError.
    if (!smallestUnitPresent && !largestUnitPresent) [[unlikely]] {
        throwRangeError(globalObject, scope, "Cannot round without a smallestUnit or largestUnit option"_s);
        return { };
    }

    // Step 23: If LargerOfTwoTemporalUnits(largestUnit, smallestUnit) is not largestUnit, throw a RangeError.
    if (smallestUnit < largestUnit) [[unlikely]] {
        throwRangeError(globalObject, scope, "smallestUnit must be smaller than largestUnit"_s);
        return { };
    }

    // Step 24: Let maximum be MaximumTemporalDurationRoundingIncrement(smallestUnit).
    // Step 25: If maximum is not ~unset~, perform ? ValidateTemporalRoundingIncrement(roundingIncrement, maximum, false).
    auto maximum = TemporalCore::maximumRoundingIncrement(smallestUnit);
    if (maximum) {
        validateTemporalRoundingIncrement(globalObject, roundingIncrement, static_cast<double>(*maximum), Inclusivity::Exclusive);
        RETURN_IF_EXCEPTION(scope, { });
    }

    // Step 26: If roundingIncrement > 1, largestUnit ≠ smallestUnit, and smallestUnit is a date unit, throw a RangeError.
    if (roundingIncrement > 1 && largestUnit != smallestUnit && smallestUnit <= TemporalUnit::Day) [[unlikely]] {
        throwRangeError(globalObject, scope, "Incompatible rounding increment and largest/smallest units"_s);
        return { };
    }

    // Step 27: If zonedRelativeTo is not undefined:
    if (relativeTo.zonedRelativeTo) {
        auto* zdt = relativeTo.zonedRelativeTo;
        // Steps 27.a-e: compute relative endpoints (ToInternalDurationRecord + AddZonedDateTime).
        auto endpoints = computeZonedRelativeEndpoints(globalObject, zdt, m_duration);
        RETURN_IF_EXCEPTION(scope, { });
        ASSERT(endpoints);
        auto& tz = zdt->timeZone();

        // Step 27 fast path: time-only output bypasses calendar diff.
        if (largestUnit > TemporalUnit::Day) {
            auto zdtInternal = ISO8601::InternalDuration::combineDateAndTimeDuration(ISO8601::Duration(), endpoints->nsB - endpoints->nsA);
            RETURN_IF_EXCEPTION(scope, { });
            zdtInternal = round(globalObject, zdtInternal, roundingIncrement, smallestUnit, roundingMode);
            RETURN_IF_EXCEPTION(scope, { });
            auto durResult = TemporalCore::temporalDurationFromInternal(zdtInternal, largestUnit);
            if (!durResult) [[unlikely]] {
                throwTemporalError(globalObject, scope, durResult.error());
                return { };
            }
            return *durResult;
        }

        // Step 27.f: internalDuration = ? DifferenceZonedDateTimeWithRounding(...).
        auto zdtDiffResult = TemporalCore::differenceZonedDateTimeWithRounding(endpoints->startExact, endpoints->endExact, tz, largestUnit, smallestUnit, roundingMode, static_cast<double>(roundingIncrement), zdt->calendarID());
        if (!zdtDiffResult) [[unlikely]] {
            throwTemporalError(globalObject, scope, zdtDiffResult.error());
            return { };
        }
        // Step 27.g: If TemporalUnitCategory(largestUnit) is ~date~, set largestUnit to ~hour~.
        TemporalUnit effectiveLargestUnit = (largestUnit <= TemporalUnit::Day) ? TemporalUnit::Hour : largestUnit;
        // Step 27.h: Return ? TemporalDurationFromInternal(internalDuration, largestUnit).
        auto durResult = TemporalCore::temporalDurationFromInternal(*zdtDiffResult, effectiveLargestUnit);
        if (!durResult) [[unlikely]] {
            throwTemporalError(globalObject, scope, durResult.error());
            return { };
        }
        return *durResult;
    }

    // Step 28: If plainRelativeTo is not undefined:
    if (relativeTo.plainRelativeTo) {
        auto& plainDate = relativeTo.plainRelativeTo->date;
        auto calendarId = relativeTo.plainRelativeTo->calendarId;
        ISO8601::PlainTime midnight;

        // Step 28.a: internalDuration = ToInternalDurationRecordWith24HourDays(duration).
        auto internalDuration = toInternalDurationRecordWith24HourDays(globalObject, m_duration);
        RETURN_IF_EXCEPTION(scope, { });

        // Steps 28.b-e: AddTime + AdjustDateDurationRecord + CalendarDateAdd → {targetDate, targetTime}.
        auto target = computePlainRelativeTarget(globalObject, calendarId, plainDate, internalDuration);
        RETURN_IF_EXCEPTION(scope, { });
        ASSERT(target);

        // Steps 28.f-g: isoDateTime = (plainDate, midnight); targetDateTime = (targetDate, targetTime).
        // Step 28.h: internalDuration = ? DifferencePlainDateTimeWithRounding(...).
        auto diffResult = TemporalCore::differencePlainDateTimeWithRounding(plainDate, midnight, target->targetDate, target->targetTime,
            calendarId, largestUnit, smallestUnit, roundingMode, static_cast<double>(roundingIncrement));
        if (!diffResult) [[unlikely]] {
            throwTemporalError(globalObject, scope, diffResult.error());
            return { };
        }

        // Step 28.i: Return ? TemporalDurationFromInternal(internalDuration, largestUnit).
        auto durResult = TemporalCore::temporalDurationFromInternal(*diffResult, largestUnit);
        if (!durResult) [[unlikely]] {
            throwTemporalError(globalObject, scope, durResult.error());
            return { };
        }
        return *durResult;
    }

    // Step 29: If IsCalendarUnit(existingLargestUnit) or IsCalendarUnit(largestUnit), throw RangeError.
    if (years() || months() || weeks() || isCalendarUnit(largestUnit)) [[unlikely]] {
        throwRangeError(globalObject, scope, "Cannot round a duration of years, months, or weeks without a relativeTo option"_s);
        return { };
    }

    // Step 30: Assert: IsCalendarUnit(smallestUnit) is false.
    ASSERT(!isCalendarUnit(smallestUnit));

    // Step 31: Let internalDuration be ToInternalDurationRecordWith24HourDays(duration).
    // Steps 32-33: Round by smallestUnit (~day~ or time unit).
    // Step 34: Return ? TemporalDurationFromInternal(internalDuration, largestUnit).
    ISO8601::InternalDuration internalDuration = toInternalDurationRecordWith24HourDays(globalObject, m_duration);
    RETURN_IF_EXCEPTION(scope, { });
    auto result = round(globalObject, internalDuration, roundingIncrement, smallestUnit, roundingMode);
    RETURN_IF_EXCEPTION(scope, { });
    auto durResult2 = TemporalCore::temporalDurationFromInternal(result, largestUnit);
    if (!durResult2) [[unlikely]] {
        throwTemporalError(globalObject, scope, durResult2.error());
        return { };
    }
    return *durResult2;
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.total
double TemporalDuration::total(JSGlobalObject* globalObject, JSValue optionsValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: branding done by caller.
    // Steps 3-7: totalOf may be a String (treated as { unit }) or an options object.
    JSObject* options = nullptr;
    String unitString;
    if (optionsValue.isString()) {
        unitString = optionsValue.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, 0);
    } else {
        options = intlGetOptionsObject(globalObject, optionsValue);
        RETURN_IF_EXCEPTION(scope, 0);
    }

    // Steps 8-10: relativeTo. Spec NOTE: "relativeTo" parsed before "unit" (alphabetical).
    RelativeToRecord relativeTo;
    if (options) {
        relativeTo = getTemporalRelativeToOption(globalObject, options);
        RETURN_IF_EXCEPTION(scope, 0);
        // Step 11: unit = ? GetTemporalUnitValuedOption(totalOf, "unit", unset).
        unitString = intlStringOption(globalObject, options, vm.propertyNames->unit, { }, { }, { });
        RETURN_IF_EXCEPTION(scope, 0);
    }

    // Step 12: ValidateTemporalUnitValue(unit, ~datetime~).
    auto unitType = temporalUnitType(unitString);
    if (!unitType) [[unlikely]] {
        throwRangeError(globalObject, scope, "unit is an invalid Temporal unit"_s);
        return 0;
    }
    TemporalUnit unit = unitType.value();

    bool hasRelativeTo = relativeTo.zonedRelativeTo || relativeTo.plainRelativeTo;

    // Step 13: no relativeTo.
    if (!hasRelativeTo) {
        // Step 13.a: reject calendar units.
        if (isCalendarUnit(unit) || years() || months() || weeks()) [[unlikely]] {
            throwRangeError(globalObject, scope, "Cannot total a duration of years, months, or weeks without a relativeTo option"_s);
            return 0;
        }
        // Steps 13.b-c: ToInternalDurationRecordWith24HourDays + TotalTimeDuration.
        auto internalDuration = toInternalDurationRecordWith24HourDays(globalObject, m_duration);
        RETURN_IF_EXCEPTION(scope, 0);
        return TemporalCore::totalTimeDuration(internalDuration.time(), unit);
    }

    // Step 14: zonedRelativeTo path → DifferenceZonedDateTimeWithTotal.
    if (relativeTo.zonedRelativeTo) {
        auto endpoints = computeZonedRelativeEndpoints(globalObject, relativeTo.zonedRelativeTo, m_duration);
        RETURN_IF_EXCEPTION(scope, 0);
        ASSERT(endpoints);
        auto result = differenceZonedDateTimeWithTotal(globalObject, relativeTo.zonedRelativeTo, *endpoints, unit);
        RETURN_IF_EXCEPTION(scope, 0);
        ASSERT(result);
        return *result;
    }

    // Step 15: plainRelativeTo path → DifferencePlainDateTimeWithTotal.
    {
        auto& plainDate = relativeTo.plainRelativeTo->date;
        auto calendarId = relativeTo.plainRelativeTo->calendarId;
        // Step 15.a: ToInternalDurationRecordWith24HourDays.
        auto internalDuration = toInternalDurationRecordWith24HourDays(globalObject, m_duration);
        RETURN_IF_EXCEPTION(scope, 0);
        // Steps 15.b-e: target {date, time}.
        auto target = computePlainRelativeTarget(globalObject, calendarId, plainDate, internalDuration);
        RETURN_IF_EXCEPTION(scope, 0);
        ASSERT(target);
        // Steps 15.f-h: DifferencePlainDateTimeWithTotal.
        auto result = differencePlainDateTimeWithTotal(globalObject, calendarId, plainDate, *target, unit);
        RETURN_IF_EXCEPTION(scope, 0);
        ASSERT(result);
        return *result;
    }
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.tostring
String TemporalDuration::toString(JSGlobalObject* globalObject, JSValue optionsValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: branding check done by the caller.
    // Step 3: Let resolvedOptions be ? GetOptionsObject(options).
    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });

    if (!options)
        RELEASE_AND_RETURN(scope, toString(globalObject));

    // Step 4: NOTE: The following steps read options in alphabetical order.
    // Step 5: Let digits be ? GetTemporalFractionalSecondDigitsOption(resolvedOptions).
    auto digits = temporalFractionalSecondDigits(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });
    // Step 6: Let roundingMode be ? GetRoundingModeOption(resolvedOptions, ~trunc~).
    auto roundingMode = temporalRoundingMode(globalObject, options, RoundingMode::Trunc);
    RETURN_IF_EXCEPTION(scope, { });
    // Step 7: Let smallestUnit be ? GetTemporalUnitValuedOption(resolvedOptions, "smallestUnit", ~unset~).
    auto smallestUnitResult = temporalUnitValued(globalObject, options, vm.propertyNames->smallestUnit);
    RETURN_IF_EXCEPTION(scope, { });
    // Step 8: Perform ? ValidateTemporalUnitValue(smallestUnit, ~time~).
    validateTemporalUnitValue(globalObject, smallestUnitResult, UnitGroup::Time, AllowedUnit::None, "smallestUnit"_s);
    RETURN_IF_EXCEPTION(scope, { });
    std::optional<TemporalUnit> smallestUnit = std::get<std::optional<TemporalUnit>>(smallestUnitResult);
    // Step 9: If smallestUnit is ~hour~ or ~minute~, throw a RangeError exception.
    if (smallestUnit == TemporalUnit::Hour || smallestUnit == TemporalUnit::Minute) [[unlikely]] {
        throwRangeError(globalObject, scope, "smallestUnit must not be \"minute\" or larger"_s);
        return { };
    }

    // Step 10: Let precision be ToSecondsStringPrecisionRecord(smallestUnit, digits).
    auto data = toSecondsStringPrecisionRecord(smallestUnit, digits);

    // Step 11: If precision.[[Unit]] is ~nanosecond~ and precision.[[Increment]] = 1, return TemporalDurationToString.
    // Precision::Auto always means increment=1 ns — a no-op for any rounding mode.
    if (std::get<0>(data.precision) == Precision::Auto)
        RELEASE_AND_RETURN(scope, toString(globalObject));

    // Step 12: Let largestUnit be DefaultTemporalLargestUnit(duration).
    auto largestUnit = TemporalCore::largestSubduration(m_duration);

    // Step 13: Let internalDuration be ToInternalDurationRecord(duration).
    auto internalDuration = TemporalCore::toInternalDurationRecord(m_duration);

    // Step 14: Let timeDuration be ? RoundTimeDuration(internalDuration.[[Time]], precision.[[Increment]], precision.[[Unit]], roundingMode).
    auto roundedTime = ISO8601::roundTimeDuration(globalObject, internalDuration.time(), data.increment, data.unit, roundingMode);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 15: Set internalDuration to CombineDateAndTimeDuration(internalDuration.[[Date]], timeDuration).
    internalDuration = ISO8601::InternalDuration::combineDateAndTimeDuration(internalDuration.dateDuration(), roundedTime);

    // Step 16: Let roundedLargestUnit be LargerOfTwoTemporalUnits(largestUnit, ~second~).
    auto roundedLargestUnit = std::min(largestUnit, TemporalUnit::Second);

    // Step 17: Let roundedDuration be ? TemporalDurationFromInternal(internalDuration, roundedLargestUnit).
    // TemporalDurationFromInternal calls CreateTemporalDuration which calls IsValidDuration.
    // When days+time together exceed 2^53 seconds after decomposition, IsValidDuration fails → RangeError.
    auto roundedDurationResult = TemporalCore::temporalDurationFromInternal(internalDuration, roundedLargestUnit);
    if (!roundedDurationResult) [[unlikely]] {
        throwTemporalError(globalObject, scope, roundedDurationResult.error());
        return { };
    }
    auto roundedDuration = *roundedDurationResult;

    // Step 18: Return TemporalDurationToString(roundedDuration, precision.[[Precision]]).
    RELEASE_AND_RETURN(scope, toString(globalObject, roundedDuration, data.precision));
}

static void appendInteger(JSGlobalObject* globalObject, StringBuilder& builder, double value)
{
    ASSERT(std::isfinite(value));

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    double absValue = std::abs(value);
    if (absValue <= maxSafeInteger()) [[likely]] {
        builder.append(absValue);
        return;
    }

    auto* bigint = JSBigInt::createFrom(globalObject, absValue);
    RETURN_IF_EXCEPTION(scope, void());

    String string = bigint->toString(globalObject, 10);
    RETURN_IF_EXCEPTION(scope, void());

    builder.append(string);
}

// TemporalDurationToString ( years, months, weeks, days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds, precision )
// https://tc39.es/proposal-temporal/#sec-temporal-temporaldurationtostring
String TemporalDuration::toString(JSGlobalObject* globalObject, const ISO8601::Duration& duration, std::tuple<Precision, unsigned> precision)
{
    ASSERT(std::get<0>(precision) == Precision::Auto || std::get<1>(precision) < 10);

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    StringBuilder builder;
    auto sign = TemporalCore::durationSign(duration);
    if (sign < 0)
        builder.append('-');

    builder.append('P');
    if (duration.years()) {
        appendInteger(globalObject, builder, duration.years());
        RETURN_IF_EXCEPTION(scope, { });
        builder.append('Y');
    }
    if (duration.months()) {
        appendInteger(globalObject, builder, duration.months());
        RETURN_IF_EXCEPTION(scope, { });
        builder.append('M');
    }
    if (duration.weeks()) {
        appendInteger(globalObject, builder, duration.weeks());
        RETURN_IF_EXCEPTION(scope, { });
        builder.append('W');
    }
    if (duration.days()) {
        appendInteger(globalObject, builder, duration.days());
        RETURN_IF_EXCEPTION(scope, { });
        builder.append('D');
    }

    auto secondsDuration = TemporalCore::timeDurationFromComponents(0, 0, duration.seconds(), duration.milliseconds(), static_cast<double>(duration.microseconds()), static_cast<double>(duration.nanoseconds()));

    if (!duration.hours() && !duration.minutes() && !secondsDuration && sign && std::get<0>(precision) == Precision::Auto)
        return builder.toString();

    builder.append('T');
    if (duration.hours()) {
        appendInteger(globalObject, builder, duration.hours());
        RETURN_IF_EXCEPTION(scope, { });
        builder.append('H');
    }
    if (duration.minutes()) {
        appendInteger(globalObject, builder, duration.minutes());
        RETURN_IF_EXCEPTION(scope, { });
        builder.append('M');
    }

    bool zeroMinutesAndHigher = TemporalCore::largestSubduration(duration) >= TemporalUnit::Second;

    if (secondsDuration || (zeroMinutesAndHigher || std::get<0>(precision) != Precision::Auto)) {
        double secondsPart = std::abs(static_cast<double>(static_cast<int64_t>(secondsDuration / 1000000000)));
        double subSecondsPart = std::abs(static_cast<double>(static_cast<int64_t>(secondsDuration % 1000000000)));
        appendInteger(globalObject, builder, secondsPart);
        RETURN_IF_EXCEPTION(scope, { });
        formatSecondsStringFraction(builder, subSecondsPart, precision);
        builder.append('S');
    }

    return builder.toString();
}

} // namespace JSC
