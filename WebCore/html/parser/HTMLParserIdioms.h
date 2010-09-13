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

#include <wtf/Forward.h>
#include <wtf/unicode/Unicode.h>

namespace WebCore {

// Space characters as defined by the HTML specification.
bool isHTMLSpace(UChar);
bool isNotHTMLSpace(UChar);

// Strip leading and trailing whitespace as defined by the HTML specification. 
String stripLeadingAndTrailingHTMLSpaces(const String&);

// An implementation of the HTML specification's algorithm to convert a number to a string for number and range types.
String serializeForNumberType(double);

// Convert the specified string to a double. If the conversion fails, the return value is false.
// Leading or trailing illegal characters cause failure, as does passing an empty string.
// The double* parameter may be 0 to check if the string can be parsed without getting the result.
bool parseToDoubleForNumberType(const String&, double*);

// Inline implementations of some of the functions declared above.

inline bool isHTMLSpace(UChar character)
{
    // FIXME: Consider branch permutations as we did in isASCIISpace.
    return character == '\t' || character == '\x0A' || character == '\x0C' || character == '\x0D' || character == ' ';
}

inline bool isNotHTMLSpace(UChar character)
{
    return !isHTMLSpace(character);
}

}

#endif
