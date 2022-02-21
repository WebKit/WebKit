/*
 * Copyright (C) 2005-2021 Apple Inc. All rights reserved.
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
#include <pal/spi/cf/CoreTextSPI.h>
#endif

#include "CachedFont.h"
#include "CharacterProperties.h"
#include "FontCache.h"
#include "FontCascade.h"
#include "FontCustomPlatformData.h"
#include "OpenTypeMathData.h"
#include "SharedBuffer.h"
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/AtomStringHash.h>

#if ENABLE(OPENTYPE_VERTICAL)
#include "OpenTypeVerticalData.h"
#endif

namespace WebCore {

unsigned GlyphPage::s_count = 0;

const float smallCapsFontSizeMultiplier = 0.7f;
const float emphasisMarkFontSizeMultiplier = 0.5f;

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(Font);

Ref<Font> Font::create(const FontPlatformData& platformData, Origin origin, Interstitial interstitial, Visibility visibility, OrientationFallback orientationFallback, std::optional<RenderingResourceIdentifier> identifier)
{
    return adoptRef(*new Font(platformData, origin, interstitial, visibility, orientationFallback, identifier));
}

Ref<Font> Font::create(Ref<SharedBuffer>&& fontFaceData, Font::Origin origin, float fontSize, bool syntheticBold, bool syntheticItalic)
{
    bool wrapping;
    auto customFontData = CachedFont::createCustomFontData(fontFaceData.get(), { }, wrapping);
    FontDescription description;
    description.setComputedSize(fontSize);
    // FIXME: Why doesn't this pass in any meaningful data for the last few arguments?
    auto platformData = CachedFont::platformDataFromCustomData(*customFontData, description, syntheticBold, syntheticItalic, { });
    return Font::create(WTFMove(platformData), origin);
}

Font::Font(const FontPlatformData& platformData, Origin origin, Interstitial interstitial, Visibility visibility, OrientationFallback orientationFallback, std::optional<RenderingResourceIdentifier> renderingResourceIdentifier)
    : m_platformData(platformData)
    , m_renderingResourceIdentifier(renderingResourceIdentifier)
    , m_origin(origin)
    , m_visibility(visibility)
    , m_treatAsFixedPitch(false)
    , m_isInterstitial(interstitial == Interstitial::Yes)
    , m_isTextOrientationFallback(orientationFallback == OrientationFallback::Yes)
    , m_isBrokenIdeographFallback(false)
    , m_hasVerticalGlyphs(false)
    , m_isUsedInSystemFallbackCache(false)
    , m_allowsAntialiasing(true)
#if PLATFORM(IOS_FAMILY)
    , m_shouldNotBeUsedForArabic(false)
#endif
{
    platformInit();
    platformGlyphInit();
    platformCharWidthInit();
#if ENABLE(OPENTYPE_VERTICAL)
    if (platformData.orientation() == FontOrientation::Vertical && orientationFallback == OrientationFallback::No) {
        m_verticalData = FontCache::forCurrentThread().verticalData(platformData);
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
#if USE(FREETYPE)
    auto* glyphPageZeroWidthSpace = glyphPage(GlyphPage::pageNumberForCodePoint(zeroWidthSpace));
    UChar32 zeroWidthSpaceCharacter = zeroWidthSpace;
#else
    // Ask for the glyph for 0 to avoid paging in ZERO WIDTH SPACE. Control characters, including 0,
    // are mapped to the ZERO WIDTH SPACE glyph for non FreeType based ports.
    auto* glyphPageZeroWidthSpace = glyphPage(0);
    UChar32 zeroWidthSpaceCharacter = 0;
#endif

    if (glyphPageZeroWidthSpace)
        m_zeroWidthSpaceGlyph = glyphPageZeroWidthSpace->glyphDataForCharacter(zeroWidthSpaceCharacter).glyph;

    if (auto* page = glyphPage(GlyphPage::pageNumberForCodePoint(space)))
        m_spaceGlyph = page->glyphDataForCharacter(space).glyph;

    // Force the glyph for ZERO WIDTH SPACE to have zero width, unless it is shared with SPACE.
    // Helvetica is an example of a non-zero width ZERO WIDTH SPACE glyph.
    // See <http://bugs.webkit.org/show_bug.cgi?id=13178> and Font::isZeroWidthSpaceGlyph()
    if (m_zeroWidthSpaceGlyph == m_spaceGlyph)
        m_zeroWidthSpaceGlyph = 0;

    // widthForGlyph depends on m_zeroWidthSpaceGlyph having the correct value.
    // Therefore all calls to widthForGlyph must happen after this point.

    Glyph zeroGlyph = { 0 };
    if (auto* page = glyphPage(GlyphPage::pageNumberForCodePoint('0')))
        zeroGlyph = page->glyphDataForCharacter('0').glyph;
    if (zeroGlyph)
        m_fontMetrics.setZeroWidth(widthForGlyph(zeroGlyph));

    // Use the width of the CJK water ideogram (U+6C34) as the
    // approximated width of ideograms in the font, as mentioned in
    // https://www.w3.org/TR/css-values-4/#ic. This is currently only used
    // to support the ic unit. If the width is not available, falls back to
    // 1em as specified.
    if (auto* page = glyphPage(GlyphPage::pageNumberForCodePoint(cjkWater))) {
        auto glyph = page->glyphDataForCharacter(cjkWater).glyph;
        m_fontMetrics.setIdeogramWidth(widthForGlyph(glyph));
    } else
        m_fontMetrics.setIdeogramWidth(platformData().size());

    m_spaceWidth = widthForGlyph(m_spaceGlyph, SyntheticBoldInclusion::Exclude); // spaceWidth() handles adding in the synthetic bold.
    auto amountToAdjustLineGap = std::min(m_fontMetrics.floatLineGap(), 0.0f);
    m_fontMetrics.setLineGap(m_fontMetrics.floatLineGap() - amountToAdjustLineGap);
    m_fontMetrics.setLineSpacing(m_fontMetrics.floatLineSpacing() - amountToAdjustLineGap);
    determinePitch();
}

Font::~Font()
{
    removeFromSystemFallbackCache();
}

RenderingResourceIdentifier Font::renderingResourceIdentifier() const
{
    if (!m_renderingResourceIdentifier)
        m_renderingResourceIdentifier = RenderingResourceIdentifier::generate();
    return *m_renderingResourceIdentifier;
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

static std::optional<size_t> codePointSupportIndex(UChar32 codePoint)
{
    // FIXME: Consider reordering these so the most common ones are at the front.
    // Doing this could cause the BitVector to fit inside inline storage and therefore
    // be both a performance and a memory progression.
    if (codePoint < 0x20)
        return codePoint;
    if (codePoint >= 0x7F && codePoint < 0xA0)
        return codePoint - 0x7F + 0x20;
    std::optional<size_t> result;
    switch (codePoint) {
    case softHyphen:
        result = 0x41;
        break;
    case newlineCharacter:
        result = 0x42;
        break;
    case tabCharacter:
        result = 0x43;
        break;
    case noBreakSpace:
        result = 0x44;
        break;
    case narrowNoBreakSpace:
        result = 0x45;
        break;
    case leftToRightMark:
        result = 0x46;
        break;
    case rightToLeftMark:
        result = 0x47;
        break;
    case leftToRightEmbed:
        result = 0x48;
        break;
    case rightToLeftEmbed:
        result = 0x49;
        break;
    case leftToRightOverride:
        result = 0x4A;
        break;
    case rightToLeftOverride:
        result = 0x4B;
        break;
    case leftToRightIsolate:
        result = 0x4C;
        break;
    case rightToLeftIsolate:
        result = 0x4D;
        break;
    case zeroWidthNonJoiner:
        result = 0x4E;
        break;
    case zeroWidthJoiner:
        result = 0x4F;
        break;
    case popDirectionalFormatting:
        result = 0x50;
        break;
    case popDirectionalIsolate:
        result = 0x51;
        break;
    case firstStrongIsolate:
        result = 0x52;
        break;
    case objectReplacementCharacter:
        result = 0x53;
        break;
    case zeroWidthNoBreakSpace:
        result = 0x54;
        break;
    default:
        result = std::nullopt;
    }

#ifndef NDEBUG
    UChar32 codePointOrder[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
        0x7F,
        0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
        0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
        softHyphen,
        newlineCharacter,
        tabCharacter,
        noBreakSpace,
        narrowNoBreakSpace,
        leftToRightMark,
        rightToLeftMark,
        leftToRightEmbed,
        rightToLeftEmbed,
        leftToRightOverride,
        rightToLeftOverride,
        leftToRightIsolate,
        rightToLeftIsolate,
        zeroWidthNonJoiner,
        zeroWidthJoiner,
        popDirectionalFormatting,
        popDirectionalIsolate,
        firstStrongIsolate,
        objectReplacementCharacter,
        zeroWidthNoBreakSpace
    };
    bool found = false;
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(codePointOrder); ++i) {
        if (codePointOrder[i] == codePoint) {
            ASSERT(i == result);
            found = true;
        }
    }
    ASSERT(found == static_cast<bool>(result));
#endif
    return result;
}

#if PLATFORM(WIN)
static void overrideControlCharacters(Vector<UChar>& buffer, unsigned start, unsigned end)
{
    auto overwriteCodePoints = [&](unsigned minimum, unsigned maximum, UChar newCodePoint) {
        unsigned begin = std::max(start, minimum);
        unsigned complete = std::min(end, maximum);
        for (unsigned i = begin; i < complete; ++i) {
            ASSERT(codePointSupportIndex(i));
            buffer[i - start] = newCodePoint;
        }
    };

    auto overwriteCodePoint = [&](UChar codePoint, UChar newCodePoint) {
        ASSERT(codePointSupportIndex(codePoint));
        if (codePoint >= start && codePoint < end)
            buffer[codePoint - start] = newCodePoint;
    };

    // Code points 0x0 - 0x20 and 0x7F - 0xA0 are control character and shouldn't render. Map them to ZERO WIDTH SPACE.
    overwriteCodePoints(nullCharacter, space, zeroWidthSpace);
    overwriteCodePoints(deleteCharacter, noBreakSpace, zeroWidthSpace);
    overwriteCodePoint(softHyphen, zeroWidthSpace);
    overwriteCodePoint(newlineCharacter, space);
    overwriteCodePoint(tabCharacter, space);
    overwriteCodePoint(noBreakSpace, space);
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
}
#endif

static RefPtr<GlyphPage> createAndFillGlyphPage(unsigned pageNumber, const Font& font)
{
#if PLATFORM(IOS_FAMILY)
    // FIXME: Times New Roman contains Arabic glyphs, but Core Text doesn't know how to shape them. See <rdar://problem/9823975>.
    // Once we have the fix for <rdar://problem/9823975> then remove this code together with Font::shouldNotBeUsedForArabic()
    // in <rdar://problem/12096835>.
    if (GlyphPage::pageNumberIsUsedForArabic(pageNumber) && font.shouldNotBeUsedForArabic())
        return nullptr;
#endif

    unsigned glyphPageSize = GlyphPage::sizeForPageNumber(pageNumber);

    unsigned start = GlyphPage::startingCodePointInPageNumber(pageNumber);
    Vector<UChar> buffer(glyphPageSize * 2 + 2);
    unsigned bufferLength;
    if (U_IS_BMP(start)) {
        bufferLength = glyphPageSize;
        for (unsigned i = 0; i < bufferLength; i++)
            buffer[i] = start + i;

#if PLATFORM(WIN)
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=215318 Delete this and use https://bugs.webkit.org/show_bug.cgi?id=215643 on Windows.
        overrideControlCharacters(buffer, start, start + glyphPageSize);
#endif
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

    return glyphPage;
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
        m_derivedFontData = makeUnique<DerivedFonts>();
    return *m_derivedFontData;
}

const Font& Font::verticalRightOrientationFont() const
{
    DerivedFonts& derivedFontData = ensureDerivedFontData();
    if (!derivedFontData.verticalRightOrientationFont) {
        auto verticalRightPlatformData = FontPlatformData::cloneWithOrientation(m_platformData, FontOrientation::Horizontal);
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

#if !USE(CORE_TEXT)

bool Font::isProbablyOnlyUsedToRenderIcons() const
{
    // FIXME: Not implemented yet.
    return false;
}

#endif

#if !LOG_DISABLED
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

#if !USE(CORE_TEXT)
GlyphBufferAdvance Font::applyTransforms(GlyphBuffer&, unsigned, unsigned, bool, bool, const AtomString&, StringView, TextDirection) const
{
    return makeGlyphBufferAdvance();
}
#endif

struct CharacterFallbackMapKey {
    AtomString locale;
    UChar32 character { 0 };
    bool isForPlatformFont { false };
};

inline bool operator==(const CharacterFallbackMapKey& a, const CharacterFallbackMapKey& b)
{
    return a.locale == b.locale && a.character == b.character && a.isForPlatformFont == b.isForPlatformFont;
}

struct CharacterFallbackMapKeyHash {
    static unsigned hash(const CharacterFallbackMapKey& key) { return computeHash(key.locale, key.character, key.isForPlatformFont); }
    static bool equal(const CharacterFallbackMapKey& a, const CharacterFallbackMapKey& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

struct CharacterFallbackMapKeyHashTraits : SimpleClassHashTraits<CharacterFallbackMapKey> {
    static void constructDeletedValue(CharacterFallbackMapKey& slot) { new (NotNull, &slot) CharacterFallbackMapKey { { }, U_SENTINEL, { } }; }
    static bool isDeletedValue(const CharacterFallbackMapKey& key) { return key.character == U_SENTINEL; }
};

// Fonts are not ref'd to avoid cycles.
// FIXME: Consider changing these maps to use WeakPtr instead of raw pointers.
using CharacterFallbackMap = HashMap<CharacterFallbackMapKey, Font*, CharacterFallbackMapKeyHash, CharacterFallbackMapKeyHashTraits>;
using SystemFallbackCache = HashMap<const Font*, CharacterFallbackMap>;

static SystemFallbackCache& systemFallbackCache()
{
    static NeverDestroyed<SystemFallbackCache> map;
    return map.get();
}

RefPtr<Font> Font::systemFallbackFontForCharacter(UChar32 character, const FontDescription& description, IsForPlatformFont isForPlatformFont) const
{
    auto fontAddResult = systemFallbackCache().add(this, CharacterFallbackMap());

    if (!character) {
        UChar codeUnit = 0;
        return FontCache::forCurrentThread().systemFallbackForCharacters(description, this, isForPlatformFont, FontCache::PreferColoredFont::No, &codeUnit, 1);
    }

    auto key = CharacterFallbackMapKey { description.computedLocale(), character, isForPlatformFont != IsForPlatformFont::No };
    return fontAddResult.iterator->value.ensure(WTFMove(key), [&] {
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
        auto font = FontCache::forCurrentThread().systemFallbackForCharacters(description, this, isForPlatformFont, FontCache::PreferColoredFont::No, codeUnits, codeUnitsLength).get();
        if (font)
            font->m_isUsedInSystemFallbackCache = true;
        return font;
    }).iterator->value;
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

#if !PLATFORM(COCOA) && !USE(FREETYPE)
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

bool Font::supportsCodePoint(UChar32 character) const
{
    // This is very similar to static_cast<bool>(glyphForCharacter(character))
    // except that glyphForCharacter() maps certain code points to ZWS (because they
    // shouldn't be visible). This function doesn't do that mapping, and instead is
    // as honest as possible about what code points the font supports. This is so
    // that we can accurately determine which characters are supported by this font
    // so we know which boundaries to break strings when we send them to the complex
    // text codepath. The complex text codepath is totally separate from this ZWS
    // replacement logic (because CoreText handles those characters instead of WebKit).
    if (auto index = codePointSupportIndex(character)) {
        m_codePointSupport.ensureSize(2 * (*index + 1));
        bool hasBeenSet = m_codePointSupport.quickSet(2 * *index);
        if (!hasBeenSet && platformSupportsCodePoint(character))
            m_codePointSupport.quickSet(2 * *index + 1);
        return m_codePointSupport.quickGet(2 * *index + 1);
    }
    return glyphForCharacter(character);
}

bool Font::canRenderCombiningCharacterSequence(const UChar* characters, size_t length) const
{
    ASSERT(isMainThread());

    auto codePoints = StringView(characters, length).codePoints();
    auto it = codePoints.begin();
    auto end = codePoints.end();
    while (it != end) {
        auto codePoint = *it;
        ++it;

        if (it != end && isVariationSelector(*it)) {
            if (!platformSupportsCodePoint(codePoint, *it)) {
                // Try the characters individually.
                if (!supportsCodePoint(codePoint) || !supportsCodePoint(*it))
                    return false;
            }
            ++it;
            continue;
        }

        if (!supportsCodePoint(codePoint))
            return false;
    }
    return true;
}

// Don't store the result of this! The hash map is free to rehash at any point, leaving this reference dangling.
const Path& Font::pathForGlyph(Glyph glyph) const
{
    if (const auto& path = m_glyphPathMap.existingMetricsForGlyph(glyph))
        return *path;
    auto path = platformPathForGlyph(glyph);
    m_glyphPathMap.setMetricsForGlyph(glyph, path);
    return *m_glyphPathMap.existingMetricsForGlyph(glyph);
}

} // namespace WebCore
