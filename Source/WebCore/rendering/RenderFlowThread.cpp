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

#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "Node.h"
#include "PaintInfo.h"
#include "RenderLayer.h"
#include "RenderRegion.h"
#include "RenderView.h"

namespace WebCore {

RenderFlowThread::RenderFlowThread(Node* node, const AtomicString& flowThread)
    : RenderBlock(node)
    , m_flowThread(flowThread)
    , m_hasValidRegions(false)
    , m_regionsInvalidated(false)
    , m_regionFittingDisableCount(0)
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

void RenderFlowThread::addFlowChild(RenderObject* newChild, RenderObject* beforeChild)
{
    // The child list is used to sort the flow thread's children render objects 
    // based on their corresponding nodes DOM order. The list is needed to avoid searching the whole DOM.

    // Do not add anonymous objects.
    if (!newChild->node())
        return;

    if (beforeChild)
        m_flowThreadChildList.insertBefore(beforeChild, newChild);
    else
        m_flowThreadChildList.add(newChild);
}

void RenderFlowThread::removeFlowChild(RenderObject* child)
{
    m_flowThreadChildList.remove(child);
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

bool RenderFlowThread::dependsOn(RenderFlowThread* otherRenderFlowThread) const
{
    if (m_layoutBeforeThreadsSet.contains(otherRenderFlowThread))
        return true;

    // Recursively traverse the m_layoutBeforeThreadsSet.
    RenderFlowThreadCountedSet::const_iterator iterator = m_layoutBeforeThreadsSet.begin();
    RenderFlowThreadCountedSet::const_iterator end = m_layoutBeforeThreadsSet.end();
    for (; iterator != end; ++iterator) {
        const RenderFlowThread* beforeFlowThread = (*iterator).first;
        if (beforeFlowThread->dependsOn(otherRenderFlowThread))
            return true;
    }

    return false;
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

    ASSERT(!renderRegion->isValid());
    if (renderRegion->parentFlowThread()) {
        if (renderRegion->parentFlowThread()->dependsOn(this)) {
            // Register ourself to get a notification when the state changes.
            renderRegion->parentFlowThread()->m_observerThreadsSet.add(this);
            return;
        }

        addDependencyOnFlowThread(renderRegion->parentFlowThread());
    }

    renderRegion->setIsValid(true);

    invalidateRegions();
}

void RenderFlowThread::removeRegionFromThread(RenderRegion* renderRegion)
{
    ASSERT(renderRegion);
    m_regionList.remove(renderRegion);
    if (renderRegion->parentFlowThread()) {
        if (!renderRegion->isValid()) {
            renderRegion->parentFlowThread()->m_observerThreadsSet.remove(this);
            // No need to invalidate the regions rectangles. The removed region
            // was not taken into account. Just return here.
            return;
        }
        removeDependencyOnFlowThread(renderRegion->parentFlowThread());
    }

    invalidateRegions();
}

void RenderFlowThread::checkInvalidRegions()
{
    for (RenderRegionList::iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        // The only reason a region would be invalid is because it has a parent flow thread.
        ASSERT(region->isValid() || region->parentFlowThread());
        if (region->isValid() || region->parentFlowThread()->dependsOn(this))
            continue;

        region->parentFlowThread()->m_observerThreadsSet.remove(this);
        addDependencyOnFlowThread(region->parentFlowThread());
        region->setIsValid(true);
        invalidateRegions();
    }

    if (m_observerThreadsSet.isEmpty())
        return;

    // Notify all the flow threads that were dependent on this flow.

    // Create a copy of the list first. That's because observers might change the list when calling checkInvalidRegions.
    Vector<RenderFlowThread*> observers;
    copyToVector(m_observerThreadsSet, observers);

    for (size_t i = 0; i < observers.size(); ++i) {
        RenderFlowThread* flowThread = observers.at(i);
        flowThread->checkInvalidRegions();
    }
}

void RenderFlowThread::addDependencyOnFlowThread(RenderFlowThread* otherFlowThread)
{
    std::pair<RenderFlowThreadCountedSet::iterator, bool> result = m_layoutBeforeThreadsSet.add(otherFlowThread);
    if (result.second) {
        // This is the first time we see this dependency. Make sure we recalculate all the dependencies.
        view()->setIsRenderFlowThreadOrderDirty(true);
    }
}

void RenderFlowThread::removeDependencyOnFlowThread(RenderFlowThread* otherFlowThread)
{
    bool removed = m_layoutBeforeThreadsSet.remove(otherFlowThread);
    if (removed) {
        checkInvalidRegions();
        view()->setIsRenderFlowThreadOrderDirty(true);
    }
}

void RenderFlowThread::pushDependencies(RenderFlowThreadList& list)
{
    for (RenderFlowThreadCountedSet::iterator iter = m_layoutBeforeThreadsSet.begin(); iter != m_layoutBeforeThreadsSet.end(); ++iter) {
        RenderFlowThread* flowThread = (*iter).first;
        if (list.contains(flowThread))
            continue;
        flowThread->pushDependencies(list);
        list.add(flowThread);
    }
}

class CurrentRenderFlowThreadMaintainer {
    WTF_MAKE_NONCOPYABLE(CurrentRenderFlowThreadMaintainer);
public:
    CurrentRenderFlowThreadMaintainer(RenderFlowThread* renderFlowThread)
        : m_renderFlowThread(renderFlowThread)
    {
        RenderView* view = m_renderFlowThread->view();
        ASSERT(!view->currentRenderFlowThread());
        view->setCurrentRenderFlowThread(m_renderFlowThread);
    }
    ~CurrentRenderFlowThreadMaintainer()
    {
        RenderView* view = m_renderFlowThread->view();
        ASSERT(view->currentRenderFlowThread() == m_renderFlowThread);
        view->setCurrentRenderFlowThread(0);
    }
private:
    RenderFlowThread* m_renderFlowThread;
};

void RenderFlowThread::layout()
{
    CurrentRenderFlowThreadMaintainer currentFlowThreadSetter(this);

    if (m_regionsInvalidated) {
        m_regionsInvalidated = false;
        if (hasRegions()) {
            int logicalHeight = 0;
            for (RenderRegionList::iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
                RenderRegion* region = *iter;

                if (!region->isValid())
                    continue;

                ASSERT(!region->needsLayout());
                
                m_hasValidRegions = true;

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
        if (!region->isValid())
            continue;
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
        if (!region->isValid())
            continue;
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

bool RenderFlowThread::hitTestRegion(const LayoutRect& regionRect, const HitTestRequest& request, HitTestResult& result, const LayoutPoint& pointInContainer, const LayoutPoint& accumulatedOffset)
{
    LayoutRect regionClippingRect(accumulatedOffset, regionRect.size());
    if (!regionClippingRect.contains(pointInContainer))
        return false;
    
    LayoutPoint renderFlowThreadOffset;
    if (style()->isFlippedBlocksWritingMode()) {
        LayoutRect flippedRegionRect(regionRect);
        flipForWritingMode(flippedRegionRect);
        renderFlowThreadOffset = LayoutPoint(regionClippingRect.location() - flippedRegionRect.location());
    } else
        renderFlowThreadOffset = LayoutPoint(regionClippingRect.location() - regionRect.location());

    LayoutPoint transformedPoint(pointInContainer.x() - renderFlowThreadOffset.x(), pointInContainer.y() - renderFlowThreadOffset.y());
    
    // Always ignore clipping, since the RenderFlowThread has nothing to do with the bounds of the FrameView.
    HitTestRequest newRequest(request.type() & HitTestRequest::IgnoreClipping);

    LayoutPoint oldPoint = result.point();
    result.setPoint(transformedPoint);
    bool isPointInsideFlowThread = layer()->hitTest(newRequest, result);
    result.setPoint(oldPoint);
    
    // FIXME: Should we set result.m_localPoint back to the RenderRegion's coordinate space or leave it in the RenderFlowThread's coordinate
    // space? Right now it's staying in the RenderFlowThread's coordinate space, which may end up being ok. We will know more when we get around to
    // patching positionForPoint.
    return isPointInsideFlowThread;
}

bool RenderFlowThread::shouldRepaint(const LayoutRect& r) const
{
    if (view()->printing() || r.isEmpty())
        return false;

    return true;
}

void RenderFlowThread::repaintRectangleInRegions(const LayoutRect& repaintRect, bool immediate)
{
    if (!shouldRepaint(repaintRect))
        return;

    for (RenderRegionList::iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        if (!region->isValid())
            continue;

        // We only have to issue a repaint in this region if the region rect intersects the repaint rect.
        LayoutRect flippedRegionRect(region->regionRect());
        flipForWritingMode(flippedRegionRect); // Put the region rect into physical coordinates.
        
        IntRect clippedRect(flippedRegionRect);
        clippedRect.intersect(repaintRect);
        if (clippedRect.isEmpty())
            continue;
        
        // Put the region rect into the region's physical coordinate space.
        clippedRect.setLocation(region->contentBoxRect().location() + (repaintRect.location() - flippedRegionRect.location()));
        
        // Now switch to the region's writing mode coordinate space and let it repaint itself.
        region->flipForWritingMode(clippedRect);
        region->repaintRectangle(clippedRect, immediate);
    }
}

RenderRegion* RenderFlowThread::renderRegionForLine(LayoutUnit position, bool extendLastRegion) const
{
    ASSERT(!m_regionsInvalidated);
    
    // All the regions should start at 0.
    ASSERT(position >= 0);
    
    // If no region matches the position and extendLastRegion is true, it will return
    // the last valid region. It is similar to auto extending the size of the last region. 
    RenderRegion* lastValidRegion = 0;
    
    // FIXME: The regions are always in order, optimize this search.
    bool useHorizontalWritingMode = isHorizontalWritingMode();
    for (RenderRegionList::const_iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        if (!region->isValid())
            continue;

        LayoutRect regionRect = region->regionRect();

        if (useHorizontalWritingMode) {
            if (regionRect.y() <= position && position < regionRect.maxY())
                return region;
            continue;
        }

        if (regionRect.x() <= position && position < regionRect.maxX())
            return region;
        
        if (extendLastRegion)
            lastValidRegion = region;
    }

    return lastValidRegion;
}

LayoutUnit RenderFlowThread::regionLogicalWidthForLine(LayoutUnit position) const
{
    const bool extendLastRegion = true;
    RenderRegion* region = renderRegionForLine(position, extendLastRegion);
    if (!region)
        return 0;

    return isHorizontalWritingMode() ? region->regionRect().width() : region->regionRect().height();
}


} // namespace WebCore
