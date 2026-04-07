/*
 * Copyright (C) 2006-2026 Apple Inc. All rights reserved.
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
#include <numbers>
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <span>
#include <wtf/MathExtras.h>

namespace WebCore {

static void fillSolidBandsCG(CGContextRef context, CGFloat left, CGFloat length, const FloatRect& clipBounds, std::span<const Gradient::SolidBand> bands)
{
    for (auto& band : bands) {
        CGFloat bandStart = left + band.startOffset * length;
        CGFloat bandEnd = left + band.endOffset * length;
        if (bandStart > bandEnd)
            std::swap(bandStart, bandEnd);
        bandStart = std::max<CGFloat>(bandStart, clipBounds.x());
        bandEnd = std::min<CGFloat>(bandEnd, clipBounds.maxX());
        if (bandStart >= bandEnd)
            continue;
        CGContextSetFillColorWithColor(context, cachedCGColor(band.color).get());
        CGContextFillRect(context, CGRectMake(bandStart, clipBounds.y(), bandEnd - bandStart, clipBounds.height()));
    }
}

static void paintLinearSolidBands(CGContextRef context, FloatPoint point0, FloatPoint point1, const Vector<Gradient::SolidBand>& bands)
{
    if (bands.isEmpty())
        return;

    CGContextStateSaver stateSaver(context);

    // Disable anti-aliasing so rectangle edges at band boundaries are pixel-sharp.
    CGContextSetShouldAntialias(context, false);

    FloatSize gradientVector = point1 - point0;
    float gradientLength = gradientVector.diagonalLength();

    CGContextTranslateCTM(context, point0.x(), point0.y());
    if (gradientLength > 0)
        CGContextRotateCTM(context, FloatPoint(gradientVector).slopeAngleRadians());

    FloatRect clipBounds = CGContextGetClipBoundingBox(context);
    if (clipBounds.isInfinite() || clipBounds.isEmpty())
        return;

    float padStart = gradientLength > 0 ? clipBounds.x() / gradientLength : 0;
    float padEnd = gradientLength > 0 ? clipBounds.maxX() / gradientLength : 1;
    padStart = std::min(padStart, bands.first().startOffset);
    padEnd = std::max(padEnd, bands.last().endOffset);

    if (padStart < bands.first().startOffset) {
        Gradient::SolidBand leadingPad { padStart, bands.first().startOffset, bands.first().color };
        fillSolidBandsCG(context, 0, gradientLength, clipBounds, { &leadingPad, 1 });
    }

    fillSolidBandsCG(context, 0, gradientLength, clipBounds, bands);

    if (padEnd > bands.last().endOffset) {
        Gradient::SolidBand trailingPad { bands.last().endOffset, padEnd, bands.last().color };
        fillSolidBandsCG(context, 0, gradientLength, clipBounds, { &trailingPad, 1 });
    }
}

static void fillConicSector(CGContextRef context, FloatPoint center, float radius, float startAngle, float endAngle, const Color& color, bool flipArcDirection)
{
    CGContextBeginPath(context);
    CGContextMoveToPoint(context, center.x(), center.y());
    CGContextAddArc(context, center.x(), center.y(), radius, startAngle, endAngle, flipArcDirection);
    CGContextClosePath(context);
    CGContextSetFillColorWithColor(context, cachedCGColor(color).get());
    CGContextFillPath(context);
}

static void paintConicSolidBands(CGContextRef context, FloatPoint center, float startAngleRadians, const Vector<Gradient::SolidBand>& bands)
{
    if (bands.isEmpty())
        return;

    CGContextSetShouldAntialias(context, false);

    FloatRect clipBounds = CGContextGetClipBoundingBox(context);
    if (clipBounds.isInfinite() || clipBounds.isEmpty())
        return;

    float dx = std::max(std::abs(clipBounds.x() - center.x()), std::abs(clipBounds.maxX() - center.x()));
    float dy = std::max(std::abs(clipBounds.y() - center.y()), std::abs(clipBounds.maxY() - center.y()));
    float radius = std::hypot(dx, dy);

    // On iOS the native coordinate system has a flipped y-axis relative to
    // macOS, which reverses the arc sweep direction. Detect this and flip.
#if PLATFORM(IOS_FAMILY)
    CGAffineTransform ctm = CGContextGetCTM(context);
    bool flipArcDirection = ctm.a * ctm.d - ctm.b * ctm.c < 0;
#else
    constexpr bool flipArcDirection = false;
#endif

    constexpr float twoPi = 2 * static_cast<float>(std::numbers::pi);

    if (bands.first().startOffset > 0)
        fillConicSector(context, center, radius, startAngleRadians, startAngleRadians + bands.first().startOffset * twoPi, bands.first().color, flipArcDirection);

    for (auto& band : bands)
        fillConicSector(context, center, radius, startAngleRadians + band.startOffset * twoPi, startAngleRadians + band.endOffset * twoPi, band.color, flipArcDirection);

    if (bands.last().endOffset < 1)
        fillConicSector(context, center, radius, startAngleRadians + bands.last().endOffset * twoPi, startAngleRadians + twoPi, bands.last().color, flipArcDirection);
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
    if (!m_platformRenderer)
        m_platformRenderer = GradientRendererCG { m_colorInterpolationMethod, m_stops.sorted() };

    WTF::switchOn(m_data,
        [&] (const LinearData& data) {
            const FloatSize gradientVector = data.point1 - data.point0;
            // Check axis-alignment in screen space by transforming the gradient
            // direction through the CTM's linear part (ignoring translation).
            CGAffineTransform ctm = CGContextGetCTM(platformContext);
            CGFloat screenX = ctm.a * gradientVector.width() + ctm.c * gradientVector.height();
            CGFloat screenY = ctm.b * gradientVector.width() + ctm.d * gradientVector.height();
            bool isAxisAligned = !screenX || !screenY;
            const auto& bands = solidBands();
            bool useSolidBands = isAxisAligned && !bands.isEmpty();

            switch (m_spreadMethod) {
            case GradientSpreadMethod::Repeat:
            case GradientSpreadMethod::Reflect: {
                CGContextStateSaver saveState(platformContext);

                FloatPoint gradientVectorNorm(gradientVector);
                gradientVectorNorm.normalize();
                CGFloat angle = gradientVectorNorm.isZero() ? 0 : atan2(gradientVectorNorm.y(), gradientVectorNorm.x());
                CGContextRotateCTM(platformContext, angle);

                if (useSolidBands)
                    CGContextSetShouldAntialias(platformContext, false);

                CGRect boundingBox = CGContextGetClipBoundingBox(platformContext);
                if (CGRectIsInfinite(boundingBox) || CGRectIsEmpty(boundingBox))
                    break;

                CGAffineTransform transform = CGAffineTransformMakeRotation(-angle);
                FloatPoint point0 = CGPointApplyAffineTransform(data.point0, transform);
                FloatPoint point1 = CGPointApplyAffineTransform(data.point1, transform);
                CGFloat dx = point1.x() - point0.x();

                CGFloat pixelSize = CGFAbs(CGContextConvertSizeToUserSpace(platformContext, CGSizeMake(1, 1)).width);
                if (CGFAbs(dx) < pixelSize)
                    dx = dx < 0 ? -pixelSize : pixelSize;

                auto drawLinearGradient = [&](CGFloat start, CGFloat end, bool flip) {
                    CGFloat left = flip ? end : start;
                    CGFloat right = flip ? start : end;

                    if (useSolidBands) {
                        fillSolidBandsCG(platformContext, left, right - left, FloatRect(boundingBox), bands);
                        return;
                    }

                    CGGradientDrawingOptions extendOptions = 0;
                    m_platformRenderer->drawLinearGradient(platformContext, CGPointMake(left, 0), CGPointMake(right, 0), extendOptions);
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
                        flip = !flip && m_spreadMethod == GradientSpreadMethod::Reflect;
                }

                // Draw gradient forward till the points are outside boundingBox.
                for (; isIntersecting(start, start + dx, boundingBox); start += dx) {
                    drawLinearGradient(start, start + dx, flip);
                    flip = !flip && m_spreadMethod == GradientSpreadMethod::Reflect;
                }

                flip = m_spreadMethod == GradientSpreadMethod::Reflect;
                CGFloat end = point0.x();

                // Should the points be moved backward towards boundingBox?
                if ((dx < 0 && isLeftOf(end, end - dx, boundingBox)) || (dx > 0 && isRightOf(end, end - dx, boundingBox))) {
                    // Move the 'end' point towards boundingBox.
                    for (; !isIntersecting(end, end - dx, boundingBox); end -= dx)
                        flip = !flip && m_spreadMethod == GradientSpreadMethod::Reflect;
                }

                // Draw gradient backward till the points are outside boundingBox.
                for (; isIntersecting(end, end - dx, boundingBox); end -= dx) {
                    drawLinearGradient(end - dx, end, flip);
                    flip = !flip && m_spreadMethod == GradientSpreadMethod::Reflect;
                }
                break;
            }
            case GradientSpreadMethod::Pad: {
                if (useSolidBands) {
                    paintLinearSolidBands(platformContext, data.point0, data.point1, bands);
                    break;
                }
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
            CGContextStateSaver stateSaver(platformContext);
            CGContextTranslateCTM(platformContext, data.point0.x(), data.point0.y());
            CGContextRotateCTM(platformContext, (CGFloat)-piOverTwoDouble);
            CGContextTranslateCTM(platformContext, -data.point0.x(), -data.point0.y());

            const auto& bands = solidBands();
            if (!bands.isEmpty()) {
                paintConicSolidBands(platformContext, data.point0, data.angleRadians, bands);
                return;
            }
            m_platformRenderer->drawConicGradient(platformContext, data.point0, data.angleRadians);
        }
    );
}

}

#endif
