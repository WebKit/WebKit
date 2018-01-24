/*
 * Copyright (C) 2017,2018 Apple Inc.  All rights reserved.
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
#include "RenderLinesClampFlow.h"
#include "RenderMultiColumnSet.h"

namespace WebCore {

class RenderLinesClampSet final : public RenderMultiColumnSet {
    WTF_MAKE_ISO_ALLOCATED(RenderLinesClampSet);
public:
    RenderLinesClampSet(RenderFragmentedFlow&, RenderStyle&&);
    
    bool requiresBalancing() const override { return true; }

    LayoutUnit startPageHeight() const { return m_startPageHeight; }
    LayoutUnit middlePageHeight() const { return m_middlePageHeight; }
    LayoutUnit endPageHeight() const { return m_endPageHeight; }

    LayoutUnit middleObjectHeight() const { return m_middleObjectHeight; }
    void setMiddleObjectHeight(LayoutUnit height) { m_middleObjectHeight = height; }

private:
    bool isRenderLinesClampSet() const override { return true; }

    // Overridden to figure out how to break up the flow into the start/middle/end
    // areas.
    bool recalculateColumnHeight(bool initial) override;

    const char* renderName() const override;
    
    LogicalExtentComputedValues computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop) const override;

    // Lines clamp doesn't support gaps or rules.
    LayoutUnit columnGap() const override { return 0; }
    void paintColumnRules(PaintInfo&, const LayoutPoint&) override { }

    LayoutRect columnRectAt(unsigned index) const override;
    unsigned columnCount() const override;
    unsigned columnIndexAtOffset(LayoutUnit, ColumnIndexCalculationMode = ClampToExistingColumns) const override;
    
    LayoutRect fragmentedFlowPortionRectAt(unsigned index) const override;
    LayoutRect fragmentedFlowPortionOverflowRect(const LayoutRect& fragmentedFlowPortion, unsigned index, unsigned colCount, LayoutUnit colGap) override;

    bool skipLayerFragmentCollectionForColumn(unsigned index) const override { return index == 1; }

    LayoutUnit customBlockProgressionAdjustmentForColumn(unsigned) const override;
    
    LayoutUnit pageLogicalTopForOffset(LayoutUnit) const override;
    LayoutUnit pageLogicalHeightForOffset(LayoutUnit) const override;

private:
    LayoutUnit m_startPageHeight; // Where to clamp the first N lines inside the fragmented flow
    LayoutUnit m_middlePageHeight; // The middle portion of the fragmented flow (does not render).
    LayoutUnit m_endPageHeight; // Where to clamp the last N lines inside the fragmented flow
    
    LayoutUnit m_middleObjectHeight; // The middle object's height plus margins.
};
    
} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderLinesClampSet, isRenderLinesClampSet())
