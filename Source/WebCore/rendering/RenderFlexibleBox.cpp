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

class RenderFlexibleBox::FlexibleBoxIterator {
public:
    explicit FlexibleBoxIterator(RenderFlexibleBox* flexibleBox)
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

        m_currentChild = toRenderBox(child);
        return m_currentChild;
    }

    void reset()
    {
        m_currentChild = 0;
    }

private:
    RenderFlexibleBox* m_flexibleBox;
    RenderBox* m_currentChild;
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

LayoutUnit RenderFlexibleBox::logicalBorderWidthForChild(RenderBox* child)
{
    if (isHorizontalWritingMode())
        return child->borderLeft() + child->borderRight();
    return child->borderTop() + child->borderBottom();
}

LayoutUnit RenderFlexibleBox::logicalPaddingWidthForChild(RenderBox* child)
{
    if (isHorizontalWritingMode())
        return child->paddingLeft() + child->paddingRight();
    return child->paddingTop() + child->paddingBottom();
}

LayoutUnit RenderFlexibleBox::logicalScrollbarHeightForChild(RenderBox* child)
{
    if (isHorizontalWritingMode())
        return child->horizontalScrollbarHeight();
    return child->verticalScrollbarWidth();
}

Length RenderFlexibleBox::marginStartStyleForChild(RenderBox* child)
{
    if (isHorizontalWritingMode())
        return style()->isLeftToRightDirection() ? child->style()->marginLeft() : child->style()->marginRight();
    return style()->isLeftToRightDirection() ? child->style()->marginTop() : child->style()->marginBottom();
}

Length RenderFlexibleBox::marginEndStyleForChild(RenderBox* child)
{
    if (isHorizontalWritingMode())
        return style()->isLeftToRightDirection() ? child->style()->marginRight() : child->style()->marginLeft();
    return style()->isLeftToRightDirection() ? child->style()->marginBottom() : child->style()->marginTop();
}

LayoutUnit RenderFlexibleBox::preferredLogicalContentWidthForFlexItem(RenderBox* child)
{
    Length width = isHorizontalWritingMode() ? child->style()->width() : child->style()->height();
    if (width.isAuto()) {
        LayoutUnit logicalWidth = isHorizontalWritingMode() == child->isHorizontalWritingMode() ? child->maxPreferredLogicalWidth() : child->logicalHeight();
        return logicalWidth - logicalBorderWidthForChild(child) - logicalScrollbarHeightForChild(child) - logicalPaddingWidthForChild(child);
    }
    return isHorizontalWritingMode() ? child->contentWidth() : child->contentHeight();
}

void RenderFlexibleBox::layoutInlineDirection(bool relayoutChildren)
{
    LayoutUnit preferredLogicalWidth;
    float totalPositiveFlexibility;
    float totalNegativeFlexibility;
    FlexibleBoxIterator iterator(this);

    computePreferredLogicalWidth(relayoutChildren, iterator, preferredLogicalWidth, totalPositiveFlexibility, totalNegativeFlexibility);
    LayoutUnit availableFreeSpace = contentLogicalWidth() - preferredLogicalWidth;

    InflexibleFlexItemSize inflexibleItems;
    WTF::Vector<LayoutUnit> childSizes;
    while (!runFreeSpaceAllocationAlgorithmInlineDirection(availableFreeSpace, totalPositiveFlexibility, totalNegativeFlexibility, inflexibleItems, childSizes)) {
        ASSERT(totalPositiveFlexibility >= 0 && totalNegativeFlexibility >= 0);
        ASSERT(inflexibleItems.size() > 0);
    }

    layoutAndPlaceChildrenInlineDirection(childSizes, availableFreeSpace, totalPositiveFlexibility);

    // FIXME: Handle distribution of cross-axis space (third distribution round).
}

float RenderFlexibleBox::logicalPositiveFlexForChild(RenderBox* child)
{
    return isHorizontalWritingMode() ? child->style()->flexboxWidthPositiveFlex() : child->style()->flexboxHeightPositiveFlex();
}

float RenderFlexibleBox::logicalNegativeFlexForChild(RenderBox* child)
{
    return isHorizontalWritingMode() ? child->style()->flexboxWidthNegativeFlex() : child->style()->flexboxHeightNegativeFlex();
}

void RenderFlexibleBox::computePreferredLogicalWidth(bool relayoutChildren, FlexibleBoxIterator& iterator, LayoutUnit& preferredLogicalWidth, float& totalPositiveFlexibility, float& totalNegativeFlexibility)
{
    preferredLogicalWidth = 0;
    totalPositiveFlexibility = totalNegativeFlexibility = 0;

    LayoutUnit flexboxAvailableLogicalWidth = availableLogicalWidth();
    for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
        // We always have to lay out flexible objects again, since the flex distribution
        // may have changed, and we need to reallocate space.
        child->clearOverrideSize();
        if (!relayoutChildren)
            child->setChildNeedsLayout(true);
        child->layoutIfNeeded();

        if (isHorizontalWritingMode()) {
            preferredLogicalWidth += child->style()->marginLeft().calcMinValue(flexboxAvailableLogicalWidth);
            preferredLogicalWidth += child->style()->marginRight().calcMinValue(flexboxAvailableLogicalWidth);
            preferredLogicalWidth += child->style()->paddingLeft().calcMinValue(flexboxAvailableLogicalWidth);
            preferredLogicalWidth += child->style()->paddingRight().calcMinValue(flexboxAvailableLogicalWidth);
        } else {
            preferredLogicalWidth += child->style()->marginTop().calcMinValue(flexboxAvailableLogicalWidth);
            preferredLogicalWidth += child->style()->marginBottom().calcMinValue(flexboxAvailableLogicalWidth);
            preferredLogicalWidth += child->style()->paddingTop().calcMinValue(flexboxAvailableLogicalWidth);
            preferredLogicalWidth += child->style()->paddingBottom().calcMinValue(flexboxAvailableLogicalWidth);
        }

        if (marginStartStyleForChild(child).isAuto())
            totalPositiveFlexibility += 1;
        if (marginEndStyleForChild(child).isAuto())
            totalPositiveFlexibility += 1;

        preferredLogicalWidth += logicalBorderWidthForChild(child);
        preferredLogicalWidth += preferredLogicalContentWidthForFlexItem(child);

        totalPositiveFlexibility += logicalPositiveFlexForChild(child);
        totalNegativeFlexibility += logicalNegativeFlexForChild(child);
    }
}

// Returns true if we successfully ran the algorithm and sized the flex items.
bool RenderFlexibleBox::runFreeSpaceAllocationAlgorithmInlineDirection(LayoutUnit& availableFreeSpace, float& totalPositiveFlexibility, float& totalNegativeFlexibility, InflexibleFlexItemSize& inflexibleItems, WTF::Vector<LayoutUnit>& childSizes)
{
    FlexibleBoxIterator iterator(this);
    childSizes.clear();

    LayoutUnit flexboxAvailableLogicalWidth = availableLogicalWidth();
    for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
        LayoutUnit childPreferredSize;
        if (inflexibleItems.contains(child))
            childPreferredSize = inflexibleItems.get(child);
        else {
            childPreferredSize = preferredLogicalContentWidthForFlexItem(child);
            if (availableFreeSpace > 0 && totalPositiveFlexibility > 0) {
                childPreferredSize += lroundf(availableFreeSpace * logicalPositiveFlexForChild(child) / totalPositiveFlexibility);

                Length childLogicalMaxWidth = isHorizontalWritingMode() ? child->style()->maxWidth() : child->style()->maxHeight();
                if (!childLogicalMaxWidth.isUndefined() && childLogicalMaxWidth.isSpecified() && childPreferredSize > childLogicalMaxWidth.calcValue(flexboxAvailableLogicalWidth)) {
                    childPreferredSize = childLogicalMaxWidth.calcValue(flexboxAvailableLogicalWidth);
                    availableFreeSpace -= childPreferredSize - preferredLogicalContentWidthForFlexItem(child);
                    totalPositiveFlexibility -= logicalPositiveFlexForChild(child);

                    inflexibleItems.set(child, childPreferredSize);
                    return false;
                }
            } else if (availableFreeSpace < 0 && totalNegativeFlexibility > 0) {
                childPreferredSize += lroundf(availableFreeSpace * logicalNegativeFlexForChild(child) / totalNegativeFlexibility);

                Length childLogicalMinWidth = isHorizontalWritingMode() ? child->style()->minWidth() : child->style()->minHeight();
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
    if (isHorizontalWritingMode())
        child->isHorizontalWritingMode() ? child->setOverrideWidth(childPreferredSize) : child->setOverrideHeight(childPreferredSize);
    else
        child->isHorizontalWritingMode() ? child->setOverrideHeight(childPreferredSize) : child->setOverrideWidth(childPreferredSize);
}

void RenderFlexibleBox::layoutAndPlaceChildrenInlineDirection(const WTF::Vector<LayoutUnit>& childSizes, LayoutUnit availableFreeSpace, float totalPositiveFlexibility)
{
    FlexibleBoxIterator iterator(this);
    LayoutUnit startEdge = borderStart() + paddingStart();

    if (hasPackingSpace(availableFreeSpace, totalPositiveFlexibility)) {
        if (style()->flexPack() == PackEnd)
            startEdge += availableFreeSpace;
        else if (style()->flexPack() == PackCenter)
            startEdge += availableFreeSpace / 2;
    }

    LayoutUnit logicalTop = borderBefore() + paddingBefore();
    LayoutUnit totalAvailableLogicalWidth = availableLogicalWidth();
    setLogicalHeight(0);
    size_t i = 0;
    for (RenderBox* child = iterator.first(); child; child = iterator.next(), ++i) {
        // FIXME: Does this need to take the scrollbar width into account?
        LayoutUnit childPreferredSize = childSizes[i] + logicalBorderWidthForChild(child) + logicalPaddingWidthForChild(child);
        setLogicalOverrideSize(child, childPreferredSize);
        child->setChildNeedsLayout(true);
        child->layoutIfNeeded();

        setLogicalHeight(std::max(logicalHeight(), borderBefore() + paddingBefore() + marginBeforeForChild(child) + logicalHeightForChild(child) + marginAfterForChild(child) + paddingAfter() + borderAfter() + scrollbarLogicalHeight()));

        if (marginStartStyleForChild(child).isAuto())
            setMarginStartForChild(child, availableFreeSpace > 0 ? lroundf(availableFreeSpace / totalPositiveFlexibility) : 0);
        if (marginEndStyleForChild(child).isAuto())
            setMarginEndForChild(child, availableFreeSpace > 0 ? lroundf(availableFreeSpace / totalPositiveFlexibility) : 0);

        startEdge += marginStartForChild(child);

        LayoutUnit childLogicalWidth = logicalWidthForChild(child);
        LayoutUnit logicalLeft = style()->isLeftToRightDirection() ? startEdge : totalAvailableLogicalWidth - startEdge - childLogicalWidth;
        // FIXME: Do repaintDuringLayoutIfMoved.
        // FIXME: Supporting layout deltas.
        setLogicalLocationForChild(child, IntPoint(logicalLeft, logicalTop));
        startEdge += childLogicalWidth + marginEndForChild(child);

        if (hasPackingSpace(availableFreeSpace, totalPositiveFlexibility) && style()->flexPack() == PackJustify && childSizes.size() > 1)
            startEdge += availableFreeSpace / (childSizes.size() - 1);
    }
}

}

#endif // ENABLE(CSS3_FLEXBOX)
