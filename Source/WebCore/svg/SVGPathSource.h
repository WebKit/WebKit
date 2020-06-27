/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
#include "SVGPathSeg.h"

namespace WebCore {


class SVGPathSource {
    WTF_MAKE_NONCOPYABLE(SVGPathSource); WTF_MAKE_FAST_ALLOCATED;
public:
    SVGPathSource() = default;
    virtual ~SVGPathSource() = default;

    virtual bool hasMoreData() const = 0;
    virtual bool moveToNextToken() = 0;
    virtual SVGPathSegType nextCommand(SVGPathSegType previousCommand) = 0;

    virtual Optional<SVGPathSegType> parseSVGSegmentType() = 0;

    struct MoveToSegment {
        FloatPoint targetPoint;
    };
    virtual Optional<MoveToSegment> parseMoveToSegment() = 0;

    struct LineToSegment {
        FloatPoint targetPoint;
    };
    virtual Optional<LineToSegment> parseLineToSegment() = 0;

    struct LineToHorizontalSegment {
        float x = 0;
    };
    virtual Optional<LineToHorizontalSegment> parseLineToHorizontalSegment() = 0;

    struct LineToVerticalSegment {
        float y = 0;
    };
    virtual Optional<LineToVerticalSegment> parseLineToVerticalSegment() = 0;

    struct CurveToCubicSegment {
        FloatPoint point1;
        FloatPoint point2;
        FloatPoint targetPoint;
    };
    virtual Optional<CurveToCubicSegment> parseCurveToCubicSegment() = 0;

    struct CurveToCubicSmoothSegment {
        FloatPoint point2;
        FloatPoint targetPoint;
    };
    virtual Optional<CurveToCubicSmoothSegment> parseCurveToCubicSmoothSegment() = 0;

    struct CurveToQuadraticSegment {
        FloatPoint point1;
        FloatPoint targetPoint;
    };
    virtual Optional<CurveToQuadraticSegment> parseCurveToQuadraticSegment() = 0;

    struct CurveToQuadraticSmoothSegment {
        FloatPoint targetPoint;
    };
    virtual Optional<CurveToQuadraticSmoothSegment> parseCurveToQuadraticSmoothSegment() = 0;

    struct ArcToSegment {
        float rx = 0;
        float ry = 0;
        float angle = 0;
        bool largeArc = false;
        bool sweep = false;
        FloatPoint targetPoint;
    };
    virtual Optional<ArcToSegment> parseArcToSegment() = 0;
};

} // namespace WebCore
