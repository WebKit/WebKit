/*
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
#include "RenderMultiColumnFlow.h"
#include "VisiblePosition.h"
#include <memory>
#include <wtf/Vector.h>
#include <wtf/WeakHashMap.h>

namespace WebCore {

class RenderBox;
class RenderBoxFragmentInfo;

// RenderMultiColumnSet represents a set of columns that all have the same width and height. By combining runs of same-size columns into a single
// object, we significantly reduce the number of unique RenderObjects required to represent columns.
//
// A simple multi-column block will have exactly one RenderMultiColumnSet child. A simple paginated multi-column block will have three
// RenderMultiColumnSet children: one for the content at the bottom of the first page (whose columns will have a shorter height), one
// for the 2nd to n-1 pages, and then one last column set that will hold the shorter columns on the final page (that may have to be balanced
// as well).
//
// Column spans result in the creation of new column sets as well, since a spanning fragment has to be placed in between the column sets that
// come before and after the span.
class RenderMultiColumnSet final : public RenderBlockFlow {
    WTF_MAKE_TZONE_ALLOCATED(RenderMultiColumnSet);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderMultiColumnSet);
public:
    RenderMultiColumnSet(RenderFragmentedFlow&, Style::ComputedStyle&&);
    virtual ~RenderMultiColumnSet();

    void setFragmentedFlowPortionRect(const LayoutRect& rect) { m_fragmentedFlowPortionRect = rect; }
    LayoutRect fragmentedFlowPortionRect() const { return m_fragmentedFlowPortionRect; }
    LayoutRect fragmentedFlowPortionOverflowRect() const;

    void attachFragment();
    void detachFragment();

    RenderFragmentedFlow* fragmentedFlow() const { return m_fragmentedFlow.get(); }

    // Set once attachFragment() has added the fragment to its fragmented flow. It stays set after detachFragment().
    bool isValid() const { return m_isValid; }

    NODELETE RenderBoxFragmentInfo* renderBoxFragmentInfo(const RenderBox&) const;
    void setRenderBoxFragmentInfo(const RenderBox&, LayoutUnit logicalLeft, LayoutUnit logicalWidth);
    std::unique_ptr<RenderBoxFragmentInfo> takeRenderBoxFragmentInfo(const RenderBox&);
    void removeRenderBoxFragmentInfo(const RenderBox&);

    void deleteAllRenderBoxFragmentInfo();

    bool isFirstFragment() const;
    bool isLastFragment() const;

    // The width and height of a single column or page in the set.
    LayoutUnit pageLogicalWidth() const { return m_computedColumnWidth; }
    LayoutUnit pageLogicalHeight() const { return m_computedColumnHeight; }

    NODELETE LayoutUnit logicalTopOfFragmentedFlowContentRect(const LayoutRect&) const;
    NODELETE LayoutUnit logicalBottomOfFragmentedFlowContentRect(const LayoutRect&) const;
    LayoutUnit logicalTopForFragmentedFlowContent() const { return logicalTopOfFragmentedFlowContentRect(fragmentedFlowPortionRect()); };
    LayoutUnit logicalBottomForFragmentedFlowContent() const { return logicalBottomOfFragmentedFlowContentRect(fragmentedFlowPortionRect()); };

    // The logical height of the entire flow thread portion used by the set, i.e. the height of all the pages
    // or columns added together.
    LayoutUnit logicalHeightOfAllFragmentedFlowContent() const { return logicalHeightInFragmentedFlow(); }

    // The top of the nearest column or page inside the set.
    LayoutUnit pageLogicalTopForOffset(LayoutUnit offset) const;

    void repaintFragmentedFlowContent(const LayoutRect& repaintRect) const;

    void collectLayerFragments(LayerFragments&, const LayoutRect& layerBoundingBox, const LayoutRect& dirtyRect) const;

    void addLayoutOverflowForBox(const RenderBox&, const LayoutRect&);
    void addVisualOverflowForBox(const RenderBox&, const LayoutRect&);
    LayoutRect visualOverflowRectForBox(const RenderBox&) const;
    LayoutRect layoutOverflowRectForBoxForPropagation(const RenderBox&);
    LayoutRect visualOverflowRectForBoxForPropagation(const RenderBox&);

    LayoutRect rectFlowPortionForBox(const RenderBox&, const LayoutRect&) const;

    bool canHaveChildren() const override { return false; }
    bool canHaveGeneratedChildren() const override { return true; }
    PositionWithAffinity positionForPoint(const LayoutPoint&, HitTestSource, const RenderMultiColumnSet*) override;

    Vector<LayoutRect> fragmentRectsForFlowContentRect(const LayoutRect&) const;
    bool contentRectSpansFragments(const LayoutRect&) const;

    RenderBlockFlow* multiColumnBlockFlow() const { return downcast<RenderBlockFlow>(parent()); }
    RenderMultiColumnFlow* multiColumnFlow() const { return downcast<RenderMultiColumnFlow>(fragmentedFlow()); }

    NODELETE RenderMultiColumnSet* nextSiblingMultiColumnSet() const;
    NODELETE RenderMultiColumnSet* previousSiblingMultiColumnSet() const;

    // Return the first object in the flow thread that's rendered inside this set.
    NODELETE RenderObject* firstRendererInFragmentedFlow() const;
    // Return the last object in the flow thread that's rendered inside this set.
    RenderObject* lastRendererInFragmentedFlow() const;

    // Return true if the specified renderer (descendant of the flow thread) is inside this column set.
    bool containsRendererInFragmentedFlow(const RenderObject&) const;

    NODELETE void setLogicalTopInFragmentedFlow(LayoutUnit);
    LayoutUnit logicalTopInFragmentedFlow() const { return isHorizontalWritingMode() ? fragmentedFlowPortionRect().y() : fragmentedFlowPortionRect().x(); }
    NODELETE void setLogicalBottomInFragmentedFlow(LayoutUnit);
    LayoutUnit logicalBottomInFragmentedFlow() const { return isHorizontalWritingMode() ? fragmentedFlowPortionRect().maxY() : fragmentedFlowPortionRect().maxX(); }
    LayoutUnit logicalHeightInFragmentedFlow() const { return isHorizontalWritingMode() ? fragmentedFlowPortionRect().height() : fragmentedFlowPortionRect().width(); }

    unsigned computedColumnCount() const { return m_computedColumnCount; }
    LayoutUnit computedColumnWidth() const { return m_computedColumnWidth; }
    LayoutUnit computedColumnHeight() const { return m_computedColumnHeight; }
    bool columnHeightComputed() const { return m_columnHeightComputed; }

    void setComputedColumnWidthAndCount(LayoutUnit width, unsigned count)
    {
        m_computedColumnWidth = width;
        m_computedColumnCount = count;
    }

    LayoutUnit heightAdjustedForSetOffset(LayoutUnit height) const;

    void updateMinimumColumnHeight(LayoutUnit height) { m_minimumColumnHeight = std::max(height, m_minimumColumnHeight); }
    LayoutUnit minimumColumnHeight() const { return m_minimumColumnHeight; }

    void updateSpaceShortageForSizeContainment(LayoutUnit shortage)
    {
        if (m_spaceShortageForSizeContainment <= 0) {
            m_spaceShortageForSizeContainment = shortage;
            return;
        }
        m_spaceShortageForSizeContainment = std::min(shortage, m_spaceShortageForSizeContainment);
    }

    unsigned forcedBreaksCount() const { return m_contentRuns.size(); }
    void clearForcedBreaks();
    void addForcedBreak(LayoutUnit offsetFromFirstPage);

    // (Re-)calculate the column height. This is first and foremost needed by sets that are to
    // balance the column height, but even when it isn't to be balanced, this is necessary if the
    // multicol container's height is constrained. If |initial| is set, and we are to balance, guess
    // an initial column height; otherwise, stretch the column height a tad. Return true if column
    // height changed and another layout pass is required.
    bool recalculateColumnHeight(bool initial);

    // Record space shortage (the amount of space that would have been enough to prevent some
    // element from being moved to the next column) at a column break. The smallest amount of space
    // shortage we find is the amount with which we will stretch the column height, if it turns out
    // after layout that the columns weren't tall enough.
    NODELETE void recordSpaceShortage(LayoutUnit spaceShortage);

    void updateLogicalWidth() override;

    void prepareForLayout(bool initial);
    // Begin laying out content for this column set. This happens at the beginning of flow thread
    // layout, and when advancing from a previous column set or spanner to this one.
    void beginFlow(RenderBlock* container);
    // Finish laying out content for this column set. This happens at end of flow thread layout, and
    // when advancing to the next column set or spanner.
    void endFlow(RenderBlock* container, LayoutUnit bottomInContainer);
    // Has this set been flowed in this layout pass?
    bool hasBeenFlowed() const { return logicalBottomInFragmentedFlow() != RenderFragmentedFlow::maxLogicalHeight(); }

    void expandToEncompassFragmentedFlowContentsIfNeeded();

    NODELETE bool requiresBalancing() const;

    LayoutPoint columnTranslationForOffset(const LayoutUnit&) const;
    
    void paintColumnRules(PaintInfo&, const LayoutPoint& paintOffset) override;

    enum ColumnHitTestTranslationMode {
        ClampHitTestTranslationToColumns,
        DoNotClampHitTestTranslationToColumns
    };
    LayoutPoint translateFragmentPointToFragmentedFlow(const LayoutPoint & logicalPoint, ColumnHitTestTranslationMode = DoNotClampHitTestTranslationToColumns) const;

    void updateHitTestResult(HitTestResult&, const LayoutPoint&) const override;
    
    LayoutRect columnRectAt(unsigned index) const;
    NODELETE unsigned columnCount() const;

    LayoutUnit columnGap() const;

    std::optional<LayoutUnit> firstLineBaseline() const final;
    std::optional<LayoutUnit> lastLineBaseline() const final;

private:
    void addOverflowFromInFlowChildren(OptionSet<ComputeOverflowOptions> = { }) override;

    void layout() override;

    Node* nodeForHitTest() const override;

    LogicalExtentComputedValues computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop) const override;

    void paintObject(PaintInfo&, const LayoutPoint&) override { }

    ASCIILiteral renderName() const override;

    void insertedIntoTree() override;
    void willBeRemovedFromTree() override;

    std::pair<LayoutUnit, LayoutUnit> computeIntrinsicLogicalWidthContributions() override;
    std::pair<LayoutUnit, LayoutUnit> computeIntrinsicLogicalWidths() const override;

    RenderOverflow* overflowForBox(const RenderBox&) const;
    LayoutRect computedVisualOverflowRectForBox(const RenderBox&) const;
    LayoutRect computedLayoutOverflowRectForBox(const RenderBox&) const;

    LayoutRect overflowRectForFragmentedFlowPortion(const LayoutRect& fragmentedFlowPortionRect, bool isFirstPortion, bool isLastPortion) const;
    void repaintFragmentedFlowContentRectangle(const LayoutRect& repaintRect, const LayoutRect& fragmentedFlowPortionRect, const LayoutPoint& fragmentLocation, const LayoutRect* fragmentedFlowPortionClipRect = 0) const;

    NODELETE LayoutRect fragmentedFlowContentRectangle(const LayoutRect&, const LayoutRect& fragmentedFlowPortionRect, const LayoutPoint& fragmentLocation, const LayoutRect* fragmentedFlowPortionClipRect = 0) const;

    LayoutUnit calculateMaxColumnHeight() const;

    LayoutUnit columnLogicalLeft(unsigned) const;
    LayoutUnit columnLogicalTop(unsigned) const;

    NODELETE LayoutRect fragmentedFlowPortionRectAt(unsigned index) const;
    LayoutRect fragmentedFlowPortionOverflowRect(const LayoutRect& fragmentedFlowPortion, unsigned index, unsigned colCount, LayoutUnit colGap) const;

    LayoutUnit initialBlockOffsetForPainting() const;

    enum ColumnIndexCalculationMode {
        ClampToExistingColumns, // Stay within the range of already existing columns.
        AssumeNewColumns // Allow column indices outside the range of already existing columns.
    };
    NODELETE unsigned columnIndexAtOffset(LayoutUnit, ColumnIndexCalculationMode = ClampToExistingColumns) const;

    std::pair<unsigned, unsigned> firstAndLastColumnsFromOffsets(LayoutUnit topOffset, LayoutUnit bottomOffset) const;

    NODELETE void setAndConstrainColumnHeight(LayoutUnit);

    // Return the index of the content run with the currently tallest columns, taking all implicit
    // breaks assumed so far into account.
    unsigned findRunWithTallestColumns() const;

    // Given the current list of content runs, make assumptions about where we need to insert
    // implicit breaks (if there's room for any at all; depending on the number of explicit breaks),
    // and store the results. This is needed in order to balance the columns.
    void distributeImplicitBreaks();

    LayoutUnit calculateBalancedHeight(bool initial) const;

    SingleThreadWeakPtr<RenderFragmentedFlow> m_fragmentedFlow;

    LayoutRect m_fragmentedFlowPortionRect;

    // This map holds unique information about a block that is split across fragments.
    // A RenderBoxFragmentInfo* tells us about any layout information for a RenderBox that
    // is unique to the fragment. For now it just holds logical width information for RenderBlocks, but eventually
    // it will also hold a custom style for any box (for fragment styling).
    using RenderBoxFragmentInfoMap = SingleThreadWeakHashMap<const RenderBox, std::unique_ptr<RenderBoxFragmentInfo>>;
    RenderBoxFragmentInfoMap m_renderBoxFragmentInfo;

    bool m_isValid { false };

    unsigned m_computedColumnCount { 1 }; // Used column count (the resulting 'N' from the pseudo-algorithm in the multicol spec)
    LayoutUnit m_computedColumnWidth; // Used column width (the resulting 'W' from the pseudo-algorithm in the multicol spec)
    LayoutUnit m_computedColumnHeight;
    LayoutUnit m_availableColumnHeight;
    bool m_columnHeightComputed { false };

    // The following variables are used when balancing the column set.
    LayoutUnit m_maxColumnHeight; // Maximum column height allowed.
    LayoutUnit m_minSpaceShortage; // The smallest amout of space shortage that caused a column break.
    LayoutUnit m_minimumColumnHeight;
    LayoutUnit m_spaceShortageForSizeContainment; // The shortage space that keeps size containment monolithic.

    // A run of content without explicit (forced) breaks; i.e. a flow thread portion between two
    // explicit breaks, between flow thread start and an explicit break, between an explicit break
    // and flow thread end, or, in cases when there are no explicit breaks at all: between flow flow
    // thread start and flow thread end. We need to know where the explicit breaks are, in order to
    // figure out where the implicit breaks will end up, so that we get the columns properly
    // balanced. A content run starts out as representing one single column, and will represent one
    // additional column for each implicit break "inserted" there.
    class ContentRun {
    public:
        ContentRun(LayoutUnit breakOffset)
            : m_breakOffset(breakOffset)
        { }

        unsigned assumedImplicitBreaks() const { return m_assumedImplicitBreaks; }
        void assumeAnotherImplicitBreak() { m_assumedImplicitBreaks++; }
        LayoutUnit breakOffset() const { return m_breakOffset; }

        // Return the column height that this content run would require, considering the implicit
        // breaks assumed so far.
        LayoutUnit columnLogicalHeight(LayoutUnit startOffset) const { return LayoutUnit(ceilf(float(m_breakOffset - startOffset) / float(m_assumedImplicitBreaks + 1))); }

    private:
        LayoutUnit m_breakOffset; // Flow thread offset where this run ends.
        unsigned m_assumedImplicitBreaks { 0 }; // Number of implicit breaks in this run assumed so far.
    };
    Vector<ContentRun, 1> m_contentRuns;
};

#ifndef NDEBUG
TextStream& operator<<(TextStream&, const RenderMultiColumnSet&);
#endif

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderMultiColumnSet, isRenderMultiColumnSet())
