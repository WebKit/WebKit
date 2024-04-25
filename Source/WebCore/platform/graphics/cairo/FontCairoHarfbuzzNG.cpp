/*
 * Copyright (C) 2006-2023 Apple Inc. All rights reserved.
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

#include "FontCache.h"
#include "SurrogatePairAwareTextIterator.h"
#include <wtf/text/CharacterProperties.h>

namespace WebCore {

bool FontCascade::canReturnFallbackFontsForComplexText()
{
    return false;
}

bool FontCascade::canExpandAroundIdeographsInComplexText()
{
    return false;
}

static bool characterSequenceIsEmoji(SurrogatePairAwareTextIterator& iterator, char32_t firstCharacter, unsigned firstClusterLength)
{
    char32_t character = firstCharacter;
    unsigned clusterLength = firstClusterLength;
    if (!iterator.consume(character, clusterLength))
        return false;

    if (isEmojiKeycapBase(character)) {
        iterator.advance(clusterLength);
        char32_t nextCharacter;
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
        char32_t nextCharacter;
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

const Font* FontCascade::fontForCombiningCharacterSequence(StringView stringView) const
{
    auto normalizedString = normalizedNFC(stringView);

    // Code below relies on normalizedNFC never narrowing a 16-bit input string into an 8-bit output string.
    // At the time of this writing, the function never does this, but in theory a future version could, and
    // we would then need to add code paths here for the simpler 8-bit case.
    auto characters = normalizedString.view.span16();
    auto length = normalizedString.view.length();

    char32_t character;
    unsigned clusterLength = 0;
    SurrogatePairAwareTextIterator iterator(characters, 0, length);
    if (!iterator.consume(character, clusterLength))
        return nullptr;

    bool isEmoji = characterSequenceIsEmoji(iterator, character, clusterLength);
    bool preferColoredFont = isEmoji;
    // U+FE0E forces text style.
    // U+FE0F forces emoji style.
    if (characters[length - 1] == 0xFE0E)
        preferColoredFont = false;
    else if (characters[length - 1] == 0xFE0F)
        preferColoredFont = true;

    RefPtr baseFont = glyphDataForCharacter(character, false, NormalVariant).font.get();
    if (baseFont
        && (clusterLength == length || baseFont->canRenderCombiningCharacterSequence(normalizedString.view))
        && (!preferColoredFont || baseFont->platformData().isColorBitmapFont()))
        return baseFont.get();

    for (unsigned i = 0; !fallbackRangesAt(i).isNull(); ++i) {
        RefPtr fallbackFont = fallbackRangesAt(i).fontForCharacter(character);
        if (!fallbackFont || fallbackFont == baseFont)
            continue;

        if (fallbackFont->canRenderCombiningCharacterSequence(normalizedString.view) && (!preferColoredFont || fallbackFont->platformData().isColorBitmapFont()))
            return fallbackFont.get();
    }

    const auto& originalFont = fallbackRangesAt(0).fontForFirstRange();
    if (auto systemFallback = FontCache::forCurrentThread().systemFallbackForCharacterCluster(m_fontDescription, originalFont, IsForPlatformFont::No, preferColoredFont ? FontCache::PreferColoredFont::Yes : FontCache::PreferColoredFont::No, normalizedString.view)) {
        if (systemFallback->canRenderCombiningCharacterSequence(normalizedString.view) && (!preferColoredFont || systemFallback->platformData().isColorBitmapFont()))
            return systemFallback.get();

        // In case of emoji, if fallback font is colored try again without the variation selector character.
        if (isEmoji && characters[length - 1] == 0xFE0F && systemFallback->platformData().isColorBitmapFont() && systemFallback->canRenderCombiningCharacterSequence(characters.first(length - 1)))
            return systemFallback.get();
    }

    return baseFont.get();
}

} // namespace WebCore

#endif // USE(CAIRO)
