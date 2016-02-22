/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef HTMLParserIdioms_h
#define HTMLParserIdioms_h

#include <unicode/uchar.h>
#include <wtf/Forward.h>

namespace WebCore {

class Decimal;
class QualifiedName;

// Space characters as defined by the HTML specification.
template<typename CharacterType> bool isHTMLSpace(CharacterType);
template<typename CharacterType> bool isComma(CharacterType);
template<typename CharacterType> bool isHTMLSpaceOrComma(CharacterType);
bool isHTMLLineBreak(UChar);
bool isNotHTMLSpace(UChar);
bool isHTMLSpaceButNotLineBreak(UChar);

// Strip leading and trailing whitespace as defined by the HTML specification. 
WEBCORE_EXPORT String stripLeadingAndTrailingHTMLSpaces(const String&);

// An implementation of the HTML specification's algorithm to convert a number to a string for number and range types.
String serializeForNumberType(const Decimal&);
String serializeForNumberType(double);

// Convert the specified string to a decimal/double. If the conversion fails, the return value is fallback value or NaN if not specified.
// Leading or trailing illegal characters cause failure, as does passing an empty string.
// The double* parameter may be 0 to check if the string can be parsed without getting the result.
Decimal parseToDecimalForNumberType(const String&);
Decimal parseToDecimalForNumberType(const String&, const Decimal& fallbackValue);
double parseToDoubleForNumberType(const String&);
double parseToDoubleForNumberType(const String&, double fallbackValue);

// http://www.whatwg.org/specs/web-apps/current-work/#rules-for-parsing-integers
bool parseHTMLInteger(const String&, int&);

// http://www.whatwg.org/specs/web-apps/current-work/#rules-for-parsing-non-negative-integers
bool parseHTMLNonNegativeInteger(const String&, unsigned int&);

bool threadSafeMatch(const QualifiedName&, const QualifiedName&);

// Inline implementations of some of the functions declared above.

template<typename CharacterType> inline bool isHTMLSpace(CharacterType character)
{
    // Histogram from Apple's page load test combined with some ad hoc browsing some other test suites.
    //
    //     82%: 216330 non-space characters, all > U+0020
    //     11%:  30017 plain space characters, U+0020
    //      5%:  12099 newline characters, U+000A
    //      2%:   5346 tab characters, U+0009
    //
    // No other characters seen. No U+000C or U+000D, and no other control characters.
    // Accordingly, we check for non-spaces first, then space, then newline, then tab, then the other characters.

    return character <= ' ' && (character == ' ' || character == '\n' || character == '\t' || character == '\r' || character == '\f');
}

inline bool isHTMLLineBreak(UChar character)
{
    return character <= '\r' && (character == '\n' || character == '\r');
}

template<typename CharacterType> inline bool isComma(CharacterType character)
{
    return character == ',';
}

template<typename CharacterType> inline bool isHTMLSpaceOrComma(CharacterType character)
{
    return isComma(character) || isHTMLSpace(character);
}

inline bool isNotHTMLSpace(UChar character)
{
    return !isHTMLSpace(character);
}

inline bool isHTMLSpaceButNotLineBreak(UChar character)
{
    return isHTMLSpace(character) && !isHTMLLineBreak(character);
}

// https://html.spec.whatwg.org/multipage/infrastructure.html#limited-to-only-non-negative-numbers-greater-than-zero
inline unsigned limitToOnlyHTMLNonNegativeNumbersGreaterThanZero(unsigned value, unsigned defaultValue = 1)
{
    return (value > 0 && value <= 2147483647) ? value : defaultValue;
}

inline unsigned limitToOnlyHTMLNonNegativeNumbersGreaterThanZero(const String& stringValue, unsigned defaultValue = 1)
{
    unsigned value = defaultValue;
    parseHTMLNonNegativeInteger(stringValue, value);
    return limitToOnlyHTMLNonNegativeNumbersGreaterThanZero(value, defaultValue);
}

// https://html.spec.whatwg.org/#reflecting-content-attributes-in-idl-attributes:idl-unsigned-long
inline unsigned limitToOnlyHTMLNonNegative(unsigned value, unsigned defaultValue = 0)
{
    return value <= 2147483647 ? value : defaultValue;
}

inline unsigned limitToOnlyHTMLNonNegative(const String& stringValue, unsigned defaultValue = 0)
{
    unsigned value = defaultValue;
    parseHTMLNonNegativeInteger(stringValue, value);
    return limitToOnlyHTMLNonNegative(value, defaultValue);
}

}

#endif
