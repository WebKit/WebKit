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

#pragma once

#include "Color.h"
#include "LayoutUnit.h"
#include "RectEdges.h"
#include "RenderObjectEnums.h"
#include "RenderStyleConstants.h"
#include <wtf/OptionSet.h>

namespace WebCore {

class RenderStyle;

class BorderEdge {
public:
    BorderEdge() = default;
    BorderEdge(float edgeWidth, Color edgeColor, BorderStyle edgeStyle, bool edgeIsTransparent, bool edgeIsPresent, float devicePixelRatio);

    BorderStyle style() const { return m_style; }
    const Color& color() const { return m_color; }
    bool isTransparent() const { return m_isTransparent; }
    bool isPresent() const { return m_isPresent; }

    inline bool hasVisibleColorAndStyle() const { return m_style > BorderStyle::Hidden && !m_isTransparent; }
    inline bool shouldRender() const { return m_isPresent && widthForPainting() && hasVisibleColorAndStyle(); }
    inline bool presentButInvisible() const { return widthForPainting() && !hasVisibleColorAndStyle(); }
    inline float widthForPainting() const { return m_isPresent ?  m_flooredToDevicePixelWidth : 0; }
    void getDoubleBorderStripeWidths(LayoutUnit& outerWidth, LayoutUnit& innerWidth) const;
    bool obscuresBackgroundEdge(float scale) const;
    bool obscuresBackground() const;

private:
    inline float borderWidthInDevicePixel(int logicalPixels) const { return LayoutUnit(logicalPixels / m_devicePixelRatio).toFloat(); }

    Color m_color;
    LayoutUnit m_width;
    float m_flooredToDevicePixelWidth { 0 };
    float m_devicePixelRatio { 1 };
    BorderStyle m_style { BorderStyle::Hidden };
    bool m_isTransparent { false };
    bool m_isPresent { false };
};

using BorderEdges = RectEdges<BorderEdge>;
BorderEdges borderEdges(const RenderStyle&, float deviceScaleFactor, bool includeLogicalLeftEdge = true, bool includeLogicalRightEdge = true);
BorderEdges borderEdgesForOutline(const RenderStyle&, float deviceScaleFactor);

inline bool edgesShareColor(const BorderEdge& firstEdge, const BorderEdge& secondEdge) { return firstEdge.color() == secondEdge.color(); }
inline BoxSideFlag edgeFlagForSide(BoxSide side) { return static_cast<BoxSideFlag>(1 << static_cast<unsigned>(side)); }
inline bool includesEdge(OptionSet<BoxSideFlag> flags, BoxSide side) { return flags.contains(edgeFlagForSide(side)); }

inline bool includesAdjacentEdges(OptionSet<BoxSideFlag> flags)
{
    return flags.containsAll({ BoxSideFlag::Top, BoxSideFlag::Right })
        || flags.containsAll({ BoxSideFlag::Right, BoxSideFlag::Bottom })
        || flags.containsAll({ BoxSideFlag::Bottom, BoxSideFlag::Left })
        || flags.containsAll({ BoxSideFlag::Left, BoxSideFlag::Top });
}

} // namespace WebCore
