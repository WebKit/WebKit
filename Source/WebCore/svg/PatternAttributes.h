/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "SVGLengthValue.h"
#include "SVGPreserveAspectRatioValue.h"
#include "SVGUnitTypes.h"
#include <wtf/WeakPtr.h>

namespace WebCore {

class SVGPatternElement;
class WeakPtrImplWithEventTargetData;

struct PatternAttributes {
    PatternAttributes() = default;

    SVGLengthValue x() const { return m_x; }
    SVGLengthValue y() const { return m_y; }
    SVGLengthValue width() const { return m_width; }
    SVGLengthValue height() const { return m_height; }
    FloatRect viewBox() const { return m_viewBox; }
    SVGPreserveAspectRatioValue preserveAspectRatio() const { return m_preserveAspectRatio; }
    SVGUnitTypes::SVGUnitType patternUnits() const { return m_patternUnits; }
    SVGUnitTypes::SVGUnitType patternContentUnits() const { return m_patternContentUnits; }
    AffineTransform patternTransform() const { return m_patternTransform; }
    const SVGPatternElement* patternContentElement() const { return m_patternContentElement.get(); }

    void setX(SVGLengthValue value)
    {
        m_x = value;
        m_xSet = true;
    }

    void setY(SVGLengthValue value)
    {
        m_y = value;
        m_ySet = true;
    }

    void setWidth(SVGLengthValue value)
    {
        m_width = value;
        m_widthSet = true;
    }

    void setHeight(SVGLengthValue value)
    {
        m_height = value;
        m_heightSet = true;
    }
    
    void setViewBox(const FloatRect& value)
    {
        m_viewBox = value;
        m_viewBoxSet = true;
    }

    void setPreserveAspectRatio(SVGPreserveAspectRatioValue value)
    {
        m_preserveAspectRatio = value;
        m_preserveAspectRatioSet = true;
    }

    void setPatternUnits(SVGUnitTypes::SVGUnitType value)
    {
        m_patternUnits = value;
        m_patternUnitsSet = true;
    }

    void setPatternContentUnits(SVGUnitTypes::SVGUnitType value)
    {
        m_patternContentUnits = value;
        m_patternContentUnitsSet = true;
    }

    void setPatternTransform(const AffineTransform& value)
    {
        m_patternTransform = value;
        m_patternTransformSet = true;
    }

    void setPatternContentElement(const SVGPatternElement* value)
    {
        m_patternContentElement = value;
        m_patternContentElementSet = true;
    }

    bool hasX() const { return m_xSet; }
    bool hasY() const { return m_ySet; }
    bool hasWidth() const { return m_widthSet; }
    bool hasHeight() const { return m_heightSet; }
    bool hasViewBox() const { return m_viewBoxSet; }
    bool hasPreserveAspectRatio() const { return m_preserveAspectRatioSet; }
    bool hasPatternUnits() const { return m_patternUnitsSet; }
    bool hasPatternContentUnits() const { return m_patternContentUnitsSet; }
    bool hasPatternTransform() const { return m_patternTransformSet; }
    bool hasPatternContentElement() const { return m_patternContentElementSet; }

private:
    // Properties
    SVGLengthValue m_x;
    SVGLengthValue m_y;
    SVGLengthValue m_width;
    SVGLengthValue m_height;
    FloatRect m_viewBox;
    SVGPreserveAspectRatioValue m_preserveAspectRatio;
    SVGUnitTypes::SVGUnitType m_patternUnits { SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX };
    SVGUnitTypes::SVGUnitType m_patternContentUnits { SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE };
    AffineTransform m_patternTransform;
    WeakPtr<const SVGPatternElement, WeakPtrImplWithEventTargetData> m_patternContentElement;

    // Property states
    bool m_xSet : 1 { false };
    bool m_ySet : 1 { false };
    bool m_widthSet : 1 { false };
    bool m_heightSet : 1 { false };
    bool m_viewBoxSet : 1 { false };
    bool m_preserveAspectRatioSet : 1 { false };
    bool m_patternUnitsSet : 1 { false };
    bool m_patternContentUnitsSet : 1 { false };
    bool m_patternTransformSet : 1 { false };
    bool m_patternContentElementSet : 1 { false };
};

} // namespace WebCore
