/*
 * Copyright (C) 2005, 2008, 2010, 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov
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
#include "Font.h"

#if PLATFORM(COCOA)
#include <pal/spi/cocoa/CoreTextSPI.h>
#endif
#include "FontCache.h"
#include "FontCascade.h"
#include "OpenTypeMathData.h"
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/AtomicStringHash.h>

#if ENABLE(OPENTYPE_VERTICAL)
#include "OpenTypeVerticalData.h"
#endif

#if USE(DIRECT2D)
#include <dwrite.h>
#endif

namespace WebCore {

unsigned GlyphPage::s_count = 0;

const float smallCapsFontSizeMultiplier = 0.7f;
const float emphasisMarkFontSizeMultiplier = 0.5f;

Font::Font(const FontPlatformData& platformData, Origin origin, Interstitial interstitial, Visibility visibility, OrientationFallback orientationFallback)
    : m_maxCharWidth(-1)
    , m_avgCharWidth(-1)
    , m_platformData(platformData)
    , m_mathData(nullptr)
    , m_origin(origin)
    , m_visibility(visibility)
    , m_treatAsFixedPitch(false)
    , m_isInterstitial(interstitial == Interstitial::Yes)
    , m_isTextOrientationFallback(orientationFallback == OrientationFallback::Yes)
    , m_isBrokenIdeographFallback(false)
    , m_hasVerticalGlyphs(false)
    , m_isUsedInSystemFallbackCache(false)
#if PLATFORM(IOS)
    , m_shouldNotBeUsedForArabic(false)
#endif
{
    platformInit();
    platformGlyphInit();
    platformCharWidthInit();
#if ENABLE(OPENTYPE_VERTICAL)
    if (platformData.orientation() == Vertical && orientationFallback == OrientationFallback::No) {
        m_verticalData = FontCache::singleton().verticalData(platformData);
        m_hasVerticalGlyphs = m_verticalData.get() && m_verticalData->hasVerticalMetrics();
    }
#endif
}

// Estimates of avgCharWidth and maxCharWidth for platforms that don't support accessing these values from the font.
void Font::initCharWidths()
{
    auto* glyphPageZero = glyphPage(GlyphPage::pageNumberForCodePoint('0'));

    // Treat the width of a '0' as the avgCharWidth.
    if (m_avgCharWidth <= 0.f && glyphPageZero) {
        Glyph digitZeroGlyph = glyphPageZero->glyphDataForCharacter('0').glyph;
        if (digitZeroGlyph)
            m_avgCharWidth = widthForGlyph(digitZeroGlyph);
    }

    // If we can't retrieve the width of a '0', fall back to the x height.
    if (m_avgCharWidth <= 0.f)
        m_avgCharWidth = m_fontMetrics.xHeight();

    if (m_maxCharWidth <= 0.f)
        m_maxCharWidth = std::max(m_avgCharWidth, m_fontMetrics.floatAscent());
}

void Font::platformGlyphInit()
{
    auto* glyphPageZero = glyphPage(0);
    auto* glyphPageCharacterZero = glyphPage(GlyphPage::pageNumberForCodePoint('0'));
    auto* glyphPageSpace = glyphPage(GlyphPage::pageNumberForCodePoint(space));

    // Ask for the glyph for 0 to avoid paging in ZERO WIDTH SPACE. Control characters, including 0,
    // are mapped to the ZERO WIDTH SPACE glyph.
    if (glyphPageZero)
        m_zeroWidthSpaceGlyph = glyphPageZero->glyphDataForCharacter(0).glyph;

    // Nasty hack to determine if we should round or ceil space widths.
    // If the font is monospace or fake monospace we ceil to ensure that 
    // every character and the space are the same width. Otherwise we round.
    if (glyphPageSpace)
        m_spaceGlyph = glyphPageSpace->glyphDataForCharacter(space).glyph;
    float width = widthForGlyph(m_spaceGlyph);
    m_spaceWidth = width;
    if (glyphPageCharacterZero)
        m_zeroGlyph = glyphPageCharacterZero->glyphDataForCharacter('0').glyph;
    m_fontMetrics.setZeroWidth(widthForGlyph(m_zeroGlyph));
    determinePitch();
    m_adjustedSpaceWidth = m_treatAsFixedPitch ? ceilf(width) : roundf(width);

    // Force the glyph for ZERO WIDTH SPACE to have zero width, unless it is shared with SPACE.
    // Helvetica is an example of a non-zero width ZERO WIDTH SPACE glyph.
    // See <http://bugs.webkit.org/show_bug.cgi?id=13178> and Font::isZeroWidthSpaceGlyph()
    if (m_zeroWidthSpaceGlyph == m_spaceGlyph)
        m_zeroWidthSpaceGlyph = 0;
}

Font::~Font()
{
    removeFromSystemFallbackCache();
}

static bool fillGlyphPage(GlyphPage& pageToFill, UChar* buffer, unsigned bufferLength, const Font& font)
{
    bool hasGlyphs = pageToFill.fill(buffer, bufferLength);
#if ENABLE(OPENTYPE_VERTICAL)
    if (hasGlyphs && font.verticalData())
        font.verticalData()->substituteWithVerticalGlyphs(&font, &pageToFill);
#else
    UNUSED_PARAM(font);
#endif
    return hasGlyphs;
}

static RefPtr<GlyphPage> createAndFillGlyphPage(unsigned pageNumber, const Font& font)
{
#if PLATFORM(IOS)
    // FIXME: Times New Roman contains Arabic glyphs, but Core Text doesn't know how to shape them. See <rdar://problem/9823975>.
    // Once we have the fix for <rdar://problem/9823975> then remove this code together with Font::shouldNotBeUsedForArabic()
    // in <rdar://problem/12096835>.
    if (GlyphPage::pageNumberIsUsedForArabic(pageNumber) && font.shouldNotBeUsedForArabic())
        return nullptr;
#endif

    unsigned glyphPageSize = GlyphPage::sizeForPageNumber(pageNumber);

    unsigned start = GlyphPage::startingCodePointInPageNumber(pageNumber);
    unsigned end = start + glyphPageSize;
    Vector<UChar> buffer(glyphPageSize * 2 + 2);
    unsigned bufferLength;
    // Fill in a buffer with the entire "page" of characters that we want to look up glyphs for.
    if (U_IS_BMP(start)) {
        bufferLength = glyphPageSize;
        for (unsigned i = 0; i < bufferLength; i++)
            buffer[i] = start + i;

        // Code points 0x0 - 0x20 and 0x7F - 0xA0 are control character and shouldn't render. Map them to ZERO WIDTH SPACE.
        auto overwriteCodePoints = [&](unsigned minimum, unsigned maximum, UChar newCodePoint) {
            unsigned begin = std::max(start, minimum);
            unsigned complete = std::min(end, maximum);
            for (unsigned i = begin; i < complete; ++i)
                buffer[i - start] = newCodePoint;
        };

        auto overwriteCodePoint = [&](UChar codePoint, UChar newCodePoint) {
            if (codePoint >= start && codePoint < end)
                buffer[codePoint - start] = newCodePoint;
        };

        overwriteCodePoints(0x0, 0x20, zeroWidthSpace);
        overwriteCodePoints(0x7F, 0xA0, zeroWidthSpace);
        overwriteCodePoint(softHyphen, zeroWidthSpace);
        overwriteCodePoint('\n', space);
        overwriteCodePoint('\t', space);
        overwriteCodePoint(noBreakSpace, space);
        overwriteCodePoint(narrowNoBreakSpace, zeroWidthSpace);
        overwriteCodePoint(leftToRightMark, zeroWidthSpace);
        overwriteCodePoint(rightToLeftMark, zeroWidthSpace);
        overwriteCodePoint(leftToRightEmbed, zeroWidthSpace);
        overwriteCodePoint(rightToLeftEmbed, zeroWidthSpace);
        overwriteCodePoint(leftToRightOverride, zeroWidthSpace);
        overwriteCodePoint(rightToLeftOverride, zeroWidthSpace);
        overwriteCodePoint(leftToRightIsolate, zeroWidthSpace);
        overwriteCodePoint(rightToLeftIsolate, zeroWidthSpace);
        overwriteCodePoint(zeroWidthNonJoiner, zeroWidthSpace);
        overwriteCodePoint(zeroWidthJoiner, zeroWidthSpace);
        overwriteCodePoint(popDirectionalFormatting, zeroWidthSpace);
        overwriteCodePoint(popDirectionalIsolate, zeroWidthSpace);
        overwriteCodePoint(firstStrongIsolate, zeroWidthSpace);
        overwriteCodePoint(objectReplacementCharacter, zeroWidthSpace);
        overwriteCodePoint(zeroWidthNoBreakSpace, zeroWidthSpace);
    } else {
        bufferLength = glyphPageSize * 2;
        for (unsigned i = 0; i < glyphPageSize; i++) {
            int c = i + start;
            buffer[i * 2] = U16_LEAD(c);
            buffer[i * 2 + 1] = U16_TRAIL(c);
        }
    }

    // Now that we have a buffer full of characters, we want to get back an array
    // of glyph indices. This part involves calling into the platform-specific
    // routine of our glyph map for actually filling in the page with the glyphs.
    // Success is not guaranteed. For example, Times fails to fill page 260, giving glyph data
    // for only 128 out of 256 characters.
    Ref<GlyphPage> glyphPage = GlyphPage::create(font);

    bool haveGlyphs = fillGlyphPage(glyphPage, buffer.data(), bufferLength, font);
    if (!haveGlyphs)
        return nullptr;

    return WTFMove(glyphPage);
}

const GlyphPage* Font::glyphPage(unsigned pageNumber) const
{
    if (!pageNumber) {
        if (!m_glyphPageZero)
            m_glyphPageZero = createAndFillGlyphPage(0, *this);
        return m_glyphPageZero.get();
    }
    auto addResult = m_glyphPages.add(pageNumber, nullptr);
    if (addResult.isNewEntry)
        addResult.iterator->value = createAndFillGlyphPage(pageNumber, *this);

    return addResult.iterator->value.get();
}

Glyph Font::glyphForCharacter(UChar32 character) const
{
    auto* page = glyphPage(GlyphPage::pageNumberForCodePoint(character));
    if (!page)
        return 0;
    return page->glyphForCharacter(character);
}

GlyphData Font::glyphDataForCharacter(UChar32 character) const
{
    auto* page = glyphPage(GlyphPage::pageNumberForCodePoint(character));
    if (!page)
        return GlyphData();
    return page->glyphDataForCharacter(character);
}

auto Font::ensureDerivedFontData() const -> DerivedFonts&
{
    if (!m_derivedFontData)
        m_derivedFontData = std::make_unique<DerivedFonts>();
    return *m_derivedFontData;
}

const Font& Font::verticalRightOrientationFont() const
{
    DerivedFonts& derivedFontData = ensureDerivedFontData();
    if (!derivedFontData.verticalRightOrientationFont) {
        auto verticalRightPlatformData = FontPlatformData::cloneWithOrientation(m_platformData, Horizontal);
        derivedFontData.verticalRightOrientationFont = create(verticalRightPlatformData, origin(), Interstitial::No, Visibility::Visible, OrientationFallback::Yes);
    }
    ASSERT(derivedFontData.verticalRightOrientationFont != this);
    return *derivedFontData.verticalRightOrientationFont;
}

const Font& Font::uprightOrientationFont() const
{
    DerivedFonts& derivedFontData = ensureDerivedFontData();
    if (!derivedFontData.uprightOrientationFont)
        derivedFontData.uprightOrientationFont = create(m_platformData, origin(), Interstitial::No, Visibility::Visible, OrientationFallback::Yes);
    ASSERT(derivedFontData.uprightOrientationFont != this);
    return *derivedFontData.uprightOrientationFont;
}

const Font& Font::invisibleFont() const
{
    DerivedFonts& derivedFontData = ensureDerivedFontData();
    if (!derivedFontData.invisibleFont)
        derivedFontData.invisibleFont = create(m_platformData, origin(), Interstitial::Yes, Visibility::Invisible);
    ASSERT(derivedFontData.invisibleFont != this);
    return *derivedFontData.invisibleFont;
}

const Font* Font::smallCapsFont(const FontDescription& fontDescription) const
{
    DerivedFonts& derivedFontData = ensureDerivedFontData();
    if (!derivedFontData.smallCapsFont)
        derivedFontData.smallCapsFont = createScaledFont(fontDescription, smallCapsFontSizeMultiplier);
    ASSERT(derivedFontData.smallCapsFont != this);
    return derivedFontData.smallCapsFont.get();
}

const Font& Font::noSynthesizableFeaturesFont() const
{
#if PLATFORM(COCOA)
    DerivedFonts& derivedFontData = ensureDerivedFontData();
    if (!derivedFontData.noSynthesizableFeaturesFont)
        derivedFontData.noSynthesizableFeaturesFont = createFontWithoutSynthesizableFeatures();
    ASSERT(derivedFontData.noSynthesizableFeaturesFont != this);
    return *derivedFontData.noSynthesizableFeaturesFont;
#else
    return *this;
#endif
}

const Font* Font::emphasisMarkFont(const FontDescription& fontDescription) const
{
    DerivedFonts& derivedFontData = ensureDerivedFontData();
    if (!derivedFontData.emphasisMarkFont)
        derivedFontData.emphasisMarkFont = createScaledFont(fontDescription, emphasisMarkFontSizeMultiplier);
    ASSERT(derivedFontData.emphasisMarkFont != this);
    return derivedFontData.emphasisMarkFont.get();
}

const Font& Font::brokenIdeographFont() const
{
    DerivedFonts& derivedFontData = ensureDerivedFontData();
    if (!derivedFontData.brokenIdeographFont) {
        derivedFontData.brokenIdeographFont = create(m_platformData, origin(), Interstitial::No);
        derivedFontData.brokenIdeographFont->m_isBrokenIdeographFallback = true;
    }
    ASSERT(derivedFontData.brokenIdeographFont != this);
    return *derivedFontData.brokenIdeographFont;
}

#ifndef NDEBUG
String Font::description() const
{
    if (origin() == Origin::Remote)
        return "[custom font]";

    return platformData().description();
}
#endif

const OpenTypeMathData* Font::mathData() const
{
    if (isInterstitial())
        return nullptr;
    if (!m_mathData) {
        m_mathData = OpenTypeMathData::create(m_platformData);
        if (!m_mathData->hasMathData())
            m_mathData = nullptr;
    }
    return m_mathData.get();
}

RefPtr<Font> Font::createScaledFont(const FontDescription& fontDescription, float scaleFactor) const
{
    return platformCreateScaledFont(fontDescription, scaleFactor);
}

bool Font::applyTransforms(GlyphBufferGlyph* glyphs, GlyphBufferAdvance* advances, size_t glyphCount, bool enableKerning, bool requiresShaping) const
{
#if PLATFORM(COCOA)
    CTFontTransformOptions options = (enableKerning ? kCTFontTransformApplyPositioning : 0) | (requiresShaping ? kCTFontTransformApplyShaping : 0);
    return CTFontTransformGlyphs(m_platformData.ctFont(), glyphs, reinterpret_cast<CGSize*>(advances), glyphCount, options);
#else
    UNUSED_PARAM(glyphs);
    UNUSED_PARAM(advances);
    UNUSED_PARAM(glyphCount);
    UNUSED_PARAM(enableKerning);
    UNUSED_PARAM(requiresShaping);
    return false;
#endif
}

class CharacterFallbackMapKey {
public:
    CharacterFallbackMapKey()
    {
    }

    CharacterFallbackMapKey(const AtomicString& locale, UChar32 character, bool isForPlatformFont)
        : locale(locale)
        , character(character)
        , isForPlatformFont(isForPlatformFont)
    {
    }

    CharacterFallbackMapKey(WTF::HashTableDeletedValueType)
        : character(-1)
    {
    }

    bool isHashTableDeletedValue() const { return character == -1; }

    bool operator==(const CharacterFallbackMapKey& other) const
    {
        return locale == other.locale && character == other.character && isForPlatformFont == other.isForPlatformFont;
    }

    static const bool emptyValueIsZero = true;

private:
    friend struct CharacterFallbackMapKeyHash;

    AtomicString locale;
    UChar32 character { 0 };
    bool isForPlatformFont { false };
};

struct CharacterFallbackMapKeyHash {
    static unsigned hash(const CharacterFallbackMapKey& key)
    {
        IntegerHasher hasher;
        hasher.add(key.character);
        hasher.add(key.isForPlatformFont);
        hasher.add(key.locale.existingHash());
        return hasher.hash();
    }

    static bool equal(const CharacterFallbackMapKey& a, const CharacterFallbackMapKey& b)
    {
        return a == b;
    }

    static const bool safeToCompareToEmptyOrDeleted = true;
};

// Fonts are not ref'd to avoid cycles.
// FIXME: Shouldn't these be WeakPtrs?
typedef HashMap<CharacterFallbackMapKey, Font*, CharacterFallbackMapKeyHash, WTF::SimpleClassHashTraits<CharacterFallbackMapKey>> CharacterFallbackMap;
typedef HashMap<const Font*, CharacterFallbackMap> SystemFallbackCache;

static SystemFallbackCache& systemFallbackCache()
{
    static NeverDestroyed<SystemFallbackCache> map;
    return map.get();
}

RefPtr<Font> Font::systemFallbackFontForCharacter(UChar32 character, const FontDescription& description, bool isForPlatformFont) const
{
    auto fontAddResult = systemFallbackCache().add(this, CharacterFallbackMap());

    if (!character) {
        UChar codeUnit = 0;
        return FontCache::singleton().systemFallbackForCharacters(description, this, isForPlatformFont, &codeUnit, 1);
    }

    auto key = CharacterFallbackMapKey(description.locale(), character, isForPlatformFont);
    auto characterAddResult = fontAddResult.iterator->value.add(WTFMove(key), nullptr);

    Font*& fallbackFont = characterAddResult.iterator->value;

    if (!fallbackFont) {
        UChar codeUnits[2];
        unsigned codeUnitsLength;
        if (U_IS_BMP(character)) {
            codeUnits[0] = FontCascade::normalizeSpaces(character);
            codeUnitsLength = 1;
        } else {
            codeUnits[0] = U16_LEAD(character);
            codeUnits[1] = U16_TRAIL(character);
            codeUnitsLength = 2;
        }

        fallbackFont = FontCache::singleton().systemFallbackForCharacters(description, this, isForPlatformFont, codeUnits, codeUnitsLength).get();
        if (fallbackFont)
            fallbackFont->m_isUsedInSystemFallbackCache = true;
    }

    return fallbackFont;
}

void Font::removeFromSystemFallbackCache()
{
    systemFallbackCache().remove(this);

    if (!m_isUsedInSystemFallbackCache)
        return;

    for (auto& characterMap : systemFallbackCache().values()) {
        Vector<CharacterFallbackMapKey, 512> toRemove;
        for (auto& entry : characterMap) {
            if (entry.value == this)
                toRemove.append(entry.key);
        }
        for (auto& key : toRemove)
            characterMap.remove(key);
    }
}

#if !PLATFORM(COCOA)
bool Font::variantCapsSupportsCharacterForSynthesis(FontVariantCaps fontVariantCaps, UChar32) const
{
    switch (fontVariantCaps) {
    case FontVariantCaps::Small:
    case FontVariantCaps::Petite:
    case FontVariantCaps::AllSmall:
    case FontVariantCaps::AllPetite:
        return false;
    default:
        // Synthesis only supports the variant-caps values listed above.
        return true;
    }
}
#endif

} // namespace WebCore
