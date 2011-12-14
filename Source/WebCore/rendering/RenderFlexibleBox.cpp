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
#include "RenderView.h"

namespace WebCore {

// Normally, -1 and 0 are not valid in a HashSet, but these are relatively likely flex-order values. Instead,
// we make the two smallest int values invalid flex-order values (in the css parser code we clamp them to
// int min + 2).
struct FlexOrderHashTraits : WTF::GenericHashTraits<int> {
    static const bool emptyValueIsZero = false;
    static int emptyValue() { return std::numeric_limits<int>::min(); }
    static void constructDeletedValue(int& slot) { slot = std::numeric_limits<int>::min() + 1; }
    static bool isDeletedValue(int value) { return value == std::numeric_limits<int>::min() + 1; }
};

typedef HashSet<int, DefaultHash<int>::Hash, FlexOrderHashTraits> FlexOrderHashSet;

class RenderFlexibleBox::TreeOrderIterator {
public:
    explicit TreeOrderIterator(RenderFlexibleBox* flexibleBox)
        : m_flexibleBox(flexibleBox)
        , m_currentChild(0)
    {
    }

    RenderBox* first()
    {
        reset();
        return next();
    }

    RenderBox* next()
    {
        m_currentChild = m_currentChild ? m_currentChild->nextSiblingBox() : m_flexibleBox->firstChildBox();

        if (m_currentChild)
            m_flexOrderValues.add(m_currentChild->style()->flexOrder());

        return m_currentChild;
    }

    void reset()
    {
        m_currentChild = 0;
    }

    const FlexOrderHashSet& flexOrderValues()
    {
        return m_flexOrderValues;
    }

private:
    RenderFlexibleBox* m_flexibleBox;
    RenderBox* m_currentChild;
    FlexOrderHashSet m_flexOrderValues;
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
    }

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

void RenderFlexibleBox::layoutBlock(bool relayoutChildren, int, BlockLayoutPass)
{
    ASSERT(needsLayout());

    if (!relayoutChildren && simplifiedLayout())
        return;

    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());
    LayoutStateMaintainer statePusher(view(), this, IntSize(x(), y()), hasTransform() || hasReflection() || style()->isFlippedBlocksWritingMode());

    IntSize previousSize = size();

    setLogicalHeight(0);
    // We need to call both of these because we grab both crossAxisExtent and mainAxisExtent in layoutFlexItems.
    computeLogicalWidth();
    computeLogicalHeight();

    m_overflow.clear();

    layoutFlexItems(relayoutChildren);

    LayoutUnit oldClientAfterEdge = clientLogicalBottom();
    computeLogicalHeight();

    if (size() != previousSize)
        relayoutChildren = true;

    layoutPositionedObjects(relayoutChildren || isRoot());

    // FIXME: css3/flexbox/repaint-rtl-column.html seems to repaint more overflow than it needs to.
    computeOverflow(oldClientAfterEdge);
    statePusher.pop();

    updateLayerTransform();

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

LayoutUnit RenderFlexibleBox::crossAxisBorderAndPaddingExtent() const
{
    return isHorizontalFlow() ? borderAndPaddingHeight() : borderAndPaddingWidth();
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
    return isHorizontalFlow() ? child->marginTop() + child->marginBottom() : child->marginLeft() + child->marginRight();
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

LayoutUnit RenderFlexibleBox::preferredMainAxisContentExtentForFlexItem(RenderBox* child) const
{
    Length mainAxisLength = mainAxisLengthForChild(child);
    if (mainAxisLength.isAuto()) {
        LayoutUnit mainAxisExtent = hasOrthogonalFlow(child) ? child->logicalHeight() : child->maxPreferredLogicalWidth();
        return mainAxisExtent - mainAxisBorderAndPaddingExtentForChild(child) - mainAxisScrollbarExtentForChild(child);
    }
    return mainAxisLength.calcMinValue(mainAxisContentExtent());
}

void RenderFlexibleBox::layoutFlexItems(bool relayoutChildren)
{
    LayoutUnit preferredMainAxisExtent;
    float totalPositiveFlexibility;
    float totalNegativeFlexibility;
    TreeOrderIterator treeIterator(this);

    computePreferredMainAxisExtent(relayoutChildren, treeIterator, preferredMainAxisExtent, totalPositiveFlexibility, totalNegativeFlexibility);
    LayoutUnit availableFreeSpace = mainAxisContentExtent() - preferredMainAxisExtent;

    FlexOrderIterator flexIterator(this, treeIterator.flexOrderValues());
    InflexibleFlexItemSize inflexibleItems;
    WTF::Vector<LayoutUnit> childSizes;
    while (!runFreeSpaceAllocationAlgorithm(flexIterator, availableFreeSpace, totalPositiveFlexibility, totalNegativeFlexibility, inflexibleItems, childSizes)) {
        ASSERT(totalPositiveFlexibility >= 0 && totalNegativeFlexibility >= 0);
        ASSERT(inflexibleItems.size() > 0);
    }

    layoutAndPlaceChildren(flexIterator, childSizes, availableFreeSpace, totalPositiveFlexibility);
}

float RenderFlexibleBox::positiveFlexForChild(RenderBox* child) const
{
    return isHorizontalFlow() ? child->style()->flexboxWidthPositiveFlex() : child->style()->flexboxHeightPositiveFlex();
}

float RenderFlexibleBox::negativeFlexForChild(RenderBox* child) const
{
    return isHorizontalFlow() ? child->style()->flexboxWidthNegativeFlex() : child->style()->flexboxHeightNegativeFlex();
}

LayoutUnit RenderFlexibleBox::availableAlignmentSpaceForChild(RenderBox* child)
{
    LayoutUnit crossContentExtent = crossAxisContentExtent();
    LayoutUnit childCrossExtent = crossAxisMarginExtentForChild(child) + crossAxisExtentForChild(child);
    return crossContentExtent - childCrossExtent;
}

LayoutUnit RenderFlexibleBox::marginBoxAscent(RenderBox* child)
{
    LayoutUnit ascent = child->firstLineBoxBaseline();
    if (ascent == -1)
        ascent = crossAxisExtentForChild(child) + flowAwareMarginAfterForChild(child);
    return ascent + flowAwareMarginBeforeForChild(child);
}

void RenderFlexibleBox::computePreferredMainAxisExtent(bool relayoutChildren, TreeOrderIterator& iterator, LayoutUnit& preferredMainAxisExtent, float& totalPositiveFlexibility, float& totalNegativeFlexibility)
{
    preferredMainAxisExtent = 0;
    totalPositiveFlexibility = totalNegativeFlexibility = 0;

    LayoutUnit flexboxAvailableContentExtent = mainAxisContentExtent();
    for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
        if (mainAxisLengthForChild(child).isAuto()) {
            child->clearOverrideSize();
            if (!relayoutChildren)
                child->setChildNeedsLayout(true);
            child->layoutIfNeeded();
        }

        // We set the margins because we want to make sure 'auto' has a margin
        // of 0 and because if we're not auto sizing, we don't do a layout that
        // computes the start/end margins.
        if (isHorizontalFlow()) {
            child->setMarginLeft(child->style()->marginLeft().calcMinValue(flexboxAvailableContentExtent));
            child->setMarginRight(child->style()->marginRight().calcMinValue(flexboxAvailableContentExtent));
            preferredMainAxisExtent += child->marginLeft() + child->marginRight();
        } else {
            child->setMarginTop(child->style()->marginTop().calcMinValue(flexboxAvailableContentExtent));
            child->setMarginBottom(child->style()->marginBottom().calcMinValue(flexboxAvailableContentExtent));
            preferredMainAxisExtent += child->marginTop() + child->marginBottom();
        }

        preferredMainAxisExtent += mainAxisBorderAndPaddingExtentForChild(child);
        preferredMainAxisExtent += preferredMainAxisContentExtentForFlexItem(child);

        totalPositiveFlexibility += positiveFlexForChild(child);
        totalNegativeFlexibility += negativeFlexForChild(child);
    }
}

// Returns true if we successfully ran the algorithm and sized the flex items.
bool RenderFlexibleBox::runFreeSpaceAllocationAlgorithm(FlexOrderIterator& iterator, LayoutUnit& availableFreeSpace, float& totalPositiveFlexibility, float& totalNegativeFlexibility, InflexibleFlexItemSize& inflexibleItems, WTF::Vector<LayoutUnit>& childSizes)
{
    childSizes.clear();

    LayoutUnit flexboxAvailableContentExtent = mainAxisContentExtent();
    for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
        LayoutUnit childPreferredSize;
        if (inflexibleItems.contains(child))
            childPreferredSize = inflexibleItems.get(child);
        else {
            childPreferredSize = preferredMainAxisContentExtentForFlexItem(child);
            if (availableFreeSpace > 0 && totalPositiveFlexibility > 0) {
                childPreferredSize += lroundf(availableFreeSpace * positiveFlexForChild(child) / totalPositiveFlexibility);

                Length childLogicalMaxWidth = isHorizontalFlow() ? child->style()->maxWidth() : child->style()->maxHeight();
                if (!childLogicalMaxWidth.isUndefined() && childLogicalMaxWidth.isSpecified() && childPreferredSize > childLogicalMaxWidth.calcValue(flexboxAvailableContentExtent)) {
                    childPreferredSize = childLogicalMaxWidth.calcValue(flexboxAvailableContentExtent);
                    availableFreeSpace -= childPreferredSize - preferredMainAxisContentExtentForFlexItem(child);
                    totalPositiveFlexibility -= positiveFlexForChild(child);

                    inflexibleItems.set(child, childPreferredSize);
                    return false;
                }
            } else if (availableFreeSpace < 0 && totalNegativeFlexibility > 0) {
                childPreferredSize += lroundf(availableFreeSpace * negativeFlexForChild(child) / totalNegativeFlexibility);

                Length childLogicalMinWidth = isHorizontalFlow() ? child->style()->minWidth() : child->style()->minHeight();
                if (!childLogicalMinWidth.isUndefined() && childLogicalMinWidth.isSpecified() && childPreferredSize < childLogicalMinWidth.calcValue(flexboxAvailableContentExtent)) {
                    childPreferredSize = childLogicalMinWidth.calcValue(flexboxAvailableContentExtent);
                    availableFreeSpace += preferredMainAxisContentExtentForFlexItem(child) - childPreferredSize;
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

static bool hasPackingSpace(LayoutUnit availableFreeSpace, float totalPositiveFlexibility)
{
    return availableFreeSpace > 0 && !totalPositiveFlexibility;
}

static LayoutUnit initialPackingOffset(LayoutUnit availableFreeSpace, float totalPositiveFlexibility, EFlexPack flexPack)
{
    if (hasPackingSpace(availableFreeSpace, totalPositiveFlexibility)) {
        if (flexPack == PackEnd)
            return availableFreeSpace;
        if (flexPack == PackCenter)
            return availableFreeSpace / 2;
    }
    return 0;
}

static LayoutUnit packingSpaceBetweenChildren(LayoutUnit availableFreeSpace, float totalPositiveFlexibility, EFlexPack flexPack, size_t numberOfChildren)
{
    if (hasPackingSpace(availableFreeSpace, totalPositiveFlexibility) && flexPack == PackJustify && numberOfChildren > 1)
        return availableFreeSpace / (numberOfChildren - 1);
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

void RenderFlexibleBox::layoutAndPlaceChildren(FlexOrderIterator& iterator, const WTF::Vector<LayoutUnit>& childSizes, LayoutUnit availableFreeSpace, float totalPositiveFlexibility)
{
    LayoutUnit mainAxisOffset = flowAwareBorderStart() + flowAwarePaddingStart();
    mainAxisOffset += initialPackingOffset(availableFreeSpace, totalPositiveFlexibility, style()->flexPack());

    LayoutUnit crossAxisOffset = flowAwareBorderBefore() + flowAwarePaddingBefore();
    LayoutUnit totalMainExtent = mainAxisExtent();
    LayoutUnit maxAscent = 0, maxDescent = 0; // Used when flex-align: baseline.
    bool shouldFlipMainAxis = !isColumnFlow() && !isLeftToRightFlow();
    size_t i = 0;
    for (RenderBox* child = iterator.first(); child; child = iterator.next(), ++i) {
        LayoutUnit childPreferredSize = childSizes[i] + mainAxisBorderAndPaddingExtentForChild(child);
        setLogicalOverrideSize(child, childPreferredSize);
        child->setChildNeedsLayout(true);
        child->layoutIfNeeded();

        if (child->style()->flexAlign() == AlignBaseline) {
            LayoutUnit ascent = marginBoxAscent(child);
            LayoutUnit descent = (crossAxisMarginExtentForChild(child) + crossAxisExtentForChild(child)) - ascent;

            maxAscent = std::max(maxAscent, ascent);
            maxDescent = std::max(maxDescent, descent);

            // FIXME: add flowAwareScrollbarLogicalHeight.
            if (crossAxisLength().isAuto())
                setCrossAxisExtent(std::max(crossAxisExtent(), crossAxisBorderAndPaddingExtent() + crossAxisMarginExtentForChild(child) + maxAscent + maxDescent + scrollbarLogicalHeight()));
        } else if (crossAxisLength().isAuto())
            setCrossAxisExtent(std::max(crossAxisExtent(), crossAxisBorderAndPaddingExtent() + crossAxisMarginExtentForChild(child) + crossAxisExtentForChild(child) + scrollbarLogicalHeight()));

        mainAxisOffset += flowAwareMarginStartForChild(child);

        LayoutUnit childMainExtent = mainAxisExtentForChild(child);
        IntPoint childLocation(shouldFlipMainAxis ? totalMainExtent - mainAxisOffset - childMainExtent : mainAxisOffset,
            crossAxisOffset + flowAwareMarginBeforeForChild(child));

        // FIXME: Supporting layout deltas.
        setFlowAwareLocationForChild(child, childLocation);
        mainAxisOffset += childMainExtent + flowAwareMarginEndForChild(child);

        mainAxisOffset += packingSpaceBetweenChildren(availableFreeSpace, totalPositiveFlexibility, style()->flexPack(), childSizes.size());

        if (isColumnFlow())
            setLogicalHeight(mainAxisOffset);
    }

    if (style()->flexDirection() == FlowColumnReverse) {
        // We have to do an extra pass for column-reverse to reposition the flex items since the start depends
        // on the height of the flexbox, which we only know after we've positioned all the flex items.
        computeLogicalHeight();
        layoutColumnReverse(iterator, childSizes, availableFreeSpace, totalPositiveFlexibility);
    }

    alignChildren(iterator, maxAscent);
}

void RenderFlexibleBox::layoutColumnReverse(FlexOrderIterator& iterator, const WTF::Vector<LayoutUnit>& childSizes, LayoutUnit availableFreeSpace, float totalPositiveFlexibility)
{
    // This is similar to the logic in layoutAndPlaceChildren, except we place the children
    // starting from the end of the flexbox. We also don't need to layout anything since we're
    // just moving the children to a new position.
    LayoutUnit mainAxisOffset = logicalHeight() - flowAwareBorderEnd() - flowAwarePaddingEnd();
    mainAxisOffset -= initialPackingOffset(availableFreeSpace, totalPositiveFlexibility, style()->flexPack());

    LayoutUnit crossAxisOffset = flowAwareBorderBefore() + flowAwarePaddingBefore();
    size_t i = 0;
    for (RenderBox* child = iterator.first(); child; child = iterator.next(), ++i) {
        mainAxisOffset -= mainAxisExtentForChild(child) + flowAwareMarginEndForChild(child);

        LayoutRect oldRect = child->frameRect();
        setFlowAwareLocationForChild(child, IntPoint(mainAxisOffset, crossAxisOffset + flowAwareMarginBeforeForChild(child)));
        if (!selfNeedsLayout() && child->checkForRepaintDuringLayout())
            child->repaintDuringLayoutIfMoved(oldRect);

        mainAxisOffset -= flowAwareMarginStartForChild(child);
        mainAxisOffset -= packingSpaceBetweenChildren(availableFreeSpace, totalPositiveFlexibility, style()->flexPack(), childSizes.size());
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

void RenderFlexibleBox::alignChildren(FlexOrderIterator& iterator, LayoutUnit maxAscent)
{
    LayoutUnit crossExtent = crossAxisExtent();

    for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
        // direction:rtl + flex-direction:column means the cross-axis direction is flipped.
        if (!style()->isLeftToRightDirection() && isColumnFlow()) {
            LayoutPoint location = flowAwareLocationForChild(child);
            location.setY(crossExtent - crossAxisExtentForChild(child) - location.y());
            setFlowAwareLocationForChild(child, location);
        }

        // FIXME: Make sure this does the right thing with column flows.
        switch (child->style()->flexAlign()) {
        case AlignStretch: {
            if (!isColumnFlow() && child->style()->logicalHeight().isAuto()) {
                LayoutUnit stretchedLogicalHeight = child->logicalHeight() + RenderFlexibleBox::availableAlignmentSpaceForChild(child);
                child->setLogicalHeight(stretchedLogicalHeight);
                child->computeLogicalHeight();
                // FIXME: We need to relayout if the height changed.
            }
            break;
        }
        case AlignStart:
            break;
        case AlignEnd:
            adjustAlignmentForChild(child, RenderFlexibleBox::availableAlignmentSpaceForChild(child));
            break;
        case AlignCenter:
            adjustAlignmentForChild(child, RenderFlexibleBox::availableAlignmentSpaceForChild(child) / 2);
            break;
        case AlignBaseline: {
            LayoutUnit ascent = marginBoxAscent(child);
            adjustAlignmentForChild(child, maxAscent - ascent);
            break;
        }
        }
    }
}

}
