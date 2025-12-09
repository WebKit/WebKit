/*
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

#include <WebCore/RenderStyleBase.h>

#include <WebCore/StyleAppleColorFilterData.h>
#include <WebCore/StyleBackdropFilterData.h>
#include <WebCore/StyleBackgroundData.h>
#include <WebCore/StyleBoxData.h>
#include <WebCore/StyleDeprecatedFlexibleBoxData.h>
#include <WebCore/StyleFillLayers.h>
#include <WebCore/StyleFilterData.h>
#include <WebCore/StyleFlexibleBoxData.h>
#include <WebCore/StyleFontData.h>
#include <WebCore/StyleFontFamily.h>
#include <WebCore/StyleFontFeatureSettings.h>
#include <WebCore/StyleFontPalette.h>
#include <WebCore/StyleFontSize.h>
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
#include <WebCore/StyleMarqueeData.h>
#include <WebCore/StyleMiscNonInheritedData.h>
#include <WebCore/StyleMultiColData.h>
#include <WebCore/StyleNonInheritedData.h>
#include <WebCore/StyleRareInheritedData.h>
#include <WebCore/StyleRareNonInheritedData.h>
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

// MARK: - RenderStyleBase::NonInheritedFlags

inline bool RenderStyleBase::NonInheritedFlags::hasPseudoStyle(PseudoElementType pseudo) const
{
    ASSERT(allPublicPseudoElementTypes.contains(pseudo));
    return EnumSet<PseudoElementType>::fromRaw(pseudoBits).contains(pseudo);
}

inline bool RenderStyleBase::NonInheritedFlags::hasAnyPublicPseudoStyles() const
{
    return !!pseudoBits;
}

// MARK: - Non-property getters

inline bool RenderStyleBase::usesViewportUnits() const
{
    return m_nonInheritedFlags.usesViewportUnits;
}

inline bool RenderStyleBase::usesContainerUnits() const
{
    return m_nonInheritedFlags.usesContainerUnits;
}

inline bool RenderStyleBase::useTreeCountingFunctions() const
{
    return m_nonInheritedFlags.useTreeCountingFunctions;
}

inline InsideLink RenderStyleBase::insideLink() const
{
    return static_cast<InsideLink>(m_inheritedFlags.insideLink);
}

inline bool RenderStyleBase::isLink() const
{
    return m_nonInheritedFlags.isLink;
}

inline bool RenderStyleBase::emptyState() const
{
    return m_nonInheritedFlags.emptyState;
}

inline bool RenderStyleBase::firstChildState() const
{
    return m_nonInheritedFlags.firstChildState;
}

inline bool RenderStyleBase::lastChildState() const
{
    return m_nonInheritedFlags.lastChildState;
}

inline bool RenderStyleBase::hasExplicitlyInheritedProperties() const
{
    return m_nonInheritedFlags.hasExplicitlyInheritedProperties;
}

inline bool RenderStyleBase::disallowsFastPathInheritance() const
{
    return m_nonInheritedFlags.disallowsFastPathInheritance;
}

inline bool RenderStyleBase::effectiveInert() const
{
    return m_rareInheritedData->effectiveInert;
}

inline bool RenderStyleBase::isEffectivelyTransparent() const
{
    return m_rareInheritedData->effectivelyTransparent;
}

inline bool RenderStyleBase::insideDefaultButton() const
{
    return m_rareInheritedData->insideDefaultButton;
}

inline bool RenderStyleBase::insideSubmitButton() const
{
    return m_rareInheritedData->insideSubmitButton;
}

inline bool RenderStyleBase::isInSubtreeWithBlendMode() const
{
    return m_rareInheritedData->isInSubtreeWithBlendMode;
}

inline bool RenderStyleBase::isForceHidden() const
{
    return m_rareInheritedData->isForceHidden;
}

inline bool RenderStyleBase::hasDisplayAffectedByAnimations() const
{
    return m_nonInheritedData->miscData->hasDisplayAffectedByAnimations;
}

inline bool RenderStyleBase::transformStyleForcedToFlat() const
{
    return static_cast<bool>(m_nonInheritedData->rareData->transformStyleForcedToFlat);
}

inline bool RenderStyleBase::usesAnchorFunctions() const
{
    return m_nonInheritedData->rareData->usesAnchorFunctions;
}

inline EnumSet<BoxAxis> RenderStyleBase::anchorFunctionScrollCompensatedAxes() const
{
    return EnumSet<BoxAxis>::fromRaw(m_nonInheritedData->rareData->anchorFunctionScrollCompensatedAxes);
}

inline bool RenderStyleBase::isPopoverInvoker() const
{
    return m_nonInheritedData->rareData->isPopoverInvoker;
}

inline bool RenderStyleBase::autoRevealsWhenFound() const
{
    return m_rareInheritedData->autoRevealsWhenFound;
}

inline bool RenderStyleBase::nativeAppearanceDisabled() const
{
    return m_nonInheritedData->rareData->nativeAppearanceDisabled;
}

inline OptionSet<EventListenerRegionType> RenderStyleBase::eventListenerRegionTypes() const
{
    return m_rareInheritedData->eventListenerRegionTypes;
}

inline bool RenderStyleBase::hasAttrContent() const
{
    return m_nonInheritedData->miscData->hasAttrContent;
}

inline std::optional<size_t> RenderStyleBase::usedPositionOptionIndex() const
{
    return m_nonInheritedData->rareData->usedPositionOptionIndex;
}

inline constexpr DisplayType RenderStyleBase::originalDisplay() const
{
    return static_cast<DisplayType>(m_nonInheritedFlags.originalDisplay);
}

inline DisplayType RenderStyleBase::effectiveDisplay() const
{
    return static_cast<DisplayType>(m_nonInheritedFlags.effectiveDisplay);
}

// MARK: - Zoom

inline bool RenderStyleBase::evaluationTimeZoomEnabled() const
{
    return m_rareInheritedData->evaluationTimeZoomEnabled;
}

inline float RenderStyleBase::deviceScaleFactor() const
{
    return m_rareInheritedData->deviceScaleFactor;
}

inline bool RenderStyleBase::useSVGZoomRulesForLength() const
{
    return m_nonInheritedData->rareData->useSVGZoomRulesForLength;
}

inline float RenderStyleBase::usedZoom() const
{
    return m_rareInheritedData->usedZoom;
}

inline Style::ZoomFactor RenderStyleBase::usedZoomForLength() const
{
    if (useSVGZoomRulesForLength())
        return Style::ZoomFactor(1.0f, deviceScaleFactor());

    if (evaluationTimeZoomEnabled())
        return Style::ZoomFactor(usedZoom(), deviceScaleFactor());

    return Style::ZoomFactor(1.0f, deviceScaleFactor());
}

// MARK: - Fonts

inline const FontCascade& RenderStyleBase::fontCascade() const
{
    return m_inheritedData->fontData->fontCascade;
}

inline Style::WebkitLocale RenderStyleBase::computedLocale() const
{
    return fontDescription().computedLocale();
}

// MARK: - Aggregates

inline const Style::InsetBox& RenderStyleBase::insetBox() const
{
    return m_nonInheritedData->surroundData->inset;
}

inline const Style::MarginBox& RenderStyleBase::marginBox() const
{
    return m_nonInheritedData->surroundData->margin;
}

inline const Style::PaddingBox& RenderStyleBase::paddingBox() const
{
    return m_nonInheritedData->surroundData->padding;
}

inline const Style::ScrollMarginBox& RenderStyleBase::scrollMarginBox() const
{
    return m_nonInheritedData->rareData->scrollMargin;
}

inline const Style::ScrollPaddingBox& RenderStyleBase::scrollPaddingBox() const
{
    return m_nonInheritedData->rareData->scrollPadding;
}

inline const Style::ScrollTimelines& RenderStyleBase::scrollTimelines() const
{
    return m_nonInheritedData->rareData->scrollTimelines;
}

inline const Style::ViewTimelines& RenderStyleBase::viewTimelines() const
{
    return m_nonInheritedData->rareData->viewTimelines;
}

inline const Style::Animations& RenderStyleBase::animations() const
{
    return m_nonInheritedData->miscData->animations;
}

inline const Style::Transitions& RenderStyleBase::transitions() const
{
    return m_nonInheritedData->miscData->transitions;
}

inline const Style::BackgroundLayers& RenderStyleBase::backgroundLayers() const
{
    return m_nonInheritedData->backgroundData->background;
}

inline const Style::MaskLayers& RenderStyleBase::maskLayers() const
{
    return m_nonInheritedData->miscData->mask;
}

inline const Style::MaskBorder& RenderStyleBase::maskBorder() const
{
    return m_nonInheritedData->rareData->maskBorder;
}

inline const Style::BorderImage& RenderStyleBase::borderImage() const
{
    return border().image();
}

inline const Style::TransformOrigin& RenderStyleBase::transformOrigin() const
{
    return m_nonInheritedData->miscData->transform->origin;
}

inline const Style::PerspectiveOrigin& RenderStyleBase::perspectiveOrigin() const
{
    return m_nonInheritedData->rareData->perspectiveOrigin;
}

inline const OutlineValue& RenderStyleBase::outline() const
{
    return m_nonInheritedData->backgroundData->outline;
}

inline const BorderData& RenderStyleBase::border() const
{
    return m_nonInheritedData->surroundData->border;
}

inline Style::LineWidthBox RenderStyleBase::borderWidth() const
{
    return border().borderWidth();
}

inline const Style::BorderRadius& RenderStyleBase::borderRadii() const
{
    return border().radii();
}

inline const BorderValue& RenderStyleBase::borderBottom() const
{
    return border().bottom();
}

inline const BorderValue& RenderStyleBase::borderLeft() const
{
    return border().left();
}

inline const BorderValue& RenderStyleBase::borderRight() const
{
    return border().right();
}

inline const BorderValue& RenderStyleBase::borderTop() const
{
    return border().top();
}

inline const BorderValue& RenderStyleBase::columnRule() const
{
    return m_nonInheritedData->miscData->multiCol->columnRule;
}

// MARK: - Properties/descriptors that are not yet generated

inline CursorType RenderStyleBase::cursorType() const
{
    return static_cast<CursorType>(m_inheritedFlags.cursorType);
}

// FIXME: Support descriptors

inline const Style::PageSize& RenderStyleBase::pageSize() const
{
    return m_nonInheritedData->rareData->pageSize;
}

// FIXME: Add a type that encapsulates both caretColor() and hasAutoCaretColor().

inline const Style::Color& RenderStyleBase::caretColor() const
{
    return m_rareInheritedData->caretColor;
}

inline bool RenderStyleBase::hasAutoCaretColor() const
{
    return m_rareInheritedData->hasAutoCaretColor;
}

inline const Style::Color& RenderStyleBase::visitedLinkCaretColor() const
{
    return m_rareInheritedData->visitedLinkCaretColor;
}

inline bool RenderStyleBase::hasVisitedLinkAutoCaretColor() const
{
    return m_rareInheritedData->hasVisitedLinkAutoCaretColor;
}

} // namespace WebCore
