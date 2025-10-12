/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2019-2025 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/FloatRect.h>
#include <WebCore/SVGLengthValue.h>
#include <WebCore/SVGUnitTypes.h>

namespace WebCore {

class CSSToLengthConversionData;
class SVGElement;
class WeakPtrImplWithEventTargetData;

template<typename> class ExceptionOr;

namespace Style {
struct PreferredSize;
struct SVGCenterCoordinateComponent;
struct SVGCoordinateComponent;
struct SVGRadius;
struct SVGRadiusComponent;
struct SVGStrokeDasharrayValue;
struct SVGStrokeDashoffset;
struct StrokeWidth;
}

class SVGLengthContext {
public:
    explicit SVGLengthContext(const SVGElement*);
    ~SVGLengthContext();

    template<typename T>
    static FloatRect resolveRectangle(const T* context, SVGUnitTypes::SVGUnitType type, const FloatRect& viewport)
    {
        return resolveRectangle(context, type, viewport, context->x(), context->y(), context->width(), context->height());
    }

    static FloatRect resolveRectangle(const SVGElement*, SVGUnitTypes::SVGUnitType, const FloatRect& viewport, const SVGLengthValue& x, const SVGLengthValue& y, const SVGLengthValue& width, const SVGLengthValue& height);
    static FloatPoint resolvePoint(const SVGElement*, SVGUnitTypes::SVGUnitType, const SVGLengthValue& x, const SVGLengthValue& y);
    static float resolveLength(const SVGElement*, SVGUnitTypes::SVGUnitType, const SVGLengthValue&);

    float valueForLength(const Style::PreferredSize&, SVGLengthMode = SVGLengthMode::Other);
    float valueForLength(const Style::SVGCenterCoordinateComponent&, SVGLengthMode = SVGLengthMode::Other);
    float valueForLength(const Style::SVGCoordinateComponent&, SVGLengthMode = SVGLengthMode::Other);
    float valueForLength(const Style::SVGRadius&, SVGLengthMode = SVGLengthMode::Other);
    float valueForLength(const Style::SVGRadiusComponent&, SVGLengthMode = SVGLengthMode::Other);
    float valueForLength(const Style::SVGStrokeDasharrayValue&, SVGLengthMode = SVGLengthMode::Other);
    float valueForLength(const Style::SVGStrokeDashoffset&, SVGLengthMode = SVGLengthMode::Other);
    float valueForLength(const Style::StrokeWidth&, SVGLengthMode = SVGLengthMode::Other);

    ExceptionOr<float> resolveValueToUserUnits(float, const CSS::LengthPercentageUnit&, SVGLengthMode) const;
    ExceptionOr<CSS::LengthPercentage<>> resolveValueFromUserUnits(float, const CSS::LengthPercentageUnit&, SVGLengthMode) const;

    std::optional<FloatSize> viewportSize() const;

private:
    ExceptionOr<float> convertValueFromUserUnitsToPercentage(float value, SVGLengthMode) const;
    ExceptionOr<float> convertValueFromPercentageToUserUnits(float value, SVGLengthMode) const;
    static float convertValueFromPercentageToUserUnits(float value, SVGLengthMode, FloatSize);

    ExceptionOr<float> convertValueFromUserUnitsToEXS(float) const;
    ExceptionOr<float> convertValueFromEXSToUserUnits(float) const;

    std::optional<FloatSize> computeViewportSize() const;
    float computeNonCalcLength(float, CSS::LengthUnit) const;
    float removeZoomFromFontOrRootFontRelativeLength(float value, CSS::LengthUnit) const;

    std::optional<CSSToLengthConversionData> cssConversionData() const;
    RefPtr<const SVGElement> protectedContext() const;

    template<typename SizeType> float valueForSizeType(const SizeType&, SVGLengthMode = SVGLengthMode::Other);

    WeakPtr<const SVGElement, WeakPtrImplWithEventTargetData> m_context;
    mutable std::optional<FloatSize> m_viewportSize;
};

} // namespace WebCore
