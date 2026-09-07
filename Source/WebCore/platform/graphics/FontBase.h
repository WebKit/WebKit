/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/FontMetrics.h>
#include <WebCore/FontPlatformData.h>
#include <WebCore/GlyphBuffer.h>
#include <WebCore/RenderingResourceIdentifier.h>
#include <wtf/AbstractRefCounted.h>
#include <wtf/BitVector.h>

#if PLATFORM(COCOA)
#include <pal/cf/OTSVGTable.h>
#endif

#if USE(SKIA)
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkTextBlob.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#endif

namespace WebCore {

#if ENABLE(MATHML)
class OpenTypeMathData;
#endif
#if ENABLE(OPENTYPE_VERTICAL)
class OpenTypeVerticalData;
#endif
#if ENABLE(MULTI_REPRESENTATION_HEIC)
struct MultiRepresentationHEICMetrics;
#endif

// Used to create platform fonts.
enum class FontOrigin : bool { Remote, Local };
enum class FontIsInterstitial : bool { No, Yes };
enum class FontVisibility : bool { Visible, Invisible };
enum class FontIsOrientationFallback : bool { No, Yes };

struct FontInternalAttributes {
    WEBCORE_EXPORT RenderingResourceIdentifier ensureRenderingResourceIdentifier() const;

    mutable std::optional<RenderingResourceIdentifier> renderingResourceIdentifier;
    FontOrigin origin : 1;
    FontIsInterstitial isInterstitial : 1;
    FontVisibility visibility : 1;
    FontIsOrientationFallback isTextOrientationFallback : 1;
};

#if USE(CORE_TEXT)
bool fontHasEitherTable(CTFontRef, unsigned tableTag1, unsigned tableTag2);
bool supportsOpenTypeFeature(CTFontRef, CFStringRef featureTag);
#endif

class FontBase : public AbstractRefCounted {
public:
    using Attributes = FontInternalAttributes;
    using Origin = FontOrigin;
    using IsInterstitial = FontIsInterstitial;
    using Visibility = FontVisibility;
    using IsOrientationFallback = FontIsOrientationFallback;

    WEBCORE_EXPORT ~FontBase();

    const FontPlatformData& platformData() const LIFETIME_BOUND { return m_platformData; }
#if ENABLE(MATHML)
    const OpenTypeMathData* mathData() const;
#endif
#if ENABLE(OPENTYPE_VERTICAL)
    inline const OpenTypeVerticalData* verticalData() const;
#endif

    const Attributes& attributes() const LIFETIME_BOUND { return m_attributes; }

    Origin origin() const { return m_attributes.origin; }
    bool isInterstitial() const { return m_attributes.isInterstitial == IsInterstitial::Yes; }
    Visibility visibility() const { return m_attributes.visibility; }
    bool isTextOrientationFallback() const { return m_attributes.isTextOrientationFallback == IsOrientationFallback::Yes; }
    WEBCORE_EXPORT RenderingResourceIdentifier renderingResourceIdentifier() const;

    const FontMetrics& fontMetrics() const LIFETIME_BOUND { return m_fontMetrics; }
    float sizePerUnit() const { return platformData().size() / (fontMetrics().unitsPerEm() ? fontMetrics().unitsPerEm() : 1); }

    float syntheticBoldOffset() const { return m_syntheticBoldOffset; }

    bool isSystemFontFallbackPlaceholder() const { return m_isSystemFontFallbackPlaceholder; }
    bool allowsAntialiasing() const { return m_allowsAntialiasing; }
    bool hasVerticalGlyphs() const { return m_hasVerticalGlyphs; }
    bool isUsedInSystemFallbackFontCache() const { return m_isUsedInSystemFallbackFontCache; }
    void setIsUsedInSystemFallbackFontCache() { m_isUsedInSystemFallbackFontCache = true; }
#if PLATFORM(IOS_FAMILY)
    bool shouldNotBeUsedForArabic() const { return m_shouldNotBeUsedForArabic; };
#endif

    bool hasAnyComplexColorFormatGlyphs(std::span<const GlyphBufferGlyph>) const;

#if USE(CORE_TEXT)
    CTFontRef ctFont() const { return m_platformData.ctFont(); }
    bool supportsSmallCaps() const;
    bool supportsAllSmallCaps() const;
    bool supportsPetiteCaps() const;
    bool supportsAllPetiteCaps() const;
    bool supportsOpenTypeAlternateHalfWidths() const;
#if ENABLE(MULTI_REPRESENTATION_HEIC)
    MultiRepresentationHEICMetrics metricsForMultiRepresentationHEIC() const;
#endif
#endif

#if USE(SKIA)
    sk_sp<SkTextBlob> buildTextBlob(std::span<const GlyphBufferGlyph>, std::span<const GlyphBufferAdvance>, FontSmoothingMode) const;
    bool enableAntialiasing(FontSmoothingMode) const;
#endif

protected:
    struct NoEmojiGlyphs { };
#if USE(SKIA)
    struct AllEmojiGlyphs { };
#endif
    struct SomeEmojiGlyphs {
        BitVector colorGlyphs;
    };
#if USE(SKIA)
    using EmojiType = Variant<NoEmojiGlyphs, AllEmojiGlyphs, SomeEmojiGlyphs>;
#else
    using EmojiType = Variant<NoEmojiGlyphs, SomeEmojiGlyphs>;
#endif

    enum class IsSystemFallbackFontPlaceholder : bool { No, Yes };

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
#endif

    WEBCORE_EXPORT FontBase(const FontPlatformData&, Origin, IsInterstitial, Visibility, IsOrientationFallback, std::optional<RenderingResourceIdentifier>);
    FontBase(IsSystemFallbackFontPlaceholder);

    void platformInit();

#if PLATFORM(COCOA)
    const PAL::OTSVGTable& otSVGTable() const;
    bool glyphHasComplexColorFormat(Glyph) const;
    bool hasComplexColorFormatTables() const;
    ComplexColorFormatGlyphs& glyphsWithComplexColorFormat() const;
#endif

    const FontPlatformData m_platformData;
    Attributes m_attributes;

#if ENABLE(MATHML)
    mutable RefPtr<OpenTypeMathData> m_mathData;
#endif
#if ENABLE(OPENTYPE_VERTICAL)
    RefPtr<OpenTypeVerticalData> m_verticalData;
#endif

#if PLATFORM(COCOA)
    enum class SupportsFeature : uint8_t {
        No,
        Yes,
        Unknown
    };
    mutable SupportsFeature m_supportsSmallCaps { SupportsFeature::Unknown };
    mutable SupportsFeature m_supportsAllSmallCaps { SupportsFeature::Unknown };
    mutable SupportsFeature m_supportsPetiteCaps { SupportsFeature::Unknown };
    mutable SupportsFeature m_supportsAllPetiteCaps { SupportsFeature::Unknown };
    mutable SupportsFeature m_supportsOpenTypeAlternateHalfWidths { SupportsFeature::Unknown };

    mutable std::optional<PAL::OTSVGTable> m_otSVGTable;
    mutable std::optional<ComplexColorFormatGlyphs> m_glyphsWithComplexColorFormat; // SVG and sbix
#endif

    FontMetrics m_fontMetrics;
    EmojiType m_emojiType { NoEmojiGlyphs { } };

    float m_syntheticBoldOffset { 0 };

    unsigned m_isSystemFontFallbackPlaceholder : 1 { false };
    unsigned m_allowsAntialiasing : 1 { true };
    unsigned m_hasVerticalGlyphs : 1;
    unsigned m_isUsedInSystemFallbackFontCache : 1 { false };
#if PLATFORM(IOS_FAMILY)
    unsigned m_shouldNotBeUsedForArabic : 1 { false };
#endif
};

} // namespace WebCore
