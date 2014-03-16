/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef BorderEdge_h
#define BorderEdge_h

#include "RenderObject.h"

namespace WebCore {

typedef unsigned BorderEdgeFlags;

class RenderStyle;
class LayoutUnit;

class BorderEdge {
public:
    enum BorderEdgeFlag {
        TopBorderEdge = 1 << BSTop,
        RightBorderEdge = 1 << BSRight,
        BottomBorderEdge = 1 << BSBottom,
        LeftBorderEdge = 1 << BSLeft,
        AllBorderEdges = TopBorderEdge | BottomBorderEdge | LeftBorderEdge | RightBorderEdge
    };

    BorderEdge();
    BorderEdge(LayoutUnit edgeWidth, Color edgeColor, EBorderStyle edgeStyle, bool edgeIsTransparent, bool edgeIsPresent, float devicePixelRatio);

    static void getBorderEdgeInfo(BorderEdge edges[], const RenderStyle&, float deviceScaleFactor, bool includeLogicalLeftEdge = true, bool includeLogicalRightEdge = true);

    EBorderStyle style() const { return m_style; }
    Color color() const { return m_color; }
    bool isTransparent() const { return m_isTransparent; }
    bool isPresent() const { return m_isPresent; }

    inline bool hasVisibleColorAndStyle() const { return m_style > BHIDDEN && !m_isTransparent; }
    inline bool shouldRender() const { return m_isPresent && widthForPainting() && hasVisibleColorAndStyle(); }
    inline bool presentButInvisible() const { return widthForPainting() && !hasVisibleColorAndStyle(); }
    inline float widthForPainting() const { return m_isPresent ?  m_flooredToDevicePixelWidth : 0; }
    void getDoubleBorderStripeWidths(LayoutUnit outerWidth, LayoutUnit innerWidth) const;
    bool obscuresBackgroundEdge(float scale) const;
    bool obscuresBackground() const;

private:
    inline float borderWidthInDevicePixel(int logicalPixels) const { return LayoutUnit(logicalPixels / m_devicePixelRatio).toFloat(); }

    LayoutUnit m_width;
    Color m_color;
    EBorderStyle m_style;
    bool m_isTransparent;
    bool m_isPresent;
    float m_flooredToDevicePixelWidth;
    float m_devicePixelRatio;
};

inline bool edgesShareColor(const BorderEdge& firstEdge, const BorderEdge& secondEdge) { return firstEdge.color() == secondEdge.color(); }
inline BorderEdge::BorderEdgeFlag edgeFlagForSide(BoxSide side) { return static_cast<BorderEdge::BorderEdgeFlag>(1 << side); }
inline bool includesEdge(BorderEdgeFlags flags, BoxSide side) { return flags & edgeFlagForSide(side); }
inline bool includesAdjacentEdges(BorderEdgeFlags flags)
{
    return (flags & (BorderEdge::TopBorderEdge | BorderEdge::RightBorderEdge)) == (BorderEdge::TopBorderEdge | BorderEdge::RightBorderEdge)
        || (flags & (BorderEdge::RightBorderEdge | BorderEdge::BottomBorderEdge)) == (BorderEdge::RightBorderEdge | BorderEdge::BottomBorderEdge)
        || (flags & (BorderEdge::BottomBorderEdge | BorderEdge::LeftBorderEdge)) == (BorderEdge::BottomBorderEdge | BorderEdge::LeftBorderEdge)
        || (flags & (BorderEdge::LeftBorderEdge | BorderEdge::TopBorderEdge)) == (BorderEdge::LeftBorderEdge | BorderEdge::TopBorderEdge);
}

} // namespace WebCore

#endif // BorderEdge_h
