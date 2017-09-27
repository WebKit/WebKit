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
#include "RenderFragmentContainer.h"

#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "IntRect.h"
#include "LayoutRepainter.h"
#include "Range.h"
#include "RenderBoxFragmentInfo.h"
#include "RenderFlowThread.h"
#include "RenderInline.h"
#include "RenderIterator.h"
#include "RenderLayer.h"
#include "RenderView.h"
#include "StyleResolver.h"

namespace WebCore {

RenderFragmentContainer::RenderFragmentContainer(Element& element, RenderStyle&& style, RenderFlowThread* flowThread)
    : RenderBlockFlow(element, WTFMove(style))
    , m_flowThread(flowThread)
    , m_isValid(false)
{
}

RenderFragmentContainer::RenderFragmentContainer(Document& document, RenderStyle&& style, RenderFlowThread* flowThread)
    : RenderBlockFlow(document, WTFMove(style))
    , m_flowThread(flowThread)
    , m_isValid(false)
{
}

LayoutPoint RenderFragmentContainer::mapFragmentPointIntoFlowThreadCoordinates(const LayoutPoint& point)
{
    // Assuming the point is relative to the fragment block, 3 cases will be considered:
    // a) top margin, padding or border.
    // b) bottom margin, padding or border.
    // c) non-content fragment area.

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

VisiblePosition RenderFragmentContainer::positionForPoint(const LayoutPoint& point, const RenderFragmentContainer* fragment)
{
    if (!isValid() || !m_flowThread->firstChild()) // checking for empty fragment blocks.
        return RenderBlock::positionForPoint(point, fragment);

    return m_flowThread->positionForPoint(mapFragmentPointIntoFlowThreadCoordinates(point), this);
}

LayoutUnit RenderFragmentContainer::pageLogicalWidth() const
{
    ASSERT(isValid());
    return m_flowThread->isHorizontalWritingMode() ? contentWidth() : contentHeight();
}

LayoutUnit RenderFragmentContainer::pageLogicalHeight() const
{
    ASSERT(isValid());
    return m_flowThread->isHorizontalWritingMode() ? contentHeight() : contentWidth();
}

LayoutUnit RenderFragmentContainer::logicalHeightOfAllFlowThreadContent() const
{
    return pageLogicalHeight();
}

LayoutRect RenderFragmentContainer::flowThreadPortionOverflowRect()
{
    return overflowRectForFlowThreadPortion(flowThreadPortionRect(), isFirstFragment(), isLastFragment(), VisualOverflow);
}

LayoutPoint RenderFragmentContainer::flowThreadPortionLocation() const
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

LayoutRect RenderFragmentContainer::overflowRectForFlowThreadPortion(const LayoutRect& flowThreadPortionRect, bool isFirstPortion, bool isLastPortion, OverflowType overflowType)
{
    ASSERT(isValid());
    if (shouldClipFlowThreadContent())
        return flowThreadPortionRect;

    LayoutRect flowThreadOverflow = overflowType == VisualOverflow ? visualOverflowRectForBox(*m_flowThread) : layoutOverflowRectForBox(m_flowThread);
    LayoutRect clipRect;
    if (m_flowThread->isHorizontalWritingMode()) {
        LayoutUnit minY = isFirstPortion ? flowThreadOverflow.y() : flowThreadPortionRect.y();
        LayoutUnit maxY = isLastPortion ? std::max(flowThreadPortionRect.maxY(), flowThreadOverflow.maxY()) : flowThreadPortionRect.maxY();
        bool clipX = style().overflowX() != OVISIBLE;
        LayoutUnit minX = clipX ? flowThreadPortionRect.x() : std::min(flowThreadPortionRect.x(), flowThreadOverflow.x());
        LayoutUnit maxX = clipX ? flowThreadPortionRect.maxX() : std::max(flowThreadPortionRect.maxX(), flowThreadOverflow.maxX());
        clipRect = LayoutRect(minX, minY, maxX - minX, maxY - minY);
    } else {
        LayoutUnit minX = isFirstPortion ? flowThreadOverflow.x() : flowThreadPortionRect.x();
        LayoutUnit maxX = isLastPortion ? std::max(flowThreadPortionRect.maxX(), flowThreadOverflow.maxX()) : flowThreadPortionRect.maxX();
        bool clipY = style().overflowY() != OVISIBLE;
        LayoutUnit minY = clipY ? flowThreadPortionRect.y() : std::min(flowThreadPortionRect.y(), flowThreadOverflow.y());
        LayoutUnit maxY = clipY ? flowThreadPortionRect.maxY() : std::max(flowThreadPortionRect.y(), flowThreadOverflow.maxY());
        clipRect = LayoutRect(minX, minY, maxX - minX, maxY - minY);
    }
    return clipRect;
}

LayoutUnit RenderFragmentContainer::pageLogicalTopForOffset(LayoutUnit /* offset */) const
{
    return flowThread()->isHorizontalWritingMode() ? flowThreadPortionRect().y() : flowThreadPortionRect().x();
}

bool RenderFragmentContainer::isFirstFragment() const
{
    ASSERT(isValid());

    return m_flowThread->firstFragment() == this;
}

bool RenderFragmentContainer::isLastFragment() const
{
    ASSERT(isValid());

    return m_flowThread->lastFragment() == this;
}

bool RenderFragmentContainer::shouldClipFlowThreadContent() const
{
    return hasOverflowClip();
}

void RenderFragmentContainer::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlockFlow::styleDidChange(diff, oldStyle);

    if (!isValid())
        return;

    if (oldStyle && oldStyle->writingMode() != style().writingMode())
        m_flowThread->fragmentChangedWritingMode(this);
}

void RenderFragmentContainer::computeOverflowFromFlowThread()
{
    ASSERT(isValid());

    LayoutRect layoutRect = layoutOverflowRectForBox(m_flowThread);
    layoutRect.setLocation(contentBoxRect().location() + (layoutRect.location() - m_flowThreadPortionRect.location()));

    // FIXME: Correctly adjust the layout overflow for writing modes.
    addLayoutOverflow(layoutRect);
    RenderFlowThread* enclosingRenderFlowThread = flowThreadContainingBlock();
    if (enclosingRenderFlowThread)
        enclosingRenderFlowThread->addFragmentsLayoutOverflow(this, layoutRect);

    updateLayerTransform();
    updateScrollInfoAfterLayout();
}

void RenderFragmentContainer::repaintFlowThreadContent(const LayoutRect& repaintRect)
{
    repaintFlowThreadContentRectangle(repaintRect, flowThreadPortionRect(), contentBoxRect().location());
}

void RenderFragmentContainer::repaintFlowThreadContentRectangle(const LayoutRect& repaintRect, const LayoutRect& flowThreadPortionRect, const LayoutPoint& fragmentLocation, const LayoutRect* flowThreadPortionClipRect)
{
    ASSERT(isValid());

    // We only have to issue a repaint in this fragment if the fragment rect intersects the repaint rect.
    LayoutRect clippedRect(repaintRect);

    if (flowThreadPortionClipRect) {
        LayoutRect flippedFlowThreadPortionClipRect(*flowThreadPortionClipRect);
        flowThread()->flipForWritingMode(flippedFlowThreadPortionClipRect);
        clippedRect.intersect(flippedFlowThreadPortionClipRect);
    }

    if (clippedRect.isEmpty())
        return;

    LayoutRect flippedFlowThreadPortionRect(flowThreadPortionRect);
    flowThread()->flipForWritingMode(flippedFlowThreadPortionRect); // Put the fragment rects into physical coordinates.

    // Put the fragment rect into the fragment's physical coordinate space.
    clippedRect.setLocation(fragmentLocation + (clippedRect.location() - flippedFlowThreadPortionRect.location()));

    // Now switch to the fragment's writing mode coordinate space and let it repaint itself.
    flipForWritingMode(clippedRect);
    
    // Issue the repaint.
    repaintRectangle(clippedRect);
}

void RenderFragmentContainer::installFlowThread()
{
    ASSERT_NOT_REACHED();
}

void RenderFragmentContainer::attachFragment()
{
    if (renderTreeBeingDestroyed())
        return;
    
    // A fragment starts off invalid.
    setIsValid(false);

    // Initialize the flow thread reference and create the flow thread object if needed.
    // The flow thread lifetime is influenced by the number of fragments attached to it,
    // and we are attaching the fragment to the flow thread.
    installFlowThread();
    
    if (!m_flowThread)
        return;

    // Only after adding the fragment to the thread, the fragment is marked to be valid.
    m_flowThread->addFragmentToThread(this);
}

void RenderFragmentContainer::detachFragment()
{
    if (m_flowThread)
        m_flowThread->removeFragmentFromThread(this);
    m_flowThread = nullptr;
}

RenderBoxFragmentInfo* RenderFragmentContainer::renderBoxFragmentInfo(const RenderBox* box) const
{
    ASSERT(isValid());
    return m_renderBoxFragmentInfo.get(box);
}

RenderBoxFragmentInfo* RenderFragmentContainer::setRenderBoxFragmentInfo(const RenderBox* box, LayoutUnit logicalLeftInset, LayoutUnit logicalRightInset,
    bool containingBlockChainIsInset)
{
    ASSERT(isValid());

    std::unique_ptr<RenderBoxFragmentInfo>& boxInfo = m_renderBoxFragmentInfo.add(box, std::make_unique<RenderBoxFragmentInfo>(logicalLeftInset, logicalRightInset, containingBlockChainIsInset)).iterator->value;
    return boxInfo.get();
}

std::unique_ptr<RenderBoxFragmentInfo> RenderFragmentContainer::takeRenderBoxFragmentInfo(const RenderBox* box)
{
    return m_renderBoxFragmentInfo.take(box);
}

void RenderFragmentContainer::removeRenderBoxFragmentInfo(const RenderBox& box)
{
    m_renderBoxFragmentInfo.remove(&box);
}

void RenderFragmentContainer::deleteAllRenderBoxFragmentInfo()
{
    m_renderBoxFragmentInfo.clear();
}

LayoutUnit RenderFragmentContainer::logicalTopOfFlowThreadContentRect(const LayoutRect& rect) const
{
    ASSERT(isValid());
    return flowThread()->isHorizontalWritingMode() ? rect.y() : rect.x();
}

LayoutUnit RenderFragmentContainer::logicalBottomOfFlowThreadContentRect(const LayoutRect& rect) const
{
    ASSERT(isValid());
    return flowThread()->isHorizontalWritingMode() ? rect.maxY() : rect.maxX();
}

void RenderFragmentContainer::insertedIntoTree()
{
    attachFragment();
    if (isValid())
        RenderBlockFlow::insertedIntoTree();
}

void RenderFragmentContainer::willBeRemovedFromTree()
{
    RenderBlockFlow::willBeRemovedFromTree();

    detachFragment();
}

void RenderFragmentContainer::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    if (!isValid()) {
        RenderBlockFlow::computeIntrinsicLogicalWidths(minLogicalWidth, maxLogicalWidth);
        return;
    }

    minLogicalWidth = m_flowThread->minPreferredLogicalWidth();
    maxLogicalWidth = m_flowThread->maxPreferredLogicalWidth();
}

void RenderFragmentContainer::computePreferredLogicalWidths()
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

void RenderFragmentContainer::adjustFragmentBoundsFromFlowThreadPortionRect(LayoutRect& fragmentBounds) const
{
    LayoutRect flippedFlowThreadPortionRect = flowThreadPortionRect();
    flowThread()->flipForWritingMode(flippedFlowThreadPortionRect);
    fragmentBounds.moveBy(flippedFlowThreadPortionRect.location());
}

void RenderFragmentContainer::ensureOverflowForBox(const RenderBox* box, RefPtr<RenderOverflow>& overflow, bool forceCreation)
{
    ASSERT(m_flowThread->renderFragmentContainerList().contains(this));
    ASSERT(isValid());

    RenderBoxFragmentInfo* boxInfo = renderBoxFragmentInfo(box);
    if (!boxInfo && !forceCreation)
        return;

    if (boxInfo && boxInfo->overflow()) {
        overflow = boxInfo->overflow();
        return;
    }
    
    LayoutRect borderBox = box->borderBoxRectInFragment(this);
    LayoutRect clientBox;
    ASSERT(m_flowThread->objectShouldFragmentInFlowFragment(box, this));

    if (!borderBox.isEmpty()) {
        borderBox = rectFlowPortionForBox(box, borderBox);
        
        clientBox = box->clientBoxRectInFragment(this);
        clientBox = rectFlowPortionForBox(box, clientBox);
        
        m_flowThread->flipForWritingModeLocalCoordinates(borderBox);
        m_flowThread->flipForWritingModeLocalCoordinates(clientBox);
    }

    if (boxInfo) {
        boxInfo->createOverflow(clientBox, borderBox);
        overflow = boxInfo->overflow();
    } else
        overflow = adoptRef(new RenderOverflow(clientBox, borderBox));
}

LayoutRect RenderFragmentContainer::rectFlowPortionForBox(const RenderBox* box, const LayoutRect& rect) const
{
    LayoutRect mappedRect = m_flowThread->mapFromLocalToFlowThread(box, rect);

    RenderFragmentContainer* startFragment = nullptr;
    RenderFragmentContainer* endFragment = nullptr;
    if (m_flowThread->getFragmentRangeForBox(box, startFragment, endFragment)) {
        if (flowThread()->isHorizontalWritingMode()) {
            if (this != startFragment)
                mappedRect.shiftYEdgeTo(std::max<LayoutUnit>(logicalTopForFlowThreadContent(), mappedRect.y()));
            if (this != endFragment)
                mappedRect.setHeight(std::max<LayoutUnit>(0, std::min<LayoutUnit>(logicalBottomForFlowThreadContent() - mappedRect.y(), mappedRect.height())));
        } else {
            if (this != startFragment)
                mappedRect.shiftXEdgeTo(std::max<LayoutUnit>(logicalTopForFlowThreadContent(), mappedRect.x()));
            if (this != endFragment)
                mappedRect.setWidth(std::max<LayoutUnit>(0, std::min<LayoutUnit>(logicalBottomForFlowThreadContent() - mappedRect.x(), mappedRect.width())));
        }
    }

    return m_flowThread->mapFromFlowThreadToLocal(box, mappedRect);
}

void RenderFragmentContainer::addLayoutOverflowForBox(const RenderBox* box, const LayoutRect& rect)
{
    if (rect.isEmpty())
        return;

    RefPtr<RenderOverflow> fragmentOverflow;
    ensureOverflowForBox(box, fragmentOverflow, false);

    if (!fragmentOverflow)
        return;

    fragmentOverflow->addLayoutOverflow(rect);
}

void RenderFragmentContainer::addVisualOverflowForBox(const RenderBox* box, const LayoutRect& rect)
{
    if (rect.isEmpty())
        return;

    RefPtr<RenderOverflow> fragmentOverflow;
    ensureOverflowForBox(box, fragmentOverflow, false);

    if (!fragmentOverflow)
        return;

    LayoutRect flippedRect = rect;
    flowThread()->flipForWritingModeLocalCoordinates(flippedRect);
    fragmentOverflow->addVisualOverflow(flippedRect);
}

LayoutRect RenderFragmentContainer::layoutOverflowRectForBox(const RenderBox* box)
{
    RefPtr<RenderOverflow> overflow;
    ensureOverflowForBox(box, overflow, true);
    
    ASSERT(overflow);
    return overflow->layoutOverflowRect();
}

LayoutRect RenderFragmentContainer::visualOverflowRectForBox(const RenderBoxModelObject& box)
{
    if (is<RenderInline>(box)) {
        const RenderInline& inlineBox = downcast<RenderInline>(box);
        return inlineBox.linesVisualOverflowBoundingBoxInFragment(this);
    }

    if (is<RenderBox>(box)) {
        RefPtr<RenderOverflow> overflow;
        ensureOverflowForBox(&downcast<RenderBox>(box), overflow, true);

        ASSERT(overflow);
        return overflow->visualOverflowRect();
    }

    ASSERT_NOT_REACHED();
    return LayoutRect();
}

// FIXME: This doesn't work for writing modes.
LayoutRect RenderFragmentContainer::layoutOverflowRectForBoxForPropagation(const RenderBox* box)
{
    // Only propagate interior layout overflow if we don't clip it.
    LayoutRect rect = box->borderBoxRectInFragment(this);
    rect = rectFlowPortionForBox(box, rect);
    if (!box->hasOverflowClip())
        rect.unite(layoutOverflowRectForBox(box));

    bool hasTransform = box->hasTransform();
    if (box->isInFlowPositioned() || hasTransform) {
        if (hasTransform)
            rect = box->layer()->currentTransform().mapRect(rect);

        if (box->isInFlowPositioned())
            rect.move(box->offsetForInFlowPosition());
    }

    return rect;
}

LayoutRect RenderFragmentContainer::visualOverflowRectForBoxForPropagation(const RenderBoxModelObject& box)
{
    LayoutRect rect = visualOverflowRectForBox(box);
    flowThread()->flipForWritingModeLocalCoordinates(rect);

    return rect;
}

CurrentRenderFragmentContainerMaintainer::CurrentRenderFragmentContainerMaintainer(RenderFragmentContainer& fragment)
    : m_fragment(fragment)
{
    RenderFlowThread* flowThread = fragment.flowThread();
    // A flow thread can have only one current fragment.
    ASSERT(!flowThread->currentFragment());
    flowThread->setCurrentFragmentMaintainer(this);
}

CurrentRenderFragmentContainerMaintainer::~CurrentRenderFragmentContainerMaintainer()
{
    RenderFlowThread* flowThread = m_fragment.flowThread();
    flowThread->setCurrentFragmentMaintainer(nullptr);
}

} // namespace WebCore
