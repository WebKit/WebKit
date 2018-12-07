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
#include "RenderFragmentedFlow.h"

#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "InlineElementBox.h"
#include "Node.h"
#include "PODIntervalTree.h"
#include "RenderBoxFragmentInfo.h"
#include "RenderFragmentContainer.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderLayerCompositor.h"
#include "RenderLayoutState.h"
#include "RenderTableCell.h"
#include "RenderTableSection.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "TransformState.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/StackStats.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderFragmentedFlow);

RenderFragmentedFlow::RenderFragmentedFlow(Document& document, RenderStyle&& style)
    : RenderBlockFlow(document, WTFMove(style))
    , m_currentFragmentMaintainer(nullptr)
    , m_fragmentsInvalidated(false)
    , m_fragmentsHaveUniformLogicalWidth(true)
    , m_fragmentsHaveUniformLogicalHeight(true)
    , m_pageLogicalSizeChanged(false)
{
    setIsRenderFragmentedFlow(true);
}

void RenderFragmentedFlow::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlockFlow::styleDidChange(diff, oldStyle);

    if (oldStyle && oldStyle->writingMode() != style().writingMode())
        invalidateFragments();
}

void RenderFragmentedFlow::removeFlowChildInfo(RenderElement& child)
{
    if (is<RenderBlockFlow>(child))
        removeLineFragmentInfo(downcast<RenderBlockFlow>(child));
    if (is<RenderBox>(child))
        removeRenderBoxFragmentInfo(downcast<RenderBox>(child));
}

void RenderFragmentedFlow::removeFragmentFromThread(RenderFragmentContainer* RenderFragmentContainer)
{
    ASSERT(RenderFragmentContainer);
    m_fragmentList.remove(RenderFragmentContainer);
}

void RenderFragmentedFlow::invalidateFragments(MarkingBehavior markingParents)
{
    if (m_fragmentsInvalidated) {
        ASSERT(selfNeedsLayout());
        return;
    }

    m_fragmentRangeMap.clear();
    m_breakBeforeToFragmentMap.clear();
    m_breakAfterToFragmentMap.clear();
    if (m_lineToFragmentMap)
        m_lineToFragmentMap->clear();
    setNeedsLayout(markingParents);

    m_fragmentsInvalidated = true;
}

void RenderFragmentedFlow::validateFragments()
{
    if (m_fragmentsInvalidated) {
        m_fragmentsInvalidated = false;
        m_fragmentsHaveUniformLogicalWidth = true;
        m_fragmentsHaveUniformLogicalHeight = true;

        if (hasFragments()) {
            LayoutUnit previousFragmentLogicalWidth;
            LayoutUnit previousFragmentLogicalHeight;
            bool firstFragmentVisited = false;
            
            for (auto& fragment : m_fragmentList) {
                ASSERT(!fragment->needsLayout() || fragment->isRenderFragmentContainerSet());

                fragment->deleteAllRenderBoxFragmentInfo();

                LayoutUnit fragmentLogicalWidth = fragment->pageLogicalWidth();
                LayoutUnit fragmentLogicalHeight = fragment->pageLogicalHeight();

                if (!firstFragmentVisited)
                    firstFragmentVisited = true;
                else {
                    if (m_fragmentsHaveUniformLogicalWidth && previousFragmentLogicalWidth != fragmentLogicalWidth)
                        m_fragmentsHaveUniformLogicalWidth = false;
                    if (m_fragmentsHaveUniformLogicalHeight && previousFragmentLogicalHeight != fragmentLogicalHeight)
                        m_fragmentsHaveUniformLogicalHeight = false;
                }

                previousFragmentLogicalWidth = fragmentLogicalWidth;
            }

            setFragmentRangeForBox(*this, m_fragmentList.first(), m_fragmentList.last());
        }
    }

    updateLogicalWidth(); // Called to get the maximum logical width for the fragment.
    updateFragmentsFragmentedFlowPortionRect();
}

void RenderFragmentedFlow::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;

    m_pageLogicalSizeChanged = m_fragmentsInvalidated && everHadLayout();

    validateFragments();

    RenderBlockFlow::layout();

    m_pageLogicalSizeChanged = false;
}

void RenderFragmentedFlow::updateLogicalWidth()
{
    LayoutUnit logicalWidth = initialLogicalWidth();
    for (auto& fragment : m_fragmentList) {
        ASSERT(!fragment->needsLayout() || fragment->isRenderFragmentContainerSet());
        logicalWidth = std::max(fragment->pageLogicalWidth(), logicalWidth);
    }
    setLogicalWidth(logicalWidth);

    // If the fragments have non-uniform logical widths, then insert inset information for the RenderFragmentedFlow.
    for (auto& fragment : m_fragmentList) {
        LayoutUnit fragmentLogicalWidth = fragment->pageLogicalWidth();
        LayoutUnit logicalLeft = style().direction() == TextDirection::LTR ? 0_lu : logicalWidth - fragmentLogicalWidth;
        fragment->setRenderBoxFragmentInfo(this, logicalLeft, fragmentLogicalWidth, false);
    }
}

RenderBox::LogicalExtentComputedValues RenderFragmentedFlow::computeLogicalHeight(LayoutUnit, LayoutUnit logicalTop) const
{
    LogicalExtentComputedValues computedValues;
    computedValues.m_position = logicalTop;
    computedValues.m_extent = 0;

    const LayoutUnit maxFlowSize = RenderFragmentedFlow::maxLogicalHeight();
    for (auto& fragment : m_fragmentList) {
        ASSERT(!fragment->needsLayout() || fragment->isRenderFragmentContainerSet());

        LayoutUnit distanceToMaxSize = maxFlowSize - computedValues.m_extent;
        computedValues.m_extent += std::min(distanceToMaxSize, fragment->logicalHeightOfAllFragmentedFlowContent());

        // If we reached the maximum size there's no point in going further.
        if (computedValues.m_extent == maxFlowSize)
            return computedValues;
    }
    return computedValues;
}

bool RenderFragmentedFlow::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    if (hitTestAction == HitTestBlockBackground)
        return false;
    return RenderBlockFlow::nodeAtPoint(request, result, locationInContainer, accumulatedOffset, hitTestAction);
}

bool RenderFragmentedFlow::shouldRepaint(const LayoutRect& r) const
{
    if (view().printing() || r.isEmpty())
        return false;

    return true;
}

void RenderFragmentedFlow::repaintRectangleInFragments(const LayoutRect& repaintRect) const
{
    if (!shouldRepaint(repaintRect) || !hasValidFragmentInfo())
        return;

    LayoutStateDisabler layoutStateDisabler(view().frameView().layoutContext()); // We can't use layout state to repaint, since the fragments are somewhere else.

    for (auto& fragment : m_fragmentList)
        fragment->repaintFragmentedFlowContent(repaintRect);
}

RenderFragmentContainer* RenderFragmentedFlow::fragmentAtBlockOffset(const RenderBox* clampBox, LayoutUnit offset, bool extendLastFragment) const
{
    ASSERT(!m_fragmentsInvalidated);

    if (m_fragmentList.isEmpty())
        return nullptr;

    if (m_fragmentList.size() == 1 && extendLastFragment)
        return m_fragmentList.first();

    if (offset <= 0)
        return clampBox ? clampBox->clampToStartAndEndFragments(m_fragmentList.first()) : m_fragmentList.first();

    FragmentSearchAdapter adapter(offset);
    m_fragmentIntervalTree.allOverlapsWithAdapter<FragmentSearchAdapter>(adapter);

    // If no fragment was found, the offset is in the flow thread overflow.
    // The last fragment will contain the offset if extendLastFragment is set or if the last fragment is a set.
    if (!adapter.result() && (extendLastFragment || m_fragmentList.last()->isRenderFragmentContainerSet()))
        return clampBox ? clampBox->clampToStartAndEndFragments(m_fragmentList.last()) : m_fragmentList.last();

    RenderFragmentContainer* fragment = adapter.result();
    if (!clampBox)
        return fragment;
    return fragment ? clampBox->clampToStartAndEndFragments(fragment) : nullptr;
}

LayoutPoint RenderFragmentedFlow::adjustedPositionRelativeToOffsetParent(const RenderBoxModelObject& boxModelObject, const LayoutPoint& startPoint) const
{
    LayoutPoint referencePoint = startPoint;
    
    const RenderBlock* objContainingBlock = boxModelObject.containingBlock();
    // FIXME: This needs to be adapted for different writing modes inside the flow thread.
    RenderFragmentContainer* startFragment = fragmentAtBlockOffset(objContainingBlock, referencePoint.y());
    if (startFragment) {
        // Take into account the offset coordinates of the fragment.
        RenderBoxModelObject* startFragmentBox = startFragment;
        RenderBoxModelObject* currObject = startFragmentBox;
        RenderBoxModelObject* currOffsetParent;
        while ((currOffsetParent = currObject->offsetParent())) {
            referencePoint.move(currObject->offsetLeft(), currObject->offsetTop());
            
            // Since we're looking for the offset relative to the body, we must also
            // take into consideration the borders of the fragment's offsetParent.
            if (is<RenderBox>(*currOffsetParent) && !currOffsetParent->isBody())
                referencePoint.move(downcast<RenderBox>(*currOffsetParent).borderLeft(), downcast<RenderBox>(*currOffsetParent).borderTop());
            
            currObject = currOffsetParent;
        }
        
        // We need to check if any of this box's containing blocks start in a different fragment
        // and if so, drop the object's top position (which was computed relative to its containing block
        // and is no longer valid) and recompute it using the fragment in which it flows as reference.
        bool wasComputedRelativeToOtherFragment = false;
        while (objContainingBlock && !is<RenderView>(*objContainingBlock)) {
            // Check if this object is in a different fragment.
            RenderFragmentContainer* parentStartFragment = nullptr;
            RenderFragmentContainer* parentEndFragment = nullptr;
            if (getFragmentRangeForBox(objContainingBlock, parentStartFragment, parentEndFragment) && parentStartFragment != startFragment) {
                wasComputedRelativeToOtherFragment = true;
                break;
            }
            objContainingBlock = objContainingBlock->containingBlock();
        }
        
        if (wasComputedRelativeToOtherFragment) {
            if (is<RenderBox>(boxModelObject)) {
                // Use borderBoxRectInFragment to account for variations such as percentage margins.
                LayoutRect borderBoxRect = downcast<RenderBox>(boxModelObject).borderBoxRectInFragment(startFragment, RenderBox::DoNotCacheRenderBoxFragmentInfo);
                referencePoint.move(borderBoxRect.location().x(), 0_lu);
            }
            
            // Get the logical top coordinate of the current object.
            LayoutUnit top;
            if (is<RenderBlock>(boxModelObject))
                top = downcast<RenderBlock>(boxModelObject).offsetFromLogicalTopOfFirstPage();
            else {
                if (boxModelObject.containingBlock())
                    top = boxModelObject.containingBlock()->offsetFromLogicalTopOfFirstPage();
                
                if (is<RenderBox>(boxModelObject))
                    top += downcast<RenderBox>(boxModelObject).topLeftLocation().y();
                else if (is<RenderInline>(boxModelObject))
                    top -= downcast<RenderInline>(boxModelObject).borderTop();
            }
            
            // Get the logical top of the fragment this object starts in
            // and compute the object's top, relative to the fragment's top.
            LayoutUnit fragmentLogicalTop = startFragment->pageLogicalTopForOffset(top);
            LayoutUnit topRelativeToFragment = top - fragmentLogicalTop;
            referencePoint.setY(startFragmentBox->offsetTop() + topRelativeToFragment);
            
            // Since the top has been overridden, check if the
            // relative/sticky positioning must be reconsidered.
            if (boxModelObject.isRelativelyPositioned())
                referencePoint.move(0_lu, boxModelObject.relativePositionOffset().height());
            else if (boxModelObject.isStickilyPositioned())
                referencePoint.move(0_lu, boxModelObject.stickyPositionOffset().height());
        }
        
        // Since we're looking for the offset relative to the body, we must also
        // take into consideration the borders of the fragment.
        referencePoint.move(startFragmentBox->borderLeft(), startFragmentBox->borderTop());
    }
    
    return referencePoint;
}

LayoutUnit RenderFragmentedFlow::pageLogicalTopForOffset(LayoutUnit offset) const
{
    RenderFragmentContainer* fragment = fragmentAtBlockOffset(0, offset, false);
    return fragment ? fragment->pageLogicalTopForOffset(offset) : 0_lu;
}

LayoutUnit RenderFragmentedFlow::pageLogicalWidthForOffset(LayoutUnit offset) const
{
    RenderFragmentContainer* fragment = fragmentAtBlockOffset(0, offset, true);
    return fragment ? fragment->pageLogicalWidth() : contentLogicalWidth();
}

LayoutUnit RenderFragmentedFlow::pageLogicalHeightForOffset(LayoutUnit offset) const
{
    RenderFragmentContainer* fragment = fragmentAtBlockOffset(0, offset, false);
    if (!fragment)
        return 0;

    return fragment->pageLogicalHeight();
}

LayoutUnit RenderFragmentedFlow::pageRemainingLogicalHeightForOffset(LayoutUnit offset, PageBoundaryRule pageBoundaryRule) const
{
    RenderFragmentContainer* fragment = fragmentAtBlockOffset(0, offset, false);
    if (!fragment)
        return 0;

    LayoutUnit pageLogicalTop = fragment->pageLogicalTopForOffset(offset);
    LayoutUnit pageLogicalHeight = fragment->pageLogicalHeight();
    LayoutUnit pageLogicalBottom = pageLogicalTop + pageLogicalHeight;
    LayoutUnit remainingHeight = pageLogicalBottom - offset;
    if (pageBoundaryRule == IncludePageBoundary) {
        // If IncludePageBoundary is set, the line exactly on the top edge of a
        // fragment will act as being part of the previous fragment.
        remainingHeight = intMod(remainingHeight, pageLogicalHeight);
    }
    return remainingHeight;
}

RenderFragmentContainer* RenderFragmentedFlow::mapFromFlowToFragment(TransformState& transformState) const
{
    if (!hasValidFragmentInfo())
        return nullptr;

    RenderFragmentContainer* RenderFragmentContainer = currentFragment();
    if (!RenderFragmentContainer) {
        LayoutRect boxRect = transformState.mappedQuad().enclosingBoundingBox();
        flipForWritingMode(boxRect);

        LayoutPoint center = boxRect.center();
        RenderFragmentContainer = fragmentAtBlockOffset(this, isHorizontalWritingMode() ? center.y() : center.x(), true);
        if (!RenderFragmentContainer)
            return nullptr;
    }

    LayoutRect flippedFragmentRect(RenderFragmentContainer->fragmentedFlowPortionRect());
    flipForWritingMode(flippedFragmentRect);

    transformState.move(RenderFragmentContainer->contentBoxRect().location() - flippedFragmentRect.location());

    return RenderFragmentContainer;
}

void RenderFragmentedFlow::removeRenderBoxFragmentInfo(RenderBox& box)
{
    if (!hasFragments())
        return;

    // If the fragment chain was invalidated the next layout will clear the box information from all the fragments.
    if (m_fragmentsInvalidated) {
        ASSERT(selfNeedsLayout());
        return;
    }

    RenderFragmentContainer* startFragment = nullptr;
    RenderFragmentContainer* endFragment = nullptr;
    if (getFragmentRangeForBox(&box, startFragment, endFragment)) {
        for (auto it = m_fragmentList.find(startFragment), end = m_fragmentList.end(); it != end; ++it) {
            RenderFragmentContainer* fragment = *it;
            fragment->removeRenderBoxFragmentInfo(box);
            if (fragment == endFragment)
                break;
        }
    }

#ifndef NDEBUG
    // We have to make sure we did not leave any RenderBoxFragmentInfo attached.
    for (auto& fragment : m_fragmentList)
        ASSERT(!fragment->renderBoxFragmentInfo(&box));
#endif

    m_fragmentRangeMap.remove(&box);
}

void RenderFragmentedFlow::removeLineFragmentInfo(const RenderBlockFlow& blockFlow)
{
    if (!m_lineToFragmentMap || blockFlow.lineLayoutPath() == SimpleLinesPath)
        return;

    for (auto* curr = blockFlow.firstRootBox(); curr; curr = curr->nextRootBox())
        m_lineToFragmentMap->remove(curr);

    ASSERT_WITH_SECURITY_IMPLICATION(checkLinesConsistency(blockFlow));
}

void RenderFragmentedFlow::logicalWidthChangedInFragmentsForBlock(const RenderBlock* block, bool& relayoutChildren)
{
    if (!hasValidFragmentInfo())
        return;

    auto it = m_fragmentRangeMap.find(block);
    if (it == m_fragmentRangeMap.end())
        return;

    RenderFragmentContainerRange& range = it->value;
    bool rangeInvalidated = range.rangeInvalidated();
    range.clearRangeInvalidated();

    // If there will be a relayout anyway skip the next steps because they only verify
    // the state of the ranges.
    if (relayoutChildren)
        return;

    // Not necessary for the flow thread, since we already computed the correct info for it.
    // If the fragments have changed invalidate the children.
    if (block == this) {
        relayoutChildren = m_pageLogicalSizeChanged;
        return;
    }

    RenderFragmentContainer* startFragment = nullptr;
    RenderFragmentContainer* endFragment = nullptr;
    if (!getFragmentRangeForBox(block, startFragment, endFragment))
        return;

    for (auto it = m_fragmentList.find(startFragment), end = m_fragmentList.end(); it != end; ++it) {
        RenderFragmentContainer* fragment = *it;
        ASSERT(!fragment->needsLayout() || fragment->isRenderFragmentContainerSet());

        // We have no information computed for this fragment so we need to do it.
        std::unique_ptr<RenderBoxFragmentInfo> oldInfo = fragment->takeRenderBoxFragmentInfo(block);
        if (!oldInfo) {
            relayoutChildren = rangeInvalidated;
            return;
        }

        LayoutUnit oldLogicalWidth = oldInfo->logicalWidth();
        RenderBoxFragmentInfo* newInfo = block->renderBoxFragmentInfo(fragment);
        if (!newInfo || newInfo->logicalWidth() != oldLogicalWidth) {
            relayoutChildren = true;
            return;
        }

        if (fragment == endFragment)
            break;
    }
}

LayoutUnit RenderFragmentedFlow::contentLogicalWidthOfFirstFragment() const
{
    RenderFragmentContainer* firstValidFragmentInFlow = firstFragment();
    if (!firstValidFragmentInFlow)
        return 0;
    return isHorizontalWritingMode() ? firstValidFragmentInFlow->contentWidth() : firstValidFragmentInFlow->contentHeight();
}

LayoutUnit RenderFragmentedFlow::contentLogicalHeightOfFirstFragment() const
{
    RenderFragmentContainer* firstValidFragmentInFlow = firstFragment();
    if (!firstValidFragmentInFlow)
        return 0;
    return isHorizontalWritingMode() ? firstValidFragmentInFlow->contentHeight() : firstValidFragmentInFlow->contentWidth();
}

LayoutUnit RenderFragmentedFlow::contentLogicalLeftOfFirstFragment() const
{
    RenderFragmentContainer* firstValidFragmentInFlow = firstFragment();
    if (!firstValidFragmentInFlow)
        return 0;
    return isHorizontalWritingMode() ? firstValidFragmentInFlow->fragmentedFlowPortionRect().x() : firstValidFragmentInFlow->fragmentedFlowPortionRect().y();
}

RenderFragmentContainer* RenderFragmentedFlow::firstFragment() const
{
    if (!hasFragments())
        return nullptr;
    return m_fragmentList.first();
}

RenderFragmentContainer* RenderFragmentedFlow::lastFragment() const
{
    if (!hasFragments())
        return nullptr;
    return m_fragmentList.last();
}

void RenderFragmentedFlow::clearRenderBoxFragmentInfoAndCustomStyle(const RenderBox& box,
    const RenderFragmentContainer* newStartFragment, const RenderFragmentContainer* newEndFragment,
    const RenderFragmentContainer* oldStartFragment, const RenderFragmentContainer* oldEndFragment)
{
    ASSERT(newStartFragment && newEndFragment && oldStartFragment && oldEndFragment);

    bool insideOldFragmentRange = false;
    bool insideNewFragmentRange = false;
    for (auto& fragment : m_fragmentList) {
        if (oldStartFragment == fragment)
            insideOldFragmentRange = true;
        if (newStartFragment == fragment)
            insideNewFragmentRange = true;

        if (!(insideOldFragmentRange && insideNewFragmentRange)) {
            if (fragment->renderBoxFragmentInfo(&box))
                fragment->removeRenderBoxFragmentInfo(box);
        }

        if (oldEndFragment == fragment)
            insideOldFragmentRange = false;
        if (newEndFragment == fragment)
            insideNewFragmentRange = false;
    }
}

void RenderFragmentedFlow::setFragmentRangeForBox(const RenderBox& box, RenderFragmentContainer* startFragment, RenderFragmentContainer* endFragment)
{
    ASSERT(hasFragments());
    ASSERT(startFragment && endFragment && startFragment->fragmentedFlow() == this && endFragment->fragmentedFlow() == this);
    auto result = m_fragmentRangeMap.set(&box, RenderFragmentContainerRange(startFragment, endFragment));
    if (result.isNewEntry)
        return;

    // If nothing changed, just bail.
    auto& range = result.iterator->value;
    if (range.startFragment() == startFragment && range.endFragment() == endFragment)
        return;
    clearRenderBoxFragmentInfoAndCustomStyle(box, startFragment, endFragment, range.startFragment(), range.endFragment());
}

bool RenderFragmentedFlow::hasCachedFragmentRangeForBox(const RenderBox& box) const
{
    return m_fragmentRangeMap.contains(&box);
}

bool RenderFragmentedFlow::getFragmentRangeForBoxFromCachedInfo(const RenderBox* box, RenderFragmentContainer*& startFragment, RenderFragmentContainer*& endFragment) const
{
    ASSERT(box);
    ASSERT(hasValidFragmentInfo());
    ASSERT((startFragment == nullptr) && (endFragment == nullptr));

    auto it = m_fragmentRangeMap.find(box);
    if (it != m_fragmentRangeMap.end()) {
        const RenderFragmentContainerRange& range = it->value;
        startFragment = range.startFragment();
        endFragment = range.endFragment();
        ASSERT(m_fragmentList.contains(startFragment) && m_fragmentList.contains(endFragment));
        return true;
    }

    return false;
}

bool RenderFragmentedFlow::getFragmentRangeForBox(const RenderBox* box, RenderFragmentContainer*& startFragment, RenderFragmentContainer*& endFragment) const
{
    ASSERT(box);

    startFragment = endFragment = nullptr;
    if (!hasValidFragmentInfo()) // We clear the ranges when we invalidate the fragments.
        return false;

    if (m_fragmentList.size() == 1) {
        startFragment = endFragment = m_fragmentList.first();
        return true;
    }

    if (getFragmentRangeForBoxFromCachedInfo(box, startFragment, endFragment))
        return true;

    return false;
}

bool RenderFragmentedFlow::computedFragmentRangeForBox(const RenderBox* box, RenderFragmentContainer*& startFragment, RenderFragmentContainer*& endFragment) const
{
    ASSERT(box);

    startFragment = endFragment = nullptr;
    if (!hasValidFragmentInfo()) // We clear the ranges when we invalidate the fragments.
        return false;

    if (getFragmentRangeForBox(box, startFragment, endFragment))
        return true;

    // Search the fragment range using the information provided by the containing block chain.
    auto* containingBlock = const_cast<RenderBox*>(box);
    while (!containingBlock->isRenderFragmentedFlow()) {
        InlineElementBox* boxWrapper = containingBlock->inlineBoxWrapper();
        if (boxWrapper && boxWrapper->root().containingFragment()) {
            startFragment = endFragment = boxWrapper->root().containingFragment();
            ASSERT(m_fragmentList.contains(startFragment));
            return true;
        }

        // FIXME: Use the containingBlock() value once we patch all the layout systems to be fragment range aware
        // (e.g. if we use containingBlock() the shadow controls of a video element won't get the range from the
        // video box because it's not a block; they need to be patched separately).
        ASSERT(containingBlock->parent());
        containingBlock = &containingBlock->parent()->enclosingBox();
        ASSERT(containingBlock);

        // If a box doesn't have a cached fragment range it usually means the box belongs to a line so startFragment should be equal with endFragment.
        // FIXME: Find the cases when this startFragment should not be equal with endFragment and make sure these boxes have cached fragment ranges.
        if (containingBlock && hasCachedFragmentRangeForBox(*containingBlock)) {
            startFragment = endFragment = fragmentAtBlockOffset(containingBlock, containingBlock->offsetFromLogicalTopOfFirstPage(), true);
            return true;
        }
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool RenderFragmentedFlow::fragmentInRange(const RenderFragmentContainer* targetFragment, const RenderFragmentContainer* startFragment, const RenderFragmentContainer* endFragment) const
{
    ASSERT(targetFragment);

    for (auto it = m_fragmentList.find(const_cast<RenderFragmentContainer*>(startFragment)), end = m_fragmentList.end(); it != end; ++it) {
        const RenderFragmentContainer* currFragment = *it;
        if (targetFragment == currFragment)
            return true;
        if (currFragment == endFragment)
            break;
    }

    return false;
}

bool RenderFragmentedFlow::objectShouldFragmentInFlowFragment(const RenderObject* object, const RenderFragmentContainer* fragment) const
{
    ASSERT(object);
    ASSERT(fragment);
    
    RenderFragmentedFlow* fragmentedFlow = object->enclosingFragmentedFlow();
    if (fragmentedFlow != this)
        return false;

    if (!m_fragmentList.contains(const_cast<RenderFragmentContainer*>(fragment)))
        return false;
    
    RenderFragmentContainer* enclosingBoxStartFragment = nullptr;
    RenderFragmentContainer* enclosingBoxEndFragment = nullptr;
    // If the box has no range, do not check fragmentInRange. Boxes inside inlines do not get ranges.
    // Instead, the containing RootInlineBox will abort when trying to paint inside the wrong fragment.
    if (computedFragmentRangeForBox(&object->enclosingBox(), enclosingBoxStartFragment, enclosingBoxEndFragment)
        && !fragmentInRange(fragment, enclosingBoxStartFragment, enclosingBoxEndFragment))
        return false;
    
    return object->isBox() || object->isRenderInline();
}

bool RenderFragmentedFlow::objectInFlowFragment(const RenderObject* object, const RenderFragmentContainer* fragment) const
{
    ASSERT(object);
    ASSERT(fragment);

    RenderFragmentedFlow* fragmentedFlow = object->enclosingFragmentedFlow();
    if (fragmentedFlow != this)
        return false;

    if (!m_fragmentList.contains(const_cast<RenderFragmentContainer*>(fragment)))
        return false;

    RenderFragmentContainer* enclosingBoxStartFragment = nullptr;
    RenderFragmentContainer* enclosingBoxEndFragment = nullptr;
    if (!getFragmentRangeForBox(&object->enclosingBox(), enclosingBoxStartFragment, enclosingBoxEndFragment))
        return false;

    if (!fragmentInRange(fragment, enclosingBoxStartFragment, enclosingBoxEndFragment))
        return false;

    if (object->isBox())
        return true;

    LayoutRect objectABBRect = object->absoluteBoundingBoxRect(true);
    if (!objectABBRect.width())
        objectABBRect.setWidth(1);
    if (!objectABBRect.height())
        objectABBRect.setHeight(1); 
    if (objectABBRect.intersects(fragment->absoluteBoundingBoxRect(true)))
        return true;

    if (fragment == lastFragment()) {
        // If the object does not intersect any of the enclosing box fragments
        // then the object is in last fragment.
        for (auto it = m_fragmentList.find(enclosingBoxStartFragment), end = m_fragmentList.end(); it != end; ++it) {
            const RenderFragmentContainer* currFragment = *it;
            if (currFragment == fragment)
                break;
            if (objectABBRect.intersects(currFragment->absoluteBoundingBoxRect(true)))
                return false;
        }
        return true;
    }

    return false;
}

#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
bool RenderFragmentedFlow::checkLinesConsistency(const RenderBlockFlow& removedBlock) const
{
    if (!m_lineToFragmentMap)
        return true;

    for (auto& linePair : *m_lineToFragmentMap.get()) {
        const RootInlineBox* line = linePair.key;
        RenderFragmentContainer* fragment = linePair.value;
        if (&line->blockFlow() == &removedBlock)
            return false;
        if (line->blockFlow().fragmentedFlowState() == NotInsideFragmentedFlow)
            return false;
        if (!m_fragmentList.contains(fragment))
            return false;
    }

    return true;
}
#endif

void RenderFragmentedFlow::clearLinesToFragmentMap()
{
    if (m_lineToFragmentMap)
        m_lineToFragmentMap->clear();
}

void RenderFragmentedFlow::deleteLines()
{
    clearLinesToFragmentMap();
    RenderBlockFlow::deleteLines();
}

void RenderFragmentedFlow::willBeDestroyed()
{
    clearLinesToFragmentMap();
    RenderBlockFlow::willBeDestroyed();
}

void RenderFragmentedFlow::markFragmentsForOverflowLayoutIfNeeded()
{
    if (!hasFragments())
        return;

    for (auto& fragment : m_fragmentList)
        fragment->setNeedsSimplifiedNormalFlowLayout();
}

void RenderFragmentedFlow::updateFragmentsFragmentedFlowPortionRect()
{
    LayoutUnit logicalHeight;
    // FIXME: Optimize not to clear the interval all the time. This implies manually managing the tree nodes lifecycle.
    m_fragmentIntervalTree.clear();
    for (auto& fragment : m_fragmentList) {
        LayoutUnit fragmentLogicalWidth = fragment->pageLogicalWidth();
        LayoutUnit fragmentLogicalHeight = std::min<LayoutUnit>(RenderFragmentedFlow::maxLogicalHeight() - logicalHeight, fragment->logicalHeightOfAllFragmentedFlowContent());

        LayoutRect fragmentRect(style().direction() == TextDirection::LTR ? 0_lu : logicalWidth() - fragmentLogicalWidth, logicalHeight, fragmentLogicalWidth, fragmentLogicalHeight);

        fragment->setFragmentedFlowPortionRect(isHorizontalWritingMode() ? fragmentRect : fragmentRect.transposedRect());

        m_fragmentIntervalTree.add(FragmentIntervalTree::createInterval(logicalHeight, logicalHeight + fragmentLogicalHeight, makeWeakPtr(fragment)));

        logicalHeight += fragmentLogicalHeight;
    }
}

// Even if we require the break to occur at offsetBreakInFragmentedFlow, because fragments may have min/max-height values,
// it is possible that the break will occur at a different offset than the original one required.
// offsetBreakAdjustment measures the different between the requested break offset and the current break offset.
bool RenderFragmentedFlow::addForcedFragmentBreak(const RenderBlock* block, LayoutUnit offsetBreakInFragmentedFlow, RenderBox*, bool, LayoutUnit* offsetBreakAdjustment)
{
    // We need to update the fragments flow thread portion rect because we are going to process
    // a break on these fragments.
    updateFragmentsFragmentedFlowPortionRect();

    // Simulate a fragment break at offsetBreakInFragmentedFlow. If it points inside an auto logical height fragment,
    // then it determines the fragment computed auto height.
    RenderFragmentContainer* fragment = fragmentAtBlockOffset(block, offsetBreakInFragmentedFlow);
    if (!fragment)
        return false;

    LayoutUnit currentFragmentOffsetInFragmentedFlow = isHorizontalWritingMode() ? fragment->fragmentedFlowPortionRect().y() : fragment->fragmentedFlowPortionRect().x();

    currentFragmentOffsetInFragmentedFlow += isHorizontalWritingMode() ? fragment->fragmentedFlowPortionRect().height() : fragment->fragmentedFlowPortionRect().width();

    if (offsetBreakAdjustment)
        *offsetBreakAdjustment = std::max<LayoutUnit>(0, currentFragmentOffsetInFragmentedFlow - offsetBreakInFragmentedFlow);

    return false;
}

void RenderFragmentedFlow::collectLayerFragments(LayerFragments& layerFragments, const LayoutRect& layerBoundingBox, const LayoutRect& dirtyRect)
{
    ASSERT(!m_fragmentsInvalidated);
    
    for (auto& fragment : m_fragmentList)
        fragment->collectLayerFragments(layerFragments, layerBoundingBox, dirtyRect);
}

LayoutRect RenderFragmentedFlow::fragmentsBoundingBox(const LayoutRect& layerBoundingBox)
{
    ASSERT(!m_fragmentsInvalidated);
    
    LayoutRect result;
    for (auto& fragment : m_fragmentList) {
        LayerFragments fragments;
        fragment->collectLayerFragments(fragments, layerBoundingBox, LayoutRect::infiniteRect());
        for (const auto& fragment : fragments) {
            LayoutRect fragmentRect(layerBoundingBox);
            fragmentRect.intersect(fragment.paginationClip);
            fragmentRect.move(fragment.paginationOffset);
            result.unite(fragmentRect);
        }
    }
    
    return result;
}

LayoutUnit RenderFragmentedFlow::offsetFromLogicalTopOfFirstFragment(const RenderBlock* currentBlock) const
{
    // As a last resort, take the slow path.
    LayoutRect blockRect(0_lu, 0_lu, currentBlock->width(), currentBlock->height());
    while (currentBlock && !is<RenderView>(*currentBlock) && !currentBlock->isRenderFragmentedFlow()) {
        RenderBlock* containerBlock = currentBlock->containingBlock();
        ASSERT(containerBlock);
        if (!containerBlock)
            return 0;
        LayoutPoint currentBlockLocation = currentBlock->location();
        if (is<RenderTableCell>(*currentBlock)) {
            if (auto* section = downcast<RenderTableCell>(*currentBlock).section())
                currentBlockLocation.moveBy(section->location());
        }

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

void RenderFragmentedFlow::FragmentSearchAdapter::collectIfNeeded(const FragmentInterval& interval)
{
    if (m_result)
        return;
    if (interval.low() <= m_offset && interval.high() > m_offset)
        m_result = interval.data();
}

void RenderFragmentedFlow::mapLocalToContainer(const RenderLayerModelObject* repaintContainer, TransformState& transformState, MapCoordinatesFlags mode, bool* wasFixed) const
{
    if (this == repaintContainer)
        return;

    if (RenderFragmentContainer* fragment = mapFromFlowToFragment(transformState)) {
        // FIXME: The cast below is probably not the best solution, we may need to find a better way.
        const RenderObject* fragmentObject = static_cast<const RenderObject*>(fragment);

        // If the repaint container is nullptr, we have to climb up to the RenderView, otherwise swap
        // it with the fragment's repaint container.
        repaintContainer = repaintContainer ? fragment->containerForRepaint() : nullptr;

        if (RenderFragmentedFlow* fragmentFragmentedFlow = fragment->enclosingFragmentedFlow()) {
            RenderFragmentContainer* startFragment = nullptr;
            RenderFragmentContainer* endFragment = nullptr;
            if (fragmentFragmentedFlow->getFragmentRangeForBox(fragment, startFragment, endFragment)) {
                CurrentRenderFragmentContainerMaintainer fragmentMaintainer(*startFragment);
                fragmentObject->mapLocalToContainer(repaintContainer, transformState, mode, wasFixed);
                return;
            }
        }

        fragmentObject->mapLocalToContainer(repaintContainer, transformState, mode, wasFixed);
    }
}

// FIXME: Make this function faster. Walking the render tree is slow, better use a caching mechanism (e.g. |cachedOffsetFromLogicalTopOfFirstFragment|).
LayoutRect RenderFragmentedFlow::mapFromLocalToFragmentedFlow(const RenderBox* box, const LayoutRect& localRect) const
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

// FIXME: Make this function faster. Walking the render tree is slow, better use a caching mechanism (e.g. |cachedOffsetFromLogicalTopOfFirstFragment|).
LayoutRect RenderFragmentedFlow::mapFromFragmentedFlowToLocal(const RenderBox* box, const LayoutRect& rect) const
{
    LayoutRect localRect = rect;
    if (box == this)
        return localRect;

    RenderBlock* containerBlock = box->containingBlock();
    ASSERT(containerBlock);
    if (!containerBlock)
        return LayoutRect();
    localRect = mapFromFragmentedFlowToLocal(containerBlock, localRect);

    LayoutPoint currentBoxLocation = box->location();
    localRect.moveBy(-currentBoxLocation);

    if (containerBlock->style().writingMode() != box->style().writingMode())
        box->flipForWritingMode(localRect);

    return localRect;
}

void RenderFragmentedFlow::flipForWritingModeLocalCoordinates(LayoutRect& rect) const
{
    if (!style().isFlippedBlocksWritingMode())
        return;
    
    if (isHorizontalWritingMode())
        rect.setY(0 - rect.maxY());
    else
        rect.setX(0 - rect.maxX());
}

void RenderFragmentedFlow::addFragmentsVisualEffectOverflow(const RenderBox* box)
{
    RenderFragmentContainer* startFragment = nullptr;
    RenderFragmentContainer* endFragment = nullptr;
    if (!getFragmentRangeForBox(box, startFragment, endFragment))
        return;

    for (auto iter = m_fragmentList.find(startFragment), end = m_fragmentList.end(); iter != end; ++iter) {
        RenderFragmentContainer* fragment = *iter;

        LayoutRect borderBox = box->borderBoxRectInFragment(fragment);
        borderBox = box->applyVisualEffectOverflow(borderBox);
        borderBox = fragment->rectFlowPortionForBox(box, borderBox);

        fragment->addVisualOverflowForBox(box, borderBox);
        if (fragment == endFragment)
            break;
    }
}

void RenderFragmentedFlow::addFragmentsVisualOverflowFromTheme(const RenderBlock* block)
{
    RenderFragmentContainer* startFragment = nullptr;
    RenderFragmentContainer* endFragment = nullptr;
    if (!getFragmentRangeForBox(block, startFragment, endFragment))
        return;

    for (auto iter = m_fragmentList.find(startFragment), end = m_fragmentList.end(); iter != end; ++iter) {
        RenderFragmentContainer* fragment = *iter;

        LayoutRect borderBox = block->borderBoxRectInFragment(fragment);
        borderBox = fragment->rectFlowPortionForBox(block, borderBox);

        FloatRect inflatedRect = borderBox;
        block->theme().adjustRepaintRect(*block, inflatedRect);

        fragment->addVisualOverflowForBox(block, snappedIntRect(LayoutRect(inflatedRect)));
        if (fragment == endFragment)
            break;
    }
}

void RenderFragmentedFlow::addFragmentsOverflowFromChild(const RenderBox* box, const RenderBox* child, const LayoutSize& delta)
{
    RenderFragmentContainer* startFragment = nullptr;
    RenderFragmentContainer* endFragment = nullptr;
    if (!getFragmentRangeForBox(child, startFragment, endFragment))
        return;

    RenderFragmentContainer* containerStartFragment = nullptr;
    RenderFragmentContainer* containerEndFragment = nullptr;
    if (!getFragmentRangeForBox(box, containerStartFragment, containerEndFragment))
        return;

    for (auto iter = m_fragmentList.find(startFragment), end = m_fragmentList.end(); iter != end; ++iter) {
        RenderFragmentContainer* fragment = *iter;
        if (!fragmentInRange(fragment, containerStartFragment, containerEndFragment)) {
            if (fragment == endFragment)
                break;
            continue;
        }

        LayoutRect childLayoutOverflowRect = fragment->layoutOverflowRectForBoxForPropagation(child);
        childLayoutOverflowRect.move(delta);
        
        fragment->addLayoutOverflowForBox(box, childLayoutOverflowRect);

        if (child->hasSelfPaintingLayer() || box->hasOverflowClip()) {
            if (fragment == endFragment)
                break;
            continue;
        }
        LayoutRect childVisualOverflowRect = fragment->visualOverflowRectForBoxForPropagation(*child);
        childVisualOverflowRect.move(delta);
        fragment->addVisualOverflowForBox(box, childVisualOverflowRect);

        if (fragment == endFragment)
            break;
    }
}
    
void RenderFragmentedFlow::addFragmentsLayoutOverflow(const RenderBox* box, const LayoutRect& layoutOverflow)
{
    RenderFragmentContainer* startFragment = nullptr;
    RenderFragmentContainer* endFragment = nullptr;
    if (!getFragmentRangeForBox(box, startFragment, endFragment))
        return;

    for (auto iter = m_fragmentList.find(startFragment), end = m_fragmentList.end(); iter != end; ++iter) {
        RenderFragmentContainer* fragment = *iter;
        LayoutRect layoutOverflowInFragment = fragment->rectFlowPortionForBox(box, layoutOverflow);

        fragment->addLayoutOverflowForBox(box, layoutOverflowInFragment);

        if (fragment == endFragment)
            break;
    }
}

void RenderFragmentedFlow::addFragmentsVisualOverflow(const RenderBox* box, const LayoutRect& visualOverflow)
{
    RenderFragmentContainer* startFragment = nullptr;
    RenderFragmentContainer* endFragment = nullptr;
    if (!getFragmentRangeForBox(box, startFragment, endFragment))
        return;
    
    for (RenderFragmentContainerList::iterator iter = m_fragmentList.find(startFragment); iter != m_fragmentList.end(); ++iter) {
        RenderFragmentContainer* fragment = *iter;
        LayoutRect visualOverflowInFragment = fragment->rectFlowPortionForBox(box, visualOverflow);
        
        fragment->addVisualOverflowForBox(box, visualOverflowInFragment);
        
        if (fragment == endFragment)
            break;
    }
}

void RenderFragmentedFlow::clearFragmentsOverflow(const RenderBox* box)
{
    RenderFragmentContainer* startFragment = nullptr;
    RenderFragmentContainer* endFragment = nullptr;
    if (!getFragmentRangeForBox(box, startFragment, endFragment))
        return;

    for (auto iter = m_fragmentList.find(startFragment), end = m_fragmentList.end(); iter != end; ++iter) {
        RenderFragmentContainer* fragment = *iter;
        RenderBoxFragmentInfo* boxInfo = fragment->renderBoxFragmentInfo(box);
        if (boxInfo && boxInfo->overflow())
            boxInfo->clearOverflow();

        if (fragment == endFragment)
            break;
    }
}

RenderFragmentContainer* RenderFragmentedFlow::currentFragment() const
{
    return m_currentFragmentMaintainer ? &m_currentFragmentMaintainer->fragment() : nullptr;
}

ContainingFragmentMap& RenderFragmentedFlow::containingFragmentMap()
{
    if (!m_lineToFragmentMap)
        m_lineToFragmentMap = std::make_unique<ContainingFragmentMap>();

    return *m_lineToFragmentMap.get();
}


} // namespace WebCore
