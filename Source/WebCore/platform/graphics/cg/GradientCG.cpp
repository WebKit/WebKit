/*
 * Copyright (C) 2006-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
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
#include "Gradient.h"

#if USE(CG)

#include "GradientRendererCG.h"
#include "GraphicsContextCG.h"
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <wtf/MathExtras.h>

namespace WebCore {

struct SolidStripe {
    float startOffset;
    float endOffset;
    Color color;
};

static Vector<SolidStripe> solidStripesForLinearGradient(const GradientColorStops& stops)
{
    const auto& sortedStops = stops.sorted().stops();
    ASSERT(!sortedStops.isEmpty());

    // Reject gradients with any color transition between stops.
    // Resolved components treat missing (none) as 0.
    for (size_t i = 0; i + 1 < sortedStops.size(); ++i) {
        if (sortedStops[i].offset != sortedStops[i + 1].offset
            && sortedStops[i].color.colorSpaceAndResolvedColorComponents() != sortedStops[i + 1].color.colorSpaceAndResolvedColorComponents())
            return { };
    }

    // Split into stripes at hard-stop boundaries (consecutive stops at the same offset).
    Vector<SolidStripe> stripes;

    size_t start = 0;
    while (start < sortedStops.size()) {
        // Find the next hard-stop boundary.
        size_t boundary = start + 1;
        while (boundary < sortedStops.size() && sortedStops[boundary].offset != sortedStops[boundary - 1].offset)
            ++boundary;

        stripes.append({ sortedStops[start].offset, sortedStops[boundary - 1].offset, sortedStops[start].color });
        start = boundary;
    }

    // Extend first/last stripe to cover the full [0, 1] range.
    if (!stripes.isEmpty()) {
        stripes.first().startOffset = std::min(stripes.first().startOffset, 0.f);
        stripes.last().endOffset = std::max(stripes.last().endOffset, 1.f);
    }

    return stripes;
}

static void paintSolidStripesLinear(CGContextRef platformContext, CGPoint startPoint, CGPoint endPoint, CGGradientDrawingOptions options, const Vector<SolidStripe>& stripes)
{
    ASSERT(!stripes.isEmpty());
    ASSERT(startPoint.y == endPoint.y || startPoint.x == endPoint.x);

    CGRect clipBox = CGContextGetClipBoundingBox(platformContext);
    if (CGRectIsEmpty(clipBox))
        return;
    ASSERT(!CGRectIsInfinite(clipBox));

    // Disable anti-aliasing so adjacent rects tile without seams.
    CGContextStateSaver saveState(platformContext);
    CGContextSetShouldAntialias(platformContext, false);

    bool isHorizontal = startPoint.y == endPoint.y;
    CGFloat axisStart = isHorizontal ? startPoint.x : startPoint.y;
    CGFloat axisEnd = isHorizontal ? endPoint.x : endPoint.y;
    CGFloat axisLen = axisEnd - axisStart;

    // Zero-length gradient line: fill with last stop color.
    if (!axisLen) {
        CGContextSetFillColorWithColor(platformContext, cachedCGColor(stripes.last().color).get());
        CGContextFillRect(platformContext, clipBox);
        return;
    }

    CGFloat crossMin = isHorizontal ? CGRectGetMinY(clipBox) : CGRectGetMinX(clipBox);
    CGFloat crossLen = isHorizontal ? clipBox.size.height : clipBox.size.width;

    CGFloat clipAxisMin = isHorizontal ? CGRectGetMinX(clipBox) : CGRectGetMinY(clipBox);
    CGFloat clipAxisMax = isHorizontal ? CGRectGetMaxX(clipBox) : CGRectGetMaxY(clipBox);

    bool extendBefore = options & kCGGradientDrawsBeforeStartLocation;
    bool extendAfter = options & kCGGradientDrawsAfterEndLocation;

    auto fillRect = [&](CGFloat lo, CGFloat hi, const Color& color) {
        if (hi == lo)
            return;
        CGContextSetFillColorWithColor(platformContext, cachedCGColor(color).get());
        CGRect r = isHorizontal
            ? CGRectMake(lo, crossMin, hi - lo, crossLen)
            : CGRectMake(crossMin, lo, crossLen, hi - lo);
        CGContextFillRect(platformContext, r);
    };

    for (size_t i = 0; i < stripes.size(); ++i) {
        CGFloat a = axisStart + stripes[i].startOffset * axisLen;
        CGFloat b = axisStart + stripes[i].endOffset * axisLen;
        CGFloat lo = std::min(a, b);
        CGFloat hi = std::max(a, b);

        // Extend first/last stripe to clip edges for Pad spread.
        if (!i && extendBefore) {
            if (axisLen >= 0)
                lo = std::min(lo, clipAxisMin);
            else
                hi = std::max(hi, clipAxisMax);
        }
        if (i + 1 == stripes.size() && extendAfter) {
            if (axisLen >= 0)
                hi = std::max(hi, clipAxisMax);
            else
                lo = std::min(lo, clipAxisMin);
        }

        fillRect(lo, hi, stripes[i].color);
    }
}

template<typename DrawTileFunc>
static void tileLinearGradient(CGContextRef platformContext, const Gradient::LinearData& data, GradientSpreadMethod spreadMethod, DrawTileFunc&& drawTile)
{
    CGContextStateSaver saveState(platformContext);
    CGGradientDrawingOptions extendOptions = 0;

    FloatPoint gradientVectorNorm(data.point1 - data.point0);
    gradientVectorNorm.normalize();
    CGFloat angle = gradientVectorNorm.isZero() ? 0 : atan2(gradientVectorNorm.y(), gradientVectorNorm.x());
    CGContextRotateCTM(platformContext, angle);

    CGRect boundingBox = CGContextGetClipBoundingBox(platformContext);
    if (CGRectIsInfinite(boundingBox) || CGRectIsEmpty(boundingBox))
        return;

    CGAffineTransform transform = CGAffineTransformMakeRotation(-angle);
    FloatPoint point0 = CGPointApplyAffineTransform(data.point0, transform);
    FloatPoint point1 = CGPointApplyAffineTransform(data.point1, transform);
    CGFloat dx = point1.x() - point0.x();

    CGFloat pixelSize = CGFAbs(CGContextConvertSizeToUserSpace(platformContext, CGSizeMake(1, 1)).width);
    if (CGFAbs(dx) < pixelSize)
        dx = dx < 0 ? -pixelSize : pixelSize;

    auto drawLinearGradient = [&](CGFloat start, CGFloat end, bool flip) {
        CGPoint left = CGPointMake(flip ? end : start, 0);
        CGPoint right = CGPointMake(flip ? start : end, 0);
        drawTile(platformContext, left, right, extendOptions);
    };

    auto isLeftOf = [](CGFloat start, CGFloat end, CGRect boundingBox) -> bool {
        return std::max(start, end) <= CGRectGetMinX(boundingBox);
    };

    auto isRightOf = [](CGFloat start, CGFloat end, CGRect boundingBox) -> bool {
        return std::min(start, end) >= CGRectGetMaxX(boundingBox);
    };

    auto isIntersecting = [](CGFloat start, CGFloat end, CGRect boundingBox) -> bool {
        return std::min(start, end) < CGRectGetMaxX(boundingBox) && CGRectGetMinX(boundingBox) < std::max(start, end);
    };

    bool flip = false;
    CGFloat start = point0.x();

    // Should the points be moved forward towards boundingBox?
    if ((dx > 0 && isLeftOf(start, start + dx, boundingBox)) || (dx < 0 && isRightOf(start, start + dx, boundingBox))) {
        // Move the 'start' point towards boundingBox.
        for (; !isIntersecting(start, start + dx, boundingBox); start += dx)
            flip = !flip && spreadMethod == GradientSpreadMethod::Reflect;
    }

    // Draw gradient forward till the points are outside boundingBox.
    for (; isIntersecting(start, start + dx, boundingBox); start += dx) {
        drawLinearGradient(start, start + dx, flip);
        flip = !flip && spreadMethod == GradientSpreadMethod::Reflect;
    }

    flip = spreadMethod == GradientSpreadMethod::Reflect;
    CGFloat end = point0.x();

    // Should the points be moved backward towards boundingBox?
    if ((dx < 0 && isLeftOf(end, end - dx, boundingBox)) || (dx > 0 && isRightOf(end, end - dx, boundingBox))) {
        // Move the 'end' point towards boundingBox.
        for (; !isIntersecting(end, end - dx, boundingBox); end -= dx)
            flip = !flip && spreadMethod == GradientSpreadMethod::Reflect;
    }

    // Draw gradient backward till the points are outside boundingBox.
    for (; isIntersecting(end, end - dx, boundingBox); end -= dx) {
        drawLinearGradient(end - dx, end, flip);
        flip = !flip && spreadMethod == GradientSpreadMethod::Reflect;
    }
}

void Gradient::stopsChanged()
{
    m_platformRenderer = { };
}

void Gradient::fill(GraphicsContext& context, const FloatRect& rect)
{
    context.clip(rect);
    paint(context);
}

void Gradient::paint(GraphicsContext& context)
{
    paint(context.platformContext());
}

void Gradient::paint(CGContextRef platformContext)
{
    if (m_stops.isEmpty())
        return;

    if (!m_platformRenderer)
        m_platformRenderer = GradientRendererCG { m_colorInterpolationMethod, m_stops.sorted() };

    WTF::switchOn(m_data,
        [&] (const LinearData& data) {
            // Fast path: bypass CG's gradient dithering for solid-color stripes.
            bool isAxisAligned = (data.point0.x() == data.point1.x())
                || (data.point0.y() == data.point1.y());
            if (isAxisAligned) {
                auto stripes = solidStripesForLinearGradient(m_stops);
                if (!stripes.isEmpty()) {
                    switch (m_spreadMethod) {
                    case GradientSpreadMethod::Pad: {
                        CGGradientDrawingOptions extendOptions = kCGGradientDrawsBeforeStartLocation | kCGGradientDrawsAfterEndLocation;
                        paintSolidStripesLinear(platformContext, data.point0, data.point1, extendOptions, stripes);
                        return;
                    }
                    case GradientSpreadMethod::Repeat:
                    case GradientSpreadMethod::Reflect:
                        tileLinearGradient(platformContext, data, m_spreadMethod,
                            [&](CGContextRef context, CGPoint left, CGPoint right, CGGradientDrawingOptions opts) {
                                paintSolidStripesLinear(context, left, right, opts, stripes);
                            });
                        return;
                    }
                }
            }

            // Normal path.
            switch (m_spreadMethod) {
            case GradientSpreadMethod::Repeat:
            case GradientSpreadMethod::Reflect:
                tileLinearGradient(platformContext, data, m_spreadMethod,
                    [&](CGContextRef context, CGPoint left, CGPoint right, CGGradientDrawingOptions opts) {
                        m_platformRenderer->drawLinearGradient(context, left, right, opts);
                    });
                break;
            case GradientSpreadMethod::Pad: {
                CGGradientDrawingOptions extendOptions = kCGGradientDrawsBeforeStartLocation | kCGGradientDrawsAfterEndLocation;
                m_platformRenderer->drawLinearGradient(platformContext, data.point0, data.point1, extendOptions);
                break;
            }
            }
        },
        [&] (const RadialData& data) {
            bool needScaling = data.aspectRatio != 1;
            if (needScaling) {
                CGContextSaveGState(platformContext);
                // Scale from the center of the gradient. We only ever scale non-deprecated gradients,
                // for which point0 == point1.
                ASSERT(data.point0 == data.point1);
                CGContextTranslateCTM(platformContext, data.point0.x(), data.point0.y());
                CGContextScaleCTM(platformContext, 1, 1 / data.aspectRatio);
                CGContextTranslateCTM(platformContext, -data.point0.x(), -data.point0.y());
            }

            CGGradientDrawingOptions extendOptions = kCGGradientDrawsBeforeStartLocation | kCGGradientDrawsAfterEndLocation;
            m_platformRenderer->drawRadialGradient(platformContext, data.point0, data.startRadius, data.point1, data.endRadius, extendOptions);

            if (needScaling)
                CGContextRestoreGState(platformContext);
        },
        [&] (const ConicData& data) {
            CGContextSaveGState(platformContext);
            CGContextTranslateCTM(platformContext, data.point0.x(), data.point0.y());
            CGContextRotateCTM(platformContext, (CGFloat)-piOverTwoDouble);
            CGContextTranslateCTM(platformContext, -data.point0.x(), -data.point0.y());
            m_platformRenderer->drawConicGradient(platformContext, data.point0, data.angleRadians);
            CGContextRestoreGState(platformContext);
        }
    );
}

}

#endif
