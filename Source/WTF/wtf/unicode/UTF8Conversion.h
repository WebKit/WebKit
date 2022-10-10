/*
 * Copyright (C) 2007-2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include <unicode/utypes.h>
#include <wtf/text/LChar.h>

namespace WTF {
namespace Unicode {

enum class ConversionResult : uint8_t {
    Success, // conversion successful
    SourceExhausted, // partial character in source, but hit end
    TargetExhausted, // insufficient room in target for conversion
    SourceIllegal // source sequence is illegal/malformed
};

// Conversion functions are strict, except for convertUTF16ToUTF8, which takes
// "strict" argument. When strict, both illegal sequences and unpaired surrogates
// will cause an error. When not, illegal sequences and unpaired surrogates are
// converted to the replacement character, except for an unpaired lead surrogate
// at the end of the source, which will instead cause a SourceExhausted error.

WTF_EXPORT_PRIVATE bool convertUTF8ToUTF16(const char* sourceStart, const char* sourceEnd, UChar** targetStart, UChar* targetEnd, bool* isSourceAllASCII = nullptr);
WTF_EXPORT_PRIVATE bool convertUTF8ToUTF16ReplacingInvalidSequences(const char* sourceStart, const char* sourceEnd, UChar** targetStart, UChar* targetEnd, bool* isSourceAllASCII = nullptr);
WTF_EXPORT_PRIVATE bool convertLatin1ToUTF8(const LChar** sourceStart, const LChar* sourceEnd, char** targetStart, char* targetEnd);
WTF_EXPORT_PRIVATE ConversionResult convertUTF16ToUTF8(const UChar** sourceStart, const UChar* sourceEnd, char** targetStart, char* targetEnd, bool strict = true);
WTF_EXPORT_PRIVATE unsigned calculateStringHashAndLengthFromUTF8MaskingTop8Bits(const char* data, const char* dataEnd, unsigned& dataLength, unsigned& utf16Length);

// Callers of these functions must check that the lengths are the same; accordingly we omit an end argument for UTF-16 and Latin-1.
bool equalUTF16WithUTF8(const UChar* stringInUTF16, const char* stringInUTF8, const char* stringInUTF8End);
bool equalLatin1WithUTF8(const LChar* stringInLatin1, const char* stringInUTF8, const char* stringInUTF8End);

} // namespace Unicode
} // namespace WTF
