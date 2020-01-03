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
        unsigned index = GlyphPage::indexForCodePoint(c);
        ASSERT_WITH_SECURITY_IMPLICATION(index < GlyphPage::size);
        return { m_glyphs[index], m_fonts[index] };
    }

    void setGlyphDataForCharacter(UChar32 c, GlyphData glyphData)
    {
        setGlyphDataForIndex(GlyphPage::indexForCodePoint(c), glyphData);
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
        m_mixedFont = makeUnique<MixedFontGlyphPage>(m_singleFont.get());
        m_singleFont = nullptr;
    }
    m_mixedFont->setGlyphDataForCharacter(character, glyphData);
}

void FontCascadeFonts::GlyphPageCacheEntry::setSingleFontPage(RefPtr<GlyphPage>&& page)
{
    ASSERT(isNull());
    m_singleFont = page;
}

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(FontCascadeFonts);

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

FontCascadeFonts::~FontCascadeFonts() = default;

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
    ASSERT(index < description.effectiveFamilyCount());

    auto& fontCache = FontCache::singleton();
    while (index < description.effectiveFamilyCount()) {
        auto visitor = WTF::makeVisitor([&](const AtomString& family) -> FontRanges {
            if (family.isEmpty())
                return FontRanges();
            if (fontSelector) {
                auto ranges = fontSelector->fontRangesForFamily(description, family);
                if (!ranges.isNull())
                    return ranges;
            }
            if (auto font = fontCache.fontForFamily(description, family))
                return FontRanges(WTFMove(font));
            return FontRanges();
        }, [&](const FontFamilyPlatformSpecification& fontFamilySpecification) -> FontRanges {
            return fontFamilySpecification.fontRanges(description);
        });
        const auto& currentFamily = description.effectiveFamilyAt(index++);
        auto ranges = WTF::visit(visitor, currentFamily);
        if (!ranges.isNull())
            return ranges;
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

    if (m_lastRealizedFallbackIndex < description.effectiveFamilyCount())
        fontRanges = realizeNextFallback(description, m_lastRealizedFallbackIndex, m_fontSelector.get());

    if (fontRanges.isNull() && m_fontSelector) {
        ASSERT(m_lastRealizedFallbackIndex >= description.effectiveFamilyCount());

        unsigned fontSelectorFallbackIndex = m_lastRealizedFallbackIndex - description.effectiveFamilyCount();
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

static GlyphData glyphDataForNonCJKCharacterWithGlyphOrientation(UChar32 character, NonCJKGlyphOrientation orientation, const GlyphData& data)
{
    bool syntheticOblique = data.font->platformData().syntheticOblique();
    if (orientation == NonCJKGlyphOrientation::Upright || shouldIgnoreRotation(character)) {
        GlyphData uprightData = data.font->uprightOrientationFont().glyphDataForCharacter(character);
        // If the glyphs are the same, then we know we can just use the horizontal glyph rotated vertically
        // to be upright. For synthetic oblique, however, we will always return the uprightData to ensure
        // that non-CJK and CJK runs are broken up. This guarantees that vertical
        // fonts without isTextOrientationFallback() set contain CJK characters only and thus we can get
        // the oblique slant correct.
        if (data.glyph == uprightData.glyph && !syntheticOblique)
            return data;
        // The glyphs are distinct, meaning that the font has a vertical-right glyph baked into it. We can't use that
        // glyph, so we fall back to the upright data and use the horizontal glyph.
        if (uprightData.font)
            return uprightData;
    } else if (orientation == NonCJKGlyphOrientation::Mixed) {
        GlyphData verticalRightData = data.font->verticalRightOrientationFont().glyphDataForCharacter(character);

        // If there is a baked-in rotated glyph, we will use it unless syntheticOblique is set. If
        // synthetic oblique is set, we fall back to the horizontal glyph. This guarantees that vertical
        // fonts without isTextOrientationFallback() set contain CJK characters only and thus we can get
        // the oblique slant correct.
        if (data.glyph != verticalRightData.glyph && !syntheticOblique)
            return data;

        // The glyphs are identical, meaning that we should just use the horizontal glyph.
        if (verticalRightData.font)
            return verticalRightData;
    }
    return data;
}

static const Font* findBestFallbackFont(FontCascadeFonts& fontCascadeFonts, const FontCascadeDescription& description, UChar32 character)
{
    for (unsigned fallbackIndex = 0; ; ++fallbackIndex) {
        auto& fontRanges = fontCascadeFonts.realizeFallbackRangesAt(description, fallbackIndex);
        if (fontRanges.isNull())
            break;
        auto* currentFont = fontRanges.glyphDataForCharacter(character, ExternalResourceDownloadPolicy::Forbid).font;
        if (!currentFont)
            currentFont = &fontRanges.fontForFirstRange();

        if (!currentFont->isInterstitial())
            return currentFont;
    }

    return nullptr;
}

GlyphData FontCascadeFonts::glyphDataForSystemFallback(UChar32 character, const FontCascadeDescription& description, FontVariant variant, bool systemFallbackShouldBeInvisible)
{
    const Font* font = findBestFallbackFont(*this, description, character);

    if (!font)
        font = &realizeFallbackRangesAt(description, 0).fontForFirstRange();

    auto systemFallbackFont = font->systemFallbackFontForCharacter(character, description, m_isForPlatformFont ? IsForPlatformFont::Yes : IsForPlatformFont::No);
    if (!systemFallbackFont)
        return GlyphData();

#if HAVE(DISALLOWABLE_USER_INSTALLED_FONTS)
    ASSERT(!systemFallbackFont->isUserInstalledFont() || description.shouldAllowUserInstalledFonts() == AllowUserInstalledFonts::Yes);
#endif

    if (systemFallbackShouldBeInvisible)
        systemFallbackFont = const_cast<Font*>(&systemFallbackFont->invisibleFont());

    if (systemFallbackFont->platformData().orientation() == FontOrientation::Vertical && !systemFallbackFont->hasVerticalGlyphs() && FontCascade::isCJKIdeographOrSymbol(character))
        variant = BrokenIdeographVariant;

    GlyphData fallbackGlyphData;
    if (variant == NormalVariant)
        fallbackGlyphData = systemFallbackFont->glyphDataForCharacter(character);
    else
        fallbackGlyphData = systemFallbackFont->variantFont(description, variant)->glyphDataForCharacter(character);

    if (fallbackGlyphData.font && fallbackGlyphData.font->platformData().orientation() == FontOrientation::Vertical && !fallbackGlyphData.font->isTextOrientationFallback()) {
        if (variant == NormalVariant && !FontCascade::isCJKIdeographOrSymbol(character))
            fallbackGlyphData = glyphDataForNonCJKCharacterWithGlyphOrientation(character, description.nonCJKGlyphOrientation(), fallbackGlyphData);
    }

    // Keep the system fallback fonts we use alive.
    if (fallbackGlyphData.glyph)
        m_systemFallbackFontSet.add(WTFMove(systemFallbackFont));

    return fallbackGlyphData;
}

enum class FallbackVisibility {
    Immaterial,
    Visible,
    Invisible
};

static void opportunisticallyStartFontDataURLLoading(const FontCascadeDescription& description, FontSelector* fontSelector)
{
    // It is a somewhat common practice for a font foundry to break up a single font into two fonts, each having a random half of
    // the alphabet, and then encoding the two fonts as data: urls (with different font-family names).
    // Therefore, if these two fonts don't get loaded at (nearly) the same time, there will be a flash of unintelligible text where
    // only a random half of the letters are visible.
    // This code attempts to pre-warm these data urls to make them load at closer to the same time. However, font loading is
    // asynchronous, and this code doesn't actually fix the race - it just makes it more likely for the two fonts to tie in the race.
    if (!fontSelector)
        return;
    for (unsigned i = 0; i < description.familyCount(); ++i)
        fontSelector->opportunisticallyStartFontDataURLLoading(description, description.familyAt(i));
}

GlyphData FontCascadeFonts::glyphDataForVariant(UChar32 character, const FontCascadeDescription& description, FontVariant variant, unsigned fallbackIndex)
{
    FallbackVisibility fallbackVisibility = FallbackVisibility::Immaterial;
    ExternalResourceDownloadPolicy policy = ExternalResourceDownloadPolicy::Allow;
    GlyphData loadingResult;
    opportunisticallyStartFontDataURLLoading(description, m_fontSelector.get());
    for (; ; ++fallbackIndex) {
        auto& fontRanges = realizeFallbackRangesAt(description, fallbackIndex);
        if (fontRanges.isNull())
            break;

        GlyphData data = fontRanges.glyphDataForCharacter(character, policy);
        if (!data.font)
            continue;

        if (data.font->isInterstitial()) {
            policy = ExternalResourceDownloadPolicy::Forbid;
            if (fallbackVisibility == FallbackVisibility::Immaterial)
                fallbackVisibility = data.font->visibility() == Font::Visibility::Visible ? FallbackVisibility::Visible : FallbackVisibility::Invisible;
            if (!loadingResult.font && data.glyph)
                loadingResult = data;
            continue;
        }

        if (fallbackVisibility == FallbackVisibility::Invisible && data.font->visibility() == Font::Visibility::Visible)
            data.font = &data.font->invisibleFont();

        if (variant == NormalVariant) {
            if (data.font->platformData().orientation() == FontOrientation::Vertical && !data.font->isTextOrientationFallback()) {
                if (!FontCascade::isCJKIdeographOrSymbol(character))
                    return glyphDataForNonCJKCharacterWithGlyphOrientation(character, description.nonCJKGlyphOrientation(), data);

                if (!data.font->hasVerticalGlyphs()) {
                    // Use the broken ideograph font data. The broken ideograph font will use the horizontal width of glyphs
                    // to make sure you get a square (even for broken glyphs like symbols used for punctuation).
                    return glyphDataForVariant(character, description, BrokenIdeographVariant, fallbackIndex);
                }
            }
        } else {
            // The variantFont function should not normally return 0.
            // But if it does, we will just render the capital letter big.
            if (const Font* variantFont = data.font->variantFont(description, variant))
                return variantFont->glyphDataForCharacter(character);
        }

        return data;
    }

    if (loadingResult.font)
        return loadingResult;
    return glyphDataForSystemFallback(character, description, variant, fallbackVisibility == FallbackVisibility::Invisible);
}

static RefPtr<GlyphPage> glyphPageFromFontRanges(unsigned pageNumber, const FontRanges& fontRanges)
{
    const Font* font = nullptr;
    UChar32 pageRangeFrom = pageNumber * GlyphPage::size;
    UChar32 pageRangeTo = pageRangeFrom + GlyphPage::size - 1;
    auto policy = ExternalResourceDownloadPolicy::Allow;
    FallbackVisibility desiredVisibility = FallbackVisibility::Immaterial;
    for (unsigned i = 0; i < fontRanges.size(); ++i) {
        auto& range = fontRanges.rangeAt(i);
        if (range.from() <= pageRangeFrom && pageRangeTo <= range.to()) {
            font = range.font(policy);
            if (!font)
                continue;
            if (font->isInterstitial()) {
                if (desiredVisibility == FallbackVisibility::Immaterial) {
                    auto fontVisibility = font->visibility();
                    if (fontVisibility == Font::Visibility::Visible)
                        desiredVisibility = FallbackVisibility::Visible;
                    else {
                        ASSERT(fontVisibility == Font::Visibility::Invisible);
                        desiredVisibility = FallbackVisibility::Invisible;
                    }
                }
                font = nullptr;
                policy = ExternalResourceDownloadPolicy::Forbid;
                continue;
            }
        }
        break;
    }
    if (!font || font->platformData().orientation() == FontOrientation::Vertical)
        return nullptr;

    if (desiredVisibility == FallbackVisibility::Invisible && font->visibility() == Font::Visibility::Visible)
        return const_cast<GlyphPage*>(font->invisibleFont().glyphPage(pageNumber));
    return const_cast<GlyphPage*>(font->glyphPage(pageNumber));
}

GlyphData FontCascadeFonts::glyphDataForCharacter(UChar32 c, const FontCascadeDescription& description, FontVariant variant)
{
    ASSERT(isMainThread());
    ASSERT(variant != AutoVariant);

    if (variant != NormalVariant)
        return glyphDataForVariant(c, description, variant);

    const unsigned pageNumber = GlyphPage::pageNumberForCodePoint(c);

    auto& cacheEntry = pageNumber ? m_cachedPages.add(pageNumber, GlyphPageCacheEntry()).iterator->value : m_cachedPageZero;

    // Initialize cache with a full page of glyph mappings from a single font.
    if (cacheEntry.isNull())
        cacheEntry.setSingleFontPage(glyphPageFromFontRanges(pageNumber, realizeFallbackRangesAt(description, 0)));

    GlyphData glyphData = cacheEntry.glyphDataForCharacter(c);
    if (!glyphData.glyph) {
        // No glyph, resolve per-character.
        ASSERT(variant == NormalVariant);
        glyphData = glyphDataForVariant(c, description, variant);
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
