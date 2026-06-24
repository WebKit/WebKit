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

// temporal_rs: ZonedDateTime::try_new (validates epochNanoseconds range)
// https://tc39.es/proposal-temporal/#sec-temporal-createtemporalzoneddatetime
TemporalZonedDateTime* TemporalZonedDateTime::tryCreate(JSGlobalObject* globalObject, Structure* structure, ISO8601::ExactTime exactTime, TimeZone timeZone, CalendarID calendarID)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: If IsValidEpochNanoseconds(epochNanoseconds) is false, throw RangeError.
    if (!exactTime.isValid()) [[unlikely]] {
        throwRangeError(globalObject, scope, "epochNanoseconds is outside of the supported range for Temporal.ZonedDateTime"_s);
        return nullptr;
    }

    // Steps 2-5: Allocate object, set [[EpochNanoseconds]], [[TimeZone]], [[Calendar]], return.
    return create(vm, structure, exactTime, timeZone, calendarID);
}

Structure* TemporalZonedDateTime::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalZonedDateTime* TemporalZonedDateTime::withExactTime(JSGlobalObject* globalObject, ISO8601::ExactTime epochNs) const
{
    return tryCreate(globalObject, globalObject->zonedDateTimeStructure(), epochNs, m_timeZone, m_calendarID);
}

TemporalZonedDateTime::TemporalZonedDateTime(VM& vm, Structure* structure, ISO8601::ExactTime exactTime, TimeZone timeZone, CalendarID calendarID)
    : Base(vm, structure)
    , m_exactTime(exactTime)
    , m_timeZone(timeZone)
    , m_calendarID(calendarID)
{
}

// https://tc39.es/proposal-temporal/#sec-temporal-getoffsetnanosecondsfor
std::optional<int64_t> TemporalZonedDateTime::getOffsetNanoseconds(JSGlobalObject* globalObject) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    // Step 1: If timeZone is an offset timezone, return offsetMinutes × 60 × 10⁹.
    // Step 2: Return GetNamedTimeZoneOffsetNanoseconds(timeZone, epochNanoseconds).
    // (Both paths are handled by TemporalCore::getOffsetNanosecondsFor.)
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
void TemporalZonedDateTime::getLocalDateAndTime(JSGlobalObject* globalObject, ISO8601::PlainDate& outDate, ISO8601::PlainTime& outTime) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    // Step 1: Let offsetNanoseconds be ! GetOffsetNanosecondsFor(timeZone, epochNanoseconds).
    auto offsetOpt = getOffsetNanoseconds(globalObject);
    RETURN_IF_EXCEPTION(scope, void());
    // Step 2: Return BalanceISODateTime(epochNanoseconds + offsetNanoseconds).
    TemporalCore::exactTimeToLocalDateAndTime(exactTime(), *offsetOpt, outDate, outTime);
}

// Internal helper: extracts the runtime TimeZone handle from an already-parsed TimeZoneRecord.
// Bracket annotation takes priority over Z, which takes priority over inline offset.
// Returns nullopt if the record has no usable timezone info.
static std::optional<TimeZone> timeZoneFromRecord(const ISO8601::TimeZoneRecord& tzRecord)
{
    auto& nameOrOffset = tzRecord.m_nameOrOffset;
    if (std::holds_alternative<int64_t>(nameOrOffset))
        return TimeZone::fromUTCOffset(std::get<int64_t>(nameOrOffset));
    auto& name = std::get<Vector<Latin1Character>>(nameOrOffset);
    if (!name.isEmpty()) {
        if (auto tzId = ISO8601::parseTimeZoneName(StringView(name.span())))
            return TimeZone::fromID(*tzId);
        return std::nullopt; // invalid IANA name in bracket
    }
    // No bracket annotation: use Z or inline offset.
    if (tzRecord.m_z)
        return TimeZone::fromID(utcTimeZoneID());
    if (tzRecord.m_offset)
        return TimeZone::fromUTCOffset(*tzRecord.m_offset);
    return std::nullopt;
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
    bool offsetHasSubMinutePrecision { false };
    bool useStartOfDay { false };
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

    // Step 5.b: ParseISODateTime(item, « TemporalDateTimeString[+Zoned] »).
    //   The DateTimeZoned production already requires a bracket TZ annotation, so the
    //   "TimeZoneAnnotation Parse Node present" check (spec step 5.d) is satisfied by parsing.
    auto parsed = ISO8601::parseISODateTime(string, ISO8601::TemporalProduction::DateTimeZoned);
    if (!parsed) [[unlikely]] {
        throwRangeError(globalObject, scope, makeString("'"_s, ellipsizeAt(100, string), "' is not a valid Temporal.ZonedDateTime string"_s));
        return std::nullopt;
    }
    auto [plainDateOpt, plainTimeOptional, tzRecordOptional, calendarOptional, matched, isShortForm] = WTF::move(*parsed);
    ASSERT(plainDateOpt && tzRecordOptional);
    auto plainDate = WTF::move(*plainDateOpt);
    auto& tzRecord = *tzRecordOptional;

    // Step 5.e: timeZone = ? ToTemporalTimeZoneIdentifier(annotation). (fused into timeZoneFromRecord)
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
    bool offsetHasSubMinutePrecision = tzRecord.m_offsetHasSubMinutePrecision;

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

    // Step 5.k: matchBehaviour = ~match-minutes~ (folded into offsetHasSubMinutePrecision = false by default).
    // Step 5.l: If offsetString has sub-minute precision, matchBehaviour = ~match-exactly~ (already in offsetHasSubMinutePrecision).

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

    bool useStartOfDay = !plainTimeOptional.has_value() && offsetBehaviour == OffsetBehaviour::Wall;

    return ZDTEpochArgs {
        plainDate,
        plainTime,
        timeZone,
        calendarID,
        offsetBehaviour,
        inlineOffsetNs,
        offsetHasSubMinutePrecision,
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

    // Steps 4.b+4.c: GetTemporalCalendarIdentifierWithISODefault + PrepareCalendarFields
    //                (all 15 ZDT fields read alphabetically in one pass).
    CalendarID calendarID = iso8601CalendarID();
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
    auto& dateFields = fields.dateFields;
    bool zdtIsNonISO = !TemporalCore::calendarIsISO(calendarID);
    double month = dateFields.month.value_or(0);
    double day = dateFields.day.value_or(0);
    double year = dateFields.year.value_or(0);
    auto& parsedMonthCode = dateFields.monthCode;

    // day and year are not in PrepareCalendarFields' requiredFieldNames, but CalendarResolveFields
    // (inside InterpretTemporalDateTimeFields) requires them — throw TypeError here to surface the
    // right error type before CalendarDateFromFields produces a RangeError from day/year = 0.
    if (!(day > 0)) [[unlikely]] {
        throwTypeError(globalObject, scope, "day property must be present"_s);
        return std::nullopt;
    }
    // year is required unless era+eraYear are both provided (calendar-specific substitution).
    if (!fields.yearPresent && !(dateFields.era && dateFields.eraYear)) [[unlikely]] {
        throwTypeError(globalObject, scope, "year property must be present"_s);
        return std::nullopt;
    }

    if (fields.monthCodePresent) {
        ASSERT(parsedMonthCode);
        if (!zdtIsNonISO && (parsedMonthCode->isLeapMonth || parsedMonthCode->monthNumber < 1 || parsedMonthCode->monthNumber > 12)) [[unlikely]] {
            throwRangeError(globalObject, scope, "month code is not valid for ISO 8601 calendar"_s);
            return std::nullopt;
        }
        if (!fields.monthPresent)
            month = parsedMonthCode->monthNumber;
        else if (month != static_cast<double>(parsedMonthCode->monthNumber)) [[unlikely]] {
            throwRangeError(globalObject, scope, "month and monthCode properties must match if both are provided"_s);
            return std::nullopt;
        }
    } else {
        if (!fields.monthPresent) [[unlikely]] {
            throwTypeError(globalObject, scope, "Either month or monthCode property must be provided"_s);
            return std::nullopt;
        }
        if (!(month > 0 && std::isfinite(month))) [[unlikely]] {
            throwRangeError(globalObject, scope, "month property must be positive and finite"_s);
            return std::nullopt;
        }
    }

    // Step 4.j: InterpretTemporalDateTimeFields → CalendarDateFromFields → isoDate.
    ISO8601::PlainDate plainDate;
    if (dateFields.era || dateFields.eraYear) {
        std::optional<StringView> era;
        if (dateFields.era)
            era = StringView(*dateFields.era);
        auto result = TemporalCore::calendarDateFromFields(
            calendarID, dateFields.year, clampTo<uint8_t>(month),
            clampTo<uint8_t>(day), era, dateFields.eraYear, parsedMonthCode, overflow);
        if (!result) [[unlikely]] {
            throwRangeError(globalObject, scope, String(result.error().message));
            return std::nullopt;
        }
        plainDate = *result;
    } else {
        if (!zdtIsNonISO) {
            if (overflow == TemporalOverflow::Constrain) {
                month = std::clamp(month, 1.0, 12.0);
                day = std::clamp(day, 1.0, 31.0);
            } else {
                if (!(month >= 1 && month <= 12)) [[unlikely]] {
                    throwRangeError(globalObject, scope, "month is out of range"_s);
                    return std::nullopt;
                }
                if (!(day >= 1 && day <= 31)) [[unlikely]] {
                    throwRangeError(globalObject, scope, "day is out of range"_s);
                    return std::nullopt;
                }
            }
        }
        plainDate = isoDateFromFields(globalObject, TemporalDateFormat::Date,
            clampTo<int32_t>(year), clampTo<uint32_t>(month), clampTo<uint32_t>(day),
            parsedMonthCode, overflow, calendarID);
        RETURN_IF_EXCEPTION(scope, std::nullopt);
    }

    // Step 4.l: time = result.[[Time]] → build PlainTime with overflow.
    ISO8601::Duration timeDur;
    timeDur.setField(TemporalUnit::Hour, fields.hour.value_or(0));
    timeDur.setField(TemporalUnit::Minute, fields.minute.value_or(0));
    timeDur.setField(TemporalUnit::Second, fields.second.value_or(0));
    timeDur.setField(TemporalUnit::Millisecond, fields.millisecond.value_or(0));
    timeDur.setField(TemporalUnit::Microsecond, fields.microsecond.value_or(0));
    timeDur.setField(TemporalUnit::Nanosecond, fields.nanosecond.value_or(0));
    auto plainTime = TemporalPlainTime::regulateTime(globalObject, WTF::move(timeDur), overflow);
    RETURN_IF_EXCEPTION(scope, std::nullopt);

    // Steps 6-8: offsetBehaviour from offsetString (fields.[[OffsetString]]).
    // No offset string → Wall; offset string present → Option (caller's offsetOpt drives prefer/reject/use/ignore).
    OffsetBehaviour offsetBehaviour = fields.offsetNs ? OffsetBehaviour::Option : OffsetBehaviour::Wall;
    int64_t inlineOffsetNs = fields.offsetNs.value_or(0);
    // Property bags always use ~match-exactly~ (spec step 4.j), so treat offset as sub-minute precision.
    bool offsetHasSubMinutePrecision = true;
    bool useStartOfDay = false;

    return ZDTEpochArgs {
        plainDate,
        plainTime,
        timeZone,
        calendarID,
        offsetBehaviour,
        inlineOffsetNs,
        offsetHasSubMinutePrecision,
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
    // ZDTEpochArgs.offsetHasSubMinutePrecision and ZDTEpochArgs.offsetBehaviour.
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

    // Steps 10-12 (unified epilogue — both string and property-bag paths converge here).
    // Steps 6-9 are encoded in args: offsetBehaviour (steps 6-8) and inlineOffsetNs (steps 9-10).
    // Step 10: epochNanoseconds = ? InterpretISODateTimeOffset(...).
    auto exactTimeResult = TemporalCore::interpretISODateTimeOffset(
        args->plainDate, args->plainTime, args->useStartOfDay,
        args->offsetBehaviour, args->offsetOpt, args->inlineOffsetNs,
        args->offsetHasSubMinutePrecision, args->timeZone, args->disambiguation);
    if (!exactTimeResult) [[unlikely]] {
        throwRangeError(globalObject, scope, exactTimeResult.error().message);
        return nullptr;
    }

    // Step 12: Return ! CreateTemporalZonedDateTime(epochNanoseconds, timeZone, calendar).
    // interpretISODateTimeOffset guarantees a valid ExactTime, so create() suffices; tryCreate()
    // adds a redundant isValid() check that acts as defense-in-depth against a buggy ICU backend.
    RELEASE_AND_RETURN(scope, TemporalZonedDateTime::tryCreate(globalObject, globalObject->zonedDateTimeStructure(), *exactTimeResult, args->timeZone, args->calendarID));
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
