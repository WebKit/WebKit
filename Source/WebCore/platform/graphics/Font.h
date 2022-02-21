/*
 * This file is part of the internal font implementation.
 *
 * Copyright (C) 2006-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2007-2008 Torch Mobile, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include "FloatRect.h"
#include "FontMetrics.h"
#include "FontPlatformData.h"
#include "GlyphBuffer.h"
#include "GlyphMetricsMap.h"
#include "GlyphPage.h"
#include "OpenTypeMathData.h"
#include "RenderingResourceIdentifier.h"
#include <wtf/BitVector.h>
#include <wtf/Hasher.h>
#include <wtf/text/StringHash.h>

#if PLATFORM(COCOA)
#include <CoreFoundation/CoreFoundation.h>
#include <pal/cf/OTSVGTable.h>
#include <wtf/RetainPtr.h>
#endif

#if ENABLE(OPENTYPE_VERTICAL)
#include "OpenTypeVerticalData.h"
#endif

#if PLATFORM(WIN)
#include <usp10.h>
#endif

namespace WebCore {

class FontCache;
class FontDescription;
class GlyphPage;
class FragmentedSharedBuffer;

struct GlyphData;

enum FontVariant { AutoVariant, NormalVariant, SmallCapsVariant, EmphasisMarkVariant, BrokenIdeographVariant };
enum Pitch { UnknownPitch, FixedPitch, VariablePitch };
enum class IsForPlatformFont : bool { No, Yes };

#if USE(CORE_TEXT)
bool fontHasTable(CTFontRef, unsigned tableTag);
bool fontHasEitherTable(CTFontRef, unsigned tableTag1, unsigned tableTag2);
#endif

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(Font);
class Font : public RefCounted<Font> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Font);
public:
    // Used to create platform fonts.
    enum class Origin : bool { Remote, Local };
    enum class Interstitial : bool { No, Yes };
    enum class Visibility : bool { Visible, Invisible };
    enum class OrientationFallback : bool { No, Yes };
    WEBCORE_EXPORT static Ref<Font> create(const FontPlatformData&, Origin = Origin::Local, Interstitial = Interstitial::No, Visibility = Visibility::Visible, OrientationFallback = OrientationFallback::No, std::optional<RenderingResourceIdentifier> = std::nullopt);
    WEBCORE_EXPORT static Ref<Font> create(Ref<SharedBuffer>&& fontFaceData, Font::Origin, float fontSize, bool syntheticBold, bool syntheticItalic);

    WEBCORE_EXPORT ~Font();

    static const Font* systemFallback() { return reinterpret_cast<const Font*>(-1); }

    const FontPlatformData& platformData() const { return m_platformData; }
    const OpenTypeMathData* mathData() const;
#if ENABLE(OPENTYPE_VERTICAL)
    const OpenTypeVerticalData* verticalData() const { return m_verticalData.get(); }
#endif

    WEBCORE_EXPORT RenderingResourceIdentifier renderingResourceIdentifier() const;

    const Font* smallCapsFont(const FontDescription&) const;
    const Font& noSynthesizableFeaturesFont() const;
    const Font* emphasisMarkFont(const FontDescription&) const;
    const Font& brokenIdeographFont() const;

    bool isProbablyOnlyUsedToRenderIcons() const;

    const Font* variantFont(const FontDescription& description, FontVariant variant) const
    {
#if PLATFORM(COCOA)
        ASSERT(variant != SmallCapsVariant);
#endif
        switch (variant) {
        case SmallCapsVariant:
            return smallCapsFont(description);
        case EmphasisMarkVariant:
            return emphasisMarkFont(description);
        case BrokenIdeographVariant:
            return &brokenIdeographFont();
        case AutoVariant:
        case NormalVariant:
            break;
        }
        ASSERT_NOT_REACHED();
        return const_cast<Font*>(this);
    }

    bool variantCapsSupportsCharacterForSynthesis(FontVariantCaps, UChar32) const;

    const Font& verticalRightOrientationFont() const;
    const Font& uprightOrientationFont() const;
    const Font& invisibleFont() const;

    bool hasVerticalGlyphs() const { return m_hasVerticalGlyphs; }
    bool isTextOrientationFallback() const { return m_isTextOrientationFallback; }

    const FontMetrics& fontMetrics() const { return m_fontMetrics; }
    float sizePerUnit() const { return platformData().size() / (fontMetrics().unitsPerEm() ? fontMetrics().unitsPerEm() : 1); }

    float maxCharWidth() const { return m_maxCharWidth; }
    void setMaxCharWidth(float maxCharWidth) { m_maxCharWidth = maxCharWidth; }

    float avgCharWidth() const { return m_avgCharWidth; }
    void setAvgCharWidth(float avgCharWidth) { m_avgCharWidth = avgCharWidth; }

    FloatRect boundsForGlyph(Glyph) const;

    // Should the result of this function include the results of synthetic bold?
    enum class SyntheticBoldInclusion {
        Incorporate,
        Exclude
    };
    float widthForGlyph(Glyph, SyntheticBoldInclusion = SyntheticBoldInclusion::Incorporate) const;

    const Path& pathForGlyph(Glyph) const; // Don't store the result of this! The hash map is free to rehash at any point, leaving this reference dangling.

    float spaceWidth(SyntheticBoldInclusion SyntheticBoldInclusion = SyntheticBoldInclusion::Incorporate) const
    {
        return m_spaceWidth + (SyntheticBoldInclusion == SyntheticBoldInclusion::Incorporate ? syntheticBoldOffset() : 0);
    }

    float syntheticBoldOffset() const { return m_syntheticBoldOffset; }

    Glyph spaceGlyph() const { return m_spaceGlyph; }
    Glyph zeroWidthSpaceGlyph() const { return m_zeroWidthSpaceGlyph; }
    bool isZeroWidthSpaceGlyph(Glyph glyph) const { return glyph == m_zeroWidthSpaceGlyph && glyph; }

    GlyphData glyphDataForCharacter(UChar32) const;
    Glyph glyphForCharacter(UChar32) const;
    bool supportsCodePoint(UChar32) const;
    bool platformSupportsCodePoint(UChar32, std::optional<UChar32> variation = std::nullopt) const;

    RefPtr<Font> systemFallbackFontForCharacter(UChar32, const FontDescription&, IsForPlatformFont) const;

    const GlyphPage* glyphPage(unsigned pageNumber) const;

    void determinePitch();
    Pitch pitch() const { return m_treatAsFixedPitch ? FixedPitch : VariablePitch; }

    Origin origin() const { return m_origin; }
    bool isInterstitial() const { return m_isInterstitial; }
    Visibility visibility() const { return m_visibility; }
    bool allowsAntialiasing() const { return m_allowsAntialiasing; }

#if !LOG_DISABLED
    String description() const;
#endif

#if PLATFORM(IOS_FAMILY)
    bool shouldNotBeUsedForArabic() const { return m_shouldNotBeUsedForArabic; };
#endif
#if PLATFORM(COCOA)
    CTFontRef getCTFont() const { return m_platformData.font(); }
    RetainPtr<CFDictionaryRef> getCFStringAttributes(bool enableKerning, FontOrientation, const AtomString& locale) const;
    const BitVector& glyphsSupportedBySmallCaps() const;
    const BitVector& glyphsSupportedByAllSmallCaps() const;
    const BitVector& glyphsSupportedByPetiteCaps() const;
    const BitVector& glyphsSupportedByAllPetiteCaps() const;
#endif

    bool canRenderCombiningCharacterSequence(const UChar*, size_t) const;
    GlyphBufferAdvance applyTransforms(GlyphBuffer&, unsigned beginningGlyphIndex, unsigned beginningStringIndex, bool enableKerning, bool requiresShaping, const AtomString& locale, StringView text, TextDirection) const;

    // Returns nullopt if none of the glyphs are OT-SVG glyphs.
    std::optional<BitVector> findOTSVGGlyphs(const GlyphBufferGlyph*, unsigned count) const;

    bool hasAnyComplexColorFormatGlyphs(const GlyphBufferGlyph*, unsigned count) const;

#if PLATFORM(WIN)
    SCRIPT_FONTPROPERTIES* scriptFontProperties() const;
    SCRIPT_CACHE* scriptCache() const { return &m_scriptCache; }
    static void setShouldApplyMacAscentHack(bool);
    static bool shouldApplyMacAscentHack();
    static float ascentConsideringMacAscentHack(const WCHAR*, float ascent, float descent);
#endif

private:
    WEBCORE_EXPORT Font(const FontPlatformData&, Origin, Interstitial, Visibility, OrientationFallback, std::optional<RenderingResourceIdentifier>);

    void platformInit();
    void platformGlyphInit();
    void platformCharWidthInit();
    void platformDestroy();

    void initCharWidths();

    RefPtr<Font> createFontWithoutSynthesizableFeatures() const;
    RefPtr<Font> createScaledFont(const FontDescription&, float scaleFactor) const;
    RefPtr<Font> platformCreateScaledFont(const FontDescription&, float scaleFactor) const;

    void removeFromSystemFallbackCache();
    
    struct DerivedFonts;
    DerivedFonts& ensureDerivedFontData() const;

#if PLATFORM(WIN)
    void initGDIFont();
    void platformCommonDestroy();
    FloatRect boundsForGDIGlyph(Glyph) const;
    float widthForGDIGlyph(Glyph) const;
#endif

    FloatRect platformBoundsForGlyph(Glyph) const;
    float platformWidthForGlyph(Glyph) const;
    Path platformPathForGlyph(Glyph) const;

#if PLATFORM(COCOA)
    class ComplexColorFormatGlyphs {
    public:
        static ComplexColorFormatGlyphs createWithNoRelevantTables();
        static ComplexColorFormatGlyphs createWithRelevantTablesAndGlyphCount(unsigned glyphCount);

        bool hasValueFor(Glyph) const;
        bool get(Glyph) const;
        void set(Glyph, bool value);

        bool hasRelevantTables() const { return m_hasRelevantTables; }

    private:
        static constexpr size_t bitForInitialized(Glyph glyphID) { return static_cast<size_t>(glyphID) * 2; }
        static constexpr size_t bitForValue(Glyph glyphID) { return static_cast<size_t>(glyphID) * 2 + 1; }
        static constexpr size_t bitsRequiredForGlyphCount(unsigned glyphCount) { return glyphCount * 2; }

        ComplexColorFormatGlyphs(bool hasRelevantTables, unsigned glyphCount)
            : m_hasRelevantTables(hasRelevantTables)
            , m_bits(bitsRequiredForGlyphCount(glyphCount))
        { }

        bool m_hasRelevantTables;
        BitVector m_bits; // pairs of (initialized, value) bits
    };

    const PAL::OTSVGTable& otSVGTable() const;
    bool glyphHasComplexColorFormat(Glyph) const;
    bool hasComplexColorFormatTables() const;
    ComplexColorFormatGlyphs& glyphsWithComplexColorFormat() const;
#endif

    FontMetrics m_fontMetrics;
    float m_maxCharWidth { -1 };
    float m_avgCharWidth { -1 };

    const FontPlatformData m_platformData;

    mutable RefPtr<GlyphPage> m_glyphPageZero;
    mutable HashMap<unsigned, RefPtr<GlyphPage>> m_glyphPages;
    mutable std::unique_ptr<GlyphMetricsMap<FloatRect>> m_glyphToBoundsMap;
    mutable GlyphMetricsMap<float> m_glyphToWidthMap;
    mutable GlyphMetricsMap<std::optional<Path>> m_glyphPathMap;
    mutable BitVector m_codePointSupport;

    mutable RefPtr<OpenTypeMathData> m_mathData;
#if ENABLE(OPENTYPE_VERTICAL)
    RefPtr<OpenTypeVerticalData> m_verticalData;
#endif

    mutable std::optional<RenderingResourceIdentifier> m_renderingResourceIdentifier;

    struct DerivedFonts {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
    public:

        RefPtr<Font> smallCapsFont;
        RefPtr<Font> noSynthesizableFeaturesFont;
        RefPtr<Font> emphasisMarkFont;
        RefPtr<Font> brokenIdeographFont;
        RefPtr<Font> verticalRightOrientationFont;
        RefPtr<Font> uprightOrientationFont;
        RefPtr<Font> invisibleFont;
    };

    mutable std::unique_ptr<DerivedFonts> m_derivedFontData;

#if PLATFORM(COCOA)
    mutable std::optional<BitVector> m_glyphsSupportedBySmallCaps;
    mutable std::optional<BitVector> m_glyphsSupportedByAllSmallCaps;
    mutable std::optional<BitVector> m_glyphsSupportedByPetiteCaps;
    mutable std::optional<BitVector> m_glyphsSupportedByAllPetiteCaps;
    mutable std::optional<PAL::OTSVGTable> m_otSVGTable;
    mutable std::optional<ComplexColorFormatGlyphs> m_glyphsWithComplexColorFormat; // SVG and sbix
#endif

#if PLATFORM(WIN)
    mutable SCRIPT_CACHE m_scriptCache;
    mutable SCRIPT_FONTPROPERTIES* m_scriptFontProperties;
#endif

    Glyph m_spaceGlyph { 0 };
    Glyph m_zeroWidthSpaceGlyph { 0 };

    Origin m_origin; // Whether or not we are custom font loaded via @font-face
    Visibility m_visibility; // @font-face's internal timer can cause us to show fonts even when a font is being downloaded.

    float m_spaceWidth { 0 };

    float m_syntheticBoldOffset { 0 };

    unsigned m_treatAsFixedPitch : 1;
    unsigned m_isInterstitial : 1; // Whether or not this custom font is the last resort placeholder for a loading font

    unsigned m_isTextOrientationFallback : 1;
    unsigned m_isBrokenIdeographFallback : 1;
    unsigned m_hasVerticalGlyphs : 1;

    unsigned m_isUsedInSystemFallbackCache : 1;
    
    unsigned m_allowsAntialiasing : 1;

#if PLATFORM(IOS_FAMILY)
    unsigned m_shouldNotBeUsedForArabic : 1;
#endif

    // Adding any non-derived information to Font needs a parallel change in WebCoreArgumentCoders.cpp.
};

#if PLATFORM(IOS_FAMILY)
bool fontFamilyShouldNotBeUsedForArabic(CFStringRef);
#endif

ALWAYS_INLINE FloatRect Font::boundsForGlyph(Glyph glyph) const
{
    if (isZeroWidthSpaceGlyph(glyph))
        return FloatRect();

    FloatRect bounds;
    if (m_glyphToBoundsMap) {
        bounds = m_glyphToBoundsMap->metricsForGlyph(glyph);
        if (bounds.width() != cGlyphSizeUnknown)
            return bounds;
    }

    bounds = platformBoundsForGlyph(glyph);
    if (!m_glyphToBoundsMap)
        m_glyphToBoundsMap = makeUnique<GlyphMetricsMap<FloatRect>>();
    m_glyphToBoundsMap->setMetricsForGlyph(glyph, bounds);
    return bounds;
}

ALWAYS_INLINE float Font::widthForGlyph(Glyph glyph, SyntheticBoldInclusion SyntheticBoldInclusion) const
{
    // The optimization of returning 0 for the zero-width-space glyph is incorrect for the LastResort font,
    // used in place of the actual font when isLoading() is true on both macOS and iOS.
    // The zero-width-space glyph in that font does not have a width of zero and, further, that glyph is used
    // for many other characters and must not be zero width when used for them.
    if (isZeroWidthSpaceGlyph(glyph) && !isInterstitial())
        return 0;

    float width = m_glyphToWidthMap.metricsForGlyph(glyph);
    if (width != cGlyphSizeUnknown)
        return width + (SyntheticBoldInclusion == SyntheticBoldInclusion::Incorporate ? syntheticBoldOffset() : 0);

#if ENABLE(OPENTYPE_VERTICAL)
    if (m_verticalData)
        width = m_verticalData->advanceHeight(this, glyph);
    else
#endif
        width = platformWidthForGlyph(glyph);

    m_glyphToWidthMap.setMetricsForGlyph(glyph, width);
    return width + (SyntheticBoldInclusion == SyntheticBoldInclusion::Incorporate ? syntheticBoldOffset() : 0);
}

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::Font::Origin> {
    using values = EnumValues<
        WebCore::Font::Origin,
        WebCore::Font::Origin::Remote,
        WebCore::Font::Origin::Local
    >;
};
template<> struct EnumTraits<WebCore::Font::Interstitial> {
    using values = EnumValues<
        WebCore::Font::Interstitial,
        WebCore::Font::Interstitial::Yes,
        WebCore::Font::Interstitial::No
    >;
};
template<> struct EnumTraits<WebCore::Font::Visibility> {
    using values = EnumValues<
        WebCore::Font::Visibility,
        WebCore::Font::Visibility::Visible,
        WebCore::Font::Visibility::Invisible
    >;
};
template<> struct EnumTraits<WebCore::Font::OrientationFallback> {
    using values = EnumValues<
        WebCore::Font::OrientationFallback,
        WebCore::Font::OrientationFallback::Yes,
        WebCore::Font::OrientationFallback::No
    >;
};

}
