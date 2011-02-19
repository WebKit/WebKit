/*
 * Copyright (C) 2011 Apple Inc.
 * Copyright (C) 2010 Sencha, Inc.
 * Copyright (C) 2010 Igalia S.L.
 * All rights reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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
#include "Timer.h"
#include <wtf/MathExtras.h>
#include <wtf/Noncopyable.h>
#include <wtf/UnusedParam.h>

using namespace std;

namespace WebCore {

static inline int roundUpToMultipleOf32(int d)
{
    return (1 + (d >> 5)) << 5;
}

// ShadowBlur needs a scratch image as the buffer for the blur filter.
// Instead of creating and destroying the buffer for every operation,
// we create a buffer which will be automatically purged via a timer.
class ScratchBuffer {
public:
    ScratchBuffer()
        : m_purgeTimer(this, &ScratchBuffer::timerFired)
#if !ASSERT_DISABLED
        , m_bufferInUse(false)
#endif
    {
    }
    
    ImageBuffer* getScratchBuffer(const IntSize& size)
    {
        ASSERT(!m_bufferInUse);
#if !ASSERT_DISABLED
        m_bufferInUse = true;
#endif
        // We do not need to recreate the buffer if the current buffer is large enough.
        if (m_imageBuffer && m_imageBuffer->width() >= size.width() && m_imageBuffer->height() >= size.height())
            return m_imageBuffer.get();

        // Round to the nearest 32 pixels so we do not grow the buffer for similar sized requests.
        IntSize roundedSize(roundUpToMultipleOf32(size.width()), roundUpToMultipleOf32(size.height()));

        m_imageBuffer = ImageBuffer::create(roundedSize);
        return m_imageBuffer.get();
    }

    void scheduleScratchBufferPurge()
    {
#if !ASSERT_DISABLED
        m_bufferInUse = false;
#endif
        if (m_purgeTimer.isActive())
            m_purgeTimer.stop();

        const double scratchBufferPurgeInterval = 2;
        m_purgeTimer.startOneShot(scratchBufferPurgeInterval);
    }
    
    static ScratchBuffer& shared();

private:
    void timerFired(Timer<ScratchBuffer>*)
    {
        clearScratchBuffer();
    }
    
    void clearScratchBuffer()
    {
        m_imageBuffer = 0;
    }

    OwnPtr<ImageBuffer> m_imageBuffer;
    Timer<ScratchBuffer> m_purgeTimer;
#if !ASSERT_DISABLED
    bool m_bufferInUse;
#endif
};

ScratchBuffer& ScratchBuffer::shared()
{
    DEFINE_STATIC_LOCAL(ScratchBuffer, scratchBuffer, ());
    return scratchBuffer;
}

static const int templateSideLength = 1;

ShadowBlur::ShadowBlur(float radius, const FloatSize& offset, const Color& color, ColorSpace colorSpace)
    : m_color(color)
    , m_colorSpace(colorSpace)
    , m_blurRadius(radius)
    , m_offset(offset)
    , m_layerImage(0)
    , m_shadowsIgnoreTransforms(false)
{
    // Limit blur radius to 128 to avoid lots of very expensive blurring.
    m_blurRadius = min<float>(m_blurRadius, 128);

    // The type of shadow is decided by the blur radius, shadow offset, and shadow color.
    if (!m_color.isValid() || !color.alpha()) {
        // Can't paint the shadow with invalid or invisible color.
        m_type = NoShadow;
    } else if (m_blurRadius > 0) {
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

void ShadowBlur::blurLayerImage(unsigned char* imageData, const IntSize& size, int rowStride)
{
    const int channels[4] =
#if CPU(BIG_ENDIAN)
        { 0, 3, 2, 0 };
#elif CPU(MIDDLE_ENDIAN)
        { 1, 2, 3, 1 };
#else
        { 3, 0, 1, 3 };
#endif

    int diameter;
    if (m_shadowsIgnoreTransforms)
        diameter = max(2, static_cast<int>(floorf((2 / 3.f) * m_blurRadius))); // Canvas shadow. FIXME: we should adjust the blur radius higher up.
    else {
        // http://dev.w3.org/csswg/css3-background/#box-shadow
        // Approximate a Gaussian blur with a standard deviation equal to half the blur radius,
        // which http://www.w3.org/TR/SVG/filters.html#feGaussianBlurElement tell us how to do.
        // However, shadows rendered according to that spec will extend a little further than m_blurRadius,
        // so we apply a fudge factor to bring the radius down slightly.
        float stdDev = m_blurRadius / 2;
        const float gaussianKernelFactor = 3 / 4.f * sqrtf(2 * piFloat);
        const float fudgeFactor = 0.88f;
        diameter = max(2, static_cast<int>(floorf(stdDev * gaussianKernelFactor * fudgeFactor + 0.5f)));
    }

    enum {
        leftLobe = 0,
        rightLobe = 1
    };

    int lobes[3][2]; // indexed by pass, and left/right lobe
    
    if (diameter & 1) {
        // if d is odd, use three box-blurs of size 'd', centered on the output pixel.
        int lobeSize = (diameter - 1) / 2;
        lobes[0][leftLobe] = lobeSize;
        lobes[0][rightLobe] = lobeSize;
        lobes[1][leftLobe] = lobeSize;
        lobes[1][rightLobe] = lobeSize;
        lobes[2][leftLobe] = lobeSize;
        lobes[2][rightLobe] = lobeSize;
    } else {
        // if d is even, two box-blurs of size 'd' (the first one centered on the pixel boundary
        // between the output pixel and the one to the left, the second one centered on the pixel
        // boundary between the output pixel and the one to the right) and one box blur of size 'd+1' centered on the output pixel
        int lobeSize = diameter / 2;
        lobes[0][leftLobe] = lobeSize;
        lobes[0][rightLobe] = lobeSize - 1;
        lobes[1][leftLobe] = lobeSize - 1;
        lobes[1][rightLobe] = lobeSize;
        lobes[2][leftLobe] = lobeSize;
        lobes[2][rightLobe] = lobeSize;
    }

    // First pass is horizontal.
    int stride = 4;
    int delta = rowStride;
    int final = size.height();
    int dim = size.width();

    // Two stages: horizontal and vertical
    for (int pass = 0; pass < 2; ++pass) {
        unsigned char* pixels = imageData;

        for (int j = 0; j < final; ++j, pixels += delta) {
            // For each step, we blur the alpha in a channel and store the result
            // in another channel for the subsequent step.
            // We use sliding window algorithm to accumulate the alpha values.
            // This is much more efficient than computing the sum of each pixels
            // covered by the box kernel size for each x.
            for (int step = 0; step < 3; ++step) {
                int side1 = lobes[step][leftLobe];
                int side2 = lobes[step][rightLobe];
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
    }
}

void ShadowBlur::adjustBlurRadius(GraphicsContext* context)
{
    if (!m_shadowsIgnoreTransforms)
        return;

    const AffineTransform transform = context->getCTM();

    // Adjust blur if we're scaling, since the radius must not be affected by transformations.
    // FIXME: use AffineTransform::isIdentityOrTranslationOrFlipped()?
    if (transform.isIdentity())
        return;

    // Calculate transformed unit vectors.
    const FloatQuad unitQuad(FloatPoint(0, 0), FloatPoint(1, 0),
                             FloatPoint(0, 1), FloatPoint(1, 1));
    const FloatQuad transformedUnitQuad = transform.mapQuad(unitQuad);

    // Calculate X axis scale factor.
    const FloatSize xUnitChange = transformedUnitQuad.p2() - transformedUnitQuad.p1();
    const float xAxisScale = sqrtf(xUnitChange.width() * xUnitChange.width()
                                   + xUnitChange.height() * xUnitChange.height());

    // Calculate Y axis scale factor.
    const FloatSize yUnitChange = transformedUnitQuad.p3() - transformedUnitQuad.p1();
    const float yAxisScale = sqrtf(yUnitChange.width() * yUnitChange.width()
                                   + yUnitChange.height() * yUnitChange.height());

    // blurLayerImage() does not support per-axis blurring, so calculate a balanced scaling.
    // FIXME: does AffineTransform.xScale()/yScale() help?
    const float scale = sqrtf(xAxisScale * yAxisScale);
    m_blurRadius = roundf(m_blurRadius / scale);
}

IntRect ShadowBlur::calculateLayerBoundingRect(GraphicsContext* context, const FloatRect& shadowedRect, const IntRect& clipRect)
{
    const float roundedRadius = ceilf(m_blurRadius);

    // Calculate the destination of the blurred and/or transformed layer.
    FloatRect layerRect;
    float inflation = 0;

    const AffineTransform transform = context->getCTM();
    if (m_shadowsIgnoreTransforms && !transform.isIdentity()) {
        FloatQuad transformedPolygon = transform.mapQuad(FloatQuad(shadowedRect));
        transformedPolygon.move(m_offset);
        layerRect = transform.inverse().mapQuad(transformedPolygon).boundingBox();
    } else {
        layerRect = shadowedRect;
        layerRect.move(m_offset);
    }

    // We expand the area by the blur radius to give extra space for the blur transition.
    if (m_type == BlurShadow) {
        layerRect.inflate(roundedRadius);
        inflation = roundedRadius;
    }

    FloatRect unclippedLayerRect = layerRect;

    if (!clipRect.contains(enclosingIntRect(layerRect))) {
        // If we are totally outside the clip region, we aren't painting at all.
        if (intersection(layerRect, clipRect).isEmpty())
            return IntRect();

        IntRect inflatedClip = clipRect;
        // Pixels at the edges can be affected by pixels outside the buffer,
        // so intersect with the clip inflated by the blur.
        if (m_type == BlurShadow)
            inflatedClip.inflate(roundedRadius);
        
        layerRect.intersect(inflatedClip);
    }

    const float frameSize = inflation * 2;
    m_sourceRect = FloatRect(0, 0, shadowedRect.width() + frameSize, shadowedRect.height() + frameSize);
    m_layerOrigin = FloatPoint(layerRect.x(), layerRect.y());
    m_layerSize = layerRect.size();

    const FloatPoint unclippedLayerOrigin = FloatPoint(unclippedLayerRect.x(), unclippedLayerRect.y());
    const FloatSize clippedOut = unclippedLayerOrigin - m_layerOrigin;

    // Set the origin as the top left corner of the scratch image, or, in case there's a clipped
    // out region, set the origin accordingly to the full bounding rect's top-left corner.
    float translationX = -shadowedRect.x() + inflation - fabsf(clippedOut.width());
    float translationY = -shadowedRect.y() + inflation - fabsf(clippedOut.height());
    m_layerContextTranslation = FloatSize(translationX, translationY);

    return enclosingIntRect(layerRect);
}

GraphicsContext* ShadowBlur::beginShadowLayer(GraphicsContext* graphicsContext, const IntRect& layerRect)
{
    adjustBlurRadius(graphicsContext);

    // Don't paint if we are totally outside the clip region.
    if (layerRect.isEmpty())
        return 0;

    m_layerImage = ScratchBuffer::shared().getScratchBuffer(layerRect.size());
    GraphicsContext* layerContext = m_layerImage->context();

    layerContext->save(); // Balanced by restore() in endShadowLayer().

    // Always clear the surface first. FIXME: we could avoid the clear on first allocation.
    // Add a pixel to avoid later edge aliasing when rotated.
    layerContext->clearRect(FloatRect(0, 0, m_layerSize.width() + 1, m_layerSize.height() + 1));
    layerContext->translate(m_layerContextTranslation);

    return layerContext;
}

void ShadowBlur::endShadowLayer(GraphicsContext* graphicsContext)
{
    if (!m_layerImage)
        return;
        
    m_layerImage->context()->restore();

    if (m_type == BlurShadow) {
        IntRect blurRect = enclosingIntRect(FloatRect(FloatPoint(), m_layerSize));
        RefPtr<ByteArray> layerData = m_layerImage->getUnmultipliedImageData(blurRect);
        blurLayerImage(layerData->data(), blurRect.size(), blurRect.width() * 4);
        m_layerImage->putUnmultipliedImageData(layerData.get(), blurRect.size(), blurRect, IntPoint());
    }

    graphicsContext->save();

    IntSize bufferSize = m_layerImage->size();
    if (bufferSize != m_layerSize) {
        // The rect passed to clipToImageBuffer() has to be the size of the entire buffer,
        // but we may not have cleared it all, so clip to the filled part first.
        graphicsContext->clip(FloatRect(m_layerOrigin, m_layerSize));
    }
    graphicsContext->clipToImageBuffer(m_layerImage, FloatRect(m_layerOrigin, bufferSize));
    graphicsContext->setFillColor(m_color, m_colorSpace);

    graphicsContext->clearShadow();
    graphicsContext->fillRect(FloatRect(m_layerOrigin, m_sourceRect.size()));
    
    graphicsContext->restore();

    m_layerImage = 0;

    // Schedule a purge of the scratch buffer. We do not need to destroy the surface.
    ScratchBuffer::shared().scheduleScratchBufferPurge();
}

static void computeSliceSizesFromRadii(int twiceRadius, const RoundedIntRect::Radii& radii, int& leftSlice, int& rightSlice, int& topSlice, int& bottomSlice)
{
    leftSlice = twiceRadius + max(radii.topLeft().width(), radii.bottomLeft().width()); 
    rightSlice = twiceRadius + max(radii.topRight().width(), radii.bottomRight().width()); 

    topSlice = twiceRadius + max(radii.topLeft().height(), radii.topRight().height());
    bottomSlice = twiceRadius + max(radii.bottomLeft().height(), radii.bottomRight().height());
}

IntSize ShadowBlur::templateSize(const RoundedIntRect::Radii& radii) const
{
    const int templateSideLength = 1;

    int leftSlice;
    int rightSlice;
    int topSlice;
    int bottomSlice;
    computeSliceSizesFromRadii(2 * ceilf(m_blurRadius), radii, leftSlice, rightSlice, topSlice, bottomSlice);
    
    return IntSize(templateSideLength + leftSlice + rightSlice,
                   templateSideLength + topSlice + bottomSlice);
}

void ShadowBlur::drawRectShadow(GraphicsContext* graphicsContext, const FloatRect& shadowedRect, const RoundedIntRect::Radii& radii)
{
    IntRect layerRect = calculateLayerBoundingRect(graphicsContext, shadowedRect, graphicsContext->clipBounds());
    if (layerRect.isEmpty())
        return;

    // drawRectShadowWithTiling does not work with rotations.
    // https://bugs.webkit.org/show_bug.cgi?id=45042
    if (!graphicsContext->getCTM().isIdentityOrTranslationOrFlipped() || m_type != BlurShadow) {
        drawRectShadowWithoutTiling(graphicsContext, shadowedRect, radii, layerRect);
        return;
    }

    IntSize templateSize = this->templateSize(radii);

    if (templateSize.width() > shadowedRect.width() || templateSize.height() > shadowedRect.height()
        || (templateSize.width() * templateSize.height() > m_sourceRect.width() * m_sourceRect.height())) {
        drawRectShadowWithoutTiling(graphicsContext, shadowedRect, radii, layerRect);
        return;
    }

    drawRectShadowWithTiling(graphicsContext, shadowedRect, radii, templateSize);
}

void ShadowBlur::drawInsetShadow(GraphicsContext* graphicsContext, const FloatRect& rect, const FloatRect& holeRect, const RoundedIntRect::Radii& holeRadii)
{
    IntRect layerRect = calculateLayerBoundingRect(graphicsContext, rect, graphicsContext->clipBounds());
    if (layerRect.isEmpty())
        return;

    // drawInsetShadowWithTiling does not work with rotations.
    // https://bugs.webkit.org/show_bug.cgi?id=45042
    if (!graphicsContext->getCTM().isIdentityOrTranslationOrFlipped() || m_type != BlurShadow) {
        drawInsetShadowWithoutTiling(graphicsContext, rect, holeRect, holeRadii, layerRect);
        return;
    }

    IntSize templateSize = this->templateSize(holeRadii);

    if (templateSize.width() > holeRect.width() || templateSize.height() > holeRect.height()
        || (templateSize.width() * templateSize.height() > holeRect.width() * holeRect.height())) {
        drawInsetShadowWithoutTiling(graphicsContext, rect, holeRect, holeRadii, layerRect);
        return;
    }

    drawInsetShadowWithTiling(graphicsContext, rect, holeRect, holeRadii, templateSize);
}

void ShadowBlur::drawRectShadowWithoutTiling(GraphicsContext* graphicsContext, const FloatRect& shadowedRect, const RoundedIntRect::Radii& radii, const IntRect& layerRect)
{
    GraphicsContext* shadowContext = beginShadowLayer(graphicsContext, layerRect);
    if (!shadowContext)
        return;

    Path path;
    path.addRoundedRect(shadowedRect, radii.topLeft(), radii.topRight(), radii.bottomLeft(), radii.bottomRight());

    shadowContext->setFillColor(Color::black, ColorSpaceDeviceRGB);
    shadowContext->fillPath(path);
    
    endShadowLayer(graphicsContext);
}

void ShadowBlur::drawInsetShadowWithoutTiling(GraphicsContext* graphicsContext, const FloatRect& rect, const FloatRect& holeRect, const RoundedIntRect::Radii& holeRadii, const IntRect& layerRect)
{
    GraphicsContext* shadowContext = beginShadowLayer(graphicsContext, layerRect);
    if (!shadowContext)
        return;

    Path path;
    path.addRect(rect);
    path.addRoundedRect(holeRect, holeRadii.topLeft(), holeRadii.topRight(), holeRadii.bottomLeft(), holeRadii.bottomRight());

    shadowContext->setFillRule(RULE_EVENODD);
    shadowContext->setFillColor(Color::black, ColorSpaceDeviceRGB);
    shadowContext->fillPath(path);

    endShadowLayer(graphicsContext);
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

void ShadowBlur::drawInsetShadowWithTiling(GraphicsContext* graphicsContext, const FloatRect& rect, const FloatRect& holeRect, const RoundedIntRect::Radii& radii, const IntSize& templateSize)
{
    graphicsContext->save();
    graphicsContext->clearShadow();

    const float roundedRadius = ceilf(m_blurRadius);
    const float twiceRadius = roundedRadius * 2;

    m_layerImage = ScratchBuffer::shared().getScratchBuffer(templateSize);

    // Draw the rectangle with hole.
    FloatRect templateBounds(0, 0, templateSize.width(), templateSize.height());
    FloatRect templateHole = FloatRect(roundedRadius, roundedRadius, templateSize.width() - twiceRadius, templateSize.height() - twiceRadius);
    Path path;
    path.addRect(templateBounds);
    path.addRoundedRect(templateHole, radii.topLeft(), radii.topRight(), radii.bottomLeft(), radii.bottomRight());

    // Draw shadow into a new ImageBuffer.
    GraphicsContext* shadowContext = m_layerImage->context();
    shadowContext->save();
    shadowContext->clearRect(templateBounds);
    shadowContext->setFillRule(RULE_EVENODD);
    shadowContext->setFillColor(Color::black, ColorSpaceDeviceRGB);
    shadowContext->fillPath(path);
    blurAndColorShadowBuffer(templateSize);
    shadowContext->restore();

    FloatRect boundingRect = rect;
    boundingRect.move(m_offset);

    FloatRect destHoleRect = holeRect;
    destHoleRect.move(m_offset);
    FloatRect destHoleBounds = destHoleRect;
    destHoleBounds.inflate(roundedRadius);

    // Fill the external part of the shadow (which may be visible because of offset).
    Path exteriorPath;
    exteriorPath.addRect(boundingRect);
    exteriorPath.addRect(destHoleBounds);

    graphicsContext->save();
    graphicsContext->setFillRule(RULE_EVENODD);
    graphicsContext->setFillColor(m_color, m_colorSpace);
    graphicsContext->fillPath(exteriorPath);
    graphicsContext->restore();
    
    drawLayerPieces(graphicsContext, destHoleBounds, radii, roundedRadius, templateSize, InnerShadow);

    graphicsContext->restore();

    m_layerImage = 0;
    // Schedule a purge of the scratch buffer.
    ScratchBuffer::shared().scheduleScratchBufferPurge();
}

void ShadowBlur::drawRectShadowWithTiling(GraphicsContext* graphicsContext, const FloatRect& shadowedRect, const RoundedIntRect::Radii& radii, const IntSize& templateSize)
{
    graphicsContext->save();
    graphicsContext->clearShadow();

    const float roundedRadius = ceilf(m_blurRadius);
    const float twiceRadius = roundedRadius * 2;

    m_layerImage = ScratchBuffer::shared().getScratchBuffer(templateSize);

    // Draw the rectangle.
    FloatRect templateShadow = FloatRect(roundedRadius, roundedRadius, templateSize.width() - twiceRadius, templateSize.height() - twiceRadius);
    Path path;
    path.addRoundedRect(templateShadow, radii.topLeft(), radii.topRight(), radii.bottomLeft(), radii.bottomRight());

    // Draw shadow into the ImageBuffer.
    GraphicsContext* shadowContext = m_layerImage->context();
    shadowContext->save();
    shadowContext->clearRect(FloatRect(0, 0, templateSize.width(), templateSize.height()));
    shadowContext->setFillColor(Color::black, ColorSpaceDeviceRGB);
    shadowContext->fillPath(path);
    blurAndColorShadowBuffer(templateSize);
    shadowContext->restore();

    FloatRect shadowBounds = shadowedRect;
    shadowBounds.move(m_offset.width(), m_offset.height());
    shadowBounds.inflate(roundedRadius);

    drawLayerPieces(graphicsContext, shadowBounds, radii, roundedRadius, templateSize, OuterShadow);

    graphicsContext->restore();

    m_layerImage = 0;
    // Schedule a purge of the scratch buffer.
    ScratchBuffer::shared().scheduleScratchBufferPurge();
}

void ShadowBlur::drawLayerPieces(GraphicsContext* graphicsContext, const FloatRect& shadowBounds, const RoundedIntRect::Radii& radii, float roundedRadius, const IntSize& templateSize, ShadowDirection direction)
{
    const float twiceRadius = roundedRadius * 2;

    int leftSlice;
    int rightSlice;
    int topSlice;
    int bottomSlice;
    computeSliceSizesFromRadii(twiceRadius, radii, leftSlice, rightSlice, topSlice, bottomSlice);

    int centerWidth = shadowBounds.width() - leftSlice - rightSlice;
    int centerHeight = shadowBounds.height() - topSlice - bottomSlice;

    if (direction == OuterShadow) {
        FloatRect shadowInterior(shadowBounds.x() + leftSlice, shadowBounds.y() + topSlice, centerWidth, centerHeight);
        if (!shadowInterior.isEmpty()) {
            graphicsContext->save();
            
            graphicsContext->setFillColor(m_color, m_colorSpace);
            graphicsContext->fillRect(shadowInterior);
            
            graphicsContext->restore();
        }
    }

    // Note that drawing the ImageBuffer is faster than creating a Image and drawing that,
    // because ImageBuffer::draw() knows that it doesn't have to copy the image bits.
    
    // Top side.
    FloatRect tileRect = FloatRect(leftSlice, 0, templateSideLength, topSlice);
    FloatRect destRect = FloatRect(shadowBounds.x() + leftSlice, shadowBounds.y(), centerWidth, topSlice);
    graphicsContext->drawImageBuffer(m_layerImage, ColorSpaceDeviceRGB, destRect, tileRect);

    // Draw the bottom side.
    tileRect.setY(templateSize.height() - bottomSlice);
    tileRect.setHeight(bottomSlice);
    destRect.setY(shadowBounds.maxY() - bottomSlice);
    destRect.setHeight(bottomSlice);
    graphicsContext->drawImageBuffer(m_layerImage, ColorSpaceDeviceRGB, destRect, tileRect);

    // Left side.
    tileRect = FloatRect(0, topSlice, leftSlice, templateSideLength);
    destRect = FloatRect(shadowBounds.x(), shadowBounds.y() + topSlice, leftSlice, centerHeight);
    graphicsContext->drawImageBuffer(m_layerImage, ColorSpaceDeviceRGB, destRect, tileRect);

    // Right side.
    tileRect.setX(templateSize.width() - rightSlice);
    tileRect.setWidth(rightSlice);
    destRect.setX(shadowBounds.maxX() - rightSlice);
    destRect.setWidth(rightSlice);
    graphicsContext->drawImageBuffer(m_layerImage, ColorSpaceDeviceRGB, destRect, tileRect);

    // Top left corner.
    tileRect = FloatRect(0, 0, leftSlice, topSlice);
    destRect = FloatRect(shadowBounds.x(), shadowBounds.y(), leftSlice, topSlice);
    graphicsContext->drawImageBuffer(m_layerImage, ColorSpaceDeviceRGB, destRect, tileRect);

    // Top right corner.
    tileRect = FloatRect(templateSize.width() - rightSlice, 0, rightSlice, topSlice);
    destRect = FloatRect(shadowBounds.maxX() - rightSlice, shadowBounds.y(), rightSlice, topSlice);
    graphicsContext->drawImageBuffer(m_layerImage, ColorSpaceDeviceRGB, destRect, tileRect);

    // Bottom right corner.
    tileRect = FloatRect(templateSize.width() - rightSlice, templateSize.height() - bottomSlice, rightSlice, bottomSlice);
    destRect = FloatRect(shadowBounds.maxX() - rightSlice, shadowBounds.maxY() - bottomSlice, rightSlice, bottomSlice);
    graphicsContext->drawImageBuffer(m_layerImage, ColorSpaceDeviceRGB, destRect, tileRect);

    // Bottom left corner.
    tileRect = FloatRect(0, templateSize.height() - bottomSlice, leftSlice, bottomSlice);
    destRect = FloatRect(shadowBounds.x(), shadowBounds.maxY() - bottomSlice, leftSlice, bottomSlice);
    graphicsContext->drawImageBuffer(m_layerImage, ColorSpaceDeviceRGB, destRect, tileRect);
}


void ShadowBlur::blurAndColorShadowBuffer(const IntSize& templateSize)
{
    {
        IntRect blurRect(IntPoint(), templateSize);
        RefPtr<ByteArray> layerData = m_layerImage->getUnmultipliedImageData(blurRect);
        blurLayerImage(layerData->data(), blurRect.size(), blurRect.width() * 4);
        m_layerImage->putUnmultipliedImageData(layerData.get(), blurRect.size(), blurRect, IntPoint());
    }

    // Mask the image with the shadow color.
    GraphicsContext* shadowContext = m_layerImage->context();
    shadowContext->setCompositeOperation(CompositeSourceIn);
    shadowContext->setFillColor(m_color, m_colorSpace);
    shadowContext->fillRect(FloatRect(0, 0, templateSize.width(), templateSize.height()));
}

} // namespace WebCore
