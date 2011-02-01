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
    {
    }
    
    ImageBuffer* getScratchBuffer(const IntSize& size)
    {
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
};

ScratchBuffer& ScratchBuffer::shared()
{
    DEFINE_STATIC_LOCAL(ScratchBuffer, scratchBuffer, ());
    return scratchBuffer;
}

ShadowBlur::ShadowBlur(float radius, const FloatSize& offset, const Color& color, ColorSpace colorSpace)
    : m_color(color)
    , m_colorSpace(colorSpace)
    , m_blurRadius(radius)
    , m_offset(offset)
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
static const float gaussianKernelFactor = 3 / 4.f * sqrtf(2 * piFloat);

// Check http://www.w3.org/TR/SVG/filters.html#feGaussianBlur.
// As noted in the SVG filter specification, running box blur 3x
// approximates a real gaussian blur nicely.

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
    else
        diameter = max(2, static_cast<int>(floorf(m_blurRadius / 2 * gaussianKernelFactor + 0.5f))); // CSS

    int dMax = diameter >> 1;
    int dMin = dMax - 1 + (diameter & 1);
    if (dMin < 0)
        dMin = 0;

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
                int side1 = (!step) ? dMin : dMax;
                int side2 = (step == 1) ? dMin : dMax;
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

    graphicsContext->clipToImageBuffer(m_layerImage, FloatRect(m_layerOrigin, m_layerImage->size()));
    graphicsContext->setFillColor(m_color, m_colorSpace);

    graphicsContext->clearShadow();
    graphicsContext->fillRect(FloatRect(m_layerOrigin, m_sourceRect.size()));
    
    graphicsContext->restore();

    m_layerImage = 0;

    // Schedule a purge of the scratch buffer. We do not need to destroy the surface.
    ScratchBuffer::shared().scheduleScratchBufferPurge();
}

void ShadowBlur::drawRectShadow(GraphicsContext* graphicsContext, const FloatRect& shadowedRect, const RoundedIntRect::Radii& radii)
{
    IntRect layerRect = calculateLayerBoundingRect(graphicsContext, shadowedRect, graphicsContext->clipBounds());

    // drawShadowedRect does not work with rotations.
    // https://bugs.webkit.org/show_bug.cgi?id=45042
    if (!graphicsContext->getCTM().isIdentityOrTranslationOrFlipped() || m_type != BlurShadow) {
        drawRectShadowWithoutTiling(graphicsContext, shadowedRect, radii, layerRect);
        return;
    }

    const float roundedRadius = ceilf(m_blurRadius);
    const float twiceRadius = roundedRadius * 2;
    
    const int templateSideLength = 1;
    
    // Find the extra space needed from the curve of the corners.
    int extraWidthFromCornerRadii = twiceRadius + max(radii.topLeft().width(), radii.bottomLeft().width()) + twiceRadius + max(radii.topRight().width(), radii.bottomRight().width());
    int extraHeightFromCornerRadii = twiceRadius + max(radii.topLeft().height(), radii.topRight().height()) + twiceRadius + max(radii.bottomLeft().height(), radii.bottomRight().height());

    // The length of a side of the buffer is the enough space for four blur radii,
    // the radii of the corners, and then 1 pixel to draw the side tiles.
    IntSize shadowTemplateSize = IntSize(templateSideLength + extraWidthFromCornerRadii, templateSideLength + extraHeightFromCornerRadii);

    if (shadowTemplateSize.width() > shadowedRect.width() || shadowTemplateSize.height() > shadowedRect.height()
        || (shadowTemplateSize.width() * shadowTemplateSize.height() > m_sourceRect.width() * m_sourceRect.height())) {
        drawRectShadowWithoutTiling(graphicsContext, shadowedRect, radii, layerRect);
        return;
    }

    drawRectShadowWithTiling(graphicsContext, shadowedRect, radii, shadowTemplateSize);
}

void ShadowBlur::drawInsetShadow(GraphicsContext* graphicsContext, const FloatRect& rect, const FloatRect& holeRect, const RoundedIntRect::Radii& holeRadii)
{
    // FIXME: add a tiling code path here.
    IntRect layerRect = calculateLayerBoundingRect(graphicsContext, rect, graphicsContext->clipBounds());

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

/*
  This function uses tiling to improve the performance of the shadow
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
     effect. We fill the central part with solid color to complete the
     shadow.
 */

void ShadowBlur::drawRectShadowWithTiling(GraphicsContext* graphicsContext, const FloatRect& shadowedRect, const RoundedIntRect::Radii& radii, const IntSize& shadowTemplateSize)
{
    const float roundedRadius = ceilf(m_blurRadius);
    const float twiceRadius = roundedRadius * 2;

    // Size of the tiling side.
    const int templateSideLength = 1;

    m_layerImage = ScratchBuffer::shared().getScratchBuffer(shadowTemplateSize);

    // Draw shadow into a new ImageBuffer.
    GraphicsContext* shadowContext = m_layerImage->context();
    shadowContext->save();
    
    shadowContext->clearRect(FloatRect(0, 0, shadowTemplateSize.width(), shadowTemplateSize.height()));

    // Draw the rectangle.
    FloatRect templateRect = FloatRect(roundedRadius, roundedRadius, shadowTemplateSize.width() - twiceRadius, shadowTemplateSize.height() - twiceRadius);
    Path path;
    path.addRoundedRect(templateRect, radii.topLeft(), radii.topRight(), radii.bottomLeft(), radii.bottomRight());

    shadowContext->setFillColor(Color(.0f, .0f, .0f, 1.f), ColorSpaceDeviceRGB);
    shadowContext->fillPath(path);

    // Blur the image.
    {
        IntRect blurRect(IntPoint(), shadowTemplateSize);
        RefPtr<ByteArray> layerData = m_layerImage->getUnmultipliedImageData(blurRect);
        blurLayerImage(layerData->data(), blurRect.size(), blurRect.width() * 4);
        m_layerImage->putUnmultipliedImageData(layerData.get(), blurRect.size(), blurRect, IntPoint());
    }

    // Mask the image with the shadow color.
    shadowContext->setCompositeOperation(CompositeSourceIn);
    shadowContext->setFillColor(m_color, m_colorSpace);
    shadowContext->fillRect(FloatRect(0, 0, shadowTemplateSize.width(), shadowTemplateSize.height()));
    
    shadowContext->restore();

    FloatRect shadowRect = shadowedRect;
    shadowRect.inflate(roundedRadius); // FIXME: duplicating code with calculateLayerBoundingRect.
    shadowRect.move(m_offset.width(), m_offset.height());

    // Fill the internal part of the shadow.
    shadowRect.inflate(-twiceRadius);
    if (!shadowRect.isEmpty()) {
        graphicsContext->save();
        
        path.clear();
        path.addRoundedRect(shadowRect, radii.topLeft(), radii.topRight(), radii.bottomLeft(), radii.bottomRight());

        graphicsContext->setFillColor(m_color, m_colorSpace);
        graphicsContext->fillPath(path);
        
        graphicsContext->restore();
    }
    shadowRect.inflate(twiceRadius);

    // Note that drawing the ImageBuffer is faster than creating a Image and drawing that,
    // because ImageBuffer::draw() knows that it doesn't have to copy the image bits.
        
    // Draw top side.
    FloatRect tileRect = FloatRect(twiceRadius + radii.topLeft().width(), 0, templateSideLength, twiceRadius);
    FloatRect destRect = tileRect;
    destRect.move(shadowRect.x(), shadowRect.y());
    destRect.setWidth(shadowRect.width() - radii.topLeft().width() - radii.topRight().width() - roundedRadius * 4);
    graphicsContext->drawImageBuffer(m_layerImage, ColorSpaceDeviceRGB, destRect, tileRect);

    // Draw the bottom side.
    tileRect = FloatRect(twiceRadius + radii.bottomLeft().width(), shadowTemplateSize.height() - twiceRadius, templateSideLength, twiceRadius);
    destRect = tileRect;
    destRect.move(shadowRect.x(), shadowRect.y() + twiceRadius + shadowedRect.height() - shadowTemplateSize.height());
    destRect.setWidth(shadowRect.width() - radii.bottomLeft().width() - radii.bottomRight().width() - roundedRadius * 4);
    graphicsContext->drawImageBuffer(m_layerImage, ColorSpaceDeviceRGB, destRect, tileRect);

    // Draw the right side.
    tileRect = FloatRect(shadowTemplateSize.width() - twiceRadius, twiceRadius + radii.topRight().height(), twiceRadius, templateSideLength);
    destRect = tileRect;
    destRect.move(shadowRect.x() + twiceRadius + shadowedRect.width() - shadowTemplateSize.width(), shadowRect.y());
    destRect.setHeight(shadowRect.height() - radii.topRight().height() - radii.bottomRight().height() - roundedRadius * 4);
    graphicsContext->drawImageBuffer(m_layerImage, ColorSpaceDeviceRGB, destRect, tileRect);

    // Draw the left side.
    tileRect = FloatRect(0, twiceRadius + radii.topLeft().height(), twiceRadius, templateSideLength);
    destRect = tileRect;
    destRect.move(shadowRect.x(), shadowRect.y());
    destRect.setHeight(shadowRect.height() - radii.topLeft().height() - radii.bottomLeft().height() - roundedRadius * 4);
    graphicsContext->drawImageBuffer(m_layerImage, ColorSpaceDeviceRGB, destRect, tileRect);

    // Draw the top left corner.
    tileRect = FloatRect(0, 0, twiceRadius + radii.topLeft().width(), twiceRadius + radii.topLeft().height());
    destRect = tileRect;
    destRect.move(shadowRect.x(), shadowRect.y());
    graphicsContext->drawImageBuffer(m_layerImage, ColorSpaceDeviceRGB, destRect, tileRect);

    // Draw the top right corner.
    tileRect = FloatRect(shadowTemplateSize.width() - twiceRadius - radii.topRight().width(), 0, twiceRadius + radii.topRight().width(),
                         twiceRadius + radii.topRight().height());
    destRect = tileRect;
    destRect.move(shadowRect.x() + shadowedRect.width() - shadowTemplateSize.width() + twiceRadius, shadowRect.y());
    graphicsContext->drawImageBuffer(m_layerImage, ColorSpaceDeviceRGB, destRect, tileRect);

    // Draw the bottom right corner.
    tileRect = FloatRect(shadowTemplateSize.width() - twiceRadius - radii.bottomRight().width(),
                         shadowTemplateSize.height() - twiceRadius - radii.bottomRight().height(),
                         twiceRadius + radii.bottomRight().width(), twiceRadius + radii.bottomRight().height());
    destRect = tileRect;
    destRect.move(shadowRect.x() + shadowedRect.width() - shadowTemplateSize.width() + twiceRadius,
                  shadowRect.y() + shadowedRect.height() - shadowTemplateSize.height() + twiceRadius);
    graphicsContext->drawImageBuffer(m_layerImage, ColorSpaceDeviceRGB, destRect, tileRect);

    // Draw the bottom left corner.
    tileRect = FloatRect(0, shadowTemplateSize.height() - twiceRadius - radii.bottomLeft().height(),
                         twiceRadius + radii.bottomLeft().width(), twiceRadius + radii.bottomLeft().height());
    destRect = tileRect;
    destRect.move(shadowRect.x(), shadowRect.y() + shadowedRect.height() - shadowTemplateSize.height() + twiceRadius);
    graphicsContext->drawImageBuffer(m_layerImage, ColorSpaceDeviceRGB, destRect, tileRect);

    m_layerImage = 0;
    // Schedule a purge of the scratch buffer.
    ScratchBuffer::shared().scheduleScratchBufferPurge();
}

} // namespace WebCore
