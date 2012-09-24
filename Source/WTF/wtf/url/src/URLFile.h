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

// Provides shared functions used by the internals of the parser and
// canonicalizer for file URLs. Do not use outside of these modules.

#ifndef URLFile_h
#define URLFile_h

#include "URLParseInternal.h"
#include <wtf/unicode/Unicode.h>

#if USE(WTFURL)

namespace WTF {

namespace URLParser {

#if OS(WINDOWS)
// We allow both "c:" and "c|" as drive identifiers.
inline bool isWindowsDriveSeparator(UChar character)
{
    return character == ':' || character == '|';
}
inline bool isWindowsDriveLetter(UChar character)
{
    return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z');
}
#endif // OS(WINDOWS)

// Returns the index of the next slash in the input after the given index, or
// specLength if the end of the input is reached.
template<typename CharacterType>
inline int findNextSlash(const CharacterType* spec, int beginIndex, int specLength)
{
    int idx = beginIndex;
    while (idx < specLength && !isURLSlash(spec[idx]))
        ++idx;
    return idx;
}

#if OS(WINDOWS)
// Returns true if the startOffset in the given spec looks like it begins a
// drive spec, for example "c:". This function explicitly handles startOffset
// values that are equal to or larger than the specLength to simplify callers.
//
// If this returns true, the spec is guaranteed to have a valid drive letter
// plus a colon starting at |startOffset|.
template<typename CharacterType>
inline bool doesBeginWindowsDriveSpec(const CharacterType* spec, int startOffset, int specLength)
{
    int remainingLength = specLength - startOffset;
    if (remainingLength < 2)
        return false; // Not enough room.
    if (!isWindowsDriveLetter(spec[startOffset]))
        return false; // Doesn't start with a valid drive letter.
    if (!isWindowsDriveSeparator(spec[startOffset + 1]))
        return false; // Isn't followed with a drive separator.
    return true;
}

// Returns true if the startOffset in the given text looks like it begins a
// UNC path, for example "\\". This function explicitly handles startOffset
// values that are equal to or larger than the specLength to simplify callers.
//
// When strictSlashes is set, this function will only accept backslashes as is
// standard for Windows. Otherwise, it will accept forward slashes as well
// which we use for a lot of URL handling.
template<typename CharacterType>
inline bool doesBeginUNCPath(const CharacterType* text, int startOffset, int length, bool strictSlashes)
{
    int remainingLength = length - startOffset;
    if (remainingLength < 2)
        return false;

    if (strictSlashes)
        return text[startOffset] == '\\' && text[startOffset + 1] == '\\';
    return isURLSlash(text[startOffset]) && isURLSlash(text[startOffset + 1]);
}
#endif // OS(WINDOWS)

} // namespace URLParser

} // namespace WTF

#endif // USE(WTFURL)

#endif // URLFile_h
