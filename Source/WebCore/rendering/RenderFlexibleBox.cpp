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

    // For overflow:scroll blocks, ensure we have both scrollbars in place always.
    if (scrollsOverflow()) {
        if (style()->overflowX() == OSCROLL)
            layer()->setHasHorizontalScrollbar(true);
        if (style()->overflowY() == OSCROLL)
            layer()->setHasVerticalScrollbar(true);
    }

    WTF::Vector<LineContext> lineContexts;
    FlexOrderHashSet flexOrderValues;
    computeMainAxisPreferredSizes(relayoutChildren, flexOrderValues);
    FlexOrderIterator flexIterator(this, flexOrderValues);
    layoutFlexItems(flexIterator, lineContexts);

    LayoutUnit oldClientAfterEdge = clientLogicalBottom();
    computeLogicalHeight();
    repositionLogicalHeightDependentFlexItems(flexIterator, lineContexts, oldClientAfterEdge);

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

void RenderFlexibleBox::repositionLogicalHeightDependentFlexItems(FlexOrderIterator& iterator, WTF::Vector<LineContext>& lineContexts, LayoutUnit& oldClientAfterEdge)
{
    LayoutUnit crossAxisStartEdge = lineContexts.isEmpty() ? ZERO_LAYOUT_UNIT : lineContexts[0].crossAxisOffset;
    packFlexLines(iterator, lineContexts);

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

Length RenderFlexibleBox::preferredLengthForChild(RenderBox* child) const
{
    Length flexLength = child->style()->flexPreferredSize();
    if (flexLength.isAuto())
        flexLength = isHorizontalFlow() ? child->style()->width() : child->style()->height();
    return flexLength;
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
    Length mainAxisLength = preferredLengthForChild(child);
    if (mainAxisLength.isAuto()) {
        LayoutUnit mainAxisExtent = hasOrthogonalFlow(child) ? child->logicalHeight() : child->maxPreferredLogicalWidth();
        return mainAxisExtent - mainAxisBorderAndPaddingExtentForChild(child);
    }
    return minimumValueForLength(mainAxisLength, mainAxisContentExtent(), view());
}

LayoutUnit RenderFlexibleBox::computeAvailableFreeSpace(LayoutUnit preferredMainAxisExtent)
{
    LayoutUnit contentExtent = 0;
    if (!isColumnFlow())
        contentExtent = mainAxisContentExtent();
    else if (hasOverrideHeight())
        contentExtent = overrideHeight() - (logicalHeight() - contentLogicalHeight());
    else {
        LayoutUnit heightResult = computeContentLogicalHeightUsing(style()->logicalHeight());
        if (heightResult == -1)
            heightResult = preferredMainAxisExtent;
        LayoutUnit minHeight = computeContentLogicalHeightUsing(style()->logicalMinHeight()); // Leave as -1 if unset.
        LayoutUnit maxHeight = style()->logicalMaxHeight().isUndefined() ? heightResult : computeContentLogicalHeightUsing(style()->logicalMaxHeight());
        if (maxHeight == -1)
            maxHeight = heightResult;
        heightResult = std::min(maxHeight, heightResult);
        heightResult = std::max(minHeight, heightResult);
        contentExtent = heightResult;
    }

    return contentExtent - preferredMainAxisExtent;
}

void RenderFlexibleBox::layoutFlexItems(FlexOrderIterator& iterator, WTF::Vector<LineContext>& lineContexts)
{
    OrderedFlexItemList orderedChildren;
    LayoutUnit preferredMainAxisExtent;
    float totalPositiveFlexibility;
    float totalNegativeFlexibility;
    LayoutUnit minMaxAppliedMainAxisExtent;

    LayoutUnit crossAxisOffset = flowAwareBorderBefore() + flowAwarePaddingBefore();
    while (computeNextFlexLine(iterator, orderedChildren, preferredMainAxisExtent, totalPositiveFlexibility, totalNegativeFlexibility, minMaxAppliedMainAxisExtent)) {
        LayoutUnit availableFreeSpace = computeAvailableFreeSpace(preferredMainAxisExtent);
        FlexSign flexSign = (minMaxAppliedMainAxisExtent < preferredMainAxisExtent + availableFreeSpace) ? PositiveFlexibility : NegativeFlexibility;
        InflexibleFlexItemSize inflexibleItems;
        WTF::Vector<LayoutUnit> childSizes;
        while (!resolveFlexibleLengths(flexSign, orderedChildren, availableFreeSpace, totalPositiveFlexibility, totalNegativeFlexibility, inflexibleItems, childSizes)) {
            ASSERT(totalPositiveFlexibility >= 0 && totalNegativeFlexibility >= 0);
            ASSERT(inflexibleItems.size() > 0);
        }

        layoutAndPlaceChildren(crossAxisOffset, orderedChildren, childSizes, availableFreeSpace, lineContexts);
    }
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
    RenderView* renderView = view();
    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        flexOrderValues.add(child->style()->flexOrder());

        if (child->isPositioned())
            continue;

        child->clearOverrideSize();
        if (preferredLengthForChild(child).isAuto()) {
            if (!relayoutChildren)
                child->setChildNeedsLayout(true);
            child->layoutIfNeeded();
        }

        // We set the margins because we want to make sure 'auto' has a margin
        // of 0 and because if we're not auto sizing, we don't do a layout that
        // computes the start/end margins.
        if (isHorizontalFlow()) {
            child->setMarginLeft(minimumValueForLength(child->style()->marginLeft(), flexboxAvailableContentExtent, renderView));
            child->setMarginRight(minimumValueForLength(child->style()->marginRight(), flexboxAvailableContentExtent, renderView));
        } else {
            child->setMarginTop(minimumValueForLength(child->style()->marginTop(), flexboxAvailableContentExtent, renderView));
            child->setMarginBottom(minimumValueForLength(child->style()->marginBottom(), flexboxAvailableContentExtent, renderView));
        }
    }
}

LayoutUnit RenderFlexibleBox::lineBreakLength()
{
    if (!isColumnFlow())
        return mainAxisContentExtent();

    LayoutUnit height = computeContentLogicalHeightUsing(style()->logicalHeight());
    if (height == -1)
        height = MAX_LAYOUT_UNIT;
    LayoutUnit maxHeight = computeContentLogicalHeightUsing(style()->logicalMaxHeight());
    if (maxHeight != -1)
        height = std::min(height, maxHeight);
    return height;
}

LayoutUnit RenderFlexibleBox::adjustChildSizeForMinAndMax(RenderBox* child, LayoutUnit childSize, LayoutUnit flexboxAvailableContentExtent)
{
    Length max = isHorizontalFlow() ? child->style()->maxWidth() : child->style()->maxHeight();
    Length min = isHorizontalFlow() ? child->style()->minWidth() : child->style()->minHeight();
    RenderView* renderView = view();
    // FIXME: valueForLength isn't quite right in quirks mode: percentage heights should check parents until a value is found.
    // https://bugs.webkit.org/show_bug.cgi?id=81809
    if (max.isSpecified() && childSize > valueForLength(max, flexboxAvailableContentExtent, renderView))
        childSize = valueForLength(max, flexboxAvailableContentExtent, renderView);
    if (min.isSpecified() && childSize < valueForLength(min, flexboxAvailableContentExtent, renderView))
        childSize = valueForLength(min, flexboxAvailableContentExtent, renderView);
    return childSize;
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
        totalPositiveFlexibility += child->style()->positiveFlex();
        totalNegativeFlexibility += child->style()->negativeFlex();

        LayoutUnit childMinMaxAppliedMainAxisExtent = adjustChildSizeForMinAndMax(child, childMainAxisExtent, flexboxAvailableContentExtent);
        minMaxAppliedMainAxisExtent += childMinMaxAppliedMainAxisExtent - childMainAxisExtent + childMainAxisMarginBoxExtent;
    }
    return true;
}

void RenderFlexibleBox::freezeViolations(const WTF::Vector<Violation>& violations, LayoutUnit& availableFreeSpace, float& totalPositiveFlexibility, float& totalNegativeFlexibility, InflexibleFlexItemSize& inflexibleItems)
{
    for (size_t i = 0; i < violations.size(); ++i) {
        RenderBox* child = violations[i].child;
        LayoutUnit childSize = violations[i].childSize;
        availableFreeSpace -= childSize - preferredMainAxisContentExtentForChild(child);
        totalPositiveFlexibility -= child->style()->positiveFlex();
        totalNegativeFlexibility -= child->style()->negativeFlex();
        inflexibleItems.set(child, childSize);
    }
}

// Returns true if we successfully ran the algorithm and sized the flex items.
bool RenderFlexibleBox::resolveFlexibleLengths(FlexSign flexSign, const OrderedFlexItemList& children, LayoutUnit& availableFreeSpace, float& totalPositiveFlexibility, float& totalNegativeFlexibility, InflexibleFlexItemSize& inflexibleItems, WTF::Vector<LayoutUnit>& childSizes)
{
    childSizes.clear();
    LayoutUnit flexboxAvailableContentExtent = mainAxisContentExtent();
    LayoutUnit totalViolation = 0;
    LayoutUnit usedFreeSpace = 0;
    WTF::Vector<Violation> minViolations;
    WTF::Vector<Violation> maxViolations;
    for (size_t i = 0; i < children.size(); ++i) {
        RenderBox* child = children[i];
        if (child->isPositioned()) {
            childSizes.append(0);
            continue;
        }

        if (inflexibleItems.contains(child))
            childSizes.append(inflexibleItems.get(child));
        else {
            LayoutUnit preferredChildSize = preferredMainAxisContentExtentForChild(child);
            LayoutUnit childSize = preferredChildSize;
            if (availableFreeSpace > 0 && totalPositiveFlexibility > 0 && flexSign == PositiveFlexibility)
                childSize += lroundf(availableFreeSpace * child->style()->positiveFlex() / totalPositiveFlexibility);
            else if (availableFreeSpace < 0 && totalNegativeFlexibility > 0  && flexSign == NegativeFlexibility)
                childSize += lroundf(availableFreeSpace * child->style()->negativeFlex() / totalNegativeFlexibility);

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
        freezeViolations(totalViolation < 0 ? maxViolations : minViolations, availableFreeSpace, totalPositiveFlexibility, totalNegativeFlexibility, inflexibleItems);
    else
        availableFreeSpace -= usedFreeSpace;

    return !totalViolation;
}

static LayoutUnit initialPackingOffset(LayoutUnit availableFreeSpace, EFlexPack flexPack, unsigned numberOfChildren)
{
    if (flexPack == PackEnd)
        return availableFreeSpace;
    if (flexPack == PackCenter)
        return availableFreeSpace / 2;
    if (flexPack == PackDistribute) {
        if (availableFreeSpace > 0 && numberOfChildren)
            return availableFreeSpace / (2 * numberOfChildren);
        if (availableFreeSpace < 0)
            return availableFreeSpace / 2;
    }
    return 0;
}

static LayoutUnit packingSpaceBetweenChildren(LayoutUnit availableFreeSpace, EFlexPack flexPack, unsigned numberOfChildren)
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
            child->setChildNeedsLayout(true, MarkOnlyThis);
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

void RenderFlexibleBox::layoutAndPlaceChildren(LayoutUnit& crossAxisOffset, const OrderedFlexItemList& children, const WTF::Vector<LayoutUnit>& childSizes, LayoutUnit availableFreeSpace, WTF::Vector<LineContext>& lineContexts)
{
    ASSERT(childSizes.size() == children.size());
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

    lineContexts.append(LineContext(crossAxisOffset, maxChildCrossAxisExtent, children.size(), maxAscent));
    crossAxisOffset += maxChildCrossAxisExtent;
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

static LayoutUnit initialLinePackingOffset(LayoutUnit availableFreeSpace, EFlexLinePack linePack, unsigned numberOfLines)
{
    if (linePack == LinePackEnd)
        return availableFreeSpace;
    if (linePack == LinePackCenter)
        return availableFreeSpace / 2;
    if (linePack == LinePackDistribute) {
        if (availableFreeSpace > 0 && numberOfLines)
            return availableFreeSpace / (2 * numberOfLines);
        if (availableFreeSpace < 0)
            return availableFreeSpace / 2;
    }
    return 0;
}

static LayoutUnit linePackingSpaceBetweenChildren(LayoutUnit availableFreeSpace, EFlexLinePack linePack, unsigned numberOfLines)
{
    if (availableFreeSpace > 0 && numberOfLines > 1) {
        if (linePack == LinePackJustify)
            return availableFreeSpace / (numberOfLines - 1);
        if (linePack == LinePackDistribute || linePack == LinePackStretch)
            return availableFreeSpace / numberOfLines;
    }
    return 0;
}

void RenderFlexibleBox::packFlexLines(FlexOrderIterator& iterator, WTF::Vector<LineContext>& lineContexts)
{
    if (!isMultiline() || style()->flexLinePack() == LinePackStart)
        return;

    LayoutUnit availableCrossAxisSpace = crossAxisContentExtent();
    for (size_t i = 0; i < lineContexts.size(); ++i)
        availableCrossAxisSpace -= lineContexts[i].crossAxisExtent;

    RenderBox* child = iterator.first();
    LayoutUnit lineOffset = initialLinePackingOffset(availableCrossAxisSpace, style()->flexLinePack(), lineContexts.size());
    for (unsigned lineNumber = 0; lineNumber < lineContexts.size(); ++lineNumber) {
        lineContexts[lineNumber].crossAxisOffset += lineOffset;
        for (size_t childNumber = 0; childNumber < lineContexts[lineNumber].numberOfChildren; ++childNumber, child = iterator.next())
            adjustAlignmentForChild(child, lineOffset);

        if (style()->flexLinePack() == LinePackStretch && availableCrossAxisSpace > 0)
            lineContexts[lineNumber].crossAxisExtent += availableCrossAxisSpace / static_cast<unsigned>(lineContexts.size());

        lineOffset += linePackingSpaceBetweenChildren(availableCrossAxisSpace, style()->flexLinePack(), lineContexts.size());
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

void RenderFlexibleBox::alignChildren(FlexOrderIterator& iterator, const WTF::Vector<LineContext>& lineContexts)
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
            if (flexAlignForChild(child) == AlignBaseline && minMarginAfterBaseline)
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
        child->setOverrideWidth(std::max(ZERO_LAYOUT_UNIT, childWidth));
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

void RenderFlexibleBox::flipForWrapReverse(FlexOrderIterator& iterator, const WTF::Vector<LineContext>& lineContexts, LayoutUnit crossAxisStartEdge)
{
    LayoutUnit contentExtent = crossAxisContentExtent();
    RenderBox* child = iterator.first();
    for (size_t lineNumber = 0; lineNumber < lineContexts.size(); ++lineNumber) {
        for (size_t childNumber = 0; childNumber < lineContexts[lineNumber].numberOfChildren; ++childNumber, child = iterator.next()) {
            ASSERT(child);
            LayoutPoint location = flowAwareLocationForChild(child);
            LayoutUnit lineCrossAxisExtent = lineContexts[lineNumber].crossAxisExtent;
            LayoutUnit originalOffset = lineContexts[lineNumber].crossAxisOffset - crossAxisStartEdge;
            LayoutUnit newOffset = contentExtent - originalOffset - lineCrossAxisExtent;
            location.setY(location.y() + newOffset - originalOffset);

            LayoutRect oldRect = child->frameRect();
            setFlowAwareLocationForChild(child, location);
            if (!selfNeedsLayout() && child->checkForRepaintDuringLayout())
                child->repaintDuringLayoutIfMoved(oldRect);
        }
    }
}

}
