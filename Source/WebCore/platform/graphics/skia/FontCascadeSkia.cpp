/*
 * Copyright (C) 2024 Igalia S.L.
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

#if USE(SKIA)
#include "FontCache.h"
#include "GraphicsContextSkia.h"
#include "SurrogatePairAwareTextIterator.h"
#include <wtf/text/CharacterProperties.h>

namespace WebCore {

void FontCascade::drawGlyphs(GraphicsContext& graphicsContext, const Font& font, std::span<const GlyphBufferGlyph> glyphs, std::span<const GlyphBufferAdvance> advances, const FloatPoint& position, FontSmoothingMode smoothingMode)
{
    auto blob = font.buildTextBlob(glyphs, advances, smoothingMode);
    if (!blob)
        return;

    static_cast<GraphicsContextSkia*>(&graphicsContext)->drawSkiaText(blob, SkFloatToScalar(position.x()), SkFloatToScalar(position.y()),
        font.enableAntialiasing(smoothingMode), font.platformData().orientation() == FontOrientation::Vertical);
}

bool FontCascade::canUseGlyphDisplayList(const RenderStyle&)
{
    return true;
}

ResolvedEmojiPolicy FontCascade::resolveEmojiPolicy(FontVariantEmoji fontVariantEmoji, char32_t character)
{
    switch (fontVariantEmoji) {
    case FontVariantEmoji::Normal:
        if (isEmojiWithPresentationByDefault(character)
            || isEmojiModifierBase(character)
            || isEmojiFitzpatrickModifier(character))
            return ResolvedEmojiPolicy::RequireEmoji;
        break;
    case FontVariantEmoji::Unicode:
        if (u_hasBinaryProperty(character, UCHAR_EMOJI))
            return isEmojiWithPresentationByDefault(character) ? ResolvedEmojiPolicy::RequireEmoji : ResolvedEmojiPolicy::RequireText;
        break;
    case FontVariantEmoji::Text:
        return ResolvedEmojiPolicy::RequireText;
    case FontVariantEmoji::Emoji:
        if (u_hasBinaryProperty(character, UCHAR_EMOJI))
            return ResolvedEmojiPolicy::RequireEmoji;
    }

    return ResolvedEmojiPolicy::NoPreference;
}

RefPtr<const Font> FontCascade::fontForCombiningCharacterSequence(StringView stringView) const
{
    ASSERT(!stringView.isEmpty());
    auto codePoints = stringView.codePoints();
    auto codePointsIterator = codePoints.begin();
    char32_t baseCharacter = *codePointsIterator;
    ++codePointsIterator;
    bool isOnlySingleCodePoint = codePointsIterator == codePoints.end();

    auto [emojiPolicy, shouldForceEmojiFont] = [&]() -> std::pair<ResolvedEmojiPolicy, bool> {
        if (!isOnlySingleCodePoint) {
            if (*codePointsIterator == emojiVariationSelector)
                return { ResolvedEmojiPolicy::RequireEmoji, true };

            if (*codePointsIterator == textVariationSelector)
                return { ResolvedEmojiPolicy::RequireText, false };
        }

        auto emojiPolicy = resolveEmojiPolicy(m_fontDescription.variantEmoji(), baseCharacter);
        return { emojiPolicy, emojiPolicy == ResolvedEmojiPolicy::RequireEmoji && m_fontDescription.variantEmoji() == FontVariantEmoji::Emoji };
    }();

    char32_t baseCharacterForBaseFont = baseCharacter;
    if (shouldForceEmojiFont) {
        // System fallback doesn't support character sequences, so here we override
        // the base character with the cat emoji to try to force an emoji font.
        baseCharacterForBaseFont = emojiCat;
    }
    GlyphData baseCharacterGlyphData = glyphDataForCharacter(baseCharacterForBaseFont, false, NormalVariant, emojiPolicy);
    if (!baseCharacterGlyphData.glyph)
        return nullptr;

    auto fontMatchesEmojiPolicy = [](const Font* font, ResolvedEmojiPolicy emojiPolicy) -> bool {
        if (!font)
            return false;

        switch (emojiPolicy) {
        case ResolvedEmojiPolicy::RequireEmoji:
            return font->platformData().isColorBitmapFont();
        case ResolvedEmojiPolicy::RequireText:
            return !font->platformData().isColorBitmapFont();
        case ResolvedEmojiPolicy::NoPreference:
            break;
        }
        return true;
    };

    if (isOnlySingleCodePoint && !shouldForceEmojiFont && fontMatchesEmojiPolicy(baseCharacterGlyphData.font.get(), emojiPolicy))
        return baseCharacterGlyphData.font.get();

    bool triedBaseCharacterFont = false;
    for (unsigned i = 0; !fallbackRangesAt(i).isNull(); ++i) {
        auto& fontRanges = fallbackRangesAt(i);
        if (fontRanges.isGenericFontFamily() && isPrivateUseAreaCharacter(baseCharacter))
            continue;

        const Font* font = fontRanges.fontForCharacter(baseCharacter);
        if (!font)
            continue;

        if (!fontMatchesEmojiPolicy(font, emojiPolicy))
            continue;

        if (font == baseCharacterGlyphData.font)
            triedBaseCharacterFont = true;

        if (font->canRenderCombiningCharacterSequence(stringView))
            return font;
    }

    if (!triedBaseCharacterFont && baseCharacterGlyphData.font && baseCharacterGlyphData.font->canRenderCombiningCharacterSequence(stringView))
        return baseCharacterGlyphData.font.get();

    bool clusterContainsOtherNonDefaultIgnorableCodePoints = [&] -> bool {
        if (isOnlySingleCodePoint)
            return false;

        do {
            if (!isDefaultIgnorableCodePoint(*codePointsIterator))
                return true;
            ++codePointsIterator;
        } while (codePointsIterator != codePoints.end());

        return false;
    }();

    // Try a system fallback for the whole cluster if needed.
    if (clusterContainsOtherNonDefaultIgnorableCodePoints) {
        auto preferColoredFont = emojiPolicy == ResolvedEmojiPolicy::RequireEmoji ? FontCache::PreferColoredFont::Yes : FontCache::PreferColoredFont::No;
        if (auto systemFallback = FontCache::forCurrentThread()->systemFallbackForCharacterCluster(m_fontDescription, fallbackRangesAt(0).fontForFirstRange(), IsForPlatformFont::No, preferColoredFont, stringView)) {
            if (systemFallback->canRenderCombiningCharacterSequence(stringView))
                return systemFallback.get();
        }
    }

    return nullptr;
}

} // namespace WebCore

#endif // USE(SKIA)
