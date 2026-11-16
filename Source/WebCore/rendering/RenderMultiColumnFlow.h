/*
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "LayerFragment.h"
#include "PODIntervalTree.h"
#include "RenderBlockFlow.h"
#include "RenderFragmentContainer.h"
#include <wtf/WeakHashMap.h>
#include <wtf/WeakListHashSet.h>

namespace WebCore {

class RenderFragmentContainer;
class RenderMultiColumnSet;
class RenderMultiColumnSpannerPlaceholder;

typedef SingleThreadWeakListHashSet<RenderFragmentContainer> RenderFragmentContainerList;
typedef PODIntervalTree<LayoutUnit, SingleThreadWeakPtr<RenderFragmentContainer>> FragmentIntervalTree;

namespace Style {
class ComputedStyle;
}

// RenderMultiColumnFlow is used to collect all the render objects that participate in a
// multi-column flow thread. It will also help in doing the layout. However, it will not render
// directly to screen. Instead, RenderMultiColumnSet objects will redirect their paint
// and nodeAtPoint methods to this object. Each RenderMultiColumnSet will actually be a viewPort
// of the RenderMultiColumnFlow.

class RenderMultiColumnFlow final : public RenderBlockFlow {
    WTF_MAKE_TZONE_ALLOCATED(RenderMultiColumnFlow);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderMultiColumnFlow);
public:
    RenderMultiColumnFlow(Document&, Style::ComputedStyle&&);
    virtual ~RenderMultiColumnFlow();

    RenderBlockFlow* multiColumnBlockFlow() const { return downcast<RenderBlockFlow>(parent()); }

    NODELETE RenderMultiColumnSet* firstMultiColumnSet() const;
    NODELETE RenderMultiColumnSet* lastMultiColumnSet() const;
    NODELETE RenderBox* firstColumnSetOrSpanner() const;
    bool hasColumnSpanner() const { return !m_spannerMap.computeIsEmpty(); }
    NODELETE static RenderBox* nextColumnSetOrSpannerSiblingOf(const RenderBox*);
    NODELETE static RenderBox* previousColumnSetOrSpannerSiblingOf(const RenderBox*);

    NODELETE RenderMultiColumnSpannerPlaceholder* findColumnSpannerPlaceholder(const RenderBox& spanner) const;

    void layout() override;

    unsigned columnCount() const { return m_columnCount; }
    LayoutUnit columnWidth() const { return m_columnWidth; }
    LayoutUnit columnHeightAvailable() const { return m_columnHeightAvailable; }
    void setColumnHeightAvailable(LayoutUnit available) { m_columnHeightAvailable = available; }
    bool inBalancingPass() const { return m_inBalancingPass; }
    void setInBalancingPass(bool balancing) { m_inBalancingPass = balancing; }
    bool needsHeightsRecalculation() const { return m_needsHeightsRecalculation; }
    void setNeedsHeightsRecalculation(bool recalculate) { m_needsHeightsRecalculation = recalculate; }

    bool shouldRelayoutForPagination() const { return !m_inBalancingPass && m_needsHeightsRecalculation; }

    void setColumnCountAndWidth(unsigned count, LayoutUnit width)
    {
        m_columnCount = count;
        m_columnWidth = width;
    }

    bool progressionIsInline() const { return m_progressionIsInline; }
    void setProgressionIsInline(bool progressionIsInline) { m_progressionIsInline = progressionIsInline; }

    bool progressionIsReversed() const { return m_progressionIsReversed; }
    void setProgressionIsReversed(bool reversed) { m_progressionIsReversed = reversed; }

    void removeFlowChildInfo(RenderElement&);

    void addFragmentToThread(RenderFragmentContainer*);
    void removeFragmentFromThread(RenderFragmentContainer&);
    const RenderFragmentContainerList& renderFragmentContainerList() const LIFETIME_BOUND { return m_fragmentList; }

    void updateLogicalWidth() override;

    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;

    bool hasFragments() const { return !m_fragmentList.computeIsEmpty(); }

    void validateFragments();
    void invalidateFragments(MarkingBehavior = MarkingBehavior::ContainingBlockChain);
    bool hasValidFragmentInfo() const { return !m_fragmentsInvalidated && !m_fragmentList.computeIsEmpty(); }

    // Called when a descendant box's layout is finished and it has been positioned within its container.
    void fragmentedFlowDescendantBoxLaidOut(RenderBox*);

    void styleDidChange(Style::Difference, const Style::ComputedStyle* oldStyle) override;

    void repaintRectangleInFragments(const LayoutRect&) const;

    LayoutUnit pageLogicalTopForOffset(LayoutUnit) const;
    LayoutUnit pageLogicalHeightForOffset(LayoutUnit) const;
    LayoutUnit pageRemainingLogicalHeightForOffset(LayoutUnit, PageBoundaryRule = IncludePageBoundary) const;

    void setPageBreak(const RenderBlock*, LayoutUnit offset, LayoutUnit spaceShortage);
    void updateMinimumPageHeight(const RenderBlock*, LayoutUnit offset, LayoutUnit minHeight);
    void updateSpaceShortageForSizeContainment(const RenderBlock*, LayoutUnit offset, LayoutUnit shortage);

    RenderFragmentContainer* fragmentAtBlockOffset(const RenderBox*, LayoutUnit, bool extendLastFragment = false) const;

    bool fragmentsHaveUniformLogicalHeight() const { return m_fragmentsHaveUniformLogicalHeight; }

    RenderFragmentContainer* mapFromFlowToFragment(TransformState&) const;

    // This method takes a logical offset and returns a physical translation that can be applied to map
    // a physical point (corresponding to the logical offset) into the fragment's physical coordinate space.
    LayoutSize physicalTranslationOffsetFromFlowToFragment(const RenderFragmentContainer*, const LayoutUnit) const;

    // The point is physical, and the result is a physical location within the fragment.
    RenderFragmentContainer* physicalTranslationFromFlowToFragment(LayoutPoint&) const;

    // This method is the inverse of the previous method and goes from fragment to flow.
    LayoutSize physicalTranslationFromFragmentToFlow(const RenderMultiColumnSet*, const LayoutPoint&) const;

    void logicalWidthChangedInFragmentsForBlock(const RenderBlock&, RelayoutChildren&);

    LayoutUnit contentLogicalWidthOfFirstFragment() const;
    LayoutUnit contentLogicalHeightOfFirstFragment() const;

    RenderFragmentContainer* firstFragment() const;
    RenderFragmentContainer* lastFragment() const;

    void setFragmentRangeForBox(const RenderBox&, RenderFragmentContainer*, RenderFragmentContainer*);
    bool getFragmentRangeForBox(const RenderBox&, RenderFragmentContainer*& startFragment, RenderFragmentContainer*& endFragment) const;
    bool computedFragmentRangeForBox(const RenderBox&, RenderFragmentContainer*& startFragment, RenderFragmentContainer*& endFragment) const;
    NODELETE bool hasCachedFragmentRangeForBox(const RenderBox&) const;

    // Check if the object should be painted in this fragment and if the fragment is part of this flow thread.
    bool objectShouldFragmentInFlowFragment(const RenderObject*, const RenderFragmentContainer*) const;

    bool addForcedFragmentBreak(const RenderBlock*, LayoutUnit, RenderBox* breakChild, bool isBefore, LayoutUnit* offsetBreakAdjustment = 0);

    bool isPageLogicalHeightKnown() const;
    bool pageLogicalSizeChanged() const { return m_pageLogicalSizeChanged; }

    void collectLayerFragments(LayerFragments&, const LayoutRect& layerBoundingBox, const LayoutRect& dirtyRect) const;
    LayoutRect fragmentsBoundingBox(const LayoutRect& layerBoundingBox) const;

    LayoutUnit offsetFromLogicalTopOfFirstFragment(const RenderBlock*) const;
    void clearRenderBoxFragmentInfo(const RenderBox&, const RenderFragmentContainer*, const RenderFragmentContainer*, const RenderFragmentContainer*, const RenderFragmentContainer*);

    void addFragmentsVisualEffectOverflow(const RenderBox&);
    void addFragmentsVisualOverflowFromTheme(const RenderBlock&);
    void addFragmentsOverflowFromChild(const RenderBox&, const RenderBox&, const LayoutSize&);
    void addFragmentsVisualOverflow(const RenderBox&, const LayoutRect&);
    void clearFragmentsOverflow(const RenderBox&);

    LayoutRect mapFromFragmentedFlowToLocal(const RenderBox*, const LayoutRect&) const;
    LayoutRect mapFromLocalToFragmentedFlow(const RenderBox*, const LayoutRect&) const;

    // Like RenderBox::physicalToBlockStartRelative(), but mirrors around 0 instead of a border box size. Per-fragment box overflow uses this space.
    NODELETE void physicalToBlockStartRelativeIgnoringSize(LayoutRect&) const;
    NODELETE void blockStartRelativeIgnoringSizeToPhysical(LayoutRect& rect) const { physicalToBlockStartRelativeIgnoringSize(rect); }

    // Used to estimate the maximum height of the flow thread.
    static LayoutUnit maxLogicalHeight() { return LayoutUnit::max() / 2; }

    NODELETE bool fragmentInRange(const RenderFragmentContainer* targetFragment, const RenderFragmentContainer* startFragment, const RenderFragmentContainer* endFragment) const;

    bool absoluteQuadsForBox(Vector<FloatQuad>&, bool*, const RenderBox&) const;

    bool boxIsFragmented(const RenderBox&) const;

    bool cachedEnclosingFragmentedFlowNeedsUpdate() const override { return false; }

    void mapAbsoluteToLocalPoint(OptionSet<MapCoordinatesMode>, TransformState&) const override;
    LayoutSize offsetFromContainer(const RenderElement&, const LayoutPoint&, bool* offsetDependsOnPoint = nullptr) const override;

    // FIXME: Eventually as column and fragment flow threads start nesting, this will end up changing.
    bool shouldCheckColumnBreaks() const;

    using SpannerMap = SingleThreadWeakHashMap<const RenderBox, SingleThreadWeakPtr<RenderMultiColumnSpannerPlaceholder>>;
    SpannerMap& spannerMap() LIFETIME_BOUND { return m_spannerMap; }

private:
    ASCIILiteral renderName() const override;
    void willBeRemovedFromTree() override;
    LogicalExtentComputedValues computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop) const override;
    // The multi-column container takes its baselines from its column sets, which know which part of the flow is in each column.
    std::optional<LayoutUnit> firstLineBaseline() const final { return { }; }
    std::optional<LayoutUnit> lastLineBaseline() const final { return { }; }

    // Always create a RenderLayer for the RenderMultiColumnFlow so that we
    // can easily avoid drawing the children directly.
    bool requiresLayer() const final { return true; }

    RenderMultiColumnFlow* locateEnclosingFragmentedFlow() const override { return const_cast<RenderMultiColumnFlow*>(this); }

    // Sets up an initial logical width of the column width even when no fragments have been generated yet.
    LayoutUnit initialLogicalWidth() const;

    void mapLocalToContainer(const RenderLayerModelObject* ancestorContainer, TransformState&, OptionSet<MapCoordinatesMode>, bool* wasFixed) const override;

    void updateFragmentsFragmentedFlowPortionRect();
    NODELETE bool shouldRepaint(const LayoutRect&) const;

    NODELETE bool getFragmentRangeForBoxFromCachedInfo(const RenderBox&, RenderFragmentContainer*& startFragment, RenderFragmentContainer*& endFragment) const;

    void removeRenderBoxFragmentInfo(const RenderBox&);

    class RenderFragmentContainerRange {
    public:
        RenderFragmentContainerRange() = default;
        RenderFragmentContainerRange(RenderFragmentContainer* start, RenderFragmentContainer* end)
        {
            setRange(start, end);
        }

        void setRange(RenderFragmentContainer* start, RenderFragmentContainer* end)
        {
            m_startFragment = start;
            m_endFragment = end;
            m_rangeInvalidated = true;
        }

        RenderFragmentContainer* startFragment() const { return m_startFragment.get(); }
        RenderFragmentContainer* endFragment() const { return m_endFragment.get(); }
        bool rangeInvalidated() const { return m_rangeInvalidated; }
        void clearRangeInvalidated() { m_rangeInvalidated = false; }

    private:
        SingleThreadWeakPtr<RenderFragmentContainer> m_startFragment;
        SingleThreadWeakPtr<RenderFragmentContainer> m_endFragment;
        bool m_rangeInvalidated { false };
    };

    class FragmentSearchAdapter;

    RenderFragmentContainerList m_fragmentList;

    // Map a box to the list of fragments in which the box is rendered.
    using RenderFragmentContainerRangeMap = SingleThreadWeakHashMap<const RenderBox, RenderFragmentContainerRange>;
    RenderFragmentContainerRangeMap m_fragmentRangeMap;

    FragmentIntervalTree m_fragmentIntervalTree;

    SpannerMap m_spannerMap;

    // The last set we worked on. It's not to be used as the "current set". The concept of a
    // "current set" is difficult, since layout may jump back and forth in the tree, due to wrong
    // top location estimates (due to e.g. margin collapsing), and possibly for other reasons.
    mutable SingleThreadWeakPtr<RenderMultiColumnSet> m_lastSetWorkedOn { nullptr };

    unsigned m_columnCount { 1 }; // The default column count/width that are based off our containing block width. These values represent only the default,
    LayoutUnit m_columnWidth { 0 }; // A multi-column block that is split across variable width pages or fragments will have different column counts and widths in each. These values will be cached (eventually) for multi-column blocks.

    LayoutUnit m_columnHeightAvailable; // Total height available to columns, or 0 if auto.

    bool m_fragmentsInvalidated : 1 { false };
    bool m_fragmentsHaveUniformLogicalHeight : 1 { true };
    bool m_pageLogicalSizeChanged : 1 { false };

    bool m_inLayout { false }; // Set while we're laying out the flow thread, during which colum set heights are unknown.
    bool m_inBalancingPass { false }; // Guard to avoid re-entering column balancing.
    bool m_needsHeightsRecalculation { false };

    bool m_progressionIsInline { false };
    bool m_progressionIsReversed { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderMultiColumnFlow, isRenderMultiColumnFlow())
