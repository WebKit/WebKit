/*
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
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

#ifndef FloatRoundedRect_h
#define FloatRoundedRect_h

#include <WebCore/CornerRadii.h>
#include <WebCore/FloatRect.h>
#include <WebCore/FloatSize.h>
#include <WebCore/LayoutRoundedRect.h>
#include <WebCore/Region.h>
#include <wtf/TZoneMalloc.h>

#if USE(SKIA)
class SkRRect;
#endif

namespace WebCore {

class Path;

class FloatRoundedRect {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(FloatRoundedRect, WEBCORE_EXPORT);
public:
    WEBCORE_EXPORT explicit FloatRoundedRect(const FloatRect& = FloatRect(), const CornerRadii& = CornerRadii());
    WEBCORE_EXPORT FloatRoundedRect(const FloatRect&, const FloatSize& topLeft, const FloatSize& topRight, const FloatSize& bottomLeft, const FloatSize& bottomRight);
    explicit FloatRoundedRect(const LayoutRoundedRect&);
    FloatRoundedRect(float x, float y, float width, float height);

    const FloatRect& rect() const { return m_rect; }
    const CornerRadii& radii() const { return m_radii; }
    bool isRounded() const { return !m_radii.isZero(); }
    bool isEmpty() const { return m_rect.isEmpty(); }

    void setRect(const FloatRect& rect) { m_rect = rect; }
    void setLocation(FloatPoint location) { m_rect.setLocation(location); }
    void setRadii(const CornerRadii& radii) { m_radii = radii; }

    void move(const FloatSize& size) { m_rect.move(size); }
    void inflate(float size) { m_rect.inflate(size);  }
    void expandRadii(float size) { m_radii.expand(size); }
    void shrinkRadii(float size) { m_radii.shrink(size); }
    void inflateWithRadii(float size);
    void adjustRadii();

    FloatRect topLeftCorner() const
    {
        return FloatRect(m_rect.x(), m_rect.y(), m_radii.topLeft().width(), m_radii.topLeft().height());
    }
    FloatRect topRightCorner() const
    {
        return FloatRect(m_rect.maxX() - m_radii.topRight().width(), m_rect.y(), m_radii.topRight().width(), m_radii.topRight().height());
    }
    FloatRect bottomLeftCorner() const
    {
        return FloatRect(m_rect.x(), m_rect.maxY() - m_radii.bottomLeft().height(), m_radii.bottomLeft().width(), m_radii.bottomLeft().height());
    }
    FloatRect bottomRightCorner() const
    {
        return FloatRect(m_rect.maxX() - m_radii.bottomRight().width(), m_rect.maxY() - m_radii.bottomRight().height(), m_radii.bottomRight().width(), m_radii.bottomRight().height());
    }

    bool isRenderable() const;
    bool xInterceptsAtY(float y, float& minXIntercept, float& maxXIntercept) const;

    bool intersectionIsRectangular(const FloatRect&) const;

    Path path() const;

    friend bool operator==(const FloatRoundedRect&, const FloatRoundedRect&) = default;

#if USE(SKIA)
    FloatRoundedRect(const SkRRect&);
    operator SkRRect() const;
#endif

private:
    FloatRect m_rect;
    CornerRadii m_radii;
};

inline float calcBorderRadiiConstraintScaleFor(const FloatRect& rect, const CornerRadii& radii)
{
    // Constrain corner radii using CSS3 rules:
    // http://www.w3.org/TR/css3-background/#the-border-radius

    float factor = 1;
    float radiiSum;

    // top
    radiiSum = radii.topLeft().width() + radii.topRight().width(); // Casts to avoid integer overflow.
    if (radiiSum > rect.width())
        factor = std::min(rect.width() / radiiSum, factor);

    // bottom
    radiiSum = radii.bottomLeft().width() + radii.bottomRight().width();
    if (radiiSum > rect.width())
        factor = std::min(rect.width() / radiiSum, factor);

    // left
    radiiSum = radii.topLeft().height() + radii.bottomLeft().height();
    if (radiiSum > rect.height())
        factor = std::min(rect.height() / radiiSum, factor);

    // right
    radiiSum = radii.topRight().height() + radii.bottomRight().height();
    if (radiiSum > rect.height())
        factor = std::min(rect.height() / radiiSum, factor);

    ASSERT(factor <= 1);
    return factor;
}

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const FloatRoundedRect&);

// Snip away rectangles from corners, roughly one per step length of arc.
WEBCORE_EXPORT Region approximateAsRegion(const FloatRoundedRect&, unsigned stepLength = 20);

} // namespace WebCore

#endif // FloatRoundedRect_h
