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
#include <skia/core/SkTextBlob.h>
#include <wtf/text/CharacterProperties.h>

namespace WebCore {

void FontCascade::drawGlyphs(GraphicsContext& graphicsContext, const Font& font, const GlyphBufferGlyph* glyphs, const GlyphBufferAdvance* advances, unsigned glyphCount, const FloatPoint& position, FontSmoothingMode smoothingMode)
{
    if (!font.platformData().size())
        return;

    const auto& fontPlatformData = font.platformData();
    const auto& skFont = fontPlatformData.skFont();

    if (!font.allowsAntialiasing())
        smoothingMode = FontSmoothingMode::NoSmoothing;

    SkFont::Edging edging;
    switch (smoothingMode) {
    case FontSmoothingMode::AutoSmoothing:
        edging = skFont.getEdging();
        break;
    case FontSmoothingMode::Antialiased:
        edging = SkFont::Edging::kAntiAlias;
        break;
    case FontSmoothingMode::SubpixelAntialiased:
        edging = SkFont::Edging::kSubpixelAntiAlias;
        break;
    case FontSmoothingMode::NoSmoothing:
        edging = SkFont::Edging::kAlias;
        break;
    }

    bool isVertical = fontPlatformData.orientation() == FontOrientation::Vertical;
    SkTextBlobBuilder builder;
    const auto& buffer = [&]() {
        if (skFont.getEdging() == edging)
            return isVertical ? builder.allocRunPos(skFont, glyphCount) : builder.allocRunPosH(skFont, glyphCount, 0);

        SkFont copiedFont = skFont;
        copiedFont.setEdging(edging);
        return isVertical ? builder.allocRunPos(copiedFont, glyphCount) : builder.allocRunPosH(copiedFont, glyphCount, 0);
    }();

    FloatSize glyphPosition;
    for (unsigned i = 0; i < glyphCount; ++i) {
        buffer.glyphs[i] = glyphs[i];

        if (isVertical) {
            glyphPosition += advances[i];
            buffer.pos[2 * i] = glyphPosition.height();
            buffer.pos[2 * i + 1] = glyphPosition.width();
        } else {
            buffer.pos[i] = glyphPosition.width();
            glyphPosition += advances[i];
        }
    }

    auto blob = builder.make();
    static_cast<GraphicsContextSkia*>(&graphicsContext)->drawSkiaText(blob, SkFloatToScalar(position.x()), SkFloatToScalar(position.y()), edging != SkFont::Edging::kAlias, isVertical);
}

bool FontCascade::canReturnFallbackFontsForComplexText()
{
    return false;
}

bool FontCascade::canExpandAroundIdeographsInComplexText()
{
    return false;
}

ResolvedEmojiPolicy FontCascade::resolveEmojiPolicy(FontVariantEmoji fontVariantEmoji, char32_t character)
{
    switch (fontVariantEmoji) {
    case FontVariantEmoji::Normal:
    case FontVariantEmoji::Unicode:
        if (isEmojiWithPresentationByDefault(character)
            || isEmojiModifierBase(character)
            || isEmojiFitzpatrickModifier(character))
            return ResolvedEmojiPolicy::RequireEmoji;
        break;
    case FontVariantEmoji::Text:
        return ResolvedEmojiPolicy::RequireText;
    case FontVariantEmoji::Emoji:
        return ResolvedEmojiPolicy::RequireEmoji;
    }

    return ResolvedEmojiPolicy::NoPreference;
}

const Font* FontCascade::fontForCombiningCharacterSequence(StringView stringView) const
{
    ASSERT(!stringView.isEmpty());
    auto codePoints = stringView.codePoints();
    auto codePointsIterator = codePoints.begin();
    char32_t baseCharacter = *codePointsIterator;
    ++codePointsIterator;
    bool isOnlySingleCodePoint = codePointsIterator == codePoints.end();
    GlyphData baseCharacterGlyphData = glyphDataForCharacter(baseCharacter, false, NormalVariant);
    if (!baseCharacterGlyphData.glyph)
        return nullptr;

    if (isOnlySingleCodePoint)
        return baseCharacterGlyphData.font.get();

    bool triedBaseCharacterFont = false;
    for (unsigned i = 0; !fallbackRangesAt(i).isNull(); ++i) {
        auto& fontRanges = fallbackRangesAt(i);
        if (fontRanges.isGenericFontFamily() && isPrivateUseAreaCharacter(baseCharacter))
            continue;

        const Font* font = fontRanges.fontForCharacter(baseCharacter);
        if (!font)
            continue;

        if (font == baseCharacterGlyphData.font)
            triedBaseCharacterFont = true;

        if (font->canRenderCombiningCharacterSequence(stringView))
            return font;
    }

    if (!triedBaseCharacterFont && baseCharacterGlyphData.font && baseCharacterGlyphData.font->canRenderCombiningCharacterSequence(stringView))
        return baseCharacterGlyphData.font.get();
    return nullptr;
}

} // namespace WebCore

#endif // USE(SKIA)
