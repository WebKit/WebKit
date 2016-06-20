/*
 * Copyright (C) 2006, 2013-2015 Apple Inc.  All rights reserved.
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
 */

#include "config.h"
#include "FontCascadeFonts.h"

#include "FontCache.h"
#include "FontCascade.h"
#include "GlyphPage.h"

namespace WebCore {

class MixedFontGlyphPage {
    WTF_MAKE_FAST_ALLOCATED;
public:
    MixedFontGlyphPage(const GlyphPage* initialPage)
    {
        if (initialPage) {
            for (unsigned i = 0; i < GlyphPage::size; ++i)
                setGlyphDataForIndex(i, initialPage->glyphDataForIndex(i));
        }
    }

    GlyphData glyphDataForCharacter(UChar32 c) const
    {
        unsigned index = GlyphPage::indexForCharacter(c);
        ASSERT_WITH_SECURITY_IMPLICATION(index < GlyphPage::size);
        return { m_glyphs[index], m_fonts[index] };
    }

    void setGlyphDataForCharacter(UChar32 c, GlyphData glyphData)
    {
        setGlyphDataForIndex(GlyphPage::indexForCharacter(c), glyphData);
    }

private:
    void setGlyphDataForIndex(unsigned index, const GlyphData& glyphData)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(index < GlyphPage::size);
        m_glyphs[index] = glyphData.glyph;
        m_fonts[index] = glyphData.font;
    }

    Glyph m_glyphs[GlyphPage::size] { };
    const Font* m_fonts[GlyphPage::size] { };
};

GlyphData FontCascadeFonts::GlyphPageCacheEntry::glyphDataForCharacter(UChar32 character)
{
    ASSERT(!(m_singleFont && m_mixedFont));
    if (m_singleFont)
        return m_singleFont->glyphDataForCharacter(character);
    if (m_mixedFont)
        return m_mixedFont->glyphDataForCharacter(character);
    return 0;
}

void FontCascadeFonts::GlyphPageCacheEntry::setGlyphDataForCharacter(UChar32 character, GlyphData glyphData)
{
    ASSERT(!glyphDataForCharacter(character).glyph);
    if (!m_mixedFont) {
        m_mixedFont = std::make_unique<MixedFontGlyphPage>(m_singleFont.get());
        m_singleFont = nullptr;
    }
    m_mixedFont->setGlyphDataForCharacter(character, glyphData);
}

void FontCascadeFonts::GlyphPageCacheEntry::setSingleFontPage(RefPtr<GlyphPage>&& page)
{
    ASSERT(isNull());
    m_singleFont = page;
}

FontCascadeFonts::FontCascadeFonts(RefPtr<FontSelector>&& fontSelector)
    : m_cachedPrimaryFont(nullptr)
    , m_fontSelector(fontSelector)
    , m_fontSelectorVersion(m_fontSelector ? m_fontSelector->version() : 0)
    , m_generation(FontCache::singleton().generation())
{
}

FontCascadeFonts::FontCascadeFonts(const FontPlatformData& platformData)
    : m_cachedPrimaryFont(nullptr)
    , m_fontSelectorVersion(0)
    , m_generation(FontCache::singleton().generation())
    , m_isForPlatformFont(true)
{
    m_realizedFallbackRanges.append(FontRanges(FontCache::singleton().fontForPlatformData(platformData)));
}

FontCascadeFonts::~FontCascadeFonts()
{
}

void FontCascadeFonts::determinePitch(const FontCascadeDescription& description)
{
    auto& primaryRanges = realizeFallbackRangesAt(description, 0);
    unsigned numRanges = primaryRanges.size();
    if (numRanges == 1)
        m_pitch = primaryRanges.fontForFirstRange().pitch();
    else
        m_pitch = VariablePitch;
}

bool FontCascadeFonts::isLoadingCustomFonts() const
{
    for (auto& fontRanges : m_realizedFallbackRanges) {
        if (fontRanges.isLoading())
            return true;
    }
    return false;
}

static FontRanges realizeNextFallback(const FontCascadeDescription& description, unsigned& index, FontSelector* fontSelector)
{
    ASSERT(index < description.familyCount());

    auto& fontCache = FontCache::singleton();
    while (index < description.familyCount()) {
        const AtomicString& family = description.familyAt(index++);
        if (family.isEmpty())
            continue;
        if (fontSelector) {
            auto ranges = fontSelector->fontRangesForFamily(description, family);
            if (!ranges.isNull())
                return ranges;
        }
        if (auto font = fontCache.fontForFamily(description, family))
            return FontRanges(WTFMove(font));
    }
    // We didn't find a font. Try to find a similar font using our own specific knowledge about our platform.
    // For example on OS X, we know to map any families containing the words Arabic, Pashto, or Urdu to the
    // Geeza Pro font.
    for (auto& family : description.families()) {
        if (auto font = fontCache.similarFont(description, family))
            return FontRanges(WTFMove(font));
    }
    return { };
}

const FontRanges& FontCascadeFonts::realizeFallbackRangesAt(const FontCascadeDescription& description, unsigned index)
{
    if (index < m_realizedFallbackRanges.size())
        return m_realizedFallbackRanges[index];

    ASSERT(index == m_realizedFallbackRanges.size());
    ASSERT(FontCache::singleton().generation() == m_generation);

    m_realizedFallbackRanges.append(FontRanges());
    auto& fontRanges = m_realizedFallbackRanges.last();

    if (!index) {
        fontRanges = realizeNextFallback(description, m_lastRealizedFallbackIndex, m_fontSelector.get());
        if (fontRanges.isNull() && m_fontSelector)
            fontRanges = m_fontSelector->fontRangesForFamily(description, standardFamily);
        if (fontRanges.isNull())
            fontRanges = FontRanges(FontCache::singleton().lastResortFallbackFont(description));
        return fontRanges;
    }

    if (m_lastRealizedFallbackIndex < description.familyCount())
        fontRanges = realizeNextFallback(description, m_lastRealizedFallbackIndex, m_fontSelector.get());

    if (fontRanges.isNull() && m_fontSelector) {
        ASSERT(m_lastRealizedFallbackIndex >= description.familyCount());

        unsigned fontSelectorFallbackIndex = m_lastRealizedFallbackIndex - description.familyCount();
        if (fontSelectorFallbackIndex == m_fontSelector->fallbackFontCount())
            return fontRanges;
        ++m_lastRealizedFallbackIndex;
        fontRanges = FontRanges(m_fontSelector->fallbackFontAt(description, fontSelectorFallbackIndex));
    }

    return fontRanges;
}

static inline bool isInRange(UChar32 character, UChar32 lowerBound, UChar32 upperBound)
{
    return character >= lowerBound && character <= upperBound;
}

static bool shouldIgnoreRotation(UChar32 character)
{
    if (character == 0x000A7 || character == 0x000A9 || character == 0x000AE)
        return true;

    if (character == 0x000B6 || character == 0x000BC || character == 0x000BD || character == 0x000BE)
        return true;

    if (isInRange(character, 0x002E5, 0x002EB))
        return true;
    
    if (isInRange(character, 0x01100, 0x011FF) || isInRange(character, 0x01401, 0x0167F) || isInRange(character, 0x01800, 0x018FF))
        return true;

    if (character == 0x02016 || character == 0x02020 || character == 0x02021 || character == 0x2030 || character == 0x02031)
        return true;

    if (isInRange(character, 0x0203B, 0x0203D) || character == 0x02042 || character == 0x02044 || character == 0x02047
        || character == 0x02048 || character == 0x02049 || character == 0x2051)
        return true;

    if (isInRange(character, 0x02065, 0x02069) || isInRange(character, 0x020DD, 0x020E0)
        || isInRange(character, 0x020E2, 0x020E4) || isInRange(character, 0x02100, 0x02117)
        || isInRange(character, 0x02119, 0x02131) || isInRange(character, 0x02133, 0x0213F))
        return true;

    if (isInRange(character, 0x02145, 0x0214A) || character == 0x0214C || character == 0x0214D
        || isInRange(character, 0x0214F, 0x0218F))
        return true;

    if (isInRange(character, 0x02300, 0x02307) || isInRange(character, 0x0230C, 0x0231F)
        || isInRange(character, 0x02322, 0x0232B) || isInRange(character, 0x0237D, 0x0239A)
        || isInRange(character, 0x023B4, 0x023B6) || isInRange(character, 0x023BA, 0x023CF)
        || isInRange(character, 0x023D1, 0x023DB) || isInRange(character, 0x023E2, 0x024FF))
        return true;

    if (isInRange(character, 0x025A0, 0x02619) || isInRange(character, 0x02620, 0x02767)
        || isInRange(character, 0x02776, 0x02793) || isInRange(character, 0x02B12, 0x02B2F)
        || isInRange(character, 0x02B4D, 0x02BFF) || isInRange(character, 0x02E80, 0x03007))
        return true;

    if (character == 0x03012 || character == 0x03013 || isInRange(character, 0x03020, 0x0302F)
        || isInRange(character, 0x03031, 0x0309F) || isInRange(character, 0x030A1, 0x030FB)
        || isInRange(character, 0x030FD, 0x0A4CF))
        return true;

    if (isInRange(character, 0x0A840, 0x0A87F) || isInRange(character, 0x0A960, 0x0A97F)
        || isInRange(character, 0x0AC00, 0x0D7FF) || isInRange(character, 0x0E000, 0x0FAFF))
        return true;

    if (isInRange(character, 0x0FE10, 0x0FE1F) || isInRange(character, 0x0FE30, 0x0FE48)
        || isInRange(character, 0x0FE50, 0x0FE57) || isInRange(character, 0x0FE5F, 0x0FE62)
        || isInRange(character, 0x0FE67, 0x0FE6F))
        return true;

    if (isInRange(character, 0x0FF01, 0x0FF07) || isInRange(character, 0x0FF0A, 0x0FF0C)
        || isInRange(character, 0x0FF0E, 0x0FF19) || character == 0x0FF1B || isInRange(character, 0x0FF1F, 0x0FF3A))
        return true;

    if (character == 0x0FF3C || character == 0x0FF3E)
        return true;

    if (isInRange(character, 0x0FF40, 0x0FF5A) || isInRange(character, 0x0FFE0, 0x0FFE2)
        || isInRange(character, 0x0FFE4, 0x0FFE7) || isInRange(character, 0x0FFF0, 0x0FFF8)
        || character == 0x0FFFD)
        return true;

    if (isInRange(character, 0x13000, 0x1342F) || isInRange(character, 0x1B000, 0x1B0FF)
        || isInRange(character, 0x1D000, 0x1D1FF) || isInRange(character, 0x1D300, 0x1D37F)
        || isInRange(character, 0x1F000, 0x1F64F) || isInRange(character, 0x1F680, 0x1F77F))
        return true;
    
    if (isInRange(character, 0x20000, 0x2FFFD) || isInRange(character, 0x30000, 0x3FFFD))
        return true;

    return false;
}

#if PLATFORM(COCOA) || USE(CAIRO)
static GlyphData glyphDataForCJKCharacterWithoutSyntheticItalic(UChar32 character, GlyphData& data)
{
    GlyphData nonItalicData = data.font->nonSyntheticItalicFont().glyphDataForCharacter(character);
    if (nonItalicData.font)
        return nonItalicData;
    return data;
}
#endif
    
static GlyphData glyphDataForNonCJKCharacterWithGlyphOrientation(UChar32 character, NonCJKGlyphOrientation orientation, const GlyphData& data)
{
    if (orientation == NonCJKGlyphOrientation::Upright || shouldIgnoreRotation(character)) {
        GlyphData uprightData = data.font->uprightOrientationFont().glyphDataForCharacter(character);
        // If the glyphs are the same, then we know we can just use the horizontal glyph rotated vertically to be upright.
        if (data.glyph == uprightData.glyph)
            return data;
        // The glyphs are distinct, meaning that the font has a vertical-right glyph baked into it. We can't use that
        // glyph, so we fall back to the upright data and use the horizontal glyph.
        if (uprightData.font)
            return uprightData;
    } else if (orientation == NonCJKGlyphOrientation::Mixed) {
        GlyphData verticalRightData = data.font->verticalRightOrientationFont().glyphDataForCharacter(character);
        // If the glyphs are distinct, we will make the assumption that the font has a vertical-right glyph baked
        // into it.
        if (data.glyph != verticalRightData.glyph)
            return data;
        // The glyphs are identical, meaning that we should just use the horizontal glyph.
        if (verticalRightData.font)
            return verticalRightData;
    }
    return data;
}

GlyphData FontCascadeFonts::glyphDataForSystemFallback(UChar32 c, const FontCascadeDescription& description, FontVariant variant)
{
    // System fallback is character-dependent.
    auto& primaryRanges = realizeFallbackRangesAt(description, 0);
    auto* originalFont = primaryRanges.fontForCharacter(c);
    if (!originalFont)
        originalFont = &primaryRanges.fontForFirstRange();

    auto systemFallbackFont = originalFont->systemFallbackFontForCharacter(c, description, m_isForPlatformFont);
    if (!systemFallbackFont)
        return GlyphData();

    if (systemFallbackFont->platformData().orientation() == Vertical && !systemFallbackFont->hasVerticalGlyphs() && FontCascade::isCJKIdeographOrSymbol(c))
        variant = BrokenIdeographVariant;

    GlyphData fallbackGlyphData;
    if (variant == NormalVariant)
        fallbackGlyphData = systemFallbackFont->glyphDataForCharacter(c);
    else
        fallbackGlyphData = systemFallbackFont->variantFont(description, variant)->glyphDataForCharacter(c);

    if (fallbackGlyphData.font && fallbackGlyphData.font->platformData().orientation() == Vertical && !fallbackGlyphData.font->isTextOrientationFallback()) {
        if (variant == NormalVariant && !FontCascade::isCJKIdeographOrSymbol(c))
            fallbackGlyphData = glyphDataForNonCJKCharacterWithGlyphOrientation(c, description.nonCJKGlyphOrientation(), fallbackGlyphData);
#if PLATFORM(COCOA) || USE(CAIRO)
        if (fallbackGlyphData.font->platformData().syntheticOblique() && FontCascade::isCJKIdeographOrSymbol(c))
            fallbackGlyphData = glyphDataForCJKCharacterWithoutSyntheticItalic(c, fallbackGlyphData);
#endif
    }

    // Keep the system fallback fonts we use alive.
    if (fallbackGlyphData.glyph)
        m_systemFallbackFontSet.add(WTFMove(systemFallbackFont));

    return fallbackGlyphData;
}

GlyphData FontCascadeFonts::glyphDataForVariant(UChar32 c, const FontCascadeDescription& description, FontVariant variant, unsigned fallbackIndex)
{
    while (true) {
        auto& fontRanges = realizeFallbackRangesAt(description, fallbackIndex++);
        if (fontRanges.isNull())
            break;
        GlyphData data = fontRanges.glyphDataForCharacter(c);
        if (!data.font)
            continue;
        // The variantFont function should not normally return 0.
        // But if it does, we will just render the capital letter big.
        if (const Font* variantFont = data.font->variantFont(description, variant))
            return variantFont->glyphDataForCharacter(c);
        return data;
    }

    return glyphDataForSystemFallback(c, description, variant);
}

GlyphData FontCascadeFonts::glyphDataForNormalVariant(UChar32 c, const FontCascadeDescription& description)
{
    for (unsigned fallbackIndex = 0; ; ++fallbackIndex) {
        auto& fontRanges = realizeFallbackRangesAt(description, fallbackIndex);
        if (fontRanges.isNull())
            break;
        GlyphData data = fontRanges.glyphDataForCharacter(c);
        if (!data.font)
            continue;
        if (data.font->platformData().orientation() == Vertical && !data.font->isTextOrientationFallback()) {
            if (!FontCascade::isCJKIdeographOrSymbol(c))
                return glyphDataForNonCJKCharacterWithGlyphOrientation(c, description.nonCJKGlyphOrientation(), data);

            if (!data.font->hasVerticalGlyphs()) {
                // Use the broken ideograph font data. The broken ideograph font will use the horizontal width of glyphs
                // to make sure you get a square (even for broken glyphs like symbols used for punctuation).
                return glyphDataForVariant(c, description, BrokenIdeographVariant, fallbackIndex);
            }
#if PLATFORM(COCOA) || USE(CAIRO)
            if (data.font->platformData().syntheticOblique())
                return glyphDataForCJKCharacterWithoutSyntheticItalic(c, data);
#endif
        }
        return data;
    }

    return glyphDataForSystemFallback(c, description, NormalVariant);
}

static RefPtr<GlyphPage> glyphPageFromFontRanges(unsigned pageNumber, const FontRanges& fontRanges)
{
    const Font* font = nullptr;
    UChar32 pageRangeFrom = pageNumber * GlyphPage::size;
    UChar32 pageRangeTo = pageRangeFrom + GlyphPage::size - 1;
    for (unsigned i = 0; i < fontRanges.size(); ++i) {
        auto& range = fontRanges.rangeAt(i);
        if (range.to()) {
            if (range.from() <= pageRangeFrom && pageRangeTo <= range.to())
                font = range.font();
            break;
        }
    }
    if (!font || font->platformData().orientation() == Vertical)
        return nullptr;

    return const_cast<GlyphPage*>(font->glyphPage(pageNumber));
}

GlyphData FontCascadeFonts::glyphDataForCharacter(UChar32 c, const FontCascadeDescription& description, FontVariant variant)
{
    ASSERT(isMainThread());
    ASSERT(variant != AutoVariant);

    if (variant != NormalVariant)
        return glyphDataForVariant(c, description, variant, 0);

    const unsigned pageNumber = c / GlyphPage::size;

    auto& cacheEntry = pageNumber ? m_cachedPages.add(pageNumber, GlyphPageCacheEntry()).iterator->value : m_cachedPageZero;

    // Initialize cache with a full page of glyph mappings from a single font.
    if (cacheEntry.isNull())
        cacheEntry.setSingleFontPage(glyphPageFromFontRanges(pageNumber, realizeFallbackRangesAt(description, 0)));

    GlyphData glyphData = cacheEntry.glyphDataForCharacter(c);
    if (!glyphData.glyph) {
        // No glyph, resolve per-character.
        glyphData = glyphDataForNormalVariant(c, description);
        // Cache the results.
        cacheEntry.setGlyphDataForCharacter(c, glyphData);
    }

    return glyphData;
}

void FontCascadeFonts::pruneSystemFallbacks()
{
    if (m_systemFallbackFontSet.isEmpty())
        return;
    // Mutable glyph pages may reference fallback fonts.
    if (m_cachedPageZero.isMixedFont())
        m_cachedPageZero = { };
    m_cachedPages.removeIf([](auto& keyAndValue) {
        return keyAndValue.value.isMixedFont();
    });
    m_systemFallbackFontSet.clear();
}

}
