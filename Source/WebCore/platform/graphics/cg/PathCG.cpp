/*
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
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

#include "CGUtilities.h"
#include "GraphicsContextCG.h"
#include "PathStream.h"
#include <mutex>
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMallocInlines.h>
namespace WebCore {

// Below contains two implementations per path element implementation:
// the member to add the path element to the PathCG, and correspoding implementation
// for adding path segment directly to the CGContext. Keep these in sync.

static void addToCGPath(CGMutablePathRef, const PathSegment&);

static inline void addToCGPath(CGMutablePathRef path, const PathCloseSubpath&)
{
    CGPathCloseSubpath(path);
}

static inline void addToCGContextPath(CGContextRef context, const PathCloseSubpath&)
{
    CGContextClosePath(context);
}

static inline void addToCGPath(CGMutablePathRef path, const PathMoveTo& moveTo)
{
    CGPathMoveToPoint(path, nullptr, moveTo.point.x(), moveTo.point.y());
}

static inline void addToCGContextPath(CGContextRef context, const PathMoveTo& segment)
{
    CGContextMoveToPoint(context, segment.point.x(), segment.point.y());
}

static inline void addToCGPath(CGMutablePathRef path, const PathLineTo& lineTo)
{
    CGPathAddLineToPoint(path, nullptr, lineTo.point.x(), lineTo.point.y());
}

static inline void addToCGContextPath(CGContextRef context, const PathLineTo& segment)
{
    CGContextAddLineToPoint(context, segment.point.x(), segment.point.y());
}

static inline void addToCGPath(CGMutablePathRef path, const PathQuadCurveTo& quadTo)
{
    CGPathAddQuadCurveToPoint(path, nullptr, quadTo.controlPoint.x(), quadTo.controlPoint.y(), quadTo.endPoint.x(), quadTo.endPoint.y());
}

static inline void addToCGContextPath(CGContextRef context, const PathQuadCurveTo& segment)
{
    CGContextAddQuadCurveToPoint(context, segment.controlPoint.x(), segment.controlPoint.y(), segment.endPoint.x(), segment.endPoint.y());
}

static inline void addToCGPath(CGMutablePathRef path, const PathBezierCurveTo& bezierTo)
{
    CGPathAddCurveToPoint(path, nullptr, bezierTo.controlPoint1.x(), bezierTo.controlPoint1.y(), bezierTo.controlPoint2.x(), bezierTo.controlPoint2.y(), bezierTo.endPoint.x(), bezierTo.endPoint.y());
}

static inline void addToCGContextPath(CGContextRef context, const PathBezierCurveTo& segment)
{
    CGContextAddCurveToPoint(context, segment.controlPoint1.x(), segment.controlPoint1.y(), segment.controlPoint2.x(), segment.controlPoint2.y(), segment.endPoint.x(), segment.endPoint.y());
}

static inline void addToCGPath(CGMutablePathRef path, const PathArcTo& arcTo)
{
    CGPathAddArcToPoint(path, nullptr, arcTo.controlPoint1.x(), arcTo.controlPoint1.y(), arcTo.controlPoint2.x(), arcTo.controlPoint2.y(), arcTo.radius);
}

static inline void addToCGContextPath(CGContextRef context, const PathArcTo& segment)
{
    CGContextAddArcToPoint(context, segment.controlPoint1.x(), segment.controlPoint1.y(), segment.controlPoint2.x(), segment.controlPoint2.y(), segment.radius);
}

static inline void addToCGPath(CGMutablePathRef path, const PathArc& arc)
{
    // CG's coordinate system increases the angle in the anticlockwise direction.
    CGPathAddArc(path, nullptr, arc.center.x(), arc.center.y(), arc.radius, arc.startAngle, arc.endAngle, arc.direction == RotationDirection::Counterclockwise);
}

static inline void addToCGContextPath(CGContextRef context, const PathArc& segment)
{
    CGContextAddArc(context, segment.center.x(), segment.center.y(), segment.radius, segment.startAngle, segment.endAngle, segment.direction == RotationDirection::Counterclockwise);
}

static inline void addToCGPath(CGMutablePathRef path, const PathClosedArc& closedArc)
{
    addToCGPath(path, closedArc.arc);
    addToCGPath(path, PathCloseSubpath());
}

static inline void addToCGContextPath(CGContextRef context, const PathClosedArc& segment)
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

static inline void addToCGPath(CGMutablePathRef path, const PathEllipse& ellipse)
{
    CGAffineTransform cgTransform = ellipseTransform(ellipse);
    // CG coordinates system increases the angle in the anticlockwise direction.
    CGPathAddArc(path, &cgTransform, 0, 0, 1, ellipse.startAngle, ellipse.endAngle, ellipse.direction == RotationDirection::Counterclockwise);
}

static inline void addToCGContextPath(CGContextRef context, const PathEllipse& segment)
{
    CGAffineTransform oldTransform = CGContextGetCTM(context);
    CGContextConcatCTM(context, ellipseTransform(segment));
    // CG coordinates system increases the angle in the anticlockwise direction.
    CGContextAddArc(context, 0, 0, 1, segment.startAngle, segment.endAngle, segment.direction == RotationDirection::Counterclockwise);
    CGContextSetCTM(context, oldTransform);
}

static inline void addToCGPath(CGMutablePathRef path, const PathEllipseInRect& ellipseInRect)
{
    CGPathAddEllipseInRect(path, nullptr, ellipseInRect.rect);
}

static inline void addToCGContextPath(CGContextRef context, const PathEllipseInRect& segment)
{
    CGContextAddEllipseInRect(context, segment.rect);
}

static inline void addToCGPath(CGMutablePathRef path, const PathRect& rect)
{
    CGPathAddRect(path, nullptr, rect.rect);
}

static inline void addToCGContextPath(CGContextRef context, const PathRect& segment)
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
static void addUnevenCornersRoundedRect(CGMutablePathRef platformPath, const FloatRoundedRect& roundedRect)
{
    enum Corners {
        BottomLeft,
        BottomRight,
        TopRight,
        TopLeft
    };

    std::array<CGSize, 4> corners {
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

    CGPathAddUnevenCornersRoundedRect(platformPath, nullptr, rectToDraw, corners.data());
}
#endif

static inline void addToCGPath(CGMutablePathRef path, const PathRoundedRect& roundedRect)
{
    if (roundedRect.strategy == PathRoundedRect::Strategy::PreferNative) {
        const auto& radii = roundedRect.roundedRect.radii();

        if (radii.hasEvenCorners()) {
            addEvenCornersRoundedRect(path, roundedRect.roundedRect.rect(), radii.topLeft());
            return;
        }

#if HAVE(CG_PATH_UNEVEN_CORNERS_ROUNDEDRECT)
        addUnevenCornersRoundedRect(path, roundedRect.roundedRect);
        return;
#endif
    }

    for (auto& segment : PathImpl::beziersForRoundedRect(roundedRect.roundedRect))
        addToCGPath(path, segment);
}

static inline void addToCGPath(CGMutablePathRef path, const PathContinuousRoundedRect& continuousRoundedRect)
{
#if HAVE(CG_PATH_CONTINUOUS_ROUNDED_RECT)
    CGPathAddContinuousRoundedRect(path, nullptr, continuousRoundedRect.rect, continuousRoundedRect.cornerWidth, continuousRoundedRect.cornerHeight);
#else
    // Continuous rounded rects are unavailable. Paint a normal rounded rect instead.
    // FIXME: Determine if PreferNative is the optimal strategy here.
    addToCGPath(path, PathRoundedRect { FloatRoundedRect { continuousRoundedRect.rect, FloatRoundedRect::Radii { continuousRoundedRect.cornerWidth, continuousRoundedRect.cornerHeight } }, PathRoundedRect::Strategy::PreferNative });
#endif
}

static inline void addToCGContextPath(CGContextRef context, const PathRoundedRect& segment)
{
    // No API to add rounded rects to context.
    RetainPtr path = adoptCF(CGPathCreateMutable());
    addToCGPath(path.get(), segment);
    // CGContextAddPath has a bug with existing MoveToPoints in context path.
    // rdar://118395262
    auto ctm = CGContextGetCTM(context);
    RetainPtr transformedPath = adoptCF(CGPathCreateCopyByTransformingPath(path.get(), &ctm));
    CGContextSetCTM(context, CGAffineTransformIdentity);
    CGContextAddPath(context, transformedPath.get());
    CGContextSetCTM(context, ctm);
}

static inline void addToCGContextPath(CGContextRef context, const PathContinuousRoundedRect& segment)
{
    // No API to add continuous rounded rects to context.
    RetainPtr path = adoptCF(CGPathCreateMutable());
    addToCGPath(path.get(), segment);
    // CGContextAddPath has a bug with existing MoveToPoints in context path.
    // rdar://118395262
    auto ctm = CGContextGetCTM(context);
    RetainPtr transformedPath = adoptCF(CGPathCreateCopyByTransformingPath(path.get(), &ctm));
    CGContextSetCTM(context, CGAffineTransformIdentity);
    CGContextAddPath(context, transformedPath.get());
    CGContextSetCTM(context, ctm);
}

static inline void addToCGPath(CGMutablePathRef path, const PathDataLine& segment)
{
    addToCGPath(path, PathMoveTo { segment.start() });
    addToCGPath(path, PathLineTo { segment.end() });
}

static inline void addToCGContextPath(CGContextRef context, const PathDataLine& segment)
{
    addToCGContextPath(context, PathMoveTo { segment.start() });
    addToCGContextPath(context, PathLineTo { segment.end() });
}

static inline void addToCGPath(CGMutablePathRef path, const PathDataQuadCurve& segment)
{
    addToCGPath(path, PathMoveTo { segment.start });
    addToCGPath(path, PathQuadCurveTo { segment.controlPoint, segment.endPoint });
}

static inline void addToCGContextPath(CGContextRef context, const PathDataQuadCurve& segment)
{
    addToCGContextPath(context, PathMoveTo { segment.start });
    addToCGContextPath(context, PathQuadCurveTo { segment.controlPoint, segment.endPoint });
}

static inline void addToCGPath(CGMutablePathRef path, const PathDataBezierCurve& segment)
{
    addToCGPath(path, PathMoveTo { segment.start });
    addToCGPath(path, PathBezierCurveTo { segment.controlPoint1, segment.controlPoint2, segment.endPoint });
}

static inline void addToCGContextPath(CGContextRef context, const PathDataBezierCurve& segment)
{
    addToCGContextPath(context, PathMoveTo { segment.start });
    addToCGContextPath(context, PathBezierCurveTo { segment.controlPoint1, segment.controlPoint2, segment.endPoint });
}

static inline void addToCGPath(CGMutablePathRef path, const PathDataArc& segment)
{
    addToCGPath(path, PathMoveTo { segment.start });
    addToCGPath(path, PathArcTo { segment.controlPoint1, segment.controlPoint2, segment.radius });
}

static inline void addToCGContextPath(CGContextRef context, const PathDataArc& segment)
{
    addToCGContextPath(context, PathMoveTo { segment.start });
    addToCGContextPath(context, PathArcTo { segment.controlPoint1, segment.controlPoint2, segment.radius });
}

inline void addToCGPath(CGMutablePathRef path, const PathSegment& anySegment)
{
    WTF::switchOn(anySegment.data(),
        [&](auto&& segment) {
            addToCGPath(path, segment);
        });
}

static inline void addToCGContextPath(CGContextRef context, const PathSegment& anySegment)
{
    WTF::switchOn(anySegment.data(),
        [&](auto&& segment) {
            addToCGContextPath(context, segment);
        });
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(PathCG);

Ref<PathCG> PathCG::create(std::span<const PathSegment> segments)
{
    RetainPtr platformPath = adoptCF(CGPathCreateMutable());
    for (auto& segment : segments)
        addToCGPath(platformPath.get(), segment);
    return PathCG::create(WTFMove(platformPath));
}

Ref<PathCG> PathCG::create(RetainPtr<CGMutablePathRef>&& platformPath)
{
    return adoptRef(*new PathCG(WTFMove(platformPath)));
}

PlatformPathPtr PathCG::emptyPlatformPath()
{
    static LazyNeverDestroyed<RetainPtr<CGMutablePathRef>> emptyPath;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        emptyPath.construct(adoptCF(CGPathCreateMutable()));
    });
    return emptyPath.get().get();
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

void PathCG::add(PathMoveTo moveTo)
{
    addToCGPath(ensureMutablePlatformPath(), moveTo);
}

void PathCG::add(PathLineTo lineTo)
{
    addToCGPath(ensureMutablePlatformPath(), lineTo);
}

void PathCG::add(PathQuadCurveTo quadTo)
{
    addToCGPath(ensureMutablePlatformPath(), quadTo);
}

void PathCG::add(PathBezierCurveTo bezierTo)
{
    addToCGPath(ensureMutablePlatformPath(), bezierTo);
}

void PathCG::add(PathArcTo arcTo)
{
    addToCGPath(ensureMutablePlatformPath(), arcTo);
}

void PathCG::add(PathArc arc)
{
    addToCGPath(ensureMutablePlatformPath(), arc);
}

void PathCG::add(PathClosedArc closedArc)
{
    addToCGPath(ensureMutablePlatformPath(), closedArc);
}

void PathCG::add(PathEllipse ellipse)
{
    addToCGPath(ensureMutablePlatformPath(), ellipse);
}

void PathCG::add(PathEllipseInRect ellipseInRect)
{
    addToCGPath(ensureMutablePlatformPath(), ellipseInRect);
}

void PathCG::add(PathRect rect)
{
    addToCGPath(ensureMutablePlatformPath(), rect);
}

void PathCG::add(PathRoundedRect roundedRect)
{
    addToCGPath(ensureMutablePlatformPath(), roundedRect);
}

void PathCG::add(PathContinuousRoundedRect continuousRoundedRect)
{
    addToCGPath(ensureMutablePlatformPath(), continuousRoundedRect);
}

void PathCG::add(PathCloseSubpath closeSubPath)
{
    addToCGPath(ensureMutablePlatformPath(), closeSubPath);
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
    auto cgPoints = pointsSpan(element);

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
    auto points = pointsSpan(element);

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

bool PathCG::strokeContains(const FloatPoint& point, NOESCAPE const Function<void(GraphicsContext&)>& strokeStyleApplier) const
{
    ASSERT(strokeStyleApplier);

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

FloatRect PathCG::strokeBoundingRect(NOESCAPE const Function<void(GraphicsContext&)>& strokeStyleApplier) const
{
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

void addToCGContextPath(CGContextRef context, const Path& path)
{
    if (auto* singleSegment = path.singleSegmentIfExists(); singleSegment) [[likely]] {
        addToCGContextPath(context, *singleSegment);
        return;
    }
    if (auto* segments = path.segmentsIfExists(); segments) [[likely]] {
        for (auto& segment : *segments)
            addToCGContextPath(context, segment);
        return;
    }
    CGContextAddPath(context, path.platformPath());
}

} // namespace WebCore

#endif // USE(CG)
