/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "TemporalTimeZone.h"

#include "ISO8601.h"
#include "JSObjectInlines.h"
#include "TemporalZonedDateTime.h"

namespace JSC {

const ClassInfo TemporalTimeZone::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(TemporalTimeZone) };

TemporalTimeZone* TemporalTimeZone::createFromID(VM& vm, Structure* structure, TimeZoneID identifier)
{
    TemporalTimeZone* format = new (NotNull, allocateCell<TemporalTimeZone>(vm)) TemporalTimeZone(vm, structure, TimeZone::named(identifier));
    format->finishCreation(vm);
    return format;
}

TemporalTimeZone* TemporalTimeZone::createFromUTCOffset(VM& vm, Structure* structure, int64_t utcOffset)
{
    TemporalTimeZone* format = new (NotNull, allocateCell<TemporalTimeZone>(vm)) TemporalTimeZone(vm, structure, TimeZone::offset(utcOffset));
    format->finishCreation(vm);
    return format;
}

TemporalTimeZone* TemporalTimeZone::createFromTimeZone(VM& vm, Structure* structure, ISO8601::TimeZone tz)
{
    TemporalTimeZone* format = new (NotNull, allocateCell<TemporalTimeZone>(vm)) TemporalTimeZone(vm, structure, tz);
    format->finishCreation(vm);
    return format;
}

Structure* TemporalTimeZone::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalTimeZone::TemporalTimeZone(VM& vm, Structure* structure, TimeZone timeZone)
    : Base(vm, structure)
    , m_timeZone(timeZone)
{
}

// https://tc39.es/proposal-temporal/#sec-temporal-getoffsetnanosecondsfor
Int128 TemporalTimeZone::getOffsetNanosecondsFor(ISO8601::TimeZone timeZone, Int128 epochNs)
{
    if (timeZone.isOffset())
        return timeZone.offsetNanoseconds();
    return ISO8601::getNamedTimeZoneOffsetNanoseconds(timeZone.asID(), epochNs);
}

// https://tc39.es/proposal-temporal/#sec-temporal-getisodatetimefor
ISO8601::PlainDateTime TemporalTimeZone::getISODateTimeFor(JSGlobalObject* globalObject, ISO8601::TimeZone timeZone, ISO8601::ExactTime epochNs)
{
    auto offsetNanoseconds = getOffsetNanosecondsFor(timeZone, epochNs.epochNanoseconds());
    auto result = TemporalCalendar::getISOPartsFromEpoch(epochNs);
    auto date = result.date();
    auto time = result.time();
    // time.nanosecond() + offsetNanoseconds can overflow int32_t, hence
    // why the arguments to balanceISODateTime are Int128s.
    return TemporalPlainDateTime::balanceISODateTime(globalObject, date.year(), date.month(), date.day(), time.hour(), time.minute(), time.second(), time.millisecond(), time.microsecond(), time.nanosecond() + offsetNanoseconds);
}


// https://tc39.es/proposal-temporal/#sec-getavailablenamedtimezoneidentifier
std::optional<ISO8601::TimeZone> TemporalTimeZone::getAvailableNamedTimeZoneIdentifier(JSGlobalObject* globalObject, TimeZoneID timeZoneIdentifier)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (timeZoneIdentifier == utcTimeZoneID())
        return ISO8601::TimeZone::offset(0);
    // FIXME: support named time zones
    throwRangeError(globalObject, scope, "getAvailableNamedTimeZoneIdentifier() not yet implemented"_s);
    return { };
}

// https://tc39.es/proposal-temporal/#sec-getavailablenamedtimezoneidentifier
std::optional<ISO8601::TimeZone> TemporalTimeZone::getAvailableNamedTimeZoneIdentifier(JSGlobalObject* globalObject, const Vector<Latin1Character>& chars)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (isUTCTimeZoneString(StringView(chars)))
        return ISO8601::TimeZone::offset(0);
    throwRangeError(globalObject, scope, "getAvailableNamedTimeZoneIdentifier() not yet implemented"_s);
    return { };
}

static std::optional<TimeZoneID> parseTimeZoneIANAName(StringView)
{
    // FIXME: support named time zones
    return std::nullopt;
}

// https://tc39.es/proposal-temporal/#sec-parsetimezoneidentifier
std::optional<ISO8601::TimeZone> TemporalTimeZone::parseTimeZoneIdentifier(StringView identifier)
{
    if (isUTCTimeZoneString(identifier))
        return ISO8601::TimeZone::utc();

    auto parseResult = ISO8601::parseUTCOffset(identifier, false); // Don't accept sub-minute precision
    bool isIANAName = false;
    if (!parseResult) {
        isIANAName = true;
        parseResult = parseTimeZoneIANAName(identifier);
    }
    if (!parseResult) [[unlikely]]
        return std::nullopt;

    if (isIANAName)
        return ISO8601::TimeZone::named(parseResult.value());

    int64_t offsetNanoseconds = parseResult.value();
    ASSERT(!(offsetNanoseconds % ISO8601::ExactTime::nsPerMinute));
    return ISO8601::TimeZone::offset(offsetNanoseconds);
}

static std::optional<ISO8601::TimeZone> parseTimeZoneFromAnnotation(const ISO8601::TimeZoneAnnotation& annotation)
{
    if (annotation.m_offset) {
        auto offsetNanoseconds = annotation.m_offset.value();
        ASSERT(!(offsetNanoseconds % ISO8601::ExactTime::nsPerMinute));
        return ISO8601::TimeZone::offset(offsetNanoseconds);
    }

    return TemporalTimeZone::parseTimeZoneIdentifier(WTF::String(annotation.m_annotation));
}

// https://tc39.es/proposal-temporal/#prod-TimeZoneIdentifier
static bool canBeTimeZoneIdentifier(StringView string)
{
    //  TimeZoneIdentifier :::
    //      UTCOffset[~SubMinutePrecision]
    //      TimeZoneIANAName
    //
    //  UTCOffset[SubMinutePrecision] :::
    //      ASCIISign Hour
    //      ASCIISign Hour TimeSeparator[+Extended] MinuteSecond
    //      ASCIISign Hour TimeSeparator[~Extended] MinuteSecond
    //      [+SubMinutePrecision] ASCIISign Hour TimeSeparator[+Extended] MinuteSecond TimeSeparator[+Extended] MinuteSecond TemporalDecimalFractionopt
    //      [+SubMinutePrecision] ASCIISign Hour TimeSeparator[~Extended] MinuteSecond TimeSeparator[~Extended] MinuteSecond TemporalDecimalFractionopt
    //
    //  TimeZoneIANAName :::
    //      TimeZoneIANANameComponent
    //      TimeZoneIANAName / TimeZoneIANANameComponent
    //
    //  TimeZoneIANANameComponent :::
    //      TZLeadingChar
    //      TimeZoneIANANameComponent TZChar
    //
    //  TZLeadingChar :::
    //      Alpha
    //      .
    //      _
    //
    if (string.isEmpty())
        return false;
    if (string[0] == '+' || string[0] == '-')
        return true;
    if (isASCIIAlpha(string[0]) || string[0] == '.' || string[0] == '_')
        return true;
    return false;
}

// https://tc39.es/proposal-temporal/#sec-temporal-parsetemporaltimezonestring
std::optional<ISO8601::TimeZone> TemporalTimeZone::parseTemporalTimeZoneString(StringView timeZoneString)
{
    if (canBeTimeZoneIdentifier(timeZoneString))
        return parseTimeZoneIdentifier(timeZoneString);
    ISO8601::TimeZoneRecord timeZoneResult;
    auto asDateTime = ISO8601::parseCalendarDateTime(timeZoneString, TemporalDateFormat::Date);
    if (asDateTime) {
        auto [date, optionalTime, optionalTimeZoneRecord, optionalCalendarRecord] = asDateTime.value();
        if (optionalTimeZoneRecord)
            timeZoneResult = optionalTimeZoneRecord.value();
        else
            return std::nullopt;
    } else {
        auto asExactTime = ISO8601::parseInstant(timeZoneString);
        if (asExactTime) {
            // FIXME: support parsing time zone annotation from Instant
            return std::nullopt;
        }
        auto asTime = ISO8601::parseCalendarTime(timeZoneString);
        if (asTime) {
            auto [time, optionalTimeZoneRecord, optionalCalendarRecord] = asTime.value();
            if (optionalTimeZoneRecord)
                timeZoneResult = optionalTimeZoneRecord.value();
            else
                return std::nullopt;
        } else {
            auto asMonthDay = ISO8601::parseCalendarDateTime(timeZoneString, TemporalDateFormat::MonthDay);
            if (asMonthDay) {
                auto [date, optionalTime, optionalTimeZoneRecord, optionalCalendarRecord] = asMonthDay.value();
                if (optionalTimeZoneRecord)
                    timeZoneResult = optionalTimeZoneRecord.value();
                else
                    return std::nullopt;
            } else {
                auto asYearMonth = ISO8601::parseCalendarDateTime(timeZoneString, TemporalDateFormat::YearMonth);
                if (asYearMonth) [[likely]] {
                    auto [date, optionalTime, optionalTimeZoneRecord, optionalCalendarRecord] = asYearMonth.value();
                    if (optionalTimeZoneRecord)
                        timeZoneResult = optionalTimeZoneRecord.value();
                    else
                        return std::nullopt;
                } else
                    return std::nullopt;
            }
        }
    }
    if (timeZoneResult.m_annotation)
        return parseTimeZoneFromAnnotation(timeZoneResult.m_annotation.value());
    if (timeZoneResult.m_z)
        return ISO8601::TimeZone::utc();
    if (timeZoneResult.m_offset) {
        // Check for sub-minute precision in offset string
        auto result = ISO8601::parseUTCOffset(WTF::String(timeZoneResult.m_offset->m_offsetString), false);
        if (!result) [[unlikely]]
            return std::nullopt;
        return ISO8601::TimeZone::offset(timeZoneResult.m_offset->m_offset);
    }
    return std::nullopt;
}

TemporalTimeZone* TemporalTimeZone::from(JSGlobalObject* globalObject, JSValue timeZoneLike)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (timeZoneLike.isObject()) {
        if (timeZoneLike.inherits<TemporalZonedDateTime>()) {
            TemporalZonedDateTime* zonedDateTime = jsCast<TemporalZonedDateTime*>(timeZoneLike);
            return TemporalTimeZone::createFromTimeZone(vm, globalObject->timeZoneStructure(), zonedDateTime->timeZone());
        }
    }

    auto timeZoneString = timeZoneLike.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    std::optional<int64_t> utcOffset = ISO8601::parseUTCOffset(timeZoneString);
    if (utcOffset)
        return TemporalTimeZone::createFromUTCOffset(vm, globalObject->timeZoneStructure(), utcOffset.value());

    std::optional<TimeZoneID> identifier = ISO8601::parseTimeZoneName(timeZoneString);
    if (identifier)
        return TemporalTimeZone::createFromID(vm, globalObject->timeZoneStructure(), identifier.value());

    std::optional<ISO8601::TimeZone> utcOffsetFromInstant = TemporalTimeZone::parseTemporalTimeZoneString(timeZoneString);
    if (utcOffsetFromInstant) {
        if (utcOffsetFromInstant->isOffset())
            return TemporalTimeZone::createFromUTCOffset(vm, globalObject->timeZoneStructure(), utcOffsetFromInstant->offsetNanoseconds());
        return TemporalTimeZone::createFromID(vm, globalObject->timeZoneStructure(), utcOffsetFromInstant->asID());
    }


    throwRangeError(globalObject, scope, "argument needs to be UTC offset string, TimeZone identifier, or temporal Instant string"_s);
    return { };
}

} // namespace JSC
