/*
 * Copyright (C) 2015 Apple, Inc.  All rights reserved.
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

#include <unicode/uchar.h>

namespace WebCore {

static inline bool isEmojiGroupCandidate(char32_t character)
{
#define SYMBOLS_AND_PICTOGRAPHS_EXTENDED_A 298
#if U_ICU_VERSION_MAJOR_NUM < 64
#define UBLOCK_SYMBOLS_AND_PICTOGRAPHS_EXTENDED_A SYMBOLS_AND_PICTOGRAPHS_EXTENDED_A
#else
static_assert(UBLOCK_SYMBOLS_AND_PICTOGRAPHS_EXTENDED_A == SYMBOLS_AND_PICTOGRAPHS_EXTENDED_A);
#endif

    switch (static_cast<int>(ublock_getCode(character))) {
    case UBLOCK_MISCELLANEOUS_SYMBOLS:
    case UBLOCK_DINGBATS:
    case UBLOCK_MISCELLANEOUS_SYMBOLS_AND_PICTOGRAPHS:
    case UBLOCK_EMOTICONS:
    case UBLOCK_TRANSPORT_AND_MAP_SYMBOLS:
    case UBLOCK_SUPPLEMENTAL_SYMBOLS_AND_PICTOGRAPHS:
    case UBLOCK_SYMBOLS_AND_PICTOGRAPHS_EXTENDED_A:
        return true;
    default:
        return false;
    }
}

static inline bool isEmojiFitzpatrickModifier(char32_t character)
{
    // U+1F3FB - EMOJI MODIFIER FITZPATRICK TYPE-1-2
    // U+1F3FC - EMOJI MODIFIER FITZPATRICK TYPE-3
    // U+1F3FD - EMOJI MODIFIER FITZPATRICK TYPE-4
    // U+1F3FE - EMOJI MODIFIER FITZPATRICK TYPE-5
    // U+1F3FF - EMOJI MODIFIER FITZPATRICK TYPE-6

    return character >= 0x1F3FB && character <= 0x1F3FF;
}

inline bool isVariationSelector(char32_t character)
{
    return character >= 0xFE00 && character <= 0xFE0F;
}

inline bool isEmojiKeycapBase(char32_t character)
{
    return (character >= '0' && character <= '9') || character == '#' || character == '*';
}

inline bool isEmojiRegionalIndicator(char32_t character)
{
    return character >= 0x1F1E6 && character <= 0x1F1FF;
}

inline bool isEmojiWithPresentationByDefault(char32_t character)
{
    return u_hasBinaryProperty(character, UCHAR_EMOJI_PRESENTATION);
}

inline bool isEmojiModifierBase(char32_t character)
{
    return u_hasBinaryProperty(character, UCHAR_EMOJI_MODIFIER_BASE);
}

inline bool isDefaultIgnorableCodePoint(char32_t character)
{
    return u_hasBinaryProperty(character, UCHAR_DEFAULT_IGNORABLE_CODE_POINT);
}

inline bool isControlCharacter(char32_t character)
{
    return u_charType(character) == U_CONTROL_CHAR;
}

inline bool isPrivateUseAreaCharacter(char32_t character)
{
    auto block = ublock_getCode(character);
    return block == UBLOCK_PRIVATE_USE_AREA || block == UBLOCK_SUPPLEMENTARY_PRIVATE_USE_AREA_A || block == UBLOCK_SUPPLEMENTARY_PRIVATE_USE_AREA_B;
}

}
