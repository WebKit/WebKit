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
#include "TemporalZonedDateTime.h"

#include "CalendarFields.h"
#include "CalendarICUBridge.h"
#include "ISO8601.h"
#include "InternalFunction.h"
#include "IntlObject.h"
#include "JSCInlines.h"
#include "TemporalCalendar.h"
#include "TemporalCoreTypes.h"
#include "TemporalDuration.h"
#include "TemporalObject.h"
#include "TemporalPlainDate.h"
#include "TemporalPlainTime.h"
#include "TimeZoneICUBridge.h"
#include "ZonedDateTimeCore.h"

#include <wtf/DateMath.h>
#include <wtf/text/MakeString.h>

namespace JSC {

const ClassInfo TemporalZonedDateTime::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(TemporalZonedDateTime) };

TemporalZonedDateTime* TemporalZonedDateTime::create(VM& vm, Structure* structure, ISO8601::ExactTime exactTime, TimeZone timeZone, CalendarID calendarID)
{
    auto* object = new (NotNull, allocateCell<TemporalZonedDateTime>(vm)) TemporalZonedDateTime(vm, structure, exactTime, timeZone, calendarID);
    object->finishCreation(vm);
    return object;
}

// https://tc39.es/proposal-temporal/#sec-temporal-createtemporalzoneddatetime
template<TemporalConstructTarget target>
static TemporalZonedDateTime* createTemporalZonedDateTimeImpl(JSGlobalObject* globalObject, ISO8601::ExactTime exactTime, TimeZone timeZone, CalendarID calendarID, TemporalNewTarget newTarget = { })
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Assert: IsValidEpochNanoseconds(epochNanoseconds) is true.
    ASSERT(exactTime.isValid());

    // Step 2: If newTarget is not present, set newTarget to %Temporal.ZonedDateTime%.
    // Step 3: Let object be ? OrdinaryCreateFromConstructor(newTarget, "%Temporal.ZonedDateTime.prototype%", « ... »).
    Structure* structure;
    if constexpr (target == TemporalConstructTarget::Intrinsic)
        structure = globalObject->zonedDateTimeStructure();
    else {
        ASSERT(newTarget.newTarget && newTarget.constructor);
        structure = JSC_GET_DERIVED_STRUCTURE(vm, zonedDateTimeStructure, newTarget.newTarget, newTarget.constructor);
        RETURN_IF_EXCEPTION(scope, { });
    }

    // Steps 4-6: set [[EpochNanoseconds]], [[TimeZone]], [[Calendar]]. Step 7: Return object.
    return TemporalZonedDateTime::create(vm, structure, exactTime, timeZone, calendarID);
}

TemporalZonedDateTime* createTemporalZonedDateTime(JSGlobalObject* globalObject, ISO8601::ExactTime exactTime, TimeZone timeZone, CalendarID calendarID)
{
    return createTemporalZonedDateTimeImpl<TemporalConstructTarget::Intrinsic>(globalObject, exactTime, timeZone, calendarID);
}

TemporalZonedDateTime* createTemporalZonedDateTime(JSGlobalObject* globalObject, ISO8601::ExactTime exactTime, TimeZone timeZone, CalendarID calendarID, TemporalNewTarget newTarget)
{
    return createTemporalZonedDateTimeImpl<TemporalConstructTarget::NewTarget>(globalObject, exactTime, timeZone, calendarID, newTarget);
}

Structure* TemporalZonedDateTime::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalZonedDateTime::TemporalZonedDateTime(VM& vm, Structure* structure, ISO8601::ExactTime exactTime, TimeZone timeZone, CalendarID calendarID)
    : Base(vm, structure)
    , m_exactTime(exactTime)
    , m_timeZone(timeZone)
    , m_calendarID(calendarID)
{
}

std::optional<int64_t> TemporalZonedDateTime::getOffsetNanoseconds(JSGlobalObject* globalObject) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto result = TemporalCore::getOffsetNanosecondsFor(m_timeZone, exactTime());
    if (!result) [[unlikely]] {
        if (result.error().kind == TemporalErrorKind::RangeError)
            throwRangeError(globalObject, scope, result.error().message);
        else
            throwTypeError(globalObject, scope, result.error().message);
        return std::nullopt;
    }
    return *result;
}

// https://tc39.es/proposal-temporal/#sec-temporal-getisodatetimefor
// Thin JS-side wrapper: forwards TemporalResult errors to the caller's ThrowScope.
ISO8601::PlainDateTime TemporalZonedDateTime::getLocalDateTime(JSGlobalObject* globalObject) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto result = TemporalCore::getISODateTimeFor(m_timeZone, exactTime());
    if (!result) [[unlikely]] {
        if (result.error().kind == TemporalErrorKind::RangeError)
            throwRangeError(globalObject, scope, result.error().message);
        else
            throwTypeError(globalObject, scope, result.error().message);
        return { };
    }
    return *result;
}

std::optional<TimeZone> timeZoneFromRecord(const ISO8601::TimeZoneRecord& tzRecord)
{
    auto& nameOrOffset = tzRecord.m_nameOrOffset;
    if (auto* offsetNanoseconds = std::get_if<int64_t>(&nameOrOffset))
        return TimeZone::fromUTCOffset(*offsetNanoseconds);
    auto& name = std::get<Vector<Latin1Character>>(nameOrOffset);
    ASSERT(!name.isEmpty());
    if (auto tzId = ISO8601::parseTimeZoneName(name.span()))
        return TimeZone::fromID(*tzId);
    return std::nullopt; // invalid IANA name in bracket
}

// Aggregate of all inputs needed by the unified steps 6-12 epilogue in TemporalZonedDateTime::from().
// Both the string path and the property bag path populate this struct and return it; from() then calls
// TemporalCore::interpretISODateTimeOffset once and creates the object.
struct ZDTEpochArgs {
    ISO8601::PlainDate plainDate;
    ISO8601::PlainTime plainTime;
    TimeZone timeZone;
    CalendarID calendarID;
    OffsetBehaviour offsetBehaviour;
    int64_t inlineOffsetNs { 0 };
    TemporalCore::MatchBehaviour matchBehaviour { TemporalCore::MatchBehaviour::MatchMinutes };
    TemporalCore::UseStartOfDay useStartOfDay { TemporalCore::UseStartOfDay::No };
    TemporalDisambiguation disambiguation { TemporalDisambiguation::Compatible };
    TemporalOffsetDisambiguation offsetOpt { TemporalOffsetDisambiguation::Reject };
};

// Steps 5.b-5.r + option resolution for the string path of ToTemporalZonedDateTime.
// Returns std::nullopt if an exception was already thrown.
static std::optional<ZDTEpochArgs> toEpochArgsFromString(JSGlobalObject* globalObject, JSString* item, JSValue optionsArg)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String string = item->value(globalObject);
    RETURN_IF_EXCEPTION(scope, std::nullopt);

    // Step 5.b: Let result be ? ParseISODateTime(item, « TemporalDateTimeString[+Zoned] »).
    //   [+Zoned] makes the bracket TimeZoneAnnotation grammatically mandatory, so this also
    //   discharges Step 5.d's "Assert: annotation is not empty".
    auto parsed = ISO8601::parseISODateTime(string, ISO8601::TemporalProduction::DateTimeZoned);
    if (!parsed) [[unlikely]] {
        throwRangeError(globalObject, scope, makeString("'"_s, ellipsizeAt(100, string), "' is not a valid Temporal.ZonedDateTime string"_s));
        return std::nullopt;
    }
    auto [plainDateOpt, plainTimeOptional, tzRecordOptional, calendarOptional, matched, isShortForm] = WTF::move(*parsed);
    ASSERT(plainDateOpt && tzRecordOptional);
    auto plainDate = WTF::move(*plainDateOpt);
    auto& tzRecord = *tzRecordOptional;

    // Steps 5.c-5.e: annotation = result.[[TimeZone]].[[TimeZoneAnnotation]];
    //   timeZone = ? ToTemporalTimeZoneIdentifier(annotation).
    auto timeZoneOpt = timeZoneFromRecord(tzRecord);
    if (!timeZoneOpt) [[unlikely]] {
        throwRangeError(globalObject, scope, makeString("'"_s, ellipsizeAt(100, string), "' contains an invalid time zone identifier"_s));
        return std::nullopt;
    }
    TimeZone timeZone = *timeZoneOpt;

    // Step 5.f: offsetString = result.[[TimeZone]].[[OffsetString]].
    // Step 5.g: If result.[[TimeZone]].[[Z]] is true, set hasUTCDesignator to true.
    bool hasUTCDesignator = tzRecord.m_z;
    int64_t inlineOffsetNs = tzRecord.m_offset.value_or(0);
    auto matchBehaviour = tzRecord.m_offsetHasSubMinutePrecision ? TemporalCore::MatchBehaviour::MatchExactly : TemporalCore::MatchBehaviour::MatchMinutes;

    // Steps 5.h-5.j: calendar = result.[[Calendar]]; if empty → "iso8601"; CanonicalizeCalendar.
    CalendarID calendarID = iso8601CalendarID();
    if (calendarOptional) {
        auto rawCal = String(calendarOptional->span()).convertToASCIILowercase();
        auto canonicalized = isBuiltinCalendar(rawCal);
        if (canonicalized)
            calendarID = *canonicalized;
        else [[unlikely]] {
            throwRangeError(globalObject, scope, makeString("'"_s, rawCal, "' is not a valid calendar identifier"_s));
            return std::nullopt;
        }
    }

    // Step 5.k: Set matchBehaviour to ~match-minutes~.
    // Step 5.l: If offsetString has sub-minute precision, set matchBehaviour to ~match-exactly~.

    // Step 5.m: resolvedOptions = ? GetOptionsObject(options).
    // Steps 5.n-5.p: disambiguation, offsetOption, overflow (all read for spec observability).
    JSObject* options = nullptr;
    if (!optionsArg.isUndefined()) {
        options = optionsArg.getObject();
        if (!options) [[unlikely]] {
            throwTypeError(globalObject, scope, "Temporal.ZonedDateTime.from: options must be an object"_s);
            return std::nullopt;
        }
    }
    TemporalDisambiguation disambiguation = TemporalDisambiguation::Compatible;
    TemporalOffsetDisambiguation offsetOpt = TemporalOffsetDisambiguation::Reject;
    if (options) {
        disambiguation = toTemporalDisambiguation(globalObject, options);
        RETURN_IF_EXCEPTION(scope, std::nullopt);
        offsetOpt = toTemporalOffset(globalObject, options, TemporalOffsetDisambiguation::Reject);
        RETURN_IF_EXCEPTION(scope, std::nullopt);
        toTemporalOverflow(globalObject, options);
        RETURN_IF_EXCEPTION(scope, std::nullopt);
    }

    // Steps 5.q-5.r: isoDate = CreateISODateRecord; time = result.[[Time]].
    ISO8601::PlainTime plainTime = plainTimeOptional.value_or(ISO8601::PlainTime());

    // Steps 6-8: derive offsetBehaviour from hasUTCDesignator and offsetString.
    OffsetBehaviour offsetBehaviour;
    if (hasUTCDesignator)
        offsetBehaviour = OffsetBehaviour::Exact;
    else if (!tzRecord.m_offset)
        offsetBehaviour = OffsetBehaviour::Wall;
    else
        offsetBehaviour = OffsetBehaviour::Option;

    auto useStartOfDay = !plainTimeOptional.has_value() && offsetBehaviour == OffsetBehaviour::Wall
        ? TemporalCore::UseStartOfDay::Yes : TemporalCore::UseStartOfDay::No;

    return ZDTEpochArgs {
        plainDate,
        plainTime,
        timeZone,
        calendarID,
        offsetBehaviour,
        inlineOffsetNs,
        matchBehaviour,
        useStartOfDay,
        disambiguation,
        offsetOpt
    };
}

// Steps 4.b-4.l for the property bag path of ToTemporalZonedDateTime.
// Returns std::nullopt if an exception was already thrown.
static std::optional<ZDTEpochArgs> toEpochArgsFromPropertyBag(JSGlobalObject* globalObject, JSObject* bag, JSValue optionsArg)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 4.b: calendar = ? GetTemporalCalendarIdentifierWithISODefault(item).
    CalendarID calendarID = getTemporalCalendarIdentifierWithISODefault(globalObject, bag);
    RETURN_IF_EXCEPTION(scope, std::nullopt);

    // Step 4.c: PrepareCalendarFields — all 15 ZDT fields read alphabetically in one pass.
    auto fields = readZonedDateTimeFieldsFromObject<ZonedDateTimeFieldMode::Full>(globalObject, bag, calendarID);
    RETURN_IF_EXCEPTION(scope, std::nullopt);

    // Steps 4.d-4.e: timeZone = fields.[[TimeZone]]; offsetString = fields.[[OffsetString]].
    TimeZone timeZone = fields.timeZone;

    // Steps 4.f-4.i: GetOptionsObject + disambiguation/offsetOption/overflow (after fields per spec).
    JSObject* options = nullptr;
    if (!optionsArg.isUndefined()) {
        if (!optionsArg.isObject()) [[unlikely]] {
            throwTypeError(globalObject, scope, "Temporal.ZonedDateTime.from: options must be an object"_s);
            return std::nullopt;
        }
        options = asObject(optionsArg);
    }
    TemporalDisambiguation disambiguation = TemporalDisambiguation::Compatible;
    TemporalOffsetDisambiguation offsetOpt = TemporalOffsetDisambiguation::Reject;
    TemporalOverflow overflow = TemporalOverflow::Constrain;
    if (options) {
        disambiguation = toTemporalDisambiguation(globalObject, options);
        RETURN_IF_EXCEPTION(scope, std::nullopt);
        offsetOpt = toTemporalOffset(globalObject, options, TemporalOffsetDisambiguation::Reject);
        RETURN_IF_EXCEPTION(scope, std::nullopt);
        overflow = toTemporalOverflow(globalObject, options);
        RETURN_IF_EXCEPTION(scope, std::nullopt);
    }

    // Steps 4.j-4.k: dateTimeResult = ? InterpretTemporalDateTimeFields(calendar, fields, overflow).
    TemporalCore::TimeFieldsIn timeFields {
        fields.hour, fields.minute, fields.second,
        fields.millisecond, fields.microsecond, fields.nanosecond,
    };
    auto pdt = interpretTemporalDateTimeFields(globalObject, calendarID, fields.dateFields, timeFields, overflow);
    RETURN_IF_EXCEPTION(scope, std::nullopt);
    ISO8601::PlainDate plainDate = pdt.date;
    ISO8601::PlainTime plainTime = pdt.time;

    // Steps 6-8: offsetBehaviour from offsetString (fields.[[OffsetString]]).
    // No offset string → Wall; offset string present → Option (caller's offsetOpt drives prefer/reject/use/ignore).
    OffsetBehaviour offsetBehaviour = fields.offsetNs ? OffsetBehaviour::Option : OffsetBehaviour::Wall;
    int64_t inlineOffsetNs = fields.offsetNs.value_or(0);
    // Property bags always use ~match-exactly~ (spec step 4.j), so treat offset as sub-minute precision.
    auto matchBehaviour = TemporalCore::MatchBehaviour::MatchExactly;
    auto useStartOfDay = TemporalCore::UseStartOfDay::No;

    return ZDTEpochArgs {
        plainDate,
        plainTime,
        timeZone,
        calendarID,
        offsetBehaviour,
        inlineOffsetNs,
        matchBehaviour,
        useStartOfDay,
        disambiguation,
        offsetOpt
    };
}

TemporalZonedDateTime* TemporalZonedDateTime::from(JSGlobalObject* globalObject, JSValue itemValue)
{
    return from(globalObject, itemValue, jsUndefined());
}

// temporal_rs: ZonedDateTime::from_str (string) / from_partial_with_provider (property bag)
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.from
// https://tc39.es/proposal-temporal/#sec-temporal-totemporalzoneddatetime
TemporalZonedDateTime* TemporalZonedDateTime::from(JSGlobalObject* globalObject, JSValue itemValue, JSValue optionsArg)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-3: _hasUTCDesignator_ = false, _matchBehaviour_ = ~match-exactly~ are deferred into
    // ZDTEpochArgs.matchBehaviour and ZDTEpochArgs.offsetBehaviour.
    // Steps 4-5 reordered: String check (step 5) precedes ZDT check (step 4.a) because a value
    // cannot be both a String and have [[InitializedTemporalZonedDateTime]], so the order is unobservable.

    std::optional<ZDTEpochArgs> args;

    // Step 5: item is a String.
    if (itemValue.isString()) {
        args = toEpochArgsFromString(globalObject, asString(itemValue), optionsArg);
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (!args)
            return nullptr;
    } else if (itemValue.inherits<TemporalZonedDateTime>()) {
        // Step 4.a: item has [[InitializedTemporalZonedDateTime]] internal slot.
        auto* zdt = uncheckedDowncast<TemporalZonedDateTime>(itemValue);
        // Step 4.a.i: resolvedOptions = ? GetOptionsObject(options).
        JSObject* options = nullptr;
        if (!optionsArg.isUndefined()) {
            options = optionsArg.getObject();
            if (!options) [[unlikely]] {
                throwTypeError(globalObject, scope, "Temporal.ZonedDateTime.from: options must be an object"_s);
                return nullptr;
            }
        }
        // Steps 4.a.ii-iv: Read disambiguation, offset (~reject~), overflow for spec observability
        //                   (NOTE: alphabetical order per the spec NOTE at step 4.a.i).
        if (options) {
            toTemporalDisambiguation(globalObject, options);
            RETURN_IF_EXCEPTION(scope, nullptr);
            toTemporalOffset(globalObject, options, TemporalOffsetDisambiguation::Reject);
            RETURN_IF_EXCEPTION(scope, nullptr);
            toTemporalOverflow(globalObject, options);
            RETURN_IF_EXCEPTION(scope, nullptr);
        }
        // Step 4.a.v: Return ! CreateTemporalZonedDateTime(item.[[EpochNanoseconds]], item.[[TimeZone]], item.[[Calendar]]).
        return TemporalZonedDateTime::create(vm, globalObject->zonedDateTimeStructure(),
            zdt->exactTime(), zdt->timeZone(), zdt->calendarID());
    } else {
        // Steps 4.b-4.l: property bag path.
        // Step 5 else: item is not a String — if also not an Object, throw TypeError.
        if (!itemValue.isObject()) [[unlikely]] {
            throwTypeError(globalObject, scope, "Temporal.ZonedDateTime.from: argument must be a ZonedDateTime, string, or object"_s);
            return nullptr;
        }
        args = toEpochArgsFromPropertyBag(globalObject, asObject(itemValue), optionsArg);
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (!args)
            return nullptr;
    }

    // Steps 6-10 are encoded in args: offsetBehaviour (Steps 6-8: exact/wall/option) and inlineOffsetNs (Steps 9-10: default 0, or ParseDateTimeUTCOffset(offsetString) when option).
    // Step 11: epochNanoseconds = ? InterpretISODateTimeOffset(...).
    auto exactTimeResult = TemporalCore::interpretISODateTimeOffset(
        args->plainDate, args->plainTime, args->useStartOfDay,
        args->offsetBehaviour, args->offsetOpt, args->inlineOffsetNs,
        args->matchBehaviour, args->timeZone, args->disambiguation);
    if (!exactTimeResult) [[unlikely]] {
        throwRangeError(globalObject, scope, exactTimeResult.error().message);
        return nullptr;
    }

    // Step 12: Return ! CreateTemporalZonedDateTime(epochNanoseconds, timeZone, calendar).
    RELEASE_AND_RETURN(scope, createTemporalZonedDateTime(globalObject, *exactTimeResult, args->timeZone, args->calendarID));
}

// temporal_rs: ZonedDateTime::epoch_ns (via get_epoch_nanoseconds_for)
// https://tc39.es/proposal-temporal/#sec-temporal-getepochnanosecondsfor
std::optional<ISO8601::ExactTime> TemporalZonedDateTime::getEpochNanosecondsFor(
    JSGlobalObject* globalObject,
    const TimeZone& timeZone,
    const ISO8601::PlainDate& date,
    const ISO8601::PlainTime& time,
    TemporalDisambiguation disambiguation)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    // Step 1: possibleEpochNs = ? GetPossibleEpochNanoseconds(timeZone, isoDateTime).
    // Step 2: Return ? DisambiguatePossibleEpochNanoseconds(possibleEpochNs, ..., disambiguation).
    auto result = TemporalCore::getEpochNanosecondsFor(timeZone, date, time, disambiguation);
    if (!result) [[unlikely]] {
        throwRangeError(globalObject, scope, result.error().message);
        return std::nullopt;
    }
    return *result;
}

} // namespace JSC
