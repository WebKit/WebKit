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
#include "LayoutShape.h"

#include "BoxLayoutShape.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "LengthFunctions.h"
#include "PixelBuffer.h"
#include "PolygonLayoutShape.h"
#include "RasterLayoutShape.h"
#include "RectangleLayoutShape.h"
#include "StylePosition.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "WindRule.h"

namespace WebCore {

static Ref<LayoutShape> createInsetShape(const FloatRoundedRect& bounds)
{
    ASSERT(bounds.rect().width() >= 0 && bounds.rect().height() >= 0);
    return adoptRef(*new BoxLayoutShape(bounds));
}

static Ref<LayoutShape> createCircleShape(const FloatPoint& center, float radius, float boxLogicalWidth)
{
    ASSERT(radius >= 0);
    return adoptRef(*new RectangleLayoutShape(FloatRect(center.x() - radius, center.y() - radius, radius*2, radius*2), FloatSize(radius, radius), boxLogicalWidth));
}

static Ref<LayoutShape> createEllipseShape(const FloatPoint& center, const FloatSize& radii, float boxLogicalWidth)
{
    ASSERT(radii.width() >= 0 && radii.height() >= 0);
    return adoptRef(*new RectangleLayoutShape(FloatRect(center.x() - radii.width(), center.y() - radii.height(), radii.width()*2, radii.height()*2), radii, boxLogicalWidth));
}

static Ref<LayoutShape> createPolygonShape(Vector<FloatPoint>&& vertices, WindRule fillRule, float boxLogicalWidth)
{
    return adoptRef(*new PolygonLayoutShape(WTFMove(vertices), fillRule, boxLogicalWidth));
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

Ref<const LayoutShape> LayoutShape::createShape(const Style::BasicShape& basicShape, const LayoutPoint& borderBoxOffset, const LayoutSize& logicalBoxSize, WritingMode writingMode, float margin)
{
    bool horizontalWritingMode = writingMode.isHorizontal();
    float boxWidth = horizontalWritingMode ? logicalBoxSize.width() : logicalBoxSize.height();
    float boxHeight = horizontalWritingMode ? logicalBoxSize.height() : logicalBoxSize.width();

    auto shape = WTF::switchOn(basicShape,
        [&](const Style::CircleFunction& circle) -> Ref<LayoutShape> {
            auto boxSize = FloatSize { boxWidth, boxHeight };
            auto center = Style::resolvePosition(*circle, boxSize);
            auto radius = Style::resolveRadius(*circle, boxSize, center);

            auto logicalCenter = physicalPointToLogical(center, logicalBoxSize.height(), writingMode);
            logicalCenter.moveBy(borderBoxOffset);

            return createCircleShape(logicalCenter, radius, logicalBoxSize.width());
        },
        [&](const Style::EllipseFunction& ellipse) -> Ref<LayoutShape> {
            auto boxSize = FloatSize { boxWidth, boxHeight };
            auto center = Style::resolvePosition(*ellipse, boxSize);
            auto radii = Style::resolveRadii(*ellipse, boxSize, center);

            auto logicalCenter = physicalPointToLogical(center, logicalBoxSize.height(), writingMode);
            logicalCenter.moveBy(borderBoxOffset);
            if (!horizontalWritingMode)
                radii = radii.transposedSize();

            return createEllipseShape(logicalCenter, radii, logicalBoxSize.width());
        },
        [&](const Style::InsetFunction& inset) -> Ref<LayoutShape> {
            float left = Style::evaluate(inset->insets.left(), boxWidth);
            float top = Style::evaluate(inset->insets.top(), boxHeight);

            FloatRect rect {
                left,
                top,
                std::max<float>(boxWidth - left - Style::evaluate(inset->insets.right(), boxWidth), 0),
                std::max<float>(boxHeight - top - Style::evaluate(inset->insets.bottom(), boxHeight), 0)
            };

            auto logicalRect = physicalRectToLogical(rect, logicalBoxSize.height(), writingMode);
            logicalRect.moveBy(borderBoxOffset);

            auto boxSize = FloatSize(boxWidth, boxHeight);
            auto isBlockLeftToRight = writingMode.isBlockLeftToRight();
            auto topLeftRadius = physicalSizeToLogical(Style::evaluate(horizontalWritingMode || isBlockLeftToRight ? inset->radii.topLeft : inset->radii.topRight, boxSize), writingMode);
            auto topRightRadius = physicalSizeToLogical(Style::evaluate(horizontalWritingMode ? inset->radii.topRight : isBlockLeftToRight ? inset->radii.bottomLeft : inset->radii.bottomRight, boxSize), writingMode);
            auto bottomLeftRadius = physicalSizeToLogical(Style::evaluate(horizontalWritingMode ? inset->radii.bottomLeft : isBlockLeftToRight ? inset->radii.topRight : inset->radii.topLeft, boxSize), writingMode);
            auto bottomRightRadius = physicalSizeToLogical(Style::evaluate(horizontalWritingMode ? inset->radii.bottomRight : isBlockLeftToRight ? inset->radii.bottomRight : inset->radii.bottomLeft, boxSize), writingMode);
            auto cornerRadii = FloatRoundedRect::Radii(topLeftRadius, topRightRadius, bottomLeftRadius, bottomRightRadius);
            if (writingMode.isBidiRTL())
                cornerRadii = { cornerRadii.topRight(), cornerRadii.topLeft(), cornerRadii.bottomRight(), cornerRadii.bottomLeft() };

            cornerRadii.scale(calcBorderRadiiConstraintScaleFor(logicalRect, cornerRadii));
            return createInsetShape(FloatRoundedRect { logicalRect, cornerRadii });
        },
        [&](const Style::PolygonFunction& polygon) -> Ref<LayoutShape> {
            auto boxSize = FloatSize { boxWidth, boxHeight };

            auto vertices = polygon->vertices.value.map([&](const auto& vertex) -> FloatPoint {
                return physicalPointToLogical(Style::evaluate(vertex, boxSize) + borderBoxOffset, logicalBoxSize.height(), writingMode);
            });

            return createPolygonShape(WTFMove(vertices), Style::windRule(*polygon), logicalBoxSize.width());
        },
        [&](const Style::PathFunction&) -> Ref<LayoutShape> {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Style::ShapeFunction&) -> Ref<LayoutShape> {
            RELEASE_ASSERT_NOT_REACHED();
        }
    );

    shape->m_writingMode = writingMode;
    shape->m_margin = margin;

    return shape;
}

Ref<const LayoutShape> LayoutShape::createRasterShape(Image* image, float threshold, const LayoutRect& imageR, const LayoutRect& marginR, WritingMode writingMode, float margin)
{
    ASSERT(marginR.height() >= 0);

    IntRect imageRect = snappedIntRect(imageR);
    IntRect marginRect = snappedIntRect(marginR);
    auto intervals = makeUnique<RasterShapeIntervals>(marginRect.height(), -marginRect.y());
    // FIXME (149420): This buffer should not be unconditionally unaccelerated.
    auto imageBuffer = ImageBuffer::create(imageRect.size(), RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1, DestinationColorSpace::SRGB(), ImageBufferPixelFormat::BGRA8);

    auto createShape = [&]() {
        auto rasterShape = adoptRef(*new RasterLayoutShape(WTFMove(intervals), marginRect.size()));
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

Ref<const LayoutShape> LayoutShape::createBoxShape(const RoundedRect& roundedRect, WritingMode writingMode, float margin)
{
    ASSERT(roundedRect.rect().width() >= 0 && roundedRect.rect().height() >= 0);

    FloatRoundedRect bounds { roundedRect };
    auto shape = adoptRef(*new BoxLayoutShape(bounds));
    shape->m_writingMode = writingMode;
    shape->m_margin = margin;

    return shape;
}

} // namespace WebCore
