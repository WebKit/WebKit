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


#ifndef RenderMultiColumnFlowThread_h
#define RenderMultiColumnFlowThread_h

#include "RenderFlowThread.h"

#include <wtf/HashMap.h>

namespace WebCore {

class RenderMultiColumnSet;
class RenderMultiColumnSpannerPlaceholder;

class RenderMultiColumnFlowThread final : public RenderFlowThread {
public:
    RenderMultiColumnFlowThread(Document&, PassRef<RenderStyle>);
    ~RenderMultiColumnFlowThread();

    virtual bool isRenderMultiColumnFlowThread() const override { return true; }

    virtual void removeFlowChildInfo(RenderObject*) override;

    RenderBlockFlow* multiColumnBlockFlow() const { return toRenderBlockFlow(parent()); }

    RenderMultiColumnSet* firstMultiColumnSet() const;
    RenderMultiColumnSet* lastMultiColumnSet() const;
    RenderBox* firstColumnSetOrSpanner() const;
    static RenderBox* nextColumnSetOrSpannerSiblingOf(const RenderBox*);
    static RenderBox* previousColumnSetOrSpannerSiblingOf(const RenderBox*);

    RenderMultiColumnSpannerPlaceholder* findColumnSpannerPlaceholder(RenderBox* spanner) const { return m_spannerMap.get(spanner); }

    virtual void layout() override final;

    // Find the set inside which the specified renderer would be rendered.
    RenderMultiColumnSet* findSetRendering(RenderObject*) const;

    // Populate the flow thread with what's currently its siblings. Called when a regular block
    // becomes a multicol container.
    void populate();

    // Empty the flow thread by moving everything to the parent. Remove all multicol specific
    // renderers. Then destroy the flow thread. Called when a multicol container becomes a regular
    // block.
    void evacuateAndDestroy();

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
    
    void computeLineGridPaginationOrigin(LayoutState&) const;
    
    virtual RenderRegion* mapFromFlowToRegion(TransformState&) const override;
    
    // This method takes a logical offset and returns a physical translation that can be applied to map
    // a physical point (corresponding to the logical offset) into the region's physical coordinate space.
    LayoutSize physicalTranslationOffsetFromFlowToRegion(const RenderRegion*, const LayoutUnit) const;
    
    // The point is physical, and the result is a physical location within the region.
    RenderRegion* physicalTranslationFromFlowToRegion(LayoutPoint&) const;
    
    // This method is the inverse of the previous method and goes from region to flow.
    LayoutSize physicalTranslationFromRegionToFlow(const RenderMultiColumnSet*, const LayoutPoint&) const;
    
    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;
    
    virtual void mapAbsoluteToLocalPoint(MapCoordinatesFlags, TransformState&) const override;
    virtual LayoutSize offsetFromContainer(RenderObject*, const LayoutPoint&, bool* offsetDependsOnPoint = nullptr) const override;
    
    // FIXME: Eventually as column and region flow threads start nesting, this will end up changing.
    virtual bool shouldCheckColumnBreaks() const override;

private:
    virtual const char* renderName() const override;
    virtual void addRegionToThread(RenderRegion*) override;
    virtual void willBeRemovedFromTree() override;
    virtual RenderObject* resolveMovedChild(RenderObject* child) const override;
    virtual void flowThreadDescendantInserted(RenderObject*) override;
    virtual void flowThreadRelativeWillBeRemoved(RenderObject*) override;
    virtual void flowThreadDescendantBoxLaidOut(RenderBox*) override;
    virtual void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const override;
    virtual LayoutUnit initialLogicalWidth() const override;
    virtual void autoGenerateRegionsToBlockOffset(LayoutUnit) override;
    virtual void setPageBreak(const RenderBlock*, LayoutUnit offset, LayoutUnit spaceShortage) override;
    virtual void updateMinimumPageHeight(const RenderBlock*, LayoutUnit offset, LayoutUnit minHeight) override;
    virtual RenderRegion* regionAtBlockOffset(const RenderBox*, LayoutUnit, bool extendLastRegion = false, RegionAutoGenerationPolicy = AllowRegionAutoGeneration) override;
    virtual void setRegionRangeForBox(const RenderBox*, RenderRegion*, RenderRegion*) override;
    virtual bool addForcedRegionBreak(const RenderBlock*, LayoutUnit, RenderBox* breakChild, bool isBefore, LayoutUnit* offsetBreakAdjustment = 0) override;
    virtual bool isPageLogicalHeightKnown() const override;

private:
    typedef HashMap<RenderBox*, RenderMultiColumnSpannerPlaceholder*> SpannerMap;
    SpannerMap m_spannerMap;

    // The last set we worked on. It's not to be used as the "current set". The concept of a
    // "current set" is difficult, since layout may jump back and forth in the tree, due to wrong
    // top location estimates (due to e.g. margin collapsing), and possibly for other reasons.
    RenderMultiColumnSet* m_lastSetWorkedOn;

    unsigned m_columnCount;   // The default column count/width that are based off our containing block width. These values represent only the default,
    LayoutUnit m_columnWidth; // A multi-column block that is split across variable width pages or regions will have different column counts and widths in each.
                              // These values will be cached (eventually) for multi-column blocks.
    LayoutUnit m_columnHeightAvailable; // Total height available to columns, or 0 if auto.
    bool m_inLayout; // Set while we're laying out the flow thread, during which colum set heights are unknown.
    bool m_inBalancingPass; // Guard to avoid re-entering column balancing.
    bool m_needsHeightsRecalculation;
    
    bool m_progressionIsInline;
    bool m_progressionIsReversed;
    bool m_beingEvacuated;
    
    static bool gShiftingSpanner;
};

RENDER_OBJECT_TYPE_CASTS(RenderMultiColumnFlowThread, isRenderMultiColumnFlowThread())

} // namespace WebCore

#endif // RenderMultiColumnFlowThread_h

