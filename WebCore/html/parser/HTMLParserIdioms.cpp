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
        return emptyAtom;

    unsigned numTrailingSpaces;
    for (numTrailingSpaces = 0; numTrailingSpaces < length; ++numTrailingSpaces) {
        if (isNotHTMLSpace(characters[length - numTrailingSpaces - 1]))
            break;
    }

    ASSERT(numLeadingSpaces + numTrailingSpaces < length);

    return string.substring(numLeadingSpaces, length - numTrailingSpaces);
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

}
