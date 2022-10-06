/*
 * Copyright (C) 2007 Nicholas Shanks <contact@nickshanks.com>
 * Copyright (C) 2008, 2013-2021 Apple Inc. All rights reserved.
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
    , m_orientation(static_cast<unsigned>(FontOrientation::Horizontal))
    , m_nonCJKGlyphOrientation(static_cast<unsigned>(NonCJKGlyphOrientation::Mixed))
    , m_widthVariant(static_cast<unsigned>(FontWidthVariant::RegularWidth))
    , m_renderingMode(static_cast<unsigned>(FontRenderingMode::Normal))
    , m_textRendering(static_cast<unsigned>(TextRenderingMode::AutoTextRendering))
    , m_script(USCRIPT_COMMON)
    , m_fontSynthesisWeight(static_cast<unsigned>(FontSynthesisLonghandValue::Auto))
    , m_fontSynthesisStyle(static_cast<unsigned>(FontSynthesisLonghandValue::Auto))
    , m_fontSynthesisCaps(static_cast<unsigned>(FontSynthesisLonghandValue::Auto))
    , m_variantCommonLigatures(static_cast<unsigned>(FontVariantLigatures::Normal))
    , m_variantDiscretionaryLigatures(static_cast<unsigned>(FontVariantLigatures::Normal))
    , m_variantHistoricalLigatures(static_cast<unsigned>(FontVariantLigatures::Normal))
    , m_variantContextualAlternates(static_cast<unsigned>(FontVariantLigatures::Normal))
    , m_variantPosition(static_cast<unsigned>(FontVariantPosition::Normal))
    , m_variantCaps(static_cast<unsigned>(FontVariantCaps::Normal))
    , m_variantNumericFigure(static_cast<unsigned>(FontVariantNumericFigure::Normal))
    , m_variantNumericSpacing(static_cast<unsigned>(FontVariantNumericSpacing::Normal))
    , m_variantNumericFraction(static_cast<unsigned>(FontVariantNumericFraction::Normal))
    , m_variantNumericOrdinal(static_cast<unsigned>(FontVariantNumericOrdinal::Normal))
    , m_variantNumericSlashedZero(static_cast<unsigned>(FontVariantNumericSlashedZero::Normal))
    , m_variantEastAsianVariant(static_cast<unsigned>(FontVariantEastAsianVariant::Normal))
    , m_variantEastAsianWidth(static_cast<unsigned>(FontVariantEastAsianWidth::Normal))
    , m_variantEastAsianRuby(static_cast<unsigned>(FontVariantEastAsianRuby::Normal))
    , m_opticalSizing(static_cast<unsigned>(FontOpticalSizing::Enabled))
    , m_fontStyleAxis(FontCascadeDescription::initialFontStyleAxis() == FontStyleAxis::ital)
    , m_shouldAllowUserInstalledFonts(static_cast<unsigned>(AllowUserInstalledFonts::No))
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

} // namespace WebCore
