/*
 * Copyright (C) 2010 Sencha, Inc.
 * Copyright (C) 2010 Igalia S.L.
 *
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
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
#include "ContextShadow.h"

#include <wtf/MathExtras.h>
#include <wtf/Noncopyable.h>

using WTF::min;
using WTF::max;

namespace WebCore {

ContextShadow::ContextShadow()
    : m_type(NoShadow)
    , m_blurRadius(0)
{
}

ContextShadow::ContextShadow(const Color& color, float radius, const FloatSize& offset)
    : m_color(color)
    , m_blurRadius(round(radius))
    , m_offset(offset)
{
    // See comments in http://webkit.org/b/40793, it seems sensible
    // to follow Skia's limit of 128 pixels of blur radius
    m_blurRadius = min(m_blurRadius, 128);

    // The type of shadow is decided by the blur radius, shadow offset, and shadow color.
    if (!m_color.isValid() || !color.alpha()) {
        // Can't paint the shadow with invalid or invisible color.
        m_type = NoShadow;
    } else if (radius > 0) {
        // Shadow is always blurred, even the offset is zero.
        m_type = BlurShadow;
    } else if (!m_offset.width() && !m_offset.height()) {
        // Without blur and zero offset means the shadow is fully hidden.
        m_type = NoShadow;
    } else {
        m_type = SolidShadow;
    }
}

void ContextShadow::clear()
{
    m_type = NoShadow;
    m_color = Color();
    m_blurRadius = 0;
    m_offset = FloatSize();
}

// Instead of integer division, we use 17.15 for fixed-point division.
static const int BlurSumShift = 15;

// Check http://www.w3.org/TR/SVG/filters.html#feGaussianBlur.
// As noted in the SVG filter specification, running box blur 3x
// approximates a real gaussian blur nicely.

void ContextShadow::blurLayerImage(unsigned char* imageData, const IntSize& size, int rowStride)
{
    int channels[4] = { 3, 0, 1, 3 };
    int dmax = m_blurRadius >> 1;
    int dmin = dmax - 1 + (m_blurRadius & 1);
    if (dmin < 0)
        dmin = 0;

    // Two stages: horizontal and vertical
    for (int k = 0; k < 2; ++k) {

        unsigned char* pixels = imageData;
        int stride = (!k) ? 4 : rowStride;
        int delta = (!k) ? rowStride : 4;
        int jfinal = (!k) ? size.height() : size.width();
        int dim = (!k) ? size.width() : size.height();

        for (int j = 0; j < jfinal; ++j, pixels += delta) {

            // For each step, we blur the alpha in a channel and store the result
            // in another channel for the subsequent step.
            // We use sliding window algorithm to accumulate the alpha values.
            // This is much more efficient than computing the sum of each pixels
            // covered by the box kernel size for each x.

            for (int step = 0; step < 3; ++step) {
                int side1 = (!step) ? dmin : dmax;
                int side2 = (step == 1) ? dmin : dmax;
                int pixelCount = side1 + 1 + side2;
                int invCount = ((1 << BlurSumShift) + pixelCount - 1) / pixelCount;
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
                    *ptr = (sum * invCount) >> BlurSumShift;
                    sum += ((ofs < dim) ? *next : alpha2) - alpha1;
                }
                prev = pixels + channels[step];
                for (; ofs < dim; ptr += stride, prev += stride, next += stride, ++i, ++ofs) {
                    *ptr = (sum * invCount) >> BlurSumShift;
                    sum += (*next) - (*prev);
                }
                for (; i < dim; ptr += stride, prev += stride, ++i) {
                    *ptr = (sum * invCount) >> BlurSumShift;
                    sum += alpha2 - (*prev);
                }
            }
        }
    }
}

void ContextShadow::calculateLayerBoundingRect(const FloatRect& layerArea, const IntRect& clipRect)
{
    // Calculate the destination of the blurred layer.
    FloatRect destinationRect(layerArea);
    destinationRect.move(m_offset);
    m_layerRect = enclosingIntRect(destinationRect);

    // We expand the area by the blur radius * 2 to give extra space for the blur transition.
    m_layerRect.inflate((m_type == BlurShadow) ? ceil(m_blurRadius * 2) : 0);

    if (!clipRect.contains(m_layerRect)) {
        // No need to have the buffer larger than the clip.
        m_layerRect.intersect(clipRect);

        // If we are totally outside the clip region, we aren't painting at all.
        if (m_layerRect.isEmpty())
            return;

        // We adjust again because the pixels at the borders are still
        // potentially affected by the pixels outside the buffer.
        if (m_type == BlurShadow)
            m_layerRect.inflate((m_type == BlurShadow) ? ceil(m_blurRadius * 2) : 0);
    }
}

} // namespace WebCore
