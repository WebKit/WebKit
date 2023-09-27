/*
 * Copyright (C) 2023 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PathSegmentData.h"

#if USE(CG)

#include "PathSegmentUtilitiesCG.h"

namespace WebCore {

void PathMoveTo::addToContext(PlatformGraphicsContext* context) const
{
    CGContextMoveToPoint(context, point.x(), point.y());
}

void PathLineTo::addToContext(PlatformGraphicsContext* context) const
{
    CGContextAddLineToPoint(context, point.x(), point.y());
}

void PathQuadCurveTo::addToContext(PlatformGraphicsContext* context) const
{
    CGContextAddQuadCurveToPoint(context, controlPoint.x(), controlPoint.y(), endPoint.x(), endPoint.y());
}

void PathBezierCurveTo::addToContext(PlatformGraphicsContext* context) const
{
    CGContextAddCurveToPoint(context, controlPoint1.x(), controlPoint1.y(), controlPoint2.x(), controlPoint2.y(), endPoint.x(), endPoint.y());
}

void PathArcTo::addToContext(PlatformGraphicsContext* context) const
{
    CGContextAddArcToPoint(context, controlPoint1.x(), controlPoint1.y(), controlPoint2.x(), controlPoint2.y(), radius);
}

void PathArc::addToContext(PlatformGraphicsContext* context) const
{
    CGContextAddArc(context, center.x(), center.y(), radius, startAngle, endAngle, direction == RotationDirection::Counterclockwise);
}

void PathEllipse::addToContext(PlatformGraphicsContext* context) const
{
    auto platformPath = adoptCF(CGPathCreateMutable());
    addEllipseToPlatformPath(platformPath.get(), center, radiusX, radiusY, rotation, startAngle, endAngle, direction);
    CGContextAddPath(context, platformPath.get());
}

void PathEllipseInRect::addToContext(PlatformGraphicsContext* context) const
{
    CGContextAddEllipseInRect(context, rect);
}

void PathRect::addToContext(PlatformGraphicsContext* context) const
{
    CGContextAddRect(context, rect);
}

void PathRoundedRect::addToContext(PlatformGraphicsContext* context) const
{
    if (strategy == PathRoundedRect::Strategy::PreferNative) {
        auto platformPath = adoptCF(CGPathCreateMutable());
        if (addRoundedRectToPlatformPath(platformPath.get(), roundedRect)) {
            CGContextAddPath(context, platformPath.get());
            return;
        }
    }

    PathStream stream;
    stream.addBeziersForRoundedRect(roundedRect);
    stream.addToContext(context);
}

void PathDataLine::addToContext(PlatformGraphicsContext* context) const
{
    CGContextMoveToPoint(context, start.x(), start.y());
    CGContextAddLineToPoint(context, end.x(), end.y());
}

void PathDataQuadCurve::addToContext(PlatformGraphicsContext* context) const
{
    CGContextMoveToPoint(context, start.x(), start.y());
    CGContextAddQuadCurveToPoint(context, controlPoint.x(), controlPoint.y(), endPoint.x(), endPoint.y());
}

void PathDataBezierCurve::addToContext(PlatformGraphicsContext* context) const
{
    CGContextMoveToPoint(context, start.x(), start.y());
    CGContextAddCurveToPoint(context, controlPoint1.x(), controlPoint1.y(), controlPoint2.x(), controlPoint2.y(), endPoint.x(), endPoint.y());
}

void PathDataArc::addToContext(PlatformGraphicsContext* context) const
{
    CGContextMoveToPoint(context, start.x(), start.y());
    CGContextAddArcToPoint(context, controlPoint1.x(), controlPoint1.y(), controlPoint2.x(), controlPoint2.y(), radius);
}

void PathCloseSubpath::addToContext(PlatformGraphicsContext* context) const
{
    CGContextClosePath(context);
}

} // namespace WebCore

#endif // USE(CG)
