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

namespace WebCore {

// Normally, -1 and 0 are not valid in a HashSet, but these are relatively likely flex-order values. Instead,
// we make the two smallest int values invalid flex-order values (in the css parser code we clamp them to
// int min + 2).
struct RenderFlexibleBox::FlexOrderHashTraits : WTF::GenericHashTraits<int> {
    static const bool emptyValueIsZero = false;
    static int emptyValue() { return std::numeric_limits<int>::min(); }
    static void constructDeletedValue(int& slot) { slot = std::numeric_limits<int>::min() + 1; }
    static bool isDeletedValue(int value) { return value == std::numeric_limits<int>::min() + 1; }
};

class RenderFlexibleBox::FlexOrderIterator {
public:
    FlexOrderIterator(RenderFlexibleBox* flexibleBox, const FlexOrderHashSet& flexOrderValues)
        : m_flexibleBox(flexibleBox)
        , m_currentChild(0)
        , m_orderValuesIterator(0)
    {
        copyToVector(flexOrderValues, m_orderValues);
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
        } while (!m_currentChild || m_currentChild->style()->flexOrder() != *m_orderValuesIterator);

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

struct RenderFlexibleBox::WrapReverseContext {
    explicit WrapReverseContext(EFlexWrap flexWrap)
        : isWrapReverse(flexWrap == FlexWrapReverse)
    {
    }

    void addCrossAxisOffset(LayoutUnit offset)
    {
        if (!isWrapReverse)
            return;
        crossAxisOffsets.append(offset);
    }

    void addNumberOfChildrenOnLine(size_t numberOfChildren)
    {
        if (!isWrapReverse)
            return;
        childrenPerLine.append(numberOfChildren);
    }

    LayoutUnit lineCrossAxisDelta(size_t line, LayoutUnit crossAxisContentExtent) const
    {
        ASSERT(line + 1 < crossAxisOffsets.size());
        LayoutUnit lineHeight = crossAxisOffsets[line + 1] - crossAxisOffsets[line];
        LayoutUnit originalOffset = crossAxisOffsets[line] - crossAxisOffsets[0];
        LayoutUnit newOffset = crossAxisContentExtent - originalOffset - lineHeight;
        return newOffset - originalOffset;
    }

    WTF::Vector<LayoutUnit> crossAxisOffsets;
    WTF::Vector<size_t> childrenPerLine;
    bool isWrapReverse;
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
    if (styleToUse->logicalWidth().isFixed() && styleToUse->logicalWidth().value() > 0)
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = computeContentBoxLogicalWidth(styleToUse->logicalWidth().value());
    else {
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = 0;

        for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
            if (child->isPositioned())
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
            layer()->setHasVerticalScrollbar(true);
            scrollbarWidth = verticalScrollbarWidth();
        } else if (!isHorizontalWritingMode() && styleToUse->overflowX() == OSCROLL) {
            layer()->setHasHorizontalScrollbar(true);
            scrollbarWidth = horizontalScrollbarHeight();
        }
    }

    m_maxPreferredLogicalWidth += scrollbarWidth;
    m_minPreferredLogicalWidth += scrollbarWidth;

    if (styleToUse->logicalMinWidth().isFixed() && styleToUse->logicalMinWidth().value() > 0) {
        m_maxPreferredLogicalWidth = std::max(m_maxPreferredLogicalWidth, computeContentBoxLogicalWidth(styleToUse->logicalMinWidth().value()));
        m_minPreferredLogicalWidth = std::max(m_minPreferredLogicalWidth, computeContentBoxLogicalWidth(styleToUse->logicalMinWidth().value()));
    }

    if (styleToUse->logicalMaxWidth().isFixed()) {
        m_maxPreferredLogicalWidth = std::min(m_maxPreferredLogicalWidth, computeContentBoxLogicalWidth(styleToUse->logicalMaxWidth().value()));
        m_minPreferredLogicalWidth = std::min(m_minPreferredLogicalWidth, computeContentBoxLogicalWidth(styleToUse->logicalMaxWidth().value()));
    }

    LayoutUnit borderAndPadding = borderAndPaddingLogicalWidth();
    m_minPreferredLogicalWidth += borderAndPadding;
    m_maxPreferredLogicalWidth += borderAndPadding;

    setPreferredLogicalWidthsDirty(false);
}

void RenderFlexibleBox::layoutBlock(bool relayoutChildren, int, BlockLayoutPass)
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

    IntSize previousSize = size();

    setLogicalHeight(0);
    // We need to call both of these because we grab both crossAxisExtent and mainAxisExtent in layoutFlexItems.
    computeLogicalWidth();
    computeLogicalHeight();

    m_overflow.clear();

    // For overflow:scroll blocks, ensure we have both scrollbars in place always.
    if (scrollsOverflow()) {
        if (style()->overflowX() == OSCROLL)
            layer()->setHasHorizontalScrollbar(true);
        if (style()->overflowY() == OSCROLL)
            layer()->setHasVerticalScrollbar(true);
    }

    layoutFlexItems(relayoutChildren);

    LayoutUnit oldClientAfterEdge = clientLogicalBottom();
    computeLogicalHeight();

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
    updateScrollInfoAfterLayout();

    repainter.repaintAfterLayout();

    setNeedsLayout(false);
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

Length RenderFlexibleBox::mainAxisLengthForChild(RenderBox* child) const
{
    return isHorizontalFlow() ? child->style()->width() : child->style()->height();
}

Length RenderFlexibleBox::crossAxisLength() const
{
    return isHorizontalFlow() ? style()->height() : style()->width();
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

LayoutUnit RenderFlexibleBox::mainAxisContentExtent() const
{
    return isHorizontalFlow() ? contentWidth() : contentHeight();
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

LayoutUnit RenderFlexibleBox::preferredMainAxisContentExtentForChild(RenderBox* child) const
{
    Length mainAxisLength = mainAxisLengthForChild(child);
    if (mainAxisLength.isAuto()) {
        LayoutUnit mainAxisExtent = hasOrthogonalFlow(child) ? child->logicalHeight() : child->maxPreferredLogicalWidth();
        return mainAxisExtent - mainAxisBorderAndPaddingExtentForChild(child);
    }
    return miminumValueForLength(mainAxisLength, mainAxisContentExtent());
}

LayoutUnit RenderFlexibleBox::computeAvailableFreeSpace(LayoutUnit preferredMainAxisExtent)
{
    if (!isColumnFlow())
        return mainAxisContentExtent() - preferredMainAxisExtent;

    if (hasOverrideHeight())
        return overrideHeight();

    LayoutUnit heightResult = computeContentLogicalHeightUsing(style()->logicalHeight());
    if (heightResult == -1)
        heightResult = preferredMainAxisExtent;
    LayoutUnit minHeight = computeContentLogicalHeightUsing(style()->logicalMinHeight()); // Leave as -1 if unset.
    LayoutUnit maxHeight = style()->logicalMaxHeight().isUndefined() ? heightResult : computeContentLogicalHeightUsing(style()->logicalMaxHeight());
    if (maxHeight == -1)
        maxHeight = heightResult;
    heightResult = std::min(maxHeight, heightResult);
    heightResult = std::max(minHeight, heightResult);

    return heightResult - preferredMainAxisExtent;
}

void RenderFlexibleBox::layoutFlexItems(bool relayoutChildren)
{
    FlexOrderHashSet flexOrderValues;
    computeMainAxisPreferredSizes(relayoutChildren, flexOrderValues);

    OrderedFlexItemList orderedChildren;
    LayoutUnit preferredMainAxisExtent;
    float totalPositiveFlexibility;
    float totalNegativeFlexibility;
    LayoutUnit minMaxAppliedMainAxisExtent;
    FlexOrderIterator flexIterator(this, flexOrderValues);

    // For wrap-reverse, we need to layout as wrap, then reverse the lines. The next two arrays
    // are some extra information so it's possible to reverse the lines.
    WrapReverseContext wrapReverseContext(style()->flexWrap());

    LayoutUnit crossAxisOffset = flowAwareBorderBefore() + flowAwarePaddingBefore();
    while (computeNextFlexLine(flexIterator, orderedChildren, preferredMainAxisExtent, totalPositiveFlexibility, totalNegativeFlexibility, minMaxAppliedMainAxisExtent)) {
        LayoutUnit availableFreeSpace = computeAvailableFreeSpace(preferredMainAxisExtent);
        FlexSign flexSign = (minMaxAppliedMainAxisExtent < preferredMainAxisExtent + availableFreeSpace) ? PositiveFlexibility : NegativeFlexibility;
        InflexibleFlexItemSize inflexibleItems;
        WTF::Vector<LayoutUnit> childSizes;
        while (!resolveFlexibleLengths(flexSign, orderedChildren, availableFreeSpace, totalPositiveFlexibility, totalNegativeFlexibility, inflexibleItems, childSizes)) {
            ASSERT(totalPositiveFlexibility >= 0 && totalNegativeFlexibility >= 0);
            ASSERT(inflexibleItems.size() > 0);
        }

        wrapReverseContext.addNumberOfChildrenOnLine(orderedChildren.size());
        wrapReverseContext.addCrossAxisOffset(crossAxisOffset);
        layoutAndPlaceChildren(crossAxisOffset, orderedChildren, childSizes, availableFreeSpace);
    }

    if (wrapReverseContext.isWrapReverse) {
        wrapReverseContext.addCrossAxisOffset(crossAxisOffset);
        flipForWrapReverse(flexIterator, wrapReverseContext);
    }

    // direction:rtl + flex-direction:column means the cross-axis direction is flipped.
    flipForRightToLeftColumn(flexIterator);
}

float RenderFlexibleBox::positiveFlexForChild(RenderBox* child) const
{
    return isHorizontalFlow() ? child->style()->flexboxWidthPositiveFlex() : child->style()->flexboxHeightPositiveFlex();
}

float RenderFlexibleBox::negativeFlexForChild(RenderBox* child) const
{
    return isHorizontalFlow() ? child->style()->flexboxWidthNegativeFlex() : child->style()->flexboxHeightNegativeFlex();
}

LayoutUnit RenderFlexibleBox::availableAlignmentSpaceForChild(LayoutUnit lineCrossAxisExtent, RenderBox* child)
{
    LayoutUnit childCrossExtent = crossAxisMarginExtentForChild(child) + crossAxisExtentForChild(child);
    return lineCrossAxisExtent - childCrossExtent;
}

LayoutUnit RenderFlexibleBox::marginBoxAscentForChild(RenderBox* child)
{
    LayoutUnit ascent = child->firstLineBoxBaseline();
    if (ascent == -1)
        ascent = crossAxisExtentForChild(child) + flowAwareMarginAfterForChild(child);
    return ascent + flowAwareMarginBeforeForChild(child);
}

void RenderFlexibleBox::computeMainAxisPreferredSizes(bool relayoutChildren, FlexOrderHashSet& flexOrderValues)
{
    LayoutUnit flexboxAvailableContentExtent = mainAxisContentExtent();
    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        flexOrderValues.add(child->style()->flexOrder());

        if (child->isPositioned())
            continue;

        child->clearOverrideSize();
        if (mainAxisLengthForChild(child).isAuto()) {
            if (!relayoutChildren)
                child->setChildNeedsLayout(true);
            child->layoutIfNeeded();
        }

        // We set the margins because we want to make sure 'auto' has a margin
        // of 0 and because if we're not auto sizing, we don't do a layout that
        // computes the start/end margins.
        if (isHorizontalFlow()) {
            child->setMarginLeft(miminumValueForLength(child->style()->marginLeft(), flexboxAvailableContentExtent));
            child->setMarginRight(miminumValueForLength(child->style()->marginRight(), flexboxAvailableContentExtent));
        } else {
            child->setMarginTop(miminumValueForLength(child->style()->marginTop(), flexboxAvailableContentExtent));
            child->setMarginBottom(miminumValueForLength(child->style()->marginBottom(), flexboxAvailableContentExtent));
        }
    }
}

LayoutUnit RenderFlexibleBox::lineBreakLength()
{
    if (!isColumnFlow())
        return mainAxisContentExtent();

    LayoutUnit height = computeContentLogicalHeightUsing(style()->logicalHeight());
    if (height == -1)
        height = std::numeric_limits<LayoutUnit>::max();
    LayoutUnit maxHeight = computeContentLogicalHeightUsing(style()->logicalMaxHeight());
    if (maxHeight != -1)
        height = std::min(height, maxHeight);
    return height;
}

bool RenderFlexibleBox::computeNextFlexLine(FlexOrderIterator& iterator, OrderedFlexItemList& orderedChildren, LayoutUnit& preferredMainAxisExtent, float& totalPositiveFlexibility, float& totalNegativeFlexibility, LayoutUnit& minMaxAppliedMainAxisExtent)
{
    orderedChildren.clear();
    preferredMainAxisExtent = 0;
    totalPositiveFlexibility = totalNegativeFlexibility = 0;
    minMaxAppliedMainAxisExtent = 0;

    if (!iterator.currentChild())
        return false;

    LayoutUnit flexboxAvailableContentExtent = mainAxisContentExtent();
    LayoutUnit lineBreak = lineBreakLength();

    for (RenderBox* child = iterator.currentChild(); child; child = iterator.next()) {
        if (child->isPositioned()) {
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
        totalPositiveFlexibility += positiveFlexForChild(child);
        totalNegativeFlexibility += negativeFlexForChild(child);

        LayoutUnit childMinMaxAppliedMainAxisExtent = childMainAxisExtent;
        // FIXME: valueForLength isn't quite right in quirks mode: percentage heights should check parents until a value is found.
        // https://bugs.webkit.org/show_bug.cgi?id=81809
        Length maxLength = isHorizontalFlow() ? child->style()->maxWidth() : child->style()->maxHeight();
        if (maxLength.isSpecified() && childMinMaxAppliedMainAxisExtent > valueForLength(maxLength, flexboxAvailableContentExtent))
            childMinMaxAppliedMainAxisExtent = valueForLength(maxLength, flexboxAvailableContentExtent);
        Length minLength = isHorizontalFlow() ? child->style()->minWidth() : child->style()->minHeight();
        if (minLength.isSpecified() && childMinMaxAppliedMainAxisExtent < valueForLength(minLength, flexboxAvailableContentExtent))
            childMinMaxAppliedMainAxisExtent = valueForLength(minLength, flexboxAvailableContentExtent);
        minMaxAppliedMainAxisExtent += childMinMaxAppliedMainAxisExtent - childMainAxisExtent + childMainAxisMarginBoxExtent;
    }
    return true;
}

// Returns true if we successfully ran the algorithm and sized the flex items.
bool RenderFlexibleBox::resolveFlexibleLengths(FlexSign, const OrderedFlexItemList& children, LayoutUnit& availableFreeSpace, float& totalPositiveFlexibility, float& totalNegativeFlexibility, InflexibleFlexItemSize& inflexibleItems, WTF::Vector<LayoutUnit>& childSizes)
{
    childSizes.clear();

    LayoutUnit flexboxAvailableContentExtent = mainAxisContentExtent();
    for (size_t i = 0; i < children.size(); ++i) {
        RenderBox* child = children[i];
        if (child->isPositioned()) {
            childSizes.append(0);
            continue;
        }

        LayoutUnit childPreferredSize;
        if (inflexibleItems.contains(child))
            childPreferredSize = inflexibleItems.get(child);
        else {
            childPreferredSize = preferredMainAxisContentExtentForChild(child);
            if (availableFreeSpace > 0 && totalPositiveFlexibility > 0) {
                childPreferredSize += lroundf(availableFreeSpace * positiveFlexForChild(child) / totalPositiveFlexibility);

                Length childLogicalMaxWidth = isHorizontalFlow() ? child->style()->maxWidth() : child->style()->maxHeight();
                if (childLogicalMaxWidth.isSpecified() && childPreferredSize > valueForLength(childLogicalMaxWidth, flexboxAvailableContentExtent)) {
                    childPreferredSize = valueForLength(childLogicalMaxWidth, flexboxAvailableContentExtent);
                    availableFreeSpace -= childPreferredSize - preferredMainAxisContentExtentForChild(child);
                    totalPositiveFlexibility -= positiveFlexForChild(child);

                    inflexibleItems.set(child, childPreferredSize);
                    return false;
                }
            } else if (availableFreeSpace < 0 && totalNegativeFlexibility > 0) {
                childPreferredSize += lroundf(availableFreeSpace * negativeFlexForChild(child) / totalNegativeFlexibility);

                Length childLogicalMinWidth = isHorizontalFlow() ? child->style()->minWidth() : child->style()->minHeight();
                if (childLogicalMinWidth.isSpecified() && childPreferredSize < valueForLength(childLogicalMinWidth, flexboxAvailableContentExtent)) {
                    childPreferredSize = valueForLength(childLogicalMinWidth, flexboxAvailableContentExtent);
                    availableFreeSpace += preferredMainAxisContentExtentForChild(child) - childPreferredSize;
                    totalNegativeFlexibility -= negativeFlexForChild(child);

                    inflexibleItems.set(child, childPreferredSize);
                    return false;
                }
            }
        }
        childSizes.append(childPreferredSize);
    }
    return true;
}

static LayoutUnit initialPackingOffset(LayoutUnit availableFreeSpace, EFlexPack flexPack, size_t numberOfChildren)
{
    if (availableFreeSpace > 0) {
        if (flexPack == PackEnd)
            return availableFreeSpace;
        if (flexPack == PackCenter)
            return availableFreeSpace / 2;
        if (flexPack == PackDistribute && numberOfChildren)
            return availableFreeSpace / (2 * numberOfChildren);
    } else if (availableFreeSpace < 0) {
        if (flexPack == PackCenter || flexPack == PackDistribute)
            return availableFreeSpace / 2;
    }
    return 0;
}

static LayoutUnit packingSpaceBetweenChildren(LayoutUnit availableFreeSpace, EFlexPack flexPack, size_t numberOfChildren)
{
    if (availableFreeSpace > 0 && numberOfChildren > 1) {
        if (flexPack == PackJustify)
            return availableFreeSpace / (numberOfChildren - 1);
        if (flexPack == PackDistribute)
            return availableFreeSpace / numberOfChildren;
    }
    return 0;
}

void RenderFlexibleBox::setLogicalOverrideSize(RenderBox* child, LayoutUnit childPreferredSize)
{
    // FIXME: Rename setOverrideWidth/setOverrideHeight to setOverrideLogicalWidth/setOverrideLogicalHeight.
    if (hasOrthogonalFlow(child))
        child->setOverrideHeight(childPreferredSize);
    else
        child->setOverrideWidth(childPreferredSize);
}

void RenderFlexibleBox::prepareChildForPositionedLayout(RenderBox* child, LayoutUnit mainAxisOffset, LayoutUnit crossAxisOffset)
{
    ASSERT(child->isPositioned());
    child->containingBlock()->insertPositionedObject(child);
    RenderLayer* childLayer = child->layer();
    LayoutUnit inlinePosition = isColumnFlow() ? crossAxisOffset : mainAxisOffset;
    if (style()->flexDirection() == FlowRowReverse)
        inlinePosition = mainAxisExtent() - mainAxisOffset;
    childLayer->setStaticInlinePosition(inlinePosition); // FIXME: Not right for regions.

    LayoutUnit staticBlockPosition = isColumnFlow() ? mainAxisOffset : crossAxisOffset;
    if (childLayer->staticBlockPosition() != staticBlockPosition) {
        childLayer->setStaticBlockPosition(staticBlockPosition);
        if (child->style()->hasStaticBlockPosition(style()->isHorizontalWritingMode()))
            child->setChildNeedsLayout(true, false);
    }
}

static EFlexAlign flexAlignForChild(RenderBox* child)
{
    EFlexAlign align = child->style()->flexItemAlign();
    if (align == AlignAuto)
        align = child->parent()->style()->flexAlign();

    if (child->parent()->style()->flexWrap() == FlexWrapReverse) {
        if (align == AlignStart)
            align = AlignEnd;
        else if (align == AlignEnd)
            align = AlignStart;
    }

    return align;
}

void RenderFlexibleBox::layoutAndPlaceChildren(LayoutUnit& crossAxisOffset, const OrderedFlexItemList& children, const WTF::Vector<LayoutUnit>& childSizes, LayoutUnit availableFreeSpace)
{
    LayoutUnit mainAxisOffset = flowAwareBorderStart() + flowAwarePaddingStart();
    mainAxisOffset += initialPackingOffset(availableFreeSpace, style()->flexPack(), childSizes.size());
    if (style()->flexDirection() == FlowRowReverse)
        mainAxisOffset += isHorizontalFlow() ? verticalScrollbarWidth() : horizontalScrollbarHeight();

    LayoutUnit totalMainExtent = mainAxisExtent();
    LayoutUnit maxAscent = 0, maxDescent = 0; // Used when flex-align: baseline.
    LayoutUnit maxChildCrossAxisExtent = 0;
    bool shouldFlipMainAxis = !isColumnFlow() && !isLeftToRightFlow();
    for (size_t i = 0; i < children.size(); ++i) {
        RenderBox* child = children[i];
        if (child->isPositioned()) {
            prepareChildForPositionedLayout(child, mainAxisOffset, crossAxisOffset);
            mainAxisOffset += packingSpaceBetweenChildren(availableFreeSpace, style()->flexPack(), childSizes.size());
            continue;
        }
        LayoutUnit childPreferredSize = childSizes[i] + mainAxisBorderAndPaddingExtentForChild(child);
        setLogicalOverrideSize(child, childPreferredSize);
        child->setChildNeedsLayout(true);
        child->layoutIfNeeded();

        LayoutUnit childCrossAxisMarginBoxExtent;
        if (flexAlignForChild(child) == AlignBaseline) {
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
        IntPoint childLocation(shouldFlipMainAxis ? totalMainExtent - mainAxisOffset - childMainExtent : mainAxisOffset,
            crossAxisOffset + flowAwareMarginBeforeForChild(child));

        // FIXME: Supporting layout deltas.
        setFlowAwareLocationForChild(child, childLocation);
        mainAxisOffset += childMainExtent + flowAwareMarginEndForChild(child);

        mainAxisOffset += packingSpaceBetweenChildren(availableFreeSpace, style()->flexPack(), childSizes.size());
    }

    if (isColumnFlow())
        setLogicalHeight(mainAxisOffset + flowAwareBorderEnd() + flowAwarePaddingEnd() + scrollbarLogicalHeight());

    if (style()->flexDirection() == FlowColumnReverse) {
        // We have to do an extra pass for column-reverse to reposition the flex items since the start depends
        // on the height of the flexbox, which we only know after we've positioned all the flex items.
        computeLogicalHeight();
        layoutColumnReverse(children, childSizes, crossAxisOffset, availableFreeSpace);
    }

    LayoutUnit lineCrossAxisExtent = isMultiline() ? maxChildCrossAxisExtent : crossAxisContentExtent();
    alignChildren(children, lineCrossAxisExtent, maxAscent);

    crossAxisOffset += lineCrossAxisExtent;
}

void RenderFlexibleBox::layoutColumnReverse(const OrderedFlexItemList& children, const WTF::Vector<LayoutUnit>& childSizes, LayoutUnit crossAxisOffset, LayoutUnit availableFreeSpace)
{
    // This is similar to the logic in layoutAndPlaceChildren, except we place the children
    // starting from the end of the flexbox. We also don't need to layout anything since we're
    // just moving the children to a new position.
    LayoutUnit mainAxisOffset = logicalHeight() - flowAwareBorderEnd() - flowAwarePaddingEnd();
    mainAxisOffset -= initialPackingOffset(availableFreeSpace, style()->flexPack(), childSizes.size());
    mainAxisOffset -= isHorizontalFlow() ? verticalScrollbarWidth() : horizontalScrollbarHeight();

    for (size_t i = 0; i < children.size(); ++i) {
        RenderBox* child = children[i];
        if (child->isPositioned()) {
            child->layer()->setStaticBlockPosition(mainAxisOffset);
            mainAxisOffset -= packingSpaceBetweenChildren(availableFreeSpace, style()->flexPack(), childSizes.size());
            continue;
        }
        mainAxisOffset -= mainAxisExtentForChild(child) + flowAwareMarginEndForChild(child);

        LayoutRect oldRect = child->frameRect();
        setFlowAwareLocationForChild(child, IntPoint(mainAxisOffset, crossAxisOffset + flowAwareMarginBeforeForChild(child)));
        if (!selfNeedsLayout() && child->checkForRepaintDuringLayout())
            child->repaintDuringLayoutIfMoved(oldRect);

        mainAxisOffset -= flowAwareMarginStartForChild(child);
        mainAxisOffset -= packingSpaceBetweenChildren(availableFreeSpace, style()->flexPack(), childSizes.size());
    }
}

void RenderFlexibleBox::adjustAlignmentForChild(RenderBox* child, LayoutUnit delta)
{
    LayoutRect oldRect = child->frameRect();

    setFlowAwareLocationForChild(child, flowAwareLocationForChild(child) + LayoutSize(0, delta));

    // If the child moved, we have to repaint it as well as any floating/positioned
    // descendants. An exception is if we need a layout. In this case, we know we're going to
    // repaint ourselves (and the child) anyway.
    if (!selfNeedsLayout() && child->checkForRepaintDuringLayout())
        child->repaintDuringLayoutIfMoved(oldRect);
}

void RenderFlexibleBox::alignChildren(const OrderedFlexItemList& children, LayoutUnit lineCrossAxisExtent, LayoutUnit maxAscent)
{
    LayoutUnit minMarginAfterBaseline = std::numeric_limits<LayoutUnit>::max();

    for (size_t i = 0; i < children.size(); ++i) {
        RenderBox* child = children[i];
        switch (flexAlignForChild(child)) {
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
        case AlignStart:
            break;
        case AlignEnd:
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

    // wrap-reverse flips the cross axis start and end. For baseline alignment, this means we
    // need to align the after edge of baseline elements with the after edge of the flex line.
    if (style()->flexWrap() == FlexWrapReverse && minMarginAfterBaseline) {
        for (size_t i = 0; i < children.size(); ++i) {
            RenderBox* child = children[i];
            if (flexAlignForChild(child) == AlignBaseline)
                adjustAlignmentForChild(child, minMarginAfterBaseline);
        }
    }
}

void RenderFlexibleBox::applyStretchAlignmentToChild(RenderBox* child, LayoutUnit lineCrossAxisExtent)
{
    if (!isColumnFlow() && child->style()->logicalHeight().isAuto()) {
        LayoutUnit logicalHeightBefore = child->logicalHeight();
        LayoutUnit stretchedLogicalHeight = child->logicalHeight() + availableAlignmentSpaceForChild(lineCrossAxisExtent, child);
        if (stretchedLogicalHeight < logicalHeightBefore)
            return;

        child->setLogicalHeight(stretchedLogicalHeight);
        child->computeLogicalHeight();

        if (child->logicalHeight() != logicalHeightBefore) {
            child->setOverrideHeight(child->logicalHeight());
            child->setLogicalHeight(0);
            child->setChildNeedsLayout(true);
            child->layoutIfNeeded();
        }
    } else if (isColumnFlow() && child->style()->logicalWidth().isAuto() && isMultiline()) {
        // FIXME: Handle min-width and max-width.
        LayoutUnit childWidth = lineCrossAxisExtent - crossAxisMarginExtentForChild(child);
        child->setOverrideWidth(std::max(0, childWidth));
        child->setChildNeedsLayout(true);
        child->layoutIfNeeded();
    }
}

void RenderFlexibleBox::flipForRightToLeftColumn(FlexOrderIterator& iterator)
{
    if (style()->isLeftToRightDirection() || !isColumnFlow())
        return;

    LayoutUnit crossExtent = crossAxisExtent();
    for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
        LayoutPoint location = flowAwareLocationForChild(child);
        location.setY(crossExtent - crossAxisExtentForChild(child) - location.y());
        setFlowAwareLocationForChild(child, location);
    }
}

void RenderFlexibleBox::flipForWrapReverse(FlexOrderIterator& iterator, const WrapReverseContext& wrapReverseContext)
{
    if (!isColumnFlow())
        computeLogicalHeight();

    size_t currentChild = 0;
    size_t lineNumber = 0;
    LayoutUnit contentExtent = crossAxisContentExtent();
    for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
        LayoutPoint location = flowAwareLocationForChild(child);
        location.setY(location.y() + wrapReverseContext.lineCrossAxisDelta(lineNumber, contentExtent));

        LayoutRect oldRect = child->frameRect();
        setFlowAwareLocationForChild(child, location);
        if (!selfNeedsLayout() && child->checkForRepaintDuringLayout())
            child->repaintDuringLayoutIfMoved(oldRect);

        if (++currentChild == wrapReverseContext.childrenPerLine[lineNumber]) {
            ++lineNumber;
            currentChild = 0;
        }
    }
}

}
