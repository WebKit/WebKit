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
#include "CSSUnits.h"
#include "FontCascade.h"
#include "FontMetrics.h"
#include "LegacyRenderSVGRoot.h"
#include "LengthFunctions.h"
#include "LocalFrame.h"
#include "RenderView.h"
#include "SVGElementTypeHelpers.h"
#include "SVGSVGElement.h"
#include "StylePreferredSize.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include <numbers>
#include <wtf/MathExtras.h>

namespace WebCore {

SVGLengthContext::SVGLengthContext(const SVGElement* context)
    : m_context(context)
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

    SVGLengthContext lengthContext(context);
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

float SVGLengthContext::valueForLength(const Length& length, SVGLengthMode lengthMode)
{
    switch (length.type()) {
    case LengthType::Fixed:
        return length.value();

    case LengthType::Percent: {
        auto result = convertValueFromPercentageToUserUnits(length.value() / 100, lengthMode);
        if (result.hasException())
            return 0;
        return result.releaseReturnValue();
    }

    case LengthType::Calculated: {
        auto viewportSize = this->viewportSize().value_or(FloatSize { });
        return length.nonNanCalculatedValue(dimensionForLengthMode(lengthMode, viewportSize));
    }

    default:
        return 0;
    }
}

float SVGLengthContext::valueForLength(const Style::PreferredSize& size, SVGLengthMode lengthMode)
{
    return WTF::switchOn(size,
        [&](const Style::PreferredSize::Fixed& fixed) -> float {
            return fixed.value;
        },
        [&](const Style::PreferredSize::Percentage& percentage) -> float {
            auto result = convertValueFromPercentageToUserUnits(percentage.value / 100, lengthMode);
            if (result.hasException())
                return 0;
            return result.releaseReturnValue();
        },
        [&](const Style::PreferredSize::Calc& calc) -> float {
            auto viewportSize = this->viewportSize().value_or(FloatSize { });
            return Style::evaluate(calc, dimensionForLengthMode(lengthMode, viewportSize));
        },
        [&](const auto&) -> float {
            return 0;
        }
    );
}

ExceptionOr<float> SVGLengthContext::convertValueToUserUnits(float value, SVGLengthType lengthType, SVGLengthMode lengthMode) const
{
    switch (lengthType) {
    case SVGLengthType::Unknown:
        return Exception { ExceptionCode::NotSupportedError };
    case SVGLengthType::Number:
        return value;
    case SVGLengthType::Pixels:
        return value;
    case SVGLengthType::Percentage:
        return convertValueFromPercentageToUserUnits(value / 100, lengthMode);
    case SVGLengthType::Ems:
        return convertValueFromEMSToUserUnits(value);
    case SVGLengthType::Exs:
        return convertValueFromEXSToUserUnits(value);
    case SVGLengthType::Lh:
        return convertValueFromLhToUserUnits(value);
    case SVGLengthType::Ch:
        return convertValueFromChToUserUnits(value);
    case SVGLengthType::Centimeters:
        return value * CSS::pixelsPerCm;
    case SVGLengthType::Millimeters:
        return value * CSS::pixelsPerMm;
    case SVGLengthType::Inches:
        return value * CSS::pixelsPerInch;
    case SVGLengthType::Points:
        return value * CSS::pixelsPerPt;
    case SVGLengthType::Picas:
        return value * CSS::pixelsPerPc;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

ExceptionOr<float> SVGLengthContext::convertValueFromUserUnits(float value, SVGLengthType lengthType, SVGLengthMode lengthMode) const
{
    switch (lengthType) {
    case SVGLengthType::Unknown:
        return Exception { ExceptionCode::NotSupportedError };
    case SVGLengthType::Number:
        return value;
    case SVGLengthType::Percentage:
        return convertValueFromUserUnitsToPercentage(value * 100, lengthMode);
    case SVGLengthType::Ems:
        return convertValueFromUserUnitsToEMS(value);
    case SVGLengthType::Exs:
        return convertValueFromUserUnitsToEXS(value);
    case SVGLengthType::Lh:
        return convertValueFromUserUnitsToLh(value);
    case SVGLengthType::Ch:
        return convertValueFromUserUnitsToCh(value);
    case SVGLengthType::Pixels:
        return value;
    case SVGLengthType::Centimeters:
        return value / CSS::pixelsPerCm;
    case SVGLengthType::Millimeters:
        return value / CSS::pixelsPerMm;
    case SVGLengthType::Inches:
        return value / CSS::pixelsPerInch;
    case SVGLengthType::Points:
        return value / CSS::pixelsPerPt;
    case SVGLengthType::Picas:
        return value / CSS::pixelsPerPc;
    }

    ASSERT_NOT_REACHED();
    return 0;
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

RefPtr<const SVGElement> SVGLengthContext::protectedContext() const
{
    return m_context.get();
}

ExceptionOr<float> SVGLengthContext::convertValueFromUserUnitsToEMS(float value) const
{
    auto* style = renderStyleForLengthResolving(protectedContext().get());
    if (!style)
        return Exception { ExceptionCode::NotSupportedError };

    float fontSize = style->computedFontSize();
    if (!fontSize)
        return Exception { ExceptionCode::NotSupportedError };

    return value / fontSize;
}

ExceptionOr<float> SVGLengthContext::convertValueFromEMSToUserUnits(float value) const
{
    auto* style = renderStyleForLengthResolving(protectedContext().get());
    if (!style)
        return Exception { ExceptionCode::NotSupportedError };

    return value * style->computedFontSize();
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

ExceptionOr<float> SVGLengthContext::convertValueFromUserUnitsToLh(float value) const
{
    auto* style = renderStyleForLengthResolving(protectedContext().get());
    if (!style)
        return Exception { ExceptionCode::NotSupportedError };

    return value / adjustForAbsoluteZoom(style->computedLineHeight(), *style);
}

ExceptionOr<float> SVGLengthContext::convertValueFromLhToUserUnits(float value) const
{
    auto* style = renderStyleForLengthResolving(protectedContext().get());
    if (!style)
        return Exception { ExceptionCode::NotSupportedError };

    return value * adjustForAbsoluteZoom(style->computedLineHeight(), *style);
}

ExceptionOr<float> SVGLengthContext::convertValueFromUserUnitsToCh(float value) const
{
    auto* style = renderStyleForLengthResolving(protectedContext().get());
    if (!style)
        return Exception { ExceptionCode::NotSupportedError };

    float zeroWidth = style->fontCascade().zeroWidth();
    if (!zeroWidth)
        return 0;

    return value / zeroWidth;
}

ExceptionOr<float> SVGLengthContext::convertValueFromChToUserUnits(float value) const
{
    auto* style = renderStyleForLengthResolving(protectedContext().get());
    if (!style)
        return Exception { ExceptionCode::NotSupportedError };

    return value * style->fontCascade().zeroWidth();
}

std::optional<FloatSize> SVGLengthContext::viewportSize() const
{
    if (!m_context)
        return std::nullopt;

    if (!m_viewportSize)
        m_viewportSize = computeViewportSize();
    
    return m_viewportSize;
}

std::optional<FloatSize> SVGLengthContext::computeViewportSize() const
{
    ASSERT(m_context);

    // Root <svg> element lengths are resolved against the top level viewport,
    // however excluding 'zoom' induced scaling. Length within the <svg> subtree
    // shall be resolved against the 'vanilla' viewport size, excluding zoom, because
    // the (anonymous) RenderSVGViewportContainer (first and only child of RenderSVGRoot)
    // applies zooming/panning for the whole SVG subtree as affine transform. Therefore
    // any length within the SVG subtree needs to exclude the 'zoom' information.
    if (m_context->isOutermostSVGSVGElement())
        return downcast<SVGSVGElement>(*protectedContext()).currentViewportSizeExcludingZoom();

    // Take size from nearest viewport element.
    RefPtr svg = dynamicDowncast<SVGSVGElement>(m_context->viewportElement());
    if (!svg)
        return std::nullopt;

    auto viewportSize = svg->currentViewBoxRect().size();
    if (viewportSize.isEmpty())
        viewportSize = svg->currentViewportSizeExcludingZoom();

    return viewportSize;
}

}
