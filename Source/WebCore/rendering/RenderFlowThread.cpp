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
#include "PODIntervalTree.h"
#include "PaintInfo.h"
#include "RenderBoxRegionInfo.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderLayerCompositor.h"
#include "RenderNamedFlowFragment.h"
#include "RenderRegion.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "TransformState.h"
#include "WebKitNamedFlow.h"
#include <wtf/StackStats.h>

namespace WebCore {

RenderFlowThread::RenderFlowThread(Document& document, PassRef<RenderStyle> style)
    : RenderBlockFlow(document, std::move(style))
    , m_previousRegionCount(0)
    , m_autoLogicalHeightRegionsCount(0)
    , m_regionsInvalidated(false)
    , m_regionsHaveUniformLogicalWidth(true)
    , m_regionsHaveUniformLogicalHeight(true)
    , m_dispatchRegionLayoutUpdateEvent(false)
    , m_dispatchRegionOversetChangeEvent(false)
    , m_pageLogicalSizeChanged(false)
    , m_layoutPhase(LayoutPhaseMeasureContent)
    , m_needsTwoPhasesLayout(false)
#if USE(ACCELERATED_COMPOSITING)
    , m_layersToRegionMappingsDirty(true)
#endif
{
    setFlowThreadState(InsideOutOfFlowThread);
}

PassRef<RenderStyle> RenderFlowThread::createFlowThreadStyle(RenderStyle* parentStyle)
{
    auto newStyle = RenderStyle::create();
    newStyle.get().inheritFrom(parentStyle);
    newStyle.get().setDisplay(BLOCK);
    newStyle.get().setPosition(AbsolutePosition);
    newStyle.get().setZIndex(0);
    newStyle.get().setLeft(Length(0, Fixed));
    newStyle.get().setTop(Length(0, Fixed));
    newStyle.get().setWidth(Length(100, Percent));
    newStyle.get().setHeight(Length(100, Percent));
    newStyle.get().font().update(0);
    return newStyle;
}

void RenderFlowThread::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlockFlow::styleDidChange(diff, oldStyle);

    if (oldStyle && oldStyle->writingMode() != style().writingMode())
        invalidateRegions();
}

void RenderFlowThread::removeFlowChildInfo(RenderObject* child)
{
    if (child->isBox())
        removeRenderBoxRegionInfo(toRenderBox(child));
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
    ASSERT(!inFinalLayoutPhase());

    if (m_regionsInvalidated) {
        ASSERT(selfNeedsLayout());
        return;
    }

    m_regionRangeMap.clear();
    m_breakBeforeToRegionMap.clear();
    m_breakAfterToRegionMap.clear();
    setNeedsLayout();

    m_regionsInvalidated = true;
}

class CurrentRenderFlowThreadDisabler {
    WTF_MAKE_NONCOPYABLE(CurrentRenderFlowThreadDisabler);
public:
    CurrentRenderFlowThreadDisabler(RenderView* view)
        : m_view(view)
        , m_renderFlowThread(0)
    {
        m_renderFlowThread = m_view->flowThreadController().currentRenderFlowThread();
        if (m_renderFlowThread)
            view->flowThreadController().setCurrentRenderFlowThread(0);
    }
    ~CurrentRenderFlowThreadDisabler()
    {
        if (m_renderFlowThread)
            m_view->flowThreadController().setCurrentRenderFlowThread(m_renderFlowThread);
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
            
            for (auto iter = m_regionList.begin(), end = m_regionList.end(); iter != end; ++iter) {
                RenderRegion* region = *iter;
                ASSERT(!region->needsLayout() || region->isRenderRegionSet());

                region->deleteAllRenderBoxRegionInfo();

                // In the measure content layout phase we need to initialize the computedAutoHeight for auto-height regions.
                // See initializeRegionsComputedAutoHeight for the explanation.
                // Also, if we have auto-height regions we can't assume m_regionsHaveUniformLogicalHeight to be true in the first phase
                // because the auto-height regions don't have their height computed yet.
                if (inMeasureContentLayoutPhase() && region->hasAutoLogicalHeight()) {
                    RenderNamedFlowFragment* namedFlowFragment = toRenderNamedFlowFragment(region);
                    namedFlowFragment->setComputedAutoHeight(namedFlowFragment->maxPageLogicalHeight());
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

            setRegionRangeForBox(this, m_regionList.first(), m_regionList.last());
        }
    }

    updateLogicalWidth(); // Called to get the maximum logical width for the region.
    updateRegionsFlowThreadPortionRect();
}

void RenderFlowThread::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;

    m_pageLogicalSizeChanged = m_regionsInvalidated && everHadLayout();

    // In case this is the second pass of the measure content phase we need to update the auto-height regions to their initial value.
    // If the region chain was invalidated this will happen anyway.
    if (!m_regionsInvalidated && inMeasureContentLayoutPhase())
        initializeRegionsComputedAutoHeight();

    // This is the first phase of the layout and because we have auto-height regions we'll need a second
    // pass to update the flow with the computed auto-height regions.
    // It's also possible to need a secondary layout if the overflow computation invalidated the region chain (e.g. overflow: auto scrollbars
    // shrunk some regions) so repropagation is required.
    m_needsTwoPhasesLayout = (inMeasureContentLayoutPhase() && hasAutoLogicalHeightRegions()) || (inOverflowLayoutPhase() && m_regionsInvalidated);

    validateRegions();

    CurrentRenderFlowThreadMaintainer currentFlowThreadSetter(this);
    RenderBlockFlow::layout();

    m_pageLogicalSizeChanged = false;

    if (lastRegion())
        lastRegion()->expandToEncompassFlowThreadContentsIfNeeded();

#if USE(ACCELERATED_COMPOSITING)
    // If there are children layers in the RenderFlowThread then we need to make sure that the
    // composited children layers will land in the right RenderRegions. Also, the parent RenderRegions
    // will get RenderLayers and become composited as needed.
    // Note that there's no need to do so for the inline multi-column as we are not moving layers into different
    // containers, but just adjusting the position of the RenderLayerBacking.
    if (!m_needsTwoPhasesLayout) {
        // If we have layers that moved from one region to another, we trigger
        // a composited layers rebuild in here to make sure that the regions will collect the right layers.
        if (updateAllLayerToRegionMappings())
            layer()->compositor().setCompositingLayersNeedRebuild();
    }
#endif

    if (shouldDispatchRegionLayoutUpdateEvent())
        dispatchRegionLayoutUpdateEvent();
    
    if (shouldDispatchRegionOversetChangeEvent())
        dispatchRegionOversetChangeEvent();
}

#if USE(ACCELERATED_COMPOSITING)
bool RenderFlowThread::hasCompositingRegionDescendant() const
{
    for (auto iter = m_regionList.begin(), end = m_regionList.end(); iter != end; ++iter)
        if (RenderLayerModelObject* layerOwner = toRenderNamedFlowFragment(*iter)->layerOwner())
            if (layerOwner->hasLayer() && layerOwner->layer()->hasCompositingDescendant())
                return true;

    return false;
}

const RenderLayerList* RenderFlowThread::getLayerListForRegion(RenderNamedFlowFragment* region)
{
    if (!m_regionToLayerListMap)
        return 0;
    updateAllLayerToRegionMappingsIfNeeded();
    auto iterator = m_regionToLayerListMap->find(region);
    return iterator == m_regionToLayerListMap->end() ? 0 : &iterator->value;
}

RenderNamedFlowFragment* RenderFlowThread::regionForCompositedLayer(RenderLayer& childLayer)
{
    if (childLayer.renderer().fixedPositionedWithNamedFlowContainingBlock())
        return 0;

    if (childLayer.renderBox()) {
        RenderRegion* startRegion = 0;
        RenderRegion* endRegion = 0;
        getRegionRangeForBox(childLayer.renderBox(), startRegion, endRegion);
        // The video tag is such a box that doesn't have a region range because it's inline (by default).
        if (startRegion)
            return toRenderNamedFlowFragment(startRegion);
    }

    // FIXME: remove this when we'll have region ranges for inlines as well.
    LayoutPoint flowThreadOffset = flooredLayoutPoint(childLayer.renderer().localToContainerPoint(LayoutPoint(), this, ApplyContainerFlip));
    return toRenderNamedFlowFragment(regionAtBlockOffset(0, flipForWritingMode(isHorizontalWritingMode() ? flowThreadOffset.y() : flowThreadOffset.x()), true, DisallowRegionAutoGeneration));
}

RenderNamedFlowFragment* RenderFlowThread::cachedRegionForCompositedLayer(RenderLayer& childLayer)
{
    if (!m_layerToRegionMap)
        return 0;
    updateAllLayerToRegionMappingsIfNeeded();
    return m_layerToRegionMap->get(&childLayer);
}

void RenderFlowThread::updateLayerToRegionMappings(RenderLayer& layer, LayerToRegionMap& layerToRegionMap, RegionToLayerListMap& regionToLayerListMap, bool& needsLayerUpdate)
{
    RenderNamedFlowFragment* region = regionForCompositedLayer(layer);
    if (!needsLayerUpdate) {
        // Figure out if we moved this layer from a region to the other.
        RenderNamedFlowFragment* previousRegion = cachedRegionForCompositedLayer(layer);
        if (previousRegion != region)
            needsLayerUpdate = true;
    }

    if (!region)
        return;

    layerToRegionMap.set(&layer, region);

    auto iterator = regionToLayerListMap.find(region);
    RenderLayerList& list = iterator == regionToLayerListMap.end() ? regionToLayerListMap.set(region, RenderLayerList()).iterator->value : iterator->value;
    ASSERT(!list.contains(&layer));
    list.append(&layer);
}

bool RenderFlowThread::updateAllLayerToRegionMappings()
{
    if (!collectsGraphicsLayersUnderRegions())
        return false;

    // We can't use currentFlowThread as it is possible to have interleaved flow threads and the wrong one could be used.
    // Let each region figure out the proper enclosing flow thread.
    CurrentRenderFlowThreadDisabler disabler(&view());

    // If the RenderFlowThread had a z-index layer update, then we need to update the composited layers too.
    bool needsLayerUpdate = layer()->isDirtyRenderFlowThread() || m_layersToRegionMappingsDirty || !m_layerToRegionMap.get();
    layer()->updateLayerListsIfNeeded();

    LayerToRegionMap layerToRegionMap;
    RegionToLayerListMap regionToLayerListMap;

    RenderLayerList* lists[] = { layer()->negZOrderList(), layer()->normalFlowList(), layer()->posZOrderList()};
    for (size_t listIndex = 0; listIndex < sizeof(lists) / sizeof(lists[0]); ++listIndex)
        if (RenderLayerList* list = lists[listIndex])
            for (size_t i = 0, listSize = list->size(); i < listSize; ++i)
                updateLayerToRegionMappings(*list->at(i), layerToRegionMap, regionToLayerListMap, needsLayerUpdate);

    if (needsLayerUpdate) {
        if (!m_layerToRegionMap)
            m_layerToRegionMap = adoptPtr(new LayerToRegionMap());
        m_layerToRegionMap->swap(layerToRegionMap);

        if (!m_regionToLayerListMap)
            m_regionToLayerListMap = adoptPtr(new RegionToLayerListMap());
        m_regionToLayerListMap->swap(regionToLayerListMap);
    }

    m_layersToRegionMappingsDirty = false;

    return needsLayerUpdate;
}
#endif

bool RenderFlowThread::collectsGraphicsLayersUnderRegions() const
{
    // We only need to map layers to regions for named flow threads.
    // Multi-column threads are displayed on top of the regions and do not require
    // distributing the layers.

    return false;
}

void RenderFlowThread::updateLogicalWidth()
{
    LayoutUnit logicalWidth = initialLogicalWidth();
    for (auto iter = m_regionList.begin(), end = m_regionList.end(); iter != end; ++iter) {
        RenderRegion* region = *iter;
        ASSERT(!region->needsLayout() || region->isRenderRegionSet());
        logicalWidth = std::max(region->pageLogicalWidth(), logicalWidth);
    }
    setLogicalWidth(logicalWidth);

    // If the regions have non-uniform logical widths, then insert inset information for the RenderFlowThread.
    for (auto iter = m_regionList.begin(), end = m_regionList.end(); iter != end; ++iter) {
        RenderRegion* region = *iter;
        LayoutUnit regionLogicalWidth = region->pageLogicalWidth();
        LayoutUnit logicalLeft = style().direction() == LTR ? LayoutUnit() : logicalWidth - regionLogicalWidth;
        region->setRenderBoxRegionInfo(this, logicalLeft, regionLogicalWidth, false);
    }
}

void RenderFlowThread::computeLogicalHeight(LayoutUnit, LayoutUnit logicalTop, LogicalExtentComputedValues& computedValues) const
{
    computedValues.m_position = logicalTop;
    computedValues.m_extent = 0;

    const LayoutUnit maxFlowSize = RenderFlowThread::maxLogicalHeight();
    for (auto iter = m_regionList.begin(), end = m_regionList.end(); iter != end; ++iter) {
        RenderRegion* region = *iter;
        ASSERT(!region->needsLayout() || region->isRenderRegionSet());

        LayoutUnit distanceToMaxSize = maxFlowSize - computedValues.m_extent;
        computedValues.m_extent += std::min(distanceToMaxSize, region->logicalHeightOfAllFlowThreadContent());

        // If we reached the maximum size there's no point in going further.
        if (computedValues.m_extent == maxFlowSize)
            return;
    }
}

LayoutRect RenderFlowThread::computeRegionClippingRect(const LayoutPoint& offset, const LayoutRect& flowThreadPortionRect, const LayoutRect& flowThreadPortionOverflowRect) const
{
    LayoutRect regionClippingRect(offset + (flowThreadPortionOverflowRect.location() - flowThreadPortionRect.location()), flowThreadPortionOverflowRect.size());
    if (style().isFlippedBlocksWritingMode())
        regionClippingRect.move(flowThreadPortionRect.size() - flowThreadPortionOverflowRect.size());
    return regionClippingRect;
}

bool RenderFlowThread::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    if (hitTestAction == HitTestBlockBackground)
        return false;
    return RenderBlockFlow::nodeAtPoint(request, result, locationInContainer, accumulatedOffset, hitTestAction);
}

bool RenderFlowThread::shouldRepaint(const LayoutRect& r) const
{
    if (view().printing() || r.isEmpty())
        return false;

    return true;
}

void RenderFlowThread::repaintRectangleInRegions(const LayoutRect& repaintRect, bool immediate) const
{
    if (!shouldRepaint(repaintRect) || !hasValidRegionInfo())
        return;

    LayoutStateDisabler layoutStateDisabler(&view()); // We can't use layout state to repaint, since the regions are somewhere else.

    // We can't use currentFlowThread as it is possible to have interleaved flow threads and the wrong one could be used.
    // Let each region figure out the proper enclosing flow thread.
    CurrentRenderFlowThreadDisabler disabler(&view());
    
    for (auto iter = m_regionList.begin(), end = m_regionList.end(); iter != end; ++iter) {
        RenderRegion* region = *iter;

        region->repaintFlowThreadContent(repaintRect, immediate);
    }
}

RenderRegion* RenderFlowThread::regionAtBlockOffset(const RenderBox* clampBox, LayoutUnit offset, bool extendLastRegion, RegionAutoGenerationPolicy autoGenerationPolicy)
{
    ASSERT(!m_regionsInvalidated);

    if (autoGenerationPolicy == AllowRegionAutoGeneration)
        autoGenerateRegionsToBlockOffset(offset);

    if (m_regionList.isEmpty())
        return 0;

    if (offset <= 0)
        return clampBox ? clampBox->clampToStartAndEndRegions(m_regionList.first()) : m_regionList.first();

    RegionSearchAdapter adapter(offset);
    m_regionIntervalTree.allOverlapsWithAdapter<RegionSearchAdapter>(adapter);

    // If no region was found, the offset is in the flow thread overflow.
    // The last region will contain the offset if extendLastRegion is set or if the last region is a set.
    if (!adapter.result() && (extendLastRegion || m_regionList.last()->isRenderRegionSet()))
        return clampBox ? clampBox->clampToStartAndEndRegions(m_regionList.last()) : m_regionList.last();

    RenderRegion* region = adapter.result();
    if (!clampBox)
        return region;
    return region ? clampBox->clampToStartAndEndRegions(region) : 0;
}
    
LayoutPoint RenderFlowThread::adjustedPositionRelativeToOffsetParent(const RenderBoxModelObject& boxModelObject, const LayoutPoint& startPoint)
{
    LayoutPoint referencePoint = startPoint;
    
    const RenderBlock* objContainingBlock = boxModelObject.containingBlock();
    // FIXME: This needs to be adapted for different writing modes inside the flow thread.
    RenderRegion* startRegion = regionAtBlockOffset(objContainingBlock, referencePoint.y());
    if (startRegion) {
        // Take into account the offset coordinates of the region.
        RenderBoxModelObject* startRegionBox = startRegion->isRenderNamedFlowFragment() ? toRenderBoxModelObject(startRegion->parent()) : startRegion;
        RenderBoxModelObject* currObject = startRegionBox;
        RenderBoxModelObject* currOffsetParent;
        while ((currOffsetParent = currObject->offsetParent())) {
            referencePoint.move(currObject->offsetLeft(), currObject->offsetTop());
            
            // Since we're looking for the offset relative to the body, we must also
            // take into consideration the borders of the region's offsetParent.
            if (currOffsetParent->isBox() && !currOffsetParent->isBody())
                referencePoint.move(toRenderBox(currOffsetParent)->borderLeft(), toRenderBox(currOffsetParent)->borderTop());
            
            currObject = currOffsetParent;
        }
        
        // We need to check if any of this box's containing blocks start in a different region
        // and if so, drop the object's top position (which was computed relative to its containing block
        // and is no longer valid) and recompute it using the region in which it flows as reference.
        bool wasComputedRelativeToOtherRegion = false;
        while (objContainingBlock && !objContainingBlock->isRenderNamedFlowThread()) {
            // Check if this object is in a different region.
            RenderRegion* parentStartRegion = 0;
            RenderRegion* parentEndRegion = 0;
            getRegionRangeForBox(objContainingBlock, parentStartRegion, parentEndRegion);
            if (parentStartRegion && parentStartRegion != startRegion) {
                wasComputedRelativeToOtherRegion = true;
                break;
            }
            objContainingBlock = objContainingBlock->containingBlock();
        }
        
        if (wasComputedRelativeToOtherRegion) {
            if (boxModelObject.isBox()) {
                // Use borderBoxRectInRegion to account for variations such as percentage margins.
                LayoutRect borderBoxRect = toRenderBox(boxModelObject).borderBoxRectInRegion(startRegion, RenderBox::DoNotCacheRenderBoxRegionInfo);
                referencePoint.move(borderBoxRect.location().x(), 0);
            }
            
            // Get the logical top coordinate of the current object.
            LayoutUnit top = 0;
            if (boxModelObject.isRenderBlock())
                top = toRenderBlock(boxModelObject).offsetFromLogicalTopOfFirstPage();
            else {
                if (boxModelObject.containingBlock())
                    top = boxModelObject.containingBlock()->offsetFromLogicalTopOfFirstPage();
                
                if (boxModelObject.isBox())
                    top += toRenderBox(boxModelObject).topLeftLocation().y();
                else if (boxModelObject.isRenderInline())
                    top -= toRenderInline(boxModelObject).borderTop();
            }
            
            // Get the logical top of the region this object starts in
            // and compute the object's top, relative to the region's top.
            LayoutUnit regionLogicalTop = startRegion->pageLogicalTopForOffset(top);
            LayoutUnit topRelativeToRegion = top - regionLogicalTop;
            referencePoint.setY(startRegionBox->offsetTop() + topRelativeToRegion);
            
            // Since the top has been overriden, check if the
            // relative/sticky positioning must be reconsidered.
            if (boxModelObject.isRelPositioned())
                referencePoint.move(0, boxModelObject.relativePositionOffset().height());
            else if (boxModelObject.isStickyPositioned())
                referencePoint.move(0, boxModelObject.stickyPositionOffset().height());
        }
        
        // Since we're looking for the offset relative to the body, we must also
        // take into consideration the borders of the region.
        referencePoint.move(startRegionBox->borderLeft(), startRegionBox->borderTop());
    }
    
    return referencePoint;
}

LayoutUnit RenderFlowThread::pageLogicalTopForOffset(LayoutUnit offset)
{
    RenderRegion* region = regionAtBlockOffset(0, offset, false, AllowRegionAutoGeneration);
    return region ? region->pageLogicalTopForOffset(offset) : LayoutUnit();
}

LayoutUnit RenderFlowThread::pageLogicalWidthForOffset(LayoutUnit offset)
{
    RenderRegion* region = regionAtBlockOffset(0, offset, true, AllowRegionAutoGeneration);
    return region ? region->pageLogicalWidth() : contentLogicalWidth();
}

LayoutUnit RenderFlowThread::pageLogicalHeightForOffset(LayoutUnit offset)
{
    RenderRegion* region = regionAtBlockOffset(0, offset, false, AllowRegionAutoGeneration);
    if (!region)
        return 0;

    return region->pageLogicalHeight();
}

LayoutUnit RenderFlowThread::pageRemainingLogicalHeightForOffset(LayoutUnit offset, PageBoundaryRule pageBoundaryRule)
{
    RenderRegion* region = regionAtBlockOffset(0, offset, false, AllowRegionAutoGeneration);
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
    RenderRegion* renderRegion = const_cast<RenderFlowThread*>(this)->regionAtBlockOffset(this, isHorizontalWritingMode() ? center.y() : center.x(), true, DisallowRegionAutoGeneration);
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

    for (auto iter = m_regionList.find(startRegion), end = m_regionList.end(); iter != end; ++iter) {
        RenderRegion* region = *iter;
        region->removeRenderBoxRegionInfo(box);
        if (region == endRegion)
            break;
    }

#ifndef NDEBUG
    // We have to make sure we did not leave any RenderBoxRegionInfo attached.
    for (auto iter = m_regionList.begin(), end = m_regionList.end(); iter != end; ++iter) {
        RenderRegion* region = *iter;
        ASSERT(!region->renderBoxRegionInfo(box));
    }
#endif

    m_regionRangeMap.remove(box);
}

void RenderFlowThread::logicalWidthChangedInRegionsForBlock(const RenderBlock* block, bool& relayoutChildren)
{
    if (!hasValidRegionInfo())
        return;

    auto it = m_regionRangeMap.find(block);
    if (it == m_regionRangeMap.end())
        return;

    RenderRegionRange& range = it->value;
    bool rangeInvalidated = range.rangeInvalidated();
    range.clearRangeInvalidated();

    // If there will be a relayout anyway skip the next steps because they only verify
    // the state of the ranges.
    if (relayoutChildren)
        return;

    RenderRegion* startRegion;
    RenderRegion* endRegion;
    getRegionRangeForBox(block, startRegion, endRegion);

    // Not necessary for the flow thread, since we already computed the correct info for it.
    // If the regions have changed invalidate the children.
    if (block == this) {
        relayoutChildren = m_pageLogicalSizeChanged;
        return;
    }

    for (auto iter = m_regionList.find(startRegion), end = m_regionList.end(); iter != end; ++iter) {
        RenderRegion* region = *iter;
        ASSERT(!region->needsLayout() || region->isRenderRegionSet());

        // We have no information computed for this region so we need to do it.
        OwnPtr<RenderBoxRegionInfo> oldInfo = region->takeRenderBoxRegionInfo(block);
        if (!oldInfo) {
            relayoutChildren = rangeInvalidated;
            return;
        }

        LayoutUnit oldLogicalWidth = oldInfo->logicalWidth();
        RenderBoxRegionInfo* newInfo = block->renderBoxRegionInfo(region);
        if (!newInfo || newInfo->logicalWidth() != oldLogicalWidth) {
            relayoutChildren = true;
            return;
        }

        if (region == endRegion)
            break;
    }
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
    if (!hasRegions())
        return 0;
    return m_regionList.first();
}

RenderRegion* RenderFlowThread::lastRegion() const
{
    if (!hasRegions())
        return 0;
    return m_regionList.last();
}

void RenderFlowThread::clearRenderBoxRegionInfoAndCustomStyle(const RenderBox* box,
    const RenderRegion* newStartRegion, const RenderRegion* newEndRegion,
    const RenderRegion* oldStartRegion, const RenderRegion* oldEndRegion)
{
    ASSERT(newStartRegion && newEndRegion && oldStartRegion && oldEndRegion);

    bool insideOldRegionRange = false;
    bool insideNewRegionRange = false;
    for (auto iter = m_regionList.begin(), end = m_regionList.end(); iter != end; ++iter) {
        RenderRegion* region = *iter;

        if (oldStartRegion == region)
            insideOldRegionRange = true;
        if (newStartRegion == region)
            insideNewRegionRange = true;

        if (!(insideOldRegionRange && insideNewRegionRange)) {
            if (region->isRenderNamedFlowFragment())
                toRenderNamedFlowFragment(region)->clearObjectStyleInRegion(box);
            if (region->renderBoxRegionInfo(box))
                region->removeRenderBoxRegionInfo(box);
        }

        if (oldEndRegion == region)
            insideOldRegionRange = false;
        if (newEndRegion == region)
            insideNewRegionRange = false;
    }
}

void RenderFlowThread::setRegionRangeForBox(const RenderBox* box, RenderRegion* startRegion, RenderRegion* endRegion)
{
    ASSERT(hasRegions());
    ASSERT(startRegion && endRegion);

    auto it = m_regionRangeMap.find(box);
    if (it == m_regionRangeMap.end()) {
        m_regionRangeMap.set(box, RenderRegionRange(startRegion, endRegion));
        return;
    }

    // If nothing changed, just bail.
    RenderRegionRange& range = it->value;
    if (range.startRegion() == startRegion && range.endRegion() == endRegion)
        return;

    clearRenderBoxRegionInfoAndCustomStyle(box, startRegion, endRegion, range.startRegion(), range.endRegion());
    range.setRange(startRegion, endRegion);
}

void RenderFlowThread::getRegionRangeForBox(const RenderBox* box, RenderRegion*& startRegion, RenderRegion*& endRegion) const
{
    startRegion = 0;
    endRegion = 0;
    auto it = m_regionRangeMap.find(box);
    if (it == m_regionRangeMap.end())
        return;

    const RenderRegionRange& range = it->value;
    startRegion = range.startRegion();
    endRegion = range.endRegion();
    ASSERT(m_regionList.contains(startRegion) && m_regionList.contains(endRegion));
}

void RenderFlowThread::applyBreakAfterContent(LayoutUnit clientHeight)
{
    // Simulate a region break at height. If it points inside an auto logical height region,
    // then it may determine the region computed autoheight.
    addForcedRegionBreak(this, clientHeight, this, false);
}

bool RenderFlowThread::regionInRange(const RenderRegion* targetRegion, const RenderRegion* startRegion, const RenderRegion* endRegion) const
{
    ASSERT(targetRegion);

    for (auto it = m_regionList.find(const_cast<RenderRegion*>(startRegion)), end = m_regionList.end(); it != end; ++it) {
        const RenderRegion* currRegion = *it;
        if (targetRegion == currRegion)
            return true;
        if (currRegion == endRegion)
            break;
    }

    return false;
}

bool RenderFlowThread::objectShouldPaintInFlowRegion(const RenderObject* object, const RenderRegion* region) const
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
    
    // If the box has no range, do not check regionInRange. Boxes inside inlines do not get ranges.
    // Instead, the containing RootInlineBox will abort when trying to paint inside the wrong region.
    if (enclosingBoxStartRegion && enclosingBoxEndRegion && !regionInRange(region, enclosingBoxStartRegion, enclosingBoxEndRegion))
        return false;
    
    return object->isBox() || object->isRenderInline();
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
        for (auto it = m_regionList.find(enclosingBoxStartRegion), end = m_regionList.end(); it != end; ++it) {
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
    for (auto iter = m_regionList.begin(), end = m_regionList.end(); iter != end; ++iter) {
        const RenderRegion* region = *iter;
        if (region->hasAutoLogicalHeight())
            autoLogicalHeightRegions++;
    }

    return autoLogicalHeightRegions == m_autoLogicalHeightRegionsCount;
}
#endif

// During the measure content layout phase of the named flow the regions are initialized with a height equal to their max-height.
// This way unforced breaks are automatically placed when a region is full and the content height/position correctly estimated.
// Also, the region where a forced break falls is exactly the region found at the forced break offset inside the flow content.
void RenderFlowThread::initializeRegionsComputedAutoHeight(RenderRegion* startRegion)
{
    ASSERT(inMeasureContentLayoutPhase());
    if (!hasAutoLogicalHeightRegions())
        return;

    for (auto regionIter = startRegion ? m_regionList.find(startRegion) : m_regionList.begin(),
        end = m_regionList.end(); regionIter != end; ++regionIter) {
        RenderRegion* region = *regionIter;
        if (region->hasAutoLogicalHeight()) {
            RenderNamedFlowFragment* namedFlowFragment = toRenderNamedFlowFragment(region);
            namedFlowFragment->setComputedAutoHeight(namedFlowFragment->maxPageLogicalHeight());
        }
    }
}

void RenderFlowThread::markAutoLogicalHeightRegionsForLayout()
{
    ASSERT(hasAutoLogicalHeightRegions());

    for (auto iter = m_regionList.begin(), end = m_regionList.end(); iter != end; ++iter) {
        RenderRegion* region = *iter;
        if (!region->hasAutoLogicalHeight())
            continue;

        // FIXME: We need to find a way to avoid marking all the regions ancestors for layout
        // as we are already inside layout.
        region->setNeedsLayout();
    }
}

void RenderFlowThread::markRegionsForOverflowLayoutIfNeeded()
{
    if (!hasRegions())
        return;

    for (auto iter = m_regionList.begin(), end = m_regionList.end(); iter != end; ++iter) {
        RenderRegion* region = *iter;
        region->setNeedsSimplifiedNormalFlowLayout();
    }
}

void RenderFlowThread::updateRegionsFlowThreadPortionRect(const RenderRegion* lastRegionWithContent)
{
    ASSERT(!lastRegionWithContent || (inMeasureContentLayoutPhase() && hasAutoLogicalHeightRegions()));
    LayoutUnit logicalHeight = 0;
    bool emptyRegionsSegment = false;
    // FIXME: Optimize not to clear the interval all the time. This implies manually managing the tree nodes lifecycle.
    m_regionIntervalTree.clear();
    m_regionIntervalTree.initIfNeeded();
    for (auto iter = m_regionList.begin(), end = m_regionList.end(); iter != end; ++iter) {
        RenderRegion* region = *iter;

        // If we find an empty auto-height region, clear the computedAutoHeight value.
        if (emptyRegionsSegment && region->hasAutoLogicalHeight())
            toRenderNamedFlowFragment(region)->clearComputedAutoHeight();

        LayoutUnit regionLogicalWidth = region->pageLogicalWidth();
        LayoutUnit regionLogicalHeight = std::min<LayoutUnit>(RenderFlowThread::maxLogicalHeight() - logicalHeight, region->logicalHeightOfAllFlowThreadContent());

        LayoutRect regionRect(style().direction() == LTR ? LayoutUnit() : logicalWidth() - regionLogicalWidth, logicalHeight, regionLogicalWidth, regionLogicalHeight);

        region->setFlowThreadPortionRect(isHorizontalWritingMode() ? regionRect : regionRect.transposedRect());

        m_regionIntervalTree.add(RegionIntervalTree::createInterval(logicalHeight, logicalHeight + regionLogicalHeight, region));

        logicalHeight += regionLogicalHeight;

        // Once we find the last region with content the next regions are considered empty.
        if (lastRegionWithContent == region)
            emptyRegionsSegment = true;
    }

    ASSERT(!lastRegionWithContent || emptyRegionsSegment);
}

// Even if we require the break to occur at offsetBreakInFlowThread, because regions may have min/max-height values,
// it is possible that the break will occur at a different offset than the original one required.
// offsetBreakAdjustment measures the different between the requested break offset and the current break offset.
bool RenderFlowThread::addForcedRegionBreak(const RenderBlock* block, LayoutUnit offsetBreakInFlowThread, RenderBox* breakChild, bool isBefore, LayoutUnit* offsetBreakAdjustment)
{
    // We take breaks into account for height computation for auto logical height regions
    // only in the layout phase in which we lay out the flows threads unconstrained
    // and we use the content breaks to determine the computed auto height for
    // auto logical height regions.
    if (!inMeasureContentLayoutPhase())
        return false;

    // Breaks can come before or after some objects. We need to track these objects, so that if we get
    // multiple breaks for the same object (for example because of multiple layouts on the same object),
    // we need to invalidate every other region after the old one and start computing from fresh.
    RenderBoxToRegionMap& mapToUse = isBefore ? m_breakBeforeToRegionMap : m_breakAfterToRegionMap;
    auto iter = mapToUse.find(breakChild);
    if (iter != mapToUse.end()) {
        auto regionIter = m_regionList.find(iter->value);
        ASSERT(regionIter != m_regionList.end());
        ASSERT((*regionIter)->hasAutoLogicalHeight());
        initializeRegionsComputedAutoHeight(*regionIter);

        // We need to update the regions flow thread portion rect because we are going to process
        // a break on these regions.
        updateRegionsFlowThreadPortionRect();
    }

    // Simulate a region break at offsetBreakInFlowThread. If it points inside an auto logical height region,
    // then it determines the region computed auto height.
    RenderRegion* region = regionAtBlockOffset(block, offsetBreakInFlowThread);
    if (!region)
        return false;

    bool lastBreakAfterContent = breakChild == this;
    bool hasComputedAutoHeight = false;

    LayoutUnit currentRegionOffsetInFlowThread = isHorizontalWritingMode() ? region->flowThreadPortionRect().y() : region->flowThreadPortionRect().x();
    LayoutUnit offsetBreakInCurrentRegion = offsetBreakInFlowThread - currentRegionOffsetInFlowThread;

    if (region->hasAutoLogicalHeight()) {
        RenderNamedFlowFragment* namedFlowFragment = toRenderNamedFlowFragment(region);

        // A forced break can appear only in an auto-height region that didn't have a forced break before.
        // This ASSERT is a good-enough heuristic to verify the above condition.
        ASSERT(namedFlowFragment->maxPageLogicalHeight() == namedFlowFragment->computedAutoHeight());

        mapToUse.set(breakChild, namedFlowFragment);

        hasComputedAutoHeight = true;

        // Compute the region height pretending that the offsetBreakInCurrentRegion is the logicalHeight for the auto-height region.
        LayoutUnit regionComputedAutoHeight = namedFlowFragment->constrainContentBoxLogicalHeightByMinMax(offsetBreakInCurrentRegion);

        // The new height of this region needs to be smaller than the initial value, the max height. A forced break is the only way to change the initial
        // height of an auto-height region besides content ending.
        ASSERT(regionComputedAutoHeight <= namedFlowFragment->maxPageLogicalHeight());

        namedFlowFragment->setComputedAutoHeight(regionComputedAutoHeight);

        currentRegionOffsetInFlowThread += regionComputedAutoHeight;
    } else
        currentRegionOffsetInFlowThread += isHorizontalWritingMode() ? region->flowThreadPortionRect().height() : region->flowThreadPortionRect().width();

    // If the break was found inside an auto-height region its size changed so we need to recompute the flow thread portion rectangles.
    // Also, if this is the last break after the content we need to clear the computedAutoHeight value on the last empty regions.
    if (hasAutoLogicalHeightRegions() && lastBreakAfterContent)
        updateRegionsFlowThreadPortionRect(region);
    else if (hasComputedAutoHeight)
        updateRegionsFlowThreadPortionRect();

    if (offsetBreakAdjustment)
        *offsetBreakAdjustment = std::max<LayoutUnit>(0, currentRegionOffsetInFlowThread - offsetBreakInFlowThread);

    return hasComputedAutoHeight;
}

void RenderFlowThread::incrementAutoLogicalHeightRegions()
{
    if (!m_autoLogicalHeightRegionsCount)
        view().flowThreadController().incrementFlowThreadsWithAutoLogicalHeightRegions();
    ++m_autoLogicalHeightRegionsCount;
}

void RenderFlowThread::decrementAutoLogicalHeightRegions()
{
    ASSERT(m_autoLogicalHeightRegionsCount > 0);
    --m_autoLogicalHeightRegionsCount;
    if (!m_autoLogicalHeightRegionsCount)
        view().flowThreadController().decrementFlowThreadsWithAutoLogicalHeightRegions();
}

void RenderFlowThread::collectLayerFragments(LayerFragments& layerFragments, const LayoutRect& layerBoundingBox, const LayoutRect& dirtyRect)
{
    ASSERT(!m_regionsInvalidated);
    
    for (auto iter = m_regionList.begin(), end = m_regionList.end(); iter != end; ++iter) {
        RenderRegion* region = *iter;
        region->collectLayerFragments(layerFragments, layerBoundingBox, dirtyRect);
    }
}

LayoutRect RenderFlowThread::fragmentsBoundingBox(const LayoutRect& layerBoundingBox)
{
    ASSERT(!m_regionsInvalidated);
    
    LayoutRect result;
    for (auto iter = m_regionList.begin(), end = m_regionList.end(); iter != end; ++iter) {
        RenderRegion* region = *iter;
        LayerFragments fragments;
        region->collectLayerFragments(fragments, layerBoundingBox, LayoutRect::infiniteRect());
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

bool RenderFlowThread::hasCachedOffsetFromLogicalTopOfFirstRegion(const RenderBox* box) const
{
    return m_boxesToOffsetMap.contains(box);
}

LayoutUnit RenderFlowThread::cachedOffsetFromLogicalTopOfFirstRegion(const RenderBox* box) const
{
    return m_boxesToOffsetMap.get(box);
}

void RenderFlowThread::setOffsetFromLogicalTopOfFirstRegion(const RenderBox* box, LayoutUnit offset)
{
    m_boxesToOffsetMap.set(box, offset);
}

void RenderFlowThread::clearOffsetFromLogicalTopOfFirstRegion(const RenderBox* box)
{
    ASSERT(m_boxesToOffsetMap.contains(box));
    m_boxesToOffsetMap.remove(box);
}

const RenderBox* RenderFlowThread::currentActiveRenderBox() const
{
    if (m_activeObjectsStack.isEmpty())
        return 0;

    const RenderObject* currentObject = m_activeObjectsStack.last();
    return currentObject->isBox() ? toRenderBox(currentObject) : 0;
}

void RenderFlowThread::pushFlowThreadLayoutState(const RenderObject& object)
{
    if (const RenderBox* currentBoxDescendant = currentActiveRenderBox()) {
        LayoutState* layoutState = currentBoxDescendant->view().layoutState();
        if (layoutState && layoutState->isPaginated()) {
            ASSERT(layoutState->m_renderer == currentBoxDescendant);
            LayoutSize offsetDelta = layoutState->m_layoutOffset - layoutState->m_pageOffset;
            setOffsetFromLogicalTopOfFirstRegion(currentBoxDescendant, currentBoxDescendant->isHorizontalWritingMode() ? offsetDelta.height() : offsetDelta.width());
        }
    }

    m_activeObjectsStack.add(&object);
}

void RenderFlowThread::popFlowThreadLayoutState()
{
    m_activeObjectsStack.removeLast();

    if (const RenderBox* currentBoxDescendant = currentActiveRenderBox()) {
        LayoutState* layoutState = currentBoxDescendant->view().layoutState();
        if (layoutState && layoutState->isPaginated())
            clearOffsetFromLogicalTopOfFirstRegion(currentBoxDescendant);
    }
}

LayoutUnit RenderFlowThread::offsetFromLogicalTopOfFirstRegion(const RenderBlock* currentBlock) const
{
    // First check if we cached the offset for the block if it's an ancestor containing block of the box
    // being currently laid out.
    if (hasCachedOffsetFromLogicalTopOfFirstRegion(currentBlock))
        return cachedOffsetFromLogicalTopOfFirstRegion(currentBlock);

    // If it's the current box being laid out, use the layout state.
    const RenderBox* currentBoxDescendant = currentActiveRenderBox();
    if (currentBlock == currentBoxDescendant) {
        LayoutState* layoutState = view().layoutState();
        ASSERT(layoutState->m_renderer == currentBlock);
        ASSERT(layoutState && layoutState->isPaginated());
        LayoutSize offsetDelta = layoutState->m_layoutOffset - layoutState->m_pageOffset;
        return currentBoxDescendant->isHorizontalWritingMode() ? offsetDelta.height() : offsetDelta.width();
    }

    // As a last resort, take the slow path.
    LayoutRect blockRect(0, 0, currentBlock->width(), currentBlock->height());
    while (currentBlock && !currentBlock->isRenderFlowThread()) {
        RenderBlock* containerBlock = currentBlock->containingBlock();
        ASSERT(containerBlock);
        if (!containerBlock)
            return 0;
        LayoutPoint currentBlockLocation = currentBlock->location();

        if (containerBlock->style().writingMode() != currentBlock->style().writingMode()) {
            // We have to put the block rect in container coordinates
            // and we have to take into account both the container and current block flipping modes
            if (containerBlock->style().isFlippedBlocksWritingMode()) {
                if (containerBlock->isHorizontalWritingMode())
                    blockRect.setY(currentBlock->height() - blockRect.maxY());
                else
                    blockRect.setX(currentBlock->width() - blockRect.maxX());
            }
            currentBlock->flipForWritingMode(blockRect);
        }
        blockRect.moveBy(currentBlockLocation);
        currentBlock = containerBlock;
    }

    return currentBlock->isHorizontalWritingMode() ? blockRect.y() : blockRect.x();
}

void RenderFlowThread::RegionSearchAdapter::collectIfNeeded(const RegionInterval& interval)
{
    if (m_result)
        return;
    if (interval.low() <= m_offset && interval.high() > m_offset)
        m_result = interval.data();
}

void RenderFlowThread::mapLocalToContainer(const RenderLayerModelObject* repaintContainer, TransformState& transformState, MapCoordinatesFlags mode, bool* wasFixed) const
{
    if (this == repaintContainer)
        return;

    if (RenderRegion* region = mapFromFlowToRegion(transformState))
        // FIXME: The cast below is probably not the best solution, we may need to find a better way.
        static_cast<const RenderObject*>(region)->mapLocalToContainer(region->containerForRepaint(), transformState, mode, wasFixed);
}

// FIXME: Make this function faster. Walking the render tree is slow, better use a caching mechanism (e.g. |cachedOffsetFromLogicalTopOfFirstRegion|).
LayoutRect RenderFlowThread::mapFromLocalToFlowThread(const RenderBox* box, const LayoutRect& localRect) const
{
    LayoutRect boxRect = localRect;

    while (box && box != this) {
        RenderBlock* containerBlock = box->containingBlock();
        ASSERT(containerBlock);
        if (!containerBlock)
            return LayoutRect();
        LayoutPoint currentBoxLocation = box->location();

        if (containerBlock->style().writingMode() != box->style().writingMode())
            box->flipForWritingMode(boxRect);

        boxRect.moveBy(currentBoxLocation);
        box = containerBlock;
    }

    return boxRect;
}

// FIXME: Make this function faster. Walking the render tree is slow, better use a caching mechanism (e.g. |cachedOffsetFromLogicalTopOfFirstRegion|).
LayoutRect RenderFlowThread::mapFromFlowThreadToLocal(const RenderBox* box, const LayoutRect& rect) const
{
    LayoutRect localRect = rect;
    if (box == this)
        return localRect;

    RenderBlock* containerBlock = box->containingBlock();
    ASSERT(containerBlock);
    if (!containerBlock)
        return LayoutRect();
    localRect = mapFromFlowThreadToLocal(containerBlock, localRect);

    LayoutPoint currentBoxLocation = box->location();
    localRect.moveBy(-currentBoxLocation);

    if (containerBlock->style().writingMode() != box->style().writingMode())
        box->flipForWritingMode(localRect);

    return localRect;
}

LayoutRect RenderFlowThread::decorationsClipRectForBoxInRegion(const RenderBox& box, RenderRegion& region) const
{
    LayoutRect visualOverflowRect = region.visualOverflowRectForBox(&box);
    LayoutUnit initialLogicalX = style().isHorizontalWritingMode() ? visualOverflowRect.x() : visualOverflowRect.y();

    // The visual overflow rect returned by visualOverflowRectForBox is already flipped but the
    // RenderRegion::rectFlowPortionForBox method expects it unflipped.
    flipForWritingModeLocalCoordinates(visualOverflowRect);
    visualOverflowRect = region.rectFlowPortionForBox(&box, visualOverflowRect);
    
    // Now flip it again.
    flipForWritingModeLocalCoordinates(visualOverflowRect);
    
    // Layers are in physical coordinates so the origin must be moved to the physical top-left of the flowthread.
    if (style().isFlippedBlocksWritingMode()) {
        if (style().isHorizontalWritingMode())
            visualOverflowRect.moveBy(LayoutPoint(0, height()));
        else
            visualOverflowRect.moveBy(LayoutPoint(width(), 0));
    }

    const RenderBox* iterBox = &box;
    while (iterBox && iterBox != this) {
        RenderBlock* containerBlock = iterBox->containingBlock();

        // FIXME: This doesn't work properly with flipped writing modes.
        // https://bugs.webkit.org/show_bug.cgi?id=125149
        if (iterBox->isPositioned()) {
            // For positioned elements, just use the layer's absolute bounding box.
            visualOverflowRect.moveBy(iterBox->layer()->absoluteBoundingBox().location());
            break;
        }

        LayoutRect currentBoxRect = iterBox->frameRect();
        if (iterBox->style().isFlippedBlocksWritingMode()) {
            if (iterBox->style().isHorizontalWritingMode())
                currentBoxRect.setY(currentBoxRect.height() - currentBoxRect.maxY());
            else
                currentBoxRect.setX(currentBoxRect.width() - currentBoxRect.maxX());
        }

        if (containerBlock->style().writingMode() != iterBox->style().writingMode())
            iterBox->flipForWritingMode(currentBoxRect);

        visualOverflowRect.moveBy(currentBoxRect.location());
        iterBox = containerBlock;
    }

    // Since the purpose of this method is to make sure the borders of a fragmented
    // element don't overflow the region in the fragmentation direction, there's no
    // point in restricting the clipping rect on the logical X axis. 
    // This also saves us the trouble of handling percent-based widths and margins
    // since the absolute bounding box of a positioned element would not contain
    // the correct coordinates relative to the region we're interested in, but rather
    // relative to the actual flow thread.
    if (style().isHorizontalWritingMode()) {
        if (initialLogicalX < visualOverflowRect.x())
            visualOverflowRect.shiftXEdgeTo(initialLogicalX);
        if (visualOverflowRect.width() < frameRect().width())
            visualOverflowRect.setWidth(frameRect().width());
    } else {
        if (initialLogicalX < visualOverflowRect.y())
            visualOverflowRect.shiftYEdgeTo(initialLogicalX);
        if (visualOverflowRect.height() < frameRect().height())
            visualOverflowRect.setHeight(frameRect().height());
    }

    return visualOverflowRect;
}

void RenderFlowThread::flipForWritingModeLocalCoordinates(LayoutRect& rect) const
{
    if (!style().isFlippedBlocksWritingMode())
        return;
    
    if (isHorizontalWritingMode())
        rect.setY(0 - rect.maxY());
    else
        rect.setX(0 - rect.maxX());
}

void RenderFlowThread::addRegionsVisualEffectOverflow(const RenderBox* box)
{
    RenderRegion* startRegion = 0;
    RenderRegion* endRegion = 0;
    getRegionRangeForBox(box, startRegion, endRegion);

    for (auto iter = m_regionList.find(startRegion), end = m_regionList.end(); iter != end; ++iter) {
        RenderRegion* region = *iter;

        LayoutRect borderBox = box->borderBoxRectInRegion(region);
        borderBox = box->applyVisualEffectOverflow(borderBox);
        borderBox = region->rectFlowPortionForBox(box, borderBox);

        region->addVisualOverflowForBox(box, borderBox);
        if (region == endRegion)
            break;
    }
}

void RenderFlowThread::addRegionsVisualOverflowFromTheme(const RenderBlock* block)
{
    RenderRegion* startRegion = 0;
    RenderRegion* endRegion = 0;
    getRegionRangeForBox(block, startRegion, endRegion);

    for (auto iter = m_regionList.find(startRegion), end = m_regionList.end(); iter != end; ++iter) {
        RenderRegion* region = *iter;

        LayoutRect borderBox = block->borderBoxRectInRegion(region);
        borderBox = region->rectFlowPortionForBox(block, borderBox);

        IntRect inflatedRect = pixelSnappedIntRect(borderBox);
        block->theme().adjustRepaintRect(block, inflatedRect);

        region->addVisualOverflowForBox(block, inflatedRect);
        if (region == endRegion)
            break;
    }
}

void RenderFlowThread::addRegionsOverflowFromChild(const RenderBox* box, const RenderBox* child, const LayoutSize& delta)
{
    RenderRegion* startRegion = 0;
    RenderRegion* endRegion = 0;
    getRegionRangeForBox(child, startRegion, endRegion);

    RenderRegion* containerStartRegion = 0;
    RenderRegion* containerEndRegion = 0;
    getRegionRangeForBox(box, containerStartRegion, containerEndRegion);

    for (auto iter = m_regionList.find(startRegion), end = m_regionList.end(); iter != end; ++iter) {
        RenderRegion* region = *iter;
        if (!regionInRange(region, containerStartRegion, containerEndRegion)) {
            if (region == endRegion)
                break;
            continue;
        }

        LayoutRect childLayoutOverflowRect = region->layoutOverflowRectForBoxForPropagation(child);
        childLayoutOverflowRect.move(delta);
        region->addLayoutOverflowForBox(box, childLayoutOverflowRect);

        if (child->hasSelfPaintingLayer() || box->hasOverflowClip()) {
            if (region == endRegion)
                break;
            continue;
        }
        LayoutRect childVisualOverflowRect = region->visualOverflowRectForBoxForPropagation(child);
        childVisualOverflowRect.move(delta);
        region->addVisualOverflowForBox(box, childVisualOverflowRect);

        if (region == endRegion)
            break;
    }
}
    
void RenderFlowThread::addRegionsLayoutOverflow(const RenderBox* box, const LayoutRect& layoutOverflow)
{
    RenderRegion* startRegion = 0;
    RenderRegion* endRegion = 0;
    getRegionRangeForBox(box, startRegion, endRegion);

    for (auto iter = m_regionList.find(startRegion), end = m_regionList.end(); iter != end; ++iter) {
        RenderRegion* region = *iter;
        LayoutRect layoutOverflowInRegion = region->rectFlowPortionForBox(box, layoutOverflow);

        region->addLayoutOverflowForBox(box, layoutOverflowInRegion);

        if (region == endRegion)
            break;
    }
}

void RenderFlowThread::addRegionsVisualOverflow(const RenderBox* box, const LayoutRect& visualOverflow)
{
    RenderRegion* startRegion = 0;
    RenderRegion* endRegion = 0;
    getRegionRangeForBox(box, startRegion, endRegion);
    
    for (RenderRegionList::iterator iter = m_regionList.find(startRegion); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        LayoutRect visualOverflowInRegion = region->rectFlowPortionForBox(box, visualOverflow);
        
        region->addVisualOverflowForBox(box, visualOverflowInRegion);
        
        if (region == endRegion)
            break;
    }
}

void RenderFlowThread::clearRegionsOverflow(const RenderBox* box)
{
    RenderRegion* startRegion = 0;
    RenderRegion* endRegion = 0;
    getRegionRangeForBox(box, startRegion, endRegion);

    for (auto iter = m_regionList.find(startRegion), end = m_regionList.end(); iter != end; ++iter) {
        RenderRegion* region = *iter;
        RenderBoxRegionInfo* boxInfo = region->renderBoxRegionInfo(box);
        if (boxInfo && boxInfo->overflow())
            boxInfo->clearOverflow();

        if (region == endRegion)
            break;
    }
}

CurrentRenderFlowThreadMaintainer::CurrentRenderFlowThreadMaintainer(RenderFlowThread* renderFlowThread)
    : m_renderFlowThread(renderFlowThread)
    , m_previousRenderFlowThread(0)
{
    if (!m_renderFlowThread)
        return;
    FlowThreadController& controller = m_renderFlowThread->view().flowThreadController();
    m_previousRenderFlowThread = controller.currentRenderFlowThread();
    // Remove the assert so we can use this to change the flow thread context.
    // ASSERT(!m_previousRenderFlowThread || !renderFlowThread->isRenderNamedFlowThread());
    controller.setCurrentRenderFlowThread(m_renderFlowThread);
}

CurrentRenderFlowThreadMaintainer::~CurrentRenderFlowThreadMaintainer()
{
    if (!m_renderFlowThread)
        return;
    FlowThreadController& controller = m_renderFlowThread->view().flowThreadController();
    ASSERT(controller.currentRenderFlowThread() == m_renderFlowThread);
    controller.setCurrentRenderFlowThread(m_previousRenderFlowThread);
}


} // namespace WebCore
