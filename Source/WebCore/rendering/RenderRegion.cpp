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
#include "RenderRegion.h"

#include "FlowThreadController.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "IntRect.h"
#include "LayoutRepainter.h"
#include "PaintInfo.h"
#include "Range.h"
#include "RenderBoxRegionInfo.h"
#include "RenderIterator.h"
#include "RenderLayer.h"
#include "RenderNamedFlowFragment.h"
#include "RenderNamedFlowThread.h"
#include "RenderView.h"
#include "StyleResolver.h"
#include <wtf/StackStats.h>

namespace WebCore {

RenderRegion::RenderRegion(Element& element, PassRef<RenderStyle> style, RenderFlowThread* flowThread)
    : RenderBlockFlow(element, std::move(style))
    , m_flowThread(flowThread)
    , m_parentNamedFlowThread(0)
    , m_isValid(false)
    , m_hasAutoLogicalHeight(false)
    , m_hasComputedAutoHeight(false)
    , m_computedAutoHeight(0)
{
}

RenderRegion::RenderRegion(Document& document, PassRef<RenderStyle> style, RenderFlowThread* flowThread)
    : RenderBlockFlow(document, std::move(style))
    , m_flowThread(flowThread)
    , m_parentNamedFlowThread(0)
    , m_isValid(false)
    , m_hasAutoLogicalHeight(false)
    , m_hasComputedAutoHeight(false)
    , m_computedAutoHeight(0)
{
}

LayoutPoint RenderRegion::mapRegionPointIntoFlowThreadCoordinates(const LayoutPoint& point)
{
    // Assuming the point is relative to the region block, 3 cases will be considered:
    // a) top margin, padding or border.
    // b) bottom margin, padding or border.
    // c) non-content region area.

    LayoutUnit pointLogicalTop(isHorizontalWritingMode() ? point.y() : point.x());
    LayoutUnit pointLogicalLeft(isHorizontalWritingMode() ? point.x() : point.y());
    LayoutUnit flowThreadLogicalTop(isHorizontalWritingMode() ? m_flowThreadPortionRect.y() : m_flowThreadPortionRect.x());
    LayoutUnit flowThreadLogicalLeft(isHorizontalWritingMode() ? m_flowThreadPortionRect.x() : m_flowThreadPortionRect.y());
    LayoutUnit flowThreadPortionTopBound(isHorizontalWritingMode() ? m_flowThreadPortionRect.height() : m_flowThreadPortionRect.width());
    LayoutUnit flowThreadPortionLeftBound(isHorizontalWritingMode() ? m_flowThreadPortionRect.width() : m_flowThreadPortionRect.height());
    LayoutUnit flowThreadPortionTopMax(isHorizontalWritingMode() ? m_flowThreadPortionRect.maxY() : m_flowThreadPortionRect.maxX());
    LayoutUnit flowThreadPortionLeftMax(isHorizontalWritingMode() ? m_flowThreadPortionRect.maxX() : m_flowThreadPortionRect.maxY());
    LayoutUnit effectiveFixedPointDenominator;
    effectiveFixedPointDenominator.setRawValue(1);

    if (pointLogicalTop < 0) {
        LayoutPoint pointInThread(0, flowThreadLogicalTop);
        return isHorizontalWritingMode() ? pointInThread : pointInThread.transposedPoint();
    }

    if (pointLogicalTop >= flowThreadPortionTopBound) {
        LayoutPoint pointInThread(flowThreadPortionLeftBound, flowThreadPortionTopMax - effectiveFixedPointDenominator);
        return isHorizontalWritingMode() ? pointInThread : pointInThread.transposedPoint();
    }

    if (pointLogicalLeft < 0) {
        LayoutPoint pointInThread(flowThreadLogicalLeft, pointLogicalTop + flowThreadLogicalTop);
        return isHorizontalWritingMode() ? pointInThread : pointInThread.transposedPoint();
    }
    if (pointLogicalLeft >= flowThreadPortionLeftBound) {
        LayoutPoint pointInThread(flowThreadPortionLeftMax - effectiveFixedPointDenominator, pointLogicalTop + flowThreadLogicalTop);
        return isHorizontalWritingMode() ? pointInThread : pointInThread.transposedPoint();
    }
    LayoutPoint pointInThread(pointLogicalLeft + flowThreadLogicalLeft, pointLogicalTop + flowThreadLogicalTop);
    return isHorizontalWritingMode() ? pointInThread : pointInThread.transposedPoint();
}

VisiblePosition RenderRegion::positionForPoint(const LayoutPoint& point)
{
    if (!m_flowThread->firstChild()) // checking for empty region blocks.
        return RenderBlock::positionForPoint(point);

    return toRenderBlock(m_flowThread->firstChild())->positionForPoint(mapRegionPointIntoFlowThreadCoordinates(point));
}

LayoutUnit RenderRegion::pageLogicalWidth() const
{
    ASSERT(m_flowThread);
    return m_flowThread->isHorizontalWritingMode() ? contentWidth() : contentHeight();
}

LayoutUnit RenderRegion::pageLogicalHeight() const
{
    ASSERT(m_flowThread);
    if (hasComputedAutoHeight() && m_flowThread->inMeasureContentLayoutPhase()) {
        ASSERT(hasAutoLogicalHeight());
        return computedAutoHeight();
    }
    return m_flowThread->isHorizontalWritingMode() ? contentHeight() : contentWidth();
}

// This method returns the maximum page size of a region with auto-height. This is the initial
// height value for auto-height regions in the first layout phase of the parent named flow.
LayoutUnit RenderRegion::maxPageLogicalHeight() const
{
    ASSERT(m_flowThread);
    ASSERT(hasAutoLogicalHeight() && m_flowThread->inMeasureContentLayoutPhase());
    return style().logicalMaxHeight().isUndefined() ? RenderFlowThread::maxLogicalHeight() : computeReplacedLogicalHeightUsing(style().logicalMaxHeight());
}

LayoutUnit RenderRegion::logicalHeightOfAllFlowThreadContent() const
{
    ASSERT(m_flowThread);
    if (hasComputedAutoHeight() && m_flowThread->inMeasureContentLayoutPhase()) {
        ASSERT(hasAutoLogicalHeight());
        return computedAutoHeight();
    }
    return m_flowThread->isHorizontalWritingMode() ? contentHeight() : contentWidth();
}

LayoutRect RenderRegion::flowThreadPortionOverflowRect()
{
    return overflowRectForFlowThreadPortion(flowThreadPortionRect(), isFirstRegion(), isLastRegion(), VisualOverflow);
}

LayoutPoint RenderRegion::flowThreadPortionLocation() const
{
    LayoutPoint portionLocation;
    LayoutRect portionRect = flowThreadPortionRect();

    if (flowThread()->style().isFlippedBlocksWritingMode()) {
        LayoutRect flippedFlowThreadPortionRect(portionRect);
        flowThread()->flipForWritingMode(flippedFlowThreadPortionRect);
        portionLocation = flippedFlowThreadPortionRect.location();
    } else
        portionLocation = portionRect.location();

    return portionLocation;
}

RenderLayer* RenderRegion::regionContainerLayer() const
{
    ASSERT(parent() && parent()->isRenderNamedFlowFragmentContainer());
    return toRenderBlockFlow(parent())->layer();
}

LayoutRect RenderRegion::overflowRectForFlowThreadPortion(const LayoutRect& flowThreadPortionRect, bool isFirstPortion, bool isLastPortion, OverflowType overflowType)
{
    ASSERT(isValid());

    bool isLastRegionWithRegionFragmentBreak = (isLastPortion && (style().regionFragment() == BreakRegionFragment));
    if (hasOverflowClip() || isLastRegionWithRegionFragmentBreak)
        return flowThreadPortionRect;

    LayoutRect flowThreadOverflow = overflowType == VisualOverflow ? visualOverflowRectForBox(m_flowThread) : layoutOverflowRectForBox(m_flowThread);

    // We are interested about the outline size only when computing the visual overflow.
    LayoutUnit outlineSize = overflowType == VisualOverflow ? LayoutUnit(maximalOutlineSize(PaintPhaseOutline)) : LayoutUnit();
    LayoutRect clipRect;
    if (m_flowThread->isHorizontalWritingMode()) {
        LayoutUnit minY = isFirstPortion ? (flowThreadOverflow.y() - outlineSize) : flowThreadPortionRect.y();
        LayoutUnit maxY = isLastPortion ? std::max(flowThreadPortionRect.maxY(), flowThreadOverflow.maxY()) + outlineSize : flowThreadPortionRect.maxY();
        bool clipX = style().overflowX() != OVISIBLE;
        LayoutUnit minX = clipX ? flowThreadPortionRect.x() : std::min(flowThreadPortionRect.x(), flowThreadOverflow.x() - outlineSize);
        LayoutUnit maxX = clipX ? flowThreadPortionRect.maxX() : std::max(flowThreadPortionRect.maxX(), (flowThreadOverflow.maxX() + outlineSize));
        clipRect = LayoutRect(minX, minY, maxX - minX, maxY - minY);
    } else {
        LayoutUnit minX = isFirstPortion ? (flowThreadOverflow.x() - outlineSize) : flowThreadPortionRect.x();
        LayoutUnit maxX = isLastPortion ? std::max(flowThreadPortionRect.maxX(), flowThreadOverflow.maxX()) + outlineSize : flowThreadPortionRect.maxX();
        bool clipY = style().overflowY() != OVISIBLE;
        LayoutUnit minY = clipY ? flowThreadPortionRect.y() : std::min(flowThreadPortionRect.y(), (flowThreadOverflow.y() - outlineSize));
        LayoutUnit maxY = clipY ? flowThreadPortionRect.maxY() : std::max(flowThreadPortionRect.y(), (flowThreadOverflow.maxY() + outlineSize));
        clipRect = LayoutRect(minX, minY, maxX - minX, maxY - minY);
    }

    return clipRect;
}

RegionOversetState RenderRegion::regionOversetState() const
{
    ASSERT(generatingElement());

    if (!isValid())
        return RegionUndefined;

    return generatingElement()->regionOversetState();
}

void RenderRegion::setRegionOversetState(RegionOversetState state)
{
    ASSERT(generatingElement());

    generatingElement()->setRegionOversetState(state);
}

LayoutUnit RenderRegion::pageLogicalTopForOffset(LayoutUnit /* offset */) const
{
    return flowThread()->isHorizontalWritingMode() ? flowThreadPortionRect().y() : flowThreadPortionRect().x();
}

bool RenderRegion::isFirstRegion() const
{
    ASSERT(isValid());

    return m_flowThread->firstRegion() == this;
}

bool RenderRegion::isLastRegion() const
{
    ASSERT(isValid());

    return m_flowThread->lastRegion() == this;
}

void RenderRegion::incrementAutoLogicalHeightCount()
{
    ASSERT(isValid());
    ASSERT(m_hasAutoLogicalHeight);

    m_flowThread->incrementAutoLogicalHeightRegions();
}

void RenderRegion::decrementAutoLogicalHeightCount()
{
    ASSERT(isValid());

    m_flowThread->decrementAutoLogicalHeightRegions();
}

void RenderRegion::updateRegionHasAutoLogicalHeightFlag()
{
    ASSERT(m_flowThread);

    if (!isValid())
        return;

    bool didHaveAutoLogicalHeight = m_hasAutoLogicalHeight;
    m_hasAutoLogicalHeight = shouldHaveAutoLogicalHeight();
    if (m_hasAutoLogicalHeight != didHaveAutoLogicalHeight) {
        if (m_hasAutoLogicalHeight)
            incrementAutoLogicalHeightCount();
        else {
            clearComputedAutoHeight();
            decrementAutoLogicalHeightCount();
        }
    }
}

bool RenderRegion::shouldHaveAutoLogicalHeight() const
{
    bool hasSpecifiedEndpointsForHeight = style().logicalTop().isSpecified() && style().logicalBottom().isSpecified();
    bool hasAnchoredEndpointsForHeight = isOutOfFlowPositioned() && hasSpecifiedEndpointsForHeight;
    return style().logicalHeight().isAuto() && !hasAnchoredEndpointsForHeight;
}
    
void RenderRegion::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlockFlow::styleDidChange(diff, oldStyle);

    if (!m_flowThread)
        return;

    updateRegionHasAutoLogicalHeightFlag();

    if (oldStyle && oldStyle->writingMode() != style().writingMode())
        m_flowThread->regionChangedWritingMode(this);
}

void RenderRegion::layoutBlock(bool relayoutChildren, LayoutUnit)
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    RenderBlockFlow::layoutBlock(relayoutChildren);

    if (isValid()) {
        LayoutRect oldRegionRect(flowThreadPortionRect());
        if (!isHorizontalWritingMode())
            oldRegionRect = oldRegionRect.transposedRect();

        if (m_flowThread->inOverflowLayoutPhase() || m_flowThread->inFinalLayoutPhase())
            computeOverflowFromFlowThread();

        if (hasAutoLogicalHeight() && m_flowThread->inMeasureContentLayoutPhase()) {
            m_flowThread->invalidateRegions();
            clearComputedAutoHeight();
            return;
        }

        if (!isRenderRegionSet() && (oldRegionRect.width() != pageLogicalWidth() || oldRegionRect.height() != pageLogicalHeight()) && !m_flowThread->inFinalLayoutPhase())
            // This can happen even if we are in the inConstrainedLayoutPhase and it will trigger a pathological layout of the flow thread.
            m_flowThread->invalidateRegions();
    }
}

void RenderRegion::computeOverflowFromFlowThread()
{
    ASSERT(isValid());
    
    LayoutRect layoutRect;
    {
        // When getting the overflow from the flow thread we need to temporarly reset the current flow thread because
        // we're changing flows.
        CurrentRenderFlowThreadMaintainer flowThreadMaintainer(m_flowThread);
        layoutRect = layoutOverflowRectForBox(m_flowThread);
    }

    layoutRect.setLocation(contentBoxRect().location() + (layoutRect.location() - m_flowThreadPortionRect.location()));

    // FIXME: Correctly adjust the layout overflow for writing modes.
    addLayoutOverflow(layoutRect);
    RenderFlowThread* enclosingRenderFlowThread = flowThreadContainingBlock();
    if (enclosingRenderFlowThread)
        enclosingRenderFlowThread->addRegionsLayoutOverflow(this, layoutRect);

    updateLayerTransform();
    updateScrollInfoAfterLayout();
}

void RenderRegion::repaintFlowThreadContent(const LayoutRect& repaintRect, bool immediate)
{
    repaintFlowThreadContentRectangle(repaintRect, immediate, flowThreadPortionRect(), contentBoxRect().location());
}

void RenderRegion::repaintFlowThreadContentRectangle(const LayoutRect& repaintRect, bool immediate, const LayoutRect& flowThreadPortionRect, const LayoutPoint& regionLocation, const LayoutRect* flowThreadPortionClipRect)
{
    ASSERT(isValid());

    // We only have to issue a repaint in this region if the region rect intersects the repaint rect.
    LayoutRect clippedRect(repaintRect);

    if (flowThreadPortionClipRect) {
        LayoutRect flippedFlowThreadPortionClipRect(*flowThreadPortionClipRect);
        flowThread()->flipForWritingMode(flippedFlowThreadPortionClipRect);
        clippedRect.intersect(flippedFlowThreadPortionClipRect);
    }

    if (clippedRect.isEmpty())
        return;

    LayoutRect flippedFlowThreadPortionRect(flowThreadPortionRect);
    flowThread()->flipForWritingMode(flippedFlowThreadPortionRect); // Put the region rects into physical coordinates.

    // Put the region rect into the region's physical coordinate space.
    clippedRect.setLocation(regionLocation + (clippedRect.location() - flippedFlowThreadPortionRect.location()));

    // Now switch to the region's writing mode coordinate space and let it repaint itself.
    flipForWritingMode(clippedRect);
    
    // Issue the repaint.
    repaintRectangle(clippedRect, immediate);
}

void RenderRegion::installFlowThread()
{
    m_flowThread = &view().flowThreadController().ensureRenderFlowThreadWithName(style().regionThread());

    // By now the flow thread should already be added to the rendering tree,
    // so we go up the rendering parents and check that this region is not part of the same
    // flow that it actually needs to display. It would create a circular reference.

    auto closestFlowThreadAncestor = ancestorsOfType<RenderNamedFlowThread>(*this).first();
    if (!closestFlowThreadAncestor) {
        m_parentNamedFlowThread = nullptr;
        return;
    }

    m_parentNamedFlowThread = &*closestFlowThreadAncestor;

    // Do not take into account a region that links a flow with itself. The dependency
    // cannot change, so it is not worth adding it to the list.
    if (m_flowThread == m_parentNamedFlowThread)
        m_flowThread = nullptr;
}

void RenderRegion::attachRegion()
{
    if (documentBeingDestroyed())
        return;
    
    // A region starts off invalid.
    setIsValid(false);

    // Initialize the flow thread reference and create the flow thread object if needed.
    // The flow thread lifetime is influenced by the number of regions attached to it,
    // and we are attaching the region to the flow thread.
    installFlowThread();
    
    if (!m_flowThread)
        return;

    // Only after adding the region to the thread, the region is marked to be valid.
    m_flowThread->addRegionToThread(this);

    // The region just got attached to the flow thread, lets check whether
    // it has region styling rules associated.
    // FIXME: Remove cast once attachRegion is moved to RenderNamedFlowFragment.
    if (isRenderNamedFlowFragment())
        toRenderNamedFlowFragment(this)->checkRegionStyle();

    if (!isValid())
        return;

    m_hasAutoLogicalHeight = shouldHaveAutoLogicalHeight();
    if (hasAutoLogicalHeight())
        incrementAutoLogicalHeightCount();
}

void RenderRegion::detachRegion()
{
    if (m_flowThread) {
        m_flowThread->removeRegionFromThread(this);
        if (hasAutoLogicalHeight())
            decrementAutoLogicalHeightCount();
    }
    m_flowThread = 0;
}

RenderBoxRegionInfo* RenderRegion::renderBoxRegionInfo(const RenderBox* box) const
{
    ASSERT(isValid());
    return m_renderBoxRegionInfo.get(box);
}

RenderBoxRegionInfo* RenderRegion::setRenderBoxRegionInfo(const RenderBox* box, LayoutUnit logicalLeftInset, LayoutUnit logicalRightInset,
    bool containingBlockChainIsInset)
{
    ASSERT(isValid());

    OwnPtr<RenderBoxRegionInfo>& boxInfo = m_renderBoxRegionInfo.add(box, adoptPtr(new RenderBoxRegionInfo(logicalLeftInset, logicalRightInset, containingBlockChainIsInset))).iterator->value;
    return boxInfo.get();
}

OwnPtr<RenderBoxRegionInfo> RenderRegion::takeRenderBoxRegionInfo(const RenderBox* box)
{
    return m_renderBoxRegionInfo.take(box);
}

void RenderRegion::removeRenderBoxRegionInfo(const RenderBox* box)
{
    m_renderBoxRegionInfo.remove(box);
}

void RenderRegion::deleteAllRenderBoxRegionInfo()
{
    m_renderBoxRegionInfo.clear();
}

LayoutUnit RenderRegion::logicalTopOfFlowThreadContentRect(const LayoutRect& rect) const
{
    ASSERT(isValid());
    return flowThread()->isHorizontalWritingMode() ? rect.y() : rect.x();
}

LayoutUnit RenderRegion::logicalBottomOfFlowThreadContentRect(const LayoutRect& rect) const
{
    ASSERT(isValid());
    return flowThread()->isHorizontalWritingMode() ? rect.maxY() : rect.maxX();
}

void RenderRegion::insertedIntoTree()
{
    attachRegion();
    if (isValid())
        RenderBlockFlow::insertedIntoTree();
}

void RenderRegion::willBeRemovedFromTree()
{
    RenderBlockFlow::willBeRemovedFromTree();

    detachRegion();
}

void RenderRegion::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    if (!isValid()) {
        RenderBlockFlow::computeIntrinsicLogicalWidths(minLogicalWidth, maxLogicalWidth);
        return;
    }

    minLogicalWidth = m_flowThread->minPreferredLogicalWidth();
    maxLogicalWidth = m_flowThread->maxPreferredLogicalWidth();
}

void RenderRegion::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    if (!isValid()) {
        RenderBlockFlow::computePreferredLogicalWidths();
        return;
    }

    // FIXME: Currently, the code handles only the <length> case for min-width/max-width.
    // It should also support other values, like percentage, calc or viewport relative.
    m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = 0;

    const RenderStyle& styleToUse = style();
    if (styleToUse.logicalWidth().isFixed() && styleToUse.logicalWidth().value() > 0)
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = adjustContentBoxLogicalWidthForBoxSizing(styleToUse.logicalWidth().value());
    else
        computeIntrinsicLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);

    if (styleToUse.logicalMinWidth().isFixed() && styleToUse.logicalMinWidth().value() > 0) {
        m_maxPreferredLogicalWidth = std::max(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse.logicalMinWidth().value()));
        m_minPreferredLogicalWidth = std::max(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse.logicalMinWidth().value()));
    }

    if (styleToUse.logicalMaxWidth().isFixed()) {
        m_maxPreferredLogicalWidth = std::min(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse.logicalMaxWidth().value()));
        m_minPreferredLogicalWidth = std::min(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse.logicalMaxWidth().value()));
    }

    LayoutUnit borderAndPadding = borderAndPaddingLogicalWidth();
    m_minPreferredLogicalWidth += borderAndPadding;
    m_maxPreferredLogicalWidth += borderAndPadding;
    setPreferredLogicalWidthsDirty(false);
}

void RenderRegion::getRanges(Vector<RefPtr<Range>>& rangeObjects) const
{
    const RenderNamedFlowThread& namedFlow = view().flowThreadController().ensureRenderFlowThreadWithName(style().regionThread());
    namedFlow.getRanges(rangeObjects, this);
}

void RenderRegion::updateLogicalHeight()
{
    RenderBlockFlow::updateLogicalHeight();

    if (!hasAutoLogicalHeight())
        return;

    // We want to update the logical height based on the computed auto-height
    // only after the measure cotnent layout phase when all the
    // auto logical height regions have a computed auto-height.
    if (m_flowThread->inMeasureContentLayoutPhase())
        return;

    // There may be regions with auto logical height that during the prerequisite layout phase
    // did not have the chance to layout flow thread content. Because of that, these regions do not
    // have a computedAutoHeight and they will not be able to fragment any flow
    // thread content.
    if (!hasComputedAutoHeight())
        return;

    LayoutUnit newLogicalHeight = computedAutoHeight() + borderAndPaddingLogicalHeight();
    ASSERT(newLogicalHeight < LayoutUnit::max() / 2);
    if (newLogicalHeight > logicalHeight()) {
        setLogicalHeight(newLogicalHeight);
        // Recalculate position of the render block after new logical height is set.
        // (needed in absolute positioning case with bottom alignment for example)
        RenderBlockFlow::updateLogicalHeight();
    }
}

void RenderRegion::adjustRegionBoundsFromFlowThreadPortionRect(const IntPoint& layerOffset, IntRect& regionBounds)
{
    LayoutRect flippedFlowThreadPortionRect = flowThreadPortionRect();
    flowThread()->flipForWritingMode(flippedFlowThreadPortionRect);
    regionBounds.moveBy(roundedIntPoint(flippedFlowThreadPortionRect.location()));

    UNUSED_PARAM(layerOffset);
}

void RenderRegion::ensureOverflowForBox(const RenderBox* box, RefPtr<RenderOverflow>& overflow, bool forceCreation)
{
    RenderFlowThread* flowThread = this->flowThread();
    ASSERT(flowThread);
    
    RenderBoxRegionInfo* boxInfo = renderBoxRegionInfo(box);
    if (!boxInfo && !forceCreation)
        return;

    if (boxInfo && boxInfo->overflow()) {
        overflow = boxInfo->overflow();
        return;
    }
    
    LayoutRect borderBox = box->borderBoxRectInRegion(this);
    LayoutRect clientBox;
    ASSERT(flowThread->objectShouldPaintInFlowRegion(box, this));

    if (!borderBox.isEmpty()) {
        borderBox = rectFlowPortionForBox(box, borderBox);
        
        clientBox = box->clientBoxRectInRegion(this);
        clientBox = rectFlowPortionForBox(box, clientBox);
        
        flowThread->flipForWritingModeLocalCoordinates(borderBox);
        flowThread->flipForWritingModeLocalCoordinates(clientBox);
    }

    if (boxInfo) {
        boxInfo->createOverflow(clientBox, borderBox);
        overflow = boxInfo->overflow();
    } else
        overflow = adoptRef(new RenderOverflow(clientBox, borderBox));
}

LayoutRect RenderRegion::rectFlowPortionForBox(const RenderBox* box, const LayoutRect& rect) const
{
    RenderRegion* startRegion = 0;
    RenderRegion* endRegion = 0;
    m_flowThread->getRegionRangeForBox(box, startRegion, endRegion);

    LayoutRect mappedRect = m_flowThread->mapFromLocalToFlowThread(box, rect);
    if (flowThread()->isHorizontalWritingMode()) {
        if (this != startRegion)
            mappedRect.shiftYEdgeTo(std::max<LayoutUnit>(logicalTopForFlowThreadContent(), mappedRect.y()));

        if (this != endRegion)
            mappedRect.setHeight(std::max<LayoutUnit>(0, std::min<LayoutUnit>(logicalBottomForFlowThreadContent() - mappedRect.y(), mappedRect.height())));
    } else {
        if (this != startRegion)
            mappedRect.shiftXEdgeTo(std::max<LayoutUnit>(logicalTopForFlowThreadContent(), mappedRect.x()));
            
        if (this != endRegion)
            mappedRect.setWidth(std::max<LayoutUnit>(0, std::min<LayoutUnit>(logicalBottomForFlowThreadContent() - mappedRect.x(), mappedRect.width())));
    }

    bool isLastRegionWithRegionFragmentBreak = (isLastRegion() && (style().regionFragment() == BreakRegionFragment));
    if (hasOverflowClip() || isLastRegionWithRegionFragmentBreak)
        mappedRect.intersect(flowThreadPortionRect());

    return mappedRect.isEmpty() ? mappedRect : m_flowThread->mapFromFlowThreadToLocal(box, mappedRect);
}

void RenderRegion::addLayoutOverflowForBox(const RenderBox* box, const LayoutRect& rect)
{
    if (rect.isEmpty())
        return;

    RefPtr<RenderOverflow> regionOverflow;
    ensureOverflowForBox(box, regionOverflow, false);

    if (!regionOverflow)
        return;

    regionOverflow->addLayoutOverflow(rect);
}

void RenderRegion::addVisualOverflowForBox(const RenderBox* box, const LayoutRect& rect)
{
    if (rect.isEmpty())
        return;

    RefPtr<RenderOverflow> regionOverflow;
    ensureOverflowForBox(box, regionOverflow, false);

    if (!regionOverflow)
        return;

    LayoutRect flippedRect = rect;
    flowThread()->flipForWritingModeLocalCoordinates(flippedRect);
    regionOverflow->addVisualOverflow(flippedRect);
}

LayoutRect RenderRegion::layoutOverflowRectForBox(const RenderBox* box)
{
    RefPtr<RenderOverflow> overflow;
    ensureOverflowForBox(box, overflow, true);
    
    ASSERT(overflow);
    return overflow->layoutOverflowRect();
}

LayoutRect RenderRegion::visualOverflowRectForBox(const RenderBox* box)
{
    RefPtr<RenderOverflow> overflow;
    ensureOverflowForBox(box, overflow, true);
    
    ASSERT(overflow);
    return overflow->visualOverflowRect();
}

// FIXME: This doesn't work for writing modes.
LayoutRect RenderRegion::layoutOverflowRectForBoxForPropagation(const RenderBox* box)
{
    // Only propagate interior layout overflow if we don't clip it.
    LayoutRect rect = box->borderBoxRectInRegion(this);
    rect = rectFlowPortionForBox(box, rect);
    if (!box->hasOverflowClip())
        rect.unite(layoutOverflowRectForBox(box));

    bool hasTransform = box->hasLayer() && box->layer()->transform();
    if (box->isInFlowPositioned() || hasTransform) {
        if (hasTransform)
            rect = box->layer()->currentTransform().mapRect(rect);

        if (box->isInFlowPositioned())
            rect.move(box->offsetForInFlowPosition());
    }

    return rect;
}

LayoutRect RenderRegion::visualOverflowRectForBoxForPropagation(const RenderBox* box)
{
    LayoutRect rect = visualOverflowRectForBox(box);
    flowThread()->flipForWritingModeLocalCoordinates(rect);

    return rect;
}

} // namespace WebCore
