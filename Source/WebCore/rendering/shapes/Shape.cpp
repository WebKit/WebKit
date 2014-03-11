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
#include "BoxShape.h"
#include "CachedImage.h"
#include "FloatSize.h"
#include "ImageBuffer.h"
#include "LengthFunctions.h"
#include "PolygonShape.h"
#include "RasterShape.h"
#include "RectangleShape.h"
#include "WindRule.h"
#include <wtf/MathExtras.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

static PassOwnPtr<Shape> createInsetShape(const FloatRoundedRect& bounds)
{
    ASSERT(bounds.rect().width() >= 0 && bounds.rect().height() >= 0);
    return adoptPtr(new BoxShape(bounds));
}

static PassOwnPtr<Shape> createRectangleShape(const FloatRect& bounds, const FloatSize& radii)
{
    ASSERT(bounds.width() >= 0 && bounds.height() >= 0 && radii.width() >= 0 && radii.height() >= 0);
    return adoptPtr(new RectangleShape(bounds, radii));
}

static PassOwnPtr<Shape> createCircleShape(const FloatPoint& center, float radius)
{
    ASSERT(radius >= 0);
    return adoptPtr(new RectangleShape(FloatRect(center.x() - radius, center.y() - radius, radius*2, radius*2), FloatSize(radius, radius)));
}

static PassOwnPtr<Shape> createEllipseShape(const FloatPoint& center, const FloatSize& radii)
{
    ASSERT(radii.width() >= 0 && radii.height() >= 0);
    return adoptPtr(new RectangleShape(FloatRect(center.x() - radii.width(), center.y() - radii.height(), radii.width()*2, radii.height()*2), radii));
}

static PassOwnPtr<Shape> createPolygonShape(PassOwnPtr<Vector<FloatPoint>> vertices, WindRule fillRule)
{
    return adoptPtr(new PolygonShape(vertices, fillRule));
}

static inline FloatRect physicalRectToLogical(const FloatRect& rect, float logicalBoxHeight, WritingMode writingMode)
{
    if (isHorizontalWritingMode(writingMode))
        return rect;
    if (isFlippedBlocksWritingMode(writingMode))
        return FloatRect(rect.y(), logicalBoxHeight - rect.maxX(), rect.height(), rect.width());
    return rect.transposedRect();
}

static inline FloatPoint physicalPointToLogical(const FloatPoint& point, float logicalBoxHeight, WritingMode writingMode)
{
    if (isHorizontalWritingMode(writingMode))
        return point;
    if (isFlippedBlocksWritingMode(writingMode))
        return FloatPoint(point.y(), logicalBoxHeight - point.x());
    return point.transposedPoint();
}

static inline FloatSize physicalSizeToLogical(const FloatSize& size, WritingMode writingMode)
{
    if (isHorizontalWritingMode(writingMode))
        return size;
    return size.transposedSize();
}

static inline void ensureRadiiDoNotOverlap(FloatRect& bounds, FloatSize& radii)
{
    float widthRatio = bounds.width() / (2 * radii.width());
    float heightRatio = bounds.height() / (2 * radii.height());
    float reductionRatio = std::min<float>(widthRatio, heightRatio);
    if (reductionRatio < 1) {
        radii.setWidth(reductionRatio * radii.width());
        radii.setHeight(reductionRatio * radii.height());
    }
}

PassOwnPtr<Shape> Shape::createShape(const BasicShape* basicShape, const LayoutSize& logicalBoxSize, WritingMode writingMode, Length margin, Length padding)
{
    ASSERT(basicShape);

    bool horizontalWritingMode = isHorizontalWritingMode(writingMode);
    float boxWidth = horizontalWritingMode ? logicalBoxSize.width() : logicalBoxSize.height();
    float boxHeight = horizontalWritingMode ? logicalBoxSize.height() : logicalBoxSize.width();
    OwnPtr<Shape> shape;

    switch (basicShape->type()) {

    case BasicShape::BasicShapeRectangleType: {
        const BasicShapeRectangle& rectangle = *static_cast<const BasicShapeRectangle*>(basicShape);
        FloatRect bounds(
            floatValueForLength(rectangle.x(), boxWidth),
            floatValueForLength(rectangle.y(), boxHeight),
            floatValueForLength(rectangle.width(), boxWidth),
            floatValueForLength(rectangle.height(), boxHeight));
        FloatSize cornerRadii(
            floatValueForLength(rectangle.cornerRadiusX(), boxWidth),
            floatValueForLength(rectangle.cornerRadiusY(), boxHeight));
        ensureRadiiDoNotOverlap(bounds, cornerRadii);
        FloatRect logicalBounds = physicalRectToLogical(bounds, logicalBoxSize.height(), writingMode);

        shape = createRectangleShape(logicalBounds, physicalSizeToLogical(cornerRadii, writingMode));
        break;
    }

    case BasicShape::DeprecatedBasicShapeCircleType: {
        const DeprecatedBasicShapeCircle* circle = static_cast<const DeprecatedBasicShapeCircle*>(basicShape);
        float centerX = floatValueForLength(circle->centerX(), boxWidth);
        float centerY = floatValueForLength(circle->centerY(), boxHeight);
        float radius = floatValueForLength(circle->radius(), sqrtf((boxWidth * boxWidth + boxHeight * boxHeight) / 2));
        FloatPoint logicalCenter = physicalPointToLogical(FloatPoint(centerX, centerY), logicalBoxSize.height(), writingMode);

        shape = createCircleShape(logicalCenter, radius);
        break;
    }

    case BasicShape::BasicShapeCircleType: {
        const BasicShapeCircle* circle = static_cast<const BasicShapeCircle*>(basicShape);
        float centerX = floatValueForCenterCoordinate(circle->centerX(), boxWidth);
        float centerY = floatValueForCenterCoordinate(circle->centerY(), boxHeight);
        float radius = circle->floatValueForRadiusInBox(boxWidth, boxHeight);
        FloatPoint logicalCenter = physicalPointToLogical(FloatPoint(centerX, centerY), logicalBoxSize.height(), writingMode);

        shape = createCircleShape(logicalCenter, radius);
        break;
    }

    case BasicShape::DeprecatedBasicShapeEllipseType: {
        const DeprecatedBasicShapeEllipse* ellipse = static_cast<const DeprecatedBasicShapeEllipse*>(basicShape);
        float centerX = floatValueForLength(ellipse->centerX(), boxWidth);
        float centerY = floatValueForLength(ellipse->centerY(), boxHeight);
        float radiusX = floatValueForLength(ellipse->radiusX(), boxWidth);
        float radiusY = floatValueForLength(ellipse->radiusY(), boxHeight);
        FloatPoint logicalCenter = physicalPointToLogical(FloatPoint(centerX, centerY), logicalBoxSize.height(), writingMode);
        FloatSize logicalRadii = physicalSizeToLogical(FloatSize(radiusX, radiusY), writingMode);

        shape = createEllipseShape(logicalCenter, logicalRadii);
        break;
    }

    case BasicShape::BasicShapeEllipseType: {
        const BasicShapeEllipse* ellipse = static_cast<const BasicShapeEllipse*>(basicShape);
        float centerX = floatValueForCenterCoordinate(ellipse->centerX(), boxWidth);
        float centerY = floatValueForCenterCoordinate(ellipse->centerY(), boxHeight);
        float radiusX = ellipse->floatValueForRadiusInBox(ellipse->radiusX(), centerX, boxWidth);
        float radiusY = ellipse->floatValueForRadiusInBox(ellipse->radiusY(), centerY, boxHeight);
        FloatPoint logicalCenter = physicalPointToLogical(FloatPoint(centerX, centerY), logicalBoxSize.height(), writingMode);

        shape = createEllipseShape(logicalCenter, FloatSize(radiusX, radiusY));
        break;
    }

    case BasicShape::BasicShapePolygonType: {
        const BasicShapePolygon& polygon = *static_cast<const BasicShapePolygon*>(basicShape);
        const Vector<Length>& values = polygon.values();
        size_t valuesSize = values.size();
        ASSERT(!(valuesSize % 2));
        OwnPtr<Vector<FloatPoint>> vertices = adoptPtr(new Vector<FloatPoint>(valuesSize / 2));
        for (unsigned i = 0; i < valuesSize; i += 2) {
            FloatPoint vertex(
                floatValueForLength(values.at(i), boxWidth),
                floatValueForLength(values.at(i + 1), boxHeight));
            (*vertices)[i / 2] = physicalPointToLogical(vertex, logicalBoxSize.height(), writingMode);
        }

        shape = createPolygonShape(vertices.release(), polygon.windRule());
        break;
    }

    case BasicShape::BasicShapeInsetRectangleType: {
        const BasicShapeInsetRectangle& rectangle = *static_cast<const BasicShapeInsetRectangle*>(basicShape);
        float left = floatValueForLength(rectangle.left(), boxWidth);
        float top = floatValueForLength(rectangle.top(), boxHeight);
        FloatRect bounds(
            left,
            top,
            std::max<float>(boxWidth - left - floatValueForLength(rectangle.right(), boxWidth), 0),
            std::max<float>(boxHeight - top - floatValueForLength(rectangle.bottom(), boxHeight), 0));
        FloatSize cornerRadii(
            floatValueForLength(rectangle.cornerRadiusX(), boxWidth),
            floatValueForLength(rectangle.cornerRadiusY(), boxHeight));
        ensureRadiiDoNotOverlap(bounds, cornerRadii);
        FloatRect logicalBounds = physicalRectToLogical(bounds, logicalBoxSize.height(), writingMode);

        shape = createRectangleShape(logicalBounds, physicalSizeToLogical(cornerRadii, writingMode));
        break;
    }

    case BasicShape::BasicShapeInsetType: {
        const BasicShapeInset& inset = *static_cast<const BasicShapeInset*>(basicShape);
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
    shape->m_margin = floatValueForLength(margin, 0);
    shape->m_padding = floatValueForLength(padding, 0);

    return shape.release();
}

PassOwnPtr<Shape> Shape::createRasterShape(Image* image, float threshold, const LayoutRect& imageR, const LayoutRect& marginR, WritingMode writingMode, Length margin, Length padding)
{
    IntRect imageRect = pixelSnappedIntRect(imageR);
    IntRect marginRect = pixelSnappedIntRect(marginR);
    OwnPtr<RasterShapeIntervals> intervals = adoptPtr(new RasterShapeIntervals(marginRect.height(), -marginRect.y()));
    std::unique_ptr<ImageBuffer> imageBuffer = ImageBuffer::create(imageRect.size());

    if (imageBuffer) {
        GraphicsContext* graphicsContext = imageBuffer->context();
        graphicsContext->drawImage(image, ColorSpaceDeviceRGB, IntRect(IntPoint(), imageRect.size()));

        RefPtr<Uint8ClampedArray> pixelArray = imageBuffer->getUnmultipliedImageData(IntRect(IntPoint(), imageRect.size()));
        unsigned pixelArrayLength = pixelArray->length();
        unsigned pixelArrayOffset = 3; // Each pixel is four bytes: RGBA.
        uint8_t alphaPixelThreshold = threshold * 255;

        int minBufferY = std::max(0, marginRect.y() - imageRect.y());
        int maxBufferY = std::min(imageRect.height(), marginRect.maxY() - imageRect.y());

        if (static_cast<unsigned>(imageRect.width() * imageRect.height() * 4) == pixelArrayLength) { // sanity check
            for (int y = minBufferY; y < maxBufferY; ++y) {
                int startX = -1;
                for (int x = 0; x < imageRect.width(); ++x, pixelArrayOffset += 4) {
                    uint8_t alpha = pixelArray->item(pixelArrayOffset);
                    bool alphaAboveThreshold = alpha > alphaPixelThreshold;
                    if (startX == -1 && alphaAboveThreshold) {
                        startX = x;
                    } else if (startX != -1 && (!alphaAboveThreshold || x == imageRect.width() - 1)) {
                        intervals->appendInterval(y + imageRect.y(), startX + imageRect.x(), x + imageRect.x());
                        startX = -1;
                    }
                }
            }
        }
    }

    OwnPtr<RasterShape> rasterShape = adoptPtr(new RasterShape(intervals.release(), marginRect.size()));
    rasterShape->m_writingMode = writingMode;
    rasterShape->m_margin = floatValueForLength(margin, 0);
    rasterShape->m_padding = floatValueForLength(padding, 0);
    return rasterShape.release();
}

PassOwnPtr<Shape> Shape::createLayoutBoxShape(const RoundedRect& roundedRect, WritingMode writingMode, Length margin, Length padding)
{
    ASSERT(roundedRect.rect().width() >= 0 && roundedRect.rect().height() >= 0);

    FloatRect rect(0, 0, roundedRect.rect().width(), roundedRect.rect().height());
    FloatRoundedRect bounds(rect, roundedRect.radii());
    OwnPtr<Shape> shape = adoptPtr(new BoxShape(bounds));
    shape->m_writingMode = writingMode;
    shape->m_margin = floatValueForLength(margin, 0);
    shape->m_padding = floatValueForLength(padding, 0);

    return shape.release();
}

} // namespace WebCore
