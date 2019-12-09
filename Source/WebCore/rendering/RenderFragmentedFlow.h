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

#pragma once

#include "LayerFragment.h"
#include "PODIntervalTree.h"
#include "RenderBlockFlow.h"
#include "RenderFragmentContainer.h"
#include <wtf/ListHashSet.h>

namespace WebCore {

class CurrentRenderFragmentContainerMaintainer;
class RenderFragmentedFlow;
class RenderStyle;
class RenderFragmentContainer;
class RootInlineBox;

typedef ListHashSet<RenderFragmentContainer*> RenderFragmentContainerList;
typedef Vector<RenderLayer*> RenderLayerList;
typedef HashMap<const RootInlineBox*, RenderFragmentContainer*> ContainingFragmentMap;

// RenderFragmentedFlow is used to collect all the render objects that participate in a
// flow thread. It will also help in doing the layout. However, it will not render
// directly to screen. Instead, RenderFragmentContainer objects will redirect their paint 
// and nodeAtPoint methods to this object. Each RenderFragmentContainer will actually be a viewPort
// of the RenderFragmentedFlow.

class RenderFragmentedFlow: public RenderBlockFlow {
    WTF_MAKE_ISO_ALLOCATED(RenderFragmentedFlow);
public:
    virtual ~RenderFragmentedFlow() = default;

    virtual void removeFlowChildInfo(RenderElement&);
#ifndef NDEBUG
    bool hasChildInfo(RenderObject* child) const { return is<RenderBox>(child) && m_fragmentRangeMap.contains(downcast<RenderBox>(child)); }
#endif

#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
    bool checkLinesConsistency(const RenderBlockFlow&) const;
#endif
    
    void deleteLines() override;

    virtual void addFragmentToThread(RenderFragmentContainer*) = 0;
    virtual void removeFragmentFromThread(RenderFragmentContainer*);
    const RenderFragmentContainerList& renderFragmentContainerList() const { return m_fragmentList; }

    void updateLogicalWidth() final;
    LogicalExtentComputedValues computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop) const override;

    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;

    bool hasFragments() const { return m_fragmentList.size(); }
    virtual void fragmentChangedWritingMode(RenderFragmentContainer*) { }

    void validateFragments();
    void invalidateFragments(MarkingBehavior = MarkContainingBlockChain);
    bool hasValidFragmentInfo() const { return !m_fragmentsInvalidated && !m_fragmentList.isEmpty(); }
    
    // Called when a descendant box's layout is finished and it has been positioned within its container.
    virtual void fragmentedFlowDescendantBoxLaidOut(RenderBox*) { }

    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    void repaintRectangleInFragments(const LayoutRect&) const;
    
    LayoutPoint adjustedPositionRelativeToOffsetParent(const RenderBoxModelObject&, const LayoutPoint&) const;

    LayoutUnit pageLogicalTopForOffset(LayoutUnit) const;
    LayoutUnit pageLogicalWidthForOffset(LayoutUnit) const;
    LayoutUnit pageLogicalHeightForOffset(LayoutUnit) const;
    LayoutUnit pageRemainingLogicalHeightForOffset(LayoutUnit, PageBoundaryRule = IncludePageBoundary) const;

    virtual void setPageBreak(const RenderBlock*, LayoutUnit /*offset*/, LayoutUnit /*spaceShortage*/) { }
    virtual void updateMinimumPageHeight(const RenderBlock*, LayoutUnit /*offset*/, LayoutUnit /*minHeight*/) { }

    virtual RenderFragmentContainer* fragmentAtBlockOffset(const RenderBox*, LayoutUnit, bool extendLastFragment = false) const;

    bool fragmentsHaveUniformLogicalWidth() const { return m_fragmentsHaveUniformLogicalWidth; }
    bool fragmentsHaveUniformLogicalHeight() const { return m_fragmentsHaveUniformLogicalHeight; }

    virtual RenderFragmentContainer* mapFromFlowToFragment(TransformState&) const;

    void logicalWidthChangedInFragmentsForBlock(const RenderBlock*, bool&);

    LayoutUnit contentLogicalWidthOfFirstFragment() const;
    LayoutUnit contentLogicalHeightOfFirstFragment() const;
    LayoutUnit contentLogicalLeftOfFirstFragment() const;
    
    RenderFragmentContainer* firstFragment() const;
    RenderFragmentContainer* lastFragment() const;

    virtual void setFragmentRangeForBox(const RenderBox&, RenderFragmentContainer*, RenderFragmentContainer*);
    bool getFragmentRangeForBox(const RenderBox*, RenderFragmentContainer*& startFragment, RenderFragmentContainer*& endFragment) const;
    bool computedFragmentRangeForBox(const RenderBox*, RenderFragmentContainer*& startFragment, RenderFragmentContainer*& endFragment) const;
    bool hasCachedFragmentRangeForBox(const RenderBox&) const;

    // Check if the object is in fragment and the fragment is part of this flow thread.
    bool objectInFlowFragment(const RenderObject*, const RenderFragmentContainer*) const;
    
    // Check if the object should be painted in this fragment and if the fragment is part of this flow thread.
    bool objectShouldFragmentInFlowFragment(const RenderObject*, const RenderFragmentContainer*) const;

    void markFragmentsForOverflowLayoutIfNeeded();

    virtual bool addForcedFragmentBreak(const RenderBlock*, LayoutUnit, RenderBox* breakChild, bool isBefore, LayoutUnit* offsetBreakAdjustment = 0);
    virtual void applyBreakAfterContent(LayoutUnit) { }

    virtual bool isPageLogicalHeightKnown() const { return true; }
    bool pageLogicalSizeChanged() const { return m_pageLogicalSizeChanged; }

    void collectLayerFragments(LayerFragments&, const LayoutRect& layerBoundingBox, const LayoutRect& dirtyRect);
    LayoutRect fragmentsBoundingBox(const LayoutRect& layerBoundingBox);

    LayoutUnit offsetFromLogicalTopOfFirstFragment(const RenderBlock*) const;
    void clearRenderBoxFragmentInfoAndCustomStyle(const RenderBox&, const RenderFragmentContainer*, const RenderFragmentContainer*, const RenderFragmentContainer*, const RenderFragmentContainer*);

    void addFragmentsVisualEffectOverflow(const RenderBox*);
    void addFragmentsVisualOverflowFromTheme(const RenderBlock*);
    void addFragmentsOverflowFromChild(const RenderBox*, const RenderBox*, const LayoutSize&);
    void addFragmentsLayoutOverflow(const RenderBox*, const LayoutRect&);
    void addFragmentsVisualOverflow(const RenderBox*, const LayoutRect&);
    void clearFragmentsOverflow(const RenderBox*);

    LayoutRect mapFromFragmentedFlowToLocal(const RenderBox*, const LayoutRect&) const;
    LayoutRect mapFromLocalToFragmentedFlow(const RenderBox*, const LayoutRect&) const;

    void flipForWritingModeLocalCoordinates(LayoutRect&) const;

    // Used to estimate the maximum height of the flow thread.
    static LayoutUnit maxLogicalHeight() { return LayoutUnit::max() / 2; }

    bool fragmentInRange(const RenderFragmentContainer* targetFragment, const RenderFragmentContainer* startFragment, const RenderFragmentContainer* endFragment) const;

    virtual bool absoluteQuadsForBox(Vector<FloatQuad>&, bool*, const RenderBox*, float, float) const { return false; }

    void layout() override;

    void setCurrentFragmentMaintainer(CurrentRenderFragmentContainerMaintainer* currentFragmentMaintainer) { m_currentFragmentMaintainer = currentFragmentMaintainer; }
    RenderFragmentContainer* currentFragment() const;

    ContainingFragmentMap& containingFragmentMap();

    bool cachedEnclosingFragmentedFlowNeedsUpdate() const override { return false; }

    // FIXME: Eventually as column and fragment flow threads start nesting, this may end up changing.
    virtual bool shouldCheckColumnBreaks() const { return false; }

private:
    // Always create a RenderLayer for the RenderFragmentedFlow so that we
    // can easily avoid drawing the children directly.
    bool requiresLayer() const final { return true; }

protected:
    RenderFragmentedFlow(Document&, RenderStyle&&);

    RenderFragmentedFlow* locateEnclosingFragmentedFlow() const override { return const_cast<RenderFragmentedFlow*>(this); }

    const char* renderName() const override = 0;

    // Overridden by columns/pages to set up an initial logical width of the page width even when
    // no fragments have been generated yet.
    virtual LayoutUnit initialLogicalWidth() const { return 0; };
    
    void clearLinesToFragmentMap();
    void willBeDestroyed() override;

    void mapLocalToContainer(const RenderLayerModelObject* repaintContainer, TransformState&, MapCoordinatesFlags, bool* wasFixed) const override;

    void updateFragmentsFragmentedFlowPortionRect();
    bool shouldRepaint(const LayoutRect&) const;

    bool getFragmentRangeForBoxFromCachedInfo(const RenderBox*, RenderFragmentContainer*& startFragment, RenderFragmentContainer*& endFragment) const;

    void removeRenderBoxFragmentInfo(RenderBox&);
    void removeLineFragmentInfo(const RenderBlockFlow&);

    class RenderFragmentContainerRange {
    public:
        RenderFragmentContainerRange() = default;
        RenderFragmentContainerRange(RenderFragmentContainer* start, RenderFragmentContainer* end)
        {
            setRange(start, end);
        }
        
        void setRange(RenderFragmentContainer* start, RenderFragmentContainer* end)
        {
            m_startFragment = makeWeakPtr(start);
            m_endFragment = makeWeakPtr(end);
            m_rangeInvalidated = true;
        }

        RenderFragmentContainer* startFragment() const { return m_startFragment.get(); }
        RenderFragmentContainer* endFragment() const { return m_endFragment.get(); }
        bool rangeInvalidated() const { return m_rangeInvalidated; }
        void clearRangeInvalidated() { m_rangeInvalidated = false; }

    private:
        WeakPtr<RenderFragmentContainer> m_startFragment;
        WeakPtr<RenderFragmentContainer> m_endFragment;
        bool m_rangeInvalidated;
    };

    class FragmentSearchAdapter;

    RenderFragmentContainerList m_fragmentList;

    // Map a line to its containing fragment.
    std::unique_ptr<ContainingFragmentMap> m_lineToFragmentMap;

    // Map a box to the list of fragments in which the box is rendered.
    using RenderFragmentContainerRangeMap = HashMap<const RenderBox*, RenderFragmentContainerRange>;
    RenderFragmentContainerRangeMap m_fragmentRangeMap;

    // Map a box with a fragment break to the auto height fragment affected by that break. 
    using RenderBoxToFragmentMap = HashMap<RenderBox*, RenderFragmentContainer*>;
    RenderBoxToFragmentMap m_breakBeforeToFragmentMap;
    RenderBoxToFragmentMap m_breakAfterToFragmentMap;

    using FragmentIntervalTree = PODIntervalTree<LayoutUnit, WeakPtr<RenderFragmentContainer>>;
    FragmentIntervalTree m_fragmentIntervalTree;

    CurrentRenderFragmentContainerMaintainer* m_currentFragmentMaintainer;

    bool m_fragmentsInvalidated : 1;
    bool m_fragmentsHaveUniformLogicalWidth : 1;
    bool m_fragmentsHaveUniformLogicalHeight : 1;
    bool m_pageLogicalSizeChanged : 1;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderFragmentedFlow, isRenderFragmentedFlow())
