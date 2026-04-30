/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#pragma once

#include <WebCore/FontCascade.h>
#include <WebCore/FontCascadeFonts.h>
#include <unicode/uchar.h>
#include <wtf/text/CharacterProperties.h>
#include <wtf/unicode/CharacterNames.h>

#if PLATFORM(COCOA)
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

namespace WebCore {

inline const Font& FontCascade::primaryFont() const
{
    if (m_fonts->cachedPrimaryFont())
        return *m_fonts->cachedPrimaryFont();
    WeakRef font = protect(m_fonts)->primaryFont(m_fontDescription, protect(fontSelector()).get());
    m_fontDescription.resolveFontSizeAdjustFromFontIfNeeded(font);
    return font;
}

inline const FontRanges& FontCascade::fallbackRangesAt(unsigned index) const
{
    return protect(m_fonts)->realizeFallbackRangesAt(m_fontDescription, protect(fontSelector()).get(), index);
}

inline bool FontCascade::isFixedPitch() const
{
    return protect(m_fonts)->isFixedPitch(m_fontDescription, protect(fontSelector()).get());
}

inline bool FontCascade::canTakeFixedPitchFastContentMeasuring() const
{
    auto cachedCanTakeFixedPitch = m_fonts->cachedCanTakeFixedPitchFastContentMeasuring();
    if (cachedCanTakeFixedPitch != TriState::Indeterminate)
        return cachedCanTakeFixedPitch == TriState::True;
    return protect(m_fonts)->canTakeFixedPitchFastContentMeasuring(m_fontDescription, protect(fontSelector()).get());
}

inline FontSelector* FontCascade::fontSelector() const
{
    return m_fontSelector.get();
}

inline float FontCascade::tabWidth(const Font& font, const TabSize& tabSize, float position, Font::SyntheticBoldInclusion syntheticBoldInclusion) const
{
    float baseTabWidth = tabSize.widthInPixels(font.spaceWidth());
    float result = 0;
    if (!baseTabWidth)
        result = letterSpacing();
    else {
        auto remainder = fmodf(position, baseTabWidth);
        if (remainder < 0)
            remainder += baseTabWidth;
        result = baseTabWidth - remainder;
        if (result < font.spaceWidth() / 2)
            result += baseTabWidth;
    }
    // If our caller passes in SyntheticBoldInclusion::Exclude, that means they're going to apply synthetic bold themselves later.
    // However, regardless of that, the space characters that are fed into the width calculation need to have their correct width, including the synthetic bold.
    // So, we've already got synthetic bold applied, so if we're supposed to exclude it, we need to subtract it out here.
    return result - (syntheticBoldInclusion == Font::SyntheticBoldInclusion::Exclude ? font.syntheticBoldOffset() : 0);
}

inline float FontCascade::widthForTextUsingSimplifiedMeasuring(StringView text, TextDirection textDirection) const
{
    if (text.isEmpty())
        return 0;
    ASSERT(codePath(TextRun(text)) != CodePath::Complex);
    auto* cacheEntry = fonts()->glyphGeometryCache().add(text, { });
    if (cacheEntry && cacheEntry->width)
        return *cacheEntry->width;

    return widthForSimpleTextSlow(text, textDirection, cacheEntry);
}

inline bool FontCascade::isPlatformFont() const
{
    return m_fonts->isForPlatformFont();
}

inline const FontMetrics& FontCascade::metricsOfPrimaryFont() const
{
    return primaryFont().fontMetrics();
}

inline bool FontCascade::isInvisibleReplacementObjectCharacter(char32_t character)
{
    if (character != objectReplacementCharacter)
        return false;
#if PLATFORM(COCOA)
    // We make an exception for Books because some already available books when converted to EPUBS might contain object replacement character that should not be visible to the user.
    return WTF::CocoaApplication::isAppleBooks();
#else
    return false;
#endif
}

inline char32_t mirrorCharacterIfNeeded(char32_t character)
{
#if HAVE(CORE_TEXT_BIDI_MIRRORING)
    // CTFontShapeGlyphs applies Bidi_Mirroring_Glyph automatically when shaping with kCTFontShapeRightToLeft.
    return character;
#else
    return u_charMirror(character);
#endif
}

inline bool FontCascade::treatAsSpace(char32_t c)
{
    return c == space || c == tabCharacter || c == newlineCharacter || c == noBreakSpace;
}

inline bool FontCascade::isCharacterWhoseGlyphsShouldBeDeletedForTextRendering(char32_t character)
{
    // https://www.w3.org/TR/css-text-3/#white-space-processing
    // "Control characters (Unicode category Cc)—other than tabs (U+0009), line feeds (U+000A), carriage returns (U+000D) and sequences that form a segment break—must be rendered as a visible glyph"
    if (character == tabCharacter || character == newlineCharacter || character == carriageReturn)
        return true;
    // Also, we're omitting Null (U+0000) from this set because Chrome and Firefox do so and it's needed for compat. See https://github.com/w3c/csswg-drafts/pull/6983.
    if (character == nullCharacter)
        return true;
    if (isControlCharacter(character))
        return false;
    // "Unsupported Default_ignorable characters must be ignored for text rendering."
    return isDefaultIgnorableCodePoint(character) || isInvisibleReplacementObjectCharacter(character);
}

inline bool FontCascade::treatAsZeroWidthSpace(char32_t c)
{
    return treatAsZeroWidthSpaceInComplexScript(c) || c == zeroWidthNonJoiner || c == zeroWidthJoiner;
}

inline bool FontCascade::treatAsZeroWidthSpaceInComplexScript(char32_t c)
{
    return c < space
        || (c >= deleteCharacter && c < noBreakSpace)
        || c == softHyphen
        || c == zeroWidthSpace
        || (c >= leftToRightMark && c <= rightToLeftMark)
        || (c >= leftToRightEmbed && c <= rightToLeftOverride)
        || c == zeroWidthNoBreakSpace
        || isInvisibleReplacementObjectCharacter(c);
}

inline char16_t FontCascade::normalizeSpaces(char16_t character)
{
    if (treatAsSpace(character))
        return space;

    if (treatAsZeroWidthSpace(character))
        return zeroWidthSpace;

    return character;
}

inline float FontCascade::widthOfSpaceString() const
{
    return width(StringView(WTF::span(space)));
}

}

#if PLATFORM(COCOA)

#include <WebCore/FontCascadeCocoaInlines.h>

#elif USE(CAIRO)

#include <WebCore/FontCascadeCairoInlines.h>

#elif USE(SKIA)

#include <WebCore/FontCascadeSkiaInlines.h>

#elif PLATFORM(WIN)

#include <WebCore/FontCascadeWinInlines.h>

#endif

