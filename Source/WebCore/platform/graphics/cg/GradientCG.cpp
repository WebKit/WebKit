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
#include <atomic>
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <wtf/HashMap.h>
#include <wtf/ListHashSet.h>
#include <wtf/Lock.h>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

namespace {

// Process-wide cache of the renderers of non-transient gradients. Those are painted across frames and
// contexts, in the GPU process also concurrently from the rendering backend work queues and the main
// thread. The cache holds no reference to the gradients: the entries of destroyed gradients and of
// replaced color stops become unreachable and are evicted as least recently used.
class GradientRendererCache {
    WTF_MAKE_NONCOPYABLE(GradientRendererCache);
public:
    GradientRendererCache() = default;

    static GradientRendererCache& singleton()
    {
        static NeverDestroyed<GradientRendererCache> cache;
        return cache;
    }

    GradientRendererCG renderer(const Gradient& gradient, const std::optional<ColorSpace>& colorSpace)
    {
        auto identifier = gradient.rendererIdentifier();
        {
            Locker locker { m_lock };
            if (auto* renderer = findRenderer(identifier, colorSpace))
                return *renderer;
        }

        // Create the renderer without holding the lock. Another thread may create one for the same key concurrently.
        GradientRendererCG renderer { gradient.colorInterpolationMethod(), gradient.stops(), colorSpace };

        Locker locker { m_lock };
        if (auto* cachedRenderer = findRenderer(identifier, colorSpace))
            return *cachedRenderer;
        m_renderers.ensure(identifier, [] {
            return Renderers { };
        }).iterator->value.append({ colorSpace, renderer });
        if (m_identifiers.size() > capacity)
            m_renderers.remove(m_identifiers.takeFirst());
        return renderer;
    }

private:
    // A gradient is usually painted in one color space only.
    struct ColorSpaceRenderer {
        std::optional<ColorSpace> colorSpace;
        GradientRendererCG renderer;
    };
    using Renderers = Vector<ColorSpaceRenderer, 1>;

    const GradientRendererCG* findRenderer(uint64_t identifier, const std::optional<ColorSpace>& colorSpace) WTF_REQUIRES_LOCK(m_lock)
    {
        m_identifiers.appendOrMoveToLast(identifier);
        auto iterator = m_renderers.find(identifier);
        if (iterator == m_renderers.end())
            return nullptr;
        for (auto& entry : iterator->value) {
            if (entry.colorSpace == colorSpace)
                return &entry.renderer;
        }
        return nullptr;
    }

    // Number of gradients whose renderers are kept.
    static constexpr size_t capacity = 512;

    Lock m_lock;
    HashMap<uint64_t, Renderers> m_renderers WTF_GUARDED_BY_LOCK(m_lock);
    // Least recently used first.
    ListHashSet<uint64_t> m_identifiers WTF_GUARDED_BY_LOCK(m_lock);
};

} // namespace

uint64_t Gradient::generateRendererIdentifier()
{
    static std::atomic<uint64_t> lastIdentifier;
    return ++lastIdentifier;
}

void Gradient::fill(GraphicsContext& context, const FloatRect& rect)
{
    context.clip(rect);
    paint(protect(context.platformContext()), renderer(GradientRendererCG::colorSpaceForDestination(context.colorSpace())));
}

GradientRendererCG Gradient::renderer(const std::optional<ColorSpace>& colorSpace) const
{
    if (!m_isTransient)
        return GradientRendererCache::singleton().renderer(*this, colorSpace);
    return GradientRendererCG { m_colorInterpolationMethod, m_stops, colorSpace };
}

void Gradient::paint(CGContextRef platformContext, const GradientRendererCG& renderer) const
{
    WTF::switchOn(m_data,
        [&] (const LinearData& data) {
            switch (m_spreadMethod) {
            case GradientSpreadMethod::Repeat:
            case GradientSpreadMethod::Reflect: {
                CGContextStateSaver saveState(platformContext);
                CGGradientDrawingOptions extendOptions = 0;

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

                    renderer.drawLinearGradient(platformContext, left, right, extendOptions);
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
                CGGradientDrawingOptions extendOptions = kCGGradientDrawsBeforeStartLocation | kCGGradientDrawsAfterEndLocation;
                renderer.drawLinearGradient(platformContext, data.point0, data.point1, extendOptions);
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
            renderer.drawRadialGradient(platformContext, data.point0, data.startRadius, data.point1, data.endRadius, extendOptions);

            if (needScaling)
                CGContextRestoreGState(platformContext);
        },
        [&] (const ConicData& data) {
            CGContextSaveGState(platformContext);
            CGContextTranslateCTM(platformContext, data.point0.x(), data.point0.y());
            CGContextRotateCTM(platformContext, (CGFloat)-piOverTwoDouble);
            CGContextTranslateCTM(platformContext, -data.point0.x(), -data.point0.y());
            renderer.drawConicGradient(platformContext, data.point0, data.angleRadians);
            CGContextRestoreGState(platformContext);
        }
    );
}

}

#endif
