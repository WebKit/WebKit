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

#include "config.h"
#include "StyleLengthResolution.h"

#include "BoxSides.h"
#include "CSSPrimitiveNumericUnits.h"
#include "CSSToLengthConversionData.h"
#include "ContainerNodeInlines.h"
#include "ContainerQueryEvaluator.h"
#include "Document.h"
#include "Element.h"
#include "FontCascade.h"
#include "FontCascadeDescription.h"
#include "FontMetrics.h"
#include "NodeRenderStyle.h"
#include "RenderBox.h"
#include "RenderBoxInlines.h"
#include "RenderStyle.h"
#include "RenderStyle+GettersInlines.h"
#include "RenderView.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"

namespace WebCore {
namespace Style {

static double adjustValueForPageZoom(double dimension, const CSSToLengthConversionData& conversionData)
{
    if (conversionData.rangeZoomOption() != CSS::RangeZoomOptions::Unzoomed)
        return dimension;

    auto* style = conversionData.style();
    if (!style || !evaluationTimeZoomEnabled(*style))
        return dimension;

    return dimension / conversionData.renderView()->zoomFactor();
}

static double lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis logicalAxis, const FloatSize& size, const RenderStyle* style)
{
    if (!style)
        return 0;

    switch (mapAxisLogicalToPhysical(style->writingMode(), logicalAxis)) {
    case BoxAxis::Horizontal:
        return size.width();
    case BoxAxis::Vertical:
        return size.height();
    }

    RELEASE_ASSERT_NOT_REACHED();
}

static double lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis logicalAxis, const FloatSize& size, const RenderView& renderView)
{
    RefPtr rootElement = renderView.document().documentElement();
    if (!rootElement)
        return 0;

    return lengthOfViewportPhysicalAxisForLogicalAxis(logicalAxis, size, rootElement->renderStyle());
}

double computeUnzoomedNonCalcLengthDouble(double value, CSS::LengthUnit lengthUnit, CSSPropertyID propertyToCompute, const FontCascade* fontCascadeForUnit, CSS::RangeZoomOptions rangeZoomOption, const RenderView* renderView)
{
    using enum CSS::LengthUnit;

    switch (lengthUnit) {
    case Px:
        return value;
    case Cm:
        return CSS::pixelsPerCm * value;
    case Mm:
        return CSS::pixelsPerMm * value;
    case Q:
        return CSS::pixelsPerQ * value;
    case In:
        return CSS::pixelsPerInch * value;
    case Pt:
        return CSS::pixelsPerPt * value;
    case Pc:
        return CSS::pixelsPerPc * value;

    // MARK: "font dependent" and "root font dependent" resolution

    case Em:
    case QuirkyEm:
    case Rem: {
        ASSERT(fontCascadeForUnit);
        auto& fontDescription = fontCascadeForUnit->fontDescription();
        return ((propertyToCompute == CSSPropertyFontSize) ? fontDescription.specifiedSize() :  fontDescription.computedSizeForRangeZoomOption(rangeZoomOption)) * value;
    }
    case Ex:
    case Rex: {
        ASSERT(fontCascadeForUnit);
        auto& fontMetrics = fontCascadeForUnit->metricsOfPrimaryFont();
        if (fontMetrics.xHeight())
            return fontMetrics.xHeight().value() * value;
        auto& fontDescription = fontCascadeForUnit->fontDescription();
        return ((propertyToCompute == CSSPropertyFontSize) ? fontDescription.specifiedSize() : fontDescription.computedSizeForRangeZoomOption(rangeZoomOption)) / 2.0 * value;
    }
    case Cap:
    case Rcap: {
        ASSERT(fontCascadeForUnit);
        auto& fontMetrics = fontCascadeForUnit->metricsOfPrimaryFont();
        if (fontMetrics.capHeight())
            return fontMetrics.capHeight().value() * value;
        return fontMetrics.intAscent() * value;
    }
    case Ch:
    case Rch:
        ASSERT(fontCascadeForUnit);
        return fontCascadeForUnit->zeroWidth() * value;
    case Ic:
    case Ric:
        ASSERT(fontCascadeForUnit);
        return fontCascadeForUnit->metricsOfPrimaryFont().ideogramWidth().value_or(0) * value;

    // MARK: "viewport percentage" resolution

    case Vh:
        return renderView ? renderView->sizeForCSSDefaultViewportUnits().height() / 100.0 * value : 0;
    case Vw:
        return renderView ? renderView->sizeForCSSDefaultViewportUnits().width() / 100.0 * value : 0;
    case Vmax:
        return renderView ? renderView->sizeForCSSDefaultViewportUnits().maxDimension() / 100.0 * value : value;
    case Vmin:
        return renderView ? renderView->sizeForCSSDefaultViewportUnits().minDimension() / 100.0 * value : value;
    case Vb:
        return renderView ? lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Block, renderView->sizeForCSSDefaultViewportUnits(), *renderView) / 100.0 * value : 0;
    case Vi:
        return renderView ? lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Inline, renderView->sizeForCSSDefaultViewportUnits(), *renderView) / 100.0 * value : 0;
    case Svh:
        return renderView ? renderView->sizeForCSSSmallViewportUnits().height() / 100.0 * value : 0;
    case Svw:
        return renderView ? renderView->sizeForCSSSmallViewportUnits().width() / 100.0 * value : 0;
    case Svmax:
        return renderView ? renderView->sizeForCSSSmallViewportUnits().maxDimension() / 100.0 * value : value;
    case Svmin:
        return renderView ? renderView->sizeForCSSSmallViewportUnits().minDimension() / 100.0 * value : value;
    case Svb:
        return renderView ? lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Block, renderView->sizeForCSSSmallViewportUnits(), *renderView) / 100.0 * value : 0;
    case Svi:
        return renderView ? lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Inline, renderView->sizeForCSSSmallViewportUnits(), *renderView) / 100.0 * value : 0;
    case Lvh:
        return renderView ? renderView->sizeForCSSLargeViewportUnits().height() / 100.0 * value : 0;
    case Lvw:
        return renderView ? renderView->sizeForCSSLargeViewportUnits().width() / 100.0 * value : 0;
    case Lvmax:
        return renderView ? renderView->sizeForCSSLargeViewportUnits().maxDimension() / 100.0 * value : value;
    case Lvmin:
        return renderView ? renderView->sizeForCSSLargeViewportUnits().minDimension() / 100.0 * value : value;
    case Lvb:
        return renderView ? lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Block, renderView->sizeForCSSLargeViewportUnits(), *renderView) / 100.0 * value : 0;
    case Lvi:
        return renderView ? lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Inline, renderView->sizeForCSSLargeViewportUnits(), *renderView) / 100.0 * value : 0;
    case Dvh:
        return renderView ? renderView->sizeForCSSDynamicViewportUnits().height() / 100.0 * value : 0;
    case Dvw:
        return renderView ? renderView->sizeForCSSDynamicViewportUnits().width() / 100.0 * value : 0;
    case Dvmax:
        return renderView ? renderView->sizeForCSSDynamicViewportUnits().maxDimension() / 100.0 * value : value;
    case Dvmin:
        return renderView ? renderView->sizeForCSSDynamicViewportUnits().minDimension() / 100.0 * value : value;
    case Dvb:
        return renderView ? lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Block, renderView->sizeForCSSDynamicViewportUnits(), *renderView) / 100.0 * value : 0;
    case Dvi:
        return renderView ? lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Inline, renderView->sizeForCSSDynamicViewportUnits(), *renderView) / 100.0 * value : 0;

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

static double adjustZoomStateForFontRelativeUnitsIfNeeded(double value, const CSSToLengthConversionData& conversionData)
{
    // Apply text zoom for font-relative units when evaluationTimeZoomEnabled with unzoomed range option.
    // computedSizeForRangeZoomOption returns unzoomed font size when rangeZoomOption is Unzoomed,
    // so we need to multiply by text zoom to get the correct final value.
    // We explicitly use zoomWithTextZoomFactor() here instead of conversionData.zoom() because
    // zoomWithTextZoomFactor() is guaranteed to return only the text zoom factor (not page zoom or other zoom types)
    // when evaluationTimeZoomEnabled is true. This ensures font-relative units scale correctly with text zoom
    // while remaining independent of other zoom mechanisms.
    if (conversionData.evaluationTimeZoomEnabled() && conversionData.rangeZoomOption() == CSS::RangeZoomOptions::Unzoomed) {
        if (CheckedPtr builderState = conversionData.styleBuilderState()) {
            auto textZoom = builderState->zoomWithTextZoomFactor();
            return value * textZoom;
        }
    }
    return value;
}

double computeCanonicalNonCalcLengthDouble(double value, CSS::LengthUnit lengthUnit, const CSSToLengthConversionData& conversionData)
{
    // We are only interested in canonicalizing to `px`, not adjusting for zoom, which will be handled later. When computing font-size, zoom is not applied in the same way, so must be special cased here.
    auto computedValue = computeNonCalcLengthDouble(value, lengthUnit, conversionData);
    if (conversionData.computingFontSize() || (conversionData.evaluationTimeZoomEnabled() && conversionData.rangeZoomOption() == CSS::RangeZoomOptions::Unzoomed))
        return computedValue;

    return computedValue / conversionData.style()->usedZoom();
}

double computeNonCalcLengthDouble(double value, CSS::LengthUnit lengthUnit, const CSSToLengthConversionData& conversionData)
{
    using enum CSS::LengthUnit;

    auto resolveContainerUnit = [&](CQ::Axis physicalAxis) -> std::optional<double> {
        ASSERT(physicalAxis == CQ::Axis::Width || physicalAxis == CQ::Axis::Height);

        conversionData.setUsesContainerUnits();

        RefPtr element = conversionData.elementForContainerUnitResolution();
        if (!element)
            return { };

        auto mode = !conversionData.style()->pseudoElementType()
            ? Style::ContainerQueryEvaluator::SelectionMode::Element
            : Style::ContainerQueryEvaluator::SelectionMode::PseudoElement;

        // "The query container for each axis is the nearest ancestor container that accepts container size queries on that axis."
        while ((element = Style::ContainerQueryEvaluator::selectContainer(physicalAxis, nullString(), *element, mode))) {
            auto* containerRenderer = dynamicDowncast<RenderBox>(element->renderer());
            if (containerRenderer && containerRenderer->hasEligibleContainmentForSizeQuery()) {
                auto widthOrHeight = physicalAxis == CQ::Axis::Width ? containerRenderer->contentBoxWidth() : containerRenderer->contentBoxHeight();
                auto adjustedWidthOrHeight = widthOrHeight.toDouble();

                if (!conversionData.computingFontSize())
                    adjustedWidthOrHeight = adjustValueForPageZoom(adjustedWidthOrHeight, conversionData);

                return adjustedWidthOrHeight * value / 100;
            }
            // For pseudo-elements the element itself can be the container. Avoid looping forever.
            mode = Style::ContainerQueryEvaluator::SelectionMode::Element;
        }
        return { };
    };

    switch (lengthUnit) {
    case Px:
        break;
    case Cm:
        value = CSS::pixelsPerCm * value;
        break;
    case Mm:
        value = CSS::pixelsPerMm * value;
        break;
    case Q:
        value = CSS::pixelsPerQ * value;
        break;
    case In:
        value = CSS::pixelsPerInch * value;
        break;
    case Pt:
        value = CSS::pixelsPerPt * value;
        break;
    case Pc:
        value = CSS::pixelsPerPc * value;
        break;

    // MARK: "font dependent" resolution

    case Em:
    case QuirkyEm:
    case Ex:
    case Cap:
    case Ch:
    case Ic:
        // FIXME: We have a bug right now where the zoom will be applied twice to EX units.
        // We really need to compute EX using fontMetrics for the original specifiedSize and not use
        // our actual constructed rendering font.
        value = computeUnzoomedNonCalcLengthDouble(value, lengthUnit, conversionData.propertyToCompute(), &conversionData.fontCascadeForFontUnits(), conversionData.rangeZoomOption());
        value = adjustZoomStateForFontRelativeUnitsIfNeeded(value, conversionData);
        break;

    case Lh:
        if (conversionData.computingLineHeight() || conversionData.computingFontSize()) {
            // Try to get the parent's computed line-height, or fall back to the initial line-height of this element's font spacing.
            value *= conversionData.parentStyle() ? conversionData.parentStyle()->computedLineHeight() : conversionData.fontCascadeForFontUnits().metricsOfPrimaryFont().intLineSpacing();
        } else if (auto fixedLineHeight = conversionData.style()->lineHeight().tryFixed()) {
            // We can't use computedLineHeightForFontUnits if the line height is fixed since
            // that will apply the usedZoomFactor. We probably should refactor it so that
            // does not happen and we don't have to special case this scenario.
            value *= Style::evaluate<LayoutUnit>(*fixedLineHeight, Style::ZoomFactor { conversionData.zoom() }).toFloat();
        } else
            value *= conversionData.computedLineHeightForFontUnits();
        break;

    // MARK: "root font dependent" resolution

    case Rcap:
    case Rch:
    case Rem:
    case Rex:
    case Ric:
        value = computeUnzoomedNonCalcLengthDouble(value, lengthUnit, conversionData.propertyToCompute(), conversionData.rootStyle() ? &conversionData.rootStyle()->fontCascade() : &conversionData.fontCascadeForFontUnits(), conversionData.rangeZoomOption());
        value = adjustZoomStateForFontRelativeUnitsIfNeeded(value, conversionData);
        break;

    case Rlh:
        if (conversionData.rootStyle()) {
            if (conversionData.computingLineHeight() || conversionData.computingFontSize())
                value *= conversionData.rootStyle()->computeLineHeight(conversionData.rootStyle()->specifiedLineHeight());
            else
                value *= conversionData.rootStyle()->computedLineHeight();
        }
        break;

    // MARK: "viewport-percentage" resolution

    case Vh:
        return value * adjustValueForPageZoom(conversionData.defaultViewportFactor().height(), conversionData);

    case Vw:
        return value * adjustValueForPageZoom(conversionData.defaultViewportFactor().width(), conversionData);

    case Vmax:
        return value * adjustValueForPageZoom(conversionData.defaultViewportFactor().maxDimension(), conversionData);

    case Vmin:
        return value * adjustValueForPageZoom(conversionData.defaultViewportFactor().minDimension(), conversionData);

    case Vb:
        return value * adjustValueForPageZoom(lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Block, conversionData.defaultViewportFactor(), conversionData.style()), conversionData);

    case Vi:
        return value * adjustValueForPageZoom(lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Inline, conversionData.defaultViewportFactor(), conversionData.style()), conversionData);

    case Svh:
        return value * adjustValueForPageZoom(conversionData.smallViewportFactor().height(), conversionData);

    case Svw:
        return value * adjustValueForPageZoom(conversionData.smallViewportFactor().width(), conversionData);

    case Svmax:
        return value * adjustValueForPageZoom(conversionData.smallViewportFactor().maxDimension(), conversionData);

    case Svmin:
        return value * adjustValueForPageZoom(conversionData.smallViewportFactor().minDimension(), conversionData);

    case Svb:
        return value * adjustValueForPageZoom(lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Block, conversionData.smallViewportFactor(), conversionData.style()), conversionData);

    case Svi:
        return value * adjustValueForPageZoom(lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Inline, conversionData.smallViewportFactor(), conversionData.style()), conversionData);

    case Lvh:
        return value * adjustValueForPageZoom(conversionData.largeViewportFactor().height(), conversionData);

    case Lvw:
        return value * adjustValueForPageZoom(conversionData.largeViewportFactor().width(), conversionData);

    case Lvmax:
        return value * adjustValueForPageZoom(conversionData.largeViewportFactor().maxDimension(), conversionData);

    case Lvmin:
        return value * adjustValueForPageZoom(conversionData.largeViewportFactor().minDimension(), conversionData);

    case Lvb:
        return value * adjustValueForPageZoom(lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Block, conversionData.largeViewportFactor(), conversionData.style()), conversionData);

    case Lvi:
        return value * adjustValueForPageZoom(lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Inline, conversionData.largeViewportFactor(), conversionData.style()), conversionData);

    case Dvh:
        return value * adjustValueForPageZoom(conversionData.dynamicViewportFactor().height(), conversionData);

    case Dvw:
        return value * adjustValueForPageZoom(conversionData.dynamicViewportFactor().width(), conversionData);

    case Dvmax:
        return value * adjustValueForPageZoom(conversionData.dynamicViewportFactor().maxDimension(), conversionData);

    case Dvmin:
        return value * adjustValueForPageZoom(conversionData.dynamicViewportFactor().minDimension(), conversionData);

    case Dvb:
        return value * adjustValueForPageZoom(lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Block, conversionData.dynamicViewportFactor(), conversionData.style()), conversionData);

    case Dvi:
        return value * adjustValueForPageZoom(lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Inline, conversionData.dynamicViewportFactor(), conversionData.style()), conversionData);

    // MARK: "container-percentage" resolution

    case Cqw: {
        if (auto resolvedValue = resolveContainerUnit(CQ::Axis::Width))
            return *resolvedValue;
        return computeNonCalcLengthDouble(value, Svw, conversionData);
    }

    case Cqh: {
        if (auto resolvedValue = resolveContainerUnit(CQ::Axis::Height))
            return *resolvedValue;
        return computeNonCalcLengthDouble(value, Svh, conversionData);
    }

    case Cqi: {
        if (auto resolvedValue = resolveContainerUnit(conversionData.style()->writingMode().isHorizontal() ? CQ::Axis::Width : CQ::Axis::Height))
            return *resolvedValue;
        return computeNonCalcLengthDouble(value, Svi, conversionData);
    }

    case Cqb: {
        if (auto resolvedValue = resolveContainerUnit(conversionData.style()->writingMode().isHorizontal() ? CQ::Axis::Height : CQ::Axis::Width))
            return *resolvedValue;
        return computeNonCalcLengthDouble(value, Svb, conversionData);
    }

    case Cqmax:
        if (value < 0)
            return std::min(computeNonCalcLengthDouble(value, Cqb, conversionData), computeNonCalcLengthDouble(value, Cqi, conversionData));
        return std::max(computeNonCalcLengthDouble(value, Cqb, conversionData), computeNonCalcLengthDouble(value, Cqi, conversionData));

    case Cqmin:
        if (value < 0)
            return std::max(computeNonCalcLengthDouble(value, Cqb, conversionData), computeNonCalcLengthDouble(value, Cqi, conversionData));
        return std::min(computeNonCalcLengthDouble(value, Cqb, conversionData), computeNonCalcLengthDouble(value, Cqi, conversionData));
    }

    // We do not apply the zoom factor when we are computing the value of the font-size property. The zooming
    // for font sizes is much more complicated, since we have to worry about enforcing the minimum font size preference
    // as well as enforcing the implicit "smart minimum."
    if (conversionData.computingFontSize() || isFontOrRootFontRelativeLength(lengthUnit))
        return value;

    return value * conversionData.zoom();
}

bool equalForLengthResolution(const RenderStyle& styleA, const RenderStyle& styleB)
{
    // These properties affect results of `computeNonCalcLengthDouble` above.

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

double emToPxDouble(double value, const CSSToLengthConversionData& conversionData)
{
    return computeNonCalcLengthDouble(value, CSS::LengthUnit::Em, conversionData);
}

double emToPxDouble(double value, const RenderStyle& style)
{
    return computeNonCalcLengthDouble(value, CSS::LengthUnit::Em, CSSToLengthConversionData(style, nullptr, nullptr, nullptr));
}

} // namespace Style
} // namespace WebCore
