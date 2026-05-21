/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

TemporalZonedDateTime* TemporalZonedDateTime::create(VM& vm, Structure* structure, ISO8601::ExactTime exactTime, TimeZone timeZone, String&& timeZoneId, String&& calendarId)
{
    auto* object = new (NotNull, allocateCell<TemporalZonedDateTime>(vm)) TemporalZonedDateTime(vm, structure, exactTime, timeZone, WTF::move(timeZoneId), WTF::move(calendarId));
    object->finishCreation(vm);
    return object;
}

// https://tc39.es/proposal-temporal/#sec-temporal-createtemporalzoneddatetime
TemporalZonedDateTime* TemporalZonedDateTime::tryCreate(JSGlobalObject* globalObject, Structure* structure, ISO8601::ExactTime exactTime, TimeZone timeZone, String&& timeZoneId, String&& calendarId)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!exactTime.isValid()) {
        throwRangeError(globalObject, scope, "epochNanoseconds is outside of the supported range for Temporal.ZonedDateTime"_s);
        return nullptr;
    }

    return create(vm, structure, exactTime, timeZone, WTF::move(timeZoneId), WTF::move(calendarId));
}

Structure* TemporalZonedDateTime::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalZonedDateTime::TemporalZonedDateTime(VM& vm, Structure* structure, ISO8601::ExactTime exactTime, TimeZone timeZone, String&& timeZoneId, String&& calendarId)
    : Base(vm, structure)
    , m_exactTime(exactTime)
    , m_timeZone(timeZone)
    , m_timeZoneId(WTF::move(timeZoneId))
    , m_calendarID(TemporalCore::calendarIDFromString(calendarId))
{
    UNUSED_PARAM(calendarId);
}

void TemporalZonedDateTime::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
}

std::optional<int64_t> TemporalZonedDateTime::getOffsetNanoseconds(JSGlobalObject* globalObject) const
{
    auto result = TemporalCore::getOffsetNanosecondsFor(m_timeZone, exactTime());
    if (!result) {
        VM& vm = globalObject->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        if (result.error().kind == TemporalErrorKind::RangeError)
            throwRangeError(globalObject, scope, result.error().message);
        else
            throwTypeError(globalObject, scope, result.error().message);
        return std::nullopt;
    }
    return *result;
}

bool TemporalZonedDateTime::getLocalDateAndTime(JSGlobalObject* globalObject, ISO8601::PlainDate& outDate, ISO8601::PlainTime& outTime) const
{
    auto offsetOpt = getOffsetNanoseconds(globalObject);
    if (!offsetOpt)
        return false;
    TemporalCore::exactTimeToLocalDateAndTime(exactTime(), *offsetOpt, outDate, outTime);
    return true;
}

// Extract a TimeZone from a parsed TimeZoneRecord, applying the bracket-annotation-first priority.
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
        return TimeZone::fromUTCOffset(0);
    if (tzRecord.m_offset)
        return TimeZone::fromUTCOffset(*tzRecord.m_offset);
    return std::nullopt;
}

// Derive the canonical timezone ID string from a TimeZoneRecord.
// Unlike TimeZone::toString(), this preserves "+00:00" for a numeric UTC offset of zero
// (TimeZone::toString() maps both "+00:00" and UTC to "UTC").
static String computeTimeZoneIdFromRecord(const ISO8601::TimeZoneRecord& tzRecord)
{
    auto& nameOrOffset = tzRecord.m_nameOrOffset;
    if (std::holds_alternative<int64_t>(nameOrOffset)) {
        // Bracket UTC offset annotation — format as "+HH:MM" (preserves "+00:00" for offset 0).
        return ISO8601::formatTimeZoneOffsetString(std::get<int64_t>(nameOrOffset));
    }
    auto& name = std::get<Vector<Latin1Character>>(nameOrOffset);
    if (!name.isEmpty()) {
        // Bracket IANA name annotation — preserve the link name (do not canonicalize).
        auto namedTz = intlAvailableNamedTimeZone(StringView(name.span()));
        if (namedTz)
            return namedTz->identifier;
        return { }; // invalid IANA name — caller should have rejected already
    }
    // No bracket annotation: use Z (→ "UTC") or the inline offset.
    if (tzRecord.m_z)
        return "UTC"_s;
    if (tzRecord.m_offset)
        return ISO8601::formatTimeZoneOffsetString(*tzRecord.m_offset);
    return { };
}

// Parse a temporal timezone string, returning both the TimeZone and its canonical ID.
// Handles bare UTC offsets, bare IANA names, and datetime strings with embedded timezone.
// For [+00:00] bracket annotation → returns (UTC TimeZone, "+00:00") preserving offset form.
// Returns nullopt for invalid strings (sub-minute offsets, bare datetime without timezone, etc.).
static std::optional<std::pair<TimeZone, String>> parseTemporalTimeZoneWithId(StringView tzString)
{
    // 1. Try as a bare UTC offset identifier (no sub-minute precision).
    if (auto offset = ISO8601::parseUTCOffset(tzString, /* parseSubMinutePrecision = */ false)) {
        String id = ISO8601::formatTimeZoneOffsetString(*offset);
        return std::make_pair(TimeZone::fromUTCOffset(*offset), WTF::move(id));
    }

    // 2. Try as a bare IANA timezone name — preserve the link name (do not canonicalize).
    if (auto namedTz = intlAvailableNamedTimeZone(tzString))
        return std::make_pair(TimeZone::fromID(namedTz->id), String(namedTz->identifier));

    // 3. Try as a datetime string with an embedded timezone identifier.
    //    Validate with parseTemporalTimeZoneIdentifier (rejects sub-minute inline offsets,
    //    bare datetime strings without timezone, etc.), then extract the canonical ID
    //    from the bracket annotation via parseCalendarDateTime.
    auto tzOpt = ISO8601::parseTemporalTimeZoneIdentifier(tzString);
    if (!tzOpt)
        return std::nullopt;

    // Re-parse the datetime string to extract the TimeZoneRecord for the ID computation.
    auto parsed = ISO8601::parseCalendarDateTime(tzString, TemporalDateFormat::Date);
    if (!parsed)
        return std::nullopt;
    auto& tzRecordOpt = std::get<2>(*parsed);
    if (!tzRecordOpt)
        return std::nullopt;

    String id = computeTimeZoneIdFromRecord(*tzRecordOpt);
    if (id.isEmpty())
        return std::nullopt;
    return std::make_pair(WTF::move(*tzOpt), WTF::move(id));
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporalzoneddatetime
// No-options overload — used by prototype methods (equals, until, since, compare) that coerce
// a ZDT argument without any disambiguation/offset options (all defaults apply).
TemporalZonedDateTime* TemporalZonedDateTime::from(JSGlobalObject* globalObject, JSValue itemValue)
{
    return from(globalObject, itemValue, jsUndefined());
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.from
// Handles: existing ZDT object (clone), ZDT string (parses with offset options),
//          property bag (reads timeZone, date/time fields, optional offset, offset options).
TemporalZonedDateTime* TemporalZonedDateTime::from(JSGlobalObject* globalObject, JSValue itemValue, JSValue optionsArg)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // =========================================================
    // STRING PATH: parse string first, then validate options.
    // Per spec, GetOptionsObject is called AFTER string parsing
    // so that a RangeError from an invalid string takes precedence
    // over a TypeError from a bad options argument.
    // =========================================================
    if (itemValue.isString()) {
        String string = asString(itemValue)->value(globalObject);
        RETURN_IF_EXCEPTION(scope, nullptr);

        auto parsed = ISO8601::parseCalendarDateTime(string, TemporalDateFormat::Date);
        if (!parsed) {
            throwRangeError(globalObject, scope, makeString("'"_s, ellipsizeAt(100, string), "' is not a valid Temporal.ZonedDateTime string"_s));
            return nullptr;
        }
        auto [plainDate, plainTimeOptional, tzRecordOptional, calendarOptional] = WTF::move(*parsed);

        if (!tzRecordOptional) {
            throwRangeError(globalObject, scope, "Temporal.ZonedDateTime string requires a time zone annotation"_s);
            return nullptr;
        }
        auto& tzRecord = *tzRecordOptional;

        // ZDT strings require a bracket annotation.
        bool hasBracket = std::holds_alternative<int64_t>(tzRecord.m_nameOrOffset)
            || !std::get<Vector<Latin1Character>>(tzRecord.m_nameOrOffset).isEmpty();
        if (!hasBracket) {
            throwRangeError(globalObject, scope, "Temporal.ZonedDateTime string requires a bracketed time zone annotation"_s);
            return nullptr;
        }

        auto timeZoneOpt = timeZoneFromRecord(tzRecord);
        if (!timeZoneOpt) {
            throwRangeError(globalObject, scope, makeString("'"_s, ellipsizeAt(100, string), "' contains an invalid time zone identifier"_s));
            return nullptr;
        }
        TimeZone timeZone = *timeZoneOpt;

        // Compute the canonical ID from the record directly (not via TimeZone::toString())
        // to preserve "+00:00" vs "UTC" distinction for numeric UTC offset annotations.
        String timeZoneId = computeTimeZoneIdFromRecord(tzRecord);

        // Determine offsetBehaviour per spec §ToTemporalZonedDateTime:
        //   Z → exact (always use 0 offset)
        //   inline offset present → option (apply offset-option disambiguation)
        //   no inline offset (IANA bracket only) → wall (ignore offset, use timezone + disambiguation)
        OffsetBehaviour offsetBehaviour;
        int64_t inlineOffsetNs = 0;
        bool offsetHasSubMinutePrecision = false;
        if (tzRecord.m_z) {
            offsetBehaviour = OffsetBehaviour::Exact;
            inlineOffsetNs = 0;
        } else if (tzRecord.m_offset) {
            offsetBehaviour = OffsetBehaviour::Option;
            inlineOffsetNs = *tzRecord.m_offset;
            offsetHasSubMinutePrecision = tzRecord.m_offsetHasSubMinutePrecision;
        } else
            offsetBehaviour = OffsetBehaviour::Wall;

        // Validate options type AFTER string parsing (RangeError from invalid string
        // must take precedence over TypeError from bad options argument).
        JSObject* options = nullptr;
        if (!optionsArg.isUndefined()) {
            options = optionsArg.getObject();
            if (!options) {
                throwTypeError(globalObject, scope, "Temporal.ZonedDateTime.from: options must be an object"_s);
                return nullptr;
            }
        }

        // Read disambiguation, offset, and overflow options.
        // All three are read even when not all are applicable (required for spec observability).
        TemporalDisambiguation disambiguation = TemporalDisambiguation::Compatible;
        TemporalOffsetDisambiguation offsetOpt = TemporalOffsetDisambiguation::Reject;
        if (options) {
            disambiguation = toTemporalDisambiguation(globalObject, options);
            RETURN_IF_EXCEPTION(scope, nullptr);
            offsetOpt = toTemporalOffset(globalObject, options, TemporalOffsetDisambiguation::Reject);
            RETURN_IF_EXCEPTION(scope, nullptr);
            // overflow is read for spec observability even though strings are already parsed.
            toTemporalOverflow(globalObject, options);
            RETURN_IF_EXCEPTION(scope, nullptr);
        }

        ISO8601::PlainTime plainTime = plainTimeOptional.value_or(ISO8601::PlainTime());

        bool useStartOfDay = !plainTimeOptional.has_value()
            && offsetBehaviour == OffsetBehaviour::Wall;

        auto exactTimeResult = TemporalCore::interpretISODateTimeOffset(
            plainDate, plainTime, useStartOfDay,
            offsetBehaviour, offsetOpt, inlineOffsetNs, offsetHasSubMinutePrecision,
            timeZone, disambiguation);
        if (!exactTimeResult) {
            throwRangeError(globalObject, scope, exactTimeResult.error().message);
            return nullptr;
        }
        ISO8601::ExactTime exactTime = *exactTimeResult;

        String calendarId = "iso8601"_s;
        if (calendarOptional) {
            auto rawCal = String(calendarOptional->span()).convertToASCIILowercase();
            auto canonicalized = isBuiltinCalendar(rawCal);
            if (canonicalized)
                calendarId = intlAvailableCalendars().at(*canonicalized);
            else {
                throwRangeError(globalObject, scope, makeString("'"_s, rawCal, "' is not a valid calendar identifier"_s));
                return nullptr;
            }
        }

        // Use tryCreate to validate the epoch is within the representable range.
        RELEASE_AND_RETURN(scope, TemporalZonedDateTime::tryCreate(globalObject, globalObject->zonedDateTimeStructure(),
            exactTime, timeZone, WTF::move(timeZoneId), WTF::move(calendarId)));
    }

    // =========================================================
    // ZDT CLONE PATH
    // Validate options type before (no fields to read for clone).
    // Per spec, all three options are read even though cloning
    // does not apply disambiguation or offset options.
    // =========================================================
    if (itemValue.inherits<TemporalZonedDateTime>()) {
        auto* zdt = uncheckedDowncast<TemporalZonedDateTime>(itemValue);
        JSObject* options = nullptr;
        if (!optionsArg.isUndefined()) {
            options = optionsArg.getObject();
            if (!options) {
                throwTypeError(globalObject, scope, "Temporal.ZonedDateTime.from: options must be an object"_s);
                return nullptr;
            }
        }
        // Read all three options for spec observability (disambiguation, offset, overflow).
        if (options) {
            toTemporalDisambiguation(globalObject, options);
            RETURN_IF_EXCEPTION(scope, nullptr);
            toTemporalOffset(globalObject, options, TemporalOffsetDisambiguation::Reject);
            RETURN_IF_EXCEPTION(scope, nullptr);
            toTemporalOverflow(globalObject, options);
            RETURN_IF_EXCEPTION(scope, nullptr);
        }
        return TemporalZonedDateTime::create(vm, globalObject->zonedDateTimeStructure(),
            zdt->exactTime(), zdt->timeZone(), String(zdt->timeZoneId()), String(zdt->calendarId()));
    }

    // =========================================================
    // PROPERTY BAG PATH
    // Fields are read in alphabetical order per PrepareTemporalFields.
    // =========================================================
    if (!itemValue.isObject()) {
        throwTypeError(globalObject, scope, "Temporal.ZonedDateTime.from: argument must be a ZonedDateTime, string, or object"_s);
        return nullptr;
    }
    JSObject* bag = asObject(itemValue);

    // Step 1: Read calendar (always first, per ToTemporalCalendar).
    String calendarId = "iso8601"_s;
    {
        JSValue calendarValue = bag->get(globalObject, vm.propertyNames->calendar);
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (!calendarValue.isUndefined()) {
            calendarId = toTemporalCalendarIdentifier(globalObject, calendarValue);
            RETURN_IF_EXCEPTION(scope, nullptr);
        }
    }

    // Step 2: Read all remaining fields in alphabetical order (PrepareTemporalFields).

    // day (required)
    JSValue dayValue = bag->get(globalObject, vm.propertyNames->day);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (dayValue.isUndefined()) {
        throwTypeError(globalObject, scope, "day property must be present"_s);
        return nullptr;
    }
    double day = dayValue.toIntegerOrInfinity(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (!(day > 0 && std::isfinite(day))) {
        throwRangeError(globalObject, scope, "day property must be positive and finite"_s);
        return nullptr;
    }

    // era, eraYear (between day and hour, alphabetical)
    std::optional<String> zdtEra;
    std::optional<int32_t> zdtEraYear;
    {
        bool calUsesEras = calendarId != "iso8601"_s && calendarId != "chinese"_s && calendarId != "dangi"_s;
        if (calUsesEras) {
            JSValue eraValue = bag->get(globalObject, Identifier::fromString(vm, "era"_s));
            RETURN_IF_EXCEPTION(scope, nullptr);
            if (!eraValue.isUndefined()) {
                zdtEra = eraValue.toWTFString(globalObject);
                RETURN_IF_EXCEPTION(scope, nullptr);
            }

            JSValue eraYearValue = bag->get(globalObject, Identifier::fromString(vm, "eraYear"_s));
            RETURN_IF_EXCEPTION(scope, nullptr);
            if (!eraYearValue.isUndefined()) {
                double ey = eraYearValue.toIntegerOrInfinity(globalObject);
                RETURN_IF_EXCEPTION(scope, nullptr);
                if (!std::isfinite(ey)) {
                    throwRangeError(globalObject, scope, "eraYear property must be finite"_s);
                    return nullptr;
                }
                zdtEraYear = static_cast<int32_t>(ey);
            }

            // Era validation happens in calendarDateFromFields when building the date.
        }
    }

    // hour (optional, default 0)
    double hour = 0;
    {
        JSValue v = bag->get(globalObject, vm.propertyNames->hour);
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (!v.isUndefined()) {
            hour = v.toIntegerOrInfinity(globalObject);
            RETURN_IF_EXCEPTION(scope, nullptr);
            if (!std::isfinite(hour)) {
                throwRangeError(globalObject, scope, "Temporal time properties must be finite"_s);
                return nullptr;
            }
        }
    }

    // microsecond (optional, default 0)
    double microsecond = 0;
    {
        JSValue v = bag->get(globalObject, vm.propertyNames->microsecond);
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (!v.isUndefined()) {
            microsecond = v.toIntegerOrInfinity(globalObject);
            RETURN_IF_EXCEPTION(scope, nullptr);
            if (!std::isfinite(microsecond)) {
                throwRangeError(globalObject, scope, "Temporal time properties must be finite"_s);
                return nullptr;
            }
        }
    }

    // millisecond (optional, default 0)
    double millisecond = 0;
    {
        JSValue v = bag->get(globalObject, vm.propertyNames->millisecond);
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (!v.isUndefined()) {
            millisecond = v.toIntegerOrInfinity(globalObject);
            RETURN_IF_EXCEPTION(scope, nullptr);
            if (!std::isfinite(millisecond)) {
                throwRangeError(globalObject, scope, "Temporal time properties must be finite"_s);
                return nullptr;
            }
        }
    }

    // minute (optional, default 0)
    double minute = 0;
    {
        JSValue v = bag->get(globalObject, vm.propertyNames->minute);
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (!v.isUndefined()) {
            minute = v.toIntegerOrInfinity(globalObject);
            RETURN_IF_EXCEPTION(scope, nullptr);
            if (!std::isfinite(minute)) {
                throwRangeError(globalObject, scope, "Temporal time properties must be finite"_s);
                return nullptr;
            }
        }
    }

    // month (optional; required if monthCode absent)
    JSValue monthValue = bag->get(globalObject, vm.propertyNames->month);
    RETURN_IF_EXCEPTION(scope, nullptr);
    double month = 0;
    if (!monthValue.isUndefined()) {
        month = monthValue.toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, nullptr);
    }

    // monthCode (optional; per spec: ToPrimitive(value, ~string~) then RequireString)
    JSValue monthCodeValue = bag->get(globalObject, vm.propertyNames->monthCode);
    RETURN_IF_EXCEPTION(scope, nullptr);
    std::optional<ParsedMonthCode> parsedMonthCode;
    bool monthCodePresent = false;
    if (!monthCodeValue.isUndefined()) {
        // Spec: ToPrimitive(value, ~string~), then if not a String → TypeError.
        // This lets observable wrapper objects (with toString) work while rejecting
        // non-string primitives and objects whose toString returns a non-string.
        auto monthCodePrimitive = monthCodeValue.toPrimitive(globalObject, PreferString);
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (!monthCodePrimitive.isString()) {
            throwTypeError(globalObject, scope, "monthCode property must be a string"_s);
            return nullptr;
        }
        auto monthCodeString = asString(monthCodePrimitive)->value(globalObject);
        RETURN_IF_EXCEPTION(scope, nullptr);
        parsedMonthCode = ISO8601::parseMonthCode(monthCodeString);
        if (!parsedMonthCode) {
            throwRangeError(globalObject, scope, "Invalid monthCode property"_s);
            return nullptr;
        }
        monthCodePresent = true;
    }

    // nanosecond (optional, default 0)
    double nanosecond = 0;
    {
        JSValue v = bag->get(globalObject, vm.propertyNames->nanosecond);
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (!v.isUndefined()) {
            nanosecond = v.toIntegerOrInfinity(globalObject);
            RETURN_IF_EXCEPTION(scope, nullptr);
            if (!std::isfinite(nanosecond)) {
                throwRangeError(globalObject, scope, "Temporal time properties must be finite"_s);
                return nullptr;
            }
        }
    }

    // offset (optional; if present, must be a string or object coercible to string)
    std::optional<int64_t> givenOffsetNs;
    {
        JSValue offsetValue = bag->get(globalObject, vm.propertyNames->offset);
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (!offsetValue.isUndefined()) {
            String offsetStr;
            if (offsetValue.isString()) {
                offsetStr = asString(offsetValue)->value(globalObject);
                RETURN_IF_EXCEPTION(scope, nullptr);
            } else if (offsetValue.isObject()) {
                // Coerce objects via ToString() (e.g. {} → "[object Object]" → RangeError below).
                offsetStr = offsetValue.toWTFString(globalObject);
                RETURN_IF_EXCEPTION(scope, nullptr);
            } else {
                // null, number, boolean, bigint, symbol → TypeError.
                throwTypeError(globalObject, scope, "Temporal.ZonedDateTime.from: offset must be a string"_s);
                return nullptr;
            }
            givenOffsetNs = ISO8601::parseUTCOffset(offsetStr);
            if (!givenOffsetNs) {
                throwRangeError(globalObject, scope, makeString("'"_s, ellipsizeAt(100, offsetStr), "' is not a valid UTC offset string"_s));
                return nullptr;
            }
        }
    }

    // second (optional, default 0)
    double second = 0;
    {
        JSValue v = bag->get(globalObject, vm.propertyNames->second);
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (!v.isUndefined()) {
            second = v.toIntegerOrInfinity(globalObject);
            RETURN_IF_EXCEPTION(scope, nullptr);
            if (!std::isfinite(second)) {
                throwRangeError(globalObject, scope, "Temporal time properties must be finite"_s);
                return nullptr;
            }
        }
    }

    // timeZone (required)
    // Accepts: ZonedDateTime object (extract timeZoneId/TimeZone), or any string
    // accepted by parseTemporalTimeZoneIdentifier (bare name/offset or datetime string).
    TimeZone timeZone;
    String timeZoneId;
    {
        JSValue tzValue = bag->get(globalObject, vm.propertyNames->timeZone);
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (tzValue.isUndefined()) {
            throwTypeError(globalObject, scope, "Temporal.ZonedDateTime.from: timeZone property is required"_s);
            return nullptr;
        }
        if (tzValue.isObject() && tzValue.inherits<TemporalZonedDateTime>()) {
            // ZonedDateTime object: extract timezone directly.
            auto* zdtTz = uncheckedDowncast<TemporalZonedDateTime>(asObject(tzValue));
            timeZone = zdtTz->timeZone();
            timeZoneId = zdtTz->timeZoneId();
        } else if (tzValue.isString()) {
            // Only strings are accepted; no implicit coercion (spec says no valueOf/toString call).
            auto tzString = asString(tzValue)->value(globalObject);
            RETURN_IF_EXCEPTION(scope, nullptr);
            auto tzWithId = parseTemporalTimeZoneWithId(tzString);
            if (!tzWithId) {
                throwRangeError(globalObject, scope, makeString("'"_s, ellipsizeAt(100, tzString), "' is not a valid time zone identifier"_s));
                return nullptr;
            }
            timeZone = tzWithId->first;
            timeZoneId = WTF::move(tzWithId->second);
        } else {
            // null, boolean, number, bigint, Symbol, plain objects, Duration, etc. → TypeError.
            throwTypeError(globalObject, scope, "Temporal.ZonedDateTime.from: timeZone must be a string or ZonedDateTime"_s);
            return nullptr;
        }
    }

    // year (required)
    JSValue yearValue = bag->get(globalObject, vm.propertyNames->year);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (yearValue.isUndefined() && !monthCodePresent) {
        // If monthCode was given, year may still be required for PlainDate construction.
        // Per spec, year is always required for ZDT property bags.
    }
    if (yearValue.isUndefined() && !(zdtEra && zdtEraYear)) {
        throwTypeError(globalObject, scope, "year property must be present"_s);
        return nullptr;
    }
    double year = yearValue.isUndefined() ? 0 : yearValue.toIntegerOrInfinity(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (!std::isfinite(year)) {
        throwRangeError(globalObject, scope, "year property must be finite"_s);
        return nullptr;
    }

    // Step 3: Validate and read options AFTER all fields (required for spec order).
    // For property bags, spec requires reading all fields before options validation.
    JSObject* options = nullptr;
    if (!optionsArg.isUndefined()) {
        if (!optionsArg.isObject()) {
            throwTypeError(globalObject, scope, "Temporal.ZonedDateTime.from: options must be an object"_s);
            return nullptr;
        }
        options = asObject(optionsArg);
    }
    TemporalDisambiguation disambiguation = TemporalDisambiguation::Compatible;
    TemporalOffsetDisambiguation offsetOpt = TemporalOffsetDisambiguation::Reject;
    TemporalOverflow overflow = TemporalOverflow::Constrain;
    if (options) {
        disambiguation = toTemporalDisambiguation(globalObject, options);
        RETURN_IF_EXCEPTION(scope, nullptr);
        offsetOpt = toTemporalOffset(globalObject, options, TemporalOffsetDisambiguation::Reject);
        RETURN_IF_EXCEPTION(scope, nullptr);
        overflow = toTemporalOverflow(globalObject, options);
        RETURN_IF_EXCEPTION(scope, nullptr);
    }

    // Step 4: Validate and resolve month/monthCode.
    bool zdtIsNonISO = calendarId != "iso8601"_s;
    if (monthCodePresent) {
        ASSERT(parsedMonthCode);
        if (!zdtIsNonISO && (parsedMonthCode->isLeapMonth || parsedMonthCode->monthNumber < 1 || parsedMonthCode->monthNumber > 12)) {
            throwRangeError(globalObject, scope, "month code is not valid for ISO 8601 calendar"_s);
            return nullptr;
        }
        if (monthValue.isUndefined())
            month = parsedMonthCode->monthNumber;
        else if (month != static_cast<double>(parsedMonthCode->monthNumber)) {
            throwRangeError(globalObject, scope, "month and monthCode properties must match if both are provided"_s);
            return nullptr;
        }
    } else {
        if (monthValue.isUndefined()) {
            throwTypeError(globalObject, scope, "Either month or monthCode property must be provided"_s);
            return nullptr;
        }
        if (!(month > 0 && std::isfinite(month))) {
            throwRangeError(globalObject, scope, "month property must be positive and finite"_s);
            return nullptr;
        }
    }

    // Step 5: Build PlainDate (apply overflow to clamp or reject out-of-range values).
    ISO8601::PlainDate plainDate;
    if (zdtEra || zdtEraYear) {
        std::optional<StringView> era;
        if (zdtEra)
            era = StringView(*zdtEra);
        auto result = TemporalCore::calendarDateFromFields(
            TemporalCore::calendarIDFromString(calendarId), static_cast<int32_t>(year), static_cast<uint8_t>(month),
            static_cast<uint8_t>(day), era, zdtEraYear, parsedMonthCode, overflow);
        if (!result) {
            throwRangeError(globalObject, scope, String(result.error().message));
            return nullptr;
        }
        plainDate = *result;
    } else {
        // Validate/clamp at double level before double→unsigned cast (UB for >= 2^32, wraps on x86).
        if (!zdtIsNonISO) {
            if (overflow == TemporalOverflow::Constrain) {
                month = std::clamp(month, 1.0, 12.0);
                day = std::clamp(day, 1.0, 31.0);
            } else {
                if (!(month >= 1 && month <= 12)) {
                    throwRangeError(globalObject, scope, "month is out of range"_s);
                    return nullptr;
                }
                if (!(day >= 1 && day <= 31)) {
                    throwRangeError(globalObject, scope, "day is out of range"_s);
                    return nullptr;
                }
            }
        }
        plainDate = isoDateFromFields(globalObject,
            TemporalDateFormat::Date,
            static_cast<int32_t>(year),
            static_cast<unsigned>(month),
            static_cast<unsigned>(day),
            parsedMonthCode,
            overflow,
            calendarId);
        RETURN_IF_EXCEPTION(scope, nullptr);
    }

    // Step 6: Build PlainTime (apply overflow to clamp or reject out-of-range values).
    ISO8601::Duration timeDur;
    timeDur.setField(TemporalUnit::Hour, hour);
    timeDur.setField(TemporalUnit::Minute, minute);
    timeDur.setField(TemporalUnit::Second, second);
    timeDur.setField(TemporalUnit::Millisecond, millisecond);
    timeDur.setField(TemporalUnit::Microsecond, microsecond);
    timeDur.setField(TemporalUnit::Nanosecond, nanosecond);
    auto plainTime = TemporalPlainTime::regulateTime(globalObject, WTF::move(timeDur), overflow);
    RETURN_IF_EXCEPTION(scope, nullptr);

    // Step 7: Apply InterpretISODateTimeOffset (property bag uses "option" offset behaviour).
    ISO8601::ExactTime exactTime;
    if (!givenOffsetNs || offsetOpt == TemporalOffsetDisambiguation::Ignore) {
        // No offset field, or "ignore": resolve using timezone + disambiguation.
        auto epochNs = getEpochNanosecondsFor(globalObject, timeZone, plainDate, plainTime, disambiguation);
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (!epochNs)
            return nullptr;
        exactTime = *epochNs;
    } else if (offsetOpt == TemporalOffsetDisambiguation::Use) {
        // "use": compute epoch directly from the given offset (epochNs = naiveNs - offsetNs).
        Int128 naiveNs = getUTCEpochNanoseconds({ plainDate, plainTime });
        exactTime = ISO8601::ExactTime(naiveNs - Int128(*givenOffsetNs));
    } else {
        // "prefer" or "reject": check if the given offset matches any possible instant.
        // CheckISODaysRange: plain date must be within ±10^8 days (spec §CheckISODaysRange).
        double dayCount = dateToDaysFrom1970(plainDate.year(), static_cast<int>(plainDate.month()) - 1, static_cast<int>(plainDate.day()));
        if (std::abs(dayCount) > 1e8) {
            throwRangeError(globalObject, scope, "wall-clock date is outside the representable range for Temporal.ZonedDateTime"_s);
            return nullptr;
        }
        auto possible = TemporalCore::getPossibleEpochNanosecondsFor(timeZone, plainDate, plainTime);
        if (!possible) {
            throwRangeError(globalObject, scope, possible.error().message);
            return nullptr;
        }
        bool found = false;
        for (auto& candidate : TemporalCore::epochCandidates(*possible)) {
            auto offsetResult = TemporalCore::getOffsetNanosecondsFor(timeZone, candidate);
            if (offsetResult && *offsetResult == *givenOffsetNs) {
                exactTime = candidate;
                found = true;
                break;
            }
        }
        if (!found) {
            if (offsetOpt == TemporalOffsetDisambiguation::Reject) {
                throwRangeError(globalObject, scope, "offset does not agree with timezone for the given date/time"_s);
                return nullptr;
            }
            // "prefer" but no match: fall through to disambiguation.
            auto epochNs = getEpochNanosecondsFor(globalObject, timeZone, plainDate, plainTime, disambiguation);
            RETURN_IF_EXCEPTION(scope, nullptr);
            if (!epochNs)
                return nullptr;
            exactTime = *epochNs;
        }
    }

    RELEASE_AND_RETURN(scope, TemporalZonedDateTime::tryCreate(globalObject, globalObject->zonedDateTimeStructure(),
        exactTime, timeZone, WTF::move(timeZoneId), WTF::move(calendarId)));
}

std::optional<ISO8601::ExactTime> TemporalZonedDateTime::getEpochNanosecondsFor(
    JSGlobalObject* globalObject,
    const TimeZone& timeZone,
    const ISO8601::PlainDate& date,
    const ISO8601::PlainTime& time,
    TemporalDisambiguation disambiguation)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto result = TemporalCore::getEpochNanosecondsFor(timeZone, date, time, disambiguation);
    if (!result) {
        throwRangeError(globalObject, scope, result.error().message);
        return std::nullopt;
    }
    return *result;
}

} // namespace JSC
