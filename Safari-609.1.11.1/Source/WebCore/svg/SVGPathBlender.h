/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "SVGPathConsumer.h"

namespace WebCore {

enum FloatBlendMode {
    BlendHorizontal,
    BlendVertical
};

class SVGPathSource;

class SVGPathBlender {
    WTF_MAKE_NONCOPYABLE(SVGPathBlender); WTF_MAKE_FAST_ALLOCATED;
public:

    static bool addAnimatedPath(SVGPathSource& from, SVGPathSource& to, SVGPathConsumer&, unsigned repeatCount);
    static bool blendAnimatedPath(SVGPathSource& from, SVGPathSource& to, SVGPathConsumer&, float);

    static bool canBlendPaths(SVGPathSource& from, SVGPathSource& to);

private:
    SVGPathBlender(SVGPathSource&, SVGPathSource&, SVGPathConsumer* = nullptr);

    bool canBlendPaths();

    bool addAnimatedPath(unsigned repeatCount);
    bool blendAnimatedPath(float progress);

    bool blendMoveToSegment(float progress);
    bool blendLineToSegment(float progress);
    bool blendLineToHorizontalSegment(float progress);
    bool blendLineToVerticalSegment(float progress);
    bool blendCurveToCubicSegment(float progress);
    bool blendCurveToCubicSmoothSegment(float progress);
    bool blendCurveToQuadraticSegment(float progress);
    bool blendCurveToQuadraticSmoothSegment(float progress);
    bool blendArcToSegment(float progress);

    float blendAnimatedDimensonalFloat(float from, float to, FloatBlendMode, float progress);
    FloatPoint blendAnimatedFloatPoint(const FloatPoint& from, const FloatPoint& to, float progress);

    SVGPathSource& m_fromSource;
    SVGPathSource& m_toSource;
    SVGPathConsumer* m_consumer; // A null consumer indicates that we're just checking blendability.

    FloatPoint m_fromCurrentPoint;
    FloatPoint m_toCurrentPoint;
    
    PathCoordinateMode m_fromMode { AbsoluteCoordinates };
    PathCoordinateMode m_toMode { AbsoluteCoordinates };
    unsigned m_addTypesCount { 0 };
    bool m_isInFirstHalfOfAnimation { false };
};

} // namespace WebCore
