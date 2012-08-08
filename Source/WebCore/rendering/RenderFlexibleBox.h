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

#include "RenderBlock.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class RenderFlexibleBox : public RenderBlock {
public:
    RenderFlexibleBox(Node*);
    virtual ~RenderFlexibleBox();

    virtual const char* renderName() const OVERRIDE;

    virtual bool isFlexibleBox() const OVERRIDE { return true; }
    virtual bool avoidsFloats() const OVERRIDE { return true; }
    virtual void computePreferredLogicalWidths() OVERRIDE;
    virtual void layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight = 0) OVERRIDE;

    virtual void paintChildren(PaintInfo& forSelf, const LayoutPoint&, PaintInfo& forChild, bool usePrintRect) OVERRIDE;

    bool isHorizontalFlow() const;

private:
    enum FlexSign {
        PositiveFlexibility,
        NegativeFlexibility,
    };

    enum PositionedLayoutMode {
        FlipForRowReverse,
        NoFlipForRowReverse,
    };

    struct OrderHashTraits;
    typedef HashSet<int, DefaultHash<int>::Hash, OrderHashTraits> OrderHashSet;

    class OrderIterator;
    typedef WTF::HashMap<const RenderBox*, LayoutUnit> InflexibleFlexItemSize;
    typedef WTF::Vector<RenderBox*> OrderedFlexItemList;

    struct LineContext;
    struct Violation;

    bool hasOrthogonalFlow(RenderBox* child) const;
    bool isColumnFlow() const;
    bool isLeftToRightFlow() const;
    bool isMultiline() const;
    Length flexBasisForChild(RenderBox* child) const;
    void setCrossAxisExtent(LayoutUnit);
    LayoutUnit crossAxisExtentForChild(RenderBox* child);
    LayoutUnit mainAxisExtentForChild(RenderBox* child);
    LayoutUnit crossAxisExtent() const;
    LayoutUnit mainAxisExtent() const;
    LayoutUnit crossAxisContentExtent() const;
    LayoutUnit mainAxisContentExtent();
    LayoutUnit computeMainAxisExtentForChild(RenderBox* child, SizeType, const Length& size, LayoutUnit maximumValue);
    WritingMode transformedWritingMode() const;
    LayoutUnit flowAwareBorderStart() const;
    LayoutUnit flowAwareBorderEnd() const;
    LayoutUnit flowAwareBorderBefore() const;
    LayoutUnit flowAwareBorderAfter() const;
    LayoutUnit flowAwarePaddingStart() const;
    LayoutUnit flowAwarePaddingEnd() const;
    LayoutUnit flowAwarePaddingBefore() const;
    LayoutUnit flowAwarePaddingAfter() const;
    LayoutUnit flowAwareMarginStartForChild(RenderBox* child) const;
    LayoutUnit flowAwareMarginEndForChild(RenderBox* child) const;
    LayoutUnit flowAwareMarginBeforeForChild(RenderBox* child) const;
    LayoutUnit flowAwareMarginAfterForChild(RenderBox* child) const;
    LayoutUnit crossAxisMarginExtentForChild(RenderBox* child) const;
    LayoutUnit crossAxisScrollbarExtent() const;
    LayoutPoint flowAwareLocationForChild(RenderBox* child) const;
    // FIXME: Supporting layout deltas.
    void setFlowAwareLocationForChild(RenderBox* child, const LayoutPoint&);
    void adjustAlignmentForChild(RenderBox* child, LayoutUnit);
    LayoutUnit mainAxisBorderAndPaddingExtentForChild(RenderBox* child) const;
    LayoutUnit mainAxisScrollbarExtentForChild(RenderBox* child) const;
    LayoutUnit preferredMainAxisContentExtentForChild(RenderBox* child);

    void layoutFlexItems(OrderIterator&, WTF::Vector<LineContext>&);
    LayoutUnit autoMarginOffsetInMainAxis(const OrderedFlexItemList&, LayoutUnit& availableFreeSpace);
    void updateAutoMarginsInMainAxis(RenderBox* child, LayoutUnit autoMarginOffset);
    bool hasAutoMarginsInCrossAxis(RenderBox* child);
    bool updateAutoMarginsInCrossAxis(RenderBox* child, LayoutUnit availableAlignmentSpace);
    void repositionLogicalHeightDependentFlexItems(OrderIterator&, WTF::Vector<LineContext>&, LayoutUnit& oldClientAfterEdge);

    LayoutUnit availableAlignmentSpaceForChild(LayoutUnit lineCrossAxisExtent, RenderBox*);
    LayoutUnit marginBoxAscentForChild(RenderBox*);

    LayoutUnit computeMarginValue(Length margin, LayoutUnit availableSize, RenderView*);
    void computeMainAxisPreferredSizes(bool relayoutChildren, OrderHashSet&);
    LayoutUnit lineBreakLength();
    LayoutUnit adjustChildSizeForMinAndMax(RenderBox*, LayoutUnit childSize, LayoutUnit flexboxAvailableContentExtent);
    bool computeNextFlexLine(OrderIterator&, OrderedFlexItemList& orderedChildren, LayoutUnit& preferredMainAxisExtent, float& totalFlexGrow, float& totalWeightedFlexShrink, LayoutUnit& minMaxAppliedMainAxisExtent);
    LayoutUnit computeAvailableFreeSpace(LayoutUnit preferredMainAxisExtent);

    bool resolveFlexibleLengths(FlexSign, const OrderedFlexItemList&, LayoutUnit& availableFreeSpace, float& totalFlexGrow, float& totalWeightedFlexShrink, InflexibleFlexItemSize&, WTF::Vector<LayoutUnit>& childSizes);
    void freezeViolations(const WTF::Vector<Violation>&, LayoutUnit& availableFreeSpace, float& totalFlexGrow, float& totalWeightedFlexShrink, InflexibleFlexItemSize&);

    void setLogicalOverrideSize(RenderBox* child, LayoutUnit childPreferredSize);
    void prepareChildForPositionedLayout(RenderBox* child, LayoutUnit mainAxisOffset, LayoutUnit crossAxisOffset, PositionedLayoutMode);
    void layoutAndPlaceChildren(LayoutUnit& crossAxisOffset, const OrderedFlexItemList&, const WTF::Vector<LayoutUnit>& childSizes, LayoutUnit availableFreeSpace, WTF::Vector<LineContext>&);
    void layoutColumnReverse(const OrderedFlexItemList&, const WTF::Vector<LayoutUnit>& childSizes, LayoutUnit crossAxisOffset, LayoutUnit availableFreeSpace);
    void alignFlexLines(OrderIterator&, WTF::Vector<LineContext>&);
    void alignChildren(OrderIterator&, const WTF::Vector<LineContext>&);
    void applyStretchAlignmentToChild(RenderBox*, LayoutUnit lineCrossAxisExtent);
    void flipForRightToLeftColumn(OrderIterator&);
    void flipForWrapReverse(OrderIterator&, const WTF::Vector<LineContext>&, LayoutUnit crossAxisStartEdge);

    OwnPtr<OrderIterator> m_orderIterator;
};

} // namespace WebCore

#endif // RenderFlexibleBox_h
