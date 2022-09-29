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

#include "OrderIterator.h"
#include "RenderBlock.h"

namespace WebCore {

class FlexItem;
    
class RenderFlexibleBox : public RenderBlock {
    WTF_MAKE_ISO_ALLOCATED(RenderFlexibleBox);
public:
    RenderFlexibleBox(Element&, RenderStyle&&);
    RenderFlexibleBox(Document&, RenderStyle&&);
    virtual ~RenderFlexibleBox();

    bool isFlexibleBox() const override { return true; }

    ASCIILiteral renderName() const override;

    bool avoidsFloats() const final { return true; }
    bool canDropAnonymousBlockChild() const final { return false; }
    void layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight = 0_lu) final;

    LayoutUnit baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const override;
    std::optional<LayoutUnit> firstLineBaseline() const override;
    std::optional<LayoutUnit> inlineBlockBaseline(LineDirectionMode) const override;

    void styleDidChange(StyleDifference, const RenderStyle*) override;
    bool hitTestChildren(const HitTestRequest&, HitTestResult&, const HitTestLocation&, const LayoutPoint& adjustedLocation, HitTestAction) override;
    void paintChildren(PaintInfo& forSelf, const LayoutPoint&, PaintInfo& forChild, bool usePrintRect) override;

    bool isHorizontalFlow() const;

    const OrderIterator& orderIterator() const { return m_orderIterator; }

    bool isTopLayoutOverflowAllowed() const override;
    bool isLeftLayoutOverflowAllowed() const override;

    virtual bool isFlexibleBoxImpl() const { return false; };
    
    bool useChildOverridingLogicalHeightForPercentageResolution(const RenderBox&);
    
    void clearCachedMainSizeForChild(const RenderBox& child);
    
    LayoutUnit cachedChildIntrinsicContentLogicalHeight(const RenderBox& child) const;
    void setCachedChildIntrinsicContentLogicalHeight(const RenderBox& child, LayoutUnit);
    void clearCachedChildIntrinsicContentLogicalHeight(const RenderBox& child);

    LayoutUnit staticMainAxisPositionForPositionedChild(const RenderBox&);
    LayoutUnit staticCrossAxisPositionForPositionedChild(const RenderBox&);
    
    LayoutUnit staticInlinePositionForPositionedChild(const RenderBox&);
    LayoutUnit staticBlockPositionForPositionedChild(const RenderBox&);
    
    // Returns true if the position changed. In that case, the child will have to
    // be laid out again.
    bool setStaticPositionForPositionedLayout(const RenderBox&);

    enum class GapType { BetweenLines, BetweenItems };
    LayoutUnit computeGap(GapType) const;

    bool shouldApplyMinBlockSizeAutoForChild(const RenderBox&) const;

    bool isComputingFlexBaseSizes() const { return m_isComputingFlexBaseSizes; }

protected:
    void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const override;

    bool shouldResetChildLogicalHeightBeforeLayout(const RenderBox&) const override { return m_shouldResetChildLogicalHeightBeforeLayout; }

private:
    enum FlexSign {
        PositiveFlexibility,
        NegativeFlexibility,
    };
    
    enum ChildLayoutType { LayoutIfNeeded, ForceLayout, NeverLayout };
    
    enum class SizeDefiniteness { Definite, Indefinite, Unknown };
    
    // Use an inline capacity of 8, since flexbox containers usually have less than 8 children.
    typedef Vector<LayoutRect, 8> ChildFrameRects;

    struct LineContext;
    
    bool mainAxisIsChildInlineAxis(const RenderBox&) const;
    bool isColumnFlow() const;
    bool isColumnOrRowReverse() const;
    bool isLeftToRightFlow() const;
    bool isMultiline() const;
    Length flexBasisForChild(const RenderBox& child) const;
    Length mainSizeLengthForChild(SizeType, const RenderBox&) const;
    Length crossSizeLengthForChild(SizeType, const RenderBox&) const;
    bool shouldApplyMinSizeAutoForChild(const RenderBox&) const;
    LayoutUnit crossAxisExtentForChild(const RenderBox& child) const;
    LayoutUnit crossAxisIntrinsicExtentForChild(RenderBox& child);
    LayoutUnit childIntrinsicLogicalHeight(RenderBox& child) const;
    LayoutUnit childIntrinsicLogicalWidth(RenderBox& child);
    LayoutUnit mainAxisExtentForChild(const RenderBox& child) const;
    LayoutUnit mainAxisContentExtentForChildIncludingScrollbar(const RenderBox& child) const;
    LayoutUnit crossAxisExtent() const;
    LayoutUnit mainAxisExtent() const;
    LayoutUnit crossAxisContentExtent() const;
    LayoutUnit mainAxisContentExtent(LayoutUnit contentLogicalHeight);
    std::optional<LayoutUnit> computeMainAxisExtentForChild(RenderBox& child, SizeType, const Length& size);
    WritingMode transformedWritingMode() const;
    LayoutUnit flowAwareBorderStart() const;
    LayoutUnit flowAwareBorderEnd() const;
    LayoutUnit flowAwareBorderBefore() const;
    LayoutUnit flowAwareBorderAfter() const;
    LayoutUnit flowAwarePaddingStart() const;
    LayoutUnit flowAwarePaddingEnd() const;
    LayoutUnit flowAwarePaddingBefore() const;
    LayoutUnit flowAwarePaddingAfter() const;
    LayoutUnit flowAwareMarginStartForChild(const RenderBox& child) const;
    LayoutUnit flowAwareMarginEndForChild(const RenderBox& child) const;
    LayoutUnit flowAwareMarginBeforeForChild(const RenderBox& child) const;
    LayoutUnit crossAxisMarginExtentForChild(const RenderBox& child) const;
    LayoutUnit mainAxisMarginExtentForChild(const RenderBox& child) const;
    LayoutUnit crossAxisScrollbarExtent() const;
    LayoutUnit crossAxisScrollbarExtentForChild(const RenderBox& child) const;
    LayoutPoint flowAwareLocationForChild(const RenderBox& child) const;
    bool childHasComputableAspectRatio(const RenderBox&) const;
    bool childHasComputableAspectRatioAndCrossSizeIsConsideredDefinite(const RenderBox&);
    bool crossAxisIsPhysicalWidth() const;
    bool childCrossSizeShouldUseContainerCrossSize(const RenderBox& child) const;
    LayoutUnit computeCrossSizeForChildUsingContainerCrossSize(const RenderBox& child) const;
    void computeChildIntrinsicLogicalWidths(RenderObject&, LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const override;
    LayoutUnit computeMainSizeFromAspectRatioUsing(const RenderBox& child, Length crossSizeLength) const;
    void setFlowAwareLocationForChild(RenderBox& child, const LayoutPoint&);
    LayoutUnit computeFlexBaseSizeForChild(RenderBox& child, LayoutUnit mainAxisBorderAndPadding, bool relayoutChildren);
    void maybeCacheChildMainIntrinsicSize(RenderBox& child, bool relayoutChildren);
    void adjustAlignmentForChild(RenderBox& child, LayoutUnit);
    ItemPosition alignmentForChild(const RenderBox& child) const;
    bool canComputePercentageFlexBasis(const RenderBox& child, const Length& flexBasis, UpdatePercentageHeightDescendants);
    bool childMainSizeIsDefinite(const RenderBox&, const Length& flexBasis);
    bool childCrossSizeIsDefinite(const RenderBox&, const Length& flexBasis);
    bool needToStretchChildLogicalHeight(const RenderBox& child) const;
    bool childHasIntrinsicMainAxisSize(const RenderBox& child);
    Overflow mainAxisOverflowForChild(const RenderBox& child) const;
    Overflow crossAxisOverflowForChild(const RenderBox& child) const;
    void cacheChildMainSize(const RenderBox& child);
    bool useChildOverridingCrossSizeForPercentageResolution(const RenderBox&);
    bool useChildOverridingMainSizeForPercentageResolution(const RenderBox&);

    void layoutFlexItems(bool relayoutChildren);
    LayoutUnit autoMarginOffsetInMainAxis(const Vector<FlexItem>&, LayoutUnit& availableFreeSpace);
    void updateAutoMarginsInMainAxis(RenderBox& child, LayoutUnit autoMarginOffset);
    bool hasAutoMarginsInCrossAxis(const RenderBox& child) const;
    bool updateAutoMarginsInCrossAxis(RenderBox& child, LayoutUnit availableAlignmentSpace);
    void repositionLogicalHeightDependentFlexItems(Vector<LineContext>&, LayoutUnit gapBetweenLines);
    
    LayoutUnit availableAlignmentSpaceForChild(LayoutUnit lineCrossAxisExtent, const RenderBox& child);
    LayoutUnit marginBoxAscentForChild(const RenderBox& child);
    
    LayoutUnit computeChildMarginValue(Length margin);
    void prepareOrderIteratorAndMargins();
    std::pair<LayoutUnit, LayoutUnit> computeFlexItemMinMaxSizes(RenderBox& child);
    LayoutUnit adjustChildSizeForAspectRatioCrossAxisMinAndMax(const RenderBox& child, LayoutUnit childSize);
    FlexItem constructFlexItem(RenderBox&, bool relayoutChildren);
    
    void freezeInflexibleItems(FlexSign, Vector<FlexItem>& children, LayoutUnit& remainingFreeSpace, double& totalFlexGrow, double& totalFlexShrink, double& totalWeightedFlexShrink);
    bool resolveFlexibleLengths(FlexSign, Vector<FlexItem>&, LayoutUnit initialFreeSpace, LayoutUnit& remainingFreeSpace, double& totalFlexGrow, double& totalFlexShrink, double& totalWeightedFlexShrink);
    void freezeViolations(Vector<FlexItem*>&, LayoutUnit& availableFreeSpace, double& totalFlexGrow, double& totalFlexShrink, double& totalWeightedFlexShrink);
    
    void resetAutoMarginsAndLogicalTopInCrossAxis(RenderBox& child);
    void setOverridingMainSizeForChild(RenderBox&, LayoutUnit);
    void prepareChildForPositionedLayout(RenderBox& child);
    void layoutAndPlaceChildren(LayoutUnit& crossAxisOffset, Vector<FlexItem>&, LayoutUnit availableFreeSpace, bool relayoutChildren, Vector<LineContext>&, LayoutUnit gapBetweenItems);
    void layoutColumnReverse(const Vector<FlexItem>&, LayoutUnit crossAxisOffset, LayoutUnit availableFreeSpace, LayoutUnit gapBetweenItems);
    void alignFlexLines(Vector<LineContext>&, LayoutUnit gapBetweenLines);
    void alignChildren(const Vector<LineContext>&);
    void applyStretchAlignmentToChild(RenderBox& child, LayoutUnit lineCrossAxisExtent);
    void flipForRightToLeftColumn(const Vector<LineContext>& lineContexts);
    void flipForWrapReverse(const Vector<LineContext>&, LayoutUnit crossAxisStartEdge);
    
    void appendChildFrameRects(ChildFrameRects&);
    void repaintChildrenDuringLayoutIfMoved(const ChildFrameRects&);

    bool childHasPercentHeightDescendants(const RenderBox&) const;

    void resetHasDefiniteHeight() { m_hasDefiniteHeight = SizeDefiniteness::Unknown; }

    void layoutUsingFlexFormattingContext();

    // This is used to cache the preferred size for orthogonal flow children so we
    // don't have to relayout to get it
    HashMap<const RenderBox*, LayoutUnit> m_intrinsicSizeAlongMainAxis;
    
    // This is used to cache the intrinsic size on the cross axis to avoid
    // relayouts when stretching.
    HashMap<const RenderBox*, LayoutUnit> m_intrinsicContentLogicalHeights;

    // This set is used to keep track of which children we laid out in this
    // current layout iteration. We need it because the ones in this set may
    // need an additional layout pass for correct stretch alignment handling, as
    // the first layout likely did not use the correct value for percentage
    // sizing of children.
    HashSet<const RenderBox*> m_relaidOutChildren;

    mutable OrderIterator m_orderIterator { *this };
    int m_numberOfInFlowChildrenOnFirstLine { -1 };
    
    // This is SizeIsUnknown outside of layoutBlock()
    SizeDefiniteness m_hasDefiniteHeight { SizeDefiniteness::Unknown };
    bool m_inLayout { false };
    bool m_shouldResetChildLogicalHeightBeforeLayout { false };
    bool m_isComputingFlexBaseSizes { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderFlexibleBox, isFlexibleBox())
