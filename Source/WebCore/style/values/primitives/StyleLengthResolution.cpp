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
static double resolveEm(const FontCascade& fontCascadeForUnit)
{
    auto& fontDescription = fontCascadeForUnit.fontDescription();
    return fontDescription.specifiedSize();
}

// Resolve the "ex" and "rex" units.
// https://drafts.csswg.org/css-values-4/#ex
// https://drafts.csswg.org/css-values-4/#rex
static double resolveEx(const FontCascade& fontCascadeForUnit)
{
    auto& fontDescription = fontCascadeForUnit.fontDescription();
    auto& fontMetrics = fontCascadeForUnit.metricsOfPrimaryFont();
    if (fontMetrics.xHeight())
        return unzoomFontMetric(fontMetrics.xHeight().value(), fontDescription);

    return fontDescription.specifiedSize() / 2.0;
}

// Resolve the "cap" and "rcap" units.
// https://drafts.csswg.org/css-values-4/#cap
// https://drafts.csswg.org/css-values-4/#rcap
static double resolveCap(const FontCascade& fontCascadeForUnit)
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
static double resolveCh(const FontCascade& fontCascadeForUnit)
{
    return unzoomFontMetric(fontCascadeForUnit.zeroWidth(), fontCascadeForUnit.fontDescription());
}

// Resolve the "ic" and "ric" units.
// https://drafts.csswg.org/css-values-4/#ic
// https://drafts.csswg.org/css-values-4/#ric
static double resolveIc(const FontCascade& fontCascadeForUnit)
{
    auto& fontDescription = fontCascadeForUnit.fontDescription();
    auto ideogramWidth = fontCascadeForUnit.metricsOfPrimaryFont().ideogramWidth();
    if (!ideogramWidth)
        return fontDescription.unzoomedComputedSize();
    return unzoomFontMetric(ideogramWidth.value(), fontDescription);
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
static double NODELETE resolveViewportPercentageLogicalAxis(const FloatSize& size, const ComputedStyle* style)
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

static std::optional<double> resolveContainerUnit(CQ::Axis physicalAxis, const auto& adaptor)
{
    ASSERT(physicalAxis == CQ::Axis::Width || physicalAxis == CQ::Axis::Height);

    adaptor.setUsesContainerUnits();

    RefPtr element = adaptor.elementForContainerUnits();
    if (!element)
        return { };

    auto mode = !adaptor.styleForContainerUnits()->pseudoElementType()
        ? ContainerQueryEvaluator::SelectionMode::Element
        : ContainerQueryEvaluator::SelectionMode::PseudoElement;

    // "The query container for each axis is the nearest ancestor container that accepts container size queries on that axis."
    while ((element = ContainerQueryEvaluator::selectContainer(CQ::ContainerRequirements { physicalAxis }, nullString(), *element, mode))) {
        auto* containerRenderer = dynamicDowncast<RenderBox>(element->renderer());
        if (containerRenderer && containerRenderer->hasEligibleContainmentForSizeQuery()) {
            auto widthOrHeight = physicalAxis == CQ::Axis::Width ? containerRenderer->contentBoxWidth() : containerRenderer->contentBoxHeight();
            auto adjustedWidthOrHeight = widthOrHeight.toDouble();

            // FIXME: Document why `font-size` is excluded or remove this special case.
            if (adaptor.property() != CSSPropertyFontSize)
                adjustedWidthOrHeight = adjustedWidthOrHeight / adaptor.renderViewForViewportUnits()->pageZoomFactor();

            return adjustedWidthOrHeight / 100;
        }
        // For pseudo-elements the element itself can be the container. Avoid looping forever.
        mode = ContainerQueryEvaluator::SelectionMode::Element;
    }
    return { };
}

// MARK: - Length Resolution

struct CSSToLengthConversionDataAdaptor {
    const CSSToLengthConversionData& conversionData;

    void setUsesViewportUnits() const
    {
        if (CheckedPtr builderState = conversionData.styleBuilderState())
            builderState->setUsesViewportUnits();
    }

    void setUsesContainerUnits() const
    {
        if (CheckedPtr builderState = conversionData.styleBuilderState())
            builderState->setIsContainerDependent();
    }

    CSSPropertyID property() const
    {
        return conversionData.property();
    }

    bool computingFontProperty() const
    {
        // FIXME: This should be generated but first we need to determine if using `font-*` so literally is the right choice. It might make more sense to have this be all properties that effect font-relative units, so would need to include things like `math-depth`.
        // See https://github.com/w3c/csswg-drafts/issues/14306.
        switch (conversionData.property()) {
        case CSSPropertyFontFeatureSettings:
        case CSSPropertyFontKerning:
        case CSSPropertyFontPalette:
        case CSSPropertyFontSize:
        case CSSPropertyFontSizeAdjust:
        case CSSPropertyFontStyle:
        case CSSPropertyFontSynthesisSmallCaps:
        case CSSPropertyFontSynthesisStyle:
        case CSSPropertyFontSynthesisWeight:
        case CSSPropertyFontVariantAlternates:
        case CSSPropertyFontVariantCaps:
        case CSSPropertyFontVariantEastAsian:
        case CSSPropertyFontVariantEmoji:
        case CSSPropertyFontVariantLigatures:
        case CSSPropertyFontVariantNumeric:
        case CSSPropertyFontVariantPosition:
        case CSSPropertyFontWeight:
        case CSSPropertyFontWidth:
#if ENABLE(VARIATION_FONTS)
        case CSSPropertyFontOpticalSizing:
        case CSSPropertyFontVariationSettings:
#endif
            return true;
        default:
            return false;
        }
    }

    bool computingLineHeightProperty() const
    {
        return conversionData.property() == CSSPropertyLineHeight;
    }

    double applyTextZoom(double value) const
    {
        if (CheckedPtr builderState = conversionData.styleBuilderState())
            return value * builderState->zoomWithTextZoomFactor();
        return value;
    }

    const FontCascade& fontCascadeForFontUnits() const
    {
        if (computingFontProperty()) {
            ASSERT(conversionData.parentStyle());
            return conversionData.parentStyle()->fontCascade();
        }

        return conversionData.style().fontCascade();
    }

    const FontCascade& fontCascadeForRootFontUnits() const
    {
        if (conversionData.rootStyle())
            return conversionData.rootStyle()->fontCascade();

        // FIXME: This should fallback to the initial style, not the element style. To fix this, we will need to make the initial style (Document::initialStyle()) available to CSSToLengthConversionData.
        return fontCascadeForFontUnits();
    }

    const ComputedStyle* styleForLineHeightUnits() const
    {
        if (computingLineHeightProperty() || computingFontProperty())
            return conversionData.parentStyle();

        // FIXME: This should fallback to the initial style, not the element style. To fix this, we will need to make the initial style (Document::initialStyle()) available to CSSToLengthConversionData.
        return &conversionData.style();
    }

    const ComputedStyle* styleForRootLineHeightUnits() const
    {
        return conversionData.rootStyle();
    }

    const ComputedStyle* styleForViewportUnits() const
    {
        return &conversionData.style();
    }

    const RenderView* renderViewForViewportUnits() const
    {
        return conversionData.renderView();
    }

    const ComputedStyle* styleForContainerUnits() const
    {
        return &conversionData.style();
    }

    const Element* elementForContainerUnits() const
    {
        return conversionData.elementForContainerUnitResolution();
    }

    bool isHorizontalForContainerUnits() const
    {
        return conversionData.style().writingMode().isHorizontal();
    }
};

struct DirectDataAdaptor {
    CSSPropertyID propertyToCompute;
    const FontCascade& fontCascadeForUnit;
    const RenderView* renderViewForUnit;

    void setUsesViewportUnits() const
    {
    }

    void setUsesContainerUnits() const
    {
    }

    CSSPropertyID property() const
    {
        return propertyToCompute;
    }

    double applyTextZoom(double value) const
    {
        // FIXME: Add support for text zoom for direct data overload.
        return value;
    }

    const FontCascade& fontCascadeForFontUnits() const
    {
        return fontCascadeForUnit;
    }

    const FontCascade& fontCascadeForRootFontUnits() const
    {
        return fontCascadeForUnit;
    }

    const ComputedStyle* styleForLineHeightUnits() const
    {
        return nullptr;
    }

    const ComputedStyle* styleForRootLineHeightUnits() const
    {
        return nullptr;
    }

    const ComputedStyle* styleForViewportUnits() const
    {
        return nullptr;
    }

    const RenderView* renderViewForViewportUnits() const
    {
        return renderViewForUnit;
    }

    const ComputedStyle* styleForContainerUnits() const
    {
        return nullptr;
    }

    const Element* elementForContainerUnits() const
    {
        return nullptr;
    }

    bool isHorizontalForContainerUnits() const
    {
        return true;
    }
};

static double resolveLengthImpl(double value, CSS::LengthUnit lengthUnit, const auto& adaptor)
{
    using enum CSS::LengthUnit;

    auto applyTextZoomIfComputingLineHeight = [&](double value) {
        return adaptor.property() == CSSPropertyLineHeight ? adaptor.applyTextZoom(value) : value;
    };

    switch (lengthUnit) {
    case Px:
        return value * applyTextZoomIfComputingLineHeight(1.0);
    case Cm:
        return value * applyTextZoomIfComputingLineHeight(CSS::pixelsPerCm);
    case Mm:
        return value * applyTextZoomIfComputingLineHeight(CSS::pixelsPerMm);
    case Q:
        return value * applyTextZoomIfComputingLineHeight(CSS::pixelsPerQ);
    case In:
        return value * applyTextZoomIfComputingLineHeight(CSS::pixelsPerInch);
    case Pt:
        return value * applyTextZoomIfComputingLineHeight(CSS::pixelsPerPt);
    case Pc:
        return value * applyTextZoomIfComputingLineHeight(CSS::pixelsPerPc);

    // MARK: "font dependent" resolution

    case Em:
    case QuirkyEm:
        return value * adaptor.applyTextZoom(resolveEm(adaptor.fontCascadeForFontUnits()));
    case Ex:
        return value * adaptor.applyTextZoom(resolveEx(adaptor.fontCascadeForFontUnits()));
    case Cap:
        return value * adaptor.applyTextZoom(resolveCap(adaptor.fontCascadeForFontUnits()));

    case Ch:
        return value * adaptor.applyTextZoom(resolveCh(adaptor.fontCascadeForFontUnits()));
    case Ic:
        return value * adaptor.applyTextZoom(resolveIc(adaptor.fontCascadeForFontUnits()));

    case Lh:
        return value * applyTextZoomIfComputingLineHeight(resolveLh(adaptor.styleForLineHeightUnits(), adaptor.fontCascadeForFontUnits()));

    // MARK: "root font dependent" resolution

    case Rem:
        return value * adaptor.applyTextZoom(resolveEm(adaptor.fontCascadeForRootFontUnits()));
    case Rcap:
        return value * adaptor.applyTextZoom(resolveCap(adaptor.fontCascadeForRootFontUnits()));
    case Rch:
        return value * adaptor.applyTextZoom(resolveCh(adaptor.fontCascadeForRootFontUnits()));
    case Rex:
        return value * adaptor.applyTextZoom(resolveEx(adaptor.fontCascadeForRootFontUnits()));
    case Ric:
        return value * adaptor.applyTextZoom(resolveIc(adaptor.fontCascadeForRootFontUnits()));

    case Rlh:
        return value * applyTextZoomIfComputingLineHeight(resolveLh(adaptor.styleForRootLineHeightUnits(), adaptor.fontCascadeForRootFontUnits()));

    // MARK: "viewport-percentage" resolution

    case Vh:
        adaptor.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Default, ViewportPhysicalDimension::Height>(adaptor.renderViewForViewportUnits());
    case Vw:
        adaptor.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Default, ViewportPhysicalDimension::Width>(adaptor.renderViewForViewportUnits());
    case Vmax:
        adaptor.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Default, ViewportPhysicalDimension::Max>(adaptor.renderViewForViewportUnits());
    case Vmin:
        adaptor.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Default, ViewportPhysicalDimension::Min>(adaptor.renderViewForViewportUnits());
    case Vb:
        adaptor.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Default, LogicalBoxAxis::Block>(adaptor.renderViewForViewportUnits(), adaptor.styleForViewportUnits());
    case Vi:
        adaptor.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Default, LogicalBoxAxis::Inline>(adaptor.renderViewForViewportUnits(), adaptor.styleForViewportUnits());

    case Svh:
        adaptor.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Small, ViewportPhysicalDimension::Height>(adaptor.renderViewForViewportUnits());
    case Svw:
        adaptor.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Small, ViewportPhysicalDimension::Width>(adaptor.renderViewForViewportUnits());
    case Svmax:
        adaptor.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Small, ViewportPhysicalDimension::Max>(adaptor.renderViewForViewportUnits());
    case Svmin:
        adaptor.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Small, ViewportPhysicalDimension::Min>(adaptor.renderViewForViewportUnits());
    case Svb:
        adaptor.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Small, LogicalBoxAxis::Block>(adaptor.renderViewForViewportUnits(), adaptor.styleForViewportUnits());
    case Svi:
        adaptor.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Small, LogicalBoxAxis::Inline>(adaptor.renderViewForViewportUnits(), adaptor.styleForViewportUnits());

    case Lvh:
        adaptor.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Large, ViewportPhysicalDimension::Height>(adaptor.renderViewForViewportUnits());
    case Lvw:
        adaptor.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Large, ViewportPhysicalDimension::Width>(adaptor.renderViewForViewportUnits());
    case Lvmax:
        adaptor.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Large, ViewportPhysicalDimension::Max>(adaptor.renderViewForViewportUnits());
    case Lvmin:
        adaptor.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Large, ViewportPhysicalDimension::Min>(adaptor.renderViewForViewportUnits());
    case Lvb:
        adaptor.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Large, LogicalBoxAxis::Block>(adaptor.renderViewForViewportUnits(), adaptor.styleForViewportUnits());
    case Lvi:
        adaptor.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Large, LogicalBoxAxis::Inline>(adaptor.renderViewForViewportUnits(), adaptor.styleForViewportUnits());

    case Dvh:
        adaptor.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Dynamic, ViewportPhysicalDimension::Height>(adaptor.renderViewForViewportUnits());
    case Dvw:
        adaptor.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Dynamic, ViewportPhysicalDimension::Width>(adaptor.renderViewForViewportUnits());
    case Dvmax:
        adaptor.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Dynamic, ViewportPhysicalDimension::Max>(adaptor.renderViewForViewportUnits());
    case Dvmin:
        adaptor.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Dynamic, ViewportPhysicalDimension::Min>(adaptor.renderViewForViewportUnits());
    case Dvb:
        adaptor.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Dynamic, LogicalBoxAxis::Block>(adaptor.renderViewForViewportUnits(), adaptor.styleForViewportUnits());
    case Dvi:
        adaptor.setUsesViewportUnits();
        return value * resolveViewportPercentageUnit<ViewportType::Dynamic, LogicalBoxAxis::Inline>(adaptor.renderViewForViewportUnits(), adaptor.styleForViewportUnits());

    // MARK: "container-percentage" resolution

    case Cqw:
        if (auto resolvedValue = resolveContainerUnit(CQ::Axis::Width, adaptor))
            return value * *resolvedValue;
        return resolveLengthImpl(value, Svw, adaptor);

    case Cqh:
        if (auto resolvedValue = resolveContainerUnit(CQ::Axis::Height, adaptor))
            return value * *resolvedValue;
        return resolveLengthImpl(value, Svh, adaptor);

    case Cqi:
        if (auto resolvedValue = resolveContainerUnit(adaptor.isHorizontalForContainerUnits() ? CQ::Axis::Width : CQ::Axis::Height, adaptor))
            return value * *resolvedValue;
        return resolveLengthImpl(value, Svi, adaptor);

    case Cqb:
        if (auto resolvedValue = resolveContainerUnit(adaptor.isHorizontalForContainerUnits() ? CQ::Axis::Height : CQ::Axis::Width, adaptor))
            return value * *resolvedValue;
        return resolveLengthImpl(value, Svb, adaptor);

    case Cqmax:
        if (value < 0)
            return std::min(resolveLengthImpl(value, Cqb, adaptor), resolveLengthImpl(value, Cqi, adaptor));
        return std::max(resolveLengthImpl(value, Cqb, adaptor), resolveLengthImpl(value, Cqi, adaptor));

    case Cqmin:
        if (value < 0)
            return std::max(resolveLengthImpl(value, Cqb, adaptor), resolveLengthImpl(value, Cqi, adaptor));
        return std::min(resolveLengthImpl(value, Cqb, adaptor), resolveLengthImpl(value, Cqi, adaptor));
    }

    RELEASE_ASSERT_NOT_REACHED();
}

double resolveLength(double value, CSS::LengthUnit lengthUnit, const BuilderState& state)
{
    return resolveLengthImpl(value, lengthUnit, CSSToLengthConversionDataAdaptor {
        .conversionData = state.cssToLengthConversionData(),
    });
}

double resolveLength(double value, CSS::LengthUnit lengthUnit, const CSSToLengthConversionData& conversionData)
{
    return resolveLengthImpl(value, lengthUnit, CSSToLengthConversionDataAdaptor {
        .conversionData = conversionData,
    });
}

double resolveLength(double value, CSS::LengthUnit lengthUnit, CSSPropertyID propertyToCompute, const FontCascade& fontCascadeForUnit, const RenderView* renderViewForUnit)
{
    return resolveLengthImpl(value, lengthUnit, DirectDataAdaptor {
        .propertyToCompute = propertyToCompute,
        .fontCascadeForUnit = fontCascadeForUnit,
        .renderViewForUnit = renderViewForUnit,
    });
}

double resolveLength(double value, CSS::LengthUnit lengthUnit, NoConversionDataRequiredToken)
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

    case Em:
    case QuirkyEm:
    case Ex:
    case Cap:
    case Ch:
    case Ic:
    case Lh:
    case Rem:
    case Rcap:
    case Rch:
    case Rex:
    case Ric:
    case Rlh:
    case Vh:
    case Vw:
    case Vmax:
    case Vmin:
    case Vb:
    case Vi:
    case Svh:
    case Svw:
    case Svmax:
    case Svmin:
    case Svb:
    case Svi:
    case Lvh:
    case Lvw:
    case Lvmax:
    case Lvmin:
    case Lvb:
    case Lvi:
    case Dvh:
    case Dvw:
    case Dvmax:
    case Dvmin:
    case Dvb:
    case Dvi:
    case Cqw:
    case Cqh:
    case Cqi:
    case Cqb:
    case Cqmax:
    case Cqmin:
        ASSERT_NOT_REACHED();
        return -1;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

bool equalForLengthResolution(const ComputedStyle& styleA, const ComputedStyle& styleB)
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

double emToPxDouble(double value, const ComputedStyle& style)
{
    return value * style.fontCascade().fontDescription().specifiedSize();
}

double emToPxDoubleZoomed(double value, const ComputedStyle& style)
{
    return value * style.fontCascade().fontDescription().computedSize();
}

} // namespace Style
} // namespace WebCore
