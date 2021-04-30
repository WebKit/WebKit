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

namespace WebCore {

bool FontCascade::canReturnFallbackFontsForComplexText()
{
    return false;
}

bool FontCascade::canExpandAroundIdeographsInComplexText()
{
    return false;
}

static bool characterSequenceIsEmoji(SurrogatePairAwareTextIterator& iterator, UChar32 firstCharacter, unsigned firstClusterLength)
{
    UChar32 character = firstCharacter;
    unsigned clusterLength = firstClusterLength;
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

const Font* FontCascade::fontForCombiningCharacterSequence(const UChar* originalCharacters, size_t originalLength) const
{
    auto normalizedString = normalizedNFC(StringView { originalCharacters, static_cast<unsigned>(originalLength) });

    // Code below relies on normalizedNFC never narrowing a 16-bit input string into an 8-bit output string.
    // At the time of this writing, the function never does this, but in theory a future version could, and
    // we would then need to add code paths here for the simpler 8-bit case.
    auto characters = normalizedString.view.characters16();
    auto length = normalizedString.view.length();

    UChar32 character;
    unsigned clusterLength = 0;
    SurrogatePairAwareTextIterator iterator(characters, 0, length, length);
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

    const Font* baseFont = glyphDataForCharacter(character, false, NormalVariant).font;
    if (baseFont
        && (clusterLength == length || baseFont->canRenderCombiningCharacterSequence(characters, length))
        && (!preferColoredFont || baseFont->platformData().isColorBitmapFont()))
        return baseFont;

    for (unsigned i = 0; !fallbackRangesAt(i).isNull(); ++i) {
        const Font* fallbackFont = fallbackRangesAt(i).fontForCharacter(character);
        if (!fallbackFont || fallbackFont == baseFont)
            continue;

        if (fallbackFont->canRenderCombiningCharacterSequence(characters, length) && (!preferColoredFont || fallbackFont->platformData().isColorBitmapFont()))
            return fallbackFont;
    }

    if (auto systemFallback = FontCache::fontCacheFallingBackToSingleton(fontSelector()).systemFallbackForCharacters(m_fontDescription, baseFont, IsForPlatformFont::No, preferColoredFont ? FontCache::PreferColoredFont::Yes : FontCache::PreferColoredFont::No, characters, length)) {
        if (systemFallback->canRenderCombiningCharacterSequence(characters, length) && (!preferColoredFont || systemFallback->platformData().isColorBitmapFont()))
            return systemFallback.get();

        // In case of emoji, if fallback font is colored try again without the variation selector character.
        if (isEmoji && characters[length - 1] == 0xFE0F && systemFallback->platformData().isColorBitmapFont() && systemFallback->canRenderCombiningCharacterSequence(characters, length - 1))
            return systemFallback.get();
    }

    return baseFont;
}

} // namespace WebCore

#endif // USE(CAIRO)
