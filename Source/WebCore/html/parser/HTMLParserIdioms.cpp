/*
 * Copyright (C) 2010-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "HTMLParserIdioms.h"

#include "Decimal.h"
#include "QualifiedName.h"
#include <limits>
#include <wtf/MathExtras.h>
#include <wtf/URL.h>
#include <wtf/dtoa.h>

namespace WebCore {

template <typename CharType>
static String stripLeadingAndTrailingHTMLSpaces(String string, CharType characters, unsigned length)
{
    unsigned numLeadingSpaces = 0;
    unsigned numTrailingSpaces = 0;

    for (; numLeadingSpaces < length; ++numLeadingSpaces) {
        if (isNotHTMLSpace(characters[numLeadingSpaces]))
            break;
    }

    if (numLeadingSpaces == length)
        return string.isNull() ? string : emptyAtom().string();

    for (; numTrailingSpaces < length; ++numTrailingSpaces) {
        if (isNotHTMLSpace(characters[length - numTrailingSpaces - 1]))
            break;
    }

    ASSERT(numLeadingSpaces + numTrailingSpaces < length);

    if (!(numLeadingSpaces | numTrailingSpaces))
        return string;

    return string.substring(numLeadingSpaces, length - (numLeadingSpaces + numTrailingSpaces));
}

String stripLeadingAndTrailingHTMLSpaces(const String& string)
{
    unsigned length = string.length();

    if (!length)
        return string.isNull() ? string : emptyAtom().string();

    if (string.is8Bit())
        return stripLeadingAndTrailingHTMLSpaces(string, string.characters8(), length);

    return stripLeadingAndTrailingHTMLSpaces(string, string.characters16(), length);
}

String serializeForNumberType(const Decimal& number)
{
    if (number.isZero()) {
        // Decimal::toString appends exponent, e.g. "0e-18"
        return number.isNegative() ? "-0" : "0";
    }
    return number.toString();
}

String serializeForNumberType(double number)
{
    // According to HTML5, "the best representation of the number n as a floating
    // point number" is a string produced by applying ToString() to n.
    return String::numberToStringECMAScript(number);
}

Decimal parseToDecimalForNumberType(const String& string, const Decimal& fallbackValue)
{
    // See HTML5 2.5.4.3 `Real numbers.' and parseToDoubleForNumberType

    // String::toDouble() accepts leading + and whitespace characters, which are not valid here.
    const UChar firstCharacter = string[0];
    if (firstCharacter != '-' && firstCharacter != '.' && !isASCIIDigit(firstCharacter))
        return fallbackValue;

    const Decimal value = Decimal::fromString(string);
    if (!value.isFinite())
        return fallbackValue;

    // Numbers are considered finite IEEE 754 single-precision floating point values.
    // See HTML5 2.5.4.3 `Real numbers.'
    // FIXME: We should use numeric_limits<double>::max for number input type.
    const Decimal floatMax = Decimal::fromDouble(std::numeric_limits<float>::max());
    if (value < -floatMax || value > floatMax)
        return fallbackValue;

    // We return +0 for -0 case.
    return value.isZero() ? Decimal(0) : value;
}

Decimal parseToDecimalForNumberType(const String& string)
{
    return parseToDecimalForNumberType(string, Decimal::nan());
}

double parseToDoubleForNumberType(const String& string, double fallbackValue)
{
    // See HTML5 2.5.4.3 `Real numbers.'

    // String::toDouble() accepts leading + and whitespace characters, which are not valid here.
    UChar firstCharacter = string[0];
    if (firstCharacter != '-' && firstCharacter != '.' && !isASCIIDigit(firstCharacter))
        return fallbackValue;

    bool valid = false;
    double value = string.toDouble(&valid);
    if (!valid)
        return fallbackValue;

    // NaN and infinity are considered valid by String::toDouble, but not valid here.
    if (!std::isfinite(value))
        return fallbackValue;

    // Numbers are considered finite IEEE 754 single-precision floating point values.
    // See HTML5 2.5.4.3 `Real numbers.'
    if (-std::numeric_limits<float>::max() > value || value > std::numeric_limits<float>::max())
        return fallbackValue;

    // The following expression converts -0 to +0.
    return value ? value : 0;
}

double parseToDoubleForNumberType(const String& string)
{
    return parseToDoubleForNumberType(string, std::numeric_limits<double>::quiet_NaN());
}

template <typename CharacterType>
static Expected<int, HTMLIntegerParsingError> parseHTMLIntegerInternal(const CharacterType* position, const CharacterType* end)
{
    while (position < end && isHTMLSpace(*position))
        ++position;

    if (position == end)
        return makeUnexpected(HTMLIntegerParsingError::Other);

    bool isNegative = false;
    if (*position == '-') {
        isNegative = true;
        ++position;
    } else if (*position == '+')
        ++position;

    if (position == end || !isASCIIDigit(*position))
        return makeUnexpected(HTMLIntegerParsingError::Other);

    constexpr int intMax = std::numeric_limits<int>::max();
    constexpr int base = 10;
    constexpr int maxMultiplier = intMax / base;

    unsigned result = 0;
    do {
        int digitValue = *position - '0';

        if (result > maxMultiplier || (result == maxMultiplier && digitValue > (intMax % base) + isNegative))
            return makeUnexpected(isNegative ? HTMLIntegerParsingError::NegativeOverflow : HTMLIntegerParsingError::PositiveOverflow);

        result = base * result + digitValue;
        ++position;
    } while (position < end && isASCIIDigit(*position));

    return isNegative ? -result : result;
}

// https://html.spec.whatwg.org/multipage/infrastructure.html#rules-for-parsing-integers
Expected<int, HTMLIntegerParsingError> parseHTMLInteger(StringView input)
{
    unsigned length = input.length();
    if (!length)
        return makeUnexpected(HTMLIntegerParsingError::Other);

    if (LIKELY(input.is8Bit())) {
        auto* start = input.characters8();
        return parseHTMLIntegerInternal(start, start + length);
    }

    auto* start = input.characters16();
    return parseHTMLIntegerInternal(start, start + length);
}

// https://html.spec.whatwg.org/multipage/infrastructure.html#rules-for-parsing-non-negative-integers
Expected<unsigned, HTMLIntegerParsingError> parseHTMLNonNegativeInteger(StringView input)
{
    auto optionalSignedResult = parseHTMLInteger(input);
    if (!optionalSignedResult)
        return makeUnexpected(WTFMove(optionalSignedResult.error()));

    if (optionalSignedResult.value() < 0)
        return makeUnexpected(HTMLIntegerParsingError::NegativeOverflow);

    return static_cast<unsigned>(optionalSignedResult.value());
}

template <typename CharacterType>
static Optional<int> parseValidHTMLNonNegativeIntegerInternal(const CharacterType* position, const CharacterType* end)
{
    // A string is a valid non-negative integer if it consists of one or more ASCII digits.
    for (auto* c = position; c < end; ++c) {
        if (!isASCIIDigit(*c))
            return WTF::nullopt;
    }

    auto optionalSignedValue = parseHTMLIntegerInternal(position, end);
    if (!optionalSignedValue || optionalSignedValue.value() < 0)
        return WTF::nullopt;

    return optionalSignedValue.value();
}

// https://html.spec.whatwg.org/#valid-non-negative-integer
Optional<int> parseValidHTMLNonNegativeInteger(StringView input)
{
    if (input.isEmpty())
        return WTF::nullopt;

    if (LIKELY(input.is8Bit())) {
        auto* start = input.characters8();
        return parseValidHTMLNonNegativeIntegerInternal(start, start + input.length());
    }

    auto* start = input.characters16();
    return parseValidHTMLNonNegativeIntegerInternal(start, start + input.length());
}

template <typename CharacterType>
static Optional<double> parseValidHTMLFloatingPointNumberInternal(const CharacterType* position, size_t length)
{
    ASSERT(length > 0);

    // parseDouble() allows the string to start with a '+' or to end with a '.' but those
    // are not valid floating point numbers as per HTML.
    if (*position == '+' || *(position + length - 1) == '.')
        return WTF::nullopt;

    size_t parsedLength = 0;
    double number = parseDouble(position, length, parsedLength);
    return parsedLength == length && std::isfinite(number) ? number : Optional<double>();
}

// https://html.spec.whatwg.org/#valid-floating-point-number
Optional<double> parseValidHTMLFloatingPointNumber(StringView input)
{
    if (input.isEmpty())
        return WTF::nullopt;

    if (LIKELY(input.is8Bit())) {
        auto* start = input.characters8();
        return parseValidHTMLFloatingPointNumberInternal(start, input.length());
    }

    auto* start = input.characters16();
    return parseValidHTMLFloatingPointNumberInternal(start, input.length());
}

static inline bool isHTMLSpaceOrDelimiter(UChar character)
{
    return isHTMLSpace(character) || character == ',' || character == ';';
}

static inline bool isNumberStart(UChar character)
{
    return isASCIIDigit(character) || character == '.' || character == '-';
}

// https://html.spec.whatwg.org/multipage/infrastructure.html#rules-for-parsing-floating-point-number-values
template <typename CharacterType>
static Vector<double> parseHTMLListOfOfFloatingPointNumberValuesInternal(const CharacterType* position, const CharacterType* end)
{
    Vector<double> numbers;

    // This skips past any leading delimiters.
    while (position < end && isHTMLSpaceOrDelimiter(*position))
        ++position;

    while (position < end) {
        // This skips past leading garbage.
        while (position < end && !(isHTMLSpaceOrDelimiter(*position) || isNumberStart(*position)))
            ++position;

        const CharacterType* numberStart = position;
        while (position < end && !isHTMLSpaceOrDelimiter(*position))
            ++position;

        size_t parsedLength = 0;
        double number = parseDouble(numberStart, position - numberStart, parsedLength);
        numbers.append(parsedLength > 0 && std::isfinite(number) ? number : 0);

        // This skips past the delimiter.
        while (position < end && isHTMLSpaceOrDelimiter(*position))
            ++position;
    }

    return numbers;
}

Vector<double> parseHTMLListOfOfFloatingPointNumberValues(StringView input)
{
    if (LIKELY(input.is8Bit())) {
        auto* start = input.characters8();
        return parseHTMLListOfOfFloatingPointNumberValuesInternal(start, start + input.length());
    }

    auto* start = input.characters16();
    return parseHTMLListOfOfFloatingPointNumberValuesInternal(start, start + input.length());
}

static bool threadSafeEqual(const StringImpl& a, const StringImpl& b)
{
    if (&a == &b)
        return true;
    if (a.hash() != b.hash())
        return false;
    return equal(a, b);
}

bool threadSafeMatch(const QualifiedName& a, const QualifiedName& b)
{
    return threadSafeEqual(*a.localName().impl(), *b.localName().impl());
}

String parseCORSSettingsAttribute(const AtomicString& value)
{
    if (value.isNull())
        return String();
    if (equalIgnoringASCIICase(value, "use-credentials"))
        return "use-credentials"_s;
    return "anonymous"_s;
}

// https://html.spec.whatwg.org/multipage/semantics.html#attr-meta-http-equiv-refresh
template <typename CharacterType>
static bool parseHTTPRefreshInternal(const CharacterType* position, const CharacterType* end, double& parsedDelay, String& parsedURL)
{
    while (position < end && isHTMLSpace(*position))
        ++position;

    unsigned time = 0;

    const CharacterType* numberStart = position;
    while (position < end && isASCIIDigit(*position))
        ++position;

    StringView timeString(numberStart, position - numberStart);
    if (timeString.isEmpty()) {
        if (position >= end || *position != '.')
            return false;
    } else {
        auto optionalNumber = parseHTMLNonNegativeInteger(timeString);
        if (!optionalNumber)
            return false;
        time = optionalNumber.value();
    }

    while (position < end && (isASCIIDigit(*position) || *position == '.'))
        ++position;

    if (position == end) {
        parsedDelay = time;
        return true;
    }

    if (*position != ';' && *position != ',' && !isHTMLSpace(*position))
        return false;

    parsedDelay = time;

    while (position < end && isHTMLSpace(*position))
        ++position;

    if (position < end && (*position == ';' || *position == ','))
        ++position;

    while (position < end && isHTMLSpace(*position))
        ++position;

    if (position == end)
        return true;

    if (*position == 'U' || *position == 'u') {
        StringView url(position, end - position);

        ++position;

        if (position < end && (*position == 'R' || *position == 'r'))
            ++position;
        else {
            parsedURL = url.toString();
            return true;
        }

        if (position < end && (*position == 'L' || *position == 'l'))
            ++position;
        else {
            parsedURL = url.toString();
            return true;
        }

        while (position < end && isHTMLSpace(*position))
            ++position;

        if (position < end && *position == '=')
            ++position;
        else {
            parsedURL = url.toString();
            return true;
        }

        while (position < end && isHTMLSpace(*position))
            ++position;
    }

    CharacterType quote;
    if (position < end && (*position == '\'' || *position == '"')) {
        quote = *position;
        ++position;
    } else
        quote = '\0';

    StringView url(position, end - position);

    if (quote != '\0') {
        size_t index = url.find(quote);
        if (index != notFound)
            url = url.substring(0, index);
    }

    parsedURL = url.toString();
    return true;
}

bool parseMetaHTTPEquivRefresh(const StringView& input, double& delay, String& url)
{
    if (LIKELY(input.is8Bit())) {
        auto* start = input.characters8();
        return parseHTTPRefreshInternal(start, start + input.length(), delay, url);
    }

    auto* start = input.characters16();
    return parseHTTPRefreshInternal(start, start + input.length(), delay, url);
}

// https://html.spec.whatwg.org/#rules-for-parsing-a-hash-name-reference
AtomicString parseHTMLHashNameReference(StringView usemap)
{
    size_t numberSignIndex = usemap.find('#');
    if (numberSignIndex == notFound)
        return nullAtom();
    return usemap.substring(numberSignIndex + 1).toAtomicString();
}

}
