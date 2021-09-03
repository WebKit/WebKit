/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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

#include "ParseInt.h"
#include <wtf/text/StringParsingBuffer.h>

namespace JSC {
namespace ISO8601 {

static constexpr int64_t nsPerHour = 1000LL * 1000 * 1000 * 60 * 60;
static constexpr int64_t nsPerMinute = 1000LL * 1000 * 1000 * 60;
static constexpr int64_t nsPerSecond = 1000LL * 1000 * 1000;
static constexpr int64_t nsPerMillisecond = 1000LL * 1000;
static constexpr int64_t nsPerMicrosecond = 1000LL;

template<typename CharType>
int32_t parseDecimalInt32(const CharType* characters, unsigned length)
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
    else if (*buffer == '-' || *buffer == 0x2212) {
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

template<typename CharacterType>
static std::optional<int64_t> parseTimeZoneNumericUTCOffset(StringParsingBuffer<CharacterType>& buffer)
{
    // TimeZoneNumericUTCOffset :
    //     TimeZoneUTCOffsetSign TimeZoneUTCOffsetHour
    //     TimeZoneUTCOffsetSign TimeZoneUTCOffsetHour : TimeZoneUTCOffsetMinute
    //     TimeZoneUTCOffsetSign TimeZoneUTCOffsetHour TimeZoneUTCOffsetMinute
    //     TimeZoneUTCOffsetSign TimeZoneUTCOffsetHour : TimeZoneUTCOffsetMinute : TimeZoneUTCOffsetSecond TimeZoneUTCOffsetFraction[opt]
    //     TimeZoneUTCOffsetSign TimeZoneUTCOffsetHour TimeZoneUTCOffsetMinute TimeZoneUTCOffsetSecond[opt]
    //
    //  Maximum and minimum values are ±23:59:59.999999999 = ±86399999999999ns, which can be represented by int64_t / double's integer part.

    // sign and hour.
    if (buffer.lengthRemaining() < 3)
        return std::nullopt;

    int64_t factor = 1;
    if (*buffer == '+')
        buffer.advance();
    else if (*buffer == '-' || *buffer == 0x2212) {
        factor = -1;
        buffer.advance();
    } else
        return std::nullopt;

    int64_t hour = 0;
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
        return (nsPerHour * hour) * factor;

    bool splitByColon = false;
    if (*buffer == ':') {
        buffer.advance();
        splitByColon = true;
    }

    int64_t minute = 0;
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
        return (nsPerHour * hour + nsPerMinute * minute) * factor;

    if (splitByColon) {
        if (*buffer == ':')
            buffer.advance();
        else
            return std::nullopt;
    }

    int64_t second = 0;
    if (buffer.lengthRemaining() < 2)
        return std::nullopt;
    auto firstSecondCharacter = *buffer;
    if (firstSecondCharacter >= '0' && firstSecondCharacter <= '5') {
        buffer.advance();
        auto secondSecondCharacter = *buffer;
        if (!isASCIIDigit(secondSecondCharacter))
            return std::nullopt;
        second = (secondSecondCharacter - '0') + 10 * (firstSecondCharacter - '0');
        ASSERT(minute < 60);
        buffer.advance();
    } else
        return std::nullopt;

    if (buffer.atEnd())
        return (nsPerHour * hour + nsPerMinute * minute + nsPerSecond * second) * factor;

    if (*buffer != '.' && *buffer != ',')
        return std::nullopt;
    buffer.advance();

    unsigned digits = 0;
    while (digits < buffer.lengthRemaining() && isASCIIDigit(buffer[digits]))
        digits++;
    if (!digits || digits > 9)
        return std::nullopt;

    Vector<LChar, 9> padded(9, '0');
    for (unsigned i = 0; i < digits; ++i)
        padded[i] = buffer[i];
    buffer.advanceBy(digits);
    if (!buffer.atEnd())
        return std::nullopt;

    int64_t millisecond = parseDecimalInt32(padded.data(), 3);
    int64_t microsecond = parseDecimalInt32(padded.data() + 3, 3);
    int64_t nanosecond = parseDecimalInt32(padded.data() + 6, 3);

    return (nsPerHour * hour + nsPerMinute * minute + nsPerSecond * second + nsPerMillisecond * millisecond + nsPerMicrosecond * microsecond + nanosecond) * factor;
}

std::optional<int64_t> parseTimeZoneNumericUTCOffset(StringView string)
{
    return readCharactersForParsing(string, [](auto buffer) -> std::optional<int64_t> {
        return parseTimeZoneNumericUTCOffset(buffer);
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
        auto fraction = WTF::numberToStringUnsigned<Vector<LChar, 9>>(nanoseconds);
        unsigned index = 0;
        unsigned paddingLength = 9 - fraction.size();
        for (; index < fraction.size(); ++index) {
            if (fraction[index] == '0')
                break;
        }
        fraction.resize(index);
        return makeString(negative ? '-' : '+', pad('0', 2, hours), ':', pad('0', 2, minutes), ':', pad('0', 2, seconds), '.', pad('0', paddingLength, emptyString()), fraction);
    }
    if (seconds)
        return makeString(negative ? '-' : '+', pad('0', 2, hours), ':', pad('0', 2, minutes), ':', pad('0', 2, seconds));
    return makeString(negative ? '-' : '+', pad('0', 2, hours), ':', pad('0', 2, minutes));
}

} // namespace ISO8601
} // namespace JSC
