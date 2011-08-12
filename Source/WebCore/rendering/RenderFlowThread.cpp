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

#include "RenderFlowThread.h"
#include "Node.h"
#include "PaintInfo.h"
#include "RenderLayer.h"
#include "RenderRegion.h"

namespace WebCore {

RenderFlowThread::RenderFlowThread(Node* node, const AtomicString& flowThread)
    : RenderBlock(node)
    , m_flowThread(flowThread)
    , m_regionsInvalidated(false)
{
    setIsAnonymous(false);
}

PassRefPtr<RenderStyle> RenderFlowThread::createFlowThreadStyle(RenderStyle* parentStyle)
{
    RefPtr<RenderStyle> newStyle(RenderStyle::create());
    newStyle->inheritFrom(parentStyle);
    newStyle->setDisplay(BLOCK);
    newStyle->setPosition(AbsolutePosition);
    newStyle->setZIndex(0);
    newStyle->setLeft(Length(0, Fixed));
    newStyle->setTop(Length(0, Fixed));
    newStyle->setWidth(Length(100, Percent));
    newStyle->setHeight(Length(100, Percent));
    newStyle->setOverflowX(OHIDDEN);
    newStyle->setOverflowY(OHIDDEN);
    newStyle->font().update(0);
    
    return newStyle.release();
}

void RenderFlowThread::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);

    if (oldStyle && oldStyle->writingMode() != style()->writingMode())
        m_regionsInvalidated = true;
}

RenderObject* RenderFlowThread::nextRendererForNode(Node* node) const
{
    FlowThreadChildList::const_iterator it = m_flowThreadChildList.begin();
    FlowThreadChildList::const_iterator end = m_flowThreadChildList.end();
    
    for (; it != end; ++it) {
        RenderObject* child = *it;
        ASSERT(child->node());
        unsigned short position = node->compareDocumentPosition(child->node());
        if (position & Node::DOCUMENT_POSITION_FOLLOWING)
            return child;
    }
    
    return 0;
}

RenderObject* RenderFlowThread::previousRendererForNode(Node* node) const
{
    if (m_flowThreadChildList.isEmpty())
        return 0;
    
    FlowThreadChildList::const_iterator begin = m_flowThreadChildList.begin();
    FlowThreadChildList::const_iterator end = m_flowThreadChildList.end();
    FlowThreadChildList::const_iterator it = end;
    
    do {
        --it;
        RenderObject* child = *it;
        ASSERT(child->node());
        unsigned short position = node->compareDocumentPosition(child->node());
        if (position & Node::DOCUMENT_POSITION_PRECEDING)
            return child;
    } while (it != begin);
    
    return 0;
}

void RenderFlowThread::addChild(RenderObject* newChild, RenderObject* beforeChild)
{
    if (beforeChild)
        m_flowThreadChildList.insertBefore(beforeChild, newChild);
    else
        m_flowThreadChildList.add(newChild);
    RenderBlock::addChild(newChild, beforeChild);
}

void RenderFlowThread::removeChild(RenderObject* child)
{
    m_flowThreadChildList.remove(child);
    RenderBlock::removeChild(child);
}

// Compare two regions to determine in which one the content should flow first.
// The function returns true if the first passed region is "less" than the second passed region.
// If the first region index < second region index, then the first region is "less" than the second region.
// If the first region index == second region index and first region appears before second region in DOM, 
// the first region is "less" than the second region.
// If the first region is "less" than the second region, the first region receives content before second region.
static bool compareRenderRegions(const RenderRegion* firstRegion, const RenderRegion* secondRegion)
{
    ASSERT(firstRegion);
    ASSERT(secondRegion);

    // First, compare only region-index properties.
    if (firstRegion->style()->regionIndex() != secondRegion->style()->regionIndex())
        return (firstRegion->style()->regionIndex() < secondRegion->style()->regionIndex());

    // If the regions have the same region-index, compare their position in dom.
    ASSERT(firstRegion->node());
    ASSERT(secondRegion->node());

    unsigned short position = firstRegion->node()->compareDocumentPosition(secondRegion->node());
    return (position & Node::DOCUMENT_POSITION_FOLLOWING);
}

void RenderFlowThread::addRegionToThread(RenderRegion* renderRegion)
{
    ASSERT(renderRegion);
    if (m_regionList.isEmpty())
        m_regionList.add(renderRegion);
    else {
        // Find the first region "greater" than renderRegion.
        RenderRegionList::iterator it = m_regionList.begin();
        while (it != m_regionList.end() && !compareRenderRegions(renderRegion, *it))
            ++it;
        m_regionList.insertBefore(it, renderRegion);
    }

    invalidateRegions();
}

void RenderFlowThread::removeRegionFromThread(RenderRegion* renderRegion)
{
    ASSERT(renderRegion);
    m_regionList.remove(renderRegion);

    invalidateRegions();
}

void RenderFlowThread::layout()
{
    if (m_regionsInvalidated) {
        m_regionsInvalidated = false;
        if (hasRegions()) {
            int logicalHeight = 0;
            for (RenderRegionList::iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
                RenderRegion* region = *iter;

                ASSERT(!region->needsLayout());

                IntRect regionRect;
                if (isHorizontalWritingMode()) {
                    regionRect = IntRect(0, logicalHeight, region->contentWidth(), region->contentHeight());
                    logicalHeight += regionRect.height();
                } else {
                    regionRect = IntRect(logicalHeight, 0, region->contentWidth(), region->contentHeight());
                    logicalHeight += regionRect.width();
                }

                region->setRegionRect(regionRect);                
            }
        }
    }

    RenderBlock::layout();
}

void RenderFlowThread::computeLogicalWidth()
{
    int logicalWidth = 0;

    for (RenderRegionList::iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        ASSERT(!region->needsLayout());
        logicalWidth = max(isHorizontalWritingMode() ? region->contentWidth() : region->contentHeight(), logicalWidth);
    }

    setLogicalWidth(logicalWidth);
}

void RenderFlowThread::computeLogicalHeight()
{
    int logicalHeight = 0;

    for (RenderRegionList::iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        ASSERT(!region->needsLayout());
        logicalHeight += isHorizontalWritingMode() ? region->contentHeight() : region->contentWidth();
    }

    setLogicalHeight(logicalHeight);
}

void RenderFlowThread::paintIntoRegion(PaintInfo& paintInfo, const LayoutRect& regionRect, const LayoutPoint& paintOffset)
{
    GraphicsContext* context = paintInfo.context;
    if (!context)
        return;

    // Adjust the clipping rect for the region.
    // paintOffset contains the offset where the painting should occur
    // adjusted with the region padding and border.
    LayoutRect regionClippingRect(paintOffset, regionRect.size());

    PaintInfo info(paintInfo);
    info.rect.intersect(regionClippingRect);

    if (!info.rect.isEmpty()) {
        context->save();

        context->clip(regionClippingRect);

        // RenderFlowThread should start painting its content in a position that is offset
        // from the region rect's current position. The amount of offset is equal to the location of
        // region in flow coordinates.
        LayoutPoint renderFlowThreadOffset;
        if (style()->isFlippedBlocksWritingMode()) {
            LayoutRect flippedRegionRect(regionRect);
            flipForWritingMode(flippedRegionRect);
            renderFlowThreadOffset = LayoutPoint(regionClippingRect.location() - flippedRegionRect.location());
        } else
            renderFlowThreadOffset = LayoutPoint(regionClippingRect.location() - regionRect.location());

        context->translate(renderFlowThreadOffset.x(), renderFlowThreadOffset.y());
        info.rect.moveBy(-renderFlowThreadOffset);
        
        layer()->paint(context, info.rect);

        context->restore();
    }
}

} // namespace WebCore
