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
        duration.setMilliseconds(factor * parseInt({ padded.data(), 3 }, 10));
        duration.setMicroseconds(factor * parseInt({ &padded.data()[3], 3 }, 10));
        duration.setNanoseconds(factor * parseInt({ &padded.data()[6], 3 }, 10));
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

} // namespace ISO8601
} // namespace JSC
