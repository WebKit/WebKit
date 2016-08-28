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
#include "RuntimeEnabledFeatures.h"
#include <limits>
#include <wtf/MathExtras.h>

namespace WebCore {

static constexpr ItemPosition selfAlignmentNormalBehaviorFlexibleBox = ItemPositionStretch;

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
    Violation(RenderBox& child, LayoutUnit childSize)
        : child(child)
        , childSize(childSize)
    {
    }

    RenderBox& child;
    LayoutUnit childSize;
};


RenderFlexibleBox::RenderFlexibleBox(Element& element, RenderStyle&& style)
    : RenderBlock(element, WTFMove(style), 0)
    , m_orderIterator(*this)
    , m_numberOfInFlowChildrenOnFirstLine(-1)
{
    setChildrenInline(false); // All of our children must be block-level.
}

RenderFlexibleBox::RenderFlexibleBox(Document& document, RenderStyle&& style)
    : RenderBlock(document, WTFMove(style), 0)
    , m_orderIterator(*this)
    , m_numberOfInFlowChildrenOnFirstLine(-1)
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

void RenderFlexibleBox::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    // FIXME: We're ignoring flex-basis here and we shouldn't. We can't start honoring it though until
    // the flex shorthand stops setting it to 0.
    // See https://bugs.webkit.org/show_bug.cgi?id=116117,
    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (child->isOutOfFlowPositioned())
            continue;

        LayoutUnit margin = marginIntrinsicLogicalWidthForChild(*child);
        bool hasOrthogonalWritingMode = child->isHorizontalWritingMode() != isHorizontalWritingMode();
        LayoutUnit minPreferredLogicalWidth = hasOrthogonalWritingMode ? child->logicalHeight() : child->minPreferredLogicalWidth();
        LayoutUnit maxPreferredLogicalWidth = hasOrthogonalWritingMode ? child->logicalHeight() : child->maxPreferredLogicalWidth();
        minPreferredLogicalWidth += margin;
        maxPreferredLogicalWidth += margin;
        if (!isColumnFlow()) {
            maxLogicalWidth += maxPreferredLogicalWidth;
            if (isMultiline()) {
                // For multiline, the min preferred width is if you put a break between each item.
                minLogicalWidth = std::max(minLogicalWidth, minPreferredLogicalWidth);
            } else
                minLogicalWidth += minPreferredLogicalWidth;
        } else {
            minLogicalWidth = std::max(minPreferredLogicalWidth, minLogicalWidth);
            if (isMultiline()) {
                // For multiline, the max preferred width is if you never break between items.
                maxLogicalWidth += maxPreferredLogicalWidth;
            } else
                maxLogicalWidth = std::max(maxPreferredLogicalWidth, maxLogicalWidth);
        }
    }

    // Due to negative margins, it is possible that we calculated a negative intrinsic width.
    // Make sure that we never return a negative width.
    minLogicalWidth = std::max(LayoutUnit(), minLogicalWidth);
    maxLogicalWidth = std::max(minLogicalWidth, maxLogicalWidth);

    LayoutUnit scrollbarWidth = intrinsicScrollbarLogicalWidth();
    maxLogicalWidth += scrollbarWidth;
    minLogicalWidth += scrollbarWidth;
}

void RenderFlexibleBox::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = 0;

    const RenderStyle& styleToUse = style();
    // FIXME: This should probably be checking for isSpecified since you should be able to use percentage, calc or viewport relative values for width.
    if (styleToUse.logicalWidth().isFixed() && styleToUse.logicalWidth().value() > 0)
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = adjustContentBoxLogicalWidthForBoxSizing(styleToUse.logicalWidth().value());
    else
        computeIntrinsicLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);

    // FIXME: This should probably be checking for isSpecified since you should be able to use percentage, calc or viewport relative values for min-width.
    if (styleToUse.logicalMinWidth().isFixed() && styleToUse.logicalMinWidth().value() > 0) {
        m_maxPreferredLogicalWidth = std::max(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse.logicalMinWidth().value()));
        m_minPreferredLogicalWidth = std::max(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse.logicalMinWidth().value()));
    }

    // FIXME: This should probably be checking for isSpecified since you should be able to use percentage, calc or viewport relative values for maxWidth.
    if (styleToUse.logicalMaxWidth().isFixed()) {
        m_maxPreferredLogicalWidth = std::min(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse.logicalMaxWidth().value()));
        m_minPreferredLogicalWidth = std::min(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse.logicalMaxWidth().value()));
    }

    LayoutUnit borderAndPadding = borderAndPaddingLogicalWidth();
    m_minPreferredLogicalWidth += borderAndPadding;
    m_maxPreferredLogicalWidth += borderAndPadding;

    setPreferredLogicalWidthsDirty(false);
}

static int synthesizedBaselineFromContentBox(const RenderBox& box, LineDirectionMode direction)
{
    return direction == HorizontalLine ? box.borderTop() + box.paddingTop() + box.contentHeight() : box.borderRight() + box.paddingRight() + box.contentWidth();
}

int RenderFlexibleBox::baselinePosition(FontBaseline, bool, LineDirectionMode direction, LinePositionMode) const
{
    int baseline = firstLineBaseline().valueOr(synthesizedBaselineFromContentBox(*this, direction));

    int marginAscent = direction == HorizontalLine ? marginTop() : marginRight();
    return baseline + marginAscent;
}

Optional<int> RenderFlexibleBox::firstLineBaseline() const
{
    if (isWritingModeRoot() || m_numberOfInFlowChildrenOnFirstLine <= 0)
        return Optional<int>();
    RenderBox* baselineChild = nullptr;
    int childNumber = 0;
    for (RenderBox* child = m_orderIterator.first(); child; child = m_orderIterator.next()) {
        if (child->isOutOfFlowPositioned())
            continue;
        if (alignmentForChild(*child) == ItemPositionBaseline && !hasAutoMarginsInCrossAxis(*child)) {
            baselineChild = child;
            break;
        }
        if (!baselineChild)
            baselineChild = child;

        ++childNumber;
        if (childNumber == m_numberOfInFlowChildrenOnFirstLine)
            break;
    }

    if (!baselineChild)
        return Optional<int>();

    if (!isColumnFlow() && hasOrthogonalFlow(*baselineChild))
        return Optional<int>(crossAxisExtentForChild(*baselineChild) + baselineChild->logicalTop());
    if (isColumnFlow() && !hasOrthogonalFlow(*baselineChild))
        return Optional<int>(mainAxisExtentForChild(*baselineChild) + baselineChild->logicalTop());

    Optional<int> baseline = baselineChild->firstLineBaseline();
    if (!baseline) {
        // FIXME: We should pass |direction| into firstLineBoxBaseline and stop bailing out if we're a writing mode root.
        // This would also fix some cases where the flexbox is orthogonal to its container.
        LineDirectionMode direction = isHorizontalWritingMode() ? HorizontalLine : VerticalLine;
        return Optional<int>(synthesizedBaselineFromContentBox(*baselineChild, direction) + baselineChild->logicalTop());
    }

    return Optional<int>(baseline.value() + baselineChild->logicalTop());
}

Optional<int> RenderFlexibleBox::inlineBlockBaseline(LineDirectionMode direction) const
{
    if (Optional<int> baseline = firstLineBaseline())
        return baseline;

    int marginAscent = direction == HorizontalLine ? marginTop() : marginRight();
    return synthesizedBaselineFromContentBox(*this, direction) + marginAscent;
}

void RenderFlexibleBox::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);

    if (oldStyle && oldStyle->resolvedAlignItems(selfAlignmentNormalBehaviorFlexibleBox).position() == ItemPositionStretch && diff == StyleDifferenceLayout) {
        // Flex items that were previously stretching need to be relayed out so we can compute new available cross axis space.
        // This is only necessary for stretching since other alignment values don't change the size of the box.
        auto& newStyle = style();
        for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
            auto& childStyle = child->style();
            auto previousAlignment = childStyle.resolvedAlignSelf(*oldStyle, selfAlignmentNormalBehaviorFlexibleBox).position();
            if (previousAlignment == ItemPositionStretch && previousAlignment != childStyle.resolvedAlignSelf(newStyle, selfAlignmentNormalBehaviorFlexibleBox).position())
                child->setChildNeedsLayout(MarkOnlyThis);
        }
    }
}

void RenderFlexibleBox::layoutBlock(bool relayoutChildren, LayoutUnit)
{
    ASSERT(needsLayout());

    if (!relayoutChildren && simplifiedLayout())
        return;

    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());

    if (recomputeLogicalWidth())
        relayoutChildren = true;

    LayoutUnit previousHeight = logicalHeight();
    setLogicalHeight(borderAndPaddingLogicalHeight() + scrollbarLogicalHeight());

    LayoutStateMaintainer statePusher(view(), *this, locationOffset(), hasTransform() || hasReflection() || style().isFlippedBlocksWritingMode());

    preparePaginationBeforeBlockLayout(relayoutChildren);

    m_numberOfInFlowChildrenOnFirstLine = -1;

    beginUpdateScrollInfoAfterLayoutTransaction();

    dirtyForLayoutFromPercentageHeightDescendants();

    prepareOrderIteratorAndMargins();

    ChildFrameRects oldChildRects;
    appendChildFrameRects(oldChildRects);
    Vector<LineContext> lineContexts;
    layoutFlexItems(relayoutChildren, lineContexts);

    updateLogicalHeight();
    repositionLogicalHeightDependentFlexItems(lineContexts);

    endAndCommitUpdateScrollInfoAfterLayoutTransaction();

    if (logicalHeight() != previousHeight)
        relayoutChildren = true;

    layoutPositionedObjects(relayoutChildren || isDocumentElementRenderer());

    repaintChildrenDuringLayoutIfMoved(oldChildRects);
    // FIXME: css3/flexbox/repaint-rtl-column.html seems to repaint more overflow than it needs to.
    computeOverflow(clientLogicalBottomAfterRepositioning());
    statePusher.pop();

    updateLayerTransform();

    // Update our scroll information if we're overflow:auto/scroll/hidden now that we know if
    // we overflow or not.
    updateScrollInfoAfterLayout();

    repainter.repaintAfterLayout();

    clearNeedsLayout();
}

void RenderFlexibleBox::appendChildFrameRects(ChildFrameRects& childFrameRects)
{
    for (RenderBox* child = m_orderIterator.first(); child; child = m_orderIterator.next()) {
        if (!child->isOutOfFlowPositioned())
            childFrameRects.append(child->frameRect());
    }
}

void RenderFlexibleBox::repaintChildrenDuringLayoutIfMoved(const ChildFrameRects& oldChildRects)
{
    size_t childIndex = 0;
    for (RenderBox* child = m_orderIterator.first(); child; child = m_orderIterator.next()) {
        if (child->isOutOfFlowPositioned())
            continue;

        // If the child moved, we have to repaint it as well as any floating/positioned
        // descendants. An exception is if we need a layout. In this case, we know we're going to
        // repaint ourselves (and the child) anyway.
        if (!selfNeedsLayout() && child->checkForRepaintDuringLayout())
            child->repaintDuringLayoutIfMoved(oldChildRects[childIndex]);
        ++childIndex;
    }
    ASSERT(childIndex == oldChildRects.size());
}

void RenderFlexibleBox::paintChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset, PaintInfo& paintInfoForChild, bool usePrintRect)
{
    for (RenderBox* child = m_orderIterator.first(); child; child = m_orderIterator.next()) {
        if (!paintChild(*child, paintInfo, paintOffset, paintInfoForChild, usePrintRect, PaintAsInlineBlock))
            return;
    }
}

void RenderFlexibleBox::repositionLogicalHeightDependentFlexItems(Vector<LineContext>& lineContexts)
{
    LayoutUnit crossAxisStartEdge = lineContexts.isEmpty() ? LayoutUnit() : lineContexts[0].crossAxisOffset;
    alignFlexLines(lineContexts);

    // If we have a single line flexbox, the line height is all the available space.
    // For flex-direction: row, this means we need to use the height, so we do this after calling updateLogicalHeight.
    if (!isMultiline() && lineContexts.size() == 1)
        lineContexts[0].crossAxisExtent = crossAxisContentExtent();
    alignChildren(lineContexts);

    if (style().flexWrap() == FlexWrapReverse)
        flipForWrapReverse(lineContexts, crossAxisStartEdge);

    // direction:rtl + flex-direction:column means the cross-axis direction is flipped.
    flipForRightToLeftColumn();
}

LayoutUnit RenderFlexibleBox::clientLogicalBottomAfterRepositioning()
{
    LayoutUnit maxChildLogicalBottom = 0;
    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (child->isOutOfFlowPositioned())
            continue;
        LayoutUnit childLogicalBottom = logicalTopForChild(*child) + logicalHeightForChild(*child) + marginAfterForChild(*child);
        maxChildLogicalBottom = std::max(maxChildLogicalBottom, childLogicalBottom);
    }
    return std::max(clientLogicalBottom(), maxChildLogicalBottom);
}

bool RenderFlexibleBox::hasOrthogonalFlow(const RenderBox& child) const
{
    // FIXME: If the child is a flexbox, then we need to check isHorizontalFlow.
    return isHorizontalFlow() != child.isHorizontalWritingMode();
}

bool RenderFlexibleBox::isColumnFlow() const
{
    return style().isColumnFlexDirection();
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
        return style().writingMode() == TopToBottomWritingMode || style().writingMode() == LeftToRightWritingMode;
    return style().isLeftToRightDirection() ^ (style().flexDirection() == FlowRowReverse);
}

bool RenderFlexibleBox::isMultiline() const
{
    return style().flexWrap() != FlexNoWrap;
}

Length RenderFlexibleBox::flexBasisForChild(RenderBox& child) const
{
    Length flexLength = child.style().flexBasis();
    if (flexLength.isAuto())
        flexLength = isHorizontalFlow() ? child.style().width() : child.style().height();
    return flexLength;
}

void RenderFlexibleBox::setCrossAxisExtent(LayoutUnit extent)
{
    if (isHorizontalFlow())
        setHeight(extent);
    else
        setWidth(extent);
}

LayoutUnit RenderFlexibleBox::crossAxisExtentForChild(RenderBox& child) const
{
    return isHorizontalFlow() ? child.height() : child.width();
}

LayoutUnit RenderFlexibleBox::mainAxisExtentForChild(RenderBox& child) const
{
    return isHorizontalFlow() ? child.width() : child.height();
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

LayoutUnit RenderFlexibleBox::mainAxisContentExtent(LayoutUnit contentLogicalHeight)
{
    if (isColumnFlow()) {
        LogicalExtentComputedValues computedValues;
        LayoutUnit borderPaddingAndScrollbar = borderAndPaddingLogicalHeight() + scrollbarLogicalHeight();
        if (contentLogicalHeight > LayoutUnit::max() - borderPaddingAndScrollbar)
            contentLogicalHeight -= borderPaddingAndScrollbar;
        LayoutUnit borderBoxLogicalHeight = contentLogicalHeight + borderPaddingAndScrollbar;
        computeLogicalHeight(borderBoxLogicalHeight, logicalTop(), computedValues);
        if (computedValues.m_extent == LayoutUnit::max())
            return computedValues.m_extent;
        return std::max(LayoutUnit::fromPixel(0), computedValues.m_extent - borderPaddingAndScrollbar);
    }
    return contentLogicalWidth();
}

Optional<LayoutUnit> RenderFlexibleBox::computeMainAxisExtentForChild(const RenderBox& child, SizeType sizeType, const Length& size)
{
    // FIXME: This is wrong for orthogonal flows. It should use the flexbox's writing-mode, not the child's in order
    // to figure out the logical height/width.
    if (isColumnFlow()) {
        // We don't have to check for "auto" here - computeContentLogicalHeight will just return Nullopt for that case anyway.
        if (size.isIntrinsic())
            const_cast<RenderBox&>(child).layoutIfNeeded(); // FIXME: Should not need to do a layout here.
        return child.computeContentLogicalHeight(sizeType, size, child.logicalHeight() - child.borderAndPaddingLogicalHeight());
    }
    // FIXME: Figure out how this should work for regions and pass in the appropriate values.
    RenderRegion* region = nullptr;
    return child.computeLogicalWidthInRegionUsing(sizeType, size, contentLogicalWidth(), *this, region) - child.borderAndPaddingLogicalWidth();
}

WritingMode RenderFlexibleBox::transformedWritingMode() const
{
    WritingMode mode = style().writingMode();
    if (!isColumnFlow())
        return mode;

    switch (mode) {
    case TopToBottomWritingMode:
    case BottomToTopWritingMode:
        return style().isLeftToRightDirection() ? LeftToRightWritingMode : RightToLeftWritingMode;
    case LeftToRightWritingMode:
    case RightToLeftWritingMode:
        return style().isLeftToRightDirection() ? TopToBottomWritingMode : BottomToTopWritingMode;
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

LayoutUnit RenderFlexibleBox::flowAwareMarginStartForChild(RenderBox& child) const
{
    if (isHorizontalFlow())
        return isLeftToRightFlow() ? child.marginLeft() : child.marginRight();
    return isLeftToRightFlow() ? child.marginTop() : child.marginBottom();
}

LayoutUnit RenderFlexibleBox::flowAwareMarginEndForChild(RenderBox& child) const
{
    if (isHorizontalFlow())
        return isLeftToRightFlow() ? child.marginRight() : child.marginLeft();
    return isLeftToRightFlow() ? child.marginBottom() : child.marginTop();
}

LayoutUnit RenderFlexibleBox::flowAwareMarginBeforeForChild(RenderBox& child) const
{
    switch (transformedWritingMode()) {
    case TopToBottomWritingMode:
        return child.marginTop();
    case BottomToTopWritingMode:
        return child.marginBottom();
    case LeftToRightWritingMode:
        return child.marginLeft();
    case RightToLeftWritingMode:
        return child.marginRight();
    }
    ASSERT_NOT_REACHED();
    return marginTop();
}

LayoutUnit RenderFlexibleBox::flowAwareMarginAfterForChild(RenderBox& child) const
{
    switch (transformedWritingMode()) {
    case TopToBottomWritingMode:
        return child.marginBottom();
    case BottomToTopWritingMode:
        return child.marginTop();
    case LeftToRightWritingMode:
        return child.marginRight();
    case RightToLeftWritingMode:
        return child.marginLeft();
    }
    ASSERT_NOT_REACHED();
    return marginBottom();
}

LayoutUnit RenderFlexibleBox::crossAxisMarginExtentForChild(RenderBox& child) const
{
    return isHorizontalFlow() ? child.verticalMarginExtent() : child.horizontalMarginExtent();
}

LayoutUnit RenderFlexibleBox::crossAxisScrollbarExtent() const
{
    return isHorizontalFlow() ? horizontalScrollbarHeight() : verticalScrollbarWidth();
}

LayoutPoint RenderFlexibleBox::flowAwareLocationForChild(RenderBox& child) const
{
    return isHorizontalFlow() ? child.location() : child.location().transposedPoint();
}

void RenderFlexibleBox::setFlowAwareLocationForChild(RenderBox& child, const LayoutPoint& location)
{
    if (isHorizontalFlow())
        child.setLocation(location);
    else
        child.setLocation(location.transposedPoint());
}

LayoutUnit RenderFlexibleBox::mainAxisBorderAndPaddingExtentForChild(RenderBox& child) const
{
    return isHorizontalFlow() ? child.horizontalBorderAndPaddingExtent() : child.verticalBorderAndPaddingExtent();
}
    
bool RenderFlexibleBox::mainAxisLengthIsDefinite(const RenderBox& child, const Length& flexBasis) const
{
    if (flexBasis.isAuto())
        return false;
    if (flexBasis.isPercentOrCalculated())
        return isColumnFlow() ? bool(child.computePercentageLogicalHeight(flexBasis)) : hasDefiniteLogicalWidth();
    return true;
}

LayoutUnit RenderFlexibleBox::mainAxisScrollbarExtentForChild(RenderBox& child) const
{
    return isHorizontalFlow() ? child.verticalScrollbarWidth() : child.horizontalScrollbarHeight();
}

LayoutUnit RenderFlexibleBox::preferredMainAxisContentExtentForChild(RenderBox& child, bool hasInfiniteLineLength)
{
    bool hasOverrideSize = child.hasOverrideLogicalContentWidth() || child.hasOverrideLogicalContentHeight();
    if (hasOverrideSize)
        child.clearOverrideSize();

    Length flexBasis = flexBasisForChild(child);
    if (flexBasis.isAuto() || (flexBasis.isFixed() && !flexBasis.value() && hasInfiniteLineLength)) {
        if (hasOrthogonalFlow(child)) {
            if (hasOverrideSize)
                child.setChildNeedsLayout(MarkOnlyThis);
            child.layoutIfNeeded();
        }
        LayoutUnit mainAxisExtent = hasOrthogonalFlow(child) ? child.logicalHeight() : child.maxPreferredLogicalWidth();
        ASSERT(mainAxisExtent - mainAxisBorderAndPaddingExtentForChild(child) >= 0);
        return mainAxisExtent - mainAxisBorderAndPaddingExtentForChild(child);
    }
    return computeMainAxisExtentForChild(child, MainOrPreferredSize, flexBasis).valueOr(0);
}

void RenderFlexibleBox::layoutFlexItems(bool relayoutChildren, Vector<LineContext>& lineContexts)
{
    OrderedFlexItemList orderedChildren;
    LayoutUnit preferredMainAxisExtent;
    double totalFlexGrow;
    double totalWeightedFlexShrink;
    LayoutUnit minMaxAppliedMainAxisExtent;

    m_orderIterator.first();
    LayoutUnit crossAxisOffset = flowAwareBorderBefore() + flowAwarePaddingBefore();
    bool hasInfiniteLineLength = false;
    while (computeNextFlexLine(orderedChildren, preferredMainAxisExtent, totalFlexGrow, totalWeightedFlexShrink, minMaxAppliedMainAxisExtent, hasInfiniteLineLength)) {
        LayoutUnit availableFreeSpace = mainAxisContentExtent(preferredMainAxisExtent) - preferredMainAxisExtent;
        FlexSign flexSign = (minMaxAppliedMainAxisExtent < preferredMainAxisExtent + availableFreeSpace) ? PositiveFlexibility : NegativeFlexibility;
        InflexibleFlexItemSize inflexibleItems;
        Vector<LayoutUnit> childSizes;
        while (!resolveFlexibleLengths(flexSign, orderedChildren, availableFreeSpace, totalFlexGrow, totalWeightedFlexShrink, inflexibleItems, childSizes, hasInfiniteLineLength)) {
            ASSERT(totalFlexGrow >= 0 && totalWeightedFlexShrink >= 0);
            ASSERT(inflexibleItems.size() > 0);
        }

        layoutAndPlaceChildren(crossAxisOffset, orderedChildren, childSizes, availableFreeSpace, relayoutChildren, lineContexts);
    }
    if (hasLineIfEmpty()) {
        // Even if computeNextFlexLine returns true, the flexbox might not have
        // a line because all our children might be out of flow positioned.
        // Instead of just checking if we have a line, make sure the flexbox
        // has at least a line's worth of height to cover this case.
        LayoutUnit minHeight = borderAndPaddingLogicalHeight()
            + lineHeight(true, isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes)
            + scrollbarLogicalHeight();
        if (height() < minHeight)
            setLogicalHeight(minHeight);
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
            if (child->style().marginLeft().isAuto())
                ++numberOfAutoMargins;
            if (child->style().marginRight().isAuto())
                ++numberOfAutoMargins;
        } else {
            if (child->style().marginTop().isAuto())
                ++numberOfAutoMargins;
            if (child->style().marginBottom().isAuto())
                ++numberOfAutoMargins;
        }
    }
    if (!numberOfAutoMargins)
        return 0;

    LayoutUnit sizeOfAutoMargin = availableFreeSpace / numberOfAutoMargins;
    availableFreeSpace = 0;
    return sizeOfAutoMargin;
}

void RenderFlexibleBox::updateAutoMarginsInMainAxis(RenderBox& child, LayoutUnit autoMarginOffset)
{
    ASSERT(autoMarginOffset >= 0);

    if (isHorizontalFlow()) {
        if (child.style().marginLeft().isAuto())
            child.setMarginLeft(autoMarginOffset);
        if (child.style().marginRight().isAuto())
            child.setMarginRight(autoMarginOffset);
    } else {
        if (child.style().marginTop().isAuto())
            child.setMarginTop(autoMarginOffset);
        if (child.style().marginBottom().isAuto())
            child.setMarginBottom(autoMarginOffset);
    }
}

bool RenderFlexibleBox::hasAutoMarginsInCrossAxis(RenderBox& child) const
{
    if (isHorizontalFlow())
        return child.style().marginTop().isAuto() || child.style().marginBottom().isAuto();
    return child.style().marginLeft().isAuto() || child.style().marginRight().isAuto();
}

LayoutUnit RenderFlexibleBox::availableAlignmentSpaceForChild(LayoutUnit lineCrossAxisExtent, RenderBox& child)
{
    ASSERT(!child.isOutOfFlowPositioned());
    LayoutUnit childCrossExtent = crossAxisMarginExtentForChild(child) + crossAxisExtentForChild(child);
    return lineCrossAxisExtent - childCrossExtent;
}

bool RenderFlexibleBox::updateAutoMarginsInCrossAxis(RenderBox& child, LayoutUnit availableAlignmentSpace)
{
    ASSERT(!child.isOutOfFlowPositioned());
    ASSERT(availableAlignmentSpace >= 0);

    bool isHorizontal = isHorizontalFlow();
    Length start = isHorizontal ? child.style().marginTop() : child.style().marginLeft();
    Length end = isHorizontal ? child.style().marginBottom() : child.style().marginRight();
    if (start.isAuto() && end.isAuto()) {
        adjustAlignmentForChild(child, availableAlignmentSpace / 2);
        if (isHorizontal) {
            child.setMarginTop(availableAlignmentSpace / 2);
            child.setMarginBottom(availableAlignmentSpace / 2);
        } else {
            child.setMarginLeft(availableAlignmentSpace / 2);
            child.setMarginRight(availableAlignmentSpace / 2);
        }
        return true;
    }
    if (start.isAuto()) {
        adjustAlignmentForChild(child, availableAlignmentSpace);
        if (isHorizontal)
            child.setMarginTop(availableAlignmentSpace);
        else
            child.setMarginLeft(availableAlignmentSpace);
        return true;
    }
    if (end.isAuto()) {
        if (isHorizontal)
            child.setMarginBottom(availableAlignmentSpace);
        else
            child.setMarginRight(availableAlignmentSpace);
        return true;
    }
    return false;
}

LayoutUnit RenderFlexibleBox::marginBoxAscentForChild(RenderBox& child)
{
    LayoutUnit ascent = child.firstLineBaseline().valueOr(crossAxisExtentForChild(child));
    return ascent + flowAwareMarginBeforeForChild(child);
}

LayoutUnit RenderFlexibleBox::computeChildMarginValue(const Length& margin)
{
    // When resolving the margins, we use the content size for resolving percent and calc (for percents in calc expressions) margins.
    // Fortunately, percent margins are always computed with respect to the block's width, even for margin-top and margin-bottom.
    LayoutUnit availableSize = contentLogicalWidth();
    return minimumValueForLength(margin, availableSize);
}

void RenderFlexibleBox::prepareOrderIteratorAndMargins()
{
    OrderIteratorPopulator populator(m_orderIterator);

    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        populator.collectChild(*child);

        if (child->isOutOfFlowPositioned())
            continue;

        // Before running the flex algorithm, 'auto' has a margin of 0.
        // Also, if we're not auto sizing, we don't do a layout that computes the start/end margins.
        if (isHorizontalFlow()) {
            child->setMarginLeft(computeChildMarginValue(child->style().marginLeft()));
            child->setMarginRight(computeChildMarginValue(child->style().marginRight()));
        } else {
            child->setMarginTop(computeChildMarginValue(child->style().marginTop()));
            child->setMarginBottom(computeChildMarginValue(child->style().marginBottom()));
        }
    }
}

bool RenderFlexibleBox::crossAxisLengthIsDefinite(const RenderBox& child, const Length& length) const
{
    if (length.isAuto())
        return false;
    if (length.isPercentOrCalculated())
        return hasOrthogonalFlow(child) ? hasDefiniteLogicalWidth() : bool(child.computePercentageLogicalHeight(length));
    return length.isFixed();
}

    
Optional<LayoutUnit> RenderFlexibleBox::computeMainSizeFromAspectRatioUsing(const RenderBox& child, Length crossSizeLength) const
{
    ASSERT(child.hasAspectRatio());
    ASSERT(child.intrinsicSize().height() > 0);
    
    Optional<LayoutUnit> crossSize;
    if (crossSizeLength.isFixed())
        crossSize = LayoutUnit(crossSizeLength.value());
    else {
        ASSERT(crossSizeLength.isPercentOrCalculated());
        crossSize = hasOrthogonalFlow(child) ?
        adjustBorderBoxLogicalWidthForBoxSizing(valueForLength(crossSizeLength, contentWidth())) :
        child.computePercentageLogicalHeight(crossSizeLength);
    }
    
    if (!crossSize)
        return crossSize;

    const LayoutSize& childIntrinsicSize = child.intrinsicSize();
    double ratio = childIntrinsicSize.width().toFloat() / childIntrinsicSize.height().toFloat();
    if (isHorizontalFlow())
        return LayoutUnit(crossSize.value() * ratio);
    return LayoutUnit(crossSize.value() / ratio);
}

LayoutUnit RenderFlexibleBox::adjustChildSizeForAspectRatioCrossAxisMinAndMax(const RenderBox& child, LayoutUnit childSize)
{
    Length crossMin = isHorizontalFlow() ? child.style().minHeight() : child.style().minWidth();
    Length crossMax = isHorizontalFlow() ? child.style().maxHeight() : child.style().maxWidth();

    if (crossAxisLengthIsDefinite(child, crossMax)) {
        Optional<LayoutUnit> maxValue = computeMainSizeFromAspectRatioUsing(child, crossMax);
        if (maxValue)
            childSize = std::min(maxValue.value(), childSize);
    }
    
    if (crossAxisLengthIsDefinite(child, crossMin)) {
        Optional<LayoutUnit> minValue = computeMainSizeFromAspectRatioUsing(child, crossMin);
        if (minValue)
            childSize = std::max(minValue.value(), childSize);
    }
    
    return childSize;
}

bool RenderFlexibleBox::useChildAspectRatio(const RenderBox& child) const
{
    if (!child.hasAspectRatio())
        return false;
    if (!child.intrinsicSize().height())
        return false;
    Length crossSize = isHorizontalFlow() ? child.style().height() : child.style().width();
    return crossAxisLengthIsDefinite(child, crossSize);
}

LayoutUnit RenderFlexibleBox::adjustChildSizeForMinAndMax(const RenderBox& child, LayoutUnit childSize)
{
    Length max = isHorizontalFlow() ? child.style().maxWidth() : child.style().maxHeight();
    Optional<LayoutUnit> maxExtent = Nullopt;
    if (max.isSpecifiedOrIntrinsic()) {
        maxExtent = computeMainAxisExtentForChild(child, MaxSize, max);
        childSize = std::min(childSize, maxExtent.valueOr(childSize));
    }
    
    Length min = isHorizontalFlow() ? child.style().minWidth() : child.style().minHeight();
    if (min.isSpecifiedOrIntrinsic())
        return std::max(childSize, computeMainAxisExtentForChild(child, MinSize, min).valueOr(childSize));
    
    if (!isFlexibleBoxImpl() && min.isAuto() && mainAxisOverflowForChild(child) == OVISIBLE && !(isColumnFlow() && is<RenderFlexibleBox>(child))) {
        // This is the implementation of CSS flexbox section 4.5 which defines the minimum size of "pure" flex
        // items. For any other item the value should be 0, this also includes RenderFlexibleBox's derived clases
        // (RenderButton, RenderFullScreen...) because that's just an implementation detail.
        // FIXME: For now we don't handle nested column flexboxes. Need to implement better intrinsic
        // size handling from the flex box spec first (4.5).
        LayoutUnit contentSize = computeMainAxisExtentForChild(child, MinSize, Length(MinContent)).value();
        ASSERT(contentSize >= 0);
        if (child.hasAspectRatio() && child.intrinsicSize().height() > 0)
            contentSize = adjustChildSizeForAspectRatioCrossAxisMinAndMax(child, contentSize);
        contentSize = std::min(contentSize, maxExtent.valueOr(contentSize));
        
        Length mainSize = isHorizontalFlow() ? child.style().width() : child.style().height();
        if (mainAxisLengthIsDefinite(child, mainSize)) {
            LayoutUnit resolvedMainSize = computeMainAxisExtentForChild(child, MainOrPreferredSize, mainSize).value();
            ASSERT(resolvedMainSize >= 0);
            LayoutUnit specifiedSize = std::min(resolvedMainSize, maxExtent.valueOr(resolvedMainSize));
            return std::max(childSize, std::min(specifiedSize, contentSize));
        } else if (useChildAspectRatio(child)) {
            Length crossSizeLength = isHorizontalFlow() ? child.style().height() : child.style().width();
            Optional<LayoutUnit> transferredSize = computeMainSizeFromAspectRatioUsing(child, crossSizeLength);
            if (transferredSize) {
                transferredSize = adjustChildSizeForAspectRatioCrossAxisMinAndMax(child, transferredSize.value());
                return std::max(childSize, std::min(transferredSize.value(), contentSize));
            }
        }
        return std::max(childSize, contentSize);
    }
    return childSize;
}

bool RenderFlexibleBox::computeNextFlexLine(OrderedFlexItemList& orderedChildren, LayoutUnit& preferredMainAxisExtent, double& totalFlexGrow, double& totalWeightedFlexShrink, LayoutUnit& minMaxAppliedMainAxisExtent, bool& hasInfiniteLineLength)
{
    orderedChildren.clear();
    preferredMainAxisExtent = 0;
    totalFlexGrow = totalWeightedFlexShrink = 0;
    minMaxAppliedMainAxisExtent = 0;

    if (!m_orderIterator.currentChild())
        return false;

    LayoutUnit lineBreakLength = mainAxisContentExtent(LayoutUnit::max());
    hasInfiniteLineLength = lineBreakLength == LayoutUnit::max();

    bool lineHasInFlowItem = false;

    for (RenderBox* child = m_orderIterator.currentChild(); child; child = m_orderIterator.next()) {
        if (child->isOutOfFlowPositioned()) {
            orderedChildren.append(child);
            continue;
        }

        LayoutUnit childMainAxisExtent = preferredMainAxisContentExtentForChild(*child, hasInfiniteLineLength);
        LayoutUnit childMainAxisMarginBoxExtent = mainAxisBorderAndPaddingExtentForChild(*child) + childMainAxisExtent;
        childMainAxisMarginBoxExtent += isHorizontalFlow() ? child->horizontalMarginExtent() : child->verticalMarginExtent();

        if (isMultiline() && preferredMainAxisExtent + childMainAxisMarginBoxExtent > lineBreakLength && lineHasInFlowItem)
            break;
        orderedChildren.append(child);
        lineHasInFlowItem  = true;
        preferredMainAxisExtent += childMainAxisMarginBoxExtent;
        totalFlexGrow += child->style().flexGrow();
        totalWeightedFlexShrink += child->style().flexShrink() * childMainAxisExtent;

        LayoutUnit childMinMaxAppliedMainAxisExtent = adjustChildSizeForMinAndMax(*child, childMainAxisExtent);
        minMaxAppliedMainAxisExtent += childMinMaxAppliedMainAxisExtent - childMainAxisExtent + childMainAxisMarginBoxExtent;
    }
    return true;
}

void RenderFlexibleBox::freezeViolations(const Vector<Violation>& violations, LayoutUnit& availableFreeSpace, double& totalFlexGrow, double& totalWeightedFlexShrink, InflexibleFlexItemSize& inflexibleItems, bool hasInfiniteLineLength)
{
    for (size_t i = 0; i < violations.size(); ++i) {
        RenderBox& child = violations[i].child;
        LayoutUnit childSize = violations[i].childSize;
        LayoutUnit preferredChildSize = preferredMainAxisContentExtentForChild(child, hasInfiniteLineLength);
        availableFreeSpace -= childSize - preferredChildSize;
        totalFlexGrow -= child.style().flexGrow();
        totalWeightedFlexShrink -= child.style().flexShrink() * preferredChildSize;
        inflexibleItems.set(&child, childSize);
    }
}

// Returns true if we successfully ran the algorithm and sized the flex items.
bool RenderFlexibleBox::resolveFlexibleLengths(FlexSign flexSign, const OrderedFlexItemList& children, LayoutUnit& availableFreeSpace, double& totalFlexGrow, double& totalWeightedFlexShrink, InflexibleFlexItemSize& inflexibleItems, Vector<LayoutUnit>& childSizes, bool hasInfiniteLineLength)
{
    childSizes.clear();
    LayoutUnit totalViolation = 0;
    LayoutUnit usedFreeSpace = 0;
    Vector<Violation> minViolations;
    Vector<Violation> maxViolations;
    for (size_t i = 0; i < children.size(); ++i) {
        RenderBox& child = *children[i];
        if (child.isOutOfFlowPositioned()) {
            childSizes.append(0);
            continue;
        }

        if (inflexibleItems.contains(&child))
            childSizes.append(inflexibleItems.get(&child));
        else {
            LayoutUnit preferredChildSize = preferredMainAxisContentExtentForChild(child, hasInfiniteLineLength);
            LayoutUnit childSize = preferredChildSize;
            double extraSpace = 0;
            if (availableFreeSpace > 0 && totalFlexGrow > 0 && flexSign == PositiveFlexibility && std::isfinite(totalFlexGrow))
                extraSpace = availableFreeSpace * child.style().flexGrow() / totalFlexGrow;
            else if (availableFreeSpace < 0 && totalWeightedFlexShrink > 0 && flexSign == NegativeFlexibility && std::isfinite(totalWeightedFlexShrink))
                extraSpace = availableFreeSpace * child.style().flexShrink() * preferredChildSize / totalWeightedFlexShrink;
            if (std::isfinite(extraSpace))
                childSize += LayoutUnit::fromFloatRound(extraSpace);

            LayoutUnit adjustedChildSize = adjustChildSizeForMinAndMax(child, childSize);
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
        freezeViolations(totalViolation < 0 ? maxViolations : minViolations, availableFreeSpace, totalFlexGrow, totalWeightedFlexShrink, inflexibleItems, hasInfiniteLineLength);
    else
        availableFreeSpace -= usedFreeSpace;

    return !totalViolation;
}

static LayoutUnit initialJustifyContentOffset(LayoutUnit availableFreeSpace, ContentPosition justifyContent, ContentDistributionType justifyContentDistribution, unsigned numberOfChildren)
{
    if (justifyContent == ContentPositionFlexEnd)
        return availableFreeSpace;
    if (justifyContent == ContentPositionCenter)
        return availableFreeSpace / 2;
    if (justifyContentDistribution == ContentDistributionSpaceAround) {
        if (availableFreeSpace > 0 && numberOfChildren)
            return availableFreeSpace / (2 * numberOfChildren);
        else
            return availableFreeSpace / 2;
    }
    return 0;
}

static LayoutUnit justifyContentSpaceBetweenChildren(LayoutUnit availableFreeSpace, ContentDistributionType justifyContentDistribution, unsigned numberOfChildren)
{
    if (availableFreeSpace > 0 && numberOfChildren > 1) {
        if (justifyContentDistribution == ContentDistributionSpaceBetween)
            return availableFreeSpace / (numberOfChildren - 1);
        if (justifyContentDistribution == ContentDistributionSpaceAround)
            return availableFreeSpace / numberOfChildren;
    }
    return 0;
}

void RenderFlexibleBox::setLogicalOverrideSize(RenderBox& child, LayoutUnit childPreferredSize)
{
    if (hasOrthogonalFlow(child))
        child.setOverrideLogicalContentHeight(childPreferredSize - child.borderAndPaddingLogicalHeight());
    else
        child.setOverrideLogicalContentWidth(childPreferredSize - child.borderAndPaddingLogicalWidth());
}

void RenderFlexibleBox::prepareChildForPositionedLayout(RenderBox& child, LayoutUnit mainAxisOffset, LayoutUnit crossAxisOffset, PositionedLayoutMode layoutMode)
{
    ASSERT(child.isOutOfFlowPositioned());
    child.containingBlock()->insertPositionedObject(child);
    RenderLayer* childLayer = child.layer();
    LayoutUnit inlinePosition = isColumnFlow() ? crossAxisOffset : mainAxisOffset;
    if (layoutMode == FlipForRowReverse && style().flexDirection() == FlowRowReverse)
        inlinePosition = mainAxisExtent() - mainAxisOffset;
    childLayer->setStaticInlinePosition(inlinePosition); // FIXME: Not right for regions.

    LayoutUnit staticBlockPosition = isColumnFlow() ? mainAxisOffset : crossAxisOffset;
    if (childLayer->staticBlockPosition() != staticBlockPosition) {
        childLayer->setStaticBlockPosition(staticBlockPosition);
        if (child.style().hasStaticBlockPosition(style().isHorizontalWritingMode()))
            child.setChildNeedsLayout(MarkOnlyThis);
    }
}

ItemPosition RenderFlexibleBox::alignmentForChild(RenderBox& child) const
{
    ItemPosition align = child.style().resolvedAlignSelf(style(), selfAlignmentNormalBehaviorFlexibleBox).position();

    if (align == ItemPositionBaseline && hasOrthogonalFlow(child))
        align = ItemPositionFlexStart;

    if (style().flexWrap() == FlexWrapReverse) {
        if (align == ItemPositionFlexStart)
            align = ItemPositionFlexEnd;
        else if (align == ItemPositionFlexEnd)
            align = ItemPositionFlexStart;
    }

    return align;
}

size_t RenderFlexibleBox::numberOfInFlowPositionedChildren(const OrderedFlexItemList& children) const
{
    size_t count = 0;
    for (size_t i = 0; i < children.size(); ++i) {
        RenderBox* child = children[i];
        if (!child->isOutOfFlowPositioned())
            ++count;
    }
    return count;
}

bool RenderFlexibleBox::needToStretchChild(RenderBox& child)
{
    if (alignmentForChild(child) != ItemPositionStretch)
        return false;

    Length crossAxisLength = isHorizontalFlow() ? child.style().height() : child.style().width();
    return crossAxisLength.isAuto();
}

void RenderFlexibleBox::resetAutoMarginsAndLogicalTopInCrossAxis(RenderBox& child)
{
    if (hasAutoMarginsInCrossAxis(child))
        child.updateLogicalHeight();
}

EOverflow RenderFlexibleBox::mainAxisOverflowForChild(const RenderBox& child) const
{
    if (isHorizontalFlow())
        return child.style().overflowX();
    return child.style().overflowY();
}

static const StyleContentAlignmentData& contentAlignmentNormalBehaviorFlexibleBox()
{
    // The justify-content property applies along the main axis, but since flexing
    // in the main axis is controlled by flex, stretch behaves as flex-start (ignoring
    // the specified fallback alignment, if any).
    // https://drafts.csswg.org/css-align/#distribution-flex
    static const StyleContentAlignmentData normalBehavior = {ContentPositionNormal, ContentDistributionStretch};
    return normalBehavior;
}

void RenderFlexibleBox::layoutAndPlaceChildren(LayoutUnit& crossAxisOffset, const OrderedFlexItemList& children, const Vector<LayoutUnit>& childSizes, LayoutUnit availableFreeSpace, bool relayoutChildren, Vector<LineContext>& lineContexts)
{
    ASSERT(childSizes.size() == children.size());

    auto position = style().resolvedJustifyContentPosition(contentAlignmentNormalBehaviorFlexibleBox());
    auto distribution = style().resolvedJustifyContentDistribution(contentAlignmentNormalBehaviorFlexibleBox());

    size_t numberOfChildrenForJustifyContent = numberOfInFlowPositionedChildren(children);
    LayoutUnit autoMarginOffset = autoMarginOffsetInMainAxis(children, availableFreeSpace);
    LayoutUnit mainAxisOffset = flowAwareBorderStart() + flowAwarePaddingStart();
    mainAxisOffset += initialJustifyContentOffset(availableFreeSpace, position, distribution, numberOfChildrenForJustifyContent);
    if (style().flexDirection() == FlowRowReverse)
        mainAxisOffset += isHorizontalFlow() ? verticalScrollbarWidth() : horizontalScrollbarHeight();

    LayoutUnit totalMainExtent = mainAxisExtent();
    LayoutUnit maxAscent = 0, maxDescent = 0; // Used when align-items: baseline.
    LayoutUnit maxChildCrossAxisExtent = 0;
    size_t seenInFlowPositionedChildren = 0;
    bool shouldFlipMainAxis = !isColumnFlow() && !isLeftToRightFlow();
    for (size_t i = 0; i < children.size(); ++i) {
        RenderBox& child = *children[i];
        if (child.isOutOfFlowPositioned()) {
            prepareChildForPositionedLayout(child, mainAxisOffset, crossAxisOffset, FlipForRowReverse);
            continue;
        }

        LayoutUnit childPreferredSize = childSizes[i] + mainAxisBorderAndPaddingExtentForChild(child);
        setLogicalOverrideSize(child, childPreferredSize);
        // FIXME: Can avoid laying out here in some cases. See https://webkit.org/b/87905.
        if (needToStretchChild(child) || childPreferredSize != mainAxisExtentForChild(child))
            child.setChildNeedsLayout(MarkOnlyThis);
        else {
            // To avoid double applying margin changes in updateAutoMarginsInCrossAxis, we reset the margins here.
            resetAutoMarginsAndLogicalTopInCrossAxis(child);
        }
        updateBlockChildDirtyBitsBeforeLayout(relayoutChildren, child);
        child.layoutIfNeeded();

        updateAutoMarginsInMainAxis(child, autoMarginOffset);

        LayoutUnit childCrossAxisMarginBoxExtent;
        if (alignmentForChild(child) == ItemPositionBaseline && !hasAutoMarginsInCrossAxis(child)) {
            LayoutUnit ascent = marginBoxAscentForChild(child);
            LayoutUnit descent = (crossAxisMarginExtentForChild(child) + crossAxisExtentForChild(child)) - ascent;

            maxAscent = std::max(maxAscent, ascent);
            maxDescent = std::max(maxDescent, descent);

            childCrossAxisMarginBoxExtent = maxAscent + maxDescent;
        } else
            childCrossAxisMarginBoxExtent = crossAxisExtentForChild(child) + crossAxisMarginExtentForChild(child);
        if (!isColumnFlow())
            setLogicalHeight(std::max(logicalHeight(), crossAxisOffset + flowAwareBorderAfter() + flowAwarePaddingAfter() + childCrossAxisMarginBoxExtent + crossAxisScrollbarExtent()));
        maxChildCrossAxisExtent = std::max(maxChildCrossAxisExtent, childCrossAxisMarginBoxExtent);

        mainAxisOffset += flowAwareMarginStartForChild(child);

        LayoutUnit childMainExtent = mainAxisExtentForChild(child);
        LayoutPoint childLocation(shouldFlipMainAxis ? totalMainExtent - mainAxisOffset - childMainExtent : mainAxisOffset,
            crossAxisOffset + flowAwareMarginBeforeForChild(child));

        // FIXME: Supporting layout deltas.
        setFlowAwareLocationForChild(child, childLocation);
        mainAxisOffset += childMainExtent + flowAwareMarginEndForChild(child);

        ++seenInFlowPositionedChildren;
        if (seenInFlowPositionedChildren < numberOfChildrenForJustifyContent)
            mainAxisOffset += justifyContentSpaceBetweenChildren(availableFreeSpace, distribution, numberOfChildrenForJustifyContent);
    }

    if (isColumnFlow())
        setLogicalHeight(mainAxisOffset + flowAwareBorderEnd() + flowAwarePaddingEnd() + scrollbarLogicalHeight());

    if (style().flexDirection() == FlowColumnReverse) {
        // We have to do an extra pass for column-reverse to reposition the flex items since the start depends
        // on the height of the flexbox, which we only know after we've positioned all the flex items.
        updateLogicalHeight();
        layoutColumnReverse(children, crossAxisOffset, availableFreeSpace);
    }

    if (m_numberOfInFlowChildrenOnFirstLine == -1)
        m_numberOfInFlowChildrenOnFirstLine = seenInFlowPositionedChildren;
    lineContexts.append(LineContext(crossAxisOffset, maxChildCrossAxisExtent, children.size(), maxAscent));
    crossAxisOffset += maxChildCrossAxisExtent;
}

void RenderFlexibleBox::layoutColumnReverse(const OrderedFlexItemList& children, LayoutUnit crossAxisOffset, LayoutUnit availableFreeSpace)
{
    auto position = style().resolvedJustifyContentPosition(contentAlignmentNormalBehaviorFlexibleBox());
    auto distribution = style().resolvedJustifyContentDistribution(contentAlignmentNormalBehaviorFlexibleBox());

    // This is similar to the logic in layoutAndPlaceChildren, except we place the children
    // starting from the end of the flexbox. We also don't need to layout anything since we're
    // just moving the children to a new position.
    size_t numberOfChildrenForJustifyContent = numberOfInFlowPositionedChildren(children);
    LayoutUnit mainAxisOffset = logicalHeight() - flowAwareBorderEnd() - flowAwarePaddingEnd();
    mainAxisOffset -= initialJustifyContentOffset(availableFreeSpace, position, distribution, numberOfChildrenForJustifyContent);
    mainAxisOffset -= isHorizontalFlow() ? verticalScrollbarWidth() : horizontalScrollbarHeight();

    size_t seenInFlowPositionedChildren = 0;
    for (size_t i = 0; i < children.size(); ++i) {
        RenderBox& child = *children[i];
        if (child.isOutOfFlowPositioned()) {
            child.layer()->setStaticBlockPosition(mainAxisOffset);
            continue;
        }
        mainAxisOffset -= mainAxisExtentForChild(child) + flowAwareMarginEndForChild(child);

        setFlowAwareLocationForChild(child, LayoutPoint(mainAxisOffset, crossAxisOffset + flowAwareMarginBeforeForChild(child)));

        mainAxisOffset -= flowAwareMarginStartForChild(child);

        ++seenInFlowPositionedChildren;
        if (seenInFlowPositionedChildren < numberOfChildrenForJustifyContent)
            mainAxisOffset -= justifyContentSpaceBetweenChildren(availableFreeSpace, distribution, numberOfChildrenForJustifyContent);
    }
}

static LayoutUnit initialAlignContentOffset(LayoutUnit availableFreeSpace, ContentPosition alignContent, ContentDistributionType alignContentDistribution, unsigned numberOfLines)
{
    if (alignContent == ContentPositionFlexEnd)
        return availableFreeSpace;
    if (alignContent == ContentPositionCenter)
        return availableFreeSpace / 2;
    if (alignContentDistribution == ContentDistributionSpaceAround) {
        if (availableFreeSpace > 0 && numberOfLines)
            return availableFreeSpace / (2 * numberOfLines);
        if (availableFreeSpace < 0)
            return availableFreeSpace / 2;
    }
    return 0;
}

static LayoutUnit alignContentSpaceBetweenChildren(LayoutUnit availableFreeSpace, ContentDistributionType alignContentDistribution, unsigned numberOfLines)
{
    if (availableFreeSpace > 0 && numberOfLines > 1) {
        if (alignContentDistribution == ContentDistributionSpaceBetween)
            return availableFreeSpace / (numberOfLines - 1);
        if (alignContentDistribution == ContentDistributionSpaceAround || alignContentDistribution == ContentDistributionStretch)
            return availableFreeSpace / numberOfLines;
    }
    return 0;
}

void RenderFlexibleBox::alignFlexLines(Vector<LineContext>& lineContexts)
{
    ContentPosition position = style().resolvedAlignContentPosition(contentAlignmentNormalBehaviorFlexibleBox());
    ContentDistributionType distribution = style().resolvedAlignContentDistribution(contentAlignmentNormalBehaviorFlexibleBox());

    if (!isMultiline() || position == ContentPositionFlexStart)
        return;

    LayoutUnit availableCrossAxisSpace = crossAxisContentExtent();
    for (size_t i = 0; i < lineContexts.size(); ++i)
        availableCrossAxisSpace -= lineContexts[i].crossAxisExtent;

    RenderBox* child = m_orderIterator.first();
    LayoutUnit lineOffset = initialAlignContentOffset(availableCrossAxisSpace, position, distribution, lineContexts.size());
    for (unsigned lineNumber = 0; lineNumber < lineContexts.size(); ++lineNumber) {
        lineContexts[lineNumber].crossAxisOffset += lineOffset;
        for (size_t childNumber = 0; childNumber < lineContexts[lineNumber].numberOfChildren; ++childNumber, child = m_orderIterator.next())
            adjustAlignmentForChild(*child, lineOffset);

        if (distribution == ContentDistributionStretch && availableCrossAxisSpace > 0)
            lineContexts[lineNumber].crossAxisExtent += availableCrossAxisSpace / static_cast<unsigned>(lineContexts.size());

        lineOffset += alignContentSpaceBetweenChildren(availableCrossAxisSpace, distribution, lineContexts.size());
    }
}

void RenderFlexibleBox::adjustAlignmentForChild(RenderBox& child, LayoutUnit delta)
{
    if (child.isOutOfFlowPositioned()) {
        LayoutUnit staticInlinePosition = child.layer()->staticInlinePosition();
        LayoutUnit staticBlockPosition = child.layer()->staticBlockPosition();
        LayoutUnit mainAxis = isColumnFlow() ? staticBlockPosition : staticInlinePosition;
        LayoutUnit crossAxis = isColumnFlow() ? staticInlinePosition : staticBlockPosition;
        crossAxis += delta;
        prepareChildForPositionedLayout(child, mainAxis, crossAxis, NoFlipForRowReverse);
        return;
    }

    setFlowAwareLocationForChild(child, flowAwareLocationForChild(child) + LayoutSize(0, delta));
}

void RenderFlexibleBox::alignChildren(const Vector<LineContext>& lineContexts)
{
    // Keep track of the space between the baseline edge and the after edge of the box for each line.
    Vector<LayoutUnit> minMarginAfterBaselines;

    RenderBox* child = m_orderIterator.first();
    for (size_t lineNumber = 0; lineNumber < lineContexts.size(); ++lineNumber) {
        LayoutUnit minMarginAfterBaseline = LayoutUnit::max();
        LayoutUnit lineCrossAxisExtent = lineContexts[lineNumber].crossAxisExtent;
        LayoutUnit maxAscent = lineContexts[lineNumber].maxAscent;

        for (size_t childNumber = 0; childNumber < lineContexts[lineNumber].numberOfChildren; ++childNumber, child = m_orderIterator.next()) {
            ASSERT(child);
            if (child->isOutOfFlowPositioned()) {
                if (style().flexWrap() == FlexWrapReverse)
                    adjustAlignmentForChild(*child, lineCrossAxisExtent);
                continue;
            }

            if (updateAutoMarginsInCrossAxis(*child, std::max(LayoutUnit::fromPixel(0), availableAlignmentSpaceForChild(lineCrossAxisExtent, *child))))
                continue;

            switch (alignmentForChild(*child)) {
            case ItemPositionAuto:
            case ItemPositionNormal:
                ASSERT_NOT_REACHED();
                break;
            case ItemPositionStart:
                // FIXME: https://webkit.org/b/135460 - The extended grammar is not supported
                // yet for FlexibleBox.
                // Defaulting to Stretch for now, as it what most of FlexBox based renders
                // expect as default.
                ASSERT(RuntimeEnabledFeatures::sharedFeatures().isCSSGridLayoutEnabled());
                FALLTHROUGH;
            case ItemPositionStretch: {
                applyStretchAlignmentToChild(*child, lineCrossAxisExtent);
                // Since wrap-reverse flips cross start and cross end, strech children should be aligned with the cross end.
                if (style().flexWrap() == FlexWrapReverse)
                    adjustAlignmentForChild(*child, availableAlignmentSpaceForChild(lineCrossAxisExtent, *child));
                break;
            }
            case ItemPositionFlexStart:
                break;
            case ItemPositionFlexEnd:
                adjustAlignmentForChild(*child, availableAlignmentSpaceForChild(lineCrossAxisExtent, *child));
                break;
            case ItemPositionCenter:
                adjustAlignmentForChild(*child, availableAlignmentSpaceForChild(lineCrossAxisExtent, *child) / 2);
                break;
            case ItemPositionBaseline: {
                // FIXME: If we get here in columns, we want the use the descent, except we currently can't get the ascent/descent of orthogonal children.
                // https://bugs.webkit.org/show_bug.cgi?id=98076
                LayoutUnit ascent = marginBoxAscentForChild(*child);
                LayoutUnit startOffset = maxAscent - ascent;
                adjustAlignmentForChild(*child, startOffset);

                if (style().flexWrap() == FlexWrapReverse)
                    minMarginAfterBaseline = std::min(minMarginAfterBaseline, availableAlignmentSpaceForChild(lineCrossAxisExtent, *child) - startOffset);
                break;
            }
            case ItemPositionLastBaseline:
            case ItemPositionSelfStart:
            case ItemPositionSelfEnd:
            case ItemPositionEnd:
            case ItemPositionLeft:
            case ItemPositionRight:
                // FIXME: https://webkit.org/b/135460 - The extended grammar is not supported
                // yet for FlexibleBox.
                ASSERT(RuntimeEnabledFeatures::sharedFeatures().isCSSGridLayoutEnabled());
                break;
            default:
                ASSERT_NOT_REACHED();
                break;
            }
        }
        minMarginAfterBaselines.append(minMarginAfterBaseline);
    }

    if (style().flexWrap() != FlexWrapReverse)
        return;

    // wrap-reverse flips the cross axis start and end. For baseline alignment, this means we
    // need to align the after edge of baseline elements with the after edge of the flex line.
    child = m_orderIterator.first();
    for (size_t lineNumber = 0; lineNumber < lineContexts.size(); ++lineNumber) {
        LayoutUnit minMarginAfterBaseline = minMarginAfterBaselines[lineNumber];
        for (size_t childNumber = 0; childNumber < lineContexts[lineNumber].numberOfChildren; ++childNumber, child = m_orderIterator.next()) {
            ASSERT(child);
            if (alignmentForChild(*child) == ItemPositionBaseline && !hasAutoMarginsInCrossAxis(*child) && minMarginAfterBaseline)
                adjustAlignmentForChild(*child, minMarginAfterBaseline);
        }
    }
}

void RenderFlexibleBox::applyStretchAlignmentToChild(RenderBox& child, LayoutUnit lineCrossAxisExtent)
{
    if (!isColumnFlow() && child.style().logicalHeight().isAuto()) {
        // FIXME: If the child has orthogonal flow, then it already has an override height set, so use it.
        if (!hasOrthogonalFlow(child)) {
            LayoutUnit stretchedLogicalHeight = child.logicalHeight() + availableAlignmentSpaceForChild(lineCrossAxisExtent, child);
            ASSERT(!child.needsLayout());
            LayoutUnit desiredLogicalHeight = child.constrainLogicalHeightByMinMax(stretchedLogicalHeight, child.logicalHeight() - child.borderAndPaddingLogicalHeight());

            // FIXME: Can avoid laying out here in some cases. See https://webkit.org/b/87905.
            if (desiredLogicalHeight != child.logicalHeight()) {
                child.setOverrideLogicalContentHeight(desiredLogicalHeight - child.borderAndPaddingLogicalHeight());
                child.setLogicalHeight(0);
                child.setChildNeedsLayout(MarkOnlyThis);
                child.layout();
            }
        }
    } else if (isColumnFlow() && child.style().logicalWidth().isAuto()) {
        // FIXME: If the child doesn't have orthogonal flow, then it already has an override width set, so use it.
        if (hasOrthogonalFlow(child)) {
            LayoutUnit childWidth = std::max<LayoutUnit>(0, lineCrossAxisExtent - crossAxisMarginExtentForChild(child));
            childWidth = child.constrainLogicalWidthInRegionByMinMax(childWidth, childWidth, *this);

            if (childWidth != child.logicalWidth()) {
                child.setOverrideLogicalContentWidth(childWidth - child.borderAndPaddingLogicalWidth());
                child.setChildNeedsLayout(MarkOnlyThis);
                child.layout();
            }
        }
    }
}

void RenderFlexibleBox::flipForRightToLeftColumn()
{
    if (style().isLeftToRightDirection() || !isColumnFlow())
        return;

    LayoutUnit crossExtent = crossAxisExtent();
    for (RenderBox* child = m_orderIterator.first(); child; child = m_orderIterator.next()) {
        if (child->isOutOfFlowPositioned())
            continue;
        LayoutPoint location = flowAwareLocationForChild(*child);
        location.setY(crossExtent - crossAxisExtentForChild(*child) - location.y());
        setFlowAwareLocationForChild(*child, location);
    }
}

void RenderFlexibleBox::flipForWrapReverse(const Vector<LineContext>& lineContexts, LayoutUnit crossAxisStartEdge)
{
    LayoutUnit contentExtent = crossAxisContentExtent();
    RenderBox* child = m_orderIterator.first();
    for (size_t lineNumber = 0; lineNumber < lineContexts.size(); ++lineNumber) {
        for (size_t childNumber = 0; childNumber < lineContexts[lineNumber].numberOfChildren; ++childNumber, child = m_orderIterator.next()) {
            ASSERT(child);
            LayoutUnit lineCrossAxisExtent = lineContexts[lineNumber].crossAxisExtent;
            LayoutUnit originalOffset = lineContexts[lineNumber].crossAxisOffset - crossAxisStartEdge;
            LayoutUnit newOffset = contentExtent - originalOffset - lineCrossAxisExtent;
            adjustAlignmentForChild(*child, newOffset - originalOffset);
        }
    }
}

bool RenderFlexibleBox::isTopLayoutOverflowAllowed() const
{
    bool hasTopOverflow = RenderBlock::isTopLayoutOverflowAllowed();
    if (hasTopOverflow || !style().isReverseFlexDirection())
        return hasTopOverflow;
    
    return !isHorizontalFlow();
}

bool RenderFlexibleBox::isLeftLayoutOverflowAllowed() const
{
    bool hasLeftOverflow = RenderBlock::isLeftLayoutOverflowAllowed();
    if (hasLeftOverflow || !style().isReverseFlexDirection())
        return hasLeftOverflow;
    
    return isHorizontalFlow();
}

}
