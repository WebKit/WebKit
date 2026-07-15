/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#include "Document.h"
#include "FontTaggedSettings.h"
#include "StyleComputedStyle.h"
#include "TextFlags.h"

namespace WebCore {

class FontCascadeDescription;

namespace Style {

class MutatorFontUpdateScope;

struct FontFamilies;
struct FontFeatureSettings;
struct FontPalette;
struct FontSizeAdjust;
struct FontStyle;
struct FontVariantAlternates;
struct FontVariantEastAsian;
struct FontVariantLigatures;
struct FontVariantNumeric;
struct FontVariationSettings;
struct FontWeight;
struct FontWidth;
struct TextAutospace;
struct TextSpacingTrim;
struct WebkitLocale;
struct Zoom;

struct MutatorContext {
    Ref<const Document> document;
    RefPtr<const Element> element { };
    const ComputedStyle* parentStyle { };
    const ComputedStyle* rootElementStyle { };
};

class Mutator {
    WTF_MAKE_NONCOPYABLE(Mutator);
public:
    Mutator(ComputedStyle&, MutatorContext&&);

    ComputedStyle& style() LIFETIME_BOUND { return m_style; }
    const ComputedStyle& style() const LIFETIME_BOUND { return m_style; }

    const ComputedStyle& parentStyle() const LIFETIME_BOUND { return *m_parentStyle; }
    const ComputedStyle* rootElementStyle() const LIFETIME_BOUND { return m_rootElementStyle; }

    const Document& document() const { return m_document; }
    const Element* element() const { return m_element.get(); }

    inline void setZoom(Zoom);
    inline void setUsedZoom(float);
    inline void setWritingMode(StyleWritingMode);
    inline void setTextOrientation(TextOrientation);

    bool fontDirty() const { return m_fontDirty; }
    void setFontDirty() { m_fontDirty = true; }

    bool NODELETE useSVGZoomRules() const;
    bool NODELETE useSVGZoomRulesForLength() const;

    // FIXME: Copying a FontCascadeDescription is really inefficient. Migrate all callers to
    // setFontDescriptionXXX() variants below, then remove these functions.
    inline void setFontDescription(FontCascadeDescription&&);
    void setFontSize(FontCascadeDescription&, float size);

    void setFontDescriptionKeywordSizeFromIdentifier(CSSValueID);
    void setFontDescriptionIsAbsoluteSize(bool);
    void setFontDescriptionFontSize(float);
    void setFontDescriptionFamilies(FontFamilies&&);
    void setFontDescriptionFeatureSettings(FontFeatureSettings&&);
    void setFontDescriptionFontPalette(FontPalette&&);
    void setFontDescriptionFontSizeAdjust(FontSizeAdjust);
    void setFontDescriptionFontSmoothing(FontSmoothingMode);
    void setFontDescriptionFontStyle(FontStyle);
    void setFontDescriptionFontSynthesisSmallCaps(FontSynthesisLonghandValue);
    void setFontDescriptionFontSynthesisStyle(FontSynthesisStyleLonghandValue);
    void setFontDescriptionFontSynthesisWeight(FontSynthesisLonghandValue);
    void setFontDescriptionKerning(Kerning);
    void setFontDescriptionOpticalSizing(FontOpticalSizing);
    void setFontDescriptionSpecifiedLocale(WebkitLocale&&);
    void setFontDescriptionTextAutospace(TextAutospace);
    void setFontDescriptionTextRenderingMode(TextRenderingMode);
    void setFontDescriptionTextSpacingTrim(TextSpacingTrim);
    void setFontDescriptionVariantCaps(FontVariantCaps);
    void setFontDescriptionVariantEmoji(FontVariantEmoji);
    void setFontDescriptionVariantPosition(FontVariantPosition);
    void setFontDescriptionVariationSettings(FontVariationSettings&&);
    void setFontDescriptionWeight(FontWeight);
    void setFontDescriptionWidth(FontWidth);
    void setFontDescriptionVariantAlternates(FontVariantAlternates&&);
    void setFontDescriptionVariantEastAsian(FontVariantEastAsian);
    void setFontDescriptionVariantEastAsianVariant(FontVariantEastAsianVariant);
    void setFontDescriptionVariantEastAsianWidth(FontVariantEastAsianWidth);
    void setFontDescriptionVariantEastAsianRuby(FontVariantEastAsianRuby);
    void setFontDescriptionKeywordSize(unsigned);
    void setFontDescriptionVariantLigatures(FontVariantLigatures);
    void setFontDescriptionVariantCommonLigatures(WebCore::FontVariantLigatures);
    void setFontDescriptionVariantDiscretionaryLigatures(WebCore::FontVariantLigatures);
    void setFontDescriptionVariantHistoricalLigatures(WebCore::FontVariantLigatures);
    void setFontDescriptionVariantContextualAlternates(WebCore::FontVariantLigatures);
    void setFontDescriptionVariantNumeric(FontVariantNumeric);
    void setFontDescriptionVariantNumericFigure(FontVariantNumericFigure);
    void setFontDescriptionVariantNumericSpacing(FontVariantNumericSpacing);
    void setFontDescriptionVariantNumericFraction(FontVariantNumericFraction);
    void setFontDescriptionVariantNumericOrdinal(FontVariantNumericOrdinal);
    void setFontDescriptionVariantNumericSlashedZero(FontVariantNumericSlashedZero);

    // The canonical way to invoke updateFont() is through a MutatorFontUpdateScope, but in some cases
    // a forced update midway through is necessary.
    void forceUpdateFont() { updateFont(); }

private:
    friend class MutatorFontUpdateScope;

    void updateFont();
#if ENABLE(TEXT_AUTOSIZING)
    void updateFontForTextSizeAdjust();
#endif
    void updateFontForZoomChange();
    void updateFontForGenericFamilyChange();
    void updateFontForOrientationChange();
    void updateFontForSizeChange();

    ComputedStyle& m_style;

    const Ref<const Document> m_document;
    const RefPtr<const Element> m_element;
    const ComputedStyle* m_parentStyle;
    const ComputedStyle* m_rootElementStyle;

    bool m_fontDirty { false };
};

class MutatorFontUpdateScope {
public:
    MutatorFontUpdateScope(Mutator& mutator)
        : m_mutator { mutator }
    {
    }

    ~MutatorFontUpdateScope()
    {
        m_mutator.updateFont();
    }

private:
    Mutator& m_mutator;
};

} // namespace Style
} // namespace WebCore
