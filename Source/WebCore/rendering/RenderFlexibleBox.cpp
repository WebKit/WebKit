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

#include "FlexibleBoxAlgorithm.h"
#include "LayoutRepainter.h"
#include "RenderChildIterator.h"
#include "RenderLayer.h"
#include "RenderLayoutState.h"
#include "RenderView.h"
#include <limits>
#include <wtf/IsoMallocInlines.h>
#include <wtf/MathExtras.h>
#include <wtf/SetForScope.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderFlexibleBox);

struct RenderFlexibleBox::LineContext {
    LineContext(LayoutUnit crossAxisOffset, LayoutUnit crossAxisExtent, LayoutUnit maxAscent, Vector<FlexItem>&& flexItems)
        : crossAxisOffset(crossAxisOffset)
        , crossAxisExtent(crossAxisExtent)
        , maxAscent(maxAscent)
        , flexItems(flexItems)
    {
    }
    
    LayoutUnit crossAxisOffset;
    LayoutUnit crossAxisExtent;
    LayoutUnit maxAscent;
    Vector<FlexItem> flexItems;
};

RenderFlexibleBox::RenderFlexibleBox(Element& element, RenderStyle&& style)
    : RenderBlock(element, WTFMove(style), 0)
{
    setChildrenInline(false); // All of our children must be block-level.
}

RenderFlexibleBox::RenderFlexibleBox(Document& document, RenderStyle&& style)
    : RenderBlock(document, WTFMove(style), 0)
{
    setChildrenInline(false); // All of our children must be block-level.
}

RenderFlexibleBox::~RenderFlexibleBox() = default;

const char* RenderFlexibleBox::renderName() const
{
    return "RenderFlexibleBox";
}

void RenderFlexibleBox::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    LayoutUnit childMinWidth;
    LayoutUnit childMaxWidth;
    bool hadExcludedChildren = computePreferredWidthsForExcludedChildren(childMinWidth, childMaxWidth);

    // FIXME: We're ignoring flex-basis here and we shouldn't. We can't start
    // honoring it though until the flex shorthand stops setting it to 0. See
    // https://bugs.webkit.org/show_bug.cgi?id=116117 and
    // https://crbug.com/240765.
    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (child->isOutOfFlowPositioned() || child->isExcludedFromNormalLayout())
            continue;
        
        LayoutUnit margin = marginIntrinsicLogicalWidthForChild(*child);
        
        LayoutUnit minPreferredLogicalWidth;
        LayoutUnit maxPreferredLogicalWidth;
        computeChildPreferredLogicalWidths(*child, minPreferredLogicalWidth, maxPreferredLogicalWidth);
        
        minPreferredLogicalWidth += margin;
        maxPreferredLogicalWidth += margin;

        if (!isColumnFlow()) {
            maxLogicalWidth += maxPreferredLogicalWidth;
            if (isMultiline()) {
                // For multiline, the min preferred width is if you put a break between
                // each item.
                minLogicalWidth = std::max(minLogicalWidth, minPreferredLogicalWidth);
            } else
                minLogicalWidth += minPreferredLogicalWidth;
        } else {
            minLogicalWidth = std::max(minPreferredLogicalWidth, minLogicalWidth);
            maxLogicalWidth = std::max(maxPreferredLogicalWidth, maxLogicalWidth);
        }
    }
    
    maxLogicalWidth = std::max(minLogicalWidth, maxLogicalWidth);
    
    // Due to negative margins, it is possible that we calculated a negative
    // intrinsic width. Make sure that we never return a negative width.
    minLogicalWidth = std::max(0_lu, minLogicalWidth);
    maxLogicalWidth = std::max(0_lu, maxLogicalWidth);
    
    if (hadExcludedChildren) {
        minLogicalWidth = std::max(minLogicalWidth, childMinWidth);
        maxLogicalWidth = std::max(maxLogicalWidth, childMaxWidth);
    }

    LayoutUnit scrollbarWidth(scrollbarLogicalWidth());
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

static int synthesizedBaselineFromBorderBox(const RenderBox& box, LineDirectionMode direction)
{
    return (direction == HorizontalLine ? box.size().height() : box.size().width()).toInt();
}

int RenderFlexibleBox::baselinePosition(FontBaseline, bool, LineDirectionMode direction, LinePositionMode) const
{
    auto baseline = firstLineBaseline();
    if (!baseline)
        return synthesizedBaselineFromBorderBox(*this, direction) + marginLogicalHeight();

    return baseline.value() + (direction == HorizontalLine ? marginTop() : marginRight()).toInt();
}

Optional<int> RenderFlexibleBox::firstLineBaseline() const
{
    if (isWritingModeRoot() || m_numberOfInFlowChildrenOnFirstLine <= 0)
        return Optional<int>();
    RenderBox* baselineChild = nullptr;
    int childNumber = 0;
    for (RenderBox* child = m_orderIterator.first(); child; child = m_orderIterator.next()) {
        if (m_orderIterator.shouldSkipChild(*child))
            continue;
        if (alignmentForChild(*child) == ItemPosition::Baseline && !hasAutoMarginsInCrossAxis(*child)) {
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
        return Optional<int>(synthesizedBaselineFromBorderBox(*baselineChild, direction) + baselineChild->logicalTop());
    }

    return Optional<int>(baseline.value() + baselineChild->logicalTop());
}

Optional<int> RenderFlexibleBox::inlineBlockBaseline(LineDirectionMode) const
{
    return firstLineBaseline();
}

static const StyleContentAlignmentData& contentAlignmentNormalBehavior()
{
    // The justify-content property applies along the main axis, but since
    // flexing in the main axis is controlled by flex, stretch behaves as
    // flex-start (ignoring the specified fallback alignment, if any).
    // https://drafts.csswg.org/css-align/#distribution-flex
    static const StyleContentAlignmentData normalBehavior = { ContentPosition::Normal, ContentDistribution::Stretch};
    return normalBehavior;
}

void RenderFlexibleBox::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);
    if (!oldStyle || diff != StyleDifference::Layout)
        return;

    if (oldStyle->resolvedAlignItems(selfAlignmentNormalBehavior()).position() == ItemPosition::Stretch) {
        // Flex items that were previously stretching need to be relayed out so we
        // can compute new available cross axis space. This is only necessary for
        // stretching since other alignment values don't change the size of the
        // box.
        for (auto& child : childrenOfType<RenderBox>(*this)) {
            ItemPosition previousAlignment = child.style().resolvedAlignSelf(oldStyle, selfAlignmentNormalBehavior()).position();
            if (previousAlignment == ItemPosition::Stretch && previousAlignment != child.style().resolvedAlignSelf(&style(), selfAlignmentNormalBehavior()).position())
                child.setChildNeedsLayout(MarkOnlyThis);
        }
    }
}

void RenderFlexibleBox::layoutBlock(bool relayoutChildren, LayoutUnit)
{
    ASSERT(needsLayout());

    if (!relayoutChildren && simplifiedLayout())
        return;

    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());

    resetLogicalHeightBeforeLayoutIfNeeded();
    m_relaidOutChildren.clear();
    
    bool oldInLayout = m_inLayout;
    m_inLayout = true;
    
    if (recomputeLogicalWidth())
        relayoutChildren = true;

    LayoutUnit previousHeight = logicalHeight();
    setLogicalHeight(borderAndPaddingLogicalHeight() + scrollbarLogicalHeight());
    {
        LayoutStateMaintainer statePusher(*this, locationOffset(), hasTransform() || hasReflection() || style().isFlippedBlocksWritingMode());

        preparePaginationBeforeBlockLayout(relayoutChildren);

        m_numberOfInFlowChildrenOnFirstLine = -1;

        beginUpdateScrollInfoAfterLayoutTransaction();

        prepareOrderIteratorAndMargins();

        // Fieldsets need to find their legend and position it inside the border of the object.
        // The legend then gets skipped during normal layout. The same is true for ruby text.
        // It doesn't get included in the normal layout process but is instead skipped.
        layoutExcludedChildren(relayoutChildren);

        ChildFrameRects oldChildRects;
        appendChildFrameRects(oldChildRects);

        layoutFlexItems(relayoutChildren);

        endAndCommitUpdateScrollInfoAfterLayoutTransaction();

        if (logicalHeight() != previousHeight)
            relayoutChildren = true;

        layoutPositionedObjects(relayoutChildren || isDocumentElementRenderer());

        repaintChildrenDuringLayoutIfMoved(oldChildRects);
        // FIXME: css3/flexbox/repaint-rtl-column.html seems to repaint more overflow than it needs to.
        computeOverflow(clientLogicalBottomAfterRepositioning());
    }
    updateLayerTransform();

    // We have to reset this, because changes to our ancestors' style can affect
    // this value. Also, this needs to be before we call updateAfterLayout, as
    // that function may re-enter this one.
    m_hasDefiniteHeight = SizeDefiniteness::Unknown;

    // Update our scroll information if we're overflow:auto/scroll/hidden now that we know if we overflow or not.
    updateScrollInfoAfterLayout();

    repainter.repaintAfterLayout();

    clearNeedsLayout();
    
    m_inLayout = oldInLayout;
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
    LayoutUnit crossAxisStartEdge = lineContexts.isEmpty() ? 0_lu : lineContexts[0].crossAxisOffset;
    alignFlexLines(lineContexts);
    
    alignChildren(lineContexts);
    
    if (style().flexWrap() == FlexWrap::Reverse)
        flipForWrapReverse(lineContexts, crossAxisStartEdge);
    
    // direction:rtl + flex-direction:column means the cross-axis direction is
    // flipped.
    flipForRightToLeftColumn(lineContexts);
}

LayoutUnit RenderFlexibleBox::clientLogicalBottomAfterRepositioning()
{
    LayoutUnit maxChildLogicalBottom;
    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (child->isOutOfFlowPositioned())
            continue;
        LayoutUnit childLogicalBottom = logicalTopForChild(*child) + logicalHeightForChild(*child) + marginAfterForChild(*child);
        maxChildLogicalBottom = std::max(maxChildLogicalBottom, childLogicalBottom);
    }
    return std::max(clientLogicalBottom(), maxChildLogicalBottom + paddingAfter());
}

bool RenderFlexibleBox::hasOrthogonalFlow(const RenderBox& child) const
{
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
    return style().isLeftToRightDirection() ^ (style().flexDirection() == FlexDirection::RowReverse);
}

bool RenderFlexibleBox::isMultiline() const
{
    return style().flexWrap() != FlexWrap::NoWrap;
}

Length RenderFlexibleBox::flexBasisForChild(const RenderBox& child) const
{
    Length flexLength = child.style().flexBasis();
    if (flexLength.isAuto())
        flexLength = isHorizontalFlow() ? child.style().width() : child.style().height();
    return flexLength;
}

LayoutUnit RenderFlexibleBox::crossAxisExtentForChild(const RenderBox& child) const
{
    return isHorizontalFlow() ? child.height() : child.width();
}

LayoutUnit RenderFlexibleBox::cachedChildIntrinsicContentLogicalHeight(const RenderBox& child) const
{
    if (child.isRenderReplaced())
        return downcast<RenderReplaced>(child).intrinsicLogicalHeight();
    
    if (m_intrinsicContentLogicalHeights.contains(&child))
        return m_intrinsicContentLogicalHeights.get(&child);
    
    return child.contentLogicalHeight();
}

void RenderFlexibleBox::setCachedChildIntrinsicContentLogicalHeight(const RenderBox& child, LayoutUnit height)
{
    if (child.isRenderReplaced())
        return; // Replaced elements know their intrinsic height already, so save space by not caching.
    m_intrinsicContentLogicalHeights.set(&child, height);
}

void RenderFlexibleBox::clearCachedChildIntrinsicContentLogicalHeight(const RenderBox& child)
{
    if (child.isRenderReplaced())
        return; // Replaced elements know their intrinsic height already, so nothing to do.
    m_intrinsicContentLogicalHeights.remove(&child);
}

LayoutUnit RenderFlexibleBox::childIntrinsicLogicalHeight(const RenderBox& child) const
{
    // This should only be called if the logical height is the cross size
    ASSERT(!hasOrthogonalFlow(child));
    if (needToStretchChildLogicalHeight(child)) {
        LayoutUnit childContentHeight = cachedChildIntrinsicContentLogicalHeight(child);
        LayoutUnit childLogicalHeight = childContentHeight + child.scrollbarLogicalHeight() + child.borderAndPaddingLogicalHeight();
        return child.constrainLogicalHeightByMinMax(childLogicalHeight, childContentHeight);
    }
    return child.logicalHeight();
}

LayoutUnit RenderFlexibleBox::childIntrinsicLogicalWidth(const RenderBox& child) const
{
    // This should only be called if the logical width is the cross size
    ASSERT(hasOrthogonalFlow(child));
    // If our height is auto, make sure that our returned height is unaffected by
    // earlier layouts by returning the max preferred logical width
    if (!crossAxisLengthIsDefinite(child, child.style().logicalWidth()))
        return child.maxPreferredLogicalWidth();
    return child.logicalWidth();
}

LayoutUnit RenderFlexibleBox::crossAxisIntrinsicExtentForChild(const RenderBox& child) const
{
    return hasOrthogonalFlow(child) ? childIntrinsicLogicalWidth(child) : childIntrinsicLogicalHeight(child);
}

LayoutUnit RenderFlexibleBox::mainAxisExtentForChild(const RenderBox& child) const
{
    return isHorizontalFlow() ? child.size().width() : child.size().height();
}

LayoutUnit RenderFlexibleBox::mainAxisContentExtentForChildIncludingScrollbar(const RenderBox& child) const
{
    return isHorizontalFlow() ? child.contentWidth() + child.verticalScrollbarWidth() : child.contentHeight() + child.horizontalScrollbarHeight();
}

LayoutUnit RenderFlexibleBox::crossAxisExtent() const
{
    return isHorizontalFlow() ? size().height() : size().width();
}

LayoutUnit RenderFlexibleBox::mainAxisExtent() const
{
    return isHorizontalFlow() ? size().width() : size().height();
}

LayoutUnit RenderFlexibleBox::crossAxisContentExtent() const
{
    return isHorizontalFlow() ? contentHeight() : contentWidth();
}

LayoutUnit RenderFlexibleBox::mainAxisContentExtent(LayoutUnit contentLogicalHeight)
{
    if (isColumnFlow()) {
        LayoutUnit borderPaddingAndScrollbar = borderAndPaddingLogicalHeight() + scrollbarLogicalHeight();
        LayoutUnit borderBoxLogicalHeight = contentLogicalHeight + borderPaddingAndScrollbar;
        auto computedValues = computeLogicalHeight(borderBoxLogicalHeight, logicalTop());
        if (computedValues.m_extent == LayoutUnit::max())
            return computedValues.m_extent;
        return std::max(0_lu, computedValues.m_extent - borderPaddingAndScrollbar);
    }
    return contentLogicalWidth();
}

Optional<LayoutUnit> RenderFlexibleBox::computeMainAxisExtentForChild(const RenderBox& child, SizeType sizeType, const Length& size)
{
    // If we have a horizontal flow, that means the main size is the width.
    // That's the logical width for horizontal writing modes, and the logical
    // height in vertical writing modes. For a vertical flow, main size is the
    // height, so it's the inverse. So we need the logical width if we have a
    // horizontal flow and horizontal writing mode, or vertical flow and vertical
    // writing mode. Otherwise we need the logical height.
    if (isHorizontalFlow() != child.style().isHorizontalWritingMode()) {
        // We don't have to check for "auto" here - computeContentLogicalHeight
        // will just return a null Optional for that case anyway. It's safe to access
        // scrollbarLogicalHeight here because ComputeNextFlexLine will have
        // already forced layout on the child. We previously did a layout out the child
        // if necessary (see ComputeNextFlexLine and the call to
        // childHasIntrinsicMainAxisSize) so we can be sure that the two height
        // calls here will return up-to-date data.
        Optional<LayoutUnit> height = child.computeContentLogicalHeight(sizeType, size, cachedChildIntrinsicContentLogicalHeight(child));
        if (!height)
            return height;
        return height.value() + child.scrollbarLogicalHeight();
    }

    // computeLogicalWidth always re-computes the intrinsic widths. However, when
    // our logical width is auto, we can just use our cached value. So let's do
    // that here. (Compare code in LayoutBlock::computePreferredLogicalWidths)
    LayoutUnit borderAndPadding = child.borderAndPaddingLogicalWidth();
    if (child.style().logicalWidth().isAuto() && !child.hasAspectRatio()) {
        if (size.type() == MinContent)
            return child.minPreferredLogicalWidth() - borderAndPadding;
        if (size.type() == MaxContent)
            return child.maxPreferredLogicalWidth() - borderAndPadding;
    }
    
    // FIXME: Figure out how this should work for regions and pass in the appropriate values.
    RenderFragmentContainer* fragment = nullptr;
    return child.computeLogicalWidthInFragmentUsing(sizeType, size, contentLogicalWidth(), *this, fragment) - borderAndPadding;
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

LayoutUnit RenderFlexibleBox::flowAwareMarginStartForChild(const RenderBox& child) const
{
    if (isHorizontalFlow())
        return isLeftToRightFlow() ? child.marginLeft() : child.marginRight();
    return isLeftToRightFlow() ? child.marginTop() : child.marginBottom();
}

LayoutUnit RenderFlexibleBox::flowAwareMarginEndForChild(const RenderBox& child) const
{
    if (isHorizontalFlow())
        return isLeftToRightFlow() ? child.marginRight() : child.marginLeft();
    return isLeftToRightFlow() ? child.marginBottom() : child.marginTop();
}

LayoutUnit RenderFlexibleBox::flowAwareMarginBeforeForChild(const RenderBox& child) const
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

LayoutUnit RenderFlexibleBox::crossAxisMarginExtentForChild(const RenderBox& child) const
{
    return isHorizontalFlow() ? child.verticalMarginExtent() : child.horizontalMarginExtent();
}

LayoutUnit RenderFlexibleBox::crossAxisScrollbarExtent() const
{
    return isHorizontalFlow() ? horizontalScrollbarHeight() : verticalScrollbarWidth();
}

LayoutPoint RenderFlexibleBox::flowAwareLocationForChild(const RenderBox& child) const
{
    return isHorizontalFlow() ? child.location() : child.location().transposedPoint();
}

bool RenderFlexibleBox::useChildAspectRatio(const RenderBox& child) const
{
    if (!child.hasAspectRatio())
        return false;
    if (!child.intrinsicSize().height()) {
        // We can't compute a ratio in this case.
        return false;
    }
    Length crossSize;
    if (isHorizontalFlow())
        crossSize = child.style().height();
    else
        crossSize = child.style().width();
    return crossAxisLengthIsDefinite(child, crossSize);
}

    
LayoutUnit RenderFlexibleBox::computeMainSizeFromAspectRatioUsing(const RenderBox& child, Length crossSizeLength) const
{
    ASSERT(child.hasAspectRatio());
    ASSERT(child.intrinsicSize().height());
    
    Optional<LayoutUnit> crossSize;
    if (crossSizeLength.isFixed())
        crossSize = LayoutUnit(crossSizeLength.value());
    else {
        ASSERT(crossSizeLength.isPercentOrCalculated());
        crossSize = hasOrthogonalFlow(child) ? adjustBorderBoxLogicalWidthForBoxSizing(valueForLength(crossSizeLength, contentWidth())) : child.computePercentageLogicalHeight(crossSizeLength);
        if (!crossSize)
            return 0_lu;
    }
    
    const LayoutSize& childIntrinsicSize = child.intrinsicSize();
    double ratio = childIntrinsicSize.width().toFloat() /
    childIntrinsicSize.height().toFloat();
    if (isHorizontalFlow())
        return LayoutUnit(crossSize.value() * ratio);
    return LayoutUnit(crossSize.value() / ratio);
}

void RenderFlexibleBox::setFlowAwareLocationForChild(RenderBox& child, const LayoutPoint& location)
{
    if (isHorizontalFlow())
        child.setLocation(location);
    else
        child.setLocation(location.transposedPoint());
}
    
bool RenderFlexibleBox::mainAxisLengthIsDefinite(const RenderBox& child, const Length& flexBasis) const
{
    if (flexBasis.isAuto())
        return false;
    if (flexBasis.isPercentOrCalculated()) {
        if (!isColumnFlow() || m_hasDefiniteHeight == SizeDefiniteness::Definite)
            return true;
        if (m_hasDefiniteHeight == SizeDefiniteness::Indefinite)
            return false;
        bool definite = child.computePercentageLogicalHeight(flexBasis) != WTF::nullopt;
        if (m_inLayout) {
            // We can reach this code even while we're not laying ourselves out, such
            // as from mainSizeForPercentageResolution.
            m_hasDefiniteHeight = definite ? SizeDefiniteness::Definite : SizeDefiniteness::Indefinite;
        }
        return definite;
    }
    return true;
}

bool RenderFlexibleBox::crossAxisLengthIsDefinite(const RenderBox& child, const Length& length) const
{
    if (length.isAuto())
        return false;
    if (length.isPercentOrCalculated()) {
        if (hasOrthogonalFlow(child) || m_hasDefiniteHeight == SizeDefiniteness::Definite)
            return true;
        if (m_hasDefiniteHeight == SizeDefiniteness::Indefinite)
            return false;
        bool definite = bool(child.computePercentageLogicalHeight(length));
        m_hasDefiniteHeight = definite ? SizeDefiniteness::Definite : SizeDefiniteness::Indefinite;
        return definite;
    }
    // FIXME: Eventually we should support other types of sizes here.
    // Requires updating computeMainSizeFromAspectRatioUsing.
    return length.isFixed();
}

void RenderFlexibleBox::cacheChildMainSize(const RenderBox& child)
{
    ASSERT(!child.needsLayout());
    LayoutUnit mainSize;
    if (hasOrthogonalFlow(child))
        mainSize = child.logicalHeight();
    else
        mainSize = child.maxPreferredLogicalWidth();
    m_intrinsicSizeAlongMainAxis.set(&child, mainSize);
    m_relaidOutChildren.add(&child);
}

void RenderFlexibleBox::clearCachedMainSizeForChild(const RenderBox& child)
{
    m_intrinsicSizeAlongMainAxis.remove(&child);
}

    
LayoutUnit RenderFlexibleBox::computeInnerFlexBaseSizeForChild(RenderBox& child, LayoutUnit mainAxisBorderAndPadding, bool relayoutChildren)
{
    child.clearOverrideContentSize();
    
    Length flexBasis = flexBasisForChild(child);
    if (mainAxisLengthIsDefinite(child, flexBasis))
        return std::max(0_lu, computeMainAxisExtentForChild(child, MainOrPreferredSize, flexBasis).value());

    // The flex basis is indefinite (=auto), so we need to compute the actual
    // width of the child. For the logical width axis we just use the preferred
    // width; for the height we need to lay out the child.
    LayoutUnit mainAxisExtent;
    if (hasOrthogonalFlow(child)) {
        updateBlockChildDirtyBitsBeforeLayout(relayoutChildren, child);
        if (child.needsLayout() || relayoutChildren || !m_intrinsicSizeAlongMainAxis.contains(&child)) {
            if (!child.needsLayout())
                child.setChildNeedsLayout(MarkOnlyThis);
            child.layoutIfNeeded();
            cacheChildMainSize(child);
        }
        mainAxisExtent = m_intrinsicSizeAlongMainAxis.get(&child);
    } else {
        // We don't need to add scrollbarLogicalWidth here because the preferred
        // width includes the scrollbar, even for overflow: auto.
        mainAxisExtent = child.maxPreferredLogicalWidth();
    }
    return mainAxisExtent - mainAxisBorderAndPadding;
}

void RenderFlexibleBox::layoutFlexItems(bool relayoutChildren)
{
    Vector<LineContext> lineContexts;
    LayoutUnit sumFlexBaseSize;
    double totalFlexGrow;
    double totalFlexShrink;
    double totalWeightedFlexShrink;
    LayoutUnit sumHypotheticalMainSize;
    
    // Set up our master list of flex items. All of the rest of the algorithm
    // should work off this list of a subset.
    // TODO(cbiesinger): That second part is not yet true.
    Vector<FlexItem> allItems;
    m_orderIterator.first();
    for (RenderBox* child = m_orderIterator.currentChild(); child; child = m_orderIterator.next()) {
        if (m_orderIterator.shouldSkipChild(*child)) {
            // Out-of-flow children are not flex items, so we skip them here.
            if (child->isOutOfFlowPositioned())
                prepareChildForPositionedLayout(*child);
            continue;
        }
        allItems.append(constructFlexItem(*child, relayoutChildren));
    }
    
    const LayoutUnit lineBreakLength = mainAxisContentExtent(LayoutUnit::max());
    FlexLayoutAlgorithm flexAlgorithm(style(), lineBreakLength, allItems);
    LayoutUnit crossAxisOffset = flowAwareBorderBefore() + flowAwarePaddingBefore();
    Vector<FlexItem> lineItems;
    size_t nextIndex = 0;
    while (flexAlgorithm.computeNextFlexLine(nextIndex, lineItems, sumFlexBaseSize, totalFlexGrow, totalFlexShrink, totalWeightedFlexShrink, sumHypotheticalMainSize)) {
        LayoutUnit containerMainInnerSize = mainAxisContentExtent(sumHypotheticalMainSize);
        // availableFreeSpace is the initial amount of free space in this flexbox.
        // remainingFreeSpace starts out at the same value but as we place and lay
        // out flex items we subtract from it. Note that both values can be
        // negative.
        LayoutUnit remainingFreeSpace = containerMainInnerSize - sumFlexBaseSize;
        FlexSign flexSign = (sumHypotheticalMainSize < containerMainInnerSize) ? PositiveFlexibility : NegativeFlexibility;
        freezeInflexibleItems(flexSign, lineItems, remainingFreeSpace, totalFlexGrow, totalFlexShrink, totalWeightedFlexShrink);
        // The initial free space gets calculated after freezing inflexible items.
        // https://drafts.csswg.org/css-flexbox/#resolve-flexible-lengths step 3
        const LayoutUnit initialFreeSpace = remainingFreeSpace;
        while (!resolveFlexibleLengths(flexSign, lineItems, initialFreeSpace, remainingFreeSpace, totalFlexGrow, totalFlexShrink, totalWeightedFlexShrink)) {
            ASSERT(totalFlexGrow >= 0);
            ASSERT(totalWeightedFlexShrink >= 0);
        }
        
        // Recalculate the remaining free space. The adjustment for flex factors
        // between 0..1 means we can't just use remainingFreeSpace here.
        remainingFreeSpace = containerMainInnerSize;
        for (size_t i = 0; i < lineItems.size(); ++i) {
            FlexItem& flexItem = lineItems[i];
            ASSERT(!flexItem.box.isOutOfFlowPositioned());
            remainingFreeSpace -= flexItem.flexedMarginBoxSize();
        }
        // This will std::move lineItems into a newly-created LineContext.
        layoutAndPlaceChildren(crossAxisOffset, lineItems, remainingFreeSpace, relayoutChildren, lineContexts);
    }

    if (hasLineIfEmpty()) {
        // Even if computeNextFlexLine returns true, the flexbox might not have
        // a line because all our children might be out of flow positioned.
        // Instead of just checking if we have a line, make sure the flexbox
        // has at least a line's worth of height to cover this case.
        LayoutUnit minHeight = borderAndPaddingLogicalHeight() + lineHeight(true, isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes) + scrollbarLogicalHeight();
        if (size().height() < minHeight)
            setLogicalHeight(minHeight);
    }
    
    updateLogicalHeight();
    repositionLogicalHeightDependentFlexItems(lineContexts);
}

LayoutUnit RenderFlexibleBox::autoMarginOffsetInMainAxis(const Vector<FlexItem>& children, LayoutUnit& availableFreeSpace)
{
    if (availableFreeSpace <= 0_lu)
        return 0_lu;
    
    int numberOfAutoMargins = 0;
    bool isHorizontal = isHorizontalFlow();
    for (size_t i = 0; i < children.size(); ++i) {
        const auto& child = children[i].box;
        ASSERT(!child.isOutOfFlowPositioned());
        if (isHorizontal) {
            if (child.style().marginLeft().isAuto())
                ++numberOfAutoMargins;
            if (child.style().marginRight().isAuto())
                ++numberOfAutoMargins;
        } else {
            if (child.style().marginTop().isAuto())
                ++numberOfAutoMargins;
            if (child.style().marginBottom().isAuto())
                ++numberOfAutoMargins;
        }
    }
    if (!numberOfAutoMargins)
        return 0_lu;
    
    LayoutUnit sizeOfAutoMargin = availableFreeSpace / numberOfAutoMargins;
    availableFreeSpace = 0_lu;
    return sizeOfAutoMargin;
}

void RenderFlexibleBox::updateAutoMarginsInMainAxis(RenderBox& child, LayoutUnit autoMarginOffset)
{
    ASSERT(autoMarginOffset >= 0_lu);
    
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

bool RenderFlexibleBox::hasAutoMarginsInCrossAxis(const RenderBox& child) const
{
    if (isHorizontalFlow())
        return child.style().marginTop().isAuto() || child.style().marginBottom().isAuto();
    return child.style().marginLeft().isAuto() || child.style().marginRight().isAuto();
}

LayoutUnit RenderFlexibleBox::availableAlignmentSpaceForChild(LayoutUnit lineCrossAxisExtent, const RenderBox& child)
{
    ASSERT(!child.isOutOfFlowPositioned());
    LayoutUnit childCrossExtent = crossAxisMarginExtentForChild(child) + crossAxisExtentForChild(child);
    return lineCrossAxisExtent - childCrossExtent;
}

bool RenderFlexibleBox::updateAutoMarginsInCrossAxis(RenderBox& child, LayoutUnit availableAlignmentSpace)
{
    ASSERT(!child.isOutOfFlowPositioned());
    ASSERT(availableAlignmentSpace >= 0_lu);
    
    bool isHorizontal = isHorizontalFlow();
    Length topOrLeft = isHorizontal ? child.style().marginTop() : child.style().marginLeft();
    Length bottomOrRight = isHorizontal ? child.style().marginBottom() : child.style().marginRight();
    if (topOrLeft.isAuto() && bottomOrRight.isAuto()) {
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
    bool shouldAdjustTopOrLeft = true;
    if (isColumnFlow() && !child.style().isLeftToRightDirection()) {
        // For column flows, only make this adjustment if topOrLeft corresponds to
        // the "before" margin, so that flipForRightToLeftColumn will do the right
        // thing.
        shouldAdjustTopOrLeft = false;
    }
    if (!isColumnFlow() && child.style().isFlippedBlocksWritingMode()) {
        // If we are a flipped writing mode, we need to adjust the opposite side.
        // This is only needed for row flows because this only affects the
        // block-direction axis.
        shouldAdjustTopOrLeft = false;
    }
    
    if (topOrLeft.isAuto()) {
        if (shouldAdjustTopOrLeft)
            adjustAlignmentForChild(child, availableAlignmentSpace);
        
        if (isHorizontal)
            child.setMarginTop(availableAlignmentSpace);
        else
            child.setMarginLeft(availableAlignmentSpace);
        return true;
    }

    if (bottomOrRight.isAuto()) {
        if (!shouldAdjustTopOrLeft)
            adjustAlignmentForChild(child, availableAlignmentSpace);
        
        if (isHorizontal)
            child.setMarginBottom(availableAlignmentSpace);
        else
            child.setMarginRight(availableAlignmentSpace);
        return true;
    }
    return false;
}

LayoutUnit RenderFlexibleBox::marginBoxAscentForChild(const RenderBox& child)
{
    LayoutUnit ascent = child.firstLineBaseline().valueOr(crossAxisExtentForChild(child));
    return ascent + flowAwareMarginBeforeForChild(child);
}

LayoutUnit RenderFlexibleBox::computeChildMarginValue(Length margin)
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
        if (!populator.collectChild(*child))
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

LayoutUnit RenderFlexibleBox::adjustChildSizeForMinAndMax(const RenderBox& child, LayoutUnit childSize)
{
    Length max = isHorizontalFlow() ? child.style().maxWidth() : child.style().maxHeight();
    Optional<LayoutUnit> maxExtent = WTF::nullopt;
    if (max.isSpecifiedOrIntrinsic()) {
        maxExtent = computeMainAxisExtentForChild(child, MaxSize, max);
        childSize = std::min(childSize, maxExtent.valueOr(childSize));
    }

    Length min = isHorizontalFlow() ? child.style().minWidth() : child.style().minHeight();
    if (min.isSpecifiedOrIntrinsic())
        return std::max(childSize, std::max(0_lu, computeMainAxisExtentForChild(child, MinSize, min).valueOr(childSize)));
    
    if (!isFlexibleBoxImpl() && min.isAuto() && mainAxisOverflowForChild(child) == Overflow::Visible && !(isColumnFlow() && is<RenderFlexibleBox>(child))) {
        // FIXME: For now, we do not handle min-height: auto for nested
        // column flexboxes. We need to implement
        // https://drafts.csswg.org/css-flexbox/#intrinsic-sizes before that
        // produces reasonable results. Tracking bug: https://crbug.com/581553
        // css-flexbox section 4.5
        // FIXME: If the min value is expected to be valid here, we need to come up with a non optional version of computeMainAxisExtentForChild and
        // ensure it's valid through the virtual calls of computeIntrinsicLogicalContentHeightUsing.
        LayoutUnit contentSize = computeMainAxisExtentForChild(child, MinSize, Length(MinContent)).valueOr(0);
        ASSERT(contentSize >= 0);
        if (child.hasAspectRatio() && child.intrinsicSize().height() > 0)
            contentSize = adjustChildSizeForAspectRatioCrossAxisMinAndMax(child, contentSize);
        contentSize = std::min(contentSize, maxExtent.valueOr(contentSize));
        
        Length mainSize = isHorizontalFlow() ? child.style().width() : child.style().height();
        if (mainAxisLengthIsDefinite(child, mainSize)) {
            LayoutUnit resolvedMainSize = computeMainAxisExtentForChild(child, MainOrPreferredSize, mainSize).valueOr(0);
            ASSERT(resolvedMainSize >= 0);
            LayoutUnit specifiedSize = std::min(resolvedMainSize, maxExtent.valueOr(resolvedMainSize));
            return std::max(childSize, std::min(specifiedSize, contentSize));
        }

        if (useChildAspectRatio(child)) {
            Length crossSizeLength = isHorizontalFlow() ? child.style().height() : child.style().width();
            Optional<LayoutUnit> transferredSize = computeMainSizeFromAspectRatioUsing(child, crossSizeLength);
            if (transferredSize) {
                transferredSize = adjustChildSizeForAspectRatioCrossAxisMinAndMax(child, transferredSize.value());
                return std::max(childSize, std::min(transferredSize.value(), contentSize));
            }
        }
        
        return std::max(childSize, contentSize);
    }

    return std::max(0_lu, childSize);
}
    
Optional<LayoutUnit> RenderFlexibleBox::crossSizeForPercentageResolution(const RenderBox& child)
{
    if (alignmentForChild(child) != ItemPosition::Stretch)
        return WTF::nullopt;

    // Here we implement https://drafts.csswg.org/css-flexbox/#algo-stretch
    if (hasOrthogonalFlow(child) && child.hasOverrideContentLogicalWidth())
        return child.overrideContentLogicalWidth();
    if (!hasOrthogonalFlow(child) && child.hasOverrideContentLogicalHeight())
        return child.overrideContentLogicalHeight();
    
    // We don't currently implement the optimization from
    // https://drafts.csswg.org/css-flexbox/#definite-sizes case 1. While that
    // could speed up a specialized case, it requires determining if we have a
    // definite size, which itself is not cheap. We can consider implementing it
    // at a later time. (The correctness is ensured by redoing layout in
    // applyStretchAlignmentToChild)
    return WTF::nullopt;
}

Optional<LayoutUnit> RenderFlexibleBox::mainSizeForPercentageResolution(const RenderBox& child)
{
    // This function implements section 9.8. Definite and Indefinite Sizes, case
    // 2) of the flexbox spec.
    // We need to check for the flexbox to have a definite main size, and for the
    // flex item to have a definite flex basis.
    const Length& flexBasis = flexBasisForChild(child);
    if (!mainAxisLengthIsDefinite(child, flexBasis))
        return WTF::nullopt;
    if (!flexBasis.isPercentOrCalculated()) {
        // If flex basis had a percentage, our size is guaranteed to be definite or
        // the flex item's size could not be definite. Otherwise, we make up a
        // percentage to check whether we have a definite size.
        if (!mainAxisLengthIsDefinite(child, Length(0, Percent)))
            return WTF::nullopt;
    }
    
    if (hasOrthogonalFlow(child))
        return child.hasOverrideContentLogicalHeight() ? Optional<LayoutUnit>(child.overrideContentLogicalHeight()) : WTF::nullopt;
    return child.hasOverrideContentLogicalWidth() ? Optional<LayoutUnit>(child.overrideContentLogicalWidth()) : WTF::nullopt;
}

Optional<LayoutUnit> RenderFlexibleBox::childLogicalHeightForPercentageResolution(const RenderBox& child)
{
    if (!hasOrthogonalFlow(child))
        return crossSizeForPercentageResolution(child);
    return mainSizeForPercentageResolution(child);
}

LayoutUnit RenderFlexibleBox::adjustChildSizeForAspectRatioCrossAxisMinAndMax(const RenderBox& child, LayoutUnit childSize)
{
    Length crossMin = isHorizontalFlow() ? child.style().minHeight() : child.style().minWidth();
    Length crossMax = isHorizontalFlow() ? child.style().maxHeight() : child.style().maxWidth();
    
    if (crossAxisLengthIsDefinite(child, crossMax)) {
        LayoutUnit maxValue = computeMainSizeFromAspectRatioUsing(child, crossMax);
        childSize = std::min(maxValue, childSize);
    }
    
    if (crossAxisLengthIsDefinite(child, crossMin)) {
        LayoutUnit minValue = computeMainSizeFromAspectRatioUsing(child, crossMin);
        childSize = std::max(minValue, childSize);
    }
    
    return childSize;
}

FlexItem RenderFlexibleBox::constructFlexItem(RenderBox& child, bool relayoutChildren)
{
    // If this condition is true, then computeMainAxisExtentForChild will call
    // child.intrinsicContentLogicalHeight() and
    // child.scrollbarLogicalHeight(), so if the child has intrinsic
    // min/max/preferred size, run layout on it now to make sure its logical
    // height and scroll bars are up to date.
    if (childHasIntrinsicMainAxisSize(child) && child.needsLayout()) {
        child.clearOverrideContentSize();
        child.setChildNeedsLayout(MarkOnlyThis);
        child.layoutIfNeeded();
        cacheChildMainSize(child);
        relayoutChildren = false;
    }
    
    LayoutUnit borderAndPadding = isHorizontalFlow() ? child.horizontalBorderAndPaddingExtent() : child.verticalBorderAndPaddingExtent();
    LayoutUnit childInnerFlexBaseSize = computeInnerFlexBaseSizeForChild(child, borderAndPadding, relayoutChildren);
    LayoutUnit childMinMaxAppliedMainAxisExtent = adjustChildSizeForMinAndMax(child, childInnerFlexBaseSize);
    LayoutUnit margin = isHorizontalFlow() ? child.horizontalMarginExtent() : child.verticalMarginExtent();
    return FlexItem(child, childInnerFlexBaseSize, childMinMaxAppliedMainAxisExtent, borderAndPadding, margin);
}
    
void RenderFlexibleBox::freezeViolations(Vector<FlexItem*>& violations, LayoutUnit& availableFreeSpace, double& totalFlexGrow, double& totalFlexShrink, double& totalWeightedFlexShrink)
{
    for (size_t i = 0; i < violations.size(); ++i) {
        ASSERT(!violations[i]->frozen);
        const auto& child = violations[i]->box;
        LayoutUnit childSize = violations[i]->flexedContentSize;
        availableFreeSpace -= childSize - violations[i]->flexBaseContentSize;
        totalFlexGrow -= child.style().flexGrow();
        totalFlexShrink -= child.style().flexShrink();
        totalWeightedFlexShrink -= child.style().flexShrink() * violations[i]->flexBaseContentSize;
        // totalWeightedFlexShrink can be negative when we exceed the precision of
        // a double when we initially calcuate totalWeightedFlexShrink. We then
        // subtract each child's weighted flex shrink with full precision, now
        // leading to a negative result. See
        // css3/flexbox/large-flex-shrink-assert.html
        totalWeightedFlexShrink = std::max(totalWeightedFlexShrink, 0.0);
        violations[i]->frozen = true;
    }
}
    
void RenderFlexibleBox::freezeInflexibleItems(FlexSign flexSign, Vector<FlexItem>& children, LayoutUnit& remainingFreeSpace, double& totalFlexGrow, double& totalFlexShrink, double& totalWeightedFlexShrink)
{
    // Per https://drafts.csswg.org/css-flexbox/#resolve-flexible-lengths step 2,
    // we freeze all items with a flex factor of 0 as well as those with a min/max
    // size violation.
    Vector<FlexItem*> newInflexibleItems;
    for (size_t i = 0; i < children.size(); ++i) {
        FlexItem& flexItem = children[i];
        const auto& child = flexItem.box;
        ASSERT(!flexItem.box.isOutOfFlowPositioned());
        ASSERT(!flexItem.frozen);
        float flexFactor = (flexSign == PositiveFlexibility) ? child.style().flexGrow() : child.style().flexShrink();
        if (!flexFactor || (flexSign == PositiveFlexibility && flexItem.flexBaseContentSize > flexItem.hypotheticalMainContentSize) || (flexSign == NegativeFlexibility && flexItem.flexBaseContentSize < flexItem.hypotheticalMainContentSize)) {
            flexItem.flexedContentSize = flexItem.hypotheticalMainContentSize;
            newInflexibleItems.append(&flexItem);
        }
    }
    freezeViolations(newInflexibleItems, remainingFreeSpace, totalFlexGrow, totalFlexShrink, totalWeightedFlexShrink);
}

// Returns true if we successfully ran the algorithm and sized the flex items.
bool RenderFlexibleBox::resolveFlexibleLengths(FlexSign flexSign, Vector<FlexItem>& children, LayoutUnit initialFreeSpace, LayoutUnit& remainingFreeSpace, double& totalFlexGrow, double& totalFlexShrink, double& totalWeightedFlexShrink)
{
    LayoutUnit totalViolation;
    LayoutUnit usedFreeSpace;
    Vector<FlexItem*> minViolations;
    Vector<FlexItem*> maxViolations;

    double sumFlexFactors = (flexSign == PositiveFlexibility) ? totalFlexGrow : totalFlexShrink;
    if (sumFlexFactors > 0 && sumFlexFactors < 1) {
        LayoutUnit fractional(initialFreeSpace * sumFlexFactors);
        if (fractional.abs() < remainingFreeSpace.abs())
            remainingFreeSpace = fractional;
    }

    for (size_t i = 0; i < children.size(); ++i) {
        FlexItem& flexItem = children[i];
        const auto& child = flexItem.box;
        
        // This check also covers out-of-flow children.
        if (flexItem.frozen)
            continue;
        
        LayoutUnit childSize = flexItem.flexBaseContentSize;
        double extraSpace = 0;
        if (remainingFreeSpace > 0 && totalFlexGrow > 0 && flexSign == PositiveFlexibility && std::isfinite(totalFlexGrow))
            extraSpace = remainingFreeSpace * child.style().flexGrow() / totalFlexGrow;
        else if (remainingFreeSpace < 0 && totalWeightedFlexShrink > 0 && flexSign == NegativeFlexibility && std::isfinite(totalWeightedFlexShrink) && child.style().flexShrink())
            extraSpace = remainingFreeSpace * child.style().flexShrink() * flexItem.flexBaseContentSize / totalWeightedFlexShrink;
        if (std::isfinite(extraSpace))
            childSize += LayoutUnit::fromFloatRound(extraSpace);
        
        LayoutUnit adjustedChildSize = adjustChildSizeForMinAndMax(child, childSize);
        ASSERT(adjustedChildSize >= 0);
        flexItem.flexedContentSize = adjustedChildSize;
        usedFreeSpace += adjustedChildSize - flexItem.flexBaseContentSize;
        
        LayoutUnit violation = adjustedChildSize - childSize;
        if (violation > 0)
            minViolations.append(&flexItem);
        else if (violation < 0)
            maxViolations.append(&flexItem);
        totalViolation += violation;
    }
    
    if (totalViolation)
        freezeViolations(totalViolation < 0 ? maxViolations : minViolations, remainingFreeSpace, totalFlexGrow, totalFlexShrink, totalWeightedFlexShrink);
    else
        remainingFreeSpace -= usedFreeSpace;
    
    return !totalViolation;
}

static LayoutUnit initialJustifyContentOffset(LayoutUnit availableFreeSpace, ContentPosition justifyContent, ContentDistribution justifyContentDistribution, unsigned numberOfChildren)
{
    if (justifyContent == ContentPosition::FlexEnd)
        return availableFreeSpace;
    if (justifyContent == ContentPosition::Center)
        return availableFreeSpace / 2;
    if (justifyContentDistribution == ContentDistribution::SpaceAround) {
        if (availableFreeSpace > 0 && numberOfChildren)
            return availableFreeSpace / (2 * numberOfChildren);
        else
            return availableFreeSpace / 2;
    }
    if (justifyContentDistribution == ContentDistribution::SpaceEvenly) {
        if (availableFreeSpace > 0 && numberOfChildren)
            return availableFreeSpace / (numberOfChildren + 1);
        // Fallback to 'center'
        return availableFreeSpace / 2;
    }
    return 0;
}

static LayoutUnit justifyContentSpaceBetweenChildren(LayoutUnit availableFreeSpace, ContentDistribution justifyContentDistribution, unsigned numberOfChildren)
{
    if (availableFreeSpace > 0 && numberOfChildren > 1) {
        if (justifyContentDistribution == ContentDistribution::SpaceBetween)
            return availableFreeSpace / (numberOfChildren - 1);
        if (justifyContentDistribution == ContentDistribution::SpaceAround)
            return availableFreeSpace / numberOfChildren;
        if (justifyContentDistribution == ContentDistribution::SpaceEvenly)
            return availableFreeSpace / (numberOfChildren + 1);
    }
    return 0;
}


static LayoutUnit alignmentOffset(LayoutUnit availableFreeSpace, ItemPosition position, LayoutUnit ascent, LayoutUnit maxAscent, bool isWrapReverse)
{
    switch (position) {
    case ItemPosition::Legacy:
    case ItemPosition::Auto:
    case ItemPosition::Normal:
        ASSERT_NOT_REACHED();
        break;
    case ItemPosition::Stretch:
        // Actual stretching must be handled by the caller. Since wrap-reverse
        // flips cross start and cross end, stretch children should be aligned
        // with the cross end. This matters because applyStretchAlignment
        // doesn't always stretch or stretch fully (explicit cross size given, or
        // stretching constrained by max-height/max-width). For flex-start and
        // flex-end this is handled by alignmentForChild().
        if (isWrapReverse)
            return availableFreeSpace;
        break;
    case ItemPosition::FlexStart:
        break;
    case ItemPosition::FlexEnd:
        return availableFreeSpace;
    case ItemPosition::Center:
        return availableFreeSpace / 2;
    case ItemPosition::Baseline:
        // FIXME: If we get here in columns, we want the use the descent, except
        // we currently can't get the ascent/descent of orthogonal children.
        // https://bugs.webkit.org/show_bug.cgi?id=98076
        return maxAscent - ascent;
    case ItemPosition::LastBaseline:
    case ItemPosition::SelfStart:
    case ItemPosition::SelfEnd:
    case ItemPosition::Start:
    case ItemPosition::End:
    case ItemPosition::Left:
    case ItemPosition::Right:
        // FIXME: Implement the extended grammar, enabled when the Grid Layout
        // feature was enabled by default.
        break;
    }
    return 0;
}

void RenderFlexibleBox::setOverrideMainAxisContentSizeForChild(RenderBox& child, LayoutUnit childPreferredSize)
{
    if (hasOrthogonalFlow(child))
        child.setOverrideContentLogicalHeight(childPreferredSize);
    else
        child.setOverrideContentLogicalWidth(childPreferredSize);
}

LayoutUnit RenderFlexibleBox::staticMainAxisPositionForPositionedChild(const RenderBox& child)
{
    const LayoutUnit availableSpace = mainAxisContentExtent(contentLogicalHeight()) - mainAxisExtentForChild(child);

    ContentPosition position = style().resolvedJustifyContentPosition(contentAlignmentNormalBehavior());
    ContentDistribution distribution = style().resolvedJustifyContentDistribution(contentAlignmentNormalBehavior());
    LayoutUnit offset = initialJustifyContentOffset(availableSpace, position, distribution, 1);
    if (style().flexDirection() == FlexDirection::RowReverse || style().flexDirection() == FlexDirection::ColumnReverse)
        offset = availableSpace - offset;
    return offset;
}

LayoutUnit RenderFlexibleBox::staticCrossAxisPositionForPositionedChild(const RenderBox& child)
{
    LayoutUnit availableSpace = crossAxisContentExtent() - crossAxisExtentForChild(child);
    return alignmentOffset(availableSpace, alignmentForChild(child), 0_lu, 0_lu, style().flexWrap() == FlexWrap::Reverse);
}

LayoutUnit RenderFlexibleBox::staticInlinePositionForPositionedChild(const RenderBox& child)
{
    return startOffsetForContent() + (isColumnFlow() ? staticCrossAxisPositionForPositionedChild(child) : staticMainAxisPositionForPositionedChild(child));
}

LayoutUnit RenderFlexibleBox::staticBlockPositionForPositionedChild(const RenderBox& child)
{
    return borderAndPaddingBefore() + (isColumnFlow() ? staticMainAxisPositionForPositionedChild(child) : staticCrossAxisPositionForPositionedChild(child));
}

bool RenderFlexibleBox::setStaticPositionForPositionedLayout(const RenderBox& child)
{
    bool positionChanged = false;
    auto* childLayer = child.layer();
    if (child.style().hasStaticInlinePosition(style().isHorizontalWritingMode())) {
        LayoutUnit inlinePosition = staticInlinePositionForPositionedChild(child);
        if (childLayer->staticInlinePosition() != inlinePosition) {
            childLayer->setStaticInlinePosition(inlinePosition);
            positionChanged = true;
        }
    }
    if (child.style().hasStaticBlockPosition(style().isHorizontalWritingMode())) {
        LayoutUnit blockPosition = staticBlockPositionForPositionedChild(child);
        if (childLayer->staticBlockPosition() != blockPosition) {
            childLayer->setStaticBlockPosition(blockPosition);
            positionChanged = true;
        }
    }
    return positionChanged;
}

void RenderFlexibleBox::prepareChildForPositionedLayout(RenderBox& child)
{
    ASSERT(child.isOutOfFlowPositioned());
    child.containingBlock()->insertPositionedObject(child);
    auto* childLayer = child.layer();
    LayoutUnit staticInlinePosition = flowAwareBorderStart() + flowAwarePaddingStart();
    if (childLayer->staticInlinePosition() != staticInlinePosition) {
        childLayer->setStaticInlinePosition(staticInlinePosition);
        if (child.style().hasStaticInlinePosition(style().isHorizontalWritingMode()))
            child.setChildNeedsLayout(MarkOnlyThis);
    }

    LayoutUnit staticBlockPosition = flowAwareBorderBefore() + flowAwarePaddingBefore();
    if (childLayer->staticBlockPosition() != staticBlockPosition) {
        childLayer->setStaticBlockPosition(staticBlockPosition);
        if (child.style().hasStaticBlockPosition(style().isHorizontalWritingMode()))
            child.setChildNeedsLayout(MarkOnlyThis);
    }
}

ItemPosition RenderFlexibleBox::alignmentForChild(const RenderBox& child) const
{
    ItemPosition align = child.style().resolvedAlignSelf(&style(), selfAlignmentNormalBehavior()).position();
    ASSERT(align != ItemPosition::Auto && align != ItemPosition::Normal);

    if (align == ItemPosition::Baseline && hasOrthogonalFlow(child))
        align = ItemPosition::FlexStart;

    if (style().flexWrap() == FlexWrap::Reverse) {
        if (align == ItemPosition::FlexStart)
            align = ItemPosition::FlexEnd;
        else if (align == ItemPosition::FlexEnd)
            align = ItemPosition::FlexStart;
    }

    return align;
}

void RenderFlexibleBox::resetAutoMarginsAndLogicalTopInCrossAxis(RenderBox& child)
{
    if (hasAutoMarginsInCrossAxis(child)) {
        child.updateLogicalHeight();
        if (isHorizontalFlow()) {
            if (child.style().marginTop().isAuto())
                child.setMarginTop(0_lu);
            if (child.style().marginBottom().isAuto())
                child.setMarginBottom(0_lu);
        } else {
            if (child.style().marginLeft().isAuto())
                child.setMarginLeft(0_lu);
            if (child.style().marginRight().isAuto())
                child.setMarginRight(0_lu);
        }
    }
}

bool RenderFlexibleBox::needToStretchChildLogicalHeight(const RenderBox& child) const
{
    // This function is a little bit magical. It relies on the fact that blocks
    // intrinsically "stretch" themselves in their inline axis, i.e. a <div> has
    // an implicit width: 100%. So the child will automatically stretch if our
    // cross axis is the child's inline axis. That's the case if:
    // - We are horizontal and the child is in vertical writing mode
    // - We are vertical and the child is in horizontal writing mode
    // Otherwise, we need to stretch if the cross axis size is auto.
    if (alignmentForChild(child) != ItemPosition::Stretch)
        return false;

    if (isHorizontalFlow() != child.style().isHorizontalWritingMode())
        return false;

    return child.style().logicalHeight().isAuto();
}

bool RenderFlexibleBox::childHasIntrinsicMainAxisSize(const RenderBox& child) const
{
    bool result = false;
    if (isHorizontalFlow() != child.style().isHorizontalWritingMode()) {
        Length childFlexBasis = flexBasisForChild(child);
        Length childMinSize = isHorizontalFlow() ? child.style().minWidth() : child.style().minHeight();
        Length childMaxSize = isHorizontalFlow() ? child.style().maxWidth() : child.style().maxHeight();
        if (childFlexBasis.isIntrinsic() || childMinSize.isIntrinsicOrAuto() || childMaxSize.isIntrinsic())
            result = true;
    }
    return result;
}

Overflow RenderFlexibleBox::mainAxisOverflowForChild(const RenderBox& child) const
{
    if (isHorizontalFlow())
        return child.style().overflowX();
    return child.style().overflowY();
}

Overflow RenderFlexibleBox::crossAxisOverflowForChild(const RenderBox& child) const
{
    if (isHorizontalFlow())
        return child.style().overflowY();
    return child.style().overflowX();
}

bool RenderFlexibleBox::hasPercentHeightDescendants(const RenderBox& renderer) const
{
    // FIXME: This function can be removed soon after webkit.org/b/204318 is fixed. 
    if (!is<RenderBlock>(renderer))
        return false;
    auto& renderBlock = downcast<RenderBlock>(renderer);
    if (!renderBlock.hasPercentHeightDescendants())
        return false;

    auto* percentHeightDescendants = renderBlock.percentHeightDescendants();
    if (!percentHeightDescendants)
        return false;

    for (auto it = percentHeightDescendants->begin(), end = percentHeightDescendants->end(); it != end; ++it) {
        bool hasOutOfFlowAncestor = false;
        for (auto* ancestor = (*it)->containingBlock(); ancestor && ancestor != &renderBlock; ancestor = ancestor->containingBlock()) {
            if (ancestor->isOutOfFlowPositioned()) {
                hasOutOfFlowAncestor = true;
                break;
            }
        }
        if (!hasOutOfFlowAncestor)
            return true;
    }
    return false;
}

void RenderFlexibleBox::layoutAndPlaceChildren(LayoutUnit& crossAxisOffset, Vector<FlexItem>& children, LayoutUnit availableFreeSpace, bool relayoutChildren, Vector<LineContext>& lineContexts)
{
    ContentPosition position = style().resolvedJustifyContentPosition(contentAlignmentNormalBehavior());
    ContentDistribution distribution = style().resolvedJustifyContentDistribution(contentAlignmentNormalBehavior());

    LayoutUnit autoMarginOffset = autoMarginOffsetInMainAxis(children, availableFreeSpace);
    LayoutUnit mainAxisOffset = flowAwareBorderStart() + flowAwarePaddingStart();
    mainAxisOffset += initialJustifyContentOffset(availableFreeSpace, position, distribution, children.size());
    if (style().flexDirection() == FlexDirection::RowReverse)
        mainAxisOffset += isHorizontalFlow() ? verticalScrollbarWidth() : horizontalScrollbarHeight();

    LayoutUnit totalMainExtent = mainAxisExtent();
    LayoutUnit maxAscent, maxDescent; // Used when align-items: baseline.
    LayoutUnit maxChildCrossAxisExtent;
    bool shouldFlipMainAxis = !isColumnFlow() && !isLeftToRightFlow();
    for (size_t i = 0; i < children.size(); ++i) {
        const auto& flexItem = children[i];
        auto& child = flexItem.box;
        bool childHadLayout = child.everHadLayout();

        ASSERT(!flexItem.box.isOutOfFlowPositioned());

        setOverrideMainAxisContentSizeForChild(child, flexItem.flexedContentSize);
        // The flexed content size and the override size include the scrollbar
        // width, so we need to compare to the size including the scrollbar.
        // TODO(cbiesinger): Should it include the scrollbar?
        if (flexItem.flexedContentSize != mainAxisContentExtentForChildIncludingScrollbar(child))
            child.setChildNeedsLayout(MarkOnlyThis);
        else {
            // To avoid double applying margin changes in
            // updateAutoMarginsInCrossAxis, we reset the margins here.
            resetAutoMarginsAndLogicalTopInCrossAxis(child);
        }
        // We may have already forced relayout for orthogonal flowing children in
        // computeInnerFlexBaseSizeForChild.
        bool forceChildRelayout = relayoutChildren && !m_relaidOutChildren.contains(&child);
        if (!forceChildRelayout && hasPercentHeightDescendants(child)) {
            // Have to force another relayout even though the child is sized
            // correctly, because its descendants are not sized correctly yet. Our
            // previous layout of the child was done without an override height set.
            // So, redo it here.
            forceChildRelayout = true;
        }
        updateBlockChildDirtyBitsBeforeLayout(forceChildRelayout, child);
        if (!child.needsLayout())
            child.markForPaginationRelayoutIfNeeded();
        if (child.needsLayout())
            m_relaidOutChildren.add(&child);
        child.layoutIfNeeded();
        if (!childHadLayout && child.checkForRepaintDuringLayout()) {
            child.repaint();
            child.repaintOverhangingFloats(true);
        }

        updateAutoMarginsInMainAxis(child, autoMarginOffset);

        LayoutUnit childCrossAxisMarginBoxExtent;
        if (alignmentForChild(child) == ItemPosition::Baseline && !hasAutoMarginsInCrossAxis(child)) {
            LayoutUnit ascent = marginBoxAscentForChild(child);
            LayoutUnit descent = (crossAxisMarginExtentForChild(child) + crossAxisExtentForChild(child)) - ascent;

            maxAscent = std::max(maxAscent, ascent);
            maxDescent = std::max(maxDescent, descent);

            // FIXME: Take scrollbar into account
            childCrossAxisMarginBoxExtent = maxAscent + maxDescent;
        } else
            childCrossAxisMarginBoxExtent = crossAxisIntrinsicExtentForChild(child) + crossAxisMarginExtentForChild(child);

        if (!isColumnFlow())
            setLogicalHeight(std::max(logicalHeight(), crossAxisOffset + flowAwareBorderAfter() + flowAwarePaddingAfter() + childCrossAxisMarginBoxExtent + crossAxisScrollbarExtent()));
        maxChildCrossAxisExtent = std::max(maxChildCrossAxisExtent, childCrossAxisMarginBoxExtent);

        mainAxisOffset += flowAwareMarginStartForChild(child);

        LayoutUnit childMainExtent = mainAxisExtentForChild(child);
        // In an RTL column situation, this will apply the margin-right/margin-end
        // on the left. This will be fixed later in flipForRightToLeftColumn.
        LayoutPoint childLocation(shouldFlipMainAxis ? totalMainExtent - mainAxisOffset - childMainExtent : mainAxisOffset, crossAxisOffset + flowAwareMarginBeforeForChild(child));
        setFlowAwareLocationForChild(child, childLocation);
        mainAxisOffset += childMainExtent + flowAwareMarginEndForChild(child);

        if (i != children.size() - 1) {
            // The last item does not get extra space added.
            mainAxisOffset += justifyContentSpaceBetweenChildren(availableFreeSpace, distribution, children.size());
        }

        // FIXME: Deal with pagination.
    }

    if (isColumnFlow())
        setLogicalHeight(std::max(logicalHeight(), mainAxisOffset + flowAwareBorderEnd() + flowAwarePaddingEnd() + scrollbarLogicalHeight()));

    if (style().flexDirection() == FlexDirection::ColumnReverse) {
        // We have to do an extra pass for column-reverse to reposition the flex
        // items since the start depends on the height of the flexbox, which we
        // only know after we've positioned all the flex items.
        updateLogicalHeight();
        layoutColumnReverse(children, crossAxisOffset, availableFreeSpace);
    }

    if (m_numberOfInFlowChildrenOnFirstLine == -1)
        m_numberOfInFlowChildrenOnFirstLine = children.size();
    lineContexts.append(LineContext(crossAxisOffset, maxChildCrossAxisExtent, maxAscent, WTFMove(children)));
    crossAxisOffset += maxChildCrossAxisExtent;
}

void RenderFlexibleBox::layoutColumnReverse(const Vector<FlexItem>& children, LayoutUnit crossAxisOffset, LayoutUnit availableFreeSpace)
{
    ContentPosition position = style().resolvedJustifyContentPosition(contentAlignmentNormalBehavior());
    ContentDistribution distribution = style().resolvedJustifyContentDistribution(contentAlignmentNormalBehavior());
    
    // This is similar to the logic in layoutAndPlaceChildren, except we place
    // the children starting from the end of the flexbox. We also don't need to
    // layout anything since we're just moving the children to a new position.
    LayoutUnit mainAxisOffset = logicalHeight() - flowAwareBorderEnd() - flowAwarePaddingEnd();
    mainAxisOffset -= initialJustifyContentOffset(availableFreeSpace, position, distribution, children.size());
    mainAxisOffset -= isHorizontalFlow() ? verticalScrollbarWidth() : horizontalScrollbarHeight();
    
    for (size_t i = 0; i < children.size(); ++i) {
        auto& child = children[i].box;
        ASSERT(!child.isOutOfFlowPositioned());
        mainAxisOffset -= mainAxisExtentForChild(child) + flowAwareMarginEndForChild(child);
        setFlowAwareLocationForChild(child, LayoutPoint(mainAxisOffset, crossAxisOffset + flowAwareMarginBeforeForChild(child)));
        mainAxisOffset -= flowAwareMarginStartForChild(child);
        mainAxisOffset -= justifyContentSpaceBetweenChildren(availableFreeSpace, distribution, children.size());
    }
}
    
static LayoutUnit initialAlignContentOffset(LayoutUnit availableFreeSpace, ContentPosition alignContent, ContentDistribution alignContentDistribution, unsigned numberOfLines)
{
    if (numberOfLines <= 1)
        return 0_lu;
    if (alignContent == ContentPosition::FlexEnd)
        return availableFreeSpace;
    if (alignContent == ContentPosition::Center)
        return availableFreeSpace / 2;
    if (alignContentDistribution == ContentDistribution::SpaceAround) {
        if (availableFreeSpace > 0 && numberOfLines)
            return availableFreeSpace / (2 * numberOfLines);
        if (availableFreeSpace < 0)
            return availableFreeSpace / 2;
    }
    if (alignContentDistribution == ContentDistribution::SpaceEvenly) {
        if (availableFreeSpace > 0)
            return availableFreeSpace / (numberOfLines + 1);
        // Fallback to 'center'
        return availableFreeSpace / 2;
    }
    return 0_lu;
}

static LayoutUnit alignContentSpaceBetweenChildren(LayoutUnit availableFreeSpace, ContentDistribution alignContentDistribution, unsigned numberOfLines)
{
    if (availableFreeSpace > 0 && numberOfLines > 1) {
        if (alignContentDistribution == ContentDistribution::SpaceBetween)
            return availableFreeSpace / (numberOfLines - 1);
        if (alignContentDistribution == ContentDistribution::SpaceAround || alignContentDistribution == ContentDistribution::Stretch)
            return availableFreeSpace / numberOfLines;
        if (alignContentDistribution == ContentDistribution::SpaceEvenly)
            return availableFreeSpace / (numberOfLines + 1);
    }
    return 0_lu;
}
    
void RenderFlexibleBox::alignFlexLines(Vector<LineContext>& lineContexts)
{
    ContentPosition position = style().resolvedAlignContentPosition(contentAlignmentNormalBehavior());
    ContentDistribution distribution = style().resolvedAlignContentDistribution(contentAlignmentNormalBehavior());
    
    // If we have a single line flexbox or a multiline line flexbox with only one
    // flex line, the line height is all the available space. For
    // flex-direction: row, this means we need to use the height, so we do this
    // after calling updateLogicalHeight.
    if (lineContexts.size() == 1) {
        lineContexts[0].crossAxisExtent = crossAxisContentExtent();
        return;
    }
    
    if (position == ContentPosition::FlexStart)
        return;
    
    LayoutUnit availableCrossAxisSpace = crossAxisContentExtent();
    for (size_t i = 0; i < lineContexts.size(); ++i)
        availableCrossAxisSpace -= lineContexts[i].crossAxisExtent;
    
    LayoutUnit lineOffset = initialAlignContentOffset(availableCrossAxisSpace, position, distribution, lineContexts.size());
    for (unsigned lineNumber = 0; lineNumber < lineContexts.size(); ++lineNumber) {
        LineContext& lineContext = lineContexts[lineNumber];
        lineContext.crossAxisOffset += lineOffset;
        for (size_t childNumber = 0; childNumber < lineContext.flexItems.size(); ++childNumber) {
            FlexItem& flexItem = lineContext.flexItems[childNumber];
            adjustAlignmentForChild(flexItem.box, lineOffset);
        }
        
        if (distribution == ContentDistribution::Stretch && availableCrossAxisSpace > 0)
            lineContexts[lineNumber].crossAxisExtent += availableCrossAxisSpace / static_cast<unsigned>(lineContexts.size());
        
        lineOffset += alignContentSpaceBetweenChildren(availableCrossAxisSpace, distribution, lineContexts.size());
    }
}
    
void RenderFlexibleBox::adjustAlignmentForChild(RenderBox& child, LayoutUnit delta)
{
    ASSERT(!child.isOutOfFlowPositioned());
    setFlowAwareLocationForChild(child, flowAwareLocationForChild(child) + LayoutSize(0_lu, delta));
}
    
void RenderFlexibleBox::alignChildren(const Vector<LineContext>& lineContexts)
{
    // Keep track of the space between the baseline edge and the after edge of
    // the box for each line.
    Vector<LayoutUnit> minMarginAfterBaselines;
    
    for (size_t lineNumber = 0; lineNumber < lineContexts.size(); ++lineNumber) {
        const LineContext& lineContext = lineContexts[lineNumber];
        
        LayoutUnit minMarginAfterBaseline = LayoutUnit::max();
        LayoutUnit lineCrossAxisExtent = lineContext.crossAxisExtent;
        LayoutUnit maxAscent = lineContext.maxAscent;
        
        for (size_t childNumber = 0; childNumber < lineContext.flexItems.size(); ++childNumber) {
            const auto& flexItem = lineContext.flexItems[childNumber];
            ASSERT(!flexItem.box.isOutOfFlowPositioned());
            
            if (updateAutoMarginsInCrossAxis(flexItem.box, std::max(0_lu, availableAlignmentSpaceForChild(lineCrossAxisExtent, flexItem.box))))
                continue;
            
            ItemPosition position = alignmentForChild(flexItem.box);
            if (position == ItemPosition::Stretch)
                applyStretchAlignmentToChild(flexItem.box, lineCrossAxisExtent);
            LayoutUnit availableSpace =
            availableAlignmentSpaceForChild(lineCrossAxisExtent, flexItem.box);
            LayoutUnit offset = alignmentOffset(availableSpace, position, marginBoxAscentForChild(flexItem.box), maxAscent, style().flexWrap() == FlexWrap::Reverse);
            adjustAlignmentForChild(flexItem.box, offset);
            if (position == ItemPosition::Baseline && style().flexWrap() == FlexWrap::Reverse)
                minMarginAfterBaseline = std::min(minMarginAfterBaseline, availableAlignmentSpaceForChild(lineCrossAxisExtent, flexItem.box) - offset);
        }

        minMarginAfterBaselines.append(minMarginAfterBaseline);
    }
    
    if (style().flexWrap() != FlexWrap::Reverse)
        return;
    
    // wrap-reverse flips the cross axis start and end. For baseline alignment,
    // this means we need to align the after edge of baseline elements with the
    // after edge of the flex line.
    for (size_t lineNumber = 0; lineNumber < lineContexts.size(); ++lineNumber) {
        const LineContext& lineContext = lineContexts[lineNumber];
        LayoutUnit minMarginAfterBaseline = minMarginAfterBaselines[lineNumber];
        for (size_t childNumber = 0; childNumber < lineContext.flexItems.size(); ++childNumber) {
            const auto& flexItem = lineContext.flexItems[childNumber];
            if (alignmentForChild(flexItem.box) == ItemPosition::Baseline && !hasAutoMarginsInCrossAxis(flexItem.box) && minMarginAfterBaseline)
                adjustAlignmentForChild(flexItem.box, minMarginAfterBaseline);
        }
    }
}

void RenderFlexibleBox::applyStretchAlignmentToChild(RenderBox& child, LayoutUnit lineCrossAxisExtent)
{
    if (!hasOrthogonalFlow(child) && child.style().logicalHeight().isAuto()) {
        LayoutUnit stretchedLogicalHeight = std::max(child.borderAndPaddingLogicalHeight(),
        lineCrossAxisExtent - crossAxisMarginExtentForChild(child));
        ASSERT(!child.needsLayout());
        LayoutUnit desiredLogicalHeight = child.constrainLogicalHeightByMinMax(stretchedLogicalHeight, cachedChildIntrinsicContentLogicalHeight(child));
        
        // FIXME: Can avoid laying out here in some cases. See https://webkit.org/b/87905.
        bool childNeedsRelayout = desiredLogicalHeight != child.logicalHeight();
        if (child.isRenderBlock() && downcast<RenderBlock>(child).hasPercentHeightDescendants() && m_relaidOutChildren.contains(&child)) {
            // Have to force another relayout even though the child is sized
            // correctly, because its descendants are not sized correctly yet. Our
            // previous layout of the child was done without an override height set.
            // So, redo it here.
            childNeedsRelayout = true;
        }
        if (childNeedsRelayout || !child.hasOverrideContentLogicalHeight())
            child.setOverrideContentLogicalHeight(desiredLogicalHeight - child.borderAndPaddingLogicalHeight());
        if (childNeedsRelayout) {
            SetForScope<bool> resetChildLogicalHeight(m_shouldResetChildLogicalHeightBeforeLayout, true);
            // We cache the child's intrinsic content logical height to avoid it being
            // reset to the stretched height.
            // FIXME: This is fragile. RenderBoxes should be smart enough to
            // determine their intrinsic content logical height correctly even when
            // there's an overrideHeight.
            LayoutUnit childIntrinsicContentLogicalHeight = cachedChildIntrinsicContentLogicalHeight(child);
            child.setChildNeedsLayout(MarkOnlyThis);
            
            // Don't use layoutChildIfNeeded to avoid setting cross axis cached size twice.
            child.layoutIfNeeded();

            setCachedChildIntrinsicContentLogicalHeight(child, childIntrinsicContentLogicalHeight);
        }
    } else if (hasOrthogonalFlow(child) && child.style().logicalWidth().isAuto()) {
        LayoutUnit childWidth = std::max(0_lu, lineCrossAxisExtent - crossAxisMarginExtentForChild(child));
        childWidth = child.constrainLogicalWidthInFragmentByMinMax(childWidth, crossAxisContentExtent(), *this, nullptr);
        
        if (childWidth != child.logicalWidth()) {
            child.setOverrideContentLogicalWidth(childWidth - child.borderAndPaddingLogicalWidth());
            child.setChildNeedsLayout(MarkOnlyThis);
            child.layoutIfNeeded();
        }
    }
}

void RenderFlexibleBox::flipForRightToLeftColumn(const Vector<LineContext>& lineContexts)
{
    if (style().isLeftToRightDirection() || !isColumnFlow())
        return;
    
    LayoutUnit crossExtent = crossAxisExtent();
    for (size_t lineNumber = 0; lineNumber < lineContexts.size(); ++lineNumber) {
        const LineContext& lineContext = lineContexts[lineNumber];
        for (size_t childNumber = 0; childNumber < lineContext.flexItems.size(); ++childNumber) {
            const auto& flexItem = lineContext.flexItems[childNumber];
            ASSERT(!flexItem.box.isOutOfFlowPositioned());
            
            LayoutPoint location = flowAwareLocationForChild(flexItem.box);
            // For vertical flows, setFlowAwareLocationForChild will transpose x and
            // y, so using the y axis for a column cross axis extent is correct.
            location.setY(crossExtent - crossAxisExtentForChild(flexItem.box) - location.y());
            if (!isHorizontalWritingMode())
                location.move(LayoutSize(0, -horizontalScrollbarHeight()));
            setFlowAwareLocationForChild(flexItem.box, location);
        }
    }
}

void RenderFlexibleBox::flipForWrapReverse(const Vector<LineContext>& lineContexts, LayoutUnit crossAxisStartEdge)
{
    LayoutUnit contentExtent = crossAxisContentExtent();
    for (size_t lineNumber = 0; lineNumber < lineContexts.size(); ++lineNumber) {
        const LineContext& lineContext = lineContexts[lineNumber];
        for (size_t childNumber = 0; childNumber < lineContext.flexItems.size(); ++childNumber) {
            const auto& flexItem = lineContext.flexItems[childNumber];
            LayoutUnit lineCrossAxisExtent = lineContexts[lineNumber].crossAxisExtent;
            LayoutUnit originalOffset = lineContexts[lineNumber].crossAxisOffset - crossAxisStartEdge;
            LayoutUnit newOffset = contentExtent - originalOffset - lineCrossAxisExtent;
            adjustAlignmentForChild(flexItem.box, newOffset - originalOffset);
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
