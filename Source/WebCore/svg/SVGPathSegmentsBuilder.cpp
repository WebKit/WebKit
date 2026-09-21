/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SVGPathSegmentsBuilder.h"

#include "FloatPoint.h"
#include "SVGPathSegType.h"
#include <wtf/text/MakeString.h>

namespace WebCore {

SVGPathSegmentsBuilder::SVGPathSegmentsBuilder(Vector<SVGPathSegment>& result)
    : m_result(result)
{
}

void SVGPathSegmentsBuilder::appendSegment(SVGPathSegType absoluteType, SVGPathSegType relativeType, PathCoordinateMode mode, Vector<float>&& values)
{
    auto type = mode == AbsoluteCoordinates ? absoluteType : relativeType;
    m_result.append({ makeString(letterForPathSegType(type)), WTF::move(values) });
}

void SVGPathSegmentsBuilder::moveTo(const FloatPoint& targetPoint, bool, PathCoordinateMode mode)
{
    appendSegment(SVGPathSegType::MoveToAbs, SVGPathSegType::MoveToRel, mode, { targetPoint.x(), targetPoint.y() });
}

void SVGPathSegmentsBuilder::lineTo(const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    appendSegment(SVGPathSegType::LineToAbs, SVGPathSegType::LineToRel, mode, { targetPoint.x(), targetPoint.y() });
}

void SVGPathSegmentsBuilder::lineToHorizontal(float x, PathCoordinateMode mode)
{
    appendSegment(SVGPathSegType::LineToHorizontalAbs, SVGPathSegType::LineToHorizontalRel, mode, { x });
}

void SVGPathSegmentsBuilder::lineToVertical(float y, PathCoordinateMode mode)
{
    appendSegment(SVGPathSegType::LineToVerticalAbs, SVGPathSegType::LineToVerticalRel, mode, { y });
}

void SVGPathSegmentsBuilder::curveToCubic(const FloatPoint& point1, const FloatPoint& point2, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    appendSegment(SVGPathSegType::CurveToCubicAbs, SVGPathSegType::CurveToCubicRel, mode,
        { point1.x(), point1.y(), point2.x(), point2.y(), targetPoint.x(), targetPoint.y() });
}

void SVGPathSegmentsBuilder::curveToCubicSmooth(const FloatPoint& point2, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    appendSegment(SVGPathSegType::CurveToCubicSmoothAbs, SVGPathSegType::CurveToCubicSmoothRel, mode,
        { point2.x(), point2.y(), targetPoint.x(), targetPoint.y() });
}

void SVGPathSegmentsBuilder::curveToQuadratic(const FloatPoint& point1, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    appendSegment(SVGPathSegType::CurveToQuadraticAbs, SVGPathSegType::CurveToQuadraticRel, mode,
        { point1.x(), point1.y(), targetPoint.x(), targetPoint.y() });
}

void SVGPathSegmentsBuilder::curveToQuadraticSmooth(const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    appendSegment(SVGPathSegType::CurveToQuadraticSmoothAbs, SVGPathSegType::CurveToQuadraticSmoothRel, mode,
        { targetPoint.x(), targetPoint.y() });
}

void SVGPathSegmentsBuilder::arcTo(float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    // The spec gives the flags no separate type, so they travel as 1 and 0 in the
    // values sequence: https://w3c.github.io/svgwg/specs/paths/#InterfaceSVGPathSegment
    appendSegment(SVGPathSegType::ArcAbs, SVGPathSegType::ArcRel, mode,
        { r1, r2, angle, largeArcFlag ? 1.0f : 0.0f, sweepFlag ? 1.0f : 0.0f, targetPoint.x(), targetPoint.y() });
}

void SVGPathSegmentsBuilder::closePath()
{
    m_result.append({ makeString(letterForPathSegType(SVGPathSegType::ClosePath)), Vector<float> { } });
}

} // namespace WebCore
