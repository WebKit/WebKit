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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

namespace WebCore {

class RenderMultiColumnFlowThread final : public RenderFlowThread {
public:
    RenderMultiColumnFlowThread(Document&, PassRef<RenderStyle>);
    ~RenderMultiColumnFlowThread();

    unsigned columnCount() const { return m_columnCount; }
    LayoutUnit columnWidth() const { return m_columnWidth; }
    LayoutUnit columnHeightAvailable() const { return m_columnHeightAvailable; }
    void setColumnHeightAvailable(LayoutUnit available) { m_columnHeightAvailable = available; }
    bool inBalancingPass() const { return m_inBalancingPass; }
    void setInBalancingPass(bool balancing) { m_inBalancingPass = balancing; }
    bool needsRebalancing() const { return m_needsRebalancing; }
    void setNeedsRebalancing(bool balancing) { m_needsRebalancing = balancing; }
    
    bool shouldRelayoutForPagination() const { return !m_inBalancingPass && m_needsRebalancing; }
    
    bool requiresBalancing() const { return !columnHeightAvailable() || parent()->style().columnFill() == ColumnFillBalance; }

    void setColumnCountAndWidth(unsigned count, LayoutUnit width)
    {
        m_columnCount = count;
        m_columnWidth = width;
    }

private:
    virtual const char* renderName() const override;
    virtual void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const override;
    virtual void autoGenerateRegionsToBlockOffset(LayoutUnit) override;
    virtual LayoutUnit initialLogicalWidth() const override;
    virtual void setPageBreak(const RenderBlock*, LayoutUnit offset, LayoutUnit spaceShortage) override;
    virtual void updateMinimumPageHeight(const RenderBlock*, LayoutUnit offset, LayoutUnit minHeight) override;
    virtual bool addForcedRegionBreak(const RenderBlock*, LayoutUnit, RenderBox* breakChild, bool isBefore, LayoutUnit* offsetBreakAdjustment = 0) override;

private:
    unsigned m_columnCount;   // The default column count/width that are based off our containing block width. These values represent only the default,
    LayoutUnit m_columnWidth; // A multi-column block that is split across variable width pages or regions will have different column counts and widths in each.
                              // These values will be cached (eventually) for multi-column blocks.
    LayoutUnit m_columnHeightAvailable; // Total height available to columns, or 0 if auto.
    bool m_inBalancingPass; // Guard to avoid re-entering column balancing.
    bool m_needsRebalancing;
};

} // namespace WebCore

#endif // RenderMultiColumnFlowThread_h

