/*
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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

#include "config.h"
#include "StyleLengthResolution.h"

#include "BoxSides.h"
#include "CSSPrimitiveNumericUnits.h"
#include "CSSToLengthConversionData.h"
#include "ContainerNodeInlines.h"
#include "ContainerQueryEvaluator.h"
#include "Document.h"
#include "Element.h"
#include "FontCascadeInlines.h"
#include "FontCascadeDescription.h"
#include "FontMetrics.h"
#include "NodeRenderStyle.h"
#include "RenderBox.h"
#include "RenderBoxInlines.h"
#include "RenderView.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleLineHeight.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"

namespace WebCore {
namespace Style {

// MARK: - Page Zoom

static double NODELETE unapplyPageZoom(double dimension, const RenderView* renderView)
{
    if (!renderView)
        return dimension;
    return dimension / renderView->pageZoomFactor();
}

static double NODELETE unapplyPageZoom(double dimension, const CSSToLengthConversionData& conversionData)
{
    return unapplyPageZoom(dimension, conversionData.renderView());
}

// MARK: - Text Zoom

static double applyTextZoom(double value, const CSSToLengthConversionData& conversionData)
{
    if (CheckedPtr builderState = conversionData.styleBuilderState())
        return value * builderState->zoomWithTextZoomFactor();
    return value;
}

static double applyTextZoomIf(bool condition, double value, const CSSToLengthConversionData& conversionData)
{
    return condition ? applyTextZoom(value, conversionData) : value;
}

// MARK: - Font Metric Zoom

// Raw font metrics include the usedZoomFactor and must be normalized.
static double unzoomFontMetric(double metric, const FontDescription& fontDescription)
{
    if (auto usedZoomFactor = fontDescription.usedZoomFactor(); usedZoomFactor > 0)
        return metric / usedZoomFactor;
    return metric;
}


// MARK: - "font dependent" and "root font dependent" resolution functions

// Resolve the "em", "rem" and "quirky-em" units.
// https://drafts.csswg.org/css-values-4/#em
// https://drafts.csswg.org/css-values-4/#rem
static double resolveEm(CSSPropertyID propertyToCompute, const FontCascade& fontCascadeForUnit)
{
    auto& fontDescription = fontCascadeForUnit.fontDescription();

    // FIXME: Should probably use specifiedSize for every property.
    if (propertyToCompute == CSSPropertyFontSize)
        return fontDescription.specifiedSize();

    return fontDescription.unzoomedComputedSize();
}

// Resolve the "ex" and "rex" units.
// https://drafts.csswg.org/css-values-4/#ex
// https://drafts.csswg.org/css-values-4/#rex
static double resolveEx(CSSPropertyID propertyToCompute, const FontCascade& fontCascadeForUnit)
{
    auto& fontDescription = fontCascadeForUnit.fontDescription();
    auto& fontMetrics = fontCascadeForUnit.metricsOfPrimaryFont();
    if (fontMetrics.xHeight())
        return unzoomFontMetric(fontMetrics.xHeight().value(), fontDescription);

    // FIXME: Should probably use specifiedSize for every property.
    if (propertyToCompute == CSSPropertyFontSize)
        return fontDescription.specifiedSize() / 2.0;

    return fontDescription.unzoomedComputedSize() / 2.0;
}

// Resolve the "cap" and "rcap" units.
// https://drafts.csswg.org/css-values-4/#cap
// https://drafts.csswg.org/css-values-4/#rcap
static double resolveCap(CSSPropertyID, const FontCascade& fontCascadeForUnit)
{
    auto& fontDescription = fontCascadeForUnit.fontDescription();
    auto& fontMetrics = fontCascadeForUnit.metricsOfPrimaryFont();
    if (fontMetrics.capHeight())
        return unzoomFontMetric(fontMetrics.capHeight().value(), fontDescription);
    return unzoomFontMetric(fontMetrics.intAscent(), fontDescription);
}

// Resolve the "ch" and "rch" units.
// https://drafts.csswg.org/css-values-4/#ch
// https://drafts.csswg.org/css-values-4/#rch
static double resolveCh(CSSPropertyID, const FontCascade& fontCascadeForUnit)
{
    return unzoomFontMetric(fontCascadeForUnit.zeroWidth(), fontCascadeForUnit.fontDescription());
}

// Resolve the "ic" and "ric" units.
// https://drafts.csswg.org/css-values-4/#ic
// https://drafts.csswg.org/css-values-4/#ric
static double resolveIc(CSSPropertyID, const FontCascade& fontCascadeForUnit)
{
    auto& fontDescription = fontCascadeForUnit.fontDescription();
    auto ideogramWidth = fontCascadeForUnit.metricsOfPrimaryFont().ideogramWidth();
    if (!ideogramWidth)
        return fontDescription.unzoomedComputedSize();
    return unzoomFontMetric(ideogramWidth.value(), fontDescription);
}


static const ComputedStyle* styleForLineHeightUnits(const CSSToLengthConversionData& conversionData)
{
    if (conversionData.computingLineHeight() || conversionData.computingFontSize())
        return conversionData.parentStyle();
    return conversionData.style();
}

static const ComputedStyle* styleForRootLineHeightUnits(const CSSToLengthConversionData& conversionData)
{
    return conversionData.rootStyle();
}

// Resolve the "lh" and "rlh" units.
// https://drafts.csswg.org/css-values-4/#lh
// https://drafts.csswg.org/css-values-4/#rlh
static double resolveLh(const ComputedStyle* style, const FontCascade& fallbackFontCascadeForUnit)
{
    if (style) {
        auto& fontCascade = style->fontCascade();
        auto& fontDescription = fontCascade.fontDescription();

        return evaluate<float>(
            style->specifiedLineHeight(),
            LineHeightEvaluationContext {
                fontDescription.specifiedSize(),
                static_cast<float>(unzoomFontMetric(fontCascade.metricsOfPrimaryFont().lineSpacing(), fontDescription)),
            },
            ZoomFactor::none()
        );
    }

    return unzoomFontMetric(fallbackFontCascadeForUnit.metricsOfPrimaryFont().lineSpacing(), fallbackFontCascadeForUnit.fontDescription());
}

// MARK: - "viewport-percentage" resolution functions

enum class ViewportType : uint8_t {
    Default,
    Small,
    Large,
    Dynamic,
};

enum class ViewportPhysicalDimension : uint8_t {
    Height,
    Width,
    Max,
    Min,
};

template<ViewportType type>
static FloatSize resolveViewportType(const RenderView& renderView)
{
    if constexpr (type == ViewportType::Default)
        return renderView.sizeForCSSDefaultViewportUnits();
    else if constexpr (type == ViewportType::Small)
        return renderView.sizeForCSSSmallViewportUnits();
    else if constexpr (type == ViewportType::Large)
        return renderView.sizeForCSSLargeViewportUnits();
    else if constexpr (type == ViewportType::Dynamic)
        return renderView.sizeForCSSDynamicViewportUnits();
}

template<ViewportPhysicalDimension axis>
static double resolveViewportPercentagePhysicalAxis(const FloatSize& size)
{
    if constexpr (axis == ViewportPhysicalDimension::Height)
        return size.height();
    else if constexpr (axis == ViewportPhysicalDimension::Width)
        return size.width();
    else if constexpr (axis == ViewportPhysicalDimension::Max)
        return size.maxDimension();
    else if constexpr (axis == ViewportPhysicalDimension::Min)
        return size.minDimension();
}

template<LogicalBoxAxis axis>
static double NODELETE resolveViewportPercentageLogicalAxis(const FloatSize& size, const ComputedStyle& style)
{
    switch (mapAxisLogicalToPhysical(style.writingMode(), axis)) {
    case BoxAxis::Horizontal:
        return size.width();
    case BoxAxis::Vertical:
        return size.height();
    }

    RELEASE_ASSERT_NOT_REACHED();
}

template<LogicalBoxAxis axis>
static double NODELETE resolveViewportPercentageLogicalAxis(const FloatSize& size, const Style::ComputedStyle* style)
{
    if (!style)
        return 0;
    return resolveViewportPercentageLogicalAxis<axis>(size, *style);
}

template<LogicalBoxAxis axis>
static double NODELETE resolveViewportPercentageLogicalAxis(const FloatSize& size, const RenderView& renderView)
{
    auto* rootElement = renderView.document().documentElement();
    if (!rootElement)
        return 0;
    return resolveViewportPercentageLogicalAxis<axis>(size, rootElement->renderStyle());
}

template<LogicalBoxAxis axis>
static double resolveViewportPercentageLogicalAxis(const FloatSize& size, const RenderView& renderView, const ComputedStyle* style)
{
    if (style)
        return resolveViewportPercentageLogicalAxis<axis>(size, *style) / renderView.pageZoomFactor();
    return resolveViewportPercentageLogicalAxis<axis>(size, renderView) / renderView.pageZoomFactor();
}

template<ViewportType type, ViewportPhysicalDimension axis>
static double resolveViewportPercentageUnit(const RenderView& renderView)
{
    return resolveViewportPercentagePhysicalAxis<axis>(resolveViewportType<type>(renderView)) / 100.0;
}

template<ViewportType type, LogicalBoxAxis axis>
static double resolveViewportPercentageUnit(const RenderView& renderView, const ComputedStyle* style)
{
    return resolveViewportPercentageLogicalAxis<axis>(resolveViewportType<type>(renderView), renderView, style) / 100.0;
}

template<ViewportType type, ViewportPhysicalDimension axis>
static double resolveViewportPercentageUnit(const RenderView* renderView)
{
    if (!renderView) {
        if constexpr (axis == ViewportPhysicalDimension::Max || axis == ViewportPhysicalDimension::Min)
            return 1;
        else
            return 0;
    }
    return resolveViewportPercentageUnit<type, axis>(*renderView) / renderView->pageZoomFactor();
}

template<ViewportType type, LogicalBoxAxis axis>
static double resolveViewportPercentageUnit(const RenderView* renderView, const ComputedStyle* style)
{
    if (!renderView)
        return 0;
    return resolveViewportPercentageUnit<type, axis>(*renderView, style) / renderView->pageZoomFactor();
}

// MARK: - "container-percentage" resolution functions

static std::optional<double> resolveContainerUnit(const CSSToLengthConversionData& conversionData, CQ::Axis physicalAxis)
{
    ASSERT(physicalAxis == CQ::Axis::Width || physicalAxis == CQ::Axis::Height);

    conversionData.setUsesContainerUnits();

    RefPtr element = conversionData.elementForContainerUnitResolution();
    if (!element)
        return { };

    auto mode = !conversionData.style()->pseudoElementType()
        ? Style::ContainerQueryEvaluator::SelectionMode::Element
        : Style::ContainerQueryEvaluator::SelectionMode::PseudoElement;

    // "The query container for each axis is the nearest ancestor container that accepts container size queries on that axis."
    while ((element = Style::ContainerQueryEvaluator::selectContainer(CQ::ContainerRequirements { physicalAxis }, nullString(), *element, mode))) {
        auto* containerRenderer = dynamicDowncast<RenderBox>(element->renderer());
        if (containerRenderer && containerRenderer->hasEligibleContainmentForSizeQuery()) {
            auto widthOrHeight = physicalAxis == CQ::Axis::Width ? containerRenderer->contentBoxWidth() : containerRenderer->contentBoxHeight();
            auto adjustedWidthOrHeight = widthOrHeight.toDouble();

            if (!conversionData.computingFontSize())
                adjustedWidthOrHeight = unapplyPageZoom(adjustedWidthOrHeight, conversionData);

            return adjustedWidthOrHeight / 100;
        }
        // For pseudo-elements the element itself can be the container. Avoid looping forever.
        mode = Style::ContainerQueryEvaluator::SelectionMode::Element;
    }
    return { };
}

// MARK: - Length Resolution

double resolveLength(double value, CSS::LengthUnit lengthUnit, CSSPropertyID propertyToCompute, const FontCascade& fontCascadeForUnit, const RenderView* renderView)
{
    using enum CSS::LengthUnit;

    switch (lengthUnit) {
    case Px:
        return value;
    case Cm:
        return value * CSS::pixelsPerCm;
    case Mm:
        return value * CSS::pixelsPerMm;
    case Q:
        return value * CSS::pixelsPerQ;
    case In:
        return value * CSS::pixelsPerInch;
    case Pt:
        return value * CSS::pixelsPerPt;
    case Pc:
        return value * CSS::pixelsPerPc;

    // MARK: "font dependent" and "root font dependent" resolution

    case Em:
    case QuirkyEm:
    case Rem:
        return value * resolveEm(propertyToCompute, fontCascadeForUnit);
    case Ex:
    case Rex:
        return value * resolveEx(propertyToCompute, fontCascadeForUnit);
    case Cap:
    case Rcap:
        return value * resolveCap(propertyToCompute, fontCascadeForUnit);
    case Ch:
    case Rch:
        return value * resolveCh(propertyToCompute, fontCascadeForUnit);
    case Ic:
    case Ric:
        return value * resolveIc(propertyToCompute, fontCascadeForUnit);

    // MARK: "viewport percentage" resolution

    case Vh:
        return value * resolveViewportPercentageUnit<ViewportType::Default, ViewportPhysicalDimension::Height>(renderView);
    case Vw:
        return value * resolveViewportPercentageUnit<ViewportType::Default, ViewportPhysicalDimension::Width>(renderView);
    case Vmax:
        return value * resolveViewportPercentageUnit<ViewportType::Default, ViewportPhysicalDimension::Max>(renderView);
    case Vmin:
        return value * resolveViewportPercentageUnit<ViewportType::Default, ViewportPhysicalDimension::Min>(renderView);
    case Vb:
        return value * resolveViewportPercentageUnit<ViewportType::Default, LogicalBoxAxis::Block>(renderView, nullptr);
    case Vi:
        return value * resolveViewportPercentageUnit<ViewportType::Default, LogicalBoxAxis::Inline>(renderView, nullptr);

    case Svh:
        return value * resolveViewportPercentageUnit<ViewportType::Small, ViewportPhysicalDimension::Height>(renderView);
    case Svw:
        return value * resolveViewportPercentageUnit<ViewportType::Small, ViewportPhysicalDimension::Width>(renderView);
    case Svmax:
        return value * resolveViewportPercentageUnit<ViewportType::Small, ViewportPhysicalDimension::Max>(renderView);
    case Svmin:
        return value * resolveViewportPercentageUnit<ViewportType::Small, ViewportPhysicalDimension::Min>(renderView);
    case Svb:
        return value * resolveViewportPercentageUnit<ViewportType::Small, LogicalBoxAxis::Block>(renderView, nullptr);
    case Svi:
        return value * resolveViewportPercentageUnit<ViewportType::Small, LogicalBoxAxis::Inline>(renderView, nullptr);

    case Lvh:
        return value * resolveViewportPercentageUnit<ViewportType::Large, ViewportPhysicalDimension::Height>(renderView);
    case Lvw:
        return value * resolveViewportPercentageUnit<ViewportType::Large, ViewportPhysicalDimension::Width>(renderView);
    case Lvmax:
        return value * resolveViewportPercentageUnit<ViewportType::Large, ViewportPhysicalDimension::Max>(renderView);
    case Lvmin:
        return value * resolveViewportPercentageUnit<ViewportType::Large, ViewportPhysicalDimension::Min>(renderView);
    case Lvb:
        return value * resolveViewportPercentageUnit<ViewportType::Large, LogicalBoxAxis::Block>(renderView, nullptr);
    case Lvi:
        return value * resolveViewportPercentageUnit<ViewportType::Large, LogicalBoxAxis::Inline>(renderView, nullptr);

    case Dvh:
        return value * resolveViewportPercentageUnit<ViewportType::Dynamic, ViewportPhysicalDimension::Height>(renderView);
    case Dvw:
        return value * resolveViewportPercentageUnit<ViewportType::Dynamic, ViewportPhysicalDimension::Width>(renderView);
    case Dvmax:
        return value * resolveViewportPercentageUnit<ViewportType::Dynamic, ViewportPhysicalDimension::Max>(renderView);
    case Dvmin:
        return value * resolveViewportPercentageUnit<ViewportType::Dynamic, ViewportPhysicalDimension::Min>(renderView);
    case Dvb:
        return value * resolveViewportPercentageUnit<ViewportType::Dynamic, LogicalBoxAxis::Block>(renderView, nullptr);
    case Dvi:
        return value * resolveViewportPercentageUnit<ViewportType::Dynamic, LogicalBoxAxis::Inline>(renderView, nullptr);

    case Lh:
    case Rlh:
    case Cqw:
    case Cqh:
    case Cqi:
    case Cqb:
    case Cqmin:
    case Cqmax:
        ASSERT_NOT_REACHED();
        return -1.0;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

double resolveLength(double value, CSS::LengthUnit lengthUnit, const CSSToLengthConversionData& conversionData)
{
    using enum CSS::LengthUnit;

    switch (lengthUnit) {
    case Px:
        return value * applyTextZoomIf(conversionData.computingLineHeight(), 1.0, conversionData);
    case Cm:
        return value * applyTextZoomIf(conversionData.computingLineHeight(), CSS::pixelsPerCm, conversionData);
    case Mm:
        return value * applyTextZoomIf(conversionData.computingLineHeight(), CSS::pixelsPerMm, conversionData);
    case Q:
        return value * applyTextZoomIf(conversionData.computingLineHeight(), CSS::pixelsPerQ, conversionData);
    case In:
        return value * applyTextZoomIf(conversionData.computingLineHeight(), CSS::pixelsPerInch, conversionData);
    case Pt:
        return value * applyTextZoomIf(conversionData.computingLineHeight(), CSS::pixelsPerPt, conversionData);
    case Pc:
        return value * applyTextZoomIf(conversionData.computingLineHeight(), CSS::pixelsPerPc, conversionData);

    // MARK: "font dependent" resolution

    case Em:
    case QuirkyEm:
        return value * applyTextZoom(resolveEm(conversionData.propertyToCompute(), conversionData.fontCascadeForFontUnits()), conversionData);
    case Ex:
        return value * applyTextZoom(resolveEx(conversionData.propertyToCompute(), conversionData.fontCascadeForFontUnits()), conversionData);
    case Cap:
        return value * applyTextZoom(resolveCap(conversionData.propertyToCompute(), conversionData.fontCascadeForFontUnits()), conversionData);

    case Ch:
        return value * applyTextZoom(resolveCh(conversionData.propertyToCompute(), conversionData.fontCascadeForFontUnits()), conversionData);
    case Ic:
        return value * applyTextZoom(resolveIc(conversionData.propertyToCompute(), conversionData.fontCascadeForFontUnits()), conversionData);

    case Lh:
        return value * applyTextZoomIf(conversionData.computingLineHeight(), resolveLh(styleForLineHeightUnits(conversionData), conversionData.fontCascadeForFontUnits()), conversionData);

    // MARK: "root font dependent" resolution

    case Rem:
        return value * applyTextZoom(resolveEm(conversionData.propertyToCompute(), conversionData.rootStyle() ? conversionData.rootStyle()->fontCascade() : conversionData.fontCascadeForFontUnits()), conversionData);
    case Rcap:
        return value * applyTextZoom(resolveCap(conversionData.propertyToCompute(), conversionData.rootStyle() ? conversionData.rootStyle()->fontCascade() : conversionData.fontCascadeForFontUnits()), conversionData);
    case Rch:
        return value * applyTextZoom(resolveCh(conversionData.propertyToCompute(), conversionData.rootStyle() ? conversionData.rootStyle()->fontCascade() : conversionData.fontCascadeForFontUnits()), conversionData);
    case Rex:
        return value * applyTextZoom(resolveEx(conversionData.propertyToCompute(), conversionData.rootStyle() ? conversionData.rootStyle()->fontCascade() : conversionData.fontCascadeForFontUnits()), conversionData);
    case Ric:
        return value * applyTextZoom(resolveIc(conversionData.propertyToCompute(), conversionData.rootStyle() ? conversionData.rootStyle()->fontCascade() : conversionData.fontCascadeForFontUnits()), conversionData);

    case Rlh:
        return value * applyTextZoomIf(conversionData.computingLineHeight(), resolveLh(styleForRootLineHeightUnits(conversionData), conversionData.rootStyle() ? conversionData.rootStyle()->fontCascade() : conversionData.fontCascadeForFontUnits()), conversionData);

    // MARK: "viewport-percentage" resolution

    case Vh:
        conversionData.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Default, ViewportPhysicalDimension::Height>(conversionData.renderView());
    case Vw:
        conversionData.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Default, ViewportPhysicalDimension::Width>(conversionData.renderView());
    case Vmax:
        conversionData.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Default, ViewportPhysicalDimension::Max>(conversionData.renderView());
    case Vmin:
        conversionData.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Default, ViewportPhysicalDimension::Min>(conversionData.renderView());
    case Vb:
        conversionData.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Default, LogicalBoxAxis::Block>(conversionData.renderView(), conversionData.style());
    case Vi:
        conversionData.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Default, LogicalBoxAxis::Inline>(conversionData.renderView(), conversionData.style());

    case Svh:
        conversionData.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Small, ViewportPhysicalDimension::Height>(conversionData.renderView());
    case Svw:
        conversionData.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Small, ViewportPhysicalDimension::Width>(conversionData.renderView());
    case Svmax:
        conversionData.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Small, ViewportPhysicalDimension::Max>(conversionData.renderView());
    case Svmin:
        conversionData.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Small, ViewportPhysicalDimension::Min>(conversionData.renderView());
    case Svb:
        conversionData.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Small, LogicalBoxAxis::Block>(conversionData.renderView(), conversionData.style());
    case Svi:
        conversionData.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Small, LogicalBoxAxis::Inline>(conversionData.renderView(), conversionData.style());

    case Lvh:
        conversionData.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Large, ViewportPhysicalDimension::Height>(conversionData.renderView());
    case Lvw:
        conversionData.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Large, ViewportPhysicalDimension::Width>(conversionData.renderView());
    case Lvmax:
        conversionData.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Large, ViewportPhysicalDimension::Max>(conversionData.renderView());
    case Lvmin:
        conversionData.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Large, ViewportPhysicalDimension::Min>(conversionData.renderView());
    case Lvb:
        conversionData.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Large, LogicalBoxAxis::Block>(conversionData.renderView(), conversionData.style());
    case Lvi:
        conversionData.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Large, LogicalBoxAxis::Inline>(conversionData.renderView(), conversionData.style());

    case Dvh:
        conversionData.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Dynamic, ViewportPhysicalDimension::Height>(conversionData.renderView());
    case Dvw:
        conversionData.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Dynamic, ViewportPhysicalDimension::Width>(conversionData.renderView());
    case Dvmax:
        conversionData.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Dynamic, ViewportPhysicalDimension::Max>(conversionData.renderView());
    case Dvmin:
        conversionData.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Dynamic, ViewportPhysicalDimension::Min>(conversionData.renderView());
    case Dvb:
        conversionData.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Dynamic, LogicalBoxAxis::Block>(conversionData.renderView(), conversionData.style());
    case Dvi:
        conversionData.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Dynamic, LogicalBoxAxis::Inline>(conversionData.renderView(), conversionData.style());

    // MARK: "container-percentage" resolution

    case Cqw:
        if (auto resolvedValue = resolveContainerUnit(conversionData, CQ::Axis::Width))
            return value * *resolvedValue;
        return resolveLength(value, Svw, conversionData);

    case Cqh:
        if (auto resolvedValue = resolveContainerUnit(conversionData, CQ::Axis::Height))
            return value * *resolvedValue;
        return resolveLength(value, Svh, conversionData);

    case Cqi:
        if (auto resolvedValue = resolveContainerUnit(conversionData, conversionData.style()->writingMode().isHorizontal() ? CQ::Axis::Width : CQ::Axis::Height))
            return value * *resolvedValue;
        return resolveLength(value, Svi, conversionData);

    case Cqb:
        if (auto resolvedValue = resolveContainerUnit(conversionData, conversionData.style()->writingMode().isHorizontal() ? CQ::Axis::Height : CQ::Axis::Width))
            return value * *resolvedValue;
        return resolveLength(value, Svb, conversionData);

    case Cqmax:
        if (value < 0)
            return std::min(resolveLength(value, Cqb, conversionData), resolveLength(value, Cqi, conversionData));
        return std::max(resolveLength(value, Cqb, conversionData), resolveLength(value, Cqi, conversionData));

    case Cqmin:
        if (value < 0)
            return std::max(resolveLength(value, Cqb, conversionData), resolveLength(value, Cqi, conversionData));
        return std::min(resolveLength(value, Cqb, conversionData), resolveLength(value, Cqi, conversionData));
    }

    RELEASE_ASSERT_NOT_REACHED();
}

bool equalForLengthResolution(const Style::ComputedStyle& styleA, const Style::ComputedStyle& styleB)
{
    // These properties affect results of `resolveLength` above.

    if (styleA.fontDescription().computedSize() != styleB.fontDescription().computedSize())
        return false;
    if (styleA.fontDescription().specifiedSize() != styleB.fontDescription().specifiedSize())
        return false;

    if (styleA.metricsOfPrimaryFont().xHeight() != styleB.metricsOfPrimaryFont().xHeight())
        return false;
    if (styleA.metricsOfPrimaryFont().zeroWidth() != styleB.metricsOfPrimaryFont().zeroWidth())
        return false;

    if (styleA.zoom() != styleB.zoom())
        return false;

    return true;
}

// MARK: - em-to-px utility functions

double emToPxDouble(double value, const CSSToLengthConversionData& conversionData)
{
    return applyTextZoom(value * conversionData.fontCascadeForFontUnits().fontDescription().unzoomedComputedSize(), conversionData);
}

double emToPxDouble(double value, const ComputedStyle& style)
{
    return emToPxDouble(value, CSSToLengthConversionData(style, nullptr, nullptr, nullptr, nullptr));
}

double emToPxDoubleZoomed(double value, const CSSToLengthConversionData& conversionData)
{
    // Text zoom is not applied here as it is already included in the FontDescription's computedSize().
    return value * conversionData.fontCascadeForFontUnits().fontDescription().computedSize();
}

double emToPxDoubleZoomed(double value, const ComputedStyle& style)
{
    return emToPxDoubleZoomed(value, CSSToLengthConversionData(style, nullptr, nullptr, nullptr, nullptr));
}

} // namespace Style
} // namespace WebCore
