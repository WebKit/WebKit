/*
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
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

#include "FlowThreadController.h"
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

RenderFlowThread::RenderFlowThread(Node* node)
    : RenderBlock(node)
    , m_hasValidRegions(false)
    , m_regionsInvalidated(false)
    , m_regionsHaveUniformLogicalWidth(true)
    , m_regionsHaveUniformLogicalHeight(true)
    , m_overset(true)
{
    ASSERT(node->document()->cssRegionsEnabled());
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

void RenderFlowThread::removeFlowChildInfo(RenderObject* child)
{
    if (child->isBox())
        removeRenderBoxRegionInfo(toRenderBox(child));
    clearRenderObjectCustomStyle(child);
}

void RenderFlowThread::addRegionToThread(RenderRegion* renderRegion)
{
    ASSERT(renderRegion);
    m_regionList.add(renderRegion);
    renderRegion->setIsValid(true);
    invalidateRegions();
    checkRegionsWithStyling();
}

void RenderFlowThread::removeRegionFromThread(RenderRegion* renderRegion)
{
    ASSERT(renderRegion);
    m_regionRangeMap.clear();
    m_regionList.remove(renderRegion);
    invalidateRegions();
    checkRegionsWithStyling();
}

class CurrentRenderFlowThreadMaintainer {
    WTF_MAKE_NONCOPYABLE(CurrentRenderFlowThreadMaintainer);
public:
    CurrentRenderFlowThreadMaintainer(RenderFlowThread* renderFlowThread)
        : m_renderFlowThread(renderFlowThread)
    {
        RenderView* view = m_renderFlowThread->view();
        ASSERT(!view->flowThreadController()->currentRenderFlowThread());
        view->flowThreadController()->setCurrentRenderFlowThread(m_renderFlowThread);
    }
    ~CurrentRenderFlowThreadMaintainer()
    {
        RenderView* view = m_renderFlowThread->view();
        ASSERT(view->flowThreadController()->currentRenderFlowThread() == m_renderFlowThread);
        view->flowThreadController()->setCurrentRenderFlowThread(0);
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
        m_renderFlowThread = m_view->flowThreadController()->currentRenderFlowThread();
        if (m_renderFlowThread)
            view->flowThreadController()->setCurrentRenderFlowThread(0);
    }
    ~CurrentRenderFlowThreadDisabler()
    {
        if (m_renderFlowThread)
            m_view->flowThreadController()->setCurrentRenderFlowThread(m_renderFlowThread);
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

                LayoutUnit regionLogicalWidth = region->logicalWidthForFlowThreadContent();
                LayoutUnit regionLogicalHeight = region->logicalHeightForFlowThreadContent();

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
                    
                LayoutUnit regionLogicalWidth = region->logicalWidthForFlowThreadContent();
                LayoutUnit regionLogicalHeight = region->logicalHeightForFlowThreadContent();
    
                LayoutRect regionRect(style()->direction() == LTR ? ZERO_LAYOUT_UNIT : logicalWidth() - regionLogicalWidth, logicalHeight, regionLogicalWidth, regionLogicalHeight);
                region->setRegionRect(isHorizontalWritingMode() ? regionRect : regionRect.transposedRect());
                logicalHeight += regionLogicalHeight;
            }
        }
    }

    CurrentRenderFlowThreadMaintainer currentFlowThreadSetter(this);
    LayoutStateMaintainer statePusher(view(), this, regionsChanged);
    RenderBlock::layout();
    statePusher.pop();
    
    if (shouldDispatchRegionLayoutUpdateEvent())
        dispatchRegionLayoutUpdateEvent();
}

void RenderFlowThread::computeLogicalWidth()
{
    LayoutUnit logicalWidth = 0;
    for (RenderRegionList::iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        if (!region->isValid())
            continue;
        ASSERT(!region->needsLayout());
        logicalWidth = max(region->logicalWidthForFlowThreadContent(), logicalWidth);
    }
    setLogicalWidth(logicalWidth);

    // If the regions have non-uniform logical widths, then insert inset information for the RenderFlowThread.
    for (RenderRegionList::iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        if (!region->isValid())
            continue;
        
        LayoutUnit regionLogicalWidth = region->logicalWidthForFlowThreadContent();
        if (regionLogicalWidth != logicalWidth) {
            LayoutUnit logicalLeft = style()->direction() == LTR ? ZERO_LAYOUT_UNIT : logicalWidth - regionLogicalWidth;
            region->setRenderBoxRegionInfo(this, logicalLeft, regionLogicalWidth, false);
        }
    }
}

void RenderFlowThread::computeLogicalHeight()
{
    LayoutUnit logicalHeight = 0;

    for (RenderRegionList::iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        if (!region->isValid())
            continue;
        ASSERT(!region->needsLayout());
        logicalHeight += region->logicalHeightForFlowThreadContent();
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
    LayoutRect regionOversetRect(region->regionOversetRect());
    LayoutRect regionClippingRect(paintOffset + (regionOversetRect.location() - regionRect.location()), regionOversetRect.size());

    PaintInfo info(paintInfo);
    info.rect.intersect(pixelSnappedIntRect(regionClippingRect));

    if (!info.rect.isEmpty()) {
        context->save();

        context->clip(regionClippingRect);

        // RenderFlowThread should start painting its content in a position that is offset
        // from the region rect's current position. The amount of offset is equal to the location of
        // region in flow coordinates.
        IntPoint renderFlowThreadOffset;
        if (style()->isFlippedBlocksWritingMode()) {
            LayoutRect flippedRegionRect(regionRect);
            flipForWritingMode(flippedRegionRect);
            renderFlowThreadOffset = roundedIntPoint(paintOffset - flippedRegionRect.location());
        } else
            renderFlowThreadOffset = roundedIntPoint(paintOffset - regionRect.location());

        context->translate(renderFlowThreadOffset.x(), renderFlowThreadOffset.y());
        info.rect.moveBy(-renderFlowThreadOffset);
        
        layer()->paint(context, info.rect, 0, 0, region, RenderLayer::PaintLayerTemporaryClipRects);

        context->restore();
    }
}

bool RenderFlowThread::hitTestRegion(RenderRegion* region, const HitTestRequest& request, HitTestResult& result, const HitTestPoint& pointInContainer, const LayoutPoint& accumulatedOffset)
{
    LayoutRect regionRect(region->regionRect());
    LayoutRect regionOversetRect = region->regionOversetRect();
    LayoutRect regionClippingRect(accumulatedOffset + (regionOversetRect.location() - regionRect.location()), regionOversetRect.size());
    if (!regionClippingRect.contains(pointInContainer.point()))
        return false;

    LayoutSize renderFlowThreadOffset;
    if (style()->isFlippedBlocksWritingMode()) {
        LayoutRect flippedRegionRect(regionRect);
        flipForWritingMode(flippedRegionRect);
        renderFlowThreadOffset = accumulatedOffset - flippedRegionRect.location();
    } else
        renderFlowThreadOffset = accumulatedOffset - regionRect.location();

    // Always ignore clipping, since the RenderFlowThread has nothing to do with the bounds of the FrameView.
    HitTestRequest newRequest(request.type() | HitTestRequest::IgnoreClipping);

    // Make a new temporary hitTestPoint in the new region.
    HitTestPoint newHitTestPoint(pointInContainer, -renderFlowThreadOffset, region);

    bool isPointInsideFlowThread = layer()->hitTest(newRequest, newHitTestPoint, result);

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
        LayoutRect flippedRegionOversetRect(region->regionOversetRect());
        flipForWritingMode(flippedRegionRect); // Put the region rects into physical coordinates.
        flipForWritingMode(flippedRegionOversetRect);

        LayoutRect clippedRect(repaintRect);
        clippedRect.intersect(flippedRegionOversetRect);
        if (clippedRect.isEmpty())
            continue;

        // Put the region rect into the region's physical coordinate space.
        clippedRect.setLocation(region->contentBoxRect().location() + (clippedRect.location() - flippedRegionRect.location()));

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
        remainingHeight = intMod(remainingHeight, regionHeight);
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

#ifndef NDEBUG
    // We have to make sure we did not leave any RenderBoxRegionInfo attached.
    for (RenderRegionList::iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        if (!region->isValid())
            continue;
        ASSERT(!region->renderBoxRegionInfo(box));
    }
#endif

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

void RenderFlowThread::clearRenderObjectCustomStyle(const RenderObject* object,
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
            region->clearObjectStyleInRegion(object);

        if (oldEndRegion == region)
            insideOldRegionRange = false;
        if (newEndRegion == region)
            insideNewRegionRange = false;
    }
}

void RenderFlowThread::setRegionRangeForBox(const RenderBox* box, LayoutUnit offsetFromLogicalTopOfFirstPage)
{
    if (!hasRegions())
        return;

    // FIXME: Not right for differing writing-modes.
    RenderRegion* startRegion = renderRegionForLine(offsetFromLogicalTopOfFirstPage, true);
    RenderRegion* endRegion = renderRegionForLine(offsetFromLogicalTopOfFirstPage + box->logicalHeight(), true);
    RenderRegionRangeMap::iterator it = m_regionRangeMap.find(box);
    if (it == m_regionRangeMap.end()) {
        m_regionRangeMap.set(box, RenderRegionRange(startRegion, endRegion));
        clearRenderObjectCustomStyle(box);
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

    clearRenderObjectCustomStyle(box, range.startRegion(), range.endRegion(), startRegion, endRegion);
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

void RenderFlowThread::computeOverflowStateForRegions(LayoutUnit oldClientAfterEdge)
{
    LayoutUnit height = oldClientAfterEdge;
    // FIXME: the visual overflow of middle region (if it is the last one to contain any content in a render flow thread)
    // might not be taken into account because the render flow thread height is greater that that regions height + its visual overflow
    // because of how computeLogicalHeight is implemented for RenderFlowThread (as a sum of all regions height).
    // This means that the middle region will be marked as fit (even if it has visual overflow flowing into the next region)
    if (hasRenderOverflow() && ( (isHorizontalWritingMode() && visualOverflowRect().maxY() > clientBoxRect().maxY()) 
                                || (!isHorizontalWritingMode() && visualOverflowRect().maxX() > clientBoxRect().maxX())))
        height = isHorizontalWritingMode() ? visualOverflowRect().maxY() : visualOverflowRect().maxX();

    RenderRegion* lastReg = lastRegion();
    for (RenderRegionList::iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        if (!region->isValid()) {
            region->setRegionState(RenderRegion::RegionUndefined);
            continue;
        }
        LayoutUnit flowMin = height - (isHorizontalWritingMode() ? region->regionRect().y() : region->regionRect().x());
        LayoutUnit flowMax = height - (isHorizontalWritingMode() ? region->regionRect().maxY() : region->regionRect().maxX());
        RenderRegion::RegionState previousState = region->regionState();
        RenderRegion::RegionState state = RenderRegion::RegionFit;
        if (flowMin <= 0)
            state = RenderRegion::RegionEmpty;
        if (flowMax > 0 && region == lastReg)
            state = RenderRegion::RegionOverset;
        region->setRegionState(state);
        // determine whether the NamedFlow object should dispatch a regionLayoutUpdate event
        // FIXME: currently it cannot determine whether a region whose regionOverset state remained either "fit" or "overset" has actually
        // changed, so it just assumes that the NamedFlow should dispatch the event
        if (previousState != state
            || state == RenderRegion::RegionFit
            || state == RenderRegion::RegionOverset)
            setDispatchRegionLayoutUpdateEvent(true);
    }

    // With the regions overflow state computed we can also set the overset flag for the named flow.
    // If there are no valid regions in the chain, overset is true
    m_overset = lastReg ? lastReg->regionState() == RenderRegion::RegionOverset : true;
}

bool RenderFlowThread::regionInRange(const RenderRegion* targetRegion, const RenderRegion* startRegion, const RenderRegion* endRegion) const
{
    ASSERT(targetRegion);

    for (RenderRegionList::const_iterator it = m_regionList.find(const_cast<RenderRegion*>(startRegion)); it != m_regionList.end(); ++it) {
        const RenderRegion* currRegion = *it;
        if (!currRegion->isValid())
            continue;
        if (targetRegion == currRegion)
            return true;
        if (currRegion == endRegion)
            break;
    }

    return false;
}

// Check if the content is flown into at least a region with region styling rules.
void RenderFlowThread::checkRegionsWithStyling()
{
    bool hasRegionsWithStyling = false;
    for (RenderRegionList::iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        if (!region->isValid())
            continue;
        if (region->hasCustomRegionStyle()) {
            hasRegionsWithStyling = true;
            break;
        }
    }
    m_hasRegionsWithStyling = hasRegionsWithStyling;
}

bool RenderFlowThread::objectInFlowRegion(const RenderObject* object, const RenderRegion* region) const
{
    ASSERT(object);
    ASSERT(region);

    if (!object->inRenderFlowThread())
        return false;
    if (object->enclosingRenderFlowThread() != this)
        return false;
    if (!m_regionList.contains(const_cast<RenderRegion*>(region)))
        return false;

    RenderBox* enclosingBox = object->enclosingBox();
    RenderRegion* enclosingBoxStartRegion = 0;
    RenderRegion* enclosingBoxEndRegion = 0;
    getRegionRangeForBox(enclosingBox, enclosingBoxStartRegion, enclosingBoxEndRegion);
    if (!regionInRange(region, enclosingBoxStartRegion, enclosingBoxEndRegion))
       return false;

    if (object->isBox())
        return true;

    LayoutRect objectABBRect = object->absoluteBoundingBoxRect(true);
    if (!objectABBRect.width())
        objectABBRect.setWidth(1);
    if (!objectABBRect.height())
        objectABBRect.setHeight(1); 
    if (objectABBRect.intersects(region->absoluteBoundingBoxRect(true)))
        return true;

    if (region == lastRegion()) {
        // If the object does not intersect any of the enclosing box regions
        // then the object is in last region.
        for (RenderRegionList::const_iterator it = m_regionList.find(enclosingBoxStartRegion); it != m_regionList.end(); ++it) {
            const RenderRegion* currRegion = *it;
            if (!region->isValid())
                continue;
            if (currRegion == region)
                break;
            if (objectABBRect.intersects(currRegion->absoluteBoundingBoxRect(true)))
                return false;
        }
        return true;
    }

    return false;
}

} // namespace WebCore
