/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc.  All rights reserved.
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

#include "GraphicsContextCG.h"
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <wtf/RetainPtr.h>

namespace WebCore {

void Gradient::stopsChanged()
{
    m_gradient = nullptr;
}

bool Gradient::hasOnlyBoundedSRGBColorStops() const
{
    for (const auto& stop : m_stops) {
        if (stop.color.colorSpace() != ColorSpace::SRGB)
            return false;
    }
    return true;
}

void Gradient::createCGGradient()
{
    sortStops();

    unsigned numStops = m_stops.size();

    const int reservedStops = 3;
    Vector<CGFloat, reservedStops> locations;
    locations.reserveInitialCapacity(numStops);

    // If all the stops are bounded sRGB (as represented by the color having the color space
    // ColorSpace::SRGB), it is faster to create a gradient using components than CGColors.
    if (hasOnlyBoundedSRGBColorStops()) {
        Vector<CGFloat, 4 * reservedStops> colorComponents;
        colorComponents.reserveInitialCapacity(numStops * 4);

        for (const auto& stop : m_stops) {
            auto [colorSpace, components] = stop.color.colorSpaceAndComponents();
            auto [r, g, b, a] = components;
            colorComponents.uncheckedAppend(r);
            colorComponents.uncheckedAppend(g);
            colorComponents.uncheckedAppend(b);
            colorComponents.uncheckedAppend(a);

            locations.uncheckedAppend(stop.offset);
        }

        m_gradient = adoptCF(CGGradientCreateWithColorComponents(sRGBColorSpaceRef(), colorComponents.data(), locations.data(), numStops));
        return;
    }

    auto colorsArray = adoptCF(CFArrayCreateMutable(0, m_stops.size(), &kCFTypeArrayCallBacks));
    for (const auto& stop : m_stops) {
        CFArrayAppendValue(colorsArray.get(), cachedCGColor(stop.color).get());
        locations.uncheckedAppend(stop.offset);
    }

#if HAVE(CORE_GRAPHICS_EXTENDED_SRGB_COLOR_SPACE)
    auto extendedColorsGradientColorSpace = extendedSRGBColorSpaceRef();
#else
    auto extendedColorsGradientColorSpace = sRGBColorSpaceRef();
#endif

    m_gradient = adoptCF(CGGradientCreateWithColors(extendedColorsGradientColorSpace, colorsArray.get(), locations.data()));
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
    CGGradientDrawingOptions extendOptions = kCGGradientDrawsBeforeStartLocation | kCGGradientDrawsAfterEndLocation;

    if (!m_gradient)
        createCGGradient();

    WTF::switchOn(m_data,
        [&] (const LinearData& data) {
            switch (m_spreadMethod) {
            case GradientSpreadMethod::Repeat:
            case GradientSpreadMethod::Reflect: {
                CGContextStateSaver saveState(platformContext);
                extendOptions = 0;

                FloatPoint gradientVectorNorm(data.point1 - data.point0);
                gradientVectorNorm.normalize();
                CGFloat angle = gradientVectorNorm.isZero() ? 0 : atan2(gradientVectorNorm.y(), gradientVectorNorm.x());
                CGContextRotateCTM(platformContext, angle);

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
                    CGPoint left = CGPointMake(flip ? end : start, 0);
                    CGPoint right = CGPointMake(flip ? start : end, 0);

                    CGContextDrawLinearGradient(platformContext, m_gradient.get(), left, right, extendOptions);
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
            case GradientSpreadMethod::Pad:
                CGContextDrawLinearGradient(platformContext, m_gradient.get(), data.point0, data.point1, extendOptions);
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

            CGContextDrawRadialGradient(platformContext, m_gradient.get(), data.point0, data.startRadius, data.point1, data.endRadius, extendOptions);

            if (needScaling)
                CGContextRestoreGState(platformContext);
        },
        [&] (const ConicData& data) {
// FIXME: Seems like this should be HAVE(CG_CONTEXT_DRAW_CONIC_GRADIENT).
// FIXME: Can we change tvOS to be like the other Cocoa platforms?
#if PLATFORM(COCOA) && !PLATFORM(APPLETV)
            CGContextSaveGState(platformContext);
            CGContextTranslateCTM(platformContext, data.point0.x(), data.point0.y());
            CGContextRotateCTM(platformContext, (CGFloat)-M_PI_2);
            CGContextTranslateCTM(platformContext, -data.point0.x(), -data.point0.y());
            CGContextDrawConicGradient(platformContext, m_gradient.get(), data.point0, data.angleRadians);
            CGContextRestoreGState(platformContext);
#else
            UNUSED_PARAM(data);
#endif
        }
    );
}

}

#endif
