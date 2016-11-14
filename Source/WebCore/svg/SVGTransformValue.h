/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
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

#include "FloatPoint.h"
#include "SVGMatrixValue.h"

namespace WebCore {

class FloatSize;

class SVGTransformValue {
public:
    enum SVGTransformType {
        SVG_TRANSFORM_UNKNOWN = 0,
        SVG_TRANSFORM_MATRIX = 1,
        SVG_TRANSFORM_TRANSLATE = 2,
        SVG_TRANSFORM_SCALE = 3,
        SVG_TRANSFORM_ROTATE = 4,
        SVG_TRANSFORM_SKEWX = 5,
        SVG_TRANSFORM_SKEWY = 6
    };

    enum ConstructionMode {
        ConstructIdentityTransform,
        ConstructZeroTransform
    };

    SVGTransformValue();
    SVGTransformValue(SVGTransformType, ConstructionMode = ConstructIdentityTransform);
    explicit SVGTransformValue(const AffineTransform&);

    SVGTransformType type() const { return m_type; }

    SVGMatrixValue& svgMatrix() { return static_cast<SVGMatrixValue&>(m_matrix); }
    AffineTransform matrix() const { return m_matrix; }
    void updateSVGMatrix();

    float angle() const { return m_angle; }
    FloatPoint rotationCenter() const { return m_center; }

    void setMatrix(const AffineTransform&);
    void setTranslate(float tx, float ty);
    void setScale(float sx, float sy);
    void setRotate(float angle, float cx, float cy);
    void setSkewX(float angle);
    void setSkewY(float angle);
    
    FloatPoint translate() const;
    FloatSize scale() const;

    bool isValid() const { return m_type != SVG_TRANSFORM_UNKNOWN; }
    String valueAsString() const;

    static const String& transformTypePrefixForParsing(SVGTransformType);

private:
    friend bool operator==(const SVGTransformValue&, const SVGTransformValue&);

    SVGTransformType m_type { SVG_TRANSFORM_UNKNOWN };
    float m_angle { 0 };
    FloatPoint m_center;
    AffineTransform m_matrix;
};

inline bool operator==(const SVGTransformValue& a, const SVGTransformValue& b)
{
    return a.m_type == b.m_type && a.m_angle == b.m_angle && a.m_matrix == b.m_matrix;
}

inline bool operator!=(const SVGTransformValue& a, const SVGTransformValue& b)
{
    return !(a == b);
}

} // namespace WebCore
