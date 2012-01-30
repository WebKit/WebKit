/*
 * Copyright 2011 Adobe Systems Incorporated. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "RenderRegion.h"

#include "CSSStyleSelector.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "IntRect.h"
#include "PaintInfo.h"
#include "RenderBoxRegionInfo.h"
#include "RenderFlowThread.h"
#include "RenderView.h"

namespace WebCore {

RenderRegion::RenderRegion(Node* node, RenderFlowThread* flowThread)
    : RenderReplaced(node, IntSize())
    , m_flowThread(flowThread)
    , m_parentFlowThread(0)
    , m_isValid(false)
    , m_hasCustomRegionStyle(false)
#ifndef NDEBUG
    , m_insideRegionPaint(false)
#endif
{
}

LayoutRect RenderRegion::regionOverflowRect() const
{
    // FIXME: Would like to just use hasOverflowClip() but we aren't a block yet. When RenderRegion is eliminated and
    // folded into RenderBlock, switch to hasOverflowClip().
    bool clipX = style()->overflowX() != OVISIBLE;
    bool clipY = style()->overflowY() != OVISIBLE;
    if ((clipX && clipY) || !isValid() || !m_flowThread)
        return regionRect();

    LayoutRect flowThreadOverflow = m_flowThread->visualOverflowRect();

    // Only clip along the flow thread axis.
    LayoutUnit outlineSize = maximalOutlineSize(PaintPhaseOutline);
    LayoutRect clipRect;
    if (m_flowThread->isHorizontalWritingMode()) {
        LayoutUnit minY = isFirstRegion() ? (flowThreadOverflow.y() - outlineSize) : regionRect().y();
        LayoutUnit maxY = isLastRegion() ? max(regionRect().maxY(), flowThreadOverflow.maxY()) + outlineSize : regionRect().maxY();
        LayoutUnit minX = clipX ? regionRect().x() : (flowThreadOverflow.x() - outlineSize);
        LayoutUnit maxX = clipX ? regionRect().maxX() : (flowThreadOverflow.maxX() + outlineSize);
        clipRect = LayoutRect(minX, minY, maxX - minX, maxY - minY);
    } else {
        LayoutUnit minX = isFirstRegion() ? (flowThreadOverflow.x() - outlineSize) : regionRect().x();
        LayoutUnit maxX = isLastRegion() ? max(regionRect().maxX(), flowThreadOverflow.maxX()) + outlineSize : regionRect().maxX();
        LayoutUnit minY = clipY ? regionRect().y() : (flowThreadOverflow.y() - outlineSize);
        LayoutUnit maxY = clipY ? regionRect().maxY() : (flowThreadOverflow.maxY() + outlineSize);
        clipRect = LayoutRect(minX, minY, maxX - minX, maxY - minY);
    }

    return clipRect;
}

bool RenderRegion::isFirstRegion() const
{
    ASSERT(isValid() && m_flowThread);
    return m_flowThread->firstRegion() == this;
}

bool RenderRegion::isLastRegion() const
{
    ASSERT(isValid() && m_flowThread);
    return m_flowThread->lastRegion() == this;
}

void RenderRegion::setRegionBoxesRegionStyle()
{
    for (RenderBoxRegionInfoMap::iterator iter = m_renderBoxRegionInfo.begin(), end = m_renderBoxRegionInfo.end(); iter != end; ++iter) {
        const RenderBox* box = iter->first;
        if (!box->canHaveRegionStyle())
            continue;

        // Save original box style to be restored later, after paint.
        RefPtr<RenderStyle> boxOriginalStyle = box->style();

        // Set the style to be used for box as the style computed in region.
        (const_cast<RenderBox*>(box))->setStyleInternal(renderBoxRegionStyle(box));

        m_renderBoxRegionStyle.set(box, boxOriginalStyle);
    }
}

void RenderRegion::restoreRegionBoxesOriginalStyle()
{
    for (RenderBoxRegionInfoMap::iterator iter = m_renderBoxRegionInfo.begin(), end = m_renderBoxRegionInfo.end(); iter != end; ++iter) {
        const RenderBox* box = iter->first;
        RenderBoxRegionStyleMap::iterator it = m_renderBoxRegionStyle.find(box);
        if (it == m_renderBoxRegionStyle.end())
            continue;

        // Restore the box style to the original style and store the box style in region for later use.
        RefPtr<RenderStyle> boxRegionStyle = box->style();
        (const_cast<RenderBox*>(box))->setStyleInternal(it->second);
        m_renderBoxRegionStyle.set(box, boxRegionStyle);
    }
}

void RenderRegion::paintReplaced(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    // Delegate painting of content in region to RenderFlowThread.
    if (!m_flowThread || !isValid())
        return;

#ifndef NDEBUG
    m_insideRegionPaint = true;
#endif

    setRegionBoxesRegionStyle();
    m_flowThread->paintIntoRegion(paintInfo, this, LayoutPoint(paintOffset.x() + borderLeft() + paddingLeft(), paintOffset.y() + borderTop() + paddingTop()));
    restoreRegionBoxesOriginalStyle();

#ifndef NDEBUG
    m_insideRegionPaint = false;
#endif
}

// Hit Testing
bool RenderRegion::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const LayoutPoint& pointInContainer, const LayoutPoint& accumulatedOffset, HitTestAction action)
{
    if (!isValid())
        return false;

    LayoutPoint adjustedLocation = accumulatedOffset + location();

    // Check our bounds next. For this purpose always assume that we can only be hit in the
    // foreground phase (which is true for replaced elements like images).
    LayoutRect boundsRect(adjustedLocation, size());
    if (visibleToHitTesting() && action == HitTestForeground && boundsRect.intersects(result.rectForPoint(pointInContainer))) {
        // Check the contents of the RenderFlowThread.
        if (m_flowThread && m_flowThread->hitTestRegion(this, request, result, pointInContainer, LayoutPoint(adjustedLocation.x() + borderLeft() + paddingLeft(), adjustedLocation.y() + borderTop() + paddingTop())))
            return true;
        updateHitTestResult(result, pointInContainer - toLayoutSize(adjustedLocation));
        if (!result.addNodeToRectBasedTestResult(node(), pointInContainer, boundsRect))
            return true;
    }

    return false;
}

void RenderRegion::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderReplaced::styleDidChange(diff, oldStyle);
    bool customRegionStyle = false;
    if (node()) {
        Element* regionElement = static_cast<Element*>(node());
        customRegionStyle = view()->document()->styleSelector()->checkRegionStyle(regionElement);
    }
    setHasCustomRegionStyle(customRegionStyle);
}

void RenderRegion::layout()
{
    RenderReplaced::layout();
    if (m_flowThread && isValid()) {
        if (regionRect().width() != contentWidth() || regionRect().height() != contentHeight())
            m_flowThread->invalidateRegions();
    }

    // FIXME: We need to find a way to set up overflow properly. Our flow thread hasn't gotten a layout
    // yet, so we can't look to it for correct information. It's possible we could wait until after the RenderFlowThread
    // gets a layout, and then try to propagate overflow information back to the region, and then mark for a second layout.
    // That second layout would then be able to use the information from the RenderFlowThread to set up overflow.
    //
    // The big problem though is that overflow needs to be region-specific. We can't simply use the RenderFlowThread's global
    // overflow values, since then we'd always think any narrow region had huge overflow (all the way to the width of the
    // RenderFlowThread itself).
    //
    // We'll need to expand RenderBoxRegionInfo to also hold left and right overflow values.
}

void RenderRegion::attachRegion()
{
    if (!m_flowThread)
        return;

    // By now the flow thread should already be added to the rendering tree,
    // so we go up the rendering parents and check that this region is not part of the same
    // flow that it actually needs to display. It would create a circular reference.
    RenderObject* parentObject = parent();
    m_parentFlowThread = 0;
    for ( ; parentObject; parentObject = parentObject->parent()) {
        if (parentObject->isRenderFlowThread()) {
            m_parentFlowThread = toRenderFlowThread(parentObject);
            // Do not take into account a region that links a flow with itself. The dependency
            // cannot change, so it is not worth adding it to the list.
            if (m_flowThread == m_parentFlowThread) {
                m_flowThread = 0;
                return;
            }
            break;
        }
    }

    m_flowThread->addRegionToThread(this);
}

void RenderRegion::detachRegion()
{
    if (m_flowThread)
        m_flowThread->removeRegionFromThread(this);
}

RenderBoxRegionInfo* RenderRegion::renderBoxRegionInfo(const RenderBox* box) const
{
    if (!m_isValid || !m_flowThread)
        return 0;
    return m_renderBoxRegionInfo.get(box);
}

RenderBoxRegionInfo* RenderRegion::setRenderBoxRegionInfo(const RenderBox* box, LayoutUnit logicalLeftInset, LayoutUnit logicalRightInset,
    bool containingBlockChainIsInset)
{
    ASSERT(m_isValid && m_flowThread);
    if (!m_isValid || !m_flowThread)
        return 0;

#ifndef NDEBUG
    ASSERT(!m_insideRegionPaint && "RenderBoxRegionInfo should not be modified inside region paint.");
#endif

    OwnPtr<RenderBoxRegionInfo>& boxInfo = m_renderBoxRegionInfo.add(box, nullptr).first->second;
    if (boxInfo)
        *boxInfo = RenderBoxRegionInfo(logicalLeftInset, logicalRightInset, containingBlockChainIsInset);
    else
        boxInfo = adoptPtr(new RenderBoxRegionInfo(logicalLeftInset, logicalRightInset, containingBlockChainIsInset));

    return boxInfo.get();
}

PassOwnPtr<RenderBoxRegionInfo> RenderRegion::takeRenderBoxRegionInfo(const RenderBox* box)
{
#ifndef NDEBUG
    ASSERT(!m_insideRegionPaint && "RenderBoxRegionInfo should not be modified inside region paint.");
#endif

    return m_renderBoxRegionInfo.take(box);
}

void RenderRegion::removeRenderBoxRegionInfo(const RenderBox* box)
{
#ifndef NDEBUG
    ASSERT(!m_insideRegionPaint && "RenderBoxRegionInfo should not be modified inside region paint.");
#endif

    m_renderBoxRegionInfo.remove(box);
}

void RenderRegion::deleteAllRenderBoxRegionInfo()
{
    m_renderBoxRegionInfo.clear();
}

LayoutUnit RenderRegion::offsetFromLogicalTopOfFirstPage() const
{
    if (!m_isValid || !m_flowThread)
        return 0;
    if (m_flowThread->isHorizontalWritingMode())
        return regionRect().y();
    return regionRect().x();
}

PassRefPtr<RenderStyle> RenderRegion::renderBoxRegionStyle(const RenderBox* renderBox)
{
    // The box for which we are asking for style in region should have its info present
    // in the region box info map.
    ASSERT(m_renderBoxRegionInfo.find(renderBox) != m_renderBoxRegionInfo.end());

    RenderBoxRegionStyleMap::iterator it = m_renderBoxRegionStyle.find(renderBox);
    if (it != m_renderBoxRegionStyle.end())
        return it->second;
    return computeStyleInRegion(renderBox);
}

PassRefPtr<RenderStyle> RenderRegion::computeStyleInRegion(const RenderBox* box)
{
    ASSERT(box);
    ASSERT(box->view());
    ASSERT(box->view()->document());
    ASSERT(!box->isAnonymous());
    ASSERT(box->node() && box->node()->isElementNode());

    Element* element = toElement(box->node());
    RefPtr<RenderStyle> renderBoxRegionStyle = box->view()->document()->styleSelector()->styleForElement(element, 0, false, false, this);
    m_renderBoxRegionStyle.add(box, renderBoxRegionStyle);

    if (!box->hasBoxDecorations()) {
        bool hasBoxDecorations = box->isTableCell() || renderBoxRegionStyle->hasBackground() || renderBoxRegionStyle->hasBorder() || renderBoxRegionStyle->hasAppearance() || renderBoxRegionStyle->boxShadow();
        (const_cast<RenderBox*>(box))->setHasBoxDecorations(hasBoxDecorations);
    }

    return renderBoxRegionStyle.release();
}

void RenderRegion::clearBoxStyleInRegion(const RenderBox* box)
{
    ASSERT(box);
    m_renderBoxRegionStyle.remove(box);
}

} // namespace WebCore
