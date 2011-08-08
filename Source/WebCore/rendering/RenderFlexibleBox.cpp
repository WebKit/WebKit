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

    // FIXME: Assume horizontal layout until flex-direction is added.
    layoutHorizontalBlock(relayoutChildren);

    computeLogicalHeight();

    if (size() != previousSize)
        relayoutChildren = true;

    layoutPositionedObjects(relayoutChildren || isRoot());

    updateLayerTransform();

    setNeedsLayout(false);
}

static LayoutUnit preferredFlexItemContentWidth(RenderBox* child)
{
    if (child->style()->width().isAuto())
        return child->maxPreferredLogicalWidth() - child->borderLeft() - child->borderRight() - child->verticalScrollbarWidth() - child->paddingLeft() - child->paddingRight();
    return child->contentWidth();
}

void RenderFlexibleBox::layoutHorizontalBlock(bool relayoutChildren)
{
    LayoutUnit preferredSize;
    float totalPositiveFlexibility;
    float totalNegativeFlexibility;
    FlexibleBoxIterator iterator(this);

    computePreferredSize(relayoutChildren, iterator, preferredSize, totalPositiveFlexibility, totalNegativeFlexibility);
    LayoutUnit availableFreeSpace = contentWidth() - preferredSize;

    LayoutUnit xOffset = borderLeft() + paddingLeft();
    LayoutUnit yOffset = borderTop() + paddingTop();
    for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
        LayoutUnit childPreferredSize = preferredFlexItemContentWidth(child);
        // FIXME: Handle max-width and min-width (we should clamp to the max/min value and restart the algorithm).
        if (availableFreeSpace > 0 && totalPositiveFlexibility > 0)
            childPreferredSize += lroundf(availableFreeSpace * child->style()->flexboxWidthPositiveFlex() / totalPositiveFlexibility);
        else if (availableFreeSpace < 0 && totalNegativeFlexibility > 0)
            childPreferredSize += lroundf(availableFreeSpace * child->style()->flexboxWidthNegativeFlex() / totalNegativeFlexibility);

        childPreferredSize += child->borderLeft() + child->borderRight() + child->paddingLeft() + child->paddingRight();
        child->setOverrideSize(LayoutSize(childPreferredSize, 0));
        child->setChildNeedsLayout(true);
        child->layoutIfNeeded();

        setHeight(std::max(height(), borderTop() + paddingTop() + child->marginTop() + child->height() + child->marginBottom() + paddingBottom() + borderBottom() + horizontalScrollbarHeight()));

        // FIXME: Handle child margins.
        child->setLocation(IntPoint(xOffset, yOffset));
        xOffset += child->width();
    }

    // FIXME: Distribute leftover space to the packing space (second distribution round).
    // FIXME: Handle distribution of vertical space (third distribution round).
}

static LayoutUnit preferredSizeForMarginsAndPadding(Length length, LayoutUnit containerLength)
{
    return length.calcMinValue(containerLength);
}

void RenderFlexibleBox::computePreferredSize(bool relayoutChildren, FlexibleBoxIterator& iterator, LayoutUnit& preferredSize, float& totalPositiveFlexibility, float& totalNegativeFlexibility)
{
    preferredSize = 0;
    totalPositiveFlexibility = totalNegativeFlexibility = 0;

    LayoutUnit flexboxAvailableLogicalWidth = availableLogicalWidth();
    for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
        // We always have to lay out flexible objects again, since the flex distribution
        // may have changed, and we need to reallocate space.
        child->clearOverrideSize();
        if (!relayoutChildren)
            child->setChildNeedsLayout(true);
        child->layoutIfNeeded();

        // FIXME: Margins and paddings set to auto have a positive flexibility of 1.
        preferredSize += preferredSizeForMarginsAndPadding(child->style()->marginLeft(), flexboxAvailableLogicalWidth);
        preferredSize += preferredSizeForMarginsAndPadding(child->style()->marginRight(), flexboxAvailableLogicalWidth);
        preferredSize += preferredSizeForMarginsAndPadding(child->style()->paddingLeft(), flexboxAvailableLogicalWidth);
        preferredSize += preferredSizeForMarginsAndPadding(child->style()->paddingRight(), flexboxAvailableLogicalWidth);

        preferredSize += child->borderLeft() + child->borderRight();

        preferredSize += preferredFlexItemContentWidth(child);

        totalPositiveFlexibility += child->style()->flexboxWidthPositiveFlex();
        totalNegativeFlexibility += child->style()->flexboxWidthNegativeFlex();
    }
}

}

#endif // ENABLE(CSS3_FLEXBOX)
