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

#include "config.h"
#include "HTMLParserIdioms.h"

#include <wtf/MathExtras.h>
#include <wtf/dtoa.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

String stripLeadingAndTrailingHTMLSpaces(const String& string)
{
    const UChar* characters = string.characters();
    unsigned length = string.length();

    unsigned numLeadingSpaces;
    for (numLeadingSpaces = 0; numLeadingSpaces < length; ++numLeadingSpaces) {
        if (isNotHTMLSpace(characters[numLeadingSpaces]))
            break;
    }

    if (numLeadingSpaces == length)
        return string.isNull() ? string : emptyAtom.string();

    unsigned numTrailingSpaces;
    for (numTrailingSpaces = 0; numTrailingSpaces < length; ++numTrailingSpaces) {
        if (isNotHTMLSpace(characters[length - numTrailingSpaces - 1]))
            break;
    }

    ASSERT(numLeadingSpaces + numTrailingSpaces < length);

    return string.substring(numLeadingSpaces, length - (numLeadingSpaces + numTrailingSpaces));
}

String serializeForNumberType(double number)
{
    // According to HTML5, "the best representation of the number n as a floating
    // point number" is a string produced by applying ToString() to n.
    NumberToStringBuffer buffer;
    unsigned length = numberToString(number, buffer);
    return String(buffer, length);
}

bool parseToDoubleForNumberType(const String& string, double* result)
{
    // See HTML5 2.4.4.3 `Real numbers.'

    // String::toDouble() accepts leading + and whitespace characters, which are not valid here.
    UChar firstCharacter = string[0];
    if (firstCharacter != '-' && !isASCIIDigit(firstCharacter))
        return false;

    bool valid = false;
    double value = string.toDouble(&valid);
    if (!valid)
        return false;

    // NaN and infinity are considered valid by String::toDouble, but not valid here.
    if (!isfinite(value))
        return false;

    if (result) {
        // The following expression converts -0 to +0.
        *result = value ? value : 0;
    }

    return true;
}

// http://www.whatwg.org/specs/web-apps/current-work/#rules-for-parsing-integers
bool parseHTMLInteger(const String& input, int& value)
{
    // Step 1
    // Step 2
    const UChar* position = input.characters();
    const UChar* end = position + input.length();

    // Step 3
    int sign = 1;

    // Step 4
    while (position < end) {
        if (!isHTMLSpace(*position))
            break;
        ++position;
    }

    // Step 5
    if (position == end)
        return false;
    ASSERT(position < end);

    // Step 6
    if (*position == '-') {
        sign = -1;
        ++position;
    } else if (*position == '+')
        ++position;
    if (position == end)
        return false;
    ASSERT(position < end);

    // Step 7
    if (!isASCIIDigit(*position))
        return false;

    // Step 8
    Vector<UChar, 16> digits;
    while (position < end) {
        if (!isASCIIDigit(*position))
            break;
        digits.append(*position++);
    }

    // Step 9
    value = sign * charactersToIntStrict(digits.data(), digits.size());
    return true;
}

}
