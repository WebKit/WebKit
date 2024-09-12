/*
 * Copyright (C) 2003-2023 Apple Inc.  All rights reserved.
 * Copyright (C) 2006, 2008 Rob Buis <buis@kde.org>
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
#include "PathCG.h"

#if USE(CG)

#include "GraphicsContextCG.h"
#include "PathStream.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMallocInlines.h>


namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PathCG);

Ref<PathCG> PathCG::create()
{
    return adoptRef(*new PathCG);
}

Ref<PathCG> PathCG::create(const PathSegment& segment)
{
    auto pathCG = PathCG::create();
    pathCG->addSegment(segment);
    return pathCG;
}

Ref<PathCG> PathCG::create(const PathStream& stream)
{
    auto pathCG = PathCG::create();
    for (auto& segment : stream.segments())
        pathCG->addSegment(segment);
    return pathCG;
}

Ref<PathCG> PathCG::create(RetainPtr<CGMutablePathRef>&& platformPath)
{
    return adoptRef(*new PathCG(WTFMove(platformPath)));
}

PathCG::PathCG()
    : m_platformPath(adoptCF(CGPathCreateMutable()))
{
}

PathCG::PathCG(RetainPtr<CGMutablePathRef>&& platformPath)
    : m_platformPath(WTFMove(platformPath))
{
    ASSERT(m_platformPath);
}

bool PathCG::definitelyEqual(const PathImpl& otherImpl) const
{
    RefPtr otherAsPathCGImpl = dynamicDowncast<PathCG>(otherImpl);
    if (!otherAsPathCGImpl) {
        // We could convert other to a CG path to compare, but that would be expensive.
        return false;
    }

    if (otherAsPathCGImpl.get() == this)
        return true;

    if (!m_platformPath && !otherAsPathCGImpl->platformPath())
        return true;

    return CGPathEqualToPath(m_platformPath.get(), otherAsPathCGImpl->platformPath());
}

Ref<PathImpl> PathCG::copy() const
{
    return create({ platformPath() });
}

PlatformPathPtr PathCG::platformPath() const
{
    return m_platformPath.get();
}

PlatformPathPtr PathCG::ensureMutablePlatformPath()
{
    if (CFGetRetainCount(m_platformPath.get()) > 1)
        m_platformPath = adoptCF(CGPathCreateMutableCopy(m_platformPath.get()));
    return m_platformPath.get();
}

// Below contains two implementations per path element implementation:
// the member to add the path element to the PathCG, and correspoding implementation
// for adding path segment directly to the CGContext. Keep these in sync.

void PathCG::add(PathMoveTo moveTo)
{
    CGPathMoveToPoint(ensureMutablePlatformPath(), nullptr, moveTo.point.x(), moveTo.point.y());
}

static inline void addToCGContextPath(CGContextRef context, PathMoveTo segment)
{
    CGContextMoveToPoint(context, segment.point.x(), segment.point.y());
}

void PathCG::add(PathLineTo lineTo)
{
    CGPathAddLineToPoint(ensureMutablePlatformPath(), nullptr, lineTo.point.x(), lineTo.point.y());
}

static inline void addToCGContextPath(CGContextRef context, PathLineTo segment)
{
    CGContextAddLineToPoint(context, segment.point.x(), segment.point.y());
}

void PathCG::add(PathQuadCurveTo quadTo)
{
    CGPathAddQuadCurveToPoint(ensureMutablePlatformPath(), nullptr, quadTo.controlPoint.x(), quadTo.controlPoint.y(), quadTo.endPoint.x(), quadTo.endPoint.y());
}

static inline void addToCGContextPath(CGContextRef context, PathQuadCurveTo segment)
{
    CGContextAddQuadCurveToPoint(context, segment.controlPoint.x(), segment.controlPoint.y(), segment.endPoint.x(), segment.endPoint.y());
}

void PathCG::add(PathBezierCurveTo bezierTo)
{
    CGPathAddCurveToPoint(ensureMutablePlatformPath(), nullptr, bezierTo.controlPoint1.x(), bezierTo.controlPoint1.y(), bezierTo.controlPoint2.x(), bezierTo.controlPoint2.y(), bezierTo.endPoint.x(), bezierTo.endPoint.y());
}

static inline void addToCGContextPath(CGContextRef context, PathBezierCurveTo segment)
{
    CGContextAddCurveToPoint(context, segment.controlPoint1.x(), segment.controlPoint1.y(), segment.controlPoint2.x(), segment.controlPoint2.y(), segment.endPoint.x(), segment.endPoint.y());
}

void PathCG::add(PathArcTo arcTo)
{
    CGPathAddArcToPoint(ensureMutablePlatformPath(), nullptr, arcTo.controlPoint1.x(), arcTo.controlPoint1.y(), arcTo.controlPoint2.x(), arcTo.controlPoint2.y(), arcTo.radius);
}

static inline void addToCGContextPath(CGContextRef context, PathArcTo segment)
{
    CGContextAddArcToPoint(context, segment.controlPoint1.x(), segment.controlPoint1.y(), segment.controlPoint2.x(), segment.controlPoint2.y(), segment.radius);
}

void PathCG::add(PathArc arc)
{
    // CG's coordinate system increases the angle in the anticlockwise direction.
    CGPathAddArc(ensureMutablePlatformPath(), nullptr, arc.center.x(), arc.center.y(), arc.radius, arc.startAngle, arc.endAngle, arc.direction == RotationDirection::Counterclockwise);
}

static inline void addToCGContextPath(CGContextRef context, PathArc segment)
{
    CGContextAddArc(context, segment.center.x(), segment.center.y(), segment.radius, segment.startAngle, segment.endAngle, segment.direction == RotationDirection::Counterclockwise);
}

void PathCG::add(PathClosedArc closedArc)
{
    add(closedArc.arc);
    add(PathCloseSubpath());
}

static inline void addToCGContextPath(CGContextRef context, PathClosedArc segment)
{
    addToCGContextPath(context, segment.arc);
    CGContextClosePath(context);
}

static inline AffineTransform ellipseTransform(const PathEllipse& ellipse)
{
    AffineTransform transform;
    transform.translate(ellipse.center.x(), ellipse.center.y()).rotateRadians(ellipse.rotation).scale(ellipse.radiusX, ellipse.radiusY);
    return transform;
}

void PathCG::add(PathEllipse ellipse)
{
    CGAffineTransform cgTransform = ellipseTransform(ellipse);
    // CG coordinates system increases the angle in the anticlockwise direction.
    CGPathAddArc(ensureMutablePlatformPath(), &cgTransform, 0, 0, 1, ellipse.startAngle, ellipse.endAngle, ellipse.direction == RotationDirection::Counterclockwise);
}

static inline void addToCGContextPath(CGContextRef context, PathEllipse segment)
{
    CGAffineTransform oldTransform = CGContextGetCTM(context);
    CGContextConcatCTM(context, ellipseTransform(segment));
    // CG coordinates system increases the angle in the anticlockwise direction.
    CGContextAddArc(context, 0, 0, 1, segment.startAngle, segment.endAngle, segment.direction == RotationDirection::Counterclockwise);
    CGContextSetCTM(context, oldTransform);
}

void PathCG::add(PathEllipseInRect ellipseInRect)
{
    CGPathAddEllipseInRect(ensureMutablePlatformPath(), nullptr, ellipseInRect.rect);
}

static inline void addToCGContextPath(CGContextRef context, PathEllipseInRect segment)
{
    CGContextAddEllipseInRect(context, segment.rect);
}

void PathCG::add(PathRect rect)
{
    CGPathAddRect(ensureMutablePlatformPath(), nullptr, rect.rect);
}

static inline void addToCGContextPath(CGContextRef context, PathRect segment)
{
    CGContextAddRect(context, segment.rect);
}

static void addEvenCornersRoundedRect(PlatformPathPtr platformPath, const FloatRect& rect, const FloatSize& radius)
{
    // Ensure that CG can render the rounded rect.
    CGFloat radiusWidth = radius.width();
    CGFloat radiusHeight = radius.height();
    CGRect rectToDraw = rect;

    CGFloat rectWidth = CGRectGetWidth(rectToDraw);
    CGFloat rectHeight = CGRectGetHeight(rectToDraw);
    if (2 * radiusWidth > rectWidth)
        radiusWidth = rectWidth / 2 - std::numeric_limits<CGFloat>::epsilon();
    if (2 * radiusHeight > rectHeight)
        radiusHeight = rectHeight / 2 - std::numeric_limits<CGFloat>::epsilon();
    CGPathAddRoundedRect(platformPath, nullptr, rectToDraw, radiusWidth, radiusHeight);
}

#if HAVE(CG_PATH_UNEVEN_CORNERS_ROUNDEDRECT)
static void addUnevenCornersRoundedRect(PlatformPathPtr platformPath, const FloatRoundedRect& roundedRect)
{
    enum Corners {
        BottomLeft,
        BottomRight,
        TopRight,
        TopLeft
    };

    CGSize corners[4] = {
        roundedRect.radii().bottomLeft(),
        roundedRect.radii().bottomRight(),
        roundedRect.radii().topRight(),
        roundedRect.radii().topLeft()
    };

    CGRect rectToDraw = roundedRect.rect();
    CGFloat rectWidth = CGRectGetWidth(rectToDraw);
    CGFloat rectHeight = CGRectGetHeight(rectToDraw);

    // Clamp the radii after conversion to CGFloats.
    corners[TopRight].width = std::min(corners[TopRight].width, rectWidth - corners[TopLeft].width);
    corners[BottomRight].width = std::min(corners[BottomRight].width, rectWidth - corners[BottomLeft].width);
    corners[BottomLeft].height = std::min(corners[BottomLeft].height, rectHeight - corners[TopLeft].height);
    corners[BottomRight].height = std::min(corners[BottomRight].height, rectHeight - corners[TopRight].height);

    CGPathAddUnevenCornersRoundedRect(platformPath, nullptr, rectToDraw, corners);
}
#endif

void PathCG::add(PathRoundedRect roundedRect)
{
    if (roundedRect.strategy == PathRoundedRect::Strategy::PreferNative) {
        const auto& radii = roundedRect.roundedRect.radii();

        if (radii.hasEvenCorners()) {
            addEvenCornersRoundedRect(ensureMutablePlatformPath(), roundedRect.roundedRect.rect(), radii.topLeft());
            return;
        }

#if HAVE(CG_PATH_UNEVEN_CORNERS_ROUNDEDRECT)
        addUnevenCornersRoundedRect(ensureMutablePlatformPath(), roundedRect.roundedRect);
        return;
#endif
    }

    addBeziersForRoundedRect(roundedRect.roundedRect);
}

static inline void addToCGContextPath(CGContextRef context, PathRoundedRect segment)
{
    // No API to add rounded rects to context.
    auto path = PathCG::create();
    path->add(WTFMove(segment));
    // CGContextAddPath has a bug with existing MoveToPoints in context path.
    // rdar://118395262
    auto ctm = CGContextGetCTM(context);
    auto transformedPath = adoptCF(CGPathCreateCopyByTransformingPath(path->platformPath(), &ctm));
    CGContextSetCTM(context, CGAffineTransformIdentity);
    CGContextAddPath(context, transformedPath.get());
    CGContextSetCTM(context, ctm);
}

void PathCG::add(PathCloseSubpath)
{
    CGPathCloseSubpath(ensureMutablePlatformPath());
}

static inline void addToCGContextPath(CGContextRef context, PathCloseSubpath)
{
    CGContextClosePath(context);
}

void PathCG::addPath(const PathCG& path, const AffineTransform& transform)
{
    CGAffineTransform transformCG = transform;

    // CG doesn't allow adding a path to itself. Optimize for the common case
    // and copy the path for the self referencing case.
    if (platformPath() != path.platformPath()) {
        CGPathAddPath(ensureMutablePlatformPath(), &transformCG, path.platformPath());
        return;
    }

    auto pathCopy = adoptCF(CGPathCreateCopy(path.platformPath()));
    CGPathAddPath(ensureMutablePlatformPath(), &transformCG, pathCopy.get());
}

static void pathElementApplierCallback(void* info, const CGPathElement* element)
{
    const auto& applier = *(PathElementApplier*)info;
    auto* cgPoints = element->points;

    switch (element->type) {
    case kCGPathElementMoveToPoint:
        applier({ PathElement::Type::MoveToPoint, { cgPoints[0] } });
        break;

    case kCGPathElementAddLineToPoint:
        applier({ PathElement::Type::AddLineToPoint, { cgPoints[0] } });
        break;

    case kCGPathElementAddQuadCurveToPoint:
        applier({ PathElement::Type::AddQuadCurveToPoint, { cgPoints[0], cgPoints[1] } });
        break;

    case kCGPathElementAddCurveToPoint:
        applier({ PathElement::Type::AddCurveToPoint, { cgPoints[0], cgPoints[1], cgPoints[2] } });
        break;

    case kCGPathElementCloseSubpath:
        applier({ PathElement::Type::CloseSubpath, { } });
        break;
    }
}

bool PathCG::applyElements(const PathElementApplier& applier) const
{
    CGPathApply(platformPath(), (void*)&applier, pathElementApplierCallback);
    return true;
}

bool PathCG::isEmpty() const
{
    return CGPathIsEmpty(platformPath());
}

FloatPoint PathCG::currentPoint() const
{
    return CGPathGetCurrentPoint(platformPath());
}

bool PathCG::transform(const AffineTransform& transform)
{
    CGAffineTransform transformCG = transform;
    m_platformPath = adoptCF(CGPathCreateMutableCopyByTransformingPath(platformPath(), &transformCG));
    return true;
}

static void copyClosingSubpathsApplierFunction(void* info, const CGPathElement* element)
{
    CGMutablePathRef path = static_cast<CGMutablePathRef>(info);
    CGPoint* points = element->points;

    switch (element->type) {
    case kCGPathElementMoveToPoint:
        if (!CGPathIsEmpty(path)) // to silence a warning when trying to close an empty path
            CGPathCloseSubpath(path); // This is the only change from CGPathCreateMutableCopy
        CGPathMoveToPoint(path, 0, points[0].x, points[0].y);
        break;
    case kCGPathElementAddLineToPoint:
        CGPathAddLineToPoint(path, 0, points[0].x, points[0].y);
        break;
    case kCGPathElementAddQuadCurveToPoint:
        CGPathAddQuadCurveToPoint(path, 0, points[0].x, points[0].y, points[1].x, points[1].y);
        break;
    case kCGPathElementAddCurveToPoint:
        CGPathAddCurveToPoint(path, 0, points[0].x, points[0].y, points[1].x, points[1].y, points[2].x, points[2].y);
        break;
    case kCGPathElementCloseSubpath:
        CGPathCloseSubpath(path);
        break;
    }
}

static RetainPtr<CGMutablePathRef> copyCGPathClosingSubpaths(CGPathRef originalPath)
{
    auto path = adoptCF(CGPathCreateMutable());
    CGPathApply(originalPath, path.get(), copyClosingSubpathsApplierFunction);
    CGPathCloseSubpath(path.get());
    return path;
}

bool PathCG::contains(const FloatPoint &point, WindRule rule) const
{
    if (isEmpty())
        return false;

    if (!fastBoundingRect().contains(point))
        return false;

    // CGPathContainsPoint returns false for non-closed paths, as a work-around, we copy
    // and close the path first. Radar 4758998 asks for a better CG API to use.
    auto path = copyCGPathClosingSubpaths(platformPath());
    return CGPathContainsPoint(path.get(), nullptr, point, rule == WindRule::EvenOdd);
}

static size_t putBytesNowhere(void*, const void*, size_t count)
{
    return count;
}

static RetainPtr<CGContextRef> createScratchContext()
{
    CGDataConsumerCallbacks callbacks = { putBytesNowhere, 0 };
    auto consumer = adoptCF(CGDataConsumerCreate(0, &callbacks));
    auto context = adoptCF(CGPDFContextCreate(consumer.get(), 0, 0));

    CGFloat black[4] = { 0, 0, 0, 1 };
    CGContextSetFillColor(context.get(), black);
    CGContextSetStrokeColor(context.get(), black);

    return context;
}

static inline CGContextRef scratchContext()
{
    static NeverDestroyed<RetainPtr<CGContextRef>> context = createScratchContext();
    return context.get().get();
}

bool PathCG::strokeContains(const FloatPoint& point, const Function<void(GraphicsContext&)>& strokeStyleApplier) const
{
    ASSERT(strokeStyleApplier);

    if (isEmpty())
        return false;

    CGContextRef context = scratchContext();

    CGContextSaveGState(context);
    CGContextBeginPath(context);
    CGContextAddPath(context, platformPath());

    GraphicsContextCG graphicsContext(context);
    strokeStyleApplier(graphicsContext);

    bool hitSuccess = CGContextPathContainsPoint(context, point, kCGPathStroke);
    CGContextRestoreGState(context);

    return hitSuccess;
}

static inline FloatRect zeroRectIfNull(CGRect rect)
{
    if (CGRectIsNull(rect))
        return { };
    return rect;
}

FloatRect PathCG::fastBoundingRect() const
{
    return zeroRectIfNull(CGPathGetBoundingBox(platformPath()));
}

FloatRect PathCG::boundingRect() const
{
    // CGPathGetBoundingBox includes the path's control points, CGPathGetPathBoundingBox does not.
    return zeroRectIfNull(CGPathGetPathBoundingBox(platformPath()));
}

FloatRect PathCG::strokeBoundingRect(const Function<void(GraphicsContext&)>& strokeStyleApplier) const
{
    if (isEmpty())
        return { };

    CGContextRef context = scratchContext();

    CGContextSaveGState(context);
    CGContextBeginPath(context);
    CGContextAddPath(context, platformPath());

    if (strokeStyleApplier) {
        GraphicsContextCG graphicsContext(context);
        strokeStyleApplier(graphicsContext);
    }

    CGContextReplacePathWithStrokedPath(context);
    CGRect box = CGContextIsPathEmpty(context) ? CGRectZero : CGContextGetPathBoundingBox(context);
    CGContextRestoreGState(context);

    return zeroRectIfNull(box);
}

static inline void addToCGContextPath(CGContextRef context, PathDataLine segment)
{
    addToCGContextPath(context, PathMoveTo { segment.start });
    addToCGContextPath(context, PathLineTo { segment.end });
}

static inline void addToCGContextPath(CGContextRef context, PathDataQuadCurve segment)
{
    addToCGContextPath(context, PathMoveTo { segment.start });
    addToCGContextPath(context, PathQuadCurveTo { segment.controlPoint, segment.endPoint });
}

static inline void addToCGContextPath(CGContextRef context, PathDataBezierCurve segment)
{
    addToCGContextPath(context, PathMoveTo { segment.start });
    addToCGContextPath(context, PathBezierCurveTo { segment.controlPoint1, segment.controlPoint2, segment.endPoint });
}

static inline void addToCGContextPath(CGContextRef context, PathDataArc segment)
{
    addToCGContextPath(context, PathMoveTo { segment.start });
    addToCGContextPath(context, PathArcTo { segment.controlPoint1, segment.controlPoint2, segment.radius });
}

static inline void addToCGContextPath(CGContextRef context, PathSegment anySegment)
{
    WTF::switchOn(WTFMove(anySegment).data(),
        [&](auto&& segment) {
            addToCGContextPath(context, WTFMove(segment));
        });
}

void addToCGContextPath(CGContextRef context, const Path& path)
{
    if (auto* singleSegment = path.singleSegmentIfExists(); LIKELY(singleSegment)) {
        addToCGContextPath(context, *singleSegment);
        return;
    }
    if (auto* segments = path.segmentsIfExists(); LIKELY(segments)) {
        for (auto& segment : *segments)
            addToCGContextPath(context, segment);
        return;
    }
    CGContextAddPath(context, path.platformPath());
}

} // namespace WebCore

#endif // USE(CG)
