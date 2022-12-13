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
#include "BasicShapeFunctions.h"
#include "CalculationValue.h"
#include "FloatRect.h"
#include "FloatRoundedRect.h"
#include "LengthFunctions.h"
#include "Path.h"
#include "RenderBox.h"
#include "SVGPathByteStream.h"
#include "SVGPathUtilities.h"
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TinyLRUCache.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

void BasicShapeCenterCoordinate::updateComputedLength()
{
    if (m_direction == BasicShapeCenterCoordinate::Direction::TopLeft) {
        m_computedLength = m_length.isUndefined() ? Length(0, LengthType::Fixed) : m_length;
        return;
    }

    if (m_length.isUndefined()) {
        m_computedLength = Length(100, LengthType::Percent);
        return;
    }
    
    m_computedLength = convertTo100PercentMinusLength(m_length);
}

struct SVGPathTransformedByteStream {
    bool operator==(const SVGPathTransformedByteStream& other) const 
    { 
        return other.offset == offset && other.zoom == zoom && other.rawStream == rawStream;
    }
    bool operator!=(const SVGPathTransformedByteStream& other) const { return !(*this == other); }
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
        path.addEllipse(rect);
        return path;
    }
};

struct RoundedRectPathPolicy : public TinyLRUCachePolicy<FloatRoundedRect, Path> {
public:
    static bool isKeyNull(const FloatRoundedRect& rect) { return rect.isEmpty(); }

    static Path createValueForKey(const FloatRoundedRect& rect)
    {
        Path path;
        path.addRoundedRect(rect);
        return path;
    }
};

struct PolygonPathPolicy : TinyLRUCachePolicy<Vector<FloatPoint>, Path> {
public:
    static bool isKeyNull(const Vector<FloatPoint>& points) { return !points.size(); }

    static Path createValueForKey(const Vector<FloatPoint>& points) { return Path::polygonPathFromPoints(points); }
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

bool BasicShapeCircle::operator==(const BasicShape& other) const
{
    if (type() != other.type())
        return false;

    auto& otherCircle = downcast<BasicShapeCircle>(other);
    return m_centerX == otherCircle.m_centerX
        && m_centerY == otherCircle.m_centerY
        && m_radius == otherCircle.m_radius;
}

float BasicShapeCircle::floatValueForRadiusInBox(float boxWidth, float boxHeight) const
{
    if (m_radius.type() == BasicShapeRadius::Type::Value)
        return floatValueForLength(m_radius.value(), std::hypot(boxWidth, boxHeight) / sqrtOfTwoFloat);

    float centerX = floatValueForCenterCoordinate(m_centerX, boxWidth);
    float centerY = floatValueForCenterCoordinate(m_centerY, boxHeight);

    float widthDelta = std::abs(boxWidth - centerX);
    float heightDelta = std::abs(boxHeight - centerY);
    if (m_radius.type() == BasicShapeRadius::Type::ClosestSide)
        return std::min(std::min(std::abs(centerX), widthDelta), std::min(std::abs(centerY), heightDelta));

    // If radius.type() == BasicShapeRadius::Type::FarthestSide.
    return std::max(std::max(std::abs(centerX), widthDelta), std::max(std::abs(centerY), heightDelta));
}

const Path& BasicShapeCircle::path(const FloatRect& boundingBox)
{
    float centerX = floatValueForCenterCoordinate(m_centerX, boundingBox.width());
    float centerY = floatValueForCenterCoordinate(m_centerY, boundingBox.height());
    float radius = floatValueForRadiusInBox(boundingBox.width(), boundingBox.height());

    return cachedEllipsePath(FloatRect(centerX - radius + boundingBox.x(), centerY - radius + boundingBox.y(), radius * 2, radius * 2));
}

bool BasicShapeCircle::canBlend(const BasicShape& other) const
{
    if (type() != other.type())
        return false;

    return radius().canBlend(downcast<BasicShapeCircle>(other).radius());
}

Ref<BasicShape> BasicShapeCircle::blend(const BasicShape& other, const BlendingContext& context) const
{
    ASSERT(type() == other.type());
    auto& otherCircle = downcast<BasicShapeCircle>(other);
    auto result =  BasicShapeCircle::create();

    result->setCenterX(m_centerX.blend(otherCircle.centerX(), context));
    result->setCenterY(m_centerY.blend(otherCircle.centerY(), context));
    result->setRadius(m_radius.blend(otherCircle.radius(), context));
    return result;
}

void BasicShapeCircle::dump(TextStream& ts) const
{
    ts.dumpProperty("center-x", centerX());
    ts.dumpProperty("center-y", centerY());
    ts.dumpProperty("radius", radius());
}

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

bool BasicShapeEllipse::operator==(const BasicShape& other) const
{
    if (type() != other.type())
        return false;

    auto& otherEllipse = downcast<BasicShapeEllipse>(other);
    return m_centerX == otherEllipse.m_centerX
        && m_centerY == otherEllipse.m_centerY
        && m_radiusX == otherEllipse.m_radiusX
        && m_radiusY == otherEllipse.m_radiusY;
}

float BasicShapeEllipse::floatValueForRadiusInBox(const BasicShapeRadius& radius, float center, float boxWidthOrHeight) const
{
    if (radius.type() == BasicShapeRadius::Type::Value)
        return floatValueForLength(radius.value(), std::abs(boxWidthOrHeight));

    float widthOrHeightDelta = std::abs(boxWidthOrHeight - center);
    if (radius.type() == BasicShapeRadius::Type::ClosestSide)
        return std::min(std::abs(center), widthOrHeightDelta);

    ASSERT(radius.type() == BasicShapeRadius::Type::FarthestSide);
    return std::max(std::abs(center), widthOrHeightDelta);
}

const Path& BasicShapeEllipse::path(const FloatRect& boundingBox)
{
    float centerX = floatValueForCenterCoordinate(m_centerX, boundingBox.width());
    float centerY = floatValueForCenterCoordinate(m_centerY, boundingBox.height());
    float radiusX = floatValueForRadiusInBox(m_radiusX, centerX, boundingBox.width());
    float radiusY = floatValueForRadiusInBox(m_radiusY, centerY, boundingBox.height());

    return cachedEllipsePath(FloatRect(centerX - radiusX + boundingBox.x(), centerY - radiusY + boundingBox.y(), radiusX * 2, radiusY * 2));
}

bool BasicShapeEllipse::canBlend(const BasicShape& other) const
{
    if (type() != other.type())
        return false;

    auto& otherEllipse = downcast<BasicShapeEllipse>(other);
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
    return result;
}

void BasicShapeEllipse::dump(TextStream& ts) const
{
    ts.dumpProperty("center-x", centerX());
    ts.dumpProperty("center-y", centerY());
    ts.dumpProperty("radius-x", radiusX());
    ts.dumpProperty("radius-y", radiusY());
}

Ref<BasicShapePolygon> BasicShapePolygon::create(WindRule windRule, Vector<Length>&& values)
{
    return adoptRef(*new BasicShapePolygon(windRule, WTFMove(values)));
}

BasicShapePolygon::BasicShapePolygon(WindRule windRule, Vector<Length>&& values)
    : m_windRule(windRule)
    , m_values(WTFMove(values))
{
}

bool BasicShapePolygon::operator==(const BasicShape& other) const
{
    if (type() != other.type())
        return false;

    auto& otherPolygon = downcast<BasicShapePolygon>(other);
    return m_windRule == otherPolygon.m_windRule
        && m_values == otherPolygon.m_values;
}

const Path& BasicShapePolygon::path(const FloatRect& boundingBox)
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

    auto& otherPolygon = downcast<BasicShapePolygon>(other);
    return values().size() == otherPolygon.values().size() && windRule() == otherPolygon.windRule();
}

Ref<BasicShape> BasicShapePolygon::blend(const BasicShape& other, const BlendingContext& context) const
{
    ASSERT(type() == other.type());

    auto& otherPolygon = downcast<BasicShapePolygon>(other);
    ASSERT(m_values.size() == otherPolygon.values().size());
    ASSERT(!(m_values.size() % 2));

    size_t length = m_values.size();
    auto result = BasicShapePolygon::create();
    if (!length)
        return result;

    result->setWindRule(otherPolygon.windRule());

    for (size_t i = 0; i < length; i = i + 2) {
        result->appendPoint(
            WebCore::blend(otherPolygon.values().at(i), m_values.at(i), context),
            WebCore::blend(otherPolygon.values().at(i + 1), m_values.at(i + 1), context));
    }

    return result;
}

void BasicShapePolygon::dump(TextStream& ts) const
{
    ts.dumpProperty("wind-rule", windRule());
    ts.dumpProperty("path", values());
}

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

const Path& BasicShapePath::path(const FloatRect& boundingBox)
{
    return cachedTransformedByteStreamPath(*m_byteStream, m_zoom, boundingBox.location());
}

bool BasicShapePath::operator==(const BasicShape& other) const
{
    if (type() != other.type())
        return false;

    auto& otherPath = downcast<BasicShapePath>(other);
    return m_zoom == otherPath.m_zoom && m_windRule == otherPath.m_windRule && *m_byteStream == *otherPath.m_byteStream;
}

bool BasicShapePath::canBlend(const BasicShape& other) const
{
    if (type() != other.type())
        return false;

    auto& otherPath = downcast<BasicShapePath>(other);
    return windRule() == otherPath.windRule() && canBlendSVGPathByteStreams(*m_byteStream, *otherPath.pathData());
}

Ref<BasicShape> BasicShapePath::blend(const BasicShape& from, const BlendingContext& context) const
{
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

bool BasicShapeInset::operator==(const BasicShape& other) const
{
    if (type() != other.type())
        return false;

    auto& otherInset = downcast<BasicShapeInset>(other);
    return m_right == otherInset.m_right
        && m_top == otherInset.m_top
        && m_bottom == otherInset.m_bottom
        && m_left == otherInset.m_left
        && m_topLeftRadius == otherInset.m_topLeftRadius
        && m_topRightRadius == otherInset.m_topRightRadius
        && m_bottomRightRadius == otherInset.m_bottomRightRadius
        && m_bottomLeftRadius == otherInset.m_bottomLeftRadius;
}

const Path& BasicShapeInset::path(const FloatRect& boundingBox)
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
    ts.dumpProperty("direction", coordinate.direction() == BasicShapeCenterCoordinate::Direction::TopLeft ? "top left" : "bottom right");
    ts.dumpProperty("length", coordinate.length());
    return ts;
}

TextStream& operator<<(TextStream& ts, const BasicShape& shape)
{
    shape.dump(ts);
    return ts;
}

} // namespace WebCore
