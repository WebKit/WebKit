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

// FIXME: RenderFlowThread should either be anonymous or be associated with some Element.
// It should not be constructed as a renderer for the Document.
RenderFlowThread::RenderFlowThread(Document* document)
    : RenderBlock(document)
    , m_autoLogicalHeightRegionsCount(0)
    , m_regionsInvalidated(false)
    , m_regionsHaveUniformLogicalWidth(true)
    , m_regionsHaveUniformLogicalHeight(true)
    , m_overset(true)
    , m_hasRegionsWithStyling(false)
    , m_dispatchRegionLayoutUpdateEvent(false)
    , m_pageLogicalSizeChanged(false)
{
    ASSERT(document->cssRegionsEnabled());
    setFlowThreadState(InsideOutOfFlowThread);
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
}

void RenderFlowThread::removeRegionFromThread(RenderRegion* renderRegion)
{
    ASSERT(renderRegion);
    m_regionList.remove(renderRegion);
}

void RenderFlowThread::invalidateRegions()
{
    if (m_regionsInvalidated) {
        ASSERT(selfNeedsLayout());
        return;
    }

    m_regionRangeMap.clear();
    m_breakBeforeToRegionMap.clear();
    m_breakAfterToRegionMap.clear();
    setNeedsLayout(true);

    m_regionsInvalidated = true;
}

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

void RenderFlowThread::validateRegions()
{
    if (m_regionsInvalidated) {
        m_regionsInvalidated = false;
        m_regionsHaveUniformLogicalWidth = true;
        m_regionsHaveUniformLogicalHeight = true;

        if (hasRegions()) {
            LayoutUnit previousRegionLogicalWidth = 0;
            LayoutUnit previousRegionLogicalHeight = 0;
            bool firstRegionVisited = false;
            
            for (RenderRegionList::iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
                RenderRegion* region = *iter;
                ASSERT(!region->needsLayout() || region->isRenderRegionSet());

                region->deleteAllRenderBoxRegionInfo();

                // In the normal layout phase we need to initialize the overrideLogicalContentHeight for auto-height regions.
                // See initializeRegionsOverrideLogicalContentHeight for the explanation.
                // Also, if we have auto-height regions we can't assume m_regionsHaveUniformLogicalHeight to be true in the first phase
                // because the auto-height regions don't have their height computed yet.
                if (view()->normalLayoutPhase() && region->hasAutoLogicalHeight()) {
                    region->setOverrideLogicalContentHeight(region->maxPageLogicalHeight());
                    m_regionsHaveUniformLogicalHeight = false;
                }

                LayoutUnit regionLogicalWidth = region->pageLogicalWidth();
                LayoutUnit regionLogicalHeight = region->pageLogicalHeight();

                if (!firstRegionVisited)
                    firstRegionVisited = true;
                else {
                    if (m_regionsHaveUniformLogicalWidth && previousRegionLogicalWidth != regionLogicalWidth)
                        m_regionsHaveUniformLogicalWidth = false;
                    if (m_regionsHaveUniformLogicalHeight && previousRegionLogicalHeight != regionLogicalHeight)
                        m_regionsHaveUniformLogicalHeight = false;
                }

                previousRegionLogicalWidth = regionLogicalWidth;
            }
        }
    }

    updateLogicalWidth(); // Called to get the maximum logical width for the region.
    updateRegionsFlowThreadPortionRect();
}

void RenderFlowThread::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;

    m_pageLogicalSizeChanged = m_regionsInvalidated && everHadLayout();
    validateRegions();

    CurrentRenderFlowThreadMaintainer currentFlowThreadSetter(this);
    RenderBlock::layout();

    m_pageLogicalSizeChanged = false;

    if (lastRegion())
        lastRegion()->expandToEncompassFlowThreadContentsIfNeeded();

    if (shouldDispatchRegionLayoutUpdateEvent())
        dispatchRegionLayoutUpdateEvent();
}

void RenderFlowThread::updateLogicalWidth()
{
    LayoutUnit logicalWidth = initialLogicalWidth();
    for (RenderRegionList::iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        ASSERT(!region->needsLayout() || region->isRenderRegionSet());
        logicalWidth = max(region->pageLogicalWidth(), logicalWidth);
    }
    setLogicalWidth(logicalWidth);

    // If the regions have non-uniform logical widths, then insert inset information for the RenderFlowThread.
    for (RenderRegionList::iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        LayoutUnit regionLogicalWidth = region->pageLogicalWidth();
        if (regionLogicalWidth != logicalWidth) {
            LayoutUnit logicalLeft = style()->direction() == LTR ? LayoutUnit() : logicalWidth - regionLogicalWidth;
            region->setRenderBoxRegionInfo(this, logicalLeft, regionLogicalWidth, false);
        }
    }
}

void RenderFlowThread::computeLogicalHeight(LayoutUnit, LayoutUnit logicalTop, LogicalExtentComputedValues& computedValues) const
{
    computedValues.m_position = logicalTop;
    computedValues.m_extent = 0;

    for (RenderRegionList::const_iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        ASSERT(!region->needsLayout() || region->isRenderRegionSet());

        computedValues.m_extent += region->logicalHeightOfAllFlowThreadContent();
    }
}

LayoutRect RenderFlowThread::computeRegionClippingRect(const LayoutPoint& offset, const LayoutRect& flowThreadPortionRect, const LayoutRect& flowThreadPortionOverflowRect) const
{
    LayoutRect regionClippingRect(offset + (flowThreadPortionOverflowRect.location() - flowThreadPortionRect.location()), flowThreadPortionOverflowRect.size());
    if (style()->isFlippedBlocksWritingMode())
        regionClippingRect.move(flowThreadPortionRect.size() - flowThreadPortionOverflowRect.size());
    return regionClippingRect;
}

void RenderFlowThread::paintFlowThreadPortionInRegion(PaintInfo& paintInfo, RenderRegion* region, LayoutRect flowThreadPortionRect, LayoutRect flowThreadPortionOverflowRect, const LayoutPoint& paintOffset) const
{
    GraphicsContext* context = paintInfo.context;
    if (!context)
        return;

    // RenderFlowThread should start painting its content in a position that is offset
    // from the region rect's current position. The amount of offset is equal to the location of
    // the flow thread portion in the flow thread's local coordinates.
    // Note that we have to pixel snap the location at which we're going to paint, since this is necessary
    // to minimize the amount of incorrect snapping that would otherwise occur.
    // If we tried to paint by applying a non-integral translation, then all the
    // layout code that attempted to pixel snap would be incorrect.
    IntPoint adjustedPaintOffset;
    LayoutPoint portionLocation;
    if (style()->isFlippedBlocksWritingMode()) {
        LayoutRect flippedFlowThreadPortionRect(flowThreadPortionRect);
        flipForWritingMode(flippedFlowThreadPortionRect);
        portionLocation = flippedFlowThreadPortionRect.location();
    } else
        portionLocation = flowThreadPortionRect.location();
    adjustedPaintOffset = roundedIntPoint(paintOffset - portionLocation);

    // The clipping rect for the region is set up by assuming the flowThreadPortionRect is going to paint offset from adjustedPaintOffset.
    // Remember that we pixel snapped and moved the paintOffset and stored the snapped result in adjustedPaintOffset. Now we add back in
    // the flowThreadPortionRect's location to get the spot where we expect the portion to actually paint. This can be non-integral and
    // that's ok. We then pixel snap the resulting clipping rect to account for snapping that will occur when the flow thread paints.
    IntRect regionClippingRect = pixelSnappedIntRect(computeRegionClippingRect(adjustedPaintOffset + portionLocation, flowThreadPortionRect, flowThreadPortionOverflowRect));

    PaintInfo info(paintInfo);
    info.rect.intersect(regionClippingRect);

    if (!info.rect.isEmpty()) {
        context->save();

        context->clip(regionClippingRect);

        context->translate(adjustedPaintOffset.x(), adjustedPaintOffset.y());
        info.rect.moveBy(-adjustedPaintOffset);
        
        layer()->paint(context, info.rect, 0, 0, region, RenderLayer::PaintLayerTemporaryClipRects);

        context->restore();
    }
}

bool RenderFlowThread::hitTestFlowThreadPortionInRegion(RenderRegion* region, LayoutRect flowThreadPortionRect, LayoutRect flowThreadPortionOverflowRect, const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset) const
{
    LayoutRect regionClippingRect = computeRegionClippingRect(accumulatedOffset, flowThreadPortionRect, flowThreadPortionOverflowRect);
    if (!regionClippingRect.contains(locationInContainer.point()))
        return false;

    LayoutSize renderFlowThreadOffset;
    if (style()->isFlippedBlocksWritingMode()) {
        LayoutRect flippedFlowThreadPortionRect(flowThreadPortionRect);
        flipForWritingMode(flippedFlowThreadPortionRect);
        renderFlowThreadOffset = accumulatedOffset - flippedFlowThreadPortionRect.location();
    } else
        renderFlowThreadOffset = accumulatedOffset - flowThreadPortionRect.location();

    // Always ignore clipping, since the RenderFlowThread has nothing to do with the bounds of the FrameView.
    HitTestRequest newRequest(request.type() | HitTestRequest::IgnoreClipping | HitTestRequest::DisallowShadowContent);
    HitTestResult tempResult(result);

    // Make a new temporary HitTestLocation in the new region.
    HitTestLocation newHitTestLocation(locationInContainer, -renderFlowThreadOffset, region);

    bool isPointInsideFlowThread = layer()->hitTest(newRequest, newHitTestLocation, tempResult);

    // We want to make sure we hit a node from the content inside the flow thread.
    isPointInsideFlowThread = isPointInsideFlowThread && !tempResult.innerNode()->isDocumentNode();
    if (isPointInsideFlowThread)
        result = tempResult;

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

void RenderFlowThread::repaintRectangleInRegions(const LayoutRect& repaintRect, bool immediate) const
{
    if (!shouldRepaint(repaintRect) || !hasValidRegionInfo())
        return;

    LayoutStateDisabler layoutStateDisabler(view()); // We can't use layout state to repaint, since the regions are somewhere else.

    // We can't use currentFlowThread as it is possible to have interleaved flow threads and the wrong one could be used.
    // Let each region figure out the proper enclosing flow thread.
    CurrentRenderFlowThreadDisabler disabler(view());
    
    for (RenderRegionList::const_iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;

        region->repaintFlowThreadContent(repaintRect, immediate);
    }
}

RenderRegion* RenderFlowThread::regionAtBlockOffset(LayoutUnit offset, bool extendLastRegion, RegionAutoGenerationPolicy autoGenerationPolicy)
{
    ASSERT(!m_regionsInvalidated);

    // If no region matches the position and extendLastRegion is true, it will return
    // the last valid region. It is similar to auto extending the size of the last region. 
    RenderRegion* lastValidRegion = 0;

    LayoutUnit accumulatedLogicalHeight = 0;

    // If our flow thread autogenerates its regions, then make sure we ensure those regions exist
    // now.
    if (autoGenerationPolicy == AllowRegionAutoGeneration)
        autoGenerateRegionsToBlockOffset(offset);

    // FIXME: The regions are always in order, optimize this search.
    for (RenderRegionList::const_iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;

        if (offset <= 0)
            return region;

        if (extendLastRegion || region->isRenderRegionSet())
            lastValidRegion = region;

        if (region->hasOverrideHeight() && view()->normalLayoutPhase()) {
            accumulatedLogicalHeight += region->overrideLogicalContentHeight();
            if (offset < accumulatedLogicalHeight)
                return region;
            continue;
        }

        LayoutRect regionRect = region->flowThreadPortionRect();
        accumulatedLogicalHeight += isHorizontalWritingMode() ? regionRect.height() : regionRect.width();
        if (offset < accumulatedLogicalHeight)
            return region;
    }

    return lastValidRegion;
}

LayoutUnit RenderFlowThread::pageLogicalTopForOffset(LayoutUnit offset)
{
    RenderRegion* region = regionAtBlockOffset(offset);
    return region ? region->pageLogicalTopForOffset(offset) : LayoutUnit();
}

LayoutUnit RenderFlowThread::pageLogicalWidthForOffset(LayoutUnit offset)
{
    RenderRegion* region = regionAtBlockOffset(offset, true);
    return region ? region->pageLogicalWidth() : contentLogicalWidth();
}

LayoutUnit RenderFlowThread::pageLogicalHeightForOffset(LayoutUnit offset)
{
    RenderRegion* region = regionAtBlockOffset(offset);
    if (!region)
        return 0;

    return region->pageLogicalHeight();
}

LayoutUnit RenderFlowThread::pageRemainingLogicalHeightForOffset(LayoutUnit offset, PageBoundaryRule pageBoundaryRule)
{
    RenderRegion* region = regionAtBlockOffset(offset);
    if (!region)
        return 0;

    LayoutUnit pageLogicalTop = region->pageLogicalTopForOffset(offset);
    LayoutUnit pageLogicalHeight = region->pageLogicalHeight();
    LayoutUnit pageLogicalBottom = pageLogicalTop + pageLogicalHeight;
    LayoutUnit remainingHeight = pageLogicalBottom - offset;
    if (pageBoundaryRule == IncludePageBoundary) {
        // If IncludePageBoundary is set, the line exactly on the top edge of a
        // region will act as being part of the previous region.
        remainingHeight = intMod(remainingHeight, pageLogicalHeight);
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
    RenderRegion* renderRegion = const_cast<RenderFlowThread*>(this)->regionAtBlockOffset(isHorizontalWritingMode() ? center.y() : center.x(), true, DisallowRegionAutoGeneration);
    if (!renderRegion)
        return 0;

    LayoutRect flippedRegionRect(renderRegion->flowThreadPortionRect());
    flipForWritingMode(flippedRegionRect);

    transformState.move(renderRegion->contentBoxRect().location() - flippedRegionRect.location());

    return renderRegion;
}

void RenderFlowThread::removeRenderBoxRegionInfo(RenderBox* box)
{
    if (!hasRegions())
        return;

    // If the region chain was invalidated the next layout will clear the box information from all the regions.
    if (m_regionsInvalidated) {
        ASSERT(selfNeedsLayout());
        return;
    }

    RenderRegion* startRegion;
    RenderRegion* endRegion;
    getRegionRangeForBox(box, startRegion, endRegion);

    for (RenderRegionList::iterator iter = m_regionList.find(startRegion); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        region->removeRenderBoxRegionInfo(box);
        if (region == endRegion)
            break;
    }

#ifndef NDEBUG
    // We have to make sure we did not leave any RenderBoxRegionInfo attached.
    for (RenderRegionList::iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        ASSERT(!region->renderBoxRegionInfo(box));
    }
#endif

    m_regionRangeMap.remove(box);
}

bool RenderFlowThread::logicalWidthChangedInRegions(const RenderBlock* block, LayoutUnit offsetFromLogicalTopOfFirstPage)
{
    if (!hasRegions())
        return false;

    RenderRegion* startRegion;
    RenderRegion* endRegion;
    getRegionRangeForBox(block, startRegion, endRegion);

    // When the region chain is invalidated the box information is discarded so we must assume the width has changed.
    if (m_pageLogicalSizeChanged && !startRegion)
        return true;

    // Not necessary for the flow thread, since we already computed the correct info for it.
    if (block == this)
        return false;

    for (RenderRegionList::iterator iter = m_regionList.find(startRegion); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        ASSERT(!region->needsLayout() || region->isRenderRegionSet());

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
    RenderRegion* firstValidRegionInFlow = firstRegion();
    if (!firstValidRegionInFlow)
        return 0;
    return isHorizontalWritingMode() ? firstValidRegionInFlow->contentWidth() : firstValidRegionInFlow->contentHeight();
}

LayoutUnit RenderFlowThread::contentLogicalHeightOfFirstRegion() const
{
    RenderRegion* firstValidRegionInFlow = firstRegion();
    if (!firstValidRegionInFlow)
        return 0;
    return isHorizontalWritingMode() ? firstValidRegionInFlow->contentHeight() : firstValidRegionInFlow->contentWidth();
}

LayoutUnit RenderFlowThread::contentLogicalLeftOfFirstRegion() const
{
    RenderRegion* firstValidRegionInFlow = firstRegion();
    if (!firstValidRegionInFlow)
        return 0;
    return isHorizontalWritingMode() ? firstValidRegionInFlow->flowThreadPortionRect().x() : firstValidRegionInFlow->flowThreadPortionRect().y();
}

RenderRegion* RenderFlowThread::firstRegion() const
{
    if (!hasValidRegionInfo())
        return 0;
    return m_regionList.first();
}

RenderRegion* RenderFlowThread::lastRegion() const
{
    if (!hasValidRegionInfo())
        return 0;
    return m_regionList.last();
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
    RenderRegion* startRegion = regionAtBlockOffset(offsetFromLogicalTopOfFirstPage, true);
    RenderRegion* endRegion = regionAtBlockOffset(offsetFromLogicalTopOfFirstPage + box->logicalHeight(), true);
    RenderRegionRangeMap::iterator it = m_regionRangeMap.find(box);
    if (it == m_regionRangeMap.end()) {
        m_regionRangeMap.set(box, RenderRegionRange(startRegion, endRegion));
        clearRenderObjectCustomStyle(box);
        return;
    }

    // If nothing changed, just bail.
    RenderRegionRange& range = it->value;
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

    const RenderRegionRange& range = it->value;
    startRegion = range.startRegion();
    endRegion = range.endRegion();
    ASSERT(m_regionList.contains(startRegion) && m_regionList.contains(endRegion));
}

void RenderFlowThread::computeOverflowStateForRegions(LayoutUnit oldClientAfterEdge)
{
    LayoutUnit height = oldClientAfterEdge;

    LayoutUnit offsetBreakAdjustment = 0;
    // Simulate a region break at height. If it points inside an auto logical height region,
    // then it may determine the region override logical content height.
    addForcedRegionBreak(height, this, false, &offsetBreakAdjustment);

    // During the normal layout phase of the flow thread all the auto-height regions have the overrideLogicalContentHeight set to max height.
    // We need to clear the overrideLogicalContentHeight for all the regions that didn't receive any content, starting with firstEmptyRegion.
    RenderRegion* firstEmptyRegion = 0;
    if (view()->normalLayoutPhase())
        firstEmptyRegion = regionAtBlockOffset(height + offsetBreakAdjustment);

    // FIXME: the visual overflow of middle region (if it is the last one to contain any content in a render flow thread)
    // might not be taken into account because the render flow thread height is greater that that regions height + its visual overflow
    // because of how computeLogicalHeight is implemented for RenderFlowThread (as a sum of all regions height).
    // This means that the middle region will be marked as fit (even if it has visual overflow flowing into the next region)
    if (hasRenderOverflow()
        && ( (isHorizontalWritingMode() && visualOverflowRect().maxY() > clientBoxRect().maxY())
            || (!isHorizontalWritingMode() && visualOverflowRect().maxX() > clientBoxRect().maxX())))
        height = isHorizontalWritingMode() ? visualOverflowRect().maxY() : visualOverflowRect().maxX();

    bool inEmptyRegionsSection = false;
    RenderRegion* lastReg = lastRegion();
    for (RenderRegionList::iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        LayoutUnit flowMin = height - (isHorizontalWritingMode() ? region->flowThreadPortionRect().y() : region->flowThreadPortionRect().x());
        LayoutUnit flowMax = height - (isHorizontalWritingMode() ? region->flowThreadPortionRect().maxY() : region->flowThreadPortionRect().maxX());
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

        if (region == firstEmptyRegion)
            inEmptyRegionsSection = true;

        // Clear the overrideLogicalContentHeight value for autoheight regions that didn't receive any content.
        if (inEmptyRegionsSection && region->hasAutoLogicalHeight())
            region->clearOverrideLogicalContentHeight();
    }

    // With the regions overflow state computed we can also set the overset flag for the named flow.
    // If there are no valid regions in the chain, overset is true.
    m_overset = lastReg ? lastReg->regionState() == RenderRegion::RegionOverset : true;
}

bool RenderFlowThread::regionInRange(const RenderRegion* targetRegion, const RenderRegion* startRegion, const RenderRegion* endRegion) const
{
    ASSERT(targetRegion);

    for (RenderRegionList::const_iterator it = m_regionList.find(const_cast<RenderRegion*>(startRegion)); it != m_regionList.end(); ++it) {
        const RenderRegion* currRegion = *it;
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

    RenderFlowThread* flowThread = object->flowThreadContainingBlock();
    if (flowThread != this)
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
            if (currRegion == region)
                break;
            if (objectABBRect.intersects(currRegion->absoluteBoundingBoxRect(true)))
                return false;
        }
        return true;
    }

    return false;
}

#ifndef NDEBUG
bool RenderFlowThread::isAutoLogicalHeightRegionsCountConsistent() const
{
    unsigned autoLogicalHeightRegions = 0;
    for (RenderRegionList::const_iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        const RenderRegion* region = *iter;
        if (region->hasAutoLogicalHeight())
            autoLogicalHeightRegions++;
    }

    return autoLogicalHeightRegions == m_autoLogicalHeightRegionsCount;
}
#endif

void RenderFlowThread::resetRegionsOverrideLogicalContentHeight()
{
    ASSERT(view()->layoutState());
    ASSERT(view()->normalLayoutPhase());

    if (!hasAutoLogicalHeightRegions())
        return;

    for (RenderRegionList::iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        if (!region->hasAutoLogicalHeight())
            continue;

        region->clearOverrideLogicalContentHeight();
        // FIXME: We need to find a way to avoid marking all the regions ancestors for layout
        // as we are already inside layout.
        region->setNeedsLayout(true);
    }
    // Make sure we don't skip any region breaks when we do the layout again.
    // Using m_regionsInvalidated to force all the RenderFlowThread children do the layout again.
    invalidateRegions();
}

// During the normal layout phase of the named flow the regions are initialized with a height equal to their max-height.
// This way unforced breaks are automatically placed when a region is full and the content height/position correctly estimated.
// Also, the region where a forced break falls is exactly the region found at the forced break offset inside the flow content.
void RenderFlowThread::initializeRegionsOverrideLogicalContentHeight(RenderRegion* startRegion)
{
    ASSERT(view()->normalLayoutPhase());
    if (!hasAutoLogicalHeightRegions())
        return;

    RenderRegionList::iterator regionIter = startRegion ? m_regionList.find(startRegion) : m_regionList.begin();
    for (; regionIter != m_regionList.end(); ++regionIter) {
        RenderRegion* region = *regionIter;
        if (region->hasAutoLogicalHeight())
            region->setOverrideLogicalContentHeight(region->maxPageLogicalHeight());
    }
}

void RenderFlowThread::markAutoLogicalHeightRegionsForLayout()
{
    ASSERT(view()->layoutState());
    ASSERT(view()->constrainedFlowThreadsLayoutPhase());

    if (!hasAutoLogicalHeightRegions())
        return;

    for (RenderRegionList::iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        if (!region->hasAutoLogicalHeight())
            continue;

        // FIXME: We need to find a way to avoid marking all the regions ancestors for layout
        // as we are already inside layout.
        region->setNeedsLayout(true);
    }

    invalidateRegions();
}

void RenderFlowThread::updateRegionsFlowThreadPortionRect()
{
    LayoutUnit logicalHeight = 0;
    for (RenderRegionList::iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;

        LayoutUnit regionLogicalWidth = region->pageLogicalWidth();
        LayoutUnit regionLogicalHeight = region->logicalHeightOfAllFlowThreadContent();

        LayoutRect regionRect(style()->direction() == LTR ? LayoutUnit() : logicalWidth() - regionLogicalWidth, logicalHeight, regionLogicalWidth, regionLogicalHeight);

        region->setFlowThreadPortionRect(isHorizontalWritingMode() ? regionRect : regionRect.transposedRect());
        logicalHeight += regionLogicalHeight;
    }
}

// Even if we require the break to occur at offsetBreakInFlowThread, because regions may have min/max-height values,
// it is possible that the break will occur at a different offset than the original one required.
// offsetBreakAdjustment measures the different between the requested break offset and the current break offset.
bool RenderFlowThread::addForcedRegionBreak(LayoutUnit offsetBreakInFlowThread, RenderObject* breakChild, bool isBefore, LayoutUnit* offsetBreakAdjustment)
{
    // We take breaks into account for height computation for auto logical height regions
    // only in the layout phase in which we lay out the flows threads unconstrained
    // and we use the content breaks to determine the overrideContentLogicalHeight for
    // auto logical height regions.
    if (view()->constrainedFlowThreadsLayoutPhase())
        return false;

    // Breaks can come before or after some objects. We need to track these objects, so that if we get
    // multiple breaks for the same object (for example because of multiple layouts on the same object),
    // we need to invalidate every other region after the old one and start computing from fresh.
    RenderObjectToRegionMap& mapToUse = isBefore ? m_breakBeforeToRegionMap : m_breakAfterToRegionMap;
    RenderObjectToRegionMap::iterator iter = mapToUse.find(breakChild);
    if (iter != mapToUse.end()) {
        RenderRegionList::iterator regionIter = m_regionList.find(iter->value);
        ASSERT(regionIter != m_regionList.end());
        ASSERT((*regionIter)->hasAutoLogicalHeight());
        initializeRegionsOverrideLogicalContentHeight(*regionIter);

        // We need to update the regions flow thread portion rect because we are going to process
        // a break on these regions.
        updateRegionsFlowThreadPortionRect();
    }

    // Simulate a region break at offsetBreakInFlowThread. If it points inside an auto logical height region,
    // then it determines the region override logical content height.
    RenderRegion* region = regionAtBlockOffset(offsetBreakInFlowThread);
    if (!region)
        return false;

    bool overrideLogicalContentHeightComputed = false;

    LayoutUnit currentRegionOffsetInFlowThread = isHorizontalWritingMode() ? region->flowThreadPortionRect().y() : region->flowThreadPortionRect().x();
    LayoutUnit offsetBreakInCurrentRegion = offsetBreakInFlowThread - currentRegionOffsetInFlowThread;

    if (region->hasAutoLogicalHeight()) {
        // A forced break can appear only in an auto-height region that didn't have a forced break before.
        // This ASSERT is a good-enough heuristic to verify the above condition.
        ASSERT(region->maxPageLogicalHeight() == region->overrideLogicalContentHeight());

        mapToUse.set(breakChild, region);

        overrideLogicalContentHeightComputed = true;

        // Compute the region height pretending that the offsetBreakInCurrentRegion is the logicalHeight for the auto-height region.
        LayoutUnit regionOverrideLogicalContentHeight = region->computeReplacedLogicalHeightRespectingMinMaxHeight(offsetBreakInCurrentRegion);

        // The new height of this region needs to be smaller than the initial value, the max height. A forced break is the only way to change the initial
        // height of an auto-height region besides content ending.
        ASSERT(regionOverrideLogicalContentHeight <= region->maxPageLogicalHeight());

        region->setOverrideLogicalContentHeight(regionOverrideLogicalContentHeight);

        currentRegionOffsetInFlowThread += regionOverrideLogicalContentHeight;
    } else
        currentRegionOffsetInFlowThread += isHorizontalWritingMode() ? region->flowThreadPortionRect().height() : region->flowThreadPortionRect().width();

    // If the break was found inside an auto-height region its size changed so we need to recompute the flow thread portion rectangles.
    if (overrideLogicalContentHeightComputed)
        updateRegionsFlowThreadPortionRect();

    if (offsetBreakAdjustment)
        *offsetBreakAdjustment = max<LayoutUnit>(0, currentRegionOffsetInFlowThread - offsetBreakInFlowThread);

    return overrideLogicalContentHeightComputed;
}

void RenderFlowThread::incrementAutoLogicalHeightRegions()
{
    if (!m_autoLogicalHeightRegionsCount)
        view()->flowThreadController()->incrementFlowThreadsWithAutoLogicalHeightRegions();
    ++m_autoLogicalHeightRegionsCount;
}

void RenderFlowThread::decrementAutoLogicalHeightRegions()
{
    ASSERT(m_autoLogicalHeightRegionsCount > 0);
    --m_autoLogicalHeightRegionsCount;
    if (!m_autoLogicalHeightRegionsCount)
        view()->flowThreadController()->decrementFlowThreadsWithAutoLogicalHeightRegions();
}

void RenderFlowThread::collectLayerFragments(LayerFragments& layerFragments, const LayoutRect& layerBoundingBox, const LayoutRect& dirtyRect)
{
    ASSERT(!m_regionsInvalidated);
    
    for (RenderRegionList::const_iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        region->collectLayerFragments(layerFragments, layerBoundingBox, dirtyRect);
    }
}

LayoutRect RenderFlowThread::fragmentsBoundingBox(const LayoutRect& layerBoundingBox)
{
    ASSERT(!m_regionsInvalidated);
    
    LayoutRect result;
    for (RenderRegionList::const_iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        LayerFragments fragments;
        region->collectLayerFragments(fragments, layerBoundingBox, PaintInfo::infiniteRect());
        for (size_t i = 0; i < fragments.size(); ++i) {
            const LayerFragment& fragment = fragments.at(i);
            LayoutRect fragmentRect(layerBoundingBox);
            fragmentRect.intersect(fragment.paginationClip);
            fragmentRect.moveBy(fragment.paginationOffset);
            result.unite(fragmentRect);
        }
    }
    
    return result;
}

CurrentRenderFlowThreadMaintainer::CurrentRenderFlowThreadMaintainer(RenderFlowThread* renderFlowThread)
    : m_renderFlowThread(renderFlowThread)
    , m_previousRenderFlowThread(0)
{
    if (!m_renderFlowThread)
        return;
    RenderView* view = m_renderFlowThread->view();
    m_previousRenderFlowThread = view->flowThreadController()->currentRenderFlowThread();
    ASSERT(!m_previousRenderFlowThread || !renderFlowThread->isRenderNamedFlowThread());
    view->flowThreadController()->setCurrentRenderFlowThread(m_renderFlowThread);
}

CurrentRenderFlowThreadMaintainer::~CurrentRenderFlowThreadMaintainer()
{
    if (!m_renderFlowThread)
        return;
    RenderView* view = m_renderFlowThread->view();
    ASSERT(view->flowThreadController()->currentRenderFlowThread() == m_renderFlowThread);
    view->flowThreadController()->setCurrentRenderFlowThread(m_previousRenderFlowThread);
}


} // namespace WebCore
