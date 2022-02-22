/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
 * Copyright (C) 2021 Apple Inc.
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
#include "ISO8601.h"

#include "IntlObject.h"
#include "ParseInt.h"
#include <limits>
#include <wtf/CheckedArithmetic.h>
#include <wtf/DateMath.h>
#include <wtf/WallTime.h>
#include <wtf/text/StringParsingBuffer.h>
#include <wtf/unicode/CharacterNames.h>

namespace JSC {
namespace ISO8601 {

static constexpr int64_t nsPerHour = 1000LL * 1000 * 1000 * 60 * 60;
static constexpr int64_t nsPerMinute = 1000LL * 1000 * 1000 * 60;
static constexpr int64_t nsPerSecond = 1000LL * 1000 * 1000;
static constexpr int64_t nsPerMillisecond = 1000LL * 1000;
static constexpr int64_t nsPerMicrosecond = 1000LL;

std::optional<TimeZoneID> parseTimeZoneName(StringView string)
{
    const auto& timeZones = intlAvailableTimeZones();
    for (unsigned index = 0; index < timeZones.size(); ++index) {
        if (equalIgnoringASCIICase(timeZones[index], string))
            return index;
    }
    return std::nullopt;
}

template<typename CharType>
static int32_t parseDecimalInt32(const CharType* characters, unsigned length)
{
    int32_t result = 0;
    for (unsigned index = 0; index < length; ++index) {
        ASSERT(isASCIIDigit(characters[index]));
        result = (result * 10) + characters[index] - '0';
    }
    return result;
}

// DurationHandleFractions ( fHours, minutes, fMinutes, seconds, fSeconds, milliseconds, fMilliseconds, microseconds, fMicroseconds, nanoseconds, fNanoseconds )
// https://tc39.es/proposal-temporal/#sec-temporal-durationhandlefractions
static void handleFraction(Duration& duration, int factor, StringView fractionString, TemporalUnit fractionType)
{
    ASSERT(fractionString.length() && fractionString.length() <= 9 && fractionString.isAllASCII());
    ASSERT(fractionType == TemporalUnit::Hour || fractionType == TemporalUnit::Minute || fractionType == TemporalUnit::Second);

    if (fractionType == TemporalUnit::Second) {
        Vector<LChar, 9> padded(9, '0');
        for (unsigned i = 0; i < fractionString.length(); i++)
            padded[i] = fractionString[i];
        duration.setMilliseconds(factor * parseDecimalInt32(padded.data(), 3));
        duration.setMicroseconds(factor * parseDecimalInt32(padded.data() + 3, 3));
        duration.setNanoseconds(factor * parseDecimalInt32(padded.data() + 6, 3));
        return;
    }

    double fraction = factor * parseInt(fractionString, 10) / std::pow(10, fractionString.length());
    if (!fraction)
        return;

    if (fractionType == TemporalUnit::Hour) {
        fraction *= 60;
        duration.setMinutes(std::trunc(fraction));
        fraction = std::fmod(fraction, 1);
        if (!fraction)
            return;
    }

    fraction *= 60;
    duration.setSeconds(std::trunc(fraction));
    fraction = std::fmod(fraction, 1);
    if (!fraction)
        return;

    fraction *= 1000;
    duration.setMilliseconds(std::trunc(fraction));
    fraction = std::fmod(fraction, 1);
    if (!fraction)
        return;

    fraction *= 1000;
    duration.setMicroseconds(std::trunc(fraction));
    fraction = std::fmod(fraction, 1);
    if (!fraction)
        return;

    duration.setNanoseconds(std::trunc(fraction * 1000));
}

// ParseTemporalDurationString ( isoString )
// https://tc39.es/proposal-temporal/#sec-temporal-parsetemporaldurationstring
template<typename CharacterType>
static std::optional<Duration> parseDuration(StringParsingBuffer<CharacterType>& buffer)
{
    // ISO 8601 duration strings are like "-P1Y2M3W4DT5H6M7.123456789S". Notes:
    // - case insensitive
    // - sign: + - −(U+2212)
    // - separator: . ,
    // - T is present iff there is a time part
    // - integral parts can have any number of digits but fractional parts have at most 9
    // - hours and minutes can have fractional parts too, but only as the LAST part of the string
    if (buffer.lengthRemaining() < 3)
        return std::nullopt;

    Duration result;

    int factor = 1;
    if (*buffer == '+')
        buffer.advance();
    else if (*buffer == '-' || *buffer == minusSign) {
        factor = -1;
        buffer.advance();
    }

    if (toASCIIUpper(*buffer) != 'P')
        return std::nullopt;

    buffer.advance();
    for (unsigned datePartIndex = 0; datePartIndex < 4 && buffer.hasCharactersRemaining() && isASCIIDigit(*buffer); buffer.advance()) {
        unsigned digits = 1;
        while (digits < buffer.lengthRemaining() && isASCIIDigit(buffer[digits]))
            digits++;

        double integer = factor * parseInt({ buffer.position(), digits }, 10);
        buffer.advanceBy(digits);
        if (buffer.atEnd())
            return std::nullopt;

        switch (toASCIIUpper(*buffer)) {
        case 'Y':
            if (datePartIndex)
                return std::nullopt;
            result.setYears(integer);
            datePartIndex = 1;
            break;
        case 'M':
            if (datePartIndex >= 2)
                return std::nullopt;
            result.setMonths(integer);
            datePartIndex = 2;
            break;
        case 'W':
            if (datePartIndex >= 3)
                return std::nullopt;
            result.setWeeks(integer);
            datePartIndex = 3;
            break;
        case 'D':
            result.setDays(integer);
            datePartIndex = 4;
            break;
        default:
            return std::nullopt;
        }
    }

    if (buffer.atEnd())
        return result;

    if (buffer.lengthRemaining() < 3 || toASCIIUpper(*buffer) != 'T')
        return std::nullopt;

    buffer.advance();
    for (unsigned timePartIndex = 0; timePartIndex < 3 && buffer.hasCharactersRemaining() && isASCIIDigit(*buffer); buffer.advance()) {
        unsigned digits = 1;
        while (digits < buffer.lengthRemaining() && isASCIIDigit(buffer[digits]))
            digits++;

        double integer = factor * parseInt({ buffer.position(), digits }, 10);
        buffer.advanceBy(digits);
        if (buffer.atEnd())
            return std::nullopt;

        StringView fractionalPart;
        if (*buffer == '.' || *buffer == ',') {
            buffer.advance();
            digits = 0;
            while (digits < buffer.lengthRemaining() && isASCIIDigit(buffer[digits]))
                digits++;
            if (!digits || digits > 9)
                return std::nullopt;

            fractionalPart = { buffer.position(), digits };
            buffer.advanceBy(digits);
            if (buffer.atEnd())
                return std::nullopt;
        }

        switch (toASCIIUpper(*buffer)) {
        case 'H':
            if (timePartIndex)
                return std::nullopt;
            result.setHours(integer);
            if (fractionalPart) {
                handleFraction(result, factor, fractionalPart, TemporalUnit::Hour);
                timePartIndex = 3;
            } else
                timePartIndex = 1;
            break;
        case 'M':
            if (timePartIndex >= 2)
                return std::nullopt;
            result.setMinutes(integer);
            if (fractionalPart) {
                handleFraction(result, factor, fractionalPart, TemporalUnit::Minute);
                timePartIndex = 3;
            } else
                timePartIndex = 2;
            break;
        case 'S':
            result.setSeconds(integer);
            if (fractionalPart)
                handleFraction(result, factor, fractionalPart, TemporalUnit::Second);
            timePartIndex = 3;
            break;
        default:
            return std::nullopt;
        }
    }

    if (buffer.hasCharactersRemaining())
        return std::nullopt;

    return result;
}

std::optional<Duration> parseDuration(StringView string)
{
    return readCharactersForParsing(string, [](auto buffer) -> std::optional<Duration> {
        return parseDuration(buffer);
    });
}


enum class Second60Mode { Accept, Reject };
template<typename CharacterType>
static std::optional<PlainTime> parseTimeSpec(StringParsingBuffer<CharacterType>& buffer, Second60Mode second60Mode)
{
    // https://tc39.es/proposal-temporal/#prod-TimeSpec
    // TimeSpec :
    //     TimeHour
    //     TimeHour : TimeMinute
    //     TimeHour TimeMinute
    //     TimeHour : TimeMinute : TimeSecond TimeFraction[opt]
    //     TimeHour TimeMinute TimeSecond TimeFraction[opt]
    //
    //  TimeSecond can be 60. And if it is 60, we interpret it as 59.
    //  https://tc39.es/proposal-temporal/#sec-temporal-parseisodatetime

    if (buffer.lengthRemaining() < 2)
        return std::nullopt;

    unsigned hour = 0;
    ASSERT(buffer.lengthRemaining() >= 2);
    auto firstHourCharacter = *buffer;
    if (firstHourCharacter >= '0' && firstHourCharacter <= '2') {
        buffer.advance();
        auto secondHourCharacter = *buffer;
        if (!isASCIIDigit(secondHourCharacter))
            return std::nullopt;
        hour = (secondHourCharacter - '0') + 10 * (firstHourCharacter - '0');
        if (hour >= 24)
            return std::nullopt;
        buffer.advance();
    } else
        return std::nullopt;

    if (buffer.atEnd())
        return PlainTime(hour, 0, 0, 0, 0, 0);

    bool splitByColon = false;
    if (*buffer == ':') {
        splitByColon = true;
        buffer.advance();
    } else if (!(*buffer >= '0' && *buffer <= '5'))
        return PlainTime(hour, 0, 0, 0, 0, 0);

    unsigned minute = 0;
    if (buffer.lengthRemaining() < 2)
        return std::nullopt;
    auto firstMinuteCharacter = *buffer;
    if (firstMinuteCharacter >= '0' && firstMinuteCharacter <= '5') {
        buffer.advance();
        auto secondMinuteCharacter = *buffer;
        if (!isASCIIDigit(secondMinuteCharacter))
            return std::nullopt;
        minute = (secondMinuteCharacter - '0') + 10 * (firstMinuteCharacter - '0');
        ASSERT(minute < 60);
        buffer.advance();
    } else
        return std::nullopt;

    if (buffer.atEnd())
        return PlainTime(hour, minute, 0, 0, 0, 0);

    if (splitByColon) {
        if (*buffer == ':')
            buffer.advance();
        else
            return PlainTime(hour, minute, 0, 0, 0, 0);
    } else if (!(*buffer >= '0' && (second60Mode == Second60Mode::Accept ? (*buffer <= '6') : (*buffer <= '5'))))
        return PlainTime(hour, minute, 0, 0, 0, 0);

    unsigned second = 0;
    if (buffer.lengthRemaining() < 2)
        return std::nullopt;
    auto firstSecondCharacter = *buffer;
    if (firstSecondCharacter >= '0' && firstSecondCharacter <= '5') {
        buffer.advance();
        auto secondSecondCharacter = *buffer;
        if (!isASCIIDigit(secondSecondCharacter))
            return std::nullopt;
        second = (secondSecondCharacter - '0') + 10 * (firstSecondCharacter - '0');
        ASSERT(second < 60);
        buffer.advance();
    } else if (second60Mode == Second60Mode::Accept && firstSecondCharacter == '6') {
        buffer.advance();
        auto secondSecondCharacter = *buffer;
        if (secondSecondCharacter != '0')
            return std::nullopt;
        second = 59;
        buffer.advance();
    } else
        return std::nullopt;

    if (buffer.atEnd())
        return PlainTime(hour, minute, second, 0, 0, 0);

    if (*buffer != '.' && *buffer != ',')
        return PlainTime(hour, minute, second, 0, 0, 0);
    buffer.advance();

    unsigned digits = 0;
    unsigned maxCount = std::min(buffer.lengthRemaining(), 9u);
    for (; digits < maxCount; ++digits) {
        if (!isASCIIDigit(buffer[digits]))
            break;
    }
    if (!digits)
        return std::nullopt;

    Vector<LChar, 9> padded(9, '0');
    for (unsigned i = 0; i < digits; ++i)
        padded[i] = buffer[i];
    buffer.advanceBy(digits);

    unsigned millisecond = parseDecimalInt32(padded.data(), 3);
    unsigned microsecond = parseDecimalInt32(padded.data() + 3, 3);
    unsigned nanosecond = parseDecimalInt32(padded.data() + 6, 3);

    return PlainTime(hour, minute, second, millisecond, microsecond, nanosecond);
}

template<typename CharacterType>
static std::optional<int64_t> parseTimeZoneNumericUTCOffset(StringParsingBuffer<CharacterType>& buffer)
{
    // TimeZoneNumericUTCOffset :
    //     TimeZoneUTCOffsetSign TimeZoneUTCOffsetHour
    //     TimeZoneUTCOffsetSign TimeZoneUTCOffsetHour : TimeZoneUTCOffsetMinute
    //     TimeZoneUTCOffsetSign TimeZoneUTCOffsetHour TimeZoneUTCOffsetMinute
    //     TimeZoneUTCOffsetSign TimeZoneUTCOffsetHour : TimeZoneUTCOffsetMinute : TimeZoneUTCOffsetSecond TimeZoneUTCOffsetFraction[opt]
    //     TimeZoneUTCOffsetSign TimeZoneUTCOffsetHour TimeZoneUTCOffsetMinute TimeZoneUTCOffsetSecond TimeZoneUTCOffsetFraction[opt]
    //
    //  This is the same to
    //     TimeZoneUTCOffsetSign TimeSpec
    //
    //  Maximum and minimum values are ±23:59:59.999999999 = ±86399999999999ns, which can be represented by int64_t / double's integer part.

    // sign and hour.
    if (buffer.lengthRemaining() < 3)
        return std::nullopt;

    int64_t factor = 1;
    if (*buffer == '+')
        buffer.advance();
    else if (*buffer == '-' || *buffer == minusSign) {
        factor = -1;
        buffer.advance();
    } else
        return std::nullopt;

    auto plainTime = parseTimeSpec(buffer, Second60Mode::Reject);
    if (!plainTime)
        return std::nullopt;

    int64_t hour = plainTime->hour();
    int64_t minute = plainTime->minute();
    int64_t second = plainTime->second();
    int64_t millisecond = plainTime->millisecond();
    int64_t microsecond = plainTime->microsecond();
    int64_t nanosecond = plainTime->nanosecond();

    return (nsPerHour * hour + nsPerMinute * minute + nsPerSecond * second + nsPerMillisecond * millisecond + nsPerMicrosecond * microsecond + nanosecond) * factor;
}

std::optional<int64_t> parseTimeZoneNumericUTCOffset(StringView string)
{
    return readCharactersForParsing(string, [](auto buffer) -> std::optional<int64_t> {
        auto result = parseTimeZoneNumericUTCOffset(buffer);
        if (!buffer.atEnd())
            return std::nullopt;
        return result;
    });
}

template<typename CharacterType>
static bool canBeCalendar(const StringParsingBuffer<CharacterType>& buffer)
{
    // https://tc39.es/proposal-temporal/#prod-Calendar
    // Calendar :
    //     [u-ca= CalendarName]
    return buffer.lengthRemaining() >= 6 && buffer[0] == '[' && buffer[1] == 'u' && buffer[2] == '-' && buffer[3] == 'c' && buffer[4] == 'a' && buffer[5] == '=';
}

template<typename CharacterType>
static bool canBeTimeZone(const StringParsingBuffer<CharacterType>& buffer, CharacterType character)
{
    switch (static_cast<UChar>(character)) {
    // UTCDesignator
    // https://tc39.es/proposal-temporal/#prod-UTCDesignator
    case 'z':
    case 'Z':
    // TimeZoneUTCOffsetSign
    // https://tc39.es/proposal-temporal/#prod-TimeZoneUTCOffsetSign
    case '+':
    case '-':
    case minusSign:
        return true;
    // TimeZoneBracketedAnnotation
    // https://tc39.es/proposal-temporal/#prod-TimeZoneBracketedAnnotation
    case '[': {
        // We should reject calendar extension case.
        // https://tc39.es/proposal-temporal/#prod-Calendar
        // Calendar :
        //     [u-ca= CalendarName]
        if (canBeCalendar(buffer))
            return false;
        return true;
    }
    default:
        return false;
    }
}

template<typename CharacterType>
static std::optional<std::variant<Vector<LChar>, int64_t>> parseTimeZoneBracketedAnnotation(StringParsingBuffer<CharacterType>& buffer)
{
    // https://tc39.es/proposal-temporal/#prod-TimeZoneBracketedAnnotation
    // TimeZoneBracketedAnnotation :
    //     [ TimeZoneBracketedName ]
    //
    // TimeZoneBracketedName :
    //     TimeZoneIANAName
    //     Etc/GMT ASCIISign Hour
    //     TimeZoneUTCOffsetName

    if (buffer.lengthRemaining() < 3)
        return std::nullopt;

    if (*buffer != '[')
        return std::nullopt;
    buffer.advance();

    switch (static_cast<UChar>(*buffer)) {
    case '+':
    case '-':
    case minusSign: {
        // TimeZoneUTCOffsetName is the same to TimeZoneNumericUTCOffset.
        auto offset = parseTimeZoneNumericUTCOffset(buffer);
        if (!offset)
            return std::nullopt;
        if (buffer.atEnd())
            return std::nullopt;
        if (*buffer != ']')
            return std::nullopt;
        buffer.advance();
        return offset.value();
    }
    case 'E': {
        // "Etc/GMT+20" and "]" => length is 11.
        if (buffer.lengthRemaining() >= 11) {
            if (buffer[0] == 'E' && buffer[1] == 't' && buffer[2] == 'c' && buffer[3] == '/' && buffer[4] == 'G' && buffer[5] == 'M' && buffer[6] == 'T') {
                auto signCharacter = buffer[7];
                // Not including minusSign since it is ASCIISign.
                if (signCharacter == '+' || signCharacter == '-') {
                    // Etc/GMT+01 is UTC-01:00. This sign is intentionally inverted.
                    // https://en.wikipedia.org/wiki/Tz_database#Area
                    int64_t factor = signCharacter == '+' ? -1 : 1;
                    int64_t hour = 0;
                    auto firstHourCharacter = buffer[8];
                    if (firstHourCharacter >= '0' && firstHourCharacter <= '2') {
                        auto secondHourCharacter = buffer[9];
                        if (isASCIIDigit(secondHourCharacter)) {
                            hour = (secondHourCharacter - '0') + 10 * (firstHourCharacter - '0');
                            if (hour < 24 && buffer[10] == ']') {
                                buffer.advanceBy(11);
                                return nsPerHour * hour * factor;
                            }
                        }
                    }
                }
            }
        }
        FALLTHROUGH;
    }
    default: {
        // TZLeadingChar :
        //     Alpha
        //     .
        //     _
        //
        // TZChar :
        //     Alpha
        //     .
        //     -
        //     _
        //
        // TimeZoneIANANameComponent :
        //     TZLeadingChar TZChar[opt] TZChar[opt] TZChar[opt] TZChar[opt] TZChar[opt] TZChar[opt] TZChar[opt] TZChar[opt] TZChar[opt] TZChar[opt] TZChar[opt] TZChar[opt] TZChar[opt] but not one of . or ..
        //
        // TimeZoneIANAName :
        //     TimeZoneIANANameComponent
        //     TimeZoneIANAName / TimeZoneIANANameComponent

        unsigned nameLength = 0;
        {
            unsigned index = 0;
            for (; index < buffer.lengthRemaining(); ++index) {
                auto character = buffer[index];
                if (character == ']')
                    break;
                if (!isASCIIAlpha(character) && character != '.' && character != '_' && character != '-' && character != '/')
                    return std::nullopt;
            }
            if (!index)
                return std::nullopt;
            nameLength = index;
        }

        auto isValidComponent = [&](unsigned start, unsigned end) {
            unsigned componentLength = end - start;
            if (!componentLength)
                return false;
            if (componentLength > 14)
                return false;
            if (componentLength == 1 && buffer[start] == '.')
                return false;
            if (componentLength == 2 && buffer[start] == '.' && buffer[start + 1] == '.')
                return false;
            return true;
        };

        unsigned currentNameComponentStartIndex = 0;
        bool isLeadingCharacterInNameComponent = true;
        for (unsigned index = 0; index < nameLength; ++index) {
            auto character = buffer[index];
            if (isLeadingCharacterInNameComponent) {
                if (!(isASCIIAlpha(character) || character == '.' || character == '_'))
                    return std::nullopt;

                currentNameComponentStartIndex = index;
                isLeadingCharacterInNameComponent = false;
                continue;
            }

            if (character == '/') {
                if (!isValidComponent(currentNameComponentStartIndex, index))
                    return std::nullopt;
                isLeadingCharacterInNameComponent = true;
                continue;
            }

            if (!(isASCIIAlpha(character) || character == '.' || character == '_' || character == '-'))
                return std::nullopt;
        }
        if (isLeadingCharacterInNameComponent)
            return std::nullopt;
        if (!isValidComponent(currentNameComponentStartIndex, nameLength))
            return std::nullopt;

        Vector<LChar> result;
        result.reserveInitialCapacity(nameLength);
        for (unsigned index = 0; index < nameLength; ++index)
            result.uncheckedAppend(buffer[index]);
        buffer.advanceBy(nameLength);

        if (buffer.atEnd())
            return std::nullopt;
        if (*buffer != ']')
            return std::nullopt;
        buffer.advance();
        return result;
    }
    }
}

template<typename CharacterType>
static std::optional<TimeZoneRecord> parseTimeZone(StringParsingBuffer<CharacterType>& buffer)
{
    if (buffer.atEnd())
        return std::nullopt;
    switch (static_cast<UChar>(*buffer)) {
    // UTCDesignator
    // https://tc39.es/proposal-temporal/#prod-UTCDesignator
    case 'z':
    case 'Z': {
        buffer.advance();
        if (!buffer.atEnd() && *buffer == '[' && canBeTimeZone(buffer, *buffer)) {
            auto timeZone = parseTimeZoneBracketedAnnotation(buffer);
            if (!timeZone)
                return std::nullopt;
            return TimeZoneRecord { true, std::nullopt, WTFMove(timeZone.value()) };
        }
        return TimeZoneRecord { true, std::nullopt, { } };
    }
    // TimeZoneUTCOffsetSign
    // https://tc39.es/proposal-temporal/#prod-TimeZoneUTCOffsetSign
    case '+':
    case '-':
    case minusSign: {
        auto offset = parseTimeZoneNumericUTCOffset(buffer);
        if (!offset)
            return std::nullopt;
        if (!buffer.atEnd() && *buffer == '[' && canBeTimeZone(buffer, *buffer)) {
            auto timeZone = parseTimeZoneBracketedAnnotation(buffer);
            if (!timeZone)
                return std::nullopt;
            return TimeZoneRecord { false, offset.value(), WTFMove(timeZone.value()) };
        }
        return TimeZoneRecord { false, offset.value(), { } };
    }
    // TimeZoneBracketedAnnotation
    // https://tc39.es/proposal-temporal/#prod-TimeZoneBracketedAnnotation
    case '[': {
        auto timeZone = parseTimeZoneBracketedAnnotation(buffer);
        if (!timeZone)
            return std::nullopt;
        return TimeZoneRecord { false, std::nullopt, WTFMove(timeZone.value()) };
    }
    default:
        return std::nullopt;
    }
}

template<typename CharacterType>
static std::optional<CalendarRecord> parseCalendar(StringParsingBuffer<CharacterType>& buffer)
{
    // https://tc39.es/proposal-temporal/#prod-TimeZoneBracketedAnnotation
    // Calendar :
    //     [u-ca= CalendarName ]
    //
    // CalendarName :
    //     CalendarNameComponent
    //     CalendarNameComponent - CalendarName
    //
    // CalendarNameComponent :
    //     CalChar CalChar CalChar CalChar[opt] CalChar[opt] CalChar[opt] CalChar[opt] CalChar[opt]
    //
    // CalChar :
    //     Alpha
    //     Digit

    if (!canBeCalendar(buffer))
        return std::nullopt;
    buffer.advanceBy(6);

    if (buffer.atEnd())
        return std::nullopt;

    unsigned nameLength = 0;
    {
        unsigned index = 0;
        for (; index < buffer.lengthRemaining(); ++index) {
            auto character = buffer[index];
            if (character == ']')
                break;
            if (!isASCIIAlpha(character) && !isASCIIDigit(character) && character != '-')
                return std::nullopt;
        }
        if (!index)
            return std::nullopt;
        nameLength = index;
    }

    auto isValidComponent = [&](unsigned start, unsigned end) {
        unsigned componentLength = end - start;
        if (componentLength < minCalendarLength)
            return false;
        if (componentLength > maxCalendarLength)
            return false;
        return true;
    };

    unsigned currentNameComponentStartIndex = 0;
    bool isLeadingCharacterInNameComponent = true;
    for (unsigned index = 0; index < nameLength; ++index) {
        auto character = buffer[index];
        if (isLeadingCharacterInNameComponent) {
            if (!(isASCIIAlpha(character) || isASCIIDigit(character)))
                return std::nullopt;

            currentNameComponentStartIndex = index;
            isLeadingCharacterInNameComponent = false;
            continue;
        }

        if (character == '-') {
            if (!isValidComponent(currentNameComponentStartIndex, index))
                return std::nullopt;
            isLeadingCharacterInNameComponent = true;
            continue;
        }

        if (!(isASCIIAlpha(character) || isASCIIDigit(character)))
            return std::nullopt;
    }
    if (isLeadingCharacterInNameComponent)
        return std::nullopt;
    if (!isValidComponent(currentNameComponentStartIndex, nameLength))
        return std::nullopt;

    Vector<LChar, maxCalendarLength> result;
    result.reserveInitialCapacity(nameLength);
    for (unsigned index = 0; index < nameLength; ++index)
        result.uncheckedAppend(buffer[index]);
    buffer.advanceBy(nameLength);

    if (buffer.atEnd())
        return std::nullopt;
    if (*buffer != ']')
        return std::nullopt;
    buffer.advance();
    return CalendarRecord { WTFMove(result) };
}

template<typename CharacterType>
static std::optional<std::tuple<PlainTime, std::optional<TimeZoneRecord>>> parseTime(StringParsingBuffer<CharacterType>& buffer)
{
    // https://tc39.es/proposal-temporal/#prod-Time
    // Time :
    //     TimeSpec TimeZone[opt]
    auto plainTime = parseTimeSpec(buffer, Second60Mode::Accept);
    if (!plainTime)
        return std::nullopt;
    if (buffer.atEnd())
        return std::tuple { WTFMove(plainTime.value()), std::nullopt };
    if (canBeTimeZone(buffer, *buffer)) {
        auto timeZone = parseTimeZone(buffer);
        if (!timeZone)
            return std::nullopt;
        return std::tuple { WTFMove(plainTime.value()), WTFMove(timeZone) };
    }
    return std::tuple { WTFMove(plainTime.value()), std::nullopt };
}

// https://tc39.es/proposal-temporal/#sec-temporal-isodaysinmonth
unsigned daysInMonth(int32_t year, unsigned month)
{
    switch (month) {
    case 1:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
    case 12:
        return 31;
    case 4:
    case 6:
    case 9:
    case 11:
        return 30;
    case 2:
        if (isLeapYear(year))
            return 29;
        return 28;
    }
    return 0;
}

template<typename CharacterType>
static std::optional<PlainDate> parseDate(StringParsingBuffer<CharacterType>& buffer)
{
    // https://tc39.es/proposal-temporal/#prod-Date
    // Date :
    //     DateYear - DateMonth - DateDay
    //     DateYear DateMonth DateDay
    //
    // DateYear :
    //     DateFourDigitYear
    //     DateExtendedYear
    //
    // DateFourDigitYear :
    //     Digit Digit Digit Digit
    //
    // DateExtendedYear :
    //     Sign Digit Digit Digit Digit Digit Digit
    //
    // DateMonth :
    //     0 NonzeroDigit
    //     10
    //     11
    //     12
    //
    // DateDay :
    //     0 NonzeroDigit
    //     1 Digit
    //     2 Digit
    //     30
    //     31

    if (buffer.atEnd())
        return std::nullopt;

    bool sixDigitsYear = false;
    int yearFactor = 1;
    if (*buffer == '+') {
        buffer.advance();
        sixDigitsYear = true;
    } else if (*buffer == '-' || *buffer == minusSign) {
        yearFactor = -1;
        buffer.advance();
        sixDigitsYear = true;
    } else if (!isASCIIDigit(*buffer))
        return std::nullopt;

    int32_t year = 0;
    if (sixDigitsYear) {
        if (buffer.lengthRemaining() < 6)
            return std::nullopt;
        for (unsigned index = 0; index < 6; ++index) {
            if (!isASCIIDigit(buffer[index]))
                return std::nullopt;
        }
        year = parseDecimalInt32(buffer.position(), 6) * yearFactor;
        buffer.advanceBy(6);
    } else {
        if (buffer.lengthRemaining() < 4)
            return std::nullopt;
        for (unsigned index = 0; index < 4; ++index) {
            if (!isASCIIDigit(buffer[index]))
                return std::nullopt;
        }
        year = parseDecimalInt32(buffer.position(), 4);
        buffer.advanceBy(4);
    }

    if (buffer.atEnd())
        return std::nullopt;

    bool splitByHyphen = false;
    if (*buffer == '-') {
        splitByHyphen = true;
        buffer.advance();
        if (buffer.lengthRemaining() < 5)
            return std::nullopt;
    } else {
        if (buffer.lengthRemaining() < 4)
            return std::nullopt;
    }
    // We ensured that buffer has enough length for month and day. We do not need to check length.

    unsigned month = 0;
    auto firstMonthCharacter = *buffer;
    if (firstMonthCharacter == '0' || firstMonthCharacter == '1') {
        buffer.advance();
        auto secondMonthCharacter = *buffer;
        if (!isASCIIDigit(secondMonthCharacter))
            return std::nullopt;
        month = (secondMonthCharacter - '0') + 10 * (firstMonthCharacter - '0');
        if (!month || month > 12)
            return std::nullopt;
        buffer.advance();
    } else
        return std::nullopt;

    if (splitByHyphen) {
        if (*buffer == '-')
            buffer.advance();
        else
            return std::nullopt;
    }

    unsigned day = 0;
    auto firstDayCharacter = *buffer;
    if (firstDayCharacter >= '0' && firstDayCharacter <= '3') {
        buffer.advance();
        auto secondDayCharacter = *buffer;
        if (!isASCIIDigit(secondDayCharacter))
            return std::nullopt;
        day = (secondDayCharacter - '0') + 10 * (firstDayCharacter - '0');
        if (!day || day > daysInMonth(year, month))
            return std::nullopt;
        buffer.advance();
    } else
        return std::nullopt;

    return PlainDate(year, month, day);
}

template<typename CharacterType>
static std::optional<std::tuple<PlainDate, std::optional<PlainTime>, std::optional<TimeZoneRecord>>> parseDateTime(StringParsingBuffer<CharacterType>& buffer)
{
    // https://tc39.es/proposal-temporal/#prod-DateTime
    // DateTime :
    //     Date TimeSpecSeparator[opt] TimeZone[opt]
    //
    // TimeSpecSeparator :
    //     DateTimeSeparator TimeSpec
    auto plainDate = parseDate(buffer);
    if (!plainDate)
        return std::nullopt;
    if (buffer.atEnd())
        return std::tuple { WTFMove(plainDate.value()), std::nullopt, std::nullopt };

    if (*buffer == ' ' || *buffer == 'T' || *buffer == 't') {
        buffer.advance();
        auto plainTimeAndTimeZone = parseTime(buffer);
        if (!plainTimeAndTimeZone)
            return std::nullopt;
        auto [plainTime, timeZone] = WTFMove(plainTimeAndTimeZone.value());
        return std::tuple { WTFMove(plainDate.value()), WTFMove(plainTime), WTFMove(timeZone) };
    }

    if (canBeTimeZone(buffer, *buffer)) {
        auto timeZone = parseTimeZone(buffer);
        if (!timeZone)
            return std::nullopt;
        return std::tuple { WTFMove(plainDate.value()), std::nullopt, WTFMove(timeZone) };
    }

    return std::tuple { WTFMove(plainDate.value()), std::nullopt, std::nullopt };
}

template<typename CharacterType>
static std::optional<std::tuple<PlainTime, std::optional<TimeZoneRecord>, std::optional<CalendarRecord>>> parseCalendarTime(StringParsingBuffer<CharacterType>& buffer)
{
    // https://tc39.es/proposal-temporal/#prod-CalendarTime
    // CalendarTime :
    //     TimeDesignator TimeSpec TimeZone[opt] Calendar[opt]
    //     TimeSpec TimeZone[opt] Calendar
    //     TimeSpecWithOptionalTimeZoneNotAmbiguous

    if (buffer.atEnd())
        return std::nullopt;

    if (*buffer == 'T' || *buffer == 't')
        buffer.advance();

    auto plainTime = parseTimeSpec(buffer, Second60Mode::Accept);
    if (!plainTime)
        return std::nullopt;
    if (buffer.atEnd())
        return std::tuple { WTFMove(plainTime.value()), std::nullopt, std::nullopt };

    std::optional<TimeZoneRecord> timeZoneOptional;
    if (canBeTimeZone(buffer, *buffer)) {
        auto timeZone = parseTimeZone(buffer);
        if (!timeZone)
            return std::nullopt;
        timeZoneOptional = WTFMove(timeZone);
    }

    if (buffer.atEnd())
        return std::tuple { WTFMove(plainTime.value()), WTFMove(timeZoneOptional), std::nullopt };

    std::optional<CalendarRecord> calendarOptional;
    if (canBeCalendar(buffer)) {
        auto calendar = parseCalendar(buffer);
        if (!calendar)
            return std::nullopt;
        calendarOptional = WTFMove(calendar);
    }

    return std::tuple { WTFMove(plainTime.value()), WTFMove(timeZoneOptional), WTFMove(calendarOptional) };
}

template<typename CharacterType>
static std::optional<std::tuple<PlainDate, std::optional<PlainTime>, std::optional<TimeZoneRecord>, std::optional<CalendarRecord>>> parseCalendarDateTime(StringParsingBuffer<CharacterType>& buffer)
{
    // https://tc39.es/proposal-temporal/#prod-DateTime
    // CalendarDateTime :
    //     DateTime CalendarName[opt]
    //
    auto dateTime = parseDateTime(buffer);
    if (!dateTime)
        return std::nullopt;

    auto [plainDate, plainTimeOptional, timeZoneOptional] = WTFMove(dateTime.value());

    if (!buffer.atEnd() && canBeCalendar(buffer)) {
        auto calendar = parseCalendar(buffer);
        if (!calendar)
            return std::nullopt;
        return std::tuple { WTFMove(plainDate), WTFMove(plainTimeOptional), WTFMove(timeZoneOptional), WTFMove(calendar) };
    }

    return std::tuple { WTFMove(plainDate), WTFMove(plainTimeOptional), WTFMove(timeZoneOptional), std::nullopt };
}

std::optional<std::tuple<PlainTime, std::optional<TimeZoneRecord>>> parseTime(StringView string)
{
    return readCharactersForParsing(string, [](auto buffer) -> std::optional<std::tuple<PlainTime, std::optional<TimeZoneRecord>>> {
        auto result = parseTime(buffer);
        if (!buffer.atEnd())
            return std::nullopt;
        return result;
    });
}

std::optional<std::tuple<PlainTime, std::optional<TimeZoneRecord>, std::optional<CalendarRecord>>> parseCalendarTime(StringView string)
{
    return readCharactersForParsing(string, [](auto buffer) -> std::optional<std::tuple<PlainTime, std::optional<TimeZoneRecord>, std::optional<CalendarRecord>>> {
        auto result = parseCalendarTime(buffer);
        if (!buffer.atEnd())
            return std::nullopt;
        return result;
    });
}

std::optional<std::tuple<PlainDate, std::optional<PlainTime>, std::optional<TimeZoneRecord>>> parseDateTime(StringView string)
{
    return readCharactersForParsing(string, [](auto buffer) -> std::optional<std::tuple<PlainDate, std::optional<PlainTime>, std::optional<TimeZoneRecord>>> {
        auto result = parseDateTime(buffer);
        if (!buffer.atEnd())
            return std::nullopt;
        return result;
    });
}

std::optional<std::tuple<PlainDate, std::optional<PlainTime>, std::optional<TimeZoneRecord>, std::optional<CalendarRecord>>> parseCalendarDateTime(StringView string)
{
    return readCharactersForParsing(string, [](auto buffer) -> std::optional<std::tuple<PlainDate, std::optional<PlainTime>, std::optional<TimeZoneRecord>, std::optional<CalendarRecord>>> {
        auto result = parseCalendarDateTime(buffer);
        if (!buffer.atEnd())
            return std::nullopt;
        return result;
    });
}

std::optional<ExactTime> parseInstant(StringView string)
{
    // https://tc39.es/proposal-temporal/#prod-TemporalInstantString
    // TemporalInstantString :
    //     Date TimeZoneOffsetRequired
    //     Date DateTimeSeparator TimeSpec TimeZoneOffsetRequired

    // https://tc39.es/proposal-temporal/#prod-TimeZoneOffsetRequired
    // TimeZoneOffsetRequired :
    //     TimeZoneUTCOffset TimeZoneBracketedAnnotation_opt

    return readCharactersForParsing(string, [](auto buffer) -> std::optional<ExactTime> {
        auto datetime = parseCalendarDateTime(buffer);
        if (!datetime)
            return std::nullopt;
        auto [plainDate, plainTimeOptional, timeZoneOptional, calendarOptional] = WTFMove(datetime.value());
        if (!timeZoneOptional || (!timeZoneOptional->m_z && !timeZoneOptional->m_offset))
            return std::nullopt;
        if (!buffer.atEnd())
            return std::nullopt;

        PlainTime plainTime = plainTimeOptional.value_or(PlainTime());

        int64_t offset = timeZoneOptional->m_z ? 0 : *timeZoneOptional->m_offset;
        return { ExactTime::fromISOPartsAndOffset(plainDate.year(), plainDate.month(), plainDate.day(), plainTime.hour(), plainTime.minute(), plainTime.second(), plainTime.millisecond(), plainTime.microsecond(), plainTime.nanosecond(), offset) };
    });
}

// https://tc39.es/proposal-temporal/#sec-temporal-formattimezoneoffsetstring
String formatTimeZoneOffsetString(int64_t offset)
{
    bool negative = false;
    if (offset < 0) {
        negative = true;
        offset = -offset; // This is OK since offset range is much narrower than [INT64_MIN, INT64_MAX] range.
    }
    int64_t nanoseconds = offset % nsPerSecond;
    int64_t seconds = (offset / nsPerSecond) % 60;
    int64_t minutes = (offset / nsPerMinute) % 60;
    int64_t hours = offset / nsPerHour;

    if (nanoseconds) {
        // Since nsPerSecond is 1000000000, stringified nanoseconds takes at most 9 characters (999999999).
        auto fraction = numberToStringUnsigned<Vector<LChar, 9>>(nanoseconds);
        unsigned paddingLength = 9 - fraction.size();
        unsigned index = fraction.size();
        std::optional<unsigned> validLength;
        while (index--) {
            if (fraction[index] != '0') {
                validLength = index + 1;
                break;
            }
        }
        if (validLength)
            fraction.resize(validLength.value());
        else
            fraction.clear();
        return makeString(negative ? '-' : '+', pad('0', 2, hours), ':', pad('0', 2, minutes), ':', pad('0', 2, seconds), '.', pad('0', paddingLength, emptyString()), fraction);
    }
    if (seconds)
        return makeString(negative ? '-' : '+', pad('0', 2, hours), ':', pad('0', 2, minutes), ':', pad('0', 2, seconds));
    return makeString(negative ? '-' : '+', pad('0', 2, hours), ':', pad('0', 2, minutes));
}

String temporalTimeToString(PlainTime plainTime, std::tuple<Precision, unsigned> precision)
{
    auto [precisionType, precisionValue] = precision;
    ASSERT(precisionType == Precision::Auto || precisionValue < 10);
    if (precisionType == Precision::Minute)
        return makeString(pad('0', 2, plainTime.hour()), ':', pad('0', 2, plainTime.minute()));

    int64_t milliseconds = plainTime.millisecond();
    int64_t microseconds = plainTime.microsecond();
    int64_t nanoseconds = plainTime.nanosecond();
    int64_t fractionNanoseconds = milliseconds * nsPerMillisecond + microseconds * nsPerMicrosecond + nanoseconds;
    if (precisionType == Precision::Auto) {
        if (!fractionNanoseconds)
            return makeString(pad('0', 2, plainTime.hour()), ':', pad('0', 2, plainTime.minute()), ':', pad('0', 2, plainTime.second()));
        auto fraction = numberToStringUnsigned<Vector<LChar, 9>>(fractionNanoseconds);
        unsigned paddingLength = 9 - fraction.size();
        unsigned index = fraction.size();
        std::optional<unsigned> validLength;
        while (index--) {
            if (fraction[index] != '0') {
                validLength = index + 1;
                break;
            }
        }
        if (validLength)
            fraction.resize(validLength.value());
        else
            fraction.clear();
        return makeString(pad('0', 2, plainTime.hour()), ':', pad('0', 2, plainTime.minute()), ':', pad('0', 2, plainTime.second()), '.', pad('0', paddingLength, emptyString()), fraction);
    }
    if (!precisionValue)
        return makeString(pad('0', 2, plainTime.hour()), ':', pad('0', 2, plainTime.minute()), ':', pad('0', 2, plainTime.second()));
    auto fraction = numberToStringUnsigned<Vector<LChar, 9>>(fractionNanoseconds);
    unsigned paddingLength = 9 - fraction.size();
    paddingLength = std::min(paddingLength, precisionValue);
    precisionValue -= paddingLength;
    fraction.resize(precisionValue);
    return makeString(pad('0', 2, plainTime.hour()), ':', pad('0', 2, plainTime.minute()), ':', pad('0', 2, plainTime.second()), '.', pad('0', paddingLength, emptyString()), fraction);
}

String temporalDateToString(PlainDate plainDate)
{
    return makeString(pad('0', 4, plainDate.year()), '-', pad('0', 2, plainDate.month()), '-', pad('0', 2, plainDate.day()));
}

// IsValidDuration ( years, months, weeks, days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds )
// https://tc39.es/proposal-temporal/#sec-temporal-isvalidduration
bool isValidDuration(const Duration& duration)
{
    int sign = 0;
    for (auto value : duration) {
        if (!std::isfinite(value) || (value < 0 && sign > 0) || (value > 0 && sign < 0))
            return false;

        if (!sign && value)
            sign = value > 0 ? 1 : -1;
    }

    return true;
}

ExactTime ExactTime::fromISOPartsAndOffset(int32_t year, uint8_t month, uint8_t day, unsigned hour, unsigned minute, unsigned second, unsigned millisecond, unsigned microsecond, unsigned nanosecond, int64_t offset)
{
    ASSERT(month >= 1 && month <= 12);
    ASSERT(day >= 1 && day <= 31);
    ASSERT(hour <= 23);
    ASSERT(minute <= 59);
    ASSERT(second <= 59);
    ASSERT(millisecond <= 999);
    ASSERT(microsecond <= 999);
    ASSERT(nanosecond <= 999);

    Int128 dateDays = static_cast<Int128>(dateToDaysFrom1970(year, month - 1, day));
    Int128 utcNanoseconds = dateDays * nsPerDay + hour * nsPerHour + minute * nsPerMinute + second * nsPerSecond + millisecond * nsPerMillisecond + microsecond * nsPerMicrosecond + nanosecond;
    return ExactTime { utcNanoseconds - offset };
}

using CheckedInt128 = Checked<Int128, RecordOverflow>;

static CheckedInt128 checkedCastDoubleToInt128(double n)
{
    // Based on __fixdfti() and __fixunsdfti() from compiler_rt:
    // https://github.com/llvm/llvm-project/blob/f3671de5500ff1f8210419226a9603a7d83b1a31/compiler-rt/lib/builtins/fp_fixint_impl.inc
    // https://github.com/llvm/llvm-project/blob/f3671de5500ff1f8210419226a9603a7d83b1a31/compiler-rt/lib/builtins/fp_fixuint_impl.inc

    static constexpr int significandBits = std::numeric_limits<double>::digits - 1;
    static constexpr int exponentBits = std::numeric_limits<uint64_t>::digits - std::numeric_limits<double>::digits;
    static constexpr int exponentBias = std::numeric_limits<double>::max_exponent - 1;
    static constexpr uint64_t implicitBit = uint64_t { 1 } << significandBits;
    static constexpr uint64_t significandMask = implicitBit - uint64_t { 1 };
    static constexpr uint64_t signMask = uint64_t { 1 } << (significandBits + exponentBits);
    static constexpr uint64_t absMask = signMask - uint64_t { 1 };

    // Break n into sign, exponent, significand parts.
    const uint64_t bits = *reinterpret_cast<uint64_t*>(&n);
    const uint64_t nAbs = bits & absMask;
    const int sign = bits & signMask ? -1 : 1;
    const int exponent = (nAbs >> significandBits) - exponentBias;
    const uint64_t significand = (nAbs & significandMask) | implicitBit;

    // If exponent is negative, the result is zero.
    if (exponent < 0)
        return { 0 };

    // If the value is too large for the integer type, overflow.
    if (exponent >= 128)
        return { WTF::ResultOverflowed };

    // If 0 <= exponent < significandBits, right shift to get the result.
    // Otherwise, shift left.
    Int128 result { significand };
    if (exponent < significandBits)
        result >>= significandBits - exponent;
    else
        result <<= exponent - significandBits;
    result *= sign;
    return { result };
}

std::optional<ExactTime> ExactTime::add(Duration duration) const
{
    ASSERT(!duration.years());
    ASSERT(!duration.months());
    ASSERT(!duration.weeks());
    ASSERT(!duration.days());

    CheckedInt128 resultNs { m_epochNanoseconds };

    // The duration's hours, minutes, seconds, and milliseconds should be
    // able to be cast into a 64-bit int. 2*1e8 24-hour days is the maximum
    // time span for exact time, so if we already know that the duration exceeds
    // that, then we can bail out.

    CheckedInt128 hours = checkedCastDoubleToInt128(duration.hours());
    resultNs += hours * ExactTime::nsPerHour;
    CheckedInt128 minutes = checkedCastDoubleToInt128(duration.minutes());
    resultNs += minutes * ExactTime::nsPerMinute;
    CheckedInt128 seconds = checkedCastDoubleToInt128(duration.seconds());
    resultNs += seconds * ExactTime::nsPerSecond;
    CheckedInt128 milliseconds = checkedCastDoubleToInt128(duration.milliseconds());
    resultNs += milliseconds * ExactTime::nsPerMillisecond;
    CheckedInt128 microseconds = checkedCastDoubleToInt128(duration.microseconds());
    resultNs += microseconds * ExactTime::nsPerMicrosecond;
    resultNs += checkedCastDoubleToInt128(duration.nanoseconds());
    if (resultNs.hasOverflowed())
        return std::nullopt;

    ExactTime result { resultNs.value() };
    if (!result.isValid())
        return std::nullopt;
    return result;
}

Int128 ExactTime::round(Int128 quantity, unsigned increment, TemporalUnit unit, RoundingMode roundingMode)
{
    Int128 incrementNs { increment };
    switch (unit) {
    case TemporalUnit::Hour: incrementNs *= ExactTime::nsPerHour; break;
    case TemporalUnit::Minute: incrementNs *= ExactTime::nsPerMinute; break;
    case TemporalUnit::Second: incrementNs *= ExactTime::nsPerSecond; break;
    case TemporalUnit::Millisecond: incrementNs *= ExactTime::nsPerMillisecond; break;
    case TemporalUnit::Microsecond: incrementNs *= ExactTime::nsPerMicrosecond; break;
    case TemporalUnit::Nanosecond: break;
    default:
        ASSERT_NOT_REACHED();
    }
    return roundNumberToIncrement(quantity, incrementNs, roundingMode);
}

// DifferenceInstant ( ns1, ns2, roundingIncrement, smallestUnit, roundingMode )
// https://tc39.es/proposal-temporal/#sec-temporal-differenceinstant
Int128 ExactTime::difference(ExactTime other, unsigned increment, TemporalUnit unit, RoundingMode roundingMode) const
{
    Int128 diff = other.m_epochNanoseconds - m_epochNanoseconds;
    return round(diff, increment, unit, roundingMode);
}

ExactTime ExactTime::round(unsigned increment, TemporalUnit unit, RoundingMode roundingMode) const
{
    return ExactTime { round(m_epochNanoseconds, increment, unit, roundingMode) };
}

ExactTime ExactTime::now()
{
    return ExactTime { WTF::currentTimeInNanoseconds() };
}

// https://tc39.es/proposal-temporal/#sec-temporal-isodatetimewithinlimits
bool isDateTimeWithinLimits(int32_t year, uint8_t month, uint8_t day, unsigned hour, unsigned minute, unsigned second, unsigned millisecond, unsigned microsecond, unsigned nanosecond)
{
    Int128 nanoseconds = ExactTime::fromISOPartsAndOffset(year, month, day, hour, minute, second, millisecond, microsecond, nanosecond, 0).epochNanoseconds();
    if (nanoseconds <= (ExactTime::minValue - ExactTime::nsPerDay))
        return false;
    if (nanoseconds >= (ExactTime::maxValue + ExactTime::nsPerDay))
        return false;
    return true;
}

} // namespace ISO8601
} // namespace JSC
