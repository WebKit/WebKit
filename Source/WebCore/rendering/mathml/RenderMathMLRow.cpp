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

#if ENABLE(MATHML)

#include "RenderMathMLRow.h"

#include "MathMLNames.h"
#include "RenderIterator.h"
#include "RenderMathMLOperator.h"
#include "RenderMathMLRoot.h"

namespace WebCore {

using namespace MathMLNames;

RenderMathMLRow::RenderMathMLRow(Element& element, Ref<RenderStyle>&& style)
    : RenderMathMLBlock(element, WTFMove(style))
{
}

RenderMathMLRow::RenderMathMLRow(Document& document, Ref<RenderStyle>&& style)
    : RenderMathMLBlock(document, WTFMove(style))
{
}

void RenderMathMLRow::updateOperatorProperties()
{
    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        if (is<RenderMathMLBlock>(*child)) {
            if (auto* renderOperator = downcast<RenderMathMLBlock>(*child).unembellishedOperator())
                renderOperator->updateOperatorProperties();
        }
    }
    setNeedsLayoutAndPrefWidthsRecalc();
}


Optional<int> RenderMathMLRow::firstLineBaseline() const
{
    RenderBox* baselineChild = firstChildBox();
    if (!baselineChild)
        return Optional<int>();

    return Optional<int>(static_cast<int>(lroundf(ascentForChild(*baselineChild) + baselineChild->logicalTop())));
}

void RenderMathMLRow::computeLineVerticalStretch(int& stretchHeightAboveBaseline, int& stretchDepthBelowBaseline)
{
    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (is<RenderMathMLBlock>(child)) {
            auto* renderOperator = downcast<RenderMathMLBlock>(child)->unembellishedOperator();
            if (renderOperator && renderOperator->hasOperatorFlag(MathMLOperatorDictionary::Stretchy))
                continue;
        }

        child->layoutIfNeeded();

        LayoutUnit childHeightAboveBaseline = ascentForChild(*child);
        LayoutUnit childDepthBelowBaseline = child->logicalHeight() - childHeightAboveBaseline;

        stretchHeightAboveBaseline = std::max<LayoutUnit>(stretchHeightAboveBaseline, childHeightAboveBaseline);
        stretchDepthBelowBaseline = std::max<LayoutUnit>(stretchDepthBelowBaseline, childDepthBelowBaseline);
    }

    // We ensure a minimal stretch size.
    if (stretchHeightAboveBaseline + stretchDepthBelowBaseline <= 0) {
        stretchHeightAboveBaseline = style().fontSize();
        stretchDepthBelowBaseline = 0;
    }
}

void RenderMathMLRow::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = 0;

    LayoutUnit preferredWidth = 0;
    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox())
        preferredWidth += child->maxPreferredLogicalWidth() + child->marginLogicalWidth();

    m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = preferredWidth + borderAndPaddingLogicalWidth();

    setPreferredLogicalWidthsDirty(false);
}

void RenderMathMLRow::layoutRowItems(int stretchHeightAboveBaseline, int stretchDepthBelowBaseline)
{
    // We first stretch the vertical operators.
    // For inline formulas, we can then calculate the logical width.
    LayoutUnit width = borderAndPaddingStart();
    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (child->isOutOfFlowPositioned())
            continue;

        if (is<RenderMathMLBlock>(child)) {
            auto renderOperator = downcast<RenderMathMLBlock>(child)->unembellishedOperator();
            if (renderOperator && renderOperator->hasOperatorFlag(MathMLOperatorDictionary::Stretchy) && renderOperator->isVertical())
                renderOperator->stretchTo(stretchHeightAboveBaseline, stretchDepthBelowBaseline);
        }

        child->layoutIfNeeded();

        width += child->marginStart() + child->logicalWidth() + child->marginEnd();
    }

    width += borderEnd() + paddingEnd();
    // FIXME: RenderMathMLRoot and RenderMathMLEnclose classes should also recalculate the exact logical width instead of using the preferred width.
    // See https://bugs.webkit.org/show_bug.cgi?id=130326
    if ((!isRenderMathMLMath() || style().display() == INLINE) && !isRenderMathMLRoot() && !isRenderMathMLMenclose())
        setLogicalWidth(width);

    LayoutUnit verticalOffset = borderTop() + paddingTop();
    LayoutUnit maxAscent = 0, maxDescent = 0; // Used baseline alignment.
    LayoutUnit horizontalOffset = borderAndPaddingStart();
    bool shouldFlipHorizontal = !style().isLeftToRightDirection();
    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
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
    // FIXME: Remove the FLEX when it is not required by the css.
    if (style().display() == BLOCK || style().display() == FLEX)
        centerBlockOffset = std::max<LayoutUnit>(0, (logicalWidth() - (horizontalOffset + borderEnd() + paddingEnd())) / 2);

    if (shouldFlipHorizontal && centerBlockOffset > 0)
        centerBlockOffset = -centerBlockOffset;

    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        LayoutUnit ascent = ascentForChild(*child);
        LayoutUnit startOffset = maxAscent - ascent;
        child->setLocation(child->location() + LayoutPoint(centerBlockOffset, startOffset));
    }
}

void RenderMathMLRow::layoutBlock(bool relayoutChildren, LayoutUnit)
{
    ASSERT(needsLayout());

    if (!relayoutChildren && simplifiedLayout())
        return;

    int stretchHeightAboveBaseline = 0;
    int stretchDepthBelowBaseline = 0;
    computeLineVerticalStretch(stretchHeightAboveBaseline, stretchDepthBelowBaseline);

    recomputeLogicalWidth();

    setLogicalHeight(borderAndPaddingLogicalHeight() + scrollbarLogicalHeight());

    layoutRowItems(stretchHeightAboveBaseline, stretchDepthBelowBaseline);

    updateLogicalHeight();

    clearNeedsLayout();
}

void RenderMathMLRow::paintChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset, PaintInfo& paintInfoForChild, bool usePrintRect)
{
    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (!paintChild(*child, paintInfo, paintOffset, paintInfoForChild, usePrintRect, PaintAsInlineBlock))
            return;
    }
}

}

#endif // ENABLE(MATHML)
