/*
 * Copyright (C) 2007-2023 Apple Inc. All rights reserved.
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

#include <unicode/uchar.h>
#include <wtf/ASCIICType.h>
#include <wtf/text/StringHasherInlines.h>
#include <wtf/unicode/CharacterNames.h>

namespace WTF::Unicode {

static constexpr char32_t sentinelCodePoint = U_SENTINEL;

bool convertLatin1ToUTF8(const LChar** sourceStart, const LChar* sourceEnd, char** targetStart, const char* targetEnd)
{
    const LChar* source;
    char* target = *targetStart;
    int32_t i = 0;
    for (source = *sourceStart; source < sourceEnd; ++source) {
        UBool sawError = false;
        // Work around bug in either Windows compiler or old version of ICU, where passing a uint8_t to
        // U8_APPEND warns, by converting from uint8_t to a wider type.
        char32_t character = *source;
        U8_APPEND(target, i, targetEnd - *targetStart, character, sawError);
        ASSERT_WITH_MESSAGE(!sawError, "UTF8 destination buffer was not big enough");
        if (sawError)
            return false;
    }
    *sourceStart = source;
    *targetStart = target + i;
    return true;
}

ConversionResult convertUTF16ToUTF8(const UChar** sourceStart, const UChar* sourceEnd, char** targetStart, const char* targetEnd, bool strict)
{
    auto result = ConversionResult::Success;
    const UChar* source = *sourceStart;
    char* target = *targetStart;
    UBool sawError = false;
    int32_t i = 0;
    while (source < sourceEnd) {
        char32_t ch;
        int j = 0;
        U16_NEXT(source, j, sourceEnd - source, ch);
        if (U_IS_SURROGATE(ch)) {
            if (source + j == sourceEnd && U_IS_SURROGATE_LEAD(ch)) {
                result = ConversionResult::SourceExhausted;
                break;
            }
            if (strict) {
                result = ConversionResult::SourceIllegal;
                break;
            }
            ch = replacementCharacter;
        }
        U8_APPEND(reinterpret_cast<uint8_t*>(target), i, targetEnd - target, ch, sawError);
        if (sawError) {
            result = ConversionResult::TargetExhausted;
            break;
        }
        source += j;
    }
    *sourceStart = source;
    *targetStart = target + i;
    return result;
}

template<bool replaceInvalidSequences>
bool convertUTF8ToUTF16Impl(const char* source, const char* sourceEnd, UChar** targetStart, const UChar* targetEnd, bool* sourceAllASCII)
{
    RELEASE_ASSERT(sourceEnd - source <= std::numeric_limits<int>::max());
    UBool error = false;
    UChar* target = *targetStart;
    RELEASE_ASSERT(targetEnd - target <= std::numeric_limits<int>::max());
    char32_t orAllData = 0;
    int targetOffset = 0;
    for (int sourceOffset = 0; sourceOffset < sourceEnd - source; ) {
        char32_t character;
        if constexpr (replaceInvalidSequences) {
            U8_NEXT_OR_FFFD(source, sourceOffset, sourceEnd - source, character);
        } else {
            U8_NEXT(source, sourceOffset, sourceEnd - source, character);
            if (character == sentinelCodePoint)
                return false;
        }
        U16_APPEND(target, targetOffset, targetEnd - target, character, error);
        if (error)
            return false;
        orAllData |= character;
    }
    RELEASE_ASSERT(target + targetOffset <= targetEnd);
    *targetStart = target + targetOffset;
    if (sourceAllASCII)
        *sourceAllASCII = isASCII(orAllData);
    return true;
}

bool convertUTF8ToUTF16(const char* source, const char* sourceEnd, UChar** targetStart, const UChar* targetEnd, bool* sourceAllASCII)
{
    return convertUTF8ToUTF16Impl<false>(source, sourceEnd, targetStart, targetEnd, sourceAllASCII);
}

bool convertUTF8ToUTF16ReplacingInvalidSequences(const char* source, const char* sourceEnd, UChar** targetStart, const UChar* targetEnd, bool* sourceAllASCII)
{
    return convertUTF8ToUTF16Impl<true>(source, sourceEnd, targetStart, targetEnd, sourceAllASCII);
}

ComputeUTFLengthsResult computeUTFLengths(const char* sourceStart, const char* sourceEnd)
{
    size_t lengthUTF8 = sourceEnd - sourceStart;
    size_t lengthUTF16 = 0;
    char32_t orAllData = 0;
    ConversionResult result = ConversionResult::Success;
    size_t sourceOffset = 0;
    while (sourceOffset < lengthUTF8) {
        char32_t character;
        size_t nextSourceOffset = sourceOffset;
        U8_NEXT(sourceStart, nextSourceOffset, lengthUTF8, character);
        if (character == sentinelCodePoint) {
            result = nextSourceOffset == lengthUTF8 ? ConversionResult::SourceExhausted : ConversionResult::SourceIllegal;
            break;
        }
        sourceOffset = nextSourceOffset;
        lengthUTF16 += U16_LENGTH(character);
        orAllData |= character;
    }
    return { result, sourceOffset, lengthUTF16, isASCII(orAllData) };
}

unsigned calculateStringHashAndLengthFromUTF8MaskingTop8Bits(const char* data, const char* dataEnd, unsigned& dataLength, unsigned& utf16Length)
{
    StringHasher stringHasher;
    utf16Length = 0;
    size_t inputOffset = 0;
    size_t inputLength = dataEnd - data;
    while (inputOffset < inputLength) {
        char32_t character;
        U8_NEXT(data, inputOffset, inputLength, character);
        if (character == sentinelCodePoint)
            return 0;
        if (U_IS_BMP(character)) {
            ASSERT(!U_IS_SURROGATE(character));
            stringHasher.addCharacter(character);
            utf16Length++;
        } else {
            ASSERT(U_IS_SUPPLEMENTARY(character));
            stringHasher.addCharacter(U16_LEAD(character));
            stringHasher.addCharacter(U16_TRAIL(character));
            utf16Length += 2;
        }
    }
    dataLength = inputOffset;
    return stringHasher.hashWithTop8BitsMasked();
}

bool equalUTF16WithUTF8(const UChar* a, const char* b, const char* bEnd)
{
    // It is the caller's responsibility to ensure a is long enough, which is why it is safe to use U16_NEXT_UNSAFE here.
    size_t offsetA = 0;
    size_t offsetB = 0;
    size_t lengthB = bEnd - b;
    while (offsetB < lengthB) {
        char32_t characterB;
        U8_NEXT(b, offsetB, lengthB, characterB);
        if (characterB == sentinelCodePoint)
            return false;
        char16_t characterA;
        U16_NEXT_UNSAFE(a, offsetA, characterA);
        if (characterB != characterA)
            return false;
    }
    return true;
}

bool equalLatin1WithUTF8(const LChar* a, const char* b, const char* bEnd)
{
    // It is the caller's responsibility to ensure a is long enough, which is why it is safe to use *a++ here.
    size_t offsetB = 0;
    size_t lengthB = bEnd - b;
    while (offsetB < lengthB) {
        char32_t characterB;
        U8_NEXT(b, offsetB, lengthB, characterB);
        if (*a++ != characterB)
            return false;
    }
    return true;
}

} // namespace WTF::Unicode
