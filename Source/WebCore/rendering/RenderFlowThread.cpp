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
#include "RenderBoxRegionInfo.h"
#include "RenderLayer.h"
#include "RenderRegion.h"
#include "RenderView.h"
#include "TransformState.h"
#include "WebKitNamedFlow.h"

namespace WebCore {

RenderFlowThread::RenderFlowThread(Node* node, const AtomicString& flowThread)
    : RenderBlock(node)
    , m_flowThread(flowThread)
    , m_hasValidRegions(false)
    , m_regionsInvalidated(false)
    , m_regionsHaveUniformLogicalWidth(true)
    , m_regionsHaveUniformLogicalHeight(true)
{
    setIsAnonymous(false);
    setInRenderFlowThread();
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
// If the first region appears before second region in DOM,
// the first region is "less" than the second region.
// If the first region is "less" than the second region, the first region receives content before second region.
static bool compareRenderRegions(const RenderRegion* firstRegion, const RenderRegion* secondRegion)
{
    ASSERT(firstRegion);
    ASSERT(secondRegion);

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
    m_regionRangeMap.clear();
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

class CurrentRenderFlowThreadDisabler {
    WTF_MAKE_NONCOPYABLE(CurrentRenderFlowThreadDisabler);
public:
    CurrentRenderFlowThreadDisabler(RenderView* view)
        : m_view(view)
        , m_renderFlowThread(0)
    {
        m_renderFlowThread = m_view->currentRenderFlowThread();
        if (m_renderFlowThread)
            view->setCurrentRenderFlowThread(0);
    }
    ~CurrentRenderFlowThreadDisabler()
    {
        if (m_renderFlowThread)
            m_view->setCurrentRenderFlowThread(m_renderFlowThread);
    }
private:
    RenderView* m_view;
    RenderFlowThread* m_renderFlowThread;
};

void RenderFlowThread::layout()
{
    bool regionsChanged = m_regionsInvalidated && everHadLayout();
    if (m_regionsInvalidated) {
        m_regionsInvalidated = false;
        m_hasValidRegions = false;
        m_regionsHaveUniformLogicalWidth = true;
        m_regionsHaveUniformLogicalHeight = true;
        m_regionRangeMap.clear();
        LayoutUnit previousRegionLogicalWidth = 0;
        LayoutUnit previousRegionLogicalHeight = 0;
        if (hasRegions()) {
            for (RenderRegionList::iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
                RenderRegion* region = *iter;
                if (!region->isValid())
                    continue;
                ASSERT(!region->needsLayout());
                
                region->deleteAllRenderBoxRegionInfo();

                LayoutUnit regionLogicalWidth;
                LayoutUnit regionLogicalHeight;

                if (isHorizontalWritingMode()) {
                    regionLogicalWidth = region->contentWidth();
                    regionLogicalHeight = region->contentHeight();
                } else {
                    regionLogicalWidth = region->contentHeight();
                    regionLogicalHeight = region->contentWidth();
                }

                if (!m_hasValidRegions)
                    m_hasValidRegions = true;
                else {
                    if (m_regionsHaveUniformLogicalWidth && previousRegionLogicalWidth != regionLogicalWidth)
                        m_regionsHaveUniformLogicalWidth = false;
                    if (m_regionsHaveUniformLogicalHeight && previousRegionLogicalHeight != regionLogicalHeight)
                        m_regionsHaveUniformLogicalHeight = false;
                }

                previousRegionLogicalWidth = regionLogicalWidth;
            }
            
            computeLogicalWidth(); // Called to get the maximum logical width for the region.
            
            LayoutUnit logicalHeight = 0;
            for (RenderRegionList::iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
                RenderRegion* region = *iter;
                if (!region->isValid())
                    continue;
                LayoutRect regionRect;
                if (isHorizontalWritingMode()) {
                    regionRect = LayoutRect(style()->direction() == LTR ? 0 : logicalWidth() - region->contentWidth(), logicalHeight, region->contentWidth(), region->contentHeight());
                    logicalHeight += regionRect.height();
                } else {
                    regionRect = LayoutRect(logicalHeight, style()->direction() == LTR ? 0 : logicalWidth() - region->contentHeight(), region->contentWidth(), region->contentHeight());
                    logicalHeight += regionRect.width();
                }
                region->setRegionRect(regionRect);
            }
        }
    }

    CurrentRenderFlowThreadMaintainer currentFlowThreadSetter(this);
    LayoutStateMaintainer statePusher(view(), this, regionsChanged);
    RenderBlock::layout();
    statePusher.pop();
}

void RenderFlowThread::computeLogicalWidth()
{
    LayoutUnit logicalWidth = 0;
    for (RenderRegionList::iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        if (!region->isValid())
            continue;
        ASSERT(!region->needsLayout());
        logicalWidth = max(isHorizontalWritingMode() ? region->contentWidth() : region->contentHeight(), logicalWidth);
    }
    setLogicalWidth(logicalWidth);

    // If the regions have non-uniform logical widths, then insert inset information for the RenderFlowThread.
    for (RenderRegionList::iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        if (!region->isValid())
            continue;
        
        LayoutUnit regionLogicalWidth = isHorizontalWritingMode() ? region->contentWidth() : region->contentHeight();
        if (regionLogicalWidth != logicalWidth) {
            LayoutUnit logicalLeft = style()->direction() == LTR ? 0 : logicalWidth - regionLogicalWidth;
            region->setRenderBoxRegionInfo(this, logicalLeft, regionLogicalWidth, false);
        }
    }
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

void RenderFlowThread::paintIntoRegion(PaintInfo& paintInfo, RenderRegion* region, const LayoutPoint& paintOffset)
{
    GraphicsContext* context = paintInfo.context;
    if (!context)
        return;

    // Adjust the clipping rect for the region.
    // paintOffset contains the offset where the painting should occur
    // adjusted with the region padding and border.
    LayoutRect regionRect(region->regionRect());
    LayoutRect regionOverflowRect(region->regionOverflowRect());
    LayoutRect regionClippingRect(paintOffset + (regionOverflowRect.location() - regionRect.location()), regionOverflowRect.size());

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
            renderFlowThreadOffset = LayoutPoint(paintOffset - flippedRegionRect.location());
        } else
            renderFlowThreadOffset = LayoutPoint(paintOffset - regionRect.location());

        context->translate(renderFlowThreadOffset.x(), renderFlowThreadOffset.y());
        info.rect.moveBy(-renderFlowThreadOffset);
        
        layer()->paint(context, info.rect, 0, 0, region, RenderLayer::PaintLayerTemporaryClipRects);

        context->restore();
    }
}

bool RenderFlowThread::hitTestRegion(RenderRegion* region, const HitTestRequest& request, HitTestResult& result, const LayoutPoint& pointInContainer, const LayoutPoint& accumulatedOffset)
{
    LayoutRect regionRect(region->regionRect());
    LayoutRect regionOverflowRect = region->regionOverflowRect();
    LayoutRect regionClippingRect(accumulatedOffset + (regionOverflowRect.location() - regionRect.location()), regionOverflowRect.size());
    if (!regionClippingRect.contains(pointInContainer))
        return false;
    
    LayoutPoint renderFlowThreadOffset;
    if (style()->isFlippedBlocksWritingMode()) {
        LayoutRect flippedRegionRect(regionRect);
        flipForWritingMode(flippedRegionRect);
        renderFlowThreadOffset = LayoutPoint(accumulatedOffset - flippedRegionRect.location());
    } else
        renderFlowThreadOffset = LayoutPoint(accumulatedOffset - regionRect.location());

    LayoutPoint transformedPoint(pointInContainer.x() - renderFlowThreadOffset.x(), pointInContainer.y() - renderFlowThreadOffset.y());
    
    // Always ignore clipping, since the RenderFlowThread has nothing to do with the bounds of the FrameView.
    HitTestRequest newRequest(request.type() & HitTestRequest::IgnoreClipping);

    RenderRegion* oldRegion = result.region();
    result.setRegion(region);
    LayoutPoint oldPoint = result.point();
    result.setPoint(transformedPoint);
    bool isPointInsideFlowThread = layer()->hitTest(newRequest, result);
    result.setPoint(oldPoint);
    result.setRegion(oldRegion);

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
    if (!shouldRepaint(repaintRect) || !hasValidRegionInfo())
        return;

    for (RenderRegionList::iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        if (!region->isValid())
            continue;

        // We only have to issue a repaint in this region if the region rect intersects the repaint rect.
        LayoutRect flippedRegionRect(region->regionRect());
        LayoutRect flippedRegionOverflowRect(region->regionOverflowRect());
        flipForWritingMode(flippedRegionRect); // Put the region rects into physical coordinates.
        flipForWritingMode(flippedRegionOverflowRect);

        LayoutRect clippedRect(flippedRegionOverflowRect);
        clippedRect.intersect(repaintRect);
        if (clippedRect.isEmpty())
            continue;
        
        // Put the region rect into the region's physical coordinate space.
        clippedRect.setLocation(region->contentBoxRect().location() + (repaintRect.location() - flippedRegionRect.location()));
        
        // Now switch to the region's writing mode coordinate space and let it repaint itself.
        region->flipForWritingMode(clippedRect);
        LayoutStateDisabler layoutStateDisabler(view()); // We can't use layout state to repaint, since the region is somewhere else.

        // Can't use currentFlowThread as it possible to have imbricated flow threads and the wrong one could be used,
        // so, we let each region figure out the proper enclosing flow thread
        CurrentRenderFlowThreadDisabler disabler(view());
        region->repaintRectangle(clippedRect, immediate);
    }
}

RenderRegion* RenderFlowThread::renderRegionForLine(LayoutUnit position, bool extendLastRegion) const
{
    ASSERT(!m_regionsInvalidated);

    // If no region matches the position and extendLastRegion is true, it will return
    // the last valid region. It is similar to auto extending the size of the last region. 
    RenderRegion* lastValidRegion = 0;
    
    // FIXME: The regions are always in order, optimize this search.
    bool useHorizontalWritingMode = isHorizontalWritingMode();
    for (RenderRegionList::const_iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        if (!region->isValid())
            continue;

        if (position <= 0)
            return region;

        LayoutRect regionRect = region->regionRect();

        if ((useHorizontalWritingMode && position < regionRect.maxY()) || (!useHorizontalWritingMode && position < regionRect.maxX()))
            return region;

        if (extendLastRegion)
            lastValidRegion = region;
    }

    return lastValidRegion;
}

LayoutUnit RenderFlowThread::regionLogicalTopForLine(LayoutUnit position) const
{
    RenderRegion* region = renderRegionForLine(position);
    if (!region)
        return 0;
    return isHorizontalWritingMode() ? region->regionRect().y() : region->regionRect().x();
}

LayoutUnit RenderFlowThread::regionLogicalWidthForLine(LayoutUnit position) const
{
    RenderRegion* region = renderRegionForLine(position, true);
    if (!region)
        return contentLogicalWidth();
    return isHorizontalWritingMode() ? region->regionRect().width() : region->regionRect().height();
}

LayoutUnit RenderFlowThread::regionLogicalHeightForLine(LayoutUnit position) const
{
    RenderRegion* region = renderRegionForLine(position);
    if (!region)
        return 0;
    return isHorizontalWritingMode() ? region->regionRect().height() : region->regionRect().width();
}

LayoutUnit RenderFlowThread::regionRemainingLogicalHeightForLine(LayoutUnit position, PageBoundaryRule pageBoundaryRule) const
{
    RenderRegion* region = renderRegionForLine(position);
    if (!region)
        return 0;

    LayoutUnit regionLogicalBottom = isHorizontalWritingMode() ? region->regionRect().maxY() : region->regionRect().maxX();
    LayoutUnit remainingHeight = regionLogicalBottom - position;
    if (pageBoundaryRule == IncludePageBoundary) {
        // If IncludePageBoundary is set, the line exactly on the top edge of a
        // region will act as being part of the previous region.
        LayoutUnit regionHeight = isHorizontalWritingMode() ? region->regionRect().height() : region->regionRect().width();
        remainingHeight = layoutMod(remainingHeight, regionHeight);
    }
    return remainingHeight;
}

RenderRegion* RenderFlowThread::mapFromFlowToRegion(TransformState& transformState) const
{
    if (!hasValidRegionInfo())
        return 0;

    LayoutRect boxRect = transformState.mappedQuad().enclosingBoundingBox();
    flipForWritingMode(boxRect);

    // FIXME: We need to refactor RenderObject::absoluteQuads to be able to split the quads across regions,
    // for now we just take the center of the mapped enclosing box and map it to a region.
    // Note: Using the center in order to avoid rounding errors.

    LayoutPoint center = boxRect.center();
    RenderRegion* renderRegion = renderRegionForLine(isHorizontalWritingMode() ? center.y() : center.x(), true);
    if (!renderRegion)
        return 0;

    LayoutRect flippedRegionRect(renderRegion->regionRect());
    flipForWritingMode(flippedRegionRect);

    transformState.move(renderRegion->contentBoxRect().location() - flippedRegionRect.location());

    return renderRegion;
}

void RenderFlowThread::removeRenderBoxRegionInfo(RenderBox* box)
{
    if (!hasRegions())
        return;

    RenderRegion* startRegion;
    RenderRegion* endRegion;
    getRegionRangeForBox(box, startRegion, endRegion);
    
    for (RenderRegionList::iterator iter = m_regionList.find(startRegion); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        if (!region->isValid())
            continue;
        region->removeRenderBoxRegionInfo(box);
        if (region == endRegion)
            break;
    }
    
    m_regionRangeMap.remove(box);
}

bool RenderFlowThread::logicalWidthChangedInRegions(const RenderBlock* block, LayoutUnit offsetFromLogicalTopOfFirstPage)
{
    if (!hasRegions() || block == this) // Not necessary, since if any region changes, we do a full pagination relayout anyway.
        return false;

    RenderRegion* startRegion;
    RenderRegion* endRegion;
    getRegionRangeForBox(block, startRegion, endRegion);

    for (RenderRegionList::iterator iter = m_regionList.find(startRegion); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        
        if (!region->isValid())
            continue;

        ASSERT(!region->needsLayout());

        OwnPtr<RenderBoxRegionInfo> oldInfo = region->takeRenderBoxRegionInfo(block);
        if (!oldInfo)
            continue;

        LayoutUnit oldLogicalWidth = oldInfo->logicalWidth();
        RenderBoxRegionInfo* newInfo = block->renderBoxRegionInfo(region, offsetFromLogicalTopOfFirstPage);
        if (!newInfo || newInfo->logicalWidth() != oldLogicalWidth)
            return true;

        if (region == endRegion)
            break;
    }

    return false;
}

LayoutUnit RenderFlowThread::contentLogicalWidthOfFirstRegion() const
{
    if (!hasValidRegionInfo())
        return 0;
    for (RenderRegionList::const_iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        if (!region->isValid())
            continue;
        return isHorizontalWritingMode() ? region->contentWidth() : region->contentHeight();
    }
    ASSERT_NOT_REACHED();
    return 0;
}

LayoutUnit RenderFlowThread::contentLogicalHeightOfFirstRegion() const
{
    if (!hasValidRegionInfo())
        return 0;
    for (RenderRegionList::const_iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        if (!region->isValid())
            continue;
        return isHorizontalWritingMode() ? region->contentHeight() : region->contentWidth();
    }
    ASSERT_NOT_REACHED();
    return 0;
}
 
LayoutUnit RenderFlowThread::contentLogicalLeftOfFirstRegion() const
{
    if (!hasValidRegionInfo())
        return 0;
    for (RenderRegionList::const_iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        if (!region->isValid())
            continue;
        return isHorizontalWritingMode() ? region->regionRect().x() : region->regionRect().y();
    }
    ASSERT_NOT_REACHED();
    return 0;
}

RenderRegion* RenderFlowThread::firstRegion() const
{
    if (!hasValidRegionInfo())
        return 0;
    for (RenderRegionList::const_iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        if (!region->isValid())
            continue;
        return region;
    }
    return 0;
}

RenderRegion* RenderFlowThread::lastRegion() const
{
    if (!hasValidRegionInfo())
        return 0;
    for (RenderRegionList::const_reverse_iterator iter = m_regionList.rbegin(); iter != m_regionList.rend(); ++iter) {
        RenderRegion* region = *iter;
        if (!region->isValid())
            continue;
        return region;
    }
    return 0;
}

void RenderFlowThread::clearRenderBoxCustomStyle(const RenderBox* box,
    const RenderRegion* oldStartRegion, const RenderRegion* oldEndRegion,
    const RenderRegion* newStartRegion, const RenderRegion* newEndRegion)
{
    // Clear the styles for the object in the regions.
    // The styles are not cleared for the regions that are contained in both ranges.
    bool insideOldRegionRange = false;
    bool insideNewRegionRange = false;
    for (RenderRegionList::iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;

        if (oldStartRegion == region)
            insideOldRegionRange = true;
        if (newStartRegion == region)
            insideNewRegionRange = true;

        if (!(insideOldRegionRange && insideNewRegionRange))
            region->clearBoxStyleInRegion(box);

        if (oldEndRegion == region)
            insideOldRegionRange = false;
        if (newEndRegion == region)
            insideNewRegionRange = false;
    }
}

void RenderFlowThread::setRegionRangeForBox(const RenderBox* box, LayoutUnit offsetFromLogicalTopOfFirstPage)
{
    // FIXME: Not right for differing writing-modes.
    RenderRegion* startRegion = renderRegionForLine(offsetFromLogicalTopOfFirstPage, true);
    RenderRegion* endRegion = renderRegionForLine(offsetFromLogicalTopOfFirstPage + box->logicalHeight(), true);
    RenderRegionRangeMap::iterator it = m_regionRangeMap.find(box);
    if (it == m_regionRangeMap.end()) {
        m_regionRangeMap.set(box, RenderRegionRange(startRegion, endRegion));
        return;
    }

    // If nothing changed, just bail.
    RenderRegionRange& range = it->second;
    if (range.startRegion() == startRegion && range.endRegion() == endRegion)
        return;

    // Delete any info that we find before our new startRegion and after our new endRegion.
    for (RenderRegionList::iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        if (region == startRegion) {
            iter = m_regionList.find(endRegion);
            continue;
        }

        region->removeRenderBoxRegionInfo(box);

        if (region == range.endRegion())
            break;
    }

    clearRenderBoxCustomStyle(box, range.startRegion(), range.endRegion(), startRegion, endRegion);
    range.setRange(startRegion, endRegion);
}

void RenderFlowThread::getRegionRangeForBox(const RenderBox* box, RenderRegion*& startRegion, RenderRegion*& endRegion) const
{
    startRegion = 0;
    endRegion = 0;
    RenderRegionRangeMap::const_iterator it = m_regionRangeMap.find(box);
    if (it == m_regionRangeMap.end())
        return;

    const RenderRegionRange& range = it->second;
    startRegion = range.startRegion();
    endRegion = range.endRegion();
    ASSERT(m_regionList.contains(startRegion) && m_regionList.contains(endRegion));
}

WebKitNamedFlow* RenderFlowThread::ensureNamedFlow()
{
    if (!m_namedFlow)
        m_namedFlow = WebKitNamedFlow::create();

    return m_namedFlow.get();
}

} // namespace WebCore
