/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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
#include "ISO8601.h"

#include "FractionToDouble.h"
#include "IntlObject.h"
#include "ParseInt.h"
#include "TemporalObject.h"
#include <limits>
#include <wtf/CheckedArithmetic.h>
#include <wtf/DateMath.h>
#include <wtf/WallTime.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringParsingBuffer.h>
#include <wtf/unicode/CharacterNames.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

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
static int32_t parseDecimalInt32(std::span<const CharType> characters)
{
    int32_t result = 0;
    for (auto character : characters) {
        ASSERT(isASCIIDigit(character));
        result = (result * 10) + character - '0';
    }
    return result;
}

// DurationHandleFractions ( fHours, minutes, fMinutes, seconds, fSeconds, milliseconds, fMilliseconds, microseconds, fMicroseconds, nanoseconds, fNanoseconds )
// https://tc39.es/proposal-temporal/#sec-temporal-durationhandlefractions
static void handleFraction(Duration& duration, int factor, StringView fractionString, TemporalUnit fractionType)
{
    auto fractionLength = fractionString.length();
    ASSERT(fractionLength && fractionLength <= 9 && fractionString.containsOnlyASCII());
    ASSERT(fractionType == TemporalUnit::Hour || fractionType == TemporalUnit::Minute || fractionType == TemporalUnit::Second);

    Vector<Latin1Character, 9> padded(9, '0');
    for (unsigned i = 0; i < fractionLength; i++)
        padded[i] = fractionString[i];

    int64_t fraction = static_cast<int64_t>(factor) * parseDecimalInt32(padded.span());
    if (!fraction)
        return;

    static constexpr int64_t divisor = 1'000'000'000LL;
    if (fractionType == TemporalUnit::Hour) {
        fraction *= 60;
        duration.setMinutes(fraction / divisor);
        fraction %= divisor;
        if (!fraction)
            return;
    }

    if (fractionType != TemporalUnit::Second) {
        fraction *= 60;
        duration.setSeconds(fraction / divisor);
        fraction %= divisor;
        if (!fraction)
            return;
    }

    duration.setMilliseconds(fraction / nsPerMillisecond);
    duration.setMicroseconds(fraction % nsPerMillisecond / nsPerMicrosecond);
    duration.setNanoseconds(fraction % nsPerMicrosecond);
}

// ParseTemporalDurationString ( isoString )
// https://tc39.es/proposal-temporal/#sec-temporal-parsetemporaldurationstring
template<typename CharacterType>
static std::optional<Duration> parseDuration(StringParsingBuffer<CharacterType>& buffer)
{
    // ISO 8601 duration strings are like "-P1Y2M3W4DT5H6M7.123456789S". Notes:
    // - case insensitive
    // - sign: + -
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
    else if (*buffer == '-') {
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

        double integer = factor * parseInt(buffer.span().first(digits), 10);
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

        double integer = factor * parseInt(buffer.span().first(digits), 10);
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

            fractionalPart = buffer.span().first(digits);
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
static std::optional<PlainTime> parseTimeSpec(StringParsingBuffer<CharacterType>& buffer, Second60Mode second60Mode, bool parseSubMinutePrecision = true)
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

    ASSERT(buffer.lengthRemaining() >= 2);
    auto firstHourCharacter = *buffer;
    if (!(firstHourCharacter >= '0' && firstHourCharacter <= '2'))
        return std::nullopt;

    buffer.advance();
    auto secondHourCharacter = *buffer;
    if (!isASCIIDigit(secondHourCharacter))
        return std::nullopt;
    unsigned hour = (secondHourCharacter - '0') + 10 * (firstHourCharacter - '0');
    if (hour >= 24)
        return std::nullopt;
    buffer.advance();

    if (buffer.atEnd())
        return PlainTime(hour, 0, 0, 0, 0, 0);

    bool splitByColon = false;
    if (*buffer == ':') {
        splitByColon = true;
        buffer.advance();
    } else if (!(*buffer >= '0' && *buffer <= '5'))
        return PlainTime(hour, 0, 0, 0, 0, 0);

    if (buffer.lengthRemaining() < 2)
        return std::nullopt;
    auto firstMinuteCharacter = *buffer;
    if (!(firstMinuteCharacter >= '0' && firstMinuteCharacter <= '5'))
        return std::nullopt;

    buffer.advance();
    auto secondMinuteCharacter = *buffer;
    if (!isASCIIDigit(secondMinuteCharacter))
        return std::nullopt;
    unsigned minute = (secondMinuteCharacter - '0') + 10 * (firstMinuteCharacter - '0');
    ASSERT(minute < 60);
    buffer.advance();

    if (buffer.atEnd())
        return PlainTime(hour, minute, 0, 0, 0, 0);

    if (splitByColon) {
        if (*buffer == ':')
            buffer.advance();
        else
            return PlainTime(hour, minute, 0, 0, 0, 0);
    } else if (!(*buffer >= '0' && (second60Mode == Second60Mode::Accept ? (*buffer <= '6') : (*buffer <= '5'))))
        return PlainTime(hour, minute, 0, 0, 0, 0);

    if (!parseSubMinutePrecision)
        return std::nullopt;

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

    size_t digits = 0;
    size_t maxCount = std::min<size_t>(buffer.lengthRemaining(), 9);
    for (; digits < maxCount; ++digits) {
        if (!isASCIIDigit(buffer[digits]))
            break;
    }
    if (!digits)
        return std::nullopt;

    Vector<Latin1Character, 9> padded(9, '0');
    for (size_t i = 0; i < digits; ++i)
        padded[i] = buffer[i];
    buffer.advanceBy(digits);

    unsigned millisecond = parseDecimalInt32(padded.span().first(3));
    unsigned microsecond = parseDecimalInt32(padded.subspan(3, 3));
    unsigned nanosecond = parseDecimalInt32(padded.subspan(6, 3));

    return PlainTime(hour, minute, second, millisecond, microsecond, nanosecond);
}

template<typename CharacterType>
static std::optional<int64_t> parseUTCOffset(StringParsingBuffer<CharacterType>& buffer, bool parseSubMinutePrecision = true)
{
    // UTCOffset[SubMinutePrecision] :
    //     ASCIISign Hour
    //     ASCIISign Hour TimeSeparator[+Extended] MinuteSecond
    //     ASCIISign Hour TimeSeparator[~Extended] MinuteSecond
    //     [+SubMinutePrecision] ASCIISign Hour TimeSeparator[+Extended] MinuteSecond TimeSeparator[+Extended] MinuteSecond TemporalDecimalFractionopt
    //     [+SubMinutePrecision] ASCIISign Hour TimeSeparator[~Extended] MinuteSecond TimeSeparator[~Extended] MinuteSecond TemporalDecimalFractionopt
    //
    //  This is the same to
    //     ASCIISign TimeSpec
    //
    //  Maximum and minimum values are ±23:59:59.999999999 = ±86399999999999ns, which can be represented by int64_t / double's integer part.

    // sign and hour.
    if (buffer.lengthRemaining() < 3)
        return std::nullopt;

    int64_t factor = 1;
    if (*buffer == '+')
        buffer.advance();
    else if (*buffer == '-') {
        factor = -1;
        buffer.advance();
    } else
        return std::nullopt;

    auto plainTime = parseTimeSpec(buffer, Second60Mode::Reject, parseSubMinutePrecision);
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

std::optional<int64_t> parseUTCOffset(StringView string, bool parseSubMinutePrecision)
{
    return readCharactersForParsing(string, [parseSubMinutePrecision](auto buffer) -> std::optional<int64_t> {
        auto result = parseUTCOffset(buffer, parseSubMinutePrecision);
        if (!buffer.atEnd())
            return std::nullopt;
        return result;
    });
}

template<typename CharacterType>
static std::optional<int64_t> parseUTCOffsetInMinutes(StringParsingBuffer<CharacterType>& buffer)
{
    // UTCOffset :::
    //     TemporalSign Hour
    //     TemporalSign Hour HourSubcomponents[+Extended]
    //     TemporalSign Hour HourSubcomponents[~Extended]
    //
    // TemporalSign :::
    //     ASCIISign
    //     <MINUS>
    //
    // ASCIISign ::: one of
    //     + -
    //
    // Hour :::
    //     0 DecimalDigit
    //     1 DecimalDigit
    //     20
    //     21
    //     22
    //     23
    //
    // HourSubcomponents[Extended] :::
    //     TimeSeparator[?Extended] MinuteSecond
    //
    // TimeSeparator[Extended] :::
    //     [+Extended] :
    //     [~Extended] [empty]
    //
    // MinuteSecond :::
    //     0 DecimalDigit
    //     1 DecimalDigit
    //     2 DecimalDigit
    //     3 DecimalDigit
    //     4 DecimalDigit
    //     5 DecimalDigit

    // sign and hour.
    if (buffer.lengthRemaining() < 3)
        return std::nullopt;

    int64_t factor = 1;
    if (*buffer == '+')
        buffer.advance();
    else if (*buffer == '-') {
        factor = -1;
        buffer.advance();
    } else
        return std::nullopt;

    ASSERT(buffer.lengthRemaining() >= 2);
    auto firstHourCharacter = *buffer;
    if (!(firstHourCharacter >= '0' && firstHourCharacter <= '2'))
        return std::nullopt;

    buffer.advance();
    auto secondHourCharacter = *buffer;
    if (!isASCIIDigit(secondHourCharacter))
        return std::nullopt;
    unsigned hour = (secondHourCharacter - '0') + 10 * (firstHourCharacter - '0');
    if (hour >= 24)
        return std::nullopt;
    buffer.advance();

    if (buffer.atEnd())
        return (hour * 60) * factor;

    if (*buffer == ':')
        buffer.advance();
    else if (!(*buffer >= '0' && *buffer <= '5'))
        return (hour * 60) * factor;

    if (buffer.lengthRemaining() < 2)
        return std::nullopt;
    auto firstMinuteCharacter = *buffer;
    if (!(firstMinuteCharacter >= '0' && firstMinuteCharacter <= '5'))
        return std::nullopt;

    buffer.advance();
    auto secondMinuteCharacter = *buffer;
    if (!isASCIIDigit(secondMinuteCharacter))
        return std::nullopt;
    unsigned minute = (secondMinuteCharacter - '0') + 10 * (firstMinuteCharacter - '0');
    ASSERT(minute < 60);
    buffer.advance();

    return (hour * 60 + minute) * factor;
}

std::optional<int64_t> parseUTCOffsetInMinutes(StringView string)
{
    return readCharactersForParsing(string, [](auto buffer) -> std::optional<int64_t> {
        auto result = parseUTCOffsetInMinutes(buffer);
        if (!buffer.atEnd())
            return std::nullopt;
        return result;
    });
}

template<typename CharacterType>
static bool canBeRFC9557Annotation(const StringParsingBuffer<CharacterType>& buffer)
{
    // https://tc39.es/proposal-temporal/#sec-temporal-parseisodatetime
    // Step 4(a)(ii)(2)(a):
    //  Let key be the source text matched by the AnnotationKey Parse Node contained within annotation
    //
    // https://tc39.es/proposal-temporal/#prod-Annotation
    // Annotation :::
    //     [ AnnotationCriticalFlag[opt] AnnotationKey = AnnotationValue ]
    //
    // AnnotationCriticalFlag :::
    //     !
    //
    // AnnotationKey :::
    //     AKeyLeadingChar
    //     AnnotationKey AKeyChar
    //
    // AKeyLeadingChar :::
    //     LowercaseAlpha
    //     _
    //
    // AKeyChar :::
    //     AKeyLeadingChar
    //     DecimalDigit
    //     -
    //
    // AnnotationValue :::
    //     AnnotationValueComponent
    //     AnnotationValueComponent - AnnotationValue
    //
    // AnnotationValueComponent :::
    //     Alpha AnnotationValueComponent[opt]
    //     DecimalDigit AnnotationValueComponent[opt]

    // This just checks for '[', followed by an optional '!' (critical flag),
    // followed by a valid key, followed by an '='.

    size_t length = buffer.lengthRemaining();
    // Because of `[`, `=`, `]`, `AnnotationKey`, and `AnnotationValue`,
    // the annotation must have length >= 5.
    if (length < 5)
        return false;
    if (*buffer != '[')
        return false;
    size_t index = 1;
    if (buffer[index] == '!')
        ++index;
    if (!isASCIILower(buffer[index]) && buffer[index] != '_')
        return false;
    ++index;
    while (index < length) {
        if (buffer[index] == '=')
            return true;
        if (isASCIILower(buffer[index]) || isASCIIDigit(buffer[index]) || buffer[index] == '-' || buffer[index] == '_')
            ++index;
        else
            return false;
    }
    return false;
}

template<typename CharacterType>
static bool canBeTimeZone(const StringParsingBuffer<CharacterType>& buffer, CharacterType character)
{
    switch (static_cast<char16_t>(character)) {
    // UTCDesignator
    // https://tc39.es/proposal-temporal/#prod-UTCDesignator
    case 'z':
    case 'Z':
    // TimeZoneUTCOffsetSign
    // https://tc39.es/proposal-temporal/#prod-TimeZoneUTCOffsetSign
    case '+':
    case '-':
        return true;
    // TimeZoneBracketedAnnotation
    // https://tc39.es/proposal-temporal/#prod-TimeZoneBracketedAnnotation
    case '[': {
        // We should reject calendar extension case.
        // For BNF, see comment in canBeRFC9557Annotation()
        if (canBeRFC9557Annotation(buffer))
            return false;
        return true;
    }
    default:
        return false;
    }
}

template<typename CharacterType>
static std::optional<Variant<Vector<Latin1Character>, int64_t>> parseTimeZoneAnnotation(StringParsingBuffer<CharacterType>& buffer)
{
    // https://tc39.es/proposal-temporal/#prod-TimeZoneAnnotation
    // TimeZoneAnnotation :
    //     [ AnnotationCriticalFlag_opt TimeZoneIdentifier ]
    // TimeZoneIdentifier :
    //     UTCOffset_[~SubMinutePrecision]
    //     TimeZoneIANAName

    if (buffer.lengthRemaining() < 3)
        return std::nullopt;

    if (*buffer != '[')
        return std::nullopt;
    buffer.advance();

    if (*buffer == '!')
        buffer.advance();

    switch (static_cast<char16_t>(*buffer)) {
    case '+':
    case '-': {
        auto offset = parseUTCOffset(buffer, false);
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
        [[fallthrough]];
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

        Vector<Latin1Character> result(buffer.consume(nameLength));

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
    switch (static_cast<char16_t>(*buffer)) {
    // UTCDesignator
    // https://tc39.es/proposal-temporal/#prod-UTCDesignator
    case 'z':
    case 'Z': {
        buffer.advance();
        if (!buffer.atEnd() && *buffer == '[' && canBeTimeZone(buffer, *buffer)) {
            auto timeZone = parseTimeZoneAnnotation(buffer);
            if (!timeZone)
                return std::nullopt;
            return TimeZoneRecord { true, std::nullopt, WTFMove(timeZone.value()) };
        }
        return TimeZoneRecord { true, std::nullopt, { } };
    }
    // TimeZoneUTCOffsetSign
    // https://tc39.es/proposal-temporal/#prod-TimeZoneUTCOffsetSign
    case '+':
    case '-': {
        auto offset = parseUTCOffset(buffer);
        if (!offset)
            return std::nullopt;
        if (!buffer.atEnd() && *buffer == '[' && canBeTimeZone(buffer, *buffer)) {
            auto timeZone = parseTimeZoneAnnotation(buffer);
            if (!timeZone)
                return std::nullopt;
            return TimeZoneRecord { false, offset.value(), WTFMove(timeZone.value()) };
        }
        return TimeZoneRecord { false, offset.value(), { } };
    }
    // TimeZoneBracketedAnnotation
    // https://tc39.es/proposal-temporal/#prod-TimeZoneBracketedAnnotation
    case '[': {
        auto timeZone = parseTimeZoneAnnotation(buffer);
        if (!timeZone)
            return std::nullopt;
        return TimeZoneRecord { false, std::nullopt, WTFMove(timeZone.value()) };
    }
    default:
        return std::nullopt;
    }
}

template<typename CharacterType>
static std::optional<RFC9557Annotation> parseOneRFC9557Annotation(StringParsingBuffer<CharacterType>& buffer)
{
    // For BNF, see comment in canBeRFC9557Annotation()

    if (!canBeRFC9557Annotation(buffer))
        return std::nullopt;
    RFC9557Flag flag = buffer[1] == '!' ? RFC9557Flag::Critical : RFC9557Flag::None;
    // Skip '[' or '[!'
    buffer.advanceBy(flag == RFC9557Flag::Critical ? 2 : 1);

    // Parse the key
    unsigned keyLength = 0;
    while (buffer[keyLength] != '=')
        keyLength++;
    if (!keyLength)
        return std::nullopt;
    auto key(buffer.span().first(keyLength));
    buffer.advanceBy(keyLength);

    if (buffer.atEnd())
        return std::nullopt;

    // Consume the '='
    buffer.advance();

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

    // Check if the key is equal to "u-ca"
    if (key.size() != 4
        || key[0] != 'u' || key[1] != '-'
        || key[2] != 'c' || key[3] != 'a') {
        // Annotation is unknown
        // Consume the rest of the annotation
        buffer.advanceBy(nameLength);
        if (buffer.atEnd() || *buffer != ']') {
            // Parse error
            return std::nullopt;
        }
        // Consume the ']'
        buffer.advance();
        return RFC9557Annotation { flag, RFC9557Key::Other, { } };
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

    Vector<Latin1Character, maxCalendarLength> result(buffer.consume(nameLength));

    if (buffer.atEnd())
        return std::nullopt;
    if (*buffer != ']')
        return std::nullopt;
    buffer.advance();
    return RFC9557Annotation { flag, RFC9557Key::Calendar, WTFMove(result) };
}

template<typename CharacterType>
static std::optional<Vector<CalendarID, 1>>
parseCalendar(StringParsingBuffer<CharacterType>& buffer)
{
    // https://tc39.es/proposal-temporal/#prod-Annotations
    //  Annotations :::
    //      Annotation Annotations[opt]

    if (!canBeRFC9557Annotation(buffer))
        return std::nullopt;

    Vector<CalendarID, 1> result;
    // https://tc39.es/proposal-temporal/#sec-temporal-parseisodatetime
    bool calendarWasCritical = false;
    while (canBeRFC9557Annotation(buffer)) {
        auto annotation = parseOneRFC9557Annotation(buffer);
        if (!annotation)
            return std::nullopt;
        if (annotation->m_key == RFC9557Key::Calendar)
            result.append(annotation->m_value);
        if (annotation->m_flag == RFC9557Flag::Critical) {
            // Check for unknown annotations with critical flag
            // step 4(a)(ii)(2)(d)(i)
            if (annotation->m_key != RFC9557Key::Calendar)
                return std::nullopt;
            // Check for multiple calendars and critical flag
            // step 4(a)(ii)(2)(c)(ii)
            if (result.size() == 1)
                calendarWasCritical = true;
            else
                return std::nullopt;
        }
        if (calendarWasCritical && result.size() > 1)
            return std::nullopt;
    }
    return result;
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

template<typename CharacterType>
static bool canBeYear(StringParsingBuffer<CharacterType>& buffer)
{
    // 4 characters for year, plus 2 more for month
    if (buffer.lengthRemaining() < 6)
        return false;
    bool hasPrefix = buffer[0] == '+' || buffer[0] == '-';
    if (!isASCIIDigit(buffer[0]) && !hasPrefix)
        return false;
    size_t start = hasPrefix ? 1 : 0;
    for (size_t i = start; i < 4 + start; i++) {
        if (!isASCIIDigit(buffer[i]))
            return false;
    }
    return true;
}

template<typename CharacterType>
static std::optional<PlainDate> parseDate(StringParsingBuffer<CharacterType>& buffer, TemporalDateFormat format)
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
    //
    //  DateSpecYearMonth :::
    //      DateYear DateSeparator_[+Extended] DateMonth
    //      DateYear DateSeparator_[~Extended] DateMonth
    //
    //  DateSpecMonthDay :::
    //      --opt DateMonth DateSeparator_[+Extended] DateDay
    //      --opt DateMonth DateSeparator_[~Extended] DateDay

    if (buffer.atEnd())
        return std::nullopt;

    int32_t year = 0;
    bool splitByHyphen = false;

    if (*buffer == '-') {
        if (buffer.lengthRemaining() > 2
            && buffer[1] == '-'
            && format == TemporalDateFormat::MonthDay) {
            buffer.advanceBy(2);
        }
    }

    // Look ahead to distinguish month from year
    if (canBeYear(buffer)) {
        bool sixDigitsYear = false;
        int yearFactor = 1;
        if (*buffer == '+') {
            buffer.advance();
            sixDigitsYear = true;
        } else if (*buffer == '-') {
            yearFactor = -1;
            buffer.advance();
            sixDigitsYear = true;
        } else if (!isASCIIDigit(*buffer))
            return std::nullopt;

        if (sixDigitsYear) {
            if (buffer.lengthRemaining() < 6)
                return std::nullopt;
            for (unsigned index = 0; index < 6; ++index) {
                if (!isASCIIDigit(buffer[index]))
                    return std::nullopt;
            }
            year = parseDecimalInt32(std::span { buffer.position(), 6 }) * yearFactor;
            if (!year && yearFactor < 0)
                return std::nullopt;
            buffer.advanceBy(6);
        } else {
            if (buffer.lengthRemaining() < 4)
                return std::nullopt;
            for (unsigned index = 0; index < 4; ++index) {
                if (!isASCIIDigit(buffer[index]))
                return std::nullopt;
            }
            year = parseDecimalInt32(std::span { buffer.position(), 4 });
            buffer.advanceBy(4);
        }

        if (buffer.atEnd())
            return std::nullopt;

        if (*buffer == '-') {
            splitByHyphen = true;
            buffer.advance();
            if (buffer.lengthRemaining() < 5 && format == TemporalDateFormat::Date)
                return std::nullopt;
        } else {
            if (buffer.lengthRemaining() < 4 && format == TemporalDateFormat::Date)
                return std::nullopt;
        }
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

    if (format == TemporalDateFormat::YearMonth && buffer.atEnd())
        return PlainDate(year, month, 1);

    if (*buffer == '-') {
        if (splitByHyphen || format != TemporalDateFormat::Date)
            buffer.advance();
        else
            return std::nullopt;
    } else if (splitByHyphen)
        return std::nullopt;

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
    } else if (format != TemporalDateFormat::YearMonth)
        return std::nullopt;

    // PlainDate represents out-of-range years using outOfRangeYear
    if (!isYearWithinLimits(year)) [[unlikely]]
        year = outOfRangeYear;

    switch (format) {
    case TemporalDateFormat::Date:
        return PlainDate(year, month, day);
    case TemporalDateFormat::YearMonth:
        return PlainDate(year, month, 1);
    case TemporalDateFormat::MonthDay:
        return PlainDate(1972, month, day);
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

template<typename CharacterType>
static std::optional<std::tuple<PlainDate, std::optional<PlainTime>, std::optional<TimeZoneRecord>>> parseDateTime(StringParsingBuffer<CharacterType>& buffer, TemporalDateFormat format)
{
    // https://tc39.es/proposal-temporal/#prod-DateTime
    // DateTime :
    //     Date TimeSpecSeparator[opt] TimeZone[opt]
    //
    // TimeSpecSeparator :
    //     DateTimeSeparator TimeSpec
    auto plainDate = parseDate(buffer, format);
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

    if (canBeTimeZone(buffer, *buffer))
        return std::nullopt;

    return std::tuple { WTFMove(plainDate.value()), std::nullopt, std::nullopt };
}

template<typename CharacterType>
static std::optional<std::tuple<PlainTime, std::optional<TimeZoneRecord>, std::optional<CalendarID>>> parseCalendarTime(StringParsingBuffer<CharacterType>& buffer)
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

    std::optional<CalendarID> calendarOptional;
    if (canBeRFC9557Annotation(buffer)) {
        auto calendars = parseCalendar(buffer);
        if (!calendars)
            return std::nullopt;
        if (calendars.value().size() > 0)
            calendarOptional = WTFMove(calendars.value()[0]);
    }

    return std::tuple { WTFMove(plainTime.value()), WTFMove(timeZoneOptional), WTFMove(calendarOptional) };
}

template<typename CharacterType>
static std::optional<std::tuple<PlainDate, std::optional<PlainTime>, std::optional<TimeZoneRecord>, std::optional<CalendarID>>> parseCalendarDateTime(StringParsingBuffer<CharacterType>& buffer, TemporalDateFormat format)
{
    // https://tc39.es/proposal-temporal/#prod-DateTime
    // CalendarDateTime :
    //     DateTime CalendarName[opt]
    //
    auto dateTime = parseDateTime(buffer, format);
    if (!dateTime)
        return std::nullopt;

    auto [plainDate, plainTimeOptional, timeZoneOptional] = WTFMove(dateTime.value());

    std::optional<CalendarID> calendarOptional;
    if (!buffer.atEnd() && canBeRFC9557Annotation(buffer)) {
        auto calendars = parseCalendar(buffer);
        if (!calendars)
            return std::nullopt;
        if (calendars.value().size() > 0)
            calendarOptional = WTFMove(calendars.value()[0]);
    }

    return std::tuple { WTFMove(plainDate), WTFMove(plainTimeOptional), WTFMove(timeZoneOptional), WTFMove(calendarOptional) };
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

template<typename CharacterType>
static bool isAmbiguousCalendarTime(StringParsingBuffer<CharacterType>& buffer)
{
    auto length = buffer.lengthRemaining();
    ASSERT(length > 1);

    // There is no ambiguity if we have a TimeDesignator.
    if (toASCIIUpper(*buffer) == 'T')
        return false;

    // The string is known to be valid as `TimeSpec TimeZone[opt]`, so DateExtendedYear and TwoDashes are not possible.
    // Actual possibilities are `DateFourDigitYear -[opt] DateMonth` and `DateMonth -[opt] DateDay`, i.e. YYYY-MM, YYYYMM, MM-DD, MMDD.
    ASSERT(isASCIIDigit(buffer[0]) && isASCIIDigit(buffer[1]));

    unsigned monthPartLength = 2;
    switch (length) {
    case 7:
        if (!isASCIIDigit(buffer[2]) || !isASCIIDigit(buffer[3]) || buffer[4] != '-' || !isASCIIDigit(buffer[5]) || !isASCIIDigit(buffer[6]))
            return false;
        buffer.advanceBy(5);
        break;
    case 6:
        if (!isASCIIDigit(buffer[2]) || !isASCIIDigit(buffer[3]) || !isASCIIDigit(buffer[4]) || !isASCIIDigit(buffer[5]))
            return false;
        buffer.advanceBy(4);
        break;
    case 5:
        if (buffer[2] != '-' || !isASCIIDigit(buffer[3]) || !isASCIIDigit(buffer[4]))
            return false;
        monthPartLength++;
        break;
    case 4:
        if (!isASCIIDigit(buffer[2]) || !isASCIIDigit(buffer[3]))
            return false;
        break;
    default:
        return false;
    }

    // Any YYYY is valid, we just need to check the MM and DD.
    unsigned month = (buffer[0] - '0') * 10 + (buffer[1] - '0');
    if (!month || month > 12)
        return false;

    buffer.advanceBy(monthPartLength);
    if (buffer.hasCharactersRemaining()) {
        unsigned day = (buffer[0] - '0') * 10 + (buffer[1] - '0');
        if (!day || day > daysInMonth(month))
            return false;
    }

    return true;
}

std::optional<std::tuple<PlainTime, std::optional<TimeZoneRecord>, std::optional<CalendarID>>> parseCalendarTime(StringView string)
{
    auto tuple = readCharactersForParsing(string, [](auto buffer) -> std::optional<std::tuple<PlainTime, std::optional<TimeZoneRecord>, std::optional<CalendarID>>> {
        auto result = parseCalendarTime(buffer);
        if (!buffer.atEnd())
            return std::nullopt;
        return result;
    });

    // Without a calendar, we need to verify that the parse isn't ambiguous with DateSpecYearMonth or DateSpecMonthDay.
    if (tuple && !std::get<2>(tuple.value())) {
        if (readCharactersForParsing(string, [](auto buffer) -> bool { return isAmbiguousCalendarTime(buffer); }))
            return std::nullopt;
    }

    return tuple;
}

std::optional<std::tuple<PlainDate, std::optional<PlainTime>, std::optional<TimeZoneRecord>>> parseDateTime(StringView string, TemporalDateFormat format)
{
    return readCharactersForParsing(string, [format](auto buffer) -> std::optional<std::tuple<PlainDate, std::optional<PlainTime>, std::optional<TimeZoneRecord>>> {
        auto result = parseDateTime(buffer, format);
        if (!buffer.atEnd())
            return std::nullopt;
        return result;
    });
}

std::optional<std::tuple<PlainDate, std::optional<PlainTime>, std::optional<TimeZoneRecord>, std::optional<CalendarID>>> parseCalendarDateTime(StringView string, TemporalDateFormat format)
{
    return readCharactersForParsing(string, [format](auto buffer) -> std::optional<std::tuple<PlainDate, std::optional<PlainTime>, std::optional<TimeZoneRecord>, std::optional<CalendarID>>> {
        auto result = parseCalendarDateTime(buffer, format);
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
        auto datetime = parseCalendarDateTime(buffer, TemporalDateFormat::Date);
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

uint8_t dayOfWeek(PlainDate plainDate)
{
    Int128 dateDays = static_cast<Int128>(dateToDaysFrom1970(plainDate.year(), plainDate.month() - 1, plainDate.day()));
    int weekDay = static_cast<int>((dateDays + 4) % 7);
    if (weekDay < 0)
        weekDay += 7;
    return !weekDay ? 7 : weekDay;
}

uint16_t dayOfYear(PlainDate plainDate)
{
    return dayInYear(plainDate.year(), plainDate.month() - 1, plainDate.day()) + 1; // Always start with 1 (1/1 is 1).
}

uint8_t weekOfYear(PlainDate plainDate)
{
    int32_t dayOfYear = ISO8601::dayOfYear(plainDate);
    int32_t dayOfWeek = ISO8601::dayOfWeek(plainDate);

    // ISO week 1 is the week containing the first Thursday (4) of the year.
    // https://en.wikipedia.org/wiki/ISO_week_date#Algorithms
    int32_t week = (dayOfYear - dayOfWeek + 10) / 7;
    if (week <= 0) {
        // Previous year's last week. Thus, 52 or 53 weeks. Getting weeks in the previous year.
        //
        // https://en.wikipedia.org/wiki/ISO_week_date#Weeks_per_year
        // > The long years, with 53 weeks in them, can be described by any of the following equivalent definitions:
        // >  - any year ending on Thursday (D, ED) and any leap year ending on Friday (DC)

        int32_t dayOfWeekForJanuaryFirst = ISO8601::dayOfWeek(PlainDate { plainDate.year(), 1, 1 });

        // Any year ending on Thursday (D, ED) -> this year's 1/1 is Friday.
        if (dayOfWeekForJanuaryFirst == 5)
            return 53;

        // Any leap year ending on Friday (DC) -> this year's 1/1 is Saturday and previous year is a leap year.
        if (dayOfWeekForJanuaryFirst == 6 && isLeapYear(plainDate.year() - 1))
            return 53;

        return 52;
    }

    if (week == 53) {
        // Check whether this is in next year's week 1.
        if ((daysInYear(plainDate.year()) - dayOfYear) < (4 - dayOfWeek))
            return 1;
    }

    return week;
}

static constexpr uint8_t daysInMonths[2][12] = {
    { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
    { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

// https://tc39.es/proposal-temporal/#sec-temporal-isodaysinmonth
uint8_t daysInMonth(int32_t year, uint8_t month)
{
    return daysInMonths[isLeapYear(year)][month - 1];
}

uint8_t daysInMonth(uint8_t month)
{
    constexpr unsigned isLeapYear = 1;
    return daysInMonths[isLeapYear][month - 1];
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
        auto fraction = numberToStringUnsigned<Vector<Latin1Character, 9>>(nanoseconds);
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
            fraction.shrink(validLength.value());
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
        auto fraction = numberToStringUnsigned<Vector<Latin1Character, 9>>(fractionNanoseconds);
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
            fraction.shrink(validLength.value());
        else
            fraction.clear();
        return makeString(pad('0', 2, plainTime.hour()), ':', pad('0', 2, plainTime.minute()), ':', pad('0', 2, plainTime.second()), '.', pad('0', paddingLength, emptyString()), fraction);
    }
    if (!precisionValue)
        return makeString(pad('0', 2, plainTime.hour()), ':', pad('0', 2, plainTime.minute()), ':', pad('0', 2, plainTime.second()));
    auto fraction = numberToStringUnsigned<Vector<Latin1Character, 9>>(fractionNanoseconds);
    unsigned paddingLength = 9 - fraction.size();
    paddingLength = std::min(paddingLength, precisionValue);
    precisionValue -= paddingLength;
    fraction.resize(precisionValue);
    return makeString(pad('0', 2, plainTime.hour()), ':', pad('0', 2, plainTime.minute()), ':', pad('0', 2, plainTime.second()), '.', pad('0', paddingLength, emptyString()), fraction);
}

static String temporalDateToString(int32_t year, int32_t month)
{
    // If we're printing a date, it should be within range
    ASSERT(isYearWithinLimits(year));

    String prefix;
    auto yearDigits = 4;
    if (year < 0 || year > 9999) {
        prefix = year < 0 ? "-"_s : "+"_s;
        yearDigits = 6;
        year = std::abs(year);
    }

    return makeString(prefix, pad('0', yearDigits, year), '-', pad('0', 2, month));
}

static String temporalDateToString(int32_t year, int32_t month, int32_t day)
{
    auto first = temporalDateToString(year, month);
    return makeString(first, '-', pad('0', 2, day));
}

String temporalDateTimeToString(PlainDate plainDate, PlainTime plainTime, std::tuple<Precision, unsigned> precision)
{
    return makeString(temporalDateToString(plainDate), 'T', temporalTimeToString(plainTime, precision));
}

String temporalDateToString(PlainDate plainDate)
{
    return temporalDateToString(plainDate.year(), plainDate.month(), plainDate.day());
}

String temporalMonthDayToString(PlainMonthDay plainMonthDay, StringView calendarName)
{
    if (calendarName == "always"_s) {
        // FIXME: print the correct calendar ID when calendars are fully implemented
        auto first = temporalDateToString(plainMonthDay.isoPlainDate());
        return makeString(first, "[u-ca=iso8601]"_s);
    }

    return makeString(pad('0', 2, plainMonthDay.month()), '-', pad('0', 2, plainMonthDay.day()));
}

String monthCode(uint32_t month)
{
    return makeString('M', pad('0', 2, month));
}

// https://tc39.es/proposal-temporal/#sec-temporal-parsemonthcode
std::optional<ParsedMonthCode> parseMonthCode(StringView monthCode)
{
    // Allow leap month marker (e.g. "M05L"), even though it doesn't apply to ISO8601 calendar
    if (monthCode.length() < 3 || monthCode.length() > 4 || !monthCode.startsWith('M') || !isASCIIDigit(monthCode[2]))
        return { };

    // 4th code unit must be 'L' because the month code is valid
    auto isLeapMonth = monthCode.length() == 4;

    uint8_t monthNumber = monthCode[2] - '0';
    monthNumber += (monthCode[1] - '0') * 10;

    return ParsedMonthCode { monthNumber, isLeapMonth };
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

} // namespace ISO8601

CheckedInt128 checkedCastDoubleToInt128(double n)
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

namespace ISO8601 {

template<TemporalUnit unit>
std::optional<Int128> Duration::totalNanoseconds() const
{
    ASSERT(unit >= TemporalUnit::Day);

    CheckedInt128 resultNs { 0 };

    if constexpr (unit <= TemporalUnit::Day) {
        CheckedInt128 days = checkedCastDoubleToInt128(this->days());
        resultNs += days * ExactTime::nsPerDay;
    }
    if constexpr (unit <= TemporalUnit::Hour) {
        CheckedInt128 hours = checkedCastDoubleToInt128(this->hours());
        resultNs += hours * ExactTime::nsPerHour;
    }
    if constexpr (unit <= TemporalUnit::Minute) {
        CheckedInt128 minutes = checkedCastDoubleToInt128(this->minutes());
        resultNs += minutes * ExactTime::nsPerMinute;
    }
    if constexpr (unit <= TemporalUnit::Second) {
        CheckedInt128 seconds = checkedCastDoubleToInt128(this->seconds());
        resultNs += seconds * ExactTime::nsPerSecond;
    }
    if constexpr (unit <= TemporalUnit::Millisecond) {
        CheckedInt128 milliseconds = checkedCastDoubleToInt128(this->milliseconds());
        resultNs += milliseconds * ExactTime::nsPerMillisecond;
    }
    if constexpr (unit <= TemporalUnit::Microsecond) {
        CheckedInt128 microseconds = checkedCastDoubleToInt128(this->microseconds());
        resultNs += microseconds * ExactTime::nsPerMicrosecond;
    }
    if constexpr (unit <= TemporalUnit::Nanosecond)
        resultNs += checkedCastDoubleToInt128(this->nanoseconds());

    if (resultNs.hasOverflowed())
        return std::nullopt;

    return resultNs;
}
template std::optional<Int128> Duration::totalNanoseconds<TemporalUnit::Day>() const;
template std::optional<Int128> Duration::totalNanoseconds<TemporalUnit::Second>() const;
template std::optional<Int128> Duration::totalNanoseconds<TemporalUnit::Millisecond>() const;
template std::optional<Int128> Duration::totalNanoseconds<TemporalUnit::Microsecond>() const;

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

    // 3. If abs(years) ≥ 2^32, return false.
    // 4. If abs(months) ≥ 2^32, return false.
    // 5. If abs(weeks) ≥ 2^32, return false.
    constexpr double limit = 1ULL << 32;
    if (std::abs(duration[TemporalUnit::Year]) >= limit || std::abs(duration[TemporalUnit::Month]) >= limit || std::abs(duration[TemporalUnit::Week]) >= limit)
        return false;

    // 6. Let normalizedSeconds be days × 86,400 + hours × 3600 + minutes × 60 + seconds + ℝ(𝔽(milliseconds)) × 10^-3 + ℝ(𝔽(microseconds)) × 10^-6 + ℝ(𝔽(nanoseconds)) × 10^-9.
    auto normalizedNanoseconds = duration.totalNanoseconds<TemporalUnit::Day>();
    // 8. If abs(normalizedSeconds) ≥ 2^53, return false.
    constexpr Int128 nanosecondsLimit = (Int128(1) << 53) * 1000000000;
    if (!normalizedNanoseconds || absInt128(normalizedNanoseconds.value()) >= nanosecondsLimit)
        return false;

    return true;
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

// https://tc39.es/proposal-temporal/#sec-temporal-roundtemporalinstant
static Int128 roundTemporalInstant(Int128 ns, unsigned increment, TemporalUnit unit, RoundingMode roundingMode)
{
    auto unitLength = lengthInNanoseconds(unit);
    auto incrementNs = increment * unitLength;
    return roundNumberToIncrementAsIfPositive(ns, incrementNs, roundingMode);
}

// https://tc39.es/proposal-temporal/#sec-validatetemporalroundingincrement
static void validateTemporalRoundingIncrement(JSGlobalObject* globalObject, unsigned increment,
    Int128 dividend, Inclusivity inclusive)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Int128 maximum = 0;
    switch (inclusive) {
    case Inclusivity::Inclusive:
        maximum = dividend;
        break;
    case Inclusivity::Exclusive:
        ASSERT(dividend > 1);
        maximum = dividend - 1;
        break;
    }
    if (increment > maximum)
        throwRangeError(globalObject, scope, "Rounding increment exceeds maximum value"_s);
    else if (dividend % increment)
        throwRangeError(globalObject, scope, "Rounding increment does not divide evenly into maximum value"_s);
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.round
// (Steps 10-17 only)
ExactTime ExactTime::round(JSGlobalObject* globalObject, unsigned increment,
    TemporalUnit unit, RoundingMode roundingMode) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Int128 maximum = 0;
    switch (unit) {
    case TemporalUnit::Hour: maximum = static_cast<Int128>(WTF::hoursPerDay); break;
    case TemporalUnit::Minute: maximum = static_cast<Int128>(minutesPerHour * WTF::hoursPerDay); break;
    case TemporalUnit::Second: maximum = static_cast<Int128>(secondsPerMinute * minutesPerHour * WTF::hoursPerDay); break;
    case TemporalUnit::Millisecond: maximum = static_cast<Int128>(msPerDay); break;
    case TemporalUnit::Microsecond: maximum = static_cast<Int128>(msPerDay * 1000); break;
    case TemporalUnit::Nanosecond: maximum = nsPerDay; break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
    validateTemporalRoundingIncrement(globalObject, increment, maximum, Inclusivity::Inclusive);
    RETURN_IF_EXCEPTION(scope, { });
    auto roundedNs = roundTemporalInstant(m_epochNanoseconds, increment, unit, roundingMode);
    return ExactTime { roundedNs };
}

// https://tc39.es/proposal-temporal/#sec-temporal-roundtimedurationtoincrement
static Int128 roundTimeDurationToIncrement(JSGlobalObject* globalObject, Int128 d, Int128 increment,
    RoundingMode roundingMode)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Int128 rounded = roundNumberToIncrementInt128(d, increment, roundingMode);
    if (absInt128(rounded) > InternalDuration::maxTimeDuration) {
        throwRangeError(globalObject, scope, "Rounded time duration exceeds maximum"_s);
        return 0;
    }
    return rounded;
}

// https://tc39.es/proposal-temporal/#sec-temporal-roundtimeduration
Int128 roundTimeDuration(JSGlobalObject* globalObject, Int128 timeDuration, unsigned increment, TemporalUnit unit, RoundingMode roundingMode)
{
    auto divisor = lengthInNanoseconds(unit);

    return roundTimeDurationToIncrement(globalObject, timeDuration,
        (divisor * increment), roundingMode);
}

// https://tc39.es/proposal-temporal/#sec-temporal-datedurationsign
static int32_t dateDurationSign(const Duration& d)
{
    if (d.years() > 0)
        return 1;
    if (d.years() < 0)
        return -1;
    if (d.months() > 0)
        return 1;
    if (d.months() < 0)
        return -1;
    if (d.weeks() > 0)
        return 1;
    if (d.weeks() < 0)
        return -1;
    if (d.days() > 0)
        return 1;
    if (d.days() < 0)
        return -1;
    return 0;
}

// https://tc39.es/proposal-temporal/#sec-temporal-internaldurationsign
int32_t ISO8601::InternalDuration::sign() const
{
    int32_t sign = dateDurationSign(m_dateDuration);
    if (sign)
        return sign;
    return timeDurationSign();
}

// https://tc39.es/proposal-temporal/#sec-temporal-combinedateandtimeduration
InternalDuration InternalDuration::combineDateAndTimeDuration(Duration dateDuration, Int128 timeDuration)
{
    int32_t dateSign = dateDurationSign(dateDuration);
    int32_t timeSign = timeDuration < 0 ? -1 : timeDuration > 0 ? 1 : 0;
    bool signsDiffer = dateSign && timeSign && dateSign != timeSign;
    ASSERT_UNUSED(signsDiffer, !signsDiffer);
    return InternalDuration { WTFMove(dateDuration), timeDuration };
}

// DifferenceInstant ( ns1, ns2, roundingIncrement, smallestUnit, roundingMode )
// https://tc39.es/proposal-temporal/#sec-temporal-differenceinstant
InternalDuration ExactTime::difference(JSGlobalObject* globalObject, ExactTime other, unsigned roundingIncrement, TemporalUnit smallestUnit, RoundingMode roundingMode) const
{
    Int128 timeDuration = other.m_epochNanoseconds - m_epochNanoseconds;
    timeDuration = roundTimeDuration(globalObject, timeDuration, roundingIncrement, smallestUnit, roundingMode);
    return InternalDuration::combineDateAndTimeDuration(ISO8601::Duration(), timeDuration);
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

// https://tc39.es/proposal-temporal/#sec-temporal-isvalidisodate
bool isValidISODate(double year, double month, double day)
{
    if (month < 1 || month > 12)
        return false;
    auto daysInMonth1 = daysInMonth(year, month);
    if (day < 1 || day > daysInMonth1)
        return false;
    return true;
}

// https://tc39.es/proposal-temporal/#sec-temporal-create-iso-date-record
PlainDate createISODateRecord(double year, double month, double day)
{
    ASSERT(isValidISODate(year, month, day));
    return PlainDate(year, month, day);
}

} // namespace ISO8601
} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
