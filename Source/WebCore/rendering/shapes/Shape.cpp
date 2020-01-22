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

#include "BasicShapeFunctions.h"
#include "BasicShapes.h"
#include "BoxShape.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "LengthFunctions.h"
#include "PolygonShape.h"
#include "RasterShape.h"
#include "RectangleShape.h"
#include "WindRule.h"

namespace WebCore {

static std::unique_ptr<Shape> createInsetShape(const FloatRoundedRect& bounds)
{
    ASSERT(bounds.rect().width() >= 0 && bounds.rect().height() >= 0);
    return makeUnique<BoxShape>(bounds);
}

static std::unique_ptr<Shape> createCircleShape(const FloatPoint& center, float radius)
{
    ASSERT(radius >= 0);
    return makeUnique<RectangleShape>(FloatRect(center.x() - radius, center.y() - radius, radius*2, radius*2), FloatSize(radius, radius));
}

static std::unique_ptr<Shape> createEllipseShape(const FloatPoint& center, const FloatSize& radii)
{
    ASSERT(radii.width() >= 0 && radii.height() >= 0);
    return makeUnique<RectangleShape>(FloatRect(center.x() - radii.width(), center.y() - radii.height(), radii.width()*2, radii.height()*2), radii);
}

static std::unique_ptr<Shape> createPolygonShape(std::unique_ptr<Vector<FloatPoint>> vertices, WindRule fillRule)
{
    return makeUnique<PolygonShape>(WTFMove(vertices), fillRule);
}

static inline FloatRect physicalRectToLogical(const FloatRect& rect, float logicalBoxHeight, WritingMode writingMode)
{
    if (isHorizontalWritingMode(writingMode))
        return rect;
    if (isFlippedWritingMode(writingMode))
        return FloatRect(rect.y(), logicalBoxHeight - rect.maxX(), rect.height(), rect.width());
    return rect.transposedRect();
}

static inline FloatPoint physicalPointToLogical(const FloatPoint& point, float logicalBoxHeight, WritingMode writingMode)
{
    if (isHorizontalWritingMode(writingMode))
        return point;
    if (isFlippedWritingMode(writingMode))
        return FloatPoint(point.y(), logicalBoxHeight - point.x());
    return point.transposedPoint();
}

static inline FloatSize physicalSizeToLogical(const FloatSize& size, WritingMode writingMode)
{
    if (isHorizontalWritingMode(writingMode))
        return size;
    return size.transposedSize();
}

std::unique_ptr<Shape> Shape::createShape(const BasicShape& basicShape, const LayoutSize& logicalBoxSize, WritingMode writingMode, float margin)
{
    bool horizontalWritingMode = isHorizontalWritingMode(writingMode);
    float boxWidth = horizontalWritingMode ? logicalBoxSize.width() : logicalBoxSize.height();
    float boxHeight = horizontalWritingMode ? logicalBoxSize.height() : logicalBoxSize.width();
    std::unique_ptr<Shape> shape;

    switch (basicShape.type()) {

    case BasicShape::Type::Circle: {
        const auto& circle = downcast<BasicShapeCircle>(basicShape);
        float centerX = floatValueForCenterCoordinate(circle.centerX(), boxWidth);
        float centerY = floatValueForCenterCoordinate(circle.centerY(), boxHeight);
        float radius = circle.floatValueForRadiusInBox(boxWidth, boxHeight);
        FloatPoint logicalCenter = physicalPointToLogical(FloatPoint(centerX, centerY), logicalBoxSize.height(), writingMode);

        shape = createCircleShape(logicalCenter, radius);
        break;
    }

    case BasicShape::Type::Ellipse: {
        const auto& ellipse = downcast<BasicShapeEllipse>(basicShape);
        float centerX = floatValueForCenterCoordinate(ellipse.centerX(), boxWidth);
        float centerY = floatValueForCenterCoordinate(ellipse.centerY(), boxHeight);
        float radiusX = ellipse.floatValueForRadiusInBox(ellipse.radiusX(), centerX, boxWidth);
        float radiusY = ellipse.floatValueForRadiusInBox(ellipse.radiusY(), centerY, boxHeight);
        FloatPoint logicalCenter = physicalPointToLogical(FloatPoint(centerX, centerY), logicalBoxSize.height(), writingMode);

        shape = createEllipseShape(logicalCenter, FloatSize(radiusX, radiusY));
        break;
    }

    case BasicShape::Type::Polygon: {
        const auto& polygon = downcast<BasicShapePolygon>(basicShape);
        const Vector<Length>& values = polygon.values();
        size_t valuesSize = values.size();
        ASSERT(!(valuesSize % 2));
        std::unique_ptr<Vector<FloatPoint>> vertices = makeUnique<Vector<FloatPoint>>(valuesSize / 2);
        for (unsigned i = 0; i < valuesSize; i += 2) {
            FloatPoint vertex(
                floatValueForLength(values.at(i), boxWidth),
                floatValueForLength(values.at(i + 1), boxHeight));
            (*vertices)[i / 2] = physicalPointToLogical(vertex, logicalBoxSize.height(), writingMode);
        }

        shape = createPolygonShape(WTFMove(vertices), polygon.windRule());
        break;
    }

    case BasicShape::Type::Inset: {
        const auto& inset = downcast<BasicShapeInset>(basicShape);
        float left = floatValueForLength(inset.left(), boxWidth);
        float top = floatValueForLength(inset.top(), boxHeight);
        FloatRect rect(left,
            top,
            std::max<float>(boxWidth - left - floatValueForLength(inset.right(), boxWidth), 0),
            std::max<float>(boxHeight - top - floatValueForLength(inset.bottom(), boxHeight), 0));
        FloatRect logicalRect = physicalRectToLogical(rect, logicalBoxSize.height(), writingMode);

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

    return shape;
}

std::unique_ptr<Shape> Shape::createRasterShape(Image* image, float threshold, const LayoutRect& imageR, const LayoutRect& marginR, WritingMode writingMode, float margin)
{
    ASSERT(marginR.height() >= 0);

    IntRect imageRect = snappedIntRect(imageR);
    IntRect marginRect = snappedIntRect(marginR);
    auto intervals = makeUnique<RasterShapeIntervals>(marginRect.height(), -marginRect.y());
    // FIXME (149420): This buffer should not be unconditionally unaccelerated.
    std::unique_ptr<ImageBuffer> imageBuffer = ImageBuffer::create(imageRect.size(), RenderingMode::Unaccelerated);

    if (imageBuffer) {
        GraphicsContext& graphicsContext = imageBuffer->context();
        if (image)
            graphicsContext.drawImage(*image, IntRect(IntPoint(), imageRect.size()));

        RefPtr<Uint8ClampedArray> pixelArray = imageBuffer->getUnmultipliedImageData(IntRect(IntPoint(), imageRect.size()));
        RELEASE_ASSERT(pixelArray);
        unsigned pixelArrayLength = pixelArray->length();
        unsigned pixelArrayOffset = 3; // Each pixel is four bytes: RGBA.
        uint8_t alphaPixelThreshold = static_cast<uint8_t>(lroundf(clampTo<float>(threshold, 0, 1) * 255.0f));

        int minBufferY = std::max(0, marginRect.y() - imageRect.y());
        int maxBufferY = std::min(imageRect.height(), marginRect.maxY() - imageRect.y());

        if ((imageRect.area() * 4).unsafeGet() == pixelArrayLength) {
            for (int y = minBufferY; y < maxBufferY; ++y) {
                int startX = -1;
                for (int x = 0; x < imageRect.width(); ++x, pixelArrayOffset += 4) {
                    uint8_t alpha = pixelArray->item(pixelArrayOffset);
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
    }

    auto rasterShape = makeUnique<RasterShape>(WTFMove(intervals), marginRect.size());
    rasterShape->m_writingMode = writingMode;
    rasterShape->m_margin = margin;
    return rasterShape;
}

std::unique_ptr<Shape> Shape::createBoxShape(const RoundedRect& roundedRect, WritingMode writingMode, float margin)
{
    ASSERT(roundedRect.rect().width() >= 0 && roundedRect.rect().height() >= 0);

    FloatRect rect(0, 0, roundedRect.rect().width(), roundedRect.rect().height());
    FloatRoundedRect bounds(rect, roundedRect.radii());
    auto shape = makeUnique<BoxShape>(bounds);
    shape->m_writingMode = writingMode;
    shape->m_margin = margin;

    return shape;
}

} // namespace WebCore
