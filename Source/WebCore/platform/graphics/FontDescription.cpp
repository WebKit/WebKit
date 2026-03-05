/*
 * Copyright (C) 2007 Nicholas Shanks <contact@nickshanks.com>
 * Copyright (C) 2008-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
#include "FontDescription.h"

#include "FontCascadeDescription.h"
#include "LocaleToScriptMapping.h"
#include <wtf/Language.h>

namespace WebCore {

FontDescription::FontDescription()
    : m_variantAlternates(FontVariantAlternates::Normal())
    , m_fontPalette({ FontPalette::Type::Normal, nullAtom() })
    , m_fontSelectionRequest { normalWeightValue(), normalWidthValue(), std::nullopt }
    , m_orientation(std::to_underlying(FontOrientation::Horizontal))
    , m_nonCJKGlyphOrientation(std::to_underlying(NonCJKGlyphOrientation::Mixed))
    , m_widthVariant(std::to_underlying(FontWidthVariant::RegularWidth))
    , m_textRendering(std::to_underlying(TextRenderingMode::Auto))
    , m_script(USCRIPT_COMMON)
    , m_fontSynthesisWeight(std::to_underlying(FontSynthesisLonghandValue::Auto))
    , m_fontSynthesisStyle(std::to_underlying(FontSynthesisLonghandValue::Auto))
    , m_fontSynthesisCaps(std::to_underlying(FontSynthesisLonghandValue::Auto))
    , m_variantCommonLigatures(std::to_underlying(FontVariantLigatures::Normal))
    , m_variantDiscretionaryLigatures(std::to_underlying(FontVariantLigatures::Normal))
    , m_variantHistoricalLigatures(std::to_underlying(FontVariantLigatures::Normal))
    , m_variantContextualAlternates(std::to_underlying(FontVariantLigatures::Normal))
    , m_variantPosition(std::to_underlying(FontVariantPosition::Normal))
    , m_variantCaps(std::to_underlying(FontVariantCaps::Normal))
    , m_variantNumericFigure(std::to_underlying(FontVariantNumericFigure::Normal))
    , m_variantNumericSpacing(std::to_underlying(FontVariantNumericSpacing::Normal))
    , m_variantNumericFraction(std::to_underlying(FontVariantNumericFraction::Normal))
    , m_variantNumericOrdinal(std::to_underlying(FontVariantNumericOrdinal::Normal))
    , m_variantNumericSlashedZero(std::to_underlying(FontVariantNumericSlashedZero::Normal))
    , m_variantEastAsianVariant(std::to_underlying(FontVariantEastAsianVariant::Normal))
    , m_variantEastAsianWidth(std::to_underlying(FontVariantEastAsianWidth::Normal))
    , m_variantEastAsianRuby(std::to_underlying(FontVariantEastAsianRuby::Normal))
    , m_variantEmoji(std::to_underlying(FontVariantEmoji::Normal))
    , m_opticalSizing(std::to_underlying(FontOpticalSizing::Auto))
    , m_fontStyleAxis(std::to_underlying(FontStyleAxis::slnt))
    , m_shouldAllowUserInstalledFonts(std::to_underlying(AllowUserInstalledFonts::No))
    , m_shouldDisableLigaturesForSpacing(false)
    , m_evaluationTimeZoomEnabled(false)
{
}

static AtomString computeSpecializedChineseLocale()
{
    // FIXME: This is not passing ShouldMinimizeLanguages::No and then getting minimized languages,
    // which may cause the matching below to fail.
    for (auto& language : userPreferredLanguages()) {
        if (startsWithLettersIgnoringASCIICase(language, "zh-"_s))
            return AtomString { language };
    }
    return "zh-hans"_s; // We have no signal. Pick one option arbitrarily.
}

static AtomString& cachedSpecializedChineseLocale()
{
    static MainThreadNeverDestroyed<AtomString> specializedChineseLocale;
    return specializedChineseLocale.get();
}

static void fontDescriptionLanguageChanged(void*)
{
    cachedSpecializedChineseLocale() = computeSpecializedChineseLocale();
}

static const AtomString& specializedChineseLocale()
{
    auto& locale = cachedSpecializedChineseLocale();
    if (cachedSpecializedChineseLocale().isNull()) {
        static char forNonNullPointer;
        addLanguageChangeObserver(&forNonNullPointer, &fontDescriptionLanguageChanged); // We will never remove the observer, so all we need is a non-null pointer.
        fontDescriptionLanguageChanged(nullptr);
    }
    return locale;
}

void FontDescription::setSpecifiedLocale(const AtomString& locale)
{
    ASSERT(isMainThread());
    m_specifiedLocale = locale;
    m_script = localeToScriptCode(m_specifiedLocale);
    m_locale = m_script == USCRIPT_HAN ? specializedChineseLocale() : m_specifiedLocale;
}

#if !PLATFORM(COCOA)
AtomString FontDescription::platformResolveGenericFamily(UScriptCode, const AtomString&, const AtomString&)
{
    return nullAtom();
}
#endif

float FontDescription::adjustedSizeForFontFace(float fontFaceSizeAdjust) const
{
    // It is not worth modifying the used size with @font-face size-adjust if we are to re-adjust it later with font-size-adjust. This is because font-size-adjust will overrule this change, since size-adjust also modifies the font's metric values and thus, keeps the aspect-value unchanged.
    return fontSizeAdjust().value ? computedSize() : fontFaceSizeAdjust * computedSize();
}

FontVariantEastAsianValues FontDescription::variantEastAsian() const
{
    return {
        variantEastAsianVariant(),
        variantEastAsianWidth(),
        variantEastAsianRuby(),
    };
}

FontVariantNumericValues FontDescription::variantNumeric() const
{
    return {
        variantNumericFigure(),
        variantNumericSpacing(),
        variantNumericFraction(),
        variantNumericOrdinal(),
        variantNumericSlashedZero(),
    };
}

FontVariantLigaturesValues FontDescription::variantLigatures() const
{
    return {
        variantCommonLigatures(),
        variantDiscretionaryLigatures(),
        variantHistoricalLigatures(),
        variantContextualAlternates(),
    };
}

FontVariantSettings FontDescription::variantSettings() const
{
    return {
        variantCommonLigatures(),
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
        variantEastAsianRuby(),
        variantEmoji()
    };
}

void FontDescription::setVariantEastAsian(FontVariantEastAsianValues values)
{
    setVariantEastAsianVariant(values.variant);
    setVariantEastAsianWidth(values.width);
    setVariantEastAsianRuby(values.ruby);
}

void FontDescription::setVariantNumeric(FontVariantNumericValues values)
{
    setVariantNumericFigure(values.figure);
    setVariantNumericSpacing(values.spacing);
    setVariantNumericFraction(values.fraction);
    setVariantNumericOrdinal(values.ordinal);
    setVariantNumericSlashedZero(values.slashedZero);
}

void FontDescription::setVariantLigatures(FontVariantLigaturesValues values)
{
    setVariantCommonLigatures(values.common);
    setVariantDiscretionaryLigatures(values.discretionary);
    setVariantHistoricalLigatures(values.historical);
    setVariantContextualAlternates(values.contextual);
}

} // namespace WebCore
