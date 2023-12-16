/*
 * Copyright (C) 2007 Nicholas Shanks <contact@nickshanks.com>
 * Copyright (C) 2008-2023 Apple Inc. All rights reserved.
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
    : m_variantAlternates(FontCascadeDescription::initialVariantAlternates())
    , m_fontPalette({ FontPalette::Type::Normal, nullAtom() })
    , m_fontSelectionRequest { FontCascadeDescription::initialWeight(), FontCascadeDescription::initialStretch(), FontCascadeDescription::initialItalic() }
    , m_orientation(enumToUnderlyingType(FontOrientation::Horizontal))
    , m_nonCJKGlyphOrientation(enumToUnderlyingType(NonCJKGlyphOrientation::Mixed))
    , m_widthVariant(enumToUnderlyingType(FontWidthVariant::RegularWidth))
    , m_textRendering(enumToUnderlyingType(TextRenderingMode::AutoTextRendering))
    , m_script(USCRIPT_COMMON)
    , m_fontSynthesisWeight(enumToUnderlyingType(FontSynthesisLonghandValue::Auto))
    , m_fontSynthesisStyle(enumToUnderlyingType(FontSynthesisLonghandValue::Auto))
    , m_fontSynthesisCaps(enumToUnderlyingType(FontSynthesisLonghandValue::Auto))
    , m_variantCommonLigatures(enumToUnderlyingType(FontVariantLigatures::Normal))
    , m_variantDiscretionaryLigatures(enumToUnderlyingType(FontVariantLigatures::Normal))
    , m_variantHistoricalLigatures(enumToUnderlyingType(FontVariantLigatures::Normal))
    , m_variantContextualAlternates(enumToUnderlyingType(FontVariantLigatures::Normal))
    , m_variantPosition(enumToUnderlyingType(FontVariantPosition::Normal))
    , m_variantCaps(enumToUnderlyingType(FontVariantCaps::Normal))
    , m_variantNumericFigure(enumToUnderlyingType(FontVariantNumericFigure::Normal))
    , m_variantNumericSpacing(enumToUnderlyingType(FontVariantNumericSpacing::Normal))
    , m_variantNumericFraction(enumToUnderlyingType(FontVariantNumericFraction::Normal))
    , m_variantNumericOrdinal(enumToUnderlyingType(FontVariantNumericOrdinal::Normal))
    , m_variantNumericSlashedZero(enumToUnderlyingType(FontVariantNumericSlashedZero::Normal))
    , m_variantEastAsianVariant(enumToUnderlyingType(FontVariantEastAsianVariant::Normal))
    , m_variantEastAsianWidth(enumToUnderlyingType(FontVariantEastAsianWidth::Normal))
    , m_variantEastAsianRuby(enumToUnderlyingType(FontVariantEastAsianRuby::Normal))
    , m_variantEmoji(enumToUnderlyingType(FontVariantEmoji::Normal))
    , m_opticalSizing(enumToUnderlyingType(FontOpticalSizing::Enabled))
    , m_fontStyleAxis(FontCascadeDescription::initialFontStyleAxis() == FontStyleAxis::ital)
    , m_shouldAllowUserInstalledFonts(enumToUnderlyingType(AllowUserInstalledFonts::No))
    , m_shouldDisableLigaturesForSpacing(false)
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
    m_script = localeToScriptCodeForFontSelection(m_specifiedLocale);
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
} // namespace WebCore
