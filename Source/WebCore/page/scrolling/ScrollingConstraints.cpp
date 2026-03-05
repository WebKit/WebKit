/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScrollingConstraints.h"

#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(AbsolutePositionConstraints);
WTF_MAKE_TZONE_ALLOCATED_IMPL(ViewportConstraints);

AbsolutePositionConstraints::AbsolutePositionConstraints(const FloatSize& alignmentOffset, const FloatPoint& layerPositionAtLastLayout)
    : m_alignmentOffset(alignmentOffset)
    , m_layerPositionAtLastLayout(layerPositionAtLastLayout)
{
}

FloatPoint ViewportConstraints::viewportRelativeLayerPosition(const FloatRect& viewportRect) const
{
    FloatSize offset;

    if (hasAnchorEdge(AnchorEdgeLeft) || !hasAnchorEdge(AnchorEdgeRight))
        offset.setWidth(viewportRect.x() - m_viewportRectAtLastLayout.x());
    else
        offset.setWidth(viewportRect.maxX() - m_viewportRectAtLastLayout.maxX());

    if (hasAnchorEdge(AnchorEdgeTop) || !hasAnchorEdge(AnchorEdgeBottom))
        offset.setHeight(viewportRect.y() - m_viewportRectAtLastLayout.y());
    else
        offset.setHeight(viewportRect.maxY() - m_viewportRectAtLastLayout.maxY());

    return m_layerPositionAtLastLayout + offset;
}

FloatSize StickyPositionViewportConstraints::computeStickyOffset(const FloatRect& constrainingRect) const
{
    FloatRect boxRect = m_stickyBoxRect;
    
    if (hasAnchorEdge(AnchorEdgeRight)) {
        float rightLimit = constrainingRect.maxX() - m_rightOffset;
        float rightDelta = std::min<float>(0, rightLimit - m_stickyBoxRect.maxX());
        float availableSpace = std::min<float>(0, m_containingBlockRect.x() - m_stickyBoxRect.x());
        if (rightDelta < availableSpace)
            rightDelta = availableSpace;

        boxRect.move(rightDelta, 0);
    }

    if (hasAnchorEdge(AnchorEdgeLeft)) {
        float leftLimit = constrainingRect.x() + m_leftOffset;
        float leftDelta = std::max<float>(0, leftLimit - m_stickyBoxRect.x());
        float availableSpace = std::max<float>(0, m_containingBlockRect.maxX() - m_stickyBoxRect.maxX());
        if (leftDelta > availableSpace)
            leftDelta = availableSpace;

        boxRect.move(leftDelta, 0);
    }
    
    if (hasAnchorEdge(AnchorEdgeBottom)) {
        float bottomLimit = constrainingRect.maxY() - m_bottomOffset;
        float bottomDelta = std::min<float>(0, bottomLimit - m_stickyBoxRect.maxY());
        float availableSpace = std::min<float>(0, m_containingBlockRect.y() - m_stickyBoxRect.y());
        if (bottomDelta < availableSpace)
            bottomDelta = availableSpace;

        boxRect.move(0, bottomDelta);
    }

    if (hasAnchorEdge(AnchorEdgeTop)) {
        float topLimit = constrainingRect.y() + m_topOffset;
        float topDelta = std::max<float>(0, topLimit - m_stickyBoxRect.y());
        float availableSpace = std::max<float>(0, m_containingBlockRect.maxY() - m_stickyBoxRect.maxY());
        if (topDelta > availableSpace)
            topDelta = availableSpace;

        boxRect.move(0, topDelta);
    }

    return boxRect.location() - m_stickyBoxRect.location();
}

FloatPoint StickyPositionViewportConstraints::anchorLayerPositionForConstrainingRect(const FloatRect& constrainingRect) const
{
    FloatSize offset = computeStickyOffset(constrainingRect);
    return anchorLayerPositionAtLastLayout() + offset - m_stickyOffsetAtLastLayout;
}

FloatPoint StickyPositionViewportConstraints::anchorLayerPositionAtLastLayout() const
{
    return m_layerPositionAtLastLayout + m_anchorLayerOffsetAtLastLayout;
}

FloatRect StickyPositionViewportConstraints::computeStickyExtent() const
{
    float minShiftX = 0;
    float maxShiftX = 0;
    float minShiftY = 0;
    float maxShiftY = 0;
    if (hasAnchorEdge(AnchorEdgeRight))
        minShiftX = std::min<float>(0, m_containingBlockRect.x() - m_stickyBoxRect.x());
    if (hasAnchorEdge(AnchorEdgeLeft))
        maxShiftX = std::max<float>(0, m_containingBlockRect.maxX() - m_stickyBoxRect.maxX());
    if (hasAnchorEdge(AnchorEdgeBottom))
        minShiftY = std::min<float>(0, m_containingBlockRect.y() - m_stickyBoxRect.y());
    if (hasAnchorEdge(AnchorEdgeTop))
        maxShiftY = std::max<float>(0, m_containingBlockRect.maxY() - m_stickyBoxRect.maxY());
    float minX = m_stickyBoxRect.x() + minShiftX;
    float minY = m_stickyBoxRect.y() + minShiftY;
    float maxX = m_stickyBoxRect.maxX() + maxShiftX;
    float maxY = m_stickyBoxRect.maxY() + maxShiftY;
    return FloatRect(minX, minY, maxX - minX, maxY - minY);
}

TextStream& operator<<(TextStream& ts, ScrollPositioningBehavior behavior)
{
    switch (behavior) {
    case ScrollPositioningBehavior::None: ts << "none"_s; break;
    case ScrollPositioningBehavior::Stationary: ts << "stationary"_s; break;
    case ScrollPositioningBehavior::Moves: ts << "moves"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, const AbsolutePositionConstraints& constraints)
{
    ts.dumpProperty("layer-position-at-last-layout"_s, constraints.layerPositionAtLastLayout());

    return ts;
}

TextStream& operator<<(TextStream& ts, const ViewportConstraints& constraints)
{
    if (auto fixedConstraints = dynamicDowncast<FixedPositionViewportConstraints>(constraints))
        return ts << *fixedConstraints;
    if (auto stickyConstraints = dynamicDowncast<StickyPositionViewportConstraints>(constraints))
        return ts << *stickyConstraints;
    ASSERT_NOT_REACHED();
    return ts;
}

TextStream& operator<<(TextStream& ts, const FixedPositionViewportConstraints& constraints)
{
    ts.dumpProperty("viewport-rect-at-last-layout"_s, constraints.viewportRectAtLastLayout());
    ts.dumpProperty("layer-position-at-last-layout"_s, constraints.layerPositionAtLastLayout());

    return ts;
}

TextStream& operator<<(TextStream& ts, const StickyPositionViewportConstraints& constraints)
{
    ts.dumpProperty("sticky-position-at-last-layout"_s, constraints.stickyOffsetAtLastLayout());
    ts.dumpProperty("viewport-rect-at-last-layout"_s, constraints.viewportRectAtLastLayout());
    ts.dumpProperty("layer-position-at-last-layout"_s, constraints.layerPositionAtLastLayout());
    ts.dumpProperty("anchor-layer-offset-at-last-layout"_s, constraints.anchorLayerOffsetAtLastLayout());

    ts.dumpProperty("sticky-box-rect"_s, constraints.stickyBoxRect());
    ts.dumpProperty("containing-block-rect"_s, constraints.containingBlockRect());
    ts.dumpProperty("constraining-rect-at-last-layout"_s, constraints.constrainingRectAtLastLayout());

    return ts;
}

} // namespace WebCore
