/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Sencha, Inc. All rights reserved.
 * Copyright (C) 2010 Igalia S.L. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2013 Digia Plc. and/or its subsidiary(-ies).
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
#include "ShadowBlur.h"

#include "AffineTransform.h"
#include "FloatQuad.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "PixelBuffer.h"
#include "Timer.h"
#include <wtf/Lock.h>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Noncopyable.h>
#include <wtf/RunLoop.h>
#include <wtf/Scope.h>

namespace WebCore {

enum {
    LeftLobe = 0,
    RightLobe = 1
};

#if USE(CG)
static inline int roundUpToMultipleOf32(int d)
{
    return (1 + (d >> 5)) << 5;
}

// ShadowBlur needs a scratch image as the buffer for the blur filter.
// Instead of creating and destroying the buffer for every operation,
// we create a buffer which will be automatically purged via a timer.
class ScratchBuffer {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ScratchBuffer()
        : m_purgeTimer(RunLoop::main(), this, &ScratchBuffer::purgeTimerFired)
    {
    }

    RefPtr<ImageBuffer> getScratchBuffer(const IntSize& size) WTF_REQUIRES_LOCK(lock())
    {
        ASSERT(lock().isHeld());
        auto releaseLayerImage = makeScopeExit([this] {
            if (m_imageBuffer)
                scheduleScratchBufferPurge();
        });

        // We do not need to recreate the buffer if the current buffer is large enough.
        if (m_imageBuffer && m_imageBuffer->logicalSize().width() >= size.width() && m_imageBuffer->logicalSize().height() >= size.height())
            return m_imageBuffer;

        // Round to the nearest 32 pixels so we do not grow the buffer for similar sized requests.
        IntSize roundedSize(roundUpToMultipleOf32(size.width()), roundUpToMultipleOf32(size.height()));

        clearScratchBuffer();

        // ShadowBlur is not used with accelerated drawing, so it's OK to make an unconditionally unaccelerated buffer.
        m_imageBuffer = ImageBuffer::create(roundedSize, RenderingMode::Unaccelerated, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
        return m_imageBuffer;
    }

    bool setCachedShadowValues(const FloatSize& radius, const Color& color, const FloatRect& shadowRect, const FloatRoundedRect::Radii& radii, const FloatSize& layerSize) WTF_REQUIRES_LOCK(lock())
    {
        ASSERT(lock().isHeld());
        if (!m_lastWasInset && m_lastRadius == radius && m_lastColor == color && m_lastShadowRect == shadowRect &&  m_lastRadii == radii && m_lastLayerSize == layerSize)
            return false;

        m_lastWasInset = false;
        m_lastRadius = radius;
        m_lastColor = color;
        m_lastShadowRect = shadowRect;
        m_lastRadii = radii;
        m_lastLayerSize = layerSize;

        return true;
    }

    bool setCachedInsetShadowValues(const FloatSize& radius, const Color& color, const FloatRect& bounds, const FloatRect& shadowRect, const FloatRoundedRect::Radii& radii) WTF_REQUIRES_LOCK(lock())
    {
        ASSERT(lock().isHeld());
        if (m_lastWasInset && m_lastRadius == radius && m_lastColor == color && m_lastInsetBounds == bounds && shadowRect == m_lastShadowRect && radii == m_lastRadii)
            return false;

        m_lastWasInset = true;
        m_lastInsetBounds = bounds;
        m_lastRadius = radius;
        m_lastColor = color;
        m_lastShadowRect = shadowRect;
        m_lastRadii = radii;

        return true;
    }

    static ScratchBuffer& singleton() WTF_REQUIRES_LOCK(lock());
    static Lock& lock() WTF_RETURNS_LOCK(s_lock) { return s_lock; }

private:
    void scheduleScratchBufferPurge()
    {
        ASSERT(lock().isHeld());
        const Seconds scratchBufferPurgeInterval { 2_s };
        m_purgeTimer.startOneShot(scratchBufferPurgeInterval);
    }

    void purgeTimerFired()
    {
        ASSERT(isMainThread());
        if (lock().tryLock()) {
            clearScratchBuffer();
            lock().unlock();
        }
    }

    void clearScratchBuffer() WTF_REQUIRES_LOCK(lock())
    {
        ASSERT(lock().isHeld());
        m_imageBuffer = nullptr;
        m_lastRadius = FloatSize();
        m_lastLayerSize = FloatSize();
    }

    static Lock s_lock;

    RefPtr<ImageBuffer> m_imageBuffer;
    RunLoop::Timer<ScratchBuffer> m_purgeTimer;

    FloatRect m_lastInsetBounds;
    FloatRect m_lastShadowRect;
    FloatRoundedRect::Radii m_lastRadii;
    Color m_lastColor;
    FloatSize m_lastRadius;
    bool m_lastWasInset { false };
    FloatSize m_lastLayerSize;
};

Lock ScratchBuffer::s_lock;

ScratchBuffer& ScratchBuffer::singleton()
{
    ASSERT(lock().isHeld());
    static NeverDestroyed<ScratchBuffer> scratchBuffer;
    return scratchBuffer;
}

static float radiusToLegacyRadius(float radius)
{
    return radius > 8 ? 8 + 4 * sqrt((radius - 8) / 2) : radius;
}
#endif

static const int templateSideLength = 1;

ShadowBlur::ShadowBlur() = default;

ShadowBlur::ShadowBlur(const FloatSize& radius, const FloatSize& offset, const Color& color, bool shadowsIgnoreTransforms)
    : m_color(color)
    , m_blurRadius(radius)
    , m_offset(offset)
    , m_shadowsIgnoreTransforms(shadowsIgnoreTransforms)
{
    updateShadowBlurValues();
}

ShadowBlur::ShadowBlur(const GraphicsContextState& state)
    : m_color(state.shadowColor)
    , m_blurRadius(state.shadowBlur, state.shadowBlur)
    , m_offset(state.shadowOffset)
    , m_shadowsIgnoreTransforms(state.shadowsIgnoreTransforms)
{
#if USE(CG)
    // Core Graphics incorrectly renders shadows with radius > 8px (<rdar://problem/8103442>),
    // but we need to preserve this buggy behavior for canvas and -webkit-box-shadow.
    if (state.shadowRadiusMode == ShadowRadiusMode::Legacy) {
        float shadowBlur = radiusToLegacyRadius(state.shadowBlur);
        m_blurRadius = FloatSize(shadowBlur, shadowBlur);
    }
#endif
    updateShadowBlurValues();
}

void ShadowBlur::setShadowValues(const FloatSize& radius, const FloatSize& offset, const Color& color, bool ignoreTransforms)
{
    m_blurRadius = radius;
    m_offset = offset;
    m_color = color;
    m_shadowsIgnoreTransforms = ignoreTransforms;

    updateShadowBlurValues();
}

void ShadowBlur::updateShadowBlurValues()
{
    // Limit blur radius to 128 to avoid lots of very expensive blurring.
    m_blurRadius = m_blurRadius.shrunkTo(FloatSize(128, 128));

    // The type of shadow is decided by the blur radius, shadow offset, and shadow color.
    if (!m_color.isVisible()) {
        // Can't paint the shadow with invalid or invisible color.
        m_type = NoShadow;
    } else if (m_blurRadius.width() > 0 || m_blurRadius.height() > 0) {
        // Shadow is always blurred, even the offset is zero.
        m_type = BlurShadow;
    } else if (!m_offset.width() && !m_offset.height()) {
        // Without blur and zero offset means the shadow is fully hidden.
        m_type = NoShadow;
    } else
        m_type = SolidShadow;
}

// Instead of integer division, we use 17.15 for fixed-point division.
static const int blurSumShift = 15;

// Takes a two dimensional array with three rows and two columns for the lobes.
static void calculateLobes(int lobes[][2], float blurRadius, bool shadowsIgnoreTransforms)
{
    int diameter;
    if (shadowsIgnoreTransforms)
        diameter = std::max(2, static_cast<int>(floorf((2 / 3.f) * blurRadius))); // Canvas shadow. FIXME: we should adjust the blur radius higher up.
    else {
        // http://dev.w3.org/csswg/css3-background/#box-shadow
        // Approximate a Gaussian blur with a standard deviation equal to half the blur radius,
        // which http://www.w3.org/TR/SVG/filters.html#feGaussianBlurElement tell us how to do.
        // However, shadows rendered according to that spec will extend a little further than m_blurRadius,
        // so we apply a fudge factor to bring the radius down slightly.
        float stdDev = blurRadius / 2;
        const float gaussianKernelFactor = 3 / 4.f * sqrtf(2 * piFloat);
        const float fudgeFactor = 0.88f;
        diameter = std::max(2, static_cast<int>(floorf(stdDev * gaussianKernelFactor * fudgeFactor + 0.5f)));
    }

    if (diameter & 1) {
        // if d is odd, use three box-blurs of size 'd', centered on the output pixel.
        int lobeSize = (diameter - 1) / 2;
        lobes[0][LeftLobe] = lobeSize;
        lobes[0][RightLobe] = lobeSize;
        lobes[1][LeftLobe] = lobeSize;
        lobes[1][RightLobe] = lobeSize;
        lobes[2][LeftLobe] = lobeSize;
        lobes[2][RightLobe] = lobeSize;
    } else {
        // if d is even, two box-blurs of size 'd' (the first one centered on the pixel boundary
        // between the output pixel and the one to the left, the second one centered on the pixel
        // boundary between the output pixel and the one to the right) and one box blur of size 'd+1' centered on the output pixel
        int lobeSize = diameter / 2;
        lobes[0][LeftLobe] = lobeSize;
        lobes[0][RightLobe] = lobeSize - 1;
        lobes[1][LeftLobe] = lobeSize - 1;
        lobes[1][RightLobe] = lobeSize;
        lobes[2][LeftLobe] = lobeSize;
        lobes[2][RightLobe] = lobeSize;
    }
}

void ShadowBlur::clear()
{
    m_type = NoShadow;
    m_color = Color();
    m_blurRadius = FloatSize();
    m_offset = FloatSize();
}

void ShadowBlur::blurLayerImage(unsigned char* imageData, const IntSize& size, int rowStride)
{
    const int channels[4] = { 3, 0, 1, 3 };

    int lobes[3][2]; // indexed by pass, and left/right lobe
    calculateLobes(lobes, m_blurRadius.width(), m_shadowsIgnoreTransforms);

    // First pass is horizontal.
    int stride = 4;
    int delta = rowStride;
    int final = size.height();
    int dim = size.width();

    // Two stages: horizontal and vertical
    for (int pass = 0; pass < 2; ++pass) {
        unsigned char* pixels = imageData;

        if (!pass && !m_blurRadius.width())
            final = 0; // Do no work if horizonal blur is zero.

        for (int j = 0; j < final; ++j, pixels += delta) {
            // For each step, we blur the alpha in a channel and store the result
            // in another channel for the subsequent step.
            // We use sliding window algorithm to accumulate the alpha values.
            // This is much more efficient than computing the sum of each pixels
            // covered by the box kernel size for each x.
            for (int step = 0; step < 3; ++step) {
                int side1 = lobes[step][LeftLobe];
                int side2 = lobes[step][RightLobe];
                int pixelCount = side1 + 1 + side2;
                int invCount = ((1 << blurSumShift) + pixelCount - 1) / pixelCount;
                int ofs = 1 + side2;
                int alpha1 = pixels[channels[step]];
                int alpha2 = pixels[(dim - 1) * stride + channels[step]];

                unsigned char* ptr = pixels + channels[step + 1];
                unsigned char* prev = pixels + stride + channels[step];
                unsigned char* next = pixels + ofs * stride + channels[step];

                int i;
                int sum = side1 * alpha1 + alpha1;
                int limit = (dim < side2 + 1) ? dim : side2 + 1;

                for (i = 1; i < limit; ++i, prev += stride)
                    sum += *prev;

                if (limit <= side2)
                    sum += (side2 - limit + 1) * alpha2;

                limit = (side1 < dim) ? side1 : dim;
                for (i = 0; i < limit; ptr += stride, next += stride, ++i, ++ofs) {
                    *ptr = (sum * invCount) >> blurSumShift;
                    sum += ((ofs < dim) ? *next : alpha2) - alpha1;
                }

                prev = pixels + channels[step];
                for (; ofs < dim; ptr += stride, prev += stride, next += stride, ++i, ++ofs) {
                    *ptr = (sum * invCount) >> blurSumShift;
                    sum += (*next) - (*prev);
                }

                for (; i < dim; ptr += stride, prev += stride, ++i) {
                    *ptr = (sum * invCount) >> blurSumShift;
                    sum += alpha2 - (*prev);
                }
            }
        }

        // Last pass is vertical.
        stride = rowStride;
        delta = 4;
        final = size.width();
        dim = size.height();

        if (!m_blurRadius.height())
            break;

        if (m_blurRadius.width() != m_blurRadius.height())
            calculateLobes(lobes, m_blurRadius.height(), m_shadowsIgnoreTransforms);
    }
}

void ShadowBlur::adjustBlurRadius(const AffineTransform& transform)
{
    if (m_shadowsIgnoreTransforms)
        m_blurRadius.scale(1 / static_cast<float>(transform.xScale()), 1 / static_cast<float>(transform.yScale()));
}

IntSize ShadowBlur::blurredEdgeSize() const
{
    IntSize edgeSize = expandedIntSize(m_blurRadius);

    // To avoid slowing down blurLayerImage() for radius == 1, we give it two empty pixels on each side.
    if (edgeSize.width() == 1)
        edgeSize.setWidth(2);

    if (edgeSize.height() == 1)
        edgeSize.setHeight(2);

    return edgeSize;
}

std::optional<ShadowBlur::LayerImageProperties> ShadowBlur::calculateLayerBoundingRect(const AffineTransform& transform, const FloatRect& shadowedRect, const IntRect& clipRect)
{
    LayerImageProperties calculatedLayerImageProperties;

    IntSize edgeSize = blurredEdgeSize();

    // Calculate the destination of the blurred and/or transformed layer.
    FloatRect layerRect;
    IntSize inflation;

    if (m_shadowsIgnoreTransforms && !transform.isIdentity()) {
        FloatQuad transformedPolygon = transform.mapQuad(FloatQuad(shadowedRect));
        transformedPolygon.move(m_offset);
        layerRect = transform.inverse().value_or(AffineTransform()).mapQuad(transformedPolygon).boundingBox();
    } else {
        layerRect = shadowedRect;
        layerRect.move(m_offset);
    }

    // We expand the area by the blur radius to give extra space for the blur transition.
    if (m_type == BlurShadow) {
        layerRect.inflateX(edgeSize.width());
        layerRect.inflateY(edgeSize.height());
        inflation = edgeSize;
    }

    FloatRect unclippedLayerRect = layerRect;

    if (!clipRect.contains(enclosingIntRect(layerRect))) {
        // If we are totally outside the clip region, we aren't painting at all.
        if (intersection(layerRect, clipRect).isEmpty())
            return std::nullopt;

        IntRect inflatedClip = clipRect;
        // Pixels at the edges can be affected by pixels outside the buffer,
        // so intersect with the clip inflated by the blur.
        if (m_type == BlurShadow) {
            inflatedClip.inflateX(edgeSize.width());
            inflatedClip.inflateY(edgeSize.height());
        } else {
            // Enlarge the clipping area 1 pixel so that the fill does not
            // bleed (due to antialiasing) even if the unaligned clip rect occurred
            inflatedClip.inflateX(1);
            inflatedClip.inflateY(1);
        }

        layerRect.intersect(inflatedClip);
    }

    IntSize frameSize = inflation;
    frameSize.scale(2);
    calculatedLayerImageProperties.shadowedResultSize = FloatSize(shadowedRect.width() + frameSize.width(), shadowedRect.height() + frameSize.height());
    calculatedLayerImageProperties.layerOrigin = FloatPoint(layerRect.x(), layerRect.y());
    calculatedLayerImageProperties.layerSize = layerRect.size();

    const FloatPoint unclippedLayerOrigin = FloatPoint(unclippedLayerRect.x(), unclippedLayerRect.y());
    const FloatSize clippedOut = unclippedLayerOrigin - calculatedLayerImageProperties.layerOrigin;

    // Set the origin as the top left corner of the scratch image, or, in case there's a clipped
    // out region, set the origin accordingly to the full bounding rect's top-left corner.
    float translationX = -shadowedRect.x() + inflation.width() - fabsf(clippedOut.width());
    float translationY = -shadowedRect.y() + inflation.height() - fabsf(clippedOut.height());
    calculatedLayerImageProperties.layerContextTranslation = FloatSize(translationX, translationY);

    return calculatedLayerImageProperties;
}

void ShadowBlur::drawShadowBuffer(GraphicsContext& graphicsContext, ImageBuffer& layerImage, const FloatPoint& layerOrigin, const FloatSize& layerSize)
{
    GraphicsContextStateSaver stateSaver(graphicsContext);

    IntSize bufferSize = layerImage.backendSize();
    if (bufferSize != layerSize) {
        // The rect passed to clipToImageBuffer() has to be the size of the entire buffer,
        // but we may not have cleared it all, so clip to the filled part first.
        graphicsContext.clip(FloatRect(layerOrigin, layerSize));
    }
    graphicsContext.clipToImageBuffer(layerImage, FloatRect(layerOrigin, bufferSize));
    graphicsContext.setFillColor(m_color);

    graphicsContext.clearShadow();
    graphicsContext.fillRect(FloatRect(layerOrigin, layerSize));
}

static void computeSliceSizesFromRadii(const IntSize& twiceRadius, const FloatRoundedRect::Radii& radii, int& leftSlice, int& rightSlice, int& topSlice, int& bottomSlice)
{
    leftSlice = twiceRadius.width() + std::max(radii.topLeft().width(), radii.bottomLeft().width());
    rightSlice = twiceRadius.width() + std::max(radii.topRight().width(), radii.bottomRight().width());

    topSlice = twiceRadius.height() + std::max(radii.topLeft().height(), radii.topRight().height());
    bottomSlice = twiceRadius.height() + std::max(radii.bottomLeft().height(), radii.bottomRight().height());
}

IntSize ShadowBlur::templateSize(const IntSize& radiusPadding, const FloatRoundedRect::Radii& radii) const
{
    const int templateSideLength = 1;

    int leftSlice;
    int rightSlice;
    int topSlice;
    int bottomSlice;

    IntSize blurExpansion = radiusPadding;
    blurExpansion.scale(2);

    computeSliceSizesFromRadii(blurExpansion, radii, leftSlice, rightSlice, topSlice, bottomSlice);

    return IntSize(templateSideLength + leftSlice + rightSlice, templateSideLength + topSlice + bottomSlice);
}

void ShadowBlur::drawRectShadow(GraphicsContext& graphicsContext, const FloatRoundedRect& shadowedRect)
{
    drawRectShadow(graphicsContext.getCTM(), graphicsContext.clipBounds(), shadowedRect,
        [this, &graphicsContext](ImageBuffer& layerImage, const FloatPoint& layerOrigin, const FloatSize& layerSize) {
            drawShadowBuffer(graphicsContext, layerImage, layerOrigin, layerSize);
        },
        [&graphicsContext](ImageBuffer& image, const FloatRect& destRect, const FloatRect& srcRect) {
            GraphicsContextStateSaver stateSaver(graphicsContext);
            graphicsContext.clearShadow();
            graphicsContext.drawImageBuffer(image, destRect, srcRect);
        },
        [&graphicsContext](const FloatRect& rect, const Color& color) {
            GraphicsContextStateSaver stateSaver(graphicsContext);
            graphicsContext.setFillColor(color);
            graphicsContext.clearShadow();
            graphicsContext.fillRect(rect);
        });
}

void ShadowBlur::drawInsetShadow(GraphicsContext& graphicsContext, const FloatRect& fullRect, const FloatRoundedRect& holeRect)
{
    drawInsetShadow(graphicsContext.getCTM(), graphicsContext.clipBounds(), fullRect, holeRect,
        [this, &graphicsContext](ImageBuffer& layerImage, const FloatPoint& layerOrigin, const FloatSize& layerSize) {
            drawShadowBuffer(graphicsContext, layerImage, layerOrigin, layerSize);
        },
        [&graphicsContext](ImageBuffer& image, const FloatRect& destRect, const FloatRect& srcRect) {
            // Note that drawing the ImageBuffer is faster than creating a Image and drawing that,
            // because ImageBuffer::draw() knows that it doesn't have to copy the image bits.
            GraphicsContextStateSaver stateSaver(graphicsContext);
            graphicsContext.clearShadow();
            graphicsContext.drawImageBuffer(image, destRect, srcRect);
        },
        [&graphicsContext](const FloatRect& rect, const FloatRect& holeRect, const Color& color) {
            Path exteriorPath;
            exteriorPath.addRect(rect);
            exteriorPath.addRect(holeRect);

            GraphicsContextStateSaver fillStateSaver(graphicsContext);
            graphicsContext.setFillRule(WindRule::EvenOdd);
            graphicsContext.setFillColor(color);
            graphicsContext.clearShadow();
            graphicsContext.fillPath(exteriorPath);
        });
}

void ShadowBlur::drawRectShadow(const AffineTransform& transform, const IntRect& clipBounds, const FloatRoundedRect& shadowedRect, const DrawBufferCallback& drawBuffer, const DrawImageCallback& drawImage, const FillRectCallback& fillRect)
{
    auto layerImageProperties = calculateLayerBoundingRect(transform, shadowedRect.rect(), clipBounds);
    if (!layerImageProperties)
        return;

    adjustBlurRadius(transform);

    bool canUseTilingTechnique = true;

    // drawRectShadowWithTiling does not work with rotations.
    // https://bugs.webkit.org/show_bug.cgi?id=45042
    if (!transform.preservesAxisAlignment() || m_type != BlurShadow)
        canUseTilingTechnique = false;

    IntSize edgeSize = blurredEdgeSize();
    IntSize templateSize = this->templateSize(edgeSize, shadowedRect.radii());
    const FloatRect& rect = shadowedRect.rect();

    if (templateSize.width() > rect.width() || templateSize.height() > rect.height()
        || (templateSize.unclampedArea() > layerImageProperties->shadowedResultSize.area()))
        canUseTilingTechnique = false;

    if (canUseTilingTechnique)
        drawRectShadowWithTiling(transform, shadowedRect, templateSize, edgeSize, drawImage, fillRect, *layerImageProperties);
    else
        drawRectShadowWithoutTiling(transform, shadowedRect, *layerImageProperties, drawBuffer);
}

void ShadowBlur::drawInsetShadow(const AffineTransform& transform, const IntRect& clipBounds, const FloatRect& fullRect, const FloatRoundedRect& holeRect, const DrawBufferCallback& drawBuffer, const DrawImageCallback& drawImage, const FillRectWithHoleCallback& fillRectWithHole)
{
    auto layerImageProperties = calculateLayerBoundingRect(transform, fullRect, clipBounds);
    if (!layerImageProperties)
        return;

    adjustBlurRadius(transform);

    bool canUseTilingTechnique = true;

    // drawRectShadowWithTiling does not work with rotations.
    // https://bugs.webkit.org/show_bug.cgi?id=45042
    if (!transform.preservesAxisAlignment() || m_type != BlurShadow)
        canUseTilingTechnique = false;

    IntSize edgeSize = blurredEdgeSize();
    IntSize templateSize = this->templateSize(edgeSize, holeRect.radii());
    const FloatRect& hRect = holeRect.rect();

    if (templateSize.width() > hRect.width() || templateSize.height() > hRect.height()
        || (templateSize.width() * templateSize.height() > hRect.width() * hRect.height()))
        canUseTilingTechnique = false;

    if (canUseTilingTechnique)
        drawInsetShadowWithTiling(transform, fullRect, holeRect, templateSize, edgeSize, drawImage, fillRectWithHole);
    else
        drawInsetShadowWithoutTiling(transform, fullRect, holeRect, *layerImageProperties, drawBuffer);
}

void ShadowBlur::drawRectShadowWithoutTiling(const AffineTransform&, const FloatRoundedRect& shadowedRect, const LayerImageProperties& layerImageProperties, const DrawBufferCallback& drawBuffer)
{
    auto layerImage = ImageBuffer::create(expandedIntSize(layerImageProperties.layerSize), RenderingMode::Unaccelerated, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
    if (!layerImage)
        return;

    GraphicsContext& shadowContext = layerImage->context();
    GraphicsContextStateSaver stateSaver(shadowContext);
    shadowContext.setFillColor(Color::black);

    {
        GraphicsContext& shadowContext = layerImage->context();
        GraphicsContextStateSaver stateSaver(shadowContext);
        shadowContext.translate(layerImageProperties.layerContextTranslation);
        shadowContext.setFillColor(Color::black);
        if (shadowedRect.radii().isZero())
            shadowContext.fillRect(shadowedRect.rect());
        else {
            Path path;
            path.addRoundedRect(shadowedRect);
            shadowContext.fillPath(path);
        }

        blurShadowBuffer(*layerImage, expandedIntSize(layerImageProperties.layerSize));
    }
    drawBuffer(*layerImage, layerImageProperties.layerOrigin, layerImageProperties.layerSize);
}

void ShadowBlur::drawInsetShadowWithoutTiling(const AffineTransform&, const FloatRect& fullRect, const FloatRoundedRect& holeRect, const LayerImageProperties& layerImageProperties, const DrawBufferCallback& drawBuffer)
{
    auto layerImage = ImageBuffer::create(expandedIntSize(layerImageProperties.layerSize), RenderingMode::Unaccelerated, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
    if (!layerImage)
        return;

    {
        GraphicsContext& shadowContext = layerImage->context();
        GraphicsContextStateSaver stateSaver(shadowContext);
        shadowContext.translate(layerImageProperties.layerContextTranslation);

        Path path;
        path.addRect(fullRect);
        if (holeRect.radii().isZero())
            path.addRect(holeRect.rect());
        else
            path.addRoundedRect(holeRect);

        shadowContext.setFillRule(WindRule::EvenOdd);
        shadowContext.setFillColor(Color::black);
        shadowContext.fillPath(path);

        blurShadowBuffer(*layerImage, expandedIntSize(layerImageProperties.layerSize));
    }

    drawBuffer(*layerImage, layerImageProperties.layerOrigin, layerImageProperties.layerSize);
}

/*
  These functions use tiling to improve the performance of the shadow
  drawing of rounded rectangles. The code basically does the following
  steps:

     1. Calculate the size of the shadow template, a rectangle that
     contains all the necessary tiles to draw the complete shadow.

     2. If that size is smaller than the real rectangle render the new
     template rectangle and its shadow in a new surface, in other case
     render the shadow of the real rectangle in the destination
     surface.

     3. Calculate the sizes and positions of the tiles and their
     destinations and use drawPattern to render the final shadow. The
     code divides the rendering in 8 tiles:

        1 | 2 | 3
       -----------
        4 |   | 5
       -----------
        6 | 7 | 8

     The corners are directly copied from the template rectangle to the
     real one and the side tiles are 1 pixel width, we use them as
     tiles to cover the destination side. The corner tiles are bigger
     than just the side of the rounded corner, we need to increase it
     because the modifications caused by the corner over the blur
     effect. We fill the central or outer part with solid color to complete
     the shadow.
 */

void ShadowBlur::drawRectShadowWithTiling(const AffineTransform& transform, const FloatRoundedRect& shadowedRect, const IntSize& templateSize, const IntSize& edgeSize, const DrawImageCallback& drawImage, const FillRectCallback& fillRect, const LayerImageProperties& layerImageProperties)
{
    FloatRect templateShadow = FloatRect(edgeSize.width(), edgeSize.height(), templateSize.width() - 2 * edgeSize.width(), templateSize.height() - 2 * edgeSize.height());

#if USE(CG)
    if (ScratchBuffer::lock().tryLock()) {
        Locker locker { AdoptLock, ScratchBuffer::lock() };
        auto layerImageBuffer = ScratchBuffer::singleton().getScratchBuffer(templateSize);
        if (!layerImageBuffer)
            return;
        bool redrawNeeded = ScratchBuffer::singleton().setCachedShadowValues(m_blurRadius, m_color, templateShadow, shadowedRect.radii(), layerImageProperties.layerSize);
        drawRectShadowWithTilingWithLayerImageBuffer(*layerImageBuffer, transform, shadowedRect, templateSize, edgeSize, drawImage, fillRect, templateShadow, redrawNeeded);
        return;
    }
#else
    UNUSED_PARAM(layerImageProperties);
#endif

    if (auto layerImageBuffer = ImageBuffer::create(templateSize, RenderingMode::Unaccelerated, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8))
        drawRectShadowWithTilingWithLayerImageBuffer(*layerImageBuffer, transform, shadowedRect, templateSize, edgeSize, drawImage, fillRect, templateShadow);
}

void ShadowBlur::drawRectShadowWithTilingWithLayerImageBuffer(ImageBuffer& layerImage, const AffineTransform& transform, const FloatRoundedRect& shadowedRect, const IntSize& templateSize, const IntSize& edgeSize, const DrawImageCallback& drawImage, const FillRectCallback& fillRect, const FloatRect& templateShadow, bool redrawNeeded)
{
    if (redrawNeeded) {
        // Draw shadow into the ImageBuffer.
        GraphicsContext& shadowContext = layerImage.context();
        GraphicsContextStateSaver shadowStateSaver(shadowContext);

        shadowContext.clearRect(FloatRect(0, 0, templateSize.width(), templateSize.height()));
        shadowContext.setFillColor(Color::black);

        if (shadowedRect.radii().isZero())
            shadowContext.fillRect(templateShadow);
        else {
            Path path;
            path.addRoundedRect(FloatRoundedRect(templateShadow, shadowedRect.radii()));
            shadowContext.fillPath(path);
        }
        blurAndColorShadowBuffer(layerImage, templateSize);
    }

    FloatSize offset = m_offset;
    if (shadowsIgnoreTransforms())
        offset.scale(1 / transform.xScale(), 1 / transform.yScale());

    FloatRect shadowBounds = shadowedRect.rect();
    shadowBounds.move(offset);
    shadowBounds.inflateX(edgeSize.width());
    shadowBounds.inflateY(edgeSize.height());

    drawLayerPiecesAndFillCenter(layerImage, shadowBounds, shadowedRect.radii(), edgeSize, templateSize, drawImage, fillRect);
}

void ShadowBlur::drawInsetShadowWithTiling(const AffineTransform& transform, const FloatRect& fullRect, const FloatRoundedRect& holeRect, const IntSize& templateSize, const IntSize& edgeSize, const DrawImageCallback& drawImage, const FillRectWithHoleCallback& fillRectWithHole)
{
    // Draw the rectangle with hole.
    FloatRect templateBounds(0, 0, templateSize.width(), templateSize.height());
    FloatRect templateHole = FloatRect(edgeSize.width(), edgeSize.height(), templateSize.width() - 2 * edgeSize.width(), templateSize.height() - 2 * edgeSize.height());

#if USE(CG)
    if (ScratchBuffer::lock().tryLock()) {
        Locker locker { AdoptLock, ScratchBuffer::lock() };
        auto layerImageBuffer = ScratchBuffer::singleton().getScratchBuffer(templateSize);
        if (!layerImageBuffer)
            return;
        bool redrawNeeded = ScratchBuffer::singleton().setCachedInsetShadowValues(m_blurRadius, m_color, templateBounds, templateHole, holeRect.radii());
        drawInsetShadowWithTilingWithLayerImageBuffer(*layerImageBuffer, transform, fullRect, holeRect, templateSize, edgeSize, drawImage, fillRectWithHole, templateBounds, templateHole, redrawNeeded);
        return;
    }
#endif

    if (auto layerImageBuffer = ImageBuffer::create(templateSize, RenderingMode::Unaccelerated, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8))
        drawInsetShadowWithTilingWithLayerImageBuffer(*layerImageBuffer, transform, fullRect, holeRect, templateSize, edgeSize, drawImage, fillRectWithHole, templateBounds, templateHole);
}

void ShadowBlur::drawInsetShadowWithTilingWithLayerImageBuffer(ImageBuffer& layerImage, const AffineTransform& transform, const FloatRect& fullRect, const FloatRoundedRect& holeRect, const IntSize& templateSize, const IntSize& edgeSize, const DrawImageCallback& drawImage, const FillRectWithHoleCallback& fillRectWithHole, const FloatRect& templateBounds, const FloatRect& templateHole, bool redrawNeeded)
{
    if (redrawNeeded) {
        // Draw shadow into a new ImageBuffer.
        GraphicsContext& shadowContext = layerImage.context();
        GraphicsContextStateSaver shadowStateSaver(shadowContext);
        shadowContext.clearRect(templateBounds);
        shadowContext.setFillRule(WindRule::EvenOdd);
        shadowContext.setFillColor(Color::black);

        Path path;
        path.addRect(templateBounds);
        if (holeRect.radii().isZero())
            path.addRect(templateHole);
        else
            path.addRoundedRect(FloatRoundedRect(templateHole, holeRect.radii()));

        shadowContext.fillPath(path);

        blurAndColorShadowBuffer(layerImage, templateSize);
    }
    FloatSize offset = m_offset;
    if (shadowsIgnoreTransforms())
        offset.scale(1 / transform.xScale(), 1 / transform.yScale());

    FloatRect boundingRect = fullRect;
    boundingRect.move(offset);

    FloatRect destHoleRect = holeRect.rect();
    destHoleRect.move(offset);
    FloatRect destHoleBounds = destHoleRect;
    destHoleBounds.inflateX(edgeSize.width());
    destHoleBounds.inflateY(edgeSize.height());

    // Fill the external part of the shadow (which may be visible because of offset).
    fillRectWithHole(boundingRect, destHoleBounds, m_color);

    drawLayerPieces(layerImage, destHoleBounds, holeRect.radii(), edgeSize, templateSize, drawImage);
}

void ShadowBlur::drawLayerPieces(ImageBuffer& layerImage, const FloatRect& shadowBounds, const FloatRoundedRect::Radii& radii, const IntSize& bufferPadding, const IntSize& templateSize, const DrawImageCallback& drawImage)
{
    const IntSize twiceRadius = IntSize(bufferPadding.width() * 2, bufferPadding.height() * 2);

    int leftSlice;
    int rightSlice;
    int topSlice;
    int bottomSlice;
    computeSliceSizesFromRadii(twiceRadius, radii, leftSlice, rightSlice, topSlice, bottomSlice);

    int centerWidth = shadowBounds.width() - leftSlice - rightSlice;
    int centerHeight = shadowBounds.height() - topSlice - bottomSlice;
    FloatRect centerRect(shadowBounds.x() + leftSlice, shadowBounds.y() + topSlice, centerWidth, centerHeight);

    // Top side.
    FloatRect tileRect = FloatRect(leftSlice, 0, templateSideLength, topSlice);
    FloatRect destRect = FloatRect(centerRect.x(), centerRect.y() - topSlice, centerRect.width(), topSlice);
    drawImage(layerImage, destRect, tileRect);

    // Draw the bottom side.
    tileRect.setY(templateSize.height() - bottomSlice);
    tileRect.setHeight(bottomSlice);
    destRect.setY(centerRect.maxY());
    destRect.setHeight(bottomSlice);
    drawImage(layerImage, destRect, tileRect);

    // Left side.
    tileRect = FloatRect(0, topSlice, leftSlice, templateSideLength);
    destRect = FloatRect(centerRect.x() - leftSlice, centerRect.y(), leftSlice, centerRect.height());
    drawImage(layerImage, destRect, tileRect);

    // Right side.
    tileRect.setX(templateSize.width() - rightSlice);
    tileRect.setWidth(rightSlice);
    destRect.setX(centerRect.maxX());
    destRect.setWidth(rightSlice);
    drawImage(layerImage, destRect, tileRect);

    // Top left corner.
    tileRect = FloatRect(0, 0, leftSlice, topSlice);
    destRect = FloatRect(centerRect.x() - leftSlice, centerRect.y() - topSlice, leftSlice, topSlice);
    drawImage(layerImage, destRect, tileRect);

    // Top right corner.
    tileRect = FloatRect(templateSize.width() - rightSlice, 0, rightSlice, topSlice);
    destRect = FloatRect(centerRect.maxX(), centerRect.y() - topSlice, rightSlice, topSlice);
    drawImage(layerImage, destRect, tileRect);

    // Bottom right corner.
    tileRect = FloatRect(templateSize.width() - rightSlice, templateSize.height() - bottomSlice, rightSlice, bottomSlice);
    destRect = FloatRect(centerRect.maxX(), centerRect.maxY(), rightSlice, bottomSlice);
    drawImage(layerImage, destRect, tileRect);

    // Bottom left corner.
    tileRect = FloatRect(0, templateSize.height() - bottomSlice, leftSlice, bottomSlice);
    destRect = FloatRect(centerRect.x() - leftSlice, centerRect.maxY(), leftSlice, bottomSlice);
    drawImage(layerImage, destRect, tileRect);
}

void ShadowBlur::drawLayerPiecesAndFillCenter(ImageBuffer& layerImage, const FloatRect& shadowBounds, const FloatRoundedRect::Radii& radii, const IntSize& bufferPadding, const IntSize& templateSize, const DrawImageCallback& drawImage, const FillRectCallback& fillRect)
{
    const IntSize twiceRadius = IntSize(bufferPadding.width() * 2, bufferPadding.height() * 2);

    int leftSlice;
    int rightSlice;
    int topSlice;
    int bottomSlice;
    computeSliceSizesFromRadii(twiceRadius, radii, leftSlice, rightSlice, topSlice, bottomSlice);

    int centerWidth = shadowBounds.width() - leftSlice - rightSlice;
    int centerHeight = shadowBounds.height() - topSlice - bottomSlice;
    FloatRect centerRect(shadowBounds.x() + leftSlice, shadowBounds.y() + topSlice, centerWidth, centerHeight);

    // Fill center
    if (!centerRect.isEmpty())
        fillRect(centerRect, m_color);

    drawLayerPieces(layerImage, shadowBounds, radii, bufferPadding, templateSize, drawImage);
}

void ShadowBlur::blurShadowBuffer(ImageBuffer& layerImage, const IntSize& templateSize)
{
    if (m_type != BlurShadow)
        return;

    PixelBufferFormat format { AlphaPremultiplication::Unpremultiplied, PixelFormat::RGBA8, DestinationColorSpace::SRGB() };
    IntRect blurRect(IntPoint(), templateSize);
    auto layerData = layerImage.getPixelBuffer(format, blurRect);
    if (!layerData)
        return;

    blurLayerImage(layerData->data().data(), blurRect.size(), blurRect.width() * 4);
    layerImage.putPixelBuffer(*layerData, blurRect);
}

void ShadowBlur::blurAndColorShadowBuffer(ImageBuffer& layerImage, const IntSize& templateSize)
{
    blurShadowBuffer(layerImage, templateSize);

    // Mask the image with the shadow color.
    GraphicsContext& shadowContext = layerImage.context();
    GraphicsContextStateSaver stateSaver(shadowContext);
    shadowContext.setCompositeOperation(CompositeOperator::SourceIn);
    shadowContext.setFillColor(m_color);
    shadowContext.fillRect(FloatRect(0, 0, templateSize.width(), templateSize.height()));
}

void ShadowBlur::drawShadowLayer(const AffineTransform& transform, const IntRect& clipBounds, const FloatRect& layerArea, const DrawShadowCallback& drawShadow, const DrawBufferCallback& drawBuffer)
{
    auto layerImageProperties = calculateLayerBoundingRect(transform, layerArea, clipBounds);
    if (!layerImageProperties)
        return;

    adjustBlurRadius(transform);

    auto layerImage = ImageBuffer::create(expandedIntSize(layerImageProperties->layerSize), RenderingMode::Unaccelerated, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
    if (!layerImage)
        return;

    {
        GraphicsContext& shadowContext = layerImage->context();
        GraphicsContextStateSaver stateSaver(shadowContext);
        shadowContext.translate(layerImageProperties->layerContextTranslation);
        drawShadow(shadowContext);
    }

    blurAndColorShadowBuffer(*layerImage, expandedIntSize(layerImageProperties->layerSize));
    drawBuffer(*layerImage, layerImageProperties->layerOrigin, layerImageProperties->layerSize);
}

} // namespace WebCore
