/*
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/StyleComputedStyleBase.h>

#include <WebCore/StyleAppleColorFilterData.h>
#include <WebCore/StyleBackdropFilterData.h>
#include <WebCore/StyleBackgroundData.h>
#include <WebCore/StyleBorderImage.h>
#include <WebCore/StyleBorderImageData.h>
#include <WebCore/StyleBoxData.h>
#include <WebCore/StyleCustomPropertyData.h>
#include <WebCore/StyleDeprecatedFlexibleBoxData.h>
#include <WebCore/StyleDisplay.h>
#include <WebCore/StyleFillLayers.h>
#include <WebCore/StyleFilterData.h>
#include <WebCore/StyleFlexibleBoxData.h>
#include <WebCore/StyleFontData.h>
#include <WebCore/StyleFontFamily.h>
#include <WebCore/StyleFontFeatureSettings.h>
#include <WebCore/StyleFontPalette.h>
#include <WebCore/StyleFontSizeAdjust.h>
#include <WebCore/StyleFontStyle.h>
#include <WebCore/StyleFontVariantAlternates.h>
#include <WebCore/StyleFontVariantEastAsian.h>
#include <WebCore/StyleFontVariantLigatures.h>
#include <WebCore/StyleFontVariantNumeric.h>
#include <WebCore/StyleFontVariationSettings.h>
#include <WebCore/StyleFontWeight.h>
#include <WebCore/StyleFontWidth.h>
#include <WebCore/StyleGridData.h>
#include <WebCore/StyleGridItemData.h>
#include <WebCore/StyleGridTrackSizingDirection.h>
#include <WebCore/StyleInheritedData.h>
#include <WebCore/StyleInheritedRareData.h>
#include <WebCore/StyleMarqueeData.h>
#include <WebCore/StyleMaskBorder.h>
#include <WebCore/StyleMaskBorderData.h>
#include <WebCore/StyleMultiColumnData.h>
#include <WebCore/StyleNonInheritedData.h>
#include <WebCore/StyleNonInheritedMiscData.h>
#include <WebCore/StyleNonInheritedRareData.h>
#include <WebCore/StyleSVGData.h>
#include <WebCore/StyleSVGFillData.h>
#include <WebCore/StyleSVGLayoutData.h>
#include <WebCore/StyleSVGMarkerResourceData.h>
#include <WebCore/StyleSVGNonInheritedMiscData.h>
#include <WebCore/StyleSVGStopData.h>
#include <WebCore/StyleSVGStrokeData.h>
#include <WebCore/StyleSurroundData.h>
#include <WebCore/StyleTextAlign.h>
#include <WebCore/StyleTextAutospace.h>
#include <WebCore/StyleTextDecorationLine.h>
#include <WebCore/StyleTextSpacingTrim.h>
#include <WebCore/StyleTextTransform.h>
#include <WebCore/StyleTransformData.h>
#include <WebCore/StyleVisitedLinkColorData.h>
#include <WebCore/StyleWebKitLocale.h>
#include <WebCore/ViewTimeline.h>

namespace WebCore {
namespace Style {

// MARK: - ComputedStyleBase::NonInheritedFlags

inline bool ComputedStyleBase::NonInheritedFlags::hasPseudoStyle(PseudoElementType pseudo) const
{
    ASSERT(allPublicPseudoElementTypes.contains(pseudo));
    return EnumSet<PseudoElementType>::fromRaw(pseudoBits).contains(pseudo);
}

inline bool ComputedStyleBase::NonInheritedFlags::hasAnyPublicPseudoStyles() const
{
    return !!pseudoBits;
}

// MARK: - Non-property getters

inline bool ComputedStyleBase::usesViewportUnits() const
{
    return m_nonInheritedFlags.usesViewportUnits;
}

inline bool ComputedStyleBase::usesContainerUnits() const
{
    return m_nonInheritedFlags.usesContainerUnits;
}

inline bool ComputedStyleBase::useTreeCountingFunctions() const
{
    return m_nonInheritedFlags.useTreeCountingFunctions;
}

inline InsideLink ComputedStyleBase::insideLink() const
{
    return static_cast<InsideLink>(m_inheritedFlags.insideLink);
}

inline bool ComputedStyleBase::isLink() const
{
    return m_nonInheritedFlags.isLink;
}

inline bool ComputedStyleBase::emptyState() const
{
    return m_nonInheritedFlags.emptyState;
}

inline bool ComputedStyleBase::firstChildState() const
{
    return m_nonInheritedFlags.firstChildState;
}

inline bool ComputedStyleBase::lastChildState() const
{
    return m_nonInheritedFlags.lastChildState;
}

inline bool ComputedStyleBase::hasExplicitlyInheritedProperties() const
{
    return m_nonInheritedFlags.hasExplicitlyInheritedProperties;
}

inline bool ComputedStyleBase::disallowsFastPathInheritance() const
{
    return m_nonInheritedFlags.disallowsFastPathInheritance;
}

inline bool ComputedStyleBase::effectiveInert() const
{
    return m_inheritedRareData->effectiveInert;
}

inline bool ComputedStyleBase::isEffectivelyTransparent() const
{
    return m_inheritedRareData->effectivelyTransparent;
}

inline bool ComputedStyleBase::insideDefaultButton() const
{
    return m_inheritedRareData->insideDefaultButton;
}

inline bool ComputedStyleBase::insideSubmitButton() const
{
    return m_inheritedRareData->insideSubmitButton;
}

inline bool ComputedStyleBase::isInSubtreeWithBlendMode() const
{
    return m_inheritedRareData->isInSubtreeWithBlendMode;
}

inline bool ComputedStyleBase::isForceHidden() const
{
    return m_inheritedRareData->isForceHidden;
}

inline bool ComputedStyleBase::hasDisplayAffectedByAnimations() const
{
    return m_nonInheritedData->miscData->hasDisplayAffectedByAnimations;
}

inline bool ComputedStyleBase::transformStyleForcedToFlat() const
{
    return static_cast<bool>(m_nonInheritedData->rareData->transformStyleForcedToFlat);
}

inline bool ComputedStyleBase::usesAnchorFunctions() const
{
    return m_nonInheritedData->rareData->usesAnchorFunctions;
}

inline EnumSet<BoxAxis> ComputedStyleBase::anchorFunctionScrollCompensatedAxes() const
{
    return EnumSet<BoxAxis>::fromRaw(m_nonInheritedData->rareData->anchorFunctionScrollCompensatedAxes);
}

inline bool ComputedStyleBase::isPopoverInvoker() const
{
    return m_nonInheritedData->rareData->isPopoverInvoker;
}

inline bool ComputedStyleBase::autoRevealsWhenFound() const
{
    return m_inheritedRareData->autoRevealsWhenFound;
}

inline bool ComputedStyleBase::nativeAppearanceDisabled() const
{
    return m_nonInheritedData->rareData->nativeAppearanceDisabled;
}

inline OptionSet<EventListenerRegionType> ComputedStyleBase::eventListenerRegionTypes() const
{
    return m_inheritedRareData->eventListenerRegionTypes;
}

inline bool ComputedStyleBase::hasAttrContent() const
{
    return m_nonInheritedData->miscData->hasAttrContent;
}

inline std::optional<size_t> ComputedStyleBase::usedPositionOptionIndex() const
{
    return m_nonInheritedData->rareData->usedPositionOptionIndex;
}

inline constexpr Display ComputedStyleBase::originalDisplay() const
{
    return Display::fromRaw(m_nonInheritedFlags.originalDisplay);
}

inline StyleAppearance ComputedStyleBase::usedAppearance() const
{
    return static_cast<StyleAppearance>(m_nonInheritedData->miscData->usedAppearance);
}

inline ContentVisibility ComputedStyleBase::usedContentVisibility() const
{
    return static_cast<ContentVisibility>(m_inheritedRareData->usedContentVisibility);
}

inline TouchAction ComputedStyleBase::usedTouchAction() const
{
    return m_inheritedRareData->usedTouchAction;
}

inline ZIndex ComputedStyleBase::usedZIndex() const
{
    return m_nonInheritedData->boxData->usedZIndex();
}

#if HAVE(CORE_MATERIAL)

inline AppleVisualEffect ComputedStyleBase::usedAppleVisualEffectForSubtree() const
{
    return static_cast<AppleVisualEffect>(m_inheritedRareData->usedAppleVisualEffectForSubtree);
}

#endif

inline std::optional<PseudoElementType> ComputedStyleBase::pseudoElementType() const
{
    return m_nonInheritedFlags.pseudoElementType ? std::make_optional(static_cast<PseudoElementType>(m_nonInheritedFlags.pseudoElementType - 1)) : std::nullopt;
}

inline const AtomString& ComputedStyleBase::pseudoElementNameArgument() const
{
    return m_nonInheritedData->rareData->pseudoElementNameArgument;
}

inline bool ComputedStyleBase::hasPseudoStyle(PseudoElementType pseudo) const
{
    return m_nonInheritedFlags.hasPseudoStyle(pseudo);
}

inline bool ComputedStyleBase::hasAnyPublicPseudoStyles() const
{
    return m_nonInheritedFlags.hasAnyPublicPseudoStyles();
}

// MARK: - Custom properties

inline const CustomPropertyData& ComputedStyleBase::inheritedCustomProperties() const
{
    return m_inheritedRareData->customProperties.get();
}

inline const CustomPropertyData& ComputedStyleBase::nonInheritedCustomProperties() const
{
    return m_nonInheritedData->rareData->customProperties.get();
}

// MARK: - Zoom

inline bool ComputedStyleBase::evaluationTimeZoomEnabled() const
{
    return m_inheritedRareData->evaluationTimeZoomEnabled;
}

inline bool ComputedStyleBase::useSVGZoomRulesForLength() const
{
    return m_nonInheritedData->rareData->useSVGZoomRulesForLength;
}

inline float ComputedStyleBase::usedZoom() const
{
    return m_inheritedRareData->usedZoom;
}

inline ZoomFactor ComputedStyleBase::usedZoomForLength() const
{
    static constexpr ZoomFactor unzoomed(1.0f);
    if (!inheritedFlags().isZoomed)
        return unzoomed;

    if (useSVGZoomRulesForLength())
        return unzoomed;

    if (evaluationTimeZoomEnabled())
        return ZoomFactor(usedZoom());

    return unzoomed;
}

// MARK: - Fonts

inline const FontCascade& ComputedStyleBase::fontCascade() const
{
    return m_inheritedData->fontData->fontCascade;
}

inline WebkitLocale ComputedStyleBase::computedLocale() const
{
    return fontDescription().computedLocale();
}

inline float ComputedStyleBase::usedLetterSpacing() const
{
    return fontCascade().letterSpacing();
}

inline float ComputedStyleBase::usedWordSpacing() const
{
    return fontCascade().wordSpacing();
}

// MARK: - Aggregates

inline const InsetBox& ComputedStyleBase::insetBox() const
{
    return m_nonInheritedData->surroundData->inset;
}

inline const MarginBox& ComputedStyleBase::marginBox() const
{
    return m_nonInheritedData->surroundData->margin;
}

inline const PaddingBox& ComputedStyleBase::paddingBox() const
{
    return m_nonInheritedData->surroundData->padding;
}

inline const ScrollMarginBox& ComputedStyleBase::scrollMarginBox() const
{
    return m_nonInheritedData->rareData->scrollMargin;
}

inline const ScrollPaddingBox& ComputedStyleBase::scrollPaddingBox() const
{
    return m_nonInheritedData->rareData->scrollPadding;
}

inline const ScrollTimelines& ComputedStyleBase::scrollTimelines() const
{
    return m_nonInheritedData->rareData->scrollTimelines;
}

inline const ViewTimelines& ComputedStyleBase::viewTimelines() const
{
    return m_nonInheritedData->rareData->viewTimelines;
}

inline const Animations& ComputedStyleBase::animations() const
{
    return m_nonInheritedData->miscData->animations;
}

inline const Transitions& ComputedStyleBase::transitions() const
{
    return m_nonInheritedData->miscData->transitions;
}

inline const BackgroundLayers& ComputedStyleBase::backgroundLayers() const
{
    return m_nonInheritedData->backgroundData->background;
}

inline const MaskLayers& ComputedStyleBase::maskLayers() const
{
    return m_nonInheritedData->miscData->mask;
}

inline const MaskBorder& ComputedStyleBase::maskBorder() const
{
    return m_nonInheritedData->rareData->maskBorder->maskBorder;
}

inline const BorderImage& ComputedStyleBase::borderImage() const
{
    return m_nonInheritedData->surroundData->border.borderImage->borderImage;
}

inline const TransformOrigin& ComputedStyleBase::transformOrigin() const
{
    return m_nonInheritedData->miscData->transform->origin;
}

inline const PerspectiveOrigin& ComputedStyleBase::perspectiveOrigin() const
{
    return m_nonInheritedData->rareData->perspectiveOrigin;
}

inline const OutlineValue& ComputedStyleBase::outline() const
{
    return m_nonInheritedData->backgroundData->outline;
}

inline const BorderData& ComputedStyleBase::border() const
{
    return m_nonInheritedData->surroundData->border;
}

inline const BorderRadius& ComputedStyleBase::borderRadii() const
{
    return border().radii;
}

inline const BorderValue& ComputedStyleBase::borderBottom() const
{
    return border().bottom();
}

inline const BorderValue& ComputedStyleBase::borderLeft() const
{
    return border().left();
}

inline const BorderValue& ComputedStyleBase::borderRight() const
{
    return border().right();
}

inline const BorderValue& ComputedStyleBase::borderTop() const
{
    return border().top();
}

inline const BorderValue& ComputedStyleBase::columnRule() const
{
    return m_nonInheritedData->miscData->multiCol->columnRule;
}

// MARK: - Properties/descriptors that are not yet generated

inline CursorType ComputedStyleBase::cursorType() const
{
    return static_cast<CursorType>(m_inheritedFlags.cursorType);
}

// FIXME: Support descriptors

inline const PageSize& ComputedStyleBase::pageSize() const
{
    return m_nonInheritedData->rareData->pageSize;
}

} // namespace Style
} // namespace WebCore
