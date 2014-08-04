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


#ifndef RenderMultiColumnSet_h
#define RenderMultiColumnSet_h

#include "RenderMultiColumnFlowThread.h"
#include "RenderRegionSet.h"
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
// Column spans result in the creation of new column sets as well, since a spanning region has to be placed in between the column sets that
// come before and after the span.
class RenderMultiColumnSet final : public RenderRegionSet {
public:
    RenderMultiColumnSet(RenderFlowThread&, PassRef<RenderStyle>);

    virtual bool isRenderMultiColumnSet() const override { return true; }

    RenderBlockFlow* multiColumnBlockFlow() const { return toRenderBlockFlow(parent()); }
    RenderMultiColumnFlowThread* multiColumnFlowThread() const { return static_cast<RenderMultiColumnFlowThread*>(flowThread()); }

    RenderMultiColumnSet* nextSiblingMultiColumnSet() const;
    RenderMultiColumnSet* previousSiblingMultiColumnSet() const;

    // Return the first object in the flow thread that's rendered inside this set.
    RenderObject* firstRendererInFlowThread() const;
    // Return the last object in the flow thread that's rendered inside this set.
    RenderObject* lastRendererInFlowThread() const;

    // Return true if the specified renderer (descendant of the flow thread) is inside this column set.
    bool containsRendererInFlowThread(RenderObject*) const;

    void setLogicalTopInFlowThread(LayoutUnit);
    LayoutUnit logicalTopInFlowThread() const { return isHorizontalWritingMode() ? flowThreadPortionRect().y() : flowThreadPortionRect().x(); }
    void setLogicalBottomInFlowThread(LayoutUnit);
    LayoutUnit logicalBottomInFlowThread() const { return isHorizontalWritingMode() ? flowThreadPortionRect().maxY() : flowThreadPortionRect().maxX(); }
    LayoutUnit logicalHeightInFlowThread() const { return isHorizontalWritingMode() ? flowThreadPortionRect().height() : flowThreadPortionRect().width(); }

    unsigned computedColumnCount() const { return m_computedColumnCount; }
    LayoutUnit computedColumnWidth() const { return m_computedColumnWidth; }
    LayoutUnit computedColumnHeight() const { return m_computedColumnHeight; }

    void setComputedColumnWidthAndCount(LayoutUnit width, unsigned count)
    {
        m_computedColumnWidth = width;
        m_computedColumnCount = count;
    }

    LayoutUnit heightAdjustedForSetOffset(LayoutUnit height) const;

    void updateMinimumColumnHeight(LayoutUnit height) { m_minimumColumnHeight = std::max(height, m_minimumColumnHeight); }
    LayoutUnit minimumColumnHeight() const { return m_minimumColumnHeight; }

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

    virtual void updateLogicalWidth() override;

    void prepareForLayout(bool initial);
    // Begin laying out content for this column set. This happens at the beginning of flow thread
    // layout, and when advancing from a previous column set or spanner to this one.
    void beginFlow(RenderBlock* container);
    // Finish laying out content for this column set. This happens at end of flow thread layout, and
    // when advancing to the next column set or spanner.
    void endFlow(RenderBlock* container, LayoutUnit bottomInContainer);
    // Has this set been flowed in this layout pass?
    bool hasBeenFlowed() const { return logicalBottomInFlowThread() != RenderFlowThread::maxLogicalHeight(); }

    bool requiresBalancing() const;

    LayoutPoint columnTranslationForOffset(const LayoutUnit&) const;
    
    void paintColumnRules(PaintInfo&, const LayoutPoint& paintOffset);

    enum ColumnHitTestTranslationMode {
        ClampHitTestTranslationToColumns,
        DoNotClampHitTestTranslationToColumns
    };
    LayoutPoint translateRegionPointToFlowThread(const LayoutPoint & logicalPoint, ColumnHitTestTranslationMode = DoNotClampHitTestTranslationToColumns) const;

    virtual void updateHitTestResult(HitTestResult&, const LayoutPoint&) override;
    
    LayoutRect columnRectAt(unsigned index) const;
    unsigned columnCount() const;

protected:
    void addOverflowFromChildren() override;
    
private:
    virtual void layout() override;

    virtual void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const override;

    virtual void paintObject(PaintInfo&, const LayoutPoint&) override { }

    virtual LayoutUnit pageLogicalWidth() const override { return m_computedColumnWidth; }
    virtual LayoutUnit pageLogicalHeight() const override { return m_computedColumnHeight; }

    virtual LayoutUnit pageLogicalTopForOffset(LayoutUnit offset) const override;

    virtual LayoutUnit logicalHeightOfAllFlowThreadContent() const override { return logicalHeightInFlowThread(); }

    virtual void repaintFlowThreadContent(const LayoutRect& repaintRect) override;

    virtual void collectLayerFragments(LayerFragments&, const LayoutRect& layerBoundingBox, const LayoutRect& dirtyRect) override;

    virtual void adjustRegionBoundsFromFlowThreadPortionRect(const LayoutPoint& layerOffset, LayoutRect& regionBounds) override;

    virtual VisiblePosition positionForPoint(const LayoutPoint&, const RenderRegion*) override;

    virtual const char* renderName() const;

    LayoutUnit calculateMaxColumnHeight() const;
    LayoutUnit columnGap() const;

    LayoutUnit columnLogicalLeft(unsigned) const;
    LayoutUnit columnLogicalTop(unsigned) const;

    LayoutRect flowThreadPortionRectAt(unsigned index) const;
    LayoutRect flowThreadPortionOverflowRect(const LayoutRect& flowThreadPortion, unsigned index, unsigned colCount, LayoutUnit colGap);

    LayoutUnit initialBlockOffsetForPainting() const;

    enum ColumnIndexCalculationMode {
        ClampToExistingColumns, // Stay within the range of already existing columns.
        AssumeNewColumns // Allow column indices outside the range of already existing columns.
    };
    unsigned columnIndexAtOffset(LayoutUnit, ColumnIndexCalculationMode = ClampToExistingColumns) const;

    void setAndConstrainColumnHeight(LayoutUnit);

    // Return the index of the content run with the currently tallest columns, taking all implicit
    // breaks assumed so far into account.
    unsigned findRunWithTallestColumns() const;

    // Given the current list of content runs, make assumptions about where we need to insert
    // implicit breaks (if there's room for any at all; depending on the number of explicit breaks),
    // and store the results. This is needed in order to balance the columns.
    void distributeImplicitBreaks();

    LayoutUnit calculateBalancedHeight(bool initial) const;

    unsigned m_computedColumnCount; // Used column count (the resulting 'N' from the pseudo-algorithm in the multicol spec)
    LayoutUnit m_computedColumnWidth; // Used column width (the resulting 'W' from the pseudo-algorithm in the multicol spec)
    LayoutUnit m_computedColumnHeight;
    LayoutUnit m_availableColumnHeight;
    
    // The following variables are used when balancing the column set.
    LayoutUnit m_maxColumnHeight; // Maximum column height allowed.
    LayoutUnit m_minSpaceShortage; // The smallest amout of space shortage that caused a column break.
    LayoutUnit m_minimumColumnHeight;

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
            , m_assumedImplicitBreaks(0) { }

        unsigned assumedImplicitBreaks() const { return m_assumedImplicitBreaks; }
        void assumeAnotherImplicitBreak() { m_assumedImplicitBreaks++; }
        LayoutUnit breakOffset() const { return m_breakOffset; }

        // Return the column height that this content run would require, considering the implicit
        // breaks assumed so far.
        LayoutUnit columnLogicalHeight(LayoutUnit startOffset) const { return ceilf(float(m_breakOffset - startOffset) / float(m_assumedImplicitBreaks + 1)); }

    private:
        LayoutUnit m_breakOffset; // Flow thread offset where this run ends.
        unsigned m_assumedImplicitBreaks; // Number of implicit breaks in this run assumed so far.
    };
    Vector<ContentRun, 1> m_contentRuns;
};

RENDER_OBJECT_TYPE_CASTS(RenderMultiColumnSet, isRenderMultiColumnSet())

} // namespace WebCore

#endif // RenderMultiColumnSet_h

