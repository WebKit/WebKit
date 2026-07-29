/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/BaselineAlignment.h>
#include <WebCore/FlexFormattingUtils.h>
#include <WebCore/LayoutIntegrationFlexLayout.h>
#include <WebCore/RenderBlock.h>
#include <wtf/Range.h>
#include <wtf/SetForScope.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {

class RenderFlexibleBox : public RenderBlock {
    WTF_MAKE_TZONE_ALLOCATED(RenderFlexibleBox);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderFlexibleBox);
public:
    RenderFlexibleBox(Type, Element&, Style::ComputedStyle&&);
    RenderFlexibleBox(Type, Document&, Style::ComputedStyle&&);
    virtual ~RenderFlexibleBox();

    using Direction = FlowDirection;

    ASCIILiteral renderName() const override;

    bool canDropAnonymousBlockChild() const final { return false; }
    void layoutBlock(RelayoutChildren, LayoutUnit pageLogicalHeight = 0_lu) final;

    std::optional<LayoutUnit> firstLineBaseline() const override;
    std::optional<LayoutUnit> lastLineBaseline() const override;

    void styleDidChange(Style::Difference, const Style::ComputedStyle*) override;
    void flexItemWillBeRemoved(const RenderBox& flexItem);
    bool hitTestChildren(const HitTestRequest&, HitTestResult&, const HitTestLocation&, const LayoutPoint& adjustedLocation, HitTestAction) override;
    void paintChildren(PaintInfo& forSelf, const LayoutPoint&, PaintInfo& forChild, bool usePrintRect) override;

    bool willStretchItem(const RenderBox& item, LogicalBoxAxis containingAxis, StretchingMode = StretchingMode::Normal) const override;

    LayoutOptionalOutsets allowedLayoutOverflow() const override;

    virtual bool isFlexibleBoxImpl() const { return false; };

    // Used by InspectorOverlay; a thin proxy to FlexFormattingUtils, which stays internal to the flex formatting context.
    using GapType = FlexFormattingUtils::GapType;
    LayoutUnit computeGap(GapType) const;

    std::optional<LayoutUnit> usedFlexItemOverridingLogicalHeightForPercentageResolution(const RenderBox&);
    bool canUseFlexItemForPercentageResolution(const RenderBox&);

    void invalidateBlockAxisSizeForFlexItem(const RenderBox& flexItem);

    void setFlexItemContentLogicalHeightFromLayout(const RenderBox& flexItem, LayoutUnit height);

    // Returns true if the position changed. In that case, the flexItem will have to be laid out again.
    bool setStaticPositionForPositionedLayout(const RenderBox&);

    bool NODELETE isComputingFlexBaseSizes() const;
    bool NODELETE isInCrossAxisStretchLayout() const;

protected:
    std::pair<LayoutUnit, LayoutUnit> computeIntrinsicLogicalWidths() const override;

private:
    friend class LayoutIntegration::FlexLayout;
    friend class LayoutIntegration::FlexItemIntrinsicWidthComputationScope;

    bool isChildEligibleForMarginTrim(Style::MarginTrimSide, const RenderBox&) const final;

    void clearFlexItemOverridingSizes();

    // The flex items' border box rects as they were before the flex algorithm ran, so layoutBlock can repaint the
    // ones it moved. Both walk the in-flow children directly (the flex item list is built inside FlexLayout::layout).
    using FlexItemBorderBoxRects = Vector<LayoutRect, 4>;
    FlexItemBorderBoxRects flexItemBorderBoxRects() const;
    void repaintFlexItemsDuringLayoutIfMoved(const FlexItemBorderBoxRects&);


    // The flex formatting context integration: RenderFlexibleBox owns it and befriends it so it can reach the
    // container's layout-phase state.
    LayoutIntegration::FlexLayout m_flexLayout { *this };

    bool m_inSimplifiedLayout { false };
    mutable bool m_inFlexItemIntrinsicWidthComputation { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderFlexibleBox, isRenderFlexibleBox())
