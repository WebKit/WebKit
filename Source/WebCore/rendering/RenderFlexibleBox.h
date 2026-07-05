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
#include <WebCore/FlexLayoutUtils.h>
#include <WebCore/OrderIterator.h>
#include <WebCore/RenderBlock.h>
#include <WebCore/RenderFlexLayout.h>
#include <wtf/Range.h>
#include <wtf/SetForScope.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {

namespace LayoutIntegration {
class FlexLayout;
}

class RenderFlexibleBox : public RenderBlock {
    WTF_MAKE_TZONE_ALLOCATED(RenderFlexibleBox);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderFlexibleBox);
public:
    const FlexLayoutUtils& flexLayoutUtils() const LIFETIME_BOUND { return m_flexLayoutUtils; }

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

    const OrderIterator& orderIterator() const LIFETIME_BOUND { return m_orderIterator; }

    LayoutOptionalOutsets allowedLayoutOverflow() const override;

    virtual bool isFlexibleBoxImpl() const { return false; };

    std::optional<LayoutUnit> usedFlexItemOverridingLogicalHeightForPercentageResolution(const RenderBox&);
    bool canUseFlexItemForPercentageResolution(const RenderBox&);

    void invalidateBlockAxisSizeForFlexItem(const RenderBox& flexItem);
    void flexItemWillBeRemoved(const RenderBox& flexItem);

    LayoutUnit flexItemContentLogicalHeight(const RenderBox& flexItem) const;
    void setFlexItemContentLogicalHeightIfNeeded(const RenderBox& flexItem, LayoutUnit height);

    LayoutUnit staticMainAxisPositionForPositionedFlexItem(const RenderBox&);
    LayoutUnit staticCrossAxisPositionForPositionedFlexItem(const RenderBox&);

    LayoutUnit staticInlinePositionForPositionedFlexItem(const RenderBox&);
    LayoutUnit staticBlockPositionForPositionedFlexItem(const RenderBox&);

    // Returns true if the position changed. In that case, the flexItem will have to
    // be laid out again.
    bool setStaticPositionForPositionedLayout(const RenderBox&);

    bool isComputingFlexBaseSizes() const { return m_isComputingFlexBaseSizes; }

    bool hasModernLayout() const { return m_hasFlexFormattingContextLayout && *m_hasFlexFormattingContextLayout; }

    bool shouldResetFlexItemLogicalHeightBeforeLayout() const { return m_shouldResetFlexItemLogicalHeightBeforeLayout; }
    bool isInCrossAxisStretchLayout() const { return m_inLayout && m_afterCrossAxisItemSizing; }

    class OverridingSizesScope {
    public:
        enum class Axis { Inline, Block, Both };

        OverridingSizesScope(RenderBox&, Axis, std::optional<LayoutUnit> size = std::nullopt);
        ~OverridingSizesScope();

    private:
        RenderBox& m_box;
        Axis m_axis;
        std::optional<LayoutUnit> m_previousOverridingBorderBoxLogicalWidth;
        std::optional<LayoutUnit> m_previousOverridingBorderBoxLogicalHeight;
    };

    class ScopedCrossAxisOverrideForFlexItem {
    public:
        enum class InvalidateContentWidths : bool { No, Yes };
        ScopedCrossAxisOverrideForFlexItem(const RenderFlexibleBox&, RenderBox& flexItem, InvalidateContentWidths);
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
    friend class FlexLayout;
    friend class FlexLayoutUtils;

    enum class SizeDefiniteness : uint8_t { Definite, Indefinite, Unknown };

    using FlexItemBorderBoxRects = Vector<LayoutRect, 4>;

    void appendFlexItemBorderBoxRects(FlexItemBorderBoxRects&);
    void repaintFlexItemsDuringLayoutIfMoved(const FlexItemBorderBoxRects&);

    template<typename SizeType> bool canComputePercentageFlexBasis(const RenderBox& flexItem, const SizeType&, UpdatePercentageHeightDescendants);
    template<typename SizeType> bool flexItemMainSizeIsDefinite(const RenderBox&, const SizeType&);

    void initializeMarginTrimState();
    bool isChildEligibleForMarginTrim(Style::MarginTrimSide, const RenderBox&) const final;

    void clearFlexItemOverridingSizes();

    const RenderBox* flexItemForFirstBaseline() const;
    const RenderBox* flexItemForLastBaseline() const;
    const RenderBox* firstBaselineCandidateOnLine(OrderIterator, size_t numberOfItemsOnLine) const;
    const RenderBox* lastBaselineCandidateOnLine(OrderIterator, size_t numberOfItemsOnLine) const;

    void prepareOrderIteratorAndMargins();

    // Builds this container's flex items (its formatting-context children) for FlexLayout to lay out.
    FlexLayoutItems collectFlexItems(RelayoutChildren);
    FlexLayoutConstraints flexLayoutConstraints();
    LayoutUnit mainAxisAvailableSpace();
    void setLogicalHeightForRowFlexContent(LayoutUnit contentLogicalHeight);
    void finalizeFlexContainerLogicalHeight(std::optional<LayoutUnit> minimumHeightForLineIfEmpty);
    void prepareFlexItemForPositionedLayout(RenderBox& flexItem);
    void adjustLogicalHeightForLineIfEmpty();
    std::optional<LayoutUnit> minimumHeightForLineIfEmpty() const;

    void resetHasDefiniteHeight() { m_hasDefiniteHeight = SizeDefiniteness::Unknown; }

    // The flex layout pipeline (FlexLayout) reaches the container's layout-phase state through these
    // rather than writing the members directly, so the state stays owned by RenderFlexibleBox.
    SetForScope<bool> scopedComputingFlexBaseSizes() { return SetForScope(m_isComputingFlexBaseSizes, true); }
    SetForScope<bool> scopedAfterMainAxisItemSizing() { return SetForScope(m_afterMainAxisItemSizing, true); }
    SetForScope<bool> scopedAfterCrossAxisItemSizing() { return SetForScope(m_afterCrossAxisItemSizing, true); }
    SetForScope<bool> scopedResetFlexItemLogicalHeightBeforeLayout() { return SetForScope(m_shouldResetFlexItemLogicalHeightBeforeLayout, true); }
    SizeDefiniteness hasDefiniteHeight() const { return m_hasDefiniteHeight; }
    void setHasDefiniteHeight(SizeDefiniteness definiteness) { m_hasDefiniteHeight = definiteness; }
    void setBlockAxisSizeForFlexItem(const RenderBox& flexItem, LayoutUnit size) { m_blockAxisSize.set(flexItem, size); }
    std::optional<LayoutUnit> blockAxisSizeForFlexItem(const RenderBox& flexItem) const { return m_blockAxisSize.getOptional(flexItem); }
    void cacheFlexItemContentLogicalHeightIfAllowed(const RenderBox& flexItem, LayoutUnit height);
    LayoutUnit computeBlockAxisContentSizeForFlexItem(RenderBox& flexItem);
    void stretchFlexItemLogicalHeight(RenderBox& flexItem, LayoutUnit desiredLogicalHeight, bool needsRelayout);
    void relayoutFlexItemForStretchedCrossSize(RenderBox& flexItem, LayoutUnit crossSize, LogicalBoxAxis crossAxis);
    void dirtyPercentHeightDescendantsWithinFlexItem(RenderBox& flexItem);
    void layoutFlexItemAfterMainSizing(FlexLayoutItem&, LayoutUnit mainSize, RelayoutChildren);
    void setOverridingMainSizeForFlexItem(RenderBox& flexItem, LayoutUnit mainSize);
    void resetAutoMarginsAndLogicalTopInCrossAxis(RenderBox& flexItem);
    bool flexItemHasPercentHeightDescendants(const RenderBox&) const;
    void markFlexItemLayoutComplete(const RenderBox& flexItem) { m_flexItemsWithCompletedLayout.add(flexItem); }
    bool hasFlexItemCompletedLayout(const RenderBox& flexItem) const { return m_flexItemsWithCompletedLayout.contains(flexItem); }
    void addItemAtFlexLineStart(const RenderBox& flexItem) { m_marginTrimItems.m_itemsAtFlexLineStart.add(flexItem); }
    void addItemAtFlexLineEnd(const RenderBox& flexItem) { m_marginTrimItems.m_itemsAtFlexLineEnd.add(flexItem); }
    void addItemOnFirstFlexLine(const RenderBox& flexItem) { m_marginTrimItems.m_itemsOnFirstFlexLine.add(flexItem); }
    void addItemOnLastFlexLine(const RenderBox& flexItem) { m_marginTrimItems.m_itemsOnLastFlexLine.add(flexItem); }

    bool layoutUsingFlexFormattingContext();

    // Inner main size for flex items where main axis is the item's block axis (column flex or orthogonal).
    HashMap<SingleThreadWeakRef<const RenderBox>, LayoutUnit> m_blockAxisSize;

    // This is used to cache the intrinsic size on the cross axis to avoid
    // relayouts when stretching.
    HashMap<SingleThreadWeakRef<const RenderBox>, LayoutUnit> m_contentLogicalHeights;

    // This set is used to keep track of which children we laid out in this
    // current layout iteration. We need it because the ones in this set may
    // need an additional layout pass for correct stretch alignment handling, as
    // the first layout likely did not use the correct value for percentage
    // sizing of children.
    SingleThreadWeakHashSet<const RenderBox> m_flexItemsWithCompletedLayout;

    mutable OrderIterator m_orderIterator { *this };
    const FlexLayoutUtils m_flexLayoutUtils { *this };
    size_t m_numberOfFlexItemsOnFirstLine { 0 };
    size_t m_numberOfFlexItemsOnLastLine { 0 };

    struct MarginTrimItems {
        SingleThreadWeakHashSet<const RenderBox> m_itemsAtFlexLineStart;
        SingleThreadWeakHashSet<const RenderBox> m_itemsAtFlexLineEnd;
        SingleThreadWeakHashSet<const RenderBox> m_itemsOnFirstFlexLine;
        SingleThreadWeakHashSet<const RenderBox> m_itemsOnLastFlexLine;
    } m_marginTrimItems;

    LayoutUnit m_alignContentStartOverflow { 0 };
    LayoutUnit m_justifyContentStartOverflow { 0 };

    // This is SizeIsUnknown outside of layoutBlock()
    SizeDefiniteness m_hasDefiniteHeight { SizeDefiniteness::Unknown };
    bool m_inLayout { false };
    bool m_afterMainAxisItemSizing { false };
    bool m_afterCrossAxisItemSizing { false };
    bool m_inSimplifiedLayout { false };
    bool m_inPostFlexUpdateScrollbarLayout { false };
    mutable bool m_inFlexItemIntrinsicWidthComputation { false };
    bool m_shouldResetFlexItemLogicalHeightBeforeLayout { false };
    bool m_isComputingFlexBaseSizes { false };
    std::optional<bool> m_hasFlexFormattingContextLayout;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderFlexibleBox, isRenderFlexibleBox())
