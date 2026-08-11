/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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

#include "SVGTransformValue.h"

namespace WebCore {
    
class AffineTransform;
    
class SVGTransformDistance {
public:
    SVGTransformDistance() = default;
    SVGTransformDistance(const SVGTransformValue& fromTransform, const SVGTransformValue& toTransform);

    SVGTransformDistance scaledDistance(float scaleFactor) const;
    SVGTransformValue addToSVGTransform(const SVGTransformValue&) const;

    static SVGTransformValue addSVGTransforms(const SVGTransformValue&, const SVGTransformValue&, unsigned repeatCount = 1);

private:
    SVGTransformDistance(SVGTransformValue::SVGTransformType, float angle, float cx, float cy, const AffineTransform&);

    SVGTransformValue::SVGTransformType m_type { SVGTransformValue::SVG_TRANSFORM_UNKNOWN };
    float m_angle { 0 };
    float m_cx { 0 };
    float m_cy { 0 };
    AffineTransform m_transform; // for storing scale, translation or matrix transforms
};

} // namespace WebCore
