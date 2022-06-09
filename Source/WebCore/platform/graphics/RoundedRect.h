/*
 * Copyright (C) 2003, 2006, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "FloatQuad.h"
#include "LayoutRect.h"
#include "LayoutSize.h"

namespace WebCore {

class FloatRoundedRect;
class LayoutUnit;
class Region;

class RoundedRect {
public:
    class Radii {
    public:
        Radii() {}
        Radii(const LayoutSize& topLeft, const LayoutSize& topRight, const LayoutSize& bottomLeft, const LayoutSize& bottomRight)
            : m_topLeft(topLeft)
            , m_topRight(topRight)
            , m_bottomLeft(bottomLeft)
            , m_bottomRight(bottomRight)
        {
        }

        void setTopLeft(const LayoutSize& size) { m_topLeft = size; }
        void setTopRight(const LayoutSize& size) { m_topRight = size; }
        void setBottomLeft(const LayoutSize& size) { m_bottomLeft = size; }
        void setBottomRight(const LayoutSize& size) { m_bottomRight = size; }
        const LayoutSize& topLeft() const { return m_topLeft; }
        const LayoutSize& topRight() const { return m_topRight; }
        const LayoutSize& bottomLeft() const { return m_bottomLeft; }
        const LayoutSize& bottomRight() const { return m_bottomRight; }

        bool isZero() const;

        void includeLogicalEdges(const Radii& edges, bool isHorizontal, bool includeLogicalLeftEdge, bool includeLogicalRightEdge);
        void excludeLogicalEdges(bool isHorizontal, bool excludeLogicalLeftEdge, bool excludeLogicalRightEdge);

        void scale(float factor);
        void expand(const LayoutUnit& topWidth, const LayoutUnit& bottomWidth, const LayoutUnit& leftWidth, const LayoutUnit& rightWidth);
        void expand(const LayoutUnit& size) { expand(size, size, size, size); }
        void shrink(const LayoutUnit& topWidth, const LayoutUnit& bottomWidth, const LayoutUnit& leftWidth, const LayoutUnit& rightWidth) { expand(-topWidth, -bottomWidth, -leftWidth, -rightWidth); }
        void shrink(const LayoutUnit& size) { shrink(size, size, size, size); }

        Radii transposedRadii() const { return Radii(m_topLeft.transposedSize(), m_topRight.transposedSize(), m_bottomLeft.transposedSize(), m_bottomRight.transposedSize()); }

    private:
        LayoutSize m_topLeft;
        LayoutSize m_topRight;
        LayoutSize m_bottomLeft;
        LayoutSize m_bottomRight;
    };

    WEBCORE_EXPORT explicit RoundedRect(const LayoutRect&, const Radii& = Radii());
    RoundedRect(const LayoutUnit&, const LayoutUnit&, const LayoutUnit& width, const LayoutUnit& height);
    WEBCORE_EXPORT RoundedRect(const LayoutRect&, const LayoutSize& topLeft, const LayoutSize& topRight, const LayoutSize& bottomLeft, const LayoutSize& bottomRight);

    const LayoutRect& rect() const { return m_rect; }
    const Radii& radii() const { return m_radii; }
    bool isRounded() const { return !m_radii.isZero(); }
    bool isEmpty() const { return m_rect.isEmpty(); }

    void setRect(const LayoutRect& rect) { m_rect = rect; }
    void setRadii(const Radii& radii) { m_radii = radii; }

    void move(const LayoutSize& size) { m_rect.move(size); }
    void moveBy(const LayoutPoint& offset) { m_rect.moveBy(offset); }
    void inflate(const LayoutUnit& size) { m_rect.inflate(size);  }
    void inflateWithRadii(const LayoutUnit& size);
    void expandRadii(const LayoutUnit& size) { m_radii.expand(size); }
    void shrinkRadii(const LayoutUnit& size) { m_radii.shrink(size); }

    void includeLogicalEdges(const Radii& edges, bool isHorizontal, bool includeLogicalLeftEdge, bool includeLogicalRightEdge);
    void excludeLogicalEdges(bool isHorizontal, bool excludeLogicalLeftEdge, bool excludeLogicalRightEdge);

    bool isRenderable() const;
    void adjustRadii();

    // Tests whether the quad intersects any part of this rounded rectangle.
    // This only works for convex quads.
    bool intersectsQuad(const FloatQuad&) const;
    WEBCORE_EXPORT bool contains(const LayoutRect&) const;

    FloatRoundedRect pixelSnappedRoundedRectForPainting(float deviceScaleFactor) const;

    RoundedRect transposedRect() const { return RoundedRect(m_rect.transposedRect(), m_radii.transposedRadii()); }

private:
    LayoutRect m_rect;
    Radii m_radii;
};

inline bool operator==(const RoundedRect::Radii& a, const RoundedRect::Radii& b)
{
    return a.topLeft() == b.topLeft() && a.topRight() == b.topRight() && a.bottomLeft() == b.bottomLeft() && a.bottomRight() == b.bottomRight();
}

inline bool operator==(const RoundedRect& a, const RoundedRect& b)
{
    return a.rect() == b.rect() && a.radii() == b.radii();
}

// Snip away rectangles from corners, roughly one per step length of arc.
WEBCORE_EXPORT Region approximateAsRegion(const RoundedRect&, unsigned stepLength = 20);

} // namespace WebCore
