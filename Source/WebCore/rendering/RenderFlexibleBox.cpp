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

#if ENABLE(CSS3_FLEXBOX)

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
        RenderObject* child = m_currentChild ? m_currentChild->nextSibling() : m_flexibleBox->firstChild();
        // FIXME: Inline nodes (like <img> or <input>) should also be treated as boxes.
        while (child && !child->isBox())
            child = child->nextSibling();

        if (child)
            m_flexOrderValues.add(child->style()->flexOrder());

        m_currentChild = toRenderBox(child);
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
        RenderObject* child = m_currentChild;
        do {
            if (!child) {
                if (m_orderValuesIterator == m_orderValues.end())
                    return 0;
                if (m_orderValuesIterator) {
                    ++m_orderValuesIterator;
                    if (m_orderValuesIterator == m_orderValues.end())
                        return 0;
                } else
                    m_orderValuesIterator = m_orderValues.begin();

                child = m_flexibleBox->firstChild();
            } else
                child = child->nextSibling();
        } while (!child || !child->isBox() || child->style()->flexOrder() != *m_orderValuesIterator);

        m_currentChild = toRenderBox(child);
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

    IntSize previousSize = size();

    computeLogicalWidth();
    computeLogicalHeight();

    m_overflow.clear();

    layoutInlineDirection(relayoutChildren);

    computeLogicalHeight();

    if (size() != previousSize)
        relayoutChildren = true;

    layoutPositionedObjects(relayoutChildren || isRoot());

    updateLayerTransform();

    setNeedsLayout(false);
}

bool RenderFlexibleBox::hasOrthogonalFlow(RenderBox* child) const
{
    // FIXME: Is the child->isHorizontalWritingMode() check correct if the child is a flexbox?
    // Should it be using child->isHorizontalFlow in that case?
    // Or do we only care about the parent's writing mode?
    return isHorizontalFlow() != child->isHorizontalWritingMode();
}

bool RenderFlexibleBox::isHorizontalFlow() const
{
    // FIXME: Take flex-flow value into account.
    return isHorizontalWritingMode();
}

bool RenderFlexibleBox::isLeftToRightFlow() const
{
    // FIXME: Take flex-flow value into account.
    return style()->isLeftToRightDirection();
}

// FIXME: Make all these flow aware methods actually be flow aware.

void RenderFlexibleBox::setFlowAwareLogicalHeight(LayoutUnit size)
{
    setLogicalHeight(size);
}

LayoutUnit RenderFlexibleBox::flowAwareLogicalHeightForChild(RenderBox* child)
{
    return logicalHeightForChild(child);
}

LayoutUnit RenderFlexibleBox::flowAwareLogicalWidthForChild(RenderBox* child)
{
    return logicalWidthForChild(child);
}

LayoutUnit RenderFlexibleBox::flowAwareLogicalHeight() const
{
    return logicalHeight();
}

LayoutUnit RenderFlexibleBox::flowAwareContentLogicalWidth() const
{
    return contentLogicalWidth();
}

LayoutUnit RenderFlexibleBox::flowAwareAvailableLogicalWidth() const
{
    return availableLogicalWidth();
}

LayoutUnit RenderFlexibleBox::flowAwareBorderStart() const
{
    return borderStart();
}

LayoutUnit RenderFlexibleBox::flowAwareBorderBefore() const
{
    return borderBefore();
}

LayoutUnit RenderFlexibleBox::flowAwareBorderAfter() const
{
    return borderAfter();
}

LayoutUnit RenderFlexibleBox::flowAwarePaddingStart() const
{
    return paddingStart();
}

LayoutUnit RenderFlexibleBox::flowAwarePaddingBefore() const
{
    return paddingBefore();
}

LayoutUnit RenderFlexibleBox::flowAwarePaddingAfter() const
{
    return paddingAfter();
}

LayoutUnit RenderFlexibleBox::flowAwareMarginStartForChild(RenderBox* child) const
{
    return marginStartForChild(child);
}

LayoutUnit RenderFlexibleBox::flowAwareMarginBeforeForChild(RenderBox* child) const
{
    return marginBeforeForChild(child);
}

LayoutUnit RenderFlexibleBox::flowAwareMarginAfterForChild(RenderBox* child) const
{
    return marginAfterForChild(child);
}

void RenderFlexibleBox::setFlowAwareMarginStartForChild(RenderBox* child, LayoutUnit margin)
{
    setMarginStartForChild(child, margin);
}

void RenderFlexibleBox::setFlowAwareMarginEndForChild(RenderBox* child, LayoutUnit margin)
{
    setMarginEndForChild(child, margin);
}

void RenderFlexibleBox::setFlowAwareLogicalLocationForChild(RenderBox* child, const LayoutPoint& location)
{
    if (isHorizontalFlow())
        child->setLocation(location);
    else
        child->setLocation(location.transposedPoint());
}

LayoutUnit RenderFlexibleBox::logicalBorderAndPaddingWidthForChild(RenderBox* child) const
{
    return isHorizontalFlow() ? child->borderAndPaddingWidth() : child->borderAndPaddingHeight();
}

LayoutUnit RenderFlexibleBox::logicalScrollbarHeightForChild(RenderBox* child) const
{
    return isHorizontalFlow() ? child->verticalScrollbarWidth() : child->horizontalScrollbarHeight();
}

Length RenderFlexibleBox::marginStartStyleForChild(RenderBox* child) const
{
    if (isHorizontalFlow())
        return isLeftToRightFlow() ? child->style()->marginLeft() : child->style()->marginRight();
    return isLeftToRightFlow() ? child->style()->marginTop() : child->style()->marginBottom();
}

Length RenderFlexibleBox::marginEndStyleForChild(RenderBox* child) const
{
    if (isHorizontalFlow())
        return isLeftToRightFlow() ? child->style()->marginRight() : child->style()->marginLeft();
    return isLeftToRightFlow() ? child->style()->marginBottom() : child->style()->marginTop();
}

LayoutUnit RenderFlexibleBox::preferredLogicalContentWidthForFlexItem(RenderBox* child) const
{
    Length width = isHorizontalFlow() ? child->style()->width() : child->style()->height();
    if (width.isAuto()) {
        LayoutUnit logicalWidth = hasOrthogonalFlow(child) ? child->logicalHeight() : child->maxPreferredLogicalWidth();
        return logicalWidth - logicalBorderAndPaddingWidthForChild(child) - logicalScrollbarHeightForChild(child);
    }
    return isHorizontalFlow() ? child->contentWidth() : child->contentHeight();
}

void RenderFlexibleBox::layoutInlineDirection(bool relayoutChildren)
{
    LayoutUnit preferredLogicalWidth;
    float totalPositiveFlexibility;
    float totalNegativeFlexibility;
    TreeOrderIterator treeIterator(this);

    computePreferredLogicalWidth(relayoutChildren, treeIterator, preferredLogicalWidth, totalPositiveFlexibility, totalNegativeFlexibility);
    LayoutUnit availableFreeSpace = flowAwareContentLogicalWidth() - preferredLogicalWidth;

    FlexOrderIterator flexIterator(this, treeIterator.flexOrderValues());
    InflexibleFlexItemSize inflexibleItems;
    WTF::Vector<LayoutUnit> childSizes;
    while (!runFreeSpaceAllocationAlgorithmInlineDirection(flexIterator, availableFreeSpace, totalPositiveFlexibility, totalNegativeFlexibility, inflexibleItems, childSizes)) {
        ASSERT(totalPositiveFlexibility >= 0 && totalNegativeFlexibility >= 0);
        ASSERT(inflexibleItems.size() > 0);
    }

    layoutAndPlaceChildrenInlineDirection(flexIterator, childSizes, availableFreeSpace, totalPositiveFlexibility);

    // FIXME: Handle distribution of cross-axis space (third distribution round).
}

float RenderFlexibleBox::logicalPositiveFlexForChild(RenderBox* child) const
{
    return isHorizontalFlow() ? child->style()->flexboxWidthPositiveFlex() : child->style()->flexboxHeightPositiveFlex();
}

float RenderFlexibleBox::logicalNegativeFlexForChild(RenderBox* child) const
{
    return isHorizontalFlow() ? child->style()->flexboxWidthNegativeFlex() : child->style()->flexboxHeightNegativeFlex();
}

void RenderFlexibleBox::computePreferredLogicalWidth(bool relayoutChildren, TreeOrderIterator& iterator, LayoutUnit& preferredLogicalWidth, float& totalPositiveFlexibility, float& totalNegativeFlexibility)
{
    preferredLogicalWidth = 0;
    totalPositiveFlexibility = totalNegativeFlexibility = 0;

    LayoutUnit flexboxAvailableLogicalWidth = flowAwareAvailableLogicalWidth();
    for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
        // We always have to lay out flexible objects again, since the flex distribution
        // may have changed, and we need to reallocate space.
        child->clearOverrideSize();
        if (!relayoutChildren)
            child->setChildNeedsLayout(true);
        child->layoutIfNeeded();

        // We can't just use marginStartForChild, et. al. because "auto" needs to be treated as 0.
        if (isHorizontalFlow()) {
            preferredLogicalWidth += child->style()->marginLeft().calcMinValue(flexboxAvailableLogicalWidth);
            preferredLogicalWidth += child->style()->marginRight().calcMinValue(flexboxAvailableLogicalWidth);
        } else {
            preferredLogicalWidth += child->style()->marginTop().calcMinValue(flexboxAvailableLogicalWidth);
            preferredLogicalWidth += child->style()->marginBottom().calcMinValue(flexboxAvailableLogicalWidth);
        }

        if (marginStartStyleForChild(child).isAuto())
            totalPositiveFlexibility += 1;
        if (marginEndStyleForChild(child).isAuto())
            totalPositiveFlexibility += 1;

        preferredLogicalWidth += logicalBorderAndPaddingWidthForChild(child);
        preferredLogicalWidth += preferredLogicalContentWidthForFlexItem(child);

        totalPositiveFlexibility += logicalPositiveFlexForChild(child);
        totalNegativeFlexibility += logicalNegativeFlexForChild(child);
    }
}

// Returns true if we successfully ran the algorithm and sized the flex items.
bool RenderFlexibleBox::runFreeSpaceAllocationAlgorithmInlineDirection(FlexOrderIterator& iterator, LayoutUnit& availableFreeSpace, float& totalPositiveFlexibility, float& totalNegativeFlexibility, InflexibleFlexItemSize& inflexibleItems, WTF::Vector<LayoutUnit>& childSizes)
{
    childSizes.clear();

    LayoutUnit flexboxAvailableLogicalWidth = flowAwareAvailableLogicalWidth();
    for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
        LayoutUnit childPreferredSize;
        if (inflexibleItems.contains(child))
            childPreferredSize = inflexibleItems.get(child);
        else {
            childPreferredSize = preferredLogicalContentWidthForFlexItem(child);
            if (availableFreeSpace > 0 && totalPositiveFlexibility > 0) {
                childPreferredSize += lroundf(availableFreeSpace * logicalPositiveFlexForChild(child) / totalPositiveFlexibility);

                Length childLogicalMaxWidth = isHorizontalFlow() ? child->style()->maxWidth() : child->style()->maxHeight();
                if (!childLogicalMaxWidth.isUndefined() && childLogicalMaxWidth.isSpecified() && childPreferredSize > childLogicalMaxWidth.calcValue(flexboxAvailableLogicalWidth)) {
                    childPreferredSize = childLogicalMaxWidth.calcValue(flexboxAvailableLogicalWidth);
                    availableFreeSpace -= childPreferredSize - preferredLogicalContentWidthForFlexItem(child);
                    totalPositiveFlexibility -= logicalPositiveFlexForChild(child);

                    inflexibleItems.set(child, childPreferredSize);
                    return false;
                }
            } else if (availableFreeSpace < 0 && totalNegativeFlexibility > 0) {
                childPreferredSize += lroundf(availableFreeSpace * logicalNegativeFlexForChild(child) / totalNegativeFlexibility);

                Length childLogicalMinWidth = isHorizontalFlow() ? child->style()->minWidth() : child->style()->minHeight();
                if (!childLogicalMinWidth.isUndefined() && childLogicalMinWidth.isSpecified() && childPreferredSize < childLogicalMinWidth.calcValue(flexboxAvailableLogicalWidth)) {
                    childPreferredSize = childLogicalMinWidth.calcValue(flexboxAvailableLogicalWidth);
                    availableFreeSpace += preferredLogicalContentWidthForFlexItem(child) - childPreferredSize;
                    totalNegativeFlexibility -= logicalNegativeFlexForChild(child);

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

void RenderFlexibleBox::setLogicalOverrideSize(RenderBox* child, LayoutUnit childPreferredSize)
{
    // FIXME: Rename setOverrideWidth/setOverrideHeight to setOverrideLogicalWidth/setOverrideLogicalHeight.
    if (hasOrthogonalFlow(child))
        child->setOverrideHeight(childPreferredSize);
    else
        child->setOverrideWidth(childPreferredSize);
}

void RenderFlexibleBox::layoutAndPlaceChildrenInlineDirection(FlexOrderIterator& iterator, const WTF::Vector<LayoutUnit>& childSizes, LayoutUnit availableFreeSpace, float totalPositiveFlexibility)
{
    LayoutUnit startEdge = flowAwareBorderStart() + flowAwarePaddingStart();

    if (hasPackingSpace(availableFreeSpace, totalPositiveFlexibility)) {
        if (style()->flexPack() == PackEnd)
            startEdge += availableFreeSpace;
        else if (style()->flexPack() == PackCenter)
            startEdge += availableFreeSpace / 2;
    }

    LayoutUnit logicalTop = flowAwareBorderBefore() + flowAwarePaddingBefore();
    LayoutUnit totalAvailableLogicalWidth = flowAwareAvailableLogicalWidth();
    setFlowAwareLogicalHeight(0);
    size_t i = 0;
    for (RenderBox* child = iterator.first(); child; child = iterator.next(), ++i) {
        // FIXME: Does this need to take the scrollbar width into account?
        LayoutUnit childPreferredSize = childSizes[i] + logicalBorderAndPaddingWidthForChild(child);
        setLogicalOverrideSize(child, childPreferredSize);
        child->setChildNeedsLayout(true);
        child->layoutIfNeeded();

        setFlowAwareLogicalHeight(std::max(flowAwareLogicalHeight(), flowAwareBorderBefore() + flowAwarePaddingBefore() + flowAwareMarginBeforeForChild(child) + flowAwareLogicalHeightForChild(child) + flowAwareMarginAfterForChild(child) + flowAwarePaddingAfter() + flowAwareBorderAfter() + scrollbarLogicalHeight()));

        if (marginStartStyleForChild(child).isAuto())
            setFlowAwareMarginStartForChild(child, availableFreeSpace > 0 ? lroundf(availableFreeSpace / totalPositiveFlexibility) : 0);
        if (marginEndStyleForChild(child).isAuto())
            setFlowAwareMarginEndForChild(child, availableFreeSpace > 0 ? lroundf(availableFreeSpace / totalPositiveFlexibility) : 0);

        startEdge += flowAwareMarginStartForChild(child);

        LayoutUnit childLogicalWidth = flowAwareLogicalWidthForChild(child);
        LayoutUnit logicalLeft = isLeftToRightFlow() ? startEdge : totalAvailableLogicalWidth - startEdge - childLogicalWidth;
        // FIXME: Do repaintDuringLayoutIfMoved.
        // FIXME: Supporting layout deltas.
        setFlowAwareLogicalLocationForChild(child, IntPoint(logicalLeft, logicalTop));
        startEdge += childLogicalWidth + marginEndForChild(child);

        if (hasPackingSpace(availableFreeSpace, totalPositiveFlexibility) && style()->flexPack() == PackJustify && childSizes.size() > 1)
            startEdge += availableFreeSpace / (childSizes.size() - 1);
    }
}

}

#endif // ENABLE(CSS3_FLEXBOX)
