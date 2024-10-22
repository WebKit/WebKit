/*
 * Copyright (C) 2010 Alex Milowski (alex@milowski.com). All rights reserved.
 * Copyright (C) 2016 Igalia S.L.
 * Copyright (C) 2023 Apple Inc. All right reserved.
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
#include "RenderMathMLBlockInlines.h"
#include "RenderMathMLOperator.h"
#include "RenderMathMLRoot.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using namespace MathMLNames;

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RenderMathMLRow);

RenderMathMLRow::RenderMathMLRow(Type type, MathMLRowElement& element, RenderStyle&& style)
    : RenderMathMLBlock(type, element, WTFMove(style))
{
    ASSERT(isRenderMathMLRow());
}

RenderMathMLRow::~RenderMathMLRow() = default;

MathMLRowElement& RenderMathMLRow::element() const
{
    return static_cast<MathMLRowElement&>(nodeForNonAnonymous());
}

std::optional<LayoutUnit> RenderMathMLRow::firstLineBaseline() const
{
    auto* baselineChild = firstInFlowChildBox();
    if (!baselineChild)
        return std::optional<LayoutUnit>();

    return LayoutUnit { static_cast<int>(lroundf(ascentForChild(*baselineChild) + baselineChild->marginBefore() + baselineChild->logicalTop())) };
}

static RenderMathMLOperator* toVerticalStretchyOperator(RenderBox* box)
{
    if (auto* mathMLBlock = dynamicDowncast<RenderMathMLBlock>(box)) {
        auto* renderOperator = mathMLBlock->unembellishedOperator();
        if (renderOperator && renderOperator->isStretchy() && renderOperator->isVertical())
            return renderOperator;
    }
    return nullptr;
}

void RenderMathMLRow::stretchVerticalOperatorsAndLayoutChildren()
{
    // First calculate stretch ascent and descent.
    LayoutUnit stretchAscent, stretchDescent;
    for (auto* child = firstInFlowChildBox(); child; child = child->nextInFlowSiblingBox()) {
        if (toVerticalStretchyOperator(child))
            continue;
        child->layoutIfNeeded();
        LayoutUnit childAscent = ascentForChild(*child) + child->marginBefore();
        LayoutUnit childDescent = child->logicalHeight() + child->marginLogicalHeight() - childAscent;
        stretchAscent = std::max(stretchAscent, childAscent);
        stretchDescent = std::max(stretchDescent, childDescent);
    }
    if (stretchAscent + stretchDescent <= 0) {
        // We ensure a minimal stretch size.
        stretchAscent = style().computedFontSize();
        stretchDescent = 0;
    }

    // Next, we stretch the vertical operators.
    for (auto* child = firstInFlowChildBox(); child; child = child->nextInFlowSiblingBox()) {
        if (auto renderOperator = toVerticalStretchyOperator(child)) {
            renderOperator->stretchTo(stretchAscent, stretchDescent);
            renderOperator->layoutIfNeeded();
            child->layoutIfNeeded();
        }
    }
}

void RenderMathMLRow::getContentBoundingBox(LayoutUnit& width, LayoutUnit& ascent, LayoutUnit& descent) const
{
    ascent = 0;
    descent = 0;
    width = 0;
    for (auto* child = firstInFlowChildBox(); child; child = child->nextInFlowSiblingBox()) {
        width += child->marginStart() + child->logicalWidth() + child->marginEnd();
        LayoutUnit childAscent = ascentForChild(*child) + child->marginBefore();
        LayoutUnit childDescent = child->logicalHeight() + child->marginLogicalHeight() - childAscent;
        ascent = std::max(ascent, childAscent);
        descent = std::max(descent, childDescent);
    }
}

LayoutUnit RenderMathMLRow::preferredLogicalWidthOfRowItems()
{
    LayoutUnit preferredWidth = 0;
    for (auto* child = firstInFlowChildBox(); child; child = child->nextInFlowSiblingBox()) {
        preferredWidth += child->maxPreferredLogicalWidth() + marginIntrinsicLogicalWidthForChild(*child);
    }
    return preferredWidth;
}

void RenderMathMLRow::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = preferredLogicalWidthOfRowItems();

    adjustPreferredLogicalWidthsForBorderAndPadding();

    setPreferredLogicalWidthsDirty(false);
}

void RenderMathMLRow::layoutRowItems(LayoutUnit width, LayoutUnit ascent)
{
    LayoutUnit horizontalOffset = 0;
    for (auto* child = firstInFlowChildBox(); child; child = child->nextInFlowSiblingBox()) {
        horizontalOffset += child->marginStart();
        LayoutUnit childVerticalOffset = ascent - ascentForChild(*child);
        LayoutUnit childWidth = child->logicalWidth();
        LayoutUnit childHorizontalOffset = writingMode().isBidiLTR() ? horizontalOffset : width - horizontalOffset - childWidth;
        auto repaintRect = child->checkForRepaintDuringLayout() ? std::make_optional(child->frameRect()) : std::nullopt;
        child->setLocation(LayoutPoint(childHorizontalOffset, childVerticalOffset));
        if (repaintRect) {
            repaintRect->uniteEvenIfEmpty(child->frameRect());
            repaintRectangle(*repaintRect);
        }
        horizontalOffset += childWidth + child->marginEnd();
    }
}

void RenderMathMLRow::layoutBlock(bool relayoutChildren, LayoutUnit)
{
    ASSERT(needsLayout());

    insertPositionedChildrenIntoContainingBlock();

    if (!relayoutChildren && simplifiedLayout())
        return;

    layoutFloatingChildren();

    recomputeLogicalWidth();
    computeAndSetBlockDirectionMarginsOfChildren();

    setLogicalHeight(borderAndPaddingLogicalHeight() + scrollbarLogicalHeight());

    LayoutUnit width, ascent, descent;
    stretchVerticalOperatorsAndLayoutChildren();
    getContentBoundingBox(width, ascent, descent);
    layoutRowItems(width, ascent);
    setLogicalWidth(width);
    setLogicalHeight(ascent + descent);

    adjustLayoutForBorderAndPadding();

    updateLogicalHeight();

    layoutPositionedObjects(relayoutChildren);

    updateScrollInfoAfterLayout();

    clearNeedsLayout();
}

}

#endif // ENABLE(MATHML)
