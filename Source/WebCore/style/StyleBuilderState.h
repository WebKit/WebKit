/*
 * Copyright (C) 2019-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include "CSSToLengthConversionData.h"
#include "Document.h"
#include "FontTaggedSettings.h"
#include "PropertyCascade.h"
#include "RenderStyle.h"
#include "RuleSet.h"
#include "SelectorChecker.h"
#include "StyleForVisitedLink.h"
#include "TextFlags.h"
#include "TreeResolutionState.h"
#include <wtf/BitSet.h>

namespace WebCore {

class FontCascadeDescription;
class FontSelectionValue;
class StyleResolver;

namespace CSSCalc {
struct RandomCachingKey;
}

namespace Style {

class BuilderState;
class Image;
struct Color;
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

enum class PositionTryFallbackTactic : uint8_t;

void maybeUpdateFontForLetterSpacingOrWordSpacing(BuilderState&, CSSValue&);

enum class ApplyValueType : uint8_t { Value, Initial, Inherit };

struct BuilderPositionTryFallback {
    RefPtr<const StyleProperties> properties;
    Vector<PositionTryFallbackTactic> tactics;
};

struct BuilderContext {
    const RefPtr<const Document> document { };
    const RenderStyle* parentStyle { };
    const RenderStyle* rootElementStyle { };
    RefPtr<const Element> element { };
    CheckedPtr<TreeResolutionState> treeResolutionState { };
    std::optional<BuilderPositionTryFallback> positionTryFallback { };
};

class BuilderState : public CanMakeCheckedPtr<BuilderState> {
    WTF_MAKE_TZONE_ALLOCATED(BuilderState);
    WTF_MAKE_NONCOPYABLE(BuilderState);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(BuilderState);
public:
    template<typename T, class... Args> friend WTF::UniqueRef<T> WTF::makeUniqueRefWithoutFastMallocCheck(Args&&...);

    static UniqueRef<BuilderState> create(RenderStyle& renderStyle)
    {
        return makeUniqueRefWithoutRefCountedCheck<BuilderState>(renderStyle);
    }

    static UniqueRef<BuilderState> create(RenderStyle& renderStyle, BuilderContext&& builderContext)
    {
        return makeUniqueRefWithoutRefCountedCheck<BuilderState>(renderStyle, WTF::move(builderContext));
    }

    ComputedStyle& style() { return m_style.computedStyle(); }
    const ComputedStyle& style() const { return m_style.computedStyle(); }

    RenderStyle& renderStyle() LIFETIME_BOUND { return m_style; }
    const RenderStyle& renderStyle() const LIFETIME_BOUND { return m_style; }

    const ComputedStyle& parentStyle() const { return m_context.parentStyle->computedStyle(); }
    const RenderStyle& parentRenderStyle() const LIFETIME_BOUND { return *m_context.parentStyle; }

    const ComputedStyle* rootElementStyle() const { return m_context.rootElementStyle ? &m_context.rootElementStyle->computedStyle() : nullptr; }
    const RenderStyle* rootElementRenderStyle() const LIFETIME_BOUND { return m_context.rootElementStyle; }

    const Document& document() const { return *m_context.document; }
    const Element* element() const { return m_context.element.get(); }

    inline void setZoom(Zoom);
    inline void setUsedZoom(float);
    inline void setWritingMode(StyleWritingMode);
    inline void setTextOrientation(TextOrientation);

    bool fontDirty() const { return m_fontDirty; }
    void setFontDirty() { m_fontDirty = true; }

    inline const FontCascadeDescription& fontDescription() LIFETIME_BOUND;
    inline const FontCascadeDescription& parentFontDescription() LIFETIME_BOUND;

    bool applyPropertyToRegularStyle() const { return m_linkMatch != SelectorChecker::MatchVisited; }
    bool applyPropertyToVisitedLinkStyle() const { return m_linkMatch != SelectorChecker::MatchLink; }

    float zoomWithTextZoomFactor();

    bool NODELETE useSVGZoomRules() const;
    bool NODELETE useSVGZoomRulesForLength() const;

    ScopeOrdinal styleScopeOrdinal() const { return m_currentProperty->styleScopeOrdinal; }

    RefPtr<Image> createStyleImage(const CSSValue&) const;

    const Vector<AtomString>& registeredContentAttributes() const LIFETIME_BOUND { return m_registeredContentAttributes; }
    void registerContentAttribute(const AtomString& attributeLocalName);

    const CSSToLengthConversionData& cssToLengthConversionData() const LIFETIME_BOUND { return m_cssToLengthConversionData; }

    void setIsBuildingKeyframeStyle() { m_isBuildingKeyframeStyle = true; }
    bool hasRevertRuleOrLayerInKeyframeStyle() const { return m_hasRevertRuleOrLayerInKeyframeStyle; }

    bool isAuthorOrigin() const
    {
        return m_currentProperty && m_currentProperty->origin == PropertyCascade::Origin::Author;
    }

    CSSPropertyID NODELETE cssPropertyID() const;

    bool NODELETE isCurrentPropertyInvalidAtComputedValueTime() const;
    void NODELETE setCurrentPropertyInvalidAtComputedValueTime();

    void NODELETE setUsesViewportUnits();
    void NODELETE setUsesContainerUnits();

    double lookupCSSRandomBaseValue(const CSSCalc::RandomCachingKey&, std::optional<CSS::Keyword::ElementShared>) const;

    // Accessors for sibling information used by the sibling-count() and sibling-index() CSS functions.
    unsigned siblingCount();
    unsigned siblingIndex();

    AnchorPositionedStates* anchorPositionedStates() LIFETIME_BOUND { return m_context.treeResolutionState ? &m_context.treeResolutionState->anchorPositionedStates : nullptr; }
    const std::optional<BuilderPositionTryFallback>& positionTryFallback() const LIFETIME_BOUND { return m_context.positionTryFallback; }

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
    void setFontDescriptionFontSynthesisStyle(FontSynthesisLonghandValue);
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

    void disableNativeAppearanceIfNeeded(CSSPropertyID, PropertyCascade::Origin);

private:
    // See the comment in maybeUpdateFontForLetterSpacingOrWordSpacing() about why this needs to be a friend.
    friend void maybeUpdateFontForLetterSpacingOrWordSpacing(BuilderState&, CSSValue&);
    friend class Builder;

    BuilderState(RenderStyle&);
    BuilderState(RenderStyle&, BuilderContext&&);

    void NODELETE adjustStyleForInterCharacterRuby();

    void updateFont();
#if ENABLE(TEXT_AUTOSIZING)
    void updateFontForTextSizeAdjust();
#endif
    void updateFontForZoomChange();
    void updateFontForGenericFamilyChange();
    void updateFontForOrientationChange();
    void updateFontForSizeChange();

    RenderStyle& m_style;
    BuilderContext m_context;

    const CSSToLengthConversionData m_cssToLengthConversionData;

    HashSet<AtomString> m_appliedCustomProperties;
    HashSet<AtomString> m_inProgressCustomProperties;
    HashSet<AtomString> m_inCycleCustomProperties;
    WTF::BitSet<cssPropertyIDEnumValueCount> m_inProgressProperties;
    WTF::BitSet<cssPropertyIDEnumValueCount> m_invalidAtComputedValueTimeProperties;

    const PropertyCascade::Property* m_currentProperty { nullptr };
    SelectorChecker::LinkMatchMask m_linkMatch { };
    const PropertyCascade* m_currentRollbackCascade { nullptr };

    bool m_fontDirty { false };
    Vector<AtomString> m_registeredContentAttributes;

    bool m_isBuildingKeyframeStyle { false };
    bool m_hasRevertRuleOrLayerInKeyframeStyle { false };
};

} // namespace Style
} // namespace WebCore
