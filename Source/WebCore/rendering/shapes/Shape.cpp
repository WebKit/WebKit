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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Shape.h"

#include "BasicShapeConversion.h"
#include "BasicShapes.h"
#include "BoxShape.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "LengthFunctions.h"
#include "PixelBuffer.h"
#include "PolygonShape.h"
#include "RasterShape.h"
#include "RectangleShape.h"
#include "WindRule.h"

namespace WebCore {

static Ref<Shape> createInsetShape(const FloatRoundedRect& bounds)
{
    ASSERT(bounds.rect().width() >= 0 && bounds.rect().height() >= 0);
    return adoptRef(*new BoxShape(bounds));
}

static Ref<Shape> createCircleShape(const FloatPoint& center, float radius)
{
    ASSERT(radius >= 0);
    return adoptRef(*new RectangleShape(FloatRect(center.x() - radius, center.y() - radius, radius*2, radius*2), FloatSize(radius, radius)));
}

static Ref<Shape> createEllipseShape(const FloatPoint& center, const FloatSize& radii)
{
    ASSERT(radii.width() >= 0 && radii.height() >= 0);
    return adoptRef(*new RectangleShape(FloatRect(center.x() - radii.width(), center.y() - radii.height(), radii.width()*2, radii.height()*2), radii));
}

static Ref<Shape> createPolygonShape(Vector<FloatPoint>&& vertices, WindRule fillRule)
{
    return adoptRef(*new PolygonShape(WTFMove(vertices), fillRule));
}

static inline FloatRect physicalRectToLogical(const FloatRect& rect, float logicalBoxHeight, WritingMode writingMode)
{
    if (writingMode.isHorizontal())
        return rect;
    if (writingMode.isBlockFlipped())
        return FloatRect(rect.y(), logicalBoxHeight - rect.maxX(), rect.height(), rect.width());
    return rect.transposedRect();
}

static inline FloatPoint physicalPointToLogical(const FloatPoint& point, float logicalBoxHeight, WritingMode writingMode)
{
    if (writingMode.isHorizontal())
        return point;
    if (writingMode.isBlockFlipped())
        return FloatPoint(point.y(), logicalBoxHeight - point.x());
    return point.transposedPoint();
}

static inline FloatSize physicalSizeToLogical(const FloatSize& size, WritingMode writingMode)
{
    if (writingMode.isHorizontal())
        return size;
    return size.transposedSize();
}

Ref<const Shape> Shape::createShape(const BasicShape& basicShape, const LayoutPoint& borderBoxOffset, const LayoutSize& logicalBoxSize, WritingMode writingMode, float margin)
{
    bool horizontalWritingMode = writingMode.isHorizontal();
    float boxWidth = horizontalWritingMode ? logicalBoxSize.width() : logicalBoxSize.height();
    float boxHeight = horizontalWritingMode ? logicalBoxSize.height() : logicalBoxSize.width();
    RefPtr<Shape> shape;

    switch (basicShape.type()) {

    case BasicShape::Type::Circle: {
        const auto& circle = uncheckedDowncast<BasicShapeCircle>(basicShape);
        float centerX = floatValueForCenterCoordinate(circle.centerX(), boxWidth);
        float centerY = floatValueForCenterCoordinate(circle.centerY(), boxHeight);
        float radius = circle.floatValueForRadiusInBox({ boxWidth, boxHeight }, { centerX, centerY });
        FloatPoint logicalCenter = physicalPointToLogical(FloatPoint(centerX, centerY), logicalBoxSize.height(), writingMode);
        logicalCenter.moveBy(borderBoxOffset);

        shape = createCircleShape(logicalCenter, radius);
        break;
    }

    case BasicShape::Type::Ellipse: {
        const auto& ellipse = uncheckedDowncast<BasicShapeEllipse>(basicShape);
        float centerX = floatValueForCenterCoordinate(ellipse.centerX(), boxWidth);
        float centerY = floatValueForCenterCoordinate(ellipse.centerY(), boxHeight);
        auto center = FloatPoint { centerX, centerY };
        auto radius = ellipse.floatSizeForRadiusInBox({ boxWidth, boxHeight }, center);
        FloatPoint logicalCenter = physicalPointToLogical(center, logicalBoxSize.height(), writingMode);
        logicalCenter.moveBy(borderBoxOffset);
        shape = createEllipseShape(logicalCenter, radius);
        break;
    }

    case BasicShape::Type::Polygon: {
        const auto& polygon = uncheckedDowncast<BasicShapePolygon>(basicShape);
        const Vector<Length>& values = polygon.values();
        size_t valuesSize = values.size();
        ASSERT(!(valuesSize % 2));
        Vector<FloatPoint> vertices(valuesSize / 2);
        for (unsigned i = 0; i < valuesSize; i += 2) {
            FloatPoint vertex(
                floatValueForLength(values.at(i), boxWidth),
                floatValueForLength(values.at(i + 1), boxHeight));
            vertex.moveBy(borderBoxOffset);
            vertices[i / 2] = physicalPointToLogical(vertex, logicalBoxSize.height(), writingMode);
        }
        shape = createPolygonShape(WTFMove(vertices), polygon.windRule());
        break;
    }

    case BasicShape::Type::Inset: {
        const auto& inset = uncheckedDowncast<BasicShapeInset>(basicShape);
        float left = floatValueForLength(inset.left(), boxWidth);
        float top = floatValueForLength(inset.top(), boxHeight);
        FloatRect rect(left,
            top,
            std::max<float>(boxWidth - left - floatValueForLength(inset.right(), boxWidth), 0),
            std::max<float>(boxHeight - top - floatValueForLength(inset.bottom(), boxHeight), 0));
        FloatRect logicalRect = physicalRectToLogical(rect, logicalBoxSize.height(), writingMode);
        logicalRect.moveBy(borderBoxOffset);

        FloatSize boxSize(boxWidth, boxHeight);
        FloatSize topLeftRadius = physicalSizeToLogical(floatSizeForLengthSize(inset.topLeftRadius(), boxSize), writingMode);
        FloatSize topRightRadius = physicalSizeToLogical(floatSizeForLengthSize(inset.topRightRadius(), boxSize), writingMode);
        FloatSize bottomLeftRadius = physicalSizeToLogical(floatSizeForLengthSize(inset.bottomLeftRadius(), boxSize), writingMode);
        FloatSize bottomRightRadius = physicalSizeToLogical(floatSizeForLengthSize(inset.bottomRightRadius(), boxSize), writingMode);
        FloatRoundedRect::Radii cornerRadii(topLeftRadius, topRightRadius, bottomLeftRadius, bottomRightRadius);

        cornerRadii.scale(calcBorderRadiiConstraintScaleFor(logicalRect, cornerRadii));

        shape = createInsetShape(FloatRoundedRect(logicalRect, cornerRadii));
        break;
    }

    default:
        ASSERT_NOT_REACHED();
    }

    shape->m_writingMode = writingMode;
    shape->m_margin = margin;

    return shape.releaseNonNull();
}

Ref<const Shape> Shape::createRasterShape(Image* image, float threshold, const LayoutRect& imageR, const LayoutRect& marginR, WritingMode writingMode, float margin)
{
    ASSERT(marginR.height() >= 0);

    IntRect imageRect = snappedIntRect(imageR);
    IntRect marginRect = snappedIntRect(marginR);
    auto intervals = makeUnique<RasterShapeIntervals>(marginRect.height(), -marginRect.y());
    // FIXME (149420): This buffer should not be unconditionally unaccelerated.
    auto imageBuffer = ImageBuffer::create(imageRect.size(), RenderingPurpose::Unspecified, 1, DestinationColorSpace::SRGB(), ImageBufferPixelFormat::BGRA8);

    auto createShape = [&]() {
        auto rasterShape = adoptRef(*new RasterShape(WTFMove(intervals), marginRect.size()));
        rasterShape->m_writingMode = writingMode;
        rasterShape->m_margin = margin;
        return rasterShape;
    };

    if (!imageBuffer)
        return createShape();

    GraphicsContext& graphicsContext = imageBuffer->context();
    if (image)
        graphicsContext.drawImage(*image, IntRect(IntPoint(), imageRect.size()));

    PixelBufferFormat format { AlphaPremultiplication::Unpremultiplied, PixelFormat::RGBA8, DestinationColorSpace::SRGB() };
    auto pixelBuffer = imageBuffer->getPixelBuffer(format, { IntPoint(), imageRect.size() });
    
    // We could get to a value where PixelBuffer could be nullptr because ImageRect.size()
    // is huge and the data size overflows. Refer rdar://problem/61793884.
    if (!pixelBuffer)
        return createShape();

    if (imageRect.area() * 4 == pixelBuffer->bytes().size()) {
        unsigned pixelArrayOffset = 3; // Each pixel is four bytes: RGBA.
        uint8_t alphaPixelThreshold = static_cast<uint8_t>(lroundf(clampTo<float>(threshold, 0, 1) * 255.0f));

        int minBufferY = std::max(0, marginRect.y() - imageRect.y());
        int maxBufferY = std::min(imageRect.height(), marginRect.maxY() - imageRect.y());

        for (int y = minBufferY; y < maxBufferY; ++y) {
            int startX = -1;
            for (int x = 0; x < imageRect.width(); ++x, pixelArrayOffset += 4) {
                uint8_t alpha = pixelBuffer->item(pixelArrayOffset);
                bool alphaAboveThreshold = alpha > alphaPixelThreshold;
                if (startX == -1 && alphaAboveThreshold) {
                    startX = x;
                } else if (startX != -1 && (!alphaAboveThreshold || x == imageRect.width() - 1)) {
                    // We're creating "end-point exclusive" intervals here. The value of an interval's x1 is
                    // the first index of an above-threshold pixel for y, and the value of x2 is 1+ the index
                    // of the last above-threshold pixel.
                    int endX = alphaAboveThreshold ? x + 1 : x;
                    intervals->intervalAt(y + imageRect.y()).unite(IntShapeInterval(startX + imageRect.x(), endX + imageRect.x()));
                    startX = -1;
                }
            }
        }
    }

    return createShape();
}

Ref<const Shape> Shape::createBoxShape(const RoundedRect& roundedRect, WritingMode writingMode, float margin)
{
    ASSERT(roundedRect.rect().width() >= 0 && roundedRect.rect().height() >= 0);

    FloatRoundedRect bounds { roundedRect };
    auto shape = adoptRef(*new BoxShape(bounds));
    shape->m_writingMode = writingMode;
    shape->m_margin = margin;

    return shape;
}

} // namespace WebCore
