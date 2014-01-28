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

#ifndef RenderFlexibleBox_h
#define RenderFlexibleBox_h

#include "OrderIterator.h"
#include "RenderBlock.h"

namespace WebCore {

class RenderFlexibleBox : public RenderBlock {
public:
    RenderFlexibleBox(Element&, PassRef<RenderStyle>);
    RenderFlexibleBox(Document&, PassRef<RenderStyle>);
    virtual ~RenderFlexibleBox();

    virtual const char* renderName() const override;

    virtual bool isFlexibleBox() const override final { return true; }
    virtual bool avoidsFloats() const override final { return true; }
    virtual bool canCollapseAnonymousBlockChild() const override final { return false; }
    virtual void layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight = 0) override final;

    virtual int baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const override;
    virtual int firstLineBaseline() const override;
    virtual int inlineBlockBaseline(LineDirectionMode) const override;

    virtual void paintChildren(PaintInfo& forSelf, const LayoutPoint&, PaintInfo& forChild, bool usePrintRect) override;

    bool isHorizontalFlow() const;

    bool isTopLayoutOverflowAllowed() const override;
    bool isLeftLayoutOverflowAllowed() const override;

protected:
    virtual void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const override;
    virtual void computePreferredLogicalWidths() override;

    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

private:
    enum FlexSign {
        PositiveFlexibility,
        NegativeFlexibility,
    };

    enum PositionedLayoutMode {
        FlipForRowReverse,
        NoFlipForRowReverse,
    };

    typedef HashMap<const RenderBox*, LayoutUnit> InflexibleFlexItemSize;
    typedef Vector<RenderBox*> OrderedFlexItemList;

    struct LineContext;
    struct Violation;

    // Use an inline capacity of 8, since flexbox containers usually have less than 8 children.
    typedef Vector<LayoutRect, 8> ChildFrameRects;

    bool hasOrthogonalFlow(RenderBox& child) const;
    bool isColumnFlow() const;
    bool isLeftToRightFlow() const;
    bool isMultiline() const;
    Length flexBasisForChild(RenderBox& child) const;
    void setCrossAxisExtent(LayoutUnit);
    LayoutUnit crossAxisExtentForChild(RenderBox& child) const;
    LayoutUnit mainAxisExtentForChild(RenderBox& child) const;
    LayoutUnit crossAxisExtent() const;
    LayoutUnit mainAxisExtent() const;
    LayoutUnit crossAxisContentExtent() const;
    LayoutUnit mainAxisContentExtent(LayoutUnit contentLogicalHeight);
    LayoutUnit computeMainAxisExtentForChild(RenderBox& child, SizeType, const Length& size);
    WritingMode transformedWritingMode() const;
    LayoutUnit flowAwareBorderStart() const;
    LayoutUnit flowAwareBorderEnd() const;
    LayoutUnit flowAwareBorderBefore() const;
    LayoutUnit flowAwareBorderAfter() const;
    LayoutUnit flowAwarePaddingStart() const;
    LayoutUnit flowAwarePaddingEnd() const;
    LayoutUnit flowAwarePaddingBefore() const;
    LayoutUnit flowAwarePaddingAfter() const;
    LayoutUnit flowAwareMarginStartForChild(RenderBox& child) const;
    LayoutUnit flowAwareMarginEndForChild(RenderBox& child) const;
    LayoutUnit flowAwareMarginBeforeForChild(RenderBox& child) const;
    LayoutUnit flowAwareMarginAfterForChild(RenderBox& child) const;
    LayoutUnit crossAxisMarginExtentForChild(RenderBox& child) const;
    LayoutUnit crossAxisScrollbarExtent() const;
    LayoutPoint flowAwareLocationForChild(RenderBox& child) const;
    // FIXME: Supporting layout deltas.
    void setFlowAwareLocationForChild(RenderBox& child, const LayoutPoint&);
    void adjustAlignmentForChild(RenderBox& child, LayoutUnit);
    EAlignItems alignmentForChild(RenderBox& child) const;
    LayoutUnit mainAxisBorderAndPaddingExtentForChild(RenderBox& child) const;
    LayoutUnit mainAxisScrollbarExtentForChild(RenderBox& child) const;
    LayoutUnit preferredMainAxisContentExtentForChild(RenderBox& child, bool hasInfiniteLineLength);

    void layoutFlexItems(bool relayoutChildren, Vector<LineContext>&);
    LayoutUnit autoMarginOffsetInMainAxis(const OrderedFlexItemList&, LayoutUnit& availableFreeSpace);
    void updateAutoMarginsInMainAxis(RenderBox& child, LayoutUnit autoMarginOffset);
    bool hasAutoMarginsInCrossAxis(RenderBox& child) const;
    bool updateAutoMarginsInCrossAxis(RenderBox& child, LayoutUnit availableAlignmentSpace);
    void repositionLogicalHeightDependentFlexItems(Vector<LineContext>&);
    LayoutUnit clientLogicalBottomAfterRepositioning();
    void appendChildFrameRects(ChildFrameRects&);
    void repaintChildrenDuringLayoutIfMoved(const ChildFrameRects&);

    LayoutUnit availableAlignmentSpaceForChild(LayoutUnit lineCrossAxisExtent, RenderBox&);
    LayoutUnit marginBoxAscentForChild(RenderBox&);

    LayoutUnit computeChildMarginValue(const Length& margin);
    void computeMainAxisPreferredSizes(OrderIterator::OrderValues&);
    LayoutUnit adjustChildSizeForMinAndMax(RenderBox&, LayoutUnit childSize);
    bool computeNextFlexLine(OrderedFlexItemList& orderedChildren, LayoutUnit& preferredMainAxisExtent, double& totalFlexGrow, double& totalWeightedFlexShrink, LayoutUnit& minMaxAppliedMainAxisExtent, bool& hasInfiniteLineLength);

    bool resolveFlexibleLengths(FlexSign, const OrderedFlexItemList&, LayoutUnit& availableFreeSpace, double& totalFlexGrow, double& totalWeightedFlexShrink, InflexibleFlexItemSize&, Vector<LayoutUnit>& childSizes, bool hasInfiniteLineLength);
    void freezeViolations(const Vector<Violation>&, LayoutUnit& availableFreeSpace, double& totalFlexGrow, double& totalWeightedFlexShrink, InflexibleFlexItemSize&, bool hasInfiniteLineLength);

    void resetAutoMarginsAndLogicalTopInCrossAxis(RenderBox&);
    bool needToStretchChild(RenderBox&);
    void setLogicalOverrideSize(RenderBox& child, LayoutUnit childPreferredSize);
    void prepareChildForPositionedLayout(RenderBox& child, LayoutUnit mainAxisOffset, LayoutUnit crossAxisOffset, PositionedLayoutMode);
    size_t numberOfInFlowPositionedChildren(const OrderedFlexItemList&) const;
    void layoutAndPlaceChildren(LayoutUnit& crossAxisOffset, const OrderedFlexItemList&, const Vector<LayoutUnit>& childSizes, LayoutUnit availableFreeSpace, bool relayoutChildren, Vector<LineContext>&);
    void layoutColumnReverse(const OrderedFlexItemList&, LayoutUnit crossAxisOffset, LayoutUnit availableFreeSpace);
    void alignFlexLines(Vector<LineContext>&);
    void alignChildren(const Vector<LineContext>&);
    void applyStretchAlignmentToChild(RenderBox&, LayoutUnit lineCrossAxisExtent);
    void flipForRightToLeftColumn();
    void flipForWrapReverse(const Vector<LineContext>&, LayoutUnit crossAxisStartEdge);

    mutable OrderIterator m_orderIterator;
    int m_numberOfInFlowChildrenOnFirstLine;
};

RENDER_OBJECT_TYPE_CASTS(RenderFlexibleBox, isFlexibleBox())

} // namespace WebCore

#endif // RenderFlexibleBox_h
