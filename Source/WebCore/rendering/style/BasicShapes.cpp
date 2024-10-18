/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "BasicShapes.h"

#include "AnimationUtilities.h"
#include "BasicShapeConversion.h"
#include "BasicShapesShape.h"
#include "CalculationValue.h"
#include "FloatRect.h"
#include "FloatRoundedRect.h"
#include "GeometryUtilities.h"
#include "LengthFunctions.h"
#include "Path.h"
#include "RenderBox.h"
#include "SVGPathByteStream.h"
#include "SVGPathUtilities.h"
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/TinyLRUCache.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(BasicShape);
WTF_MAKE_TZONE_ALLOCATED_IMPL(BasicShapeCircleOrEllipse);
WTF_MAKE_TZONE_ALLOCATED_IMPL(BasicShapeCircle);
WTF_MAKE_TZONE_ALLOCATED_IMPL(BasicShapeEllipse);
WTF_MAKE_TZONE_ALLOCATED_IMPL(BasicShapePolygon);
WTF_MAKE_TZONE_ALLOCATED_IMPL(BasicShapePath);
WTF_MAKE_TZONE_ALLOCATED_IMPL(BasicShapeInset);
WTF_MAKE_TZONE_ALLOCATED_IMPL(BasicShapeRect);
WTF_MAKE_TZONE_ALLOCATED_IMPL(BasicShapeXywh);

struct SVGPathTransformedByteStream {
    friend bool operator==(const SVGPathTransformedByteStream&, const SVGPathTransformedByteStream&) = default;
    bool isEmpty() const { return rawStream.isEmpty(); }

    Path path() const
    {
        Path path = buildPathFromByteStream(rawStream);
        if (zoom != 1)
            path.transform(AffineTransform().scale(zoom));
        path.translate(toFloatSize(offset));
        return path;
    }
    
    SVGPathByteStream rawStream;
    float zoom;
    FloatPoint offset;
};

struct EllipsePathPolicy : public TinyLRUCachePolicy<FloatRect, Path> {
public:
    static bool isKeyNull(const FloatRect& rect) { return rect.isEmpty(); }

    static Path createValueForKey(const FloatRect& rect)
    {
        Path path;
        path.addEllipseInRect(rect);
        return path;
    }
};

struct RoundedRectPathPolicy : public TinyLRUCachePolicy<FloatRoundedRect, Path> {
public:
    static bool isKeyNull(const FloatRoundedRect& rect) { return rect.isEmpty(); }

    static Path createValueForKey(const FloatRoundedRect& rect)
    {
        Path path;
        path.addRoundedRect(rect, PathRoundedRect::Strategy::PreferBezier);
        return path;
    }
};

struct PolygonPathPolicy : TinyLRUCachePolicy<Vector<FloatPoint>, Path> {
public:
    static bool isKeyNull(const Vector<FloatPoint>& points) { return !points.size(); }

    static Path createValueForKey(const Vector<FloatPoint>& points) { return Path(points); }
};

struct TransformedByteStreamPathPolicy : TinyLRUCachePolicy<SVGPathTransformedByteStream, Path> {
    static bool isKeyNull(const SVGPathTransformedByteStream& stream) { return stream.isEmpty(); }

    static Path createValueForKey(const SVGPathTransformedByteStream& stream) { return stream.path(); }
};

static const Path& cachedEllipsePath(const FloatRect& rect)
{
    static NeverDestroyed<TinyLRUCache<FloatRect, Path, 4, EllipsePathPolicy>> cache;
    return cache.get().get(rect);
}

static const Path& cachedRoundedRectPath(const FloatRoundedRect& rect)
{
    static NeverDestroyed<TinyLRUCache<FloatRoundedRect, Path, 4, RoundedRectPathPolicy>> cache;
    return cache.get().get(rect);
}

static const Path& cachedPolygonPath(const Vector<FloatPoint>& points)
{
    static NeverDestroyed<TinyLRUCache<Vector<FloatPoint>, Path, 4, PolygonPathPolicy>> cache;
    return cache.get().get(points);
}

static const Path& cachedTransformedByteStreamPath(const SVGPathByteStream& stream, float zoom, const FloatPoint& offset)
{
    static NeverDestroyed<TinyLRUCache<SVGPathTransformedByteStream, Path, 4, TransformedByteStreamPathPolicy>> cache;
    return cache.get().get(SVGPathTransformedByteStream { stream, zoom, offset });
}

// MARK: -

Ref<BasicShapeCircle> BasicShapeCircle::create(BasicShapeCenterCoordinate&& centerX, BasicShapeCenterCoordinate&& centerY, BasicShapeRadius&& radius)
{
    return adoptRef(*new BasicShapeCircle(WTFMove(centerX), WTFMove(centerY), WTFMove(radius)));
}

BasicShapeCircle::BasicShapeCircle(BasicShapeCenterCoordinate&& centerX, BasicShapeCenterCoordinate&& centerY, BasicShapeRadius&& radius)
    : m_centerX(WTFMove(centerX))
    , m_centerY(WTFMove(centerY))
    , m_radius(WTFMove(radius))
{
}

Ref<BasicShape> BasicShapeCircle::clone() const
{
    auto centerX = m_centerX;
    auto centerY = m_centerY;
    auto radius = m_radius;
    return adoptRef(*new BasicShapeCircle(WTFMove(centerX), WTFMove(centerY), WTFMove(radius)));
}

bool BasicShapeCircle::operator==(const BasicShape& other) const
{
    if (type() != other.type())
        return false;

    auto& otherCircle = uncheckedDowncast<BasicShapeCircle>(other);
    return m_centerX == otherCircle.m_centerX
        && m_centerY == otherCircle.m_centerY
        && m_radius == otherCircle.m_radius
        && positionWasOmitted() == otherCircle.positionWasOmitted();
}

float BasicShapeCircle::floatValueForRadiusInBox(FloatSize boxSize, FloatPoint center) const
{
    switch (m_radius.type()) {
    case BasicShapeRadius::Type::Value:
        return floatValueForLength(m_radius.value(), boxSize.diagonalLength() / sqrtOfTwoFloat);

    case BasicShapeRadius::Type::ClosestSide:
        return distanceToClosestSide(center, boxSize);

    case BasicShapeRadius::Type::FarthestSide:
        return distanceToFarthestSide(center, boxSize);

    case BasicShapeRadius::Type::ClosestCorner:
        return distanceToClosestCorner(center, boxSize);

    case BasicShapeRadius::Type::FarthestCorner:
        return distanceToFarthestCorner(center, boxSize);
    }
    return 0;
}

Path BasicShapeCircle::pathForCenterCoordinate(const FloatRect& boundingBox, FloatPoint center) const
{
    float radius = floatValueForRadiusInBox(boundingBox.size(), center);
    return cachedEllipsePath(FloatRect(center.x() - radius + boundingBox.x(), center.y() - radius + boundingBox.y(), radius * 2, radius * 2));
}

Path BasicShapeCircle::path(const FloatRect& boundingBox) const
{
    return pathForCenterCoordinate(boundingBox, { floatValueForCenterCoordinate(m_centerX, boundingBox.width()), floatValueForCenterCoordinate(m_centerY, boundingBox.height()) });
}

bool BasicShapeCircle::canBlend(const BasicShape& other) const
{
    if (type() != other.type())
        return false;

    return radius().canBlend(uncheckedDowncast<BasicShapeCircle>(other).radius());
}

Ref<BasicShape> BasicShapeCircle::blend(const BasicShape& other, const BlendingContext& context) const
{
    ASSERT(type() == other.type());
    auto& otherCircle = downcast<BasicShapeCircle>(other);
    auto result =  BasicShapeCircle::create();

    result->setCenterX(m_centerX.blend(otherCircle.centerX(), context));
    result->setCenterY(m_centerY.blend(otherCircle.centerY(), context));
    result->setRadius(m_radius.blend(otherCircle.radius(), context));
    result->setPositionWasOmitted(positionWasOmitted() && otherCircle.positionWasOmitted());
    return result;
}

void BasicShapeCircle::dump(TextStream& ts) const
{
    ts.dumpProperty("center-x", centerX());
    ts.dumpProperty("center-y", centerY());
    ts.dumpProperty("radius", radius());
}

// MARK: -

Ref<BasicShapeEllipse> BasicShapeEllipse::create(BasicShapeCenterCoordinate&& centerX, BasicShapeCenterCoordinate&& centerY, BasicShapeRadius&& radiusX, BasicShapeRadius&& radiusY)
{
    return adoptRef(*new BasicShapeEllipse(WTFMove(centerX), WTFMove(centerY), WTFMove(radiusX), WTFMove(radiusY)));
}

BasicShapeEllipse::BasicShapeEllipse(BasicShapeCenterCoordinate&& centerX, BasicShapeCenterCoordinate&& centerY, BasicShapeRadius&& radiusX, BasicShapeRadius&& radiusY)
    : m_centerX(WTFMove(centerX))
    , m_centerY(WTFMove(centerY))
    , m_radiusX(WTFMove(radiusX))
    , m_radiusY(WTFMove(radiusY))
{
}

Ref<BasicShape> BasicShapeEllipse::clone() const
{
    auto centerX = m_centerX;
    auto centerY = m_centerY;
    auto radiusX = m_radiusX;
    auto radiusY = m_radiusY;
    return adoptRef(*new BasicShapeEllipse(WTFMove(centerX), WTFMove(centerY), WTFMove(radiusX), WTFMove(radiusY)));
}

bool BasicShapeEllipse::operator==(const BasicShape& other) const
{
    if (type() != other.type())
        return false;

    auto& otherEllipse = uncheckedDowncast<BasicShapeEllipse>(other);
    return m_centerX == otherEllipse.m_centerX
        && m_centerY == otherEllipse.m_centerY
        && m_radiusX == otherEllipse.m_radiusX
        && m_radiusY == otherEllipse.m_radiusY
        && positionWasOmitted() == otherEllipse.positionWasOmitted();
}

FloatSize BasicShapeEllipse::floatSizeForRadiusInBox(FloatSize boxSize, FloatPoint center) const
{
    auto sizeForAxis = [&](const BasicShapeRadius& radius, float centerValue, float dimensionSize) {
        switch (radius.type()) {
        case BasicShapeRadius::Type::Value:
            return floatValueForLength(radius.value(), std::abs(dimensionSize));

        case BasicShapeRadius::Type::ClosestSide:
            return std::min(std::abs(centerValue), std::abs(dimensionSize - centerValue));

        case BasicShapeRadius::Type::FarthestSide:
            return std::max(std::abs(centerValue), std::abs(dimensionSize - centerValue));

        case BasicShapeRadius::Type::ClosestCorner:
            return distanceToClosestCorner(center, boxSize);

        case BasicShapeRadius::Type::FarthestCorner:
            return distanceToFarthestCorner(center, boxSize);
        }
        return 0.0f;
    };

    return { sizeForAxis(m_radiusX, center.x(), boxSize.width()), sizeForAxis(m_radiusY, center.y(), boxSize.height()) };
}

Path BasicShapeEllipse::pathForCenterCoordinate(const FloatRect& boundingBox, FloatPoint center) const
{
    auto radius = floatSizeForRadiusInBox(boundingBox.size(), center);
    return cachedEllipsePath(FloatRect(center.x() - radius.width() + boundingBox.x(), center.y() - radius.height() + boundingBox.y(), radius.width() * 2, radius.height() * 2));
}

Path BasicShapeEllipse::path(const FloatRect& boundingBox) const
{
    return pathForCenterCoordinate(boundingBox, { floatValueForCenterCoordinate(m_centerX, boundingBox.width()), floatValueForCenterCoordinate(m_centerY, boundingBox.height()) });
}

bool BasicShapeEllipse::canBlend(const BasicShape& other) const
{
    if (type() != other.type())
        return false;

    auto& otherEllipse = uncheckedDowncast<BasicShapeEllipse>(other);
    return radiusX().canBlend(otherEllipse.radiusX()) && radiusY().canBlend(otherEllipse.radiusY());
}

Ref<BasicShape> BasicShapeEllipse::blend(const BasicShape& other, const BlendingContext& context) const
{
    ASSERT(type() == other.type());
    auto& otherEllipse = downcast<BasicShapeEllipse>(other);
    auto result = BasicShapeEllipse::create();

    if (m_radiusX.type() != BasicShapeRadius::Type::Value || otherEllipse.radiusX().type() != BasicShapeRadius::Type::Value
        || m_radiusY.type() != BasicShapeRadius::Type::Value || otherEllipse.radiusY().type() != BasicShapeRadius::Type::Value) {
        result->setCenterX(otherEllipse.centerX());
        result->setCenterY(otherEllipse.centerY());
        result->setRadiusX(otherEllipse.radiusX());
        result->setRadiusY(otherEllipse.radiusY());
        return result;
    }

    result->setCenterX(m_centerX.blend(otherEllipse.centerX(), context));
    result->setCenterY(m_centerY.blend(otherEllipse.centerY(), context));
    result->setRadiusX(m_radiusX.blend(otherEllipse.radiusX(), context));
    result->setRadiusY(m_radiusY.blend(otherEllipse.radiusY(), context));
    result->setPositionWasOmitted(positionWasOmitted() && otherEllipse.positionWasOmitted());
    return result;
}

void BasicShapeEllipse::dump(TextStream& ts) const
{
    ts.dumpProperty("center-x", centerX());
    ts.dumpProperty("center-y", centerY());
    ts.dumpProperty("radius-x", radiusX());
    ts.dumpProperty("radius-y", radiusY());
}

// MARK: -

Ref<BasicShapeRect> BasicShapeRect::create(Length&& top, Length&& right, Length&& bottom, Length&& left, LengthSize&& topLeftRadius, LengthSize&& topRightRadius, LengthSize&& bottomRightRadius, LengthSize&& bottomLeftRadius)
{
    return adoptRef(*new BasicShapeRect(WTFMove(top), WTFMove(right), WTFMove(bottom), WTFMove(left), WTFMove(topLeftRadius), WTFMove(topRightRadius), WTFMove(bottomRightRadius), WTFMove(bottomLeftRadius)));
}

BasicShapeRect::BasicShapeRect(Length&& top, Length&& right, Length&& bottom, Length&& left, LengthSize&& topLeftRadius, LengthSize&& topRightRadius, LengthSize&& bottomRightRadius, LengthSize&& bottomLeftRadius)
    : m_edges(RectEdges<Length> { WTFMove(top), WTFMove(right), WTFMove(bottom), WTFMove(left) })
    , m_topLeftRadius(WTFMove(topLeftRadius))
    , m_topRightRadius(WTFMove(topRightRadius))
    , m_bottomRightRadius(WTFMove(bottomRightRadius))
    , m_bottomLeftRadius(WTFMove(bottomLeftRadius))
{
}

BasicShapeRect::BasicShapeRect(RectEdges<Length>&& edges, LengthSize&& topLeftRadius, LengthSize&& topRightRadius, LengthSize&& bottomRightRadius, LengthSize&& bottomLeftRadius)
    : m_edges(WTFMove(edges))
    , m_topLeftRadius(WTFMove(topLeftRadius))
    , m_topRightRadius(WTFMove(topRightRadius))
    , m_bottomRightRadius(WTFMove(bottomRightRadius))
    , m_bottomLeftRadius(WTFMove(bottomLeftRadius))
{
}

Ref<BasicShape> BasicShapeRect::clone() const
{
    auto edges = m_edges;

    auto topLeftRadius = m_topLeftRadius;
    auto topRightRadius = m_topRightRadius;
    auto bottomRightRadius = m_bottomRightRadius;
    auto bottomLeftRadius = m_bottomLeftRadius;

    return adoptRef(*new BasicShapeRect(WTFMove(edges), WTFMove(topLeftRadius), WTFMove(topRightRadius), WTFMove(bottomRightRadius), WTFMove(bottomLeftRadius)));
}

bool BasicShapeRect::operator==(const BasicShape& other) const
{
    if (type() != other.type())
        return false;

    auto& otherRect = uncheckedDowncast<BasicShapeRect>(other);
    return m_edges == otherRect.m_edges
        && m_topLeftRadius == otherRect.m_topLeftRadius
        && m_topRightRadius == otherRect.m_topRightRadius
        && m_bottomRightRadius == otherRect.m_bottomRightRadius
        && m_bottomLeftRadius == otherRect.m_bottomLeftRadius;
}

Path BasicShapeRect::path(const FloatRect& boundingBox) const
{
    auto top = m_edges.top().isAuto() ? 0 : floatValueForLength(m_edges.top(), boundingBox.height());
    auto right = m_edges.right().isAuto() ? boundingBox.width() : floatValueForLength(m_edges.right(), boundingBox.width());
    auto bottom = m_edges.bottom().isAuto() ? boundingBox.height() : floatValueForLength(m_edges.bottom(), boundingBox.height());
    auto left = m_edges.left().isAuto() ? 0 : floatValueForLength(m_edges.left(), boundingBox.width());

    right = std::max(right, left);
    bottom = std::max(top, bottom);

    auto rect = FloatRect(left + boundingBox.x(), top + boundingBox.y(), right - left, bottom - top);
    auto radii = FloatRoundedRect::Radii(floatSizeForLengthSize(m_topLeftRadius, boundingBox.size()),
        floatSizeForLengthSize(m_topRightRadius, boundingBox.size()),
        floatSizeForLengthSize(m_bottomLeftRadius, boundingBox.size()),
        floatSizeForLengthSize(m_bottomRightRadius, boundingBox.size()));

    radii.scale(calcBorderRadiiConstraintScaleFor(rect, radii));

    return cachedRoundedRectPath(FloatRoundedRect(rect, radii));
}

bool BasicShapeRect::canBlend(const BasicShape& other) const
{
    // FIXME: Allow interpolation with inset() shape.
    return type() == other.type();
}

Ref<BasicShape> BasicShapeRect::blend(const BasicShape& other, const BlendingContext& context) const
{
    ASSERT(type() == other.type());
    auto& otherRect = downcast<BasicShapeRect>(other);
    auto result = BasicShapeRect::create();

    result->setTop(WebCore::blend(otherRect.top(), top(), context));
    result->setRight(WebCore::blend(otherRect.right(), right(), context));
    result->setBottom(WebCore::blend(otherRect.bottom(), bottom(), context));
    result->setLeft(WebCore::blend(otherRect.left(), left(), context));

    result->setTopLeftRadius(WebCore::blend(otherRect.topLeftRadius(), topLeftRadius(), context, ValueRange::NonNegative));
    result->setTopRightRadius(WebCore::blend(otherRect.topRightRadius(), topRightRadius(), context, ValueRange::NonNegative));
    result->setBottomRightRadius(WebCore::blend(otherRect.bottomRightRadius(), bottomRightRadius(), context, ValueRange::NonNegative));
    result->setBottomLeftRadius(WebCore::blend(otherRect.bottomLeftRadius(), bottomLeftRadius(), context, ValueRange::NonNegative));

    return result;
}

void BasicShapeRect::dump(TextStream& ts) const
{
    ts.dumpProperty("top", top());
    ts.dumpProperty("right", right());
    ts.dumpProperty("bottom", bottom());
    ts.dumpProperty("left", left());

    ts.dumpProperty("top-left-radius", topLeftRadius());
    ts.dumpProperty("top-right-radius", topRightRadius());
    ts.dumpProperty("bottom-right-radius", bottomRightRadius());
    ts.dumpProperty("bottom-left-radius", bottomLeftRadius());
}

// MARK: -

Ref<BasicShapeXywh> BasicShapeXywh::create(Length&& insetX, Length&& insetY, Length&& width, Length&& height, LengthSize&& topLeftRadius, LengthSize&& topRightRadius, LengthSize&& bottomRightRadius, LengthSize&& bottomLeftRadius)
{
    return adoptRef(*new BasicShapeXywh(WTFMove(insetX), WTFMove(insetY), WTFMove(width), WTFMove(height), WTFMove(topLeftRadius), WTFMove(topRightRadius), WTFMove(bottomRightRadius), WTFMove(bottomLeftRadius)));
}

BasicShapeXywh::BasicShapeXywh(Length&& insetX, Length&& insetY, Length&& width, Length&& height, LengthSize&& topLeftRadius, LengthSize&& topRightRadius, LengthSize&& bottomRightRadius, LengthSize&& bottomLeftRadius)
    : m_insetX(WTFMove(insetX))
    , m_insetY(WTFMove(insetY))
    , m_width(WTFMove(width))
    , m_height(WTFMove(height))
    , m_topLeftRadius(WTFMove(topLeftRadius))
    , m_topRightRadius(WTFMove(topRightRadius))
    , m_bottomRightRadius(WTFMove(bottomRightRadius))
    , m_bottomLeftRadius(WTFMove(bottomLeftRadius))
{
}

Ref<BasicShape> BasicShapeXywh::clone() const
{
    auto insetX = m_insetX;
    auto insetY = m_insetY;
    auto width = m_width;
    auto height = m_height;

    auto topLeftRadius = m_topLeftRadius;
    auto topRightRadius = m_topRightRadius;
    auto bottomRightRadius = m_bottomRightRadius;
    auto bottomLeftRadius = m_bottomLeftRadius;

    return adoptRef(*new BasicShapeXywh(WTFMove(insetX), WTFMove(insetY), WTFMove(width), WTFMove(height), WTFMove(topLeftRadius), WTFMove(topRightRadius), WTFMove(bottomRightRadius), WTFMove(bottomLeftRadius)));
}

bool BasicShapeXywh::operator==(const BasicShape& other) const
{
    if (type() != other.type())
        return false;

    auto& otherXywh = uncheckedDowncast<BasicShapeXywh>(other);
    return m_insetX == otherXywh.m_insetX
        && m_insetY == otherXywh.m_insetY
        && m_width == otherXywh.m_width
        && m_height == otherXywh.m_height
        && m_topLeftRadius == otherXywh.m_topLeftRadius
        && m_topRightRadius == otherXywh.m_topRightRadius
        && m_bottomRightRadius == otherXywh.m_bottomRightRadius
        && m_bottomLeftRadius == otherXywh.m_bottomLeftRadius;
}

Path BasicShapeXywh::path(const FloatRect& boundingBox) const
{
    auto insetX = floatValueForLength(m_insetX, boundingBox.width());
    auto insetY = floatValueForLength(m_insetY, boundingBox.height());
    auto width = floatValueForLength(m_width, boundingBox.width());
    auto height = floatValueForLength(m_height, boundingBox.height());

    auto rect = FloatRect(boundingBox.x() + insetX, boundingBox.y() + insetY, width, height);
    auto radii = FloatRoundedRect::Radii(floatSizeForLengthSize(m_topLeftRadius, boundingBox.size()),
        floatSizeForLengthSize(m_topRightRadius, boundingBox.size()),
        floatSizeForLengthSize(m_bottomLeftRadius, boundingBox.size()),
        floatSizeForLengthSize(m_bottomRightRadius, boundingBox.size()));

    radii.scale(calcBorderRadiiConstraintScaleFor(rect, radii));

    return cachedRoundedRectPath(FloatRoundedRect(rect, radii));
}

bool BasicShapeXywh::canBlend(const BasicShape& other) const
{
    // FIXME: Allow interpolation with inset() shape.
    return type() == other.type();
}

Ref<BasicShape> BasicShapeXywh::blend(const BasicShape& other, const BlendingContext& context) const
{
    ASSERT(type() == other.type());
    auto& otherXywh = downcast<BasicShapeXywh>(other);
    auto result = BasicShapeXywh::create();

    result->setInsetX(WebCore::blend(otherXywh.insetX(), m_insetX, context));
    result->setInsetY(WebCore::blend(otherXywh.insetY(), m_insetY, context));
    result->setWidth(WebCore::blend(otherXywh.width(), m_width, context));
    result->setHeight(WebCore::blend(otherXywh.height(), m_height, context));

    result->setTopLeftRadius(WebCore::blend(otherXywh.topLeftRadius(), topLeftRadius(), context, ValueRange::NonNegative));
    result->setTopRightRadius(WebCore::blend(otherXywh.topRightRadius(), topRightRadius(), context, ValueRange::NonNegative));
    result->setBottomRightRadius(WebCore::blend(otherXywh.bottomRightRadius(), bottomRightRadius(), context, ValueRange::NonNegative));
    result->setBottomLeftRadius(WebCore::blend(otherXywh.bottomLeftRadius(), bottomLeftRadius(), context, ValueRange::NonNegative));

    return result;
}

void BasicShapeXywh::dump(TextStream& ts) const
{
    ts.dumpProperty("x", insetX());
    ts.dumpProperty("y", insetY());
    ts.dumpProperty("width", width());
    ts.dumpProperty("height", height());

    ts.dumpProperty("top-left-radius", topLeftRadius());
    ts.dumpProperty("top-right-radius", topRightRadius());
    ts.dumpProperty("bottom-right-radius", bottomRightRadius());
    ts.dumpProperty("bottom-left-radius", bottomLeftRadius());

}

// MARK: -

Ref<BasicShapePolygon> BasicShapePolygon::create(WindRule windRule, Vector<Length>&& values)
{
    return adoptRef(*new BasicShapePolygon(windRule, WTFMove(values)));
}

BasicShapePolygon::BasicShapePolygon(WindRule windRule, Vector<Length>&& values)
    : m_windRule(windRule)
    , m_values(WTFMove(values))
{
}

Ref<BasicShape> BasicShapePolygon::clone() const
{
    auto values = m_values;
    return adoptRef(*new BasicShapePolygon(m_windRule, WTFMove(values)));
}

bool BasicShapePolygon::operator==(const BasicShape& other) const
{
    if (type() != other.type())
        return false;

    auto& otherPolygon = uncheckedDowncast<BasicShapePolygon>(other);
    return m_windRule == otherPolygon.m_windRule
        && m_values == otherPolygon.m_values;
}

Path BasicShapePolygon::path(const FloatRect& boundingBox) const
{
    ASSERT(!(m_values.size() % 2));
    size_t length = m_values.size();

    Vector<FloatPoint> points(length / 2);
    for (size_t i = 0; i < points.size(); ++i) {
        points[i].setX(floatValueForLength(m_values.at(i * 2), boundingBox.width()) + boundingBox.x());
        points[i].setY(floatValueForLength(m_values.at(i * 2 + 1), boundingBox.height()) + boundingBox.y());
    }

    return cachedPolygonPath(points);
}

bool BasicShapePolygon::canBlend(const BasicShape& other) const
{
    if (type() != other.type())
        return false;

    auto& otherPolygon = uncheckedDowncast<BasicShapePolygon>(other);
    return values().size() == otherPolygon.values().size() && windRule() == otherPolygon.windRule();
}

Ref<BasicShape> BasicShapePolygon::blend(const BasicShape& from, const BlendingContext& context) const
{
    ASSERT(type() == from.type());

    auto& fromPolygon = downcast<BasicShapePolygon>(from);
    ASSERT(m_values.size() == fromPolygon.values().size());
    ASSERT(!(m_values.size() % 2));

    size_t length = m_values.size();
    auto result = BasicShapePolygon::create();
    if (!length)
        return result;

    result->setWindRule(windRule());

    for (size_t i = 0; i < length; i = i + 2) {
        result->appendPoint(
            WebCore::blend(fromPolygon.values().at(i), m_values.at(i), context),
            WebCore::blend(fromPolygon.values().at(i + 1), m_values.at(i + 1), context));
    }

    return result;
}

void BasicShapePolygon::dump(TextStream& ts) const
{
    ts.dumpProperty("wind-rule", windRule());
    ts.dumpProperty("path", values());
}

// MARK: -

Ref<BasicShapePath> BasicShapePath::create(std::unique_ptr<SVGPathByteStream>&& byteStream, float zoom, WindRule windRule)
{
    return adoptRef(*new BasicShapePath(WTFMove(byteStream), zoom, windRule));
}

BasicShapePath::BasicShapePath(std::unique_ptr<SVGPathByteStream>&& byteStream)
    : m_byteStream(WTFMove(byteStream))
{
}

BasicShapePath::BasicShapePath(std::unique_ptr<SVGPathByteStream>&& byteStream, float zoom, WindRule windRule)
    : m_byteStream(WTFMove(byteStream))
    , m_zoom(zoom)
    , m_windRule(windRule)
{
}

BasicShapePath::~BasicShapePath() = default;

Ref<BasicShape> BasicShapePath::clone() const
{
    std::unique_ptr<SVGPathByteStream> byteStream;
    if (m_byteStream)
        byteStream = m_byteStream->copy();
    return adoptRef(*new BasicShapePath(WTFMove(byteStream), m_zoom, m_windRule));
}

Path BasicShapePath::path(const FloatRect& boundingBox) const
{
    return cachedTransformedByteStreamPath(*m_byteStream, m_zoom, boundingBox.location());
}

bool BasicShapePath::operator==(const BasicShape& other) const
{
    if (type() != other.type())
        return false;

    auto& otherPath = uncheckedDowncast<BasicShapePath>(other);
    return m_zoom == otherPath.m_zoom && m_windRule == otherPath.m_windRule && *m_byteStream == *otherPath.m_byteStream;
}

bool BasicShapePath::canBlend(const BasicShape& other) const
{
    if (other.type() == Type::Shape)
        return other.canBlend(*this);

    if (type() != other.type())
        return false;

    auto& otherPath = uncheckedDowncast<BasicShapePath>(other);
    return windRule() == otherPath.windRule() && canBlendSVGPathByteStreams(*m_byteStream, *otherPath.pathData());
}

Ref<BasicShape> BasicShapePath::blend(const BasicShape& from, const BlendingContext& context) const
{
    if (from.type() == Type::Shape)
        return BasicShapeShape::blendWithPath(from, *this, context);

    ASSERT(type() == from.type());

    auto& fromPath = downcast<BasicShapePath>(from);

    auto resultingPathBytes = makeUnique<SVGPathByteStream>();
    buildAnimatedSVGPathByteStream(*fromPath.m_byteStream, *m_byteStream, *resultingPathBytes, context.progress);
    auto result = BasicShapePath::create(WTFMove(resultingPathBytes));
    result->setWindRule(windRule());
    result->setZoom(m_zoom);
    return result;
}

void BasicShapePath::dump(TextStream& ts) const
{
    ts.dumpProperty("wind-rule", windRule());
    // FIXME: print the byte stream?
}

// MARK: -

Ref<BasicShapeInset> BasicShapeInset::create(Length&& right, Length&& top, Length&& bottom, Length&& left, LengthSize&& topLeftRadius, LengthSize&& topRightRadius, LengthSize&& bottomRightRadius, LengthSize&& bottomLeftRadius)
{
    return adoptRef(*new BasicShapeInset(WTFMove(right), WTFMove(top), WTFMove(bottom), WTFMove(left), WTFMove(topLeftRadius), WTFMove(topRightRadius), WTFMove(bottomRightRadius), WTFMove(bottomLeftRadius)));
}

BasicShapeInset::BasicShapeInset(Length&& right, Length&& top, Length&& bottom, Length&& left, LengthSize&& topLeftRadius, LengthSize&& topRightRadius, LengthSize&& bottomRightRadius, LengthSize&& bottomLeftRadius)
    : m_right(WTFMove(right))
    , m_top(WTFMove(top))
    , m_bottom(WTFMove(bottom))
    , m_left(WTFMove(left))
    , m_topLeftRadius(WTFMove(topLeftRadius))
    , m_topRightRadius(WTFMove(topRightRadius))
    , m_bottomRightRadius(WTFMove(bottomRightRadius))
    , m_bottomLeftRadius(WTFMove(bottomLeftRadius))
{
}

Ref<BasicShape> BasicShapeInset::clone() const
{
    auto right = m_right;
    auto top = m_top;
    auto bottom = m_bottom;
    auto left = m_left;
    auto topLeftRadius = m_topLeftRadius;
    auto topRightRadius = m_topRightRadius;
    auto bottomRightRadius = m_bottomRightRadius;
    auto bottomLeftRadius = m_bottomLeftRadius;
    return adoptRef(*new BasicShapeInset(WTFMove(right), WTFMove(top), WTFMove(bottom), WTFMove(left), WTFMove(topLeftRadius), WTFMove(topRightRadius), WTFMove(bottomRightRadius), WTFMove(bottomLeftRadius)));
}

bool BasicShapeInset::operator==(const BasicShape& other) const
{
    if (type() != other.type())
        return false;

    auto& otherInset = uncheckedDowncast<BasicShapeInset>(other);
    return m_right == otherInset.m_right
        && m_top == otherInset.m_top
        && m_bottom == otherInset.m_bottom
        && m_left == otherInset.m_left
        && m_topLeftRadius == otherInset.m_topLeftRadius
        && m_topRightRadius == otherInset.m_topRightRadius
        && m_bottomRightRadius == otherInset.m_bottomRightRadius
        && m_bottomLeftRadius == otherInset.m_bottomLeftRadius;
}

Path BasicShapeInset::path(const FloatRect& boundingBox) const
{
    float left = floatValueForLength(m_left, boundingBox.width());
    float top = floatValueForLength(m_top, boundingBox.height());
    auto rect = FloatRect(left + boundingBox.x(), top + boundingBox.y(),
        std::max<float>(boundingBox.width() - left - floatValueForLength(m_right, boundingBox.width()), 0),
        std::max<float>(boundingBox.height() - top - floatValueForLength(m_bottom, boundingBox.height()), 0));
    auto radii = FloatRoundedRect::Radii(floatSizeForLengthSize(m_topLeftRadius, boundingBox.size()),
        floatSizeForLengthSize(m_topRightRadius, boundingBox.size()),
        floatSizeForLengthSize(m_bottomLeftRadius, boundingBox.size()),
        floatSizeForLengthSize(m_bottomRightRadius, boundingBox.size()));
    radii.scale(calcBorderRadiiConstraintScaleFor(rect, radii));

    return cachedRoundedRectPath(FloatRoundedRect(rect, radii));
}

bool BasicShapeInset::canBlend(const BasicShape& other) const
{
    return type() == other.type();
}

Ref<BasicShape> BasicShapeInset::blend(const BasicShape& from, const BlendingContext& context) const
{
    ASSERT(type() == from.type());

    auto& fromInset = downcast<BasicShapeInset>(from);
    auto result =  BasicShapeInset::create();
    result->setTop(WebCore::blend(fromInset.top(), top(), context));
    result->setRight(WebCore::blend(fromInset.right(), right(), context));
    result->setBottom(WebCore::blend(fromInset.bottom(), bottom(), context));
    result->setLeft(WebCore::blend(fromInset.left(), left(), context));

    result->setTopLeftRadius(WebCore::blend(fromInset.topLeftRadius(), topLeftRadius(), context, ValueRange::NonNegative));
    result->setTopRightRadius(WebCore::blend(fromInset.topRightRadius(), topRightRadius(), context, ValueRange::NonNegative));
    result->setBottomRightRadius(WebCore::blend(fromInset.bottomRightRadius(), bottomRightRadius(), context, ValueRange::NonNegative));
    result->setBottomLeftRadius(WebCore::blend(fromInset.bottomLeftRadius(), bottomLeftRadius(), context, ValueRange::NonNegative));

    return result;
}

void BasicShapeInset::dump(TextStream& ts) const
{
    ts.dumpProperty("top", top());
    ts.dumpProperty("right", right());
    ts.dumpProperty("bottom", bottom());
    ts.dumpProperty("left", left());

    ts.dumpProperty("top-left-radius", topLeftRadius());
    ts.dumpProperty("top-right-radius", topRightRadius());
    ts.dumpProperty("bottom-right-radius", bottomRightRadius());
    ts.dumpProperty("bottom-left-radius", bottomLeftRadius());
}

static TextStream& operator<<(TextStream& ts, BasicShapeRadius::Type radiusType)
{
    switch (radiusType) {
    case BasicShapeRadius::Type::Value: ts << "value"; break;
    case BasicShapeRadius::Type::ClosestSide: ts << "closest-side"; break;
    case BasicShapeRadius::Type::FarthestSide: ts << "farthest-side"; break;
    case BasicShapeRadius::Type::ClosestCorner: ts << "closest-corner"; break;
    case BasicShapeRadius::Type::FarthestCorner: ts << "farthest-corner"; break;
    }
    return ts;
}

// MARK: -

TextStream& operator<<(TextStream& ts, CoordinateAffinity affinity)
{
    switch (affinity) {
    case CoordinateAffinity::Relative: ts << "relative"_s; break;
    case CoordinateAffinity::Absolute: ts << "absolute"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ControlPointAnchoring anchoring)
{
    switch (anchoring) {
    case ControlPointAnchoring::FromStart: ts << "from start"_s; break;
    case ControlPointAnchoring::FromEnd: ts << "from end"_s; break;
    case ControlPointAnchoring::FromOrigin: ts << "from origin"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, const BasicShapeRadius& radius)
{
    ts.dumpProperty("value", radius.value());
    ts.dumpProperty("type", radius.type());
    return ts;
}

TextStream& operator<<(TextStream& ts, const BasicShapeCenterCoordinate& coordinate)
{
    ts.dumpProperty("length", coordinate.length());
    return ts;
}

TextStream& operator<<(TextStream& ts, const BasicShape& shape)
{
    shape.dump(ts);
    return ts;
}

} // namespace WebCore
