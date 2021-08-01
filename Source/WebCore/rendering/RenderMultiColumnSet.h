/*
 * Copyright (C) 2012 Apple Inc.  All rights reserved.
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
#include "RenderFragmentContainerSet.h"
#include "RenderMultiColumnFlow.h"
#include <wtf/Vector.h>

namespace WebCore {

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
class RenderMultiColumnSet final : public RenderFragmentContainerSet {
    WTF_MAKE_ISO_ALLOCATED(RenderMultiColumnSet);
public:
    RenderMultiColumnSet(RenderFragmentedFlow&, RenderStyle&&);

    RenderBlockFlow* multiColumnBlockFlow() const { return downcast<RenderBlockFlow>(parent()); }
    RenderMultiColumnFlow* multiColumnFlow() const { return static_cast<RenderMultiColumnFlow*>(fragmentedFlow()); }

    RenderMultiColumnSet* nextSiblingMultiColumnSet() const;
    RenderMultiColumnSet* previousSiblingMultiColumnSet() const;

    // Return the first object in the flow thread that's rendered inside this set.
    RenderObject* firstRendererInFragmentedFlow() const;
    // Return the last object in the flow thread that's rendered inside this set.
    RenderObject* lastRendererInFragmentedFlow() const;

    // Return true if the specified renderer (descendant of the flow thread) is inside this column set.
    bool containsRendererInFragmentedFlow(const RenderObject&) const;

    void setLogicalTopInFragmentedFlow(LayoutUnit);
    LayoutUnit logicalTopInFragmentedFlow() const { return isHorizontalWritingMode() ? fragmentedFlowPortionRect().y() : fragmentedFlowPortionRect().x(); }
    void setLogicalBottomInFragmentedFlow(LayoutUnit);
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
    void recordSpaceShortage(LayoutUnit spaceShortage);

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

    bool requiresBalancing() const;

    LayoutPoint columnTranslationForOffset(const LayoutUnit&) const;
    
    void paintColumnRules(PaintInfo&, const LayoutPoint& paintOffset) override;

    enum ColumnHitTestTranslationMode {
        ClampHitTestTranslationToColumns,
        DoNotClampHitTestTranslationToColumns
    };
    LayoutPoint translateFragmentPointToFragmentedFlow(const LayoutPoint & logicalPoint, ColumnHitTestTranslationMode = DoNotClampHitTestTranslationToColumns) const;

    void updateHitTestResult(HitTestResult&, const LayoutPoint&) override;
    
    LayoutRect columnRectAt(unsigned index) const;
    unsigned columnCount() const;

    LayoutUnit columnGap() const;

private:
    void addOverflowFromChildren() override;
    
    bool isRenderMultiColumnSet() const override { return true; }
    void layout() override;

    Node* nodeForHitTest() const override;

    LogicalExtentComputedValues computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop) const override;

    void paintObject(PaintInfo&, const LayoutPoint&) override { }

    LayoutUnit pageLogicalWidth() const override { return m_computedColumnWidth; }
    LayoutUnit pageLogicalHeight() const override { return m_computedColumnHeight; }

    LayoutUnit pageLogicalTopForOffset(LayoutUnit offset) const override;

    LayoutUnit logicalHeightOfAllFragmentedFlowContent() const override { return logicalHeightInFragmentedFlow(); }

    void repaintFragmentedFlowContent(const LayoutRect& repaintRect) override;

    void collectLayerFragments(LayerFragments&, const LayoutRect& layerBoundingBox, const LayoutRect& dirtyRect) override;

    void adjustFragmentBoundsFromFragmentedFlowPortionRect(LayoutRect& fragmentBounds) const override;

    Vector<LayoutRect> fragmentRectsForFlowContentRect(const LayoutRect&) final;

    VisiblePosition positionForPoint(const LayoutPoint&, const RenderFragmentContainer*) override;

    const char* renderName() const override;

    LayoutUnit calculateMaxColumnHeight() const;

    LayoutUnit columnLogicalLeft(unsigned) const;
    LayoutUnit columnLogicalTop(unsigned) const;

    LayoutRect fragmentedFlowPortionRectAt(unsigned index) const;
    LayoutRect fragmentedFlowPortionOverflowRect(const LayoutRect& fragmentedFlowPortion, unsigned index, unsigned colCount, LayoutUnit colGap);

    LayoutUnit initialBlockOffsetForPainting() const;

    enum ColumnIndexCalculationMode {
        ClampToExistingColumns, // Stay within the range of already existing columns.
        AssumeNewColumns // Allow column indices outside the range of already existing columns.
    };
    unsigned columnIndexAtOffset(LayoutUnit, ColumnIndexCalculationMode = ClampToExistingColumns) const;

    std::pair<unsigned, unsigned> firstAndLastColumnsFromOffsets(LayoutUnit topOffset, LayoutUnit bottomOffset) const;

    void setAndConstrainColumnHeight(LayoutUnit);

    // Return the index of the content run with the currently tallest columns, taking all implicit
    // breaks assumed so far into account.
    unsigned findRunWithTallestColumns() const;

    // Given the current list of content runs, make assumptions about where we need to insert
    // implicit breaks (if there's room for any at all; depending on the number of explicit breaks),
    // and store the results. This is needed in order to balance the columns.
    void distributeImplicitBreaks();

    LayoutUnit calculateBalancedHeight(bool initial) const;

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

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderMultiColumnSet, isRenderMultiColumnSet())
