/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Gradient.h"

#include "CSSParser.h"
#include "GraphicsContext.h"

#include "SkGradientShader.h"
#include "SkiaUtils.h"

namespace WebCore {

void Gradient::platformDestroy()
{
    if (m_gradient)
        SkSafeUnref(m_gradient);
    m_gradient = 0;
}

static inline U8CPU F2B(float x)
{
    return static_cast<int>(x * 255);
}

static SkColor makeSkColor(float a, float r, float g, float b)
{
    return SkColorSetARGB(F2B(a), F2B(r), F2B(g), F2B(b));
}

// Determine the total number of stops needed, including pseudo-stops at the
// ends as necessary.
static size_t totalStopsNeeded(const Gradient::ColorStop* stopData, size_t count)
{
    // N.B.:  The tests in this function should kept in sync with the ones in
    // fillStops(), or badness happens.
    const Gradient::ColorStop* stop = stopData;
    size_t countUsed = count;
    if (count < 1 || stop->stop > 0.0)
        countUsed++;
    stop += count - 1;
    if (count < 1 || stop->stop < 1.0)
        countUsed++;
    return countUsed;
}

// Collect sorted stop position and color information into the pos and colors 
// buffers, ensuring stops at both 0.0 and 1.0.  The buffers must be large
// enough to hold information for all stops, including the new endpoints if
// stops at 0.0 and 1.0 aren't already included.
static void fillStops(const Gradient::ColorStop* stopData,
                       size_t count, SkScalar* pos, SkColor* colors)
{ 
    const Gradient::ColorStop* stop = stopData;
    size_t start = 0;
    if (count < 1) {
        // A gradient with no stops must be transparent black.
        pos[0] = WebCoreFloatToSkScalar(0.0);
        colors[0] = makeSkColor(0.0, 0.0, 0.0, 0.0);
        start = 1;
    } else if (stop->stop > 0.0) {
        // Copy the first stop to 0.0. The first stop position may have a slight
        // rounding error, but we don't care in this float comparison, since
        // 0.0 comes through cleanly and people aren't likely to want a gradient
        // with a stop at (0 + epsilon).
        pos[0] = WebCoreFloatToSkScalar(0.0);
        colors[0] = makeSkColor(stop->alpha, stop->red, stop->green, stop->blue);
        start = 1;
    }

    for (size_t i = start; i < start + count; i++) {
        pos[i] = WebCoreFloatToSkScalar(stop->stop);
        colors[i] = makeSkColor(stop->alpha, stop->red, stop->green, stop->blue);
        ++stop;
    }

    // Copy the last stop to 1.0 if needed.  See comment above about this float
    // comparison.
    if (count < 1 || (--stop)->stop < 1.0) {
        pos[start + count] = WebCoreFloatToSkScalar(1.0);
        colors[start + count] = colors[start + count - 1];
    }
}

SkShader* Gradient::platformGradient()
{
    if (m_gradient)
        return m_gradient;

    sortStopsIfNecessary();
    ASSERT(m_stopsSorted);

    size_t countUsed = totalStopsNeeded(m_stops.data(), m_stops.size());
    ASSERT(countUsed >= 2);
    ASSERT(countUsed >= m_stops.size());

    // FIXME: Why is all this manual pointer math needed?!
    SkAutoMalloc storage(countUsed * (sizeof(SkColor) + sizeof(SkScalar)));
    SkColor* colors = (SkColor*)storage.get();
    SkScalar* pos = (SkScalar*)(colors + countUsed);

    fillStops(m_stops.data(), m_stops.size(), pos, colors);

    SkShader::TileMode tile = SkShader::kClamp_TileMode;
    switch (m_spreadMethod) {
    case SpreadMethodReflect:
        tile = SkShader::kMirror_TileMode;
        break;
    case SpreadMethodRepeat:
        tile = SkShader::kRepeat_TileMode;
        break;
    case SpreadMethodPad:
        tile = SkShader::kClamp_TileMode;
        break;
    }

    if (m_radial) {
        // Since the two-point radial gradient is slower than the plain radial,
        // only use it if we have to.
        if (m_p0 == m_p1 && m_r0 <= 0.0f) {
            // The radius we give to Skia must be positive (and non-zero).  If
            // we're given a zero radius, just ask for a very small radius so
            // Skia will still return an object.
            SkScalar radius = m_r1 > 0 ? WebCoreFloatToSkScalar(m_r1) : SK_ScalarMin;
            m_gradient = SkGradientShader::CreateRadial(m_p1, radius, colors, pos, static_cast<int>(countUsed), tile);
        } else {
            // The radii we give to Skia must be positive.  If we're given a 
            // negative radius, ask for zero instead.
            SkScalar radius0 = m_r0 >= 0.0f ? WebCoreFloatToSkScalar(m_r0) : 0;
            SkScalar radius1 = m_r1 >= 0.0f ? WebCoreFloatToSkScalar(m_r1) : 0;
            m_gradient = SkGradientShader::CreateTwoPointRadial(m_p0, radius0, m_p1, radius1, colors, pos, static_cast<int>(countUsed), tile);
        }

        if (aspectRatio() != 1) {
            // CSS3 elliptical gradients: apply the elliptical scaling at the
            // gradient center point.
            m_gradientSpaceTransformation.translate(m_p0.x(), m_p0.y());
            m_gradientSpaceTransformation.scale(1, 1 / aspectRatio());
            m_gradientSpaceTransformation.translate(-m_p0.x(), -m_p0.y());
            ASSERT(m_p0 == m_p1);
        }
    } else {
        SkPoint pts[2] = { m_p0, m_p1 };
        m_gradient = SkGradientShader::CreateLinear(pts, colors, pos, static_cast<int>(countUsed), tile);
    }

    ASSERT(m_gradient);
    SkMatrix matrix = m_gradientSpaceTransformation;
    m_gradient->setLocalMatrix(matrix);
    return m_gradient;
}

void Gradient::fill(GraphicsContext* context, const FloatRect& rect)
{
    context->setFillGradient(this);
    context->fillRect(rect);
}

void Gradient::setPlatformGradientSpaceTransform(const AffineTransform& matrix)
{
    if (m_gradient)
        m_gradient->setLocalMatrix(m_gradientSpaceTransformation);
}

} // namespace WebCore
