/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
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
 * along with this library; see the file COPYING.LIother.m_  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USm_
 *
 */

#pragma once

#include "FontSelectionAlgorithm.h"
#include "FontTaggedSettings.h"
#include "TextFlags.h"
#include "WebKitFontFamilyNames.h"
#include <unicode/uscript.h>
#include <wtf/MathExtras.h>

namespace WebCore {

using namespace WebKitFontFamilyNames;

class FontDescription {
public:
    WEBCORE_EXPORT FontDescription();

    bool operator==(const FontDescription&) const;
    bool operator!=(const FontDescription& other) const { return !(*this == other); }

    float computedSize() const { return m_computedSize; }
    unsigned computedPixelSize() const { return unsigned(m_computedSize + 0.5f); }
    Optional<FontSelectionValue> italic() const { return m_fontSelectionRequest.slope; }
    FontSelectionValue stretch() const { return m_fontSelectionRequest.width; }
    FontSelectionValue weight() const { return m_fontSelectionRequest.weight; }
    FontSelectionRequest fontSelectionRequest() const { return m_fontSelectionRequest; }
    FontRenderingMode renderingMode() const { return static_cast<FontRenderingMode>(m_renderingMode); }
    TextRenderingMode textRenderingMode() const { return static_cast<TextRenderingMode>(m_textRendering); }
    UScriptCode script() const { return static_cast<UScriptCode>(m_script); }
    const AtomString& locale() const { return m_locale; }

    FontOrientation orientation() const { return static_cast<FontOrientation>(m_orientation); }
    NonCJKGlyphOrientation nonCJKGlyphOrientation() const { return static_cast<NonCJKGlyphOrientation>(m_nonCJKGlyphOrientation); }
    FontWidthVariant widthVariant() const { return static_cast<FontWidthVariant>(m_widthVariant); }
    const FontFeatureSettings& featureSettings() const { return m_featureSettings; }
    const FontVariationSettings& variationSettings() const { return m_variationSettings; }
    FontSynthesis fontSynthesis() const { return static_cast<FontSynthesis>(m_fontSynthesis); }
    FontVariantLigatures variantCommonLigatures() const { return static_cast<FontVariantLigatures>(m_variantCommonLigatures); }
    FontVariantLigatures variantDiscretionaryLigatures() const { return static_cast<FontVariantLigatures>(m_variantDiscretionaryLigatures); }
    FontVariantLigatures variantHistoricalLigatures() const { return static_cast<FontVariantLigatures>(m_variantHistoricalLigatures); }
    FontVariantLigatures variantContextualAlternates() const { return static_cast<FontVariantLigatures>(m_variantContextualAlternates); }
    FontVariantPosition variantPosition() const { return static_cast<FontVariantPosition>(m_variantPosition); }
    FontVariantCaps variantCaps() const { return static_cast<FontVariantCaps>(m_variantCaps); }
    FontVariantNumericFigure variantNumericFigure() const { return static_cast<FontVariantNumericFigure>(m_variantNumericFigure); }
    FontVariantNumericSpacing variantNumericSpacing() const { return static_cast<FontVariantNumericSpacing>(m_variantNumericSpacing); }
    FontVariantNumericFraction variantNumericFraction() const { return static_cast<FontVariantNumericFraction>(m_variantNumericFraction); }
    FontVariantNumericOrdinal variantNumericOrdinal() const { return static_cast<FontVariantNumericOrdinal>(m_variantNumericOrdinal); }
    FontVariantNumericSlashedZero variantNumericSlashedZero() const { return static_cast<FontVariantNumericSlashedZero>(m_variantNumericSlashedZero); }
    FontVariantAlternates variantAlternates() const { return static_cast<FontVariantAlternates>(m_variantAlternates); }
    FontVariantEastAsianVariant variantEastAsianVariant() const { return static_cast<FontVariantEastAsianVariant>(m_variantEastAsianVariant); }
    FontVariantEastAsianWidth variantEastAsianWidth() const { return static_cast<FontVariantEastAsianWidth>(m_variantEastAsianWidth); }
    FontVariantEastAsianRuby variantEastAsianRuby() const { return static_cast<FontVariantEastAsianRuby>(m_variantEastAsianRuby); }
    FontVariantSettings variantSettings() const
    {
        return { variantCommonLigatures(),
            variantDiscretionaryLigatures(),
            variantHistoricalLigatures(),
            variantContextualAlternates(),
            variantPosition(),
            variantCaps(),
            variantNumericFigure(),
            variantNumericSpacing(),
            variantNumericFraction(),
            variantNumericOrdinal(),
            variantNumericSlashedZero(),
            variantAlternates(),
            variantEastAsianVariant(),
            variantEastAsianWidth(),
            variantEastAsianRuby() };
    }
    FontOpticalSizing opticalSizing() const { return static_cast<FontOpticalSizing>(m_opticalSizing); }
    FontStyleAxis fontStyleAxis() const { return m_fontStyleAxis ? FontStyleAxis::ital : FontStyleAxis::slnt; }
    AllowUserInstalledFonts shouldAllowUserInstalledFonts() const { return static_cast<AllowUserInstalledFonts>(m_shouldAllowUserInstalledFonts); }
    bool shouldAllowDesignSystemUIFonts() const { return m_shouldAllowDesignSystemUIFonts; }

    void setComputedSize(float s) { m_computedSize = clampToFloat(s); }
    void setItalic(Optional<FontSelectionValue> italic) { m_fontSelectionRequest.slope = italic; }
    void setStretch(FontSelectionValue stretch) { m_fontSelectionRequest.width = stretch; }
    void setIsItalic(bool isItalic) { setItalic(isItalic ? Optional<FontSelectionValue> { italicValue() } : Optional<FontSelectionValue> { }); }
    void setWeight(FontSelectionValue weight) { m_fontSelectionRequest.weight = weight; }
    void setRenderingMode(FontRenderingMode mode) { m_renderingMode = static_cast<unsigned>(mode); }
    void setTextRenderingMode(TextRenderingMode rendering) { m_textRendering = static_cast<unsigned>(rendering); }
    void setOrientation(FontOrientation orientation) { m_orientation = static_cast<unsigned>(orientation); }
    void setNonCJKGlyphOrientation(NonCJKGlyphOrientation orientation) { m_nonCJKGlyphOrientation = static_cast<unsigned>(orientation); }
    void setWidthVariant(FontWidthVariant widthVariant) { m_widthVariant = static_cast<unsigned>(widthVariant); } // Make sure new callers of this sync with FontPlatformData::isForTextCombine()!
    void setLocale(const AtomString&);
    void setFeatureSettings(FontFeatureSettings&& settings) { m_featureSettings = WTFMove(settings); }
#if ENABLE(VARIATION_FONTS)
    void setVariationSettings(FontVariationSettings&& settings) { m_variationSettings = WTFMove(settings); }
#endif
    void setFontSynthesis(FontSynthesis fontSynthesis) { m_fontSynthesis = fontSynthesis; }
    void setVariantCommonLigatures(FontVariantLigatures variant) { m_variantCommonLigatures = static_cast<unsigned>(variant); }
    void setVariantDiscretionaryLigatures(FontVariantLigatures variant) { m_variantDiscretionaryLigatures = static_cast<unsigned>(variant); }
    void setVariantHistoricalLigatures(FontVariantLigatures variant) { m_variantHistoricalLigatures = static_cast<unsigned>(variant); }
    void setVariantContextualAlternates(FontVariantLigatures variant) { m_variantContextualAlternates = static_cast<unsigned>(variant); }
    void setVariantPosition(FontVariantPosition variant) { m_variantPosition = static_cast<unsigned>(variant); }
    void setVariantCaps(FontVariantCaps variant) { m_variantCaps = static_cast<unsigned>(variant); }
    void setVariantNumericFigure(FontVariantNumericFigure variant) { m_variantNumericFigure = static_cast<unsigned>(variant); }
    void setVariantNumericSpacing(FontVariantNumericSpacing variant) { m_variantNumericSpacing = static_cast<unsigned>(variant); }
    void setVariantNumericFraction(FontVariantNumericFraction variant) { m_variantNumericFraction = static_cast<unsigned>(variant); }
    void setVariantNumericOrdinal(FontVariantNumericOrdinal variant) { m_variantNumericOrdinal = static_cast<unsigned>(variant); }
    void setVariantNumericSlashedZero(FontVariantNumericSlashedZero variant) { m_variantNumericSlashedZero = static_cast<unsigned>(variant); }
    void setVariantAlternates(FontVariantAlternates variant) { m_variantAlternates = static_cast<unsigned>(variant); }
    void setVariantEastAsianVariant(FontVariantEastAsianVariant variant) { m_variantEastAsianVariant = static_cast<unsigned>(variant); }
    void setVariantEastAsianWidth(FontVariantEastAsianWidth variant) { m_variantEastAsianWidth = static_cast<unsigned>(variant); }
    void setVariantEastAsianRuby(FontVariantEastAsianRuby variant) { m_variantEastAsianRuby = static_cast<unsigned>(variant); }
    void setOpticalSizing(FontOpticalSizing sizing) { m_opticalSizing = static_cast<unsigned>(sizing); }
    void setFontStyleAxis(FontStyleAxis axis) { m_fontStyleAxis = axis == FontStyleAxis::ital; }
    void setShouldAllowUserInstalledFonts(AllowUserInstalledFonts shouldAllowUserInstalledFonts) { m_shouldAllowUserInstalledFonts = static_cast<unsigned>(shouldAllowUserInstalledFonts); }
    void setShouldAllowDesignSystemUIFonts(bool allow) { m_shouldAllowDesignSystemUIFonts = allow; }

    static AtomString platformResolveGenericFamily(UScriptCode, const AtomString& locale, const AtomString& familyName);

private:
    // FIXME: Investigate moving these into their own object on the heap (to save memory).
    FontFeatureSettings m_featureSettings;
    FontVariationSettings m_variationSettings;
    AtomString m_locale;

    FontSelectionRequest m_fontSelectionRequest;
    float m_computedSize { 0 }; // Computed size adjusted for the minimum font size and the zoom factor.
    unsigned m_orientation : 1; // FontOrientation - Whether the font is rendering on a horizontal line or a vertical line.
    unsigned m_nonCJKGlyphOrientation : 1; // NonCJKGlyphOrientation - Only used by vertical text. Determines the default orientation for non-ideograph glyphs.
    unsigned m_widthVariant : 2; // FontWidthVariant
    unsigned m_renderingMode : 1; // Used to switch between CG and GDI text on Windows.
    unsigned m_textRendering : 2; // TextRenderingMode
    unsigned m_script : 7; // Used to help choose an appropriate font for generic font families.
    unsigned m_fontSynthesis : 3; // FontSynthesis type
    unsigned m_variantCommonLigatures : 2; // FontVariantLigatures
    unsigned m_variantDiscretionaryLigatures : 2; // FontVariantLigatures
    unsigned m_variantHistoricalLigatures : 2; // FontVariantLigatures
    unsigned m_variantContextualAlternates : 2; // FontVariantLigatures
    unsigned m_variantPosition : 2; // FontVariantPosition
    unsigned m_variantCaps : 3; // FontVariantCaps
    unsigned m_variantNumericFigure : 2; // FontVariantNumericFigure
    unsigned m_variantNumericSpacing : 2; // FontVariantNumericSpacing
    unsigned m_variantNumericFraction : 2; // FontVariantNumericFraction
    unsigned m_variantNumericOrdinal : 1; // FontVariantNumericOrdinal
    unsigned m_variantNumericSlashedZero : 1; // FontVariantNumericSlashedZero
    unsigned m_variantAlternates : 1; // FontVariantAlternates
    unsigned m_variantEastAsianVariant : 3; // FontVariantEastAsianVariant
    unsigned m_variantEastAsianWidth : 2; // FontVariantEastAsianWidth
    unsigned m_variantEastAsianRuby : 1; // FontVariantEastAsianRuby
    unsigned m_opticalSizing : 1; // FontOpticalSizing
    unsigned m_fontStyleAxis : 1; // Whether "font-style: italic" or "font-style: oblique 20deg" was specified
    unsigned m_shouldAllowUserInstalledFonts : 1; // AllowUserInstalledFonts: If this description is allowed to match a user-installed font
    unsigned m_shouldAllowDesignSystemUIFonts : 1; // Whether -apple-system-ui-serif, -apple-system-ui-monospaced, and -apple-system-ui-rounded are supported
};

inline bool FontDescription::operator==(const FontDescription& other) const
{
    return m_computedSize == other.m_computedSize
        && m_fontSelectionRequest == other.m_fontSelectionRequest
        && m_renderingMode == other.m_renderingMode
        && m_textRendering == other.m_textRendering
        && m_orientation == other.m_orientation
        && m_nonCJKGlyphOrientation == other.m_nonCJKGlyphOrientation
        && m_widthVariant == other.m_widthVariant
        && m_locale == other.m_locale
        && m_featureSettings == other.m_featureSettings
#if ENABLE(VARIATION_FONTS)
        && m_variationSettings == other.m_variationSettings
#endif
        && m_fontSynthesis == other.m_fontSynthesis
        && m_variantCommonLigatures == other.m_variantCommonLigatures
        && m_variantDiscretionaryLigatures == other.m_variantDiscretionaryLigatures
        && m_variantHistoricalLigatures == other.m_variantHistoricalLigatures
        && m_variantContextualAlternates == other.m_variantContextualAlternates
        && m_variantPosition == other.m_variantPosition
        && m_variantCaps == other.m_variantCaps
        && m_variantNumericFigure == other.m_variantNumericFigure
        && m_variantNumericSpacing == other.m_variantNumericSpacing
        && m_variantNumericFraction == other.m_variantNumericFraction
        && m_variantNumericOrdinal == other.m_variantNumericOrdinal
        && m_variantNumericSlashedZero == other.m_variantNumericSlashedZero
        && m_variantAlternates == other.m_variantAlternates
        && m_variantEastAsianVariant == other.m_variantEastAsianVariant
        && m_variantEastAsianWidth == other.m_variantEastAsianWidth
        && m_variantEastAsianRuby == other.m_variantEastAsianRuby
        && m_opticalSizing == other.m_opticalSizing
        && m_fontStyleAxis == other.m_fontStyleAxis
        && m_shouldAllowUserInstalledFonts == other.m_shouldAllowUserInstalledFonts
        && m_shouldAllowDesignSystemUIFonts == other.m_shouldAllowDesignSystemUIFonts;
}

}
