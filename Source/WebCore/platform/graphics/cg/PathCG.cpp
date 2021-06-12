/*
 * Copyright (C) 2003, 2006 Apple Inc.  All rights reserved.
 *                     2006, 2008 Rob Buis <buis@kde.org>
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
#include "Path.h"

#if USE(CG)

#include "AffineTransform.h"
#include "FloatRect.h"
#include "GraphicsContextCG.h"
#include "IntRect.h"
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <wtf/MathExtras.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

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

Path Path::polygonPathFromPoints(const Vector<FloatPoint>& points)
{
    Path path;
    if (points.size() < 2)
        return path;

    Vector<CGPoint, 32> cgPoints;
    cgPoints.reserveInitialCapacity(points.size());
    for (size_t i = 0; i < points.size(); ++i)
        cgPoints.uncheckedAppend(points[i]);

    CGPathAddLines(path.ensurePlatformPath(), nullptr, cgPoints.data(), cgPoints.size());
    path.closeSubpath();
    return path;
}

void Path::createCGPath() const
{
    if (m_path)
        return;

    m_path = adoptCF(CGPathCreateMutable());

    WTF::switchOn(m_inlineData,
        [&](Monostate) { }, // Start with an empty path.
        [&](const MoveData& move) {
            CGPathMoveToPoint(m_path.get(), nullptr, move.location.x(), move.location.y());
        },
        [&](const LineData& line) {
            CGPathMoveToPoint(m_path.get(), nullptr, line.start.x(), line.start.y());
            CGPathAddLineToPoint(m_path.get(), nullptr, line.end.x(), line.end.y());
        },
        [&](const ArcData& arc) {
            if (arc.type == ArcData::Type::LineAndArc || arc.type == ArcData::Type::ClosedLineAndArc)
                CGPathMoveToPoint(m_path.get(), nullptr, arc.start.x(), arc.start.y());
            CGPathAddArc(m_path.get(), nullptr, arc.center.x(), arc.center.y(), arc.radius, arc.startAngle, arc.endAngle, arc.clockwise);
            if (arc.type == ArcData::Type::ClosedLineAndArc)
                CGPathAddLineToPoint(m_path.get(), nullptr, arc.start.x(), arc.start.y());
        },
        [&](const QuadCurveData& curve) {
            CGPathMoveToPoint(m_path.get(), nullptr, curve.startPoint.x(), curve.startPoint.y());
            CGPathAddQuadCurveToPoint(m_path.get(), nullptr, curve.controlPoint.x(), curve.controlPoint.y(), curve.endPoint.x(), curve.endPoint.y());
        },
        [&](const BezierCurveData& curve) {
            CGPathMoveToPoint(m_path.get(), nullptr, curve.startPoint.x(), curve.startPoint.y());
            CGPathAddCurveToPoint(m_path.get(), nullptr, curve.controlPoint1.x(), curve.controlPoint1.y(), curve.controlPoint2.x(), curve.controlPoint2.y(), curve.endPoint.x(), curve.endPoint.y());
        }
    );
}

Path::Path(RetainPtr<CGMutablePathRef>&& path)
    : m_path(WTFMove(path))
{
}

Path::Path() = default;
Path::~Path() = default;

PlatformPathPtr Path::platformPath() const
{
    if (!m_path && hasInlineData())
        createCGPath();
    return m_path.get();
}

PlatformPathPtr Path::ensurePlatformPath()
{
    createCGPath();
    if (m_copyPathBeforeMutation) {
        if (CFGetRetainCount(m_path.get()) > 1)
            m_path = adoptCF(CGPathCreateMutableCopy(m_path.get()));
        m_copyPathBeforeMutation = false;
    }
    m_inlineData = Monostate { };
    return m_path.get();
}

bool Path::isNull() const
{
    return !m_path && !hasInlineData();
}

Path::Path(const Path& other)
{
    m_path = { other.m_path };
    m_inlineData = other.m_inlineData;
    if (m_path) {
        m_copyPathBeforeMutation = true;
        other.m_copyPathBeforeMutation = true;
    }
}

Path::Path(Path&& other)
    : m_path(std::exchange(other.m_path, nullptr))
    , m_inlineData(std::exchange(other.m_inlineData, Monostate { }))
    , m_copyPathBeforeMutation(std::exchange(other.m_copyPathBeforeMutation, false))
{
}

void Path::swap(Path& otherPath)
{
    std::swap(m_path, otherPath.m_path);
    std::swap(m_inlineData, otherPath.m_inlineData);
    std::swap(m_copyPathBeforeMutation, otherPath.m_copyPathBeforeMutation);
}

Path& Path::operator=(const Path& other)
{
    Path copy { other };
    swap(copy);
    return *this;
}

Path& Path::operator=(Path&& other)
{
    Path copy { WTFMove(other) };
    swap(copy);
    return *this;
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

bool Path::contains(const FloatPoint &point, WindRule rule) const
{
    if (isNull())
        return false;

    if (!fastBoundingRect().contains(point))
        return false;

    // CGPathContainsPoint returns false for non-closed paths, as a work-around, we copy and close the path first.  Radar 4758998 asks for a better CG API to use
    auto path = copyCGPathClosingSubpaths(platformPath());
    return CGPathContainsPoint(path.get(), nullptr, point, rule == WindRule::EvenOdd);
}

bool Path::strokeContains(const FloatPoint& point, const Function<void(GraphicsContext&)>& strokeStyleApplier) const
{
    ASSERT(strokeStyleApplier);

    if (isNull())
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

void Path::translate(const FloatSize& size)
{
    transform(AffineTransform(1, 0, 0, 1, size.width(), size.height()));
}

void Path::transform(const AffineTransform& transform)
{
    if (transform.isIdentity() || isEmpty())
        return;

    CGAffineTransform transformCG = transform;
#if PLATFORM(WIN)
    auto path = adoptCF(CGPathCreateMutable());
    CGPathAddPath(path.get(), &transformCG, platformPath());
#else
    auto path = adoptCF(CGPathCreateMutableCopyByTransformingPath(platformPath(), &transformCG));
#endif
    m_path = WTFMove(path);
    m_copyPathBeforeMutation = false;
    m_inlineData = Monostate { };
}

static inline FloatRect zeroRectIfNull(CGRect rect)
{
    if (CGRectIsNull(rect))
        return { };
    return rect;
}

FloatRect Path::boundingRectSlowCase() const
{
    // CGPathGetBoundingBox includes the path's control points, CGPathGetPathBoundingBox does not.
    return zeroRectIfNull(CGPathGetPathBoundingBox(platformPath()));
}

FloatRect Path::fastBoundingRectSlowCase() const
{
    return zeroRectIfNull(CGPathGetBoundingBox(platformPath()));
}

FloatRect Path::strokeBoundingRect(const Function<void(GraphicsContext&)>& strokeStyleApplier) const
{
    if (isNull())
        return CGRectZero;

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

    return CGRectIsNull(box) ? CGRectZero : box;
}

void Path::moveToSlowCase(const FloatPoint& point)
{
    CGPathMoveToPoint(ensurePlatformPath(), nullptr, point.x(), point.y());
}

void Path::addLineToSlowCase(const FloatPoint& p)
{
    CGPathAddLineToPoint(ensurePlatformPath(), nullptr, p.x(), p.y());
}

void Path::addQuadCurveToSlowCase(const FloatPoint& cp, const FloatPoint& p)
{
    CGPathAddQuadCurveToPoint(ensurePlatformPath(), nullptr, cp.x(), cp.y(), p.x(), p.y());
}

void Path::addBezierCurveToSlowCase(const FloatPoint& cp1, const FloatPoint& cp2, const FloatPoint& p)
{
    CGPathAddCurveToPoint(ensurePlatformPath(), nullptr, cp1.x(), cp1.y(), cp2.x(), cp2.y(), p.x(), p.y());
}

void Path::addArcTo(const FloatPoint& p1, const FloatPoint& p2, float radius)
{
    CGPathAddArcToPoint(ensurePlatformPath(), nullptr, p1.x(), p1.y(), p2.x(), p2.y(), radius);
}

void Path::platformAddPathForRoundedRect(const FloatRect& rect, const FloatSize& topLeftRadius, const FloatSize& topRightRadius, const FloatSize& bottomLeftRadius, const FloatSize& bottomRightRadius)
{
#if PLATFORM(COCOA)
    bool equalWidths = (topLeftRadius.width() == topRightRadius.width() && topRightRadius.width() == bottomLeftRadius.width() && bottomLeftRadius.width() == bottomRightRadius.width());
    bool equalHeights = (topLeftRadius.height() == bottomLeftRadius.height() && bottomLeftRadius.height() == topRightRadius.height() && topRightRadius.height() == bottomRightRadius.height());

    if (equalWidths && equalHeights) {
        // Ensure that CG can render the rounded rect.
        CGFloat radiusWidth = topLeftRadius.width();
        CGFloat radiusHeight = topLeftRadius.height();
        CGRect rectToDraw = rect;
        CGFloat rectWidth = CGRectGetWidth(rectToDraw);
        CGFloat rectHeight = CGRectGetHeight(rectToDraw);
        if (2 * radiusWidth > rectWidth)
            radiusWidth = rectWidth / 2 - std::numeric_limits<CGFloat>::epsilon();
        if (2 * radiusHeight > rectHeight)
            radiusHeight = rectHeight / 2 - std::numeric_limits<CGFloat>::epsilon();
        CGPathAddRoundedRect(ensurePlatformPath(), nullptr, rectToDraw, radiusWidth, radiusHeight);
        return;
    }

#if HAVE(CG_PATH_UNEVEN_CORNERS_ROUNDEDRECT)
    CGRect rectToDraw = rect;
    
    enum Corners {
        BottomLeft,
        BottomRight,
        TopRight,
        TopLeft
    };
    CGSize corners[4] = { bottomLeftRadius, bottomRightRadius, topRightRadius, topLeftRadius };

    CGFloat rectWidth = CGRectGetWidth(rectToDraw);
    CGFloat rectHeight = CGRectGetHeight(rectToDraw);
    
    // Clamp the radii after conversion to CGFloats.
    corners[TopRight].width = std::min(corners[TopRight].width, rectWidth - corners[TopLeft].width);
    corners[BottomRight].width = std::min(corners[BottomRight].width, rectWidth - corners[BottomLeft].width);
    corners[BottomLeft].height = std::min(corners[BottomLeft].height, rectHeight - corners[TopLeft].height);
    corners[BottomRight].height = std::min(corners[BottomRight].height, rectHeight - corners[TopRight].height);

    CGPathAddUnevenCornersRoundedRect(ensurePlatformPath(), nullptr, rectToDraw, corners);
    return;
#endif
#endif

    addBeziersForRoundedRect(rect, topLeftRadius, topRightRadius, bottomLeftRadius, bottomRightRadius);
}

void Path::closeSubpath()
{
    // FIXME: Unclear if close commands should have meaning for a null path.
    if (isNull())
        return;

    CGPathCloseSubpath(ensurePlatformPath());
}

void Path::addArcSlowCase(const FloatPoint& p, float radius, float startAngle, float endAngle, bool clockwise)
{
    CGPathAddArc(ensurePlatformPath(), nullptr, p.x(), p.y(), radius, startAngle, endAngle, clockwise);
}

void Path::addRect(const FloatRect& r)
{
    CGPathAddRect(ensurePlatformPath(), 0, r);
}

void Path::addEllipse(FloatPoint p, float radiusX, float radiusY, float rotation, float startAngle, float endAngle, bool anticlockwise)
{
    AffineTransform transform;
    transform.translate(p.x(), p.y()).rotate(rad2deg(rotation)).scale(radiusX, radiusY);

    CGAffineTransform cgTransform = transform;
    CGPathAddArc(ensurePlatformPath(), &cgTransform, 0, 0, 1, startAngle, endAngle, anticlockwise);
}

void Path::addEllipse(const FloatRect& r)
{
    CGPathAddEllipseInRect(ensurePlatformPath(), 0, r);
}

void Path::addPath(const Path& path, const AffineTransform& transform)
{
    if (!path.platformPath())
        return;

    if (!transform.isInvertible())
        return;

    CGAffineTransform transformCG = transform;
    // CG doesn't allow adding a path to itself. Optimize for the common case
    // and copy the path for the self referencing case.
    if (ensurePlatformPath() != path.platformPath()) {
        CGPathAddPath(ensurePlatformPath(), &transformCG, path.platformPath());
        return;
    }
    auto pathCopy = adoptCF(CGPathCreateCopy(path.platformPath()));
    CGPathAddPath(ensurePlatformPath(), &transformCG, pathCopy.get());
}

void Path::clear()
{
    if (isNull())
        return;

    m_path.clear();
    m_inlineData = Monostate { };
    m_copyPathBeforeMutation = false;
}

bool Path::isEmptySlowCase() const
{
    return CGPathIsEmpty(m_path.get());
}

FloatPoint Path::currentPointSlowCase() const
{
    return CGPathGetCurrentPoint(platformPath());
}

static void CGPathApplierToPathApplier(void* info, const CGPathElement* element)
{
    const PathApplierFunction& function = *(PathApplierFunction*)info;
    PathElement pathElement;
    pathElement.type = (PathElement::Type)element->type;
    CGPoint* cgPoints = element->points;
    switch (element->type) {
    case kCGPathElementMoveToPoint:
    case kCGPathElementAddLineToPoint:
        pathElement.points[0] = cgPoints[0];
        break;
    case kCGPathElementAddQuadCurveToPoint:
        pathElement.points[0] = cgPoints[0];
        pathElement.points[1] = cgPoints[1];
        break;
    case kCGPathElementAddCurveToPoint:
        pathElement.points[0] = cgPoints[0];
        pathElement.points[1] = cgPoints[1];
        pathElement.points[2] = cgPoints[2];
        break;
    case kCGPathElementCloseSubpath:
        break;
    }
    function(pathElement);
}

void Path::applySlowCase(const PathApplierFunction& function) const
{
    CGPathApply(platformPath(), (void*)&function, CGPathApplierToPathApplier);
}

#if HAVE(CGPATH_GET_NUMBER_OF_ELEMENTS)

size_t Path::elementCountSlowCase() const
{
    return CGPathGetNumberOfElements(platformPath());
}

#endif // HAVE(CGPATH_GET_NUMBER_OF_ELEMENTS)

}

#endif // USE(CG)
