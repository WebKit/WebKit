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

    const char* renderName() const override;

    bool avoidsFloats() const final { return true; }
    bool canDropAnonymousBlockChild() const final { return false; }
    void layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight = 0_lu) final;

    int baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const override;
    Optional<int> firstLineBaseline() const override;
    Optional<int> inlineBlockBaseline(LineDirectionMode) const override;

    void styleDidChange(StyleDifference, const RenderStyle*) override;
    bool hitTestChildren(const HitTestRequest&, HitTestResult&, const HitTestLocation&, const LayoutPoint& adjustedLocation, HitTestAction) override;
    void paintChildren(PaintInfo& forSelf, const LayoutPoint&, PaintInfo& forChild, bool usePrintRect) override;

    bool isHorizontalFlow() const;

    const OrderIterator& orderIterator() const { return m_orderIterator; }

    bool isTopLayoutOverflowAllowed() const override;
    bool isLeftLayoutOverflowAllowed() const override;

    virtual bool isFlexibleBoxImpl() const { return false; };
    
    Optional<LayoutUnit> childLogicalHeightForPercentageResolution(const RenderBox&);
    
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

protected:
    void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const override;
    void computePreferredLogicalWidths() override;

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
    
    bool hasOrthogonalFlow(const RenderBox& child) const;
    bool isColumnFlow() const;
    bool isLeftToRightFlow() const;
    bool isMultiline() const;
    Length flexBasisForChild(const RenderBox& child) const;
    bool shouldApplyMinSizeAutoForChild(const RenderBox&) const;
    LayoutUnit crossAxisExtentForChild(const RenderBox& child) const;
    LayoutUnit crossAxisIntrinsicExtentForChild(const RenderBox& child) const;
    LayoutUnit childIntrinsicLogicalHeight(const RenderBox& child) const;
    LayoutUnit childIntrinsicLogicalWidth(const RenderBox& child) const;
    LayoutUnit mainAxisExtentForChild(const RenderBox& child) const;
    LayoutUnit mainAxisContentExtentForChildIncludingScrollbar(const RenderBox& child) const;
    LayoutUnit crossAxisExtent() const;
    LayoutUnit mainAxisExtent() const;
    LayoutUnit crossAxisContentExtent() const;
    LayoutUnit mainAxisContentExtent(LayoutUnit contentLogicalHeight);
    Optional<LayoutUnit> computeMainAxisExtentForChild(const RenderBox& child, SizeType, const Length& size);
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
    LayoutUnit crossAxisScrollbarExtent() const;
    LayoutUnit crossAxisScrollbarExtentForChild(const RenderBox& child) const;
    LayoutPoint flowAwareLocationForChild(const RenderBox& child) const;
    bool useChildAspectRatio(const RenderBox& child) const;
    LayoutUnit computeMainSizeFromAspectRatioUsing(const RenderBox& child, Length crossSizeLength) const;
    void setFlowAwareLocationForChild(RenderBox& child, const LayoutPoint&);
    LayoutUnit computeInnerFlexBaseSizeForChild(RenderBox& child, LayoutUnit mainAxisBorderAndPadding, bool relayoutChildren);
    void adjustAlignmentForChild(RenderBox& child, LayoutUnit);
    ItemPosition alignmentForChild(const RenderBox& child) const;
    bool mainAxisLengthIsDefinite(const RenderBox& child, const Length& flexBasis) const;
    bool crossAxisLengthIsDefinite(const RenderBox& child, const Length& flexBasis) const;
    bool needToStretchChildLogicalHeight(const RenderBox& child) const;
    bool childHasIntrinsicMainAxisSize(const RenderBox& child) const;
    Overflow mainAxisOverflowForChild(const RenderBox& child) const;
    Overflow crossAxisOverflowForChild(const RenderBox& child) const;
    void cacheChildMainSize(const RenderBox& child);
    Optional<LayoutUnit> crossSizeForPercentageResolution(const RenderBox&);
    Optional<LayoutUnit> mainSizeForPercentageResolution(const RenderBox&);

    void layoutFlexItems(bool relayoutChildren);
    LayoutUnit autoMarginOffsetInMainAxis(const Vector<FlexItem>&, LayoutUnit& availableFreeSpace);
    void updateAutoMarginsInMainAxis(RenderBox& child, LayoutUnit autoMarginOffset);
    bool hasAutoMarginsInCrossAxis(const RenderBox& child) const;
    bool updateAutoMarginsInCrossAxis(RenderBox& child, LayoutUnit availableAlignmentSpace);
    void repositionLogicalHeightDependentFlexItems(Vector<LineContext>&);
    LayoutUnit clientLogicalBottomAfterRepositioning();
    
    LayoutUnit availableAlignmentSpaceForChild(LayoutUnit lineCrossAxisExtent, const RenderBox& child);
    LayoutUnit marginBoxAscentForChild(const RenderBox& child);
    
    LayoutUnit computeChildMarginValue(Length margin);
    void prepareOrderIteratorAndMargins();
    LayoutUnit adjustChildSizeForMinAndMax(const RenderBox& child, LayoutUnit childSize);
    LayoutUnit adjustChildSizeForAspectRatioCrossAxisMinAndMax(const RenderBox& child, LayoutUnit childSize);
    FlexItem constructFlexItem(RenderBox&, bool relayoutChildren);
    
    void freezeInflexibleItems(FlexSign, Vector<FlexItem>& children, LayoutUnit& remainingFreeSpace, double& totalFlexGrow, double& totalFlexShrink, double& totalWeightedFlexShrink);
    bool resolveFlexibleLengths(FlexSign, Vector<FlexItem>&, LayoutUnit initialFreeSpace, LayoutUnit& remainingFreeSpace, double& totalFlexGrow, double& totalFlexShrink, double& totalWeightedFlexShrink);
    void freezeViolations(Vector<FlexItem*>&, LayoutUnit& availableFreeSpace, double& totalFlexGrow, double& totalFlexShrink, double& totalWeightedFlexShrink);
    
    void resetAutoMarginsAndLogicalTopInCrossAxis(RenderBox& child);
    void setOverrideMainAxisContentSizeForChild(RenderBox& child, LayoutUnit childPreferredSize);
    void prepareChildForPositionedLayout(RenderBox& child);
    void layoutAndPlaceChildren(LayoutUnit& crossAxisOffset, Vector<FlexItem>&, LayoutUnit availableFreeSpace, bool relayoutChildren, Vector<LineContext>&);
    void layoutColumnReverse(const Vector<FlexItem>&, LayoutUnit crossAxisOffset, LayoutUnit availableFreeSpace);
    void alignFlexLines(Vector<LineContext>&);
    void alignChildren(const Vector<LineContext>&);
    void applyStretchAlignmentToChild(RenderBox& child, LayoutUnit lineCrossAxisExtent);
    void flipForRightToLeftColumn(const Vector<LineContext>& lineContexts);
    void flipForWrapReverse(const Vector<LineContext>&, LayoutUnit crossAxisStartEdge);
    
    void appendChildFrameRects(ChildFrameRects&);
    void repaintChildrenDuringLayoutIfMoved(const ChildFrameRects&);

    bool hasPercentHeightDescendants(const RenderBox&) const;

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
    Vector<RenderBox*> m_reversedOrderIteratorForHitTesting;
    int m_numberOfInFlowChildrenOnFirstLine { -1 };
    
    // This is SizeIsUnknown outside of layoutBlock()
    mutable SizeDefiniteness m_hasDefiniteHeight { SizeDefiniteness::Unknown };
    bool m_inLayout { false };
    bool m_shouldResetChildLogicalHeightBeforeLayout { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderFlexibleBox, isFlexibleBox())
