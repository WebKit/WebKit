/*
 * Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

#include "CSSHelper.h"
#include "FontMetrics.h"
#include "Frame.h"
#include "LengthFunctions.h"
#include "RenderSVGRoot.h"
#include "RenderSVGViewportContainer.h"
#include "RenderView.h"
#include "SVGSVGElement.h"

namespace WebCore {

SVGLengthContext::SVGLengthContext(const SVGElement* context)
    : m_context(context)
{
}

SVGLengthContext::SVGLengthContext(const SVGElement* context, const FloatRect& viewport)
    : m_context(context)
    , m_overriddenViewport(viewport)
{
}

FloatRect SVGLengthContext::resolveRectangle(const SVGElement* context, SVGUnitTypes::SVGUnitType type, const FloatRect& viewport, const SVGLengthValue& x, const SVGLengthValue& y, const SVGLengthValue& width, const SVGLengthValue& height)
{
    ASSERT(type != SVGUnitTypes::SVG_UNIT_TYPE_UNKNOWN);
    if (type == SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE) {
        SVGLengthContext lengthContext(context);
        return FloatRect(x.value(lengthContext), y.value(lengthContext), width.value(lengthContext), height.value(lengthContext));
    }

    SVGLengthContext lengthContext(context, viewport);
    return FloatRect(x.value(lengthContext) + viewport.x(),
                     y.value(lengthContext) + viewport.y(),
                     width.value(lengthContext),
                     height.value(lengthContext));
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

float SVGLengthContext::valueForLength(const Length& length, SVGLengthMode mode)
{
    if (length.isPercent()) {
        auto result = convertValueFromPercentageToUserUnits(length.value() / 100, mode);
        if (result.hasException())
            return 0;
        return result.releaseReturnValue();
    }
    if (length.isAuto() || !length.isSpecified())
        return 0;

    FloatSize viewportSize;
    determineViewport(viewportSize);

    switch (mode) {
    case LengthModeWidth:
        return floatValueForLength(length, viewportSize.width());
    case LengthModeHeight:
        return floatValueForLength(length, viewportSize.height());
    case LengthModeOther:
        return floatValueForLength(length, std::sqrt(viewportSize.diagonalLengthSquared() / 2));
    };
    return 0;
}

ExceptionOr<float> SVGLengthContext::convertValueToUserUnits(float value, SVGLengthMode mode, SVGLengthType fromUnit) const
{
    // If the SVGLengthContext carries a custom viewport, force resolving against it.
    if (!m_overriddenViewport.isEmpty()) {
        // 100% = 100.0 instead of 1.0 for historical reasons, this could eventually be changed
        if (fromUnit == LengthTypePercentage)
            value /= 100;
        return convertValueFromPercentageToUserUnits(value, mode);
    }

    switch (fromUnit) {
    case LengthTypeUnknown:
        return Exception { NotSupportedError };
    case LengthTypeNumber:
        return value;
    case LengthTypePX:
        return value;
    case LengthTypePercentage:
        return convertValueFromPercentageToUserUnits(value / 100, mode);
    case LengthTypeEMS:
        return convertValueFromEMSToUserUnits(value);
    case LengthTypeEXS:
        return convertValueFromEXSToUserUnits(value);
    case LengthTypeCM:
        return value * cssPixelsPerInch / 2.54f;
    case LengthTypeMM:
        return value * cssPixelsPerInch / 25.4f;
    case LengthTypeIN:
        return value * cssPixelsPerInch;
    case LengthTypePT:
        return value * cssPixelsPerInch / 72;
    case LengthTypePC:
        return value * cssPixelsPerInch / 6;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

ExceptionOr<float> SVGLengthContext::convertValueFromUserUnits(float value, SVGLengthMode mode, SVGLengthType toUnit) const
{
    switch (toUnit) {
    case LengthTypeUnknown:
        return Exception { NotSupportedError };
    case LengthTypeNumber:
        return value;
    case LengthTypePercentage:
        return convertValueFromUserUnitsToPercentage(value * 100, mode);
    case LengthTypeEMS:
        return convertValueFromUserUnitsToEMS(value);
    case LengthTypeEXS:
        return convertValueFromUserUnitsToEXS(value);
    case LengthTypePX:
        return value;
    case LengthTypeCM:
        return value * 2.54f / cssPixelsPerInch;
    case LengthTypeMM:
        return value * 25.4f / cssPixelsPerInch;
    case LengthTypeIN:
        return value / cssPixelsPerInch;
    case LengthTypePT:
        return value * 72 / cssPixelsPerInch;
    case LengthTypePC:
        return value * 6 / cssPixelsPerInch;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

ExceptionOr<float> SVGLengthContext::convertValueFromUserUnitsToPercentage(float value, SVGLengthMode mode) const
{
    FloatSize viewportSize;
    if (!determineViewport(viewportSize))
        return Exception { NotSupportedError };

    switch (mode) {
    case LengthModeWidth:
        return value / viewportSize.width() * 100;
    case LengthModeHeight:
        return value / viewportSize.height() * 100;
    case LengthModeOther:
        return value / (std::sqrt(viewportSize.diagonalLengthSquared() / 2)) * 100;
    };

    ASSERT_NOT_REACHED();
    return 0;
}

ExceptionOr<float> SVGLengthContext::convertValueFromPercentageToUserUnits(float value, SVGLengthMode mode) const
{
    FloatSize viewportSize;
    if (!determineViewport(viewportSize))
        return Exception { NotSupportedError };

    switch (mode) {
    case LengthModeWidth:
        return value * viewportSize.width();
    case LengthModeHeight:
        return value * viewportSize.height();
    case LengthModeOther:
        return value * std::sqrt(viewportSize.diagonalLengthSquared() / 2);
    };

    ASSERT_NOT_REACHED();
    return 0;
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

    // There must be at least a RenderSVGRoot renderer, carrying a style.
    ASSERT_NOT_REACHED();
    return nullptr;
}

ExceptionOr<float> SVGLengthContext::convertValueFromUserUnitsToEMS(float value) const
{
    auto* style = renderStyleForLengthResolving(m_context);
    if (!style)
        return Exception { NotSupportedError };

    float fontSize = style->computedFontPixelSize();
    if (!fontSize)
        return Exception { NotSupportedError };

    return value / fontSize;
}

ExceptionOr<float> SVGLengthContext::convertValueFromEMSToUserUnits(float value) const
{
    auto* style = renderStyleForLengthResolving(m_context);
    if (!style)
        return Exception { NotSupportedError };

    return value * style->computedFontPixelSize();
}

ExceptionOr<float> SVGLengthContext::convertValueFromUserUnitsToEXS(float value) const
{
    auto* style = renderStyleForLengthResolving(m_context);
    if (!style)
        return Exception { NotSupportedError };

    // Use of ceil allows a pixel match to the W3Cs expected output of coords-units-03-b.svg
    // if this causes problems in real world cases maybe it would be best to remove this
    float xHeight = std::ceil(style->fontMetrics().xHeight());
    if (!xHeight)
        return Exception { NotSupportedError };

    return value / xHeight;
}

ExceptionOr<float> SVGLengthContext::convertValueFromEXSToUserUnits(float value) const
{
    auto* style = renderStyleForLengthResolving(m_context);
    if (!style)
        return Exception { NotSupportedError };

    // Use of ceil allows a pixel match to the W3Cs expected output of coords-units-03-b.svg
    // if this causes problems in real world cases maybe it would be best to remove this
    return value * std::ceil(style->fontMetrics().xHeight());
}

bool SVGLengthContext::determineViewport(FloatSize& viewportSize) const
{
    if (!m_context)
        return false;

    // If an overridden viewport is given, it has precedence.
    if (!m_overriddenViewport.isEmpty()) {
        viewportSize = m_overriddenViewport.size();
        return true;
    }

    // Root <svg> element lengths are resolved against the top level viewport.
    if (m_context->isOutermostSVGSVGElement()) {
        viewportSize = downcast<SVGSVGElement>(*m_context).currentViewportSize();
        return true;
    }

    // Take size from nearest viewport element.
    auto viewportElement = makeRefPtr(m_context->viewportElement());
    if (!is<SVGSVGElement>(viewportElement))
        return false;

    const SVGSVGElement& svg = downcast<SVGSVGElement>(*viewportElement);
    viewportSize = svg.currentViewBoxRect().size();
    if (viewportSize.isEmpty())
        viewportSize = svg.currentViewportSize();

    return true;
}

}
