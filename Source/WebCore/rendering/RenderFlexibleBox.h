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
#include <WebCore/FlexFormattingContext.h>
#include <WebCore/FlexFormattingUtils.h>
#include <WebCore/FlexLayoutState.h>
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
    bool hitTestChildren(const HitTestRequest&, HitTestResult&, const HitTestLocation&, const LayoutPoint& adjustedLocation, HitTestAction) override;
    void paintChildren(PaintInfo& forSelf, const LayoutPoint&, PaintInfo& forChild, bool usePrintRect) override;

    bool willStretchItem(const RenderBox& item, LogicalBoxAxis containingAxis, StretchingMode = StretchingMode::Normal) const override;

    const Vector<SingleThreadWeakPtr<RenderBox>>& flexItems() const LIFETIME_BOUND { return m_flexItems; }

    LayoutOptionalOutsets allowedLayoutOverflow() const override;

    virtual bool isFlexibleBoxImpl() const { return false; };

    // Flex-container queries used by non-flex layout code (RenderBox/RenderBlock/RenderBlockFlow/InspectorOverlay);
    // thin proxies to FlexFormattingUtils, which stays internal to the flex formatting context.
    using GapType = FlexFormattingUtils::GapType;
    bool useContentBasedMinimumBlockSize(const RenderBox& flexItem) const;
    bool hasStretchedFlexItemWithAspectRatio() const;
    LayoutUnit computeGap(GapType) const;
    bool NODELETE isHorizontalFlow() const;
    bool isMultiline() const;
    bool mainAxisIsFlexItemInlineAxis(const RenderBox& flexItem) const;
    Style::FlexBasis flexBasisForFlexItem(const RenderBox& flexItem) const;
    ItemPosition alignmentForFlexItem(const RenderBox& flexItem) const;
    bool hasDefiniteCrossSizeForFlexItem(const RenderBox& flexItem) const;

    std::optional<LayoutUnit> usedFlexItemOverridingLogicalHeightForPercentageResolution(const RenderBox&);
    bool canUseFlexItemForPercentageResolution(const RenderBox&);

    void invalidateBlockAxisSizeForFlexItem(const RenderBox& flexItem);
    void flexItemWillBeRemoved(const RenderBox& flexItem);

    LayoutUnit flexItemContentLogicalHeight(const RenderBox& flexItem) const;
    void setFlexItemContentLogicalHeightIfNeeded(const RenderBox& flexItem, LayoutUnit height);

    // Returns true if the position changed. In that case, the flexItem will have to be laid out again.
    bool setStaticPositionForPositionedLayout(const RenderBox&);

    bool isComputingFlexBaseSizes() const { return m_flexLayoutState && m_flexLayoutState->phase() == FlexLayoutState::Phase::ComputingFlexBaseSizes; }

    bool isInCrossAxisStretchLayout() const { return m_flexLayoutState && m_flexLayoutState->phase() == FlexLayoutState::Phase::CrossAxisItemSizing; }

    class OverridingSizesScope {
    public:
        enum class Axis { Inline, Block, Both };

        OverridingSizesScope(RenderBox&, Axis, std::optional<LayoutUnit> size = std::nullopt);
        ~OverridingSizesScope();

    private:
        const CheckedRef<RenderBox> m_box;
        Axis m_axis;
        std::optional<LayoutUnit> m_previousOverridingBorderBoxLogicalWidth;
        std::optional<LayoutUnit> m_previousOverridingBorderBoxLogicalHeight;
    };

    class ScopedCrossAxisOverrideForFlexItem {
    public:
        enum class InvalidateContentWidths : bool { No, Yes };
        ScopedCrossAxisOverrideForFlexItem(RenderBox& flexItem, InvalidateContentWidths);
        ~ScopedCrossAxisOverrideForFlexItem();

    private:
        SetForScope<bool> m_intrinsicWidthComputation;
        std::optional<OverridingSizesScope> m_overridingScope;
#if ASSERT_ENABLED
        RenderBox& m_flexItem;
        bool m_didInvalidateContentLogicalWidths { false };
#endif
    };

protected:
    std::pair<LayoutUnit, LayoutUnit> computeIntrinsicLogicalWidths() const override;

private:
    friend class FlexFormattingContext;
    friend class FlexFormattingUtils;
    friend class LayoutIntegration::FlexLayout;
    friend class LayoutIntegration::FlexIntegrationUtils;

    using FlexItemBorderBoxRects = Vector<LayoutRect, 4>;

    void appendFlexItemBorderBoxRects(FlexItemBorderBoxRects&);
    void repaintFlexItemsDuringLayoutIfMoved(const FlexItemBorderBoxRects&);

    template<typename SizeType> bool canComputePercentageFlexBasis(const RenderBox& flexItem, const SizeType&, UpdatePercentageHeightDescendants);
    template<typename SizeType> bool flexItemMainSizeIsDefinite(const RenderBox&, const SizeType&);

    void initializeMarginTrimState();
    bool isChildEligibleForMarginTrim(Style::MarginTrimSide, const RenderBox&) const final;

    void clearFlexItemOverridingSizes();

    void prepareFlexItemsAndMargins();

    FlexContainerUsedExtents updateFlexContainerLogicalHeight(LayoutUnit flexContentBlockExtent);

    FlexLayoutState& flexLayoutState() LIFETIME_BOUND { ASSERT(m_flexLayoutState); return *m_flexLayoutState; }

    void setBlockAxisSizeForFlexItem(const RenderBox& flexItem, LayoutUnit size) { m_blockAxisSize.set(flexItem, size); }
    std::optional<LayoutUnit> blockAxisSizeForFlexItem(const RenderBox& flexItem) const { return m_blockAxisSize.getOptional(flexItem); }
    void cacheFlexItemContentLogicalHeightIfAllowed(const RenderBox& flexItem, LayoutUnit height);
    LayoutUnit computeBlockAxisContentSizeForFlexItem(RenderBox& flexItem);
    void dirtyPercentHeightDescendantsWithinFlexItem(RenderBox& flexItem);
    void resetAutoMarginsAndLogicalTopInCrossAxis(RenderBox& flexItem);
    bool flexItemHasPercentHeightDescendants(const RenderBox&) const;
    void addItemAtFlexLineStart(const RenderBox& flexItem) { m_marginTrimItems.m_itemsAtFlexLineStart.add(flexItem); }
    void addItemAtFlexLineEnd(const RenderBox& flexItem) { m_marginTrimItems.m_itemsAtFlexLineEnd.add(flexItem); }
    void addItemOnFirstFlexLine(const RenderBox& flexItem) { m_marginTrimItems.m_itemsOnFirstFlexLine.add(flexItem); }
    void addItemOnLastFlexLine(const RenderBox& flexItem) { m_marginTrimItems.m_itemsOnLastFlexLine.add(flexItem); }

    // Inner main size for flex items where main axis is the item's block axis (column flex or orthogonal).
    HashMap<SingleThreadWeakRef<const RenderBox>, LayoutUnit> m_blockAxisSize;

    // This is used to cache the intrinsic size on the cross axis to avoid
    // relayouts when stretching.
    HashMap<SingleThreadWeakRef<const RenderBox>, LayoutUnit> m_contentLogicalHeights;

    Vector<SingleThreadWeakPtr<RenderBox>> m_flexItems;
    // The flex formatting context integration: RenderFlexibleBox owns it and befriends it so it can reach the
    // container's layout-phase state.
    LayoutIntegration::FlexLayout m_flexLayout { *this };

    struct MarginTrimItems {
        SingleThreadWeakHashSet<const RenderBox> m_itemsAtFlexLineStart;
        SingleThreadWeakHashSet<const RenderBox> m_itemsAtFlexLineEnd;
        SingleThreadWeakHashSet<const RenderBox> m_itemsOnFirstFlexLine;
        SingleThreadWeakHashSet<const RenderBox> m_itemsOnLastFlexLine;
    } m_marginTrimItems;

    LayoutUnit m_alignContentStartOverflow { 0 };
    LayoutUnit m_justifyContentStartOverflow { 0 };

    std::optional<FlexLayoutState> m_flexLayoutState;
    bool m_inSimplifiedLayout { false };
    mutable bool m_inFlexItemIntrinsicWidthComputation { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderFlexibleBox, isRenderFlexibleBox())
