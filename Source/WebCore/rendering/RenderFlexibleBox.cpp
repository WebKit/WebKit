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

#include "config.h"
#include "RenderFlexibleBox.h"

#include "LayoutRepainter.h"
#include "RenderLayer.h"
#include "RenderView.h"
#include <limits>
#include <wtf/MathExtras.h>

namespace WebCore {

// Normally, -1 and 0 are not valid in a HashSet, but these are relatively likely order: values. Instead,
// we make the two smallest int values invalid order: values (in the css parser code we clamp them to
// int min + 2).
struct RenderFlexibleBox::OrderHashTraits : WTF::GenericHashTraits<int> {
    static const bool emptyValueIsZero = false;
    static int emptyValue() { return std::numeric_limits<int>::min(); }
    static void constructDeletedValue(int& slot) { slot = std::numeric_limits<int>::min() + 1; }
    static bool isDeletedValue(int value) { return value == std::numeric_limits<int>::min() + 1; }
};

class RenderFlexibleBox::OrderIterator {
public:
    OrderIterator(RenderFlexibleBox* flexibleBox, const OrderHashSet& orderValues)
        : m_flexibleBox(flexibleBox)
        , m_currentChild(0)
        , m_orderValuesIterator(0)
    {
        copyToVector(orderValues, m_orderValues);
        std::sort(m_orderValues.begin(), m_orderValues.end());
        first();
    }

    RenderBox* currentChild() { return m_currentChild; }

    RenderBox* first()
    {
        reset();
        return next();
    }

    RenderBox* next()
    {
        do {
            if (!m_currentChild) {
                if (m_orderValuesIterator == m_orderValues.end())
                    return 0;
                if (m_orderValuesIterator) {
                    ++m_orderValuesIterator;
                    if (m_orderValuesIterator == m_orderValues.end())
                        return 0;
                } else
                    m_orderValuesIterator = m_orderValues.begin();

                m_currentChild = m_flexibleBox->firstChildBox();
            } else
                m_currentChild = m_currentChild->nextSiblingBox();
        } while (!m_currentChild || m_currentChild->style()->order() != *m_orderValuesIterator);

        return m_currentChild;
    }

    void reset()
    {
        m_currentChild = 0;
        m_orderValuesIterator = 0;
    }

private:
    RenderFlexibleBox* m_flexibleBox;
    RenderBox* m_currentChild;
    Vector<int> m_orderValues;
    Vector<int>::const_iterator m_orderValuesIterator;
};

struct RenderFlexibleBox::LineContext {
    LineContext(LayoutUnit crossAxisOffset, LayoutUnit crossAxisExtent, size_t numberOfChildren, LayoutUnit maxAscent)
        : crossAxisOffset(crossAxisOffset)
        , crossAxisExtent(crossAxisExtent)
        , numberOfChildren(numberOfChildren)
        , maxAscent(maxAscent)
    {
    }

    LayoutUnit crossAxisOffset;
    LayoutUnit crossAxisExtent;
    size_t numberOfChildren;
    LayoutUnit maxAscent;
};

struct RenderFlexibleBox::Violation {
    Violation(RenderBox* child, LayoutUnit childSize)
        : child(child)
        , childSize(childSize)
    {
    }

    RenderBox* child;
    LayoutUnit childSize;
};


RenderFlexibleBox::RenderFlexibleBox(Node* node)
    : RenderBlock(node)
{
    setChildrenInline(false); // All of our children must be block-level.
}

RenderFlexibleBox::~RenderFlexibleBox()
{
}

const char* RenderFlexibleBox::renderName() const
{
    return "RenderFlexibleBox";
}

static LayoutUnit marginLogicalWidthForChild(RenderBox* child, RenderStyle* parentStyle)
{
    // A margin has three types: fixed, percentage, and auto (variable).
    // Auto and percentage margins become 0 when computing min/max width.
    // Fixed margins can be added in as is.
    Length marginLeft = child->style()->marginStartUsing(parentStyle);
    Length marginRight = child->style()->marginEndUsing(parentStyle);
    LayoutUnit margin = 0;
    if (marginLeft.isFixed())
        margin += marginLeft.value();
    if (marginRight.isFixed())
        margin += marginRight.value();
    return margin;
}

void RenderFlexibleBox::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    RenderStyle* styleToUse = style();
    // FIXME: This should probably be checking for isSpecified since you should be able to use percentage, calc or viewport relative values for width.
    if (styleToUse->logicalWidth().isFixed() && styleToUse->logicalWidth().value() > 0)
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = computeContentBoxLogicalWidth(styleToUse->logicalWidth().value());
    else {
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = 0;

        for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
            if (child->isOutOfFlowPositioned())
                continue;

            LayoutUnit margin = marginLogicalWidthForChild(child, style());
            bool hasOrthogonalWritingMode = child->isHorizontalWritingMode() != isHorizontalWritingMode();
            LayoutUnit minPreferredLogicalWidth = hasOrthogonalWritingMode ? child->logicalHeight() : child->minPreferredLogicalWidth();
            LayoutUnit maxPreferredLogicalWidth = hasOrthogonalWritingMode ? child->logicalHeight() : child->maxPreferredLogicalWidth();
            minPreferredLogicalWidth += margin;
            maxPreferredLogicalWidth += margin;
            if (!isColumnFlow()) {
                m_maxPreferredLogicalWidth += maxPreferredLogicalWidth;
                if (isMultiline()) {
                    // For multiline, the min preferred width is if you put a break between each item.
                    m_minPreferredLogicalWidth = std::max(m_minPreferredLogicalWidth, minPreferredLogicalWidth);
                } else
                    m_minPreferredLogicalWidth += minPreferredLogicalWidth;
            } else {
                m_minPreferredLogicalWidth = std::max(minPreferredLogicalWidth, m_minPreferredLogicalWidth);
                if (isMultiline()) {
                    // For multiline, the max preferred width is if you put a break between each item.
                    m_maxPreferredLogicalWidth += maxPreferredLogicalWidth;
                } else
                    m_maxPreferredLogicalWidth = std::max(maxPreferredLogicalWidth, m_maxPreferredLogicalWidth);
            }
        }

        m_maxPreferredLogicalWidth = std::max(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);
    }

    LayoutUnit scrollbarWidth = 0;
    if (hasOverflowClip()) {
        if (isHorizontalWritingMode() && styleToUse->overflowY() == OSCROLL) {
            ASSERT(layer()->hasVerticalScrollbar());
            scrollbarWidth = verticalScrollbarWidth();
        } else if (!isHorizontalWritingMode() && styleToUse->overflowX() == OSCROLL) {
            ASSERT(layer()->hasHorizontalScrollbar());
            scrollbarWidth = horizontalScrollbarHeight();
        }
    }

    m_maxPreferredLogicalWidth += scrollbarWidth;
    m_minPreferredLogicalWidth += scrollbarWidth;

    // FIXME: This should probably be checking for isSpecified since you should be able to use percentage, calc or viewport relative values for min-width.
    if (styleToUse->logicalMinWidth().isFixed() && styleToUse->logicalMinWidth().value() > 0) {
        m_maxPreferredLogicalWidth = std::max(m_maxPreferredLogicalWidth, computeContentBoxLogicalWidth(styleToUse->logicalMinWidth().value()));
        m_minPreferredLogicalWidth = std::max(m_minPreferredLogicalWidth, computeContentBoxLogicalWidth(styleToUse->logicalMinWidth().value()));
    }

    // FIXME: This should probably be checking for isSpecified since you should be able to use percentage, calc or viewport relative values for maxWidth.
    if (styleToUse->logicalMaxWidth().isFixed()) {
        m_maxPreferredLogicalWidth = std::min(m_maxPreferredLogicalWidth, computeContentBoxLogicalWidth(styleToUse->logicalMaxWidth().value()));
        m_minPreferredLogicalWidth = std::min(m_minPreferredLogicalWidth, computeContentBoxLogicalWidth(styleToUse->logicalMaxWidth().value()));
    }

    LayoutUnit borderAndPadding = borderAndPaddingLogicalWidth();
    m_minPreferredLogicalWidth += borderAndPadding;
    m_maxPreferredLogicalWidth += borderAndPadding;

    setPreferredLogicalWidthsDirty(false);
}

void RenderFlexibleBox::layoutBlock(bool relayoutChildren, LayoutUnit)
{
    ASSERT(needsLayout());

    if (!relayoutChildren && simplifiedLayout())
        return;

    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());
    LayoutStateMaintainer statePusher(view(), this, locationOffset(), hasTransform() || hasReflection() || style()->isFlippedBlocksWritingMode());

    if (inRenderFlowThread()) {
        // Regions changing widths can force us to relayout our children.
        if (logicalWidthChangedInRegions())
            relayoutChildren = true;
    }
    computeInitialRegionRangeForBlock();

    LayoutSize previousSize = size();

    setLogicalHeight(0);
    computeLogicalWidth();

    m_overflow.clear();

    WTF::Vector<LineContext> lineContexts;
    OrderHashSet orderValues;
    computeMainAxisPreferredSizes(relayoutChildren, orderValues);
    m_orderIterator = adoptPtr(new OrderIterator(this, orderValues));
    layoutFlexItems(*m_orderIterator, lineContexts);

    LayoutUnit oldClientAfterEdge = clientLogicalBottom();
    computeLogicalHeight();
    repositionLogicalHeightDependentFlexItems(*m_orderIterator, lineContexts, oldClientAfterEdge);

    if (size() != previousSize)
        relayoutChildren = true;

    layoutPositionedObjects(relayoutChildren || isRoot());

    computeRegionRangeForBlock();

    // FIXME: css3/flexbox/repaint-rtl-column.html seems to repaint more overflow than it needs to.
    computeOverflow(oldClientAfterEdge);
    statePusher.pop();

    updateLayerTransform();

    // Update our scroll information if we're overflow:auto/scroll/hidden now that we know if
    // we overflow or not.
    if (hasOverflowClip())
        layer()->updateScrollInfoAfterLayout();

    repainter.repaintAfterLayout();

    setNeedsLayout(false);
}

void RenderFlexibleBox::paintChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset, PaintInfo& paintInfoForChild, bool usePrintRect)
{
    ASSERT(m_orderIterator);

    for (RenderBox* child = m_orderIterator->first(); child; child = m_orderIterator->next()) {
        if (!paintChild(child, paintInfo, paintOffset, paintInfoForChild, usePrintRect))
            return;
    }
}

void RenderFlexibleBox::repositionLogicalHeightDependentFlexItems(OrderIterator& iterator, WTF::Vector<LineContext>& lineContexts, LayoutUnit& oldClientAfterEdge)
{
    LayoutUnit crossAxisStartEdge = lineContexts.isEmpty() ? ZERO_LAYOUT_UNIT : lineContexts[0].crossAxisOffset;
    alignFlexLines(iterator, lineContexts);

    // If we have a single line flexbox, the line height is all the available space.
    // For flex-direction: row, this means we need to use the height, so we do this after calling computeLogicalHeight.
    if (!isMultiline() && lineContexts.size() == 1)
        lineContexts[0].crossAxisExtent = crossAxisContentExtent();
    alignChildren(iterator, lineContexts);

    if (style()->flexWrap() == FlexWrapReverse) {
        if (isHorizontalFlow())
            oldClientAfterEdge = clientLogicalBottom();
        flipForWrapReverse(iterator, lineContexts, crossAxisStartEdge);
    }

    // direction:rtl + flex-direction:column means the cross-axis direction is flipped.
    flipForRightToLeftColumn(iterator);
}

bool RenderFlexibleBox::hasOrthogonalFlow(RenderBox* child) const
{
    // FIXME: If the child is a flexbox, then we need to check isHorizontalFlow.
    return isHorizontalFlow() != child->isHorizontalWritingMode();
}

bool RenderFlexibleBox::isColumnFlow() const
{
    return style()->isColumnFlexDirection();
}

bool RenderFlexibleBox::isHorizontalFlow() const
{
    if (isHorizontalWritingMode())
        return !isColumnFlow();
    return isColumnFlow();
}

bool RenderFlexibleBox::isLeftToRightFlow() const
{
    if (isColumnFlow())
        return style()->writingMode() == TopToBottomWritingMode || style()->writingMode() == LeftToRightWritingMode;
    return style()->isLeftToRightDirection() ^ (style()->flexDirection() == FlowRowReverse);
}

bool RenderFlexibleBox::isMultiline() const
{
    return style()->flexWrap() != FlexWrapNone;
}

Length RenderFlexibleBox::flexBasisForChild(RenderBox* child) const
{
    Length flexLength = child->style()->flexBasis();
    if (flexLength.isAuto())
        flexLength = isHorizontalFlow() ? child->style()->width() : child->style()->height();
    return flexLength;
}

void RenderFlexibleBox::setCrossAxisExtent(LayoutUnit extent)
{
    if (isHorizontalFlow())
        setHeight(extent);
    else
        setWidth(extent);
}

LayoutUnit RenderFlexibleBox::crossAxisExtentForChild(RenderBox* child)
{
    return isHorizontalFlow() ? child->height() : child->width();
}

LayoutUnit RenderFlexibleBox::mainAxisExtentForChild(RenderBox* child)
{
    return isHorizontalFlow() ? child->width() : child->height();
}

LayoutUnit RenderFlexibleBox::crossAxisExtent() const
{
    return isHorizontalFlow() ? height() : width();
}

LayoutUnit RenderFlexibleBox::mainAxisExtent() const
{
    return isHorizontalFlow() ? width() : height();
}

LayoutUnit RenderFlexibleBox::crossAxisContentExtent() const
{
    return isHorizontalFlow() ? contentHeight() : contentWidth();
}

LayoutUnit RenderFlexibleBox::mainAxisContentExtent()
{
    if (isColumnFlow())
        return std::max(LayoutUnit(0), computeLogicalClientHeight(MainOrPreferredSize, style()->logicalHeight()));
    return contentLogicalWidth();
}

LayoutUnit RenderFlexibleBox::computeMainAxisExtentForChild(RenderBox* child, SizeType sizeType, const Length& size, LayoutUnit maximumValue)
{
    // FIXME: This is wrong for orthogonal flows. It should use the flexbox's writing-mode, not the child's in order
    // to figure out the logical height/width.
    if (isColumnFlow())
        return child->computeLogicalClientHeight(sizeType, size);
    return child->computeContentBoxLogicalWidth(valueForLength(size, maximumValue, view()));
}

WritingMode RenderFlexibleBox::transformedWritingMode() const
{
    WritingMode mode = style()->writingMode();
    if (!isColumnFlow())
        return mode;

    switch (mode) {
    case TopToBottomWritingMode:
    case BottomToTopWritingMode:
        return style()->isLeftToRightDirection() ? LeftToRightWritingMode : RightToLeftWritingMode;
    case LeftToRightWritingMode:
    case RightToLeftWritingMode:
        return style()->isLeftToRightDirection() ? TopToBottomWritingMode : BottomToTopWritingMode;
    }
    ASSERT_NOT_REACHED();
    return TopToBottomWritingMode;
}

LayoutUnit RenderFlexibleBox::flowAwareBorderStart() const
{
    if (isHorizontalFlow())
        return isLeftToRightFlow() ? borderLeft() : borderRight();
    return isLeftToRightFlow() ? borderTop() : borderBottom();
}

LayoutUnit RenderFlexibleBox::flowAwareBorderEnd() const
{
    if (isHorizontalFlow())
        return isLeftToRightFlow() ? borderRight() : borderLeft();
    return isLeftToRightFlow() ? borderBottom() : borderTop();
}

LayoutUnit RenderFlexibleBox::flowAwareBorderBefore() const
{
    switch (transformedWritingMode()) {
    case TopToBottomWritingMode:
        return borderTop();
    case BottomToTopWritingMode:
        return borderBottom();
    case LeftToRightWritingMode:
        return borderLeft();
    case RightToLeftWritingMode:
        return borderRight();
    }
    ASSERT_NOT_REACHED();
    return borderTop();
}

LayoutUnit RenderFlexibleBox::flowAwareBorderAfter() const
{
    switch (transformedWritingMode()) {
    case TopToBottomWritingMode:
        return borderBottom();
    case BottomToTopWritingMode:
        return borderTop();
    case LeftToRightWritingMode:
        return borderRight();
    case RightToLeftWritingMode:
        return borderLeft();
    }
    ASSERT_NOT_REACHED();
    return borderTop();
}

LayoutUnit RenderFlexibleBox::flowAwarePaddingStart() const
{
    if (isHorizontalFlow())
        return isLeftToRightFlow() ? paddingLeft() : paddingRight();
    return isLeftToRightFlow() ? paddingTop() : paddingBottom();
}

LayoutUnit RenderFlexibleBox::flowAwarePaddingEnd() const
{
    if (isHorizontalFlow())
        return isLeftToRightFlow() ? paddingRight() : paddingLeft();
    return isLeftToRightFlow() ? paddingBottom() : paddingTop();
}

LayoutUnit RenderFlexibleBox::flowAwarePaddingBefore() const
{
    switch (transformedWritingMode()) {
    case TopToBottomWritingMode:
        return paddingTop();
    case BottomToTopWritingMode:
        return paddingBottom();
    case LeftToRightWritingMode:
        return paddingLeft();
    case RightToLeftWritingMode:
        return paddingRight();
    }
    ASSERT_NOT_REACHED();
    return paddingTop();
}

LayoutUnit RenderFlexibleBox::flowAwarePaddingAfter() const
{
    switch (transformedWritingMode()) {
    case TopToBottomWritingMode:
        return paddingBottom();
    case BottomToTopWritingMode:
        return paddingTop();
    case LeftToRightWritingMode:
        return paddingRight();
    case RightToLeftWritingMode:
        return paddingLeft();
    }
    ASSERT_NOT_REACHED();
    return paddingTop();
}

LayoutUnit RenderFlexibleBox::flowAwareMarginStartForChild(RenderBox* child) const
{
    if (isHorizontalFlow())
        return isLeftToRightFlow() ? child->marginLeft() : child->marginRight();
    return isLeftToRightFlow() ? child->marginTop() : child->marginBottom();
}

LayoutUnit RenderFlexibleBox::flowAwareMarginEndForChild(RenderBox* child) const
{
    if (isHorizontalFlow())
        return isLeftToRightFlow() ? child->marginRight() : child->marginLeft();
    return isLeftToRightFlow() ? child->marginBottom() : child->marginTop();
}

LayoutUnit RenderFlexibleBox::flowAwareMarginBeforeForChild(RenderBox* child) const
{
    switch (transformedWritingMode()) {
    case TopToBottomWritingMode:
        return child->marginTop();
    case BottomToTopWritingMode:
        return child->marginBottom();
    case LeftToRightWritingMode:
        return child->marginLeft();
    case RightToLeftWritingMode:
        return child->marginRight();
    }
    ASSERT_NOT_REACHED();
    return marginTop();
}

LayoutUnit RenderFlexibleBox::flowAwareMarginAfterForChild(RenderBox* child) const
{
    switch (transformedWritingMode()) {
    case TopToBottomWritingMode:
        return child->marginBottom();
    case BottomToTopWritingMode:
        return child->marginTop();
    case LeftToRightWritingMode:
        return child->marginRight();
    case RightToLeftWritingMode:
        return child->marginLeft();
    }
    ASSERT_NOT_REACHED();
    return marginBottom();
}

LayoutUnit RenderFlexibleBox::crossAxisMarginExtentForChild(RenderBox* child) const
{
    return isHorizontalFlow() ? child->marginHeight() : child->marginWidth();
}

LayoutUnit RenderFlexibleBox::crossAxisScrollbarExtent() const
{
    return isHorizontalFlow() ? horizontalScrollbarHeight() : verticalScrollbarWidth();
}

LayoutPoint RenderFlexibleBox::flowAwareLocationForChild(RenderBox* child) const
{
    return isHorizontalFlow() ? child->location() : child->location().transposedPoint();
}

void RenderFlexibleBox::setFlowAwareLocationForChild(RenderBox* child, const LayoutPoint& location)
{
    if (isHorizontalFlow())
        child->setLocation(location);
    else
        child->setLocation(location.transposedPoint());
}

LayoutUnit RenderFlexibleBox::mainAxisBorderAndPaddingExtentForChild(RenderBox* child) const
{
    return isHorizontalFlow() ? child->borderAndPaddingWidth() : child->borderAndPaddingHeight();
}

LayoutUnit RenderFlexibleBox::mainAxisScrollbarExtentForChild(RenderBox* child) const
{
    return isHorizontalFlow() ? child->verticalScrollbarWidth() : child->horizontalScrollbarHeight();
}

LayoutUnit RenderFlexibleBox::preferredMainAxisContentExtentForChild(RenderBox* child)
{
    Length flexBasis = flexBasisForChild(child);
    if (flexBasis.isAuto()) {
        LayoutUnit mainAxisExtent = hasOrthogonalFlow(child) ? child->logicalHeight() : child->maxPreferredLogicalWidth();
        return mainAxisExtent - mainAxisBorderAndPaddingExtentForChild(child);
    }
    return std::max(LayoutUnit(0), computeMainAxisExtentForChild(child, MainOrPreferredSize, flexBasis, mainAxisContentExtent()));
}

LayoutUnit RenderFlexibleBox::computeAvailableFreeSpace(LayoutUnit preferredMainAxisExtent)
{
    LayoutUnit contentExtent = 0;
    if (!isColumnFlow())
        contentExtent = mainAxisContentExtent();
    else if (hasOverrideHeight())
        contentExtent = overrideLogicalContentHeight();
    else {
        LayoutUnit heightResult = computeLogicalClientHeight(MainOrPreferredSize, style()->logicalHeight());
        if (heightResult == -1)
            heightResult = preferredMainAxisExtent;
        LayoutUnit minHeight = computeLogicalClientHeight(MinSize, style()->logicalMinHeight()); // Leave as -1 if unset.
        LayoutUnit maxHeight = style()->logicalMaxHeight().isUndefined() ? heightResult : computeLogicalClientHeight(MaxSize, style()->logicalMaxHeight());
        if (maxHeight == -1)
            maxHeight = heightResult;
        heightResult = std::min(maxHeight, heightResult);
        heightResult = std::max(minHeight, heightResult);
        contentExtent = heightResult;
    }

    return contentExtent - preferredMainAxisExtent;
}

void RenderFlexibleBox::layoutFlexItems(OrderIterator& iterator, WTF::Vector<LineContext>& lineContexts)
{
    OrderedFlexItemList orderedChildren;
    LayoutUnit preferredMainAxisExtent;
    float totalFlexGrow;
    float totalWeightedFlexShrink;
    LayoutUnit minMaxAppliedMainAxisExtent;

    LayoutUnit crossAxisOffset = flowAwareBorderBefore() + flowAwarePaddingBefore();
    while (computeNextFlexLine(iterator, orderedChildren, preferredMainAxisExtent, totalFlexGrow, totalWeightedFlexShrink, minMaxAppliedMainAxisExtent)) {
        LayoutUnit availableFreeSpace = computeAvailableFreeSpace(preferredMainAxisExtent);
        FlexSign flexSign = (minMaxAppliedMainAxisExtent < preferredMainAxisExtent + availableFreeSpace) ? PositiveFlexibility : NegativeFlexibility;
        InflexibleFlexItemSize inflexibleItems;
        WTF::Vector<LayoutUnit> childSizes;
        while (!resolveFlexibleLengths(flexSign, orderedChildren, availableFreeSpace, totalFlexGrow, totalWeightedFlexShrink, inflexibleItems, childSizes)) {
            ASSERT(totalFlexGrow >= 0 && totalWeightedFlexShrink >= 0);
            ASSERT(inflexibleItems.size() > 0);
        }

        layoutAndPlaceChildren(crossAxisOffset, orderedChildren, childSizes, availableFreeSpace, lineContexts);
    }
}

LayoutUnit RenderFlexibleBox::autoMarginOffsetInMainAxis(const OrderedFlexItemList& children, LayoutUnit& availableFreeSpace)
{
    if (availableFreeSpace <= 0)
        return 0;

    int numberOfAutoMargins = 0;
    bool isHorizontal = isHorizontalFlow();
    for (size_t i = 0; i < children.size(); ++i) {
        RenderBox* child = children[i];
        if (child->isOutOfFlowPositioned())
            continue;
        if (isHorizontal) {
            if (child->style()->marginLeft().isAuto())
                ++numberOfAutoMargins;
            if (child->style()->marginRight().isAuto())
                ++numberOfAutoMargins;
        } else {
            if (child->style()->marginTop().isAuto())
                ++numberOfAutoMargins;
            if (child->style()->marginBottom().isAuto())
                ++numberOfAutoMargins;
        }
    }
    if (!numberOfAutoMargins)
        return 0;

    LayoutUnit sizeOfAutoMargin = availableFreeSpace / numberOfAutoMargins;
    availableFreeSpace = 0;
    return sizeOfAutoMargin;
}

void RenderFlexibleBox::updateAutoMarginsInMainAxis(RenderBox* child, LayoutUnit autoMarginOffset)
{
    if (isHorizontalFlow()) {
        if (child->style()->marginLeft().isAuto())
            child->setMarginLeft(autoMarginOffset);
        if (child->style()->marginRight().isAuto())
            child->setMarginRight(autoMarginOffset);
    } else {
        if (child->style()->marginTop().isAuto())
            child->setMarginTop(autoMarginOffset);
        if (child->style()->marginBottom().isAuto())
            child->setMarginBottom(autoMarginOffset);
    }
}

bool RenderFlexibleBox::hasAutoMarginsInCrossAxis(RenderBox* child)
{
    if (isHorizontalFlow())
        return child->style()->marginTop().isAuto() || child->style()->marginBottom().isAuto();
    return child->style()->marginLeft().isAuto() || child->style()->marginRight().isAuto();
}

LayoutUnit RenderFlexibleBox::availableAlignmentSpaceForChild(LayoutUnit lineCrossAxisExtent, RenderBox* child)
{
    LayoutUnit childCrossExtent = 0;
    if (!child->isOutOfFlowPositioned())
        childCrossExtent = crossAxisMarginExtentForChild(child) + crossAxisExtentForChild(child);
    return lineCrossAxisExtent - childCrossExtent;
}

bool RenderFlexibleBox::updateAutoMarginsInCrossAxis(RenderBox* child, LayoutUnit availableAlignmentSpace)
{
    bool isHorizontal = isHorizontalFlow();
    Length start = isHorizontal ? child->style()->marginTop() : child->style()->marginLeft();
    Length end = isHorizontal ? child->style()->marginBottom() : child->style()->marginRight();
    if (start.isAuto() && end.isAuto()) {
        adjustAlignmentForChild(child, availableAlignmentSpace / 2);
        if (isHorizontal) {
            child->setMarginTop(availableAlignmentSpace / 2);
            child->setMarginBottom(availableAlignmentSpace / 2);
        } else {
            child->setMarginLeft(availableAlignmentSpace / 2);
            child->setMarginRight(availableAlignmentSpace / 2);
        }
        return true;
    }
    if (start.isAuto()) {
        adjustAlignmentForChild(child, availableAlignmentSpace);
        if (isHorizontal)
            child->setMarginTop(availableAlignmentSpace);
        else
            child->setMarginLeft(availableAlignmentSpace);
        return true;
    }
    if (end.isAuto()) {
        if (isHorizontal)
            child->setMarginBottom(availableAlignmentSpace);
        else
            child->setMarginRight(availableAlignmentSpace);
        return true;
    }
    return false;
}

LayoutUnit RenderFlexibleBox::marginBoxAscentForChild(RenderBox* child)
{
    LayoutUnit ascent = child->firstLineBoxBaseline();
    if (ascent == -1)
        ascent = crossAxisExtentForChild(child) + flowAwareMarginAfterForChild(child);
    return ascent + flowAwareMarginBeforeForChild(child);
}

LayoutUnit RenderFlexibleBox::computeMarginValue(Length margin, LayoutUnit availableSize, RenderView* view)
{
    // CSS always computes percent margins with respect to the containing block's width, even for margin-top/margin-bottom.
    if (margin.isPercent())
        availableSize = logicalWidth();
    return minimumValueForLength(margin, availableSize, view);
}

void RenderFlexibleBox::computeMainAxisPreferredSizes(bool relayoutChildren, OrderHashSet& orderValues)
{
    LayoutUnit flexboxAvailableContentExtent = mainAxisContentExtent();
    RenderView* renderView = view();
    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        orderValues.add(child->style()->order());

        if (child->isOutOfFlowPositioned())
            continue;

        child->clearOverrideSize();
        // Only need to layout here if we will need to get the logicalHeight of the child in computeNextFlexLine.
        Length childMainAxisMin = isHorizontalFlow() ? child->style()->minWidth() : child->style()->minHeight();
        if (hasOrthogonalFlow(child) && (flexBasisForChild(child).isAuto() || childMainAxisMin.isAuto())) {
            if (!relayoutChildren)
                child->setChildNeedsLayout(true, MarkOnlyThis);
            child->layoutIfNeeded();
        }

        // Before running the flex algorithm, 'auto' has a margin of 0.
        // Also, if we're not auto sizing, we don't do a layout that computes the start/end margins.
        if (isHorizontalFlow()) {
            child->setMarginLeft(computeMarginValue(child->style()->marginLeft(), flexboxAvailableContentExtent, renderView));
            child->setMarginRight(computeMarginValue(child->style()->marginRight(), flexboxAvailableContentExtent, renderView));
        } else {
            child->setMarginTop(computeMarginValue(child->style()->marginTop(), flexboxAvailableContentExtent, renderView));
            child->setMarginBottom(computeMarginValue(child->style()->marginBottom(), flexboxAvailableContentExtent, renderView));
        }
    }
}

LayoutUnit RenderFlexibleBox::lineBreakLength()
{
    if (!isColumnFlow())
        return mainAxisContentExtent();

    LayoutUnit height = computeLogicalClientHeight(MainOrPreferredSize, style()->logicalHeight());
    if (height == -1)
        height = MAX_LAYOUT_UNIT;
    LayoutUnit maxHeight = computeLogicalClientHeight(MaxSize, style()->logicalMaxHeight());
    if (maxHeight != -1)
        height = std::min(height, maxHeight);
    return height;
}

LayoutUnit RenderFlexibleBox::adjustChildSizeForMinAndMax(RenderBox* child, LayoutUnit childSize, LayoutUnit flexboxAvailableContentExtent)
{
    // FIXME: Support intrinsic min/max lengths.
    Length max = isHorizontalFlow() ? child->style()->maxWidth() : child->style()->maxHeight();
    if (max.isSpecified()) {
        LayoutUnit maxExtent = computeMainAxisExtentForChild(child, MaxSize, max, flexboxAvailableContentExtent);
        if (maxExtent != -1 && childSize > maxExtent)
            childSize = maxExtent;
    }

    Length min = isHorizontalFlow() ? child->style()->minWidth() : child->style()->minHeight();
    LayoutUnit minExtent = 0;
    if (min.isSpecified())
        minExtent = computeMainAxisExtentForChild(child, MinSize, min, flexboxAvailableContentExtent);
    else if (min.isAuto()) {
        minExtent = hasOrthogonalFlow(child) ? child->logicalHeight() : child->minPreferredLogicalWidth();
        minExtent -= mainAxisBorderAndPaddingExtentForChild(child);
    }
    return std::max(childSize, minExtent);
}

bool RenderFlexibleBox::computeNextFlexLine(OrderIterator& iterator, OrderedFlexItemList& orderedChildren, LayoutUnit& preferredMainAxisExtent, float& totalFlexGrow, float& totalWeightedFlexShrink, LayoutUnit& minMaxAppliedMainAxisExtent)
{
    orderedChildren.clear();
    preferredMainAxisExtent = 0;
    totalFlexGrow = totalWeightedFlexShrink = 0;
    minMaxAppliedMainAxisExtent = 0;

    if (!iterator.currentChild())
        return false;

    LayoutUnit flexboxAvailableContentExtent = mainAxisContentExtent();
    LayoutUnit lineBreak = lineBreakLength();

    for (RenderBox* child = iterator.currentChild(); child; child = iterator.next()) {
        if (child->isOutOfFlowPositioned()) {
            orderedChildren.append(child);
            continue;
        }

        LayoutUnit childMainAxisExtent = preferredMainAxisContentExtentForChild(child);
        LayoutUnit childMainAxisMarginBoxExtent = mainAxisBorderAndPaddingExtentForChild(child) + childMainAxisExtent;
        childMainAxisMarginBoxExtent += isHorizontalFlow() ? child->marginWidth() : child->marginHeight();

        if (isMultiline() && preferredMainAxisExtent + childMainAxisMarginBoxExtent > lineBreak && orderedChildren.size() > 0)
            break;
        orderedChildren.append(child);
        preferredMainAxisExtent += childMainAxisMarginBoxExtent;
        totalFlexGrow += child->style()->flexGrow();
        totalWeightedFlexShrink += child->style()->flexShrink() * childMainAxisExtent;

        LayoutUnit childMinMaxAppliedMainAxisExtent = adjustChildSizeForMinAndMax(child, childMainAxisExtent, flexboxAvailableContentExtent);
        minMaxAppliedMainAxisExtent += childMinMaxAppliedMainAxisExtent - childMainAxisExtent + childMainAxisMarginBoxExtent;
    }
    return true;
}

void RenderFlexibleBox::freezeViolations(const WTF::Vector<Violation>& violations, LayoutUnit& availableFreeSpace, float& totalFlexGrow, float& totalWeightedFlexShrink, InflexibleFlexItemSize& inflexibleItems)
{
    for (size_t i = 0; i < violations.size(); ++i) {
        RenderBox* child = violations[i].child;
        LayoutUnit childSize = violations[i].childSize;
        LayoutUnit preferredChildSize = preferredMainAxisContentExtentForChild(child);
        availableFreeSpace -= childSize - preferredChildSize;
        totalFlexGrow -= child->style()->flexGrow();
        totalWeightedFlexShrink -= child->style()->flexShrink() * preferredChildSize;
        inflexibleItems.set(child, childSize);
    }
}

// Returns true if we successfully ran the algorithm and sized the flex items.
bool RenderFlexibleBox::resolveFlexibleLengths(FlexSign flexSign, const OrderedFlexItemList& children, LayoutUnit& availableFreeSpace, float& totalFlexGrow, float& totalWeightedFlexShrink, InflexibleFlexItemSize& inflexibleItems, WTF::Vector<LayoutUnit>& childSizes)
{
    childSizes.clear();
    LayoutUnit flexboxAvailableContentExtent = mainAxisContentExtent();
    LayoutUnit totalViolation = 0;
    LayoutUnit usedFreeSpace = 0;
    WTF::Vector<Violation> minViolations;
    WTF::Vector<Violation> maxViolations;
    for (size_t i = 0; i < children.size(); ++i) {
        RenderBox* child = children[i];
        if (child->isOutOfFlowPositioned()) {
            childSizes.append(0);
            continue;
        }

        if (inflexibleItems.contains(child))
            childSizes.append(inflexibleItems.get(child));
        else {
            LayoutUnit preferredChildSize = preferredMainAxisContentExtentForChild(child);
            LayoutUnit childSize = preferredChildSize;
            if (availableFreeSpace > 0 && totalFlexGrow > 0 && flexSign == PositiveFlexibility && isfinite(totalFlexGrow))
                childSize += roundedLayoutUnit(availableFreeSpace * child->style()->flexGrow() / totalFlexGrow);
            else if (availableFreeSpace < 0 && totalWeightedFlexShrink > 0 && flexSign == NegativeFlexibility && isfinite(totalWeightedFlexShrink))
                childSize += roundedLayoutUnit(availableFreeSpace * child->style()->flexShrink() * preferredChildSize / totalWeightedFlexShrink);

            LayoutUnit adjustedChildSize = adjustChildSizeForMinAndMax(child, childSize, flexboxAvailableContentExtent);
            childSizes.append(adjustedChildSize);
            usedFreeSpace += adjustedChildSize - preferredChildSize;

            LayoutUnit violation = adjustedChildSize - childSize;
            if (violation > 0)
                minViolations.append(Violation(child, adjustedChildSize));
            else if (violation < 0)
                maxViolations.append(Violation(child, adjustedChildSize));
            totalViolation += violation;
        }
    }

    if (totalViolation)
        freezeViolations(totalViolation < 0 ? maxViolations : minViolations, availableFreeSpace, totalFlexGrow, totalWeightedFlexShrink, inflexibleItems);
    else
        availableFreeSpace -= usedFreeSpace;

    return !totalViolation;
}

static LayoutUnit initialJustifyContentOffset(LayoutUnit availableFreeSpace, EJustifyContent justifyContent, unsigned numberOfChildren)
{
    if (justifyContent == JustifyFlexEnd)
        return availableFreeSpace;
    if (justifyContent == JustifyCenter)
        return availableFreeSpace / 2;
    if (justifyContent == JustifySpaceAround) {
        if (availableFreeSpace > 0 && numberOfChildren)
            return availableFreeSpace / (2 * numberOfChildren);
        if (availableFreeSpace < 0)
            return availableFreeSpace / 2;
    }
    return 0;
}

static LayoutUnit justifyContentSpaceBetweenChildren(LayoutUnit availableFreeSpace, EJustifyContent justifyContent, unsigned numberOfChildren)
{
    if (availableFreeSpace > 0 && numberOfChildren > 1) {
        if (justifyContent == JustifySpaceBetween)
            return availableFreeSpace / (numberOfChildren - 1);
        if (justifyContent == JustifySpaceAround)
            return availableFreeSpace / numberOfChildren;
    }
    return 0;
}

void RenderFlexibleBox::setLogicalOverrideSize(RenderBox* child, LayoutUnit childPreferredSize)
{
    if (hasOrthogonalFlow(child))
        child->setOverrideLogicalContentHeight(childPreferredSize - child->borderAndPaddingLogicalHeight());
    else
        child->setOverrideLogicalContentWidth(childPreferredSize - child->borderAndPaddingLogicalWidth());
}

void RenderFlexibleBox::prepareChildForPositionedLayout(RenderBox* child, LayoutUnit mainAxisOffset, LayoutUnit crossAxisOffset, PositionedLayoutMode layoutMode)
{
    ASSERT(child->isOutOfFlowPositioned());
    child->containingBlock()->insertPositionedObject(child);
    RenderLayer* childLayer = child->layer();
    LayoutUnit inlinePosition = isColumnFlow() ? crossAxisOffset : mainAxisOffset;
    if (layoutMode == FlipForRowReverse && style()->flexDirection() == FlowRowReverse)
        inlinePosition = mainAxisExtent() - mainAxisOffset;
    childLayer->setStaticInlinePosition(inlinePosition); // FIXME: Not right for regions.

    LayoutUnit staticBlockPosition = isColumnFlow() ? mainAxisOffset : crossAxisOffset;
    if (childLayer->staticBlockPosition() != staticBlockPosition) {
        childLayer->setStaticBlockPosition(staticBlockPosition);
        if (child->style()->hasStaticBlockPosition(style()->isHorizontalWritingMode()))
            child->setChildNeedsLayout(true, MarkOnlyThis);
    }
}

static EAlignItems alignmentForChild(RenderBox* child)
{
    EAlignItems align = child->style()->alignSelf();
    if (align == AlignAuto)
        align = child->parent()->style()->alignItems();

    if (child->parent()->style()->flexWrap() == FlexWrapReverse) {
        if (align == AlignFlexStart)
            align = AlignFlexEnd;
        else if (align == AlignFlexEnd)
            align = AlignFlexStart;
    }

    return align;
}

void RenderFlexibleBox::layoutAndPlaceChildren(LayoutUnit& crossAxisOffset, const OrderedFlexItemList& children, const WTF::Vector<LayoutUnit>& childSizes, LayoutUnit availableFreeSpace, WTF::Vector<LineContext>& lineContexts)
{
    ASSERT(childSizes.size() == children.size());

    LayoutUnit autoMarginOffset = autoMarginOffsetInMainAxis(children, availableFreeSpace);
    LayoutUnit mainAxisOffset = flowAwareBorderStart() + flowAwarePaddingStart();
    mainAxisOffset += initialJustifyContentOffset(availableFreeSpace, style()->justifyContent(), childSizes.size());
    if (style()->flexDirection() == FlowRowReverse)
        mainAxisOffset += isHorizontalFlow() ? verticalScrollbarWidth() : horizontalScrollbarHeight();

    LayoutUnit totalMainExtent = mainAxisExtent();
    LayoutUnit maxAscent = 0, maxDescent = 0; // Used when align-items: baseline.
    LayoutUnit maxChildCrossAxisExtent = 0;
    bool shouldFlipMainAxis = !isColumnFlow() && !isLeftToRightFlow();
    for (size_t i = 0; i < children.size(); ++i) {
        RenderBox* child = children[i];
        if (child->isOutOfFlowPositioned()) {
            prepareChildForPositionedLayout(child, mainAxisOffset, crossAxisOffset, FlipForRowReverse);
            mainAxisOffset += justifyContentSpaceBetweenChildren(availableFreeSpace, style()->justifyContent(), childSizes.size());
            continue;
        }
        LayoutUnit childPreferredSize = childSizes[i] + mainAxisBorderAndPaddingExtentForChild(child);
        setLogicalOverrideSize(child, childPreferredSize);
        // FIXME: Can avoid laying out here in some cases. See https://webkit.org/b/87905.
        child->setChildNeedsLayout(true, MarkOnlyThis);
        child->layoutIfNeeded();

        updateAutoMarginsInMainAxis(child, autoMarginOffset);

        LayoutUnit childCrossAxisMarginBoxExtent;
        if (alignmentForChild(child) == AlignBaseline && !hasAutoMarginsInCrossAxis(child)) {
            LayoutUnit ascent = marginBoxAscentForChild(child);
            LayoutUnit descent = (crossAxisMarginExtentForChild(child) + crossAxisExtentForChild(child)) - ascent;

            maxAscent = std::max(maxAscent, ascent);
            maxDescent = std::max(maxDescent, descent);

            childCrossAxisMarginBoxExtent = maxAscent + maxDescent;
        } else
            childCrossAxisMarginBoxExtent = crossAxisExtentForChild(child) + crossAxisMarginExtentForChild(child);
        if (!isColumnFlow() && style()->logicalHeight().isAuto())
            setLogicalHeight(std::max(logicalHeight(), crossAxisOffset + flowAwareBorderAfter() + flowAwarePaddingAfter() + childCrossAxisMarginBoxExtent + crossAxisScrollbarExtent()));
        maxChildCrossAxisExtent = std::max(maxChildCrossAxisExtent, childCrossAxisMarginBoxExtent);

        mainAxisOffset += flowAwareMarginStartForChild(child);

        LayoutUnit childMainExtent = mainAxisExtentForChild(child);
        LayoutPoint childLocation(shouldFlipMainAxis ? totalMainExtent - mainAxisOffset - childMainExtent : mainAxisOffset,
            crossAxisOffset + flowAwareMarginBeforeForChild(child));

        // FIXME: Supporting layout deltas.
        setFlowAwareLocationForChild(child, childLocation);
        mainAxisOffset += childMainExtent + flowAwareMarginEndForChild(child);

        mainAxisOffset += justifyContentSpaceBetweenChildren(availableFreeSpace, style()->justifyContent(), childSizes.size());
    }

    if (isColumnFlow())
        setLogicalHeight(mainAxisOffset + flowAwareBorderEnd() + flowAwarePaddingEnd() + scrollbarLogicalHeight());

    if (style()->flexDirection() == FlowColumnReverse) {
        // We have to do an extra pass for column-reverse to reposition the flex items since the start depends
        // on the height of the flexbox, which we only know after we've positioned all the flex items.
        computeLogicalHeight();
        layoutColumnReverse(children, childSizes, crossAxisOffset, availableFreeSpace);
    }

    lineContexts.append(LineContext(crossAxisOffset, maxChildCrossAxisExtent, children.size(), maxAscent));
    crossAxisOffset += maxChildCrossAxisExtent;
}

void RenderFlexibleBox::layoutColumnReverse(const OrderedFlexItemList& children, const WTF::Vector<LayoutUnit>& childSizes, LayoutUnit crossAxisOffset, LayoutUnit availableFreeSpace)
{
    // This is similar to the logic in layoutAndPlaceChildren, except we place the children
    // starting from the end of the flexbox. We also don't need to layout anything since we're
    // just moving the children to a new position.
    LayoutUnit mainAxisOffset = logicalHeight() - flowAwareBorderEnd() - flowAwarePaddingEnd();
    mainAxisOffset -= initialJustifyContentOffset(availableFreeSpace, style()->justifyContent(), childSizes.size());
    mainAxisOffset -= isHorizontalFlow() ? verticalScrollbarWidth() : horizontalScrollbarHeight();

    for (size_t i = 0; i < children.size(); ++i) {
        RenderBox* child = children[i];
        if (child->isOutOfFlowPositioned()) {
            child->layer()->setStaticBlockPosition(mainAxisOffset);
            mainAxisOffset -= justifyContentSpaceBetweenChildren(availableFreeSpace, style()->justifyContent(), childSizes.size());
            continue;
        }
        mainAxisOffset -= mainAxisExtentForChild(child) + flowAwareMarginEndForChild(child);

        LayoutRect oldRect = child->frameRect();
        setFlowAwareLocationForChild(child, LayoutPoint(mainAxisOffset, crossAxisOffset + flowAwareMarginBeforeForChild(child)));
        if (!selfNeedsLayout() && child->checkForRepaintDuringLayout())
            child->repaintDuringLayoutIfMoved(oldRect);

        mainAxisOffset -= flowAwareMarginStartForChild(child);
        mainAxisOffset -= justifyContentSpaceBetweenChildren(availableFreeSpace, style()->justifyContent(), childSizes.size());
    }
}

static LayoutUnit initialAlignContentOffset(LayoutUnit availableFreeSpace, EAlignContent alignContent, unsigned numberOfLines)
{
    if (alignContent == AlignContentFlexEnd)
        return availableFreeSpace;
    if (alignContent == AlignContentCenter)
        return availableFreeSpace / 2;
    if (alignContent == AlignContentSpaceAround) {
        if (availableFreeSpace > 0 && numberOfLines)
            return availableFreeSpace / (2 * numberOfLines);
        if (availableFreeSpace < 0)
            return availableFreeSpace / 2;
    }
    return 0;
}

static LayoutUnit alignContentSpaceBetweenChildren(LayoutUnit availableFreeSpace, EAlignContent alignContent, unsigned numberOfLines)
{
    if (availableFreeSpace > 0 && numberOfLines > 1) {
        if (alignContent == AlignContentSpaceBetween)
            return availableFreeSpace / (numberOfLines - 1);
        if (alignContent == AlignContentSpaceAround || alignContent == AlignContentStretch)
            return availableFreeSpace / numberOfLines;
    }
    return 0;
}

void RenderFlexibleBox::alignFlexLines(OrderIterator& iterator, WTF::Vector<LineContext>& lineContexts)
{
    if (!isMultiline() || style()->alignContent() == AlignContentFlexStart)
        return;

    LayoutUnit availableCrossAxisSpace = crossAxisContentExtent();
    for (size_t i = 0; i < lineContexts.size(); ++i)
        availableCrossAxisSpace -= lineContexts[i].crossAxisExtent;

    RenderBox* child = iterator.first();
    LayoutUnit lineOffset = initialAlignContentOffset(availableCrossAxisSpace, style()->alignContent(), lineContexts.size());
    for (unsigned lineNumber = 0; lineNumber < lineContexts.size(); ++lineNumber) {
        lineContexts[lineNumber].crossAxisOffset += lineOffset;
        for (size_t childNumber = 0; childNumber < lineContexts[lineNumber].numberOfChildren; ++childNumber, child = iterator.next())
            adjustAlignmentForChild(child, lineOffset);

        if (style()->alignContent() == AlignContentStretch && availableCrossAxisSpace > 0)
            lineContexts[lineNumber].crossAxisExtent += availableCrossAxisSpace / static_cast<unsigned>(lineContexts.size());

        lineOffset += alignContentSpaceBetweenChildren(availableCrossAxisSpace, style()->alignContent(), lineContexts.size());
    }
}

void RenderFlexibleBox::adjustAlignmentForChild(RenderBox* child, LayoutUnit delta)
{
    if (child->isOutOfFlowPositioned()) {
        LayoutUnit staticInlinePosition = child->layer()->staticInlinePosition();
        LayoutUnit staticBlockPosition = child->layer()->staticBlockPosition();
        LayoutUnit mainAxis = isColumnFlow() ? staticBlockPosition : staticInlinePosition;
        LayoutUnit crossAxis = isColumnFlow() ? staticInlinePosition : staticBlockPosition;
        crossAxis += delta;
        prepareChildForPositionedLayout(child, mainAxis, crossAxis, NoFlipForRowReverse);
        return;
    }

    LayoutRect oldRect = child->frameRect();
    setFlowAwareLocationForChild(child, flowAwareLocationForChild(child) + LayoutSize(0, delta));

    // If the child moved, we have to repaint it as well as any floating/positioned
    // descendants. An exception is if we need a layout. In this case, we know we're going to
    // repaint ourselves (and the child) anyway.
    if (!selfNeedsLayout() && child->checkForRepaintDuringLayout())
        child->repaintDuringLayoutIfMoved(oldRect);
}

void RenderFlexibleBox::alignChildren(OrderIterator& iterator, const WTF::Vector<LineContext>& lineContexts)
{
    // Keep track of the space between the baseline edge and the after edge of the box for each line.
    WTF::Vector<LayoutUnit> minMarginAfterBaselines;

    RenderBox* child = iterator.first();
    for (size_t lineNumber = 0; lineNumber < lineContexts.size(); ++lineNumber) {
        LayoutUnit minMarginAfterBaseline = MAX_LAYOUT_UNIT;
        LayoutUnit lineCrossAxisExtent = lineContexts[lineNumber].crossAxisExtent;
        LayoutUnit maxAscent = lineContexts[lineNumber].maxAscent;

        for (size_t childNumber = 0; childNumber < lineContexts[lineNumber].numberOfChildren; ++childNumber, child = iterator.next()) {
            ASSERT(child);
            if (updateAutoMarginsInCrossAxis(child, availableAlignmentSpaceForChild(lineCrossAxisExtent, child)))
                continue;

            switch (alignmentForChild(child)) {
            case AlignAuto:
                ASSERT_NOT_REACHED();
                break;
            case AlignStretch: {
                applyStretchAlignmentToChild(child, lineCrossAxisExtent);
                // Since wrap-reverse flips cross start and cross end, strech children should be aligned with the cross end.
                if (style()->flexWrap() == FlexWrapReverse)
                    adjustAlignmentForChild(child, availableAlignmentSpaceForChild(lineCrossAxisExtent, child));
                break;
            }
            case AlignFlexStart:
                break;
            case AlignFlexEnd:
                adjustAlignmentForChild(child, availableAlignmentSpaceForChild(lineCrossAxisExtent, child));
                break;
            case AlignCenter:
                adjustAlignmentForChild(child, availableAlignmentSpaceForChild(lineCrossAxisExtent, child) / 2);
                break;
            case AlignBaseline: {
                LayoutUnit ascent = marginBoxAscentForChild(child);
                LayoutUnit startOffset = maxAscent - ascent;
                adjustAlignmentForChild(child, startOffset);

                if (style()->flexWrap() == FlexWrapReverse)
                    minMarginAfterBaseline = std::min(minMarginAfterBaseline, availableAlignmentSpaceForChild(lineCrossAxisExtent, child) - startOffset);
                break;
            }
            }
        }
        minMarginAfterBaselines.append(minMarginAfterBaseline);
    }

    if (style()->flexWrap() != FlexWrapReverse)
        return;

    // wrap-reverse flips the cross axis start and end. For baseline alignment, this means we
    // need to align the after edge of baseline elements with the after edge of the flex line.
    child = iterator.first();
    for (size_t lineNumber = 0; lineNumber < lineContexts.size(); ++lineNumber) {
        LayoutUnit minMarginAfterBaseline = minMarginAfterBaselines[lineNumber];
        for (size_t childNumber = 0; childNumber < lineContexts[lineNumber].numberOfChildren; ++childNumber, child = iterator.next()) {
            ASSERT(child);
            if (alignmentForChild(child) == AlignBaseline && !hasAutoMarginsInCrossAxis(child) && minMarginAfterBaseline)
                adjustAlignmentForChild(child, minMarginAfterBaseline);
        }
    }
}

void RenderFlexibleBox::applyStretchAlignmentToChild(RenderBox* child, LayoutUnit lineCrossAxisExtent)
{
    if (!isColumnFlow() && child->style()->logicalHeight().isAuto()) {
        LayoutUnit logicalHeightBefore = child->logicalHeight();
        LayoutUnit stretchedLogicalHeight = child->logicalHeight() + availableAlignmentSpaceForChild(lineCrossAxisExtent, child);

        child->setLogicalHeight(stretchedLogicalHeight);
        child->computeLogicalHeight();

        // FIXME: Can avoid laying out here in some cases. See https://webkit.org/b/87905.
        if (child->logicalHeight() != logicalHeightBefore) {
            child->setOverrideLogicalContentHeight(child->logicalHeight() - child->borderAndPaddingLogicalHeight());
            child->setLogicalHeight(0);
            child->setChildNeedsLayout(true, MarkOnlyThis);
            child->layoutIfNeeded();
        }
    } else if (isColumnFlow() && child->style()->logicalWidth().isAuto() && isMultiline()) {
        // FIXME: Handle min-width and max-width.
        LayoutUnit childWidth = lineCrossAxisExtent - crossAxisMarginExtentForChild(child);
        child->setOverrideLogicalContentWidth(std::max(ZERO_LAYOUT_UNIT, childWidth));
        child->setChildNeedsLayout(true, MarkOnlyThis);
        child->layoutIfNeeded();
    }
}

void RenderFlexibleBox::flipForRightToLeftColumn(OrderIterator& iterator)
{
    if (style()->isLeftToRightDirection() || !isColumnFlow())
        return;

    LayoutUnit crossExtent = crossAxisExtent();
    for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
        if (child->isOutOfFlowPositioned())
            continue;
        LayoutPoint location = flowAwareLocationForChild(child);
        location.setY(crossExtent - crossAxisExtentForChild(child) - location.y());
        setFlowAwareLocationForChild(child, location);
    }
}

void RenderFlexibleBox::flipForWrapReverse(OrderIterator& iterator, const WTF::Vector<LineContext>& lineContexts, LayoutUnit crossAxisStartEdge)
{
    LayoutUnit contentExtent = crossAxisContentExtent();
    RenderBox* child = iterator.first();
    for (size_t lineNumber = 0; lineNumber < lineContexts.size(); ++lineNumber) {
        for (size_t childNumber = 0; childNumber < lineContexts[lineNumber].numberOfChildren; ++childNumber, child = iterator.next()) {
            ASSERT(child);
            LayoutUnit lineCrossAxisExtent = lineContexts[lineNumber].crossAxisExtent;
            LayoutUnit originalOffset = lineContexts[lineNumber].crossAxisOffset - crossAxisStartEdge;
            LayoutUnit newOffset = contentExtent - originalOffset - lineCrossAxisExtent;
            adjustAlignmentForChild(child, newOffset - originalOffset);
        }
    }
}

}
