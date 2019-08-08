/*
 * Copyright (C) 2016 Apple Inc.  All rights reserved.
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

#if USE(DIRECT2D)

#include "AffineTransform.h"
#include "COMPtr.h"
#include "FloatRect.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "PlatformContextDirect2D.h"
#include "StrokeStyleApplier.h"
#include <d2d1.h>
#include <wtf/MathExtras.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

static inline ID2D1RenderTarget* scratchRenderTarget()
{
    static COMPtr<ID2D1RenderTarget> renderTarget = adoptCOM(GraphicsContext::defaultRenderTarget());
    return renderTarget.get();
}

Path Path::polygonPathFromPoints(const Vector<FloatPoint>& points)
{
    Path path;
    if (points.size() < 2)
        return path;

    Vector<D2D1_POINT_2F, 32> d2dPoints;
    d2dPoints.reserveInitialCapacity(points.size() - 1);
    for (auto point : points)
        d2dPoints.uncheckedAppend(point);

    path.moveTo(points.first());

    ASSERT(path.activePath());

    path.activePath()->AddLines(d2dPoints.data(), d2dPoints.size());
    path.closeSubpath();

    return path;
}

Path::Path() = default;

Path::~Path() = default;

PlatformPathPtr Path::ensurePlatformPath()
{
    if (!m_path) {
        HRESULT hr = GraphicsContext::systemFactory()->CreateGeometryGroup(D2D1_FILL_MODE_WINDING, nullptr, 0, &m_path);
        ASSERT(SUCCEEDED(hr));
        if (FAILED(hr))
            return nullptr;
    }

    return m_path.get();
}

void Path::appendGeometry(ID2D1Geometry* geometry)
{
    unsigned geometryCount = m_path ? m_path->GetSourceGeometryCount() : 0;
    Vector<ID2D1Geometry*> geometries(geometryCount, nullptr);

    // Note: 'GetSourceGeometries' returns geometries that have a +1 ref count.
    // so they must be released before we return.
    if (geometryCount)
        m_path->GetSourceGeometries(geometries.data(), geometryCount);

    geometry->AddRef();
    geometries.append(geometry);

    auto fillMode = m_path ? m_path->GetFillMode() : D2D1_FILL_MODE_WINDING;

    COMPtr<ID2D1GeometryGroup> protectedPath = m_path;
    m_path = nullptr;

    HRESULT hr = GraphicsContext::systemFactory()->CreateGeometryGroup(fillMode, geometries.data(), geometries.size(), &m_path);
    RELEASE_ASSERT(SUCCEEDED(hr));

    for (auto entry : geometries)
        entry->Release();
}

void Path::createGeometryWithFillMode(WindRule webkitFillMode, COMPtr<ID2D1GeometryGroup>& path) const
{
    RELEASE_ASSERT(m_path);

    auto fillMode = (webkitFillMode == WindRule::EvenOdd) ? D2D1_FILL_MODE_ALTERNATE : D2D1_FILL_MODE_WINDING;

    if (fillMode == m_path->GetFillMode()) {
        path = m_path;
        return;
    }

    unsigned geometryCount = m_path->GetSourceGeometryCount();

    Vector<ID2D1Geometry*> geometries(geometryCount, nullptr);
    ASSERT(geometryCount);

    // Note: 'GetSourceGeometries' returns geometries that have a +1 ref count.
    // so they must be released before we return.
    m_path->GetSourceGeometries(geometries.data(), geometryCount);

    HRESULT hr = GraphicsContext::systemFactory()->CreateGeometryGroup(fillMode, geometries.data(), geometries.size(), &path);
    RELEASE_ASSERT(SUCCEEDED(hr));

    for (auto entry : geometries)
        entry->Release();
}

Path::Path(const Path& other)
{
    if (other.platformPath() && other.activePath()) {
        auto otherPath = other.platformPath();

        unsigned geometryCount = otherPath->GetSourceGeometryCount();

        Vector<ID2D1Geometry*> geometries(geometryCount, nullptr);
        ASSERT(geometryCount);

        // Note: 'GetSourceGeometries' returns geometries that have a +1 ref count.
        // so they must be released before we return.
        otherPath->GetSourceGeometries(geometries.data(), geometryCount);

        HRESULT hr = GraphicsContext::systemFactory()->CreateGeometryGroup(other.m_path->GetFillMode(), geometries.data(), geometryCount, &m_path);
        RELEASE_ASSERT(SUCCEEDED(hr));

        for (auto entry : geometries)
            entry->Release();
    }
}
    
Path::Path(Path&& other)
{
    m_path = other.m_path;
    m_activePath = other.m_activePath;
    m_activePathGeometry = other.m_activePathGeometry;
    other.m_path = nullptr;
    other.m_activePath = nullptr;
    other.m_activePathGeometry = nullptr;
}

Path& Path::operator=(const Path& other)
{
    if (this == &other)
        return *this;
    m_path = other.m_path;
    m_activePath = other.m_activePath;
    m_activePathGeometry = other.m_activePathGeometry;
    return *this;
}

Path& Path::operator=(Path&& other)
{
    if (this == &other)
        return *this;
    m_path = other.m_path;
    m_activePath = other.m_activePath;
    m_activePathGeometry = other.m_activePathGeometry;
    other.m_path = nullptr;
    other.m_activePath = nullptr;
    other.m_activePathGeometry = nullptr;
    return *this;
}

HRESULT Path::initializePathState()
{
    m_path = nullptr;
    m_activePath = nullptr;
    m_activePathGeometry = nullptr;

    GraphicsContext::systemFactory()->CreatePathGeometry(&m_activePathGeometry);

    Vector<ID2D1Geometry*> geometries;
    geometries.append(m_activePathGeometry.get());

    HRESULT hr = GraphicsContext::systemFactory()->CreateGeometryGroup(D2D1_FILL_MODE_WINDING, geometries.data(), geometries.size(), &m_path);
    if (FAILED(hr))
        return hr;

    return m_activePathGeometry->Open(&m_activePath);
}

void Path::drawDidComplete()
{
    FloatPoint currentPoint = this->currentPoint();

    // To maintain proper semantics with CG, we need to clear our Direct2D
    // path objects when a draw has finished.
    HRESULT hr = const_cast<Path*>(this)->initializePathState();

    if (!SUCCEEDED(hr))
        return;

    m_activePath->SetFillMode(D2D1_FILL_MODE_WINDING);

    m_activePath->BeginFigure(currentPoint, D2D1_FIGURE_BEGIN_FILLED);
    ++m_openFigureCount;
}

bool Path::contains(const FloatPoint& point, WindRule rule) const
{
    if (isNull())
        return false;

    if (!fastBoundingRect().contains(point))
        return false;

    BOOL contains;
    if (!SUCCEEDED(m_path->FillContainsPoint(D2D1::Point2F(point.x(), point.y()), nullptr, &contains)))
        return false;

    return contains;
}

bool Path::strokeContains(StrokeStyleApplier* applier, const FloatPoint& point) const
{
    if (isNull())
        return false;

    ASSERT(applier);

    PlatformContextDirect2D scratchContextD2D(scratchRenderTarget());
    GraphicsContext scratchContext(&scratchContextD2D, GraphicsContext::BitmapRenderingContextType::GPUMemory);
    applier->strokeStyle(&scratchContext);

    BOOL containsPoint = false;
    HRESULT hr = m_path->StrokeContainsPoint(point, scratchContext.strokeThickness(), scratchContext.platformStrokeStyle(), nullptr, 1.0f, &containsPoint);
    if (!SUCCEEDED(hr))
        return false;

    return containsPoint;
}

void Path::closeAnyOpenGeometries()
{
    ASSERT(m_activePath);

    if (!m_openFigureCount)
        return;

    while (m_openFigureCount) {
        m_activePath->EndFigure(D2D1_FIGURE_END_OPEN);
        --m_openFigureCount;
    }

    HRESULT hr = m_activePath->Close();
    ASSERT(SUCCEEDED(hr));
}

void Path::translate(const FloatSize& size)
{
    transform(AffineTransform(1, 0, 0, 1, size.width(), size.height()));
}

void Path::transform(const AffineTransform& transform)
{
    if (transform.isIdentity() || isEmpty())
        return;

    Optional<FloatPoint> currentPoint;
    if (hasCurrentPoint())
        currentPoint = this->currentPoint();

    bool pathIsActive = false;
    if (m_activePath) {
        closeAnyOpenGeometries();
        m_activePath = nullptr;
        m_activePathGeometry = nullptr;
        pathIsActive = true;
    }

    COMPtr<ID2D1TransformedGeometry> transformedPath;
    if (!SUCCEEDED(GraphicsContext::systemFactory()->CreateTransformedGeometry(m_path.get(), transform, &transformedPath)))
        return;

    Vector<ID2D1Geometry*> geometries;
    geometries.append(transformedPath.get());

    if (pathIsActive) {
        GraphicsContext::systemFactory()->CreatePathGeometry(&m_activePathGeometry);
        m_activePathGeometry->Open(&m_activePath);
        geometries.append(m_activePathGeometry.get());
    }

    auto fillMode = m_path->GetFillMode();

    m_path = nullptr;

    HRESULT hr = GraphicsContext::systemFactory()->CreateGeometryGroup(fillMode, geometries.data(), geometries.size(), &m_path);
    RELEASE_ASSERT(SUCCEEDED(hr));

    if (!currentPoint)
        return;

    if (!m_activePath)
        return;

    m_activePath->SetFillMode(fillMode);

    auto transformedPoint = transform.mapPoint(currentPoint.value());
    m_activePath->BeginFigure(transformedPoint, D2D1_FIGURE_BEGIN_FILLED);
    m_openFigureCount = 1;
}

FloatRect Path::boundingRect() const
{
    if (isNull())
        return FloatRect();

    D2D1_RECT_F bounds = { };
    if (!SUCCEEDED(m_path->GetBounds(nullptr, &bounds)))
        return FloatRect();

    return bounds;
}

FloatRect Path::fastBoundingRect() const
{
    if (isNull())
        return FloatRect();

    D2D1_RECT_F bounds = { };
    if (!SUCCEEDED(m_path->GetBounds(nullptr, &bounds)))
        return FloatRect();

    return bounds;
}

FloatRect Path::strokeBoundingRect(StrokeStyleApplier* applier) const
{
    if (isNull())
        return FloatRect();

    if (!applier)
        return boundingRect();

    UNUSED_PARAM(applier);
    notImplemented();

    // Just return regular bounding rect for now.
    return boundingRect();
}

void Path::openFigureAtCurrentPointIfNecessary()
{
    if (m_openFigureCount)
        return;

    m_activePath->SetFillMode(D2D1_FILL_MODE_WINDING);
    m_activePath->BeginFigure(currentPoint(), D2D1_FIGURE_BEGIN_FILLED);
    ++m_openFigureCount;
}

void Path::moveTo(const FloatPoint& point)
{
    if (m_activePath) {
        closeAnyOpenGeometries();
        m_activePath = nullptr;
        m_activePathGeometry = nullptr;
    }

    GraphicsContext::systemFactory()->CreatePathGeometry(&m_activePathGeometry);

    appendGeometry(m_activePathGeometry.get());

    if (!SUCCEEDED(m_activePathGeometry->Open(&m_activePath)))
        return;

    m_activePath->SetFillMode(D2D1_FILL_MODE_WINDING);
    m_activePath->BeginFigure(point, D2D1_FIGURE_BEGIN_FILLED);
    m_openFigureCount = 1;
}

void Path::addLineTo(const FloatPoint& point)
{
    ASSERT(m_activePath.get());

    openFigureAtCurrentPointIfNecessary();
    m_activePath->AddLine(point);
}

void Path::addQuadCurveTo(const FloatPoint& cp, const FloatPoint& p)
{
    ASSERT(m_activePath.get());

    openFigureAtCurrentPointIfNecessary();
    m_activePath->AddQuadraticBezier(D2D1::QuadraticBezierSegment(cp, p));
}

void Path::addBezierCurveTo(const FloatPoint& cp1, const FloatPoint& cp2, const FloatPoint& p)
{
    ASSERT(m_activePath.get());

    openFigureAtCurrentPointIfNecessary();
    m_activePath->AddBezier(D2D1::BezierSegment(cp1, cp2, p));
}

void Path::addArcTo(const FloatPoint& p1, const FloatPoint& p2, float radius)
{
    ASSERT(m_activePath.get());

    FloatPoint p0 = currentPoint();

    if (p1 == p0 || p1 == p2 || WTF::areEssentiallyEqual(radius, 0.0f))
        return addLineTo(p1);

    float direction = (p2.x() - p1.x()) * (p0.y() - p1.y()) + (p2.y() - p1.y()) * (p1.x() - p0.x());
    if (WTF::areEssentiallyEqual(direction, 0.0f))
        return addLineTo(p1);

    auto a2 = toFloatPoint(p0 - p1).lengthSquared();
    auto b2 = toFloatPoint(p1 - p2).lengthSquared();
    auto c2 = toFloatPoint(p0 - p2).lengthSquared();

    double cosx = (a2 + b2 - c2) / (2.0 * std::sqrt(a2 * b2));
    double sinx = std::sqrt(1.0 - cosx * cosx);
    double d = radius / ((1 - cosx) / sinx);

    auto an = toFloatPoint(p1 - p0).scaled(1.0 / std::sqrt(a2));
    auto bn = toFloatPoint(p1 - p2).scaled(1.0 / std::sqrt(b2));

    auto startPoint = toFloatPoint(p1 - an.scaled(d));
    auto p4 = toFloatPoint(p1 - bn.scaled(d));

    bool anticlockwise = (direction < 0);
    an.scale(radius * (anticlockwise ? 1 : -1));

    FloatPoint center(startPoint.x() + an.y(), startPoint.y() - an.x());

    double angle0 = atan2(startPoint.y() - center.y(), startPoint.x() - center.x());
    double angle1 = atan2(p4.y() - center.y(), p4.x() - center.x());

    openFigureAtCurrentPointIfNecessary();
    addLineTo(startPoint);
    addArc(center, radius, angle0, angle1, anticlockwise);
}

static bool equalRadiusWidths(const FloatSize& topLeftRadius, const FloatSize& topRightRadius, const FloatSize& bottomLeftRadius, const FloatSize& bottomRightRadius)
{
    return topLeftRadius.width() == topRightRadius.width()
        && topRightRadius.width() == bottomLeftRadius.width()
        && bottomLeftRadius.width() == bottomRightRadius.width();
}

static bool equalRadiusHeights(const FloatSize& topLeftRadius, const FloatSize& topRightRadius, const FloatSize& bottomLeftRadius, const FloatSize& bottomRightRadius)
{
    return topLeftRadius.height() == bottomLeftRadius.height()
        && bottomLeftRadius.height() == topRightRadius.height()
        && topRightRadius.height() == bottomRightRadius.height();
}

void Path::platformAddPathForRoundedRect(const FloatRect& rect, const FloatSize& topLeftRadius, const FloatSize& topRightRadius, const FloatSize& bottomLeftRadius, const FloatSize& bottomRightRadius)
{
    bool equalWidths = equalRadiusWidths(topLeftRadius, topRightRadius, bottomLeftRadius, bottomRightRadius);
    bool equalHeights = equalRadiusHeights(topLeftRadius, topRightRadius, bottomLeftRadius, bottomRightRadius);

    if (equalWidths && equalHeights) {
        // Ensure that CG can render the rounded rect.
        float radiusWidth = topLeftRadius.width();
        float radiusHeight = topLeftRadius.height();
        auto rectToDraw = D2D1::RectF(rect.x(), rect.y(), rect.maxX(), rect.maxY());

        COMPtr<ID2D1RoundedRectangleGeometry> roundRect;
        HRESULT hr = GraphicsContext::systemFactory()->CreateRoundedRectangleGeometry(D2D1::RoundedRect(rectToDraw, radiusWidth, radiusHeight), &roundRect);
        RELEASE_ASSERT(SUCCEEDED(hr));
        appendGeometry(roundRect.get());
        return;
    }

    addBeziersForRoundedRect(rect, topLeftRadius, topRightRadius, bottomLeftRadius, bottomRightRadius);
}

void Path::closeSubpath()
{
    if (isNull())
        return;

    if (!m_activePath) {
        ASSERT(!m_openFigureCount);
        ASSERT(!m_activePathGeometry);
        return;
    }

    if (!m_openFigureCount)
        return;

    m_activePath->EndFigure(D2D1_FIGURE_END_CLOSED);
    --m_openFigureCount;
    if (m_openFigureCount > 0) {
        ASSERT(m_activePathGeometry);
        return;
    }

    HRESULT hr = m_activePath->Close();
    ASSERT(SUCCEEDED(hr));
}

static FloatPoint arcStart(const FloatPoint& center, float radius, float startAngle)
{
    FloatPoint startingPoint = center;
    float startX = radius * std::cos(startAngle);
    float startY = radius * std::sin(startAngle);
    startingPoint.move(startX, startY);
    return startingPoint;
}

const float twoPi = 2.0f * piFloat;

static void drawArcSection(ID2D1GeometrySink* sink, const FloatPoint& center, float radius, float startAngle, float endAngle, bool anticlockwise)
{
    // Direct2D wants us to specify the end point of the arc, not the center. It will be drawn from
    // whatever the current point in the 'm_activePath' is.
    FloatPoint endPoint = center;
    float endX = radius * std::cos(endAngle);
    float endY = radius * std::sin(endAngle);
    endPoint.move(endX, endY);

    D2D1_SWEEP_DIRECTION direction = anticlockwise ? D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE : D2D1_SWEEP_DIRECTION_CLOCKWISE;
    sink->AddArc(D2D1::ArcSegment(endPoint, D2D1::SizeF(radius, radius), 0, direction, D2D1_ARC_SIZE_SMALL));
}

void Path::addArc(const FloatPoint& center, float radius, float startAngle, float endAngle, bool anticlockwise)
{
    auto arcStartPoint = arcStart(center, radius, startAngle);
    if (!m_activePath)
        moveTo(arcStartPoint);
    else if (!areEssentiallyEqual(currentPoint(), arcStartPoint)) {
        // If the arc defined by the center and radius does not intersect the current position,
        // we need to draw a line from the current position to the starting point of the arc.
        addLineTo(arcStartPoint);
    }

    if (WTF::areEssentiallyEqual(std::abs(endAngle - startAngle), twoPi))
        return addEllipse(FloatRect(center.x() - radius, center.y() - radius, 2.0 * radius, 2.0 * radius));

    if (anticlockwise) {
        if (endAngle > startAngle) {
            endAngle -= twoPi * std::ceil((endAngle - startAngle) / twoPi);
            ASSERT(endAngle <= startAngle);
        }
    } else {
        if (startAngle > endAngle) {
            startAngle -= twoPi * std::ceil((startAngle - endAngle) / twoPi);
            ASSERT(startAngle <= endAngle);
        }
    }

    const float delta = anticlockwise ? -piOverTwoFloat : piOverTwoFloat;
    float remainingArcAngle = endAngle - startAngle;

    while ((remainingArcAngle > 0 && remainingArcAngle > delta) || (remainingArcAngle < 0 && remainingArcAngle < delta)) {
        const double currentEndAngle = startAngle + delta;
        drawArcSection(m_activePath.get(), center, radius, startAngle, currentEndAngle, anticlockwise);
        startAngle = currentEndAngle;
        remainingArcAngle -= delta;
    }

    // Handle any remaining part of the arc:
    if (std::abs(remainingArcAngle) > 1e-6)
        drawArcSection(m_activePath.get(), center, radius, startAngle, startAngle + remainingArcAngle, anticlockwise);
}

void Path::addRect(const FloatRect& r)
{
    if (!m_activePath)
        moveTo(r.location());

    COMPtr<ID2D1RectangleGeometry> rectangle;
    HRESULT hr = GraphicsContext::systemFactory()->CreateRectangleGeometry(r, &rectangle);
    RELEASE_ASSERT(SUCCEEDED(hr));
    appendGeometry(rectangle.get());
}

void Path::addEllipse(FloatPoint p, float radiusX, float radiusY, float rotation, float startAngle, float endAngle, bool anticlockwise)
{
    AffineTransform transform;
    transform.translate(p.x(), p.y()).rotate(rad2deg(rotation)).scale(radiusX, radiusY);

    notImplemented();
}

void Path::addEllipse(const FloatRect& r)
{
    if (!m_activePath)
        moveTo(r.location());

    COMPtr<ID2D1EllipseGeometry> ellipse;
    // Note: The radii of the ellipse contained within a rectange are half the width and height of the rect.
    HRESULT hr = GraphicsContext::systemFactory()->CreateEllipseGeometry(D2D1::Ellipse(r.center(), 0.5 * r.width(), 0.5 * r.height()), &ellipse);
    RELEASE_ASSERT(SUCCEEDED(hr));
    appendGeometry(ellipse.get());
}

void Path::addPath(const Path& path, const AffineTransform& transform)
{
    if (!path.platformPath())
        return;

    if (!transform.isInvertible())
        return;

    notImplemented();
}


void Path::clear()
{
    if (isNull())
        return;

    m_path = nullptr;
    m_activePath = nullptr;
    m_activePathGeometry = nullptr;
}

bool Path::isEmpty() const
{
    if (isNull())
        return true;

    if (!m_path->GetSourceGeometryCount())
        return true;

    return false;
}

bool Path::hasCurrentPoint() const
{
    return !isEmpty();
}
    
FloatPoint Path::currentPoint() const 
{
    if (isNull())
        return FloatPoint();

    float length = 0;
    HRESULT hr = m_path->ComputeLength(nullptr, &length);
    if (!SUCCEEDED(hr))
        return FloatPoint();

    D2D1_POINT_2F point = { };
    D2D1_POINT_2F tangent = { };
    hr = m_path->ComputePointAtLength(length, nullptr, &point, &tangent);
    if (!SUCCEEDED(hr))
        return FloatPoint();

    return point;
}

float Path::length() const
{
    float length = 0;
    HRESULT hr = m_path->ComputeLength(nullptr, &length);
    if (!SUCCEEDED(hr))
        return 0;

    return length;
}

void Path::apply(const PathApplierFunction& function) const
{
    if (isNull())
        return;

    notImplemented();
}

}

#endif // USE(DIRECT2D)
