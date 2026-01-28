/*
 * Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007-2025 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "SVGLengthContext.h"

#include "ContainerNodeInlines.h"
#include "CSSToLengthConversionData.h"
#include "CSSUnits.h"
#include "FontCascade.h"
#include "FontMetrics.h"
#include "LegacyRenderSVGRoot.h"
#include "LocalFrame.h"
#include "RenderStyle+GettersInlines.h"
#include "RenderView.h"
#include "SVGElement.h"
#include "SVGElementTypeHelpers.h"
#include "SVGSVGElement.h"
#include "StyleLengthResolution.h"
#include "StylePreferredSize.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "StyleSVGCenterCoordinateComponent.h"
#include "StyleSVGCoordinateComponent.h"
#include "StyleSVGRadius.h"
#include "StyleSVGRadiusComponent.h"
#include "StyleSVGStrokeDasharray.h"
#include "StyleSVGStrokeDashoffset.h"
#include "StyleStrokeWidth.h"
#include <numbers>
#include <wtf/MathExtras.h>

namespace WebCore {

SVGLengthContext::SVGLengthContext(const SVGElement* context, const std::optional<FloatSize>& viewportSize)
    : m_context(context)
    , m_viewportSize(!m_context ? viewportSize : std::nullopt)
{
}

SVGLengthContext::~SVGLengthContext() = default;

FloatRect SVGLengthContext::resolveRectangle(const SVGElement* context, SVGUnitTypes::SVGUnitType type, const FloatRect& viewport, const SVGLengthValue& x, const SVGLengthValue& y, const SVGLengthValue& width, const SVGLengthValue& height)
{
    ASSERT(type != SVGUnitTypes::SVG_UNIT_TYPE_UNKNOWN);
    if (type != SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE) {
        auto viewportSize = viewport.size();
        return FloatRect(
            convertValueFromPercentageToUserUnits(x.valueAsPercentage(), x.lengthMode(), viewportSize) + viewport.x(),
            convertValueFromPercentageToUserUnits(y.valueAsPercentage(), y.lengthMode(), viewportSize) + viewport.y(),
            convertValueFromPercentageToUserUnits(width.valueAsPercentage(), width.lengthMode(), viewportSize),
            convertValueFromPercentageToUserUnits(height.valueAsPercentage(), height.lengthMode(), viewportSize));
    }

    SVGLengthContext lengthContext(context, viewport.size());
    return FloatRect(x.value(lengthContext), y.value(lengthContext), width.value(lengthContext), height.value(lengthContext));
}

FloatPoint SVGLengthContext::resolvePoint(const SVGElement* context, SVGUnitTypes::SVGUnitType type, const SVGLengthValue& x, const SVGLengthValue& y)
{
    ASSERT(type != SVGUnitTypes::SVG_UNIT_TYPE_UNKNOWN);
    if (type == SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE) {
        SVGLengthContext lengthContext(context);
        return FloatPoint(x.value(lengthContext), y.value(lengthContext));
    }

    // FIXME: valueAsPercentage() won't be correct for eg. cm units. They need to be resolved in user space and then be considered in objectBoundingBox space.
    return FloatPoint(x.valueAsPercentage(), y.valueAsPercentage());
}

float SVGLengthContext::resolveLength(const SVGElement* context, SVGUnitTypes::SVGUnitType type, const SVGLengthValue& x)
{
    ASSERT(type != SVGUnitTypes::SVG_UNIT_TYPE_UNKNOWN);
    if (type == SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE) {
        SVGLengthContext lengthContext(context);
        return x.value(lengthContext);
    }

    // FIXME: valueAsPercentage() won't be correct for eg. cm units. They need to be resolved in user space and then be considered in objectBoundingBox space.
    return x.valueAsPercentage();
}

static inline float dimensionForLengthMode(SVGLengthMode mode, FloatSize viewportSize)
{
    switch (mode) {
    case SVGLengthMode::Width:
        return viewportSize.width();
    case SVGLengthMode::Height:
        return viewportSize.height();
    case SVGLengthMode::Other:
        return viewportSize.diagonalLength() / std::numbers::sqrt2_v<float>;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

template<typename SizeType> float SVGLengthContext::valueForSizeType(const SizeType& size, Style::ZoomFactor usedZoom, SVGLengthMode lengthMode)
    requires (SizeType::Fixed::zoomOptions == CSS::RangeZoomOptions::Unzoomed || SizeType::Calc::range.zoomOptions == CSS::RangeZoomOptions::Unzoomed)
{
    return WTF::switchOn(size,
        [&](const typename SizeType::Fixed& fixed) -> float {
            return Style::evaluate<float>(fixed, usedZoom);
        },
        [&](const typename SizeType::Percentage& percentage) -> float {
            auto result = convertValueFromPercentageToUserUnits(percentage.value / 100, lengthMode);
            if (result.hasException())
                return 0;
            return result.releaseReturnValue();
        },
        [&](const typename SizeType::Calc& calc) -> float {
            auto viewportSize = this->viewportSize().value_or(FloatSize { });
            return Style::evaluate<float>(calc, dimensionForLengthMode(lengthMode, viewportSize), usedZoom);
        },
        [&](const auto&) -> float {
            return 0;
        }
    );

}

template<typename SizeType> float SVGLengthContext::valueForSizeType(const SizeType& size, Style::ZoomNeeded zoomNeeded, SVGLengthMode lengthMode)
{
    return WTF::switchOn(size,
        [&](const typename SizeType::Fixed& fixed) -> float {
            return Style::evaluate<float>(fixed, zoomNeeded);
        },
        [&](const typename SizeType::Percentage& percentage) -> float {
            auto result = convertValueFromPercentageToUserUnits(percentage.value / 100, lengthMode);
            if (result.hasException())
                return 0;
            return result.releaseReturnValue();
        },
        [&](const typename SizeType::Calc& calc) -> float {
            auto viewportSize = this->viewportSize().value_or(FloatSize { });
            return Style::evaluate<float>(calc, dimensionForLengthMode(lengthMode, viewportSize), zoomNeeded);
        },
        [&](const auto&) -> float {
            return 0;
        }
    );

}

float SVGLengthContext::valueForLength(const Style::PreferredSize& size, Style::ZoomFactor usedZoom, SVGLengthMode lengthMode)
{
    return valueForSizeType(size, usedZoom, lengthMode);
}

float SVGLengthContext::valueForLength(const Style::SVGCenterCoordinateComponent& size, Style::ZoomNeeded zoomNeeded, SVGLengthMode lengthMode)
{
    return valueForSizeType(size, zoomNeeded, lengthMode);
}

float SVGLengthContext::valueForLength(const Style::SVGCoordinateComponent& size, Style::ZoomNeeded zoomNeeded, SVGLengthMode lengthMode)
{
    return valueForSizeType(size, zoomNeeded, lengthMode);
}

float SVGLengthContext::valueForLength(const Style::SVGRadius& size, Style::ZoomNeeded zoomNeeded, SVGLengthMode lengthMode)
{
    return valueForSizeType(size, zoomNeeded, lengthMode);
}

float SVGLengthContext::valueForLength(const Style::SVGRadiusComponent& size, Style::ZoomNeeded zoomNeeded, SVGLengthMode lengthMode)
{
    return valueForSizeType(size, zoomNeeded, lengthMode);
}

float SVGLengthContext::valueForLength(const Style::SVGStrokeDasharrayValue& size, Style::ZoomNeeded zoomNeeded, SVGLengthMode lengthMode)
{
    return valueForSizeType(size, zoomNeeded, lengthMode);
}

float SVGLengthContext::valueForLength(const Style::SVGStrokeDashoffset& size, Style::ZoomNeeded zoomNeeded, SVGLengthMode lengthMode)
{
    return valueForSizeType(size, zoomNeeded, lengthMode);
}

float SVGLengthContext::valueForLength(const Style::StrokeWidth& size, Style::ZoomFactor usedZoom, SVGLengthMode lengthMode)
{
    return valueForSizeType(size, usedZoom, lengthMode);
}

float SVGLengthContext::computeNonCalcLength(float inputValue, CSS::LengthUnit unit) const
{
    if (!conversionToCanonicalUnitRequiresConversionData(unit))
        return clampTo<float>(Style::computeNonCalcLengthDouble(inputValue, unit, { }));


    auto conversionData = cssConversionData();
    if (!conversionData)
        return 0.0f;

    auto resolvedValue = clampTo<float>(Style::computeNonCalcLengthDouble(inputValue, unit, *conversionData));

    // "Font dependent" or "Root font dependent" resolve against computed font sizes, which may include
    // CSS zoom scaling. However, lengths within the SVG subtree shall be resolved
    // excluding zoom, because the (anonymous) RenderSVGViewportContainer applies zooming
    // for the whole SVG subtree as an affine transform. Therefore any font-relative length
    // within the SVG subtree needs to exclude the 'zoom' information.
    if (CSS::isFontOrRootFontRelativeLength(unit))
        resolvedValue = removeZoomFromFontOrRootFontRelativeLength(resolvedValue, unit);

    return resolvedValue;
}

float SVGLengthContext::removeZoomFromFontOrRootFontRelativeLength(float value, CSS::LengthUnit unit) const
{
    RefPtr svgElement = m_context->isOutermostSVGSVGElement()
        ? downcast<SVGSVGElement>(m_context.get())
        : dynamicDowncast<SVGSVGElement>(m_context->viewportElement());

    if (!svgElement || !svgElement->renderer())
        return value;

    float usedZoom = 1.0f;

    if (CSS::isFontRelativeLength(unit))
        usedZoom = svgElement->renderer()->style().usedZoom();
    else if (CSS::isRootFontRelativeLength(unit)) {
        if (auto* rootRenderer = svgElement->document().documentElement()->renderer())
            usedZoom = rootRenderer->style().usedZoom();
    }

    return (usedZoom != 1.0f) ? value / usedZoom : value;
}

ExceptionOr<float> SVGLengthContext::resolveValueToUserUnits(float value, const CSS::LengthPercentageUnit& targetUnit, SVGLengthMode lengthMode) const
{
    switch (targetUnit) {
    case CSS::LengthPercentageUnit::Percentage:
        return convertValueFromPercentageToUserUnits(value / 100.0, lengthMode);

    case CSS::LengthPercentageUnit::Ex:
        // FIXME: Legacy quirk. Using the computeNonCalcLengthDouble conversion here causes test failures
        // (e.g. coords-units-03-b.svg drifting from 150 > ~139). Needs deeper investigation before unifying.
        return convertValueFromEXSToUserUnits(value);

    default: {
        auto cssUnit = toCSSUnitType(targetUnit);
        auto lengthUnit = CSS::toLengthUnit(cssUnit);

        if (!lengthUnit)
            return Exception { ExceptionCode::NotSupportedError };

        return computeNonCalcLength(value, *lengthUnit);
    }
    }
}

ExceptionOr<CSS::LengthPercentage<>> SVGLengthContext::resolveValueFromUserUnits(float value, const CSS::LengthPercentageUnit& targetUnit, SVGLengthMode lengthMode) const
{
    switch (targetUnit) {
    case CSS::LengthPercentageUnit::Percentage: {
        auto percent = convertValueFromUserUnitsToPercentage(value, lengthMode);
        if (percent.hasException())
            return percent.releaseException();
        return CSS::LengthPercentage<>(targetUnit, percent.releaseReturnValue());
    }

    case CSS::LengthPercentageUnit::Ex: {
        auto exVal = convertValueFromUserUnitsToEXS(value);
        if (exVal.hasException())
            return exVal.releaseException();
        return CSS::LengthPercentage<>(targetUnit, exVal.releaseReturnValue());
    }

    default: {
        auto cssUnit = toCSSUnitType(targetUnit);
        auto lengthUnit = CSS::toLengthUnit(cssUnit);

        if (!lengthUnit)
            return Exception { ExceptionCode::NotSupportedError };

        auto pxPerUnit = computeNonCalcLength(1.0, *lengthUnit);
        if (!pxPerUnit)
            return Exception { ExceptionCode::NotSupportedError };

        return CSS::LengthPercentage<>(targetUnit, value / pxPerUnit);
    }
    }
}

ExceptionOr<float> SVGLengthContext::convertValueFromUserUnitsToPercentage(float value, SVGLengthMode lengthMode) const
{
    auto viewportSize = this->viewportSize();
    if (!viewportSize)
        return Exception { ExceptionCode::NotSupportedError };

    if (auto divisor = dimensionForLengthMode(lengthMode, *viewportSize))
        return value / divisor * 100;

    return value;
}

ExceptionOr<float> SVGLengthContext::convertValueFromPercentageToUserUnits(float value, SVGLengthMode lengthMode) const
{
    auto viewportSize = this->viewportSize();
    if (!viewportSize)
        return Exception { ExceptionCode::NotSupportedError };

    return convertValueFromPercentageToUserUnits(value, lengthMode, *viewportSize);
}

float SVGLengthContext::convertValueFromPercentageToUserUnits(float value, SVGLengthMode lengthMode, FloatSize viewportSize)
{
    return value * dimensionForLengthMode(lengthMode, viewportSize);
}

static inline const RenderStyle* renderStyleForLengthResolving(const SVGElement* context)
{
    if (!context)
        return nullptr;

    const ContainerNode* currentContext = context;
    do {
        if (currentContext->renderer())
            return &currentContext->renderer()->style();
        currentContext = currentContext->parentNode();
    } while (currentContext);

    return nullptr;
}

static inline const RenderStyle* rootRenderStyleForLengthResolving(const SVGElement* svgElement)
{
    if (!svgElement)
        return nullptr;

    RefPtr rootElement = svgElement->document().documentElement();
    if (!rootElement || !rootElement->renderer())
        return nullptr;

    return &rootElement->renderer()->style();
}

std::optional<CSSToLengthConversionData> SVGLengthContext::cssConversionData() const
{
    auto element = m_context;
    if (!element)
        return std::nullopt;

    auto* currentStyle = renderStyleForLengthResolving(element.get());
    if (!currentStyle)
        return std::nullopt;

    auto* rootStyle = rootRenderStyleForLengthResolving(element.get());

    const RenderStyle* parentStyle = nullptr;
    if (auto* renderer = element->renderer())
        parentStyle = renderer->parentStyle();

    return CSSToLengthConversionData {
        *currentStyle,
        rootStyle,
        parentStyle,
        element->document().renderView(),
        element.get(),
    };
}

RefPtr<const SVGElement> SVGLengthContext::protectedContext() const
{
    return m_context.get();
}

ExceptionOr<float> SVGLengthContext::convertValueFromUserUnitsToEXS(float value) const
{
    auto* style = renderStyleForLengthResolving(protectedContext().get());
    if (!style)
        return Exception { ExceptionCode::NotSupportedError };

    // Use of ceil allows a pixel match to the W3Cs expected output of coords-units-03-b.svg
    // if this causes problems in real world cases maybe it would be best to remove this
    float xHeight = std::ceil(style->metricsOfPrimaryFont().xHeight().value_or(0));
    if (!xHeight)
        return Exception { ExceptionCode::NotSupportedError };

    return value / xHeight;
}

ExceptionOr<float> SVGLengthContext::convertValueFromEXSToUserUnits(float value) const
{
    auto* style = renderStyleForLengthResolving(protectedContext().get());
    if (!style)
        return Exception { ExceptionCode::NotSupportedError };

    // Use of ceil allows a pixel match to the W3Cs expected output of coords-units-03-b.svg
    // if this causes problems in real world cases maybe it would be best to remove this
    return value * std::ceil(style->metricsOfPrimaryFont().xHeight().value_or(0));
}

std::optional<FloatSize> SVGLengthContext::viewportSize() const
{
    if (!m_context)
        return m_viewportSize;

    if (!m_viewportSize)
        m_viewportSize = computeViewportSize();
    
    return m_viewportSize;
}

std::optional<FloatSize> SVGLengthContext::computeViewportSize() const
{
    using ViewportElementType = SVGElement::ViewportElementType;

    ASSERT(m_context);

    // Root <svg> element lengths are resolved against the top level viewport,
    // however excluding 'zoom' induced scaling. Length within the <svg> subtree
    // shall be resolved against the 'vanilla' viewport size, excluding zoom, because
    // the (anonymous) RenderSVGViewportContainer (first and only child of RenderSVGRoot)
    // applies zooming/panning for the whole SVG subtree as affine transform. Therefore
    // any length within the SVG subtree needs to exclude the 'zoom' information.
    if (m_context->isOutermostSVGSVGElement())
        return downcast<SVGSVGElement>(*protectedContext()).currentViewportSizeExcludingZoom();

    // Take size from nearest SVGSVGElement, skipping over <symbol> elements.
    RefPtr svg = dynamicDowncast<SVGSVGElement>(m_context->viewportElement(ViewportElementType::SVGSVGOnly));
    if (!svg)
        return std::nullopt;

    auto viewportSize = svg->currentViewBoxRect().size();
    if (viewportSize.isEmpty())
        viewportSize = svg->currentViewportSizeExcludingZoom();

    return viewportSize;
}

}
