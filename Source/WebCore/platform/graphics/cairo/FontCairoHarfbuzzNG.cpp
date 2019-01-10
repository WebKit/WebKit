/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Intel Corporation
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
#include "FontCascade.h"

#if USE(CAIRO)

#include "CharacterProperties.h"
#include "FontCache.h"
#include "SurrogatePairAwareTextIterator.h"
#include <unicode/normlzr.h>

namespace WebCore {

bool FontCascade::canReturnFallbackFontsForComplexText()
{
    return false;
}

bool FontCascade::canExpandAroundIdeographsInComplexText()
{
    return false;
}

static bool characterSequenceIsEmoji(const Vector<UChar, 4>& normalizedCharacters, int32_t normalizedLength)
{
    UChar32 character;
    unsigned clusterLength = 0;
    SurrogatePairAwareTextIterator iterator(normalizedCharacters.data(), 0, normalizedLength, normalizedLength);
    if (!iterator.consume(character, clusterLength))
        return false;

    if (isEmojiKeycapBase(character)) {
        iterator.advance(clusterLength);
        UChar32 nextCharacter;
        if (!iterator.consume(nextCharacter, clusterLength))
            return false;

        if (nextCharacter == combiningEnclosingKeycap)
            return true;

        // Variation selector 16.
        if (nextCharacter == 0xFE0F) {
            iterator.advance(clusterLength);
            if (!iterator.consume(nextCharacter, clusterLength))
                return false;

            if (nextCharacter == combiningEnclosingKeycap)
                return true;
        }

        return false;
    }

    // Regional indicator.
    if (isEmojiRegionalIndicator(character)) {
        iterator.advance(clusterLength);
        UChar32 nextCharacter;
        if (!iterator.consume(nextCharacter, clusterLength))
            return false;

        if (isEmojiRegionalIndicator(nextCharacter))
            return true;

        return false;
    }

    if (character == combiningEnclosingKeycap)
        return true;

    if (isEmojiWithPresentationByDefault(character)
        || isEmojiModifierBase(character)
        || isEmojiFitzpatrickModifier(character))
        return true;

    return false;
}

const Font* FontCascade::fontForCombiningCharacterSequence(const UChar* characters, size_t length) const
{
    UErrorCode error = U_ZERO_ERROR;
    Vector<UChar, 4> normalizedCharacters(length);
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    int32_t normalizedLength = unorm_normalize(characters, length, UNORM_NFC, UNORM_UNICODE_3_2, normalizedCharacters.data(), length, &error);
    ALLOW_DEPRECATED_DECLARATIONS_END
    if (U_FAILURE(error))
        return nullptr;

    UChar32 character;
    unsigned clusterLength = 0;
    SurrogatePairAwareTextIterator iterator(normalizedCharacters.data(), 0, normalizedLength, normalizedLength);
    if (!iterator.consume(character, clusterLength))
        return nullptr;

    bool isEmoji = characterSequenceIsEmoji(normalizedCharacters, normalizedLength);

    const Font* baseFont = glyphDataForCharacter(character, false, NormalVariant).font;
    if (baseFont
        && (static_cast<int32_t>(clusterLength) == normalizedLength || baseFont->canRenderCombiningCharacterSequence(characters, length))
        && (!isEmoji || baseFont->platformData().isColorBitmapFont()))
        return baseFont;

    for (unsigned i = 0; !fallbackRangesAt(i).isNull(); ++i) {
        const Font* fallbackFont = fallbackRangesAt(i).fontForCharacter(character);
        if (!fallbackFont || fallbackFont == baseFont)
            continue;

        if (fallbackFont->canRenderCombiningCharacterSequence(characters, length) && (!isEmoji || fallbackFont->platformData().isColorBitmapFont()))
            return fallbackFont;
    }

    if (auto systemFallback = FontCache::singleton().systemFallbackForCharacters(m_fontDescription, baseFont, IsForPlatformFont::No, isEmoji ? FontCache::PreferColoredFont::Yes : FontCache::PreferColoredFont::No, characters, length)) {
        if (systemFallback->canRenderCombiningCharacterSequence(characters, length) && (!isEmoji || systemFallback->platformData().isColorBitmapFont()))
            return systemFallback.get();
    }

    return baseFont;
}

} // namespace WebCore

#endif // USE(CAIRO)
