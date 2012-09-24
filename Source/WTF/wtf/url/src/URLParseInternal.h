/*
 * Copyright 2007 Google Inc. All rights reserved.
 * Copyright 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Contains common inline helper functions used by the URL parsing routines.

#ifndef URLParseInternal_h
#define URLParseInternal_h

#include "URLParse.h"
#include <wtf/unicode/Unicode.h>

#if USE(WTFURL)

namespace WTF {

namespace URLParser {

// We treat slashes and backslashes the same for IE compatability.
inline bool isURLSlash(UChar ch)
{
    return ch == '/' || ch == '\\';
}

// Returns true if we should trim this character from the URL because it is a
// space or a control character.
inline bool shouldTrimFromURL(UChar ch)
{
    return ch <= ' ';
}

// Given an already-initialized begin index and length, this shrinks the range
// to eliminate "should-be-trimmed" characters. Note that the length does *not*
// indicate the length of untrimmed data from |*begin|, but rather the position
// in the input string (so the string starts at character |*begin| in the spec,
// and goes until |*len|).
template<typename CharacterType>
inline void trimURL(const CharacterType* spec, int& begin, int& length)
{
    // Strip leading whitespace and control characters.
    while (begin < length && shouldTrimFromURL(spec[begin]))
        ++begin;

    // Strip trailing whitespace and control characters. We need the >i test for
    // when the input string is all blanks; we don't want to back past the input.
    while (length > begin && shouldTrimFromURL(spec[length - 1]))
        --length;
}

// Counts the number of consecutive slashes starting at the given offset
// in the given string of the given length.
template<typename CharacterType>
inline int countConsecutiveSlashes(const CharacterType *str, int begin_offset, int strLength)
{
    int count = 0;
    while (begin_offset + count < strLength && isURLSlash(str[begin_offset + count]))
        ++count;
    return count;
}

// Internal functions in URLParser.cc that parse the path, that is, everything
// following the authority section. The input is the range of everything
// following the authority section, and the output is the identified ranges.
//
// This is designed for the file URL parser or other consumers who may do
// special stuff at the beginning, but want regular path parsing, it just
// maps to the internal parsing function for paths.
void parsePathInternal(const char* spec,
                       const URLComponent& path,
                       URLComponent* filepath,
                       URLComponent* query,
                       URLComponent* fragment);
void parsePathInternal(const UChar* spec,
                       const URLComponent& path,
                       URLComponent* filepath,
                       URLComponent* query,
                       URLComponent* fragment);


// Given a spec and a pointer to the character after the colon following the
// scheme, this parses it and fills in the structure, Every item in the parsed
// structure is filled EXCEPT for the scheme, which is untouched.
void parseAfterScheme(const char* spec, int specLength, int afterScheme, URLSegments& parsed);
void parseAfterScheme(const UChar* spec, int specLength, int afterScheme, URLSegments& parsed);

} // namespace URLParser

} // namespace WTF

#endif // USE(WTFURL)

#endif // URLParseInternal_h
