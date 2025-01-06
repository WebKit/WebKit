/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "PathSkia.h"

#if USE(SKIA)
#include "GraphicsContextSkia.h"
#include "NotImplemented.h"
#include "PathStream.h"

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkPathUtils.h>
#include <skia/core/SkRRect.h>
#include <skia/core/SkSurface.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

#include <wtf/NeverDestroyed.h>

namespace WebCore {

Ref<PathSkia> PathSkia::create()
{
    return adoptRef(*new PathSkia);
}

Ref<PathSkia> PathSkia::create(const PathSegment& segment)
{
    auto pathSkia = PathSkia::create();
    pathSkia->addSegment(segment);
    return pathSkia;
}

Ref<PathSkia> PathSkia::create(const PathStream& stream)
{
    auto pathSkia = PathSkia::create();
    for (auto& segment : stream.segments())
        pathSkia->addSegment(segment);
    return pathSkia;
}

PathSkia::PathSkia(const SkPath& platformPath)
    : m_platformPath(platformPath)
{
}

bool PathSkia::definitelyEqual(const PathImpl& otherImpl) const
{
    RefPtr otherAsPathSkia = dynamicDowncast<PathSkia>(otherImpl);
    if (!otherAsPathSkia) {
        // We could convert other to a platform path to compare, but that would be expensive.
        return false;
    }

    if (otherAsPathSkia.get() == this)
        return true;

    return m_platformPath == otherAsPathSkia->m_platformPath;
}

Ref<PathImpl> PathSkia::copy() const
{
    return adoptRef(*new PathSkia(m_platformPath));
}

PlatformPathPtr PathSkia::platformPath() const
{
    return const_cast<SkPath*>(&m_platformPath);
}

void PathSkia::add(PathMoveTo moveTo)
{
    m_platformPath.moveTo(SkFloatToScalar(moveTo.point.x()), SkFloatToScalar(moveTo.point.y()));
}

void PathSkia::add(PathLineTo lineTo)
{
    m_platformPath.lineTo(SkFloatToScalar(lineTo.point.x()), SkFloatToScalar(lineTo.point.y()));
}

void PathSkia::add(PathQuadCurveTo quadTo)
{
    m_platformPath.quadTo(SkFloatToScalar(quadTo.controlPoint.x()), SkFloatToScalar(quadTo.controlPoint.y()), SkFloatToScalar(quadTo.endPoint.x()), SkFloatToScalar(quadTo.endPoint.y()));
}

void PathSkia::add(PathBezierCurveTo cubicTo)
{
    m_platformPath.cubicTo(SkFloatToScalar(cubicTo.controlPoint1.x()), SkFloatToScalar(cubicTo.controlPoint1.y()), SkFloatToScalar(cubicTo.controlPoint2.x()), SkFloatToScalar(cubicTo.controlPoint2.y()),
        SkFloatToScalar(cubicTo.endPoint.x()), SkFloatToScalar(cubicTo.endPoint.y()));
}

void PathSkia::add(PathArcTo arcTo)
{
    m_platformPath.arcTo(SkFloatToScalar(arcTo.controlPoint1.x()), SkFloatToScalar(arcTo.controlPoint1.y()), SkFloatToScalar(arcTo.controlPoint2.x()), SkFloatToScalar(arcTo.controlPoint2.y()),
        SkFloatToScalar(arcTo.radius));
}

void PathSkia::addEllipse(const FloatPoint& center, float radiusX, float radiusY, float startAngle, float endAngle, RotationDirection direction)
{
    auto x = SkFloatToScalar(center.x());
    auto y = SkFloatToScalar(center.y());
    auto radiusXScalar = SkFloatToScalar(radiusX);
    auto radiusYScalar = SkFloatToScalar(radiusY);
    SkRect oval = { x - radiusXScalar, y - radiusYScalar, x + radiusXScalar, y + radiusYScalar };

    if (direction == RotationDirection::Clockwise && startAngle > endAngle)
        endAngle = startAngle + (2 * piFloat - fmodf(startAngle - endAngle, 2 * piFloat));
    else if (direction == RotationDirection::Counterclockwise && startAngle < endAngle)
        endAngle = startAngle - (2 * piFloat - fmodf(endAngle - startAngle, 2 * piFloat));

    auto sweepAngle = endAngle - startAngle;
    SkScalar startDegrees = SkFloatToScalar(startAngle * 180 / piFloat);
    SkScalar sweepDegrees = SkFloatToScalar(sweepAngle * 180 / piFloat);

    // SkPath::arcTo can't handle the sweepAngle that is equal to 360, so in those
    // cases we add two arcs with sweepAngle = 180. SkPath::addOval can handle sweepAngle
    // that is 360, but it creates a closed path.
    SkScalar s360 = SkIntToScalar(360);
    if (SkScalarNearlyEqual(sweepDegrees, s360)) {
        SkScalar s180 = SkIntToScalar(180);
        m_platformPath.arcTo(oval, startDegrees, s180, false);
        m_platformPath.arcTo(oval, startDegrees + s180, s180, false);
    } else if (SkScalarNearlyEqual(sweepDegrees, -s360)) {
        SkScalar s180 = SkIntToScalar(180);
        m_platformPath.arcTo(oval, startDegrees, -s180, false);
        m_platformPath.arcTo(oval, startDegrees - s180, -s180, false);
    } else
        m_platformPath.arcTo(oval, startDegrees, sweepDegrees, false);
}

void PathSkia::add(PathArc arc)
{
    addEllipse(arc.center, arc.radius, arc.radius, arc.startAngle, arc.endAngle, arc.direction);
}

void PathSkia::add(PathClosedArc closedArc)
{
    add(closedArc.arc);
    add(PathCloseSubpath());
}

void PathSkia::add(PathEllipse ellipse)
{
    if (!ellipse.rotation) {
        addEllipse(ellipse.center, ellipse.radiusX, ellipse.radiusY, ellipse.startAngle, ellipse.endAngle, ellipse.direction);
        return;
    }

    AffineTransform transform;
    transform.translate(ellipse.center.x(), ellipse.center.y()).rotateRadians(ellipse.rotation);
    auto inverseTransform = transform.inverse().value();
    m_platformPath.transform(inverseTransform, nullptr);
    addEllipse({ }, ellipse.radiusX, ellipse.radiusY, ellipse.startAngle, ellipse.endAngle, ellipse.direction);
    m_platformPath.transform(transform);
}

void PathSkia::add(PathEllipseInRect ellipseInRect)
{
    m_platformPath.addOval(ellipseInRect.rect);
}

void PathSkia::add(PathRect rect)
{
    m_platformPath.addRect(rect.rect);
}

void PathSkia::add(PathRoundedRect roundedRect)
{
    if (roundedRect.strategy == PathRoundedRect::Strategy::PreferNative)
        m_platformPath.addRRect(roundedRect.roundedRect);
    else
        addBeziersForRoundedRect(roundedRect.roundedRect);
}

void PathSkia::add(PathCloseSubpath)
{
    m_platformPath.close();
}

void PathSkia::addPath(const PathSkia& path, const AffineTransform& transform)
{
    m_platformPath.addPath(*path.platformPath(), transform);
}

bool PathSkia::applyElements(const PathElementApplier& applier) const
{
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // GLib/Win port

    auto convertPoints = [](std::span<FloatPoint, 3> dst, const SkPoint src[], int count) {
        for (int i = 0; i < count; ++i) {
            dst[i].setX(SkScalarToFloat(src[i].fX));
            dst[i].setY(SkScalarToFloat(src[i].fY));
        }
    };

    SkPath::RawIter iter(m_platformPath);
    SkPoint skPoints[4];
    PathElement pathElement;
    while (true) {
        switch (iter.next(skPoints)) {
        case SkPath::kMove_Verb:
            pathElement.type = PathElement::Type::MoveToPoint;
            convertPoints(pathElement.points, &skPoints[0], 1);
            break;
        case SkPath::kLine_Verb:
            pathElement.type = PathElement::Type::AddLineToPoint;
            convertPoints(pathElement.points, &skPoints[1], 1);
            break;
        case SkPath::kQuad_Verb:
            pathElement.type = PathElement::Type::AddQuadCurveToPoint;
            convertPoints(pathElement.points, &skPoints[1], 2);
            break;
        case SkPath::kCubic_Verb:
            pathElement.type = PathElement::Type::AddCurveToPoint;
            convertPoints(pathElement.points, &skPoints[1], 3);
            break;
        case SkPath::kConic_Verb: {
            // Approximate conic with quads.
            // The amount of quads can be altered to change the performance/precision tradeoff.
            // At the moment of writing, at least 4 quads are needed to satisfy layout tests.
            pathElement.type = PathElement::Type::AddQuadCurveToPoint;
            const int quadCountLog2 = 2;
            const unsigned quadCount = 1 << quadCountLog2;
            SkPoint quadPoints[1 + 2 * quadCount];
            SkPath::ConvertConicToQuads(skPoints[0], skPoints[1], skPoints[2], iter.conicWeight(), quadPoints, quadCountLog2);
            for (unsigned quadIndex = 0; quadIndex < quadCount; quadIndex++) {
                convertPoints(pathElement.points, &quadPoints[1 + 2 * quadIndex], 2);
                applier(pathElement);
            }
            continue;
        }
        case SkPath::kClose_Verb:
            pathElement.type = PathElement::Type::CloseSubpath;
            break;
        case SkPath::kDone_Verb:
            return true;
        }
        applier(pathElement);
    }
    return true;

    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
}

bool PathSkia::isEmpty() const
{
    return m_platformPath.isEmpty();
}

FloatPoint PathSkia::currentPoint() const
{
    if (m_platformPath.countPoints() > 0) {
        SkPoint lastPoint;
        m_platformPath.getLastPt(&lastPoint);
        return { SkScalarToFloat(lastPoint.fX), SkScalarToFloat(lastPoint.fY) };
    }

    return { };
}

bool PathSkia::transform(const AffineTransform& matrix)
{
    m_platformPath.transform(matrix, nullptr);
    return true;
}

bool PathSkia::contains(const FloatPoint& point, WindRule windRule) const
{
    if (isEmpty() || !std::isfinite(point.x()) || !std::isfinite(point.y()))
        return false;

    auto toSkiaFillType = [](const WindRule& windRule) -> SkPathFillType {
        switch (windRule) {
        case WindRule::EvenOdd:
            return SkPathFillType::kEvenOdd;
        case WindRule::NonZero:
            return SkPathFillType::kWinding;
        }

        return SkPathFillType::kWinding;
    };

    SkScalar x = point.x();
    SkScalar y = point.y();
    auto fillType = toSkiaFillType(windRule);
    if (m_platformPath.getFillType() != fillType) {
        SkPath pathCopy = m_platformPath;
        pathCopy.setFillType(fillType);
        return pathCopy.contains(x, y);
    }
    return m_platformPath.contains(x, y);
}

bool PathSkia::strokeContains(const FloatPoint& point, const Function<void(GraphicsContext&)>& strokeStyleApplier) const
{
    if (isEmpty() || !std::isfinite(point.x()) || !std::isfinite(point.y()))
        return false;

    auto surface = SkSurfaces::Null(1, 1);
    GraphicsContextSkia graphicsContext(*surface->getCanvas(), RenderingMode::Unaccelerated, RenderingPurpose::Unspecified);
    strokeStyleApplier(graphicsContext);

    // FIXME: Compute stroke precision.
    SkPaint paint = graphicsContext.createStrokePaint();
    SkPath strokePath;
    skpathutils::FillPathWithPaint(m_platformPath, paint, &strokePath, nullptr);
    return strokePath.contains(SkScalar(point.x()), SkScalar(point.y()));
}

FloatRect PathSkia::fastBoundingRect() const
{
    return m_platformPath.getBounds();
}

FloatRect PathSkia::boundingRect() const
{
    return m_platformPath.computeTightBounds();
}

FloatRect PathSkia::strokeBoundingRect(const Function<void(GraphicsContext&)>& strokeStyleApplier) const
{
    if (isEmpty())
        return { };

    auto surface = SkSurfaces::Null(1, 1);
    GraphicsContextSkia graphicsContext(*surface->getCanvas(), RenderingMode::Unaccelerated, RenderingPurpose::Unspecified);
    strokeStyleApplier(graphicsContext);

    // Skia stroke resolution scale for reduced-precision requirements.
    constexpr float strokePrecision = 0.3f;
    SkPaint paint = graphicsContext.createStrokePaint();
    SkPath strokePath;
    skpathutils::FillPathWithPaint(m_platformPath, paint, &strokePath, nullptr, strokePrecision);
    return strokePath.computeTightBounds();
}

} // namespace WebCore

#endif // USE(SKIA)
