/*
 * Copyright (C) 2007-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
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

#include "config.h"
#include <wtf/unicode/UTF8Conversion.h>

#include <wtf/ASCIICType.h>
#include <wtf/text/StringHasher.h>
#include <wtf/unicode/CharacterNames.h>

namespace WTF {
namespace Unicode {

bool convertLatin1ToUTF8(const LChar** sourceStart, const LChar* sourceEnd, char** targetStart, char* targetEnd)
{
    const LChar* source;
    char* target = *targetStart;
    int i = 0;
    for (source = *sourceStart; source < sourceEnd; ++source) {
        UBool sawError = false;
        // Work around bug in either Windows compiler or old version of ICU, where passing a uint8_t to
        // U8_APPEND warns, by converting from uint8_t to a wider type.
        UChar32 character = *source;
        U8_APPEND(reinterpret_cast<uint8_t*>(target), i, targetEnd - *targetStart, character, sawError);
        if (sawError)
            return false;
    }
    *sourceStart = source;
    *targetStart = target + i;
    return true;
}

ConversionResult convertUTF16ToUTF8(const UChar** sourceStart, const UChar* sourceEnd, char** targetStart, char* targetEnd, bool strict)
{
    ConversionResult result = ConversionOK;
    const UChar* source = *sourceStart;
    char* target = *targetStart;
    UBool sawError = false;
    int i = 0;
    while (source < sourceEnd) {
        UChar32 ch;
        int j = 0;
        U16_NEXT(source, j, sourceEnd - source, ch);
        if (U_IS_SURROGATE(ch)) {
            if (source + j == sourceEnd && U_IS_SURROGATE_LEAD(ch)) {
                result = SourceExhausted;
                break;
            }
            if (strict) {
                result = SourceIllegal;
                break;
            }
            ch = replacementCharacter;
        }
        U8_APPEND(reinterpret_cast<uint8_t*>(target), i, targetEnd - target, ch, sawError);
        if (sawError) {
            result = TargetExhausted;
            break;
        }
        source += j;
    }
    *sourceStart = source;
    *targetStart = target + i;
    return result;
}

bool convertUTF8ToUTF16(const char* source, const char* sourceEnd, UChar** targetStart, UChar* targetEnd, bool* sourceAllASCII)
{
    RELEASE_ASSERT(sourceEnd - source <= std::numeric_limits<int>::max());
    UBool error = false;
    UChar* target = *targetStart;
    UChar32 orAllData = 0;
    int targetOffset = 0;
    for (int sourceOffset = 0; sourceOffset < sourceEnd - source; ) {
        UChar32 character;
        U8_NEXT(reinterpret_cast<const uint8_t*>(source), sourceOffset, sourceEnd - source, character);
        if (character < 0)
            return false;
        U16_APPEND(target, targetOffset, targetEnd - target, character, error);
        if (error)
            return false;
        orAllData |= character;
    }
    *targetStart = target + targetOffset;
    if (sourceAllASCII)
        *sourceAllASCII = isASCII(orAllData);
    return true;
}

unsigned calculateStringHashAndLengthFromUTF8MaskingTop8Bits(const char* data, const char* dataEnd, unsigned& dataLength, unsigned& utf16Length)
{
    StringHasher stringHasher;
    utf16Length = 0;

    int inputOffset = 0;
    int inputLength = dataEnd - data;
    while (inputOffset < inputLength) {
        UChar32 character;
        U8_NEXT(reinterpret_cast<const uint8_t*>(data), inputOffset, inputLength, character);
        if (character < 0)
            return 0;

        if (U_IS_BMP(character)) {
            ASSERT(!U_IS_SURROGATE(character));
            stringHasher.addCharacter(character);
            utf16Length++;
        } else {
            ASSERT(U_IS_SUPPLEMENTARY(character));
            stringHasher.addCharacters(U16_LEAD(character), U16_TRAIL(character));
            utf16Length += 2;
        }
    }

    dataLength = inputOffset;
    return stringHasher.hashWithTop8BitsMasked();
}

bool equalUTF16WithUTF8(const UChar* a, const char* b, const char* bEnd)
{
    while (b < bEnd) {
        int offset = 0;
        UChar32 character;
        U8_NEXT(reinterpret_cast<const uint8_t*>(b), offset, bEnd - b, character);
        if (character < 0)
            return false;
        b += offset;

        if (U_IS_BMP(character)) {
            ASSERT(!U_IS_SURROGATE(character));
            if (*a++ != character)
                return false;
        } else {
            ASSERT(U_IS_SUPPLEMENTARY(character));
            if (*a++ != U16_LEAD(character))
                return false;
            if (*a++ != U16_TRAIL(character))
                return false;
        }
    }

    return true;
}

bool equalLatin1WithUTF8(const LChar* a, const char* b, const char* bEnd)
{
    while (b < bEnd) {
        if (isASCII(*a) || isASCII(*b)) {
            if (*a++ != *b++)
                return false;
            continue;
        }

        if (b + 1 == bEnd)
            return false;

        if ((b[0] & 0xE0) != 0xC0 || (b[1] & 0xC0) != 0x80)
            return false;

        LChar character = ((b[0] & 0x1F) << 6) | (b[1] & 0x3F);

        b += 2;

        if (*a++ != character)
            return false;
    }

    return true;
}

} // namespace Unicode
} // namespace WTF
