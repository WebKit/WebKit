/*
 * Copyright (C) 2010 Alex Milowski (alex@milowski.com). All rights reserved.
 * Copyright (C) 2016 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
#include "RenderMathMLRow.h"

#if ENABLE(MATHML)

#include "MathMLNames.h"
#include "MathMLRowElement.h"
#include "RenderIterator.h"
#include "RenderMathMLOperator.h"
#include "RenderMathMLRoot.h"

namespace WebCore {

using namespace MathMLNames;

RenderMathMLRow::RenderMathMLRow(MathMLRowElement& element, RenderStyle&& style)
    : RenderMathMLBlock(element, WTFMove(style))
{
}

MathMLRowElement& RenderMathMLRow::element() const
{
    return static_cast<MathMLRowElement&>(nodeForNonAnonymous());
}

std::optional<int> RenderMathMLRow::firstLineBaseline() const
{
    auto* baselineChild = firstChildBox();
    if (!baselineChild)
        return std::optional<int>();

    return std::optional<int>(static_cast<int>(lroundf(ascentForChild(*baselineChild) + baselineChild->logicalTop())));
}

void RenderMathMLRow::computeLineVerticalStretch(LayoutUnit& ascent, LayoutUnit& descent)
{
    for (auto* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (is<RenderMathMLBlock>(child)) {
            auto* renderOperator = downcast<RenderMathMLBlock>(child)->unembellishedOperator();
            if (renderOperator && renderOperator->isStretchy())
                continue;
        }

        child->layoutIfNeeded();

        LayoutUnit childHeightAboveBaseline = ascentForChild(*child);
        LayoutUnit childDepthBelowBaseline = child->logicalHeight() - childHeightAboveBaseline;

        ascent = std::max(ascent, childHeightAboveBaseline);
        descent = std::max(descent, childDepthBelowBaseline);
    }

    // We ensure a minimal stretch size.
    if (ascent + descent <= 0) {
        ascent = style().computedFontPixelSize();
        descent = 0;
    }
}

void RenderMathMLRow::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = 0;

    LayoutUnit preferredWidth = 0;
    for (auto* child = firstChildBox(); child; child = child->nextSiblingBox())
        preferredWidth += child->maxPreferredLogicalWidth() + child->marginLogicalWidth();

    m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = preferredWidth + borderAndPaddingLogicalWidth();

    setPreferredLogicalWidthsDirty(false);
}

void RenderMathMLRow::layoutRowItems(LayoutUnit& ascent, LayoutUnit& descent)
{
    // We first stretch the vertical operators.
    // For inline formulas, we can then calculate the logical width.
    LayoutUnit width = borderAndPaddingStart();
    for (auto* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (child->isOutOfFlowPositioned())
            continue;

        if (is<RenderMathMLBlock>(child)) {
            auto renderOperator = downcast<RenderMathMLBlock>(child)->unembellishedOperator();
            if (renderOperator && renderOperator->isStretchy() && renderOperator->isVertical())
                renderOperator->stretchTo(ascent, descent);
        }

        child->layoutIfNeeded();

        width += child->marginStart() + child->logicalWidth() + child->marginEnd();
    }

    width += borderEnd() + paddingEnd();
    if ((!isRenderMathMLMath() || style().display() == INLINE))
        setLogicalWidth(width);

    LayoutUnit verticalOffset = borderTop() + paddingTop();
    LayoutUnit maxAscent = 0, maxDescent = 0; // Used baseline alignment.
    LayoutUnit horizontalOffset = borderAndPaddingStart();
    bool shouldFlipHorizontal = !style().isLeftToRightDirection();
    for (auto* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (child->isOutOfFlowPositioned()) {
            child->containingBlock()->insertPositionedObject(*child);
            continue;
        }
        LayoutUnit childHorizontalExtent = child->logicalWidth();
        LayoutUnit ascent = ascentForChild(*child);
        LayoutUnit descent = child->verticalMarginExtent() + child->logicalHeight() - ascent;
        maxAscent = std::max(maxAscent, ascent);
        maxDescent = std::max(maxDescent, descent);
        LayoutUnit childVerticalMarginBoxExtent = maxAscent + maxDescent;

        horizontalOffset += child->marginStart();

        setLogicalHeight(std::max(logicalHeight(), verticalOffset + borderBottom() + paddingBottom() + childVerticalMarginBoxExtent + horizontalScrollbarHeight()));

        LayoutPoint childLocation(shouldFlipHorizontal ? logicalWidth() - horizontalOffset - childHorizontalExtent : horizontalOffset, verticalOffset + child->marginTop());
        child->setLocation(childLocation);

        horizontalOffset += childHorizontalExtent + child->marginEnd();
    }

    LayoutUnit centerBlockOffset = 0;
    if (style().display() == BLOCK)
        centerBlockOffset = std::max<LayoutUnit>(0, (logicalWidth() - (horizontalOffset + borderEnd() + paddingEnd())) / 2);

    if (shouldFlipHorizontal && centerBlockOffset > 0)
        centerBlockOffset = -centerBlockOffset;

    for (auto* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        LayoutUnit ascent = ascentForChild(*child);
        LayoutUnit startOffset = maxAscent - ascent;
        child->setLocation(child->location() + LayoutPoint(centerBlockOffset, startOffset));
    }

    ascent = maxAscent;
    descent = maxDescent;
}

void RenderMathMLRow::layoutBlock(bool relayoutChildren, LayoutUnit)
{
    ASSERT(needsLayout());

    if (!relayoutChildren && simplifiedLayout())
        return;

    LayoutUnit ascent = 0;
    LayoutUnit descent = 0;
    computeLineVerticalStretch(ascent, descent);

    recomputeLogicalWidth();

    setLogicalHeight(borderAndPaddingLogicalHeight() + scrollbarLogicalHeight());

    layoutRowItems(ascent, descent);

    updateLogicalHeight();

    clearNeedsLayout();
}

}

#endif // ENABLE(MATHML)
