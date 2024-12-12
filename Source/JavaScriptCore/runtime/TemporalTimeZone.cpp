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
#include "ParseInt.h"
#include <wtf/text/StringParsingBuffer.h>

namespace JSC {

const ClassInfo TemporalTimeZone::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(TemporalTimeZone) };

TemporalTimeZone* TemporalTimeZone::createFromID(VM& vm, Structure* structure, TimeZoneID identifier)
{
    TemporalTimeZone* format = new (NotNull, allocateCell<TemporalTimeZone>(vm)) TemporalTimeZone(vm, structure, TimeZone { std::in_place_index_t<0>(), identifier });
    format->finishCreation(vm);
    return format;
}

TemporalTimeZone* TemporalTimeZone::createFromUTCOffset(VM& vm, Structure* structure, int64_t utcOffset)
{
    TemporalTimeZone* format = new (NotNull, allocateCell<TemporalTimeZone>(vm)) TemporalTimeZone(vm, structure, TimeZone { std::in_place_index_t<1>(), utcOffset });
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

// https://tc39.es/proposal-temporal/#sec-parsetimezoneidentifier
static std::optional<TimeZoneID> parseTimeZoneIdentifier(StringView identifier)
{
/*
    auto parseResult = parseUTCOffset(identifier);
    bool isIANAName = false;
    if (!parseResult) {
        isIANAName = true;
        parseResult = parseTimeZoneIANAName(identifier);
    }
    if (!parseResult)
        return std::nullopt;
       
    if (isIANAName)
        return TimeZoneRecord { parseResult, 0 };
    Int128 offsetNanoseconds = parseDateTimeUTCOffset(identifier);
    Int128 offsetMinutes = offsetNanoseconds / (60 * 1000000000);
    ASSERT(offsetNanoseconds % (60 * 1000000000) == 0);
    return TimeZoneRecord { "", offsetMinutes };
*/
    if (identifier == "UTC"_s)
        return utcTimeZoneID();
    return std::nullopt; // TODO
}

template<typename CharacterType>
std::optional<int64_t> parseDateTimeUTCOffset_(StringParsingBuffer<CharacterType>& buffer)
{
    //  UTCOffset[SubMinutePrecision] :::
    //      ASCIISign Hour
    //      ASCIISign Hour TimeSeparator[+Extended] MinuteSecond
    //      ASCIISign Hour TimeSeparator[~Extended] MinuteSecond
    //      [+SubMinutePrecision] ASCIISign Hour TimeSeparator[+Extended] MinuteSecond TimeSeparator[+Extended] MinuteSecond TemporalDecimalFractionopt
    //      [+SubMinutePrecision] ASCIISign Hour TimeSeparator[~Extended] MinuteSecond TimeSeparator[~Extended] MinuteSecond TemporalDecimalFractionopt

    int sign = 1;
    if (*buffer == '+')
        buffer.advance();
    else if (*buffer == '-') {
        sign = -1;
        buffer.advance();
    }

    unsigned digits = 1;
    while (digits < buffer.lengthRemaining() && isASCIIDigit(buffer[digits]))
        digits++;

    double hours = parseInt(buffer.span().first(digits), 10);
    if (hours > 23)
        return std::nullopt;
    buffer.advanceBy(digits);

    if (*buffer != ':') 
        return std::nullopt;
    buffer.advance();

    digits = 1;
    while (digits < buffer.lengthRemaining() && isASCIIDigit(buffer[digits]))
        digits++;
    
    double minutes = parseInt(buffer.span().first(digits), 10);
    if (minutes > 59)
        return std::nullopt;
    buffer.advanceBy(digits);
    
    return sign * (((hours * 60 + minutes)));
} 

std::optional<int64_t> TemporalTimeZone::parseDateTimeUTCOffset(StringView string)
{
    return readCharactersForParsing(string, [](auto buffer) -> std::optional<int64_t> {
            return parseDateTimeUTCOffset_(buffer);
    });
}

// https://tc39.es/proposal-temporal/#prod-TimeZoneIdentifier
bool canBeTimeZoneIdentifier(StringView string)
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
    if (string[0] == '+' || string[0] == '-')
        return true;
    if (isASCIIAlpha(string[0]) || string[0] == '.' || string[0] == '_')
        return true;
    return false;
}

#if false
// https://tc39.es/proposal-temporal/#sec-temporal-parsetemporaltimezonestring
template<typename CharacterType>
std::optional<ISO8601::TimeZone> TemporalTimeZone::parseTemporalTimeZoneString(StringParsingBuffer<CharacterType>& buffer)
{
    if (canBeTimeZoneIdentifier(buffer))
        return parseTimeZoneIdentifier(buffer);
    TimeZoneRecord result;
/*
    if (canBeTemporalDateTimeString(buffer))
        result = parseISODateTime(buffer, TemporalParseTimeZoneFormat::DateTime);
    else if (canBeTemporalInstantString(buffer))
        result = parseISODateTime(buffer, TemporalParseTimeZoneFormat::Instant);
    else if (canBeTemporalTimeString(buffer))
        result = parseISODateTime(buffer, TemporalParseTimeZoneFormat::Time);
    else if (canBeTemporalMonthDayString(buffer))
        result = parseISODateTime(buffer, TemporalParseTimeZoneFormat::MonthDay);
    else if (canBeTemporalYearMonthString(buffer))
        result = parseISODateTime(buffer, TemporalParseTimeZoneFormat::YearMonth);
*/
    result = parseISODateTime(buffer, false);
    auto timeZoneResult = result.timeZone;
    if (timeZoneResult.m_annotation)
        return parseTimeZoneIdentifier(timeZoneResult.m_annotation);
    if (timeZoneResult.m_z)
        return utcTimeZoneID();
    if (timeZoneResult.m_offset)
        return offsetTimeZone(timeZoneResult.m_offset);
    return std::nullopt;
}
#endif

// https://tc39.es/proposal-temporal/#sec-temporal-parsetemporaltimezonestring
std::optional<ISO8601::TimeZone> TemporalTimeZone::parseTemporalTimeZoneString(StringView timeZoneString)
{
    if (canBeTimeZoneIdentifier(timeZoneString))
        return parseTimeZoneIdentifier(timeZoneString);
    // TODO
    // return parseTimeZoneIdentifier(timeZoneString);
    ISO8601::TimeZoneRecord result;
    auto asDateTime = ISO8601::parseCalendarDateTime(timeZoneString, TemporalDateFormat::Date);
    if (asDateTime) {
        auto [date, optionalTime, optionalTimeZoneRecord, optionalCalendarRecord] = asDateTime.value();
        if (optionalTimeZoneRecord)
            result = optionalTimeZoneRecord.value();
        else
            return std::nullopt;
    }
    else {
        auto asExactTime = ISO8601::parseInstant(timeZoneString);
        if (asExactTime) {
            // TODO
/*
            auto [exactTime, optionalTimeZoneRecord] = asExactTime.value();
            if (optionalTimeZoneRecord)
                result = optionalTimeZoneRecord.value();
            else
*/
                return std::nullopt;
        } else {
            auto asTime = ISO8601::parseCalendarTime(timeZoneString);
            if (asTime) {
                auto [time, optionalTimeZoneRecord, optionalCalendarRecord] = asTime.value();
                if (optionalTimeZoneRecord)
                    result = optionalTimeZoneRecord.value();
                else
                    return std::nullopt;
            }
            else {
                auto asMonthDay = ISO8601::parseCalendarDateTime(timeZoneString, TemporalDateFormat::MonthDay);
                if (asMonthDay) {
                    auto [date, optionalTime, optionalTimeZoneRecord, optionalCalendarRecord] = asMonthDay.value();
                    if (optionalTimeZoneRecord)
                        result = optionalTimeZoneRecord.value();
                    else
                        return std::nullopt;
                }
                else {
                    auto asYearMonth = ISO8601::parseCalendarDateTime(timeZoneString, TemporalDateFormat::YearMonth);
                    if (asYearMonth) {
                        auto [date, optionalTime, optionalTimeZoneRecord, optionalCalendarRecord] = asYearMonth.value();
                        if (optionalTimeZoneRecord)
                            result = optionalTimeZoneRecord.value();
                        else
                            return std::nullopt;
                    } else {
                        return std::nullopt;
                    }
                }
            }
        }
    }
    auto timeZoneResult = result; // TODO: ???
    if (timeZoneResult.m_annotation) {
        return parseTimeZoneIdentifier(WTF::String(timeZoneResult.m_annotation.value()));
    }
    if (timeZoneResult.m_z)
        return utcTimeZoneID();
    if (timeZoneResult.m_offset_string) {
        // Check for sub-minute precision in offset string
        Vector<LChar> ignore;
        auto result = ISO8601::parseUTCOffset(WTF::String(std::get<0>(timeZoneResult.m_offset_string.value())),
            ignore, false);
        if (!result)
            return std::nullopt;
        return std::get<1>(timeZoneResult.m_offset_string.value());
    }
    return std::nullopt;
// TODO: check that argument-propertybag-timezone-string-datetime.js works
}

// https://tc39.es/proposal-temporal/#sec-temporal.timezone.from
// https://tc39.es/proposal-temporal/#sec-temporal-totemporaltimezone
JSObject* TemporalTimeZone::from(JSGlobalObject* globalObject, JSValue timeZoneLike)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (timeZoneLike.isObject()) {
        JSObject* timeZoneLikeObject = jsCast<JSObject*>(timeZoneLike);

        // FIXME: We need to implement code retrieving TimeZone from Temporal Date Like objects. But
        // currently they are not implemented yet.

        bool hasProperty = timeZoneLikeObject->hasProperty(globalObject, vm.propertyNames->timeZone);
        RETURN_IF_EXCEPTION(scope, { });
        if (!hasProperty)
            return timeZoneLikeObject;

        timeZoneLike = timeZoneLikeObject->get(globalObject, vm.propertyNames->timeZone);
        if (timeZoneLike.isObject()) {
            JSObject* timeZoneLikeObject = jsCast<JSObject*>(timeZoneLike);
            bool hasProperty = timeZoneLikeObject->hasProperty(globalObject, vm.propertyNames->timeZone);
            RETURN_IF_EXCEPTION(scope, { });
            if (!hasProperty)
                return timeZoneLikeObject;
        }
    }

    auto timeZoneString = timeZoneLike.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    Vector<LChar> ignore;
    std::optional<int64_t> utcOffset = ISO8601::parseUTCOffset(timeZoneString, ignore);
    if (utcOffset)
        return TemporalTimeZone::createFromUTCOffset(vm, globalObject->timeZoneStructure(), utcOffset.value());

    std::optional<TimeZoneID> identifier = ISO8601::parseTimeZoneName(timeZoneString);
    if (identifier)
        return TemporalTimeZone::createFromID(vm, globalObject->timeZoneStructure(), identifier.value());

    std::optional<ISO8601::TimeZone> utcOffsetFromInstant = parseTemporalTimeZoneString(timeZoneString);
    if (utcOffsetFromInstant) {
        if (std::holds_alternative<int64_t>(utcOffsetFromInstant.value()))
            return TemporalTimeZone::createFromUTCOffset(vm, globalObject->timeZoneStructure(), std::get<int64_t>(utcOffsetFromInstant.value()));
        else
            return TemporalTimeZone::createFromID(vm, globalObject->timeZoneStructure(), std::get<TimeZoneID>(utcOffsetFromInstant.value()));
    }


    throwRangeError(globalObject, scope, "argument needs to be UTC offset string, TimeZone identifier, or temporal Instant string"_s);
    return { };
}

} // namespace JSC
