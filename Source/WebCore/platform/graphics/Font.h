/*
 * Copyright (C) 2006-2025 Apple Inc. All rights reserved.
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

#include <WebCore/FloatRect.h>
#include <WebCore/FontBase.h>
#include <WebCore/GlyphBuffer.h>
#include <WebCore/GlyphMetricsMap.h>
#include <WebCore/GlyphPage.h>
#include <WebCore/TrustedFonts.h>
#include <wtf/Platform.h>
#include <wtf/WeakPtr.h>

#if PLATFORM(WIN)
#include <usp10.h>
#endif

namespace WTF {
class TextStream;
}

namespace WebCore {

class FontCache;
class FontDescription;

struct GlyphData;

enum class FontVariant : uint8_t { Auto, Normal, SmallCaps, EmphasisMark, BrokenIdeograph };
enum class PitchType : uint8_t { Unknown, Fixed, Variable };
enum class IsForPlatformFont : bool { No, Yes };

#if USE(CORE_TEXT)
using IPCFontData = Variant<WebCore::InstalledFont, WebCore::CustomFontCreationData>;
#endif

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(Font);
class Font : public FontBase, public RefCounted<Font>, public CanMakeSingleThreadWeakPtr<Font> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Font, Font);
public:
    WEBCORE_EXPORT static Ref<Font> create(const FontPlatformData&, Origin = Origin::Local, IsInterstitial = IsInterstitial::No, Visibility = Visibility::Visible, IsOrientationFallback = IsOrientationFallback::No, std::optional<RenderingResourceIdentifier> = std::nullopt);
    WEBCORE_EXPORT static Ref<Font> create(Ref<SharedBuffer>&& fontFaceData, Font::Origin, float fontSize, bool syntheticBold, bool syntheticItalic, DownloadableBinaryFontTrustedTypes);
    WEBCORE_EXPORT static Ref<Font> create(FontInternalAttributes&&, FontPlatformData&&);

    WEBCORE_EXPORT ~Font();

    static Ref<Font> createSystemFallbackFontPlaceholder() { return adoptRef(*new Font(IsSystemFallbackFontPlaceholder::Yes)); }

    const Font* smallCapsFont(const FontDescription&) const;
    const Font& noSynthesizableFeaturesFont() const;
    const Font* emphasisMarkFont(const FontDescription&) const;
    const Font& brokenIdeographFont() const;
    const RefPtr<Font> halfWidthFont() const;

    bool isProbablyOnlyUsedToRenderIcons() const;

    const Font* variantFont(const FontDescription& description, FontVariant variant) const
    {
        switch (variant) {
        case FontVariant::SmallCaps:
            return smallCapsFont(description);
        case FontVariant::EmphasisMark:
            return emphasisMarkFont(description);
        case FontVariant::BrokenIdeograph:
            return &brokenIdeographFont();
        case FontVariant::Auto:
        case FontVariant::Normal:
            break;
        }
        ASSERT_NOT_REACHED();
        return const_cast<Font*>(this);
    }

    bool variantCapsSupportedForSynthesis(FontVariantCaps) const;

    const Font& verticalRightOrientationFont() const;
    const Font& uprightOrientationFont() const;
    const Font& invisibleFont() const;

    float maxCharWidth() const { return m_maxCharWidth; }
    void setMaxCharWidth(float maxCharWidth) { m_maxCharWidth = maxCharWidth; }

    float avgCharWidth() const { return m_avgCharWidth; }
    void setAvgCharWidth(float avgCharWidth) { m_avgCharWidth = avgCharWidth; }

    FloatRect boundsForGlyph(Glyph) const;
#if USE(CORE_TEXT) || USE(SKIA)
    static constexpr size_t inlineGlyphRunCapacity = 256;
    Vector<FloatRect, inlineGlyphRunCapacity> boundsForGlyphs(std::span<const Glyph>) const;
#endif

    // Should the result of this function include the results of synthetic bold?
    enum class SyntheticBoldInclusion {
        Incorporate,
        Exclude
    };

    float widthForGlyph(Glyph, SyntheticBoldInclusion = SyntheticBoldInclusion::Incorporate) const;

    Path pathForGlyph(Glyph) const;

    float NODELETE spaceWidth(SyntheticBoldInclusion SyntheticBoldInclusion = SyntheticBoldInclusion::Incorporate) const
    {
        return m_spaceWidth + (SyntheticBoldInclusion == SyntheticBoldInclusion::Incorporate ? syntheticBoldOffset() : 0);
    }

    Glyph spaceGlyph() const { return m_spaceGlyph; }
    Glyph zeroWidthSpaceGlyph() const { return m_zeroWidthSpaceGlyph; }
    bool isZeroWidthSpaceGlyph(Glyph glyph) const { return glyph == m_zeroWidthSpaceGlyph && glyph; }

    GlyphData glyphDataForCharacter(char32_t) const;
    Glyph glyphForCharacter(char32_t) const;
    bool supportsCodePoint(char32_t) const;
    bool platformSupportsCodePoint(char32_t, std::optional<char32_t> variation = std::nullopt) const;

    RefPtr<Font> systemFallbackFontForCharacterCluster(StringView, const FontDescription&, ResolvedEmojiPolicy, IsForPlatformFont) const;

    const GlyphPage* glyphPage(unsigned pageNumber) const;

    void determinePitch();
    PitchType pitch() const { return m_treatAsFixedPitch ? PitchType::Fixed : PitchType::Variable; }
    bool canTakeFixedPitchFastContentMeasuring() const { return m_canTakeFixedPitchFastContentMeasuring; }

#if !LOG_DISABLED
    String description() const;
#endif

    bool canRenderCombiningCharacterSequence(StringView) const;
    GlyphBufferAdvance applyTransforms(GlyphBuffer&, unsigned beginningGlyphIndex, unsigned beginningStringIndex, bool enableKerning, bool requiresShaping, const AtomString& locale, StringView text, TextDirection) const;

    // Returns nullopt if none of the glyphs are OT-SVG glyphs.
    std::optional<BitVector> findOTSVGGlyphs(std::span<const GlyphBufferGlyph>) const;

#if USE(CORE_TEXT)
    WEBCORE_EXPORT static std::optional<Ref<Font>> fromIPCData(IPCFontData&&);
    WEBCORE_EXPORT IPCFontData toSerializableFont() const;
    WEBCORE_EXPORT std::optional<InstalledFont> toSerializableInstalledFont() const;
#endif
#if PLATFORM(WIN)
    SCRIPT_CACHE* scriptCache() const LIFETIME_BOUND { return &m_scriptCache; }
#endif

    ColorGlyphType colorGlyphType(Glyph) const;

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

private:
    WEBCORE_EXPORT Font(const FontPlatformData&, Origin, IsInterstitial, Visibility, IsOrientationFallback, std::optional<RenderingResourceIdentifier>);
    using FontBase::FontBase;

    void platformGlyphInit();
    void platformCharWidthInit();
    void platformCharHeightInit();
    void NODELETE platformDestroy();

    void applyFontMetricsOverrides();

    void initCharWidths();

    void initZeroWidth(Glyph);

    RefPtr<Font> createFontWithoutSynthesizableFeatures() const;
    RefPtr<Font> createScaledFont(const FontDescription&, float scaleFactor) const;
    RefPtr<Font> platformCreateScaledFont(const FontDescription&, float scaleFactor) const;
    RefPtr<Font> createHalfWidthFont() const;
    RefPtr<Font> platformCreateHalfWidthFont() const;

    struct DerivedFonts;
    DerivedFonts& ensureDerivedFontData() const;

    FloatRect platformBoundsForGlyph(Glyph) const;
#if USE(CORE_TEXT) || USE(SKIA)
    Vector<FloatRect, inlineGlyphRunCapacity> platformBoundsForGlyphs(const Vector<Glyph, inlineGlyphRunCapacity>&) const;
#endif
    float platformWidthForGlyph(Glyph) const;
    Path platformPathForGlyph(Glyph) const;

    mutable HashMap<unsigned, RefPtr<GlyphPage>, IntHash<unsigned>, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> m_glyphPages;
    mutable GlyphMetricsMap<float> m_glyphToWidthMap;
    mutable std::unique_ptr<GlyphMetricsMap<FloatRect>> m_glyphToBoundsMap;
    // FIXME: Find a more efficient way to represent std::optional<Path>.
    mutable std::unique_ptr<GlyphMetricsMap<std::optional<Path>>> m_glyphPathMap;
    mutable BitVector m_codePointSupport;

    struct DerivedFonts {
        WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(DerivedFonts);
    public:

        RefPtr<Font> smallCapsFont;
        RefPtr<Font> noSynthesizableFeaturesFont;
        RefPtr<Font> emphasisMarkFont;
        RefPtr<Font> brokenIdeographFont;
        RefPtr<Font> verticalRightOrientationFont;
        RefPtr<Font> uprightOrientationFont;
        RefPtr<Font> invisibleFont;
        RefPtr<Font> halfWidthFont;
    };

    mutable std::unique_ptr<DerivedFonts> m_derivedFontData;

#if PLATFORM(WIN)
    mutable SCRIPT_CACHE m_scriptCache { 0 };
#endif

    Glyph m_spaceGlyph { 0 };
    Glyph m_zeroWidthSpaceGlyph { 0 };

    float m_maxCharWidth { -1 };
    float m_avgCharWidth { -1 };
    float m_spaceWidth { 0 };

    unsigned m_treatAsFixedPitch : 1;
    unsigned m_canTakeFixedPitchFastContentMeasuring : 1 { false };
    unsigned m_isBrokenIdeographFallback : 1;
};

#if PLATFORM(IOS_FAMILY)
bool fontFamilyShouldNotBeUsedForArabic(CFStringRef);
#endif

#if !LOG_DISABLED
WEBCORE_EXPORT TextStream& operator<<(TextStream&, const Font&);
TextStream& operator<<(TextStream&, const GlyphBuffer&);
#endif

} // namespace WebCore
